// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <optional>

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/keep_alive/profile_keep_alive_types.h"
#include "chrome/browser/profiles/keep_alive/scoped_profile_keep_alive.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/tab_loader_tester.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/sessions/tab_restore_service_load_waiter.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_live_tab_context.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/javascript_dialogs/app_modal_dialog_controller.h"
#include "components/javascript_dialogs/app_modal_dialog_view.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/sessions/core/tab_restore_service_observer.h"
#include "components/sessions/core/tab_restore_types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/range/range.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
#include "chrome/browser/sessions/tab_loader.h"
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

namespace sessions {
class TabRestoreTest : public InProcessBrowserTest {
 public:
  TabRestoreTest()
      : active_browser_list_(nullptr),
        animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    url1_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot1.html"));
    url2_ = ui_test_utils::GetTestUrl(
        base::FilePath().AppendASCII("session_history"),
        base::FilePath().AppendASCII("bot2.html"));
  }

  TabRestoreTest(const TabRestoreTest&) = delete;
  TabRestoreTest& operator=(const TabRestoreTest&) = delete;

#if BUILDFLAG(ENABLE_SESSION_SERVICE)
  void SetMaxSimultaneousLoadsForTesting(TabLoader* tab_loader) {
    TabLoaderTester tester(tab_loader);
    tester.SetMaxSimultaneousLoadsForTesting(1);
  }
#endif

 protected:
  void SetUpOnMainThread() override {
    active_browser_list_ = BrowserList::GetInstance();

    host_resolver()->AddRule("*", "127.0.0.1");
    content::SetupCrossSiteRedirector(embedded_test_server());
  }

  Browser* GetBrowser(int index) {
    CHECK(static_cast<int>(active_browser_list_->size()) > index);
    return active_browser_list_->get(index);
  }

  // Adds tabs to the given browser, all navigated to url1_(Uses a file://
  // scheme). Returns the final number of tabs.
  int AddFileSchemeTabs(Browser* browser, int how_many) {
    int starting_tab_count = browser->tab_strip_model()->count();

    for (int i = 0; i < how_many; ++i) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser, url1_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    int tab_count = browser->tab_strip_model()->count();
    EXPECT_EQ(starting_tab_count + how_many, tab_count);
    return tab_count;
  }

  // Same as AddSomeTabs but uses the https:// scheme instead of url1_ which
  // uses a file scheme path.
  int AddHTTPSSchemeTabs(Browser* browser, int num_tabs) {
    int starting_tab_count = browser->tab_strip_model()->count();

    for (int i = 0; i < num_tabs; ++i) {
      ui_test_utils::NavigateToURLWithDisposition(
          browser,
          GURL(std::string("https://www.") + base::NumberToString(i) +
               std::string(".com")),
          WindowOpenDisposition::NEW_FOREGROUND_TAB,
          ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    }
    int tab_count = browser->tab_strip_model()->count();
    EXPECT_EQ(starting_tab_count + num_tabs, tab_count);
    return tab_count;
  }

  void CloseTab(int index) {
    content::WebContentsDestroyedWatcher destroyed_watcher(
        browser()->tab_strip_model()->GetWebContentsAt(index));
    browser()->tab_strip_model()->CloseWebContentsAt(
        index, TabCloseTypes::CLOSE_CREATE_HISTORICAL_TAB);
    destroyed_watcher.Wait();
  }

  void CloseGroup(const tab_groups::TabGroupId& group) {
    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    int first_tab_in_group =
        tab_strip_model->group_model()->GetTabGroup(group)->ListTabs().start();
    content::WebContentsDestroyedWatcher destroyed_watcher(
        tab_strip_model->GetWebContentsAt(first_tab_in_group));
    browser()->tab_strip_model()->CloseAllTabsInGroup(group);
    destroyed_watcher.Wait();
  }

  // Uses the undo-close-tab accelerator to undo the most recent close
  // operation.
  content::WebContents* RestoreMostRecentlyClosed(Browser* browser) {
    ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;
    {
      TabRestoreServiceLoadWaiter waiter(
          TabRestoreServiceFactory::GetForProfile(browser->profile()));
      chrome::RestoreTab(browser);
      waiter.Wait();
    }
    content::WebContents* new_tab = tab_added_waiter.Wait();
    content::WaitForLoadStop(new_tab);
    return new_tab;
  }

  // Uses the undo-close-tab accelerator to undo a close-tab or close-window
  // operation. The newly restored tab is expected to appear in the
  // window at index |expected_window_index|, at the |expected_tabstrip_index|,
  // and to be active. If |expected_window_index| is equal to the number of
  // current windows, the restored tab is expected to be created in a new
  // window (since the index is 0-based).
  std::optional<tab_groups::TabGroupId> RestoreTab(
      int expected_window_index,
      int expected_tabstrip_index) {
    int window_count = static_cast<int>(active_browser_list_->size());
    CHECK_GT(window_count, 0);

    bool expect_new_window = (expected_window_index == window_count);

    Browser* browser;
    if (expect_new_window) {
      browser = active_browser_list_->get(0);
    } else {
      browser = GetBrowser(expected_window_index);
    }
    int tab_count = browser->tab_strip_model()->count();
    CHECK_GT(tab_count, 0);

    // Restore the tab.
    content::WebContents* new_tab = RestoreMostRecentlyClosed(browser);

    if (expect_new_window) {
      int new_window_count = static_cast<int>(active_browser_list_->size());
      EXPECT_EQ(++window_count, new_window_count);
      browser = GetBrowser(expected_window_index);
    } else {
      EXPECT_EQ(++tab_count, browser->tab_strip_model()->count());
    }

    EXPECT_EQ(chrome::FindBrowserWithTab(new_tab), browser);

    // Get a handle to the restored tab.
    CHECK_GT(browser->tab_strip_model()->count(), expected_tabstrip_index);

    // Ensure that the tab and window are active.
    EXPECT_EQ(expected_tabstrip_index,
              browser->tab_strip_model()->active_index());
    std::optional<tab_groups::TabGroupId> restored_group_id =
        browser->tab_strip_model()->GetTabGroupForTab(
            browser->tab_strip_model()->GetIndexOfWebContents(new_tab));

    return restored_group_id;
  }

  // Uses the undo-close-tab accelerator to undo a close-group operation.
  // The first tab in the group |expected_group| is expected to appear in the
  // window at index |expected_window_index|, at the |expected_tabstrip_index|.
  // If |expected_window_index| is equal to the number of current windows, the
  // restored tab is expected to be created in a new window.
  tab_groups::TabGroupId RestoreGroup(tab_groups::TabGroupId expected_group,
                                      int expected_window_index,
                                      int expected_tabstrip_index) {
    int window_count = static_cast<int>(active_browser_list_->size());
    CHECK_GT(window_count, 0);

    bool expect_new_window = (expected_window_index == window_count);

    Browser* browser;
    if (expect_new_window) {
      browser = active_browser_list_->get(0);
    } else {
      browser = GetBrowser(expected_window_index);
    }

    // Get the baseline conditions to compare against post-restore.
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    TabGroupModel* group_model = tab_strip_model->group_model();
    int tab_count = tab_strip_model->count();
    int group_count = group_model->ListTabGroups().size();
    CHECK_GT(tab_count, 0);

    // Restore the group. Returns the last tab in the group that is restored.
    content::WebContents* content = RestoreMostRecentlyClosed(browser);
    CHECK(content);

    std::optional<tab_groups::TabGroupId> restored_group_id =
        tab_strip_model->GetTabGroupForTab(
            tab_strip_model->GetIndexOfWebContents(content));
    CHECK(restored_group_id.has_value())
        << "Expected restored tab to be part of a group but wasn't";

    if (!tab_groups::IsTabGroupsSaveV2Enabled()) {
      CHECK_EQ(restored_group_id.value(), expected_group);
    }

    // Reset all baseline conditions if a new window is expected to be opened.
    // Note that we're resetting what browser models to compare against as well
    // as the baseline counts for those models.
    if (expect_new_window) {
      EXPECT_EQ(++window_count, static_cast<int>(active_browser_list_->size()));
      browser = GetBrowser(expected_window_index);
      tab_strip_model = browser->tab_strip_model();
      group_model = tab_strip_model->group_model();
      tab_count = 0;
      group_count = 0;
    }

    EXPECT_EQ(++group_count,
              static_cast<int>(group_model->ListTabGroups().size()));

    // TODO(crbug.com/347746086): When TabGroupsSaveV2 is enabled, restored
    // groups are often not opened on the main restore flow. Additionally, their
    // tab group ids are regenerated to prevent collisions. This new id will be
    // different than `expected_group`. Groups are add into a list, so the most
    // recent one can be found at the end, otherwise, the group was already open
    // and we had an id for it. Clean this up post launch.
    gfx::Range tabs_in_group =
        group_model->GetTabGroup(restored_group_id.value())->ListTabs();

    // Expect the entire group to be restored to the right place.
    EXPECT_EQ(tab_count + static_cast<int>(tabs_in_group.length()),
              tab_strip_model->count());
    EXPECT_EQ(static_cast<int>(tabs_in_group.start()), expected_tabstrip_index);

    return restored_group_id.value();
  }

  void GoBack(Browser* browser) {
    content::LoadStopObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    chrome::GoBack(browser, WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }

  void GoForward(Browser* browser) {
    content::LoadStopObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    chrome::GoForward(browser, WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }

  void EnsureTabFinishedRestoring(content::WebContents* tab) {
    content::NavigationController* controller = &tab->GetController();
    if (!controller->NeedsReload() && !controller->GetPendingEntry() &&
        !tab->IsLoading())
      return;

    content::LoadStopObserver observer(tab);
    observer.Wait();
  }

  void EnableSessionService(
      SessionStartupPref::Type type = SessionStartupPref::Type::DEFAULT) {
    SessionStartupPref pref(type);
    Profile* profile = browser()->profile();
    SessionStartupPref::SetStartupPref(profile, pref);
  }

  GURL url1_;
  GURL url2_;

  raw_ptr<const BrowserList> active_browser_list_;

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
};

// Close the end tab in the current window, then restore it. The tab should be
// in its original position, and active.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, Basic) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  int tab_count = AddFileSchemeTabs(browser(), 1);

  int closed_tab_index = tab_count - 1;
  CloseTab(closed_tab_index);
  EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, closed_tab_index));

  // And make sure everything looks right.
  EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(closed_tab_index, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  // Make sure that the navigation type reported is "back_forward" on the
  // duplicated tab.
  EXPECT_EQ(
      "back_forward",
      content::EvalJs(browser()->tab_strip_model()->GetActiveWebContents(),
                      "performance.getEntriesByType('navigation')[0].type"));
}

// Close a tab not at the end of the current window, then restore it. The tab
// should be in its original position, and active.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, MiddleTab) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  AddFileSchemeTabs(browser(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  CloseTab(closed_tab_index);
  EXPECT_EQ(starting_tab_count + 2, browser()->tab_strip_model()->count());

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, closed_tab_index));

  // And make sure everything looks right.
  EXPECT_EQ(starting_tab_count + 3, browser()->tab_strip_model()->count());
  EXPECT_EQ(closed_tab_index, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Close a tab, switch windows, then restore the tab. The tab should be in its
// original window and position, and active.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreToDifferentWindow) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  AddFileSchemeTabs(browser(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  CloseTab(closed_tab_index);
  EXPECT_EQ(starting_tab_count + 2, browser()->tab_strip_model()->count());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Restore tab into original browser.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, closed_tab_index));

  // And make sure everything looks right.
  EXPECT_EQ(starting_tab_count + 3, browser()->tab_strip_model()->count());
  EXPECT_EQ(closed_tab_index, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Close a tab, open a new window, close the first window, then restore the
// tab. It should be in a new window.
// If this becomes flaky, use http://crbug.com/14774
IN_PROC_BROWSER_TEST_F(TabRestoreTest, DISABLED_BasicRestoreFromClosedWindow) {
  // Navigate to url1 then url2.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1_));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2_));

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Close the final tab in the first browser.
  CloseTab(0);
  ui_test_utils::WaitForBrowserToClose();

  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, 0));

  // Tab should be in a new window.
  Browser* browser = GetBrowser(1);
  content::WebContents* web_contents =
      browser->tab_strip_model()->GetActiveWebContents();
  // And make sure the URLs match.
  EXPECT_EQ(url2_, web_contents->GetURL());
  GoBack(browser);
  EXPECT_EQ(url1_, web_contents->GetURL());
}

#if BUILDFLAG(IS_WIN)
// Flakily times out: http://crbug.com/171503
#define MAYBE_DontLoadRestoredTab DISABLED_DontLoadRestoredTab
#else
#define MAYBE_DontLoadRestoredTab DontLoadRestoredTab
#endif

// Restore a tab then make sure it doesn't restore again.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, MAYBE_DontLoadRestoredTab) {
  // Add two tabs
  int starting_tab_count = browser()->tab_strip_model()->count();
  AddFileSchemeTabs(browser(), 2);
  ASSERT_EQ(browser()->tab_strip_model()->count(), starting_tab_count + 2);

  // Close one of them.
  CloseTab(0);
  ASSERT_EQ(browser()->tab_strip_model()->count(), starting_tab_count + 1);

  // Restore it.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, 0));
  ASSERT_EQ(browser()->tab_strip_model()->count(), starting_tab_count + 2);

  // Make sure that there's nothing else to restore.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(service);
  EXPECT_TRUE(service->entries().empty());
}

// Open a window with multiple tabs, close a tab, then close the window.
// Restore both and make sure the tab goes back into the window.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreWindowAndTab) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  AddFileSchemeTabs(browser(), 3);

  // Close one in the middle
  int closed_tab_index = starting_tab_count + 1;
  CloseTab(closed_tab_index);
  EXPECT_EQ(starting_tab_count + 2, browser()->tab_strip_model()->count());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Close the first browser.
  const int active_tab_index = browser()->tab_strip_model()->active_index();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list_->size());

  // Restore the first window. The expected_tabstrip_index (second argument)
  // indicates the expected active tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, active_tab_index));
  Browser* browser = GetBrowser(1);
  EXPECT_EQ(starting_tab_count + 2, browser->tab_strip_model()->count());

  // Restore the closed tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, closed_tab_index));
  EXPECT_EQ(starting_tab_count + 3, browser->tab_strip_model()->count());
  EXPECT_EQ(url1_,
            browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Open a window with two tabs, close both (closing the window), then restore
// both. Make sure both restored tabs are in the same window.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreIntoSameWindow) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Navigate the rightmost one to url2_ for easier identification.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Close all but one tab in the first browser, left to right.
  while (browser()->tab_strip_model()->count() > 1)
    CloseTab(0);

  // Close the last tab, closing the browser.
  CloseTab(0);
  ui_test_utils::WaitForBrowserToClose();
  EXPECT_EQ(1u, active_browser_list_->size());

  // Restore the last-closed tab into a new window.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, 0));
  Browser* browser = GetBrowser(1);
  EXPECT_EQ(1, browser->tab_strip_model()->count());
  EXPECT_EQ(url2_,
            browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Restore the next-to-last-closed tab into the same window.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, 0));
  EXPECT_EQ(2, browser->tab_strip_model()->count());
  EXPECT_EQ(url1_,
            browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Open a window with two tabs, close the window, then restore the window.
// Ensure that the restored window has the expected bounds.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreWindowBounds) {
  // Create a browser window with two tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Create a second browser window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Deliberately change the bounds of the first window to something different.
  gfx::Rect bounds = browser()->window()->GetBounds();
  bounds.set_width(700);
  bounds.set_height(480);
  bounds.Offset(20, 20);
  browser()->window()->SetBounds(bounds);
  gfx::Rect bounds2 = browser()->window()->GetBounds();
  ASSERT_EQ(bounds, bounds2);

  // Close the first window.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list_->size());

  // Check that the TabRestoreService has the contents of the closed window and
  // the correct bounds.
  Browser* browser = GetBrowser(0);
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  const sessions::TabRestoreService::Entries& entries = service->entries();
  EXPECT_EQ(1u, entries.size());
  sessions::tab_restore::Entry* entry = entries.front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  sessions::tab_restore::Window* entry_win =
      static_cast<sessions::tab_restore::Window*>(entry);
  EXPECT_EQ(bounds, entry_win->bounds);
  auto& tabs = entry_win->tabs;
  EXPECT_EQ(2u, tabs.size());

  // Restore the window. Ensure that a second window is created, that is has 2
  // tabs, and that it has the expected bounds.
  service->RestoreMostRecentEntry(browser->live_tab_context());
  EXPECT_EQ(2u, active_browser_list_->size());
  browser = GetBrowser(1);
  EXPECT_EQ(2, browser->tab_strip_model()->count());
  // We expect the overridden bounds to the browser window to have been
  // specified at window creation. The actual bounds of the window itself may
  // change as the browser refuses to create windows that are offscreen, so will
  // adjust bounds slightly in some cases.
  EXPECT_EQ(bounds, browser->override_bounds());
}

// Close a group not at the end of the current window, then restore it. The
// group should be at the end of the tabstrip.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreGroup) {
  // Manually add tabs since TabGroupsSaveV2 filters out file urls since those
  // links can expose user data and or trigger automatic downloads.
  AddHTTPSSchemeTabs(browser(), 3);

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  CloseGroup(group);

  tab_groups::TabGroupId restored_group_id = RestoreGroup(group, 0, 2);
  const TabGroupModel* group_model =
      browser()->tab_strip_model()->group_model();
  EXPECT_EQ(group_model->GetTabGroup(restored_group_id)->ListTabs(),
            gfx::Range(2, 4));
}

// Verifies that restoring a grouped tab in a browser that does not support tab
// groups, does restore the tab but does not recreate the group.
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       RestoreGroupInBrowserThatDoesNotSupportGroups) {
  // Create a browser that does not support groups and try to restore a
  // grouped tab. This should restore the tab and not recreate the group.
  Browser::CreateParams app_browser_params =
      Browser::CreateParams::CreateForApp("App Name", true, gfx::Rect(),
                                          browser()->profile(), false);
  Browser* app_browser = Browser::Create(app_browser_params);
  EXPECT_FALSE(app_browser->tab_strip_model()->group_model());

  // Create a tab entry with a group and add it to TabRestoreService directly.
  auto service = std::make_unique<sessions::TabRestoreServiceImpl>(
      std::make_unique<ChromeTabRestoreServiceClient>(app_browser->profile()),
      app_browser->profile()->GetPrefs(), nullptr);

  tab_groups::TabGroupId group_id = tab_groups::TabGroupId::GenerateNew();
  std::unique_ptr<sessions::tab_restore::Tab> tab =
      std::make_unique<sessions::tab_restore::Tab>();
  tab->current_navigation_index = 0;
  tab->group = group_id;
  tab->navigations.push_back(
      sessions::ContentTestHelper::CreateNavigation("https://1.com", "1"));
  tab->group_visual_data = tab_groups::TabGroupVisualData(
      u"Group Title", tab_groups::TabGroupColorId::kBlue);

  service->mutable_entries()->push_front(std::move(tab));

  EXPECT_EQ(1u, service->entries().size());
  EXPECT_EQ(0, app_browser->tab_strip_model()->count());

  service->RestoreMostRecentEntry(app_browser->live_tab_context());

  EXPECT_EQ(0u, service->entries().size());
  EXPECT_EQ(1, app_browser->tab_strip_model()->count());
}

// Close a grouped tab, then the entire group. Restore both. The group should be
// opened at the end of the tabstrip.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreGroupedTabThenGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  // Manually add tabs since TabGroupsSaveV2 filters out file urls since those
  // links can expose user data and or trigger automatic downloads.
  AddHTTPSSchemeTabs(browser(), 3);

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2, 3});

  CloseTab(2);
  CloseGroup(group);
  tab_groups::TabGroupId restored_group_id = RestoreGroup(group, 0, 1);

  // TODO(crbug.com/347746086): When TabGroupsSaveV2 is enabled the tab will be
  // restored at the end of the group instead of the original index.
  const int expected_tabstrip_index =
      tab_groups::IsTabGroupsSaveV2Enabled() ? 3 : 2;
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, expected_tabstrip_index));

  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(restored_group_id)
                ->ListTabs(),
            gfx::Range(1, 4));
}

// Close a group that contains all tabs in a window, resulting in the window
// closing. Then restore the group. The window should be recreated with the
// group intact.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreGroupInNewWindow) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  // Navigate the first tab to something other than about:blank since that
  // cannot be saved in tab groups properly.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("https://www.1.com")));

  // Add all tabs in the starting browser to a group.
  browser()->tab_strip_model()->AddToNewGroup({0});

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ASSERT_EQ(2u, active_browser_list_->size());

  // Close the original group, which closes the original window. We spawn a new
  // tab if the group is the only element in the browser and is closing. This
  // prevents the browser from actually closing, so we close it manually
  // instead.
  CloseBrowserSynchronously(GetBrowser(0));
  EXPECT_EQ(1u, active_browser_list_->size());

  // Restore the original group, which should create a new window.
  std::optional<tab_groups::TabGroupId> restored_group_id = RestoreTab(1, 0);
  ASSERT_TRUE(restored_group_id.has_value());

  Browser* browser = GetBrowser(1);
  EXPECT_EQ(1, browser->tab_strip_model()->count());

  const TabGroupModel* group_model = browser->tab_strip_model()->group_model();
  EXPECT_EQ(group_model->GetTabGroup(restored_group_id.value())->ListTabs(),
            gfx::Range(0, 1));
}

// Close a group that contains a tab with an unload handler. Reject the
// unload handler, resulting in the tab not closing while the group does. Then
// restore the group. The group should restore intact and duplicate the
// still-open tab.
// TODO(crbug.com/40750891): Run unload handlers before the group is closed.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreGroupWithUnloadHandlerRejected) {
  class OnceGroupDeletionWaiter : public TabStripModelObserver {
   public:
    explicit OnceGroupDeletionWaiter(TabStripModel* tab_strip_model)
        : tab_strip_model_(tab_strip_model) {
      tab_strip_model_->AddObserver(this);
    }

    ~OnceGroupDeletionWaiter() override {
      tab_strip_model_->RemoveObserver(this);
    }

    void Wait() {
      if (!called_) {
        run_loop_.Run();
      }
    }
    void OnTabGroupChanged(const TabGroupChange& change) override {
      if (change.type == TabGroupChange::Type::kClosed) {
        called_ = true;
        run_loop_.Quit();
      }
    }

   private:
    bool called_ = false;
    base::RunLoop run_loop_;
    raw_ptr<TabStripModel> tab_strip_model_;
  };

  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const char kUnloadHTML[] =
      "<html><body>"
      "<script>window.onbeforeunload=function(e){return 'foo';}</script>"
      "</body></html>";
  GURL unload_url = GURL(std::string("data:text/html,") + kUnloadHTML);

  // Set up the tabstrip with:
  // 0: An ungrouped tab (already present).
  // 1: A grouped tab.
  // 2: A grouped tab with an unload handler.

  // When TabGroupsSaveV2 is enabled, we must manually add non file:// tabs
  // since we filter out urls which could expose user data on other devices when
  // we add them to the saved group. We also protect from triggering automatic
  // downloads this way.
  AddHTTPSSchemeTabs(browser(), 1);

  // Add the unload handler tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), unload_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  ASSERT_EQ(group_model->GetTabGroup(group)->ListTabs(), gfx::Range(1, 3));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  // Attempt to close the group. Group close is not going to be possible due
  // to the tab that has the unload handler, and will therefore leave that tab
  // in the tabstrip. The group will then be ungrouped. The tab that had the
  // unload handler will no longer have a group.
  content::PrepContentsForBeforeUnloadTest(
      browser()->tab_strip_model()->GetWebContentsAt(2));
  ASSERT_EQ(browser()->tab_strip_model()->group_model()->ListTabGroups().size(),
            1u);

  OnceGroupDeletionWaiter group_deletion_waiter(browser()->tab_strip_model());
  CloseGroup(group);

  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  dialog->view()->CancelAppModalDialog();

  // Group deletion should be called on the group.
  group_deletion_waiter.Wait();

  // The group should have ungrouped.
  EXPECT_FALSE(
      browser()->tab_strip_model()->group_model()->ContainsTabGroup(group));
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  EXPECT_EQ(browser()->tab_strip_model()->group_model()->ListTabGroups().size(),
            0u);

  // Restore the group, which will restore all tabs, including one that is now
  // a duplicate of the unclosed tab.
  tab_groups::TabGroupId restored_group_id = RestoreGroup(group, 0, 2);
  EXPECT_EQ(browser()->tab_strip_model()->group_model()->ListTabGroups().size(),
            1u);

  // The additional tab comes from the unload url since it is not a standard
  // https://www domain. For TabGroupsSaveV2 we do not store these to prevent
  // cross device attacks / information leaks which could disadvantage the user.
  EXPECT_EQ(group_model->GetTabGroup(restored_group_id)->ListTabs(),
            gfx::Range(2, tab_groups::IsTabGroupsSaveV2Enabled() ? 5 : 4));
  EXPECT_EQ(browser()->tab_strip_model()->count(),
            tab_groups::IsTabGroupsSaveV2Enabled() ? 5 : 4);

  // Close the tab with the unload handler, otherwise it will prevent test
  // cleanup.
  browser()->tab_strip_model()->CloseAllTabs();
  dialog = ui_test_utils::WaitForAppModalDialog();
  dialog->view()->AcceptAppModalDialog();
}

// Close a group that contains a tab with an unload handler. Accept the
// unload handler, resulting in the tab (and group) closing anyway. Then restore
// the group. The group should restore intact with no duplicates.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreGroupWithUnloadHandlerAccepted) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const char kUnloadHTML[] =
      "<html><body>"
      "<script>window.onbeforeunload=function(e){return 'foo';}</script>"
      "</body></html>";
  GURL unload_url = GURL(std::string("data:text/html,") + kUnloadHTML);

  // Set up the tabstrip with:
  // 0: An ungrouped tab (already present).
  // 1: A grouped tab.
  // 2: A grouped tab with an unload handler.
  AddFileSchemeTabs(browser(), 1);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), unload_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  const TabGroupModel* group_model =
      browser()->tab_strip_model()->group_model();
  ASSERT_EQ(group_model->GetTabGroup(group)->ListTabs(), gfx::Range(1, 3));
  ASSERT_EQ(browser()->tab_strip_model()->count(), 3);

  // Close the group, then accept the unload handler and wait for the tab to
  // close.
  content::WebContents* tab_with_unload_handler =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  content::PrepContentsForBeforeUnloadTest(tab_with_unload_handler);
  content::WebContentsDestroyedWatcher destroyed_watcher(
      tab_with_unload_handler);
  CloseGroup(group);
  javascript_dialogs::AppModalDialogController* dialog =
      ui_test_utils::WaitForAppModalDialog();
  dialog->view()->AcceptAppModalDialog();
  destroyed_watcher.Wait();

  // Restore the group, which should restore the original group intact.
  tab_groups::TabGroupId restored_group_id = RestoreGroup(group, 0, 1);

  // The 2 additional tabs come from the tabs that do not have a standard
  // https://www domain. For TabGroupsSaveV2 we do not store these to prevent
  // cross device attacks / information leaks which could disadvantage the user.
  EXPECT_EQ(group_model->GetTabGroup(restored_group_id)->ListTabs(),
            gfx::Range(1, tab_groups::IsTabGroupsSaveV2Enabled() ? 5 : 3));
}

// Open a window with two tabs, close both (closing the window), then restore
// one by ID. Guards against regression of crbug.com/622752.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreTabFromClosedWindowByID) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Close the window.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list_->size());

  // Check that the TabRestoreService has the contents of the closed window.
  Browser* browser = GetBrowser(0);
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser->profile());
  const sessions::TabRestoreService::Entries& entries = service->entries();
  EXPECT_EQ(1u, entries.size());
  sessions::tab_restore::Entry* entry = entries.front().get();
  ASSERT_EQ(sessions::tab_restore::Type::WINDOW, entry->type);
  sessions::tab_restore::Window* entry_win =
      static_cast<sessions::tab_restore::Window*>(entry);
  auto& tabs = entry_win->tabs;
  EXPECT_EQ(3u, tabs.size());
  EXPECT_EQ(url::kAboutBlankURL, tabs[0]->navigations.front().virtual_url());
  EXPECT_EQ(url1_, tabs[1]->navigations.front().virtual_url());
  EXPECT_EQ(url2_, tabs[2]->navigations.front().virtual_url());
  EXPECT_EQ(2, entry_win->selected_tab_index);

  // Find the Tab to restore.
  SessionID tab_id_to_restore = SessionID::InvalidValue();
  bool found_tab_to_restore = false;
  for (const auto& tab_ptr : tabs) {
    auto& tab = *tab_ptr;
    if (tab.navigations[tab.current_navigation_index].virtual_url() == url1_) {
      tab_id_to_restore = tab.id;
      found_tab_to_restore = true;
      break;
    }
  }
  ASSERT_TRUE(found_tab_to_restore);

  // Restore the tab into the current window.
  EXPECT_EQ(1, browser->tab_strip_model()->count());
  ui_test_utils::TabAddedWaiter tab_added_waiter(browser);
  service->RestoreEntryById(browser->live_tab_context(), tab_id_to_restore,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB);
  auto* new_tab = tab_added_waiter.Wait();
  content::WaitForLoadStop(new_tab);

  // Check that the tab was correctly restored.
  EXPECT_EQ(2, browser->tab_strip_model()->count());
  EXPECT_EQ(url1_,
            browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Check that the window entry was adjusted.
  EXPECT_EQ(2u, tabs.size());
  EXPECT_EQ(url::kAboutBlankURL, tabs[0]->navigations.front().virtual_url());
  EXPECT_EQ(url2_, tabs[1]->navigations.front().virtual_url());
  EXPECT_EQ(1, entry_win->selected_tab_index);
}

// Tests that a duplicate history entry is not created when we restore a page
// to an existing SiteInstance.  (Bug 1230446)
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreWithExistingSiteInstance) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL http_url1(embedded_test_server()->GetURL("/title1.html"));
  GURL http_url2(embedded_test_server()->GetURL("/title2.html"));
  int tab_count = browser()->tab_strip_model()->count();

  // Add a tab
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), http_url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(++tab_count, browser()->tab_strip_model()->count());

  // Navigate to another same-site URL.
  content::WebContents* tab =
      browser()->tab_strip_model()->GetWebContentsAt(tab_count - 1);
  content::LoadStopObserver observer(tab);
  static_cast<content::WebContentsDelegate*>(browser())->OpenURLFromTab(
      tab,
      content::OpenURLParams(http_url2, content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  observer.Wait();

  // Close the tab.
  CloseTab(1);

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance used by the restored tab
  // will already exist when the restore happens.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), http_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restore the closed tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, tab_count - 1));

  // And make sure the URLs match.
  EXPECT_EQ(http_url2,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(browser());
  EXPECT_EQ(http_url1,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// See crbug.com/248574
#if BUILDFLAG(IS_WIN)
#define MAYBE_RestoreCrossSiteWithExistingSiteInstance \
  DISABLED_RestoreCrossSiteWithExistingSiteInstance
#else
#define MAYBE_RestoreCrossSiteWithExistingSiteInstance \
  RestoreCrossSiteWithExistingSiteInstance
#endif

// Tests that the SiteInstances used for entries in a restored tab's history
// are given appropriate max page IDs, even if the renderer for the entry
// already exists.  (Bug 1204135)
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       MAYBE_RestoreCrossSiteWithExistingSiteInstance) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL http_url1(embedded_test_server()->GetURL("/title1.html"));
  GURL http_url2(embedded_test_server()->GetURL("/title2.html"));

  int tab_count = browser()->tab_strip_model()->count();

  // Add a tab
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), http_url1, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_EQ(++tab_count, browser()->tab_strip_model()->count());

  // Navigate to more URLs, then a cross-site URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url2));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url1));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1_));

  // Close the tab.
  CloseTab(1);

  // Create a new tab to the original site.  Assuming process-per-site is
  // enabled, this will ensure that the SiteInstance will already exist when
  // the user clicks Back in the restored tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), http_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restore the closed tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, tab_count - 1));

  // And make sure the URLs match.
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(browser());
  EXPECT_EQ(http_url1,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Navigating to a new URL should clear the forward list, because the max
  // page ID of the renderer should have been updated when we restored the tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url2));
  EXPECT_FALSE(chrome::CanGoForward(browser()));
  EXPECT_EQ(http_url2,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreWindow) {
  // Create a new window.
  size_t window_count = active_browser_list_->size();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(++window_count, active_browser_list_->size());

  // Create two more tabs, one with url1, the other url2.
  int initial_tab_count = browser()->tab_strip_model()->count();
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close the window.
  const int active_tab_index = browser()->tab_strip_model()->active_index();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(window_count - 1, active_browser_list_->size());

  // Restore the window.
  ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;
  chrome::RestoreTab(active_browser_list_->get(0));
  EXPECT_EQ(window_count, active_browser_list_->size());

  Browser* browser = GetBrowser(1);
  EXPECT_EQ(initial_tab_count + 2, browser->tab_strip_model()->count());
  EXPECT_TRUE(content::WaitForLoadStop(tab_added_waiter.Wait()));

  EXPECT_EQ(active_tab_index, browser->tab_strip_model()->active_index());
  content::WebContents* restored_tab =
      browser->tab_strip_model()->GetWebContentsAt(initial_tab_count + 1);
  EnsureTabFinishedRestoring(restored_tab);
  EXPECT_EQ(url2_, restored_tab->GetURL());

  restored_tab =
      browser->tab_strip_model()->GetWebContentsAt(initial_tab_count);
  EnsureTabFinishedRestoring(restored_tab);
  EXPECT_EQ(url1_, restored_tab->GetURL());
}

// Verifies that active tab index is the same as before closing.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreWindow_ActiveTabIndex) {
  AddFileSchemeTabs(browser(), 4);

  // Create a second browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  constexpr int kActiveTabIndex = 2;
  browser()->tab_strip_model()->ActivateTabAt(kActiveTabIndex);

  // Close the first browser.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list_->size());

  // Restore the first browser. Verify the active tab index.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, kActiveTabIndex));
}

// https://crbug.com/825305: Timeout flakiness on Mac10.13 Tests (dbg) and
// PASS/FAIL flakiness on Linux Chromium OS ASan LSan Tests (1) bot.
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    (!defined(NDEBUG) && !BUILDFLAG(IS_WIN))
#define MAYBE_RestoreTabWithSpecialURL DISABLED_RestoreTabWithSpecialURL
#else
#define MAYBE_RestoreTabWithSpecialURL RestoreTabWithSpecialURL
#endif

// Restore tab with special URL chrome://credits/ and make sure the page loads
// properly after restore. See http://crbug.com/31905.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, MAYBE_RestoreTabWithSpecialURL) {
  // Navigate new tab to a special URL.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUICreditsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close the tab.
  CloseTab(1);

  // Restore the closed tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, 1));
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  EnsureTabFinishedRestoring(tab);

  // See if content is as expected.
  EXPECT_GT(
      ui_test_utils::FindInPage(tab, u"webkit", true, false, nullptr, nullptr),
      0);
}

// https://crbug.com/667932: Flakiness on linux_chromium_asan_rel_ng bot.
// https://crbug.com/825305: Timeout flakiness on Mac10.13 Tests (dbg) bots.
// Also fails on Linux Tests (dbg).
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER) || \
    (!defined(NDEBUG) && !BUILDFLAG(IS_WIN))
#define MAYBE_RestoreTabWithSpecialURLOnBack DISABLED_RestoreTabWithSpecialURLOnBack
#else
#define MAYBE_RestoreTabWithSpecialURLOnBack RestoreTabWithSpecialURLOnBack
#endif

// Restore tab with special URL in its navigation history, go back to that
// entry and see that it loads properly. See http://crbug.com/31905
IN_PROC_BROWSER_TEST_F(TabRestoreTest, MAYBE_RestoreTabWithSpecialURLOnBack) {
  ASSERT_TRUE(embedded_test_server()->Start());

  const GURL http_url(embedded_test_server()->GetURL("/title1.html"));

  // Navigate new tab to a special URL.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUICreditsURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Then navigate to a normal URL.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), http_url));

  // Close the tab.
  CloseTab(1);

  // Restore the closed tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, 1));
  content::WebContents* tab = browser()->tab_strip_model()->GetWebContentsAt(1);
  EnsureTabFinishedRestoring(tab);
  ASSERT_EQ(http_url, tab->GetURL());

  // Go back, and see if content is as expected.
  GoBack(browser());
  EXPECT_GT(
      ui_test_utils::FindInPage(tab, u"webkit", true, false, nullptr, nullptr),
      0);
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, PRE_RestoreOnStartup) {
  // This results in a new tab at the end with url1.
  AddFileSchemeTabs(browser(), 1);

  while (browser()->tab_strip_model()->count())
    CloseTab(0);
}

// Verifies restoring a tab works on startup.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreOnStartup) {
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, 1));
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

// Regression test to ensure the tab_restore::Window object populates the
// tab_groups mapping appropriately when loading the last session after a
// browser restart. See crbug.com/338555375.
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       PRE_WindowMappingHasGroupDataAfterRestart) {
  // Enable session service in default mode.
  EnableSessionService();

  // Navigate to url1 in the current tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Add a second tab so the window entry will be logged instead of a single tab
  // when the browser closes.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Add the tab to a group.
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({0});
  browser()
      ->tab_strip_model()
      ->group_model()
      ->GetTabGroup(group)
      ->SetVisualData(tab_groups::TabGroupVisualData(
          u"Group title", tab_groups::TabGroupColorId::kGreen,
          /*is_collapsed=*/false));
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, WindowMappingHasGroupDataAfterRestart) {
  // Enable session service in default mode.
  EnableSessionService();

  sessions::TabRestoreService* tab_restore_service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  CHECK(tab_restore_service);

  ASSERT_EQ(1u, tab_restore_service->entries().size());
  sessions::tab_restore::Entry* entry =
      tab_restore_service->entries().front().get();
  ASSERT_EQ(sessions::tab_restore::WINDOW, entry->type);

  auto* window = static_cast<sessions::tab_restore::Window*>(entry);
  ASSERT_EQ(2u, window->tabs.size());
  ASSERT_EQ(1u, window->tab_groups.size());

  const sessions::tab_restore::Group* first_group =
      window->tab_groups.begin()->second.get();
  tab_groups::TabGroupVisualData expected_visual_data(
      u"Group title", tab_groups::TabGroupColorId::kGreen,
      /*is_collapsed=*/false);
  EXPECT_EQ(expected_visual_data, first_group->visual_data);
}

// Check that TabRestoreService and SessionService do not try to restore the
// same thing.
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       RestoreFirstBrowserWhenSessionServiceEnabled) {
  // Do not exit from test or delete the Profile* when last browser is closed.
  ScopedKeepAlive keep_alive(KeepAliveOrigin::SESSION_RESTORE,
                             KeepAliveRestartOption::DISABLED);
  ScopedProfileKeepAlive profile_keep_alive(
      browser()->profile(), ProfileKeepAliveOrigin::kSessionRestore);

  // Enable session service.
  SessionStartupPref pref(SessionStartupPref::LAST);
  Profile* profile = browser()->profile();
  SessionStartupPref::SetStartupPref(profile, pref);

  // Add tabs and close browser.
  AddFileSchemeTabs(browser(), 3);
  // 1st tab is about:blank added by InProcessBrowserTest.
  EXPECT_EQ(4, browser()->tab_strip_model()->count());
  CloseBrowserSynchronously(browser());

  SessionRestoreTestHelper helper;
  // Restore browser (this is what Cmd-Shift-T does on Mac).
  chrome::OpenWindowWithRestoredTabs(profile);
  if (SessionRestore::IsRestoring(profile))
    helper.Wait();
  Browser* browser = GetBrowser(0);
  EXPECT_EQ(4, browser->tab_strip_model()->count());
}

// Test is flaky on Win and Mac. crbug.com/1241761, crbug.com/330838232.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#define MAYBE_TabsFromRestoredWindowsAreLoadedGradually \
  DISABLED_TabsFromRestoredWindowsAreLoadedGradually
#else
#define MAYBE_TabsFromRestoredWindowsAreLoadedGradually \
  TabsFromRestoredWindowsAreLoadedGradually
#endif
// TabLoader (used here) is available only when browser is built
// with ENABLE_SESSION_SERVICE.
#if BUILDFLAG(ENABLE_SESSION_SERVICE)
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       MAYBE_TabsFromRestoredWindowsAreLoadedGradually) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  Browser* browser2 = GetBrowser(1);

  // Add tabs and close browser.
  const int tabs_count = 4;
  AddFileSchemeTabs(browser2,
                    tabs_count - browser2->tab_strip_model()->count());
  EXPECT_EQ(tabs_count, browser2->tab_strip_model()->count());
  const int active_tab_index = browser2->tab_strip_model()->active_index();
  CloseBrowserSynchronously(browser2);

  // Limit the number of restored tabs that are loaded.
  TabLoaderTester::SetMaxLoadedTabCountForTesting(2);

  // When the tab loader is created configure it for this test. This ensures
  // that no more than 1 loading slot is used for the test.
  base::RepeatingCallback<void(TabLoader*)> callback =
      base::BindRepeating(&TabRestoreTest::SetMaxSimultaneousLoadsForTesting,
                          base::Unretained(this));
  TabLoaderTester::SetConstructionCallbackForTesting(&callback);

  // Restore recently closed window.
  chrome::OpenWindowWithRestoredTabs(browser()->profile());
  ASSERT_EQ(2U, active_browser_list_->size());
  browser2 = GetBrowser(1);

  EXPECT_EQ(tabs_count, browser2->tab_strip_model()->count());
  EXPECT_EQ(active_tab_index, browser2->tab_strip_model()->active_index());

  // These two tabs should be loaded by TabLoader.
  EnsureTabFinishedRestoring(browser2->tab_strip_model()->GetWebContentsAt(0));
  EnsureTabFinishedRestoring(
      browser2->tab_strip_model()->GetWebContentsAt(active_tab_index));

  // The following isn't necessary but just to be sure there is no any async
  // task that could have an impact on the expectations below.
  content::RunAllPendingInMessageLoop();

  // These tabs shouldn't want to be loaded.
  for (int tab_idx = 1; tab_idx < tabs_count; ++tab_idx) {
    if (tab_idx == active_tab_index) {
      continue;  // Active tab should be loaded.
    }
    auto* contents = browser2->tab_strip_model()->GetWebContentsAt(tab_idx);
    EXPECT_FALSE(contents->IsLoading());
    EXPECT_TRUE(contents->GetController().NeedsReload());
  }

  // Clean up the callback.
  TabLoaderTester::SetConstructionCallbackForTesting(nullptr);
}
#endif  // BUILDFLAG(ENABLE_SESSION_SERVICE)

IN_PROC_BROWSER_TEST_F(TabRestoreTest, PRE_GetRestoreTabType) {
  // The TabRestoreService should get initialized (Loaded)
  // automatically upon launch.
  // Wait for robustness because InProcessBrowserTest::PreRunTestOnMainThread
  // does not flush the task scheduler.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(service);
  TabRestoreServiceLoadWaiter waiter(service);
  waiter.Wait();

  // When we start, we should get nothing.
  EXPECT_TRUE(service->entries().empty());

  // Add a tab and close it
  AddFileSchemeTabs(browser(), 1);
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  content::WebContents* tab_to_close =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::WebContentsDestroyedWatcher destroyed_watcher(tab_to_close);
  browser()->tab_strip_model()->CloseSelectedTabs();
  destroyed_watcher.Wait();

  // We now should see a Tab as the restore type.
  ASSERT_EQ(1u, service->entries().size());
  EXPECT_EQ(sessions::tab_restore::Type::TAB, service->entries().front()->type);
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, GetRestoreTabType) {
  // The TabRestoreService should get initialized (Loaded)
  // automatically upon launch.
  // Wait for robustness because InProcessBrowserTest::PreRunTestOnMainThread
  // does not flush the task scheduler.
  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(service);
  TabRestoreServiceLoadWaiter waiter(service);
  waiter.Wait();

  // When we start this time we should get a Tab.
  ASSERT_GE(service->entries().size(), 1u);
  EXPECT_EQ(sessions::tab_restore::Type::TAB, service->entries().front()->type);
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreWindowWithName) {
  AddFileSchemeTabs(browser(), 1);
  browser()->SetWindowUserTitle("foobar");

  // Create a second browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Close the first browser.
  const int active_tab_index = browser()->tab_strip_model()->active_index();
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list_->size());

  // Restore the first browser.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, active_tab_index));
  Browser* browser = GetBrowser(1);
  EXPECT_EQ("foobar", browser->user_title());
}

// Closing the last tab in a group then restoring will place the group back with
// its metadata.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreSingleGroupedTab) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const int tab_count = AddFileSchemeTabs(browser(), 1);
  ASSERT_LE(2, tab_count);

  const int grouped_tab_index = tab_count - 1;
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({grouped_tab_index});
  const tab_groups::TabGroupVisualData visual_data(
      u"Foo", tab_groups::TabGroupColorId::kCyan);

  TabGroup* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  group->SetVisualData(visual_data);
  CloseTab(grouped_tab_index);

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, grouped_tab_index));
  ASSERT_EQ(tab_count, browser()->tab_strip_model()->count());

  group = browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  EXPECT_EQ(group_id, browser()
                          ->tab_strip_model()
                          ->GetTabGroupForTab(grouped_tab_index)
                          .value());
  const tab_groups::TabGroupVisualData* data = group->visual_data();
  EXPECT_EQ(data->title(), visual_data.title());
  EXPECT_EQ(data->color(), visual_data.color());
}

// Closing the last tab in a collapsed group then restoring will place the group
// back collapsed with its metadata.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreCollapsedGroupTab) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const int tab_count = AddFileSchemeTabs(browser(), 1);
  ASSERT_LE(2, tab_count);

  const int grouped_tab_index = tab_count - 1;
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({grouped_tab_index});
  const tab_groups::TabGroupVisualData visual_data(
      u"Foo", tab_groups::TabGroupColorId::kCyan, true);

  TabGroup* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  ASSERT_TRUE(group);
  group->SetVisualData(visual_data);

  CloseTab(grouped_tab_index);

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, grouped_tab_index));
  ASSERT_EQ(tab_count, browser()->tab_strip_model()->count());

  EXPECT_EQ(group_id, browser()
                          ->tab_strip_model()
                          ->GetTabGroupForTab(grouped_tab_index)
                          .value());
  const tab_groups::TabGroupVisualData* data = browser()
                                                   ->tab_strip_model()
                                                   ->group_model()
                                                   ->GetTabGroup(group_id)
                                                   ->visual_data();
  ASSERT_TRUE(data);
  EXPECT_EQ(data->title(), visual_data.title());
  EXPECT_EQ(data->color(), visual_data.color());
  EXPECT_TRUE(data->is_collapsed());
}

// Closing a tab in a collapsed group then restoring the tab will not expand the
// group upon restore.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreTabIntoCollapsedGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const int tab_count = AddFileSchemeTabs(browser(), 2);
  ASSERT_LE(3, tab_count);

  const int closed_tab_index = 1;

  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  const tab_groups::TabGroupVisualData visual_data(
      u"Foo", tab_groups::TabGroupColorId::kCyan, true);
  TabGroup* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  group->SetVisualData(visual_data);

  CloseTab(closed_tab_index);

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, closed_tab_index));
  ASSERT_EQ(tab_count, browser()->tab_strip_model()->count());

  EXPECT_EQ(group_id, browser()
                          ->tab_strip_model()
                          ->GetTabGroupForTab(closed_tab_index)
                          .value());
  const tab_groups::TabGroupVisualData* data = browser()
                                                   ->tab_strip_model()
                                                   ->group_model()
                                                   ->GetTabGroup(group_id)
                                                   ->visual_data();
  EXPECT_EQ(data->title(), visual_data.title());
  EXPECT_EQ(data->color(), visual_data.color());

  // TODO(crbug.com/347746086): When TabGroupsSaveV2 is enabled we always expand
  // the group when restoring. Clean this up post launch.
  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    EXPECT_FALSE(data->is_collapsed());
  } else {
    EXPECT_TRUE(data->is_collapsed());
  }
}

// Closing a tab in a group then updating the metadata before restoring will
// place the group back and update the metadata.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreTabIntoGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const int tab_count = AddFileSchemeTabs(browser(), 2);
  ASSERT_LE(3, tab_count);

  const int closed_tab_index = 1;

  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});
  const tab_groups::TabGroupVisualData visual_data_1(
      u"Foo1", tab_groups::TabGroupColorId::kCyan);
  const tab_groups::TabGroupVisualData visual_data_2(
      u"Foo2", tab_groups::TabGroupColorId::kCyan);
  TabGroup* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);

  group->SetVisualData(visual_data_1);
  CloseTab(closed_tab_index);
  group->SetVisualData(visual_data_2);

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, closed_tab_index));
  ASSERT_EQ(tab_count, browser()->tab_strip_model()->count());

  EXPECT_EQ(group_id, browser()
                          ->tab_strip_model()
                          ->GetTabGroupForTab(closed_tab_index)
                          .value());
  const tab_groups::TabGroupVisualData* data = group->visual_data();

  // TODO(crbug.com/347746086): When TabGroupsSaveV2 is enabled we no longer
  // proactively update the visual data. Clean this up post launch.
  const tab_groups::TabGroupVisualData actual_visual_data =
      tab_groups::IsTabGroupsSaveV2Enabled() ? visual_data_2 : visual_data_1;

  EXPECT_EQ(data->title(), actual_visual_data.title());
  EXPECT_EQ(data->color(), actual_visual_data.color());
}

// Closing a tab in a group then moving the group to a new window before
// restoring will place the tab in the group in the new window.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreTabIntoGroupInNewWindow) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const int tab_count = AddFileSchemeTabs(browser(), 3);
  ASSERT_LE(4, tab_count);

  const int closed_tab_index = 1;

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({0, 1});

  CloseTab(closed_tab_index);
  chrome::MoveTabsToNewWindow(browser(), {0}, group);

  // Expect the tab to be restored to the new window, inside the group.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(1, closed_tab_index));
  Browser* new_browser = active_browser_list_->get(1);
  ASSERT_EQ(2u, new_browser->tab_strip_model()
                    ->group_model()
                    ->GetTabGroup(group)
                    ->ListTabs()
                    .length());
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreWindowWithGroupedTabs) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ASSERT_EQ(2u, active_browser_list_->size());

  // Manually add tabs since TabGroupsSaveV2 filters out file urls since those
  // links can expose user data and or trigger automatic downloads.
  AddHTTPSSchemeTabs(browser(), 3);
  constexpr int tab_count = 4;

  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  tab_groups::TabGroupId group1 = browser()->tab_strip_model()->AddToNewGroup(
      {tab_count - 3, tab_count - 2});
  tab_groups::TabGroupVisualData group1_data(u"Foo",
                                             tab_groups::TabGroupColorId::kRed);
  group_model->GetTabGroup(group1)->SetVisualData(group1_data);

  tab_groups::TabGroupId group2 =
      browser()->tab_strip_model()->AddToNewGroup({tab_count - 1});
  tab_groups::TabGroupVisualData group2_data(
      u"Bar", tab_groups::TabGroupColorId::kBlue);
  group_model->GetTabGroup(group2)->SetVisualData(group2_data);

  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, active_browser_list_->size());

  chrome::RestoreTab(GetBrowser(0));
  ASSERT_EQ(2u, active_browser_list_->size());

  Browser* restored_window = GetBrowser(1);
  TabGroupModel* restored_group_model =
      restored_window->tab_strip_model()->group_model();
  ASSERT_EQ(tab_count, restored_window->tab_strip_model()->count());
  auto restored_group1 =
      restored_window->tab_strip_model()->GetTabGroupForTab(tab_count - 3);
  ASSERT_TRUE(restored_group1);
  EXPECT_EQ(
      restored_window->tab_strip_model()->GetTabGroupForTab(tab_count - 3),
      restored_window->tab_strip_model()->GetTabGroupForTab(tab_count - 2));
  auto restored_group2 =
      restored_window->tab_strip_model()->GetTabGroupForTab(tab_count - 1);
  ASSERT_TRUE(restored_group2);
  EXPECT_NE(restored_group2, restored_group1);

  EXPECT_EQ(
      group1_data,
      *restored_group_model->GetTabGroup(*restored_group1)->visual_data());
  EXPECT_EQ(
      group2_data,
      *restored_group_model->GetTabGroup(*restored_group2)->visual_data());
}

// Ensure a tab is not restored between tabs of another group.
// Regression test for https://crbug.com/1109368.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, DoesNotRestoreIntoOtherGroup) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  TabStripModel* const tabstrip = browser()->tab_strip_model();

  tabstrip->AddToNewGroup({0});
  const tab_groups::TabGroupId group1 = tabstrip->GetTabGroupForTab(0).value();

  AddFileSchemeTabs(browser(), 1);
  tabstrip->AddToNewGroup({1});
  const tab_groups::TabGroupId group2 = tabstrip->GetTabGroupForTab(1).value();

  CloseTab(1);

  ASSERT_EQ(1, tabstrip->count());
  EXPECT_EQ(group1, tabstrip->GetTabGroupForTab(0));

  AddFileSchemeTabs(browser(), 1);
  tabstrip->AddToExistingGroup({1}, group1);

  // The restored tab of |group2| should be placed to the right of |group1|.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, 2));
  EXPECT_EQ(group1, tabstrip->GetTabGroupForTab(0));
  EXPECT_EQ(group1, tabstrip->GetTabGroupForTab(1));
  EXPECT_EQ(group2, tabstrip->GetTabGroupForTab(2));
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, DoesNotRestoreReaderModePages) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  int tab_count = AddFileSchemeTabs(browser(), 1);
  int interesting_tab = tab_count - 1;
  ASSERT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Navigate the tab to a reader mode page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome-distiller://any")));
  EXPECT_EQ(GURL("chrome-distiller://any"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Close it. Restoring restores the tab which came before.
  CloseTab(interesting_tab);
  EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, interesting_tab));

  // And make sure everything looks right.
  EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(interesting_tab, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       DoesNotRestoreReaderModePageBehindInHistory) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  int tab_count = AddFileSchemeTabs(browser(), 1);
  int interesting_tab = tab_count - 1;
  ASSERT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Navigate to some random page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example1.com")));

  // Navigate the tab to a reader mode page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome-distiller://any")));

  // Navigate to another random page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example2.com")));

  // Close it. Restoring restores example2 site.
  CloseTab(interesting_tab);
  EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, interesting_tab));

  // And make sure everything looks right.
  EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(interesting_tab, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GURL("https://www.example2.com"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Going back should bring us to example1, Reader Mode has been omitted.
  ASSERT_TRUE(chrome::CanGoBack(browser()));
  GoBack(browser());
  EXPECT_EQ(GURL("https://www.example1.com"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  GoBack(browser());
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       DoesNotRestoreReaderModePageAheadInHistory) {
  int starting_tab_count = browser()->tab_strip_model()->count();
  int tab_count = AddFileSchemeTabs(browser(), 1);
  int interesting_tab = tab_count - 1;
  ASSERT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Navigate to some random page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example1.com")));

  // Navigate the tab to a reader mode page.
  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome-distiller://any")));

  // Navigate to another random page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("https://www.example2.com")));

  // Go back to the example1.
  GoBack(browser());
  GoBack(browser());
  EXPECT_EQ(GURL("https://www.example1.com"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Close it. Restoring restores example1 site.
  CloseTab(interesting_tab);
  EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, interesting_tab));

  // And make sure everything looks right.
  EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(interesting_tab, browser()->tab_strip_model()->active_index());
  EXPECT_EQ(GURL("https://www.example1.com"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Going forward should bring us to example2, Reader Mode has been omitted.
  ASSERT_TRUE(chrome::CanGoForward(browser()));
  GoForward(browser());
  EXPECT_EQ(GURL("https://www.example2.com"),
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  GoBack(browser());
  GoBack(browser());
  EXPECT_EQ(url1_,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Check that TabRestore.Tab.TimeBetweenClosedAndRestored histogram is recorded
// on tab restore.
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       TimeBetweenTabClosedAndRestoredRecorded) {
  base::HistogramTester histogram_tester;
  const char kTimeBetweenTabClosedAndRestored[] =
      "TabRestore.Tab.TimeBetweenClosedAndRestored";

  int starting_tab_count = browser()->tab_strip_model()->count();
  AddFileSchemeTabs(browser(), 3);

  // Close the tab in the middle.
  int closed_tab_index = starting_tab_count + 1;
  CloseTab(closed_tab_index);

  EXPECT_EQ(
      histogram_tester.GetAllSamples(kTimeBetweenTabClosedAndRestored).size(),
      0U);

  // Restore the tab. This should record the kTimeBetweenTabClosedAndRestored
  // histogram.
  RestoreTab(0, closed_tab_index);
  EXPECT_EQ(
      histogram_tester.GetAllSamples(kTimeBetweenTabClosedAndRestored).size(),
      1U);
}

// Check that TabRestore.Window.TimeBetweenClosedAndRestored histogram is
// recorded on window restore.
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       TimeBetweenWindowClosedAndRestoredRecorded) {
  base::HistogramTester histogram_tester;
  const char kTimeBetweenWindowClosedAndRestored[] =
      "TabRestore.Window.TimeBetweenClosedAndRestored";

  // Create a new window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  // Create two more tabs, one with url1, the other url2.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Close the window.
  CloseBrowserSynchronously(browser());

  EXPECT_EQ(histogram_tester.GetAllSamples(kTimeBetweenWindowClosedAndRestored)
                .size(),
            0U);

  // Restore the window. This should record kTimeBetweenWindowClosedAndRestored
  // histogram.
  ui_test_utils::AllBrowserTabAddedWaiter tab_added_waiter;
  chrome::RestoreTab(active_browser_list_->get(0));
  EXPECT_TRUE(content::WaitForLoadStop(tab_added_waiter.Wait()));

  EXPECT_EQ(histogram_tester.GetAllSamples(kTimeBetweenWindowClosedAndRestored)
                .size(),
            1U);
}

// // Check that TabRestore.Group.TimeBetweenClosedAndRestored histogram is
// recorded on group restore.
IN_PROC_BROWSER_TEST_F(TabRestoreTest,
                       TimeBetweenGroupClosedAndRestoredRecorded) {
  base::HistogramTester histogram_tester;
  const char kTimeBetweenGroupClosedAndRestored[] =
      "TabRestore.Group.TimeBetweenClosedAndRestored";

  AddFileSchemeTabs(browser(), 3);
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  CloseGroup(group);

  // Restore closed group. This should record kTimeBetweenGroupClosedAndRestored
  // histogram.
  ASSERT_NO_FATAL_FAILURE(RestoreGroup(group, 0, 2));

  EXPECT_EQ(
      histogram_tester.GetAllSamples(kTimeBetweenGroupClosedAndRestored).size(),
      1U);
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, PRE_PRE_RestoreAfterMultipleRestarts) {
  // Enable session service in default mode.
  EnableSessionService();

  // Navigate to url1 in the current tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

IN_PROC_BROWSER_TEST_F(TabRestoreTest, PRE_RestoreAfterMultipleRestarts) {
  // Enable session service in default mode.
  EnableSessionService();

  // Navigate to url2 in the current tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url2_, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

// Verifies restoring tabs from previous sessions.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreAfterMultipleRestarts) {
  // Enable session service in default mode.
  EnableSessionService();

  // Restore url2 from one session ago.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, 1));
  EXPECT_EQ(url2_, browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());

  // Restore url1 from two sessions ago.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, 2));
  EXPECT_EQ(url1_, browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());
}

// Test that it is possible to navigate back to a restored about:blank history
// entry with a non-null initiator origin.  This test cases covers
// https://crbug.com/1116320 - a scenario where (before
// https://crrev.com/c/2551302) the restore type was different from LAST_SESSION
// (e.g. the test below used CURRENT_SESSION).
//
// See also MultiOriginSessionRestoreTest.BackToAboutBlank1
IN_PROC_BROWSER_TEST_F(TabRestoreTest, BackToAboutBlank) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL initial_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  url::Origin initial_origin = url::Origin::Create(initial_url);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));

  content::WebContents* old_popup = nullptr;
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();
  {
    // Open a new popup.
    EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ExecJs(tab1, "var w = window.open('/title2.html');"));
    old_popup = popup_observer.GetWebContents();
    EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
    EXPECT_EQ(initial_origin,
              old_popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
    EXPECT_TRUE(WaitForLoadStop(old_popup));
  }

  {
    // Navigate the popup to about:blank, inheriting the opener origin. Note
    // that we didn't immediately open the popup to about:blank to avoid making
    // it use the initial NavigationEntry, which can't be navigated back to.
    content::TestNavigationObserver nav_observer(old_popup);
    ASSERT_TRUE(ExecJs(tab1, "w.location.href = 'about:blank';"));
    nav_observer.Wait();
    EXPECT_EQ(GURL(url::kAboutBlankURL),
              old_popup->GetPrimaryMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(initial_origin,
              old_popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  }

  // Navigate the popup to another site.
  GURL other_url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  url::Origin other_origin = url::Origin::Create(other_url);
  {
    content::TestNavigationObserver nav_observer(old_popup);
    ASSERT_TRUE(content::ExecJs(
        old_popup, content::JsReplace("location = $1", other_url)));
    nav_observer.Wait();
  }
  EXPECT_EQ(other_url, old_popup->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(other_origin,
            old_popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  ASSERT_TRUE(old_popup->GetController().CanGoBack());

  // Close the popup.
  int closed_tab_index = browser()->tab_strip_model()->active_index();
  EXPECT_EQ(1, closed_tab_index);
  CloseTab(closed_tab_index);
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Reopen the popup.
  content::WebContents* new_popup = nullptr;
  {
    content::WebContentsAddedObserver restored_tab_observer;
    RestoreTab(0, closed_tab_index);
    EXPECT_EQ(2, browser()->tab_strip_model()->count());
    new_popup = restored_tab_observer.GetWebContents();
  }
  EXPECT_EQ(other_url, new_popup->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(other_origin,
            new_popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  ASSERT_TRUE(new_popup->GetController().CanGoBack());
  int reopened_tab_index = browser()->tab_strip_model()->active_index();
  EXPECT_EQ(1, reopened_tab_index);

  // Navigate the popup back to about:blank.
  GoBack(browser());
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            new_popup->GetPrimaryMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(initial_origin,
            new_popup->GetPrimaryMainFrame()->GetLastCommittedOrigin());
}

// Ensures group IDs are regenerated for restored windows so that we don't split
// the same group between multiple windows. See https://crbug.com/1202102. This
// test is temporary until a more comprehensive fix is implemented.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoredWindowHasNewGroupIds) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());

  // When TabGroupsSaveV2 is enabled, we must manually add non file:// tabs
  // since we filter out urls which could expose user data on other devices when
  // we add them to the saved group. We also protect from triggering automatic
  // downloads this way.
  AddHTTPSSchemeTabs(browser(), 2);

  ASSERT_EQ(3, browser()->tab_strip_model()->count());

  // Create a new browser from which to restore the first.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ASSERT_EQ(2u, active_browser_list_->size());
  Browser* second_browser = GetBrowser(1);
  ASSERT_NE(browser(), second_browser);

  auto original_group = browser()->tab_strip_model()->AddToNewGroup({1, 2});
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, active_browser_list_->size());

  // We should have a restore entry for the window.
  const sessions::TabRestoreService::Entries& entries = service->entries();
  ASSERT_GE(entries.size(), 1u);
  ASSERT_EQ(entries.front()->type, sessions::tab_restore::Type::WINDOW);

  // Restore the window.
  std::vector<sessions::LiveTab*> restored_window_tabs =
      service->RestoreEntryById(second_browser->live_tab_context(),
                                entries.front()->id,
                                WindowOpenDisposition::NEW_FOREGROUND_TAB);
  ASSERT_EQ(2u, active_browser_list_->size());

  // TODO(crbug.com/347746086): When TabGroupsSaveV2 is enabled we will opt to
  // open the saved group instead of individually restoring all of the tabs in
  // the group one at a time. Because of this, RestoreEntryById will only return
  // one tab as being restored. Clean this guard up post launch.
  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    ASSERT_EQ(1u, restored_window_tabs.size());
  } else {
    ASSERT_EQ(3u, restored_window_tabs.size());
  }

  Browser* third_browser = GetBrowser(1);
  ASSERT_NE(second_browser, third_browser);
  ASSERT_EQ(3, third_browser->tab_strip_model()->count());

  // TODO(crbug.com/347746086): Remove guard post TabGroupsSaveV2 launch.
  if (tab_groups::IsTabGroupsSaveV2Enabled()) {
    // The group ID should be new.
    EXPECT_NE(original_group,
              third_browser->tab_strip_model()->GetTabGroupForTab(1));
  } else {
    // We should use the existing group ID.
    EXPECT_EQ(original_group,
              third_browser->tab_strip_model()->GetTabGroupForTab(1));
  }
}

// Ensures window.tab_groups is kept in sync with the groups referenced
// in window.tabs
IN_PROC_BROWSER_TEST_F(TabRestoreTest, WindowTabGroupsMatchesWindowTabs) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());

  AddFileSchemeTabs(browser(), 3);
  ASSERT_EQ(4, browser()->tab_strip_model()->count());

  // Create a new browser from which to restore the first.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ASSERT_EQ(2u, active_browser_list_->size());
  Browser* second_browser = GetBrowser(1);
  ASSERT_NE(browser(), second_browser);

  const auto single_entry_group =
      browser()->tab_strip_model()->AddToNewGroup({3});
  const auto double_entry_group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, active_browser_list_->size());

  // We should have a restore entry for the window.
  const sessions::TabRestoreService::Entries& entries = service->entries();
  ASSERT_GE(entries.size(), 1u);
  ASSERT_EQ(entries.front()->type, sessions::tab_restore::Type::WINDOW);

  const auto* const window_entry =
      static_cast<sessions::tab_restore::Window*>(entries.front().get());

  ASSERT_TRUE(window_entry->tab_groups.contains(single_entry_group));
  ASSERT_TRUE(window_entry->tab_groups.contains(double_entry_group));

  // Restore the first and only tab in the single entry group.
  service->RestoreEntryById(second_browser->live_tab_context(),
                            window_entry->tabs[3]->id,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB);
  // The window should no longer track the single entry group.
  ASSERT_FALSE(window_entry->tab_groups.contains(single_entry_group));

  // Restore one of the tabs in the double entry group.
  service->RestoreEntryById(second_browser->live_tab_context(),
                            window_entry->tabs[2]->id,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB);
  // The window should still track the double entry group.
  ASSERT_NE(window_entry->tab_groups.find(double_entry_group),
            window_entry->tab_groups.end());
  ASSERT_TRUE(window_entry->tab_groups.contains(double_entry_group));

  // Restore the remaining tab in the double entry group.
  service->RestoreEntryById(second_browser->live_tab_context(),
                            window_entry->tabs[1]->id,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB);
  // The window should no longer track the double entry group.
  ASSERT_FALSE(window_entry->tab_groups.contains(double_entry_group));
}

// Ensures that we can restore an entire tab group from a window all at once.
IN_PROC_BROWSER_TEST_F(TabRestoreTest, RestoreEntireGroupInWindow) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());

  AddFileSchemeTabs(browser(), 3);
  ASSERT_EQ(4, browser()->tab_strip_model()->count());

  // Create a new browser from which to restore the first.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  ASSERT_EQ(2u, active_browser_list_->size());
  Browser* second_browser = GetBrowser(1);
  ASSERT_NE(browser(), second_browser);

  const auto single_entry_group_id =
      browser()->tab_strip_model()->AddToNewGroup({3});
  const auto double_entry_group_id =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  CloseBrowserSynchronously(browser());
  ASSERT_EQ(1u, active_browser_list_->size());

  // We should have a restore entry for the window.
  const sessions::TabRestoreService::Entries& entries = service->entries();
  ASSERT_GE(entries.size(), 1u);
  ASSERT_EQ(entries.front()->type, sessions::tab_restore::Type::WINDOW);

  const auto* const window_entry =
      static_cast<sessions::tab_restore::Window*>(entries.front().get());
  ASSERT_TRUE(window_entry->tab_groups.contains(single_entry_group_id));
  ASSERT_TRUE(window_entry->tab_groups.contains(double_entry_group_id));

  // Restore the double entry group.
  const auto& double_entry_group =
      *window_entry->tab_groups.at(double_entry_group_id).get();
  service->RestoreEntryById(second_browser->live_tab_context(),
                            double_entry_group.id,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB);

  // The window should no longer track the double entry group.
  ASSERT_FALSE(window_entry->tab_groups.contains(double_entry_group_id));

  // Restore one of the tabs in the double entry group.
  const auto& single_entry_group =
      *window_entry->tab_groups.at(single_entry_group_id).get();
  service->RestoreEntryById(second_browser->live_tab_context(),
                            single_entry_group.id,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB);

  // The window should no longer track the single entry group.
  ASSERT_FALSE(window_entry->tab_groups.contains(single_entry_group_id));

  // There should only be one tab left in the window.
  EXPECT_EQ(1u, window_entry->tabs.size());
}

class SoftNavigationTabRestoreTest : public TabRestoreTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TabRestoreTest::SetUpCommandLine(command_line);
    features_list_.InitWithFeatures({blink::features::kSoftNavigationHeuristics,
                                     blink::features::kNavigationId},
                                    {});
  }

 private:
  base::test::ScopedFeatureList features_list_;
};

// TODO(crbug.com/40285531): Test is found flaky on linux, win and mac,most
// probably due to mouseclicks not working consistently.
IN_PROC_BROWSER_TEST_F(SoftNavigationTabRestoreTest,
                       DISABLED_SoftNavigationToRestoredTab) {
  // Set up a test web server.
  embedded_test_server()->ServeFilesFromSourceDirectory("content/test/data");
  content::SetupCrossSiteRedirector(embedded_test_server());
  ASSERT_TRUE(embedded_test_server()->Start());

  // Navigate to the first page.
  int starting_tab_count = browser()->tab_strip_model()->count();
  GURL soft_nav_url = embedded_test_server()->GetURL(
      "/session_history/soft-navigation-and-back.html");
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), soft_nav_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  int tab_count = browser()->tab_strip_model()->count();

  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());

  // Observe soft navigations.
  ASSERT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
    window.soft_nav_promise = new Promise(r => {
      new PerformanceObserver(r).observe({
        type: 'soft-navigation',
      })
    });
  )",
                     content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Perform a soft navigation and wait until it's reported.
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));
  ASSERT_TRUE(ExecJs(browser()->tab_strip_model()->GetActiveWebContents(), R"(
    window.soft_nav_promise;
  )"));

  // Close the tab.
  int closed_tab_index = tab_count - 1;
  CloseTab(closed_tab_index);
  EXPECT_EQ(starting_tab_count, browser()->tab_strip_model()->count());

  // Restore the tab.
  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, closed_tab_index));

  // Soft-navigate back.
  content::SimulateMouseClickAt(
      browser()->tab_strip_model()->GetActiveWebContents(), 0,
      blink::WebMouseEvent::Button::kLeft, gfx::Point(100, 100));

  // TODO(crbug.com/40283341) - We're not actually getting a report on
  // the second navigation, as they are not committed. We need to actually wait
  // for the second soft navigation and see that it fires, once the bug is
  // fixed.

  // And make sure everything looks right.
  EXPECT_EQ(starting_tab_count + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(closed_tab_index, browser()->tab_strip_model()->active_index());
  // TODO(crbug.com/40283341) - validate the the URL is back to the
  // original one, once the bug is fixed.
}

class TabRestoreSavedGroupsTest : public TabRestoreTest,
                                  public ::testing::WithParamInterface<bool> {
 public:
  TabRestoreSavedGroupsTest() {
    if (GetParam()) {
      scoped_feature_list_.InitWithFeatures(
          {tab_groups::kTabGroupsSaveV2,
           tab_groups::kTabGroupSyncServiceDesktopMigration},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          {tab_groups::kTabGroupsSaveV2},
          {tab_groups::kTabGroupSyncServiceDesktopMigration});
    }
  }

  // Adds |how_many| tabs to the given browser, all navigated to the youtube.com
  // so when they are closed they are logged in TabRestore. Returns the final
  // number of tabs.
  void AddTabs(Browser* browser, int how_many) {
    for (int i = 0; i < how_many; ++i) {
      AddTab(browser, GURL("https://www.youtube.com"));
    }
  }

  // Adds tab navigated to |url| in the given |browser|.
  void AddTab(Browser* browser, const GURL& url) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser, url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Close a group, then restore it. The group should continue to be saved.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest, RestoreGroup) {
  AddTabs(browser(), 2);
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();

  // Close the group.
  CloseGroup(group);

  // Restore the group.
  chrome::RestoreTab(browser());

  // Check that there is only 1 saved group after restoring.
  ASSERT_TRUE(service->GetGroup(saved_group_id));
  EXPECT_EQ(1u, service->GetAllGroups().size());

  const std::optional<tab_groups::SavedTabGroup> saved_group =
      service->GetGroup(saved_group_id);

  // Verify the saved group reopend properly. The local group id should be
  // different since it is respun when restoring to avoid conflicts.
  EXPECT_TRUE(saved_group->local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group->saved_guid());
  EXPECT_EQ(2u, saved_group->saved_tabs().size());

  // Check the number of tabs in the tabstrip are the same.
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(saved_group->local_group_id().value())
                ->ListTabs(),
            gfx::Range(1, 3));
}

// Close a saved group and restore it. The group should not have updated its
// color to match the restored entry.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       RestoreSavedGroupDoesNotUpdateVisualData) {
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    // TODO(crbug.com/366267926): Fix test after migration.
    GTEST_SKIP()
        << "This test is not supported with the migration flag enabled";
  }

  AddTabs(browser(), 2);
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  TabGroup* const tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group);
  CHECK(tab_group);

  std::u16string original_title = u"This is the original title";
  tab_groups::TabGroupColorId original_color =
      tab_groups::TabGroupColorId::kRed;
  tab_groups::TabGroupVisualData og_visual_data(original_title, original_color);
  tab_group->SetVisualData(og_visual_data);

  std::optional<tab_groups::SavedTabGroup> saved_group =
      service->GetGroup(group);
  ASSERT_TRUE(saved_group);
  base::Uuid saved_group_id = saved_group->saved_guid();

  // Close the group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group);

  // Update the visual data of the saved group.
  std::u16string new_title = u"This is a new title";
  tab_groups::TabGroupColorId new_color = tab_groups::TabGroupColorId::kCyan;
  tab_groups::TabGroupVisualData new_visual_data(new_title, new_color);

  tab_groups::SavedTabGroupKeyedService* old_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser()->profile());
  old_service->model()->UpdatedVisualDataFromSync(saved_group_id,
                                                  &new_visual_data);

  // Restore it.
  chrome::RestoreTab(browser());

  // Check that the restored group keeps the new visual data.
  saved_group = service->GetGroup(saved_group_id);
  CHECK(saved_group);

  EXPECT_NE(original_title, saved_group->title());
  EXPECT_NE(original_color, saved_group->color());
  EXPECT_EQ(new_title, saved_group->title());
  EXPECT_EQ(new_color, saved_group->color());
}

// Verify that any tabs that exist in the restored group but not the saved group
// are added to the saved group.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       RestoreSavedGroupAddsUniqueTabs) {
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    // TODO(crbug.com/366267926): Fix test after migration.
    GTEST_SKIP()
        << "This test is not supported with the migration flag enabled";
  }

  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTabs(browser(), 2);

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  TabGroup* const tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group);
  CHECK(tab_group);

  std::u16string original_title = u"This is the original title";
  tab_groups::TabGroupColorId original_color =
      tab_groups::TabGroupColorId::kRed;
  tab_groups::TabGroupVisualData og_visual_data(original_title, original_color);
  tab_group->SetVisualData(og_visual_data);

  const std::optional<tab_groups::SavedTabGroup> saved_group =
      service->GetGroup(group);
  ASSERT_TRUE(saved_group);

  // Store the ids before closing the group.
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();
  const base::Uuid saved_tab_id_1 =
      saved_group->saved_tabs()[0].saved_tab_guid();
  const base::Uuid saved_tab_id_2 =
      saved_group->saved_tabs()[1].saved_tab_guid();

  // Close the group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group);

  // Only update the tabs using the old service. For the new service this is
  // unsupported while the tabs are closed. In order to do a similar thing, we
  // will either need to implement functions that use the sync ids, OR manually
  // simulate sync events using a fake sync server (See
  // single_client_saved_tab_groups_sync_test.cc).
  tab_groups::SavedTabGroupTab tab_1(GURL("https://www.1.com"), u"Tab 1",
                                     saved_group_id, 0, saved_tab_id_1);
  tab_groups::SavedTabGroupTab tab_2(GURL("https://www.2.com"), u"Tab 2",
                                     saved_group_id, 0, saved_tab_id_2);

  tab_groups::SavedTabGroupKeyedService* original_service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser()->profile());
  original_service->model()->UpdateTabInGroup(saved_group_id, tab_1);
  original_service->model()->UpdateTabInGroup(saved_group_id, tab_2);

  // Restore the group.
  chrome::RestoreTab(browser());

  const std::optional<tab_groups::SavedTabGroup> restored_saved_group =
      service->GetGroup(saved_group_id);

  // There are 2 www.youtube.com urls, only 1 should be added.
  EXPECT_TRUE(restored_saved_group);
  EXPECT_EQ(3u, restored_saved_group->saved_tabs().size());
}

// Verify that a restored group which is already open does not open a new group
// but focuses a tab in the group.
// TODO(crbug.com/353618704): Re-enable this test
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_RestoreSavedGroupFocusedIfOpenAlready \
  DISABLED_RestoreSavedGroupFocusedIfOpenAlready
#else
#define MAYBE_RestoreSavedGroupFocusedIfOpenAlready \
  RestoreSavedGroupFocusedIfOpenAlready
#endif
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       MAYBE_RestoreSavedGroupFocusedIfOpenAlready) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTabs(browser(), 2);
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});
  TabGroup* const tab_group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group);
  CHECK(tab_group);

  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();

  // Close the group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group);

  // Reopen the group.
  service->OpenTabGroup(
      saved_group_id,
      std::make_unique<tab_groups::TabGroupActionContextDesktop>(
          browser(), tab_groups::OpeningSource::kOpenedFromTabRestore));

  // Focus a tab not in the group.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());

  // Restore it.
  chrome::RestoreTab(browser());

  // Check first tab in group focused.
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  std::optional<tab_groups::TabGroupId> restored_group_id =
      browser()->tab_strip_model()->GetActiveTab()->group();
  ASSERT_TRUE(restored_group_id.has_value());
  EXPECT_TRUE(service->GetGroup(restored_group_id.value()));
}

// Verify that when restored tabs in unsaved groups make the group saved.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest, RestoreTabInUnsavedGroup) {
  if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
    // TODO(crbug.com/366267926): Fix test after migration.
    GTEST_SKIP()
        << "This test is not supported with the migration flag enabled";
  }

  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  // Open 2 unique tabs. Duplicate URLs are not reopened when restoring.
  AddTab(browser(), GURL("https://www.1.com"));
  AddTab(browser(), GURL("https://www.2.com"));

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  // Close the first tab in the group.
  CloseTab(1);

  // Restore it.
  chrome::RestoreTab(browser());

  // Check that there is still only 1 saved group after restoring.
  ASSERT_EQ(1u, service->GetAllGroups().size());

  // Verify the tab that was just restored and the tab that was open in the
  // group are saved.
  tab_groups::SavedTabGroup saved_group = service->GetAllGroups()[0];
  EXPECT_EQ(group, saved_group.local_group_id().value());
  ASSERT_EQ(2u, saved_group.saved_tabs().size());

  // Check the number of tabs in the tabstrip are the same.
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(group)
                ->ListTabs(),
            gfx::Range(1, 3));

  // Closing all tabs individually should unsave the group but do it manually in
  // case that behavior changes in the future.
  service->RemoveGroup(group);

  // Close both tabs individually and restore them. Verify both tabs added to
  // the group.
  CloseTab(1);
  CloseTab(1);

  // Restore the first tab.
  chrome::RestoreTab(browser());

  // Check that there is only 1 saved group after restoring. And it only has the
  // one tab we have restored.
  saved_group = service->GetAllGroups()[0];
  ASSERT_EQ(1u, service->GetAllGroups().size());
  EXPECT_EQ(1u, saved_group.saved_tabs().size());

  // Restore the second tab.
  chrome::RestoreTab(browser());

  // Check that there is still only 1 saved group after restoring. It should
  // have 2 tabs now.
  saved_group = service->GetAllGroups()[0];
  ASSERT_EQ(1u, service->GetAllGroups().size());
  EXPECT_EQ(2u, saved_group.saved_tabs().size());
}

// Verify restoring a tab part of a currently open saved group, adds the tab to
// the saved group.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest, RestoreTabInSavedGroup) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTab(browser(), GURL("https://www.1.com"));
  AddTab(browser(), GURL("https://www.2.com"));

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();
  EXPECT_EQ(1u, service->GetAllGroups().size());

  // Close the first tab in the group.
  CloseTab(1);

  tab_groups::SavedTabGroup saved_group = *service->GetGroup(saved_group_id);

  EXPECT_TRUE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(1u, saved_group.saved_tabs().size());

  // Restore the tab.
  chrome::RestoreTab(browser());

  saved_group = *service->GetGroup(saved_group_id);

  // Verify the saved group reopend properly. The local group id should be
  // different since it is respun when restoring to avoid conflicts.
  EXPECT_TRUE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(2u, saved_group.saved_tabs().size());

  // Check the number of tabs in the tabstrip are the same.
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(saved_group.local_group_id().value())
                ->ListTabs(),
            gfx::Range(1, 3));
}

// Verify closing all tabs in a group individually, then restoring all of the
// tabs puts them in the same group.
IN_PROC_BROWSER_TEST_P(
    TabRestoreSavedGroupsTest,
    ClosingAllTabsInGroupThenRestoringTabsPutsThemInSameGroup) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTab(browser(), GURL("https://www.1.com"));
  AddTab(browser(), GURL("https://www.2.com"));
  AddTab(browser(), GURL("https://www.3.com"));

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2, 3});

  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();
  EXPECT_EQ(1u, service->GetAllGroups().size());

  // Close all tabs in the group one by one.
  CloseTab(3);
  CloseTab(2);
  CloseTab(1);

  EXPECT_TRUE(service->GetAllGroups().empty());
  EXPECT_TRUE(
      browser()->tab_strip_model()->group_model()->ListTabGroups().empty());

  // Restore the tab.
  chrome::RestoreTab(browser());
  EXPECT_EQ(1u, service->GetAllGroups().size());
  EXPECT_EQ(
      1u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());

  tab_groups::TabGroupId restored_id =
      browser()->tab_strip_model()->group_model()->ListTabGroups().back();
  ASSERT_TRUE(service->GetGroup(restored_id));

  chrome::RestoreTab(browser());
  EXPECT_EQ(1u, service->GetAllGroups().size());
  EXPECT_EQ(
      1u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());

  chrome::RestoreTab(browser());
  EXPECT_EQ(1u, service->GetAllGroups().size());
  EXPECT_EQ(
      1u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());

  tab_groups::SavedTabGroup saved_group = *service->GetGroup(restored_id);

  // Verify the saved group reopend properly. The local group id should be
  // different since it is respun when restoring to avoid conflicts.
  EXPECT_NE(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(3u, saved_group.saved_tabs().size());

  // Check the number of tabs in the tabstrip are the same.
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(saved_group.local_group_id().value())
                ->ListTabs(),
            gfx::Range(1, 4));
}

// Verify restoring a tab part of a recently restored saved group, adds the tab
// to the saved group.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       RestoreTabAfterGroupRestored) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTab(browser(), GURL("https://www.1.com"));
  AddTab(browser(), GURL("https://www.2.com"));

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();
  EXPECT_EQ(1u, service->GetAllGroups().size());

  // Close the first tab in the group.
  CloseTab(1);

  // Close the group.
  CloseGroup(group);

  tab_groups::SavedTabGroup saved_group = *service->GetGroup(saved_group_id);

  EXPECT_FALSE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(1u, saved_group.saved_tabs().size());

  // Restore the group.
  chrome::RestoreTab(browser());
  saved_group = *service->GetGroup(saved_group_id);

  // Verify the saved group reopened properly. The local group id should be
  // different since it is respun when restoring to avoid conflicts.
  EXPECT_TRUE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(1u, saved_group.saved_tabs().size());

  // Restore the tab.
  chrome::RestoreTab(browser());
  saved_group = *service->GetGroup(saved_group_id);

  ASSERT_EQ(
      1u, browser()->tab_strip_model()->group_model()->ListTabGroups().size());
  // Verify that the second tab was added to the group.
  EXPECT_EQ(2u, saved_group.saved_tabs().size());

  // Check the number of tabs in the tabstrip are the same.
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(saved_group.local_group_id().value())
                ->ListTabs(),
            gfx::Range(1, 3));
}

// Verify restoring a tab part of a closed group, opens the entire group and
// adds the tab to it with its navigation stack.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest, RestoreTabWhenGroupIsClosed) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTab(browser(), GURL("https://www.1.com"));
  AddTab(browser(), GURL("https://www.2.com"));

  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();
  EXPECT_EQ(1u, service->GetAllGroups().size());

  // Navigate the second tab in the group a few times.
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.3.com"), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.4.com"), WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  EXPECT_EQ(3, web_contents->GetController().GetEntryCount());

  // Close the second tab in the group.
  CloseTab(2);

  // Close the group.
  browser()->tab_strip_model()->CloseAllTabsInGroup(group);

  EXPECT_EQ(1u, service->GetAllGroups().size());

  tab_groups::SavedTabGroup saved_group = *service->GetGroup(saved_group_id);

  EXPECT_FALSE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(1u, saved_group.saved_tabs().size());

  // There should be one tab left in the browser.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Restore the tab we navigated on which should be the last entry.
  sessions::TabRestoreService* trs_service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  EXPECT_EQ(2u, trs_service->entries().size());
  trs_service->RestoreEntryById(browser()->live_tab_context(),
                                trs_service->entries().back()->id,
                                WindowOpenDisposition::NEW_FOREGROUND_TAB);
  EXPECT_EQ(3, browser()->tab_strip_model()->count());

  saved_group = *service->GetGroup(saved_group_id);

  // Verify the saved tab was added to the correct group.
  EXPECT_TRUE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(2u, saved_group.saved_tabs().size());

  // Check the number of tabs in the tabstrip are the same.
  // State: |New Tab| Group[|Restored Tab 1| |Restored Tab 2|]
  EXPECT_EQ(browser()
                ->tab_strip_model()
                ->group_model()
                ->GetTabGroup(saved_group.local_group_id().value())
                ->ListTabs(),
            gfx::Range(1, 3));

  content::WebContents* restored_contents =
      browser()->tab_strip_model()->GetWebContentsAt(2);
  EXPECT_EQ(3, restored_contents->GetController().GetEntryCount());
}

// Verify restoring a window with a saved group (that is closed) opens the saved
// group in the window instead of creating a new group.
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       RestoreWindowWithClosedSavedGroup) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTab(browser(), GURL("https://www.1.com"));
  AddTab(browser(), GURL("https://www.2.com"));

  // Add tabs to a group.
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();
  EXPECT_EQ(1u, service->GetAllGroups().size());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Close the first browser.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list_->size());

  // Use the newly opened browser to restore the closed window.
  chrome::RestoreTab(active_browser_list_->get(0));
  Browser* browser = GetBrowser(1);
  const std::vector<tab_groups::TabGroupId>& group_ids =
      browser->tab_strip_model()->group_model()->ListTabGroups();

  // Check that the restored window has 3 tabs, 1 group that is still saved
  // with the same saved group id.
  EXPECT_EQ(3, browser->tab_strip_model()->count());
  EXPECT_EQ(1u, service->GetAllGroups().size());
  EXPECT_EQ(1u, group_ids.size());
  EXPECT_TRUE(service->GetGroup(group_ids[0]));
  EXPECT_TRUE(service->GetGroup(saved_group_id));

  tab_groups::SavedTabGroup saved_group = *service->GetGroup(saved_group_id);

  EXPECT_TRUE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(2u, saved_group.saved_tabs().size());
}

// Verify that restoring a window with a saved group that is already open does
// not restore that group twice.
// TODO(crbug.com/353618704): Re-enable this test
#if defined(ADDRESS_SANITIZER) || defined(MEMORY_SANITIZER)
#define MAYBE_RestoreWindowWithOpenedSavedGroup \
  DISABLED_RestoreWindowWithOpenedSavedGroup
#else
#define MAYBE_RestoreWindowWithOpenedSavedGroup \
  RestoreWindowWithOpenedSavedGroup
#endif
IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       MAYBE_RestoreWindowWithOpenedSavedGroup) {
  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  AddTab(browser(), GURL("https://www.1.com"));
  AddTab(browser(), GURL("https://www.2.com"));
  AddTab(browser(), GURL("https://www.3.com"));

  // Add tabs to a group.
  tab_groups::TabGroupId group =
      browser()->tab_strip_model()->AddToNewGroup({1, 2});

  ASSERT_TRUE(service->GetGroup(group));
  base::Uuid saved_group_id = service->GetGroup(group)->saved_guid();
  EXPECT_EQ(1u, service->GetAllGroups().size());

  // Create a new browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);
  EXPECT_EQ(2u, active_browser_list_->size());

  // Close the first browser.
  CloseBrowserSynchronously(browser());
  EXPECT_EQ(1u, active_browser_list_->size());

  // Open the saved group in the second browser.
  Browser* second_browser = GetBrowser(0);
  service->OpenTabGroup(
      saved_group_id,
      std::make_unique<tab_groups::TabGroupActionContextDesktop>(
          second_browser, tab_groups::OpeningSource::kOpenedFromTabRestore));

  // Use the second browser to restore the closed window.
  chrome::RestoreTab(second_browser);
  Browser* first_browser = GetBrowser(1);

  const std::vector<tab_groups::TabGroupId>& first_browser_group_ids =
      first_browser->tab_strip_model()->group_model()->ListTabGroups();
  const std::vector<tab_groups::TabGroupId>& second_browser_group_ids =
      second_browser->tab_strip_model()->group_model()->ListTabGroups();

  // Verify there is only 1 saved group, the first browser has 4 tabs (how it
  // was originally), and the second browser has 1 tab (new tab page).
  EXPECT_EQ(1u, service->GetAllGroups().size());
  EXPECT_EQ(4, first_browser->tab_strip_model()->count());
  EXPECT_EQ(1, second_browser->tab_strip_model()->count());

  EXPECT_TRUE(second_browser_group_ids.empty());
  EXPECT_EQ(1u, first_browser_group_ids.size());
  EXPECT_TRUE(service->GetGroup(first_browser_group_ids[0]));
  EXPECT_TRUE(service->GetGroup(saved_group_id));

  tab_groups::SavedTabGroup saved_group = *service->GetGroup(saved_group_id);

  EXPECT_TRUE(saved_group.local_group_id().has_value());
  EXPECT_EQ(saved_group_id, saved_group.saved_guid());
  EXPECT_EQ(2u, saved_group.saved_tabs().size());
}

IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       PRE_SavedGroupNotDuplicatedAfterRestart) {
  // Enable session service in default mode.
  EnableSessionService();

  // Navigate to url1 in the current tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1_, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Add the tab to a group.
  browser()->tab_strip_model()->AddToNewGroup({0});
}

IN_PROC_BROWSER_TEST_P(TabRestoreSavedGroupsTest,
                       SavedGroupNotDuplicatedAfterRestart) {
  // Enable session service in default mode.
  EnableSessionService();

  tab_groups::TabGroupSyncService* service =
      tab_groups::SavedTabGroupUtils::GetServiceForProfile(
          browser()->profile());
  ASSERT_TRUE(service);

  // Verify there is only 1 saved group in the model and it is not open.
  ASSERT_EQ(1u, service->GetAllGroups().size());
  tab_groups::SavedTabGroup saved_group = service->GetAllGroups()[0];
  EXPECT_EQ(std::nullopt, saved_group.local_group_id());

  const base::Uuid& saved_id = saved_group.saved_guid();

  // Restore the group.
  // We use this over RestoreGroup() since we don't have reference to the
  // previous group id defined in the PRE step to this test.
  chrome::RestoreTab(browser());

  // Verify the browser has a single tab group.
  TabGroupModel* group_model = browser()->tab_strip_model()->group_model();
  EXPECT_EQ(1u, group_model->ListTabGroups().size());

  // Verify there is still only 1 saved group and that it is open now.
  EXPECT_EQ(1u, service->GetAllGroups().size());
  saved_group = *service->GetGroup(saved_id);
  EXPECT_TRUE(saved_group.local_group_id().has_value());

  // Verify the local group id exists in the TabGroupModel.
  EXPECT_TRUE(
      group_model->ContainsTabGroup(saved_group.local_group_id().value()));
}

INSTANTIATE_TEST_SUITE_P(TabRestore,
                         TabRestoreSavedGroupsTest,
                         testing::Bool());

class TabRestoreSycnedServiceTest : public TabRestoreTest {
 public:
  TabRestoreSycnedServiceTest() {
    scoped_feature_list_.InitWithFeatures(
        {tab_groups::kTabGroupsSaveV2,
         tab_groups::kTabGroupSyncServiceDesktopMigration},
        {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Closing the last tab in a collapsed group then restoring will place the group
// back collapsed with its metadata. Exact same test as
// TabRestoreTest.RestoreCollapsedGroupTab but goes through the sync service
// path initiated in BrowserLiveTabContext::AddRestoredTab.
IN_PROC_BROWSER_TEST_F(TabRestoreSycnedServiceTest, RestoreCollapsedGroupTab) {
  ASSERT_TRUE(browser()->tab_strip_model()->SupportsTabGroups());

  const int tab_count = AddFileSchemeTabs(browser(), 1);
  ASSERT_LE(2, tab_count);

  const int grouped_tab_index = tab_count - 1;
  tab_groups::TabGroupId group_id =
      browser()->tab_strip_model()->AddToNewGroup({grouped_tab_index});
  const tab_groups::TabGroupVisualData visual_data(
      u"Foo", tab_groups::TabGroupColorId::kCyan, true);

  TabGroup* group =
      browser()->tab_strip_model()->group_model()->GetTabGroup(group_id);
  ASSERT_TRUE(group);
  group->SetVisualData(visual_data);

  CloseTab(grouped_tab_index);

  ASSERT_NO_FATAL_FAILURE(RestoreTab(0, grouped_tab_index));
  ASSERT_EQ(tab_count, browser()->tab_strip_model()->count());

  EXPECT_EQ(group_id, browser()
                          ->tab_strip_model()
                          ->GetTabGroupForTab(grouped_tab_index)
                          .value());
  const tab_groups::TabGroupVisualData* data = browser()
                                                   ->tab_strip_model()
                                                   ->group_model()
                                                   ->GetTabGroup(group_id)
                                                   ->visual_data();
  ASSERT_TRUE(data);
  EXPECT_EQ(data->title(), visual_data.title());
  EXPECT_EQ(data->color(), visual_data.color());
  EXPECT_TRUE(data->is_collapsed());
}
}  // namespace sessions
