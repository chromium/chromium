// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

class PaymentHandlerInstallFailedTest
    : public PaymentRequestPlatformBrowserTestBase {
 private:
  void OnErrorDisplayed() override {
    PaymentRequestPlatformBrowserTestBase::OnErrorDisplayed();
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&PaymentHandlerInstallFailedTest::CloseDialog,
                                  base::Unretained(this)));
  }

  void CloseDialog() { ASSERT_TRUE(test_controller()->CloseDialog()); }
};

// When the service worker file is missing, the error message should mention
// that the payment handler install failed. The metrics should record a failed
// installation.
IN_PROC_BROWSER_TEST_F(PaymentHandlerInstallFailedTest, Test) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 0);

  std::string method_name =
      https_server()->GetURL("a.com", "/paulpay.test/pay").spec();
  ASSERT_NE('/', method_name[method_name.length() - 1]);
  NavigateTo("b.com", "/can_make_payment_checker.html");
  EXPECT_EQ("true", content::EvalJs(
                        GetActiveWebContents(),
                        content::JsReplace("canMakePayment($1)", method_name)));

  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 0);

  NavigateTo("b.com", "/payment_handler_status.html");
  EXPECT_EQ("AbortError: Failed to install the payment handler.",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("getStatus($1)", method_name)));

  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", true, 0);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", false, 1);
}

}  // namespace
}  // namespace payments
