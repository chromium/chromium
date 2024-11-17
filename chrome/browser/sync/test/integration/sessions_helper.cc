// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/sessions_helper.h"

#include <stddef.h>

#include <set>
#include <utility>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_datatype_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/sync/service/sync_client.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "url/gurl.h"

using sync_datatype_helper::test;

namespace sessions_helper {

namespace {

Profile* GetProfileOrDie(int browser_index) {
  Profile* profile = test()->GetProfile(browser_index);
  CHECK(profile);
  return profile;
}

Browser* GetBrowserOrDie(int browser_index) {
  Browser* browser = test()->GetBrowser(browser_index);
  CHECK(browser);
  return browser;
}

bool SessionsSyncBridgeHasTabWithURL(int browser_index, const GURL& url) {
  content::RunAllPendingInMessageLoop();
  const sync_sessions::SyncedSession* local_session;
  if (!GetLocalSession(browser_index, &local_session)) {
    return false;
  }

  CHECK(local_session);
  if (local_session->windows.empty()) {
    DVLOG(1) << "Empty windows vector";
    return false;
  }

  int nav_index;
  sessions::SerializedNavigationEntry nav;
  for (const auto& [window_id, window] : local_session->windows) {
    CHECK(window);
    if (window->wrapped_window.tabs.empty()) {
      DVLOG(1) << "Empty tabs vector";
      continue;
    }
    for (const std::unique_ptr<sessions::SessionTab>& tab :
         window->wrapped_window.tabs) {
      if (tab->navigations.empty()) {
        DVLOG(1) << "Empty navigations vector";
        continue;
      }
      nav_index = tab->current_navigation_index;
      CHECK_GE(nav_index, 0);
      CHECK_LT(nav_index, static_cast<int>(tab->navigations.size()));
      nav = tab->navigations[nav_index];
      if (nav.virtual_url() == url) {
        DVLOG(1) << "Found tab with url " << url.spec();
        DVLOG(1) << "Timestamp is " << nav.timestamp().ToInternalValue();
        if (nav.title().empty()) {
          DVLOG(1) << "Title empty -- tab hasn't finished loading yet";
          continue;
        }
        return true;
      }
    }
  }
  DVLOG(1) << "Could not find tab with url " << url.spec();
  return false;
}

bool CompareSyncedSessions(const sync_sessions::SyncedSession* lhs,
                           const sync_sessions::SyncedSession* rhs) {
  if (!lhs || !rhs || lhs->windows.empty() || rhs->windows.empty()) {
    // Catchall for uncomparable data.
    return false;
  }

  return lhs->windows < rhs->windows;
}

void SortSyncedSessions(SyncedSessionVector* sessions) {
  base::ranges::sort(*sessions, CompareSyncedSessions);
}

std::vector<sync_pb::SessionSpecifics> SyncEntitiesToSessionSpecifics(
    std::vector<sync_pb::SyncEntity> entities) {
  std::vector<sync_pb::SessionSpecifics> sessions;
  for (sync_pb::SyncEntity& entity : entities) {
    DCHECK(entity.specifics().has_session());
    sessions.push_back(std::move(entity.specifics().session()));
  }
  return sessions;
}

}  // namespace

bool GetLocalSession(int browser_index,
                     const sync_sessions::SyncedSession** session) {
  CHECK(session);
  sync_sessions::SessionSyncService* session_sync_service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(
          GetProfileOrDie(browser_index));
  CHECK(session_sync_service);
  sync_sessions::OpenTabsUIDelegate* delegate =
      session_sync_service->GetOpenTabsUIDelegate();
  if (!delegate) {
    return false;
  }
  return delegate->GetLocalSession(session);
}

bool OpenTab(int browser_index, const GURL& url) {
  DVLOG(1) << "Opening tab: " << url.spec() << " using browser "
           << browser_index << ".";
  TabStripModel* tab_strip = GetBrowserOrDie(browser_index)->tab_strip_model();
  int tab_index = tab_strip->count();
  return OpenTabAtIndex(browser_index, tab_index, url);
}

bool OpenTabAtIndex(int browser_index, int tab_index, const GURL& url) {
  chrome::AddTabAt(GetBrowserOrDie(browser_index), url, tab_index, true);
  return WaitForTabToLoad(browser_index, url,
                          test()
                              ->GetBrowser(browser_index)
                              ->tab_strip_model()
                              ->GetWebContentsAt(tab_index));
}

bool OpenMultipleTabs(int browser_index, const std::vector<GURL>& urls) {
  Browser* browser = GetBrowserOrDie(browser_index);
  for (const GURL& url : urls) {
    DVLOG(1) << "Opening tab: " << url.spec() << " using browser "
             << browser_index << ".";
    ShowSingletonTab(browser, url);
  }
  return WaitForTabsToLoad(browser_index, urls);
}

void CloseTab(int browser_index, int tab_index) {
  TabStripModel* tab_strip = GetBrowserOrDie(browser_index)->tab_strip_model();
  tab_strip->CloseWebContentsAt(tab_index, TabCloseTypes::CLOSE_USER_GESTURE);
}

void MoveTab(int from_browser_index, int to_browser_index, int tab_index) {
  std::unique_ptr<tabs::TabModel> detached_tab =
      test()
          ->GetBrowser(from_browser_index)
          ->tab_strip_model()
          ->DetachTabAtForInsertion(tab_index);

  TabStripModel* target_strip =
      test()->GetBrowser(to_browser_index)->tab_strip_model();
  target_strip->InsertDetachedTabAt(
      target_strip->count(), std::move(detached_tab), AddTabTypes::ADD_ACTIVE);
}

void NavigateTab(int browser_index, const GURL& url) {
  NavigateParams params(GetBrowserOrDie(browser_index), url,
                        ui::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::CURRENT_TAB;
  ui_test_utils::NavigateToURL(&params);
}

void NavigateTabBack(int browser_index) {
  content::WebContents* web_contents =
      GetBrowserOrDie(browser_index)->tab_strip_model()->GetWebContentsAt(0);
  content::TestNavigationObserver observer(web_contents);
  web_contents->GetController().GoBack();
  observer.WaitForNavigationFinished();
}

void NavigateTabForward(int browser_index) {
  content::WebContents* web_contents =
      GetBrowserOrDie(browser_index)->tab_strip_model()->GetWebContentsAt(0);
  content::TestNavigationObserver observer(web_contents);
  web_contents->GetController().GoForward();
  observer.WaitForNavigationFinished();
}

bool WaitForTabsToLoad(int browser_index, const std::vector<GURL>& urls) {
  int tab_index = 0;
  for (const GURL& url : urls) {
    content::WebContents* web_contents = test()
                                             ->GetBrowser(browser_index)
                                             ->tab_strip_model()
                                             ->GetWebContentsAt(tab_index);
    if (!web_contents) {
      LOG(ERROR) << "Tab " << tab_index << " does not exist";
      return false;
    }
    bool success = WaitForTabToLoad(browser_index, url, web_contents);
    if (!success) {
      return false;
    }
    tab_index++;
  }
  return true;
}

bool WaitForTabToLoad(int browser_index,
                      const GURL& url,
                      content::WebContents* web_contents) {
  CHECK(web_contents);
  DVLOG(1) << "Waiting for session to propagate to associator.";
  base::TimeTicks start_time = base::TimeTicks::Now();
  base::TimeTicks end_time = start_time + TestTimeouts::action_max_timeout();
  bool found = false;
  while (!found) {
    found = SessionsSyncBridgeHasTabWithURL(browser_index, url);
    if (base::TimeTicks::Now() >= end_time) {
      LOG(ERROR) << "Failed to find url " << url.spec() << " in tab after "
                 << TestTimeouts::action_max_timeout().InSecondsF()
                 << " seconds.";
      return false;
    }
    if (!found) {
      content::WaitForLoadStop(web_contents);
    }
  }
  return true;
}

bool GetLocalWindows(int browser_index, ScopedWindowMap* local_windows) {
  // The local session provided by GetLocalSession is owned, and has lifetime
  // controlled, by the sessions sync manager, so we must make our own copy.
  const sync_sessions::SyncedSession* local_session;
  if (!GetLocalSession(browser_index, &local_session)) {
    return false;
  }
  for (const auto& [window_id, synced_window] : local_session->windows) {
    const sessions::SessionWindow& window = synced_window->wrapped_window;
    std::unique_ptr<sync_sessions::SyncedSessionWindow> new_window =
        std::make_unique<sync_sessions::SyncedSessionWindow>();
    new_window->wrapped_window.window_id =
        SessionID::FromSerializedValue(window.window_id.id());
    for (const std::unique_ptr<sessions::SessionTab>& tab : window.tabs) {
      std::unique_ptr<sessions::SessionTab> new_tab =
          std::make_unique<sessions::SessionTab>();
      new_tab->navigations.resize(tab->navigations.size());
      base::ranges::copy(tab->navigations, new_tab->navigations.begin());
      new_window->wrapped_window.tabs.push_back(std::move(new_tab));
    }
    SessionID id = new_window->wrapped_window.window_id;
    (*local_windows)[id] = std::move(new_window);
  }

  return true;
}

bool CheckInitialState(int browser_index) {
  if (0 != GetNumWindows(browser_index)) {
    return false;
  }
  if (0 != GetNumForeignSessions(browser_index)) {
    return false;
  }
  return true;
}

int GetNumWindows(int browser_index) {
  const sync_sessions::SyncedSession* local_session;
  if (!GetLocalSession(browser_index, &local_session)) {
    return 0;
  }
  return local_session->windows.size();
}

int GetNumForeignSessions(int browser_index) {
  SyncedSessionVector sessions;
  if (!SessionSyncServiceFactory::GetInstance()
           ->GetForProfile(GetProfileOrDie(browser_index))
           ->GetOpenTabsUIDelegate()
           ->GetAllForeignSessions(&sessions)) {
    return 0;
  }
  return sessions.size();
}

bool GetSessionData(int browser_index, SyncedSessionVector* sessions) {
  if (!SessionSyncServiceFactory::GetInstance()
           ->GetForProfile(GetProfileOrDie(browser_index))
           ->GetOpenTabsUIDelegate()
           ->GetAllForeignSessions(sessions)) {
    return false;
  }
  SortSyncedSessions(sessions);
  return true;
}

bool NavigationEquals(const sessions::SerializedNavigationEntry& expected,
                      const sessions::SerializedNavigationEntry& actual) {
  if (expected.virtual_url() != actual.virtual_url()) {
    LOG(ERROR) << "Expected url " << expected.virtual_url() << ", actual "
               << actual.virtual_url();
    return false;
  }
  if (expected.referrer_url() != actual.referrer_url()) {
    LOG(ERROR) << "Expected referrer " << expected.referrer_url() << ", actual "
               << actual.referrer_url();
    return false;
  }
  if (expected.title() != actual.title()) {
    LOG(ERROR) << "Expected title " << expected.title() << ", actual "
               << actual.title();
    return false;
  }
  if (!ui::PageTransitionTypeIncludingQualifiersIs(expected.transition_type(),
                                                   actual.transition_type())) {
    LOG(ERROR) << "Expected transition " << expected.transition_type()
               << ", actual " << actual.transition_type();
    return false;
  }
  return true;
}

bool WindowsMatch(const ScopedWindowMap& win1, const ScopedWindowMap& win2) {
  sessions::SessionTab* client0_tab;
  sessions::SessionTab* client1_tab;
  if (win1.size() != win2.size()) {
    LOG(ERROR) << "Win size doesn't match, win1 size: " << win1.size()
               << ", win2 size: " << win2.size();
    return false;
  }
  for (const auto& [guid, window1] : win1) {
    auto iter = win2.find(guid);
    if (iter == win2.end()) {
      LOG(ERROR) << "Session doesn't match";
      return false;
    }

    const sync_sessions::SyncedSessionWindow* window2 = iter->second.get();
    if (window1->wrapped_window.tabs.size() !=
        window2->wrapped_window.tabs.size()) {
      LOG(ERROR) << "Tab size doesn't match, tab1 size: "
                 << window1->wrapped_window.tabs.size()
                 << ", tab2 size: " << window2->wrapped_window.tabs.size();
      return false;
    }
    for (size_t t = 0; t < window1->wrapped_window.tabs.size(); ++t) {
      client0_tab = window1->wrapped_window.tabs[t].get();
      client1_tab = window2->wrapped_window.tabs[t].get();
      if (client0_tab->navigations.size() != client1_tab->navigations.size()) {
        return false;
      }
      for (size_t n = 0; n < client0_tab->navigations.size(); ++n) {
        if (!NavigationEquals(client0_tab->navigations[n],
                              client1_tab->navigations[n])) {
          return false;
        }
      }
    }
  }

  return true;
}

void DeleteForeignSession(int browser_index, std::string session_tag) {
  SessionSyncServiceFactory::GetInstance()
      ->GetForProfile(GetProfileOrDie(browser_index))
      ->GetOpenTabsUIDelegate()
      ->DeleteForeignSession(session_tag);
}

ForeignSessionsMatchChecker::ForeignSessionsMatchChecker(
    int profile_index,
    int foreign_profile_index)
    : MultiClientStatusChangeChecker(
          sync_datatype_helper::test()->GetSyncServices()),
      profile_index_(profile_index),
      foreign_profile_index_(foreign_profile_index) {}

bool ForeignSessionsMatchChecker::IsExitConditionSatisfied(std::ostream* os) {
  *os << "Waiting for matching foreign sessions";

  const sync_sessions::SyncedSession* foreign_local_sessions;
  if (!GetLocalSession(foreign_profile_index_, &foreign_local_sessions)) {
    *os << "Cannot get local sessions from profile " << foreign_profile_index_
        << ".";
    return false;
  }
  CHECK(foreign_local_sessions);

  SyncedSessionVector sessions;
  GetSessionData(profile_index_, &sessions);

  if (foreign_local_sessions->windows.empty() && sessions.empty()) {
    // The case when the remote session has deleted all tabs. In this case if
    // there is no local windows and remote sessions, then it is considered to
    // match.
    return true;
  }

  for (const sync_sessions::SyncedSession* remote_session : sessions) {
    if (WindowsMatch(remote_session->windows,
                     foreign_local_sessions->windows)) {
      return true;
    }
  }

  *os << "Can't match sessions for profile " << foreign_profile_index_ << ".";
  return false;
}

SessionEntitiesChecker::SessionEntitiesChecker(const Matcher& matcher)
    : matcher_(matcher) {}

SessionEntitiesChecker::~SessionEntitiesChecker() = default;

bool SessionEntitiesChecker::IsExitConditionSatisfied(std::ostream* os) {
  std::vector<sync_pb::SessionSpecifics> entities =
      SyncEntitiesToSessionSpecifics(
          fake_server()->GetSyncEntitiesByDataType(syncer::SESSIONS));

  testing::StringMatchResultListener result_listener;
  const bool matches =
      testing::ExplainMatchResult(matcher_, entities, &result_listener);
  *os << result_listener.str();
  return matches;
}

}  // namespace sessions_helper
