// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

namespace payments {

namespace {

class PaymentHandlerUninstallTest
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentHandlerUninstallTest() = default;
  ~PaymentHandlerUninstallTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("a.com", "/payment_handler.html");
  }
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerUninstallTest, URLBasedPaymentMethod) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  // Launch the payment request and confirm checkout completion.
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("launch($1)", method_name)));
  WaitForObservedEvent();

  // Uninstall the payment app and verify that a new request.show() gets
  // rejected after the app uninstallation.
  EXPECT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("uninstall($1)",
                                   https_server()->GetURL(
                                       "a.com", "/payment_handler_sw.js"))));
  ResetEventWaiterForSingleEvent(TestEvent::kNotSupportedError);
  EXPECT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("launchWithoutWaitForResponse($1)", method_name)));
  WaitForObservedEvent();
}

}  // namespace

}  // namespace payments
