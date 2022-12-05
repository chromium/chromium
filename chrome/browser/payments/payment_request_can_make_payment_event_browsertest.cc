// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_app_install_util.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

// This test suite verifies that the the "canmakepayment" event does not fire
// for standardized payment methods.

namespace payments {

using PaymentRequestCanMakePaymentEventTest =
    PaymentRequestPlatformBrowserTestBase;

// A payment handler with one URL-based payment method (its own origin) does not
// receive a "canmakepayment" event from a PaymentRequest for "interledger"
// payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest, OneUrl) {
  std::string method;
  InstallPaymentApp("a.com", "/can_make_payment_true_responder.js", &method);
  EXPECT_EQ(0U, method.find("https://"));

  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");
  EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(),
                                     "hasEnrolledInstrument('interledger')"));
}

// A payment handler with one standardized payment method ("interledger") does
// not receive a "canmakepayment" event from a PaymentRequest for "interledger"
// payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest, OneStandard) {
  ASSERT_TRUE(
      PaymentAppInstallUtil::InstallPaymentAppForPaymentMethodIdentifier(
          *GetActiveWebContents(),
          https_server()->GetURL("a.com",
                                 "/can_make_payment_true_responder.js"),
          "interledger", PaymentAppInstallUtil::IconInstall::kWithIcon));

  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");
  EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(),
                                     "hasEnrolledInstrument('interledger')"));
}

}  // namespace payments
