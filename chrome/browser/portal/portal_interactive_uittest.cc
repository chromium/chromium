// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

using content::WebContents;

class PortalInteractiveUITest : public InProcessBrowserTest {
 public:
  PortalInteractiveUITest() = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kPortals);
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PortalInteractiveUITest,
                       FocusTransfersAcrossActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/activate.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(contents, "loadPromise"));
  EXPECT_TRUE(content::ExecJs(contents,
                              "var blurPromise = new Promise(r => {"
                              "  window.onblur = () => r(true)"
                              "})"));
  EXPECT_TRUE(content::ExecJs(contents,
                              "var button = document.createElement('button');"
                              "document.body.appendChild(button);"
                              "button.focus();"
                              "var buttonBlurPromise = new Promise(r => {"
                              "  button.onblur = () => r(true)"
                              "});"));
  WebContents* portal_contents = contents->GetInnerWebContents()[0];
  EXPECT_TRUE(content::ExecJs(portal_contents,
                              "var focusPromise = new Promise(r => {"
                              "  window.onfocus = () => r(true)"
                              "})"));

  // Activate the portal, and then check if the predecessor contents lost focus,
  // and the portal contents got focus.
  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(true, content::EvalJs(contents, "blurPromise"));
  EXPECT_EQ(true, content::EvalJs(contents, "buttonBlurPromise"));
  EXPECT_EQ(true, content::EvalJs(portal_contents, "focusPromise"));
}

IN_PROC_BROWSER_TEST_F(PortalInteractiveUITest, AutofocusAcrossActivation) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url(embedded_test_server()->GetURL("/portal/autofocus.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  WebContents* contents = browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_EQ(true, content::EvalJs(contents, "loadComplete"));

  // Elements with autofocus are only focused after a rendering update, so we
  // call requestAnimationFrame() to wait for the update.
  WebContents* portal_contents = contents->GetInnerWebContents()[0];
  EXPECT_EQ(true, content::EvalJs(portal_contents, "rAF()"));

  // Check that autofocused element inside the portal is the active element, but
  // focus event hasn't been dispatched.
  EXPECT_EQ(true, content::EvalJs(portal_contents, "checkActiveElement()"));
  EXPECT_EQ(false, content::EvalJs(portal_contents, "focusEventDispatched"));

  // Activate the portal, and then check if the autofocused element got focus.
  EXPECT_EQ(true, content::EvalJs(contents, "activate()"));
  EXPECT_EQ(true, content::EvalJs(portal_contents, "checkActiveElement()"));
  EXPECT_EQ(true, content::EvalJs(portal_contents, "focusPromise"));
}
