// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
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
}  // namespace
}  // namespace payments
