// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"

// This test suite verifies that the the "canmakepayment" event does not fire
// for standardized payment methods.

namespace payments {

class PaymentRequestCanMakePaymentEventTest
    : public PaymentRequestPlatformBrowserTestBase {};

// A payment handler with two standardized payment methods ("interledger" and
// "basic-card") and one URL-based payment method (its own scope) does not
// receive a "canmakepayment" event from a PaymentRequest for "interledger"
// payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest,
                       TwoStandardOneUrl) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "install('can_make_payment_true_responder.js', "
                            "['interledger', 'basic-card'], true)"));
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");

  EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(),
                                     "hasEnrolledInstrument('interledger')"));
}

// A payment handler with two standardized payment methods ("interledger" and
// "basic-card") does not receive a "canmakepayment" event from a PaymentRequest
// for "interledger" payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest, TwoStandard) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "install('can_make_payment_true_responder.js', "
                            "['interledger', 'basic-card'], false)"));
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");

  EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(),
                                     "hasEnrolledInstrument('interledger')"));
}

// A payment handler with one standardized payment method ("interledger") does
// not receive a "canmakepayment" event from a PaymentRequest for "interledger"
// payment method.
IN_PROC_BROWSER_TEST_F(PaymentRequestCanMakePaymentEventTest, OneStandard) {
  NavigateTo("a.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "install('can_make_payment_true_responder.js', "
                            "['interledger'], false)"));
  NavigateTo("b.com", "/has_enrolled_instrument_checker.html");

  EXPECT_EQ("false", content::EvalJs(GetActiveWebContents(),
                                     "hasEnrolledInstrument('interledger')"));
}

}  // namespace payments
