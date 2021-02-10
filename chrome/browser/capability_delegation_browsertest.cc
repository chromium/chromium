// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "url/gurl.h"

class CapabilityDelegationBrowserTest
    : public payments::PaymentRequestPlatformBrowserTestBase {
 public:
  CapabilityDelegationBrowserTest() {
    feature_list_.InitAndEnableFeature(
        features::kCapabilityDelegationPaymentRequest);
  }

  ~CapabilityDelegationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(https_server()->InitializeAndListen());
    content::SetupCrossSiteRedirector(https_server());
    https_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/capability_delegation");
    https_server()->StartAcceptingConnections();
  }

 private:
  base::test::ScopedFeatureList feature_list_;

  DISALLOW_COPY_AND_ASSIGN(CapabilityDelegationBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CapabilityDelegationBrowserTest, PaymentRequest) {
  // Navigate the top frame.
  GURL main_url(
      https_server()->GetURL("a.com", "/payment_request_delegation.html"));
  ui_test_utils::NavigateToURL(browser(), main_url);

  // Navigate the sub-frame cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL cross_site_url(
      https_server()->GetURL("b.com", "/payment_request_delegation_sub.html"));
  EXPECT_TRUE(
      NavigateIframeToURL(active_web_contents, "iframe", cross_site_url));

  // Confirm that the subframe is cross-process.
  content::RenderFrameHost* frame_host =
      ChildFrameAt(active_web_contents->GetMainFrame(), 0);
  ASSERT_TRUE(frame_host);
  EXPECT_EQ(cross_site_url, frame_host->GetLastCommittedURL());
  EXPECT_TRUE(frame_host->IsCrossProcessSubframe());

  // Without either user activation or payment request token, PaymentRequest
  // dialog is not allowed.
  EXPECT_EQ("NotAllowedError",
            content::EvalJs(active_web_contents, "sendRequestToSubframe(false)",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Without user activation but with the delegation option, PaymentRequest
  // dialog is not allowed.
  EXPECT_EQ("NotAllowedError",
            content::EvalJs(active_web_contents, "sendRequestToSubframe(true)",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // With user activation but without the delegation option, PaymentRequest
  // dialog is not allowed.
  EXPECT_EQ("NotAllowedError", content::EvalJs(active_web_contents,
                                               "sendRequestToSubframe(false)"));

  // With both user activation and the delegation option, PaymentRequest dialog
  // is shown and then successfully aborted by the script.
  EXPECT_EQ("AbortError", content::EvalJs(active_web_contents,
                                          "sendRequestToSubframe(true)"));
}
