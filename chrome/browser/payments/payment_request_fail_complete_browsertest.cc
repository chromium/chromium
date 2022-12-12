// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentRequetFailCompleteTest
    : public PaymentRequestPlatformBrowserTestBase {
 private:
  // PaymentRequestTestObserver:
  void OnErrorDisplayed() override {
    PaymentRequestPlatformBrowserTestBase::OnErrorDisplayed();
    ASSERT_TRUE(test_controller()->CloseDialog());
  }
};

IN_PROC_BROWSER_TEST_F(PaymentRequetFailCompleteTest, TransactionFailed) {
  NavigateTo("a.com", "/payment_request_fail_complete_test.html");
  GURL method = https_server()->GetURL("b.com", "/nickpay.test/pay");
  std::string fail_complete = content::JsReplace(
      "failComplete([{supportedMethods:$1}])", method.spec());

  EXPECT_EQ("Transaction failed",
            content::EvalJs(GetActiveWebContents(), fail_complete));
}

}  // namespace payments