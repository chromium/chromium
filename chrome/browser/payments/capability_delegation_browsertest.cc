// Copyright 2021 The Chromium Authors
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
  CapabilityDelegationBrowserTest() = default;

  CapabilityDelegationBrowserTest(const CapabilityDelegationBrowserTest&) =
      delete;
  CapabilityDelegationBrowserTest& operator=(
      const CapabilityDelegationBrowserTest&) = delete;

  ~CapabilityDelegationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    content::SetupCrossSiteRedirector(https_server());
    https_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/capability_delegation");
    payments::PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// TODO(crbug.com/40935396): Disabled due to excessive flakiness across
//   multiple platforms (Mac, Linux, Windows, etc.).
IN_PROC_BROWSER_TEST_F(CapabilityDelegationBrowserTest,
                       DISABLED_CrossOriginPaymentRequest) {
  // Install a payment app that responds to the abortpayment event, which is
  // used by this test to determine that the app was successfully run.
  std::string payment_method;
  InstallPaymentApp("a.com", "/abort_responder_app.js", &payment_method);

  // Navigate the top frame.
  GURL main_url(
      https_server()->GetURL("a.com", "/payment_request_delegation.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the sub-frame cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL cross_site_url(
      https_server()->GetURL("b.com", "/payment_request_delegation_sub.html"));
  EXPECT_TRUE(
      NavigateIframeToURL(active_web_contents, "iframe", cross_site_url));

  const std::string subframe_origin =
      https_server()->GetOrigin("b.com").Serialize();

  // Confirm that the subframe is cross-process depending on the process
  // model.
  content::RenderFrameHost* frame_host =
      ChildFrameAt(active_web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(frame_host);
  EXPECT_EQ(cross_site_url, frame_host->GetLastCommittedURL());
  auto* main_instance =
      active_web_contents->GetPrimaryMainFrame()->GetSiteInstance();
  auto* subframe_instance = frame_host->GetSiteInstance();
  if (main_instance->RequiresDedicatedProcess()) {
    // Subframe is cross process because it can't be place in the main frame's
    // process.
    EXPECT_TRUE(frame_host->IsCrossProcessSubframe());
  } else {
    // The main frame does not require a dedicated process so the subframe will
    // be placed in the same process as the main frame.
    EXPECT_FALSE(frame_host->IsCrossProcessSubframe());
    EXPECT_FALSE(subframe_instance->RequiresDedicatedProcess());
    EXPECT_EQ(content::AreDefaultSiteInstancesEnabled(),
              main_instance == subframe_instance);
  }

  // One activationless show is allowed and then successfully aborted by the
  // script.
  EXPECT_EQ(
      "AbortError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(false, $1, $2)",
                                         payment_method, subframe_origin),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Without either user activation or payment request token, PaymentRequest
  // dialog is not allowed.
  EXPECT_EQ(
      "SecurityError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(false, $1, $2)",
                                         payment_method, subframe_origin),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Without user activation but with the delegation option, the delegation
  // postMessage is not allowed.
  EXPECT_EQ(
      "NotAllowedError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(true, $1, $2)",
                                         payment_method, subframe_origin),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // With user activation but without the delegation option, PaymentRequest
  // dialog is not allowed.
  EXPECT_EQ(
      "SecurityError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(false, $1, $2)",
                                         payment_method, subframe_origin)));

  // With both user activation and the delegation option, PaymentRequest dialog
  // is shown and then successfully aborted by the script.
  EXPECT_EQ(
      "AbortError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(true, $1, $2)",
                                         payment_method, subframe_origin)));
}

// TODO(crbug.com/40935396): Disabled due to excessive flakiness across
//   multiple platforms (Mac, Linux, Windows, etc.).
IN_PROC_BROWSER_TEST_F(CapabilityDelegationBrowserTest,
                       DISABLED_SameOriginPaymentRequest) {
  // Install a payment app that responds to the abortpayment event, which is
  // used by this test to determine that the app was successfully run.
  std::string payment_method;
  InstallPaymentApp("a.com", "/abort_responder_app.js", &payment_method);

  // Navigate the top frame.
  GURL main_url(
      https_server()->GetURL("a.com", "/payment_request_delegation.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // Navigate the sub-frame cross-site.
  content::WebContents* active_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  GURL subframe_url(
      https_server()->GetURL("a.com", "/payment_request_delegation_sub.html"));
  EXPECT_TRUE(NavigateIframeToURL(active_web_contents, "iframe", subframe_url));

  const std::string subframe_origin = "/";

  // Confirm that the subframe is same-process.
  content::RenderFrameHost* frame_host =
      ChildFrameAt(active_web_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(frame_host);
  EXPECT_EQ(subframe_url, frame_host->GetLastCommittedURL());
  EXPECT_FALSE(frame_host->IsCrossProcessSubframe());

  // One activationless show is allowed and then successfully aborted by the
  // script.
  EXPECT_EQ(
      "AbortError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(false, $1, $2)",
                                         payment_method, subframe_origin),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Without either user activation or payment request token, PaymentRequest
  // dialog is not allowed.
  EXPECT_EQ(
      "SecurityError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(false, $1, $2)",
                                         payment_method, subframe_origin),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // Without user activation but with the delegation option, the delegation
  // postMessage is not allowed.
  EXPECT_EQ(
      "NotAllowedError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(true, $1, $2)",
                                         payment_method, subframe_origin),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // With user activation but without the delegation option, PaymentRequest
  // dialog is shown and then successfully aborted by the script.
  EXPECT_EQ(
      "AbortError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(false, $1, $2)",
                                         payment_method, subframe_origin)));

  // With both user activation and the delegation option, PaymentRequest dialog
  // is shown and then successfully aborted by the script.
  EXPECT_EQ(
      "AbortError",
      content::EvalJs(active_web_contents,
                      content::JsReplace("sendRequestToSubframe(true, $1, $2)",
                                         payment_method, subframe_origin)));
}
