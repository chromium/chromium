// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

struct TestCase {
  TestCase(const std::string& init_test_code,
           const std::string& expected_output)
      : init_test_code(init_test_code), expected_output(expected_output) {}

  ~TestCase() = default;

  const std::string init_test_code;
  const std::string expected_output;
};

class PaymentHandlerChangePaymentMethodTest
    : public PaymentRequestPlatformBrowserTestBase,
      public testing::WithParamInterface<TestCase> {};

IN_PROC_BROWSER_TEST_P(PaymentHandlerChangePaymentMethodTest, Test) {
  NavigateTo("a.com", "/change_payment_method.html");

  std::string method_name;
  InstallPaymentApp("a.com", "/change_payment_method_app.js", &method_name);

  ASSERT_TRUE(
      content::ExecJs(GetActiveWebContents(), GetParam().init_test_code));

  std::string actual_output =
      content::EvalJs(GetActiveWebContents(),
                      "outputChangePaymentMethodReturnValue(request);")
          .ExtractString();

  // The test expectations are hard-coded, but the embedded test server changes
  // its port number in every test, e.g., https://a.com:34548.
  ASSERT_EQ(ClearPortNumber(actual_output), GetParam().expected_output)
      << "When executing " << GetParam().init_test_code;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    PaymentHandlerChangePaymentMethodTest,
    testing::Values(
        // If the merchant does not have a "paymentmethodchange" event handler,
        // then calling PaymentRequestEvent.changePaymentMethod() in the payment
        // handler will return null.
        TestCase("initTestNoHandler();",
                 "PaymentRequest.show(): changePaymentMethod() returned: null"),
        // If the merchant responds to the "paymentmethodchange" event with a
        // rejected promise, then PaymentRequest.show() gets rejected.
        TestCase("initTestReject()",
                 "PaymentRequest.show() rejected with: Error for test"),
        // If the merchant throws an "Error" in its "paymentmethodchange" event
        // handler, then PaymentRequest.show() gets rejected.
        TestCase("initTestThrow()",
                 "PaymentRequest.show() rejected with: Error: Error for test"),
        // If the merchant responds to a "paymentmethodchange" event with
        // updated details, including modifiers for multiple payment method
        // names, then the invoked payment handler receives the updated details,
        // except the modifiers for non-matching payment method names.
        TestCase(
            "initTestDetails()",
            "PaymentRequest.show(): changePaymentMethod() returned: "
            "{\"error\":\"Error for test\","
            "\"modifiers\":[{\"data\":{\"soup\":\"potato\"},"
            "\"supportedMethods\":\"https://a.com\","
            "\"total\":{\"amount\":{\"currency\":\"EUR\",\"value\":\"0.03\"},"
            "\"label\":\"\",\"pending\":false}}],"
            "\"paymentMethodErrors\":{\"country\":\"Unsupported country\"},"
            "\"total\":{\"currency\":\"GBP\",\"value\":\"0.02\"}}")));

}  // namespace
}  // namespace payments
