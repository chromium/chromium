// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

using PaymentRequestIdTest = PaymentRequestPlatformBrowserTestBase;

IN_PROC_BROWSER_TEST_F(PaymentRequestIdTest, ResponseIdEqualsRequestId) {
  NavigateTo("a.com", "/payment_request_id_test.html");
  GURL method = https_server()->GetURL("b.com", "/nickpay.test/pay");
  std::string get_id = content::JsReplace("getResponseId($1)", method.spec());

  EXPECT_EQ("my_payment_id", content::EvalJs(GetActiveWebContents(), get_id));
}

}  // namespace payments
