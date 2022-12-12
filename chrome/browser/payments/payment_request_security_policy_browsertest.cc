// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentRequestSecurityPolicyBrowsertest
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  void ExpectPaymentRequestCSPViolationRecorded(bool expected) {
    // Navigate away in order to flush use counters.
    ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       GURL(url::kAboutBlankURL)));
    histogram_tester_.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kPaymentRequestCSPViolation,
        expected ? 1 : 0);
  }

  void BuildPaymentRequest() {
    ResetEventWaiterForEventSequence(
        {TestEvent::kCanMakePaymentCalled, TestEvent::kCanMakePaymentReturned});

    // The CSP check happens in buildPaymentRequest. We only call canMakePayment
    // to ensure the promise resolves before metrics are checked.
    EXPECT_EQ(false,
              content::EvalJs(
                  GetActiveWebContents(),
                  content::JsReplace(
                      "buildPaymentRequest($1).canMakePayment()",
                      https_server()->GetURL("bobpay.test", "/csp-test"))));

    WaitForObservedEvent();
  }

 private:
  base::HistogramTester histogram_tester_;
};

// Ensure that the PaymentRequestCSPViolation use counter is recorded.
IN_PROC_BROWSER_TEST_F(PaymentRequestSecurityPolicyBrowsertest, CSPViolation) {
  NavigateTo("a.com", "/payment_request_csp_violation.html");

  BuildPaymentRequest();

  ExpectPaymentRequestCSPViolationRecorded(true);
}

// Ensure that there is no CSP violation with `connect-src *`.
IN_PROC_BROWSER_TEST_F(PaymentRequestSecurityPolicyBrowsertest, CSPAllowAll) {
  NavigateTo("a.com", "/payment_request_csp_allow_all.html");

  BuildPaymentRequest();

  ExpectPaymentRequestCSPViolationRecorded(false);
}

// Ensure that there is no CSP violation with `connect-src
// https://bobpay.test:*`
IN_PROC_BROWSER_TEST_F(PaymentRequestSecurityPolicyBrowsertest,
                       CSPAllowSpecific) {
  NavigateTo("a.com", "/payment_request_csp_allow_specific.html");

  BuildPaymentRequest();

  ExpectPaymentRequestCSPViolationRecorded(false);
}

}  // namespace
}  // namespace payments
