// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/content/ssl_blocking_page.h"
#include "components/security_interstitials/core/controller_client.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/request_handler_util.h"
#include "ui/accessibility/ax_enums.mojom.h"

class InterstitialAccessibilityBrowserTest : public InProcessBrowserTest {
 public:
  InterstitialAccessibilityBrowserTest()
      : https_server_mismatched_(net::EmbeddedTestServer::TYPE_HTTPS) {
    https_server_mismatched_.SetSSLConfig(
        net::EmbeddedTestServer::CERT_MISMATCHED_NAME);
    https_server_mismatched_.AddDefaultHandlers(GetChromeTestDataDir());
  }

  std::string GetNameOfFocusedNode(content::WebContents* web_contents) {
    ui::AXNodeData focused_node_data =
        content::GetFocusedAccessibilityNodeInfo(web_contents);
    return focused_node_data.GetStringAttribute(
        ax::mojom::StringAttribute::kName);
  }

 protected:
  net::EmbeddedTestServer https_server_mismatched_;

  bool IsShowingInterstitial(content::WebContents* tab) {
    security_interstitials::SecurityInterstitialTabHelper* helper =
        security_interstitials::SecurityInterstitialTabHelper::FromWebContents(
            tab);
    if (!helper) {
      return false;
    }
    return helper->GetBlockingPageForCurrentlyCommittedNavigationForTesting() !=
           nullptr;
  }

  void ProceedThroughInterstitial(content::WebContents* web_contents) {
    content::TestNavigationObserver nav_observer(web_contents, 1);
    std::string javascript = "window.certificateErrorPageController.proceed();";
    ASSERT_TRUE(content::ExecuteScript(web_contents, javascript));
    nav_observer.Wait();
    return;
  }
};

IN_PROC_BROWSER_TEST_F(InterstitialAccessibilityBrowserTest,
                       TestSSLInterstitialAccessibility) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::EnableAccessibilityForWebContents(web_contents);

  ASSERT_TRUE(https_server_mismatched_.Start());

  // Navigate to a page with an SSL error on it.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_mismatched_.GetURL("/ssl/blank_page.html")));

  // Ensure that we got an interstitial page.
  ASSERT_FALSE(web_contents->IsCrashed());
  EXPECT_TRUE(IsShowingInterstitial(web_contents));

  // Now check from the perspective of accessibility - we should be focused
  // on a page with title "Privacy error". Keep waiting on accessibility
  // focus events until we get that page.
  while (GetNameOfFocusedNode(web_contents) != "Privacy error")
    content::WaitForAccessibilityFocusChange();

  // Now proceed through the interstitial and ensure we get accessibility
  // focus on the actual page.
  ProceedThroughInterstitial(web_contents);
  while (GetNameOfFocusedNode(web_contents) != "I am a blank page.")
    content::WaitForAccessibilityFocusChange();
}
