// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_api.h"

#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_builder.h"
#include "ui/display/test/scoped_screen_override.h"
#include "ui/display/test/test_screen.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "chrome/browser/chromeos/policy/dlp/mock_dlp_content_manager.h"
#endif

namespace extensions {

using display::test::ScopedScreenOverride;

namespace {

std::unique_ptr<base::ListValue> RunTabsQueryFunction(
    Browser* browser,
    const Extension* extension,
    const std::string& query_info) {
  scoped_refptr<TabsQueryFunction> function(new TabsQueryFunction());
  function->set_extension(extension);
  std::unique_ptr<base::Value> value(
      extension_function_test_utils::RunFunctionAndReturnSingleResult(
          function.get(), query_info, browser, api_test_utils::NONE));
  return base::ListValue::From(std::move(value));
}

// Creates an extension with "tabs" permission.
scoped_refptr<const Extension> CreateTabsExtension() {
  return ExtensionBuilder("Extension with tabs permission")
      .AddPermission("tabs")
      .Build();
}

// Creates an WebContents with |urls| as history.
std::unique_ptr<content::WebContents> CreateWebContentsWithHistory(
    Profile* profile,
    const std::vector<GURL>& urls) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile, nullptr);

  for (const auto& url : urls) {
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents.get(), url);
    EXPECT_EQ(url, web_contents->GetLastCommittedURL());
    EXPECT_EQ(url, web_contents->GetVisibleURL());
  }

  return web_contents;
}

}  // namespace

class TabsApiUnitTest : public ExtensionServiceTestBase {
 protected:
  TabsApiUnitTest() {}
  ~TabsApiUnitTest() override {}

  Browser* browser() { return browser_.get(); }
  TestBrowserWindow* browser_window() { return browser_window_.get(); }

  TabStripModel* GetTabStripModel() { return browser_->tab_strip_model(); }

 private:
  // ExtensionServiceTestBase:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  display::test::TestScreen test_screen_;

  std::unique_ptr<ScopedScreenOverride> scoped_screen_override_;

  DISALLOW_COPY_AND_ASSIGN(TabsApiUnitTest);
};

void TabsApiUnitTest::SetUp() {
  // Force TabManager/TabLifecycleUnitSource creation.
  g_browser_process->GetTabManager();

  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  browser_window_.reset(new TestBrowserWindow());
  Browser::CreateParams params(profile(), true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_.reset(new Browser(params));
  scoped_screen_override_ =
      std::make_unique<ScopedScreenOverride>(&test_screen_);
}

void TabsApiUnitTest::TearDown() {
  browser_.reset();
  browser_window_.reset();
  ExtensionServiceTestBase::TearDown();
}

TEST_F(TabsApiUnitTest, QueryWithoutTabsPermission) {
  GURL tab_urls[] = {GURL("http://www.google.com"),
                     GURL("http://www.example.com"),
                     GURL("https://www.google.com")};
  std::string tab_titles[] = {"", "Sample title", "Sample title"};

  // Add 3 web contentses to the browser.
  content::WebContents* web_contentses[base::size(tab_urls)];
  for (size_t i = 0; i < base::size(tab_urls); ++i) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContents* raw_web_contents = web_contents.get();
    web_contentses[i] = raw_web_contents;
    browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                    true);
    EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
              raw_web_contents);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(raw_web_contents);
    web_contents_tester->NavigateAndCommit(tab_urls[i]);
    raw_web_contents->GetController().GetVisibleEntry()->SetTitle(
        base::ASCIIToUTF16(tab_titles[i]));
  }

  const char* kTitleAndURLQueryInfo =
      "[{\"title\": \"Sample title\", \"url\": \"*://www.google.com/*\"}]";

  // An extension without "tabs" permission will see none of the 3 tabs.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Test").Build();
  std::unique_ptr<base::ListValue> tabs_list_without_permission(
      RunTabsQueryFunction(browser(), extension.get(), kTitleAndURLQueryInfo));
  ASSERT_TRUE(tabs_list_without_permission);
  EXPECT_EQ(0u, tabs_list_without_permission->GetSize());

  // An extension with "tabs" permission however will see the third tab.
  scoped_refptr<const Extension> extension_with_permission =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Extension with tabs permission")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("permissions", ListBuilder().Append("tabs").Build())
                  .Build())
          .Build();
  std::unique_ptr<base::ListValue> tabs_list_with_permission(
      RunTabsQueryFunction(browser(), extension_with_permission.get(),
                           kTitleAndURLQueryInfo));
  ASSERT_TRUE(tabs_list_with_permission);
  ASSERT_EQ(1u, tabs_list_with_permission->GetSize());

  const base::DictionaryValue* third_tab_info;
  ASSERT_TRUE(tabs_list_with_permission->GetDictionary(0, &third_tab_info));
  int third_tab_id = -1;
  ASSERT_TRUE(third_tab_info->GetInteger("id", &third_tab_id));
  EXPECT_EQ(ExtensionTabUtil::GetTabId(web_contentses[2]), third_tab_id);

  while (!browser()->tab_strip_model()->empty())
    browser()->tab_strip_model()->DetachWebContentsAt(0);
}

TEST_F(TabsApiUnitTest, QueryWithHostPermission) {
  GURL tab_urls[] = {GURL("http://www.google.com"),
                     GURL("http://www.example.com"),
                     GURL("https://www.google.com/test")};
  std::string tab_titles[] = {"", "Sample title", "Sample title"};

  // Add 3 web contentses to the browser.
  content::WebContents* web_contentses[base::size(tab_urls)];
  for (size_t i = 0; i < base::size(tab_urls); ++i) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
    content::WebContents* raw_web_contents = web_contents.get();
    web_contentses[i] = raw_web_contents;
    browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                    true);
    EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
              raw_web_contents);
    content::WebContentsTester* web_contents_tester =
        content::WebContentsTester::For(raw_web_contents);
    web_contents_tester->NavigateAndCommit(tab_urls[i]);
    raw_web_contents->GetController().GetVisibleEntry()->SetTitle(
        base::ASCIIToUTF16(tab_titles[i]));
  }

  const char* kTitleAndURLQueryInfo =
      "[{\"title\": \"Sample title\", \"url\": \"*://www.google.com/*\"}]";

  // An extension with "host" permission will only see the third tab.
  scoped_refptr<const Extension> extension_with_permission =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Extension with tabs permission")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       ListBuilder().Append("*://www.google.com/*").Build())
                  .Build())
          .Build();

  {
    std::unique_ptr<base::ListValue> tabs_list_with_permission(
        RunTabsQueryFunction(browser(), extension_with_permission.get(),
                             kTitleAndURLQueryInfo));
    ASSERT_TRUE(tabs_list_with_permission);
    ASSERT_EQ(1u, tabs_list_with_permission->GetSize());

    const base::DictionaryValue* third_tab_info;
    ASSERT_TRUE(tabs_list_with_permission->GetDictionary(0, &third_tab_info));
    int third_tab_id = -1;
    ASSERT_TRUE(third_tab_info->GetInteger("id", &third_tab_id));
    EXPECT_EQ(ExtensionTabUtil::GetTabId(web_contentses[2]), third_tab_id);
  }

  // Try the same without title, first and third tabs will match.
  const char* kURLQueryInfo = "[{\"url\": \"*://www.google.com/*\"}]";
  {
    std::unique_ptr<base::ListValue> tabs_list_with_permission(
        RunTabsQueryFunction(browser(), extension_with_permission.get(),
                             kURLQueryInfo));
    ASSERT_TRUE(tabs_list_with_permission);
    ASSERT_EQ(2u, tabs_list_with_permission->GetSize());

    const base::DictionaryValue* first_tab_info;
    const base::DictionaryValue* third_tab_info;
    ASSERT_TRUE(tabs_list_with_permission->GetDictionary(0, &first_tab_info));
    ASSERT_TRUE(tabs_list_with_permission->GetDictionary(1, &third_tab_info));

    std::vector<int> expected_tabs_ids;
    expected_tabs_ids.push_back(ExtensionTabUtil::GetTabId(web_contentses[0]));
    expected_tabs_ids.push_back(ExtensionTabUtil::GetTabId(web_contentses[2]));

    int first_tab_id = -1;
    ASSERT_TRUE(first_tab_info->GetInteger("id", &first_tab_id));
    EXPECT_TRUE(base::Contains(expected_tabs_ids, first_tab_id));

    int third_tab_id = -1;
    ASSERT_TRUE(third_tab_info->GetInteger("id", &third_tab_id));
    EXPECT_TRUE(base::Contains(expected_tabs_ids, third_tab_id));
  }
  while (!browser()->tab_strip_model()->empty())
    browser()->tab_strip_model()->DetachWebContentsAt(0);
}

// Test that using the PDF extension for tab updates is treated as a
// renderer-initiated navigation. crbug.com/660498
TEST_F(TabsApiUnitTest, PDFExtensionNavigation) {
  DictionaryBuilder manifest;
  manifest.Set("name", "pdfext")
      .Set("description", "desc")
      .Set("version", "0.1")
      .Set("manifest_version", 2)
      .Set("permissions", ListBuilder().Append("tabs").Build());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(manifest.Build())
          .SetID(extension_misc::kPdfExtensionId)
          .Build();
  ASSERT_TRUE(extension);

  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContents* raw_web_contents = web_contents.get();
  ASSERT_TRUE(raw_web_contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_web_contents);
  const GURL kGoogle("http://www.google.com");
  web_contents_tester->NavigateAndCommit(kGoogle);
  EXPECT_EQ(kGoogle, raw_web_contents->GetLastCommittedURL());
  EXPECT_EQ(kGoogle, raw_web_contents->GetVisibleURL());

  CreateSessionServiceTabHelper(raw_web_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_web_contents).id();
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  true);

  scoped_refptr<TabsUpdateFunction> function = new TabsUpdateFunction();
  function->set_extension(extension.get());
  function->set_browser_context(profile());
  std::unique_ptr<base::ListValue> args(
      extension_function_test_utils::ParseList(
          base::StringPrintf(R"([%d, {"url":"http://example.com"}])", tab_id)));
  function->SetArgs(base::Value::FromUniquePtrValue(std::move(args)));
  api_test_utils::SendResponseHelper response_helper(function.get());
  function->RunWithValidation()->Execute();

  EXPECT_EQ(kGoogle, raw_web_contents->GetLastCommittedURL());
  EXPECT_EQ(kGoogle, raw_web_contents->GetVisibleURL());

  // Clean up.
  response_helper.WaitForResponse();
  while (!browser()->tab_strip_model()->empty())
    browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  base::RunLoop().RunUntilIdle();
}

// Tests that non-validation failure in tabs.executeScript results in error, and
// not bad_message.
// Regression test for https://crbug.com/642794.
TEST_F(TabsApiUnitTest, ExecuteScriptNoTabIsNonFatalError) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();
  scoped_refptr<TabsExecuteScriptFunction> function(
      new TabsExecuteScriptFunction());
  function->set_extension(extension_with_tabs_permission);
  const char* kArgs = R"(["", {"code": ""}])";
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), kArgs,
      browser(),  // browser() doesn't have any tabs.
      api_test_utils::NONE);
  EXPECT_EQ(tabs_constants::kNoTabInBrowserWindowError, error);
}

// Tests that calling chrome.tabs.update updates the URL as expected.
TEST_F(TabsApiUnitTest, TabsUpdate) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("UpdateTest").Build();
  const GURL kExampleCom("http://example.com");
  const GURL kChromiumOrg("https://chromium.org");

  // Add a web contents to the browser.
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), raw_contents);
  CreateSessionServiceTabHelper(raw_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_contents).id();

  // Navigate the browser to example.com
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_contents);
  web_contents_tester->NavigateAndCommit(kExampleCom);
  EXPECT_EQ(kExampleCom, raw_contents->GetLastCommittedURL());

  // Use the TabsUpdateFunction to navigate to chromium.org
  auto function = base::MakeRefCounted<TabsUpdateFunction>();
  function->set_extension(extension);
  static constexpr char kFormatArgs[] = R"([%d, {"url": "%s"}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_id, kChromiumOrg.spec().c_str());
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));
  content::NavigationController& controller =
      browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  content::RenderFrameHostTester::CommitPendingLoad(&controller);
  EXPECT_EQ(kChromiumOrg, raw_contents->GetLastCommittedURL());

  // Clean up.
  browser()->tab_strip_model()->CloseAllTabs();
}

// Tests that calling chrome.tabs.update with a JavaScript URL results
// in an error.
TEST_F(TabsApiUnitTest, TabsUpdateJavaScriptUrlNotAllowed) {
  // An extension with access to www.example.com.
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(
              DictionaryBuilder()
                  .Set("name", "Extension with a host permission")
                  .Set("version", "1.0")
                  .Set("manifest_version", 2)
                  .Set("permissions",
                       ListBuilder().Append("http://www.example.com/*").Build())
                  .Build())
          .Build();
  auto function = base::MakeRefCounted<TabsUpdateFunction>();
  function->set_extension(extension);

  // Add a web contents to the browser.
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
  content::WebContents* raw_contents = contents.get();
  browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(), raw_contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_contents);
  web_contents_tester->NavigateAndCommit(GURL("http://www.example.com"));
  CreateSessionServiceTabHelper(raw_contents);
  int tab_id = sessions::SessionTabHelper::IdForTab(raw_contents).id();

  static constexpr char kFormatArgs[] = R"([%d, {"url": "%s"}])";
  const std::string args = base::StringPrintf(
      kFormatArgs, tab_id, "javascript:void(document.title = 'Won't work')");
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), args, browser(), api_test_utils::NONE);
  EXPECT_EQ(tabs_constants::kJavaScriptUrlsNotAllowedInTabsUpdate, error);

  // Clean up.
  browser()->tab_strip_model()->CloseAllTabs();
}

// Test that the tabs.move() function correctly rearranges sets of tabs within a
// single window.
TEST_F(TabsApiUnitTest, TabsMoveWithinWindow) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("MoveWithinWindowTest").Build();

  // Add several web contents to the browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    browser()->tab_strip_model()->AppendWebContents(std::move(contents),
                                                    /* foreground */ true);
  }
  ASSERT_EQ(kNumTabs, browser()->tab_strip_model()->count());

  // Use the TabsMoveFunction to move tabs 0, 2, and 4 to index 1.
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] = R"([[%d, %d, %d], {"index": 1}])";
  const std::string args =
      base::StringPrintf(kFormatArgs, tab_ids[0], tab_ids[2], tab_ids[4]);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(0), web_contentses[1]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(1), web_contentses[0]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(2), web_contentses[2]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(3), web_contentses[4]);
  EXPECT_EQ(tab_strip_model->GetWebContentsAt(4), web_contentses[3]);

  // Clean up.
  browser()->tab_strip_model()->CloseAllTabs();
}

// Test that the tabs.move() function correctly rearranges sets of tabs across
// windows.
TEST_F(TabsApiUnitTest, TabsMoveAcrossWindows) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("MoveAcrossWindowTest").Build();

  // Add several web contents to the original browser and get their tab IDs.
  constexpr int kNumTabs = 5;
  std::vector<int> tab_ids;
  std::vector<content::WebContents*> web_contentses;
  for (int i = 0; i < kNumTabs; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));

    CreateSessionServiceTabHelper(contents.get());
    tab_ids.push_back(
        sessions::SessionTabHelper::IdForTab(contents.get()).id());
    web_contentses.push_back(contents.get());

    browser()->tab_strip_model()->AppendWebContents(std::move(contents),
                                                    /* foreground */ true);
  }
  ASSERT_EQ(kNumTabs, browser()->tab_strip_model()->count());

  // Create a new window and add a few tabs, getting the ID of the last tab.
  TestBrowserWindow* window2 = new TestBrowserWindow;
  // TestBrowserWindowOwner handles its own lifetime, and also cleans up
  // |window2|.
  new TestBrowserWindowOwner(window2);
  Browser::CreateParams params(profile(), /* user_gesture */ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = window2;
  std::unique_ptr<Browser> browser2 = std::make_unique<Browser>(params);
  BrowserList::SetLastActive(browser2.get());
  int window_id2 = ExtensionTabUtil::GetWindowId(browser2.get());

  constexpr int kNumTabs2 = 3;
  for (int i = 0; i < kNumTabs2; ++i) {
    std::unique_ptr<content::WebContents> contents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr));
    CreateSessionServiceTabHelper(contents.get());
    browser2->tab_strip_model()->AppendWebContents(std::move(contents),
                                                   /* foreground */ true);
  }
  ASSERT_EQ(kNumTabs2, browser2->tab_strip_model()->count());

  content::WebContents* web_contents2 =
      browser2->tab_strip_model()->GetWebContentsAt(2);
  int tab_id2 = sessions::SessionTabHelper::IdForTab(web_contents2).id();

  // Use the TabsMoveFunction to move tab 2 from browser2 and tabs 0, 2, and 4
  // from the original browser to index 1 of browser2.
  constexpr int kNumTabsMovedAcrossWindows = 3;
  auto function = base::MakeRefCounted<TabsMoveFunction>();
  function->set_extension(extension);
  constexpr char kFormatArgs[] =
      R"([[%d, %d, %d, %d], {"windowId": %d, "index": 1}])";
  const std::string args = base::StringPrintf(
      kFormatArgs, tab_id2, tab_ids[0], tab_ids[2], tab_ids[4], window_id2);
  ASSERT_TRUE(extension_function_test_utils::RunFunction(
      function.get(), args, browser(), api_test_utils::NONE));

  TabStripModel* tab_strip_model2 = browser2->tab_strip_model();
  ASSERT_EQ(kNumTabs2 + kNumTabsMovedAcrossWindows, tab_strip_model2->count());
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(1), web_contents2);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(2), web_contentses[0]);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(3), web_contentses[2]);
  EXPECT_EQ(tab_strip_model2->GetWebContentsAt(4), web_contentses[4]);

  // Clean up.
  browser()->tab_strip_model()->CloseAllTabs();
  browser2->tab_strip_model()->CloseAllTabs();
}

TEST_F(TabsApiUnitTest, TabsGoForwardNoSelectedTabError) {
  scoped_refptr<const Extension> extension = CreateTabsExtension();
  auto function = base::MakeRefCounted<TabsGoForwardFunction>();
  function->set_extension(extension);
  // No active tab results in an error.
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[]",
      browser(),  // browser() doesn't have any tabs.
      api_test_utils::NONE);
  EXPECT_EQ(tabs_constants::kNoSelectedTabError, error);
}

TEST_F(TabsApiUnitTest, TabsGoForwardAndBack) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();

  const std::vector<GURL> urls = {GURL("http://www.foo.com"),
                                  GURL("http://www.bar.com")};
  std::unique_ptr<content::WebContents> web_contents =
      CreateWebContentsWithHistory(profile(), urls);
  content::WebContents* raw_web_contents = web_contents.get();
  ASSERT_TRUE(raw_web_contents);

  CreateSessionServiceTabHelper(raw_web_contents);
  const int tab_id =
      sessions::SessionTabHelper::IdForTab(raw_web_contents).id();
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  /* foreground */ true);
  // Go back with chrome.tabs.goBack.
  auto goback_function = base::MakeRefCounted<TabsGoBackFunction>();
  goback_function->set_extension(extension_with_tabs_permission.get());
  extension_function_test_utils::RunFunction(
      goback_function.get(), base::StringPrintf("[%d]", tab_id), browser(),
      api_test_utils::INCLUDE_INCOGNITO);

  content::WebContents* active_webcontent =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::NavigationController& controller =
      active_webcontent->GetController();
  content::RenderFrameHostTester::CommitPendingLoad(&controller);
  EXPECT_EQ(urls[0], raw_web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[0], raw_web_contents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // Go forward with chrome.tabs.goForward.
  auto goforward_function = base::MakeRefCounted<TabsGoForwardFunction>();
  goforward_function->set_extension(extension_with_tabs_permission.get());
  extension_function_test_utils::RunFunction(
      goforward_function.get(), base::StringPrintf("[%d]", tab_id), browser(),
      api_test_utils::INCLUDE_INCOGNITO);

  content::RenderFrameHostTester::CommitPendingLoad(
      &active_webcontent->GetController());
  EXPECT_EQ(urls[1], raw_web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[1], raw_web_contents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // If there's no next page, chrome.tabs.goForward should return an error.
  auto goforward_function2 = base::MakeRefCounted<TabsGoForwardFunction>();
  goforward_function2->set_extension(extension_with_tabs_permission.get());
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      goforward_function2.get(), base::StringPrintf("[%d]", tab_id), browser(),
      api_test_utils::NONE);
  EXPECT_EQ(tabs_constants::kNotFoundNextPageError, error);
  EXPECT_EQ(urls[1], raw_web_contents->GetLastCommittedURL());
  EXPECT_EQ(urls[1], raw_web_contents->GetVisibleURL());

  // Clean up.
  while (!browser()->tab_strip_model()->empty())
    browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  base::RunLoop().RunUntilIdle();
}

TEST_F(TabsApiUnitTest, TabsGoForwardAndBackWithoutTabId) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();

  // Create first tab with history.
  const std::vector<GURL> tab1_urls = {GURL("http://www.foo.com"),
                                       GURL("http://www.bar.com")};
  std::unique_ptr<content::WebContents> tab1_webcontents =
      CreateWebContentsWithHistory(profile(), tab1_urls);
  content::WebContents* tab1_raw_webcontents = tab1_webcontents.get();
  ASSERT_TRUE(tab1_raw_webcontents);
  EXPECT_EQ(tab1_urls[1], tab1_raw_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab1_urls[1], tab1_raw_webcontents->GetVisibleURL());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  tab_strip_model->AppendWebContents(std::move(tab1_webcontents), true);
  const int tab1_index =
      tab_strip_model->GetIndexOfWebContents(tab1_raw_webcontents);

  // Create second tab with history.
  const std::vector<GURL> tab2_urls = {GURL("http://www.chrome.com"),
                                       GURL("http://www.google.com")};
  std::unique_ptr<content::WebContents> tab2_webcontents =
      CreateWebContentsWithHistory(profile(), tab2_urls);
  content::WebContents* tab2_raw_webcontents = tab2_webcontents.get();
  ASSERT_TRUE(tab2_raw_webcontents);
  EXPECT_EQ(tab2_urls[1], tab2_raw_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab2_urls[1], tab2_raw_webcontents->GetVisibleURL());
  tab_strip_model->AppendWebContents(std::move(tab2_webcontents), true);
  const int tab2_index =
      tab_strip_model->GetIndexOfWebContents(tab2_raw_webcontents);
  ASSERT_EQ(2, tab_strip_model->count());

  // Activate first tab.
  tab_strip_model->ActivateTabAt(tab1_index,
                                 {TabStripModel::GestureType::kOther});

  // Go back without tab_id. But first tab should be navigated since it's
  // activated.
  auto goback_function = base::MakeRefCounted<TabsGoBackFunction>();
  goback_function->set_extension(extension_with_tabs_permission.get());
  extension_function_test_utils::RunFunction(goback_function.get(), "[]",
                                             browser(),
                                             api_test_utils::INCLUDE_INCOGNITO);

  content::NavigationController& controller =
      tab1_raw_webcontents->GetController();
  content::RenderFrameHostTester::CommitPendingLoad(&controller);
  EXPECT_EQ(tab1_urls[0], tab1_raw_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab1_urls[0], tab1_raw_webcontents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // Go forward without tab_id.
  auto goforward_function = base::MakeRefCounted<TabsGoForwardFunction>();
  goforward_function->set_extension(extension_with_tabs_permission.get());
  extension_function_test_utils::RunFunction(goforward_function.get(), "[]",
                                             browser(),
                                             api_test_utils::INCLUDE_INCOGNITO);

  content::RenderFrameHostTester::CommitPendingLoad(&controller);
  EXPECT_EQ(tab1_urls[1], tab1_raw_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab1_urls[1], tab1_raw_webcontents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller.GetLastCommittedEntry()->GetTransitionType());

  // Activate second tab.
  tab_strip_model->ActivateTabAt(tab2_index,
                                 {TabStripModel::GestureType::kOther});

  auto goback_function2 = base::MakeRefCounted<TabsGoBackFunction>();
  goback_function2->set_extension(extension_with_tabs_permission.get());
  extension_function_test_utils::RunFunction(goback_function2.get(), "[]",
                                             browser(),
                                             api_test_utils::INCLUDE_INCOGNITO);

  content::NavigationController& controller2 =
      tab2_raw_webcontents->GetController();
  content::RenderFrameHostTester::CommitPendingLoad(&controller2);
  EXPECT_EQ(tab2_urls[0], tab2_raw_webcontents->GetLastCommittedURL());
  EXPECT_EQ(tab2_urls[0], tab2_raw_webcontents->GetVisibleURL());
  EXPECT_TRUE(ui::PAGE_TRANSITION_FORWARD_BACK &
              controller2.GetLastCommittedEntry()->GetTransitionType());

  // Clean up.
  while (!browser()->tab_strip_model()->empty())
    browser()->tab_strip_model()->CloseWebContentsAt(0, 0);
  base::RunLoop().RunUntilIdle();
}

#if defined(OS_CHROMEOS)
TEST_F(TabsApiUnitTest, DontCreateTabsInLockedFullscreenMode) {
  scoped_refptr<const Extension> extension_with_tabs_permission =
      CreateTabsExtension();

  browser_window()->SetNativeWindow(new aura::Window(nullptr));

  auto function = base::MakeRefCounted<TabsCreateFunction>();

  function->set_extension(extension_with_tabs_permission.get());

  // In locked fullscreen mode we should not be able to create any tabs.
  browser_window()->GetNativeWindow()->SetProperty(
      ash::kWindowPinTypeKey, ash::WindowPinType::kTrustedPinned);

  EXPECT_EQ(tabs_constants::kLockedFullscreenModeNewTabError,
            extension_function_test_utils::RunFunctionAndReturnError(
                function.get(), "[{}]", browser(), api_test_utils::NONE));
}

// Ensure tabs.captureVisibleTab respects any Data Leak Prevention restrictions.
TEST_F(TabsApiUnitTest, ScreenshotsRestricted) {
  // Setup the function and extension.
  scoped_refptr<const Extension> extension = ExtensionBuilder("Screenshot")
                                                 .AddPermission("tabs")
                                                 .AddPermission("<all_urls>")
                                                 .Build();
  auto function = base::MakeRefCounted<TabsCaptureVisibleTabFunction>();
  function->set_extension(extension.get());

  // Add a visible tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents.get());
  const GURL kGoogle("http://www.google.com");
  web_contents_tester->NavigateAndCommit(kGoogle);
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  /*foreground=*/true);

  // Setup Data Leak Prevention restriction.
  policy::MockDlpContentManager mock_dlp_content_manager;
  policy::DlpContentManager::SetDlpContentManagerForTesting(
      &mock_dlp_content_manager);
  EXPECT_CALL(mock_dlp_content_manager, IsScreenshotRestricted(testing::_))
      .Times(1)
      .WillOnce(testing::Return(true));

  // Run the function and check result.
  std::string error = extension_function_test_utils::RunFunctionAndReturnError(
      function.get(), "[{}]", browser(), api_test_utils::NONE);
  EXPECT_EQ(tabs_constants::kScreenshotsDisabled, error);

  // Clean up.
  browser()->tab_strip_model()->CloseAllTabs();
  policy::DlpContentManager::ResetDlpContentManagerForTesting();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace extensions
