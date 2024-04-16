// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class AbortPaymentHandlerTest : public PaymentRequestPlatformBrowserTestBase {};

// TODO(crbug.com/40720284): fix flakiness and reenable
#if BUILDFLAG(IS_MAC)
#define MAYBE_CanAbortInvokedInstalledPaymentHandler \
  DISABLED_CanAbortInvokedInstalledPaymentHandler
#else
#define MAYBE_CanAbortInvokedInstalledPaymentHandler \
  CanAbortInvokedInstalledPaymentHandler
#endif

IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       MAYBE_CanAbortInvokedInstalledPaymentHandler) {
  std::string method_name;
  InstallPaymentApp("a.com", "/abort_responder_app.js", &method_name);

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Abort completed",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/true)));
}

IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       CanAbortInvokedJitPaymentHandler) {
  std::string method_name =
      https_server()->GetURL("a.com", "/abort_responder_app.json").spec();
  ASSERT_NE('/', method_name[method_name.length() - 1]);

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Abort completed",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/true)));
}

IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       InstalledPaymentHandlerCanRefuseAbort) {
  std::string method_name;
  InstallPaymentApp("a.com", "/abort_responder_app.js", &method_name);

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Failed to execute 'abort' on 'PaymentRequest': Unable to abort the "
      "payment",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/false)));
}

IN_PROC_BROWSER_TEST_F(AbortPaymentHandlerTest,
                       JitPaymentHandlerCanRefuseAbort) {
  std::string method_name =
      https_server()->GetURL("a.com", "/abort_responder_app.json").spec();
  ASSERT_NE('/', method_name[method_name.length() - 1]);

  NavigateTo("b.com", "/payment_handler_aborter.html");
  EXPECT_EQ(
      "Failed to execute 'abort' on 'PaymentRequest': Unable to abort the "
      "payment",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace("launchAndAbort($1, $2)", method_name,
                                         /*abortResponse=*/false)));
}

}  // namespace
}  // namespace payments
