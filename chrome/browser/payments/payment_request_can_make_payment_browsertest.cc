// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
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

class PaymentRequestCanMakePaymentQueryWithFeatureTest
    : public PaymentRequestCanMakePaymentQueryTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  PaymentRequestCanMakePaymentQueryWithFeatureTest() {
    std::vector<base::test::FeatureRefAndParams> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features = {
        features::kWebPaymentsExperimentalFeatures};

    if (CanMakePaymentTrueWhenPrivateFeatureEnabled()) {
      enabled_features.push_back(
          {features::kCanMakePaymentTrueWhenPrivate, {}});
    } else {
      disabled_features.push_back(features::kCanMakePaymentTrueWhenPrivate);
    }

    scoped_feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                                       disabled_features);
  }

  ~PaymentRequestCanMakePaymentQueryWithFeatureTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestCanMakePaymentQueryTest::SetUpOnMainThread();
    test_controller()->SetCanMakePaymentEnabledPref(
        CanMakePaymentEnabledPref());
  }

  bool CanMakePaymentEnabledPref() const { return std::get<0>(GetParam()); }

  bool CanMakePaymentTrueWhenPrivateFeatureEnabled() const {
    return std::get<1>(GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// A user has installed a payment app that responds "true" to the
// "canmakepayment" event. This test verifies the behavior of canMakePayment()
// and hasEnrolledInstrument() with different combinations of the
// CanMakePaymentEnabled pref and the kCanMakePaymentTrueWhenPrivate feature
// flag.
IN_PROC_BROWSER_TEST_P(PaymentRequestCanMakePaymentQueryWithFeatureTest,
                       AppRespondsTrueToCanMakePaymentEvent) {
  base::HistogramTester histogram_tester;

  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_true_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  // When the pref is true, canMakePayment should be true.
  // When the pref is false, canMakePayment is true only if the
  // kCanMakePaymentTrueWhenPrivate feature is enabled.
  ExpectCanMakePayment(CanMakePaymentEnabledPref() ||
                           CanMakePaymentTrueWhenPrivateFeatureEnabled(),
                       method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CanMakePayment.CallAllowedByPref",
      /*sample=*/CanMakePaymentEnabledPref(),
      /*expected_bucket_count=*/1);

  // hasEnrolledInstrument is only true if the pref is true. The feature
  // does not affect it.
  ExpectHasEnrolledInstrument(CanMakePaymentEnabledPref(), method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.HasEnrolledInstrument.CallAllowedByPref",
      /*sample=*/CanMakePaymentEnabledPref(),
      /*expected_bucket_count=*/1);
}

// A user has installed a payment app that responds "false" to the
// "canmakepayment" event. This test verifies the behavior of canMakePayment()
// and hasEnrolledInstrument() with different combinations of the
// CanMakePaymentEnabled pref and the kCanMakePaymentTrueWhenPrivate feature
// flag. hasEnrolledInstrument() should always be false in this case.
IN_PROC_BROWSER_TEST_P(PaymentRequestCanMakePaymentQueryWithFeatureTest,
                       AppRespondsFalseToCanMakePaymentEvent) {
  base::HistogramTester histogram_tester;

  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_false_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  // When the pref is true, canMakePayment should be true.
  // When the pref is false, canMakePayment is true only if the
  // kCanMakePaymentTrueWhenPrivate feature is enabled.
  ExpectCanMakePayment(CanMakePaymentEnabledPref() ||
                           CanMakePaymentTrueWhenPrivateFeatureEnabled(),
                       method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CanMakePayment.CallAllowedByPref",
      /*sample=*/CanMakePaymentEnabledPref(),
      /*expected_bucket_count=*/1);

  // hasEnrolledInstrument is always false for this app.
  ExpectHasEnrolledInstrument(false, method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.HasEnrolledInstrument.CallAllowedByPref",
      /*sample=*/CanMakePaymentEnabledPref(),
      /*expected_bucket_count=*/1);
}

// A website requests a payment method that is not supported by any installed
// payment handler. This test verifies the behavior of canMakePayment() and
// hasEnrolledInstrument() with different combinations of the
// CanMakePaymentEnabled pref and the kCanMakePaymentTrueWhenPrivate feature
// flag. hasEnrolledInstrument() should always be false in this case.
IN_PROC_BROWSER_TEST_P(PaymentRequestCanMakePaymentQueryWithFeatureTest,
                       UnsupportedUrlBasedMethod) {
  base::HistogramTester histogram_tester;

  // Use a non-existent payment app.
  std::string method = "https://non-existent-payment-handler.com/pay.js";

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");

  // canMakePayment should only be true if the pref is false and the
  // kCanMakePaymentTrueWhenPrivate feature is enabled.
  ExpectCanMakePayment(!CanMakePaymentEnabledPref() &&
                           CanMakePaymentTrueWhenPrivateFeatureEnabled(),
                       method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.CanMakePayment.CallAllowedByPref",
      /*sample=*/CanMakePaymentEnabledPref(),
      /*expected_bucket_count=*/1);

  // hasEnrolledInstrument is always false since the app doesn't exist.
  ExpectHasEnrolledInstrument(false, method);
  histogram_tester.ExpectUniqueSample(
      "PaymentRequest.HasEnrolledInstrument.CallAllowedByPref",
      /*sample=*/CanMakePaymentEnabledPref(),
      /*expected_bucket_count=*/1);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PaymentRequestCanMakePaymentQueryWithFeatureTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    [](const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
      return base::StringPrintf(
          "Pref%s_Feature%s", std::get<0>(info.param) ? "True" : "False",
          std::get<1>(info.param) ? "Enabled" : "Disabled");
    });

// Pages without a valid SSL certificate always get "false" from
// canMakePayment() and hasEnrolledInstrument() and NotSupported error from
// show().
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentQueryTest, InvalidSSL) {
  std::string method;
  InstallPaymentApp("a.com", "/payment_request_success_responder.js", &method);

  NavigateTo("b.com", "/payment_request_can_make_payment_query_test.html");
  test_controller()->SetValidSsl(false);

  // canMakePayment() will either reject or resolve with "false", depending on
  // timing of when the browser completes the SSL check and when the website
  // calls canMakePayment().
  // TODO(crbug.com/40858197): More consistent canMakePayment() behavior.
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("checkCanMakePayment($1)", method)),
      testing::AnyOf(
          content::EvalJsResult::ErrorIs("a JavaScript error: \"false\"\n"),
          content::EvalJsResult::IsOkAndHolds(testing::Eq(false))));

  // hasEnrolledInstrument() will either reject or resolve with "false",
  // depending on timing of when the browser completes the SSL check and when
  // the website calls hasEnrolledInstrument().
  // TODO(crbug.com/40858197): More consistent hasEnrolledInstrument() behavior.
  EXPECT_THAT(
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("checkHasEnrolledInstrument($1)", method)),
      testing::AnyOf(
          content::EvalJsResult::ErrorIs("a JavaScript error: \"false\"\n"),
          content::EvalJsResult::IsOkAndHolds(testing::Eq(false))));

  EXPECT_EQ("NotSupportedError: Invalid SSL certificate",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("getShowResponse($1)", method)));
}

}  // namespace payments
