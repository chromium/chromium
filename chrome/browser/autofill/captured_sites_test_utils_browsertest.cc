// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/captured_sites_test_utils.h"

#include "chrome/browser/autofill/autofill_uitest.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using captured_sites_test_utils::TestRecipeReplayer;

class CapturedSitesTestUtilsBrowserTest : public autofill::AutofillUiTest {
 public:
  void SetUpOnMainThread() override {
    autofill::AutofillUiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
  }
};

// Tests that evaluating script helpers on a RenderFrameHost after it has
// unloaded or navigated away returns false without crashing the test process.
IN_PROC_BROWSER_TEST_F(
    CapturedSitesTestUtilsBrowserTest,
    ScrollElementIntoView_NavigatingFrame_ReturnsFalseWithoutCrash) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
  GURL url2 = embedded_test_server()->GetURL("b.com", "/title2.html");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Save the primary main frame before starting navigation to url2.
  content::RenderFrameHostWrapper old_rfh(web_contents->GetPrimaryMainFrame());

  // Start cross-site navigation to url2 asynchronously.
  content::TestNavigationManager navigation_manager(web_contents, url2);
  ASSERT_TRUE(content::ExecJs(
      old_rfh.get(), content::JsReplace("window.location.href = $1;", url2)));
  ASSERT_TRUE(navigation_manager.WaitForNavigationFinished());

  // Attempting to scroll an element on an unloaded frame should return false.
  EXPECT_FALSE(
      TestRecipeReplayer::ScrollElementIntoView("//button", old_rfh.get()));
}

}  // namespace
