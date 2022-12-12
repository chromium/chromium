// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class ColocatedPaymentManifestsTest
    : public PaymentRequestPlatformBrowserTestBase {};

// When "/pay" contains both the payment method manifest and the web app
// manifest, then payment should still work.
IN_PROC_BROWSER_TEST_F(ColocatedPaymentManifestsTest, CanPay) {
  std::string method_name =
      https_server()->GetURL("a.com", "/orenpay.test/pay").spec();
  ASSERT_NE('/', method_name[method_name.length() - 1]);

  NavigateTo("b.com", "/can_make_payment_checker.html");
  EXPECT_EQ("true", content::EvalJs(
                        GetActiveWebContents(),
                        content::JsReplace("canMakePayment($1)", method_name)));

  NavigateTo("b.com", "/payment_handler_status.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("getStatus($1)", method_name)));
}

}  // namespace
}  // namespace payments
