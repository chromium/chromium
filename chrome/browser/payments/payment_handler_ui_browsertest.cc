// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/error_strings.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace payments {
namespace {

using PaymentHandlerUiBrowserTest = PaymentRequestPlatformBrowserTestBase;

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_OpenPaymentTwiceShouldBeSuccessful \
  DISABLED_OpenPaymentTwiceShouldBeSuccessful
#else
#define MAYBE_OpenPaymentTwiceShouldBeSuccessful \
  OpenPaymentTwiceShouldBeSuccessful
#endif

// Make sure a page can open the Payment UI multiple times.
IN_PROC_BROWSER_TEST_F(PaymentHandlerUiBrowserTest,
                       MAYBE_OpenPaymentTwiceShouldBeSuccessful) {
  NavigateTo("/maxpay.test/merchant.html");

  std::string payment_method;
  InstallPaymentApp("maxpay.test", "/maxpay.test/payment_handler_sw.js",
                    &payment_method);
  EXPECT_EQ("app_is_ready",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "launchAndWaitUntilReady($1, $2)",
                                https_server()->GetURL(
                                    "maxpay.test",
                                    "/maxpay.test/payment_handler_window.html"),
                                payment_method)));
  EXPECT_TRUE(test_controller()->ClickPaymentHandlerCloseButton());
  EXPECT_EQ("User closed the Payment Request UI.",
            content::EvalJs(GetActiveWebContents(), "getResult()"));

  // The second time should be successful.
  EXPECT_EQ("app_is_ready",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace(
                                "launchAndWaitUntilReady($1, $2)",
                                https_server()->GetURL(
                                    "maxpay.test",
                                    "/maxpay.test/payment_handler_window.html"),
                                payment_method)));
  EXPECT_TRUE(test_controller()->ClickPaymentHandlerCloseButton());
  EXPECT_EQ("User closed the Payment Request UI.",
            content::EvalJs(GetActiveWebContents(), "getResult()"));
}

// Test that PaymentRequest allows one call to show() without a user activation.
class PaymentRequestActivationlessShowTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
 protected:
  void ExpectEvent2(JourneyLogger::Event2 event, bool expected) {
    std::vector<base::Bucket> buckets =
        histogram_tester_.GetAllSamples("PaymentRequest.Events2");
    ASSERT_EQ(1U, buckets.size());
    EXPECT_EQ(expected, (buckets[0].min & static_cast<int>(event)) != 0);
  }

 private:
  base::HistogramTester histogram_tester_;
};

// TODO(crbug.com/41492121): Fix flakiness and re-enable.
IN_PROC_BROWSER_TEST_F(PaymentRequestActivationlessShowTest,
                       DISABLED_ActivationlessShow) {
  std::string payment_method =
      https_server()->GetURL("a.com", "/orenpay.test/pay").spec();
  NavigateTo("b.com", "/payment_handler_status.html");
  std::string show_js = content::JsReplace("getStatus($1)", payment_method);

  // The first call to show() without a user gesture succeeds.
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(), show_js,
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));
  ExpectEvent2(JourneyLogger::Event2::kActivationlessShow, true);

  // A second call to show() without a user gesture gives an error.
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(), show_js,
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractString(),
      ::testing::HasSubstr(errors::kCannotShowWithoutUserActivation));

  // A following call to show() with a user gesture succeeds.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), show_js));
}

// Test that the JourneyLogger event for ActivationlessShow is not recorded when
// there is a user activation.
IN_PROC_BROWSER_TEST_F(PaymentRequestActivationlessShowTest,
                       EventNotRecordedForActivatedShow) {
  std::string payment_method =
      https_server()->GetURL("a.com", "/orenpay.test/pay").spec();
  NavigateTo("b.com", "/payment_handler_status.html");
  std::string show_js = content::JsReplace("getStatus($1)", payment_method);

  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), show_js));

  ExpectEvent2(JourneyLogger::Event2::kActivationlessShow, false);
}

}  // namespace
}  // namespace payments
