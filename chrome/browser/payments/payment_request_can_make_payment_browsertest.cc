// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {

class PaymentRequestCanMakePaymentQueryTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void ExpectCanMakePayment(bool expected, const std::string& method) {
    EXPECT_EQ(
        expected,
        content::EvalJs(GetActiveWebContents(),
                        content::JsReplace("checkCanMakePayment($1)", method)));
  }

  void ExpectHasEnrolledInstrument(bool expected, const std::string& method) {
    EXPECT_EQ(expected,
              content::EvalJs(GetActiveWebContents(),
                              content::JsReplace(
                                  "checkHasEnrolledInstrument($1)", method)));
  }
};

// A user has installed a payment app that responds "false" to the
// "canmakepayment" event. PaymentRequest.canMakePayment() should  return true,
// but PaymentRequest.hasEnrolledInstrument() should return false.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       AppRespondsFalseToCanMakePaymentEvent) {
  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_false_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  ExpectCanMakePayment(true, method);
  ExpectHasEnrolledInstrument(false, method);
}

// A user has installed a payment app that responds "true" to the
// "canmakepayment" event. Both PaymentRequest.canMakePayment() and
// PaymentRequest.hasEnrolledInstrument() should return true."
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       AppRespondsTrueToCanMakePaymentEvent) {
  base::HistogramTester histogram_tester;
  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_true_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  ExpectCanMakePayment(true, method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CanMakePayment.CallAllowedByPref", /*sample=*/1,
      /*expected_bucket_count=*/1);

  ExpectHasEnrolledInstrument(true, method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.HasEnrolledInstrument.CallAllowedByPref", /*sample=*/1,
      /*expected_bucket_count=*/1);
}

// A user has installed a payment app that responds "true" to the
// "canmakepayment" event and the user is in incognito mode. In this case,
// hasEnrolledInstrument() returns false because the "canmakepayment" event is
// not fired in incognito mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       IncognitoModeWithInstalledPaymentHandler) {
  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_true_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");
  test_controller()->SetOffTheRecord(true);

  ExpectCanMakePayment(true, method);
  ExpectHasEnrolledInstrument(false, method);
}

// Nickpay is requested but not installed, but it supports just-in-time
// installation. In this case canMakePayment() returns true and
// hasEnrolledInstrument() returns false.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       AppIsNotInstalledButCanBeInstalledJustInTime) {
  std::string method =
      https_server()->GetURL("a.com", "/nickpay.test/pay").spec();

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  ExpectCanMakePayment(true, method);
  ExpectHasEnrolledInstrument(false, method);
}

// Nickpay is requested in incognito mode and it supports just-in-time
// installation but is not installed. In this case canMakePayment() returns true
// and hasEnrolledInstrument() returns false as in a normal mode.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       IncognitoModeWithJITInstallableButNotInstalledApp) {
  std::string method =
      https_server()->GetURL("a.com", "/nickpay.test/pay").spec();

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");
  test_controller()->SetOffTheRecord(true);

  ExpectCanMakePayment(true, method);
  ExpectHasEnrolledInstrument(false, method);
}

// Test the case where canMakePayment would return true, but the user has
// disabled the API in settings.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       CanMakePayment_SupportedButDisabled) {
  base::HistogramTester histogram_tester;
  test_controller()->SetCanMakePaymentEnabledPref(false);

  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_true_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  ExpectCanMakePayment(false, method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CanMakePayment.CallAllowedByPref", /*sample=*/0,
      /*expected_bucket_count=*/1);
}

// Test the case where hasEnrolledInstrument would return true, but the user has
// disabled the API in settings.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest,
                       HasEnrolledInstrument_SupportedButDisabled) {
  base::HistogramTester histogram_tester;
  test_controller()->SetCanMakePaymentEnabledPref(false);

  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_true_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  ExpectHasEnrolledInstrument(false, method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.HasEnrolledInstrument.CallAllowedByPref", /*sample=*/0,
      /*expected_bucket_count=*/1);
}

// Pages without a valid SSL certificate always get "false" from
// canMakePayment() and hasEnrolledInstrument() and NotSupported error from
// show().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest, InvalidSSL) {
  std::string method;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  content::EvalJsResult can_make_payment_result =
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("checkCanMakePayment($1)", method));
  // canMakePayment() will either reject or resolve with "false", depending on
  // timing of when the browser completes the SSL check and when the website
  // calls canMakePayment().
  // TODO(crbug.com/40858197): More consistent canMakePayment() behavior.
  EXPECT_TRUE("a JavaScript error: \"false\"\n" ==
                  can_make_payment_result.error ||
              false == can_make_payment_result.ExtractBool());

  content::EvalJsResult has_enrolled_instrument_result = content::EvalJs(
      GetActiveWebContents(),
      content::JsReplace("checkHasEnrolledInstrument($1)", method));
  // hasEnrolledInstrument() will either reject or resolve with "false",
  // depending on timing of when the browser completes the SSL check and when
  // the website calls hasEnrolledInstrument().
  // TODO(crbug.com/40858197): More consistent hasEnrolledInstrument() behavior.
  EXPECT_TRUE("a JavaScript error: \"false\"\n" ==
                  has_enrolled_instrument_result.error ||
              false == has_enrolled_instrument_result.ExtractBool());

  EXPECT_EQ("NotSupportedError: Invalid SSL certificate",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("getShowResponse($1)", method)));
}

}  // namespace payments
