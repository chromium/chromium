// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

using ::testing::StartsWith;

// Tests for cases where web-based payment handlers reject the promise passed to
// PaymentRequestEvent.respondWith, indicating either user cancellation of the
// flow from within the payment app, or an internal error within the app.
//
// The tests are parameterized to be run with and without the
// kPaymentRequestSupportReportingAppError feature.
class PaymentHandlerErrorTest : public PaymentRequestPlatformBrowserTestBase,
                                public testing::WithParamInterface<bool> {
 public:
  PaymentHandlerErrorTest() {
    feature_list_.InitWithFeatureState(
        features::kPaymentRequestSupportReportingAppError, GetParam());
  }

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("/payment_handler_status.html");
  }

  std::string ShowWithData(const std::string& method, const std::string& data) {
    return content::EvalJs(
               GetActiveWebContents(),
               content::JsReplace("getStatusForMethodData([{supportedMethods: "
                                  "$1, data: JSON.parse($2)}])",
                                  method, data))
        .ExtractString();
  }

  // PaymentRequestTestObserver:
  void OnErrorDisplayed() override {
    PaymentRequestPlatformBrowserTestBase::OnErrorDisplayed();

    // Immediately close the dialog, to let the show() promise be resolved.
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            base::IgnoreResult(&PaymentRequestTestController::CloseDialog),
            base::Unretained(test_controller())));
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All, PaymentHandlerErrorTest, testing::Bool());

IN_PROC_BROWSER_TEST_P(PaymentHandlerErrorTest, RejectWithOperationError) {
  std::string method;
  InstallPaymentApp("a.com", "/error_responder_app.js", &method);

  // When the payment handler rejects with OperationError, the merchant should
  // receive either OperationError (feature enabled) or AbortError (feature
  // disabled).
  EXPECT_THAT(ShowWithData(method, "{\"errorType\": \"operation_error\"}"),
              StartsWith(GetParam() ? "OperationError" : "AbortError"));
}

IN_PROC_BROWSER_TEST_P(PaymentHandlerErrorTest, GenericReject) {
  std::string method;
  InstallPaymentApp("a.com", "/error_responder_app.js", &method);

  // When the payment handler rejects with a generic error, the merchant should
  // receive AbortError whether the feature is on or off.
  EXPECT_THAT(ShowWithData(method, "{\"errorType\": \"reject\"}"),
              StartsWith("AbortError"));
}

}  // namespace
}  // namespace payments
