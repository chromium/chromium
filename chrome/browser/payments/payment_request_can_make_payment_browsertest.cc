// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

namespace payments {

class PaymentRequestCanMakePaymentQueryTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentRequestCanMakePaymentQueryTest(
      const PaymentRequestCanMakePaymentQueryTest&) = delete;
  PaymentRequestCanMakePaymentQueryTest& operator=(
      const PaymentRequestCanMakePaymentQueryTest&) = delete;
  net::EmbeddedTestServer nickpay_server_;

 protected:
  PaymentRequestCanMakePaymentQueryTest()
      : nickpay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();

    // Choosing nickpay for its JIT installation support.
    nickpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/nickpay.com/");

    ASSERT_TRUE(nickpay_server_.Start());
  }

  void NavigateTo(const std::string& file_path) {
    PaymentRequestPlatformBrowserTestBase::NavigateTo("a.com", file_path);
  }

  void CallCanMakePaymentWithMethod(const std::string& method) {
    ResetEventWaiterForEventSequence(
        {TestEvent::kCanMakePaymentCalled, TestEvent::kCanMakePaymentReturned});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(),
        content::JsReplace("buyWithMethods([{supportedMethods:$1}]);",
                           method)));
    WaitForObservedEvent();
  }

  void CallHasEnrolledInstrumentWithMethod(const std::string& method) {
    ResetEventWaiterForEventSequence(
        {TestEvent::kHasEnrolledInstrumentCalled,
         TestEvent::kHasEnrolledInstrumentReturned});
    ASSERT_TRUE(content::ExecuteScript(
        GetActiveWebContents(),
        content::JsReplace(
            "hasEnrolledInstrumentWithMethods([{supportedMethods:$1}]);",
            method)));
    WaitForObservedEvent();
  }
};

// A payment method is required, user has installed the payment app, the
// payment app responds true to the "canmakepayment" event.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported) {
  std::string method;
  InstallPaymentApp("a.com", "payment_request_success_responder.js", &method);

  NavigateTo("/payment_request_can_make_payment_query_test.html");

  CallCanMakePaymentWithMethod(method);
  ExpectBodyContains("true");

  CallHasEnrolledInstrumentWithMethod(method);
  ExpectBodyContains("true");
}

// A payment method is required, user has installed the payment app, the
// payment app responds true to the "canmakepayment" event and user is in
// incognito mode. In this case, hasEnrolledInstrument() returns false because
// the "canmakepayment" event is not fired in incognito mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_Supported_InIncognitoMode) {
  std::string method;
  InstallPaymentApp("a.com", "payment_request_success_responder.js", &method);

  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetOffTheRecord(true);

  CallCanMakePaymentWithMethod(method);
  ExpectBodyContains("true");

  CallHasEnrolledInstrumentWithMethod(method);
  ExpectBodyContains("false");
}

// Nickpay is requested but not installed, but it supports just-in-time
// installation. In this case canMakePayment() returns true and
// hasEnrolledInstrument() returns false.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       DISABLED_CanMakePayment_NotSupported) {
  std::string method = nickpay_server_.GetURL("nickpay.com", "/pay").spec();

  NavigateTo("/payment_request_can_make_payment_query_test.html");

  CallCanMakePaymentWithMethod(method);
  ExpectBodyContains("true");

  CallHasEnrolledInstrumentWithMethod(method);
  ExpectBodyContains("false");
}

// Nickpay is requested in incognito mode and it supports just-in-time
// installation but is not installed. In this case canMakePayment() returns true
// and hasEnrolledInstrument() returns false as in a normal mode.
// DISABLED for flakiness. See https://crbug.com/1288946
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       DISABLED_CanMakePayment_NotSupported_InIncognitoMode) {
  std::string method = nickpay_server_.GetURL("nickpay.com", "/pay").spec();

  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetOffTheRecord(true);

  CallCanMakePaymentWithMethod(method);
  ExpectBodyContains("true");

  CallHasEnrolledInstrumentWithMethod(method);
  ExpectBodyContains("false");
}

// Test the case where canMakePayment/hasEnrolledInstrument would return true,
// but the user has disabled those APIs in settings.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_SupportedButDisabled) {
  test_controller()->SetCanMakePaymentEnabledPref(false);

  std::string method;
  InstallPaymentApp("a.com", "payment_request_success_responder.js", &method);

  NavigateTo("/payment_request_can_make_payment_query_test.html");

  CallCanMakePaymentWithMethod(method);
  ExpectBodyContains("false");

  CallHasEnrolledInstrumentWithMethod(method);
  ExpectBodyContains("false");
}

// Pages without a valid SSL certificate always get "false" from
// .canMakePayment().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_InvalidSSL) {
  std::string method;
  InstallPaymentApp("a.com", "payment_request_success_responder.js", &method);

  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  ResetEventWaiterForEventSequence({TestEvent::kConnectionTerminated});
  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      content::JsReplace("buyWithMethods([{supportedMethods:$1}]);", method)));
  WaitForObservedEvent();
  ExpectBodyContains("false");
}

// Pages without a valid SSL certificate always get NotSupported error from
// .show().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest, Show_InvalidSSL) {
  std::string method;
  InstallPaymentApp("a.com", "payment_request_success_responder.js", &method);

  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  ResetEventWaiterForEventSequence({TestEvent::kConnectionTerminated});
  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      content::JsReplace("showWithMethods([{supportedMethods:$1}]);", method)));
  WaitForObservedEvent();
  ExpectBodyContains("NotSupportedError: Invalid SSL certificate");
}

// Pages without a valid SSL certificate always get "false" from
// .hasEnrolledInstrument().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       HasEnrolledInstrument_InvalidSSL) {
  std::string method;
  InstallPaymentApp("a.com", "payment_request_success_responder.js", &method);

  NavigateTo("/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  ResetEventWaiterForEventSequence({TestEvent::kConnectionTerminated});
  ASSERT_TRUE(content::ExecuteScript(
      GetActiveWebContents(),
      content::JsReplace(
          "hasEnrolledInstrumentWithMethods([{supportedMethods:$1}]);",
          method)));
  WaitForObservedEvent();
  ExpectBodyContains("false");
}

}  // namespace payments
