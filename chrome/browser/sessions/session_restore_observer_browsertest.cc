// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <unordered_map>

#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/session_startup_pref.h"
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
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::WebContents;
using content::NavigationHandle;

// This class records session-restore states of a tab when it starts navigation.
class NavigationStartWebContentsObserver : public content::WebContentsObserver {
 public:
  explicit NavigationStartWebContentsObserver(WebContents* contents)
      : WebContentsObserver(contents) {}

  // content::WebContentsObserver implementation:
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    WebContents* contents = navigation_handle->GetWebContents();
    resource_coordinator::TabManager* tab_manager =
        g_browser_process->GetTabManager();
    ASSERT_TRUE(tab_manager);

    is_session_restored_ = tab_manager->IsTabInSessionRestore(contents);
    is_restored_in_foreground_ =
        tab_manager->IsTabRestoredInForeground(contents);
  }

  // Returns the session-restore states at the navigation start.
  bool is_session_restored() const { return is_session_restored_; }
  bool is_restored_in_foreground() const { return is_restored_in_foreground_; }

 private:
  bool is_session_restored_ = false;
  bool is_restored_in_foreground_ = false;

  DISALLOW_COPY_AND_ASSIGN(NavigationStartWebContentsObserver);
};

class MockSessionRestoreObserver : public SessionRestoreObserver {
 public:
  MockSessionRestoreObserver() { SessionRestore::AddObserver(this); }
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
  void OnWillRestoreTab(WebContents* contents) override {
    navigation_start_observers_.emplace(
        contents, new NavigationStartWebContentsObserver(contents));
  }

  NavigationStartWebContentsObserver*
  GetNavigationStartWebContentsObserverForTab(WebContents* contents) {
    return navigation_start_observers_[contents].get();
  }

 private:
  std::vector<SessionRestoreEvent> session_restore_events_;
  std::unordered_map<WebContents*,
                     std::unique_ptr<NavigationStartWebContentsObserver>>
      navigation_start_observers_;

  DISALLOW_COPY_AND_ASSIGN(MockSessionRestoreObserver);
};

class SessionRestoreObserverTest : public InProcessBrowserTest {
 protected:
  SessionRestoreObserverTest() {}

  void SetUpOnMainThread() override {
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
#if defined(OS_CHROMEOS)
    SessionServiceTestHelper helper(
        SessionServiceFactory::GetForProfile(browser()->profile()));
    helper.SetForceBrowserNotAliveWithNoWindows(true);
    helper.ReleaseService();
#endif
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  Browser* QuitBrowserAndRestore(Browser* browser) {
    Profile* profile = browser->profile();

    std::unique_ptr<ScopedKeepAlive> keep_alive(new ScopedKeepAlive(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED));
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

  DISALLOW_COPY_AND_ASSIGN(SessionRestoreObserverTest);
};

#if defined(OS_LINUX)
#define MAYBE_SingleTabSessionRestore DISABLED_SingleTabSessionRestore
#else
#define MAYBE_SingleTabSessionRestore SingleTabSessionRestore
#endif
IN_PROC_BROWSER_TEST_F(SessionRestoreObserverTest,
                       MAYBE_SingleTabSessionRestore) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
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

  // The only restored tab should be in foreground.
  NavigationStartWebContentsObserver* observer =
      session_restore_observer().GetNavigationStartWebContentsObserverForTab(
          new_browser->tab_strip_model()->GetWebContentsAt(0));
  EXPECT_TRUE(observer->is_session_restored());
  EXPECT_TRUE(observer->is_restored_in_foreground());

  // A new foreground tab should not be created by session restore.
  ui_test_utils::NavigateToURLWithDisposition(
      new_browser, GetTestURL(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
  resource_coordinator::TabManager* tab_manager =
      g_browser_process->GetTabManager();
  WebContents* contents = new_browser->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_TRUE(contents);
  EXPECT_FALSE(tab_manager->IsTabInSessionRestore(contents));
  EXPECT_FALSE(tab_manager->IsTabRestoredInForeground(contents));
}

IN_PROC_BROWSER_TEST_F(SessionRestoreObserverTest, MultipleTabSessionRestore) {
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetTestURL(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_NAVIGATION);
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

  // The first tab should be restored in foreground.
  NavigationStartWebContentsObserver* observer =
      session_restore_observer().GetNavigationStartWebContentsObserverForTab(
          new_browser->tab_strip_model()->GetWebContentsAt(0));
  ASSERT_TRUE(observer);
  EXPECT_TRUE(observer->is_session_restored());
  EXPECT_TRUE(observer->is_restored_in_foreground());

  // The second tab should be restored in background.
  observer =
      session_restore_observer().GetNavigationStartWebContentsObserverForTab(
          new_browser->tab_strip_model()->GetWebContentsAt(1));
  ASSERT_TRUE(observer);
  EXPECT_TRUE(observer->is_session_restored());
  EXPECT_FALSE(observer->is_restored_in_foreground());
}
