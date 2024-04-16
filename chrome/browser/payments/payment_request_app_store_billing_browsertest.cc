// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/chromeos_buildflags.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"

namespace payments {
namespace {

// This test suite tests the PaymentRequest behaviour specifically in the cases
// where the merchant uses app store billing methods.
class PaymentRequestAppStoreBillingTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentRequestAppStoreBillingTest() = default;
  ~PaymentRequestAppStoreBillingTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/app_store_billing_tests/index.html");
  }
};

class PaymentRequestTwaBillingTest : public PaymentRequestAppStoreBillingTest {
 public:
  PaymentRequestTwaBillingTest() = default;
  ~PaymentRequestTwaBillingTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      payments::features::kAppStoreBilling};
};

// When requesting app store billing methods (e.g., google play store method)
// not in a Trusted Web Activity, the request should fail with an error message
// indicating that the method is only supported in TWA.
IN_PROC_BROWSER_TEST_F(
    PaymentRequestAppStoreBillingTest,
    ErrorMessageActionableWhenRequestAppStoreBillingNotInTwa) {
  std::string expected = "success";
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "addSupportedMethod('https://play.google.com/billing')"));
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(), "createPaymentRequest()"));
  EXPECT_EQ(
      "NotSupportedError: The payment method "
      "\"https://play.google.com/billing\" is not supported. Payment method "
      "https://play.google.com/billing is only supported in Trusted Web "
      "Activity.",
      content::EvalJs(GetActiveWebContents(), "show()"));
}

// Test requesting app store billing method (e.g., google play store method)
// in a Trusted Web Activity
// TODO(crbug.com/40700424): This test should simulate being in a TWA such that
// Play Billing is discovered as an app store payment app.
IN_PROC_BROWSER_TEST_F(PaymentRequestTwaBillingTest,
                       RequestAppStoreBillingInTwa) {
  test_controller()->SetTwaPackageName("com.merchant.twa");

  std::string expected = "success";
  EXPECT_EQ(
      expected,
      content::EvalJs(GetActiveWebContents(),
                      "addSupportedMethod('https://play.google.com/billing')"));
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(), "createPaymentRequest()"));

  std::string expected_error =
      "NotSupportedError: The payment method "
      "\"https://play.google.com/billing\" is not supported.";

#if BUILDFLAG(IS_CHROMEOS_ASH)
  expected_error = expected_error + " Unable to invoke Android apps.";
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  expected_error =
      expected_error + " Cannot connect to the OS from Lacros browser.";
#endif  // BUILDFLAG(IS_CHROMEOS)

  // We expect the standard NotSupportedError inside a TWA because Play Billing
  // isn't supported yet.
  EXPECT_EQ(expected_error, content::EvalJs(GetActiveWebContents(), "show()"));
}

// Prove that requesting with a non-app-store method would not produce the same
// error message as in test
// "ErrorMessageActionableWhenRequestAppStoreBillingNotInTwa".
IN_PROC_BROWSER_TEST_F(PaymentRequestAppStoreBillingTest,
                       NormalErrorMessageForNonStoreUrl) {
  std::string expected = "success";
  std::string non_app_store_pay =
      https_server()
          ->GetURL("/app_store_billing_tests/method_manifest.json")
          .spec();
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "addSupportedMethod('" + non_app_store_pay + "')"));
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(), "createPaymentRequest()"));

  std::string expected_error = "NotSupportedError: The payment method \"" +
                               non_app_store_pay + "\" is not supported.";
  EXPECT_EQ(expected_error, content::EvalJs(GetActiveWebContents(), "show()"));
}
}  // namespace
}  // namespace payments
