// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/strings/grit/services_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"

using content::WebContents;
using task_manager::browsertest_util::ColumnSpecifier;
using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyApp;
using task_manager::browsertest_util::MatchAnyExtension;
using task_manager::browsertest_util::MatchAnySubframe;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchAnyUtility;
using task_manager::browsertest_util::MatchApp;
using task_manager::browsertest_util::MatchExtension;
using task_manager::browsertest_util::MatchSubframe;
using task_manager::browsertest_util::MatchTab;
using task_manager::browsertest_util::MatchUtility;
using task_manager::browsertest_util::WaitForTaskManagerRows;
using task_manager::browsertest_util::WaitForTaskManagerStatToExceed;

namespace {

const base::FilePath::CharType* kTitle1File = FILE_PATH_LITERAL("title1.html");

}  // namespace

class TaskManagerBrowserTest : public extensions::ExtensionBrowserTest {
 public:
  TaskManagerBrowserTest() {}
  ~TaskManagerBrowserTest() override {}

  task_manager::TaskManagerTester* model() { return model_.get(); }

  void ShowTaskManager() {
    // Show the task manager. This populates the model, and helps with debugging
    // (you see the task manager).
    chrome::ShowTaskManager(browser());
    model_ = task_manager::TaskManagerTester::Create(
        base::Bind(&TaskManagerBrowserTest::TaskManagerTableModelSanityCheck,
                   base::Unretained(this)));
  }

  void HideTaskManager() {
    model_.reset();

    // Hide the task manager, and wait for it to go.
    chrome::HideTaskManager();
    base::RunLoop().RunUntilIdle();  // OnWindowClosed happens asynchronously.
  }

  GURL GetTestURL() {
    return ui_test_utils::GetTestUrl(
        base::FilePath(base::FilePath::kCurrentDirectory),
        base::FilePath(kTitle1File));
  }

  int FindResourceIndex(const base::string16& title) {
    for (int i = 0; i < model_->GetRowCount(); ++i) {
      if (title == model_->GetRowTitle(i))
        return i;
    }
    return -1;
  }

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionBrowserTest::SetUpCommandLine(command_line);

    // Do not launch device discovery process.
    command_line->AppendSwitch(switches::kDisableDeviceDiscoveryNotifications);
  }

  void TearDownOnMainThread() override {
    model_.reset();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(base::DIR_SOURCE_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("content/test/data/"));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  void TaskManagerTableModelSanityCheck() {
    // Ensure the groups are self-consistent.
    for (int i = 0; i < model()->GetRowCount(); ++i) {
      int start, length;
      model()->GetRowsGroupRange(i, &start, &length);
      for (int j = 0; j < length; ++j) {
        int start2, length2;
        model()->GetRowsGroupRange(start + j, &start2, &length2);
        EXPECT_EQ(start, start2);
        EXPECT_EQ(length, length2);
      }
    }
  }

  std::unique_ptr<task_manager::TaskManagerTester> model_;
  DISALLOW_COPY_AND_ASSIGN(TaskManagerBrowserTest);
};

class TaskManagerUtilityProcessBrowserTest : public TaskManagerBrowserTest {
 public:
  TaskManagerUtilityProcessBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TaskManagerBrowserTest::SetUpCommandLine(command_line);

    // Use a trivial PAC script to ensure that some javascript is being
    // executed.
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl,
        "data:,function FindProxyForURL(url, host){return \"DIRECT;\";}");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerUtilityProcessBrowserTest);
};

// Parameterized variant of TaskManagerBrowserTest which runs with/without
// --site-per-process, which enables out of process iframes (OOPIFs).
class TaskManagerOOPIFBrowserTest : public TaskManagerBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  TaskManagerOOPIFBrowserTest() {}

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TaskManagerBrowserTest::SetUpCommandLine(command_line);
    if (GetParam())
      content::IsolateAllSitesForTesting(command_line);
  }

  bool ShouldExpectSubframes() {
    return content::AreAllSitesIsolatedForTesting();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TaskManagerOOPIFBrowserTest);
};

INSTANTIATE_TEST_SUITE_P(DefaultIsolation,
                         TaskManagerOOPIFBrowserTest,
                         ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         TaskManagerOOPIFBrowserTest,
                         ::testing::Values(true));

#if defined(OS_MACOSX) || defined(OS_LINUX)
#define MAYBE_ShutdownWhileOpen DISABLED_ShutdownWhileOpen
#else
#define MAYBE_ShutdownWhileOpen ShutdownWhileOpen
#endif

// Regression test for http://crbug.com/13361
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_ShutdownWhileOpen) {
  ShowTaskManager();
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeTabContentsChanges) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));

  // Open a new tab and make sure the task manager notices it.
  AddTabAtIndex(0, GetTestURL(), ui::PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Close the tab and verify that we notice.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabStripModel::CLOSE_NONE);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillTab) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));

  // Open a new tab and make sure the task manager notices it.
  AddTabAtIndex(0, GetTestURL(), ui::PAGE_TRANSITION_TYPED);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Killing the tab via task manager should remove the row.
  int tab = FindResourceIndex(MatchTab("title1.html"));
  ASSERT_NE(-1, tab);
  ASSERT_TRUE(model()->GetTabId(tab).is_valid());
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    model()->Kill(tab);
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  }

  // Tab should reappear in task manager upon reload.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
}

// Regression test for http://crbug.com/444945.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NavigateAwayFromHungRenderer) {
  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  GURL url1(embedded_test_server()->GetURL("/title2.html"));
  GURL url3(embedded_test_server()->GetURL("a.com", "/iframe.html"));

  // Open a new tab and make sure the task manager notices it.
  AddTabAtIndex(0, url1, ui::PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // Initiate a navigation that will create a new WebContents in the same
  // SiteInstance. Then immediately hang the renderer so that title3.html can't
  // load in this process.
  content::WebContentsAddedObserver web_contents_added_observer;
  int dummy_value = 0;
  ASSERT_TRUE(content::ExecuteScriptAndExtractInt(
      tab1->GetMainFrame(),
      "window.open('title3.html', '_blank');\n"
      "window.domAutomationController.send(55);\n"
      "while(1);",
      &dummy_value));

  // Blocks until a new WebContents appears as a result of window.open().
  WebContents* tab2 = web_contents_added_observer.GetWebContents();

  // Make sure the new WebContents is in tab1's hung renderer process.
  ASSERT_NE(nullptr, tab2);
  ASSERT_NE(tab1, tab2);
  ASSERT_EQ(tab1->GetMainFrame()->GetProcess(),
            tab2->GetMainFrame()->GetProcess())
      << "New WebContents must be in the same process as the old WebContents, "
      << "so that the new tab doesn't finish loading (what this test is all "
      << "about)";
  ASSERT_EQ(tab1->GetSiteInstance(), tab2->GetSiteInstance())
      << "New WebContents must initially be in the same site instance as the "
      << "old WebContents";

  // Now navigate this tab to a different site. This should wind up in a
  // different renderer process, so it should complete and show up in the task
  // manager.
  tab2->OpenURL(content::OpenURLParams(url3, content::Referrer(),
                                       WindowOpenDisposition::CURRENT_TAB,
                                       ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionTabChanges) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("good").AppendASCII("Extensions")
                    .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                    .AppendASCII("1.0.0.0")));

  // Browser, Extension background page, and the New Tab Page.
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Open a new tab to an extension URL. Afterwards, the third entry (background
  // page) should be an extension resource whose title starts with "Extension:".
  // The fourth entry (page.html) is also of type extension and has both a
  // WebContents and an extension. The title should start with "Extension:".
  GURL url("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html");
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int extension_tab = FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_NE(-1, extension_tab);
  ASSERT_TRUE(model()->GetTabId(extension_tab).is_valid());

  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_FALSE(model()->GetTabId(background_page).is_valid());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeExtensionTab) {
  // With the task manager closed, open a new tab to an extension URL.
  // Afterwards, when we open the task manager, the third entry (background
  // page) should be an extension resource whose title starts with "Extension:".
  // The fourth entry (page.html) is also of type extension and has both a
  // WebContents and an extension. The title should start with "Extension:".
  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("good")
                                .AppendASCII("Extensions")
                                .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
                                .AppendASCII("1.0.0.0")));
  GURL url("chrome-extension://behllobkkfkfnphdnhnkndlbkcpglgmj/page.html");
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);

  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  int extension_tab = FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_NE(-1, extension_tab);
  ASSERT_TRUE(model()->GetTabId(extension_tab).is_valid());

  int background_page = FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_NE(-1, background_page);
  ASSERT_FALSE(model()->GetTabId(background_page).is_valid());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTabChanges) {
  ShowTaskManager();

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("packaged_app")));
  const extensions::Extension* extension =
      extension_registry()->GetExtensionById(
          last_loaded_extension_id(), extensions::ExtensionRegistry::ENABLED);

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);

  // There should be 1 "App: " tab and the original new tab page.
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchApp("Packaged App Test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension.
  int app_tab = FindResourceIndex(MatchApp("Packaged App Test"));
  ASSERT_NE(-1, app_tab);
  ASSERT_TRUE(model()->GetTabId(app_tab).is_valid());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());

  // Unload extension to make sure the tab goes away.
  UnloadExtension(last_loaded_extension_id());

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeAppTab) {
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("packaged_app")));
  const extensions::Extension* extension =
      extension_registry()->GetExtensionById(
          last_loaded_extension_id(), extensions::ExtensionRegistry::ENABLED);

  // Open a new tab to the app's launch URL and make sure we notice that.
  GURL url(extension->GetResourceURL("main.html"));
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);

  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchApp("Packaged App Test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension.
  int app_tab = FindResourceIndex(MatchApp("Packaged App Test"));
  ASSERT_NE(-1, app_tab);
  ASSERT_TRUE(model()->GetTabId(app_tab).is_valid());
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabChanges) {
  ShowTaskManager();

  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL::Replacements replace_host;
  replace_host.SetHostStr("localhost");
  GURL base_url = embedded_test_server()->GetURL(
      "/extensions/api_test/app_process/");
  base_url = base_url.ReplaceComponents(replace_host);

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Check that the new entry's title starts with "Tab:".
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));
  // Force the TaskManager to query the title.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Now reload and check that the last entry's title now starts with "App:".
  ui_test_utils::NavigateToURL(browser(), url);

  // Force the TaskManager to query the title.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchApp("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Disable extension and reload.
  DisableExtension(last_loaded_extension_id());
  ui_test_utils::NavigateToURL(browser(), url);

  // The hosted app should now show up as a normal "Tab: ".
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabAfterReload) {
  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "localhost", "/extensions/api_test/app_process/");

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));

  // Now reload, which should transition this tab to being an App.
  ui_test_utils::NavigateToURL(browser(), url);

  ShowTaskManager();

  // The TaskManager should show this as an "App: "
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, NoticeHostedAppTabBeforeReload) {
  // The app under test acts on URLs whose host is "localhost",
  // so the URLs we navigate to must have host "localhost".
  GURL base_url = embedded_test_server()->GetURL(
      "localhost", "/extensions/api_test/app_process/");

  // Open a new tab to an app URL before the app is loaded.
  GURL url(base_url.Resolve("path1/empty.html"));
  content::WindowedNotificationObserver observer(
      content::NOTIFICATION_NAV_ENTRY_COMMITTED,
      content::NotificationService::AllSources());
  AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED);
  observer.Wait();

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));

  ShowTaskManager();

  // The TaskManager should show this as a "Tab: " because the page hasn't been
  // reloaded since the hosted app was installed.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
}

// Regression test for http://crbug.com/18693.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, ReloadExtension) {
  ShowTaskManager();
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("common").AppendASCII("background_page")));

  // Wait until we see the loaded extension in the task manager (the three
  // resources are: the browser process, New Tab Page, and the extension).
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("background_page")));

  // Reload the extension a few times and make sure our resource count doesn't
  // increase.
  std::string extension_id = last_loaded_extension_id();
  for (int i = 1; i <= 5; i++) {
    SCOPED_TRACE(testing::Message() << "Reloading extension for the " << i
                                    << "th time.");
    ReloadExtension(extension_id);
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchExtension("background_page")));
  }
}

// Checks that task manager counts a worker thread JS heap size.
// http://crbug.com/241066
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, WebWorkerJSHeapMemory) {
  ShowTaskManager();
  model()->ToggleColumnVisibility(ColumnSpecifier::V8_MEMORY);
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  size_t minimal_heap_size = 4 * 1024 * 1024 * sizeof(void*);
  std::string test_js = base::StringPrintf(
      "var blob = new Blob([\n"
      "    'mem = new Array(%lu);',\n"
      "    'for (var i = 0; i < mem.length; i += 16)',"
      "    '  mem[i] = i;',\n"
      "    'postMessage(\"okay\");']);\n"
      "blobURL = window.URL.createObjectURL(blob);\n"
      "var worker = new Worker(blobURL);\n"
      "worker.addEventListener('message', function(e) {\n"
      "  window.domAutomationController.send(e.data);\n"  // e.data == "okay"
      "});\n"
      "worker.postMessage('go');\n",
      static_cast<unsigned long>(minimal_heap_size));
  std::string ok;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(), test_js, &ok));
  ASSERT_EQ("okay", ok);

  // The worker has allocated objects of at least |minimal_heap_size| bytes.
  // Wait for the heap stats to reflect this.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::V8_MEMORY, minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::V8_MEMORY_USED,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::MEMORY_FOOTPRINT,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
}

// Checks that task manager counts renderer JS heap size.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, JSHeapMemory) {
  ShowTaskManager();
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  size_t minimal_heap_size = 4 * 1024 * 1024 * sizeof(void*);
  std::string test_js = base::StringPrintf(
      "mem = new Array(%lu);\n"
      "for (var i = 0; i < mem.length; i += 16)\n"
      "  mem[i] = i;\n"
      "window.domAutomationController.send(\"okay\");\n",
      static_cast<unsigned long>(minimal_heap_size));
  std::string ok;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(), test_js, &ok));
  ASSERT_EQ("okay", ok);

  model()->ToggleColumnVisibility(ColumnSpecifier::V8_MEMORY);

  // The page's js has allocated objects of at least |minimal_heap_size| bytes.
  // Wait for the heap stats to reflect this.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::V8_MEMORY, minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::V8_MEMORY_USED,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
}

#if defined(MEMORY_SANITIZER) || defined(OS_LINUX)
// This tests times out when MSan is enabled. See https://crbug.com/890313.
// Failing on Linux CFI. See https://crbug.com/995132.
#define MAYBE_SentDataObserved DISABLED_SentDataObserved
#else
#define MAYBE_SentDataObserved SentDataObserved
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_SentDataObserved) {
  ShowTaskManager();
  GURL test_gurl = embedded_test_server()->GetURL("/title1.html");

  ui_test_utils::NavigateToURL(browser(), test_gurl);
  std::string test_js = R"(
      document.title = 'network use';
      var mem = new Uint8Array(16 << 20);
      for (var i = 0; i < mem.length; i += 16) {
        mem[i] = i;
      }
      var formData = new FormData();
      formData.append('StringKey1', new Blob([mem]));
      var request =
          new Request(location.href, {method: 'POST', body: formData});
      fetch(request).then(response => response.text());
      )";

  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->ExecuteJavaScriptForTests(base::UTF8ToUTF16(test_js),
                                  base::NullCallback());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("network use"), ColumnSpecifier::TOTAL_NETWORK_USE, 16000000));
  // There shouldn't be too much usage on the browser process. Note that it
  // should be the first row since tasks are sorted by process ID then by task
  // ID.
  EXPECT_GE(20000,
            model()->GetColumnValue(ColumnSpecifier::TOTAL_NETWORK_USE, 0));
}

#if defined(MEMORY_SANITIZER) || defined(OS_LINUX)
// This tests times out when MSan is enabled. See https://crbug.com/890313.
// Failing on Linux CFI. See https://crbug.com/995132.
#define MAYBE_TotalSentDataObserved DISABLED_TotalSentDataObserved
#else
#define MAYBE_TotalSentDataObserved TotalSentDataObserved
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_TotalSentDataObserved) {
  ShowTaskManager();
  GURL test_gurl = embedded_test_server()->GetURL("/title1.html");

  ui_test_utils::NavigateToURL(browser(), test_gurl);
  std::string test_js = R"(
      document.title = 'network use';
      var mem = new Uint8Array(16 << 20);
      for (var i = 0; i < mem.length; i += 16) {
        mem[i] = i;
      }
      var formData = new FormData();
      formData.append('StringKey1', new Blob([mem]));
      var request =
          new Request(location.href, {method: 'POST', body: formData});
      fetch(request).then(response => response.text());
      )";

  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->ExecuteJavaScriptForTests(base::UTF8ToUTF16(test_js),
                                  base::NullCallback());

  // This test uses |setTimeout| to exceed the Nyquist ratio to ensure that at
  // least 1 refresh has happened of no traffic.
  test_js = R"(
      var request =
          new Request(location.href, {method: 'POST', body: formData});
      setTimeout(
          () => {fetch(request).then(response => response.text())}, 2000);
      )";

  browser()
      ->tab_strip_model()
      ->GetActiveWebContents()
      ->GetMainFrame()
      ->ExecuteJavaScriptForTests(base::UTF8ToUTF16(test_js),
                                  base::NullCallback());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("network use"), ColumnSpecifier::TOTAL_NETWORK_USE,
      16000000 * 2));
  // There shouldn't be too much usage on the browser process. Note that it
  // should be the first row since tasks are sorted by process ID then by task
  // ID.
  EXPECT_GE(20000,
            model()->GetColumnValue(ColumnSpecifier::TOTAL_NETWORK_USE, 0));
}

// Checks that task manager counts idle wakeups.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, IdleWakeups) {
  ShowTaskManager();
  model()->ToggleColumnVisibility(ColumnSpecifier::IDLE_WAKEUPS);

  ui_test_utils::NavigateToURL(browser(), GetTestURL());

  std::string test_js =
    "function myWait() {\n"
    "  setTimeout(function() { myWait(); }, 1)\n"
    "}\n"
    "myWait();\n"
    "window.domAutomationController.send(\"okay\");\n";

  std::string ok;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(), test_js, &ok));
  ASSERT_EQ("okay", ok);

// The script above should trigger a lot of idle wakeups - up to 1000 per
// second. Let's make sure we get at least 100 (in case the test runs slow).
// On Mac, set a lower threshold because Chrome Mac generates fewer wakes.
#if defined(OS_MACOSX)
  const int kMinExpectedWakeCount = 50;
#else
  const int kMinExpectedWakeCount = 100;
#endif  // defined(OS_MACOSX)
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::IDLE_WAKEUPS,
      kMinExpectedWakeCount));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
}

// Crashes on multiple builders.  http://crbug.com/1025346
// Checks that task manager counts utility process JS heap size.
IN_PROC_BROWSER_TEST_F(TaskManagerUtilityProcessBrowserTest,
                       DISABLED_UtilityJSHeapMemory) {
  ShowTaskManager();
  model()->ToggleColumnVisibility(ColumnSpecifier::V8_MEMORY);

  auto proxy_resolver_name =
      l10n_util::GetStringUTF16(IDS_PROXY_RESOLVER_DISPLAY_NAME);
  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  // The PAC script is trivial, so don't expect a large heap.
  size_t minimal_heap_size = 1024;
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchUtility(proxy_resolver_name), ColumnSpecifier::V8_MEMORY,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchUtility(proxy_resolver_name), ColumnSpecifier::V8_MEMORY_USED,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchUtility(proxy_resolver_name)));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsNewDockedWindow) {
  ShowTaskManager();  // Task manager shown BEFORE dev tools window.

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsNewUndockedWindow) {
  ShowTaskManager();  // Task manager shown BEFORE dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsOldDockedWindow) {
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), true);
  ShowTaskManager();  // Task manager shown AFTER dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, DevToolsOldUndockedWindow) {
  DevToolsWindow* devtools =
      DevToolsWindowTesting::OpenDevToolsWindowSync(browser(), false);
  ShowTaskManager();  // Task manager shown AFTER dev tools window.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnyTab()));
  DevToolsWindowTesting::CloseDevToolsWindowSync(devtools);
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, HistoryNavigationInNewTab) {
  ShowTaskManager();

  ui_test_utils::NavigateToURL(browser(), GetTestURL());
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  ui_test_utils::NavigateToURL(browser(), GURL("about:version"));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("About Version")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  chrome::GoBack(browser(), WindowOpenDisposition::NEW_BACKGROUND_TAB);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("About Version")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // In http://crbug.com/738169, the task_manager::Task for the background tab
  // was created with process id 0, resulting in zero values for all process
  // metrics. Ensure that this is not the case.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::PROCESS_ID,
      base::kNullProcessId));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("title1.html"), ColumnSpecifier::MEMORY_FOOTPRINT, 1000));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("About Version"), ColumnSpecifier::MEMORY_FOOTPRINT, 1000));
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest, SubframeHistoryNavigation) {
  if (!ShouldExpectSubframes())
    return;  // This test is lame without OOPIFs.

  ShowTaskManager();

  // This URL will have two out-of-process iframe processes (for b.com and
  // c.com) under --site-per-process: it's an a.com page containing a b.com
  // <iframe> containing a b.com <iframe> containing a c.com <iframe>.
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "a.com", "/cross_site_iframe_factory.html?a(b(b(c)))"));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Cross-site iframe factory")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Simulate a user gesture on the frame about to be navigated so that the
  // corresponding navigation entry is not marked as skippable.
  content::RenderFrameHost* child_frame = ChildFrameAt(tab->GetMainFrame(), 0);
  content::RenderFrameHost* grandchild_frame = ChildFrameAt(child_frame, 0);
  grandchild_frame->ExecuteJavaScriptWithUserGestureForTests(
      base::UTF8ToUTF16("a=5"));

  GURL d_url = embedded_test_server()->GetURL(
      "d.com", "/cross_site_iframe_factory.html?d(e)");
  ASSERT_TRUE(content::ExecuteScript(
      tab->GetMainFrame(),
      "frames[0][0].location.href = '" + d_url.spec() + "';"));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(0, MatchSubframe("http://c.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://d.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://e.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnySubframe()));

  ASSERT_TRUE(chrome::CanGoBack(browser()));
  chrome::GoBack(browser(), WindowOpenDisposition::CURRENT_TAB);

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(0, MatchSubframe("http://d.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(0, MatchSubframe("http://e.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));

  ASSERT_TRUE(chrome::CanGoForward(browser()));
  chrome::GoForward(browser(), WindowOpenDisposition::CURRENT_TAB);

  // When the subframe appears in the cloned process, it must have a valid
  // process ID.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchSubframe("http://d.com/"), ColumnSpecifier::PROCESS_ID,
      base::kNullProcessId));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchSubframe("http://e.com/"), ColumnSpecifier::PROCESS_ID,
      base::kNullProcessId));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(0, MatchSubframe("http://c.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://d.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://e.com/")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnySubframe()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Subframe processes should report some amount of physical memory usage.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchSubframe("http://d.com/"), ColumnSpecifier::MEMORY_FOOTPRINT, 1000));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchSubframe("http://e.com/"), ColumnSpecifier::MEMORY_FOOTPRINT, 1000));
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest, KillSubframe) {
  ShowTaskManager();

  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL main_url(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(main_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  GURL b_url;
  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));

    // Remember |b_url| to be able to later renavigate to the same URL without
    // doing any process swaps (we want to avoid redirects that would happen
    // when going through /cross-site/foo.com/..., because
    // https://crbug.com/642958 wouldn't repro in presence of process swaps).
    navigation_observer.Wait();
    b_url = browser()
                ->tab_strip_model()
                ->GetActiveWebContents()
                ->GetAllFrames()[1]
                ->GetLastCommittedURL();
    ASSERT_EQ(b_url.host(), "b.com");  // Sanity check of test code / setup.

    {
      content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
      int subframe_b = FindResourceIndex(MatchSubframe("http://b.com/"));
      ASSERT_NE(-1, subframe_b);
      ASSERT_TRUE(model()->GetTabId(subframe_b).is_valid());
      model()->Kill(subframe_b);

      ASSERT_NO_FATAL_FAILURE(
          WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
      ASSERT_NO_FATAL_FAILURE(
          WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
      ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
      ASSERT_NO_FATAL_FAILURE(
          WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
    }
  }

  HideTaskManager();
  ShowTaskManager();

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));

    // Reload the subframe and verify it has re-appeared in the task manager.
    // This is a regression test for https://crbug.com/642958.
    ASSERT_TRUE(content::ExecuteScript(
        browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
        "document.getElementById('frame1').src = '" + b_url.spec() + "';"));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  }
}

// Tests what happens when a tab navigates to a site (a.com) that it previously
// has a cross-process subframe into (b.com).
IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest, NavigateToSubframeProcess) {
  ShowTaskManager();

  // Navigate the tab to a page on a.com with cross-process subframes to
  // b.com and c.com.
  GURL a_dotcom(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  // Now navigate to a page on b.com with a simple (same-site) iframe.
  // This should not show any subframe resources in the task manager.
  GURL b_dotcom(
      embedded_test_server()->GetURL("/cross-site/b.com/iframe.html"));

  browser()->OpenURL(content::OpenURLParams(b_dotcom, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  HideTaskManager();
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest,
                       NavigateToSiteWithSubframeToOriginalSite) {
  ShowTaskManager();

  // Navigate to a page on b.com with a simple (same-site) iframe.
  // This should not show any subframe resources in the task manager.
  GURL b_dotcom(
      embedded_test_server()->GetURL("/cross-site/b.com/iframe.html"));

  browser()->OpenURL(content::OpenURLParams(b_dotcom, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));

  // Now navigate the tab to a page on a.com with cross-process subframes to
  // b.com and c.com.
  GURL a_dotcom(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  HideTaskManager();
  ShowTaskManager();

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }
}

// Tests what happens when a tab navigates a cross-frame iframe (to b.com)
// back to the site of the parent document (a.com).
IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest,
                       CrossSiteIframeBecomesSameSite) {
  ShowTaskManager();

  // Navigate the tab to a page on a.com with cross-process subframes to
  // b.com and c.com.
  content::TestNavigationObserver navigation_observer(
      browser()->tab_strip_model()->GetActiveWebContents());
  GURL a_dotcom(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  // Navigate the b.com frame back to a.com. It is no longer a cross-site iframe
  navigation_observer.Wait();
  const std::string r_script =
      R"( document.getElementById('frame1').src='/title1.html';
          document.title='aac'; )";
  ASSERT_TRUE(content::ExecuteScript(
      browser()->tab_strip_model()->GetActiveWebContents()->GetMainFrame(),
      r_script));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("aac")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("aac")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  HideTaskManager();
  ShowTaskManager();

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(0, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
  }
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("aac")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest,
                       LeavePageWithCrossSiteIframes) {
  ShowTaskManager();

  // Navigate the tab to a page on a.com with cross-process subframes.
  GURL a_dotcom_with_iframes(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(
      a_dotcom_with_iframes, content::Referrer(),
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  if (!ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnySubframe()));
  }

  // Navigate the tab to a page on a.com without cross-process subframes, and
  // the subframe processes should disappear.
  GURL a_dotcom_simple(
      embedded_test_server()->GetURL("/cross-site/a.com/title2.html"));
  browser()->OpenURL(content::OpenURLParams(
      a_dotcom_simple, content::Referrer(), WindowOpenDisposition::CURRENT_TAB,
      ui::PAGE_TRANSITION_TYPED, false));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));

  HideTaskManager();
  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
}

IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest, OrderingOfDependentRows) {
  ShowTaskManager();

  GURL a_with_frames(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b,c(d,a,b,c))"));
  browser()->OpenURL(content::OpenURLParams(a_with_frames, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false));

  if (ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://d.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(3, MatchAnySubframe()));
  }
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Cross-site iframe factory")));

  int index = FindResourceIndex(MatchTab("Cross-site iframe factory"));
  std::vector<int> subframe_offsets;
  if (ShouldExpectSubframes()) {
    subframe_offsets = {
        FindResourceIndex(MatchSubframe("http://b.com/")) - index,
        FindResourceIndex(MatchSubframe("http://c.com/")) - index,
        FindResourceIndex(MatchSubframe("http://d.com/")) - index};
    EXPECT_THAT(subframe_offsets, testing::UnorderedElementsAre(1, 2, 3));
  }

  // Opening a new tab should appear below the existing tab.
  GURL other_tab_url(embedded_test_server()->GetURL(
      "d.com", "/cross_site_iframe_factory.html?d(a(c(b)))"));
  browser()->OpenURL(
      content::OpenURLParams(other_tab_url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_TYPED, false));

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(2, MatchTab("Cross-site iframe factory")));
  if (ShouldExpectSubframes()) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(6, MatchAnySubframe()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(2, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(2, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://d.com/")));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchSubframe("http://a.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(6, MatchAnySubframe()));
  } else {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
  }

  // The first tab may have moved in absolute position in the list (due to
  // random e.g. zygote or gpu activity).
  index = FindResourceIndex(MatchTab("Cross-site iframe factory"));

  // All of Tab 2's subframes will reuse Tab 1's existing processes for
  // corresponding sites.  Tab 2's d.com main frame row should then appear
  // after all the subframe processes.
  int tab2_subframe_count = ShouldExpectSubframes() ? 3 : 0;
  int tab2_main_frame_index = index +
                              static_cast<int>(subframe_offsets.size()) +
                              tab2_subframe_count + 1;
  EXPECT_EQ("Tab: Cross-site iframe factory",
            base::UTF16ToUTF8(model()->GetRowTitle(tab2_main_frame_index)));

  if (ShouldExpectSubframes()) {
    // Tab 2's a.com subframe should share Tab 1's main frame process and go
    // directly below it.
    EXPECT_EQ(index + 1, FindResourceIndex(MatchSubframe("http://a.com/")));

    // The other tab 2 subframes (b.com and c.com) should join existing
    // subframe processes from tab 1.  Check that the b.com and c.com subframe
    // processes now have two rows each.
    int subframe_b_index = FindResourceIndex(MatchSubframe("http://b.com/"));
    int subframe_c_index = FindResourceIndex(MatchSubframe("http://c.com/"));
    int subframe_d_index = FindResourceIndex(MatchSubframe("http://d.com/"));
    EXPECT_EQ("Subframe: http://b.com/",
              base::UTF16ToUTF8(model()->GetRowTitle(subframe_b_index + 1)));
    EXPECT_EQ("Subframe: http://c.com/",
              base::UTF16ToUTF8(model()->GetRowTitle(subframe_c_index + 1)));

    // The subframe processes should preserve their relative ordering.
    EXPECT_EQ(subframe_offsets[0] < subframe_offsets[1],
              subframe_b_index < subframe_c_index);
    EXPECT_EQ(subframe_offsets[1] < subframe_offsets[2],
              subframe_c_index < subframe_d_index);
    EXPECT_EQ(subframe_offsets[0] < subframe_offsets[2],
              subframe_b_index < subframe_d_index);
  }
}
