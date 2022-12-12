// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "chrome/test/payments/personal_data_manager_test_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class HasEnrolledInstrumentPaymentHandlerTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  HasEnrolledInstrumentPaymentHandlerTest(
      const HasEnrolledInstrumentPaymentHandlerTest&) = delete;
  HasEnrolledInstrumentPaymentHandlerTest& operator=(
      const HasEnrolledInstrumentPaymentHandlerTest&) = delete;

  ~HasEnrolledInstrumentPaymentHandlerTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/has_enrolled_instrument.html");
  }

  // Helper function to test that all variations of hasEnrolledInstrument()
  // returns |expected|.
  void ExpectHasEnrolledInstrumentIs(bool expected,
                                     const std::string& payment_method) {
    EXPECT_EQ(expected, content::EvalJs(
                            GetActiveWebContents(),
                            content::JsReplace("hasEnrolledInstrument({}, $1)",
                                               payment_method)));
    EXPECT_EQ(
        expected,
        content::EvalJs(GetActiveWebContents(),
                        content::JsReplace(
                            "hasEnrolledInstrument({requestShipping:true}, $1)",
                            payment_method)));
    EXPECT_EQ(expected,
              content::EvalJs(
                  GetActiveWebContents(),
                  content::JsReplace(
                      "hasEnrolledInstrument({requestPayerEmail:true}, $1)",
                      payment_method)));
  }

 protected:
  HasEnrolledInstrumentPaymentHandlerTest() = default;
};

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentPaymentHandlerTest,
                       FalseWithoutPaymentHandler) {
  ExpectHasEnrolledInstrumentIs(false, "non-existent-payment-method");
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentPaymentHandlerTest,
                       FalseWithPaymentHandlerAvailableToInstallJustInTime) {
  std::string payment_method =
      https_server()->GetURL("nickpay.test", "/nickpay.test/pay").spec();

  ExpectHasEnrolledInstrumentIs(false, payment_method);
}

IN_PROC_BROWSER_TEST_F(HasEnrolledInstrumentPaymentHandlerTest,
                       TrueWithInstalledPaymentHandler) {
  std::string payment_method;
  InstallPaymentApp("nickpay.test", "/nickpay.test/app.js", &payment_method);
  NavigateTo("/has_enrolled_instrument.html");

  ExpectHasEnrolledInstrumentIs(true, payment_method);
}

}  // namespace
}  // namespace payments
