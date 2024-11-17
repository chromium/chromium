// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/search/search_api.h"

#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/sessions/session_tab_helper_factory.h"
#include "chrome/test/base/search_test_utils.h"
#include "chrome/test/base/test_browser_window.h"
#include "components/search_engines/template_url_service.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension_builder.h"

namespace extensions {

namespace {

// Creates an extension with "search" permission.
scoped_refptr<const Extension> CreateSearchExtension() {
  return ExtensionBuilder("Extension with search permission")
      .AddAPIPermission("search")
      .Build();
}

scoped_refptr<SearchQueryFunction> CreateSearchFunction(
    scoped_refptr<const Extension> extension) {
  auto function = base::MakeRefCounted<SearchQueryFunction>();
  function->set_extension(extension.get());
  function->set_has_callback(true);
  return function;
}

std::unique_ptr<content::WebContents> CreateWebContents(Browser* browser) {
  GURL url("https://example.com");
  std::unique_ptr<content::WebContents> contents(
      content::WebContentsTester::CreateTestWebContents(browser->profile(),
                                                        nullptr));
  content::WebContents* raw_contents = contents.get();
  browser->tab_strip_model()->AppendWebContents(std::move(contents), true);
  EXPECT_EQ(browser->tab_strip_model()->GetActiveWebContents(), raw_contents);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(raw_contents);
  web_contents_tester->NavigateAndCommit(url);
  CreateSessionServiceTabHelper(raw_contents);
  return contents;
}

}  // namespace

class SearchApiUnitTest : public ExtensionServiceTestBase {
 protected:
  SearchApiUnitTest() = default;
  ~SearchApiUnitTest() override = default;

  Browser* browser() { return browser_.get(); }
  TestBrowserWindow* browser_window() { return browser_window_.get(); }
  TabStripModel* GetTabStripModel() { return browser_->tab_strip_model(); }
  extensions::SearchQueryFunction* function() { return function_.get(); }
  void RunFunctionAndExpectError(const std::string& input,
                                 std::string_view expected);

 private:
  void SetUp() override;
  void TearDown() override;

  // The browser (and accompanying window).
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;

  scoped_refptr<extensions::SearchQueryFunction> function_;
};

void SearchApiUnitTest::SetUp() {
  ExtensionServiceTestBase::SetUp();
  InitializeEmptyExtensionService();

  // Force TabManager/TabLifecycleUnitSource creation.
  g_browser_process->GetTabManager();

  browser_window_ = std::make_unique<TestBrowserWindow>();
  Browser::CreateParams params(profile(), /*user_gesture*/ true);
  params.type = Browser::TYPE_NORMAL;
  params.window = browser_window_.get();
  browser_ = std::unique_ptr<Browser>(Browser::Create(params));

  // Mock TemplateURLService.
  auto* template_url_service = static_cast<TemplateURLService*>(
      TemplateURLServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(),
          base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor)));
  search_test_utils::WaitForTemplateURLServiceToLoad(template_url_service);

  scoped_refptr<const Extension> extension = CreateSearchExtension();
  function_ = CreateSearchFunction(extension);
  CreateWebContents(browser());
}

void SearchApiUnitTest::TearDown() {
  browser_->tab_strip_model()->CloseAllTabs();
  browser_ = nullptr;
  browser_window_ = nullptr;
  ExtensionServiceTestBase::TearDown();
}

void SearchApiUnitTest::RunFunctionAndExpectError(const std::string& input,
                                                  std::string_view expected) {
  auto result = api_test_utils::RunFunctionAndReturnError(function(), input,
                                                          browser()->profile());
  EXPECT_EQ(expected, result);
}

// Test for error if search field is empty string.
TEST_F(SearchApiUnitTest, QueryEmpty) {
  RunFunctionAndExpectError(R"([{"text": ""}])", "Empty text parameter.");
}

// Test for error if both disposition and tabId are populated.
TEST_F(SearchApiUnitTest, DispositionAndTabIDValid) {
  RunFunctionAndExpectError(
      R"([{"text": "1", "disposition": "NEW_TAB", "tabId": 1}])",
      "Cannot set both 'disposition' and 'tabId'.");
}

// Test for error if both disposition and tabId are populated.
TEST_F(SearchApiUnitTest, InvalidTabId) {
  RunFunctionAndExpectError(R"([{"text": "1", "tabId": -1}])",
                            "No tab with id: -1.");
}

// Test for error if missing browser context.
TEST_F(SearchApiUnitTest, NoActiveBrowser) {
  auto result = api_test_utils::RunFunctionAndReturnError(
      function(), R"([{"text": "1"}])", nullptr);
  EXPECT_EQ("No active browser.", result);
}

}  // namespace extensions
