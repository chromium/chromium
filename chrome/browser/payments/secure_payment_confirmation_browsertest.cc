// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

static constexpr char kTestMethodData[] =
    "[{ "
    "  supportedMethods: 'secure-payment-confirmation',"
    "  data: {"
    "    action: 'authenticate',"
    "    instrumentId: 'x',"
    "    networkData: Uint8Array.from('x', c => c.charCodeAt(0)),"
    "    timeout: 60000,"
    "    fallbackUrl: 'https://fallback.example/url'"
    "}}]";

std::string getInvokePaymentRequestSnippet() {
  return base::StringPrintf("getStatusForMethodData(%s)", kTestMethodData);
}

class SecurePaymentConfirmationTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentRequestPlatformBrowserTestBase::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest, NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      getInvokePaymentRequestSnippet()));
}

#if defined(OS_ANDROID)
// TODO(https://crbug.com/1110320): Implement SetHasAuthenticator() for Android,
// so this behavior can be tested on Android as well.
#define MAYBE_PaymentSheetShowsApp DISABLED_PaymentSheetShowsApp
#else
#define MAYBE_PaymentSheetShowsApp PaymentSheetShowsApp
#endif  // OS_ANDROID
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_PaymentSheetShowsApp) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/payment_handler_status.html");
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  // ExecJs starts executing JavaScript and immediately returns, not waiting for
  // any promise to return.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              getInvokePaymentRequestSnippet()));

  WaitForObservedEvent();
  ASSERT_FALSE(test_controller()->app_descriptions().empty());
  EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  EXPECT_EQ("Stub label", test_controller()->app_descriptions().front().label);
}

// canMakePayment() and hasEnrolledInstrument() should return false on platforms
// without a compatible authenticator.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       CanMakePayment_NoAuthenticator) {
  test_controller()->SetHasAuthenticator(false);
  NavigateTo("a.com", "/can_make_payment_checker.html");

  {
    std::string snippet =
        base::StringPrintf("canMakePaymentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
  {
    std::string snippet = base::StringPrintf(
        "hasEnrolledInstrumentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
}

// canMakePayment() and hasEnrolledInstrument() should return true on platforms
// with a compatible authenticator regardless of the presence of payment
// credentials.
#if defined(OS_ANDROID)
// TODO(https://crbug.com/1110320): Implement SetHasAuthenticator() for Android,
// so this behavior can be tested on Android as well.
#define MAYBE_CanMakePayment_HasAuthenticator \
  DISABLED_CanMakePayment_HasAuthenticator
#else
#define MAYBE_CanMakePayment_HasAuthenticator CanMakePayment_HasAuthenticator
#endif  // OS_ANDROID
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationTest,
                       MAYBE_CanMakePayment_HasAuthenticator) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/can_make_payment_checker.html");

  {
    std::string snippet =
        base::StringPrintf("canMakePaymentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("true", content::EvalJs(GetActiveWebContents(), snippet));
  }
  {
    std::string snippet = base::StringPrintf(
        "hasEnrolledInstrumentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("true", content::EvalJs(GetActiveWebContents(), snippet));
  }
}

// Intentionally do not enable the "SecurePaymentConfirmation" Blink runtime
// feature.
class SecurePaymentConfirmationDisabledTest
    : public PaymentRequestPlatformBrowserTestBase {};

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledTest,
                       PaymentMethodNotSupported) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/payment_handler_status.html");

  // EvalJs waits for JavaScript promise to resolve.
  EXPECT_EQ(
      "The payment method \"secure-payment-confirmation\" is not supported.",
      content::EvalJs(GetActiveWebContents(),
                      getInvokePaymentRequestSnippet()));
}

IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationDisabledTest,
                       CannotMakePayment) {
  test_controller()->SetHasAuthenticator(true);
  NavigateTo("a.com", "/can_make_payment_checker.html");

  {
    std::string snippet =
        base::StringPrintf("canMakePaymentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
  {
    std::string snippet = base::StringPrintf(
        "hasEnrolledInstrumentForMethodData(%s)", kTestMethodData);
    EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(), snippet));
  }
}

}  // namespace
}  // namespace payments
