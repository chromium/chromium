// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace payments {
namespace {

class PaymentRequestSecurityPolicyBrowsertest
    : public PaymentRequestPlatformBrowserTestBase,
      public testing::WithParamInterface<bool> {
 protected:
  PaymentRequestSecurityPolicyBrowsertest() {
    if (IsCSPEnforced()) {
      features_.InitAndDisableFeature(
          blink::features::kIgnoreCSPInWebPaymentAPI);
    } else {
      features_.InitAndEnableFeature(
          blink::features::kIgnoreCSPInWebPaymentAPI);
    }
  }

  ~PaymentRequestSecurityPolicyBrowsertest() override = default;

  bool IsCSPEnforced() { return GetParam(); }

  void ExpectPaymentRequestCSPViolationRecorded(bool expected) {
    // Navigate away in order to flush use counters.
    ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(),
                                       GURL(url::kAboutBlankURL)));
    histogram_tester_.ExpectBucketCount(
        "Blink.UseCounter.Features",
        blink::mojom::WebFeature::kPaymentRequestCSPViolation,
        expected ? 1 : 0);
  }

  void CheckCanMakePayment() {
    // The CSP check happens in `new PaymentRequest()`. The test calls
    // canMakePayment() to ensure the promise resolves before metrics are
    // checked.
    ASSERT_EQ("false", content::EvalJs(GetActiveWebContents(),
                                       content::JsReplace(
                                           "checkCanMakePayment($1)",
                                           https_server()->GetURL(
                                               "bobpay.test", "/csp-test"))));
  }

 private:
  base::HistogramTester histogram_tester_;
  base::test::ScopedFeatureList features_;
};

// Ensure that the PaymentRequestCSPViolation use counter is recorded when CSP
// is bypassed.
IN_PROC_BROWSER_TEST_P(PaymentRequestSecurityPolicyBrowsertest, CSPViolation) {
  NavigateTo("a.com", "/payment_request_csp_violation.html");

  std::string script =
      content::JsReplace("checkCanMakePayment($1)",
                         https_server()->GetURL("bobpay.test", "/csp-test"));
  if (IsCSPEnforced()) {
    EXPECT_THAT(content::EvalJs(GetActiveWebContents(), script).ExtractString(),
                testing::MatchesRegex(
                    "RangeError: Failed to construct 'PaymentRequest': "
                    "https://bobpay.test:\\d+/csp-test payment method "
                    "identifier violates Content Security Policy."));
    ExpectPaymentRequestCSPViolationRecorded(false);
  } else {
    ASSERT_EQ("false", content::EvalJs(GetActiveWebContents(), script));
    ExpectPaymentRequestCSPViolationRecorded(true);
  }
}

// Ensure that there is no CSP violation with `connect-src *`.
IN_PROC_BROWSER_TEST_P(PaymentRequestSecurityPolicyBrowsertest, CSPAllowAll) {
  NavigateTo("a.com", "/payment_request_csp_allow_all.html");

  CheckCanMakePayment();

  ExpectPaymentRequestCSPViolationRecorded(false);
}

// Ensure that there is no CSP violation with `connect-src
// https://bobpay.test:*`
IN_PROC_BROWSER_TEST_P(PaymentRequestSecurityPolicyBrowsertest,
                       CSPAllowSpecific) {
  NavigateTo("a.com", "/payment_request_csp_allow_specific.html");

  CheckCanMakePayment();

  ExpectPaymentRequestCSPViolationRecorded(false);
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         PaymentRequestSecurityPolicyBrowsertest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "CSPEnforced" : "CSPIgnored";
                         });

}  // namespace
}  // namespace payments
