// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/error_strings.h"
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

// Test that the PaymentRequestAllowOneActivationlessShow feature allows one
// call to show() without a user activation.
class PaymentRequestActivationlessShowTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "PaymentRequestAllowOneActivationlessShow");
  }
};

IN_PROC_BROWSER_TEST_F(PaymentRequestActivationlessShowTest,
                       ActivationlessShow) {
  std::string payment_method =
      https_server()->GetURL("a.com", "/orenpay.test/pay").spec();
  NavigateTo("b.com", "/payment_handler_status.html");
  std::string show_js = content::JsReplace("getStatus($1)", payment_method);

  // The first call to show() without a user gesture succeeds.
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(), show_js,
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE));

  // A second call to show() without a user gesture gives an error.
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(), show_js,
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractString(),
      ::testing::HasSubstr(errors::kCannotShowWithoutUserActivation));

  // A following call to show() with a user gesture succeeds.
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), show_js));
}

// Test that activationless show() call is not allowed with the
// PaymentRequetsAllowOneActivationlessShow feature disabled.
class PaymentRequestActivationlessShowDisabledTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kDisableBlinkFeatures,
                                    "PaymentRequestAllowOneActivationlessShow");
  }
};

IN_PROC_BROWSER_TEST_F(PaymentRequestActivationlessShowDisabledTest,
                       ActivationlessShow) {
  std::string payment_method =
      https_server()->GetURL("a.com", "/orenpay.test/pay").spec();
  NavigateTo("b.com", "/payment_handler_status.html");

  // The error message with activationless show enabled has changed, so the old
  // error message is hard coded below and will be removed post launch.
  EXPECT_THAT(
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("getStatus($1)", payment_method),
                      content::EvalJsOptions::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractString(),
      ::testing::HasSubstr(
          "Failed to execute 'show' on 'PaymentRequest': PaymentRequest.show() "
          "requires either transient user activation or delegated payment "
          "request capability"));
}

}  // namespace
}  // namespace payments
