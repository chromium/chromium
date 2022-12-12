// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class TwoPaymentRequestsTest : public PaymentRequestPlatformBrowserTestBase {};

// Instantiating two PaymentRequest objects at the same time should provide
// identical results.
IN_PROC_BROWSER_TEST_F(TwoPaymentRequestsTest, Smoke) {
  std::string method_name;
  InstallPaymentApp("alicepay.test", "/alicepay.test/app1/app.js",
                    &method_name);

  NavigateTo("test.com", "/two_payment_requests.html");
  EXPECT_EQ(
      "true",
      content::EvalJs(GetActiveWebContents(),
                      content::JsReplace(
                          "hasEnrolledInstrumentInTwoPaymentRequestObjects($1)",
                          method_name)));
}

}  // namespace
}  // namespace payments
