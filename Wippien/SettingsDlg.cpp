// SettingsDlg.cpp: implementation of the CSettingsDlg class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SettingsDlg.h"
#include "ComBSTR2.h"
#include "Buffer.h"
#include "Jabber.h"
#include "Settings.h"
#include "Ethernet.h"
#include "MainDlg.h"
#include "ChatRoom.h"
#include "Notify.h"
#include <io.h>
#include <fcntl.h>
#include <sys\stat.h>
#include <Iphlpapi.h>

#ifdef _SKINMAGICKEY
#include "SkinMagicLib.h"
#endif
#include "DownloadSkinDlg.h"
#include "UpdateHandler.h"
#include "HideContact.h"

extern CAppModule _Module;
extern CSettings _Settings;
extern CJabber *_Jabber;
extern CMainDlg _MainDlg;
extern CEthernet _Ethernet;
extern CNotify _Notify;

void ResampleImageIfNeeded(CxImage *img, int size);
void ResampleImageIfNeeded(CxImage *img, int sizeX, int sizeY);
void PopulateChatRoomListview(void);
char *trim(char *text);


static const char Base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

BOOL _LoadIconFromResource(CxImage *img, char *restype, int imgformat, int resid);
int uuencode(unsigned char *src, unsigned int srclength,char *target, size_t targsize);
int uudecode(const char *src, u_char *target, size_t targsize);
int b64_pton(char const *src, u_char *target, size_t targsize);
int b64_ntop(u_char const *src, size_t srclength, char *target, size_t targsize);

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

int b64_ntop(u_char const *src, size_t srclength, char *target, size_t targsize) {
	/* [<][>][^][v][top][bottom][index][help] */
	size_t datalength = 0;
	u_char input[3];
	u_char output[4];
	size_t i;
	
	while (2 < srclength) {
		input[0] = *src++;
		input[1] = *src++;
		input[2] = *src++;
		srclength -= 3;
		
		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
		output[3] = input[2] & 0x3f;
		
		if (datalength + 4 > targsize)
			return (-1);
		target[datalength++] = Base64[output[0]];
		target[datalength++] = Base64[output[1]];
		target[datalength++] = Base64[output[2]];
		target[datalength++] = Base64[output[3]];
	}
    
	/* Now we worry about padding. */
	if (0 != srclength) {
		/* Get what's left. */
		input[0] = input[1] = input[2] = '\0';
		for (i = 0; i < srclength; i++)
			input[i] = *src++;
        
		output[0] = input[0] >> 2;
		output[1] = ((input[0] & 0x03) << 4) + (input[1] >> 4);
		output[2] = ((input[1] & 0x0f) << 2) + (input[2] >> 6);
		
		if (datalength + 4 > targsize)
			return (-1);
		target[datalength++] = Base64[output[0]];
		target[datalength++] = Base64[output[1]];
		if (srclength == 1)
			target[datalength++] = Pad64;
		else
			target[datalength++] = Base64[output[2]];
		target[datalength++] = Pad64;
	}
	if (datalength >= targsize)
		return (-1);
	target[datalength] = '\0';      /* Returned value doesn't count \0. */
	return (datalength);
}

int uuencode(unsigned char *src, unsigned int srclength,
    char *target, size_t targsize)
{
	return b64_ntop(src, srclength, target, targsize);
}


CSettingsDlg::CSettingsDlg(BOOL Modal)
{
	m_Modal = Modal;
	m_WhiteBrush = CreateSolidBrush(RGB(255,255,255));
	m_DlgPos = 0;
	m_OnInit = TRUE;
	m_VCardChanged = FALSE;
	m_NeedRestart = FALSE;
}

CSettingsDlg::~CSettingsDlg()
{
	DeleteObject(m_WhiteBrush);
}

LRESULT CSettingsDlg::OnCtlColorStatic(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	return (LRESULT)m_WhiteBrush;
}

LRESULT CSettingsDlg::OnNext(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_DlgPos+1 < m_Dialogs.size())
	{
		_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[m_DlgPos];
//		if (tem->Apply())
			ShowDialog(++m_DlgPos);
	}
	else
	{
		OnOk(wNotifyCode, wID, hWndCtl, bHandled);
	}
	return 0;
}

LRESULT CSettingsDlg::OnBack(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_DlgPos > 0)
	{
		ShowDialog(--m_DlgPos);
	}
	return 0;
}

LRESULT CSettingsDlg::OnTreeNotify(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{
	if (m_OnInit)
		return FALSE;

	NMTREEVIEW *pnmtv;

	pnmtv = (NMTREEVIEW *)pnmh;
	switch (pnmh->code)
	{
		case TVN_SELCHANGED:	
			for (int i=0;i<(signed)m_Dialogs.size();i++)
			{
				_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[i];
				if (tem->m_hItem == pnmtv->itemNew.hItem)
				{

//					_CSettingsTemplate *tem1 = (_CSettingsTemplate *)m_Dialogs[m_DlgPos];
//					if (tem1->Apply())
					{
						ShowDialog(i);
						return FALSE;
					}
				}
			}
			break;
	}

	return FALSE;
}

LRESULT CSettingsDlg::OnInitDialog(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	m_OnInit = TRUE;
	m_Tree.Attach(GetDlgItem(IDC_TREE1));
	
	SetIcon(LoadIcon(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDR_SETTINGSICON)));

	CFontHandle DlgFont = GetFont();
	CClientDC dcScreen(NULL);
	
	LOGFONT LogFont = {0};
	DlgFont.GetLogFont(&LogFont);

	_tcscpy(LogFont.lfFaceName, _T("arial bold"));
	LogFont.lfWeight = FW_BOLD;
	LogFont.lfCharSet = DEFAULT_CHARSET;
	LogFont.lfHeight = -::MulDiv(8, dcScreen.GetDeviceCaps(LOGPIXELSY), 72);
	DlgTitle1.CreateFontIndirect(&LogFont);
	
	_tcscpy(LogFont.lfFaceName, _T("arial"));
	LogFont.lfWeight = FW_NORMAL;
	LogFont.lfCharSet = DEFAULT_CHARSET;
	LogFont.lfHeight = -::MulDiv(8, dcScreen.GetDeviceCaps(LOGPIXELSY), 72);
	DlgTitle2.CreateFontIndirect(&LogFont);

	CWindow w1 = GetDlgItem(IDC_TEXT1);
	w1.SetFont(DlgTitle1);
	CWindow w2 = GetDlgItem(IDC_TEXT2);
	w2.SetFont(DlgTitle2);

	for (int i=0;i<(signed)m_Dialogs.size();i++)
	{
		_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[i];
		if (tem->PATH && *tem->PATH)
		{
			tem->m_hItem = AddToTree(tem->PATH, NULL);
		}
		tem->Init(m_hWnd);
//		tem->m_Changed = FALSE;
		tem->m_NeedRestart = FALSE;
		tem->m_VCardChanged = FALSE;
		tem->m_Starting=FALSE;
	}

	if (m_Modal) // is wizard
	{
		::ShowWindow(GetDlgItem(IDC_TREE1), SW_HIDE);
		::ShowWindow(GetDlgItem(IDC_TREEBITMAP), SW_SHOW);
	}
	else
	{
		::ShowWindow(GetDlgItem(IDC_TREE1), SW_SHOW);
		::ShowWindow(GetDlgItem(IDC_TREEBITMAP), SW_HIDE);
	}

	m_OnInit = FALSE;

	// select 0th
	m_DlgPos = 0;
	if (m_Dialogs.size())
	{
			_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[0];
		::SendMessage(GetDlgItem(IDC_TREE1), TVM_SELECTITEM, TVGN_CARET, (LPARAM)tem->m_hItem);
	}

	
	//	ShowDialog(m_DlgPos);
//	CenterWindow(GetParent());
//	OnSize(uMsg, wParam, lParam, bHandled);

	CenterWindow(GetParent());

	SetWindowText(_Settings.Translate("Settings"));
	SetDlgItemText(IDC_BACK, _Settings.Translate("< &Back"));
	SetDlgItemText(IDC_NEXT, _Settings.Translate("&Next >"));
	SetDlgItemText(IDOK, _Settings.Translate("O&K"));
	SetDlgItemText(IDCANCEL, _Settings.Translate("&Cancel"));
	return TRUE;
}

void CSettingsDlg::ShowDialog(int Index)
{

	m_DlgPos = Index;

	RECT rc;
	RECT rctree, rcme;
	::GetWindowRect(GetDlgItem(IDC_TREE1), &rctree);
	GetWindowRect(&rcme);

	rc.top = 65;
	rc.left = rctree.right - rctree.left + 15;
	
	rc.bottom = rcme.bottom-rcme.top - 140;
	rc.right = rcme.right - rcme.left - rc.left;


	if (m_Dialogs.size())
	{
		_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[Index];
		SetDlgItemText(IDC_TEXT1, tem->TEXT1);
		SetDlgItemText(IDC_TEXT2, tem->TEXT2);


		for (int i=0;i<(signed)m_Dialogs.size();i++)
		{
			_CSettingsTemplate *temold = (_CSettingsTemplate *)m_Dialogs[i];
			if (i != Index)
				temold->Show(FALSE, NULL);
			else
				tem->Show(TRUE, &rc);
		}
	}

	if (m_Modal)
	{
		if (Index == 0)
			::EnableWindow(GetDlgItem(IDC_BACK), FALSE);
		else
			::EnableWindow(GetDlgItem(IDC_BACK), TRUE);	

		if (Index == m_Dialogs.size()-1)
		{
			::SetWindowText(GetDlgItem(IDC_NEXT), _Settings.Translate("&Finish"));
//			::EnableWindow(GetDlgItem(IDC_NEXT), FALSE);
		}
		else
		{
			::SetWindowText(GetDlgItem(IDC_NEXT), _Settings.Translate("&Next >"));
//			::EnableWindow(GetDlgItem(IDC_NEXT), TRUE);
		}
	}
	else
	{
		::ShowWindow(GetDlgItem(IDC_BACK), SW_HIDE);
		::ShowWindow(GetDlgItem(IDC_NEXT), SW_HIDE);
	}
}


HTREEITEM CSettingsDlg::FindTree(char *Path, HTREEITEM at)
{
	// find this item, or create it if it doesn't exis
	
	HTREEITEM root;
	if (at)
		root = m_Tree.GetChildItem(at);
	else
		root = m_Tree.GetRootItem();
	do
	{
		BSTR t = NULL;
		CComBSTR2 t1;
		m_Tree.GetItemText(root, t);
		t1.Empty();
		t1 = t;
		::SysFreeString(t);

		if (!strcmp(Path, t1.ToString()))
			return root;
	
		root = m_Tree.GetNextItem(root, TVGN_NEXT);
	} while (root);

	return NULL;
}

HTREEITEM CSettingsDlg::AddToTree(char *Path, HTREEITEM at)
{
	HTREEITEM t = NULL;
	Buffer p;
	
	p.Append(Path);
	p.Append("\0",1);
	char *path = p.Ptr();

	char *right = strchr(path, '\\');
	if (right)
	{
		*right = 0;
		right++;
	}

	HTREEITEM it = FindTree(path, at);
	if (!it)
	{
		TV_INSERTSTRUCT TreeItem;
		TreeItem.hParent = at?at:NULL; 
		TreeItem.hInsertAfter = TVI_LAST ;
		TreeItem.itemex.mask = TVIF_TEXT /*| TVIF_CHILDREN  | TVIF_PARAM*/;
		TreeItem.itemex.pszText = path;
	
		it = m_Tree.InsertItem(&TreeItem);	
	}	

	if (at)
		m_Tree.Expand(at);
	if (right)
	{
		it = AddToTree(right, it);
	}
	return it;
}

LRESULT CSettingsDlg::OnOk(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{

	if (m_Modal) // is wizard
	{
		for (int i=0;i<(signed)m_Dialogs.size();i++)
		{
			_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[i];
			tem->DoDefault();
		}
	}


//	int dp = m_DlgPos;
	for (int i=0;i<(signed)m_Dialogs.size();i++)
	{
		_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[i];
//		if (i<=dp)
		{
			ShowDialog(i);
			if (!tem->Apply())
			{
				return FALSE;
			}
		}
	}
	ShowWindow(SW_HIDE);

	_Settings.Save();

	if (m_VCardChanged)
	{
		if (_Jabber)
		{
			// and request vcard
#ifndef _WODXMPPLIB
			WODXMPPCOMLib::IXMPPVCard *vc;
			if (SUCCEEDED(_Jabber->m_Jabb->get_VCard(&vc)))
#else
			void *vc;
			WODXMPPCOMLib::XMPP_GetVCard(_Jabber->m_Jabb, &vc);
			if (vc)
#endif
			{

				// put photo...
//				CxImage cx;
//				cx.Decode((unsigned char *)_Settings.m_Icon.Ptr(), _Settings.m_Icon.Len(), CXIMAGE_FORMAT_PNG);

				if (_Settings.m_Icon.Len())
				{

#ifndef _WODXMPPLIB
					SAFEARRAY * psaiNew; 
					SAFEARRAYBOUND aDim[1]; 
					aDim[0].lLbound = 1; 
					aDim[0].cElements = _Settings.m_Icon.Len(); 

					// Equivalent to: Dim aiNew(1 To 8) As Integer.
					psaiNew = SafeArrayCreate(VT_UI1, 1, aDim);
					if (psaiNew)
					{
						char HUGEP *data;
						SafeArrayAccessData(psaiNew, (void HUGEP**) &data);
						memcpy(data, _Settings.m_Icon.Ptr(), _Settings.m_Icon.Len());
						SafeArrayUnaccessData(psaiNew);

						vc->put_PhotoData(&psaiNew);
					}
#else
					WODXMPPCOMLib::XMPP_VCard_SetPhotoData(vc, _Settings.m_Icon.Ptr(), _Settings.m_Icon.Len());
#endif

				}

#ifndef _WODXMPPLIB
				vc->Send();
				vc->Release();
#else
				WODXMPPCOMLib::XMPP_VCard_Send(vc);
				WODXMPPCOMLib::XMPP_VCard_Free(vc);
#endif
			}
		}
	}

	
	if (m_Modal)
		EndDialog(1);
	else
		DestroyWindow();

/*	while (m_Dialogs.size())
	{
		_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[0];
		m_Dialogs.erase(m_Dialogs.begin());
		delete tem;
	}
*/
	return 0;
}	
LRESULT CSettingsDlg::OnCancel(WORD /*wNotifyCode*/, WORD wID, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
	if (m_Modal)
		EndDialog(0);
	else
		DestroyWindow();

/*	while (m_Dialogs.size())
	{
		_CSettingsTemplate *tem = (_CSettingsTemplate *)m_Dialogs[0];
		m_Dialogs.erase(m_Dialogs.begin());
//		delete tem;
	}
*/
	return 0;
}	


CSettingsDlg::CSettingsJID::CSettingsJID() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("Identity");
	TEXT1 = _Settings.Translate("Please choose your logon information.");
	sprintf(m_TEXT, "%s%s", _Settings.Translate("This is shown on your friend's list."), _Settings.Translate("You can use your GMAIL account here."));
	TEXT2 = m_TEXT;


	m_Jabber = NULL;
	m_TestSuccess = FALSE;
}

CSettingsDlg::CSettingsJID::~CSettingsJID()
{
	if (m_Jabber)
		delete m_Jabber;
}

LRESULT CSettingsDlg::CSettingsJID::OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_Starting)
		return TRUE;

	switch (wID)
	{
		case IDC_EDIT_JID3:
		case IDC_EDIT_JID4:			
		case IDC_EDIT2_JID:
		case IDC_USESSLWRAPPER:
		case IDC_EDIT_JID:
		case IDC_RESOURCE:
			NeedRestart();
			if (wID == IDC_EDIT_JID)
			{
				char buff[16384];
				memset(buff, 0, 16384);
				::SendMessage(GetDlgItem(IDC_EDIT_JID), WM_GETTEXT, 16384, (LPARAM)buff);
				if (buff[0])
				{
					_strlwr(buff);
					char *a = trim(buff);

					if (!strcmp(a, "newuser@wippien.com"))
						::EnableWindow(GetDlgItem(IDC_TEST_JID), FALSE);
				}
			}
			break;
	}
	return FALSE;
}

LRESULT CSettingsDlg::CSettingsJID::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsJID::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	CComBSTR2 j;
	j = _Settings.m_JID;
	if (!j.Length())
	{
		char buff[8192], buff2[8192];
		memset(buff, 0, sizeof(buff));
		memset(buff2, 0, sizeof(buff2));

		unsigned long size = sizeof(buff);
		GetUserName(buff, &size);
		int l = strlen(buff);
		int k = 0;
		for (int i=0;i<l;i++)
		{
			if (k?isalnum(buff[i]):isalpha(buff[i]))
				buff2[k++]=buff[i];
			else
			if (k && buff[i]==' ')
				buff2[k++]='_';
		}
		if (strlen(buff2))
		{
			srand(GetTickCount());
			sprintf(buff, "%s%d@wippien.com", buff2, rand()%99);
			j = buff;
		}
		else
			j = "newuser@wippien.com";
	}
	SetDlgItemTextW(m_hWnd, IDC_EDIT_JID, j);
	::SetDlgItemTextW(m_hWnd, IDC_EDIT2_JID, _Settings.m_Password);
	
	SendMessage(GetDlgItem(IDC_RESOURCE), CB_ADDSTRING, 0, (LPARAM)_Settings.Translate("Home"));
	SendMessage(GetDlgItem(IDC_RESOURCE), CB_ADDSTRING, 0, (LPARAM)_Settings.Translate("Work"));
	SendMessage(GetDlgItem(IDC_RESOURCE), CB_ADDSTRING, 0, (LPARAM)_Settings.Translate("Office"));
	SendMessage(GetDlgItem(IDC_RESOURCE), CB_ADDSTRING, 0, (LPARAM)_Settings.Translate("Laptop"));
	SendMessage(GetDlgItem(IDC_RESOURCE), CB_ADDSTRING, 0, (LPARAM)_Settings.Translate("Desktop"));
	SendMessage(GetDlgItem(IDC_RESOURCE), CB_ADDSTRING, 0, (LPARAM)WIPPIENIM3);
	if (!_Settings.m_Resource.Length())
	{
		_Settings.m_Resource = "Wippien";
		_Settings.m_StaticResource = "Wippien";
	}

	CComBSTR2 res = _Settings.m_StaticResource;
	SetDlgItemText(IDC_RESOURCE, res.ToString());

	CComBSTR2 hs = _Settings.m_ServerHost;
	char buff[1024];
	sprintf(buff, "%u", _Settings.m_ServerPort);
	SetDlgItemText(IDC_EDIT_JID3, hs.ToString());
	SetDlgItemText(IDC_EDIT_JID4, buff);

	if (_Settings.m_JID.Length())
		::SendMessage(GetDlgItem(IDC_RADIO1_JID), BM_SETCHECK, BST_CHECKED, NULL);
	else
		::SendMessage(GetDlgItem(IDC_RADIO2_JID), BM_SETCHECK, BST_CHECKED, NULL);

	if (_Settings.m_UseSSLWrapper)
		::SendMessage(GetDlgItem(IDC_USESSLWRAPPER), BM_SETCHECK, BST_CHECKED, NULL);


	::EnableWindow(GetDlgItem(IDC_TEST_JID), FALSE);
	::EnableWindow(::GetDlgItem(m_Owner, IDOK), FALSE);
	::EnableWindow(::GetDlgItem(m_Owner, IDC_NEXT), FALSE);

	SetDlgItemText(IDC_LABEL_JID1, _Settings.Translate("You should enter your jabber ID (JID) here. Wippien uses Jabber network for presence - to notify your contacts that you're online, to view other contacts etc."));
	SetDlgItemText(IDC_LABEL_JID2, _Settings.Translate("If you don't have JID, you should register for new one. You can do it on any jabber server you wish. There are many public servers available. You can find list of currently open servers by clicking on this button."));
	SetDlgItemText(IDB_BUTTON_JID1, _Settings.Translate("&Search"));
	SetDlgItemText(IDC_RADIO2_JID, _Settings.Translate("I need new Jabber account"));
	SetDlgItemText(IDC_RADIO1_JID, _Settings.Translate("I have existing Jabber account"));
	SetDlgItemText(IDC_S1, _Settings.Translate("JID"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Password"));
	SetDlgItemText(IDC_S12, _Settings.Translate("Resource"));
	SetDlgItemText(IDC_TEST_JID, _Settings.Translate("&Test Account"));
	SetDlgItemText(IDC_S3, _Settings.Translate("Server"));
	SetDlgItemText(IDC_S4, _Settings.Translate("Port"));
	SetDlgItemText(IDC_USESSLWRAPPER, _Settings.Translate("Use SSL wrapper on port 443 (for GTalk)"));
	SetDlgItemText(IDC_S5, _Settings.Translate("Please click on 'Test Account' to proceed."));
	SetDlgItemText(IDC_S6, _Settings.Translate("You can use your GMAIL account here."));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsJID::Apply(void)
{
	CComBSTR2 cb;
	cb.FromTextBox(GetDlgItem(IDC_EDIT_JID));
	char *buff = cb.ToUTF8String();
	if (buff && buff[0])
	{
		char *a = strchr(buff, '@');
		if (a && *++a)
		{
			_Settings.m_JID = cb;			
		}
		else
		{
			::SetFocus(GetDlgItem(IDC_EDIT_JID));
			MessageBox(_Settings.Translate("JID should contain server name, formatted like user@server.com"), _Settings.Translate("Invalid logon details"), MB_OK);
			return FALSE;
		}
	}
	else
	{
		::SetFocus(GetDlgItem(IDC_EDIT_JID));
		MessageBox(_Settings.Translate("You must enter your JID."), _Settings.Translate("Invalid logon details"), MB_OK);
		return FALSE;
	}			

	cb.Empty();
	cb.FromTextBox(GetDlgItem(IDC_EDIT2_JID));
	buff = cb.ToUTF8String();
	if (buff && buff[0])
		_Settings.m_Password = cb;
	else
	{
		::SetFocus(GetDlgItem(IDC_EDIT2_JID));
		MessageBox(_Settings.Translate("You must enter your password."), _Settings.Translate("Invalid logon details"), MB_OK);

		return FALSE;
	}			

	char bf[16384];
	*bf = 0;
	::SendMessage(GetDlgItem(IDC_RESOURCE), WM_GETTEXT, 16384, (LPARAM)bf);
	if (bf[0])
	{
		char *p = bf;
		while (*p)
		{
			if (!isalnum(*p))
				*p='_';
			p++;
		}
		_Settings.m_Resource = bf;
		_Settings.m_StaticResource = bf;
	}

	*bf = 0;
	::SendMessage(GetDlgItem(IDC_EDIT_JID3), WM_GETTEXT, 16384, (LPARAM)bf);
	if (bf[0])
		_Settings.m_ServerHost = bf;

	*bf = 0;
	::SendMessage(GetDlgItem(IDC_EDIT_JID4), WM_GETTEXT, 16384, (LPARAM)bf);
	if (bf[0])
	{
		_Settings.m_ServerPort = atol(bf);
		if (_Settings.m_ServerPort<=0)
			_Settings.m_ServerPort = 5222;
	}


	if (::SendMessage(GetDlgItem(IDC_USESSLWRAPPER), BM_GETCHECK, NULL, NULL))
		_Settings.m_UseSSLWrapper = TRUE;
	else
		_Settings.m_UseSSLWrapper= 0;

	return TRUE;
}

LRESULT CSettingsDlg::CSettingsJID::OnBtnSearch(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	ShellExecute(NULL, "open", "http://www.jabber.org/im-services", "", "", 0);
	return 0;
}

LRESULT CSettingsDlg::CSettingsJID::OnJIDPassChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	m_TestSuccess = FALSE;
	BOOL cantest = TRUE;
	CComBSTR2 b;
	b.FromTextBox(GetDlgItem(IDC_EDIT_JID));
	char *buff = b.ToUTF8String();
	if (buff && buff[0])
	{
		_strlwr(buff);
		char *a = trim(buff);

		if (!strcmp(a, "newuser@wippien.com"))
			cantest = FALSE;
	}

	::EnableWindow(GetDlgItem(IDC_TEST_JID), cantest?TRUE:FALSE);

	::EnableWindow(::GetDlgItem(m_Owner, IDOK), FALSE);
	::EnableWindow(::GetDlgItem(m_Owner, IDC_NEXT), FALSE);
	return FALSE;
}

LRESULT CSettingsDlg::CSettingsJID::OnJIDChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CComBSTR2 b;
	b.FromTextBox(GetDlgItem(IDC_EDIT_JID));
	char *buff = b.ToUTF8String();
	if (buff && buff[0])
	{
		char *a = strchr(buff, '@');
		if (a)
		{
			a++;
			if (*a)
			{
				if (strcmp(a, "gmail.com"))
					SetDlgItemText(IDC_EDIT_JID3, a);
				else
					SetDlgItemText(IDC_EDIT_JID3, "talk.google.com");
			}
		}
	}
	return OnJIDPassChange(wNotifyCode, wID, hWndCtl, bHandled);
}

LRESULT CSettingsDlg::CSettingsJID::OnBtnTest(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CComBSTR2 bufj, bufp;
	bufj.FromTextBox(GetDlgItem(IDC_EDIT_JID));
	char *at = bufj.ToString();
	bufp.FromTextBox(GetDlgItem(IDC_EDIT2_JID));
	char bufserv[1024], bufport[1024];
	
	if (!strchr(at, '@'))
	{		
		bufj += "@wippien.com";
		::SendMessage(GetDlgItem(IDC_EDIT_JID), WM_SETTEXT, 0, (LPARAM)bufj.ToString());
	}
	::SendMessage(GetDlgItem(IDC_EDIT_JID3), WM_GETTEXT, sizeof(bufserv), (LPARAM)bufserv);
	::SendMessage(GetDlgItem(IDC_EDIT_JID4), WM_GETTEXT, sizeof(bufport), (LPARAM)bufport);

	if (!*bufserv)
	{
		strcpy(bufserv, "wippien.com");
		::SendMessage(GetDlgItem(IDC_EDIT_JID3), WM_SETTEXT, 0, (LPARAM)bufserv);
	}	

	
	int port = atol(bufport);
	if (!port)
		port = 5222;

	BOOL usessl = FALSE;
	if (::SendMessage(GetDlgItem(IDC_USESSLWRAPPER), BM_GETCHECK, NULL, NULL))
		usessl = TRUE;
	
	BOOL regnew = ::SendMessage(GetDlgItem(IDC_RADIO2_JID), BM_GETSTATE, NULL, NULL);

	if (m_Jabber)
	{
		delete m_Jabber;
	}
	m_Jabber = new CJabberWiz(this);
	m_Jabber->Connect(bufj.ToUTF8String(), bufp.ToUTF8String(), bufserv, port, regnew, usessl);
//			GetPropertySheet().SetWizardButtons(PSWIZB_BACK | PSWIZB_NEXT);
	return 0;
}

LRESULT CSettingsDlg::CSettingsJID::OnBtnUseSSLWrapper(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (::SendMessage(GetDlgItem(IDC_USESSLWRAPPER), BM_GETCHECK, NULL, NULL))
		::SendMessage(GetDlgItem(IDC_EDIT_JID4), WM_SETTEXT, 0, (LPARAM)"443");
	else
		::SendMessage(GetDlgItem(IDC_EDIT_JID4), WM_SETTEXT, 0, (LPARAM)"5222");

	::EnableWindow(GetDlgItem(IDC_TEST_JID), FALSE);


	return 0;
}

void CSettingsDlg::CSettingsJID::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsJID::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

#ifndef _WODXMPPLIB

// Define the ftp events to be handled:
_ATL_FUNC_INFO SettingsConnectedInfo = {CC_STDCALL, VT_EMPTY, 0};
_ATL_FUNC_INFO SettingsDisconnectedInfo = {CC_STDCALL, VT_EMPTY, 2, {VT_I4,VT_BSTR}};
_ATL_FUNC_INFO SettingsStateChangeInfo = {CC_STDCALL, VT_EMPTY, 1, {VT_I4}};

class CSettingsDlg::CSettingsJID::CJabberEvents : public IDispEventSimpleImpl<1, CSettingsDlg::CSettingsJID::CJabberEvents, &__uuidof(WODXMPPCOMLib::_IwodXMPPComEvents)>
{
public:
    CJabberEvents (CSettingsDlg::CSettingsJID::CJabberWiz * ppJ)
    {
        m_pJ = ppJ;
        DispEventAdvise ( (IUnknown*)m_pJ->m_Jabb);
    }

    virtual ~CJabberEvents ()
    {
        DispEventUnadvise ( (IUnknown*)m_pJ->m_Jabb);
//        m_pJ->m_Jabb.Release();
    }

#endif

#ifdef _WODXMPPLIB
	void XMPP_SettingsDlgConnected(void *wodXMPP)
	{
		CSettingsDlg::CSettingsJID::CJabberWiz * m_pJ;
		WODXMPPCOMLib::XMPP_GetTag(wodXMPP, (void **)&m_pJ);

#else
    void __stdcall DispConnected ()
    {
#endif

		m_pJ->m_Owner->m_TestSuccess = TRUE;
		::EnableWindow(::GetDlgItem(m_pJ->m_Owner->m_hWnd, IDC_TEST_JID), FALSE);
		::EnableWindow(::GetDlgItem(m_pJ->m_Owner->m_Owner, IDOK), TRUE);
		::EnableWindow(::GetDlgItem(m_pJ->m_Owner->m_Owner, IDC_NEXT), TRUE);
		::MessageBox(NULL, "Success!", "Jabber test", MB_OK);
    }
#ifdef _WODXMPPLIB
	void XMPP_SettingsDlgDisconnected(void *wodXMPP, long ErrorCode, char *ErrorText)
	{
		CSettingsDlg::CSettingsJID::CJabberWiz * m_pJ;
		WODXMPPCOMLib::XMPP_GetTag(wodXMPP, (void **)&m_pJ);		
#else
    void __stdcall DispDisconnected (long ErrorCode, BSTR ErrorText)
    {
#endif
		CComBSTR2 e = ErrorText;
		if (ErrorCode)
			::MessageBox(NULL, e.ToString(), _Settings.Translate("Jabber error"), MB_OK);
    }
#ifdef _WODXMPPLIB
	void XMPP_SettingsDLGStateChange(void *wodXMPP, WODXMPPCOMLib::StatesEnum OldState)
	{
		CSettingsDlg::CSettingsJID::CJabberWiz * m_pJ;
		WODXMPPCOMLib::XMPP_GetTag(wodXMPP, (void **)&m_pJ);
#else
    void __stdcall DispStateChange(WODXMPPCOMLib::StatesEnum OldState)
    {
#endif

#ifndef _WODXMPPLIB
		CComBSTR2 b;
		VARIANT var;
		var.vt = VT_ERROR;
		if (SUCCEEDED(m_pJ->m_Jabb->get_StateText(var, &b)))
			::SetDlgItemText(m_pJ->m_Owner->m_hWnd, IDC_JABBER_CONN_STATUS, b.ToString());
#else
		char bf[10240];
		int bflen = sizeof(bf);
		WODXMPPCOMLib::StatesEnum st;
		WODXMPPCOMLib::XMPP_GetState(wodXMPP, &st);
		WODXMPPCOMLib::XMPP_GetStateText(wodXMPP, st, bf, &bflen);
		::SetDlgItemText(m_pJ->m_Owner->m_hWnd, IDC_JABBER_CONN_STATUS, bf);
#endif
    }
    
#ifndef _WODXMPPLIB
    BEGIN_SINK_MAP (CJabberEvents)
        SINK_ENTRY_INFO (1,__uuidof(WODXMPPCOMLib::_IwodXMPPComEvents),0,DispConnected,&SettingsConnectedInfo)
        SINK_ENTRY_INFO (1,__uuidof(WODXMPPCOMLib::_IwodXMPPComEvents),1,DispDisconnected,&SettingsDisconnectedInfo)
        SINK_ENTRY_INFO (1,__uuidof(WODXMPPCOMLib::_IwodXMPPComEvents),2,DispStateChange,&SettingsStateChangeInfo)
        END_SINK_MAP ()
private:
	CJabberWiz * m_pJ;
};
#endif

CSettingsDlg::CSettingsJID::CJabberWiz::CJabberWiz(CSettingsDlg::CSettingsJID *Owner)
{
#ifndef _WODXMPPLIB
	m_Jabb.CoCreateInstance(__uuidof(WODXMPPCOMLib::wodXMPPCom));
	m_Events = new CJabberEvents(this);
#else
	memset(&m_Events, 0, sizeof(m_Events));
	m_Events.Connected = XMPP_SettingsDlgConnected;
	m_Events.Disconnected = XMPP_SettingsDlgDisconnected;
	m_Events.StateChange = XMPP_SettingsDLGStateChange;
	m_Jabb = WODXMPPCOMLib::_XMPP_Create(&m_Events);
	WODXMPPCOMLib::XMPP_SetTag(m_Jabb, (void *)this);
#endif


#ifdef WODXMPP_LICENSE_KEY
	CComBSTR blic(WODXMPP_LICENSE_KEY);
	m_Jabb->put_LicenseKey(blic);
#endif
	m_Owner = Owner;
}

CSettingsDlg::CSettingsJID::CJabberWiz::~CJabberWiz()
{
#ifndef _WODXMPPLIB
	m_Jabb->Disconnect();
	delete m_Events;
#else
	WODXMPPCOMLib::XMPP_Disconnect(m_Jabb);
#endif
}

void CSettingsDlg::CSettingsJID::CJabberWiz::Connect(char *JID, char *pass, char *hostname, int port, BOOL registernew, BOOL usessl)
{
	char *a = strchr(JID, '/');
	if (a)
		*a = 0;

	CComBSTR2 _l;
	_l.FromUTF8String(JID, strlen(JID));
	CComBSTR l = _l ,p = pass, h = hostname;
	l += "/WippienTest";

	VARIANT var;
	if (h.Length())
	{
		var.vt = VT_BSTR;
		var.bstrVal = h;
	}
	else
		var.vt = VT_ERROR;
	
#ifndef _WODXMPPLIB
	m_Jabb->put_Login(l);
	m_Jabb->put_Password(p);
	if (port)
		m_Jabb->put_Port(port);

	m_Jabb->put_AutoVisible(VARIANT_FALSE);

	if (registernew)
		m_Jabb->put_Register(VARIANT_TRUE);

	if (usessl)
		m_Jabb->put_Security((WODXMPPCOMLib::SecurityEnum)3);
	else
		m_Jabb->put_Security((WODXMPPCOMLib::SecurityEnum)1);

	m_Jabb->put_DebugFile(_Settings.m_JabberDebugFile);

	try
	{
		m_Jabb->Connect(var);
	}
	catch (_com_error e)
	{		
		CComBSTR2 b;
		m_Jabb->get_LastErrorText(&b);
		::MessageBox(NULL, b.ToString(), _Settings.Translate("Jabber error"), MB_OK);
	}

#else
	CComBSTR2 l1 = l;
	WODXMPPCOMLib::XMPP_SetLogin(m_Jabb, l1.ToUTF8String());
	WODXMPPCOMLib::XMPP_SetPassword(m_Jabb, pass);
	if (port)
		WODXMPPCOMLib::XMPP_SetPort(m_Jabb, port);
	if (registernew)
		WODXMPPCOMLib::XMPP_SetRegister(m_Jabb, TRUE);
	WODXMPPCOMLib::XMPP_SetAutoVisible(m_Jabb, FALSE);

	if (usessl)
		WODXMPPCOMLib::XMPP_SetSecurity(m_Jabb, (WODXMPPCOMLib::SecurityEnum)3); // security implicit
	else
		WODXMPPCOMLib::XMPP_SetSecurity(m_Jabb, (WODXMPPCOMLib::SecurityEnum)1); // security allowed

	CComBSTR2 df = _Settings.m_JabberDebugFile;
	WODXMPPCOMLib::XMPP_SetDebugFile(m_Jabb, df.ToString());
	
	long hr = WODXMPPCOMLib::XMPP_Connect(m_Jabb, hostname);
	if (hr)
	{
		char buff[1024];
		int bflen = sizeof(buff);
		WODXMPPCOMLib::XMPP_GetLastErrorText(m_Jabb, buff, &bflen);
		::MessageBox(NULL, buff, _Settings.Translate("Jabber error"), MB_OK);
	}
#endif
}

void CSettingsDlg::CSettingsJID::CJabberWiz::Disconnect(void)
{
#ifndef _WODXMPPLIB
	m_Jabb->Disconnect();
#else
	WODXMPPCOMLib::XMPP_Disconnect(m_Jabb);
#endif
}

CSettingsDlg::CSettingsIcon::CSettingsIcon() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Identity");
	mp += "\\";
	mp += _Settings.Translate("Icon");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Please choose your icon.");
	TEXT2 = _Settings.Translate("This is usually visible to your contacts.");

	SelectedImage = NULL;
	m_WasShown = FALSE;
	_LoadIconFromResource(&m_BaseImage, "PNG", CXIMAGE_FORMAT_PNG, ID_PNG_AVATARBASE);
}

CSettingsDlg::CSettingsIcon::~CSettingsIcon()
{
	while (m_Image.size())
	{
		CxImage *img = m_Image[0];
		delete img;
		m_Image.erase(m_Image.begin());
	}
}

#define IMGSIZE	64

int FindCxImageFormat(const CString& ext)
{
		int type = 0;
		if (ext == "bmp")					type = CXIMAGE_FORMAT_BMP;
#if CXIMAGE_SUPPORT_JPG
		else if (ext=="jpg"||ext=="jpeg")	type = CXIMAGE_FORMAT_JPG;
#endif
#if CXIMAGE_SUPPORT_GIF
		else if (ext == "gif")				type = CXIMAGE_FORMAT_GIF;
#endif
#if CXIMAGE_SUPPORT_PNG
		else if (ext == "png")				type = CXIMAGE_FORMAT_PNG;
#endif
#if CXIMAGE_SUPPORT_MNG
		else if (ext=="mng"||ext=="jng")	type = CXIMAGE_FORMAT_MNG;
#endif
#if CXIMAGE_SUPPORT_ICO
		else if (ext == "ico")				type = CXIMAGE_FORMAT_ICO;
#endif
#if CXIMAGE_SUPPORT_TIF
		else if (ext=="tiff"||ext=="tif")	type = CXIMAGE_FORMAT_TIF;
#endif
#if CXIMAGE_SUPPORT_TGA
		else if (ext=="tga")				type = CXIMAGE_FORMAT_TGA;
#endif
#if CXIMAGE_SUPPORT_PCX
		else if (ext=="pcx")				type = CXIMAGE_FORMAT_PCX;
#endif
#if CXIMAGE_SUPPORT_WBMP
		else if (ext=="wbmp")				type = CXIMAGE_FORMAT_WBMP;
#endif
#if CXIMAGE_SUPPORT_WMF
		else if (ext=="wmf"||ext=="emf")	type = CXIMAGE_FORMAT_WMF;
#endif
#if CXIMAGE_SUPPORT_J2K
		else if (ext=="j2k"||ext=="jp2")	type = CXIMAGE_FORMAT_J2K;
#endif
#if CXIMAGE_SUPPORT_JBG
		else if (ext=="jbg")				type = CXIMAGE_FORMAT_JBG;
#endif
#if CXIMAGE_SUPPORT_JP2
		else if (ext=="jp2"||ext=="j2k")	type = CXIMAGE_FORMAT_JP2;
#endif
#if CXIMAGE_SUPPORT_JPC
		else if (ext=="jpc"||ext=="j2c")	type = CXIMAGE_FORMAT_JPC;
#endif
#if CXIMAGE_SUPPORT_PGX
		else if (ext=="pgx")				type = CXIMAGE_FORMAT_PGX;
#endif
#if CXIMAGE_SUPPORT_RAS
		else if (ext=="ras")				type = CXIMAGE_FORMAT_RAS;
#endif
#if CXIMAGE_SUPPORT_PNM
		else if (ext=="pnm"||ext=="pgm"||ext=="ppm") type = CXIMAGE_FORMAT_PNM;
#endif
		else type = CXIMAGE_FORMAT_UNKNOWN;

	return type;
}

LRESULT CSettingsDlg::CSettingsIcon::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	DrawNickIcon(dcPaint);
	return TRUE;
}

void _CalcRect(int imgwidth, int imgheight, int rcwidth, int rcheight, long *xpos, long *ypos, long *xwidth, long *xheight, int imgsizewidth, int imgsizeheight)
{
	*xpos = (rcwidth - imgwidth)/2;
	if (*xpos<0) *xpos = 0;
	*ypos = (rcheight - imgheight)/2;
	if (*ypos<0) *ypos = 0;

	*xwidth = (imgwidth>imgsizewidth?imgsizewidth:imgwidth);
	*xheight = (imgheight>imgsizeheight?imgsizeheight:imgheight);

}

void _CalcRect(int imgwidth, int imgheight, int rcwidth, int rcheight, long *xpos, long *ypos, long *xwidth, long *xheight, int imgsize)
{
	_CalcRect(imgwidth, imgheight, rcwidth, rcheight, xpos, ypos, xwidth, xheight, imgsize, imgsize);
}
void CalcRect(int imgwidth, int imgheight, int rcwidth, int rcheight, long *xpos, long *ypos, long *xwidth, long *xheight)
{
	_CalcRect(imgwidth, imgheight, rcwidth, rcheight, xpos, ypos, xwidth, xheight, IMGSIZE);
}


void CSettingsDlg::CSettingsIcon::DrawNickIcon(HDC dc)
{
	if (SelectedImage)
	{

		RECT rc, rci;
		::GetWindowRect(GetDlgItem(IDC_CHOOSEICON), &rci);

		rc.left = 5;
		rc.right = rc.left+IMGSIZE;
		rc.top = 270;
		rc.bottom = rc.top+IMGSIZE;
		::FillRect(dc, &rc, CreateSolidBrush(GetSysColor(COLOR_3DFACE)));
		CalcRect(SelectedImage->head.biWidth, SelectedImage->head.biHeight, IMGSIZE, IMGSIZE, &rci.left, &rci.top, &rci.right, &rci.bottom);
		SelectedImage->Draw(dc, rc.left + rci.left, rc.top + rci.top, rci.right, rci.bottom, NULL, FALSE);
	}
}
LRESULT CSettingsDlg::CSettingsIcon::OnNickIcon(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	int i = m_IconList.GetCurSel();
	if (i>=0 && i<(signed)m_Image.size())
	{
		m_WasShown = TRUE;
		SelectedImage = m_Image[i];
		HDC dc = GetDC();
		DrawNickIcon(dc);
		ReleaseDC(dc);
		VCardChanged();
	}
	return TRUE;
}
LRESULT CSettingsDlg::CSettingsIcon::OnAddNewCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char *szFilter = _Settings.Translate("All image files (*.bmp;*.jpg;*.jpeg;*.png;*.gif)\0*.bmp;*.jpg;*.jpeg;*.gif;*.png\0Windows bitmaps (*.bmp)\0*.bmp\0Jpeg files (*.jpg,*.jpeg)\0*.jpeg;*.jpg\0GIF files (*.gif)\0*.gif\0PNG files (*.png)\0*.png\0All Files (*.*)\0*\0\0");
	CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_ENABLESIZING  | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, 
		NULL, m_hWnd);

	cf.m_ofn.lpstrFilter = szFilter;
//		cf.m_ofn.lpstrDefExt = "*.bmp;*.jpg;*.jpeg;*.gif;*.png";

	if (cf.DoModal() == IDOK)
	{
		int i = strlen(cf.m_szFileName);
		while (i>0 && cf.m_szFileName[i] !='.') i--;
		i++;
		CxImage *image = new CxImage();
		if (image->Load(cf.m_szFileName, FindCxImageFormat(&cf.m_szFileName[i])))
		{
			m_IconList.InsertString(m_Image.size(), "");
			m_Image.push_back(image);
			m_IconList.SetCurSel(m_IconList.GetCount()-1);
			BOOL bh;
			OnNickIcon(0,0,0,bh);
		}
		else
			delete image;
	}
	return 0;
}

LRESULT CSettingsDlg::CSettingsIcon::OnMeasureItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPMEASUREITEMSTRUCT lp = (LPMEASUREITEMSTRUCT)lParam;
	lp->itemWidth = IMGSIZE+2;
	lp->itemHeight = IMGSIZE+2;
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsIcon::OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPDRAWITEMSTRUCT lp = (LPDRAWITEMSTRUCT)lParam;

	HBRUSH hb = (HBRUSH)CreateSolidBrush(RGB(255,255,255));
	::FillRect(lp->hDC, &lp->rcItem, hb);
	DeleteObject(hb);

	if (lp->itemState & ODS_SELECTED)
	{
		m_BaseImage.Draw(lp->hDC, lp->rcItem.left , lp->rcItem.top , IMGSIZE+2, IMGSIZE+2, NULL, FALSE);
	}

	if (lp->itemID >= m_Image.size())
		return FALSE;
	
	long xpos, ypos, xwidth, xheight;
	CalcRect(m_Image[lp->itemID]->head.biWidth, m_Image[lp->itemID]->head.biHeight, 
		lp->rcItem.right - lp->rcItem.left, lp->rcItem.bottom - lp->rcItem.top,
		&xpos, &ypos, &xwidth, &xheight);
	
	m_Image[lp->itemID]->Draw(lp->hDC, lp->rcItem.left + xpos + 1, lp->rcItem.top + ypos + 1, xwidth, xheight, NULL, FALSE);
	return TRUE;
}

void CSettingsDlg::CSettingsIcon::AddImage(int ImageId)
{

	CxImage *image = new CxImage();
	if (_LoadIconFromResource(image, "PNG", CXIMAGE_FORMAT_PNG, ImageId))
	{
		m_IconList.InsertString(m_Image.size(), "");
		m_Image.push_back(image);
	}
	else
		delete image;
}

LRESULT CSettingsDlg::CSettingsIcon::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_IconList.Attach(GetDlgItem(IDC_CHOOSEICON));

	int i;
	for (i=IDB_HUMAN1;i<=IDB_HUMAN61;i++)
		AddImage(i);
				
	if (_Settings.m_Icon.Len())
	{
		StaticImage.Decode((unsigned char *)_Settings.m_Icon.Ptr(), _Settings.m_Icon.Len(), CXIMAGE_FORMAT_PNG);			
		SelectedImage = &StaticImage;
	}
	SetDlgItemText(IDC_ADDNEW, _Settings.Translate("Add &New"));
	return TRUE;
}


BOOL CSettingsDlg::CSettingsIcon::Apply(void)
{
	if (SelectedImage)
	{
		CxFile *fTmp;
		CxMemFile *fMem = new CxMemFile(NULL, 0);
		fMem->Open();
		fTmp = fMem;
//		SelectedImage->Resample(48, 48, 2, NULL);
		ResampleImageIfNeeded(SelectedImage, 58);

		SelectedImage->Encode(fTmp, CXIMAGE_FORMAT_PNG);
		_Settings.m_Icon.Clear();
		_Settings.m_Icon.Append((char *)fMem->GetBuffer(), fMem->Size());


		// create our own image
		if (_Settings.m_Icon.Len())
		{
			_MainDlg.m_UserImage.Decode((unsigned char *)_Settings.m_Icon.Ptr(), _Settings.m_Icon.Len(), CXIMAGE_FORMAT_PNG);
			if (_MainDlg.IsWindow())
				_MainDlg.Invalidate();
		}
			
	}
	return TRUE;
}
void CSettingsDlg::CSettingsIcon::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsIcon::DoDefault(void)
{
	if (!m_WasShown && !_Settings.m_Icon.Len())
	{
		srand( (unsigned)time( NULL ) );
		SelectedImage = m_Image[rand() % m_Image.size()];
		Apply();
	}
}

void CSettingsDlg::CSettingsIcon::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}

}

CSettingsDlg::CSettingsEthernet::CSettingsEthernet() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("Network");
	TEXT1 = _Settings.Translate("Please choose your IP address and netmask.");
	TEXT2 = _Settings.Translate("This defines network range to be used by wippien.");

	m_WasShown = FALSE;
}

CSettingsDlg::CSettingsEthernet::~CSettingsEthernet()
{

}

LRESULT CSettingsDlg::CSettingsEthernet::OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_Starting)
		return TRUE;

//	switch (wID)
//	{
//		case IDC_NETWORK_ETHERNET:
//		case IDC_NETMASK_ETHERNET:
//		case IDC_UDP_PORT:
//			NeedRestart();
//			break;
//
//	}
	return FALSE;
}

void CSettingsDlg::CSettingsEthernet::DoDefault(void)
{
	BOOL bh = FALSE;
	Apply();
}

LRESULT CSettingsDlg::CSettingsEthernet::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}



LRESULT CSettingsDlg::CSettingsEthernet::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (_Settings.m_MyLastNetwork)
		SendDlgItemMessage(IDC_ETHERNET_IP, IPM_SETADDRESS, 0, ntohl(_Settings.m_MyLastNetwork));

	if (_Settings.m_MyLastNetmask)
		SendDlgItemMessage(IDC_NETMASK_ETHERNET, IPM_SETADDRESS, 0, ntohl(_Settings.m_MyLastNetmask));
	else
		SendDlgItemMessage(IDC_NETMASK_ETHERNET, IPM_SETADDRESS, 0, 4294967040L);

	if (_Settings.m_Gateway)
		SendDlgItemMessage(IDC_ETHERNET_GATEWAY, IPM_SETADDRESS, 0, ntohl(_Settings.m_Gateway));


	if (_Settings.m_UseGateway)
		::SendMessage(GetDlgItem(IDC_GATEWAY), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_GATEWAY), BM_SETCHECK, FALSE, NULL);


	if (_Settings.m_ObtainIPAddress == 0)
		::SendMessage(GetDlgItem(IDC_IPADDRESSIGNORE), BM_SETCHECK, BST_CHECKED, NULL);
	else
	if (_Settings.m_ObtainIPAddress == 1)
		::SendMessage(GetDlgItem(IDC_IPADDRESSBYIPMEDIATOR), BM_SETCHECK, BST_CHECKED, NULL);
	else
	if (_Settings.m_ObtainIPAddress == 2)
		::SendMessage(GetDlgItem(IDC_IPADDRESSSTATIC), BM_SETCHECK, BST_CHECKED, NULL);

//	CComBSTR2 m = _Settings
	CComBSTR2 m = _Settings.m_IPProviderURL;
	SetDlgItemText(IDC_IPOBTAINURL, m.ToString());

//	char buff[1024];
//	itoa(_Settings.m_UDPPort, buff, 10);
//	SetDlgItemText(IDC_UDP_PORT, buff);

	if (_Settings.m_DoNotShow[DONOTSHOW_NOETHERNET] == '1')
		::SendMessage(GetDlgItem(IDC_CHECK_ETHERNET), BM_SETCHECK, FALSE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CHECK_ETHERNET), BM_SETCHECK, TRUE, NULL);

	if (_Settings.m_DisconnectEthernetOnExit)
		::SendMessage(GetDlgItem(IDC_DISCONNECT_ETHERNET), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_DISCONNECT_ETHERNET), BM_SETCHECK, FALSE, NULL);

	SetDlgItemText(IDC_CHECK_ETHERNET, _Settings.Translate("Show notification if ethernet adapter is unavailable."));
	SetDlgItemText(IDC_DISCONNECT_ETHERNET, _Settings.Translate("Disconnect adapter on exit"));
	SetDlgItemText(IDC_GATEWAY, _Settings.Translate("Gateway"));
	SetDlgItemText(IDC_S1, _Settings.Translate("UDP port fort for outoing and incoming packets (0 for auto select)"));
	SetDlgItemText(IDC_S2, _Settings.Translate("IP address for Wippien network adapter"));
	SetDlgItemText(IDC_IPADDRESSIGNORE, _Settings.Translate("Ignore. IP will be set outside of this program."));
	SetDlgItemText(IDC_IPADDRESSBYIPMEDIATOR, _Settings.Translate("Obtained from IP provider database, on URL"));
	SetDlgItemText(IDC_IPADDRESSSTATIC, _Settings.Translate("Use following static address"));
	SetDlgItemText(IDC_S3, _Settings.Translate("IP address"));
	SetDlgItemText(IDC_S4, _Settings.Translate("Netmask"));
	return TRUE;
}


BOOL CSettingsDlg::CSettingsEthernet::Apply(void)
{
	BOOL changed = FALSE;

	unsigned long mip;
	SendDlgItemMessage(IDC_ETHERNET_IP, IPM_GETADDRESS, 0, (LPARAM)&mip);
	if (mip)
	{
		// no erro is reported otherwise
		if (htonl(mip) != _Settings.m_MyLastNetwork)
			changed = TRUE;
		_Settings.m_MyLastNetwork = htonl(mip);
	}


	BOOL usegate = FALSE;
	if (::SendMessage(GetDlgItem(IDC_GATEWAY), BM_GETSTATE, NULL, NULL))
		usegate = TRUE;
	else
		usegate = FALSE;
	SendDlgItemMessage(IDC_NETMASK_ETHERNET, IPM_GETADDRESS, 0, (LPARAM)&mip);

	if (htonl(mip) != _Settings.m_MyLastNetmask || usegate != _Settings.m_UseGateway)
		changed = TRUE;
	_Settings.m_MyLastNetmask = htonl(mip);
	_Settings.m_UseGateway = usegate;

	SendDlgItemMessage(IDC_ETHERNET_GATEWAY, IPM_GETADDRESS, 0, (LPARAM)&mip);
	
	if (htonl(mip) != _Settings.m_Gateway)
		changed = TRUE;
	_Settings.m_Gateway = htonl(mip);
	
	int oldobtain = _Settings.m_ObtainIPAddress;
	if (::SendMessage(GetDlgItem(IDC_IPADDRESSIGNORE), BM_GETSTATE, NULL, NULL))
		_Settings.m_ObtainIPAddress = 0;
	else	
	if (::SendMessage(GetDlgItem(IDC_IPADDRESSBYIPMEDIATOR), BM_GETSTATE, NULL, NULL))
		_Settings.m_ObtainIPAddress = 1;
	else
	if (::SendMessage(GetDlgItem(IDC_IPADDRESSSTATIC), BM_GETSTATE, NULL, NULL))
		_Settings.m_ObtainIPAddress = 2;
	if (_Settings.m_ObtainIPAddress != oldobtain)
		changed = TRUE;

	if (::SendMessage(GetDlgItem(IDC_CHECK_ETHERNET), BM_GETSTATE, NULL, NULL))
		_Settings.m_DoNotShow[DONOTSHOW_NOETHERNET] = '0';
	else
		_Settings.m_DoNotShow[DONOTSHOW_NOETHERNET] = '1';


	if (::SendMessage(GetDlgItem(IDC_DISCONNECT_ETHERNET), BM_GETSTATE, NULL, NULL))
		_Settings.m_DisconnectEthernetOnExit = TRUE;
	else
		_Settings.m_DisconnectEthernetOnExit = FALSE;

	if (::SendMessage(GetDlgItem(IDC_GATEWAY), BM_GETSTATE, NULL, NULL))
		_Settings.m_UseGateway = TRUE;
	else
		_Settings.m_UseGateway = FALSE;


	char buff[1024];
//	if (::SendMessage(GetDlgItem(IDC_UDP_PORT), WM_GETTEXT, 1024, (LPARAM)buff))
//	{
//		_Settings.m_UDPPort = atoi(buff);
//	}
	
	if (::SendMessage(GetDlgItem(IDC_IPOBTAINURL), WM_GETTEXT, 1024, (LPARAM)buff))
	{
		_Settings.m_IPProviderURL = buff;
	}
	

	if (changed)
	{
		unsigned long ip = _Settings.m_MyLastNetwork;
		_Settings.m_MyLastNetwork = 0;
		_Ethernet.Start(ip, _Settings.m_MyLastNetmask);
	}	
	return TRUE;
}
void CSettingsDlg::CSettingsEthernet::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsEthernet::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			m_WasShown = TRUE;
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
		}
		else
			ShowWindow(SW_HIDE);
	}
}
CSettingsDlg::CSettingsMediator::CSettingsMediator() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Network");
	mp += "\\";
	mp += _Settings.Translate("Mediator");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Please choose who will be your mediator.");
	TEXT2 = _Settings.Translate("This defines people that mediate P2P connections.");
}

CSettingsDlg::CSettingsMediator::~CSettingsMediator()
{

}

LRESULT CSettingsDlg::CSettingsMediator::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsMediator::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

	if (_Settings.m_AllowAnyMediator)
		::SendMessage(GetDlgItem(IDC_CHECK_ALLOWANYMEDIATOR), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CHECK_ALLOWANYMEDIATOR), BM_SETCHECK, FALSE, NULL);

	m_MediatorList.Attach(GetDlgItem(IDC_MEDIATORLIST));
	m_ImageList.Create(16,16, ILC_COLOR24|ILC_MASK, 2, 0);
	m_ImageList.AddIcon(LoadIcon(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDI_ICON_NO)));
	m_ImageList.AddIcon(LoadIcon(_Module.GetModuleInstance(), MAKEINTRESOURCE(IDI_ICON_YES)));
	m_MediatorList.SendMessage(LVM_SETIMAGELIST , LVSIL_NORMAL , (LPARAM)(HIMAGELIST)m_ImageList); 
	m_MediatorList.SendMessage(LVM_SETIMAGELIST , LVSIL_SMALL , (LPARAM)(HIMAGELIST)m_ImageList); 


	SendDlgItemMessage(IDC_MEDIATORLIST,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_BORDERSELECT | LVS_EX_FULLROWSELECT); 
	LV_COLUMN lvcol = {0};
	lvcol.mask=LVCF_TEXT | LVCF_WIDTH;
	lvcol.pszText=_Settings.Translate("Last check");
	lvcol.cchTextMax = sizeof(lvcol.pszText);
	lvcol.cx = 110;	
	SendDlgItemMessage(IDC_MEDIATORLIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 
	lvcol.pszText=_Settings.Translate("Port");
	lvcol.cchTextMax = sizeof(lvcol.pszText);
	lvcol.cx = 60;	
	SendDlgItemMessage(IDC_MEDIATORLIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 
	lvcol.pszText = _Settings.Translate("Hostname");
	lvcol.cchTextMax = sizeof(lvcol.pszText);
	lvcol.cx = 160;
	SendDlgItemMessage(IDC_MEDIATORLIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 
	lvcol.pszText = "";
	lvcol.cchTextMax = sizeof(lvcol.pszText);
	lvcol.cx = 20;
	SendDlgItemMessage(IDC_MEDIATORLIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 


	for (int i=0;i<(signed)_Settings.m_LinkMediators.size();i++)
	{
		CSettings::LinkMediatorStruct *st = (CSettings::LinkMediatorStruct *)_Settings.m_LinkMediators[i];

		LVITEM it = {0};
		it.mask = LVIF_IMAGE;
		it.iItem = i;
		if (st->Permanent)
			it.iImage = 1;
		else
			it.iImage = 0;
		it.iItem = SendMessage(GetDlgItem(IDC_MEDIATORLIST), LVM_INSERTITEM, 0, (LPARAM)&it);

		it.mask = LVIF_TEXT;
		it.iSubItem = 1;
		it.pszText = st->Host;
		it.cchTextMax = strlen(it.pszText);
		SendMessage(GetDlgItem(IDC_MEDIATORLIST), LVM_SETITEM, 0, (LPARAM)&it);

		char buff[1024];
		sprintf(buff, "%d", st->Port);
		it.iSubItem = 2;
		it.pszText = buff;
		it.cchTextMax = strlen(it.pszText);
					
		SendMessage(GetDlgItem(IDC_MEDIATORLIST), LVM_SETITEM, 0, (LPARAM)&it);

		it.iSubItem = 3;
		if (st->Permanent)
			it.pszText = _Settings.Translate("Permanent");
		else
			it.pszText = _Settings.Translate("Temporary");
		it.cchTextMax = strlen(it.pszText);
		
		SendMessage(GetDlgItem(IDC_MEDIATORLIST), LVM_SETITEM, 0, (LPARAM)&it);	
	}


	SetDlgItemText(IDC_S1, _Settings.Translate("Following mediators can be used"));
	SetDlgItemText(IDC_CHECK_ALLOWANYMEDIATOR, _Settings.Translate("Allow new mediators to be added"));

	return TRUE;
}

BOOL CSettingsDlg::CSettingsMediator::Apply(void)
{
	if (::SendMessage(GetDlgItem(IDC_CHECK_ALLOWANYMEDIATOR), BM_GETSTATE, NULL, NULL))
		_Settings.m_AllowAnyMediator = TRUE;
	else
		_Settings.m_AllowAnyMediator = FALSE;
	
	return TRUE;
}
void CSettingsDlg::CSettingsMediator::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsMediator::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}


CSettingsDlg::CSettingsAccounts::CSettingsAccounts() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Identity");
	mp += "\\";
	mp += _Settings.Translate("Accounts");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Enter all your accounts here.");
	TEXT2 = _Settings.Translate("You can add any of accounts supported by your Jabber server (typically ICQ, MSN, etc..)");

	AccRegister = 0;
	AccRegisters[0] = 0;

	// icq
	AccRegisters[1] = "https://www.icq.com/register/";

	// aim
	AccRegisters[2] = "https://my.screenname.aol.com/_cqr/login/login.psp?sitedomain=my.screenname.aol.com&authLev=2&siteState=&lang=en&locale=us&createSn=1";


	// msn
	AccRegisters[3] = "http://messenger.msn.com/download/getstarted.aspx";


	// yahoo
	AccRegisters[4] = "https://edit.yahoo.com/config/eval_register?.done=http://ca.messenger.yahoo.com&.src=pg&.intl=ca";

	_Jabber->m_ServiceRegisterHwnd = NULL;
	_Jabber->m_ServiceRefreshHwnd = NULL;

	m_RegVisible = FALSE;
}

CSettingsDlg::CSettingsAccounts::~CSettingsAccounts() 
{

}

LRESULT CSettingsDlg::CSettingsAccounts::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsAccounts::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_ServiceName.Empty();
	m_ServicesList.Attach(GetDlgItem(IDC_LISTREGISTEREDGATEWAYS));

	RefreshGatewaysList();

	SetDlgItemText(IDC_S1, _Settings.Translate("Your current Instant messaging connections"));
	SetDlgItemText(IDC_ADDNEWACCOUNT, _Settings.Translate("Add new account"));
	SetDlgItemText(IDC_REMOVEACCOUNT, _Settings.Translate("Remove account"));
	SetDlgItemText(IDC_OPENNEWACCOUNT, _Settings.Translate("Open new account"));
	SetDlgItemText(IDC_STATIC_USERNAME, _Settings.Translate("Username:"));
	SetDlgItemText(IDC_STATIC_PASSWORD, _Settings.Translate("Password:"));
	SetDlgItemText(IDC_ACCOUNT_REGISTER, _Settings.Translate("Register!"));

	return TRUE;
}
int CSettingsDlg::CSettingsAccounts::GetServiceType(char *t)
{
	if (strstr(t, "icq"))
		return IDB_SERVICE_ICQ;
	else
	if (strstr(t, "aim"))
		return IDB_SERVICE_AIM;
	else
	if (strstr(t, "yahoo"))
		return IDB_SERVICE_YAHOO;
	else
	if (strstr(t, "msn"))
		return IDB_SERVICE_MSN;
	else
	if (strstr(t, "messenger"))
		return IDB_SERVICE_MSN;
	else
	if (strstr(t, "google"))
		return IDB_SERVICE_GOOGLE;
	else
	if (strstr(t, "gmail"))
		return IDB_SERVICE_GOOGLE;

	return 0;
}

LRESULT CSettingsDlg::CSettingsAccounts::OnRefresh(UINT /*uMsg*/, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	RefreshGatewaysList();
	return TRUE;
}
void CSettingsDlg::CSettingsAccounts::RefreshGatewaysList(void)
{
	char buff[1024];

	m_ServicesList.ResetContent();

#ifndef _WODXMPPLIB
	WODXMPPCOMLib::IXMPPServices *serv = NULL;
	if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
	{
		short count;
		if (SUCCEEDED(serv->get_Count(&count)))
		{
			for (int i=0;i<count;i++)
			{
				WODXMPPCOMLib::IXMPPService *s = NULL;
				VARIANT var;
				var.vt = VT_I2;
				var.iVal = i;
				if (SUCCEEDED(serv->get_Item(var, &s)))
				{
#else
	short count = 0;
	WODXMPPCOMLib::XMPP_ServicesGetCount(_Jabber->m_Jabb, &count);
	for (int i=0;i<count;i++)
	{
		void *s = NULL;
		WODXMPPCOMLib::XMPP_ServicesGetService(_Jabber->m_Jabb, i, &s);
		if (s)
		{
			{
				{
#endif

					
					short ct = 0;
#ifndef _WODXMPPLIB
					WODXMPPCOMLib::IXMPPIdentities *idents;
					if (SUCCEEDED(s->get_Identity(&idents)))
					{
						if (SUCCEEDED(idents->get_Count(&ct)))
						{
#else
					WODXMPPCOMLib::XMPP_Service_GetIdentitiesCount(s, &ct);
					{
						{
#endif

							VARIANT_BOOL bl;
#ifndef _WODXMPPLIB
							if (SUCCEEDED(s->get_Registered(&bl)))
#else
							WODXMPPCOMLib::XMPP_Service_GetRegistered(s, &bl);
#endif
							{
								if (bl)
								{
									for (int c=0;c<ct;c++)
									{
#ifndef _WODXMPPLIB
										WODXMPPCOMLib::IXMPPIdentity *idty;
										var.iVal = c;
										if (SUCCEEDED(idents->get_Item(var, &idty)))
										{
											WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
											idty->get_Category(&id);
											if (!c)
											{			
#else
										char idbuf[1024];
										int idlen = sizeof(idbuf);
										void *idty = NULL;
										WODXMPPCOMLib::XMPP_Service_GetIdentity(s, c, &idty);
										if (idty)
										{
											WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
											WODXMPPCOMLib::XMPP_Identity_GetCategory(idty, &id);
											{
#endif												
												// get name
												CComBSTR2 type, n;

#ifndef _WODXMPPLIB
												idty->get_Name(&n);
												idty->get_Type(&type);
												char *t = type.ToString();
												strlwr(t);
												idty->Release();
#else
												WODXMPPCOMLib::XMPP_Identity_GetName(idty, idbuf, &idlen);
												n = idbuf;
												idlen = sizeof(idbuf);
												WODXMPPCOMLib::XMPP_Identity_GetType(idty, idbuf, &idlen);
												_strlwr(idbuf);
												char *t = idbuf;
												WODXMPPCOMLib::XMPP_Identity_Free(idty);
#endif
												CComBSTR2 b;
#ifndef _WODXMPPLIB
												if (SUCCEEDED(s->get_JID(&b)))
#else
												idlen = sizeof(idbuf);
												WODXMPPCOMLib::XMPP_Service_GetJID(s, idbuf, &idlen);
												b = idbuf;
#endif
												{
													char *jd1 = b.ToString();
													char *jd2 = strchr(jd1, '/');
													if (jd2)
														*jd2 = 0;
													sprintf(buff, "%s (%s)", n.ToString(), jd1);
													int j = m_ServicesList.InsertString(-1, buff);
													if (j != LB_ERR)
													{
														m_ServicesList.SetItemData(j, (unsigned long)s);
													}
												}
											}
										}
									}
								}
							}
						}
					}
#ifndef _WODXMPPLIB
					s->Release();
#else
					if (s)
						WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
				}
			}
		}
#ifndef _WODXMPPLIB
		serv->Release();
#endif
	}
}

/*		BOOL CSettingsDlg::CSettingsAccounts::OnQueryCancel() {
	if (MessageBox(DOYOUWANTTOCLOSE, CLOSINGWIZARD, MB_ICONINFORMATION | MB_YESNO | MB_DEFBUTTON2) == IDYES)
		return FALSE;
	else
		return TRUE;
}
*/		

LRESULT CSettingsDlg::CSettingsAccounts::OnOpenNewAccount(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (AccRegister)
		ShellExecute(NULL, "open", AccRegisters[AccRegister], "", "", 0);

	return TRUE;
}
LRESULT CSettingsDlg::CSettingsAccounts::OnRemoveAccount(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	_Jabber->m_ServiceRefreshHwnd = m_hWnd;

	CComBSTR a;
	char buff[1024];

	buff[0] = 0;
	int i = m_ServicesList.GetCurSel();
	if (i>=0)
	{
		void *x = (void *)m_ServicesList.GetItemData(i);
		m_ServicesList.GetText(i, buff);
		if (buff[0])
		{
			char buff2[1024];
			sprintf(buff2, _Settings.Translate("Are you sure you want to delete account '%s'?"), buff);
			int i = MessageBox(buff2, _Settings.Translate("Delete registered account?"), MB_ICONQUESTION | MB_YESNO);
			if (i==6)
			{

#ifndef _WODXMPPLIB
				WODXMPPCOMLib::IXMPPServices *serv = NULL;
				if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
				{
					short count;
					if (SUCCEEDED(serv->get_Count(&count)))
					{
						for (int i=0;i<count;i++)
						{
							WODXMPPCOMLib::IXMPPService *s = NULL;
							VARIANT var;
							var.vt = VT_I2;
							var.iVal = i;
							if (SUCCEEDED(serv->get_Item(var, &s)))
							{
								if (s == x)
								{
									s->Unregister();
									s->Release();
									break;
								}
								s->Release();				
							}
						}
					}
				}
#else
				short count = 0;
				WODXMPPCOMLib::XMPP_ServicesGetCount(_Jabber->m_Jabb, &count);
				for (int i=0;i<count;i++)
				{
					void *s = NULL;
					WODXMPPCOMLib::XMPP_ServicesGetService(_Jabber->m_Jabb, i, &s);
					if (s == x)
					{
						WODXMPPCOMLib::XMPP_Service_Unregister(s);
						WODXMPPCOMLib::XMPP_Service_Free(s);
						break;
					}
					WODXMPPCOMLib::XMPP_Service_Free(s);

				}
#endif
			}
		}
	}
	return TRUE;
}
LRESULT CSettingsDlg::CSettingsAccounts::OnAccountRegister(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_ServiceName.Length())
	{
		// ok, let's register this service
#ifndef _WODXMPPLIB
		WODXMPPCOMLib::IXMPPServices *serv = NULL;
		if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
		{
			VARIANT var;
			var.vt = VT_BSTR;
			var.bstrVal = m_ServiceName;
			WODXMPPCOMLib::IXMPPService *s;
			if (SUCCEEDED(serv->get_Item(var, &s)))
			{
#else
		void *s = NULL;
		WODXMPPCOMLib::XMPP_ServicesGetServiceByJID(_Jabber->m_Jabb, m_ServiceName.ToString(), &s);
		{
			{
#endif
				CComBSTR2 log, pass;
				char buff[1024];
				buff[0]=0;
				GetDlgItemText(IDC_EDIT_SCREENNAME, buff, 1024);
				if (buff[0])
				{
					log = buff;
					buff[0]=0;
					GetDlgItemText(IDC_EDIT_PASSWORD, buff, 1024);
					if (buff[0])
					{
#ifndef _WODXMPPLIB
						pass = buff;
						s->put_Login(log);
						s->put_Password(pass);
						s->Register();
#else
						CComBSTR2 l = log;
						WODXMPPCOMLib::XMPP_Service_SetLogin(s, l.ToString());
						WODXMPPCOMLib::XMPP_Service_SetPassword(s, buff);
						WODXMPPCOMLib::XMPP_Service_Register(s);
#endif

						SetAccRegisterWindowVisibility(SW_HIDE);
					}
				}
#ifndef _WODXMPPLIB
				s->Release();
#else
				WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
			}
#ifndef _WODXMPPLIB
			serv->Release();
#endif
		}				
	}
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsAccounts::OnAddNewAccount(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	SetAccRegisterWindowVisibility(SW_HIDE);
	_Jabber->m_ServiceRefreshHwnd = m_hWnd;
	m_ServiceName.Empty();

#ifndef _WODXMPPLIB
	WODXMPPCOMLib::IXMPPServices *serv = NULL;
	if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
	{
		short count;
		if (SUCCEEDED(serv->get_Count(&count)))
		{
#else
	short count = 0;
	WODXMPPCOMLib::XMPP_ServicesGetCount(_Jabber->m_Jabb, &count);
	{
		{
#endif
			HMENU h = CreateMenu();
			HMENU hpop = CreatePopupMenu(); 
			AppendMenu(h, MF_STRING | MF_POPUP, (UINT) hpop,  _Settings.Translate("Accounts")); 


			CCommandBarCtrlXP m_Menu;
			m_Menu.Create(/*m_Tree.*/m_hWnd, rcDefault, NULL, ATL_SIMPLE_CMDBAR_PANE_STYLE);
			int i;
			for (i=0;i<count;i++)
			{
#ifndef _WODXMPPLIB
				WODXMPPCOMLib::IXMPPService *s = NULL;
				VARIANT var;
				var.vt = VT_I2;
				var.iVal = i;
				if (SUCCEEDED(serv->get_Item(var, &s)))
#else
				void *s = NULL;
				WODXMPPCOMLib::XMPP_ServicesGetService(_Jabber->m_Jabb, i, &s);
#endif
				{
					short ct;
#ifndef _WODXMPPLIB
					WODXMPPCOMLib::IXMPPIdentities *idents;
					if (SUCCEEDED(s->get_Identity(&idents)))
					{
						if (SUCCEEDED(idents->get_Count(&ct)))
						{
#else
					WODXMPPCOMLib::XMPP_Service_GetIdentitiesCount(s, &ct);
					{
						{
#endif
							for (int c=0;c<ct;c++)
							{
#ifndef _WODXMPPLIB
								WODXMPPCOMLib::IXMPPIdentity *idty;
								var.iVal = c;
								if (SUCCEEDED(idents->get_Item(var, &idty)))
								{
									WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
									idty->get_Category(&id);
									if ((m_ShowAllServices && !c) || id == (WODXMPPCOMLib::IdentityCategoryEnum)/*::CatGateway*/9)
									{
										
										// get name
										CComBSTR2 type;
										idty->get_Type(&type);
										char *t = type.ToString();
										strlwr(t);

										idty->Release();

#else					
								WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
								void *idty = NULL;
								WODXMPPCOMLib::XMPP_Service_GetIdentity(s, c, &idty);
								if (idty)
								{
									WODXMPPCOMLib::XMPP_Identity_GetCategory(idty, &id);
									if ((m_ShowAllServices && !c) || id == (WODXMPPCOMLib::IdentityCategoryEnum)/*::CatGateway*/9)
									{
										char t[1024];
										int tlen = sizeof(t);
										WODXMPPCOMLib::XMPP_Identity_GetType(idty, t, &tlen);
										_strlwr(t);

										WODXMPPCOMLib::XMPP_Identity_Free(idty);
#endif

										CComBSTR2 b;
#ifndef _WODXMPPLIB
										if (SUCCEEDED(s->get_JID(&b)))
#else
										char jd[1024];
										tlen = sizeof(jd);
										WODXMPPCOMLib::XMPP_Service_GetJID(s, jd, &tlen);
										b = jd;
#endif
										{
											int val = GetServiceType(t);
											if (val)
												val += 30000;
											else
												val = i;

#ifndef _WODXMPPLIB
											var.iVal = val;
											s->put_Tag(var);
#else
											WODXMPPCOMLib::XMPP_Service_SetTag(s, (void *)val);
#endif

											AppendMenu(hpop, MF_STRING, val, b.ToString());
										}
									}
								}
							}
						}
					}
#ifndef _WODXMPPLIB
					s->Release();
#else
					WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
				}
			}

			// add separator
			AppendMenu(hpop, MF_SEPARATOR, 0, NULL);
			if (m_ShowAllServices)
				AppendMenu(hpop, MF_STRING, 29999, _Settings.Translate("Show only gateways"));
			else
				AppendMenu(hpop, MF_STRING, 29999, _Settings.Translate("Show all services"));

			_MainDlg.m_UserList.AddMenuImage(IDB_SERVICE_ICQ, IDB_SERVICE_ICQ+30000);
			_MainDlg.m_UserList.AddMenuImage(IDB_SERVICE_AIM, IDB_SERVICE_AIM+30000);
			_MainDlg.m_UserList.AddMenuImage(IDB_SERVICE_MSN, IDB_SERVICE_MSN+30000);
			_MainDlg.m_UserList.AddMenuImage(IDB_SERVICE_YAHOO, IDB_SERVICE_YAHOO+30000);


			m_Menu.AttachMenu(h);
			RECT rt;
			::GetWindowRect(GetDlgItem(IDC_ADDNEWACCOUNT), &rt);
			rt.top += 20;
			int sel = m_Menu.TrackPopupMenu(hpop, TPM_LEFTALIGN | TPM_RETURNCMD, rt.left, rt.bottom, 0);

			m_Menu.DestroyWindow();
			DestroyMenu(h);

			if (sel == 29999)
			{
				m_ShowAllServices = !m_ShowAllServices;
				return OnAddNewAccount(wNotifyCode, wID, hWndCtl, bHandled);
			}
			else
			{
			
				for (i=0;i<count;i++)
				{
#ifndef _WODXMPPLIB
					WODXMPPCOMLib::IXMPPService *s = NULL;
					VARIANT var;
					var.vt = VT_I2;
					var.iVal = i;
					if (SUCCEEDED(serv->get_Item(var, &s)))
					{
						s->get_Tag(&var);
						if (var.vt == VT_I2 && var.iVal == sel)
						{
#else
					void *s;
					WODXMPPCOMLib::XMPP_ServicesGetService(_Jabber->m_Jabb, i, &s);
					{
						long v = 0;
						WODXMPPCOMLib::XMPP_Service_GetTag(s, (void **)&v);
						if (v && v == sel)
						{

#endif
							m_ServiceName.Empty();
							_Jabber->m_ServiceRegisterHwnd = GetDlgItem(IDC_SERVICE_DESCRIPTION);

#ifndef _WODXMPPLIB
							s->get_JID(&m_ServiceName);
							s->RegisterInfo();
#else
							char sn[1024];
							int snlen = sizeof(sn);
							WODXMPPCOMLib::XMPP_Service_GetJID(s, sn, &snlen);
							m_ServiceName = sn;
							WODXMPPCOMLib::XMPP_Service_RegisterInfo(s);
#endif
							
							sel -= 30000;
							if (sel == IDB_SERVICE_ICQ)
								AccRegister = 1;
							else
							if (sel == IDB_SERVICE_AIM)
								AccRegister = 2;
							else
							if (sel == IDB_SERVICE_MSN)
								AccRegister = 3;
							else
							if (sel == IDB_SERVICE_YAHOO)
								AccRegister = 4;
							else
								AccRegister = 0; // choose service type

							SetAccRegisterWindowVisibility(SW_SHOW);
							if (!AccRegister)
								::ShowWindow(GetDlgItem(IDC_OPENNEWACCOUNT), SW_HIDE);
							::SetFocus(GetDlgItem(IDC_EDIT_SCREENNAME));
						}
#ifndef _WODXMPPLIB
						s->Release();
#else
						WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
					}
				}
			}

		}
#ifndef _WODXMPPLIB
		serv->Release();
#endif
	}

	return TRUE;
}
LRESULT CSettingsDlg::CSettingsAccounts::OnListClick(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_ServicesList.GetCurSel()>=0)
		::EnableWindow(GetDlgItem(IDC_REMOVEACCOUNT), TRUE);
	return 0;
}

LRESULT CSettingsDlg::CSettingsAccounts::OnMeasureItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPMEASUREITEMSTRUCT lp = (LPMEASUREITEMSTRUCT)lParam;
	lp->itemWidth = 0;
	lp->itemHeight = 20;

	return TRUE;
}
LRESULT CSettingsDlg::CSettingsAccounts::OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPDRAWITEMSTRUCT lp = (LPDRAWITEMSTRUCT)lParam;

	char buff[1024];
	BOOL sel = (lp->itemState & ODS_SELECTED?TRUE:FALSE);

	HBRUSH hb = sel ? CreateSolidBrush(RGB(0,0,128)) : CreateSolidBrush(RGB(255,255,255));
	::FillRect(lp->hDC, &lp->rcItem, hb);
	DeleteObject(hb);

	buff[0] = 0;
	if (SendMessage(lp->hwndItem, LB_GETTEXT, lp->itemID, (LPARAM)buff))
	{
		SetBkMode(lp->hDC, TRANSPARENT);
		SetTextColor(lp->hDC, sel?RGB(255,255,255):RGB(0,0,0));
		TextOut(lp->hDC, lp->rcItem.left+20, lp->rcItem.top+1, buff, strlen(buff));
	}

	// should I draw icon?
	int icon = 0;
#ifndef _WODXMPPLIB
	WODXMPPCOMLib::IXMPPServices *serv = NULL;
	if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
	{
		short count;
		if (SUCCEEDED(serv->get_Count(&count)))
		{
			for (int i=0;i<count;i++)
			{
				WODXMPPCOMLib::IXMPPService *s = NULL;
				VARIANT var;
				var.vt = VT_I2;
				var.iVal = i;
				if (SUCCEEDED(serv->get_Item(var, &s)))
				{
#else
	short count;
	WODXMPPCOMLib::XMPP_ServicesGetCount(_Jabber->m_Jabb, &count);
	for (int i=0;i<count;i++)
	{
		void *s = NULL;
		WODXMPPCOMLib::XMPP_ServicesGetService(_Jabber->m_Jabb, i, &s);
		if (s)
		{
			{
				{
#endif
					if ((void *)s == (void *)lp->itemData)
					{
						short ct;
#ifndef _WODXMPPLIB
						WODXMPPCOMLib::IXMPPIdentities *idents;
						if (SUCCEEDED(s->get_Identity(&idents)))
						{
							if (SUCCEEDED(idents->get_Count(&ct)))
							{
#else
						WODXMPPCOMLib::XMPP_Service_GetIdentitiesCount(s, &ct);
						{
							{

#endif
								for (int c=0;c<ct;c++)
								{
#ifndef _WODXMPPLIB
									WODXMPPCOMLib::IXMPPIdentity *idty;
									var.iVal = c;
									if (SUCCEEDED(idents->get_Item(var, &idty)))
									{
										WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
										idty->get_Category(&id);
										if (!c)
										{		
											// get name
											CComBSTR2 type;//, n;

//											idty->get_Name(&n);

											idty->get_Type(&type);
											char *t = type.ToString();
											strlwr(t);

											icon = GetServiceType(t);
										}
										idty->Release();
									}
#else
									void *idty = NULL;
									WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
									WODXMPPCOMLib::XMPP_Service_GetIdentity(s, c, &idty);
									if (idty)
									{
										WODXMPPCOMLib::XMPP_Identity_GetCategory(idty, &id);
										if (!c)
										{
											char t[1024];
											int tlen = sizeof(t);
											WODXMPPCOMLib::XMPP_Identity_GetType(idty, t, &tlen);
											_strlwr(t);
											icon = GetServiceType(t);
										}
										WODXMPPCOMLib::XMPP_Identity_Free(idty);
									}
#endif
								}
							}
						}
#ifndef _WODXMPPLIB
						s->Release();
#else
						WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
						break;
					}
#ifndef _WODXMPPLIB
					s->Release();
#else
					WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
				}
			}
		}
	}

	if (icon)
	{
		CxImage cxImage;
	
		if (_LoadIconFromResource(&cxImage, "PNG", CXIMAGE_FORMAT_PNG, icon))
		{
			cxImage.Draw(lp->hDC, lp->rcItem.left+2,lp->rcItem.top+2);
		}
	}
	return TRUE;
}

void CSettingsDlg::CSettingsAccounts::SetAccRegisterWindowVisibility(int show)
{
	if (show == SW_SHOW)
		m_RegVisible = TRUE;
	else
		m_RegVisible = FALSE;
	::ShowWindow(GetDlgItem(IDC_SERVICE_DESCRIPTION), show);
	::ShowWindow(GetDlgItem(IDC_STATIC_USERNAME), show);
	::ShowWindow(GetDlgItem(IDC_STATIC_PASSWORD), show);
	::ShowWindow(GetDlgItem(IDC_NEW_STATIC), show);
	::ShowWindow(GetDlgItem(IDC_EDIT_SCREENNAME), show);
	::ShowWindow(GetDlgItem(IDC_EDIT_PASSWORD), show);
	::ShowWindow(GetDlgItem(IDC_ACCOUNT_REGISTER), show);
	::ShowWindow(GetDlgItem(IDC_OPENNEWACCOUNT), show);
	SetDlgItemText(IDC_EDIT_SCREENNAME, "");
	SetDlgItemText(IDC_EDIT_PASSWORD, "");
}

BOOL CSettingsDlg::CSettingsAccounts::Apply(void)
{
	return TRUE;
}

void CSettingsDlg::CSettingsAccounts::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsAccounts::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::EnableWindow(::GetDlgItem(m_Owner, IDOK), FALSE);
			::EnableWindow(::GetDlgItem(m_Owner, IDC_NEXT), FALSE);

			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
		{
			::EnableWindow(::GetDlgItem(m_Owner, IDOK), TRUE);
			::EnableWindow(::GetDlgItem(m_Owner, IDC_NEXT), TRUE);
			ShowWindow(SW_HIDE);
		}
	}
}

CSettingsDlg::CSettingsContactsAddRemove::CSettingsContactsAddRemove() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Contacts");
	mp += "\\";
	mp += _Settings.Translate("Add/Remove");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Find new contacts.");
	TEXT2 = _Settings.Translate("You can add new Jabber, ICQ, MSN... contacts");


	_Jabber->m_ServiceRegisterHwnd = NULL;
	_Jabber->m_ServiceRefreshHwnd = NULL;
}

CSettingsDlg::CSettingsContactsAddRemove::~CSettingsContactsAddRemove()
{

}

LRESULT CSettingsDlg::CSettingsContactsAddRemove::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsContactsAddRemove::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_Combo1.Attach(GetDlgItem(IDC_CONTACTTYPELIST));
	m_Combo2.Attach(GetDlgItem(IDC_GROUPLIST));
	RefreshGatewaysList();
	RefreshGroupsList();

	::SetFocus(GetDlgItem(IDC_CONTACTID));

	SetDlgItemText(IDC_S1, _Settings.Translate("Contact JID"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Contact Type"));
	SetDlgItemText(IDC_CONTACTVISIBLE, _Settings.Translate("Visible Name"));
	SetDlgItemText(IDC_ADDNEWCONTACT, _Settings.Translate("Add Contact"));
	SetDlgItemText(IDC_S4, _Settings.Translate("Put contact to the group"));
	SetDlgItemText(IDC_ADDNEWGROUP, _Settings.Translate("Add Group"));
	SetDlgItemText(IDC_REMOVEGROUP, _Settings.Translate("Remove Group"));
	return TRUE;
}

void CSettingsDlg::CSettingsContactsAddRemove::RefreshGatewaysList(void)
{
	char buff[1024];

	m_Combo1.ResetContent();

	m_Combo1.AddString("    <Jabber>");
	m_Combo1.AddString("@gmail.com    <GMail>");

#ifndef _WODXMPPLIB
	WODXMPPCOMLib::IXMPPServices *serv = NULL;
	if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
	{
		short count;
		if (SUCCEEDED(serv->get_Count(&count)))
		{
			for (int i=0;i<count;i++)
			{
				WODXMPPCOMLib::IXMPPService *s = NULL;
				VARIANT var;
				var.vt = VT_I2;
				var.iVal = i;
				if (SUCCEEDED(serv->get_Item(var, &s)))
				{
#else
	void *s = NULL;
	short count = 0;
	WODXMPPCOMLib::XMPP_ServicesGetCount(_Jabber->m_Jabb, &count);
	for (int i=0;i<count;i++)
	{
		WODXMPPCOMLib::XMPP_ServicesGetService(_Jabber->m_Jabb, i, &s);
		if (s)
		{
			{
				{
#endif
					short ct;
#ifndef _WODXMPPLIB
					WODXMPPCOMLib::IXMPPIdentities *idents;
					if (SUCCEEDED(s->get_Identity(&idents)))
					{
						if (SUCCEEDED(idents->get_Count(&ct)))
						{

							VARIANT_BOOL bl;
							if (SUCCEEDED(s->get_Registered(&bl)))
							{
								if (bl)
								{
#else
					WODXMPPCOMLib::XMPP_Service_GetIdentitiesCount(s, &ct);
					VARIANT_BOOL bl;
					WODXMPPCOMLib::XMPP_Service_GetRegistered(s, &bl);
					if (bl)
					{
						{
							{
								{
#endif
									for (int c=0;c<ct;c++)
									{
#ifndef _WODXMPPLIB
										WODXMPPCOMLib::IXMPPIdentity *idty;
										var.iVal = c;
										if (SUCCEEDED(idents->get_Item(var, &idty)))
										{
//											WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
//											idty->get_Category(&id);
											if (!c)
											{														
												// get name
												CComBSTR2 name;

												idty->get_Name(&name);
												idty->Release();

												CComBSTR2 b;
												if (SUCCEEDED(s->get_JID(&b)))
												{								
#else
										void *idty = NULL;
										WODXMPPCOMLib::XMPP_Service_GetIdentity(s, c, &idty);
										if (idty)
										{
											WODXMPPCOMLib::IdentityCategoryEnum id = (WODXMPPCOMLib::IdentityCategoryEnum)0;
											WODXMPPCOMLib::XMPP_Identity_GetCategory(idty, &id);
											if (!c)
											{
												char bf[1024];
												int bflen = sizeof(bf);
												WODXMPPCOMLib::XMPP_Identity_GetName(idty, bf, &bflen);
												CComBSTR2 name = bf;
												WODXMPPCOMLib::XMPP_Identity_Free(idty);

												bflen = sizeof(bf);
												WODXMPPCOMLib::XMPP_Service_GetJID(s, bf, &bflen);
												CComBSTR2 b = bf;
												{

#endif
													sprintf(buff, "@%s   <%s>", b.ToString(), name.ToString());
													int j = m_Combo1.InsertString(-1, buff);
													if (j != LB_ERR)
													{
														m_Combo1.SetItemData(j, (unsigned long)s);
													}
												}
											}
											else
											{
#ifndef _WODXMPPLIB
												if (idty)
													idty->Release();
#else
												if (idty)
													WODXMPPCOMLib::XMPP_Identity_Free(idty);
#endif
											}
										}
									}
								}
							}
						}
					}
#ifndef _WODXMPPLIB								
					s->Release();
#else
					WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
				}
			}
		}
#ifndef _WODXMPPLIB
		serv->Release();
#endif
	}
	m_Combo1.SetCurSel(0);
}

void CSettingsDlg::CSettingsContactsAddRemove::RefreshGroupsList(void)
{
	m_Combo2.ResetContent();
	for (int i = 0; i < (signed)_Settings.m_Groups.size(); i++)
	{
		CSettings::TreeGroup *tg = (CSettings::TreeGroup *)_Settings.m_Groups[i];
		if (strcmp(tg->Name, GROUP_OFFLINE))
		{
			m_Combo2.AddString(tg->Name);
		}
	}
}

LRESULT CSettingsDlg::CSettingsContactsAddRemove::OnAddNewGroup(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{

	char buff[1024];
	if (GetDlgItemText(IDC_GROUPLIST, buff, 1024))
	{
		BOOL found = FALSE;
		// do we have this already?
		for (int i=0;i<(signed)_Settings.m_Groups.size();i++)
		{
			CSettings::TreeGroup *tg = _Settings.m_Groups[i];
			if (!lstrcmpi(tg->Name, buff))
				found = TRUE;
		}
		if (!found)
		{
			// ok, add this
			int i = strlen(buff);
			char *a = (char *)malloc(i+1);
			memcpy(a, buff, i+1);
			CSettings::TreeGroup *tg = new CSettings::TreeGroup;
			tg->Item = NULL;
			tg->Open = FALSE;
			tg->Name = a;
			tg->VisibleName = a;
			tg->Block = FALSE;
			tg->CountBuff[0] = 0;
			tg->Temporary = FALSE;
			_Settings.PushGroupSorted(tg);

			RefreshGroupsList();
			_MainDlg.m_UserList.PostMessage(WM_REFRESH, NULL, 0);
		}
	}
	
	return FALSE;
}
LRESULT CSettingsDlg::CSettingsContactsAddRemove::OnRemoveGroup(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{

	char buff[1024];
	if (GetDlgItemText(IDC_GROUPLIST, buff, 1024))
	{
		if (!_MainDlg.m_UserList.DeleteGroup(buff))
		{
//			RefreshGroupsList();
			_MainDlg.m_UserList.PostMessage(WM_REFRESH, NULL, 0);
		}
	}
	return FALSE;
}
LRESULT CSettingsDlg::CSettingsContactsAddRemove::OnContactID(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char buff[1024];
	if (wNotifyCode == EN_CHANGE)
	{
		if (GetDlgItemText(IDC_CONTACTID, buff, 1024))
		{
			char *a = strchr(buff, '@');
			if (a)
				*a = 0;
			a = strchr(buff, '%');
			if (a)
				*a = 0;

			SetDlgItemText(IDC_VISIBLENAME, buff);
		}
	}
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsContactsAddRemove::OnAddNewContact(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CComBSTR a;
	char buff[1024], grp[1024];


	buff[0] = 0;
	if (GetDlgItemText(IDC_CONTACTID, buff, 1024))
	{
		int i = m_Combo1.GetCurSel();
		if (i>=0)
		{
			if (i==0)
			{
				char *b1 = strchr(buff, '@');
				if (!b1) // add address
				{
					// same as mine
					CComBSTR2 j1 = _Settings.m_JID;
					char *j2 = j1.ToString();
					char *j3 = strchr(j2, '@');
						
					strcat(buff, "@");
					if (j3)
					{
						j3++;
						strcat(buff, j3);
					}
					else
					{
						CComBSTR2 b2 = _Settings.m_ServerHost;
						strcat(buff, b2.ToString());
					}
				}
			}
			else
			if (i==1) // gmail
			{
				char *b1 = strchr(buff, '@');
				if (!b1) // add address
					strcat(buff, "@gmail.com");
			}
			else
			{
				void *x = (void *)m_Combo1.GetItemData(i);
				BOOL foundgateway = FALSE;
				if (x)
				{
#ifndef _WODXMPPLIB
					WODXMPPCOMLib::IXMPPServices *serv = NULL;
					if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
					{
						short count;
						if (SUCCEEDED(serv->get_Count(&count)))
						{
#else
					short count;
					WODXMPPCOMLib::XMPP_ServicesGetCount(_Jabber->m_Jabb, &count);
					{
						{

#endif
							for (int i=0;i<count && !foundgateway;i++)
							{
#ifndef _WODXMPPLIB
								WODXMPPCOMLib::IXMPPService *s = NULL;
								VARIANT var;
								var.vt = VT_I2;
								var.iVal = i;
								if (SUCCEEDED(serv->get_Item(var, &s)))
								{
#else
								void *s = NULL;
								WODXMPPCOMLib::XMPP_ServicesGetService(_Jabber->m_Jabb, i, &s);
								{
#endif
									if (s == x)
									{
										foundgateway = TRUE;
										break;
									}
#ifndef _WODXMPPLIB
									s->Release();				
#else
									WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
								}
							}
						}
					}
				}
				if (foundgateway)
				{
					// append gateway 
					char *c;
					while (c = strchr(buff, '@')) *c = '%';
				
					// get gateway JID
					CComBSTR2 j;
#ifndef _WODXMPPLIB
					WODXMPPCOMLib::IXMPPService *s = (WODXMPPCOMLib::IXMPPService *)x;
					if (SUCCEEDED(s->get_JID(&j)))
					{
						strcat(buff, "@");
						strcat(buff, j.ToString());
					}
					s->Release();
#else
					char bfs[1024];
					int bflen = sizeof(bfs);
					WODXMPPCOMLib::XMPP_Service_GetJID(x, bfs, &bflen);
					strcat(buff, "@");
					strcat(buff, bfs);
					WODXMPPCOMLib::XMPP_Service_Free(x);
#endif			
				}
			}



#ifndef _WODXMPPLIB
			// create new contact
			WODXMPPCOMLib::IXMPPContact *ct;
			WODXMPPCOMLib::IXMPPContacts *cts;

			if (SUCCEEDED(_Jabber->m_Jabb->get_Contacts(&cts)))
			{
				CComBSTR b = buff;
				if (SUCCEEDED(cts->raw_Add(b, &ct)))
				{
#else
			void *ct = NULL;
			WODXMPPCOMLib::XMPP_ContactsAdd(_Jabber->m_Jabb, buff, &ct);
			{
				{
#endif
					if (GetDlgItemText(IDC_GROUPLIST, grp, 1024))
					{
						// is this new group?
						BOOL f = FALSE;
						for (int j=0;!f && j<(signed)_Settings.m_Groups.size();j++)
						{
							CSettings::TreeGroup *tg = _Settings.m_Groups[j];
							if (!strcmp(tg->Name, grp))
								f = TRUE;
						}
						if (!f)
						{
							CSettings::TreeGroup *tg = new CSettings::TreeGroup;
							tg->Item = NULL;
							tg->Open = TRUE;
							tg->Temporary = FALSE;
							tg->Block = FALSE;
							tg->CountBuff[0] = 0;
							char *a = (char *)malloc(strlen(grp)+1);
							memset(a, 0, strlen(grp)+1);
							memcpy(a, grp, strlen(grp));
							tg->Name = a;
							tg->VisibleName = a;
							_Settings.PushGroupSorted(tg);
						}

#ifndef _WODXMPPLIB
						// set up group
						CComBSTR2 g = grp;
						ct->put_Group(g);

#else
						WODXMPPCOMLib::XMPP_Contact_SetGroup(ct, grp);
#endif
					}

					
#ifndef _WODXMPPLIB
					try
					{
						ct->Subscribe();
					}catch(_com_error e)
					{
						e = e;
						MessageBeep(-1);
					}
#else
					WODXMPPCOMLib::XMPP_Contact_Subscribe(ct);
#endif

					// and subscribe
					::SendMessage(m_Owner, WM_COMMAND, MAKELONG(IDCANCEL, 0), 0);
//					::DestroyWindow(GetParent());
				}
#ifndef _WODXMPPLIB
				if (ct)
					ct->Release();
#else
				WODXMPPCOMLib::XMPP_Contacts_Free(ct);
#endif
			}
#ifndef _WODXMPPLIB
			cts->Release();
#endif
		}
	}
	return TRUE;
}

BOOL CSettingsDlg::CSettingsContactsAddRemove::Apply(void)
{
	return TRUE;
}

void CSettingsDlg::CSettingsContactsAddRemove::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsContactsAddRemove::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::EnableWindow(::GetDlgItem(m_Owner, IDOK), FALSE);
			::EnableWindow(::GetDlgItem(m_Owner, IDC_NEXT), FALSE);


			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
		{
			::EnableWindow(::GetDlgItem(m_Owner, IDOK), TRUE);
			::EnableWindow(::GetDlgItem(m_Owner, IDC_NEXT), TRUE);

			ShowWindow(SW_HIDE);
		}
	}
}

CSettingsDlg::CSettingsLogging::CSettingsLogging() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("System");
	mp += "\\";
	mp += _Settings.Translate("Logging");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Setup log files and debug info.");
	TEXT2 = _Settings.Translate("Specify paths below");
}

CSettingsDlg::CSettingsLogging::~CSettingsLogging()
{

}


LRESULT CSettingsDlg::CSettingsLogging::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsLogging::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

	::SetFocus(GetDlgItem(IDC_LOG_JABBER));
	CComBSTR2 j = _Settings.m_JabberDebugFile;
	SetDlgItemText(IDC_LOG_JABBER, j.ToString());

	CComBSTR2 j1 = _Settings.m_SocketDebugFile;
	SetDlgItemText(IDC_LOG_SOCKET, j1.ToString());
	
	CComBSTR2 j3 = _Settings.m_VPNSocketDebugFolder;
	SetDlgItemText(IDC_LOG_VPNSOCKET, j3.ToString());
	

	CComBSTR2 j2 = _Settings.m_FunctionDebugFile;
	SetDlgItemText(IDC_LOG_FUNCTION, j2.ToString());

	char buff[1024];
	int i = _Settings.m_DeleteFunctionLogMb;
	i /= 1024;
	i /= 1024; // these are MB
	sprintf(buff, "%d", i);
	SetDlgItemText(IDC_FUNCTIONDELETEMB, buff);
	
	SetDlgItemText(IDC_S1, _Settings.Translate("Log Jabber communication"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Log socket communication"));
	SetDlgItemText(IDC_S3, _Settings.Translate("Log VPN communication on P2P startup"));
	SetDlgItemText(IDC_S4, _Settings.Translate("Log raw function calls (may grow quickly!)"));
	SetDlgItemText(IDC_S5, _Settings.Translate("Delete raw log on each"));
	SetDlgItemText(IDC_S6, _Settings.Translate("megabytes"));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsLogging::Apply(void)
{
	char buff[16384];
	*buff = 0;
	::SendMessage(GetDlgItem(IDC_LOG_JABBER), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
	{
		_Settings.m_JabberDebugFile = buff;
	}
	else
		_Settings.m_JabberDebugFile.Empty();

	*buff = 0;
	::SendMessage(GetDlgItem(IDC_LOG_SOCKET), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
	{
		_Settings.m_SocketDebugFile = buff;
	}
	else
		_Settings.m_SocketDebugFile.Empty();

	*buff = 0;
	::SendMessage(GetDlgItem(IDC_LOG_VPNSOCKET), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
	{
		_Settings.m_VPNSocketDebugFolder = buff;
	}
	else
		_Settings.m_VPNSocketDebugFolder.Empty();

	*buff = 0;
	::SendMessage(GetDlgItem(IDC_LOG_FUNCTION), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
	{
		_Settings.m_FunctionDebugFile = buff;
	}
	else
		_Settings.m_FunctionDebugFile.Empty();

	::SendMessage(GetDlgItem(IDC_FUNCTIONDELETEMB), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
	{
		int i = atol(buff);
		i *= 1024;
		i *= 1024;
		_Settings.m_DeleteFunctionLogMb = i;
	}
	else
		_Settings.m_DeleteFunctionLogMb = 0;

	if (_Jabber)
	{
#ifndef _WODXMPPLIB
		_Jabber->m_Jabb->put_DebugFile(_Settings.m_JabberDebugFile);
#else
		CComBSTR2 jdf = _Settings.m_JabberDebugFile;
		WODXMPPCOMLib::XMPP_SetDebugFile(_Jabber->m_Jabb, jdf.ToString());
#endif
	}
	return TRUE;
}

void CSettingsDlg::CSettingsLogging::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsLogging::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

LRESULT CSettingsDlg::CSettingsLogging::OnBrowseCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char *szFilter = "All Files (*.*)\0*\0TXT files (*.txt)\0*.txt\0\0";
	CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_ENABLESIZING  | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, 
		NULL, m_hWnd);

	cf.m_ofn.lpstrFilter = szFilter;
	cf.m_ofn.lpstrDefExt = "*";

	if (cf.DoModal() == IDOK)
	{
		SendMessage(GetDlgItem(IDC_LOG_JABBER), WM_SETTEXT, 0, (LPARAM)cf.m_szFileName);
	}
	return 0;
}

LRESULT CSettingsDlg::CSettingsLogging::OnBrowse2Cmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char *szFilter = _Settings.Translate("All Files (*.*)\0*\0TXT files (*.txt)\0*.txt\0\0");
	CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_ENABLESIZING  | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, 
		NULL, m_hWnd);

	cf.m_ofn.lpstrFilter = szFilter;
	cf.m_ofn.lpstrDefExt = "*";

	if (cf.DoModal() == IDOK)
	{
		SendMessage(GetDlgItem(IDC_LOG_SOCKET), WM_SETTEXT, 0, (LPARAM)cf.m_szFileName);
	}
	return 0;
}

LRESULT CSettingsDlg::CSettingsLogging::OnBrowse3Cmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char *szFilter = _Settings.Translate("All Files (*.*)\0*\0TXT files (*.txt)\0*.txt\0\0");
	CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_ENABLESIZING  | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, 
		NULL, m_hWnd);

	cf.m_ofn.lpstrFilter = szFilter;
	cf.m_ofn.lpstrDefExt = "*";

	if (cf.DoModal() == IDOK)
	{
		SendMessage(GetDlgItem(IDC_LOG_FUNCTION), WM_SETTEXT, 0, (LPARAM)cf.m_szFileName);
	}
	return 0;
}

LRESULT CSettingsDlg::CSettingsLogging::OnBrowse4Cmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char *szFilter = _Settings.Translate("All Files (*.*)\0*\0TXT files (*.txt)\0*.txt\0\0");
	CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_ENABLESIZING  | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, 
		NULL, m_hWnd);
	
	cf.m_ofn.lpstrFilter = szFilter;
	cf.m_ofn.lpstrDefExt = "*";
	
	if (cf.DoModal() == IDOK)
	{
		SendMessage(GetDlgItem(IDC_LOG_VPNSOCKET), WM_SETTEXT, 0, (LPARAM)cf.m_szFileName);
	}
	return 0;
}

CSettingsDlg::CSettingsMsgWin::CSettingsMsgWin() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("System");
	mp += "\\";
	mp += _Settings.Translate("Messages");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Chat window settings.");
	TEXT2 = _Settings.Translate("For all open chat windows.");
}

CSettingsDlg::CSettingsMsgWin::~CSettingsMsgWin()
{

}

LRESULT CSettingsDlg::CSettingsMsgWin::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

	if (_Settings.m_TimestampMessages)
		::SendMessage(GetDlgItem(IDC_TIMESTAMP_MESSAGES), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_TIMESTAMP_MESSAGES), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowMessageHistory)
		::SendMessage(GetDlgItem(IDC_SHOW_MESSAGE_HISTORY), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOW_MESSAGE_HISTORY), BM_SETCHECK, FALSE, NULL);

	SetDlgItemText(IDC_S1, _Settings.Translate("Various settings regarding chat window"));
	SetDlgItemText(IDC_TIMESTAMP_MESSAGES, _Settings.Translate("Timestamp messages"));
	SetDlgItemText(IDC_SHOW_MESSAGE_HISTORY, _Settings.Translate("Show message history"));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsMsgWin::Apply(void)
{
	if (::SendMessage(GetDlgItem(IDC_TIMESTAMP_MESSAGES), BM_GETSTATE, NULL, NULL))
		_Settings.m_TimestampMessages = TRUE;
	else
		_Settings.m_TimestampMessages = FALSE;
	
	if (::SendMessage(GetDlgItem(IDC_SHOW_MESSAGE_HISTORY), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowMessageHistory = TRUE;
	else
		_Settings.m_ShowMessageHistory = FALSE;

	return TRUE;
}
void CSettingsDlg::CSettingsMsgWin::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsMsgWin::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

CSettingsDlg::CSettingsUser1::CSettingsUser1() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("General info");
	TEXT1 = _Settings.Translate("General information about");
	TEXT2 = m_Text2;

	m_IsContact = TRUE;
}

CSettingsDlg::CSettingsUser1::~CSettingsUser1()
{

}

LRESULT CSettingsDlg::CSettingsUser1::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetDlgItemText(IDC_S1, _Settings.Translate("Nickname"));
	SetDlgItemText(IDC_S2, _Settings.Translate("First name"));
	SetDlgItemText(IDC_S3, _Settings.Translate("Last name"));
	SetDlgItemText(IDC_S4, _Settings.Translate("Email"));
	SetDlgItemText(IDC_S5, _Settings.Translate("URL"));
	SetDlgItemText(IDC_S6, _Settings.Translate("Notes"));
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsUser1::OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_Starting)
		return TRUE;

	switch (wID)
	{
		case IDC_SETTINGS_USER1_NICKNAME:
		case IDC_SETTINGS_USER1_FIRSTNAME:
		case IDC_SETTINGS_USER1_LASTNAME:
		case IDC_SETTINGS_USER1_EMAIL:
		case IDC_SETTINGS_USER1_URL:
		case IDC_SETTINGS_USER1_DESCRIPTION:
			VCardChanged();
			break;

	}
	return FALSE;
}


LRESULT CSettingsDlg::CSettingsUser1::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& /*bHandled*/)
{
	CPaintDC dcPaint(m_hWnd);
	CDC dc_ff;			// Memory base device context for flicker free painting
	CBitmap bm_ff;		// The bitmap we paint into
	HBITMAP bm_old;

	int x = m_Image.GetWidth();
//	if (x>128) x = 128;
	int y = m_Image.GetHeight();
//	if (y>64) y = 64;

	if (x && y)
	{

		dc_ff.CreateCompatibleDC(dcPaint);
		bm_ff.CreateCompatibleBitmap(dcPaint, x, y);
	
		bm_old = dc_ff.SelectBitmap(bm_ff);

		RECT rect = {0};
		rect.bottom += y;
		rect.right += x;
		HBRUSH hbr = GetSysColorBrush(COLOR_3DFACE);
		FillRect(dc_ff, &rect, hbr);

	
		m_Image.Draw(dc_ff, 0, 0);
	
		dcPaint.BitBlt(230, 10, x, y, dc_ff, 0, 0, SRCCOPY);
	
		dc_ff.SelectBitmap(bm_old);
	}

	return FALSE;
}
		
#ifndef _WODXMPPLIB
void CSettingsDlg::CSettingsUser1::InitData(WODXMPPCOMLib::IXMPPVCard *card)
#else
void CSettingsDlg::CSettingsUser1::InitData(void *card)
#endif
{
	if (!IsWindow())
		return;
	if (m_IsContact)
	{
		CUser *user = _MainDlg.m_UserList.GetUserByJID(m_Text2);
		if (user)
		{
			CxImage img;
//			if (user->m_Icon.Len())
			if (user->LoadUserImage(&img))
			{
				m_Image.Clear();
				m_Image.Copy(img);
//				m_Image.Decode((unsigned char *)user->m_Icon.Ptr(), user->m_Icon.Len(), CXIMAGE_FORMAT_PNG);
				ResampleImageIfNeeded(&m_Image, 170, 100);
				if (IsWindow())
					Invalidate();
			}
		}
	}

#ifdef _WODXMPPLIB
	char bff[1024];
	int bfflen;
#endif


#ifndef _WODXMPPLIB
	CComBSTR2 n1;
	card->get_NickName(&n1);
	SetDlgItemText(IDC_SETTINGS_USER1_NICKNAME, n1.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetNickName(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER1_NICKNAME, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n2;
	card->get_FirstName(&n2);
	SetDlgItemText(IDC_SETTINGS_USER1_FIRSTNAME, n2.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetFirstName(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER1_FIRSTNAME, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n3;
	card->get_LastName(&n3);
	SetDlgItemText(IDC_SETTINGS_USER1_LASTNAME, n3.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetLastName(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER1_LASTNAME, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n4;
	card->get_Email(&n4);
	SetDlgItemText(IDC_SETTINGS_USER1_EMAIL, n4.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetEmail(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER1_EMAIL, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n5;
	card->get_URL(&n5);
	SetDlgItemText(IDC_SETTINGS_USER1_URL, n5.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetURL(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER1_URL, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n6;
	card->get_Description(&n6);
	SetDlgItemText(IDC_SETTINGS_USER1_DESCRIPTION, n6.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetDescription(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER1_DESCRIPTION, bff);
#endif

}

void CSettingsDlg::CSettingsUser1::Lock(BOOL dolock)
{
	SendDlgItemMessage(IDC_SETTINGS_USER1_NICKNAME, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER1_FIRSTNAME, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER1_LASTNAME, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER1_EMAIL, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER1_URL, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER1_DESCRIPTION, EM_SETREADONLY, dolock, NULL);

}

BOOL CSettingsDlg::CSettingsUser1::Apply(void)
{
	char buff[1024];
	if (!m_IsContact)
	{
#ifndef _WODXMPPLIB
		WODXMPPCOMLib::IXMPPVCard *vc = NULL;
		if (_Jabber)
			_Jabber->m_Jabb->get_VCard(&vc);
#else
		void *vc = NULL;
		WODXMPPCOMLib::XMPP_GetVCard(_Jabber->m_Jabb, &vc);
#endif
		if (vc)
		{

			CComBSTR n;
			SendDlgItemMessage(IDC_SETTINGS_USER1_NICKNAME, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_NickName(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetNickName(vc, buff);
#endif

			SendDlgItemMessage(IDC_SETTINGS_USER1_FIRSTNAME, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_FirstName(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetFirstName(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER1_LASTNAME, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_LastName(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetLastName(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER1_EMAIL, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_Email(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetEmail(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER1_URL, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_URL(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetURL(vc, buff);
#endif


			SendDlgItemMessage(IDC_SETTINGS_USER1_DESCRIPTION, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_Description(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetDescription(vc, buff);
#endif
			
		}
	}

	
	return TRUE;
}
void CSettingsDlg::CSettingsUser1::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsUser1::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}
CSettingsDlg::CSettingsUser2::CSettingsUser2() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("Personal information");
	TEXT1 = _Settings.Translate("Home details for");
	TEXT2 = m_Text2;
	m_IsContact = TRUE;
}

CSettingsDlg::CSettingsUser2::~CSettingsUser2()
{

}

LRESULT CSettingsDlg::CSettingsUser2::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsUser2::OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_Starting)
		return TRUE;

	switch (wID)
	{
		case IDC_SETTINGS_USER2_ADDRESS:
		case IDC_SETTINGS_USER2_ADDRESSEXT:
		case IDC_SETTINGS_USER2_CITY:
		case IDC_SETTINGS_USER2_STATE:
		case IDC_SETTINGS_USER2_ZIP:
		case IDC_SETTINGS_USER2_COUNTRY:
		case IDC_SETTINGS_USER2_PHONE:
		case IDC_SETTINGS_USER2_FAX:
		case IDC_SETTINGS_USER2_BDAY:
			VCardChanged();
			break;


	}
	return FALSE;
}


LRESULT CSettingsDlg::CSettingsUser2::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetDlgItemText(IDC_S1, _Settings.Translate("Address"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Address Ext."));
	SetDlgItemText(IDC_S3, _Settings.Translate("City"));
	SetDlgItemText(IDC_S4, _Settings.Translate("State"));
	SetDlgItemText(IDC_S5, _Settings.Translate("ZIP"));
	SetDlgItemText(IDC_S6, _Settings.Translate("Country"));
	SetDlgItemText(IDC_STATIC_BIRTHDAY, _Settings.Translate("Birthday"));
	SetDlgItemText(IDC_S7, _Settings.Translate("Voice Tel."));
	SetDlgItemText(IDC_S8, _Settings.Translate("Fax"));
	return TRUE;
}

#ifndef _WODXMPPLIB
void CSettingsDlg::CSettingsUser2::InitData(WODXMPPCOMLib::IXMPPVCard *card)
#else
void CSettingsDlg::CSettingsUser2::InitData(void *card)
#endif
{
	if (!IsWindow())
		return;

#ifdef _WODXMPPLIB
	char bff[1024];
	int bfflen;
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n1;
	card->get_HomeAddress(&n1);
	SetDlgItemText(IDC_SETTINGS_USER2_ADDRESS, n1.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomeAddress(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_ADDRESS, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n2;
	card->get_HomeAddressExt(&n2);
	SetDlgItemText(IDC_SETTINGS_USER2_ADDRESSEXT, n2.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomeAddressExt(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_ADDRESSEXT, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n3;
	card->get_HomeCity(&n3);
	SetDlgItemText(IDC_SETTINGS_USER2_CITY, n3.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomeCity(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_CITY, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n4;
	card->get_HomeState(&n4);
	SetDlgItemText(IDC_SETTINGS_USER2_STATE, n4.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomeState(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_STATE, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n5;
	card->get_HomeZip(&n5);
	SetDlgItemText(IDC_SETTINGS_USER2_ZIP, n5.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomeZip(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_ZIP, bff);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n6;
	card->get_HomeCountry(&n6);
	SetDlgItemText(IDC_SETTINGS_USER2_COUNTRY, n6.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomeCountry(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_COUNTRY, bff);
#endif

	DATE day = 0;
#ifndef _WODXMPPLIB
	card->get_BirthDay(&day);
#else
	WODXMPPCOMLib::XMPP_VCard_GetBirthDay(card, &day);
#endif
	if (day)
	{
		SYSTEMTIME st;
		memset(&st, 0, sizeof(st));
		VariantTimeToSystemTime(day, &st);
		SendDlgItemMessage(IDC_SETTINGS_USER2_BDAY,DTM_SETSYSTEMTIME, GDT_VALID, (LPARAM)&st); 
	}
	else
		SendDlgItemMessage(IDC_SETTINGS_USER2_BDAY,DTM_SETSYSTEMTIME, GDT_NONE, NULL); 

#ifndef _WODXMPPLIB
	CComBSTR2 n8;
	card->get_HomePhone(&n8);
	SetDlgItemText(IDC_SETTINGS_USER2_PHONE, n8.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomePhone(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_PHONE, bff);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n9;
	card->get_HomeFax(&n9);
	SetDlgItemText(IDC_SETTINGS_USER2_FAX, n9.ToString());
#else
	bfflen = sizeof(bff);
	WODXMPPCOMLib::XMPP_VCard_GetHomeFax(card, bff, &bfflen);
	SetDlgItemText(IDC_SETTINGS_USER2_FAX, bff);
#endif
	

}

void CSettingsDlg::CSettingsUser2::Lock(BOOL dolock)
{
	SendDlgItemMessage(IDC_SETTINGS_USER2_ADDRESS, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER2_ADDRESSEXT, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER2_CITY, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER2_STATE, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER2_ZIP, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER2_COUNTRY, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER2_PHONE, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER2_FAX, EM_SETREADONLY, dolock, NULL);

	::EnableWindow(GetDlgItem(IDC_SETTINGS_USER2_BDAY), dolock?FALSE:TRUE);
}


BOOL CSettingsDlg::CSettingsUser2::Apply(void)
{
	char buff[1024];
	if (!m_IsContact)
	{
#ifndef _WODXMPPLIB
		WODXMPPCOMLib::IXMPPVCard *vc = NULL;
		if (_Jabber)
			_Jabber->m_Jabb->get_VCard(&vc);
#else
		void *vc = NULL;
		WODXMPPCOMLib::XMPP_GetVCard(_Jabber->m_Jabb, &vc);
#endif
		if (vc)
		{

			CComBSTR n;
			SendDlgItemMessage(IDC_SETTINGS_USER2_ADDRESS, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomeAddress(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomeAddress(vc, buff);
#endif

			SendDlgItemMessage(IDC_SETTINGS_USER2_ADDRESSEXT, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomeAddressExt(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomeAddressExt(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER2_CITY, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomeCity(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomeCity(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER2_STATE, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomeState(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomeState(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER2_ZIP, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomeZip(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomeZip(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER2_COUNTRY, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomeCountry(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomeCountry(vc, buff);
#endif

			SendDlgItemMessage(IDC_SETTINGS_USER2_PHONE, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomePhone(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomePhone(vc, buff);
#endif
			

			SendDlgItemMessage(IDC_SETTINGS_USER2_FAX, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_HomeFax(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetHomeFax(vc, buff);
#endif
			
			
			SYSTEMTIME st;
			memset(&st, 0, sizeof(st));
			if (SendDlgItemMessage(IDC_SETTINGS_USER2_BDAY,DTM_GETSYSTEMTIME, 0, (LPARAM)&st) == GDT_VALID)
			{
				DATE d;
				SystemTimeToVariantTime(&st, &d);
#ifndef _WODXMPPLIB
				vc->put_BirthDay(d);
#else
				WODXMPPCOMLib::XMPP_VCard_SetBirthDay(vc, d);
#endif
				
			}
		}
	}

	return TRUE;
}
void CSettingsDlg::CSettingsUser2::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsUser2::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

CSettingsDlg::CSettingsUser3::CSettingsUser3() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("Business information");
	TEXT1 = _Settings.Translate("Work details for");
	TEXT2 = m_Text2;

	m_IsContact = TRUE;
}

CSettingsDlg::CSettingsUser3::~CSettingsUser3()
{

}

LRESULT CSettingsDlg::CSettingsUser3::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsUser3::OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_Starting)
		return TRUE;

	switch (wID)
	{
			case IDC_SETTINGS_USER3_ADDRESS:
			case IDC_SETTINGS_USER3_ADDRESSEXT:
			case IDC_SETTINGS_USER3_CITY:
			case IDC_SETTINGS_USER3_STATE:
			case IDC_SETTINGS_USER3_ZIP:
			case IDC_SETTINGS_USER3_COUNTRY:
			case IDC_SETTINGS_USER3_PHONE:
			case IDC_SETTINGS_USER3_FAX:
			case IDC_SETTINGS_USER3_TITLE:
			case IDC_SETTINGS_USER3_COMPANY:
			case IDC_SETTINGS_USER3_DEPARTMENT:
			VCardChanged();
			break;

	}
	return FALSE;
}
LRESULT CSettingsDlg::CSettingsUser3::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	SetDlgItemText(IDC_S1, _Settings.Translate("Address"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Address Ext."));
	SetDlgItemText(IDC_S3, _Settings.Translate("City"));
	SetDlgItemText(IDC_S4, _Settings.Translate("State"));
	SetDlgItemText(IDC_S5, _Settings.Translate("ZIP"));
	SetDlgItemText(IDC_S6, _Settings.Translate("Country"));
	SetDlgItemText(IDC_S7, _Settings.Translate("Title"));
	SetDlgItemText(IDC_S8, _Settings.Translate("Voice Tel."));
	SetDlgItemText(IDC_S9, _Settings.Translate("Fax"));
	SetDlgItemText(IDC_S10, _Settings.Translate("Company name"));
	SetDlgItemText(IDC_S11, _Settings.Translate("Department"));

	return TRUE;
}
#ifndef _WODXMPPLIB
void CSettingsDlg::CSettingsUser3::InitData(WODXMPPCOMLib::IXMPPVCard *card)
#else
void CSettingsDlg::CSettingsUser3::InitData(void *card)
#endif
{
	if (!IsWindow())
		return;

#ifdef _WODXMPPLIB
	char bf[1024];
	int bfl = 0;
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n1;
	card->get_WorkAddress(&n1);
	SetDlgItemText(IDC_SETTINGS_USER3_ADDRESS, n1.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkAddress(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_ADDRESS, bf);
#endif

#ifndef _WODXMPPLIB
	CComBSTR2 n2;
	card->get_WorkAddressExt(&n2);
	SetDlgItemText(IDC_SETTINGS_USER3_ADDRESSEXT, n2.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkAddressExt(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_ADDRESSEXT, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n3;
	card->get_WorkCity(&n3);
	SetDlgItemText(IDC_SETTINGS_USER3_CITY, n3.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkCity(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_CITY, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n4;
	card->get_WorkState(&n4);
	SetDlgItemText(IDC_SETTINGS_USER3_STATE, n4.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkState(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_STATE, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n5;
	card->get_WorkZip(&n5);
	SetDlgItemText(IDC_SETTINGS_USER3_ZIP, n5.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkZip(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_ZIP, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n6;
	card->get_WorkCountry(&n6);
	SetDlgItemText(IDC_SETTINGS_USER3_COUNTRY, n6.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkCountry(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_COUNTRY, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n7;
	card->get_WorkPhone(&n7);
	SetDlgItemText(IDC_SETTINGS_USER3_PHONE, n7.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkPhone(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_PHONE, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n8;
	card->get_WorkFax(&n8);
	SetDlgItemText(IDC_SETTINGS_USER3_FAX, n8.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetWorkFax(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_FAX, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n9;
	card->get_Title(&n9);
	SetDlgItemText(IDC_SETTINGS_USER3_TITLE, n9.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetTitle(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_TITLE, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n10;
	card->get_OrganizationName(&n10);
	SetDlgItemText(IDC_SETTINGS_USER3_COMPANY, n10.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetOrganizationName(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_COMPANY, bf);
#endif
	

#ifndef _WODXMPPLIB
	CComBSTR2 n11;
	card->get_OrganizationUnit(&n11);
	SetDlgItemText(IDC_SETTINGS_USER3_DEPARTMENT, n11.ToString());
#else
	bfl = sizeof(bf);
	WODXMPPCOMLib::XMPP_VCard_GetOrganizationUnit(card, bf, &bfl);
	SetDlgItemText(IDC_SETTINGS_USER3_DEPARTMENT, bf);
#endif
	

}

void CSettingsDlg::CSettingsUser3::Lock(BOOL dolock)
{
	SendDlgItemMessage(IDC_SETTINGS_USER3_ADDRESS, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_ADDRESSEXT, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_CITY, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_STATE, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_ZIP, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_COUNTRY, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_PHONE, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_FAX, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_TITLE, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_COMPANY, EM_SETREADONLY, dolock, NULL);
	SendDlgItemMessage(IDC_SETTINGS_USER3_DEPARTMENT, EM_SETREADONLY, dolock, NULL);
}


BOOL CSettingsDlg::CSettingsUser3::Apply(void)
{
	char buff[1024];
	if (!m_IsContact)
	{
#ifndef _WODXMPPLIB
		WODXMPPCOMLib::IXMPPVCard *vc = NULL;
		if (_Jabber)
			_Jabber->m_Jabb->get_VCard(&vc);
#else
		void *vc = NULL;
		WODXMPPCOMLib::XMPP_GetVCard(_Jabber->m_Jabb, &vc);
#endif

		if (vc)
		{
			CComBSTR n;
			SendDlgItemMessage(IDC_SETTINGS_USER3_ADDRESS, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkAddress(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkAddress(vc, buff);
#endif

			SendDlgItemMessage(IDC_SETTINGS_USER3_ADDRESSEXT, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkAddressExt(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkAddressExt(vc, buff);
#endif
			
			SendDlgItemMessage(IDC_SETTINGS_USER3_CITY, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkCity(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkCity(vc, buff);
#endif
			
			SendDlgItemMessage(IDC_SETTINGS_USER3_STATE, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkState(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkState(vc, buff);
#endif
			
			SendDlgItemMessage(IDC_SETTINGS_USER3_ZIP, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkZip(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkZip(vc, buff);
#endif

			SendDlgItemMessage(IDC_SETTINGS_USER3_COUNTRY, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkCountry(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkCountry(vc, buff);
#endif

			SendDlgItemMessage(IDC_SETTINGS_USER3_PHONE, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkPhone(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkPhone(vc, buff);
#endif

			SendDlgItemMessage(IDC_SETTINGS_USER3_FAX, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_WorkFax(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetWorkFax(vc, buff);
#endif
			
			SendDlgItemMessage(IDC_SETTINGS_USER3_TITLE, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_Title(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetTitle(vc, buff);
#endif
			
			SendDlgItemMessage(IDC_SETTINGS_USER3_COMPANY, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_OrganizationName(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetOrganizationName(vc, buff);
#endif
			
			SendDlgItemMessage(IDC_SETTINGS_USER3_DEPARTMENT, WM_GETTEXT, 1024, (LPARAM)buff);
#ifndef _WODXMPPLIB
			n.Empty();
			n = buff;
			vc->put_OrganizationUnit(n);
#else
			WODXMPPCOMLib::XMPP_VCard_SetOrganizationUnit(vc, buff);
#endif
			
		}
	}
	return TRUE;
}
void CSettingsDlg::CSettingsUser3::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsUser3::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}
CSettingsDlg::CSettingsUser4::CSettingsUser4() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("Network");
	TEXT1 = _Settings.Translate("Network configuration for");
	TEXT2 = m_Text2;
}

CSettingsDlg::CSettingsUser4::~CSettingsUser4()
{

}


LRESULT CSettingsDlg::CSettingsUser4::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsUser4::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	LV_COLUMN lvcol = {0};
	lvcol.mask=/*LVCF_TEXT | */LVCF_WIDTH;
//	lvcol.pszText="Interface";
	lvcol.cx = 210;

//	SendDlgItemMessage(IDC_INTERFACELIST,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_BORDERSELECT | LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES | LVS_EX_GRIDLINES); 
	SendDlgItemMessage(IDC_INTERFACELIST,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_BORDERSELECT | LVS_EX_FULLROWSELECT | /*LVS_EX_CHECKBOXES | */LVS_EX_GRIDLINES); 
	SendDlgItemMessage(IDC_INTERFACELIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 
	lvcol.cx = 80;
	SendDlgItemMessage(IDC_INTERFACELIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 


	CFontHandle DlgFont = GetFont();
	CClientDC dcScreen(NULL);
	
	LOGFONT LogFont = {0};
	DlgFont.GetLogFont(&LogFont);

	_tcscpy(LogFont.lfFaceName, _T("arial bold"));
	LogFont.lfWeight = FW_BOLD;
	LogFont.lfCharSet = DEFAULT_CHARSET;
	LogFont.lfHeight = -::MulDiv(8, dcScreen.GetDeviceCaps(LOGPIXELSY), 72);
	m_BoldFont.CreateFontIndirect(&LogFont);

	return TRUE;
}

void CSettingsDlg::CSettingsUser4::InitData(void)
{
	if (!IsWindow() || !::IsWindow(GetDlgItem(IDC_INTERFACELIST)))
		return;

	SendDlgItemMessage(IDC_INTERFACELIST, LVM_DELETEALLITEMS, 0, 0);

	CUser *user = _MainDlg.m_UserList.GetUserByJID(m_Text2);
	if (user)
	{
		LVITEM it = {0};
		it.mask = LVIF_TEXT;
		if (user->m_AllowedRemoteAny)
			it.pszText = (char *)_Settings.Translate("allow");
		else
			it.pszText = (char *)_Settings.Translate("deny");
		it.cchTextMax = strlen(it.pszText);


		int res = SendDlgItemMessage(IDC_INTERFACELIST, LVM_INSERTITEM, 0, (LPARAM)&it);
		it.iItem = res;
		it.iSubItem = 1;
		it.mask = LVIF_TEXT;
		it.pszText = _Settings.Translate("all other");
		it.cchTextMax = strlen(it.pszText);

		SendDlgItemMessage(IDC_INTERFACELIST, LVM_SETITEM, 0, (LPARAM)&it);
//		if (user->m_AllowedRemoteAny)
			ListView_SetItemState(GetDlgItem(IDC_INTERFACELIST), res, LVIS_SELECTED, LVIS_OVERLAYMASK );

		for (int i=0;i<(signed)user->m_AllowedRemoteIPs.size();i++)
		{
			IPAddressConnectionStruct *ips = (IPAddressConnectionStruct *)user->m_AllowedRemoteIPs[i];

			in_addr ar;
			ar.S_un.S_addr = ips->Address;
			char *a = inet_ntoa(ar);
			it.mask = LVIF_TEXT;
			it.iSubItem = 0;
			it.iItem = 0;
			if (ips->Ignored)
				it.pszText = _Settings.Translate("not specified");
			else
			{
				if (ips->Allowed)
					it.pszText = (char *)_Settings.Translate("allow");
				else
					it.pszText = (char *)_Settings.Translate("deny");
			}
			it.cchTextMax = strlen(it.pszText);

			res = SendDlgItemMessage(IDC_INTERFACELIST, LVM_INSERTITEM, 0, (LPARAM)&it);
			it.iSubItem = 1;
			it.iItem = res;
			it.pszText = a;
			it.cchTextMax = strlen(a);
			SendDlgItemMessage(IDC_INTERFACELIST, LVM_SETITEM, 0, (LPARAM)&it);
		}

		it.iItem = 0;
		it.iSubItem = 0;
		it.mask = LVIF_TEXT;
		if (user->m_AllowedRemoteMediator)
			it.pszText = (char *)_Settings.Translate("allow");
		else
			it.pszText = (char *)_Settings.Translate("deny");


		res = SendDlgItemMessage(IDC_INTERFACELIST, LVM_INSERTITEM, 0, (LPARAM)&it);
		it.mask = LVIF_TEXT;
		it.iSubItem = 1;
		it.iItem = res;
		it.pszText = _Settings.Translate("address given by mediator");
		it.cchTextMax = strlen(it.pszText);
		SendDlgItemMessage(IDC_INTERFACELIST, LVM_SETITEM, 0, (LPARAM)&it);
	}

	SetDlgItemText(IDC_S1, _Settings.Translate("Allowed interfaces for direct connection"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Click on first column to change values"));
	SetDlgItemText(IDC_SETTINGS_USER4_RESETALL, _Settings.Translate("&Allow All"));
	SetDlgItemText(IDC_S3, _Settings.Translate("If you choose 'not specified' value, then same value is used as in 'all other' setting."));
}

LRESULT CSettingsDlg::CSettingsUser4::OnInterfaceList(int idCtrl, LPNMHDR pnmh, BOOL& bHandled)
{

	NMLISTVIEW *nm = (NMLISTVIEW *)pnmh;

	switch (pnmh->code)
	{
		case NM_CLICK:
		case NM_DBLCLK:
			{
				CUser *user = _MainDlg.m_UserList.GetUserByJID(m_Text2);
				if (user)
				{
					int count = SendDlgItemMessage(IDC_INTERFACELIST, LVM_GETITEMCOUNT, 0, 0);
					if (nm->iItem == 0)
					{
						user->m_AllowedRemoteMediator = !user->m_AllowedRemoteMediator;
					}
					else
					if (nm->iItem == (count-1))
					{
						user->m_AllowedRemoteAny = !user->m_AllowedRemoteAny;
					}
					else
					{
						int j = count-nm->iItem-2;
						if (j<0 || j>=(signed)user->m_AllowedRemoteIPs.size())
							return 0;

						IPAddressConnectionStruct *ips = (IPAddressConnectionStruct *)user->m_AllowedRemoteIPs[j];
						if (ips->Ignored)
							ips->Ignored = !ips->Ignored;
						else
						{
							if (!ips->Allowed)
								ips->Allowed = !ips->Allowed;
							else
							{
								ips->Allowed = FALSE;
								ips->Ignored = TRUE;
							}
						}
					}
					InitData();
				}
			}
			break;
	}
	return 0;
}	


LRESULT CSettingsDlg::CSettingsUser4::OnBtnResetAll(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	// ok, enumerate intefaces
	CUser *user = _MainDlg.m_UserList.GetUserByJID(m_Text2);
	if (user)
	{
		for (int i=0;i<(signed)user->m_AllowedRemoteIPs.size();i++)
		{
			IPAddressConnectionStruct *ips = (IPAddressConnectionStruct *)user->m_AllowedRemoteIPs[i];
			ips->Allowed = TRUE;
			ips->Ignored = TRUE;
		}

		user->m_AllowedRemoteAny = TRUE;
		user->m_AllowedRemoteMediator = TRUE;
	}

	InitData();
	return 0;
}

void CSettingsDlg::CSettingsUser4::Lock(BOOL)
{

}

BOOL CSettingsDlg::CSettingsUser4::Apply(void)
{

	return TRUE;
}
void CSettingsDlg::CSettingsUser4::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsUser4::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

LRESULT CSettingsDlg::CSettingsUser4::OnMeasureItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	MEASUREITEMSTRUCT *lpm = (MEASUREITEMSTRUCT *)lParam;
	CClientDC dc(m_hWnd);
	TEXTMETRIC tm;

	dc.SelectFont(GetFont());
	dc.GetTextMetrics(&tm);

	lpm->itemHeight = (tm.tmHeight + tm.tmExternalLeading) +3 ;
	return 0;
}

LRESULT CSettingsDlg::CSettingsUser4::OnDrawItem(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM lParam, BOOL& /*bHandled*/)
{
	char buff[1024];
	DRAWITEMSTRUCT *lpm = (DRAWITEMSTRUCT *)lParam;

	HDC dc = lpm->hDC;
	CRect item_rect = lpm->rcItem;
	int item_id = lpm->itemID;

	SetTextColor(dc, RGB(0,0,0));

	BOOL isbold = FALSE;
	BOOL isignored = FALSE;

	CUser *user = _MainDlg.m_UserList.GetUserByJID(m_Text2);
	if (user)
	{
		int count = SendDlgItemMessage(IDC_INTERFACELIST, LVM_GETITEMCOUNT, 0, 0);
		if (item_id == 0)
		{
			if (user->m_AllowedRemoteMediator)
				isbold = TRUE;
		}
		else
		if (item_id == (count-1))
		{
			if (user->m_AllowedRemoteAny)
				isbold = TRUE;
		}
		else
		{
			int j = count-item_id-2;
			if (j>=0 && j<(signed)user->m_AllowedRemoteIPs.size())
			{
				IPAddressConnectionStruct *ips = (IPAddressConnectionStruct *)user->m_AllowedRemoteIPs[j];
				if (ips->Ignored)
					isignored = TRUE;
				else
				{
					if (!ips->Ignored && ips->Allowed)
						isbold = TRUE;
				}
			}
		}
	}

	if (isignored)
		::SetTextColor(dc, RGB(128,128,128));
	else
		::SetTextColor(dc, RGB(0,0,0));

	HGDIOBJ oldfont = NULL;
	if (isbold)
	 oldfont = ::SelectObject(dc, m_BoldFont);


	LVITEM tvi = {0};
	tvi.iItem = item_id;
	tvi.mask = LVIF_TEXT;
	tvi.pszText = buff;
	tvi.cchTextMax = sizeof(buff);
	item_rect.left += 5;
	if (SendDlgItemMessage(IDC_INTERFACELIST, LVM_GETITEM, 0, (LPARAM)&tvi))
	{
		DrawText(dc, tvi.pszText, strlen(tvi.pszText), item_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}

	tvi.iItem = item_id;
	tvi.iSubItem = 1;
	tvi.mask = LVIF_TEXT;
	tvi.pszText = buff;
	tvi.cchTextMax = sizeof(buff);
	item_rect.left += 80;
	if (SendDlgItemMessage(IDC_INTERFACELIST, LVM_GETITEM, 0, (LPARAM)&tvi))
	{
		DrawText(dc, tvi.pszText, strlen(tvi.pszText), item_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}
	if (oldfont)
		::SelectObject(dc, oldfont);
	
	return 0;
}

char m_SkinThreadBuffer[1024];
CSettingsDlg::CSettingsSkins::CSettingsSkins() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Appearance");
	mp += "\\";
	mp += _Settings.Translate("Skins");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Choose skin from below list.");
	TEXT2 = _Settings.Translate("What will Wippien wear?");
	memset(m_SkinThreadBuffer, 0, sizeof(m_SkinThreadBuffer));

	_LoadIconFromResource(&m_BaseImage, "PNG", CXIMAGE_FORMAT_PNG, ID_PNG_SKINBASE);
}

CSettingsDlg::CSettingsSkins::~CSettingsSkins()
{
	while (m_Image.size())
	{
		CxImage *img = m_Image[0];
		delete img;
		m_Image.erase(m_Image.begin());
	}
}

LRESULT CSettingsDlg::CSettingsSkins::OnMeasureItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPMEASUREITEMSTRUCT lp = (LPMEASUREITEMSTRUCT)lParam;
//	if (lp->itemID>=0 && lp->itemID < m_Image.size())
	{
//		CxImage *img = m_Image[lp->itemID];
//		lp->itemWidth = img->GetWidth();
//		lp->itemHeight = img->GetHeight();
		lp->itemWidth = 150;
		lp->itemHeight = 141;
	}
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsSkins::OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	LPDRAWITEMSTRUCT lp = (LPDRAWITEMSTRUCT)lParam;

	HBRUSH hb = (HBRUSH)CreateSolidBrush(RGB(255,255,255));
	::FillRect(lp->hDC, &lp->rcItem, hb);
	DeleteObject(hb);
	if (lp->itemState & ODS_SELECTED)
	{
		m_BaseImage.Draw(lp->hDC, lp->rcItem.left , lp->rcItem.top , 150, 140, NULL, FALSE);
	}

	if (lp->itemID >= m_Image.size())
		return FALSE;
	
	if (lp->itemID)
	{
		lp->rcItem.top += 20;
		long xpos, ypos, xwidth, xheight;
		_CalcRect(m_Image[lp->itemID]->head.biWidth, m_Image[lp->itemID]->head.biHeight, 
			lp->rcItem.right - lp->rcItem.left, lp->rcItem.bottom - lp->rcItem.top,
			&xpos, &ypos, &xwidth, &xheight , 150, 100);
	
		m_Image[lp->itemID]->Draw(lp->hDC, lp->rcItem.left + xpos + 1, lp->rcItem.top + ypos, xwidth, xheight, NULL, FALSE);
		lp->rcItem.top -= 20;
	}

	char buff[1024];
	memset(buff, 0, sizeof(buff));
	m_SkinList.GetText(lp->itemID, buff);
	SetBkMode(lp->hDC, TRANSPARENT);
//	if ((lp->itemState & ODS_SELECTED))
//		SetTextColor(lp->hDC, RGB(255,255,255));
//	else
		SetTextColor(lp->hDC, RGB(0,0,0));
	DrawText(lp->hDC, buff, strlen(buff), &lp->rcItem, DT_TOP | DT_CENTER);
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsSkins::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_SkinList.Attach(GetDlgItem(IDC_SKINLIST));
	m_SkinList.ResetContent();

	while (m_Image.size())
	{
		CxImage *img = m_Image[0];
		delete img;
		m_Image.erase(m_Image.begin());
	}

	m_Image.push_back(NULL);
	m_SkinList.InsertString(0, _Settings.Translate(".. none .."));

	char buff[MAX_PATH];
	sprintf(buff, _Settings.m_MyPath);
	strcat(buff, "Skin\\*.smf");
	WIN32_FIND_DATA fd;
	memset(&fd, 0, sizeof(fd));
	HANDLE h = FindFirstFile(buff, &fd);
	if (h != INVALID_HANDLE_VALUE)
	{
		BOOL fin = FALSE;
		while (!fin)
		{
			// add the image
			Buffer b;
			char bf[16384];
			sprintf(bf, _Settings.m_MyPath);
			strcat(bf, "Skin\\");
			strcat(bf, fd.cFileName);

			int i = strlen(bf);
			if (i>3)
			{
				bf[i-3] = 0;
				strcat(bf, "png");
			}
			int handle = _open(bf, O_BINARY | O_RDONLY, S_IREAD | S_IWRITE);
			if (handle != (-1))
			{
				i = 0;
				do
				{
					i = _read(handle, bf, sizeof(bf));
					if (i>0)
						b.Append(bf, i);
				} while (i>0);
				_close(handle);

				if (b.Len())
				{
					CxImage *img = new CxImage();
					CxMemFile fTmp((BYTE*)b.Ptr(),b.Len());
					img->Decode(&fTmp, CXIMAGE_FORMAT_PNG);
					m_Image.push_back(img);
					i = strlen(fd.cFileName);
					if (i>4)
						fd.cFileName[i-4] = 0;
					m_SkinList.InsertString(m_Image.size()-1, fd.cFileName);
				}
			}

			if (!FindNextFile(h, &fd))
				fin = TRUE;
		}
	}

	::EnableWindow(GetDlgItem(IDB_APPLY), FALSE);
	SetDlgItemText(IDC_S1, _Settings.Translate("Following skin files are found"));
	SetDlgItemText(IDC_S2, _Settings.Translate("To add more skins, you can also paste them to 'Skin' subfolder. Add corresponding 100x100 PNG file for preview."));
	SetDlgItemText(IDB_APPLY, _Settings.Translate("&Apply"));
	SetDlgItemText(IDB_MORESKINS, _Settings.Translate("&More skins..."));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsSkins::Apply(void)
{
	return TRUE;
}

void CSettingsDlg::CSettingsSkins::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsSkins::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

LRESULT CSettingsDlg::CSettingsSkins::OnRefresh(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	OnInitDialog(uMsg, wParam, lParam, bHandled);
	HWND h = GetDlgItem(IDC_SKINLIST);
	::SendMessage(h, LB_SELECTSTRING, 0, (LPARAM)m_SkinThreadBuffer);
	OnApplyBtn(0,0,0,bHandled);
	::SendMessage(h, LB_SELECTSTRING, 0, (LPARAM)m_SkinThreadBuffer);


	return FALSE;
}

LRESULT CSettingsDlg::CSettingsSkins::OnApplyBtn(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{

	::EnableWindow(GetDlgItem(IDB_APPLY), FALSE);

#ifdef _SKINMAGICKEY
	if (_MainDlg.m_WearSkin)
		RemoveWindowSkin(_MainDlg.m_hWnd);
	_MainDlg.m_WearSkin = FALSE;		
#endif

	_Settings.m_Skin.Empty();

	long exstyle = _MainDlg.GetWindowLong(GWL_EXSTYLE);
	exstyle |= WS_EX_TOOLWINDOW;
	exstyle -= WS_EX_TOOLWINDOW;
	long style = _MainDlg.GetWindowLong(GWL_STYLE);
	style |= WS_SYSMENU;

	int i = m_SkinList.GetCurSel();
	if (i>0)
	{
		char buff[MAX_PATH], buff2[MAX_PATH];
		memset(buff2, 0, sizeof(buff2));
		m_SkinList.GetText(i, buff2);
		_Settings.m_Skin = buff2;
		_Settings.m_Skin.Append(".smf");

		strcpy(buff, _Settings.m_MyPath);
		strcat(buff, "Skin\\");
		strcat(buff, buff2);
		strcat(buff, ".smf");
					
#ifdef _SKINMAGICKEY

		if (LoadSkinFile(buff))
			_MainDlg.m_WearSkin = TRUE;

		if (_MainDlg.m_WearSkin)
		{
			exstyle |= WS_EX_TOOLWINDOW;
			style -= WS_SYSMENU;
			_MainDlg.SetWindowLong(GWL_STYLE, style);
			_MainDlg.SetWindowLong(GWL_EXSTYLE, exstyle);
			SetWindowSkin(_MainDlg.m_hWnd, "MainFrame");
		}
#endif
	}
	if (!_MainDlg.m_WearSkin)
	{
		_MainDlg.SetWindowLong(GWL_STYLE, style);
		_MainDlg.SetWindowLong(GWL_EXSTYLE, exstyle);
	}
	_MainDlg.ShowWindow(SW_HIDE);
	_MainDlg.ShowWindow(SW_MINIMIZE);
	_MainDlg.ShowWindow(SW_SHOWNORMAL);

	return TRUE;
}

LRESULT CSettingsDlg::CSettingsSkins::OnMoreSkins(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CDownloadSkinDlg dlg(m_hWnd);
	dlg.DoModal();
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsSkins::OnSkinList(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
//	ATLTRACE("wNotifyCode = %d, wID = %x\r\n", wNotifyCode, wID);
	if (wNotifyCode == LBN_SELCHANGE)
	{
		::EnableWindow(GetDlgItem(IDB_APPLY), TRUE);
	}

	return FALSE;
}

CSettingsDlg::CSettingsAutoAway::CSettingsAutoAway() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("System");
	mp += "\\";
	mp += _Settings.Translate("Auto Away");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Auto away settings.");
	TEXT2 = _Settings.Translate("Determines how will Wippien behave when system is inactive.");
}

CSettingsDlg::CSettingsAutoAway::~CSettingsAutoAway()
{
}

LRESULT CSettingsDlg::CSettingsAutoAway::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	char buff[1024];

	sprintf(buff, "%lu", _Settings.m_AutoAwayMinutes);
	SetDlgItemText(IDC_AUTOAWAYMINUTES, buff);

	sprintf(buff, "%lu", _Settings.m_ExtendedAwayMinutes);
	SetDlgItemText(IDC_EXTENDEDAWAYMINUTES, buff);
	
	sprintf(buff, "%lu", _Settings.m_AutoDisconnectMinutes);
	SetDlgItemText(IDC_AUTODISCONNECTMINUTES, buff);


	CComBSTR2 ma1 = _Settings.m_AutoAwayMessage;
	SetDlgItemText(IDC_AUTOAWAYMESSAGE, ma1.ToString());

	CComBSTR2 ma2 = _Settings.m_ExtendedAwayMessage;
	SetDlgItemText(IDC_EXTENDEDAWAYMESSAGE, ma2.ToString());

	if (_Settings.m_AutoSetBack)
		::SendMessage(GetDlgItem(IDC_SETBACKONACTIVITY), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SETBACKONACTIVITY), BM_SETCHECK, FALSE, NULL);


	SetDlgItemText(IDC_S1, _Settings.Translate("Wait"));
	SetDlgItemText(IDC_S2, _Settings.Translate("minutes before going to away mode (0 = disable)"));
	SetDlgItemText(IDC_S3, _Settings.Translate("Wait"));
	SetDlgItemText(IDC_S4, _Settings.Translate("minutes before going to extended away mode (0 = disable)"));
	SetDlgItemText(IDC_S5, _Settings.Translate("Wait"));
	SetDlgItemText(IDC_S6, _Settings.Translate("minutes before disconnecting (0 = disable)"));
	SetDlgItemText(IDC_S7, _Settings.Translate("Auto-Away message"));
	SetDlgItemText(IDC_S8, _Settings.Translate("Extended-Away message"));
	SetDlgItemText(IDC_SETBACKONACTIVITY, _Settings.Translate("Set back when activity is detected again."));

	return TRUE;
}

BOOL CSettingsDlg::CSettingsAutoAway::Apply(void)
{
	char buff[16384];
	
	*buff = 0;
	::SendMessage(GetDlgItem(IDC_AUTOAWAYMINUTES), WM_GETTEXT, 16384, (LPARAM)buff);
	_Settings.m_AutoAwayMinutes = atol(buff);

	*buff = 0;
	::SendMessage(GetDlgItem(IDC_EXTENDEDAWAYMINUTES), WM_GETTEXT, 16384, (LPARAM)buff);
	_Settings.m_ExtendedAwayMinutes = atol(buff);

	*buff = 0;
	::SendMessage(GetDlgItem(IDC_AUTODISCONNECTMINUTES), WM_GETTEXT, 16384, (LPARAM)buff);
	_Settings.m_AutoDisconnectMinutes = atol(buff);

	*buff = 0;
	::SendMessage(GetDlgItem(IDC_AUTOAWAYMESSAGE), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
	{
		_Settings.m_AutoAwayMessage = buff;
	}
	else
		_Settings.m_AutoAwayMessage.Empty();

	*buff = 0;
	::SendMessage(GetDlgItem(IDC_EXTENDEDAWAYMESSAGE), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
	{
		_Settings.m_ExtendedAwayMessage = buff;
	}
	else
		_Settings.m_ExtendedAwayMessage.Empty();

	if (::SendMessage(GetDlgItem(IDC_SETBACKONACTIVITY), BM_GETSTATE, NULL, NULL))
		_Settings.m_AutoSetBack = TRUE;
	else
		_Settings.m_AutoSetBack = FALSE;

	return TRUE;
}

void CSettingsDlg::CSettingsAutoAway::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsAutoAway::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

CSettingsDlg::CSettingsSystem::CSettingsSystem() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("System");
	TEXT1 = _Settings.Translate("Basic system messages.");
	TEXT2 = _Settings.Translate("Define Wippien application's behavior.");
}

CSettingsDlg::CSettingsSystem::~CSettingsSystem()
{
}

LRESULT CSettingsDlg::CSettingsSystem::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (_Settings.m_ShowInTaskbar)
		::SendMessage(GetDlgItem(IDC_SHOWINTASKBAR), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWINTASKBAR), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_DoAlign)
		::SendMessage(GetDlgItem(IDC_ALIGNTOBORDER), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_ALIGNTOBORDER), BM_SETCHECK, FALSE, NULL);
	
	if (_Settings.m_SnapToBorder)
		::SendMessage(GetDlgItem(IDC_SNAPTOBORDER), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SNAPTOBORDER), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_AutoHide)
		::SendMessage(GetDlgItem(IDC_AUTHOHIDEONINACTIVITY), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_AUTHOHIDEONINACTIVITY), BM_SETCHECK, FALSE, NULL);
	
	char buff[1024];
	sprintf(buff, "%d", _Settings.m_AutoHideSeconds);
	SetDlgItemText(IDC_AUTHOHIDEONINACTIVITYSECONDS, buff);
	
	if (_Settings.m_IsTopMost)
		::SendMessage(GetDlgItem(IDC_ALWAYSONTOP), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_ALWAYSONTOP), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_DoNotShow[DONOTSHOW_NOALLOWEXIT] == '0')
		::SendMessage(GetDlgItem(IDC_CONFIRMEXIT), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CONFIRMEXIT), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_PasswordProtectAll)
		::SendMessage(GetDlgItem(IDC_PASSWORDPROTECTEVERYTHING), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_PASSWORDPROTECTSETTINGS), BM_SETCHECK, TRUE, NULL);

	if (_Settings.m_UsePowerOptions)
		::SendMessage(GetDlgItem(IDC_USEPOWEROPTIONS), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_USEPOWEROPTIONS), BM_SETCHECK, FALSE, NULL);
	

	CComBSTR2 j = _Settings.m_PasswordProtectPassword;
	SetDlgItemText(IDC_PASSWORDPROTECTPASSWORD, j.ToString());

	CComBSTR2 j1 = _Settings.m_SettingsFolder;
	SetDlgItemText(IDC_SETTINGSFOLDER, j1.ToString());

	SetDlgItemText(IDC_SHOWINTASKBAR, _Settings.Translate("Show Wippien in taskbar"));
	SetDlgItemText(IDC_ALIGNTOBORDER, _Settings.Translate("Rearrange desktop when Wippien window is aligned to border"));
	SetDlgItemText(IDC_SNAPTOBORDER, _Settings.Translate("Snap to desktop border"));
	SetDlgItemText(IDC_AUTHOHIDEONINACTIVITY, _Settings.Translate("Auto hide if Wippien is inactive for"));
	SetDlgItemText(IDC_S1, _Settings.Translate("seconds"));
	SetDlgItemText(IDC_ALWAYSONTOP, _Settings.Translate("Always on top"));
	SetDlgItemText(IDC_CONFIRMEXIT, _Settings.Translate("Confirm exit from application"));
	SetDlgItemText(IDC_USEPOWEROPTIONS, _Settings.Translate("Disconnect on computer Sleep/Hibernate, Connect on Resume"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Password protect"));
	SetDlgItemText(IDC_PASSWORDPROTECTSETTINGS, _Settings.Translate("Settings only"));
	SetDlgItemText(IDC_PASSWORDPROTECTEVERYTHING, _Settings.Translate("Everything"));
	SetDlgItemText(IDC_S3, _Settings.Translate("Settings folder (changing below value requires wippien restart)"));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsSystem::Apply(void)
{
	BOOL wastopmost = _Settings.m_IsTopMost;
	BOOL wasaligned = _Settings.m_DoAlign;

	if (::SendMessage(GetDlgItem(IDC_SHOWINTASKBAR), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowInTaskbar = TRUE;
	else
		_Settings.m_ShowInTaskbar = FALSE;

	if (::SendMessage(GetDlgItem(IDC_ALIGNTOBORDER), BM_GETSTATE, NULL, NULL))
		_Settings.m_DoAlign = TRUE;
	else
		_Settings.m_DoAlign = FALSE;

	if (::SendMessage(GetDlgItem(IDC_SNAPTOBORDER), BM_GETSTATE, NULL, NULL))
		_Settings.m_SnapToBorder = TRUE;
	else
		_Settings.m_SnapToBorder = FALSE;

	if (::SendMessage(GetDlgItem(IDC_AUTHOHIDEONINACTIVITY), BM_GETSTATE, NULL, NULL))
		_Settings.m_AutoHide = TRUE;
	else
		_Settings.m_AutoHide = FALSE;
	
	char buff[16384];
	memset(buff, 0, 16384);
	::SendMessage(GetDlgItem(IDC_AUTHOHIDEONINACTIVITYSECONDS), WM_GETTEXT, 16384, (LPARAM)buff);
	_Settings.m_AutoHideSeconds = atoi(buff);
	if (!_Settings.m_AutoHideSeconds)
		_Settings.m_AutoHide = FALSE;

	if (::SendMessage(GetDlgItem(IDC_ALWAYSONTOP), BM_GETSTATE, NULL, NULL))
		_Settings.m_IsTopMost = TRUE;
	else
		_Settings.m_IsTopMost = FALSE;

	if (::SendMessage(GetDlgItem(IDC_CONFIRMEXIT), BM_GETSTATE, NULL, NULL))
		_Settings.m_DoNotShow[DONOTSHOW_NOALLOWEXIT] = '0';
	else
		_Settings.m_DoNotShow[DONOTSHOW_NOALLOWEXIT] = '1';

	if (::SendMessage(GetDlgItem(IDC_USEPOWEROPTIONS), BM_GETCHECK, NULL, NULL))
		_Settings.m_UsePowerOptions = 1;
	else
		_Settings.m_UsePowerOptions = 0;
	
	if (_MainDlg.m_EmptyWin)
	{
		if (_Settings.m_ShowInTaskbar)
			::ShowWindow(_MainDlg.m_EmptyWin, SW_SHOW);
		else
			::ShowWindow(_MainDlg.m_EmptyWin, SW_HIDE);
	}
	if (wastopmost != _Settings.m_IsTopMost)
	{
		if (_Settings.m_IsTopMost)
			::SetWindowPos(_MainDlg.m_hWnd, HWND_TOPMOST,  0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		else
			::SetWindowPos(_MainDlg.m_hWnd, HWND_NOTOPMOST,  0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	}
	if (wasaligned != _Settings.m_DoAlign)
	{
//		if (_Settings.m_DoAlign)
		{
			_MainDlg.CheckAppbar();
/*			RECT rc;
			::GetWindowRect(_MainDlg.m_hWnd, &rc);
			::ShowWindow(_MainDlg.m_hWnd, SW_HIDE);
			::SetWindowPos(_MainDlg.m_hWnd, _Settings.m_IsTopMost?HWND_TOPMOST:HWND_NOTOPMOST, 0,0 , 100,100, SWP_DRAWFRAME); 
			::SetWindowPos(_MainDlg.m_hWnd, _Settings.m_IsTopMost?HWND_TOPMOST:HWND_NOTOPMOST, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, SWP_DRAWFRAME); 
			::ShowWindow(_MainDlg.m_hWnd, SW_SHOW);
//			::MoveWindow(_MainDlg.m_hWnd, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, TRUE);
*/
		}
	}

//	if (::SendMessage(GetDlgItem(IDC_PASSWORDPROTECT), BM_GETSTATE, NULL, NULL))
//		_Settings.m_PasswordProtect = TRUE;
//	else
//		_Settings.m_PasswordProtect = FALSE;

	if (::SendMessage(GetDlgItem(IDC_PASSWORDPROTECTSETTINGS), BM_GETCHECK, NULL, NULL))
		_Settings.m_PasswordProtectAll = FALSE;
	else
		_Settings.m_PasswordProtectAll = TRUE;

	buff[0] = 0;
	::SendMessage(GetDlgItem(IDC_PASSWORDPROTECTPASSWORD), WM_GETTEXT, 16384, (LPARAM)buff);
	_Settings.m_PasswordProtectPassword = buff;
	if (!_Settings.m_PasswordProtectPassword.Length())
		_Settings.m_NowProtected = FALSE;

	buff[0] = 0;
	::SendMessage(GetDlgItem(IDC_SETTINGSFOLDER), WM_GETTEXT, 16384, (LPARAM)buff);
	_Settings.m_SettingsFolder = buff;

	return TRUE;
}

void CSettingsDlg::CSettingsSystem::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsSystem::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

CSettingsDlg::CSettingsContacts::CSettingsContacts() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("Contacts");
	TEXT1 = _Settings.Translate("Contacts basic settings.");
	TEXT2 = "";
}

CSettingsDlg::CSettingsContacts::~CSettingsContacts()
{
}

LRESULT CSettingsDlg::CSettingsContacts::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

	if (_Settings.m_DeleteContactsOnStartup)
		::SendMessage(GetDlgItem(IDC_CONTACTS_DELETEONSTARTUP), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CONTACTS_DELETEONSTARTUP), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_DeleteContactsOnConnect)
		::SendMessage(GetDlgItem(IDC_CONTACTS_DELETEONCONNECT), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CONTACTS_DELETEONCONNECT), BM_SETCHECK, FALSE, NULL);
	
	if (_Settings.m_AutoConnectVPNOnNetwork)
		::SendMessage(GetDlgItem(IDC_AUTOCONNECTONTRAFFIC), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_AUTOCONNECTONTRAFFIC), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowNotificationPopup)
		::SendMessage(GetDlgItem(IDC_SHOWNOTIFICATION), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWNOTIFICATION), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_AutoConnectVPNOnStartup)
		::SendMessage(GetDlgItem(IDC_AUTOCONNECTONSTARTUP), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_AUTOCONNECTONSTARTUP), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_AuthContacts == 0)
			::SendMessage(GetDlgItem(IDC_CONTACTS_AUTH0), BM_SETCHECK, BST_CHECKED, NULL);
	else
	if (_Settings.m_AuthContacts == 1)
			::SendMessage(GetDlgItem(IDC_CONTACTS_AUTH1), BM_SETCHECK, BST_CHECKED, NULL);
	else
	if (_Settings.m_AuthContacts == 2)
			::SendMessage(GetDlgItem(IDC_CONTACTS_AUTH2), BM_SETCHECK, BST_CHECKED, NULL);

	SetDlgItemText(IDC_S1, _Settings.Translate("Contact list persistance"));
	SetDlgItemText(IDC_CONTACTS_DELETEONSTARTUP, _Settings.Translate("Delete all local contacts on startup"));
	SetDlgItemText(IDC_CONTACTS_DELETEONCONNECT, _Settings.Translate("Delete all local contacts on connect"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Contacts subscription authorization"));
	SetDlgItemText(IDC_CONTACTS_AUTH0, _Settings.Translate("Never authorize"));
	SetDlgItemText(IDC_CONTACTS_AUTH1, _Settings.Translate("Ask me"));
	SetDlgItemText(IDC_CONTACTS_AUTH2, _Settings.Translate("Always authorize"));
	SetDlgItemText(IDC_AUTOCONNECTONSTARTUP, _Settings.Translate("Automatically establish VPN on startup"));
	SetDlgItemText(IDC_AUTOCONNECTONTRAFFIC, _Settings.Translate("Automatically establish VPN on network traffic"));
	SetDlgItemText(IDC_SHOWNOTIFICATION, _Settings.Translate("Show notification popup window when contact goes online/offline"));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsContacts::Apply(void)
{
	if (::SendMessage(GetDlgItem(IDC_CONTACTS_DELETEONSTARTUP), BM_GETSTATE, NULL, NULL))
		_Settings.m_DeleteContactsOnStartup = TRUE;
	else
		_Settings.m_DeleteContactsOnStartup = FALSE;

	if (::SendMessage(GetDlgItem(IDC_CONTACTS_DELETEONCONNECT), BM_GETSTATE, NULL, NULL))
		_Settings.m_DeleteContactsOnConnect = TRUE;
	else
		_Settings.m_DeleteContactsOnConnect = FALSE;


	if (::SendMessage(GetDlgItem(IDC_SHOWNOTIFICATION), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowNotificationPopup = TRUE;
	else
		_Settings.m_ShowNotificationPopup = FALSE;

	if (::SendMessage(GetDlgItem(IDC_AUTOCONNECTONTRAFFIC), BM_GETSTATE, NULL, NULL))
		_Settings.m_AutoConnectVPNOnNetwork = TRUE;
	else
		_Settings.m_AutoConnectVPNOnNetwork = FALSE;

	if (::SendMessage(GetDlgItem(IDC_AUTOCONNECTONSTARTUP), BM_GETSTATE, NULL, NULL))
		_Settings.m_AutoConnectVPNOnStartup = TRUE;
	else
		_Settings.m_AutoConnectVPNOnStartup = FALSE;
	
	if (::SendMessage(GetDlgItem(IDC_CONTACTS_AUTH0), BM_GETSTATE, NULL, NULL))
		_Settings.m_AuthContacts = 0;
	else	
	if (::SendMessage(GetDlgItem(IDC_CONTACTS_AUTH1), BM_GETSTATE, NULL, NULL))
		_Settings.m_AuthContacts = 1;
	else
	if (::SendMessage(GetDlgItem(IDC_CONTACTS_AUTH2), BM_GETSTATE, NULL, NULL))
		_Settings.m_AuthContacts = 2;

	return TRUE;
}

void CSettingsDlg::CSettingsContacts::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsContacts::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

CSettingsDlg::CSettingsSystemUpdate::CSettingsSystemUpdate() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("System");
	mp += "\\";
	mp += _Settings.Translate("Updates");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Update definitions.");
	TEXT2 = _Settings.Translate("Define when and how will Wippien update itself.");
}

CSettingsDlg::CSettingsSystemUpdate::~CSettingsSystemUpdate()
{
}

LRESULT CSettingsDlg::CSettingsSystemUpdate::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (_Settings.m_CheckUpdate)
		::SendMessage(GetDlgItem(IDC_CHECKUPDATE), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CHECKUPDATE), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_CheckUpdateConnect)
		::SendMessage(GetDlgItem(IDC_CHECKUPDATE_CONNECT), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CHECKUPDATE_CONNECT), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_CheckUpdateTimed)
		::SendMessage(GetDlgItem(IDC_CHECKUPDATE_TIMED), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CHECKUPDATE_TIMED), BM_SETCHECK, FALSE, NULL);

	char buff[1024];
	_itoa(_Settings.m_CheckUpdateTimedNum, buff, 10);
	SetDlgItemText(IDC_CHECKUPDATE_TIMEDNUM, buff);

	if (_Settings.m_CheckUpdateSilently)
		::SendMessage(GetDlgItem(IDC_CHECKUPDATESILENTLY), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_CHECKUPDATESILENTLY), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowUpdaterMessages)
		::SendMessage(GetDlgItem(IDC_SHOWUPDATERMESSAGES), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWUPDATERMESSAGES), BM_SETCHECK, FALSE, NULL);
	

	SetDlgItemText(IDC_CHECKUPDATE, _Settings.Translate("Check for updates when Wippien starts"));
	SetDlgItemText(IDC_CHECKUPDATE_CONNECT, _Settings.Translate("Check for updates on Connect"));
	SetDlgItemText(IDC_CHECKUPDATE_TIMED, _Settings.Translate("Check for updates each"));
	SetDlgItemText(IDC_CHECKUPDATESILENTLY, _Settings.Translate("Update and restart application silently"));
	SetDlgItemText(IDC_SHOWUPDATERMESSAGES, _Settings.Translate("Show system messages received from updater"));
	SetDlgItemText(IDC_UPDATE_CHECKNOW, _Settings.Translate("&Check now"));
	SetDlgItemText(IDC_S1, _Settings.Translate("minutes"));
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsSystemUpdate::OnCheckNow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	// should we check for updates?
//	CUpdateHandler *h = new CUpdateHandler();
	// already have something new?
	if (_UpdateHandler)
	{
		_UpdateHandler->m_Silently = FALSE;
/* changed with wodappupdate
	if (h->ReplaceUpdates())
	{
		return FALSE;
	}
*/
//	if (_Settings.m_CheckUpdate)
		_UpdateHandler->DownloadUpdates(FALSE);
	}
	return 0;
}


BOOL CSettingsDlg::CSettingsSystemUpdate::Apply(void)
{

	if (::SendMessage(GetDlgItem(IDC_CHECKUPDATE), BM_GETSTATE, NULL, NULL))
		_Settings.m_CheckUpdate = TRUE;
	else
		_Settings.m_CheckUpdate = FALSE;

	if (::SendMessage(GetDlgItem(IDC_CHECKUPDATE_CONNECT), BM_GETSTATE, NULL, NULL))
		_Settings.m_CheckUpdateConnect = TRUE;
	else
		_Settings.m_CheckUpdateConnect = FALSE;

	if (::SendMessage(GetDlgItem(IDC_CHECKUPDATE_TIMED), BM_GETSTATE, NULL, NULL))
		_Settings.m_CheckUpdateTimed = TRUE;
	else
		_Settings.m_CheckUpdateTimed = FALSE;

	char buff[16384];
	memset(buff, 0, 16384);
	::SendMessage(GetDlgItem(IDC_CHECKUPDATE_TIMEDNUM), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
		_Settings.m_CheckUpdateTimedNum = atol(buff);

	if (::SendMessage(GetDlgItem(IDC_CHECKUPDATESILENTLY), BM_GETSTATE, NULL, NULL))
		_Settings.m_CheckUpdateSilently = TRUE;
	else
		_Settings.m_CheckUpdateSilently = FALSE;

	if (::SendMessage(GetDlgItem(IDC_SHOWUPDATERMESSAGES), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowUpdaterMessages = TRUE;
	else
		_Settings.m_ShowUpdaterMessages = FALSE;
	
	return TRUE;
}

void CSettingsDlg::CSettingsSystemUpdate::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsSystemUpdate::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}
CSettingsDlg::CSettingsMTU::CSettingsMTU() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Network");
	mp += "\\";
	mp += _Settings.Translate("MTU");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Please determine maximum transfer unit.");
	TEXT2 = _Settings.Translate("If some peer doesn't respond to large packets...");
}

CSettingsDlg::CSettingsMTU::~CSettingsMTU()
{

}

LRESULT CSettingsDlg::CSettingsMTU::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsMTU::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (_Settings.m_FixedMTU)
		::SendMessage(GetDlgItem(IDC_USEFIXEDMTU), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_AUTODETECTMTU), BM_SETCHECK, TRUE, NULL);

	char buff[1024];
	sprintf(buff, "%u", _Settings.m_FixedMTUNum);
	SetDlgItemText(IDC_USEFIXEDMTUNUM, buff);

	SetDlgItemText(IDC_S1, _Settings.Translate("MTU setting defines maximum UDP packet that can be sent to peers. If your network connection has lower MTU value than wippien, it is possible that peer never receives large packets."));
	SetDlgItemText(IDC_USEFIXEDMTU, _Settings.Translate("Use fixed MTU"));
	SetDlgItemText(IDC_AUTODETECTMTU, _Settings.Translate("Auto-detect MTU"));
	SetDlgItemText(IDC_S2, _Settings.Translate("(will become effective as peers reconnect)"));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsMTU::Apply(void)
{
	if (::SendMessage(GetDlgItem(IDC_USEFIXEDMTU), BM_GETCHECK, NULL, NULL))
		_Settings.m_FixedMTU = TRUE;
	else
		_Settings.m_FixedMTU = FALSE;

	char buff[16384];
	memset(buff, 0, 16384);
	::SendMessage(GetDlgItem(IDC_USEFIXEDMTUNUM), WM_GETTEXT, 16384, (LPARAM)buff);
	if (buff[0])
		_Settings.m_FixedMTUNum = atol(buff);

	return TRUE;
}
void CSettingsDlg::CSettingsMTU::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsMTU::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}


CSettingsDlg::CSettingsSound::CSettingsSound() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("System");
	mp += "\\";
	mp += _Settings.Translate("Sounds");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Setup sounds to be played on various events.");
	TEXT2 = _Settings.Translate("Choose internal ones, or browse for WAV files.");
	m_IChange = FALSE;
}

CSettingsDlg::CSettingsSound::~CSettingsSound()
{

}


LRESULT CSettingsDlg::CSettingsSound::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsSound::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_SoundList.Attach(GetDlgItem(IDC_SOUNDEVENTS));
	m_SoundList.ResetContent();


	m_SoundList.InsertString(0, _Settings.Translate("Contact online"));
	m_SoundList.InsertString(1, _Settings.Translate("Contact offline"));
	m_SoundList.InsertString(2, _Settings.Translate("Message in"));
	m_SoundList.InsertString(3, _Settings.Translate("Message out"));
	m_SoundList.InsertString(4, _Settings.Translate("Error"));

	strcpy(m_Sound[0], ((CComBSTR2)_Notify.m_Online).ToString());
	strcpy(m_Sound[1], ((CComBSTR2)_Notify.m_Offline).ToString());
	strcpy(m_Sound[2], ((CComBSTR2)_Notify.m_MsgIn).ToString());
	strcpy(m_Sound[3], ((CComBSTR2)_Notify.m_MsgOut).ToString());
	strcpy(m_Sound[4], ((CComBSTR2)_Notify.m_Error).ToString());
	
	SetDlgItemText(IDC_S1, _Settings.Translate("Program events"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Sound (leave empty for default sound)"));
	SetDlgItemText(IDB_PLAY, _Settings.Translate("&Play"));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsSound::Apply(void)
{
	char buff[16384];
	*buff = 0;

	_Notify.m_Online.Empty();
	_Notify.m_Online = m_Sound[0];

	_Notify.m_Offline.Empty();
	_Notify.m_Offline = m_Sound[1];

	_Notify.m_MsgIn.Empty();
	_Notify.m_MsgIn = m_Sound[2];

	_Notify.m_MsgOut.Empty();
	_Notify.m_MsgOut = m_Sound[3];

	_Notify.m_Error.Empty();
	_Notify.m_Error = m_Sound[4];

	return TRUE;
}

void CSettingsDlg::CSettingsSound::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsSound::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

LRESULT CSettingsDlg::CSettingsSound::OnBrowseCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char *szFilter = _Settings.Translate("WAV files (*.wav)\0*.wav\0All Files (*.*)\0*\0\0");
	CFileDialog cf(TRUE, NULL, NULL, OFN_HIDEREADONLY | OFN_ENABLESIZING  | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST, 
		NULL, m_hWnd);

	cf.m_ofn.lpstrFilter = szFilter;
	cf.m_ofn.lpstrDefExt = "wav";

	if (cf.DoModal() == IDOK)
	{
		m_IChange = FALSE;
		SendMessage(GetDlgItem(IDC_SOUND), WM_SETTEXT, 0, (LPARAM)cf.m_szFileName);
	}
	return 0;
}

LRESULT CSettingsDlg::CSettingsSound::OnPlayCmd(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	char buff[1024];

	memset(buff, 0, sizeof(buff));
	::SendMessage(GetDlgItem(IDC_SOUND), WM_GETTEXT, sizeof(buff), (LPARAM)buff);
	if (*buff)
	{
		sndPlaySound(buff, SND_ASYNC);
	}
	else
	{
		int i = m_SoundList.GetCurSel();
		if (i>=0)
		{
			switch (i)
			{
				case 0:
					_Notify.Beep(NULL, IDR_SOUNDON, TRUE);
					break;
				case 1:
					_Notify.Beep(NULL, IDR_SOUNDOFF, TRUE);
					break;
				case 2:
					_Notify.Beep(NULL, IDR_SOUNDMSGIN, TRUE);
					break;
				case 3:
					_Notify.Beep(NULL, IDR_SOUNDMSGOUT, TRUE);
					break;
				case 4:
					_Notify.Beep(NULL, IDR_SOUNDNONO, TRUE);
					break;
			}
		}
	}
	return 0;
}

LRESULT CSettingsDlg::CSettingsSound::OnSoundList(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
//	ATLTRACE("wNotifyCode = %d, wID = %x\r\n", wNotifyCode, wID);
	if (wNotifyCode == LBN_SELCHANGE)
	{
		m_IChange = TRUE;
		SendMessage(GetDlgItem(IDC_SOUND), WM_SETTEXT, 0, (LPARAM)"");
		int i = m_SoundList.GetCurSel();
		if (i>=0)
		{
			SendMessage(GetDlgItem(IDC_SOUND), WM_SETTEXT, 0, (LPARAM)m_Sound[i]);
		}
		m_IChange = FALSE;
	}

	return FALSE;
}

LRESULT CSettingsDlg::CSettingsSound::OnEditSound(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (wNotifyCode == EN_CHANGE)
	{
		if (!m_IChange)
		{
			::EnableWindow(GetDlgItem(IDB_APPLY), TRUE);
			int i = m_SoundList.GetCurSel();
			if (i>=0)
			{
				memset(m_Sound[i], 0, MAX_PATH);
				::SendMessage(GetDlgItem(IDC_SOUND), WM_GETTEXT, MAX_PATH, (LPARAM)m_Sound[i]);
			}
		}
	}

	return FALSE;
}

CSettingsDlg::CSettingsContactsHide::CSettingsContactsHide() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Contacts");
	mp += "\\";
	mp += _Settings.Translate("Hide");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Hide contacts.");
	TEXT2 = _Settings.Translate("You can hide some contacts from your sight.");

	m_Changed = FALSE;
}

CSettingsDlg::CSettingsContactsHide::~CSettingsContactsHide() 
{

}

LRESULT CSettingsDlg::CSettingsContactsHide::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsContactsHide::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	m_HiddenContacts.Attach(GetDlgItem(IDC_HIDDENCONTACTS));

	char *from = _Settings.m_HiddenContactsBuffer.Ptr();
	for (int i=0;i<(signed)_Settings.m_HiddenContacts.size();i++)
	{
		int j = _Settings.m_HiddenContacts[i];
		m_HiddenContacts.AddString(from+j);
	}

	SetDlgItemText(IDC_S1, _Settings.Translate("Currently hidden contacts"));
	SetDlgItemText(IDC_BLOCKNEWCONTACT, _Settings.Translate("Hide new contact"));
	SetDlgItemText(IDC_UNBLOCKCONTACT, _Settings.Translate("Unhide contact"));
	SetDlgItemText(IDC_S2, _Settings.Translate("All contacts listed here *NEVER* appear in Wippien.. They don't exist for you anymore. This way you can ignore people completely."));

	return TRUE;
}

BOOL CSettingsDlg::CSettingsContactsHide::Apply(void)
{
	if (m_Changed)
	{
		char buff[16384];
		while (_Settings.m_HiddenContacts.size())
			_Settings.m_HiddenContacts.erase(_Settings.m_HiddenContacts.begin());
		_Settings.m_HiddenContactsBuffer.Clear();

		for (int i=0;i<m_HiddenContacts.GetCount();i++)
		{
			buff[0] = 0;
			if (m_HiddenContacts.GetText(i, buff))
			{
				char *a = strchr(buff, '/');
				if (a)
					*a = 0;

				char *to = NULL;
				int l = strlen(buff);
				_Settings.m_HiddenContactsBuffer.AppendSpace(&to, l+1);
				memset(to, 0, l+1);
				memcpy(to, buff, l);
				_Settings.m_HiddenContacts.push_back(to - _Settings.m_HiddenContactsBuffer.Ptr());
			}
			
		}
		_MainDlg.m_UserList.PostMessage(WM_REFRESH, NULL, 0);
	}
	return TRUE;
}

void CSettingsDlg::CSettingsContactsHide::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsContactsHide::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

LRESULT CSettingsDlg::CSettingsContactsHide::OnBlockNewContact(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	CHiddenContact ndlg;
	ndlg.DoModal();

	if (ndlg.m_SelectedContact[0])
	{
		m_Changed = TRUE;
		m_HiddenContacts.AddString(ndlg.m_SelectedContact);					
	}

	return 0;
}

LRESULT CSettingsDlg::CSettingsContactsHide::OnUnblockContact(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_HiddenContacts.GetCurSel()>=0)
	{
		m_Changed = TRUE;
		m_HiddenContacts.DeleteString(m_HiddenContacts.GetCurSel());
		::EnableWindow(GetDlgItem(IDC_UNBLOCKCONTACT), FALSE);
	}
	return 0;
}

LRESULT CSettingsDlg::CSettingsContactsHide::OnListClick(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	if (m_HiddenContacts.GetCurSel()>=0)
		::EnableWindow(GetDlgItem(IDC_UNBLOCKCONTACT), TRUE);
	return 0;
}

CSettingsDlg::CSettingsAppearance::CSettingsAppearance() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("Appearance");
	TEXT1 = _Settings.Translate("Appearance settings.");
	TEXT2 = _Settings.Translate("Define how will Wippien's GUI appear to you.");
}

CSettingsDlg::CSettingsAppearance::~CSettingsAppearance()
{
}

LRESULT CSettingsDlg::CSettingsAppearance::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	if (_Settings.m_ShowMyPicture)
		::SendMessage(GetDlgItem(IDC_SHOWPICTURE), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWPICTURE), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowMyName)
		::SendMessage(GetDlgItem(IDC_SHOWNAME), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWNAME), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowMyIP)
		::SendMessage(GetDlgItem(IDC_SHOWIP), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWIP), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowMyStatus)
		::SendMessage(GetDlgItem(IDC_SHOWSTATUS), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWSTATUS), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowContactPicture)
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTPICTURE), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTPICTURE), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowContactLastOnline)
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTLASTONLINE), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTLASTONLINE), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowContactActivity)
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTACTIVITYMONITOR), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTACTIVITYMONITOR), BM_SETCHECK, FALSE, NULL);
	
	//	if (_Settings.m_ShowContactName)
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTNAME), BM_SETCHECK, TRUE, NULL);
//	else
//		::SendMessage(GetDlgItem(IDC_SHOWCONTACTNAME), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowContactIP)
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTIP), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTIP), BM_SETCHECK, FALSE, NULL);

	if (_Settings.m_ShowContactStatus)
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTSTATUS), BM_SETCHECK, TRUE, NULL);
	else
		::SendMessage(GetDlgItem(IDC_SHOWCONTACTSTATUS), BM_SETCHECK, FALSE, NULL);	


	SetDlgItemText(IDC_S1, _Settings.Translate("Visible details"));
	SetDlgItemText(IDC_SHOWPICTURE, _Settings.Translate("Avatar picture"));
	SetDlgItemText(IDC_SHOWNAME, _Settings.Translate("Name"));
	SetDlgItemText(IDC_SHOWIP, _Settings.Translate("IP address"));
	SetDlgItemText(IDC_SHOWSTATUS, _Settings.Translate("Status text"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Contact's visible details"));
	SetDlgItemText(IDC_SHOWCONTACTPICTURE, _Settings.Translate("Avatar picture"));
	SetDlgItemText(IDC_SHOWCONTACTNAME, _Settings.Translate("Name"));
	SetDlgItemText(IDC_SHOWCONTACTLASTONLINE, _Settings.Translate("Last online time"));
	SetDlgItemText(IDC_SHOWCONTACTIP, _Settings.Translate("IP address"));
	SetDlgItemText(IDC_SHOWCONTACTSTATUS, _Settings.Translate("Status text"));
	SetDlgItemText(IDC_SHOWCONTACTACTIVITYMONITOR, _Settings.Translate("Activity Monitor"));


	return TRUE;
}

LRESULT CSettingsDlg::CSettingsAppearance::OnCheckNow(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	// should we check for updates?
//	CUpdateHandler *h = new CUpdateHandler();
	// already have something new?
	if (_UpdateHandler)
	{
		_UpdateHandler->m_Silently = FALSE;
/* changed with wodappupdate
	if (h->ReplaceUpdates())
	{
		return FALSE;
	}
*/
//	if (_Settings.m_CheckUpdate)
		_UpdateHandler->DownloadUpdates(FALSE);
	}
	return 0;
}


BOOL CSettingsDlg::CSettingsAppearance::Apply(void)
{
	BOOL oldmypic = _Settings.m_ShowMyPicture;
	if (::SendMessage(GetDlgItem(IDC_SHOWPICTURE), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowMyPicture = TRUE;
	else
		_Settings.m_ShowMyPicture = FALSE;

	BOOL oldmyname = _Settings.m_ShowMyName;
	if (::SendMessage(GetDlgItem(IDC_SHOWNAME), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowMyName = TRUE;
	else
		_Settings.m_ShowMyName = FALSE;

	BOOL oldmyip = _Settings.m_ShowMyIP;
	if (::SendMessage(GetDlgItem(IDC_SHOWIP), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowMyIP = TRUE;
	else
		_Settings.m_ShowMyIP = FALSE;

	BOOL oldmystatus = _Settings.m_ShowMyStatus;
	if (::SendMessage(GetDlgItem(IDC_SHOWSTATUS), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowMyStatus = TRUE;
	else
		_Settings.m_ShowMyStatus = FALSE;

	BOOL oldpic = _Settings.m_ShowContactPicture;
	if (::SendMessage(GetDlgItem(IDC_SHOWCONTACTPICTURE), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowContactPicture = TRUE;
	else
		_Settings.m_ShowContactPicture = FALSE;

	BOOL oldlastonline = _Settings.m_ShowContactLastOnline;
	if (::SendMessage(GetDlgItem(IDC_SHOWCONTACTLASTONLINE), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowContactLastOnline = TRUE;
	else
		_Settings.m_ShowContactLastOnline = FALSE;

	BOOL lastactivity = _Settings.m_ShowContactActivity;
	if (::SendMessage(GetDlgItem(IDC_SHOWCONTACTACTIVITYMONITOR), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowContactActivity= TRUE;
	else
		_Settings.m_ShowContactActivity = FALSE;
	
/*	BOOL oldname = _Settings.m_ShowContactName;
	if (::SendMessage(GetDlgItem(IDC_SHOWCONTACTNAME), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowContactName = TRUE;
	else
		_Settings.m_ShowContactName = FALSE;
*/
	BOOL oldip = _Settings.m_ShowContactIP;
	if (::SendMessage(GetDlgItem(IDC_SHOWCONTACTIP), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowContactIP = TRUE;
	else
		_Settings.m_ShowContactIP = FALSE;

	BOOL oldstatus = _Settings.m_ShowContactStatus;
	if (::SendMessage(GetDlgItem(IDC_SHOWCONTACTSTATUS), BM_GETSTATE, NULL, NULL))
		_Settings.m_ShowContactStatus = TRUE;
	else
		_Settings.m_ShowContactStatus = FALSE;

	if (oldip != _Settings.m_ShowContactIP /*|| oldname != _Settings.m_ShowContactName */|| oldpic != _Settings.m_ShowContactPicture || oldstatus != _Settings.m_ShowContactStatus || oldlastonline != _Settings.m_ShowContactLastOnline || lastactivity != _Settings.m_ShowContactActivity)
		_MainDlg.m_UserList.PostMessage(WM_REFRESH, NULL, 0);

	if (oldmystatus != _Settings.m_ShowMyStatus || oldmypic != _Settings.m_ShowMyPicture || oldmyname != _Settings.m_ShowMyName || oldmyip != _Settings.m_ShowMyIP || lastactivity != _Settings.m_ShowContactActivity)
		_MainDlg.RecalcInternalItems();

	return TRUE;
}

void CSettingsDlg::CSettingsAppearance::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsAppearance::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}

CSettingsDlg::CSettingsContactsSort::CSettingsContactsSort() : _CSettingsTemplate()
{
	CComBSTR mp = _Settings.Translate("Contacts");
	mp += "\\";
	mp += _Settings.Translate("Sort");
	mPATH = mp;
	PATH = mPATH.ToString();
	TEXT1 = _Settings.Translate("Sorts contacts.");
	TEXT2 = _Settings.Translate("Pick one of sort options.");
}

CSettingsDlg::CSettingsContactsSort::~CSettingsContactsSort()
{

}

LRESULT CSettingsDlg::CSettingsContactsSort::OnPaint(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
{
	CPaintDC dcPaint(m_hWnd);
	return TRUE;
}


LRESULT CSettingsDlg::CSettingsContactsSort::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{
	int dlgitem = IDC_SORTBYNAME;
	switch (_Settings.m_SortContacts)
	{
		case 0:
			dlgitem = IDC_SORTNONE;
			break;

		case 2:
			dlgitem = IDC_SORTBYJID;
			break;

		case 3:
			dlgitem = IDC_SORTBYIPBOTTOM;
			break;

		case 4:
			dlgitem = IDC_SORTBYIPTOP;
			break;
	}
	::SendMessage(GetDlgItem(dlgitem), BM_SETCHECK, TRUE, NULL);

	SetDlgItemText(IDC_S1, _Settings.Translate("Sort contact list"));
	SetDlgItemText(IDC_SORTNONE, _Settings.Translate("unsorted"));
	SetDlgItemText(IDC_SORTBYNAME, _Settings.Translate("By name"));
	SetDlgItemText(IDC_SORTBYJID, _Settings.Translate("By JID"));
	SetDlgItemText(IDC_SORTBYIPBOTTOM, _Settings.Translate("by IP (without IP goes bottom)"));
	SetDlgItemText(IDC_SORTBYIPTOP, _Settings.Translate("by IP (without IP goes top)"));
	return TRUE;
}

BOOL CSettingsDlg::CSettingsContactsSort::Apply(void)
{
	int oldsort = _Settings.m_SortContacts;

	_Settings.m_SortContacts = 1;

	if (::SendMessage(GetDlgItem(IDC_SORTNONE), BM_GETCHECK, NULL, NULL)) _Settings.m_SortContacts = 0;
	if (::SendMessage(GetDlgItem(IDC_SORTBYJID), BM_GETCHECK, NULL, NULL)) _Settings.m_SortContacts = 2;
	if (::SendMessage(GetDlgItem(IDC_SORTBYIPBOTTOM), BM_GETCHECK, NULL, NULL)) _Settings.m_SortContacts = 3;
	if (::SendMessage(GetDlgItem(IDC_SORTBYIPTOP), BM_GETCHECK, NULL, NULL)) _Settings.m_SortContacts = 4;

	if (_Settings.m_SortContacts != oldsort)
	{
		_MainDlg.m_UserList.m_SortedUsersBuffer.Clear();
		_MainDlg.m_UserList.PostMessage(WM_REFRESH, NULL, 0);
	}

	return TRUE;
}
void CSettingsDlg::CSettingsContactsSort::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsContactsSort::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}


CSettingsDlg::CSettingsChatRooms::CSettingsChatRooms() : _CSettingsTemplate()
{
	PATH = _Settings.Translate("ChatRooms");
	TEXT1 = _Settings.Translate("Browse or create chat rooms. (STILL IN BETA!!)");
	TEXT2 = _Settings.Translate("Specify or browse for a room to join or create new one.");

	_Jabber->m_ServiceRegisterHwnd = NULL;
	_Jabber->m_ServiceRefreshHwnd = NULL;

}

CSettingsDlg::CSettingsChatRooms::~CSettingsChatRooms()
{
}

LRESULT CSettingsDlg::CSettingsChatRooms::OnInitDialog(UINT /*uMsg*/, WPARAM /*wParam*/, LPARAM /*lParam*/, BOOL& /*bHandled*/)
{

	m_ServicesList.Attach(GetDlgItem(IDC_CHATROOM_GATEWAYLIST));
	m_RoomList.Attach(GetDlgItem(IDC_CHATROOM_ROOMLIST));
	m_NewRoomServicesList.Attach(GetDlgItem(IDC_CHATROOM_GATEWAY2));
	m_ServicesList.InsertString(-1, _Settings.Translate("- All gateways -"));
//	::EnableWindow(::GetDlgItem(m_Owner, IDOK), FALSE);
	::EnableWindow(::GetDlgItem(m_Owner, IDC_NEXT), FALSE);
	::EnableWindow(::GetDlgItem(m_Owner, IDC_BACK), FALSE);


	SendDlgItemMessage(IDC_CHATROOM_ROOMLIST,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_BORDERSELECT | LVS_EX_FULLROWSELECT); 
	LV_COLUMN lvcol = {0};
	lvcol.mask=LVCF_TEXT | LVCF_WIDTH;
	lvcol.pszText=_Settings.Translate("Service gateway");
	lvcol.cx = 165;	
	SendDlgItemMessage(IDC_CHATROOM_ROOMLIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 
	lvcol.pszText = _Settings.Translate("Room name");
	lvcol.cx = 210;
	SendDlgItemMessage(IDC_CHATROOM_ROOMLIST,LVM_INSERTCOLUMN,0,(LPARAM)&lvcol); 


	_Jabber->m_ServiceRegisterHwnd = GetDlgItem(IDC_CHATROOM_ROOMLIST);

	CComBSTR2 j = _Settings.m_Nick;
	SendMessage(GetDlgItem(IDC_CHATROOM_NICKNAME), WM_SETTEXT, 0, (LPARAM)j.ToString());



#ifndef _WODXMPPLIB
	WODXMPPCOMLib::IXMPPServices *serv = NULL;
	WODXMPPCOMLib::IXMPPChatRooms *rs = NULL;
	_Jabber->m_Jabb->get_ChatRooms(&rs);
	if (rs)
	{
		if (SUCCEEDED(rs->get_Services(&serv)))
		{
			short count;
			if (SUCCEEDED(serv->get_Count(&count)))
			{
				for (int i=0;i<count;i++)
				{
					WODXMPPCOMLib::IXMPPService *s = NULL;
					VARIANT var;
					var.vt = VT_I2;
					var.iVal = i;
					if (SUCCEEDED(serv->get_Item(var, &s)))
					{
						CComBSTR2 j;
						s->get_JID(&j);
						m_ServicesList.InsertString(-1, j.ToString());
						m_NewRoomServicesList.InsertString(-1, j.ToString());

						s->Release();
					}
				}
			}
			serv->Release();
		}
		rs->Release();
	}
#else
	short count = 0;
	WODXMPPCOMLib::XMPP_ChatRooms_ServicesGetCount(_Jabber->m_Jabb, &count);
	for (int i=0;i<count;i++)
	{
		void *s = NULL;
		WODXMPPCOMLib::XMPP_ChatRooms_ServicesGetService(_Jabber->m_Jabb, i, &s);
		if (s)
		{
			char jid[1024] = {0};
			int jlen = sizeof(jid);
			WODXMPPCOMLib::XMPP_Service_GetJID(s, jid, &jlen);
			if (jlen>0 && jid[0])
			{
				m_ServicesList.InsertString(-1, jid);
				m_NewRoomServicesList.InsertString(-1, jid);
			}	
			WODXMPPCOMLib::XMPP_Service_Free(s);
		}
	}
#endif

	SetDlgItemText(IDC_S1, _Settings.Translate("Nickname for chatrooms"));
	SetDlgItemText(IDC_S2, _Settings.Translate("Show available rooms on following gateways"));
	SetDlgItemText(IDC_S3, _Settings.Translate("Click above to select room, or enter new chat room details below"));
	SetDlgItemText(IDC_S4, _Settings.Translate("Room name to create or join"));
	SetDlgItemText(IDC_S5, _Settings.Translate("Password (empty if none)"));
	SetDlgItemText(IDC_S6, _Settings.Translate("Gateway"));
	SetDlgItemText(IDC_CHATROOM_JOIN, _Settings.Translate("Join or Create room"));
	SetDlgItemText(IDC_CHATROOM_BLOCKUSERSATSTARTUP, _Settings.Translate("Block users at startup"));


	PopulateChatRoomListview();
	return TRUE;
}

void PopulateChatRoomListview(void)
{
	if (_Jabber->m_ServiceRegisterHwnd)
	{
		SendMessage(_Jabber->m_ServiceRegisterHwnd, LVM_DELETEALLITEMS, 0, 0);
#ifndef _WODXMPPLIB	
		WODXMPPCOMLib::IXMPPChatRooms *rooms = NULL;
		if (SUCCEEDED(_Jabber->m_Jabb->get_ChatRooms(&rooms)))
		{
			long count;
			VARIANT var;
			var.vt = VT_I4;
			if (SUCCEEDED(rooms->get_Count(&count)))
			{
				for (int i=0;i<count;i++)
				{
					CComBSTR srv, name;
					var.llVal = i;
					WODXMPPCOMLib::IXMPPChatRoom *room = NULL;
					rooms->get_Room(var, &room);
					if (room)
					{
						WODXMPPCOMLib::IXMPPService *svc = NULL;
						room->get_Service(&svc);
						if (svc)
						{
							svc->get_JID(&srv);
							svc->Release();
						}
						room->get_Name(&name);

						LVITEM it = {0};
						it.mask = LVIF_TEXT;
						CComBSTR2 bf = name;
						it.pszText = bf.ToString();
						it.cchTextMax = strlen(it.pszText);
						
						int res = SendMessage(_Jabber->m_ServiceRegisterHwnd, LVM_INSERTITEM, 0, (LPARAM)&it);
						it.iItem = res;
						it.iSubItem = 1;
						it.mask = LVIF_TEXT;
						CComBSTR2 j = srv;
						it.pszText = j.ToString();
						it.cchTextMax = strlen(it.pszText);
						
						SendMessage(_Jabber->m_ServiceRegisterHwnd, LVM_SETITEM, 0, (LPARAM)&it);
						room->Release();
					}

				}
			}
			rooms->Release();
		}
#else
		long count = 0;
		WODXMPPCOMLib::XMPP_ChatRooms_GetCount(_Jabber->m_Jabb, &count);
		for (int i=0;i<count;i++)
		{
			void *c = NULL;
			WODXMPPCOMLib::XMPP_GetChatRoomByID(_Jabber->m_Jabb, i, &c);
			if (c)
			{
				void *s = NULL;
				WODXMPPCOMLib::XMPP_ChatRoom_GetService(c, &s);
				char buff[1024] = {0};
				int blen = sizeof(buff);
				if(s)
				{
					WODXMPPCOMLib::XMPP_Service_GetJID(s, buff, &blen);
					WODXMPPCOMLib::XMPP_Service_Free(s);
				}
				
				
				char bf[1024] = {0};
				blen = sizeof(bf);
				WODXMPPCOMLib::XMPP_ChatRoom_GetName(c, bf, &blen);
				if (*bf)
				{
					LVITEM it = {0};
					it.mask = LVIF_TEXT;
					it.pszText = bf;
					it.cchTextMax = strlen(it.pszText);
					
					int res = SendMessage(_Jabber->m_ServiceRegisterHwnd, LVM_INSERTITEM, 0, (LPARAM)&it);
					it.iItem = res;
					it.iSubItem = 1;
					it.mask = LVIF_TEXT;
					it.pszText = buff;
					it.cchTextMax = strlen(it.pszText);
					
					SendMessage(_Jabber->m_ServiceRegisterHwnd, LVM_SETITEM, 0, (LPARAM)&it);
				}
				WODXMPPCOMLib::XMPP_ChatRoom_Free(c);
			}
		}
#endif
		
	}
}

LRESULT CSettingsDlg::CSettingsChatRooms::OnButtonClick(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{	
	// let's join the room!

	char buff[1024] = {0};
	SendDlgItemMessage(IDC_CHATROOM_ROOMNAME, WM_GETTEXT, sizeof(buff), (LPARAM)buff);
	if (*buff)
	{
		if (!strchr(buff, '@'))
		{
			char buff2[1024] = {0};
			SendDlgItemMessage(IDC_CHATROOM_GATEWAY2, WM_GETTEXT, sizeof(buff2), (LPARAM)buff2);
			if (*buff2)
			{
				strcat(buff, "@");
				strcat(buff, buff2);
			}
		}

#ifndef _WODXMPPLIB
		WODXMPPCOMLib::IXMPPChatRooms *rs = NULL;
		WODXMPPCOMLib::IXMPPChatRoom *chatroom = NULL;
		_Jabber->m_Jabb->get_ChatRooms(&rs);
		if (rs)
		{
			CComBSTR n = buff;
			VARIANT var;
			var.vt = VT_BSTR;
			var.bstrVal = n;
			rs->get_Room(var, &chatroom);
			if (!chatroom)
			{
				VARIANT v;
				v.vt = VT_ERROR;
				rs->raw_Add(n, v, &chatroom);
			}
		}

#else
		void *chatroom = NULL;
		char buff1[1024] = {0}, buff2[1024] = {0};
		strcpy(buff1, buff);
		WODXMPPCOMLib::XMPP_GetChatRoomByName(_Jabber->m_Jabb, buff1, &chatroom);
		if (!chatroom)
		{
			strcpy(buff1, buff);
			WODXMPPCOMLib::XMPP_ChatRooms_Add(_Jabber->m_Jabb, buff1, &chatroom);
		}
#endif
		if (chatroom)
		{
			// none found? Add new!
			CChatRoom *room = new CChatRoom();
			strcpy(room->m_JID, buff);
			_MainDlg.m_ChatRooms.push_back(room);

			*buff = 0;
			SendDlgItemMessage(IDC_CHATROOM_ROOMPASS, WM_GETTEXT, sizeof(buff), (LPARAM)buff);

			char nickbuff[1024] = {0};
			SendDlgItemMessage(IDC_CHATROOM_NICKNAME, WM_GETTEXT, sizeof(nickbuff), (LPARAM)nickbuff);
			strcpy(room->m_Nick, nickbuff);

			if (::SendMessage(GetDlgItem(IDC_CHATROOM_BLOCKUSERSATSTARTUP), BM_GETCHECK, NULL, NULL)) 
				room->m_Block = TRUE;
			else
				room->m_Block = FALSE;
				

#ifndef _WODXMPPLIB
			CComBSTR p = room->m_Password;
			chatroom->put_Password(p);
			CComBSTR2 n1 = room->m_Nick;
			chatroom->put_Nick(n1);
			chatroom->put_ShowMyself(VARIANT_FALSE);
			chatroom->Join();
			chatroom->Release();
#else
			WODXMPPCOMLib::XMPP_ChatRoom_SetPassword(chatroom, buff);
			strcpy(room->m_Password, buff);
			WODXMPPCOMLib::XMPP_ChatRoom_SetNick(chatroom, nickbuff);
			WODXMPPCOMLib::XMPP_ChatRoom_SetShowMyself(chatroom, FALSE);
			WODXMPPCOMLib::XMPP_ChatRoom_Join(chatroom);
			WODXMPPCOMLib::XMPP_ChatRoom_Free(chatroom);
#endif
			::PostMessage(m_Owner, WM_COMMAND, IDOK, IDOK);
		}

#ifndef _WODXMPPLIB
		if (rs)
			rs->Release();
#endif
	}			

	return TRUE;
}

LRESULT CSettingsDlg::CSettingsChatRooms::OnChatRoom2Change(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{	
	return OnChange(wNotifyCode, wID, hWndCtl, bHandled);
}
LRESULT CSettingsDlg::CSettingsChatRooms::OnChange(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
	BOOL enable = FALSE;

	char buff[1024] = {0};
	SendDlgItemMessage(IDC_CHATROOM_ROOMNAME, WM_GETTEXT, sizeof(buff), (LPARAM)buff);
	if (*buff)
	{
		if (strchr(buff, '@'))
			enable = TRUE;
		else
		{
			*buff = 0;
			SendDlgItemMessage(IDC_CHATROOM_GATEWAY2, WM_GETTEXT, sizeof(buff), (LPARAM)buff);
			if (*buff)
				enable = TRUE;	
		}
	}
	::EnableWindow(GetDlgItem(IDC_CHATROOM_JOIN), enable);
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsChatRooms::OnGatewayListClick(WORD wNotifyCode, WORD wID, HWND hWndCtl, BOOL& bHandled)
{
//	ATLTRACE("code = %x\r\n", wNotifyCode);
	if (wNotifyCode == CBN_CLOSEUP)
	{
		char buff[1024];
		if (GetDlgItemText(IDC_CHATROOM_GATEWAYLIST, buff, 1024))
		{
#ifndef _WODXMPPLIB
			WODXMPPCOMLib::IXMPPServices *serv = NULL;
			if (SUCCEEDED(_Jabber->m_Jabb->get_Services(&serv)))
			{
				VARIANT var;
				var.vt = VT_BSTR;
				CComBSTR sn = buff;
				var.bstrVal = sn;
				WODXMPPCOMLib::IXMPPService *s = NULL;
				serv->get_Item(var, &s);

				WODXMPPCOMLib::IXMPPChatRooms *rs = NULL;	
				_Jabber->m_Jabb->get_ChatRooms(&rs);
				if (rs)
				{
					VARIANT var;
					if (s)
					{
						var.vt = VT_DISPATCH;
						var.pdispVal = s;
					}
					else
						var.vt = VT_ERROR;

					rs->List(var);
					rs->Release();
				}
				if (s)
					s->Release();
				serv->Release();
			}
#else
			void *s = NULL;
			if (*buff)
				WODXMPPCOMLib::XMPP_ServicesGetServiceByJID(_Jabber->m_Jabb, buff, &s);
			
			WODXMPPCOMLib::XMPP_ChatRooms_List(_Jabber->m_Jabb, s);
			if (s)
				WODXMPPCOMLib::XMPP_Service_Free(s);
#endif
		}
	}
	return TRUE;
}

LRESULT CSettingsDlg::CSettingsChatRooms::OnRoomList(int /*idCtrl*/, LPNMHDR pnmh, BOOL& bHandled)
{
	NMLISTVIEW *nm = (NMLISTVIEW *)pnmh;
	
	switch (pnmh->code)
	{
		case NM_CLICK:
		case NM_DBLCLK:
		{
			int i = SendDlgItemMessage(IDC_CHATROOM_ROOMLIST,LVM_GETNEXTITEM, -1,LVNI_SELECTED); // return item selected
			if (i!=LB_ERR)
			{
				char room[1024] = {0}, service[1024] = {0};
				LVITEM lv = {0};
				lv.mask = LVIF_TEXT;
				lv.pszText = room;
				lv.cchTextMax = sizeof(room);
				lv.iItem = i;
				SendDlgItemMessage(IDC_CHATROOM_ROOMLIST,LVM_GETITEM, 0,(LPARAM)(LPLVITEM)&lv);
				lv.pszText = service;
				lv.cchTextMax = sizeof(service);
				lv.iSubItem = 1;
				SendDlgItemMessage(IDC_CHATROOM_ROOMLIST,LVM_GETITEM, 0,(LPARAM)(LPLVITEM)&lv);


				SendDlgItemMessage(IDC_CHATROOM_GATEWAY2, CB_SELECTSTRING, -1, (LPARAM)service);
				SendDlgItemMessage(IDC_CHATROOM_ROOMNAME, WM_SETTEXT, 0, (LPARAM)room);

				if (pnmh->code == NM_DBLCLK)
					OnButtonClick(0, 0,0,bHandled);
			}
		}
		break;
	}
	return 0;
}

BOOL CSettingsDlg::CSettingsChatRooms::Apply(void)
{
	char buff[1024] = {0};
	SendDlgItemMessage(IDC_CHATROOM_NICKNAME, WM_GETTEXT, sizeof(buff), (LPARAM)buff);
	if (*buff)
	{
		_Settings.m_Nick = buff;
	}		
	return TRUE;
}

void CSettingsDlg::CSettingsChatRooms::Init(HWND Owner)
{
	m_Owner = Owner;
	Create(Owner);
}

void CSettingsDlg::CSettingsChatRooms::Show(BOOL Show, RECT *rc)
{
	if (IsWindow())
	{
		if (Show)
		{
			::SetWindowPos(m_hWnd, NULL, rc->left, rc->top, rc->right, rc->bottom, SWP_NOZORDER);
			ShowWindow(SW_SHOW);
			SetFocus();
		}
		else
			ShowWindow(SW_HIDE);
	}
}


