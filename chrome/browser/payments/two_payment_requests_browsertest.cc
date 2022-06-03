// Copyright 2020 The Chromium Authors. All rights reserved.
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
  std::string app_host = "alicepay.com";
  std::string method_name = https_server()->GetURL(app_host, "/").spec();
  EXPECT_EQ('/', method_name[method_name.size() - 1]);
  method_name = method_name.substr(0, method_name.size() - 1);
  EXPECT_NE('/', method_name[method_name.size() - 1]);

  std::string install_script = content::JsReplace(
      "install('alicepay.com/app1/app.js', [$1], false)", method_name);
  NavigateTo(app_host, "/payment_handler_installer.html");
  EXPECT_EQ("success", content::EvalJs(GetActiveWebContents(), install_script));

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
