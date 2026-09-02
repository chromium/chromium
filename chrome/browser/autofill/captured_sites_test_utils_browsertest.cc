// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/captured_sites_test_utils.h"

#include "chrome/browser/autofill/autofill_uitest.h"
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
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace {

using captured_sites_test_utils::TestRecipeReplayer;

class CapturedSitesTestUtilsBrowserTest : public autofill::AutofillUiTest {
 public:
  void SetUpOnMainThread() override {
    autofill::AutofillUiTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 protected:
  content::RenderFrameHostWrapper NavigateAwayAndGetUnloadedFrame() {
    GURL url1 = embedded_test_server()->GetURL("a.com", "/title1.html");
    GURL url2 = embedded_test_server()->GetURL("b.com", "/title2.html");

    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHostWrapper old_rfh(
        web_contents->GetPrimaryMainFrame());

    content::TestNavigationManager navigation_manager(web_contents, url2);
    EXPECT_TRUE(content::ExecJs(
        old_rfh.get(), content::JsReplace("window.location.href = $1;", url2)));
    EXPECT_TRUE(navigation_manager.WaitForNavigationFinished());
    EXPECT_TRUE(navigation_manager.was_committed());
    EXPECT_TRUE(old_rfh.IsDestroyed() || !old_rfh->IsActive());

    return old_rfh;
  }
};

// Tests that evaluating script and input helpers on a RenderFrameHost after it
// has unloaded or navigated away returns false without crashing the test
// process.
IN_PROC_BROWSER_TEST_F(
    CapturedSitesTestUtilsBrowserTest,
    ScrollElementIntoView_NavigatingFrame_ReturnsFalseWithoutCrash) {
  content::RenderFrameHostWrapper old_rfh = NavigateAwayAndGetUnloadedFrame();
  EXPECT_FALSE(
      TestRecipeReplayer::ScrollElementIntoView("//button", old_rfh.get()));
}

IN_PROC_BROWSER_TEST_F(
    CapturedSitesTestUtilsBrowserTest,
    PlaceFocusOnElement_NavigatingFrame_ReturnsFalseWithoutCrash) {
  content::RenderFrameHostWrapper old_rfh = NavigateAwayAndGetUnloadedFrame();
  EXPECT_FALSE(
      TestRecipeReplayer::PlaceFocusOnElement("//button", {}, old_rfh.get()));
}

IN_PROC_BROWSER_TEST_F(
    CapturedSitesTestUtilsBrowserTest,
    GetBoundingRectOfTargetElement_NavigatingFrame_ReturnsFalseWithoutCrash) {
  content::RenderFrameHostWrapper old_rfh = NavigateAwayAndGetUnloadedFrame();
  gfx::Rect rect;
  EXPECT_FALSE(TestRecipeReplayer::GetBoundingRectOfTargetElement(
      "//button", {}, old_rfh.get(), &rect));
}

IN_PROC_BROWSER_TEST_F(
    CapturedSitesTestUtilsBrowserTest,
    SimulateLeftMouseClickAt_NavigatingFrame_ReturnsFalseWithoutCrash) {
  content::RenderFrameHostWrapper old_rfh = NavigateAwayAndGetUnloadedFrame();
  EXPECT_FALSE(TestRecipeReplayer::SimulateLeftMouseClickAt(gfx::Point(0, 0),
                                                            old_rfh.get()));
}

IN_PROC_BROWSER_TEST_F(
    CapturedSitesTestUtilsBrowserTest,
    SimulateMouseHoverAt_NavigatingFrame_ReturnsFalseWithoutCrash) {
  content::RenderFrameHostWrapper old_rfh = NavigateAwayAndGetUnloadedFrame();
  EXPECT_FALSE(TestRecipeReplayer::SimulateMouseHoverAt(old_rfh.get(),
                                                        gfx::Point(0, 0)));
}

}  // namespace
