// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/payments/payment_app_install_util.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/content/service_worker_payment_app_finder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class IgnorePaymentMethodTest : public PaymentRequestPlatformBrowserTestBase {
 protected:
  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();

    method_name_ = https_server()->GetURL("a.com", "/nickpay.test/pay").spec();
    ASSERT_NE('/', method_name_[method_name_.length() - 1]);
  }

  void InstallTestPaymentHandler(const std::string& file_name) {
    ASSERT_TRUE(
        PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
            *GetActiveWebContents(), https_server()->GetURL("a.com", file_name),
            method_name_, PaymentAppInstallUtil::IconInstall::kWithIcon));
  }

  void VerifyFunctionOutput(const std::string& expected_return_value,
                            const std::string& function_name) {
    EXPECT_EQ(expected_return_value,
              content::EvalJs(GetActiveWebContents(),
                              content::JsReplace(function_name, method_name_)));
  }

  ServiceWorkerPaymentAppFinder* GetFinder() {
    return ServiceWorkerPaymentAppFinder::GetOrCreateForCurrentDocument(
        GetActiveWebContents()->GetPrimaryMainFrame());
  }

  std::string method_name_;
};

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest, InstalledPHCannotMakePayments) {
  InstallTestPaymentHandler("/can_make_payment_true_responder.js");
  NavigateTo("b.com", "/can_make_payment_checker.html");
  VerifyFunctionOutput("true", "canMakePayment($1)");

  GetFinder()->IgnorePaymentMethodForTest(method_name_);

  VerifyFunctionOutput("false", "canMakePayment($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       InstalledPHHasNoEnrolledInstruments) {
  InstallTestPaymentHandler("/can_make_payment_true_responder.js");
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");
  VerifyFunctionOutput("true", "hasEnrolledInstrument($1)");

  GetFinder()->IgnorePaymentMethodForTest(method_name_);

  VerifyFunctionOutput("false", "hasEnrolledInstrument($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest, InstalledPHCannotBeLaunched) {
  InstallTestPaymentHandler("/payment_request_success_responder.js");
  NavigateTo("b.com", "/payment_handler_status.html");
  VerifyFunctionOutput("success", "getStatus($1)");

  GetFinder()->IgnorePaymentMethodForTest(method_name_);

  VerifyFunctionOutput("NotSupportedError: The payment method \"" +
                           method_name_ + "\" is not supported.",
                       "getStatus($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       JITInstallablePHCannotMakePayments) {
  NavigateTo("b.com", "/can_make_payment_checker.html");
  VerifyFunctionOutput("true", "canMakePayment($1)");

  GetFinder()->IgnorePaymentMethodForTest(method_name_);

  VerifyFunctionOutput("false", "canMakePayment($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       JITInstallablePHHasNoEnrolledInstruments) {
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");
  VerifyFunctionOutput("false", "hasEnrolledInstrument($1)");

  GetFinder()->IgnorePaymentMethodForTest(method_name_);

  VerifyFunctionOutput("false", "hasEnrolledInstrument($1)");
}

IN_PROC_BROWSER_TEST_F(IgnorePaymentMethodTest,
                       JITInstallablePHCanBeInstalledAndLaunchedByDefault) {
  NavigateTo("b.com", "/payment_handler_status.html");
  VerifyFunctionOutput("success", "getStatus($1)");
}

IN_PROC_BROWSER_TEST_F(
    IgnorePaymentMethodTest,
    JITInstallablePHCannotBeInstalledAndLaunchedWhenIgnored) {
  NavigateTo("b.com", "/payment_handler_status.html");

  GetFinder()->IgnorePaymentMethodForTest(method_name_);

  VerifyFunctionOutput("NotSupportedError: The payment method \"" +
                           method_name_ + "\" is not supported.",
                       "getStatus($1)");
}

}  // namespace
}  // namespace payments
