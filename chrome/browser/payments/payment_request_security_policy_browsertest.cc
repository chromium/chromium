// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features_generated.h"

namespace payments {
namespace {

class PaymentRequestSecurityPolicyBrowsertest
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentRequestSecurityPolicyBrowsertest() = default;
  ~PaymentRequestSecurityPolicyBrowsertest() override = default;

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
};

// Ensure that the PaymentRequestCSPViolation use counter is recorded when CSP
// is bypassed.
IN_PROC_BROWSER_TEST_F(PaymentRequestSecurityPolicyBrowsertest, CSPViolation) {
  NavigateTo("a.com", "/payment_request_csp_violation.html");

  std::string script =
      content::JsReplace("checkCanMakePayment($1)",
                         https_server()->GetURL("bobpay.test", "/csp-test"));
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(), script).ExtractString(),
      testing::MatchesRegex("RangeError: Failed to construct 'PaymentRequest': "
                            "https://bobpay.test:\\d+/csp-test payment method "
                            "identifier violates Content Security Policy."));
}

// Ensure that there is no CSP violation with `connect-src *`.
IN_PROC_BROWSER_TEST_F(PaymentRequestSecurityPolicyBrowsertest, CSPAllowAll) {
  NavigateTo("a.com", "/payment_request_csp_allow_all.html");
  CheckCanMakePayment();
}

// Ensure that there is no CSP violation with `connect-src
// https://bobpay.test:*`
IN_PROC_BROWSER_TEST_F(PaymentRequestSecurityPolicyBrowsertest,
                       CSPAllowSpecific) {
  NavigateTo("a.com", "/payment_request_csp_allow_specific.html");
  CheckCanMakePayment();
}

}  // namespace
}  // namespace payments
