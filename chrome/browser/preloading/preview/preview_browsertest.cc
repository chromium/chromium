// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "chrome/browser/preloading/preview/preview_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

class PreviewBrowserTest : public PlatformBrowserTest {
 public:
  PreviewBrowserTest()
      : helper_(std::make_unique<test::PreviewTestHelper>(
            base::BindRepeating(&PreviewBrowserTest::web_contents,
                                base::Unretained(this)))) {}

  void SetUp() override { PlatformBrowserTest::SetUp(); }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    embedded_test_server()->ServeFilesFromDirectory(
        base::PathService::CheckedGet(chrome::DIR_TEST_DATA));
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
  }

  content::WebContents* web_contents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  test::PreviewTestHelper& helper() { return *helper_.get(); }

 private:
  std::unique_ptr<test::PreviewTestHelper> helper_;
};

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, CancelWhenPrimaryPageChanged) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  helper().InitiatePreview(embedded_test_server()->GetURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);

  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(true,
            content::EvalJs(preview_web_contents, "document.prerendering"));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  ASSERT_FALSE(preview_web_contents);
}

IN_PROC_BROWSER_TEST_F(PreviewBrowserTest, PromoteToNewTab) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  helper().InitiatePreview(embedded_test_server()->GetURL("/title2.html"));
  helper().WaitUntilLoadFinished();

  base::WeakPtr<content::WebContents> preview_web_contents =
      helper().GetWebContentsForPreviewTab();
  ASSERT_TRUE(preview_web_contents);

  EXPECT_EQ(1, tab_strip_model->count());
  EXPECT_EQ(true,
            content::EvalJs(preview_web_contents, "document.prerendering"));

  helper().PromoteToNewTab();

  EXPECT_EQ(2, tab_strip_model->count());
  EXPECT_EQ(false,
            content::EvalJs(preview_web_contents, "document.prerendering"));
}
