// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/page_capture/page_capture_api.h"

#include <memory>

#include "base/test/values_test_util.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_browser_window.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/api_test_utils.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "url/gurl.h"

namespace extensions {

class PageCaptureApiUnitTest : public ExtensionServiceTestBase {
 protected:
  void SetUp() override {
    ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    browser_window_ = std::make_unique<TestBrowserWindow>();
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    params.window = browser_window_.get();
    browser_ = std::unique_ptr<Browser>(Browser::Create(params));
  }

  void TearDown() override {
    browser_.reset();
    browser_window_.reset();
    ExtensionServiceTestBase::TearDown();
  }
  Browser* browser() { return browser_.get(); }

 private:
  std::unique_ptr<TestBrowserWindow> browser_window_;
  std::unique_ptr<Browser> browser_;
};

// Tests that if a page navigates during a call to pageCature.saveAsMHTML(), the
// API call will result in an error.
TEST_F(PageCaptureApiUnitTest, PageNavigationDuringSaveAsMHTML) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Page Capture").AddAPIPermission("pageCapture").Build();
  auto function = base::MakeRefCounted<PageCaptureSaveAsMHTMLFunction>();
  function->set_extension(extension.get());

  // Add a visible tab.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  content::WebContentsTester* web_contents_tester =
      content::WebContentsTester::For(web_contents.get());
  browser()->tab_strip_model()->AppendWebContents(std::move(web_contents),
                                                  /*foreground=*/true);
  web_contents_tester->NavigateAndCommit(GURL("https://www.google.com"));
  const int tab_id = ExtensionTabUtil::GetTabId(
      browser()->tab_strip_model()->GetWebContentsAt(0));

  // To capture the page as MHTML, the extension function needs to hop from the
  // UI thread to the IO thread to create the temporary file, then back to the
  // UI thread to actually save the page contents. Since the URL access is only
  // checked initially and a navigation could happen during the thread hops, the
  // extension function should result in an error if a navigation happens
  // between the initial check and the actual capture. To simulate this we start
  // the extension function running, then trigger a synchronous navigation
  // using the WebContentsTester immediately which will happen before the
  // messaging between threads finishes.
  // Regression test for crbug.com/1494490
  function->SetBrowserContextForTesting(profile());
  function->SetArgs(
      base::Value::List().Append(base::Value::Dict().Set("tabId", tab_id)));
  api_test_utils::SendResponseHelper response_helper(function.get());
  function->RunWithValidation().Execute();

  web_contents_tester->NavigateAndCommit(GURL("https://www.chromium.org"));
  response_helper.WaitForResponse();
  const base::Value::List* results = function->GetResultListForTest();
  ASSERT_TRUE(results);
  EXPECT_TRUE(results->empty()) << "Did not expect a result";
  CHECK(function->response_type());
  EXPECT_EQ(ExtensionFunction::FAILED, *function->response_type());
  EXPECT_EQ("Tab navigated before capture could complete.",
            function->GetError());

  // Clean up.
  browser()->tab_strip_model()->CloseAllTabs();
}

}  // namespace extensions
