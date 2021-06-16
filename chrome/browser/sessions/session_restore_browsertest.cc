// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "base/util/memory_pressure/fake_memory_pressure_monitor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/prefs/session_startup_pref.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/resource_coordinator/session_restore_policy.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/sessions/session_restore_test_helper.h"
#include "chrome/browser/sessions/session_restore_test_utils.h"
#include "chrome/browser/sessions/session_service.h"
#include "chrome/browser/sessions/session_service_factory.h"
#include "chrome/browser/sessions/session_service_log.h"
#include "chrome/browser/sessions/session_service_test_helper.h"
#include "chrome/browser/sessions/tab_loader_delegate.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/startup/startup_types.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/keep_alive_registry/keep_alive_types.h"
#include "components/keep_alive_registry/scoped_keep_alive.h"
#include "components/sessions/content/content_live_tab.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/session_constants.h"
#include "components/sessions/core/session_types.h"
#include "components/sessions/core/tab_restore_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/slow_http_response.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/features.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_palette.h"

#if BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
#include "chrome/browser/sessions/app_session_service.h"
#include "chrome/browser/sessions/app_session_service_factory.h"
#include "chrome/browser/sessions/app_session_service_test_helper.h"
#endif

#if defined(OS_MAC)
#include "base/mac/scoped_nsautorelease_pool.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

using sessions::ContentTestHelper;
using sessions::SerializedNavigationEntry;
using sessions::SerializedNavigationEntryTestHelper;

GURL GetUrl1() {
  return ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("session_history"),
      base::FilePath().AppendASCII("bot1.html"));
}

GURL GetUrl2() {
  return ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("session_history"),
      base::FilePath().AppendASCII("bot2.html"));
}

GURL GetUrl3() {
  return ui_test_utils::GetTestUrl(
      base::FilePath().AppendASCII("session_history"),
      base::FilePath().AppendASCII("bot3.html"));
}

bool WaitForTabToLoad(Browser* browser, int index) {
  if (index >= browser->tab_strip_model()->count())
    return false;
  content::WebContents* contents =
      browser->tab_strip_model()->GetWebContentsAt(index);
  contents->GetController().LoadIfNecessary();
  return content::WaitForLoadStop(contents);
}

void WaitForTabsToLoad(Browser* browser) {
  for (int i = 0; i < browser->tab_strip_model()->count(); ++i)
    EXPECT_TRUE(WaitForTabToLoad(browser, i));
}

class SessionRestoreTest : public InProcessBrowserTest {
 public:
  SessionRestoreTest() = default;
  ~SessionRestoreTest() override = default;

 protected:
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(nkostylev): Investigate if we can remove this switch.
    command_line->AppendSwitch(switches::kCreateBrowserOnStartupForTests);
  }
#endif

  void SetUpOnMainThread() override {
    active_browser_list_ = BrowserList::GetInstance();

    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(browser()->profile(), pref);
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const testing::TestInfo* const test_info =
        testing::UnitTest::GetInstance()->current_test_info();
    if (strcmp(test_info->name(), "NoSessionRestoreNewWindowChromeOS") != 0) {
      // Undo the effect of kBrowserAliveWithNoWindows in defaults.cc so that we
      // can get these test to work without quitting.
      SessionServiceTestHelper helper(browser()->profile());
      helper.SetForceBrowserNotAliveWithNoWindows(true);
    }
#endif
  }

  Browser* QuitBrowserAndRestore(Browser* browser, int expected_tab_count) {
    return QuitBrowserAndRestoreWithURL(
        browser, expected_tab_count, GURL(), true);
  }

  Browser* QuitBrowserAndRestoreWithURL(Browser* browser,
                                        int expected_tab_count,
                                        const GURL& url,
                                        bool no_memory_pressure) {
    Profile* profile = browser->profile();

    // Close the browser.
    auto keep_alive = std::make_unique<ScopedKeepAlive>(
        KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
    auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
        profile, ProfileKeepAliveOrigin::kBrowserWindow);
    CloseBrowserSynchronously(browser);

    ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
    SessionRestoreTestHelper restore_observer;

    // Ensure the session service factory is started, even if it was explicitly
    // shut down.
    SessionServiceTestHelper helper(profile);
    helper.SetForceBrowserNotAliveWithNoWindows(true);

    // Create a new window, which should trigger session restore.
    if (url.is_empty()) {
      chrome::NewEmptyWindow(profile);
    } else {
      NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
      Navigate(&params);
    }

    Browser* new_browser =
        chrome::FindBrowserWithWebContents(tab_waiter.Wait());

    // Stop loading anything more if we are running out of space.
    if (!no_memory_pressure) {
      fake_memory_pressure_monitor_.SetAndNotifyMemoryPressure(
          base::MemoryPressureMonitor::MemoryPressureLevel::
              MEMORY_PRESSURE_LEVEL_CRITICAL);
    }
    restore_observer.Wait();

    if (no_memory_pressure)
      WaitForTabsToLoad(new_browser);

    keep_alive.reset();
    profile_keep_alive.reset();

    return new_browser;
  }

  void GoBack(Browser* browser) {
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    chrome::GoBack(browser, WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }

  void GoForward(Browser* browser) {
    content::TestNavigationObserver observer(
        browser->tab_strip_model()->GetActiveWebContents());
    chrome::GoForward(browser, WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }

  void AssertOneWindowWithOneTab(Browser* browser) {
    ASSERT_EQ(1u, active_browser_list_->size());
    ASSERT_EQ(1, browser->tab_strip_model()->count());
  }

  int RenderProcessHostCount() {
    content::RenderProcessHost::iterator hosts =
        content::RenderProcessHost::AllHostsIterator();
    int count = 0;
    while (!hosts.IsAtEnd()) {
      if (hosts.GetCurrentValue()->IsInitializedAndNotDead())
        count++;
      hosts.Advance();
    }
    return count;
  }

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* CreateSecondaryProfile(int profile_num) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    base::FilePath profile_path = profile_manager->user_data_dir();
    profile_path = profile_path.AppendASCII(
        base::StringPrintf("New Profile %d", profile_num));
    Profile* profile = profile_manager->GetProfile(profile_path);
    SessionStartupPref pref(SessionStartupPref::LAST);
    SessionStartupPref::SetStartupPref(profile, pref);
    return profile;
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  const BrowserList* active_browser_list_ = nullptr;

 private:
  util::test::FakeMemoryPressureMonitor fake_memory_pressure_monitor_;
};

// Activates the smart restore behaviour and tracks the loading of tabs.
class SmartSessionRestoreTest : public SessionRestoreTest,
                                public content::NotificationObserver {
 public:
  SmartSessionRestoreTest() = default;

  void StartObserving(size_t num_tabs) {
    // Start by clearing everything so it can be reused in the same test.
    web_contents_.clear();
    registrar_.RemoveAll();
    num_tabs_ = num_tabs;
    registrar_.Add(this, content::NOTIFICATION_LOAD_START,
                   content::NotificationService::AllSources());
  }
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override {
    switch (type) {
      case content::NOTIFICATION_LOAD_START: {
        content::NavigationController* controller =
            content::Source<content::NavigationController>(source).ptr();
        web_contents_.push_back(controller->GetWebContents());
        if (web_contents_.size() == num_tabs_)
          message_loop_runner_->Quit();
        break;
      }
    }
  }
  const std::vector<content::WebContents*>& web_contents() const {
    return web_contents_;
  }

  void WaitForAllTabsToStartLoading() {
    message_loop_runner_ = new content::MessageLoopRunner;
    message_loop_runner_->Run();
  }

 protected:
  static const size_t kExpectedNumTabs;
  static const char* const kUrls[];

 private:
  content::NotificationRegistrar registrar_;
  // Ordered by load start order.
  std::vector<content::WebContents*> web_contents_;
  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;
  size_t num_tabs_;
  testing::ScopedAlwaysLoadSessionRestoreTestPolicy test_policy_;

  DISALLOW_COPY_AND_ASSIGN(SmartSessionRestoreTest);
};

// static
const size_t SmartSessionRestoreTest::kExpectedNumTabs = 6;
// static
const char* const SmartSessionRestoreTest::kUrls[] = {
    "http://google.com/1",
    "http://google.com/2",
    "http://google.com/3",
    "http://google.com/4",
    "http://google.com/5",
    "http://google.com/6"};

// Restore session with url passed in command line.
class SessionRestoreWithURLInCommandLineTest : public SessionRestoreTest {
 public:
  SessionRestoreWithURLInCommandLineTest() = default;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    SessionRestoreTest::SetUpCommandLine(command_line);
    command_line_url_ = ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(FILE_PATH_LITERAL("title1.html")));
    command_line->AppendArg(command_line_url_.spec());
  }

  GURL command_line_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionRestoreWithURLInCommandLineTest);
};

// Verifies that restored tabs have a root window. This is important
// otherwise the wrong information is communicated to the renderer.
// (http://crbug.com/342672).
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoredTabsShouldHaveWindow) {
  // Create tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restart and session restore the tabs.
  Browser* restored = QuitBrowserAndRestore(browser(), 3);
  TabStripModel* tab_strip_model = restored->tab_strip_model();
  const int tabs = tab_strip_model->count();
  ASSERT_EQ(3, tabs);

  // Check the restored tabs have a window to get screen info from.
  // On Aura it should also have a root window.
  for (int i = 0; i < tabs; ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    EXPECT_TRUE(contents->GetTopLevelNativeWindow());
#if defined(USE_AURA)
    EXPECT_TRUE(contents->GetNativeView()->GetRootWindow());
#endif
  }
}

// Verify that restored tabs have correct disposition. Only one tab should
// have "visible" visibility state, the rest should not.
// (http://crbug.com/155365 http://crbug.com/118269)
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
    RestoredTabsHaveCorrectVisibilityState) {
  // Create tabs.
  GURL test_page(ui_test_utils::GetTestUrl(base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("tab-restore-visibility.html"))));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restart and session restore the tabs.
  content::DOMMessageQueue message_queue;
  Browser* restored = QuitBrowserAndRestore(browser(), 3);
  for (int i = 0; i < 2; ++i) {
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"READY\"", message);
  }

  // There should be 3 restored tabs in the new browser.
  TabStripModel* tab_strip_model = restored->tab_strip_model();
  const int tabs = tab_strip_model->count();
  ASSERT_EQ(3, tabs);

  // The middle tab only should have visible disposition.
  for (int i = 0; i < tabs; ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    std::string document_visibility_state;
    const char kGetStateJS[] = "window.domAutomationController.send("
        "window.document.visibilityState);";
    EXPECT_TRUE(content::ExecuteScriptAndExtractString(
        contents, kGetStateJS, &document_visibility_state));
    if (i == 1) {
      EXPECT_EQ("visible", document_visibility_state);
    } else {
      EXPECT_EQ("hidden", document_visibility_state);
    }
  }
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoredTabsHaveCorrectInitialSize) {
  // Create tabs.
  GURL test_page(ui_test_utils::GetTestUrl(
      base::FilePath(),
      base::FilePath(FILE_PATH_LITERAL("tab-restore-visibility.html"))));
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), test_page, WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restart and session restore the tabs.
  content::DOMMessageQueue message_queue;
  Browser* restored = QuitBrowserAndRestore(browser(), 3);
  for (int i = 0; i < 2; ++i) {
    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_EQ("\"READY\"", message);
  }

  // There should be 3 restored tabs in the new browser.
  TabStripModel* tab_strip_model = restored->tab_strip_model();
  const int tabs = tab_strip_model->count();
  ASSERT_EQ(3, tabs);

  const gfx::Size contents_size = restored->window()->GetContentsSize();
  for (int i = 0; i < tabs; ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    int width = 0;
    const char kGetWidthJS[] =
        "window.domAutomationController.send("
        "window.innerWidth);";
    EXPECT_TRUE(
        content::ExecuteScriptAndExtractInt(contents, kGetWidthJS, &width));
    int height = 0;
    const char kGetHeigthJS[] =
        "window.domAutomationController.send("
        "window.innerHeight);";
    EXPECT_TRUE(
        content::ExecuteScriptAndExtractInt(contents, kGetHeigthJS, &height));
    const gfx::Size tab_size(width, height);
    EXPECT_EQ(contents_size, tab_size);
  }
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Verify that session restore does not occur when a user opens a browser window
// when no other browser windows are open on ChromeOS.
// TODO(pkotwicz): Add test which doesn't open incognito browser once
// disable-zero-browsers-open-for-tests is removed.
// (http://crbug.com/119175)
// TODO(pkotwicz): Mac should have the behavior outlined by this test. It should
// not do session restore if an incognito window is already open.
// (http://crbug.com/120927)
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, NoSessionRestoreNewWindowChromeOS) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  Browser* incognito_browser = CreateIncognitoBrowser();
  chrome::AddTabAt(incognito_browser, GURL(), -1, true);
  incognito_browser->window()->Show();

  // Close the normal browser. After this we only have the incognito window
  // open.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should open NTP.
  chrome::NewWindow(incognito_browser);
  Browser* new_browser = BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(new_browser, incognito_browser);

  ASSERT_TRUE(new_browser);
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
  EXPECT_EQ(GURL(chrome::kChromeUINewTabURL),
            new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}

// Test that maximized applications get restored maximized.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, MaximizedApps) {
  const char* app_name = "TestApp";
  Browser* app_browser = CreateBrowserForApp(app_name, browser()->profile());
  app_browser->window()->Maximize();
  app_browser->window()->Show();
  EXPECT_TRUE(app_browser->window()->IsMaximized());
  EXPECT_TRUE(app_browser->is_type_app());

  // Close the normal browser. After this we only have the app_browser window.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should open NTP.
  chrome::NewWindow(app_browser);
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();

  ASSERT_TRUE(new_browser);
  EXPECT_TRUE(app_browser->window()->IsMaximized());
  EXPECT_TRUE(app_browser->is_type_app());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Creates a tabbed browser and popup and makes sure we restore both.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, NormalAndPopup) {
  // Open a popup.
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  ASSERT_EQ(2u, active_browser_list_->size());

  // Simulate an exit by shutting down the session service. If we don't do this
  // the first window close is treated as though the user closed the window
  // and won't be restored.
  SessionServiceFactory::ShutdownForProfile(browser()->profile());

  // Restart and make sure we have two windows.
  CloseBrowserSynchronously(popup);
  QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(2u, active_browser_list_->size());
  EXPECT_EQ(Browser::TYPE_NORMAL, active_browser_list_->get(0)->type());
  EXPECT_EQ(Browser::TYPE_POPUP, active_browser_list_->get(1)->type());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreIndividualTabFromWindow) {
  GURL url1(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));
  // Any page that will yield a 200 status code will work here.
  GURL url2(chrome::kChromeUIVersionURL);
  GURL url3(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title3.html"))));

  // Add and navigate three tabs.
  ui_test_utils::NavigateToURL(browser(), url1);
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), url2,
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    chrome::AddSelectedTabWithURL(browser(), url3,
                                  ui::PAGE_TRANSITION_LINK);
    observer.Wait();
  }

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();

  browser()->window()->Close();

  // Expect a window with three tabs.
  ASSERT_EQ(1U, service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::WINDOW,
            service->entries().front()->type);
  auto* window = static_cast<sessions::TabRestoreService::Window*>(
      service->entries().front().get());
  EXPECT_EQ(3U, window->tabs.size());

  // Find the SessionID for entry2. Since the session service was destroyed,
  // there is no guarantee that the SessionID for the tab has remained the same.
  base::Time timestamp;
  int http_status_code = 0;
  for (const auto& tab_ptr : window->tabs) {
    const sessions::TabRestoreService::Tab& tab = *tab_ptr;
    // If this tab held url2, then restore this single tab.
    if (tab.navigations[0].virtual_url() == url2) {
      timestamp = tab.navigations[0].timestamp();
      http_status_code = tab.navigations[0].http_status_code();
      std::vector<sessions::LiveTab*> content = service->RestoreEntryById(
          NULL, tab.id, WindowOpenDisposition::UNKNOWN);
      ASSERT_EQ(1U, content.size());
      sessions::ContentLiveTab* live_tab =
          static_cast<sessions::ContentLiveTab*>(content[0]);
      ASSERT_TRUE(live_tab);
      EXPECT_EQ(url2, live_tab->web_contents()->GetURL());
      break;
    }
  }
  EXPECT_FALSE(timestamp.is_null());
  EXPECT_EQ(200, http_status_code);

  // Make sure that the restored tab is removed from the service.
  ASSERT_EQ(1U, service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::WINDOW,
            service->entries().front()->type);
  window = static_cast<sessions::TabRestoreService::Window*>(
      service->entries().front().get());
  EXPECT_EQ(2U, window->tabs.size());

  // Make sure that the restored tab was restored with the correct
  // timestamp and status code.
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);
  content::NavigationEntry* entry =
      contents->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  EXPECT_EQ(timestamp, entry->GetTimestamp());
  EXPECT_EQ(http_status_code, entry->GetHttpStatusCode());
}

// Flaky on Linux. https://crbug.com/537592.
#if defined(OS_LINUX) || defined(OS_CHROMEOS)
#define MAYBE_WindowWithOneTab DISABLED_WindowWithOneTab
#else
#define MAYBE_WindowWithOneTab WindowWithOneTab
#endif
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, MAYBE_WindowWithOneTab) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  sessions::TabRestoreService* service =
      TabRestoreServiceFactory::GetForProfile(browser()->profile());
  service->ClearEntries();
  EXPECT_EQ(0U, service->entries().size());

  // Close the window.
  browser()->window()->Close();

  // Expect the window to be converted to a tab by the TRS.
  EXPECT_EQ(1U, service->entries().size());
  ASSERT_EQ(sessions::TabRestoreService::TAB, service->entries().front()->type);
  auto* tab = static_cast<const sessions::TabRestoreService::Tab*>(
      service->entries().front().get());

  // Restore the tab.
  std::vector<sessions::LiveTab*> content =
      service->RestoreEntryById(NULL, tab->id, WindowOpenDisposition::UNKNOWN);
  ASSERT_EQ(1U, content.size());
  ASSERT_TRUE(content[0]);
  EXPECT_EQ(url, static_cast<sessions::ContentLiveTab*>(content[0])
                     ->web_contents()
                     ->GetURL());

  // Make sure the restore was successful.
  EXPECT_EQ(0U, service->entries().size());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
// This test does not apply to ChromeOS as ChromeOS does not do session
// restore when a new window is open.

// Verifies we remember the last browser window when closing the last
// non-incognito window while an incognito window is open.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, IncognitotoNonIncognito) {
  GURL url(ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html"))));

  // Add a single tab.
  ui_test_utils::NavigateToURL(browser(), url);

  // Create a new incognito window.
  Browser* incognito_browser = CreateIncognitoBrowser();
  chrome::AddTabAt(incognito_browser, GURL(), -1, true);
  incognito_browser->window()->Show();

  // Close the normal browser. After this we only have the incognito window
  // open.
  CloseBrowserSynchronously(browser());

  // Create a new window, which should trigger session restore.
  chrome::NewWindow(incognito_browser);
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();

  // The first tab should have 'url' as its url.
  ASSERT_TRUE(new_browser);
  EXPECT_EQ(url, new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}
#endif  // !OS_CHROMEOS

namespace {

// Verifies that the given NavigationController has exactly two
// entries that correspond to the given URLs and that all entries have non-null
// timestamps.
void VerifyNavigationEntries(content::NavigationController& controller,
                             GURL url1,
                             GURL url2) {
  ASSERT_EQ(2, controller.GetEntryCount());
  EXPECT_EQ(1, controller.GetCurrentEntryIndex());
  EXPECT_EQ(url1, controller.GetEntryAtIndex(0)->GetURL());
  EXPECT_EQ(url2, controller.GetEntryAtIndex(1)->GetURL());
  EXPECT_FALSE(controller.GetEntryAtIndex(0)->GetTimestamp().is_null());
  EXPECT_FALSE(controller.GetEntryAtIndex(1)->GetTimestamp().is_null());
}

}  // namespace

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreForeignTab) {
  GURL url1("http://google.com");
  GURL url2("http://google2.com");

  // Set up the restore data.
  sessions::SessionTab tab;
  tab.tab_visual_index = 0;
  tab.current_navigation_index = 1;
  tab.pinned = false;
  tab.navigations.push_back(
      ContentTestHelper::CreateNavigation(url1.spec(), "one"));
  tab.navigations.push_back(
      ContentTestHelper::CreateNavigation(url2.spec(), "two"));

  for (size_t i = 0; i < tab.navigations.size(); ++i) {
    ASSERT_FALSE(tab.navigations[i].timestamp().is_null());
    tab.navigations[i].set_index(i);
    tab.navigations[i].set_encoded_page_state("");
  }

  ASSERT_EQ(1, browser()->tab_strip_model()->count());

  // Restore in the current tab.
  content::WebContents* tab_content = NULL;
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    tab_content = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab,
        WindowOpenDisposition::CURRENT_TAB);
    observer.Wait();
  }
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().ua_string_override.empty());
  ASSERT_TRUE(tab_content);
  ASSERT_EQ(url2, tab_content->GetURL());

  // Restore in a new tab.
  tab_content = NULL;
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    tab_content = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab,
        WindowOpenDisposition::NEW_BACKGROUND_TAB);
    observer.Wait();
  }
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  web_contents = browser()->tab_strip_model()->GetWebContentsAt(1);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().ua_string_override.empty());
  ASSERT_TRUE(tab_content);
  ASSERT_EQ(url2, tab_content->GetURL());

  // Restore in a new window.
  Browser* new_browser = NULL;
  tab_content = NULL;
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    tab_content = SessionRestore::RestoreForeignSessionTab(
        browser()->tab_strip_model()->GetActiveWebContents(), tab,
        WindowOpenDisposition::NEW_WINDOW);
    observer.Wait();
    new_browser = BrowserList::GetInstance()->GetLastActive();
    EXPECT_NE(new_browser, browser());
  }

  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  web_contents = new_browser->tab_strip_model()->GetWebContentsAt(0);
  VerifyNavigationEntries(web_contents->GetController(), url1, url2);
  ASSERT_TRUE(web_contents->GetUserAgentOverride().ua_string_override.empty());
  ASSERT_TRUE(tab_content);
  ASSERT_EQ(url2, tab_content->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreForeignSession) {
  Profile* profile = browser()->profile();

  GURL url1("http://google.com");
  GURL url2("http://google2.com");
  SerializedNavigationEntry nav1 =
      ContentTestHelper::CreateNavigation(url1.spec(), "one");
  SerializedNavigationEntry nav2 =
      ContentTestHelper::CreateNavigation(url2.spec(), "two");
  SerializedNavigationEntryTestHelper::SetIsOverridingUserAgent(true, &nav2);

  // Set up the restore data -- one window with two tabs.
  std::vector<const sessions::SessionWindow*> session;
  sessions::SessionWindow window;
  {
    auto tab1 = std::make_unique<sessions::SessionTab>();
    tab1->tab_visual_index = 0;
    tab1->current_navigation_index = 0;
    tab1->pinned = true;
    tab1->navigations.push_back(
        ContentTestHelper::CreateNavigation(url1.spec(), "one"));
    window.tabs.push_back(std::move(tab1));
  }

  {
    auto tab2 = std::make_unique<sessions::SessionTab>();
    tab2->tab_visual_index = 1;
    tab2->current_navigation_index = 0;
    tab2->pinned = false;
    tab2->navigations.push_back(
        ContentTestHelper::CreateNavigation(url2.spec(), "two"));
    window.tabs.push_back(std::move(tab2));
  }

  // Leave a third tab empty. Should have no effect on restored session, but
  // simulates partially complete foreign session data.
  window.tabs.push_back(std::make_unique<sessions::SessionTab>());

  session.push_back(static_cast<const sessions::SessionWindow*>(&window));
  std::vector<Browser*> browsers = SessionRestore::RestoreForeignSessionWindows(
      profile, session.begin(), session.end());
  ASSERT_EQ(1u, browsers.size());
  Browser* new_browser = browsers[0];
  ASSERT_TRUE(new_browser);
  EXPECT_NE(new_browser, browser());
  EXPECT_EQ(new_browser->profile(), browser()->profile());
  ASSERT_EQ(2u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());

  content::WebContents* web_contents_1 =
      new_browser->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* web_contents_2 =
      new_browser->tab_strip_model()->GetWebContentsAt(1);
  ASSERT_EQ(url1, web_contents_1->GetURL());
  ASSERT_EQ(url2, web_contents_2->GetURL());

  // Check user agent override state.
  ASSERT_TRUE(
      web_contents_1->GetUserAgentOverride().ua_string_override.empty());
  ASSERT_TRUE(
      web_contents_2->GetUserAgentOverride().ua_string_override.empty());

  content::NavigationEntry* entry =
      web_contents_1->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  ASSERT_FALSE(entry->GetIsOverridingUserAgent());

  entry = web_contents_2->GetController().GetLastCommittedEntry();
  ASSERT_TRUE(entry);
  ASSERT_FALSE(entry->GetIsOverridingUserAgent());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, Basic) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURL(browser(), GetUrl2());

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

namespace {

// Groups the tabs in |model| according to |specified_groups|.
void CreateTabGroups(TabStripModel* model,
                     base::span<const base::Optional<int>> specified_groups) {
  ASSERT_EQ(model->count(), static_cast<int>(specified_groups.size()));

  // Maps |specified_groups| IDs to actual group IDs in |model|.
  base::flat_map<int, tab_groups::TabGroupId> group_map;

  for (int i = 0; i < model->count(); ++i) {
    if (specified_groups[i] == base::nullopt)
      continue;

    const int specified_group = specified_groups[i].value();
    auto match = group_map.find(specified_group);

    // If |group_map| doesn't contain a value for |specified_group|, we can
    // assume we haven't created the group yet.
    if (match == group_map.end()) {
      const tab_groups::TabGroupId actual_group = model->AddToNewGroup({i});
      group_map.insert(std::make_pair(specified_group, actual_group));
    } else {
      const content::WebContents* const contents = model->GetWebContentsAt(i);
      model->AddToExistingGroup({i}, match->second);
      // Make sure we didn't move the tab.
      EXPECT_EQ(contents, model->GetWebContentsAt(i));
    }
  }
}

// Checks that the grouping of tabs in |model| is equivalent to that specified
// in |specified_groups| up to relabeling of the group IDs.
void CheckTabGrouping(TabStripModel* model,
                      base::span<const base::Optional<int>> specified_groups) {
  ASSERT_EQ(model->count(), static_cast<int>(specified_groups.size()));

  // Maps |specified_groups| IDs to actual group IDs in |model|.
  base::flat_map<int, tab_groups::TabGroupId> group_map;

  for (int i = 0; i < model->count(); ++i) {
    SCOPED_TRACE(i);

    const base::Optional<int> specified_group = specified_groups[i];
    const base::Optional<tab_groups::TabGroupId> actual_group =
        model->GetTabGroupForTab(i);

    // The tab should be grouped iff it's grouped in |specified_groups|.
    EXPECT_EQ(actual_group.has_value(), specified_group.has_value());

    if (actual_group.has_value() && specified_group.has_value()) {
      auto match = group_map.find(specified_group.value());
      if (match == group_map.end()) {
        group_map.insert(
            std::make_pair(specified_group.value(), actual_group.value()));
      } else {
        EXPECT_EQ(actual_group.value(), match->second);
      }
    }
  }
}

// Returns the optional group ID for each tab in a vector.
std::vector<base::Optional<tab_groups::TabGroupId>> GetTabGroups(
    const TabStripModel* model) {
  std::vector<base::Optional<tab_groups::TabGroupId>> result(model->count());
  for (int i = 0; i < model->count(); ++i)
    result[i] = model->GetTabGroupForTab(i);
  return result;
}

// Building session state from scratch and from an existing browser use
// different code paths. So, create a parametrized test fixture to run each test
// with and without a command reset. The bool test parameter determines whether
// to do a command reset when quitting and restoring.
class SessionRestoreTabGroupsTest : public SessionRestoreTest,
                                    public testing::WithParamInterface<bool> {
 public:
  SessionRestoreTabGroupsTest() = default;

 protected:
  void SetUpOnMainThread() override {
    SessionRestoreTest::SetUpOnMainThread();
  }

  Browser* QuitBrowserAndRestore(Browser* browser, int expected_tab_count) {
    // The test parameter determines whether to do a command reset.
    if (GetParam()) {
      SessionService* const session_service =
          SessionServiceFactory::GetForProfile(browser->profile());
      session_service->ResetFromCurrentBrowsers();
    }

    return SessionRestoreTest::QuitBrowserAndRestore(browser,
                                                     expected_tab_count);
  }

 private:
  base::test::ScopedFeatureList feature_override_;
};

}  // namespace

IN_PROC_BROWSER_TEST_P(SessionRestoreTabGroupsTest, TabsWithGroups) {
  constexpr int kNumTabs = 6;
  const std::array<base::Optional<int>, kNumTabs> group_spec = {
      0, 0, base::nullopt, base::nullopt, 1, 1};

  // Open |kNumTabs| tabs.
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  for (int i = 1; i < kNumTabs; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GetUrl1(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }
  ASSERT_EQ(kNumTabs, browser()->tab_strip_model()->count());

  CreateTabGroups(browser()->tab_strip_model(), group_spec);
  ASSERT_NO_FATAL_FAILURE(
      CheckTabGrouping(browser()->tab_strip_model(), group_spec));
  const auto groups = GetTabGroups(browser()->tab_strip_model());

  Browser* new_browser = QuitBrowserAndRestore(browser(), kNumTabs);
  ASSERT_EQ(kNumTabs, new_browser->tab_strip_model()->count());
  ASSERT_NO_FATAL_FAILURE(
      CheckTabGrouping(new_browser->tab_strip_model(), group_spec));
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTabGroupsTest, GroupMetadataRestored) {
  // Open up 4 more tabs, making 5 including the initial tab.
  for (int i = 0; i < 4; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GetUrl1(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  TabStripModel* const tsm = browser()->tab_strip_model();
  ASSERT_EQ(5, tsm->count());

  // Group the first 2 and second 2 tabs, making for 2 groups with 2 tabs and 1
  // ungrouped tab in the strip.
  const tab_groups::TabGroupId group1 = tsm->AddToNewGroup({0, 1});
  const tab_groups::TabGroupId group2 = tsm->AddToNewGroup({2, 3});

  // Get the default visual data for the first group and set custom visual data
  // for the second.
  const tab_groups::TabGroupVisualData group1_data =
      *tsm->group_model()->GetTabGroup(group1)->visual_data();
  const tab_groups::TabGroupVisualData group2_data(
      u"Foo", tab_groups::TabGroupColorId::kBlue, true);
  tsm->group_model()->GetTabGroup(group2)->SetVisualData(group2_data);

  Browser* const new_browser = QuitBrowserAndRestore(browser(), 5);
  TabStripModel* const new_tsm = new_browser->tab_strip_model();
  ASSERT_EQ(5, new_tsm->count());

  const base::Optional<tab_groups::TabGroupId> new_group1 =
      new_tsm->GetTabGroupForTab(0);
  const base::Optional<tab_groups::TabGroupId> new_group2 =
      new_tsm->GetTabGroupForTab(2);

  ASSERT_TRUE(new_group1);
  ASSERT_TRUE(new_group2);

  // Check that the restored visual data is the same.
  const tab_groups::TabGroupVisualData* const group1_restored_data =
      new_tsm->group_model()->GetTabGroup(*new_group1)->visual_data();
  const tab_groups::TabGroupVisualData* const group2_restored_data =
      new_tsm->group_model()->GetTabGroup(*new_group2)->visual_data();

  EXPECT_EQ(group1_data.title(), group1_restored_data->title());
  EXPECT_EQ(group1_data.color(), group1_restored_data->color());
  EXPECT_EQ(group1_data.is_collapsed(), group1_restored_data->is_collapsed());
  EXPECT_EQ(group2_data.title(), group2_restored_data->title());
  EXPECT_EQ(group2_data.color(), group2_restored_data->color());
  EXPECT_EQ(group2_data.is_collapsed(), group2_restored_data->is_collapsed());
}

IN_PROC_BROWSER_TEST_P(SessionRestoreTabGroupsTest,
                       TabGroupIDsRelabeledOnRestore) {
  constexpr int kNumTabs = 3;
  const std::array<base::Optional<int>, kNumTabs> group_spec = {0, 0, 1};

  // Open |kNumTabs| tabs.
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  for (int i = 1; i < kNumTabs; ++i) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GetUrl1(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }
  ASSERT_EQ(kNumTabs, browser()->tab_strip_model()->count());

  CreateTabGroups(browser()->tab_strip_model(), group_spec);
  ASSERT_NO_FATAL_FAILURE(
      CheckTabGrouping(browser()->tab_strip_model(), group_spec));
  const auto orig_groups = GetTabGroups(browser()->tab_strip_model());

  Browser* const new_browser = QuitBrowserAndRestore(browser(), 5);
  TabStripModel* const new_tsm = new_browser->tab_strip_model();
  ASSERT_EQ(kNumTabs, new_tsm->count());
  ASSERT_NO_FATAL_FAILURE(CheckTabGrouping(new_tsm, group_spec));
  const auto new_groups = GetTabGroups(new_tsm);

  for (int i = 0; i < kNumTabs; ++i) {
    SCOPED_TRACE(i);
    EXPECT_NE(orig_groups[i], new_groups[i]);
  }
}

INSTANTIATE_TEST_SUITE_P(WithAndWithoutReset,
                         SessionRestoreTabGroupsTest,
                         testing::Values(false, true));

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreAfterDelete) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURL(browser(), GetUrl2());
  ui_test_utils::NavigateToURL(browser(), GetUrl3());

  content::NavigationController& controller =
      browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  // Three urls and the NTP.
  EXPECT_EQ(4, controller.GetEntryCount());
  controller.DeleteNavigationEntries(
      base::BindLambdaForTesting([&](content::NavigationEntry* entry) {
        return entry->GetURL() == GetUrl2();
      }));
  EXPECT_EQ(3, controller.GetEntryCount());

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  content::NavigationController& new_controller =
      new_browser->tab_strip_model()->GetActiveWebContents()->GetController();
  EXPECT_EQ(3, new_controller.GetEntryCount());
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(GetUrl3(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, StartupPagesWithOnlyNtp) {
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  content::WebContentsDestroyedWatcher original_tab_destroyed_watcher(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls.push_back(GetUrl1());
  pref.urls.push_back(GetUrl2());
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  SessionRestore::OpenStartupPagesAfterCrash(browser());
  // Wait until the original tab finished closing.
  original_tab_destroyed_watcher.Wait();

  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetUrl1(),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GetUrl2(),
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, StartupPagesWithExistingPages) {
  ui_test_utils::NavigateToURL(browser(), GetUrl3());

  SessionStartupPref pref(SessionStartupPref::URLS);
  pref.urls.push_back(GetUrl1());
  pref.urls.push_back(GetUrl2());
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);

  SessionRestore::OpenStartupPagesAfterCrash(browser());

  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ(GetUrl3(),
            browser()->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GetUrl1(),
            browser()->tab_strip_model()->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(GetUrl2(),
            browser()->tab_strip_model()->GetWebContentsAt(2)->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, NoMemoryPressureLoadsAllTabs) {
  // Add several tabs to the browser. Restart the browser and check that all
  // tabs got loaded properly.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  Browser* restored =
      QuitBrowserAndRestoreWithURL(browser(), 1, GURL(), true);
  TabStripModel* tab_strip_model = restored->tab_strip_model();

  ASSERT_EQ(1u, active_browser_list_->size());

  ASSERT_EQ(3, tab_strip_model->count());
  // All render widgets should be initialized by now.
  ASSERT_TRUE(
      tab_strip_model->GetWebContentsAt(0)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(1)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(2)->GetRenderWidgetHostView());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, MemoryPressureLoadsNotAllTabs) {
  // Add several tabs to the browser. Restart the browser and check that all
  // tabs got loaded properly.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Restore the brwoser, but instead of directly waiting, we issue a critical
  // memory pressure event and finish then the loading.
  Browser* restored =
      QuitBrowserAndRestoreWithURL(browser(), 1, GURL(), false);

  TabStripModel* tab_strip_model = restored->tab_strip_model();

  ASSERT_EQ(1u, active_browser_list_->size());

  ASSERT_EQ(3, tab_strip_model->count());
  // At least one of the render widgets should not be initialized yet.
  ASSERT_FALSE(
      tab_strip_model->GetWebContentsAt(0)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(1)->GetRenderWidgetHostView() &&
      tab_strip_model->GetWebContentsAt(2)->GetRenderWidgetHostView());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreWebUI) {
  const GURL webui_url(chrome::kChromeUIOmniboxURL);
  ui_test_utils::NavigateToURL(browser(), webui_url);
  content::WebContents* old_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(content::BINDINGS_POLICY_MOJO_WEB_UI &
              old_tab->GetMainFrame()->GetEnabledBindings());

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  content::WebContents* new_tab =
      new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(webui_url, new_tab->GetURL());
  EXPECT_TRUE(content::BINDINGS_POLICY_MOJO_WEB_UI &
              new_tab->GetMainFrame()->GetEnabledBindings());
}

// http://crbug.com/803510 : Flaky on dbg and ASan bots.
#if defined(ADDRESS_SANITIZER) || !defined(NDEBUG)
#define MAYBE_RestoreWebUISettings DISABLED_RestoreWebUISettings
#else
#define MAYBE_RestoreWebUISettings RestoreWebUISettings
#endif
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, MAYBE_RestoreWebUISettings) {
  const GURL webui_url(chrome::kChromeUISettingsURL);
  ui_test_utils::NavigateToURL(browser(), webui_url);
  content::WebContents* old_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_TRUE(old_tab->GetMainFrame()->GetEnabledBindings() &
              content::BINDINGS_POLICY_WEB_UI);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  content::WebContents* new_tab =
      new_browser->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(webui_url, new_tab->GetURL());
  EXPECT_TRUE(new_tab->GetMainFrame()->GetEnabledBindings() &
              content::BINDINGS_POLICY_WEB_UI);
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoresForwardAndBackwardNavs) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURL(browser(), GetUrl2());
  ui_test_utils::NavigateToURL(browser(), GetUrl3());

  GoBack(browser());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoForward(new_browser);
  ASSERT_EQ(GetUrl3(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  GoBack(new_browser);
  ASSERT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Test renderer-initiated back/forward as well.
  GURL go_back_url("javascript:history.back();");
  ui_test_utils::NavigateToURL(new_browser, go_back_url);
  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Tests that the SiteInstances used for entries in a restored tab's history
// are given appropriate max page IDs, so that going back to a restored
// cross-site page and then forward again works.  (Bug 1204135)
// This test fails. See http://crbug.com/237497.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       DISABLED_RestoresCrossSiteForwardAndBackwardNavs) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL cross_site_url(embedded_test_server()->GetURL("/title2.html"));

  // Visit URLs on different sites.
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURL(browser(), cross_site_url);
  ui_test_utils::NavigateToURL(browser(), GetUrl2());

  GoBack(browser());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());

  // Check that back and forward work as expected.
  ASSERT_EQ(cross_site_url,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  GoBack(new_browser);
  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  GoForward(new_browser);
  ASSERT_EQ(cross_site_url,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  // Test renderer-initiated back/forward as well.
  GURL go_forward_url("javascript:history.forward();");
  ui_test_utils::NavigateToURL(new_browser, go_forward_url);
  ASSERT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, TwoTabsSecondSelected) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 2);

  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(1, new_browser->tab_strip_model()->active_index());
  ASSERT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());

  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, OnErrorWritingSessionCommands) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  auto* session_service =
      SessionServiceFactory::GetForProfile(browser()->profile());
  session_service->OnErrorWritingSessionCommands();

  Browser* new_browser = QuitBrowserAndRestore(browser(), 2);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(1, new_browser->tab_strip_model()->active_index());
  ASSERT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
}

// Creates two tabs, closes one, quits and makes sure only one tab is restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ClosedTabStaysClosed) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  chrome::CloseTab(browser());

  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);

  AssertOneWindowWithOneTab(new_browser);
  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Closes the one and only tab and verifies it is not restored.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, CloseSingleTabRestoresNothing) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());

  Profile* profile = browser()->profile();
  std::unique_ptr<ScopedKeepAlive> keep_alive(new ScopedKeepAlive(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED));

  chrome::CloseTab(browser());
  ui_test_utils::WaitForBrowserToClose(browser());

  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  SessionRestoreTestHelper restore_observer;

  // Ensure the session service factory is started, even if it was explicitly
  // shut down.
  SessionServiceTestHelper helper(profile);
  helper.SetForceBrowserNotAliveWithNoWindows(true);

  chrome::NewEmptyWindow(profile);

  Browser* new_browser = chrome::FindBrowserWithWebContents(tab_waiter.Wait());

  restore_observer.Wait();
  WaitForTabsToLoad(new_browser);

  keep_alive.reset();

  AssertOneWindowWithOneTab(new_browser);
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Verifies that launching with no previous session to a url which closes itself
// results in no session being restored on the next launch.
// Regression test for http://crbug.com/1052096
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       AutoClosedSingleTabDoesNotGetRestored) {
  Profile* profile = browser()->profile();
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // First close the original browser to clear the session information (as
  // verified by CloseSingleTabRestoresNothing).
  chrome::CloseTab(browser());
  ui_test_utils::WaitForBrowserToClose(browser());

  SessionRestoreTestHelper restore_observer;

  // Ensure the session service factory is started, even if it was explicitly
  // shut down.
  SessionServiceTestHelper helper(profile);
  helper.SetForceBrowserNotAliveWithNoWindows(true);

  // Create a new browser by navigating to the test page.
  GURL url = ui_test_utils::GetTestUrl(
      base::FilePath(base::FilePath::kCurrentDirectory),
      base::FilePath(FILE_PATH_LITERAL("title1.html")));
  NavigateParams params(profile, url, ui::PAGE_TRANSITION_LINK);
  Navigate(&params);

  restore_observer.Wait();
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  ui_test_utils::BrowserChangeObserver browser_removed_observer(
      params.browser,
      ui_test_utils::BrowserChangeObserver::ChangeType::kRemoved);

  // Have the page trigger closing the browser.
  ASSERT_TRUE(
      content::ExecJs(params.browser->tab_strip_model()->GetActiveWebContents(),
                      "window.open('', '_self').close()"));

  // Wait for the browser to close as a result of the single tab closing
  // itself.
  browser_removed_observer.Wait();

  ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;
  SessionRestoreTestHelper restore_observer2;

  // Create a new browser from scratch and verify the tab is not restored.
  chrome::NewEmptyWindow(profile);

  Browser* new_browser = chrome::FindBrowserWithWebContents(tab_waiter.Wait());

  restore_observer2.Wait();
  WaitForTabsToLoad(new_browser);

  keep_alive.reset();
  profile_keep_alive.reset();

  AssertOneWindowWithOneTab(new_browser);
  EXPECT_EQ(chrome::kChromeUINewTabURL,
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Ensures active tab properly restored when tabs before it closed.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ActiveIndexUpdatedAtClose) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl3(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  browser()->tab_strip_model()->CloseWebContentsAt(
      0,
      TabStripModel::CLOSE_CREATE_HISTORICAL_TAB);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 2);

  ASSERT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  ASSERT_EQ(new_browser->tab_strip_model()->active_index(), 0);
}

// Ensures active tab properly restored when tabs are inserted before it .
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ActiveIndexUpdatedAtInsert) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  NavigateParams navigate_params(browser(), GetUrl3(),
                                 ui::PAGE_TRANSITION_TYPED);
  navigate_params.tabstrip_index = 0;
  navigate_params.disposition = WindowOpenDisposition::NEW_BACKGROUND_TAB;
  ui_test_utils::NavigateToURL(&navigate_params);

  Browser* new_browser = QuitBrowserAndRestore(browser(), 3);

  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  ASSERT_EQ(new_browser->tab_strip_model()->active_index(), 1);
}

#if !defined(OS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS) && \
    !BUILDFLAG(IS_CHROMEOS_ASH)
// This test doesn't apply to Mac or Lacros; see GetCommandLineForRelaunch
// for details. It was disabled for a long time so might never have worked on
// Chrome OS Ash.

// Launches an app window, closes tabbed browser, launches and makes sure
// we restore the tabbed browser url.
// If this test flakes, use http://crbug.com/29110
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       RestoreAfterClosingTabbedBrowserWithAppAndLaunching) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());

  // Launch an app.
  base::CommandLine app_launch_arguments = GetCommandLineForRelaunch();
  app_launch_arguments.AppendSwitchASCII(switches::kApp, GetUrl2().spec());

  base::LaunchProcess(app_launch_arguments, base::LaunchOptionsForTest());
  Browser* app_window = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_EQ(2u, active_browser_list_->size());

  // Close the first window. The only window left is the App window.
  CloseBrowserSynchronously(browser());

  // Restore the session, which should bring back the first window with
  // GetUrl1().
  Browser* new_browser = QuitBrowserAndRestore(app_window, 1);

  AssertOneWindowWithOneTab(new_browser);

  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

#endif  // !!defined(OS_MAC) && !BUILDFLAG(IS_CHROMEOS_LACROS) &&
        // !BUILDFLAG(IS_CHROMEOS_ASH)

// Creates two windows, closes one, restores, make sure only one window open.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, TwoWindowsCloseOneRestoreOnlyOne) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());

  // Open a second window.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL), WindowOpenDisposition::NEW_WINDOW,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_BROWSER);

  ASSERT_EQ(2u, active_browser_list_->size());

  // Close it.
  Browser* new_window = active_browser_list_->get(1);
  CloseBrowserSynchronously(new_window);

  // Restart and make sure we have only one window with one tab and the url
  // is GetUrl1().
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);

  AssertOneWindowWithOneTab(new_browser);

  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Make sure after a restore the number of processes matches that of the number
// of processes running before the restore. This creates a new tab so that
// we should have two new tabs running.  (This test will pass in both
// process-per-site and process-per-site-instance, because we treat the new tab
// as a special case in process-per-site-instance so that it only ever uses one
// process.)
//
// Flaky: http://code.google.com/p/chromium/issues/detail?id=52022
// Unfortunately, the fix at http://codereview.chromium.org/6546078
// breaks NTP background image refreshing, so ThemeSource had to revert to
// replacing the existing data source.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ShareProcessesOnRestore) {
  // Create two new tabs.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  int expected_process_count = RenderProcessHostCount();

  // Restart.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 3);

  ASSERT_EQ(3, new_browser->tab_strip_model()->count());

  ASSERT_EQ(expected_process_count, RenderProcessHostCount());
}

// Test that changing the user agent override will persist it to disk.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, PersistAndRestoreUserAgentOverride) {
  // Create a tab with an overridden user agent.
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  blink::UserAgentOverride ua_override;
  ua_override.ua_string_override = "override";
  ua_override.ua_metadata_override.emplace();
  ua_override.ua_metadata_override->brand_version_list.emplace_back("Overrider",
                                                                    "0");
  browser()->tab_strip_model()->GetWebContentsAt(0)->SetUserAgentOverride(
      ua_override, false);

  // Create a tab without an overridden user agent.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());

  // Kill the original browser then open a new one to trigger a restore.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(1, new_browser->tab_strip_model()->active_index());

  // Confirm that the user agent overrides are properly set.
  blink::UserAgentOverride over0 = new_browser->tab_strip_model()
                                       ->GetWebContentsAt(0)
                                       ->GetUserAgentOverride();
  EXPECT_EQ("override", over0.ua_string_override);
  ASSERT_TRUE(over0.ua_metadata_override.has_value());
  EXPECT_TRUE(over0.ua_metadata_override == ua_override.ua_metadata_override);

  blink::UserAgentOverride over1 = new_browser->tab_strip_model()
                                       ->GetWebContentsAt(1)
                                       ->GetUserAgentOverride();
  EXPECT_EQ(std::string(), over1.ua_string_override);
  EXPECT_FALSE(over1.ua_metadata_override.has_value());
}

// Regression test for crbug.com/125958. When restoring a pinned selected tab in
// a setting where there are existing tabs, the selected index computation was
// wrong, leading to the wrong tab getting selected, DCHECKs firing, and the
// pinned tab not getting loaded.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestorePinnedSelectedTab) {
  // Create a pinned tab.
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  browser()->tab_strip_model()->SetTabPinned(0, true);
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  // Create a nonpinned tab.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  // Select the pinned tab.
  browser()->tab_strip_model()->ActivateTabAt(
      0, {TabStripModel::GestureType::kOther});
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  Profile* profile = browser()->profile();

  // This will also initiate a session restore, but we're not interested in it.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(0, new_browser->tab_strip_model()->active_index());
  // Close the pinned tab.
  chrome::CloseTab(new_browser);
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  ASSERT_EQ(0, new_browser->tab_strip_model()->active_index());
  // Use the existing tab to navigate away, so that we can verify it was really
  // clobbered.
  ui_test_utils::NavigateToURL(new_browser, GetUrl3());

  // Restore the session again, clobbering the existing tab.
  SessionRestore::RestoreSession(
      profile, new_browser,
      SessionRestore::CLOBBER_CURRENT_TAB | SessionRestore::SYNCHRONOUS,
      std::vector<GURL>());

  // The pinned tab is the selected tab.
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  EXPECT_EQ(0, new_browser->tab_strip_model()->active_index());
  EXPECT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  EXPECT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetWebContentsAt(1)->GetURL());
}

// Regression test for crbug.com/240156. When restoring tabs with a navigation,
// the navigation should take active tab focus.
// Flaky on Mac. http://crbug.com/656211.
#if defined(OS_MAC)
#define MAYBE_RestoreWithNavigateSelectedTab \
  DISABLED_RestoreWithNavigateSelectedTab
#else
#define MAYBE_RestoreWithNavigateSelectedTab RestoreWithNavigateSelectedTab
#endif
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       MAYBE_RestoreWithNavigateSelectedTab) {
  // Create 2 tabs.
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restore the session by calling chrome::Navigate().
  Browser* new_browser =
      QuitBrowserAndRestoreWithURL(browser(), 3, GetUrl3(), true);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(3, new_browser->tab_strip_model()->count());
  // Navigated url should be the active tab.
  ASSERT_EQ(GetUrl3(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

// Ensure that AUTO_SUBFRAME navigations in subframes are restored.
// See https://crbug.com/638088.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreAfterAutoSubframe) {
  // Load a page with a blank iframe, then navigate the iframe.  This will be an
  // auto-subframe commit, and we expect it to be restored.
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL main_url(embedded_test_server()->GetURL("/iframe_blank.html"));
  GURL subframe_url(embedded_test_server()->GetURL("/title1.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);
  {
    content::TestNavigationObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    std::string nav_frame_script =
        content::JsReplace("frames[0].location.href = $1;", subframe_url);
    ASSERT_TRUE(
        content::ExecJs(browser()->tab_strip_model()->GetActiveWebContents(),
                        nav_frame_script));
    observer.Wait();
  }

  // Restore the session.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());

  // The restored page should have the right iframe.
  content::WebContents* new_web_contents =
      new_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_EQ(main_url, new_web_contents->GetURL());
  EXPECT_EQ(subframe_url.possibly_invalid_spec(),
            content::EvalJs(new_web_contents, "frames[0].location.href"));
}

// Do a clobber restore from the new tab page. This test follows the code path
// of a crash followed by the user clicking restore from the new tab page.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, ClobberRestoreTest) {
  // Create 2 tabs.
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  ASSERT_EQ(0, browser()->tab_strip_model()->active_index());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetUrl2(), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_EQ(1, browser()->tab_strip_model()->active_index());
  Profile* profile = browser()->profile();

  // This will also initiate a session restore, but we're not interested in it.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  ASSERT_EQ(1, new_browser->tab_strip_model()->active_index());
  // Close the first tab.
  chrome::CloseTab(new_browser);
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  ASSERT_EQ(0, new_browser->tab_strip_model()->active_index());
  // Use the existing tab to navigate to the NTP.
  ui_test_utils::NavigateToURL(new_browser, GURL(chrome::kChromeUINewTabURL));
  content::WebContentsDestroyedWatcher existing_tab_destroyed_watcher(
      new_browser->tab_strip_model()->GetWebContentsAt(0));

  // Restore the session again, clobbering the existing tab.
  SessionRestore::RestoreSession(
      profile, new_browser,
      SessionRestore::CLOBBER_CURRENT_TAB | SessionRestore::SYNCHRONOUS,
      std::vector<GURL>());

  // Wait until the existing tab finished closing.
  existing_tab_destroyed_watcher.Wait();

  // 2 tabs should have been restored, with the existing tab clobbered, giving
  // us a total of 2 tabs.
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  EXPECT_EQ(1, new_browser->tab_strip_model()->active_index());
  EXPECT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GetUrl2(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, SessionStorage) {
  ui_test_utils::NavigateToURL(browser(), GetUrl1());
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  ASSERT_TRUE(controller->GetDefaultSessionStorageNamespace());
  std::string session_storage_id =
      controller->GetDefaultSessionStorageNamespace()->id();
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(GetUrl1(),
            new_browser->tab_strip_model()->GetActiveWebContents()->GetURL());
  content::NavigationController* new_controller =
      &new_browser->tab_strip_model()->GetActiveWebContents()->GetController();
  ASSERT_TRUE(new_controller->GetDefaultSessionStorageNamespace());
  std::string restored_session_storage_id =
      new_controller->GetDefaultSessionStorageNamespace()->id();
  EXPECT_EQ(session_storage_id, restored_session_storage_id);
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, SessionStorageAfterTabReplace) {
  // Simulate what prerendering does: create a new WebContents with the same
  // SessionStorageNamespace as an existing tab, then replace the tab with it.
  {
    content::NavigationController* controller =
        &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
    ASSERT_TRUE(controller->GetDefaultSessionStorageNamespace());

    content::SessionStorageNamespaceMap session_storage_namespace_map =
        content::CreateMapWithDefaultSessionStorageNamespace(
            browser()->profile(),
            controller->GetDefaultSessionStorageNamespace());

    std::unique_ptr<content::WebContents> web_contents(
        content::WebContents::CreateWithSessionStorage(
            content::WebContents::CreateParams(browser()->profile()),
            session_storage_namespace_map));

    TabStripModel* tab_strip_model = browser()->tab_strip_model();
    std::unique_ptr<content::WebContents> old_web_contents =
        tab_strip_model->ReplaceWebContentsAt(tab_strip_model->active_index(),
                                              std::move(web_contents));
    // Navigate with the new tab.
    ui_test_utils::NavigateToURL(browser(), GetUrl2());
    // old_web_contents goes out of scope.
  }

  // Check that the sessionStorage data is going to be persisted.
  content::NavigationController* controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  EXPECT_TRUE(
      controller->GetDefaultSessionStorageNamespace()->should_persist());

  // Quit and restore. Check that no extra tabs were created.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  EXPECT_EQ(1, new_browser->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest, TabWithDownloadDoesNotGetRestored) {
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(browser()->is_type_normal());

  GURL first_download_url =
      embedded_test_server()->GetURL("/downloads/a_zip_file.zip");

  {
    content::DownloadTestObserverTerminal observer(
        content::BrowserContext::GetDownloadManager(browser()->profile()), 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);
    ui_test_utils::NavigateToURL(browser(), first_download_url);
    observer.WaitForFinished();

    ASSERT_EQ(1, browser()->tab_strip_model()->count());
  }

  {
    content::DownloadManager* download_manager =
        content::BrowserContext::GetDownloadManager(browser()->profile());
    content::DownloadTestObserverInProgress in_progress_counter(
        download_manager, 2);
    content::DownloadTestObserverTerminal observer(
        download_manager, 1,
        content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);

    Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
    ASSERT_EQ(1, new_browser->tab_strip_model()->count());

    // In addition to restarting the browser, create a new download in a new
    // tab. If session restore erroneously created a new download, then its
    // initiation task chain should strictly precede the task chain for the new
    // download initiated here. While the download termination is asynchronous,
    // the erroneous download should enter the IN_PROGRESS state prior to the
    // second download reaching COMPLETE.
    //
    // Hence verifying that there was only one IN_PROGRESS download by the time
    // the new download completes ensures that there is no second download.
    GURL second_download_url =
        embedded_test_server()->GetURL("/downloads/image-octet-stream.png");
    ui_test_utils::NavigateToURLWithDisposition(
        new_browser, second_download_url,
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    ASSERT_EQ(2, new_browser->tab_strip_model()->count());

    observer.WaitForFinished();
    EXPECT_EQ(1u, in_progress_counter.NumDownloadsSeenInState(
                      download::DownloadItem::IN_PROGRESS));
    EXPECT_EQ(
        1u, observer.NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

    // We still need to verify that the second download that completed above is
    // the new one that we initiated. This would be true iff the DownloadManager
    // has exactly two downloads and they correspond to |first_download_url| and
    // |second_download_url|.
    std::vector<download::DownloadItem*> downloads;
    download_manager->GetAllDownloads(&downloads);
    ASSERT_EQ(2u, downloads.size());
    std::set<GURL> download_urls{downloads[0]->GetURL(),
                                 downloads[1]->GetURL()};
    std::set<GURL> expected_urls{first_download_url, second_download_url};
    EXPECT_EQ(expected_urls, download_urls);
  }
}

// Test is flaky on Linux and Windows: https://crbug.com/1181867
#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_LINUX) && !defined(OS_WIN)
namespace {

class MultiBrowserObserver : public BrowserListObserver {
 public:
  enum class Event {
    kAdded,
    kRemoved,
  };
  MultiBrowserObserver(size_t num_expected, Event event)
      : num_expected_(num_expected), event_(event) {
    BrowserList::AddObserver(this);
  }
  ~MultiBrowserObserver() override { BrowserList::RemoveObserver(this); }

  // Note that the returned pointers might no longer be valid (because the
  // Browser objects were closed).
  std::vector<Browser*> Wait() {
    run_loop_.Run();
    return browsers_;
  }

  // BrowserListObserver implementation.
  void OnBrowserAdded(Browser* browser) override {
    if (event_ == Event::kAdded) {
      browsers_.push_back(browser);
      if (--num_expected_ == 0)
        run_loop_.Quit();
    }
  }
  void OnBrowserRemoved(Browser* browser) override {
    if (event_ == Event::kRemoved) {
      browsers_.push_back(browser);
      if (--num_expected_ == 0)
        run_loop_.Quit();
    }
  }

 private:
  size_t num_expected_;
  Event event_;
  std::vector<Browser*> browsers_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(MultiBrowserObserver);
};

}  // namespace

// Test that when closing a profile with multiple browsers, all browsers are
// restored when the profile is reopened.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest, RestoreAllBrowsers) {
  // Create two profiles with two browsers each.
  Browser* first_profile_browser_one = browser();
  chrome::NewWindow(first_profile_browser_one);
  Browser* first_profile_browser_two =
      BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(first_profile_browser_one, first_profile_browser_two);

  Profile* second_profile = CreateSecondaryProfile(1);
  base::FilePath second_profile_path = second_profile->GetPath();
  profiles::FindOrCreateNewWindowForProfile(
      second_profile, chrome::startup::IS_NOT_PROCESS_STARTUP,
      chrome::startup::IS_NOT_FIRST_RUN, false);
  Browser* second_profile_browser_one = ui_test_utils::WaitForBrowserToOpen();
  chrome::NewWindow(second_profile_browser_one);
  Browser* second_profile_browser_two =
      BrowserList::GetInstance()->GetLastActive();
  EXPECT_NE(second_profile_browser_one, second_profile_browser_two);

  // Navigate the tab in each browser to a unique URL we can later reidentify.
  ui_test_utils::NavigateToURL(first_profile_browser_one,
                               GURL("data:,profile 1 browser 1"));
  ui_test_utils::NavigateToURL(first_profile_browser_two,
                               GURL("data:,profile 1 browser 2"));
  ui_test_utils::NavigateToURL(second_profile_browser_one,
                               GURL("data:,profile 2 browser 1"));
  ui_test_utils::NavigateToURL(second_profile_browser_two,
                               GURL("data:,profile 2 browser 2"));

  // Double-check preconditions.
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_EQ(profile_manager->GetNumberOfProfiles(), 2u);
  ASSERT_EQ(chrome::GetTotalBrowserCount(), 4u);

  // Close all profiles associated with the second profile.
  MultiBrowserObserver removed_observer(2,
                                        MultiBrowserObserver::Event::kRemoved);
  BrowserList::GetInstance()->CloseAllBrowsersWithProfile(
      second_profile, BrowserList::CloseCallback(),
      BrowserList::CloseCallback(), false);
  removed_observer.Wait();

  // The second profile should have no browsers anymore at this point.
  ASSERT_EQ(chrome::FindBrowserWithProfile(second_profile), nullptr);
  ASSERT_EQ(chrome::GetTotalBrowserCount(), 2u);

  // Clean up now stale pointers.
  second_profile_browser_one = nullptr;
  second_profile_browser_two = nullptr;
  second_profile = nullptr;  // See DestroyProfileOnBrowserClose flag.

  // Reopen the second profile and trigger session restore.
  MultiBrowserObserver added_observer(2, MultiBrowserObserver::Event::kAdded);
  profiles::SwitchToProfile(second_profile_path, false, {});
  std::vector<Browser*> browsers = added_observer.Wait();

  // Verify that the correct URLs where restored.
  std::set<GURL> expected_urls;
  expected_urls.insert(GURL("data:,profile 2 browser 1"));
  expected_urls.insert(GURL("data:,profile 2 browser 2"));
  for (Browser* browser : browsers) {
    WaitForTabsToLoad(browser);
    TabStripModel* tab_strip_model = browser->tab_strip_model();
    EXPECT_EQ(tab_strip_model->count(), 1);
    EXPECT_EQ(tab_strip_model->active_index(), 0);
    EXPECT_EQ(
        expected_urls.erase(tab_strip_model->GetActiveWebContents()->GetURL()),
        1u)
        << "Browser with unexpected URL "
        << tab_strip_model->GetActiveWebContents()
               ->GetURL()
               .possibly_invalid_spec();
  }
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// PRE_CorrectLoadingOrder is flaky on ChromeOS MSAN and Mac.
// See http://crbug.com/493167.
#if (BUILDFLAG(IS_CHROMEOS_ASH) && defined(MEMORY_SANITIZER)) || defined(OS_MAC)
#define MAYBE_PRE_CorrectLoadingOrder DISABLED_PRE_CorrectLoadingOrder
#define MAYBE_CorrectLoadingOrder DISABLED_CorrectLoadingOrder
#else
#define MAYBE_PRE_CorrectLoadingOrder PRE_CorrectLoadingOrder
#define MAYBE_CorrectLoadingOrder CorrectLoadingOrder
#endif
IN_PROC_BROWSER_TEST_F(SmartSessionRestoreTest, MAYBE_PRE_CorrectLoadingOrder) {
  Profile* profile = browser()->profile();

  const int activation_order[] = {4, 2, 1, 5, 0, 3};

  // Replace the first tab and add the other tabs.
  ui_test_utils::NavigateToURL(browser(), GURL(kUrls[0]));
  for (size_t i = 1; i < kExpectedNumTabs; i++) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), GURL(kUrls[i]), WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  ASSERT_EQ(static_cast<int>(kExpectedNumTabs),
            browser()->tab_strip_model()->count());

  // Activate the tabs one by one following the specified activation order.
  for (int i : activation_order)
    browser()->tab_strip_model()->ActivateTabAt(
        i, {TabStripModel::GestureType::kOther});

  // Close the browser.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser());

  StartObserving(kExpectedNumTabs);

  // Create a new window, which should trigger session restore.
  chrome::NewEmptyWindow(profile);
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(new_browser);
  WaitForAllTabsToStartLoading();
  keep_alive.reset();
  profile_keep_alive.reset();

  ASSERT_EQ(kExpectedNumTabs, web_contents().size());
  // Test that we have observed the tabs being loaded in the inverse order of
  // their activation (MRU). Also validate that their last active time is in the
  // correct order.
  for (size_t i = 0; i < web_contents().size(); i++) {
    GURL expected_url = GURL(kUrls[activation_order[kExpectedNumTabs - i - 1]]);
    ASSERT_EQ(expected_url, web_contents()[i]->GetLastCommittedURL());
    if (i > 0) {
      ASSERT_GT(web_contents()[i - 1]->GetLastActiveTime(),
                web_contents()[i]->GetLastActiveTime());
    }
  }

  // Activate the 2nd tab before the browser closes. This should be persisted in
  // the following test.
  new_browser->tab_strip_model()->ActivateTabAt(
      1, {TabStripModel::GestureType::kOther});
}

IN_PROC_BROWSER_TEST_F(SmartSessionRestoreTest, MAYBE_CorrectLoadingOrder) {
  const int activation_order[] = {4, 2, 5, 0, 3, 1};
  Profile* profile = browser()->profile();

  // Close the browser that gets opened automatically so we can track the order
  // of loading of the tabs.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);
  CloseBrowserSynchronously(browser());
  // We have an extra tab that is added when the test starts, which gets ignored
  // later when we test for proper order.
  StartObserving(kExpectedNumTabs + 1);

  // Create a new window, which should trigger session restore.
  chrome::NewEmptyWindow(profile);
  Browser* new_browser = ui_test_utils::WaitForBrowserToOpen();
  ASSERT_TRUE(new_browser);
  WaitForAllTabsToStartLoading();
  keep_alive.reset();
  profile_keep_alive.reset();

  ASSERT_EQ(kExpectedNumTabs + 1, web_contents().size());

  // Test that we have observed the tabs being loaded in the inverse order of
  // their activation (MRU). Also validate that their last active time is in the
  // correct order.
  //
  // Note that we ignore the first tab as it's an empty one that is added
  // automatically at the start of the test.
  for (size_t i = 1; i < web_contents().size(); i++) {
    GURL expected_url = GURL(kUrls[activation_order[kExpectedNumTabs - i]]);
    ASSERT_EQ(expected_url, web_contents()[i]->GetLastCommittedURL());
    if (i > 0) {
      ASSERT_GT(web_contents()[i - 1]->GetLastActiveTime(),
                web_contents()[i]->GetLastActiveTime());
    }
  }
}

IN_PROC_BROWSER_TEST_F(SessionRestoreWithURLInCommandLineTest,
                       PRE_TabWithURLFromCommandLineIsActive) {
  SessionStartupPref pref(SessionStartupPref::DEFAULT);
  SessionStartupPref::SetStartupPref(browser()->profile(), pref);
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // Add 3 pinned tabs.
  for (const auto& url : {GetUrl1(), GetUrl2(), GetUrl3()}) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
    tab_strip_model->SetTabPinned(tab_strip_model->active_index(), true);
  }
  EXPECT_EQ(4, tab_strip_model->count());
  EXPECT_EQ(3, tab_strip_model->IndexOfFirstNonPinnedTab());
}

IN_PROC_BROWSER_TEST_F(SessionRestoreWithURLInCommandLineTest,
                       TabWithURLFromCommandLineIsActive) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(4, tab_strip_model->count());
  EXPECT_EQ(3, tab_strip_model->active_index());
  EXPECT_EQ(command_line_url_,
            tab_strip_model->GetActiveWebContents()->GetURL());

  // Check that the all pinned tabs have been restored.
  EXPECT_EQ(3, tab_strip_model->IndexOfFirstNonPinnedTab());
  EXPECT_EQ(GetUrl1(), tab_strip_model->GetWebContentsAt(0)->GetURL());
  EXPECT_EQ(GetUrl2(), tab_strip_model->GetWebContentsAt(1)->GetURL());
  EXPECT_EQ(GetUrl3(), tab_strip_model->GetWebContentsAt(2)->GetURL());
}

class MultiOriginSessionRestoreTest : public SessionRestoreTest {
 public:
  MultiOriginSessionRestoreTest()
      : https_test_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    SessionRestoreTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    https_test_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    ASSERT_TRUE(https_test_server_.Start());
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  content::WebContents* GetTab(Browser* browser, int tab_index) {
    DCHECK_LT(tab_index, browser->tab_strip_model()->count());
    return browser->tab_strip_model()->GetWebContentsAt(tab_index);
  }

  std::string GetContent(Browser* browser, int tab_index) {
    return EvalJs(GetTab(browser, tab_index), "document.body.innerText")
        .ExtractString();
  }

  GURL GetSameOriginUrl(const std::string& path_and_query) {
    return https_test_server_.GetURL(path_and_query);
  }

  GURL GetCrossSiteUrl(const std::string& path_and_query) {
    return embedded_test_server()->GetURL("another.origin.example.com",
                                          path_and_query);
  }

 private:
  net::EmbeddedTestServer https_test_server_;
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(MultiOriginSessionRestoreTest);
};

// Test that Sec-Fetch-Site http request header is correctly replayed during
// session restore.  This is a regression test for https://crbug.com/976055.
IN_PROC_BROWSER_TEST_F(MultiOriginSessionRestoreTest, SecFetchSite) {
  GURL sec_fetch_url = GetSameOriginUrl("/echoheader?sec-fetch-site");

  // Tab #1: Same-origin navigation.
  ui_test_utils::NavigateToURL(browser(), GetSameOriginUrl("/title1.html"));
  {
    content::WebContents* tab1 = GetTab(browser(), 0);
    content::TestNavigationObserver nav_observer(tab1);
    ASSERT_TRUE(content::ExecJs(
        tab1, content::JsReplace("location = $1", sec_fetch_url)));
    nav_observer.Wait();
  }

  // Tab #2: Cross-site navigation.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GetCrossSiteUrl("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  {
    content::WebContents* tab2 = GetTab(browser(), 1);
    content::TestNavigationObserver nav_observer(tab2);
    ASSERT_TRUE(content::ExecJs(
        tab2, content::JsReplace("location = $1", sec_fetch_url)));
    nav_observer.Wait();
  }

  // Tab #3: Omnibox navigation.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), sec_fetch_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Verify that all the tabs have seen the expected Sec-Fetch-Site header.
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  EXPECT_EQ("same-origin", GetContent(browser(), 0));
  EXPECT_EQ("cross-site", GetContent(browser(), 1));
  EXPECT_EQ("none", GetContent(browser(), 2));

  // Kill the original browser then open a new one to trigger a restore.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());

  // Verify again (after session restore) that all the tabs have seen the
  // expected Sec-Fetch-Site header.  This is the main verification for
  // https://crbug.com/976055.
  ASSERT_EQ(3, new_browser->tab_strip_model()->count());
  EXPECT_EQ("same-origin", GetContent(new_browser, 0));
  EXPECT_EQ("cross-site", GetContent(new_browser, 1));
  EXPECT_EQ("none", GetContent(new_browser, 2));
}

// Test that it is possible to navigate back to a restored about:blank history
// entry with a non-null initiator origin.  This test cases covers the original
// repro steps reported in https://crbug.com/1026474.
//
// See also TabRestoreTest.BackToAboutBlank
IN_PROC_BROWSER_TEST_F(MultiOriginSessionRestoreTest, BackToAboutBlank1) {
  // Open about:blank in a new tab.
  GURL initial_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  url::Origin initial_origin = url::Origin::Create(initial_url);
  ui_test_utils::NavigateToURL(browser(), initial_url);
  content::WebContents* old_popup = nullptr;
  {
    content::WebContents* tab1 = GetTab(browser(), 0);
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ExecJs(tab1, "window.open('about:blank')"));
    old_popup = popup_observer.GetWebContents();
    EXPECT_EQ(GURL(url::kAboutBlankURL),
              old_popup->GetMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(initial_origin,
              old_popup->GetMainFrame()->GetLastCommittedOrigin());
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
  EXPECT_EQ(other_url, old_popup->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(other_origin, old_popup->GetMainFrame()->GetLastCommittedOrigin());
  ASSERT_TRUE(old_popup->GetController().CanGoBack());

  // Kill the original browser then open a new one to trigger a restore.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  content::WebContents* new_popup = GetTab(new_browser, 1);
  old_popup = nullptr;

  // Verify that the restored popup hosts |other_url|.
  EXPECT_EQ(other_url, new_popup->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(other_origin, new_popup->GetMainFrame()->GetLastCommittedOrigin());
  ASSERT_TRUE(new_popup->GetController().CanGoBack());

  // Navigate the popup back to about:blank.
  {
    content::TestNavigationObserver nav_observer(new_popup);
    new_popup->GetController().GoBack();
    nav_observer.Wait();
  }
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            new_popup->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(initial_origin,
            new_popup->GetMainFrame()->GetLastCommittedOrigin());
}

// Test that it is possible to navigate back to a restored about:blank history
// entry with a missing initiator origin.  Note that this scenario did not hit
// the CHECK from https://crbug.com/1026474, because the CHECK is/was skipped
// for opaque origins (which would be the case for about:blank with a missing
// initiator origin).
IN_PROC_BROWSER_TEST_F(MultiOriginSessionRestoreTest,
                       BackToAboutBlank1_Omnibox) {
  // Browser-initiated navigation to about:blank.
  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));
  content::WebContents* old_tab = GetTab(browser(), 0);
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            old_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_TRUE(old_tab->GetMainFrame()->GetLastCommittedOrigin().opaque());

  // Navigate the tab to another site.
  GURL other_url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  url::Origin other_origin = url::Origin::Create(other_url);
  {
    content::TestNavigationObserver nav_observer(old_tab);
    ASSERT_TRUE(content::ExecJs(
        old_tab, content::JsReplace("location = $1", other_url)));
    nav_observer.Wait();
  }
  EXPECT_EQ(other_url, old_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(other_origin, old_tab->GetMainFrame()->GetLastCommittedOrigin());
  ASSERT_TRUE(old_tab->GetController().CanGoBack());

  // Kill the original browser then open a new one to trigger a restore.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  content::WebContents* new_tab = GetTab(new_browser, 0);
  old_tab = nullptr;

  // Verify that the restored popup hosts |other_url|.
  EXPECT_EQ(other_url, new_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(other_origin, new_tab->GetMainFrame()->GetLastCommittedOrigin());
  ASSERT_TRUE(new_tab->GetController().CanGoBack());

  // Navigate the popup back to about:blank.
  {
    content::TestNavigationObserver nav_observer(new_tab);
    new_tab->GetController().GoBack();
    nav_observer.Wait();
  }
  EXPECT_EQ(GURL(url::kAboutBlankURL),
            new_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_TRUE(new_tab->GetMainFrame()->GetLastCommittedOrigin().opaque());
}

// Test that it is possible to navigate back to a restored about:blank history
// entry with a non-null initiator origin.  This test cases covers the variant
// of the repro that was reported in https://crbug.com/1016954#c27.
IN_PROC_BROWSER_TEST_F(MultiOriginSessionRestoreTest, BackToAboutBlank2) {
  // Open about:blank#foo in a new tab.
  //
  // Note that about:blank (rather than about:blank#foo) wouldn't work, because
  // about:blank is treated by the renderer-side as an initial, empty history
  // entry and replaced during the navigation to |other_url| below.
  GURL initial_url = embedded_test_server()->GetURL("foo.com", "/title1.html");
  url::Origin initial_origin = url::Origin::Create(initial_url);
  ui_test_utils::NavigateToURL(browser(), initial_url);
  {
    content::WebContents* tab1 = GetTab(browser(), 0);
    content::WebContentsAddedObserver popup_observer;
    ASSERT_TRUE(ExecJs(tab1, "window.open('about:blank#foo')"));
    content::WebContents* old_popup = popup_observer.GetWebContents();
    EXPECT_EQ(GURL("about:blank#foo"),
              old_popup->GetMainFrame()->GetLastCommittedURL());
    EXPECT_EQ(initial_origin,
              old_popup->GetMainFrame()->GetLastCommittedOrigin());
  }

  // Kill the original browser then open a new one to trigger a restore.
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(2, new_browser->tab_strip_model()->count());
  content::WebContents* new_popup = GetTab(new_browser, 1);

  // Verify that the restored popup hosts about:blank#foo.
  EXPECT_EQ(GURL("about:blank#foo"),
            new_popup->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(initial_origin,
            new_popup->GetMainFrame()->GetLastCommittedOrigin());

  // Navigate the popup to another site.
  GURL other_url = embedded_test_server()->GetURL("bar.com", "/title1.html");
  url::Origin other_origin = url::Origin::Create(other_url);
  {
    content::TestNavigationObserver nav_observer(new_popup);
    ASSERT_TRUE(content::ExecJs(
        new_popup, content::JsReplace("location = $1", other_url)));
    nav_observer.Wait();
  }
  EXPECT_EQ(other_url, new_popup->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(other_origin, new_popup->GetMainFrame()->GetLastCommittedOrigin());
  ASSERT_TRUE(new_popup->GetController().CanGoBack());

  // Navigate the popup back to about:blank#foo.
  {
    content::TestNavigationObserver nav_observer(new_popup);
    new_popup->GetController().GoBack();
    nav_observer.Wait();
  }
  EXPECT_EQ(GURL("about:blank#foo"),
            new_popup->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(initial_origin,
            new_popup->GetMainFrame()->GetLastCommittedOrigin());
}

// Test that it is possible to navigate back to a subframe with a restored
// about:blank history entry with a non-null initiator origin - see
// https://crbug.com/1026474.
IN_PROC_BROWSER_TEST_F(MultiOriginSessionRestoreTest,
                       BackToAboutBlankSubframe) {
  // Navigate to a.com(a.com/title2.html).
  GURL main_url = embedded_test_server()->GetURL(
      "a.com", "/frame_tree/page_with_samesite_frame.html");
  url::Origin a_origin = url::Origin::Create(main_url);
  GURL subframe_url = embedded_test_server()->GetURL("a.com", "/title2.html");
  ui_test_utils::NavigateToURL(browser(), main_url);
  content::WebContents* old_tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(main_url, old_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(a_origin, old_tab->GetMainFrame()->GetLastCommittedOrigin());
  ASSERT_EQ(2u, old_tab->GetAllFrames().size());
  content::RenderFrameHost* subframe = old_tab->GetAllFrames()[1];
  EXPECT_EQ(subframe_url, subframe->GetLastCommittedURL());
  EXPECT_EQ(a_origin, subframe->GetLastCommittedOrigin());

  // Have main frame initiate navigating the subframe to about:blank.
  // Expected state after the navigation: a.com(a.com-blank).
  {
    content::TestNavigationObserver nav_observer(old_tab);
    ASSERT_TRUE(
        content::ExecJs(old_tab, "window.open('about:blank', 'subframe');"));
    nav_observer.Wait();
  }
  ASSERT_EQ(2u, old_tab->GetAllFrames().size());
  EXPECT_EQ(subframe, old_tab->GetAllFrames()[1]);
  EXPECT_EQ(GURL(url::kAboutBlankURL), subframe->GetLastCommittedURL());
  EXPECT_EQ(a_origin, subframe->GetLastCommittedOrigin());

  // Have subframe (about:blank from a.com origin) navigate itself to c.com.
  // Expected state after the navigation: a.com(c.com).
  GURL c_url = embedded_test_server()->GetURL("c.com", "/title3.html");
  url::Origin c_origin = url::Origin::Create(c_url);
  {
    content::TestNavigationObserver nav_observer(old_tab);
    ASSERT_TRUE(
        content::ExecJs(subframe, content::JsReplace("location = $1;", c_url)));
    nav_observer.Wait();
  }
  ASSERT_EQ(2u, old_tab->GetAllFrames().size());
  subframe = old_tab->GetAllFrames()[1];
  EXPECT_EQ(c_url, subframe->GetLastCommittedURL());
  EXPECT_EQ(c_origin, subframe->GetLastCommittedOrigin());

  // Kill the original browser then open a new one to trigger a restore.
  ASSERT_TRUE(old_tab->GetController().CanGoBack());
  Browser* new_browser = QuitBrowserAndRestore(browser(), 1);
  ASSERT_EQ(1u, active_browser_list_->size());
  ASSERT_EQ(1, new_browser->tab_strip_model()->count());
  content::WebContents* new_tab =
      new_browser->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(new_tab->GetController().CanGoBack());
  old_tab = nullptr;

  // Verify that the restored tab hosts: a.com(c.com).
  EXPECT_EQ(main_url, new_tab->GetMainFrame()->GetLastCommittedURL());
  EXPECT_EQ(a_origin, new_tab->GetMainFrame()->GetLastCommittedOrigin());
  ASSERT_EQ(2u, new_tab->GetAllFrames().size());
  subframe = new_tab->GetAllFrames()[1];
  EXPECT_EQ(c_url, subframe->GetLastCommittedURL());
  EXPECT_EQ(c_origin, subframe->GetLastCommittedOrigin());

  // Check that main frame and subframe are in the same BrowsingInstance.
  content::SiteInstance* subframe_instance_c = subframe->GetSiteInstance();
  EXPECT_TRUE(new_tab->GetMainFrame()->GetSiteInstance()->IsRelatedSiteInstance(
      subframe_instance_c));

  // Go back - this should reach: a.com(a.com-blank).
  {
    content::TestNavigationObserver nav_observer(new_tab);
    new_tab->GetController().GoBack();
    nav_observer.Wait();
  }
  ASSERT_EQ(2u, new_tab->GetAllFrames().size());
  subframe = new_tab->GetAllFrames()[1];
  EXPECT_EQ(GURL(url::kAboutBlankURL), subframe->GetLastCommittedURL());
  EXPECT_EQ(a_origin, subframe->GetLastCommittedOrigin());

  // Check that we're still in the same BrowsingInstance.
  EXPECT_TRUE(new_tab->GetMainFrame()->GetSiteInstance()->IsRelatedSiteInstance(
      subframe->GetSiteInstance()));
  EXPECT_TRUE(
      subframe_instance_c->IsRelatedSiteInstance(subframe->GetSiteInstance()));
}

// Check that TabManager.TimeSinceTabClosedUntilRestored histogram is not
// recorded on session restore.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       TimeSinceTabClosedUntilRestoredNotRecorded) {
  base::HistogramTester histogram_tester;
  const char kTimeSinceTabClosedUntilRestored[] =
      "TabManager.TimeSinceTabClosedUntilRestored";

  // Add several tabs to the browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restart the browser and check that the metric is not recorded.
  EXPECT_EQ(
      histogram_tester.GetAllSamples(kTimeSinceTabClosedUntilRestored).size(),
      0U);

  QuitBrowserAndRestoreWithURL(browser(), 1, GURL(), true);

  EXPECT_EQ(
      histogram_tester.GetAllSamples(kTimeSinceTabClosedUntilRestored).size(),
      0U);
}

// Check that TabManager.TimeSinceWindowClosedUntilRestored histogram is not
// recorded on session restore.
IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       TimeSinceWindowClosedUntilRestoredNotRecorded) {
  base::HistogramTester histogram_tester;
  const char kTimeSinceWindowClosedUntilRestored[] =
      "TabManager.TimeSinceWindowClosedUntilRestored";

  // Add several tabs to the browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Restart the browser and check that the metric is not recorded.
  EXPECT_EQ(histogram_tester.GetAllSamples(kTimeSinceWindowClosedUntilRestored)
                .size(),
            0U);

  QuitBrowserAndRestoreWithURL(browser(), 1, GURL(), true);

  EXPECT_EQ(histogram_tester.GetAllSamples(kTimeSinceWindowClosedUntilRestored)
                .size(),
            0U);
}

// This class and tests are to verify reading a file with an error in it results
// in a restore and logging the error. The file was created from the test
// TwoTabsSecondSelected with an extra command at the end of the file that does
// not have the right amount of data in it. This should still result in a
// restore.
//
// The test is split in to two parts. An empty one whose sole purpose is to
// ensure on the second run the profile is not marked as new. This is necessary
// as the startup flow won't honor kRestoreLastSession for a new profile.
// The real test copies the file and verifies restore.
class SessionRestoreWithIncompleteFileTest : public InProcessBrowserTest {
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kRestoreLastSession);
  }
  bool SetUpUserDataDirectory() override {
    const bool result = InProcessBrowserTest::SetUpUserDataDirectory();
    if (!result)
      return false;

    // Copy a file over that has an incomplete command. The file should still
    // be read, but a read error should be logged.
    base::FilePath user_data_dir;
    EXPECT_TRUE(base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir));
#if BUILDFLAG(IS_CHROMEOS_ASH)
    const std::string profile_dir = chrome::kTestUserProfileDir;
#else
    const std::string profile_dir = chrome::kInitialProfile;
#endif
    base::FilePath sessions_dir = user_data_dir.AppendASCII(profile_dir);
    if (!base::DeletePathRecursively(
            sessions_dir.Append(sessions::kSessionsDirectory))) {
      ADD_FAILURE() << "Unable to delete sessions directory";
      return false;
    }
    if (!base::CreateDirectory(sessions_dir)) {
      ADD_FAILURE() << "Unable to create sessions directory";
      return false;
    }
    base::FilePath session_file_path = sessions_dir.Append(
        base::FilePath(sessions::kLegacyCurrentSessionFileName));
    base::FilePath data_dir;
    if (!base::PathService::Get(chrome::DIR_TEST_DATA, &data_dir)) {
      ADD_FAILURE() << "Unable to get data dir";
      return false;
    }
    base::FilePath bogus_file_path =
        data_dir.AppendASCII("sessions")
            .AppendASCII("file_with_incomplete_command");
    if (!base::CopyFile(bogus_file_path, session_file_path)) {
      ADD_FAILURE() << "Unable to copy file for test";
      return false;
    }
    return true;
  }
};

IN_PROC_BROWSER_TEST_F(SessionRestoreWithIncompleteFileTest,
                       PRE_LogsReadError) {
  // Does nothing as kRestoreLastSession is only used if the profile is not a
  // new (just created) profile. At this point the profile is new, but after the
  // restart and LogsReadError() runs it will no longer be new.
}

IN_PROC_BROWSER_TEST_F(SessionRestoreWithIncompleteFileTest, LogsReadError) {
  // The file that was copied over should result in two tabs with the second
  // selected (mirrors that of TwoTabsSecondSelected). It has one extra tab as
  // the startup path appends a url to the commandline, which triggers another
  // tab. The first two tabs come from the saved session file.
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());
  ASSERT_EQ(3, browser()->tab_strip_model()->count());
  // Use ExtractFileName() as the path written to the restore file is from a
  // local build.
  EXPECT_EQ("bot1.html", browser()
                             ->tab_strip_model()
                             ->GetWebContentsAt(0)
                             ->GetURL()
                             .ExtractFileName());
  EXPECT_EQ("bot2.html", browser()
                             ->tab_strip_model()
                             ->GetWebContentsAt(1)
                             ->GetURL()
                             .ExtractFileName());
  // The tab at index 2 is the one created by startup.

  // Ensure there is a restore event
  auto events = GetSessionServiceEvents(browser()->profile());
  for (auto iter = events.rbegin(); iter != events.rend(); ++iter) {
    // For normal shutdown (as this test triggers) kRestore should always occur
    // after kExit. This iterates in reverse, so that kRestore should occur
    // first.
    ASSERT_NE(SessionServiceEventLogType::kExit, iter->type);
    if (iter->type == SessionServiceEventLogType::kRestore) {
      EXPECT_EQ(1, iter->data.restore.encountered_error_reading);
      break;
    }
  }
}

IN_PROC_BROWSER_TEST_F(SessionRestoreTest,
                       SameDocumentNavigationWithNothingCommittedAfterRestore) {
  // The test sets this closure before each navigation to /sometimes-slow in
  // order to control the response for that navigation.
  content::SlowHttpResponse::GotRequestCallback got_slow_request;

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/sometimes-slow")
          return nullptr;
        DCHECK(got_slow_request)
            << "Set `got_slow_request` before each navigation request.";
        return std::make_unique<content::SlowHttpResponse>(
            std::move(got_slow_request));
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("/sometimes-slow");
  GURL url2 = embedded_test_server()->GetURL("/sometimes-slow#foo");

  content::WebContents* wc =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Successfully navigate to `url1`.
  got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
  EXPECT_TRUE(NavigateToURL(wc, url1));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // We perform session restore under memory pressure so the tab is not restored
  // in the background.
  Browser* new_browser = QuitBrowserAndRestoreWithURL(
      browser(), 2, GURL(), /*no_memory_pressure=*/false);
  wc = new_browser->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(2, new_browser->tab_strip_model()->count());
  EXPECT_NE(0, new_browser->tab_strip_model()->active_index());

  // Now we reactivate the tab. This brings the process back to life for the
  // current RenderFrameHost, but we prevent it from loading before we perform a
  // navigation.
  {
    base::RunLoop loop;
    got_slow_request =
        base::BindLambdaForTesting([&](base::OnceClosure start_response,
                                       base::OnceClosure finish_response) {
          // Never starts the response, but informs the test the request has
          // been received.
          base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
              FROM_HERE,
              base::BindOnce([](base::RunLoop* loop) { loop->Quit(); }, &loop),
              base::TimeDelta::FromSeconds(1));
          // loop.Quit();
        });
    new_browser->tab_strip_model()->ActivateTabAt(0);
    loop.Run();
  }
  // The navigation has not completed, but the renderer has come alive.
  EXPECT_TRUE(wc->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(wc->GetMainFrame()->GetLastCommittedURL().spec(), "");

  // Now try to navigate to `url2`. We're currently trying to load `url1` since
  // the above navigation will be delayed. Going to `url2` should be a
  // same-document navigation according to the urls alone. But it can't be since
  // the current frame host does not actually have a document loaded.
  content::NavigationHandleCommitObserver nav_observer(wc, url2);
  {
    content::NavigationController::LoadURLParams params(url2);
    params.transition_type = ui::PageTransitionFromInt(
        ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);

    got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
    wc->GetController().LoadURLWithParams(params);
  }
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_TRUE(nav_observer.has_committed());
  EXPECT_FALSE(nav_observer.was_same_document());
}

IN_PROC_BROWSER_TEST_F(
    SessionRestoreTest,
    SameDocumentHistoryNavigationWithNothingCommittedAfterRestore) {
  // The test sets this closure before each navigation to /sometimes-slow in
  // order to control the response for that navigation.
  content::SlowHttpResponse::GotRequestCallback got_slow_request;

  embedded_test_server()->RegisterRequestHandler(base::BindLambdaForTesting(
      [&](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url != "/sometimes-slow")
          return nullptr;
        DCHECK(got_slow_request)
            << "Set `got_slow_request` before each navigation request.";
        return std::make_unique<content::SlowHttpResponse>(
            std::move(got_slow_request));
      }));
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("/sometimes-slow");
  GURL url2 = embedded_test_server()->GetURL("/sometimes-slow#foo");

  content::WebContents* wc =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Successfully navigate to `url1`, then do a same-document navigation to
  // `url2`.
  got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
  EXPECT_TRUE(NavigateToURL(wc, url1));
  EXPECT_TRUE(NavigateToURL(wc, url2));

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // We perform session restore under memory pressure so the tab is not restored
  // in the background.
  Browser* new_browser = QuitBrowserAndRestoreWithURL(
      browser(), 2, GURL(), /*no_memory_pressure=*/false);
  wc = new_browser->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(2, new_browser->tab_strip_model()->count());
  EXPECT_NE(0, new_browser->tab_strip_model()->active_index());

  // Now we reactivate the tab. This brings the process back to life for the
  // current RenderFrameHost, but we prevent it from loading before we perform a
  // navigation.
  {
    base::RunLoop loop;
    got_slow_request =
        base::BindLambdaForTesting([&](base::OnceClosure start_response,
                                       base::OnceClosure finish_response) {
          // Never starts the response, but informs the test the request has
          // been received.
          loop.Quit();
        });
    new_browser->tab_strip_model()->ActivateTabAt(0);
    loop.Run();
  }
  // The navigation has not completed, but the renderer has come alive.
  EXPECT_TRUE(wc->GetMainFrame()->IsRenderFrameLive());
  EXPECT_EQ(wc->GetMainFrame()->GetLastCommittedURL().spec(), "");

  content::NavigationHandleCommitObserver back_observer(wc, url1);
  // Now try to go back. We're currently at `url2`, so going back to `url1`
  // should be a same-document history navigation according to the
  // NavigationEntry. But it can't be since the current frame host does not
  // actually have a document loaded.
  got_slow_request = content::SlowHttpResponse::FinishResponseImmediately();
  wc->GetController().GoBack();
  EXPECT_TRUE(WaitForLoadStop(wc));
  EXPECT_TRUE(back_observer.has_committed());
  EXPECT_FALSE(back_observer.was_same_document());
}

#if BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
class AppSessionRestoreTest : public SessionRestoreTest {
 public:
  AppSessionRestoreTest() = default;
  ~AppSessionRestoreTest() override = default;

 protected:
  void ShutdownServices(Profile* profile) {
    // Pretend to 'close the browser'.
    // Just shutdown the services as we would if the browser is shutting down
    // for real.
    AppSessionServiceFactory::ShutdownForProfile(profile);
    SessionServiceFactory::ShutdownForProfile(profile);
  }

  void StartupServices(Profile* profile) {
    // We need to start up the services again before restoring.
    AppSessionServiceFactory::GetForProfileForSessionRestore(profile);
    SessionServiceFactory::GetForProfileForSessionRestore(profile);
  }

  web_app::AppId InstallPWA(Profile* profile, const GURL& start_url) {
    auto web_app_info = std::make_unique<WebApplicationInfo>();
    web_app_info->start_url = start_url;
    web_app_info->scope = start_url.GetWithoutFilename();
    web_app_info->open_as_window = true;
    web_app_info->title = u"A Web App";
    return web_app::InstallWebApp(profile, std::move(web_app_info));
  }
};

// This is disabled on mac pending http://crbug.com/1194201
#if defined(OS_MAC)
#define MAYBE_BasicAppSessionRestore DISABLED_BasicAppSessionRestore
#else
#define MAYBE_BasicAppSessionRestore BasicAppSessionRestore
#endif
// A simple test that apps are being tracked by AppSessionService correctly.
// Open 1 app for a total of 1 app, 1 browser.
// Do a simulated shutdown and restore, check for 2 apps 2 browsers.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest, MAYBE_BasicAppSessionRestore) {
  Profile* profile = browser()->profile();

  auto example_url = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  web_app::AppId app_id = InstallPWA(profile, example_url);
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  EXPECT_TRUE(app_browser->is_type_app());

  // At this point, we have a browser and an app.
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  int apps = 0;
  int browsers = 0;
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_APP) {
      apps++;
      EXPECT_EQ(browser, app_browser);
    } else if (browser->type() == Browser::Type::TYPE_NORMAL) {
      browsers++;
      EXPECT_EQ(browser->tab_strip_model()->count(), 2);
      auto tab1_url = browser->tab_strip_model()->GetWebContentsAt(0)->GetURL();
      auto tab2_url = browser->tab_strip_model()->GetWebContentsAt(1)->GetURL();
      EXPECT_EQ(tab2_url, example_url2);
    }
  }
  EXPECT_EQ(browsers, 1);
  EXPECT_EQ(apps, 1);

  // Pretend to 'close the browser'.
  ShutdownServices(profile);

  // Now trigger a restore.
  // We need to start up the services again before restoring.
  StartupServices(profile);

  SessionRestore::RestoreSession(
      profile, nullptr,
      SessionRestore::SYNCHRONOUS | SessionRestore::RESTORE_APPS, {});

  // We should get +1 browser +1 app.
  // Ensure the apps are the same, and ensure the browsers are the same.
  ASSERT_EQ(4u, BrowserList::GetInstance()->size());
  apps = 0;
  browsers = 0;
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_APP) {
      EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(browser, app_id));
      apps++;
      auto url = browser->tab_strip_model()->GetWebContentsAt(0)->GetURL();
      EXPECT_EQ(url, example_url);
    } else if (browser->type() == Browser::Type::TYPE_NORMAL) {
      browsers++;
      // Every browser should look the same, with two tabs and example_url2
      // on the second tab.
      EXPECT_EQ(browser->tab_strip_model()->count(), 2);
      auto tab1_url = browser->tab_strip_model()->GetWebContentsAt(0)->GetURL();
      auto tab2_url = browser->tab_strip_model()->GetWebContentsAt(1)->GetURL();
      EXPECT_EQ(tab2_url, example_url2);
    }
  }
  EXPECT_EQ(browsers, 2);
  EXPECT_EQ(apps, 2);
}

// This is disabled on mac pending http://crbug.com/1194201
#if defined(OS_MAC)
#define MAYBE_IsolatedFromBrowserRestore DISABLED_IsolatedFromBrowserRestore
#else
#define MAYBE_IsolatedFromBrowserRestore IsolatedFromBrowserRestore
#endif
// This test ensures browser sessions are preserved no matter what happens to
// apps. In particular this is important when apps are still open when there are
// no browser windows open, then a browser reopens.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest,
                       MAYBE_IsolatedFromBrowserRestore) {
  Profile* profile = browser()->profile();
  auto example_url = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");

  // Add a tab so we can recognize if this browser gets restored correctly.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Open a PWA.
  web_app::AppId app_id = InstallPWA(profile, example_url);
  // App #1
  web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  // Close the browser.
  CloseBrowserSynchronously(browser());

  // Open a few more PWAs.
  web_app::LaunchWebAppBrowserAndWait(profile, app_id);  // App #2
  web_app::LaunchWebAppBrowserAndWait(profile, app_id);  // #3
  web_app::LaunchWebAppBrowserAndWait(profile, app_id);  // #4

  // Verify there are 4 apps.
  ASSERT_EQ(4u, BrowserList::GetInstance()->size());
  for (Browser* browser : *(BrowserList::GetInstance())) {
    ASSERT_EQ(browser->type(), Browser::Type::TYPE_APP);
    EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(browser, app_id));
  }

  // Now open the browser window.
  // This should be treated the same as a browser opening when nothing,
  // i.e. it will trigger a browser restore.
  SessionRestoreTestHelper restore_observer;
  chrome::NewEmptyWindow(profile);
  restore_observer.Wait();

  // Ensure there's 4 apps and 1 restored window
  int apps = 0;
  int browsers = 0;
  Browser* restored_browser = nullptr;
  ASSERT_EQ(5u, BrowserList::GetInstance()->size());
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_NORMAL) {
      restored_browser = browser;
      browsers++;
    } else if (browser->type() == Browser::Type::TYPE_APP) {
      apps++;
    }
  }
  ASSERT_EQ(apps, 4);
  ASSERT_EQ(browsers, 1);
  ASSERT_NE(restored_browser, nullptr);

  // now check we restored the browser correctly.
  EXPECT_EQ(restored_browser->tab_strip_model()->count(), 2);
  auto tab1_url =
      restored_browser->tab_strip_model()->GetWebContentsAt(0)->GetURL();
  auto tab2_url =
      restored_browser->tab_strip_model()->GetWebContentsAt(1)->GetURL();
  EXPECT_EQ(tab2_url, example_url2);
}

// This is disabled on mac pending http://crbug.com/1194201
// These tests currently fail on linux due to http://crbug.com/1196493.
// To keep the coverage from the rest of the test, we disable the failing check
// on linux for window-maximization.
#if defined(OS_MAC)
#define MAYBE_RestoreAppMinimized DISABLED_RestoreAppMinimized
#else
#define MAYBE_RestoreAppMinimized RestoreAppMinimized
#endif
// This test minimizes an app, ensures it restores correctly.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest, MAYBE_RestoreAppMinimized) {
  Profile* profile = browser()->profile();
  auto example_url = GURL("http://www.example.com");

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Open a PWA.
  web_app::AppId app_id = InstallPWA(profile, example_url);
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  app_browser->window()->Minimize();

  // Pretend to 'close the browser'.
  // Just shutdown the services as we would if the browser is shutting down for
  // real.
  ShutdownServices(profile);

  // Now that SessionServices are off, we can close stuff to simulate a closure.
  CloseBrowserSynchronously(app_browser);
  CloseBrowserSynchronously(browser());

  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  StartupServices(profile);

  SessionRestore::RestoreSession(
      profile, nullptr,
      SessionRestore::SYNCHRONOUS | SessionRestore::RESTORE_APPS, {});

  // It opens up the browser and the app.
  app_browser = nullptr;
  Browser* normal_browser = nullptr;
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_APP) {
      EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(browser, app_id));
#if !defined(OS_LINUX)
      EXPECT_TRUE(browser->window()->IsMinimized());
#endif
      EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
                example_url);
      app_browser = browser;
    } else {
      normal_browser = browser;
    }
  }

  ASSERT_NE(app_browser, nullptr);
  ASSERT_NE(normal_browser, nullptr);

  keep_alive.reset();
  profile_keep_alive.reset();
}

// This is disabled on mac pending http://crbug.com/1194201
// These tests currently fail on linux due to http://crbug.com/1196493.
// In order to keep the coverage from the rest of the test, the checks that
// fail on linux are explicitly disabled.
#if defined(OS_MAC)
#define MAYBE_RestoreMaximizedApp DISABLED_RestoreMaximizedApp
#else
#define MAYBE_RestoreMaximizedApp RestoreMaximizedApp
#endif
// This test maximizes an app, ensures it restores correctly.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest, MAYBE_RestoreMaximizedApp) {
  Profile* profile = browser()->profile();
  auto example_url = GURL("http://www.example.com");

  // Open a PWA.
  web_app::AppId app_id = InstallPWA(profile, example_url);
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  // Maximize.
  app_browser->window()->Maximize();

  // Pretend to 'close the browser'.
  // Just shutdown the services as we would if the browser is shutting down for
  // real.
  ShutdownServices(profile);

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Now that SessionServices are off, we can close stuff to simulate a closure.
  CloseBrowserSynchronously(app_browser);
  CloseBrowserSynchronously(browser());

  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Now trigger a restore.
  // We need to start up the services again before restoring.
  StartupServices(profile);

  SessionRestore::RestoreSession(
      profile, nullptr,
      SessionRestore::SYNCHRONOUS | SessionRestore::RESTORE_APPS, {});

  app_browser = nullptr;
  Browser* normal_browser = nullptr;
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_APP) {
      EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(browser, app_id));
#if !defined(OS_LINUX)
      EXPECT_TRUE(browser->window()->IsMaximized());
#endif
      EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(0)->GetURL(),
                example_url);
      app_browser = browser;
    } else {
      normal_browser = browser;
    }
  }

  // It opens up the browser and the app.
  ASSERT_NE(app_browser, nullptr);
  ASSERT_NE(normal_browser, nullptr);
  profile = normal_browser->profile();
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());

  keep_alive.reset();
  profile_keep_alive.reset();
}

#if !defined(OS_MAC)
// This test does not make sense on mac, since when apps are opened,
// the browser must open. This test opens an app when nothing is open,
// then closes it. Then opens a browser to ensure the user's previous browser
// session was preserved.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest,
                       OpeningAppDoesNotAffectBrowserSession) {
  Profile* profile = browser()->profile();
  auto example_url = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");

  // Add two tabs to the normal browser.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  CloseBrowserSynchronously(browser());

  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Pretend to 'close the browser'.
  // Just shutdown the services as we would if the browser is shutting down for
  // real.
  ShutdownServices(profile);

  // 'open the browser' again
  StartupServices(profile);

  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Open a PWA.
  web_app::AppId app_id = InstallPWA(profile, example_url);
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  CloseBrowserSynchronously(app_browser);

  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Now restore.
  SessionRestore::RestoreSession(profile, nullptr, SessionRestore::SYNCHRONOUS,
                                 {});

  // There's just one window open at the moment.
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Check we got all the tabs back.
  Browser* browser = BrowserList::GetInstance()->get(0);
  EXPECT_TRUE(browser->type() == Browser::Type::TYPE_NORMAL);
  EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            example_url);
  EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(2)->GetURL(),
            example_url2);

  keep_alive.reset();
  profile_keep_alive.reset();
}
#endif  //  #if !defined(OS_MAC)

// This tests if an app being the last remaining window does not interfere
// with the last browser window being restored later.
// This test also tests that apps aren't restored on normal startups.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest,
                       AppWindowsDontIntefereWithBrowserSessionRestore) {
  auto example_url = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");
  auto app_url = GURL("http://www.example3.com");

  // Add two tabs to the normal browser. So we can tell it restored correctly.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  Profile* profile = browser()->profile();

  // Open a PWA.
  web_app::AppId app_id = InstallPWA(profile, app_url);
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  // App and 3 tab browser.
  ASSERT_EQ(2u, BrowserList::GetInstance()->size());

  // Don't kill the test.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  // Note the ordering here is important. The browser is closed first,
  // then the app. This means previously, nothing would be restored.
  CloseBrowserSynchronously(browser());
  CloseBrowserSynchronously(app_browser);

  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // Now open a browser window.
  // This should be treated the same as a browser opening when nothing,
  // i.e. it will trigger a browser restore.
  SessionRestoreTestHelper restore_observer;
  chrome::NewEmptyWindow(profile);
  restore_observer.Wait();

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Check we got all the tabs back.
  Browser* browser = BrowserList::GetInstance()->get(0);
  EXPECT_EQ(browser->tab_strip_model()->count(), 3);
  EXPECT_TRUE(browser->type() == Browser::Type::TYPE_NORMAL);
  EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            example_url);
  EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(2)->GetURL(),
            example_url2);

  keep_alive.reset();
  profile_keep_alive.reset();
}

// This test ensures AppSessionService is notified of app restorations
// correctly.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest, CtrlShiftTRestoresAppsCorrectly) {
  Profile* profile = browser()->profile();
  auto example_url = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");
  auto example_url3 = GURL("http://www.example3.com");

  // Install 3 PWAs.
  web_app::AppId app_id = InstallPWA(profile, example_url);
  web_app::AppId app_id2 = InstallPWA(profile, example_url2);
  web_app::AppId app_id3 = InstallPWA(profile, example_url3);

  // Open all 3.
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
  Browser* app_browser2 = web_app::LaunchWebAppBrowserAndWait(profile, app_id2);
  Browser* app_browser3 = web_app::LaunchWebAppBrowserAndWait(profile, app_id3);

  // 3 apps + basic browser.
  ASSERT_EQ(4u, BrowserList::GetInstance()->size());

  // Close all 3.
  CloseBrowserSynchronously(app_browser);
  CloseBrowserSynchronously(app_browser2);
  CloseBrowserSynchronously(app_browser3);

  // Just the basic browser.
  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Ctrl-Shift-T 3 times.
  chrome::RestoreTab(browser());
  chrome::RestoreTab(browser());
  chrome::RestoreTab(browser());

  // Ensure there's 4. Three apps, plus 1 basic test browser.
  bool app1_seen = false;
  bool app2_seen = false;
  bool app3_seen = false;
  ASSERT_EQ(4u, BrowserList::GetInstance()->size());
  for (Browser* browser : *(BrowserList::GetInstance())) {
    if (browser->type() == Browser::Type::TYPE_APP) {
      if (web_app::AppBrowserController::IsForWebApp(browser, app_id)) {
        EXPECT_FALSE(app1_seen);
        app1_seen = true;
      } else if (web_app::AppBrowserController::IsForWebApp(browser, app_id2)) {
        EXPECT_FALSE(app2_seen);
        app2_seen = true;
      } else if (web_app::AppBrowserController::IsForWebApp(browser, app_id3)) {
        EXPECT_FALSE(app3_seen);
        app3_seen = true;
      }
    }
  }
  EXPECT_TRUE(app1_seen);
  EXPECT_TRUE(app2_seen);
  EXPECT_TRUE(app3_seen);
}

// Request a no app restore and ensure no app was reopened.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest, NoAppRestore) {
  Profile* profile = browser()->profile();

  auto app_url = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  web_app::AppId app_id = InstallPWA(profile, app_url);
  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);

  ASSERT_EQ(2u, BrowserList::GetInstance()->size());

  // Don't kill the test.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  CloseBrowserSynchronously(browser());

  ShutdownServices(profile);

  CloseBrowserSynchronously(app_browser);

  ASSERT_EQ(0u, BrowserList::GetInstance()->size());

  // 'open the browser' again
  StartupServices(profile);

  // Now restore.
  SessionRestore::RestoreSession(profile, nullptr, SessionRestore::SYNCHRONOUS,
                                 {});

  ASSERT_EQ(1u, BrowserList::GetInstance()->size());

  // Check we got all the tabs back.
  Browser* browser = BrowserList::GetInstance()->get(0);
  EXPECT_EQ(browser->tab_strip_model()->count(), 2);
  EXPECT_TRUE(browser->type() == Browser::Type::TYPE_NORMAL);
  EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            example_url2);

  keep_alive.reset();
  profile_keep_alive.reset();
}

// Do a complex scenario that should only restore an app.
// Have a browser session saved in disk, then open and close two separate
// apps in sequence. Now try to restore that browser.
IN_PROC_BROWSER_TEST_F(AppSessionRestoreTest, InvokeTwoAppsThenRestore) {
  Profile* profile = browser()->profile();
  auto app_url = GURL("http://www.example.com");
  auto app_url2 = GURL("http://www.example.com");
  auto example_url2 = GURL("http://www.example2.com");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), example_url2, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Don't kill the test.
  auto keep_alive = std::make_unique<ScopedKeepAlive>(
      KeepAliveOrigin::SESSION_RESTORE, KeepAliveRestartOption::DISABLED);
  auto profile_keep_alive = std::make_unique<ScopedProfileKeepAlive>(
      profile, ProfileKeepAliveOrigin::kBrowserWindow);

  web_app::AppId app_id = InstallPWA(profile, app_url);
  web_app::AppId app_id2 = InstallPWA(profile, app_url2);

  CloseBrowserSynchronously(browser());

  Browser* app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id);
  CloseBrowserSynchronously(app_browser);

  app_browser = web_app::LaunchWebAppBrowserAndWait(profile, app_id2);
  CloseBrowserSynchronously(app_browser);

  // Now open a browser window.
  // This should be treated the same as a browser opening when nothing,
  // i.e. it will trigger a browser restore.
  SessionRestoreTestHelper restore_observer;
  chrome::NewEmptyWindow(profile);
  restore_observer.Wait();

  // Check we got all the tabs back.
  Browser* browser = BrowserList::GetInstance()->get(0);
  EXPECT_EQ(browser->tab_strip_model()->count(), 2);
  EXPECT_TRUE(browser->type() == Browser::Type::TYPE_NORMAL);
  EXPECT_EQ(browser->tab_strip_model()->GetWebContentsAt(1)->GetURL(),
            example_url2);

  keep_alive.reset();
  profile_keep_alive.reset();
}
#endif  //  BUILDFLAG(ENABLE_APP_SESSION_SERVICE)
