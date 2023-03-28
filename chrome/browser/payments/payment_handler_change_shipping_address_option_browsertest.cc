// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {
namespace {

constexpr const char* kNoMerchantResponseExpectedOutput =
    "PaymentRequest.show(): changeShipping[Address|Option]() returned: null";
constexpr const char* kPromiseRejectedExpectedOutput =
    "PaymentRequest.show() rejected with: Error for test";
constexpr const char* kExeptionThrownExpectedOutput =
    "PaymentRequest.show() rejected with: Error: Error for test";
constexpr const char* kSuccessfulMerchantResponseExpectedOutput =
    "PaymentRequest.show(): changeShipping[Address|Option]() returned: "
    "{\"error\":\"Error for "
    "test\",\"modifiers\":[{\"data\":{\"soup\":\"potato\"},"
    "\"supportedMethods\":\"https://a.com"
    "\",\"total\":{\"amount\":{\"currency\":\"EUR\",\"value\":\"0.03\"},"
    "\"label\":\"\",\"pending\":false}}],\"paymentMethodErrors\":{\"country\":"
    "\"Unsupported "
    "country\"},\"shippingAddressErrors\":{\"addressLine\":\"\",\"city\":\"\","
    "\"country\":\"US only "
    "shipping\",\"dependentLocality\":\"\",\"organization\":\"\",\"phone\":"
    "\"\",\"postalCode\":\"\",\"recipient\":\"\",\"region\":\"\","
    "\"sortingCode\":\"\"},\"shippingOptions\":[{\"amount\":{\"currency\":"
    "\"JPY\",\"value\":\"0.05\"},\"id\":\"id\",\"label\":\"Shipping "
    "option\",\"selected\":true}],\"total\":{\"currency\":\"GBP\",\"value\":"
    "\"0.02\"}}";

enum class ChangeType { kAddressChange, kOptionChange };

struct TestCase {
  TestCase(const std::string& init_test_code,
           const std::string& expected_output,
           ChangeType change_type)
      : init_test_code(init_test_code),
        expected_output(expected_output),
        change_type(change_type) {}

  ~TestCase() {}

  const std::string init_test_code;
  const std::string expected_output;
  const ChangeType change_type;
};

class PaymentHandlerChangeShippingAddressOptionTest
    : public PaymentRequestPlatformBrowserTestBase,
      public testing::WithParamInterface<TestCase> {
 protected:
  PaymentHandlerChangeShippingAddressOptionTest() = default;
  ~PaymentHandlerChangeShippingAddressOptionTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("a.com", "/change_shipping_address_option.html");
  }

  std::string getTestType() {
    return GetParam().change_type == ChangeType::kOptionChange ? "option"
                                                               : "address";
  }
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerChangeShippingAddressOptionTest, Test) {
  std::string method_name;
  InstallPaymentApp("a.com", "/change_shipping_" + getTestType() + "_app.js",
                    &method_name);

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "delegateShippingAddressToPaymentHandler()"));

  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(), GetParam().init_test_code));

  std::string actual_output =
      content::EvalJs(GetActiveWebContents(),
                      "outputChangeShippingAddressOptionReturnValue(request);")
          .ExtractString();

  // The test expectations are hard-coded, but the embedded test server changes
  // its port number in every test, e.g., https://a.com:34548.
  EXPECT_EQ(ClearPortNumber(actual_output), GetParam().expected_output)
      << "When executing " << GetParam().init_test_code;
}

// If the merchant does not have a "shipping(address|option)change" event
// handler, then calling PaymentRequestEvent.changeShipping(Address|Option)() in
// the payment handler will return null.
INSTANTIATE_TEST_SUITE_P(
    NoMerchantResponse,
    PaymentHandlerChangeShippingAddressOptionTest,
    testing::Values(TestCase("initTestNoHandler();",
                             kNoMerchantResponseExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestNoHandler();",
                             kNoMerchantResponseExpectedOutput,
                             ChangeType::kOptionChange)));

// If the merchant responds to the "payment(address|option)change" event with a
// rejected promise or throws an "Error" inside the promise, then
// PaymentRequest.show() gets rejected.
INSTANTIATE_TEST_SUITE_P(
    ErrorCases,
    PaymentHandlerChangeShippingAddressOptionTest,
    testing::Values(TestCase("initTestReject('shippingaddresschange')",
                             kPromiseRejectedExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestReject('shippingoptionchange')",
                             kPromiseRejectedExpectedOutput,
                             ChangeType::kOptionChange),
                    TestCase("initTestThrow('shippingaddresschange')",
                             kExeptionThrownExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestThrow('shippingoptionchange')",
                             kExeptionThrownExpectedOutput,
                             ChangeType::kOptionChange)));

// If the merchant responds to a "payment(address|option)change" event with
// updated details, including modifiers for multiple payment method
// names, then the invoked payment handler receives the updated details,
// except the modifiers for non-matching payment method names.
INSTANTIATE_TEST_SUITE_P(
    MerchantResponse,
    PaymentHandlerChangeShippingAddressOptionTest,
    testing::Values(TestCase("initTestDetails('shippingaddresschange')",
                             kSuccessfulMerchantResponseExpectedOutput,
                             ChangeType::kAddressChange),
                    TestCase("initTestDetails('shippingoptionchange')",
                             kSuccessfulMerchantResponseExpectedOutput,
                             ChangeType::kOptionChange)));

}  // namespace
}  // namespace payments
