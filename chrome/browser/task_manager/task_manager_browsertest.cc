// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window_testing.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "chrome/browser/notifications/notification_test_util.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/predictors/autocomplete_action_predictor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager_browsertest_util.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "chrome/browser/task_manager/task_manager_tester.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/web_applications/extensions/web_app_extension_shortcut.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "components/infobars/core/infobar.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "content/public/test/test_navigation_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/strings/grit/services_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/url_constants.h"

using content::WebContents;
using task_manager::browsertest_util::ColumnSpecifier;
using task_manager::browsertest_util::MatchAboutBlankTab;
using task_manager::browsertest_util::MatchAnyApp;
using task_manager::browsertest_util::MatchAnyBFCache;
using task_manager::browsertest_util::MatchAnyExtension;
using task_manager::browsertest_util::MatchAnyFencedFrame;
using task_manager::browsertest_util::MatchAnyIncognitoFencedFrame;
using task_manager::browsertest_util::MatchAnyIncognitoTab;
using task_manager::browsertest_util::MatchAnyPrerender;
using task_manager::browsertest_util::MatchAnySubframe;
using task_manager::browsertest_util::MatchAnyTab;
using task_manager::browsertest_util::MatchAnyUtility;
using task_manager::browsertest_util::MatchApp;
using task_manager::browsertest_util::MatchBFCache;
using task_manager::browsertest_util::MatchExtension;
using task_manager::browsertest_util::MatchFencedFrame;
using task_manager::browsertest_util::MatchIncognitoFencedFrame;
using task_manager::browsertest_util::MatchIncognitoTab;
using task_manager::browsertest_util::MatchPrerender;
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
  TaskManagerBrowserTest() = default;
  TaskManagerBrowserTest(const TaskManagerBrowserTest&) = delete;
  TaskManagerBrowserTest& operator=(const TaskManagerBrowserTest&) = delete;
  ~TaskManagerBrowserTest() override = default;

  task_manager::TaskManagerTester* model() { return model_.get(); }

  void ShowTaskManager() {
    // Show the task manager. This populates the model, and helps with debugging
    // (you see the task manager).
    chrome::ShowTaskManager(browser());
    model_ = task_manager::TaskManagerTester::Create(base::BindRepeating(
        &TaskManagerBrowserTest::TaskManagerTableModelSanityCheck,
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

  std::optional<size_t> FindResourceIndex(const std::u16string& title) {
    for (size_t i = 0; i < model_->GetRowCount(); ++i) {
      if (title == model_->GetRowTitle(i))
        return i;
    }
    return std::nullopt;
  }

 protected:
  void TearDownOnMainThread() override {
    model_.reset();
    extensions::ExtensionBrowserTest::TearDownOnMainThread();
  }

  void SetUpOnMainThread() override {
    extensions::ExtensionBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");

    // Add content/test/data so we can use cross_site_iframe_factory.html
    base::FilePath test_data_dir;
    ASSERT_TRUE(
        base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data_dir.AppendASCII("content/test/data/"));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(embedded_test_server());
    embedded_test_server()->StartAcceptingConnections();
  }

 private:
  void TaskManagerTableModelSanityCheck() {
    // Ensure the groups are self-consistent.
    for (size_t i = 0; i < model()->GetRowCount(); ++i) {
      size_t start, length;
      model()->GetRowsGroupRange(i, &start, &length);
      for (size_t j = 0; j < length; ++j) {
        size_t start2, length2;
        model()->GetRowsGroupRange(start + j, &start2, &length2);
        EXPECT_EQ(start, start2);
        EXPECT_EQ(length, length2);
      }
    }
  }

  std::unique_ptr<task_manager::TaskManagerTester> model_;
};

class TaskManagerUtilityProcessBrowserTest : public TaskManagerBrowserTest {
 public:
  TaskManagerUtilityProcessBrowserTest() = default;
  TaskManagerUtilityProcessBrowserTest(
      const TaskManagerUtilityProcessBrowserTest&) = delete;
  TaskManagerUtilityProcessBrowserTest& operator=(
      const TaskManagerUtilityProcessBrowserTest&) = delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TaskManagerBrowserTest::SetUpCommandLine(command_line);

    // Use a trivial PAC script to ensure that some javascript is being
    // executed.
    command_line->AppendSwitchASCII(
        switches::kProxyPacUrl,
        "data:,function FindProxyForURL(url, host){return \"DIRECT;\";}");
  }
};

// Parameterized variant of TaskManagerBrowserTest which runs with/without
// --site-per-process, which enables out of process iframes (OOPIFs).
class TaskManagerOOPIFBrowserTest : public TaskManagerBrowserTest,
                                    public testing::WithParamInterface<bool> {
 public:
  TaskManagerOOPIFBrowserTest() = default;
  TaskManagerOOPIFBrowserTest(const TaskManagerOOPIFBrowserTest&) = delete;
  TaskManagerOOPIFBrowserTest& operator=(const TaskManagerOOPIFBrowserTest&) =
      delete;

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    TaskManagerBrowserTest::SetUpCommandLine(command_line);
    if (GetParam())
      content::IsolateAllSitesForTesting(command_line);
  }

  bool ShouldExpectSubframes() {
    return content::AreAllSitesIsolatedForTesting();
  }
};

INSTANTIATE_TEST_SUITE_P(DefaultIsolation,
                         TaskManagerOOPIFBrowserTest,
                         ::testing::Values(false));
INSTANTIATE_TEST_SUITE_P(SitePerProcess,
                         TaskManagerOOPIFBrowserTest,
                         ::testing::Values(true));

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
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
  ASSERT_TRUE(AddTabAtIndex(0, GetTestURL(), ui::PAGE_TRANSITION_TYPED));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Close the tab and verify that we notice.
  browser()->tab_strip_model()->CloseWebContentsAt(0,
                                                   TabCloseTypes::CLOSE_NONE);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
}

IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, KillTab) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchTab("title1.html")));

  // Open a new tab and make sure the task manager notices it.
  ASSERT_TRUE(AddTabAtIndex(0, GetTestURL(), ui::PAGE_TRANSITION_TYPED));

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));

  // Killing the tab via task manager should remove the row.
  std::optional<size_t> tab = FindResourceIndex(MatchTab("title1.html"));
  ASSERT_TRUE(tab.has_value());
  ASSERT_TRUE(model()->GetTabId(tab.value()).is_valid());
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    model()->Kill(tab.value());
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
  ASSERT_TRUE(AddTabAtIndex(0, url1, ui::PAGE_TRANSITION_TYPED));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyTab()));
  WebContents* tab1 = browser()->tab_strip_model()->GetActiveWebContents();

  // Initiate a navigation that will create a new WebContents in the same
  // SiteInstance. Then immediately hang the renderer so that title3.html can't
  // load in this process.
  content::WebContentsAddedObserver web_contents_added_observer;
  content::DOMMessageQueue message_queue;
  content::ExecuteScriptAsync(tab1->GetPrimaryMainFrame(),
                              "window.open('title3.html', '_blank');\n"
                              "window.domAutomationController.send(false);\n"
                              "while(1);");
  std::string message;
  EXPECT_TRUE(message_queue.WaitForMessage(&message));
  EXPECT_EQ("false", message);

  // Blocks until a new WebContents appears as a result of window.open().
  WebContents* tab2 = web_contents_added_observer.GetWebContents();

  // Make sure the new WebContents is in tab1's hung renderer process.
  ASSERT_NE(nullptr, tab2);
  ASSERT_NE(tab1, tab2);
  ASSERT_EQ(tab1->GetPrimaryMainFrame()->GetProcess(),
            tab2->GetPrimaryMainFrame()->GetProcess())
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
                                       ui::PAGE_TRANSITION_TYPED, false),
                /*navigation_handle_callback=*/{});

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
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  std::optional<size_t> extension_tab =
      FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_TRUE(extension_tab.has_value());
  ASSERT_TRUE(model()->GetTabId(extension_tab.value()).is_valid());

  std::optional<size_t> background_page =
      FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_TRUE(background_page.has_value());
  ASSERT_FALSE(model()->GetTabId(background_page.value()).is_valid());
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
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchExtension("Foobar")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchExtension("My extension 1")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(2, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  std::optional<size_t> extension_tab =
      FindResourceIndex(MatchExtension("Foobar"));
  ASSERT_TRUE(extension_tab.has_value());
  ASSERT_TRUE(model()->GetTabId(extension_tab.value()).is_valid());

  std::optional<size_t> background_page =
      FindResourceIndex(MatchExtension("My extension 1"));
  ASSERT_TRUE(background_page.has_value());
  ASSERT_FALSE(model()->GetTabId(background_page.value()).is_valid());
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
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  // There should be 1 "App: " tab and the original new tab page.
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchApp("Packaged App Test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension.
  std::optional<size_t> app_tab =
      FindResourceIndex(MatchApp("Packaged App Test"));
  ASSERT_TRUE(app_tab.has_value());
  ASSERT_TRUE(model()->GetTabId(app_tab.value()).is_valid());
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
  ASSERT_TRUE(AddTabAtIndex(0, url, ui::PAGE_TRANSITION_TYPED));

  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchApp("Packaged App Test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));

  // Check that the third entry (main.html) is of type extension and has both
  // a tab contents and an extension.
  std::optional<size_t> app_tab =
      FindResourceIndex(MatchApp("Packaged App Test"));
  ASSERT_TRUE(app_tab.has_value());
  ASSERT_TRUE(model()->GetTabId(app_tab.value()).is_valid());
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
  NavigateToURLWithDisposition(browser(), url,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Force the TaskManager to query the title.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyApp()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchApp("Unmodified")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyExtension()));

  // Disable extension and reload.
  DisableExtension(last_loaded_extension_id());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  NavigateToURLWithDisposition(browser(), url,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Load the hosted app and make sure it still starts with "Tab:",
  // since it hasn't changed to an app process yet.
  ASSERT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("api_test").AppendASCII("app_process")));

  // Now reload, which should transition this tab to being an App.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

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
  NavigateToURLWithDisposition(browser(), url,
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

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
  // Workers require a trustworthy (e.g. https) context.
  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory("chrome/test/data");
  ASSERT_TRUE(https_server.Start());
  GURL test_url = https_server.GetURL("/title1.html");

  ShowTaskManager();
  model()->ToggleColumnVisibility(ColumnSpecifier::V8_MEMORY);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_url));
  size_t minimal_heap_size = 4 * 1024 * 1024 * sizeof(void*);
  std::string test_js = base::StringPrintf(
      "var blob = new Blob([\n"
      "    'mem = new Array(%lu);',\n"
      "    'for (var i = 0; i < mem.length; i += 16)',"
      "    '  mem[i] = i;',\n"
      "    'postMessage(\"okay\");']);\n"
      "blobURL = window.URL.createObjectURL(blob);\n"
      "var worker = new Worker(blobURL);\n"
      "new Promise(resolve => {\n"
      "  worker.addEventListener('message', function(e) {\n"
      "    resolve(e.data);\n"  // e.data == "okay"
      "  });\n"
      "  worker.postMessage('go');\n"
      "});\n",
      static_cast<unsigned long>(minimal_heap_size));
  ASSERT_EQ("okay",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(), test_js));

  // The worker has allocated objects of at least |minimal_heap_size| bytes.
  // Wait for the heap stats to reflect this.
  const char kTabWildcard[] = "127.0.0.1:*/title1.html";
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab(kTabWildcard), ColumnSpecifier::V8_MEMORY, minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab(kTabWildcard), ColumnSpecifier::V8_MEMORY_USED,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab(kTabWildcard), ColumnSpecifier::MEMORY_FOOTPRINT,
      minimal_heap_size));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab(kTabWildcard)));
}

// Checks that task manager counts renderer JS heap size.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, JSHeapMemory) {
  ShowTaskManager();
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  size_t minimal_heap_size = 4 * 1024 * 1024 * sizeof(void*);
  std::string test_js = base::StringPrintf(
      "mem = new Array(%lu);\n"
      "for (var i = 0; i < mem.length; i += 16)\n"
      "  mem[i] = i;\n"
      "\"okay\";\n",
      static_cast<unsigned long>(minimal_heap_size));
  std::string ok;
  ASSERT_EQ("okay",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(), test_js));

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

#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// This tests times out when MSan is enabled. See https://crbug.com/890313.
// Failing on Linux CFI. See https://crbug.com/995132.
#define MAYBE_SentDataObserved DISABLED_SentDataObserved
#else
#define MAYBE_SentDataObserved SentDataObserved
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_SentDataObserved) {
  ShowTaskManager();
  GURL test_gurl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_gurl));
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
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptForTests(base::UTF8ToUTF16(test_js),
                                  base::NullCallback(),
                                  content::ISOLATED_WORLD_ID_GLOBAL);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("network use"), ColumnSpecifier::TOTAL_NETWORK_USE, 16000000));
  // There shouldn't be too much usage on the browser process. Note that it
  // should be the first row since tasks are sorted by process ID then by task
  // ID.
  EXPECT_GE(20000,
            model()->GetColumnValue(ColumnSpecifier::TOTAL_NETWORK_USE, 0));
}

#if defined(MEMORY_SANITIZER) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// This tests times out when MSan is enabled. See https://crbug.com/890313.
// Failing on Linux CFI. See https://crbug.com/995132.
#define MAYBE_TotalSentDataObserved DISABLED_TotalSentDataObserved
#else
#define MAYBE_TotalSentDataObserved TotalSentDataObserved
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_TotalSentDataObserved) {
  ShowTaskManager();
  GURL test_gurl = embedded_test_server()->GetURL("/title1.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), test_gurl));
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
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptForTests(base::UTF8ToUTF16(test_js),
                                  base::NullCallback(),
                                  content::ISOLATED_WORLD_ID_GLOBAL);

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
      ->GetPrimaryMainFrame()
      ->ExecuteJavaScriptForTests(base::UTF8ToUTF16(test_js),
                                  base::NullCallback(),
                                  content::ISOLATED_WORLD_ID_GLOBAL);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerStatToExceed(
      MatchTab("network use"), ColumnSpecifier::TOTAL_NETWORK_USE,
      16000000 * 2));
  // There shouldn't be too much usage on the browser process. Note that it
  // should be the first row since tasks are sorted by process ID then by task
  // ID.
  EXPECT_GE(20000,
            model()->GetColumnValue(ColumnSpecifier::TOTAL_NETWORK_USE, 0));
}

// Checks that task manager counts idle wakeups. Since this test relies on
// forcing actual system-level idle wakeups to happen, it is inherently
// dependent on the load of the rest of the system, details of the OS scheduler,
// and so on, which makes it very prone to flakes.
#if BUILDFLAG(IS_MAC)
// This test is too flaky to be useable on Mac, because of the reasons given
// above.
#define MAYBE_IdleWakeups DISABLED_IdleWakeups
#else
#define MAYBE_IdleWakeups IdleWakeups
#endif
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest, MAYBE_IdleWakeups) {
  ShowTaskManager();
  model()->ToggleColumnVisibility(ColumnSpecifier::IDLE_WAKEUPS);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));

  std::string test_js =
      "function myWait() {\n"
      "  setTimeout(function() { myWait(); }, 1)\n"
      "}\n"
      "myWait();\n"
      "\"okay\";\n";

  ASSERT_EQ("okay",
            content::EvalJs(
                browser()->tab_strip_model()->GetActiveWebContents(), test_js));

  // The script above should trigger a lot of idle wakeups - up to 1000 per
  // second. Let's make sure we get at least 100 (in case the test runs slow).
  const int kMinExpectedWakeCount = 100;
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
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

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GetTestURL()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("title1.html")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(browser(), GURL("chrome://version/")));
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
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "a.com", "/cross_site_iframe_factory.html?a(b(b(c)))")));

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
  content::RenderFrameHost* child_frame =
      ChildFrameAt(tab->GetPrimaryMainFrame(), 0);
  content::RenderFrameHost* grandchild_frame = ChildFrameAt(child_frame, 0);
  grandchild_frame->ExecuteJavaScriptWithUserGestureForTests(
      u"a=5", base::NullCallback(), content::ISOLATED_WORLD_ID_GLOBAL);

  GURL d_url = embedded_test_server()->GetURL(
      "d.com", "/cross_site_iframe_factory.html?d(e)");
  ASSERT_TRUE(
      content::ExecJs(tab->GetPrimaryMainFrame(),
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
  int expected_c_subframes = 1;
  if (content::IsIsolatedOriginRequiredToGuaranteeDedicatedProcess()) {
    // Isolate b.com so that it will be forced into a separate process. This
    // will prevent the main frame and c.com subframe from being placed in the
    // the process that gets killed by this test.
    content::IsolateOriginsForTesting(
        embedded_test_server(),
        browser()->tab_strip_model()->GetActiveWebContents(), {"b.com"});

    // Do not expect to see subframe information for c.com. This is because
    // c.com will not require a dedicated process and will be placed in the same
    // process as the main frame (a.com).
    expected_c_subframes = 0;
  }

  auto check_num_subframes = [](int expected_b_subframes,
                                int expected_c_subframes) {
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
        expected_b_subframes, MatchSubframe("http://b.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
        expected_c_subframes, MatchSubframe("http://c.com/")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
        expected_b_subframes + expected_c_subframes, MatchAnySubframe()));
  };
  browser()->OpenURL(content::OpenURLParams(main_url, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));

  // Verify the expected number of b.com and c.com subframes.
  ASSERT_NO_FATAL_FAILURE(check_num_subframes(1, expected_c_subframes));

  // Remember |b_url| to be able to later renavigate to the same URL without
  // doing any process swaps (we want to avoid redirects that would happen
  // when going through /cross-site/foo.com/..., because
  // https://crbug.com/642958 wouldn't repro in presence of process swaps).
  navigation_observer.Wait();
  auto* b_frame =
      ChildFrameAt(browser()->tab_strip_model()->GetActiveWebContents(), 0);
  GURL b_url = b_frame->GetLastCommittedURL();
  ASSERT_EQ(b_url.host(), "b.com");  // Sanity check of test code / setup.
  ASSERT_TRUE(b_frame->GetSiteInstance()->RequiresDedicatedProcess());
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    std::optional<size_t> subframe_b =
        FindResourceIndex(MatchSubframe("http://b.com/"));
    ASSERT_TRUE(subframe_b.has_value());
    ASSERT_TRUE(model()->GetTabId(subframe_b.value()).is_valid());
    model()->Kill(subframe_b.value());

    // Verify the expected number of b.com and c.com subframes.
    ASSERT_NO_FATAL_FAILURE(check_num_subframes(0, expected_c_subframes));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
  }

  HideTaskManager();
  ShowTaskManager();

  // Verify the expected number of b.com and c.com subframes.
  ASSERT_NO_FATAL_FAILURE(check_num_subframes(0, expected_c_subframes));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));

  // Reload the subframe and verify it has re-appeared in the task manager.
  // This is a regression test for https://crbug.com/642958.
  ASSERT_TRUE(content::ExecJs(
      browser()
          ->tab_strip_model()
          ->GetActiveWebContents()
          ->GetPrimaryMainFrame(),
      "document.getElementById('frame1').src = '" + b_url.spec() + "';"));

  // Verify the expected number of b.com and c.com subframes.
  ASSERT_NO_FATAL_FAILURE(check_num_subframes(1, expected_c_subframes));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("cross-site iframe test")));
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
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

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
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

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
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchTab("iframe test")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));

  // Now navigate the tab to a page on a.com with cross-process subframes to
  // b.com and c.com.
  GURL a_dotcom(embedded_test_server()->GetURL(
      "/cross-site/a.com/iframe_cross_site.html"));
  browser()->OpenURL(content::OpenURLParams(a_dotcom, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

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
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

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
  ASSERT_TRUE(content::ExecJs(browser()
                                  ->tab_strip_model()
                                  ->GetActiveWebContents()
                                  ->GetPrimaryMainFrame(),
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
  browser()->OpenURL(
      content::OpenURLParams(a_dotcom_with_iframes, content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

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
  browser()->OpenURL(
      content::OpenURLParams(a_dotcom_simple, content::Referrer(),
                             WindowOpenDisposition::CURRENT_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));

  HideTaskManager();
  ShowTaskManager();

  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
}

// TODO(crbug.com/40710551): disabled as test is flaky.
IN_PROC_BROWSER_TEST_P(TaskManagerOOPIFBrowserTest,
                       DISABLED_OrderingOfDependentRows) {
  ShowTaskManager();

  GURL a_with_frames(embedded_test_server()->GetURL(
      "a.com", "/cross_site_iframe_factory.html?a(b,b,c(d,a,b,c))"));
  browser()->OpenURL(content::OpenURLParams(a_with_frames, content::Referrer(),
                                            WindowOpenDisposition::CURRENT_TAB,
                                            ui::PAGE_TRANSITION_TYPED, false),
                     /*navigation_handle_callback=*/{});

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

  std::optional<size_t> index =
      FindResourceIndex(MatchTab("Cross-site iframe factory"));
  ASSERT_TRUE(index.has_value());
  std::vector<size_t> subframe_offsets;
  if (ShouldExpectSubframes()) {
    std::optional<size_t> index_b =
        FindResourceIndex(MatchSubframe("http://b.com/"));
    ASSERT_TRUE(index_b.has_value());
    std::optional<size_t> index_c =
        FindResourceIndex(MatchSubframe("http://c.com/"));
    ASSERT_TRUE(index_c.has_value());
    std::optional<size_t> index_d =
        FindResourceIndex(MatchSubframe("http://d.com/"));
    ASSERT_TRUE(index_d.has_value());
    subframe_offsets = {index_b.value() - index.value(),
                        index_c.value() - index.value(),
                        index_d.value() - index.value()};
    EXPECT_THAT(subframe_offsets, testing::UnorderedElementsAre(1u, 2u, 3u));
  }

  // Opening a new tab should appear below the existing tab.
  GURL other_tab_url(embedded_test_server()->GetURL(
      "d.com", "/cross_site_iframe_factory.html?d(a(c(b)))"));
  browser()->OpenURL(
      content::OpenURLParams(other_tab_url, content::Referrer(),
                             WindowOpenDisposition::NEW_FOREGROUND_TAB,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

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
  ASSERT_TRUE(index.has_value());

  // All of Tab 2's subframes will reuse Tab 1's existing processes for
  // corresponding sites.  Tab 2's d.com main frame row should then appear
  // after all the subframe processes.
  size_t tab2_subframe_count = ShouldExpectSubframes() ? 3 : 0;
  size_t tab2_main_frame_index =
      index.value() + subframe_offsets.size() + tab2_subframe_count + 1;
  EXPECT_EQ("Tab: Cross-site iframe factory",
            base::UTF16ToUTF8(model()->GetRowTitle(tab2_main_frame_index)));

  if (ShouldExpectSubframes()) {
    // Tab 2's a.com subframe should share Tab 1's main frame process and go
    // directly below it.
    EXPECT_EQ(index.value() + 1,
              FindResourceIndex(MatchSubframe("http://a.com/")));

    // The other tab 2 subframes (b.com and c.com) should join existing
    // subframe processes from tab 1.  Check that the b.com and c.com subframe
    // processes now have two rows each.
    std::optional<size_t> subframe_b_index =
        FindResourceIndex(MatchSubframe("http://b.com/"));
    ASSERT_TRUE(subframe_b_index.has_value());
    std::optional<size_t> subframe_c_index =
        FindResourceIndex(MatchSubframe("http://c.com/"));
    ASSERT_TRUE(subframe_c_index.has_value());
    std::optional<size_t> subframe_d_index =
        FindResourceIndex(MatchSubframe("http://d.com/"));
    ASSERT_TRUE(subframe_d_index.has_value());
    EXPECT_EQ(
        "Subframe: http://b.com/",
        base::UTF16ToUTF8(model()->GetRowTitle(subframe_b_index.value() + 1)));
    EXPECT_EQ(
        "Subframe: http://c.com/",
        base::UTF16ToUTF8(model()->GetRowTitle(subframe_c_index.value() + 1)));

    // The subframe processes should preserve their relative ordering.
    EXPECT_EQ(subframe_offsets[0] < subframe_offsets[1],
              subframe_b_index < subframe_c_index);
    EXPECT_EQ(subframe_offsets[1] < subframe_offsets[2],
              subframe_c_index < subframe_d_index);
    EXPECT_EQ(subframe_offsets[0] < subframe_offsets[2],
              subframe_b_index < subframe_d_index);
  }
}

//==============================================================================
// Prerender tasks test.
namespace {
// Prerender trigger page URL.
const char kMainPageUrl[] = "/title2.html";
// The prerendered URL.
const char kPrerenderURL[] = "/title1.html";

class AutocompleteActionPredictorObserverImpl
    : public predictors::AutocompleteActionPredictor::Observer {
 public:
  explicit AutocompleteActionPredictorObserverImpl(
      predictors::AutocompleteActionPredictor* predictor) {
    observation_.Observe(predictor);
  }

  ~AutocompleteActionPredictorObserverImpl() override = default;

  void WaitForInitialization() {
    base::RunLoop loop;
    waiting_ = loop.QuitClosure();
    loop.Run();
  }

  // predictors::AutocompleteActionPredictor::Observer:
  void OnInitialized() override {
    DCHECK(waiting_);
    std::move(waiting_).Run();
  }

  base::ScopedObservation<predictors::AutocompleteActionPredictor,
                          predictors::AutocompleteActionPredictor::Observer>
      observation_{this};
  base::OnceClosure waiting_;
};

class PrerenderTaskBrowserTest : public TaskManagerBrowserTest {
 public:
  PrerenderTaskBrowserTest() {
    // `blink::features::kPrerender2` and
    // `blink::features::kPrerender2MemoryControls` are enabled in
    // |prerender_helper_|.
    prerender_helper_ = std::make_unique<content::test::PrerenderTestHelper>(
        base::BindRepeating(&PrerenderTaskBrowserTest::GetActiveWebContents,
                            base::Unretained(this)));
    feature_list_.InitWithFeaturesAndParameters(
        content::GetDefaultEnabledBackForwardCacheFeaturesForTesting(
            /*ignore_outstanding_network_request=*/false),
        /*disabled_features=*/{});
    EXPECT_TRUE(content::BackForwardCache::IsBackForwardCacheFeatureEnabled());
  }
  PrerenderTaskBrowserTest(const PrerenderTaskBrowserTest&) = delete;
  PrerenderTaskBrowserTest& operator=(const PrerenderTaskBrowserTest&) = delete;
  ~PrerenderTaskBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
    ASSERT_TRUE(content::AreAllSitesIsolatedForTesting());
    TaskManagerBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    embedded_test_server()->StartAcceptingConnections();
  }

  void NavigateTo(std::string_view page_url) const {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), embedded_test_server()->GetURL(page_url)));
  }

  WebContents* GetActiveWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  predictors::AutocompleteActionPredictor* GetAutocompleteActionPredictor() {
    return predictors::AutocompleteActionPredictorFactory::GetForProfile(
        browser()->profile());
  }

  void WaitForAutocompleteActionPredictorInitialization() {
    if (GetAutocompleteActionPredictor()->initialized()) {
      return;
    }
    AutocompleteActionPredictorObserverImpl predictor_observer(
        GetAutocompleteActionPredictor());
    predictor_observer.WaitForInitialization();
  }

  WebContents* NavigateToURLWithDispositionAndTransition(
      const GURL& url,
      WindowOpenDisposition disposition,
      ui::PageTransition transition) {
    return GetActiveWebContents()->OpenURL(
        content::OpenURLParams(url, content::Referrer(), disposition,
                               transition,
                               /*is_renderer_initiated=*/false),
        /*navigation_handle_callback=*/{});
  }

  content::test::PrerenderTestHelper* prerender_helper() {
    return prerender_helper_.get();
  }

  // Prerender's task title is constructed from |RFH->GetLastCommittedURL|,
  // which contains the port of the testing webserver.
  std::string port() const {
    return base::NumberToString(embedded_test_server()->port());
  }

 private:
  std::unique_ptr<content::test::PrerenderTestHelper> prerender_helper_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// TODO(crbug.com/40232771): Flaky on Windows7.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ProperlyShowsTasks DISABLED_ProperlyShowsTasks
#else
#define MAYBE_ProperlyShowsTasks ProperlyShowsTasks
#endif
// Tests that the task manager properly:
// 1. shows the Prerender entry when the speculation rule is injected;
// 2. shows the Prerender entry when the manager is closed and reopened.
// 3. deletes the Prerender entry when the prerendered page is activated.
IN_PROC_BROWSER_TEST_F(PrerenderTaskBrowserTest, MAYBE_ProperlyShowsTasks) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(kMainPageUrl);

  const auto prerender_gurl = embedded_test_server()->GetURL(kPrerenderURL);
  std::string server_port;
  if (prerender_gurl.has_port()) {
    server_port = prerender_gurl.port();
  }

  // Inject the speculation rule and wait for prerender to complete.
  prerender_helper()->AddPrerender(prerender_gurl);

  // Must have one tab task, one prerender task.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrerender()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrerender(prerender_gurl.spec())));

  // "Close" the task manager and "reopen" it. We should see the same tasks.
  HideTaskManager();
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrerender()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrerender(prerender_gurl.spec())));

  // Activate the prerender page. The triggering page is placed in BFCache,
  // and the prerendered page is activated.
  content::test::PrerenderHostObserver obs(*GetActiveWebContents(),
                                           prerender_gurl);
  content::test::PrerenderTestHelper::NavigatePrimaryPage(
      *GetActiveWebContents(), prerender_gurl);
  ASSERT_TRUE(obs.was_activated());

  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyBFCache()));
  // Take out the "http://".
  const auto tab_title =
      url_formatter::FormatUrl(embedded_test_server()->GetURL(kPrerenderURL));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab(base::UTF16ToUTF8(tab_title))));
  if (content::SiteIsolationPolicy::AreOriginKeyedProcessesEnabledByDefault() &&
      !server_port.empty()) {
    // When kOriginKeyedProcessesByDefault is enabled, we need to include the
    // port number as the SiteInstance's site_url will include it.
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
        1, MatchBFCache("http://127.0.0.1:" + server_port + "/")));
  } else {
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchBFCache("http://127.0.0.1/")));
  }
}

// TODO(crbug.com/40232771): Flaky on Windows7.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DeletesTaskAfterPrerenderKilled \
  DISABLED_DeletesTaskAfterPrerenderKilled
#else
#define MAYBE_DeletesTaskAfterPrerenderKilled DeletesTaskAfterPrerenderKilled
#endif
// Tests that the task manager properly deletes the prerender task once the
// prerender is cancelled.
IN_PROC_BROWSER_TEST_F(PrerenderTaskBrowserTest,
                       MAYBE_DeletesTaskAfterPrerenderKilled) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(kMainPageUrl);

  const auto prerender_gurl = embedded_test_server()->GetURL(kPrerenderURL);
  prerender_helper()->AddPrerender(prerender_gurl);

  // Must have one tab task, one prerender task.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrerender()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrerender(prerender_gurl.spec())));

  // Terminate the prerender task, which should signal the task manager to
  // remove the prerender task entry.
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    std::optional<size_t> prerender_row =
        FindResourceIndex(MatchPrerender(prerender_gurl.spec()));
    ASSERT_TRUE(prerender_row.has_value());
    ASSERT_TRUE(model()->GetTabId(prerender_row.value()).is_valid());
    model()->Kill(prerender_row.value());
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyPrerender()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  }
}

// TODO(crbug.com/40232771): Flaky on Windows7.
#if BUILDFLAG(IS_WIN)
#define MAYBE_DeletesTaskAfterTriggerPageKilled \
  DISABLED_DeletesTaskAfterTriggerPageKilled
#else
#define MAYBE_DeletesTaskAfterTriggerPageKilled \
  DeletesTaskAfterTriggerPageKilled
#endif
// Tests that the task manager properly deletes the task of the trigger tab and
// prerender when the trigger is terminated.
IN_PROC_BROWSER_TEST_F(PrerenderTaskBrowserTest,
                       MAYBE_DeletesTaskAfterTriggerPageKilled) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(kMainPageUrl);

  const auto prerender_gurl = embedded_test_server()->GetURL(kPrerenderURL);
  prerender_helper()->AddPrerender(prerender_gurl);

  // Must have one tab task, one prerender task.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrerender()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrerender(prerender_gurl.spec())));

  // Terminate the prerender task, which should signal the task manager to
  // remove the prerender task entry.
  {
    base::HistogramTester histogram_tester;
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    std::optional<size_t> trigger_row =
        FindResourceIndex(MatchTab("Title Of Awesomeness"));
    ASSERT_TRUE(trigger_row.has_value());
    ASSERT_TRUE(model()->GetTabId(trigger_row.value()).is_valid());
    model()->Kill(trigger_row.value());
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyTab()));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyPrerender()));
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus.SpeculationRule",
        /*PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled=*/57, 1);
  }
}

// TODO(crbug.com/40232771): Flaky on Windows7.
#if BUILDFLAG(IS_WIN)
#define MAYBE_ProperlyShowsPrerenderTaskByAutocompletePredictor \
  DISABLED_ProperlyShowsPrerenderTaskByAutocompletePredictor
#else
#define MAYBE_ProperlyShowsPrerenderTaskByAutocompletePredictor \
  ProperlyShowsPrerenderTaskByAutocompletePredictor
#endif
// Test that the autocomplete action predictor trigger Prerender tasks are
// properly displayed. Such predictor is used to trigger Omnibox Prerender.
IN_PROC_BROWSER_TEST_F(
    PrerenderTaskBrowserTest,
    MAYBE_ProperlyShowsPrerenderTaskByAutocompletePredictor) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(kMainPageUrl);

  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  const auto prerender_gurl = embedded_test_server()->GetURL(kPrerenderURL);
  GetAutocompleteActionPredictor()->StartPrerendering(
      prerender_gurl, *(browser()->tab_strip_model()->GetActiveWebContents()),
      gfx::Size(50, 50));

  // One task for main page and one for the prerendered page.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrerender()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrerender(prerender_gurl.spec())));
  // Main task stays after prerendered task is terminated.
  {
    base::HistogramTester histogram_tester;
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    std::optional<size_t> prerender_row =
        FindResourceIndex(MatchPrerender(prerender_gurl.spec()));
    ASSERT_TRUE(prerender_row.has_value());
    ASSERT_TRUE(model()->GetTabId(prerender_row.value()).is_valid());
    model()->Kill(prerender_row.value());
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyPrerender()));
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
        "DirectURLInput",
        /*PrerenderFinalStatus::kRendererProcessKilled=*/14, 1);
  }
  // Both tasks are deleted after main task is terminated.
  {
    // Use a different URL because re-using the same URL does not trigger new
    // prerendering:
    // https://crsrc.org/c/chrome/browser/predictors/autocomplete_action_predictor.cc;l=208;drc=a08a4e1c3f6862b3b1385b8a040a4fdb524e509d
    base::HistogramTester histogram_tester;
    const char kNewPrerenderURL[] = "/title3.html";
    const auto new_prerender_gurl =
        embedded_test_server()->GetURL(kNewPrerenderURL);
    GetAutocompleteActionPredictor()->StartPrerendering(
        embedded_test_server()->GetURL(kNewPrerenderURL),
        *GetActiveWebContents(), gfx::Size(50, 50));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrerender()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchPrerender(new_prerender_gurl.spec())));

    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    std::optional<size_t> trigger_row =
        FindResourceIndex(MatchTab("Title Of Awesomeness"));
    ASSERT_TRUE(trigger_row.has_value());
    ASSERT_TRUE(model()->GetTabId(trigger_row.value()).is_valid());
    model()->Kill(trigger_row.value());
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyTab()));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyPrerender()));
    histogram_tester.ExpectUniqueSample(
        "Prerender.Experimental.PrerenderHostFinalStatus.Embedder_"
        "DirectURLInput",
        /*PrerenderFinalStatus::kPrimaryMainFrameRendererProcessKilled=*/57, 1);
  }
}

// TODO(crbug.com/40232771): Flaky on Windows7.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OmniboxPrerenderActivationClearsTask \
  DISABLED_OmniboxPrerenderActivationClearsTask
#else
#define MAYBE_OmniboxPrerenderActivationClearsTask \
  OmniboxPrerenderActivationClearsTask
#endif
// Test that the Omnibox-triggered prerender activation clears the prerender
// entry in the task manager.
IN_PROC_BROWSER_TEST_F(PrerenderTaskBrowserTest,
                       MAYBE_OmniboxPrerenderActivationClearsTask) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(kMainPageUrl);

  ASSERT_TRUE(GetAutocompleteActionPredictor());
  WaitForAutocompleteActionPredictorInitialization();
  const auto prerender_gurl = embedded_test_server()->GetURL(kPrerenderURL);
  GetAutocompleteActionPredictor()->StartPrerendering(
      prerender_gurl, *GetActiveWebContents(), gfx::Size(50, 50));

  // One task for main page and one for the prerendered page.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyPrerender()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchPrerender(prerender_gurl.spec())));

  // Activate the Omnibox prerender, after which the prerender task should
  // disappear.
  content::test::PrerenderHostObserver obs(*GetActiveWebContents(),
                                           prerender_gurl);
  // |ui::PAGE_TRANSITION_FROM_ADDRESS_BAR| augmentation is required for omnibox
  // activation.
  auto* web_contents = NavigateToURLWithDispositionAndTransition(
      prerender_gurl, WindowOpenDisposition::CURRENT_TAB,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  obs.WaitForActivation();
  ASSERT_TRUE(obs.was_activated());
  ASSERT_EQ(web_contents, GetActiveWebContents());  // Current tab.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyPrerender()));
  // Take out the "http://".
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1,
      MatchTab(base::UTF16ToUTF8(url_formatter::FormatUrl(prerender_gurl)))));
}

//==============================================================================
// FencedFrame tasks test.
namespace {

class FencedFrameTaskBrowserTest : public TaskManagerBrowserTest {
 public:
  FencedFrameTaskBrowserTest() {
    EXPECT_TRUE(blink::features::IsFencedFramesEnabled());
  }
  FencedFrameTaskBrowserTest(const FencedFrameTaskBrowserTest&) = delete;
  FencedFrameTaskBrowserTest& operator=(const FencedFrameTaskBrowserTest&) =
      delete;
  ~FencedFrameTaskBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    content::IsolateAllSitesForTesting(command_line);
    ASSERT_TRUE(content::AreAllSitesIsolatedForTesting());
    TaskManagerBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    https_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    https_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    content::SetupCrossSiteRedirector(https_server());
    ASSERT_TRUE(https_server()->InitializeAndListen());
    https_server()->StartAcceptingConnections();
  }

  void NavigateTo(Browser* browser,
                  std::string_view host,
                  std::string_view rel_url) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser, https_server()->GetURL(host, rel_url)));
  }

  std::string GetFencedFrameTitle(const GURL& url) const {
    GURL::Replacements replacements;
    replacements.ClearPath();
    replacements.ClearRef();
    if (!content::SiteIsolationPolicy::
            AreOriginKeyedProcessesEnabledByDefault()) {
      // Only include the port for origin-isolated urls.
      replacements.ClearPort();
    }
    return url.ReplaceComponents(replacements).spec();
  }

  net::EmbeddedTestServer* https_server() { return &https_server_; }

  content::test::FencedFrameTestHelper* helper() { return helper_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_;
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<content::test::FencedFrameTestHelper> helper_ =
      std::make_unique<content::test::FencedFrameTestHelper>();
};

// TODO(crbug.com/40285326): This fails with the field trial testing config.
class FencedFrameTaskBrowserTestNoTestingConfig
    : public FencedFrameTaskBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    FencedFrameTaskBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch("disable-field-trial-config");
  }
};

}  // namespace

// Testing that the task manager properly displays fenced frame tasks with
// re-opening task manager, and with fenced frame navigations.
IN_PROC_BROWSER_TEST_F(FencedFrameTaskBrowserTestNoTestingConfig,
                       ProperlyShowsTasks) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(browser(), "a.test", "/title2.html");
  // Create two fenced frames.
  auto* main_frame = browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetPrimaryMainFrame();
  const auto initial_gurl =
      https_server()->GetURL("a.test", "/fenced_frames/title1.html");
  content::RenderFrameHostWrapper fenced_frame_rfh(
      helper()->CreateFencedFrame(main_frame, initial_gurl));
  ASSERT_TRUE(fenced_frame_rfh);

  // One task for the embedder. Same origin fenced frame does not show up in the
  // task manager.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));

  // Navigate the same-site FF to a cross-site url. The changes should be
  // reflected in the task manager.
  const auto cross_site_gurl =
      https_server()->GetURL("b.test", "/fenced_frames/title2.html");
  helper()->NavigateFrameInFencedFrameTree(fenced_frame_rfh.get(),
                                           cross_site_gurl);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchFencedFrame(GetFencedFrameTitle(cross_site_gurl))));

  // Close the task manager and re-open it, all tasks should be re-created.
  HideTaskManager();
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchFencedFrame(GetFencedFrameTitle(cross_site_gurl))));

  // Terminate the fenced frame. The embedder frame remains intact.
  {
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    std::optional<size_t> fenced_frame_row = FindResourceIndex(
        MatchFencedFrame(GetFencedFrameTitle(cross_site_gurl)));
    ASSERT_TRUE(fenced_frame_row.has_value());
    ASSERT_TRUE(model()->GetTabId(fenced_frame_row.value()).is_valid());
    model()->Kill(fenced_frame_row.value());
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyFencedFrame()));
    ASSERT_NO_FATAL_FAILURE(
        WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  }
  // Re-create the fenced frame and terminate the embedding frame. The
  // embedder's task and the remaining fenced frame tasks are destroyed.
  {
    helper()->CreateFencedFrame(main_frame, initial_gurl);
    content::ScopedAllowRendererCrashes scoped_allow_renderer_crashes;
    std::optional<size_t> embedder_row =
        FindResourceIndex(MatchTab("Title Of Awesomeness"));
    ASSERT_TRUE(embedder_row.has_value());
    ASSERT_TRUE(model()->GetTabId(embedder_row.value()).is_valid());
    model()->Kill(embedder_row.value());
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyTab()));
    ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyFencedFrame()));
  }
}

// Test that the empty fenced frame (one without a `src`) is not shown in the
// task manager. Not shown because we cannot observe any navigation events for
// fenced frame creation (only |RenderFrameCreated| is triggered).
IN_PROC_BROWSER_TEST_F(FencedFrameTaskBrowserTest, EmptyFencedFrameNotShown) {
  const std::string kEmptyFencedFrameSnippet = R"(
    const ff = document.createElement("fencedframe");
    document.body.appendChild(ff);
  )";

  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(browser(), "a.test", "/title2.html");

  auto* main_frame = browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetPrimaryMainFrame();
  ASSERT_TRUE(content::ExecJs(main_frame, kEmptyFencedFrameSnippet));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnyFencedFrame()));

  // Navigation on the empty fenced frame should create an entry.
  auto* fenced_frame_rfh =
      content::test::FencedFrameTestHelper::GetMostRecentlyAddedFencedFrame(
          main_frame);
  ASSERT_NE(fenced_frame_rfh, nullptr);
  const auto fenced_frame_gurl =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  helper()->NavigateFrameInFencedFrameTree(fenced_frame_rfh, fenced_frame_gurl);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchFencedFrame(GetFencedFrameTitle(fenced_frame_gurl))));
}

// Tests that the task manager properly shows tasks in Incognito mode.
IN_PROC_BROWSER_TEST_F(FencedFrameTaskBrowserTest, ShowsIncognitoTask) {
  auto* incognito_browser = CreateIncognitoBrowser();
  ASSERT_NE(incognito_browser, nullptr);
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(incognito_browser, "a.test", "/title2.html");
  auto* main_frame = incognito_browser->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetPrimaryMainFrame();
  const auto fenced_frame_gurl =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  content::RenderFrameHostWrapper ff_rfh(
      helper()->CreateFencedFrame(main_frame, fenced_frame_gurl));
  ASSERT_TRUE(ff_rfh);
  // Two tasks: one for the incognito main frame and another for the incognito
  // fenced frames.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyIncognitoTab()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchAnyIncognitoFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchIncognitoTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchIncognitoFencedFrame(GetFencedFrameTitle(fenced_frame_gurl))));
}

// Test that clicking on the task manager fenced frame task row brings the focus
// to the embedder page.
IN_PROC_BROWSER_TEST_F(FencedFrameTaskBrowserTest, TaskActivationChangesFocus) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(browser(), "a.test", "/title2.html");
  // Create one fenced frame.
  auto* main_frame = browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetPrimaryMainFrame();
  const auto fenced_frame_gurl =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  content::RenderFrameHostWrapper ff_rfh(
      helper()->CreateFencedFrame(main_frame, fenced_frame_gurl));
  ASSERT_TRUE(ff_rfh);

  // One main tab task, one fenced frame task.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchFencedFrame(GetFencedFrameTitle(fenced_frame_gurl))));

  // Open a new tab of "about:blank". This appends an active WebContents at
  // index 1.
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(url::kAboutBlankURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The WebContents of "about:blank" is active.
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 1);

  const std::optional<size_t> fenced_frame_task_row = FindResourceIndex(
      MatchFencedFrame(GetFencedFrameTitle(fenced_frame_gurl)));
  ASSERT_TRUE(fenced_frame_task_row.has_value());
  model()->Activate(fenced_frame_task_row.value());

  // The WebContents of the embedder page is active.
  ASSERT_EQ(browser()->tab_strip_model()->active_index(), 0);
}

// Test that same-document navigation does not change the task's title.
IN_PROC_BROWSER_TEST_F(FencedFrameTaskBrowserTest,
                       NoTitleChangeForSameDocNavigation) {
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAboutBlankTab()));

  NavigateTo(browser(), "a.test", "/title2.html");
  // Create one fenced frame.
  auto* main_frame = browser()
                         ->tab_strip_model()
                         ->GetActiveWebContents()
                         ->GetPrimaryMainFrame();
  const auto fenced_frame_gurl =
      https_server()->GetURL("b.test", "/fenced_frames/title1.html");
  content::RenderFrameHostWrapper ff_rfh(
      helper()->CreateFencedFrame(main_frame, fenced_frame_gurl));
  ASSERT_TRUE(ff_rfh);

  // One main tab task, one fenced frame task.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchFencedFrame(GetFencedFrameTitle(fenced_frame_gurl))));

  // Same-doc navigation of the fenced frame.
  const auto same_doc_navi_gurl = https_server()->GetURL(
      "b.test", base::StrCat({"/fenced_frames/title1.html", "#same_doc_navi"}));
  helper()->NavigateFrameInFencedFrameTree(ff_rfh.get(), same_doc_navi_gurl);
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyFencedFrame()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(
      1, MatchFencedFrame(GetFencedFrameTitle(fenced_frame_gurl))));
}

// Asserts that the task manager does not attempt to create any task for a RFH
// in `kPendingCommit` or `kPendingDeletion` state. Creating tasks during these
// two states will trigger a `NOTREACHED()` in
// `WebContentsTaskProvider::WebContentsEntry::CreateTaskForFrame`.
IN_PROC_BROWSER_TEST_F(TaskManagerBrowserTest,
                       NoCrashOnPendingCommitPendingDeletaionRFH) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("a.test", "/title2.html")));
  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* main_frame = web_contents->GetPrimaryMainFrame();

  const std::string kCreateAndNavigateIFrame = R"(
    const iframe = document.createElement("iframe");
    iframe.src = $1;
    document.body.appendChild(iframe);
  )";

  // Create a cross-origin iframe, because we don't show tasks for iframes of
  // the same origin.
  const GURL cross_origin_subframe_url =
      embedded_test_server()->GetURL("b.test", "/title3.html");
  content::TestNavigationManager nav_obs(web_contents,
                                         cross_origin_subframe_url);
  ASSERT_TRUE(ExecJs(
      main_frame,
      content::JsReplace(kCreateAndNavigateIFrame, cross_origin_subframe_url)));
  ASSERT_TRUE(nav_obs.WaitForRequestStart());

  ShowTaskManager();
  // Main frame. The task manager does not create tasks for speculative RFHs.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));

  nav_obs.ResumeNavigation();
  ASSERT_TRUE(nav_obs.WaitForNavigationFinished());

  // Main frame + subframe after the navigation is resumed.
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnySubframe()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchSubframe("http://b.test/")));

  HideTaskManager();
  // Get hold of the subframe RFH, and stop it from being deleted.
  content::RenderFrameHostWrapper subframe_rfh(
      content::ChildFrameAt(main_frame, 0));
  content::LeaveInPendingDeletionState(subframe_rfh.get());

  const std::string kRemoveIFrame = R"(
    const iframe = document.querySelector('iframe');
    document.body.removeChild(iframe);
  )";
  ASSERT_TRUE(ExecJs(main_frame, kRemoveIFrame));

  // The `kPendingDeletion` subframe RFH is not destroyed, and reachable from
  // the `WebContents`, so it's possible for
  // `WebContentsTaskProvider::WebContentsEntry::CreateAllTasks()` to create a
  // task for it.
  ASSERT_FALSE(subframe_rfh.IsDestroyed());
  bool reached = false;
  web_contents->ForEachRenderFrameHost([&](content::RenderFrameHost* rfh) {
    if (rfh == subframe_rfh.get()) {
      reached = true;
    }
  });
  ASSERT_TRUE(reached);

  // However we shouldn't create any tasks for a RFH to be deleted.
  ShowTaskManager();
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(1, MatchAnyTab()));
  ASSERT_NO_FATAL_FAILURE(
      WaitForTaskManagerRows(1, MatchTab("Title Of Awesomeness")));
  ASSERT_NO_FATAL_FAILURE(WaitForTaskManagerRows(0, MatchAnySubframe()));
}
