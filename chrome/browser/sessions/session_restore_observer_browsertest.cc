// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <unordered_map>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/tab_load_tracker_test_support.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sessions/session_restore_observer.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::NavigationHandle;

class MockSessionRestoreObserver : public SessionRestoreObserver {
 public:
  MockSessionRestoreObserver() { SessionRestore::AddObserver(this); }

  MockSessionRestoreObserver(const MockSessionRestoreObserver&) = delete;
  MockSessionRestoreObserver& operator=(const MockSessionRestoreObserver&) =
      delete;

  ~MockSessionRestoreObserver() { SessionRestore::RemoveObserver(this); }

  enum class SessionRestoreEvent { kStartedLoadingTabs, kFinishedLoadingTabs };

  const std::vector<SessionRestoreEvent>& session_restore_events() const {
    return session_restore_events_;
  }

  // SessionRestoreObserver implementation:
  void OnSessionRestoreStartedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::kStartedLoadingTabs);
  }
  void OnSessionRestoreFinishedLoadingTabs() override {
    session_restore_events_.emplace_back(
        SessionRestoreEvent::kFinishedLoadingTabs);
  }

 private:
  std::vector<SessionRestoreEvent> session_restore_events_;
};

class SessionRestoreObserverTest : public InProcessBrowserTest {
 protected:
  SessionRestoreObserverTest() {}

  SessionRestoreObserverTest(const SessionRestoreObserverTest&) = delete;
  SessionRestoreObserverTest& operator=(const SessionRestoreObserverTest&) =
      delete;

  void SetUpOnMainThread() override {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfile(browser()->profile()));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
#endif
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Browser* QuitBrowserAndRestore(Browser* browser) {
    Profile* profile = browser->profile();

    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
    CloseBrowserSynchronously(browser);

    // Create a new window, which should trigger session restore.
    chrome::NewEmptyWindow(profile);
    SessionRestoreTestHelper().Wait();
    return BrowserList::GetInstance()->GetLastActive();
  }

  void WaitForTabsToLoad(Browser* browser) {
    for (int i = 0; i < browser->tab_strip_model()->count(); ++i) {
      WebContents* contents = browser->tab_strip_model()->GetWebContentsAt(i);
      contents->GetController().LoadIfNecessary();
      resource_coordinator::WaitForTransitionToLoaded(contents);
    }
  }

  GURL GetTestURL() const {
    return embedded_test_server()->GetURL("/title1.html");
  }

  const std::vector<MockSessionRestoreObserver::SessionRestoreEvent>&
  session_restore_events() const {
    return mock_observer_.session_restore_events();
  }

  size_t number_of_session_restore_events() const {
    return session_restore_events().size();
  }

  MockSessionRestoreObserver& session_restore_observer() {
    return mock_observer_;
  }

 private:
  MockSessionRestoreObserver mock_observer_;
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SingleTabSessionRestore DISABLED_SingleTabSessionRestore
#else
#define MAYBE_SingleTabSessionRestore SingleTabSessionRestore
#endif
IN_PROC_BROWSER_TEST_F(SessionRestoreObserverTest,
                       MAYBE_SingleTabSessionRestore) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  Browser* new_browser = QuitBrowserAndRestore(browser());

  // The restored browser should have 1 tab.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(1, tab_strip->count());

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::kStartedLoadingTabs,
      session_restore_events()[0]);

  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));
  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::kFinishedLoadingTabs,
      session_restore_events()[1]);
}

IN_PROC_BROWSER_TEST_F(SessionRestoreObserverTest, MultipleTabSessionRestore) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  Browser* new_browser = QuitBrowserAndRestore(browser());

  // The restored browser should have 2 tabs.
  TabStripModel* tab_strip = new_browser->tab_strip_model();
  ASSERT_TRUE(tab_strip);
  ASSERT_EQ(2, tab_strip->count());

  ASSERT_EQ(1u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::kStartedLoadingTabs,
      session_restore_events()[0]);

  ASSERT_NO_FATAL_FAILURE(WaitForTabsToLoad(new_browser));
  ASSERT_EQ(2u, number_of_session_restore_events());
  EXPECT_EQ(
      MockSessionRestoreObserver::SessionRestoreEvent::kFinishedLoadingTabs,
      session_restore_events()[1]);
}
