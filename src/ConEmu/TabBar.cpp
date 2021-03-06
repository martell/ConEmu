﻿
/*
Copyright (c) 2009-2014 Maximus5
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. The name of the authors may not be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ''AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#define HIDE_USE_EXCEPTION_INFO

#define SHOWDEBUGSTR

#define DEBUGSTRTABS(s) //DEBUGSTR(s)
#define DEBUGSTRRECENT(s) DEBUGSTR(s)
#define DEBUGSTRCOUNT(s) DEBUGSTR(s)

#ifdef _DEBUG
#define PRINT_RECENT_STACK
//#undef PRINT_RECENT_STACK
#else
#undef PRINT_RECENT_STACK
#endif

#include <windows.h>
#include <commctrl.h>
#include "header.h"
#include "TabBar.h"
#include "TabCtrlBase.h"
#include "TabCtrlWin.h"
#include "Options.h"
#include "ConEmu.h"
#include "VirtualConsole.h"
#include "TrayIcon.h"
#include "VConChild.h"
#include "VConGroup.h"
#include "Status.h"
#include "Menu.h"

WARNING("!!! Запустили far, открыли edit, перешли в панель, открыли второй edit, ESC, ни одна вкладка не активна");
// Более того, если есть еще одна консоль - активной станет первая вкладка следующей НЕАКТИВНОЙ консоли
WARNING("не меняются табы при переключении на другую консоль");

TODO("Для WinXP можно поиграться стилем WS_EX_COMPOSITED");

WARNING("isTabFrame на удаление");

#ifndef TBN_GETINFOTIP
#define TBN_GETINFOTIP TBN_GETINFOTIPW
#endif

#ifndef RB_SETWINDOWTHEME
#define CCM_SETWINDOWTHEME      (CCM_FIRST + 0xb)
#define RB_SETWINDOWTHEME       CCM_SETWINDOWTHEME
#endif

WARNING("TB_GETIDEALSIZE - awailable on XP only, use insted TB_GETMAXSIZE");
#ifndef TB_GETIDEALSIZE
#define TB_GETIDEALSIZE         (WM_USER + 99)
#endif

//enum ToolbarMainBitmapIdx
//{
//	BID_FIST_CON = 0,
//	BID_LAST_CON = (MAX_CONSOLE_COUNT-1),
//	BID_NEWCON_IDX,
//	BID_ALTERNATIVE_IDX,
//	BID_MINIMIZE_IDX,
//	BID_MAXIMIZE_IDX,
//	BID_RESTORE_IDX,
//	BID_APPCLOSE_IDX,
//	BID_DUMMYBTN_IDX,
//	BID_TOOLBAR_LAST_IDX,
//};

//typedef long (WINAPI* ThemeFunction_t)();

CTabBarClass::CTabBarClass()
{
	_active = false;
	_visible = false;
	_tabHeight = 0;
	mn_CurSelTab = 0;
	mb_ForceRecalcHeight = false;
	mb_DisableRedraw = FALSE;
	//memset(&m_Margins, 0, sizeof(m_Margins));
	//m_Margins = gpSet->rcTabMargins; // !! Значения отступов
	//_titleShouldChange = false;
	//mb_Enabled = TRUE;
	mb_PostUpdateCalled = FALSE;
	mb_PostUpdateRequested = FALSE;
	mn_PostUpdateTick = 0;
	//mn_MsgUpdateTabs = RegisterWindowMessage(CONEMUMSG_UPDATETABS);
	memset(&m_Tab4Tip, 0, sizeof(m_Tab4Tip));
	mb_InKeySwitching = FALSE;
	ms_TmpTabText[0] = 0;
	mn_InUpdate = 0;
	mp_DummyTab = new CTabID(NULL, NULL, fwt_Panels|fwt_CurrentFarWnd, 0, 0, 0);
	_ASSERTE(mp_DummyTab->RefCount()==1);

	#ifdef TAB_REF_PLACE
	m_Tabs.SetPlace("TabBar.cpp:tabs.m_Tabs",0);
	#endif

	mp_Rebar = new CTabPanelWin(this);
}

CTabBarClass::~CTabBarClass()
{
	SafeDelete(mp_Rebar);

	mp_DummyTab->Release();

	// Освободить все табы
	m_Tabs.ReleaseTabs(false);

	//m_TabStack.ReleaseTabs(false);
	_ASSERTE(gpConEmu->isMainThread());
	for (INT_PTR i = 0; i < m_TabStack.size(); i++)
	{
		if (m_TabStack[i])
		{
			#ifdef TAB_REF_PLACE
			m_TabStack[i]->DelPlace("TabBar.cpp:m_TabStack",0);
			#endif
			m_TabStack[i]->Release();
		}
	}
	m_TabStack.clear();
}

void CTabBarClass::RePaint()
{
	mp_Rebar->RePaintInt();
}

//void CTabBarClass::Refresh(BOOL abFarActive)
//{
//    Enable(abFarActive);
//}

void CTabBarClass::Reset()
{
	if (!_active)
	{
		return;
	}

	/*ConEmuTab tab; memset(&tab, 0, sizeof(tab));
	tab.Pos=0;
	tab.Current=1;
	tab.Type = 1;*/
	//gpConEmu->mp_TabBar->Update(&tab, 1);
	Update();
}

void CTabBarClass::Retrieve()
{
	if (gpSet->isTabs == 0)
		return; // если табов нет ВООБЩЕ - и читать ничего не нужно

	if (!CVConGroup::isFar())
	{
		Reset();
		return;
	}

	TODO("Retrieve() может нужно выполнить в RCon?");
	//CConEmuPipe pipe;
	//if (pipe.Init(_T("CTabBarClass::Retrieve"), TRUE))
	//{
	//  DWORD cbWritten=0;
	//  if (pipe.Execute(CMD_REQTABS))
	//  {
	//      gpConEmu->DebugStep(_T("Tabs: Checking for plugin (1 sec)"));
	//      // Подождем немножко, проверим что плагин живой
	//      cbWritten = WaitForSingleObject(pipe.hEventAlive, CONEMUALIVETIMEOUT);
	//      if (cbWritten!=WAIT_OBJECT_0) {
	//          TCHAR szErr[MAX_PATH];
	//          _wsprintf(szErr, countof(szErr), _T("ConEmu plugin is not active!\r\nProcessID=%i"), pipe.nPID);
	//          MBoxA(szErr);
	//      } else {
	//          gpConEmu->DebugStep(_T("Tabs: Waiting for result (10 sec)"));
	//          cbWritten = WaitForSingleObject(pipe.hEventReady, CONEMUREADYTIMEOUT);
	//          if (cbWritten!=WAIT_OBJECT_0) {
	//              TCHAR szErr[MAX_PATH];
	//              _wsprintf(szErr, countof(szErr), _T("Command waiting time exceeds!\r\nConEmu plugin is locked?\r\nProcessID=%i"), pipe.nPID);
	//              MBoxA(szErr);
	//          } else {
	//              gpConEmu->DebugStep(_T("Tabs: Recieving data"));
	//              DWORD cbBytesRead=0;
	//              int nTabCount=0;
	//              pipe.Read(&nTabCount, sizeof(nTabCount), &cbBytesRead);
	//              if (nTabCount<=0) {
	//                  gpConEmu->DebugStep(_T("Tabs: data empty"));
	//                  this->Reset();
	//              } else {
	//                  COPYDATASTRUCT cds = {0};
	//
	//                  cds.dwData = nTabCount;
	//                  cds.lpData = pipe.GetPtr(); // хвост
	//                  gpConEmu->OnCopyData(&cds);
	//                  gpConEmu->DebugStep(NULL);
	//              }
	//          }
	//      }
	//  }
	//}
}

int CTabBarClass::CreateTabIcon(LPCWSTR asIconDescr, bool bAdmin, LPCWSTR asWorkDir)
{
	if (!gpSet->isTabIcons)
		return -1;
	return m_TabIcons.CreateTabIcon(asIconDescr, bAdmin, asWorkDir);
}

HIMAGELIST CTabBarClass::GetTabIcons()
{
	if (!m_TabIcons.IsInitialized())
		return NULL;
	return (HIMAGELIST)m_TabIcons;
}

int CTabBarClass::GetTabIcon(bool bAdmin)
{
	return m_TabIcons.GetTabIcon(bAdmin);
}

void CTabBarClass::SelectTab(int i)
{
	if (mp_Rebar->IsTabbarCreated())
		mn_CurSelTab = mp_Rebar->SelectTabInt(i); // Меняем выделение, только если оно реально меняется
	else if (i >= 0 && i < GetItemCount())
		mn_CurSelTab = i;
}

int CTabBarClass::GetCurSel()
{
	if (mp_Rebar->IsTabbarCreated())
		mn_CurSelTab = mp_Rebar->GetCurSelInt();
	return mn_CurSelTab;
}

int CTabBarClass::GetItemCount()
{
	int nCurCount = 0;

	TODO("Здесь вообще не нужно обращаться к mp_Rebar, табы должны храниться здесь");
	if (mp_Rebar->IsTabbarCreated())
		nCurCount = mp_Rebar->GetItemCountInt();
	else
		nCurCount = m_Tabs.GetCount();

	return nCurCount;
}

int CTabBarClass::CountActiveTabs(int nMax /*= 0*/)
{
	int  nTabs = 0;
	bool bHideInactiveConsoleTabs = gpSet->bHideInactiveConsoleTabs;

	for (int V = 0; V < MAX_CONSOLE_COUNT; V++)
	{
		CVConGuard guard;
		if (!CVConGroup::GetVCon(V, &guard))
			continue;
		CVirtualConsole* pVCon = guard.VCon();

		if (bHideInactiveConsoleTabs)
		{
			if (!CVConGroup::isActive(pVCon))
				continue;
		}

		nTabs += pVCon->RCon()->GetTabCount(TRUE);

		if ((nMax > 0) && (nTabs >= nMax))
			break;
	}

	return nTabs;
}

void CTabBarClass::DeleteItem(int I)
{
	if (mp_Rebar->IsTabbarCreated())
	{
		mp_Rebar->DeleteItemInt(I);
	}
}


/*char CTabBarClass::FarTabShortcut(int tabIndex)
{
    return tabIndex < 10 ? '0' + tabIndex : 'A' + tabIndex - 10;
}*/

bool CTabBarClass::NeedPostUpdate()
{
	return (mb_PostUpdateCalled || mb_PostUpdateRequested);
}

void CTabBarClass::RequestPostUpdate()
{
	if (mb_PostUpdateCalled)
	{
		DWORD nDelta = GetTickCount() - mn_PostUpdateTick;

		// Может так получиться, что флажок остался, а Post не вызвался
		if (nDelta <= POST_UPDATE_TIMEOUT)
			return; // Уже
	}

	if (mn_InUpdate > 0)
	{
		mb_PostUpdateRequested = TRUE;
		DEBUGSTRTABS(L"   PostRequesting CTabBarClass::Update\n");
	}
	else
	{
		mb_PostUpdateCalled = TRUE;
		DEBUGSTRTABS(L"   Posting CTabBarClass::Update\n");
		//PostMessage(ghWnd, mn_MsgUpdateTabs, 0, 0);
		gpConEmu->RequestPostUpdateTabs();
		mn_PostUpdateTick = GetTickCount();
	}
}

bool CTabBarClass::GetVConFromTab(int nTabIdx, CVConGuard* rpVCon, DWORD* rpWndIndex)
{
	bool lbRc = false;
	CTab tab(__FILE__,__LINE__);
	CVConGuard VCon;
	DWORD wndIndex = 0;

	if (m_Tabs.GetTabByIndex(nTabIdx, tab))
	{
		wndIndex = tab->Info.nFarWindowID;

		if (!gpConEmu->isValid((CVirtualConsole*)tab->Info.pVCon))
		{
			RequestPostUpdate();
		}
		else
		{
			VCon = (CVirtualConsole*)tab->Info.pVCon;
			lbRc = true;
		}
	}

	if (rpVCon)
		rpVCon->Attach(VCon.VCon());

	if (rpWndIndex)
		*rpWndIndex = lbRc ? wndIndex : 0;

	return lbRc;
}

LRESULT CTabBarClass::OnTimer(WPARAM wParam)
{
	if (mp_Rebar)
		return mp_Rebar->OnTimerInt(wParam);
	return 0;
}

bool CTabBarClass::IsTabsActive()
{
	if (!this)
	{
		_ASSERTE(this!=NULL);
		return false;
	}
	return _active;
}

bool CTabBarClass::IsTabsShown()
{
	if (!this)
	{
		_ASSERTE(this!=NULL);
		return false;
	}

	// Было "IsWindowVisible(mh_Tabbar)", что некорректно, т.к. оно учитывает состояние ghWnd

	// Надо использовать внутренние статусы!
	return _active && mp_Rebar->IsTabbarCreated() && _visible;
}

void CTabBarClass::Activate(BOOL abPreSyncConsole/*=FALSE*/)
{
	if (!mp_Rebar->IsCreated())
	{
		if (!m_TabIcons.IsInitialized())
		{
			m_TabIcons.Initialize();
		}
		// Создать
		mp_Rebar->CreateRebar();
		// Узнать высоту созданного
		_tabHeight = mp_Rebar->QueryTabbarHeight();
		// Передернуть
		OnCaptionHidden();
	}

	_active = true;
	if (abPreSyncConsole && (gpConEmu->WindowMode == wmNormal))
	{
		RECT rcIdeal = gpConEmu->GetIdealRect();
		CVConGroup::SyncConsoleToWindow(&rcIdeal, TRUE);
	}
	gpConEmu->OnTabbarActivated(true);
	UpdatePosition();
}

void CTabBarClass::Deactivate(BOOL abPreSyncConsole/*=FALSE*/)
{
	if (!_active)
		return;

	_active = false;
	if (abPreSyncConsole && !(gpConEmu->isZoomed() || gpConEmu->isFullScreen()))
	{
		RECT rcIdeal = gpConEmu->GetIdealRect();
		CVConGroup::SyncConsoleToWindow(&rcIdeal, true);
	}
	gpConEmu->OnTabbarActivated(false);
	UpdatePosition();
}

int  CTabBarClass::UpdateAddTab(HANDLE hUpdate, int& tabIdx, int& nCurTab, bool& bStackChanged, CVirtualConsole* pVCon, DWORD nFlags/*UpdateAddTabFlags*/)
{
	CRealConsole *pRCon = pVCon->RCon();
	if (!pRCon)
	{
		_ASSERTE(pRCon!=NULL);
		return 0;
	}

	bool bVConActive = CVConGroup::isActive(pVCon, false);
	bool bAllWindows = (gpSet->bShowFarWindows && !(pRCon->GetActiveTabType() & fwt_ModalFarWnd));
	int rFrom = bAllWindows ? 0 : pRCon->GetActiveTab();
	int rFound = 0;

	for (int I = rFrom; bAllWindows || !rFound; I++)
	{
		#ifdef _DEBUG
			if (this != gpConEmu->mp_TabBar)
			{
				_ASSERTE(this == gpConEmu->mp_TabBar);
			}
			MCHKHEAP;
		#endif

		// Check first, if the tab exists
		CTab tab(__FILE__,__LINE__);
		if (!pRCon->GetTab(I, tab))
			break;

		// bShowFarWindows проверяем, чтобы не проколоться с недоступностью единственного таба
		if (gpSet->bHideDisabledTabs && gpSet->bShowFarWindows)
		{
			if (!pRCon->CanActivateFarWindow(I))
				continue;
		}


		#ifdef _DEBUG
			if (this != gpConEmu->mp_TabBar)
			{
				_ASSERTE(this == gpConEmu->mp_TabBar);
			}
			MCHKHEAP;
		#endif

		// Additional flags (to add or not to add)
		if (nFlags & (uat_PanelsOrModalsOnly|uat_PanelsOnly))
		{
			if ((tab->Type() != fwt_Panels))
			{
				if (nFlags & uat_PanelsOnly)
					continue;
				if (!(tab->Flags() & fwt_ModalFarWnd))
					continue;
				// Редакторы/вьюверы из "far /e ..." здесь НЕ добавлять!
				// Они либо показываются как "Консоль" (без сплитов)
				// Либо как отдельный таб редактора
				if (!pRCon->isFarPanelAllowed())
					continue;
			}
		}
		if (nFlags & uat_NonModals)
		{
			// Смысл в том, что если VCon активна,
			// то модальный редактор показан *первой* вкладкой группы (вместо панелей).
			// Для НЕ активных VCon - модальный редактор нужно все-равно допихнуть в табы,
			// а то таб этого редактор вообще не будет виден пользователю.
			if (bVConActive && (tab->Flags() & fwt_ModalFarWnd) && pRCon->isFarPanelAllowed())
				continue;
		}
		if (nFlags & uat_NonPanels)
		{
			if ((tab->Type() == fwt_Panels) && pRCon->isFarPanelAllowed())
				continue;
		}

		// Prepare tab to display
		WARNING("TabIcon, trim words?");
		int iTabIcon = PrepareTab(tab, pVCon);

		#if 0
		vct.pVCon = pVCon;
		vct.nFarWindowId = I;
		#endif


		m_Tabs.UpdateAppend(hUpdate, tab, FALSE);

		// Физически (WinAPI) добавляет закладку, или меняет (при необходимости) заголовок существующей
		mp_Rebar->AddTabInt(tab->GetLabel(), tabIdx, (tab->Flags() & fwt_Elevated)==fwt_Elevated, iTabIcon);

		// Add current (selected) tab to the top of recent stack
		if ((tab->Flags() & fwt_CurrentFarWnd) && bVConActive)
		{
			// !!! RCon must return ONLY ONE active tab !!!
			// with only temorarily exception during edit/view activation from panels

			#ifdef _DEBUG
			static int iFails = 0;
			if (nCurTab == -1)
			{
				iFails = 0;
			}
			else
			{
				_ASSERTE((nCurTab == -1) || (nCurTab == 0 && iFails == 0 && tab->Type() == fwt_Panels));
				iFails++;
			}
			#endif

			nCurTab = tabIdx;
			if (AddStack(tab))
				bStackChanged = true;
		}

		rFound++;
		tabIdx++;


		#ifdef _DEBUG
			if (this != gpConEmu->mp_TabBar)
			{
				_ASSERTE(this == gpConEmu->mp_TabBar);
			}
		#endif
	}

	return rFound;
}

void CTabBarClass::Update(BOOL abPosted/*=FALSE*/)
{
	#ifdef _DEBUG
	if (this != gpConEmu->mp_TabBar)
	{
		_ASSERTE(this == gpConEmu->mp_TabBar);
	}
	#endif

	MCHKHEAP
	/*if (!_active)
	{
	    return;
	}*/ // Теперь - ВСЕГДА! т.к. сами управляем мультиконсолью

	if (mb_DisableRedraw)
	{
		_ASSERTE(FALSE && "mb_DisableRedraw?"); // Надо?
		return;
	}

	if (!gpConEmu->isMainThread())
	{
		RequestPostUpdate();
		return;
	}

	gpConEmu->mp_Status->UpdateStatusBar();

	mb_PostUpdateCalled = FALSE;

	#ifdef _DEBUG
	_ASSERTE(mn_InUpdate >= 0);
	if (mn_InUpdate > 0)
	{
		_ASSERTE(mn_InUpdate == 0);
	}
	#endif

	mn_InUpdate ++;

	MCHKHEAP
	int V, I, tabIdx = 0, nCurTab = -1;
	BOOL bShowFarWindows = gpSet->bShowFarWindows;

	// Выполняться должно только в основной нити, так что CriticalSection не нужна

	#ifdef _DEBUG
	if (this != gpConEmu->mp_TabBar)
	{
		_ASSERTE(this == gpConEmu->mp_TabBar);
	}
	#endif

	TODO("Обработка gpSet->bHideInactiveConsoleTabs для новых табов");
	MCHKHEAP


	// Check if we need to AutoSHOW or AutoHIDE tab bar
	if (!IsTabsActive() && gpSet->isTabs)
	{
		int nTabs = CountActiveTabs(2);
		if (nTabs > 1)
		{
			Activate();
		}
	}
	else if (IsTabsActive() && gpSet->isTabs==2)
	{
		int nTabs = CountActiveTabs(2);
		if (nTabs <= 1)
		{
			Deactivate();
		}
	}


    // Validation?
	#ifdef _DEBUG
	if (this != gpConEmu->mp_TabBar)
	{
		_ASSERTE(this == gpConEmu->mp_TabBar);
	}
	#endif



	MCHKHEAP
	HANDLE hUpdate = m_Tabs.UpdateBegin();
	_ASSERTE(hUpdate!=NULL);

	bool bStackChanged = false;

	/* ********************* */
	/*          Go           */
	/* ********************* */
	{
		MMap<CVConGroup*,CVirtualConsole*> Groups; Groups.Init(MAX_CONSOLE_COUNT, true);

		for (V = 0; V < MAX_CONSOLE_COUNT; V++)
		{
			//if (!(pVCon = gpConEmu->GetVCon(V))) continue;
			CVConGuard guard;
			if (!CVConGroup::GetVCon(V, &guard))
				continue;
			CVirtualConsole* pVCon = guard.VCon();

			BOOL lbActive = CVConGroup::isActive(pVCon, false);

			if (gpSet->bHideInactiveConsoleTabs && !lbActive)
				continue;

			if (gpSet->isOneTabPerGroup)
			{
				CVConGroup *pGr;
				CVConGuard VGrActive;
				if (CVConGroup::isGroup(pVCon, &pGr, &VGrActive))
				{
					CVirtualConsole* pGrVCon;

					if (Groups.Get(pGr, &pGrVCon))
						continue; // эта группа уже есть

					pGrVCon = VGrActive.VCon();
					Groups.Set(pGr, pGrVCon);

					// И показывать таб нужно от "активной" консоли, а не от первой в группе
					if (pVCon != pGrVCon)
					{
						guard = pGrVCon;
						pVCon = pGrVCon;
					}

					if (!lbActive)
					{
						lbActive = CVConGroup::isActive(pVCon, true);
					}

					// Показывать редакторы из всех групп?
					if (gpSet->bShowFarWindows)
					{
						MArray<CVConGuard*> Panes;
						int nPanes = pGr->GetGroupPanes(&Panes);

						// Только если в группе более одного таба - тогда нужно дополниетльная логика населения...
						if (nPanes > 1)
						{
							// Первым табом - показать текущую панель, либо МОДАЛЬНЫЙ редактор/вьювер
							// Редакторы из "far /e ..." здесь НЕ добавлять!
							if (!pVCon->RCon()->isFarPanelAllowed()
								|| !UpdateAddTab(hUpdate, tabIdx, nCurTab, bStackChanged, pVCon, uat_PanelsOrModalsOnly))
							{
								// Если есть - добавить ОДНУ панель, чтобы табы сплита не прыгали туда-сюда
								for (int K = 0; K < nPanes; K++)
								{
									if (Panes[K]->VCon()->RCon()->isFarPanelAllowed())
									{
										if (UpdateAddTab(hUpdate, tabIdx, nCurTab, bStackChanged,
												Panes[K]->VCon(), uat_PanelsOnly) > 0)
											break;
									}
								}
							}

							// Потом - все оставшиеся редакторы/вьюверы (в том числе и "far /e ...")
							for (int K = 0; K < nPanes; K++)
							{
								UpdateAddTab(hUpdate, tabIdx, nCurTab, bStackChanged,
									Panes[K]->VCon(), uat_NonModals|uat_NonPanels);
							}

							// Release
							CVConGroup::FreePanesArray(Panes);

							// Already processed, next VCon
							continue;
						}
					}
				}
			}

			UpdateAddTab(hUpdate, tabIdx, nCurTab, bStackChanged, pVCon, uat_AnyTab);
		}

		Groups.Release();
	}

	MCHKHEAP

	// Must be at least one tab ("ConEmu -Detached" for example)
	if (tabIdx == 0)
	{
		m_Tabs.UpdateAppend(hUpdate, mp_DummyTab, FALSE);

		// Физически (WinAPI) добавляет закладку, или меняет (при необходимости) заголовок существующей
		mp_Rebar->AddTabInt(gpConEmu->GetDefaultTabLabel(), tabIdx, gpConEmu->mb_IsUacAdmin, -1);

		nCurTab = tabIdx;
		tabIdx++;
	}

	m_Tabs.UpdateEnd(hUpdate, 0);

	// Проверим стек последних выбранных
	if (CheckStack())
		bStackChanged = true;

	#ifdef PRINT_RECENT_STACK
	PrintRecentStack();
	#endif

	#ifdef _DEBUG
	static int nPrevVisible, nPrevStacked;
	{
		wchar_t szDbg[100];
		int nNewVisible = m_Tabs.GetCount();
		int nNewStacked = m_TabStack.size();
		_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"*** Tab list updated. Visible:%u, Stacked:%u, StackChanged:%s\n",
			nNewVisible, nNewStacked, bStackChanged ? L"Yes" : L"No");
		DEBUGSTRCOUNT(szDbg);
		nPrevVisible = nNewVisible;
		nPrevStacked = nNewStacked;
	}
	#endif

	// удалить лишние закладки (визуально)
	int nCurCount = GetItemCount();

	#ifdef _DEBUG
	wchar_t szDbg[128];
	_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"CTabBarClass::Update.  ItemCount=%i, PrevItemCount=%i\n", tabIdx, nCurCount);
	DEBUGSTRTABS(szDbg);
	#endif

	if (mp_Rebar->IsTabbarCreated())
	{
		for (I = tabIdx; I < nCurCount; I++)
		{
			#ifdef _DEBUG
			_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"   Deleting tab=%i\n", I+1);
			DEBUGSTRTABS(szDbg);
			#endif

			DeleteItem(tabIdx);
		}
	}

	MCHKHEAP

	if (mb_InKeySwitching)
	{
		if (mn_CurSelTab >= nCurCount)  // Если выбранный таб вылез за границы
			mb_InKeySwitching = FALSE;
	}

	if (!mb_InKeySwitching && nCurTab != -1)
	{
		SelectTab(nCurTab);
	}

	UpdateToolConsoles();

	//if (gpSet->isTabsInCaption)
	//{
	//	SendMessage(ghWnd, WM_NCPAINT, 0, 0);
	//}

	mn_InUpdate --;

	if (mb_PostUpdateRequested)
	{
		mb_PostUpdateCalled = FALSE;
		mb_PostUpdateRequested = FALSE;
		RequestPostUpdate();
	}

	MCHKHEAP
	return; // Just for clearness
}

RECT CTabBarClass::GetMargins()
{
	_ASSERTE(this);
	RECT rcNewMargins = {0,0};

	if (_active || (gpSet->isTabs == 1))
	{
		if (!_tabHeight)
		{
			// Вычислить высоту
			GetTabbarHeight();
		}

		if (_tabHeight /*&& IsTabsShown()*/)
		{
			_ASSERTE(_tabHeight!=0 && "Height must be evaluated already!");

			if (gpSet->nTabsLocation == 1)
				rcNewMargins = MakeRect(0,0,0,_tabHeight);
			else
				rcNewMargins = MakeRect(0,_tabHeight,0,0);

			//if (memcmp(&rcNewMargins, &m_Margins, sizeof(m_Margins)) != 0)
			//{
			//	m_Margins = rcNewMargins;
			//	gpSet->UpdateMargins(m_Margins);
			//}
		}
	}
	//return m_Margins;

	return rcNewMargins;
}

void CTabBarClass::UpdatePosition()
{
	if (!mp_Rebar->IsCreated())
		return;

	if (gpConEmu->isIconic())
		return; // иначе расчет размеров будет некорректным!

	DEBUGSTRTABS(_active ? L"CTabBarClass::UpdatePosition(activate)\n" : L"CTabBarClass::UpdatePosition(DEactivate)\n");

	#ifdef _DEBUG
	DWORD_PTR dwStyle = GetWindowLongPtr(ghWnd, GWL_STYLE);
	#endif

	if (_active)
	{
		_visible = true;

		mp_Rebar->ShowBar(true);

		#ifdef _DEBUG
		dwStyle = GetWindowLongPtr(ghWnd, GWL_STYLE);
		#endif

		if (!gpConEmu->InCreateWindow())
		{
			//gpConEmu->Sync ConsoleToWindow(); -- 2009.07.04 Sync должен быть выполнен в самом ReSize
			gpConEmu->ReSize(TRUE);
		}
	}
	else
	{
		_visible = false;

		//gpConEmu->Sync ConsoleToWindow(); -- 2009.07.04 Sync должен быть выполнен в самом ReSize
		gpConEmu->ReSize(TRUE);

		// _active уже сбросили, поэтому реально спрятать можно и позже
		mp_Rebar->ShowBar(false);
	}
}

void CTabBarClass::Reposition()
{
	if (!_active)
	{
		return;
	}

	mp_Rebar->RepositionInt();
}

LRESULT CTabBarClass::OnNotify(LPNMHDR nmhdr)
{
	if (!this)
		return false;

	if (!_active)
	{
		return false;
	}

	LRESULT lResult = 0;

	if (mp_Rebar->OnNotifyInt(nmhdr, lResult))
		return lResult;


	if (nmhdr->code == TBN_GETINFOTIP && mp_Rebar->IsToolbarNotify(nmhdr))
	{
		if (!gpSet->isMultiShowButtons)
			return 0;

		LPNMTBGETINFOTIP pDisp = (LPNMTBGETINFOTIP)nmhdr;

		//if (pDisp->iItem>=1 && pDisp->iItem<=MAX_CONSOLE_COUNT)
		if (pDisp->iItem == TID_ACTIVE_NUMBER)
		{
			if (!pDisp->pszText || !pDisp->cchTextMax)
				return false;

			CVConGuard VCon;
			CVirtualConsole* pVCon = (gpConEmu->GetActiveVCon(&VCon) >= 0) ? VCon.VCon() : NULL;
			LPCWSTR pszTitle = pVCon ? pVCon->RCon()->GetTitle() : NULL;

			if (pszTitle)
			{
				lstrcpyn(pDisp->pszText, pszTitle, pDisp->cchTextMax);
			}
			else
			{
				pDisp->pszText[0] = 0;
			}
		}
		else if (pDisp->iItem == TID_CREATE_CON)
		{
			lstrcpyn(pDisp->pszText, _T("Create new console"), pDisp->cchTextMax);
		}
		else if (pDisp->iItem == TID_ALTERNATIVE)
		{
			bool lbChecked = mp_Rebar->GetToolBtnChecked(TID_ALTERNATIVE);
			lstrcpyn(pDisp->pszText,
			         lbChecked ? L"Alternative mode is ON (console freezed)" : L"Alternative mode is off",
			         pDisp->cchTextMax);
		}
		else if (pDisp->iItem == TID_SCROLL)
		{
			bool lbChecked = mp_Rebar->GetToolBtnChecked(TID_SCROLL);
			lstrcpyn(pDisp->pszText,
			         lbChecked ? L"BufferHeight mode is ON (scrolling enabled)" : L"BufferHeight mode is off",
			         pDisp->cchTextMax);
		}
		else if (pDisp->iItem == TID_MINIMIZE)
		{
			lstrcpyn(pDisp->pszText, _T("Minimize window"), pDisp->cchTextMax);
		}
		else if (pDisp->iItem == TID_MAXIMIZE)
		{
			lstrcpyn(pDisp->pszText, _T("Maximize window"), pDisp->cchTextMax);
		}
		else if (pDisp->iItem == TID_APPCLOSE)
		{
			lstrcpyn(pDisp->pszText, _T("Close ALL consoles"), pDisp->cchTextMax);
		}
		else if (pDisp->iItem == TID_COPYING)
		{
			lstrcpyn(pDisp->pszText, _T("Show copying queue"), pDisp->cchTextMax);
		}

		return true;
	}

	if (nmhdr->code == TBN_DROPDOWN && mp_Rebar->IsToolbarNotify(nmhdr))
	{
		LPNMTOOLBAR pBtn = (LPNMTOOLBAR)nmhdr;
		switch (pBtn->iItem)
		{
		case TID_ACTIVE_NUMBER:
			OnChooseTabPopup();
			break;
		case TID_CREATE_CON:
			gpConEmu->mp_Menu->OnNewConPopupMenu(NULL, 0, isPressed(VK_SHIFT));
			break;
		}
		return TBDDRET_DEFAULT;
	}

	if (nmhdr->code == TTN_GETDISPINFO && mp_Rebar->IsTabbarNotify(nmhdr))
	{
		LPNMTTDISPINFO pDisp = (LPNMTTDISPINFO)nmhdr;
		DWORD wndIndex = 0;
		pDisp->hinst = NULL;
		pDisp->szText[0] = 0;
		pDisp->lpszText = NULL;
		POINT ptScr = {}; GetCursorPos(&ptScr);
		int iPage = mp_Rebar->GetTabFromPoint(ptScr);

		if (iPage >= 0)
		{
			// Если в табе нет "…" - тип не нужен
			if (!mp_Rebar->GetTabText(iPage, ms_TmpTabText, countof(ms_TmpTabText)))
				return 0;
			if (!wcschr(ms_TmpTabText, L'\x2026' /*"…"*/))
				return 0;

			CVConGuard VCon;
			if (!GetVConFromTab(iPage, &VCon, &wndIndex))
				return 0;

			CTab tab(__FILE__,__LINE__);
			if (!VCon->RCon()->GetTab(wndIndex, tab))
				return 0;

			lstrcpyn(ms_TmpTabText, VCon->RCon()->GetTabTitle(tab), countof(ms_TmpTabText));
			pDisp->lpszText = ms_TmpTabText;
		}

		return true;
	}

	return false;
}

void CTabBarClass::OnCommand(WPARAM wParam, LPARAM lParam)
{
	if (!this)
		return;

	if (!mp_Rebar->IsToolbarCommand(wParam, lParam))
		return;

	if (!gpSet->isMultiShowButtons)
	{
		_ASSERTE(gpSet->isMultiShowButtons);
		return;
	}

	if (wParam == TID_ACTIVE_NUMBER)
	{
		//gpConEmu->ConActivate(wParam-1);
		OnChooseTabPopup();
	}
	else if (wParam == TID_CREATE_CON)
	{
		if (gpConEmu->IsGesturesEnabled())
			gpConEmu->mp_Menu->OnNewConPopupMenu(NULL, 0, isPressed(VK_SHIFT));
		else
			gpConEmu->RecreateAction(gpSetCls->GetDefaultCreateAction(), gpSet->isMultiNewConfirm || isPressed(VK_SHIFT));
	}
	else if (wParam == TID_ALTERNATIVE)
	{
		CVConGuard VCon;
		CVirtualConsole* pVCon = (gpConEmu->GetActiveVCon(&VCon) >= 0) ? VCon.VCon() : NULL;
		// Вернуть на тулбар _текущее_ состояние режима
		mp_Rebar->SetToolBtnChecked(TID_ALTERNATIVE, pVCon ? pVCon->RCon()->isAlternative() : false);
		// И собственно Action
		gpConEmu->AskChangeAlternative();
	}
	else if (wParam == TID_SCROLL)
	{
		CVConGuard VCon;
		CVirtualConsole* pVCon = (gpConEmu->GetActiveVCon(&VCon) >= 0) ? VCon.VCon() : NULL;
		// Вернуть на тулбар _текущее_ состояние режима
		mp_Rebar->SetToolBtnChecked(TID_SCROLL, pVCon ? pVCon->RCon()->isBufferHeight() : false);
		// И собственно Action
		gpConEmu->AskChangeBufferHeight();
	}
	else if (wParam == TID_MINIMIZE)
	{
		PostMessage(ghWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0);
	}
	else if (wParam == TID_MAXIMIZE)
	{
		// Чтобы клик случайно не провалился в консоль
		gpConEmu->mouse.state |= MOUSE_SIZING_DBLCKL;
		// Аналог AltF9
		gpConEmu->DoMaximizeRestore();
	}
	else if (wParam == TID_APPCLOSE)
	{
		gpConEmu->PostScClose();
	}
	else if (wParam == TID_COPYING)
	{
		gpConEmu->OnCopyingState();
	}
}

void CTabBarClass::Invalidate()
{
	if (gpConEmu->mp_TabBar->IsTabsActive())
		mp_Rebar->InvalidateBar();
}

void CTabBarClass::OnCaptionHidden()
{
	if (!this) return;

	mp_Rebar->OnCaptionHiddenChanged(gpSet->isCaptionHidden());
}

void CTabBarClass::OnWindowStateChanged()
{
	if (!this) return;

	mp_Rebar->OnWindowStateChanged(gpConEmu->GetWindowMode());
}

// Обновить наличие кнопок консолей на тулбаре
void CTabBarClass::UpdateToolConsoles(bool abForcePos/*=false*/)
{
	if (!mp_Rebar->IsToolbarCreated())
		return;

	int nNewActiveIdx = gpConEmu->ActiveConNum(); // 0-based

	OnConsoleActivated(nNewActiveIdx);
}

// nConNumber - 0-based
void CTabBarClass::OnConsoleActivated(int nConNumber)
{
	if (!mp_Rebar->IsToolbarCreated())
		return;

	mp_Rebar->OnConsoleActivatedInt(nConNumber);
}

void CTabBarClass::OnBufferHeight(BOOL abBufferHeight)
{
	if (!mp_Rebar->IsToolbarCreated())
		return;

	mp_Rebar->SetToolBtnChecked(TID_SCROLL, (abBufferHeight!=FALSE));
}

void CTabBarClass::OnAlternative(BOOL abAlternative)
{
	if (!mp_Rebar->IsToolbarCreated())
		return;

	mp_Rebar->SetToolBtnChecked(TID_ALTERNATIVE, (abAlternative!=FALSE));
}

void CTabBarClass::OnShowButtonsChanged()
{
	if (gpSet->isMultiShowButtons)
	{
		mp_Rebar->ShowToolbar(true);
	}

	Reposition();
}

int CTabBarClass::GetTabbarHeight()
{
	if (!this) return 0;

	_ASSERTE(gpSet->isTabs!=0);

	if (mb_ForceRecalcHeight || (_tabHeight == 0))
	{
		// Узнать высоту созданного
		_tabHeight = mp_Rebar->QueryTabbarHeight();
	}

	return _tabHeight;
}

int CTabBarClass::PrepareTab(CTab& pTab, CVirtualConsole *apVCon)
{
	int iTabIcon = -1;

	#ifdef _DEBUG
	if (this != gpConEmu->mp_TabBar)
	{
		_ASSERTE(this == gpConEmu->mp_TabBar);
	}
	#endif

	MCHKHEAP
	// get file name
	TCHAR dummy[MAX_PATH*2];
	TCHAR fileName[MAX_PATH+4]; fileName[0] = 0;
	TCHAR szFormat[32];
	TCHAR szEllip[MAX_PATH+1];
	//wchar_t /**tFileName=NULL,*/ *pszNo=NULL, *pszTitle=NULL;
	int nSplit = 0;
	int nMaxLen = 0; //gpSet->nTabLenMax - _tcslen(szFormat) + 2/* %s */;
	int origLength = 0; //_tcslen(tFileName);

	CRealConsole* pRCon = apVCon ? apVCon->RCon() : NULL;
	bool bIsFar = pRCon ? pRCon->isFar() : false;

	if (apVCon && (pTab->Info.nFarWindowID == 0))
	{
		iTabIcon = apVCon->RCon()->GetRootProcessIcon();
	}

	LPCWSTR pszTabName = pRCon->GetTabTitle(pTab);

	if (pTab->Name.Empty() || (pTab->Type() == fwt_Panels))
	{
		//_tcscpy(szFormat, _T("%s"));
		lstrcpyn(szFormat, bIsFar ? gpSet->szTabPanels : gpSet->szTabConsole, countof(szFormat));
		nMaxLen = gpSet->nTabLenMax - _tcslen(szFormat) + 2/* %s */;

		lstrcpyn(fileName, pszTabName, countof(fileName));

		if (gpSet->pszTabSkipWords && *gpSet->pszTabSkipWords)
		{
			StripWords(fileName, gpSet->pszTabSkipWords);
		}
		origLength = _tcslen(fileName);
		//if (origLength>6) {
		//    // Чтобы в заголовке было что-то вроде "{C:\Program Fil...- Far"
		//    //                              вместо "{C:\Program F...} - Far"
		//	После добавления суффиков к заголовку фара - оно уже влезать не будет в любом случае... Так что если панели - '...' строго ставить в конце
		//    if (lstrcmp(tFileName + origLength - 6, L" - Far") == 0)
		//        nSplit = nMaxLen - 6;
		//}
	}
	else
	{

		LPTSTR tFileName = NULL;
		if (GetFullPathName(pszTabName, countof(dummy), dummy, &tFileName) && tFileName && *tFileName)
			lstrcpyn(fileName, tFileName, countof(fileName));
		else
			lstrcpyn(fileName, pszTabName, countof(fileName));

		if (pTab->Type() == fwt_Editor)
		{
			if (pTab->Flags() & fwt_ModifiedFarWnd)
				lstrcpyn(szFormat, gpSet->szTabEditorModified, countof(szFormat));
			else
				lstrcpyn(szFormat, gpSet->szTabEditor, countof(szFormat));
		}
		else if (pTab->Type() == fwt_Viewer)
		{
			lstrcpyn(szFormat, gpSet->szTabViewer, countof(szFormat));
		}
		else
		{
			_ASSERTE(FALSE && "Must be processed in previous branch");
			lstrcpyn(szFormat, bIsFar ? gpSet->szTabPanels : gpSet->szTabConsole, countof(szFormat));
		}
	}

	// restrict length
	if (!nMaxLen)
		nMaxLen = gpSet->nTabLenMax - _tcslen(szFormat) + 2/* %s */;

	if (!origLength)
		origLength = _tcslen(fileName);
	if (nMaxLen<15) nMaxLen=15; else if (nMaxLen>=MAX_PATH) nMaxLen=MAX_PATH-1;

	if (origLength > nMaxLen)
	{
		/*_tcsnset(fileName, _T('\0'), MAX_PATH);
		_tcsncat(fileName, tFileName, 10);
		_tcsncat(fileName, _T("..."), 3);
		_tcsncat(fileName, tFileName + origLength - 10, 10);*/
		//if (!nSplit)
		//    nSplit = nMaxLen*2/3;
		//// 2009-09-20 Если в заголовке нет расширения (отсутствует точка)
		//const wchar_t* pszAdmin = gpSet->szAdminTitleSuffix;
		//const wchar_t* pszFrom = tFileName + origLength - (nMaxLen - nSplit);
		//if (!wcschr(pszFrom, L'.') && (*pszAdmin && !wcsstr(tFileName, pszAdmin)))
		//{
		//	// то троеточие ставить в конец, а не середину
		//	nSplit = nMaxLen;
		//}
		// "{C:\Program Files} - Far 2.1283 Administrator x64"
		// После добавления суффиков к заголовку фара - оно уже влезать не будет в любом случае... Так что если панели - '...' строго ставить в конце
		nSplit = nMaxLen;
		_tcsncpy(szEllip, fileName, nSplit); szEllip[nSplit]=0;
		szEllip[nSplit] = L'\x2026' /*"…"*/;
		szEllip[nSplit+1] = 0;
		//_tcscat(szEllip, L"\x2026" /*"…"*/);
		//_tcscat(szEllip, tFileName + origLength - (nMaxLen - nSplit));
		//tFileName = szEllip;
		lstrcpyn(fileName, szEllip, countof(fileName));
	}

	// szFormat различается для Panel/Viewer(*)/Editor(*)
	// Пример: "%i-[%s] *"
	////pszNo = wcsstr(szFormat, L"%i");
	////pszTitle = wcsstr(szFormat, L"%s");
	////if (pszNo == NULL)
	////	_wsprintf(fileName, SKIPLEN(countof(fileName)) szFormat, tFileName);
	////else if (pszNo < pszTitle || pszTitle == NULL)
	////	_wsprintf(fileName, SKIPLEN(countof(fileName)) szFormat, pTab->Pos, tFileName);
	////else
	////	_wsprintf(fileName, SKIPLEN(countof(fileName)) szFormat, tFileName, pTab->Pos);
	//wcscpy(pTab->Name, fileName);
	const TCHAR* pszFmt = szFormat;
	TCHAR* pszDst = dummy;
	TCHAR* pszStart = pszDst;
	TCHAR* pszEnd = dummy + countof(dummy) - 1; // в конце еще нужно зарезервировать место для '\0'

	if (!pszFmt || !*pszFmt)
	{
		pszFmt = _T("%s");
	}
	*pszDst = 0;

	bool bRenamedTab = false;
	if (pTab->Flags() & fwt_Renamed)
	{
		if (wcsstr(pszFmt, L"%s") == NULL)
		{
			if (wcsstr(pszFmt, L"%n") != NULL)
				bRenamedTab = true;
			else
				pszFmt = _T("%s");
		}
	}

	TCHAR szTmp[64];
	bool  bAppendAdmin = gpSet->isAdminSuffix() && (pTab->Flags() & fwt_Elevated);

	while (*pszFmt && pszDst < pszEnd)
	{
		if (*pszFmt == _T('%'))
		{
			pszFmt++;
			LPCTSTR pszText = NULL;
			switch (*pszFmt)
			{
				case _T('s'): case _T('S'):
					pszText = fileName;
					break;
				case _T('i'): case _T('I'):
					_wsprintf(szTmp, SKIPLEN(countof(szTmp)) _T("%i"), pTab->Info.nFarWindowID);
					pszText = szTmp;
					break;
				case _T('p'): case _T('P'):
					if (!apVCon || !apVCon->RCon())
					{
						wcscpy_c(szTmp, _T("?"));
					}
					else
					{
						_wsprintf(szTmp, SKIPLEN(countof(szTmp)) _T("%u"), apVCon->RCon()->GetActivePID());
					}
					pszText = szTmp;
					break;
				case _T('c'): case _T('C'):
					{
						int iCon = gpConEmu->isVConValid(apVCon);
						if (iCon > 0)
							_wsprintf(szTmp, SKIPLEN(countof(szTmp)) _T("%u"), iCon);
						else
							wcscpy_c(szTmp, _T("?"));
						pszText = szTmp;
					}
					break;
				case _T('n'): case _T('N'):
					{
						pszText = bRenamedTab ? fileName : pRCon ? pRCon->GetActiveProcessName() : NULL;
						wcscpy_c(szTmp, (pszText && *pszText) ? pszText : L"?");
						pszText = szTmp;
					}
					break;
				case _T('a'): case _T('A'):
					pszText = bAppendAdmin ? gpSet->szAdminTitleSuffix : NULL;
					bAppendAdmin = false;
					break;
				case _T('%'):
					pszText = L"%";
					break;
				case 0:
					pszFmt--;
					break;
			}
			pszFmt++;
			if (pszText)
			{
				if ((*(pszDst-1) == L' ') && (*pszText == L' '))
					pszText = SkipNonPrintable(pszText);
				while (*pszText && pszDst < pszEnd)
				{
					*(pszDst++) = *(pszText++);
				}
			}
		}
		else if ((pszDst > pszStart) && (*(pszDst-1) == L' ') && (*pszFmt == L' '))
		{
			pszFmt++; // Avoid adding sequential spaces (e.g. if some macros was empty)
		}
		else
		{
			*(pszDst++) = *(pszFmt++);
		}
	}

	// Fin. Append smth else?
	if (bAppendAdmin)
	{
		LPCTSTR pszText = gpSet->szAdminTitleSuffix;
		if (pszText)
		{
			while (*pszText && pszDst < pszEnd)
			{
				*(pszDst++) = *(pszText++);
			}
		}
	}

	*pszDst = 0;

	#ifdef _DEBUG
	if (dummy[0] && *(pszDst-1) == L' ')
		*pszDst = 0;
	#endif

	pTab->SetLabel(dummy);

	MCHKHEAP;

	return iTabIcon;
}


// Переключение табов

void CTabBarClass::PrintRecentStack()
{
#ifdef PRINT_RECENT_STACK
	if (!this)
		return;
	wchar_t szDbg[100];
	DEBUGSTRRECENT(L"=== Printing recent tab stack ===\n");
	for (INT_PTR i = 0; i < m_TabStack.size(); i++)
	{
		CTabID* p = m_TabStack[i];
		if (p == mp_DummyTab)
			continue;
		if (!p)
		{
			_ASSERTE(p!=NULL);
			continue;
		}
		_wsprintf(szDbg, SKIPLEN(countof(szDbg)) L"%2u: %s", i+1,
			(p->Info.Status == tisPassive) ? L"<passive> " :
			(p->Info.Status == tisEmpty) ? L"<not_init> " :
			(p->Info.Status == tisInvalid) ? L"<invalid> " :
			L"");
		lstrcpyn(szDbg+lstrlen(szDbg), p->GetLabel(), 60);
		wcscat_c(szDbg, L"\n");
		DEBUGSTRRECENT(szDbg);
	}
	DEBUGSTRRECENT(L"===== Recent tab stack done =====\n");
#endif
}

int CTabBarClass::GetNextTab(bool abForward, bool abAltStyle/*=false*/)
{
	BOOL lbRecentMode = (gpSet->isTabs != 0) &&
	                    (((abAltStyle == FALSE) ? gpSet->isTabRecent : !gpSet->isTabRecent));
	int nNewSel = -1;

	// We need "visible" position (selected tab from tabbar)
	// It may differs from actual active tab during switching
	int nCurSel = GetCurSel();
	int nCurCount = GetItemCount();

	#ifdef PRINT_RECENT_STACK
	// Debug method
	PrintRecentStack();
	#endif

	if (lbRecentMode)
	{
		// index in the m_TabStack
		int idxCur = -1;

		// service descriptor
		CTab tabCur(__FILE__,__LINE__);

		// find m_TabStack's index of the current tab
		if (m_Tabs.GetTabByIndex(nCurSel, tabCur))
		{
			for (int i = 0; i < m_TabStack.size(); i++)
			{
				if (m_TabStack[i] == tabCur.Tab())
				{
					idxCur = i; break;
				}
			}
		}

		_ASSERTE(idxCur!=-1);

		nNewSel = GetNextTabHelper(idxCur, abForward, true);

		// Succeeded?
		if (nNewSel >= 0)
			return nNewSel;

		_ASSERTE(nNewSel >= 0);
	}

	// Choose tab straightly
	nNewSel = GetNextTabHelper(nCurSel, abForward, false);

	// Succeeded?
	if (nNewSel >= 0)
		return nNewSel;

	if (nNewSel == -1 && nCurCount > 0)
	{
		_ASSERTE(FALSE && "One tab must be <active> any time");
	}

	// There is no "another" tab?
	return -1;
}

int CTabBarClass::GetNextTabHelper(int idxFrom, bool abForward, bool abRecent)
{
	int nNewSel = -1, iTab;

	// What we iterate
	int nCurCount = abRecent ? m_TabStack.size() : GetItemCount();
	if (idxFrom >= nCurCount)
	{
		_ASSERTE(idxFrom < nCurCount);
		idxFrom = -1;
	}

	// Lets check
	if (abForward)
	{
		if (idxFrom == -1)
			idxFrom = 0; // Assert already shown

		for (int idx = idxFrom+1; idx < nCurCount; idx++)
		{
			if (abRecent && (m_TabStack[idx]->Info.Status != tisValid))
				continue;

			iTab = abRecent ? m_Tabs.GetIndexByTab(m_TabStack[idx]) : idx;

			if (CanActivateTab(iTab))
			{
				nNewSel = iTab;
				goto wrap;
			}
		}

		for (int idx = 0; idx < idxFrom; idx++)
		{
			if (abRecent && (m_TabStack[idx]->Info.Status != tisValid))
				continue;

			iTab = abRecent ? m_Tabs.GetIndexByTab(m_TabStack[idx]) : idx;

			if (CanActivateTab(iTab))
			{
				nNewSel = iTab;
				goto wrap;
			}
		}
	}
	else
	{
		if (idxFrom == -1)
			idxFrom = nCurCount - 1; // Assert already shown

		for (int idx = idxFrom-1; idx >= 0; idx--)
		{
			if (abRecent && (m_TabStack[idx]->Info.Status != tisValid))
				continue;

			iTab = abRecent ? m_Tabs.GetIndexByTab(m_TabStack[idx]) : idx;

			if (CanActivateTab(iTab))
			{
				nNewSel = iTab;
				goto wrap;
			}
		}

		for (int idx = nCurCount-1; idx > idxFrom; idx--)
		{
			if (abRecent && (m_TabStack[idx]->Info.Status != tisValid))
				continue;

			iTab = abRecent ? m_Tabs.GetIndexByTab(m_TabStack[idx]) : idx;

			if (CanActivateTab(iTab))
			{
				nNewSel = iTab;
				goto wrap;
			}
		}
	}

wrap:
	return nNewSel;
}

void CTabBarClass::SwitchNext(BOOL abAltStyle/*=FALSE*/)
{
	Switch(TRUE, abAltStyle);
}

void CTabBarClass::SwitchPrev(BOOL abAltStyle/*=FALSE*/)
{
	Switch(FALSE, abAltStyle);
}

void CTabBarClass::Switch(BOOL abForward, BOOL abAltStyle/*=FALSE*/)
{
	int nNewSel = GetNextTab(abForward, abAltStyle);

	if (nNewSel != -1)
	{
		// mh_Tabbar может быть и создан, Но отключен пользователем!
		if (gpSet->isTabLazy && mp_Rebar->IsTabbarCreated() && gpSet->isTabs)
		{
			mb_InKeySwitching = TRUE;
			// Пока Ctrl не отпущен - только подсвечиваем таб, а не переключаем реально
			SelectTab(nNewSel);
		}
		else
		{
			mp_Rebar->FarSendChangeTab(nNewSel);
			mb_InKeySwitching = FALSE;
		}
	}
}

BOOL CTabBarClass::IsInSwitch()
{
	return mb_InKeySwitching;
}

void CTabBarClass::SwitchCommit()
{
	if (!mb_InKeySwitching) return;

	int nCurSel = GetCurSel();
	mb_InKeySwitching = FALSE;
	CVirtualConsole* pVCon = mp_Rebar->FarSendChangeTab(nCurSel);
	UNREFERENCED_PARAMETER(pVCon);
}

void CTabBarClass::SwitchRollback()
{
	if (mb_InKeySwitching)
	{
		mb_InKeySwitching = FALSE;
		Update();
	}
}

// Убьет из стека старые, и добавит новые (неучтенные)
bool CTabBarClass::CheckStack()
{
	bool bStackChanged = false;
	_ASSERTE(gpConEmu->isMainThread());
	INT_PTR i, j;

	bool lbExist = false;
	j = 0;

	// Remove all absent items
	while (j < m_TabStack.size())
	{
		// If refcount was decreased to 1, that means that CTabID is left only in m_TabStack
		// All other references was eliminated
		if (m_TabStack[j]->Info.Status == tisInvalid)
		{
			#ifdef TAB_REF_PLACE
			m_TabStack[j]->DelPlace("TabBar.cpp:m_TabStack",0);
			#endif
			m_TabStack[j]->Release();
			m_TabStack.erase(j);
			bStackChanged = true;
		}
		else
		{
			j++;
		}
	}

	CTab tab("TabBar.cpp:CheckStack",__LINE__);

	for (i = 0; m_Tabs.GetTabByIndex(i, tab); ++i)
	{
		if (tab.Tab() == mp_DummyTab)
			continue;

		lbExist = false;

		for (j = 0; j < m_TabStack.size(); ++j)
		{
			if (tab.Tab() == m_TabStack[j])
			{
				lbExist = true; break;
			}
		}

		if (!lbExist)
		{
			m_TabStack.push_back(tab.AddRef("TabBar.cpp:m_TabStack",0));
			bStackChanged = true;
		}
	}

	return bStackChanged;
}

// Убьет из стека отсутствующих и поместит tab на верх стека
bool CTabBarClass::AddStack(CTab& tab)
{
	if (tab.Tab() == mp_DummyTab)
		return false;

	bool bStackChanged = false;
	_ASSERTE(gpConEmu->isMainThread());
	bool lbExist = false;

	if (!m_TabStack.empty())
	{
		int iter = 0;

		while (iter < m_TabStack.size())
		{
			if (m_TabStack[iter] == tab.Tab())
			{
				if (iter > 0)
				{
					CTabID* pTab = m_TabStack[iter];
					m_TabStack.erase(iter);
					m_TabStack.insert(0, pTab);
					bStackChanged = true;
				}

				lbExist = true;
				break;
			}

			++iter;
		}
	}

	// поместить наверх стека
	if (!lbExist)
	{
		CTabID* pTab = tab.AddRef("TabBar.cpp:m_TabStack",0);
		m_TabStack.insert(0, pTab);
		bStackChanged = true;
	}

	return bStackChanged;
}

BOOL CTabBarClass::CanActivateTab(int nTabIdx)
{
	CVConGuard VCon;
	DWORD wndIndex = 0;

	if (!GetVConFromTab(nTabIdx, &VCon, &wndIndex))
		return FALSE;

	if (!VCon->RCon()->CanActivateFarWindow(wndIndex))
		return FALSE;

	return TRUE;
}

BOOL CTabBarClass::OnKeyboard(UINT messg, WPARAM wParam, LPARAM lParam)
{
	//if (!IsShown()) return FALSE; -- всегда. Табы теперь есть в памяти
	BOOL lbAltPressed = isPressed(VK_MENU);

	if (messg == WM_KEYDOWN && wParam == VK_TAB)
	{
		if (!isPressed(VK_SHIFT))
			SwitchNext(lbAltPressed);
		else
			SwitchPrev(lbAltPressed);

		return TRUE;
	}
	else if (mb_InKeySwitching && messg == WM_KEYDOWN && !lbAltPressed
	        && (wParam == VK_UP || wParam == VK_DOWN || wParam == VK_LEFT || wParam == VK_RIGHT))
	{
		bool bRecent = gpSet->isTabRecent;
		gpSet->isTabRecent = false;
		BOOL bForward = (wParam == VK_RIGHT || wParam == VK_DOWN);
		Switch(bForward);
		gpSet->isTabRecent = bRecent;

		return TRUE;
	}

	return FALSE;
}

void CTabBarClass::SetRedraw(BOOL abEnableRedraw)
{
	mb_DisableRedraw = !abEnableRedraw;
}

void CTabBarClass::ShowTabError(LPCTSTR asInfo, int tabIndex)
{
	mp_Rebar->ShowTabErrorInt(asInfo, tabIndex);
}

void CTabBarClass::OnChooseTabPopup()
{
	RECT rcBtnRect = {0};
	mp_Rebar->GetToolBtnRect(TID_ACTIVE_NUMBER, &rcBtnRect);
	POINT pt = {rcBtnRect.right,rcBtnRect.bottom};

	gpConEmu->ChooseTabFromMenu(FALSE, pt, TPM_RIGHTALIGN|TPM_TOPALIGN);
}

int CTabBarClass::ActiveTabByName(int anType, LPCWSTR asName, CVirtualConsole** ppVCon)
{
	int nTab = -1;
	CVirtualConsole *pVCon = NULL;
	if (ppVCon)
		*ppVCon = NULL;

	TODO("TabBarClass::ActiveTabByName - найти таб по имени");

	INT_PTR V, I;
	int tabIdx = 0;

	for (V = 0; V < MAX_CONSOLE_COUNT && nTab == -1; V++)
	{
		if (!(pVCon = gpConEmu->GetVCon(V))) continue;

		#ifdef _DEBUG
		BOOL lbActive = CVConGroup::isActive(pVCon, false);
		#endif

		//111120 - Эту опцию игнорируем. Если редактор открыт в другой консоли - активируем ее потом
		//if (gpSet->bHideInactiveConsoleTabs)
		//{
		//	if (!lbActive) continue;
		//}

		CRealConsole *pRCon = pVCon->RCon();

		for (I = 0; TRUE; I++)
		{
			CTab tab(__FILE__,__LINE__);
			if (!pRCon->GetTab(I, tab))
				break;

			if (tab->Type() == (anType & fwt_TypeMask))
			{
				// Тут GetName() использовать нельзя, т.к. оно может возвращать "переименованное пользователем"
				// А здесь ищется конкретный редактор-вьювер фара (то есть нужно правильное полное имя-путь к файлу)
				LPCWSTR pszNamePtr = tab->Name.Ptr();
				LPCWSTR pszName = PointToName(pszNamePtr);
				if (pszNamePtr
					&& ((lstrcmpi(pszName, asName) == 0))
						|| ((pszNamePtr != pszName) && (lstrcmpi(pszNamePtr, asName) == 0)))
				{
					nTab = tabIdx;
					break;
				}
			}

			tabIdx++;
		}
	}


	if (nTab >= 0)
	{
		if (!CanActivateTab(nTab))
		{
			nTab = -2;
		}
		else
		{
			pVCon = mp_Rebar->FarSendChangeTab(nTab);
			if (!pVCon)
				nTab = -2;
		}
	}

	if (ppVCon)
		*ppVCon = pVCon;

	return nTab;
}

void CTabBarClass::UpdateTabFont()
{
	mp_Rebar->UpdateTabFontInt();
}

// Прямоугольник в клиентских координатах ghWnd!
bool CTabBarClass::GetRebarClientRect(RECT* rc)
{
	return mp_Rebar->GetRebarClientRect(rc);
}

bool CTabBarClass::GetActiveTabRect(RECT* rcTab)
{
	bool bSet = false;

	if (!IsTabsShown())
	{
		_ASSERTE(IsTabsShown());
		memset(rcTab, 0, sizeof(*rcTab));
	}
	else
	{
		bSet = mp_Rebar->GetTabRect(-1/*Active*/, rcTab);
	}

	return bSet;
}

bool CTabBarClass::Toolbar_GetBtnRect(int nCmd, RECT* rcBtnRect)
{
	if (!IsTabsShown())
	{
		return false;
	}
	return mp_Rebar->GetToolBtnRect(nCmd, rcBtnRect);
}
