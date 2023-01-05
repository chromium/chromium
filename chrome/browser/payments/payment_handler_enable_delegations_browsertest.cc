// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace payments {
namespace {

class PaymentHandlerEnableDelegationsTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:
  PaymentHandlerEnableDelegationsTest() = default;

  PaymentHandlerEnableDelegationsTest(
      const PaymentHandlerEnableDelegationsTest&) = delete;
  PaymentHandlerEnableDelegationsTest& operator=(
      const PaymentHandlerEnableDelegationsTest&) = delete;

  ~PaymentHandlerEnableDelegationsTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    NavigateTo("a.com", "/payment_handler.html");
  }
};

IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest,
                       InvalidDelegations) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);

  std::string expected =
      "TypeError: Failed to execute 'enableDelegations' on 'PaymentManager': "
      "The provided value 'invalid_delegation' is not a valid enum value of "
      "type PaymentDelegation.";
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['invalid_delegation'])"));
}

// Since the payment handler can provide shipping and there is only one app, the
// browser skips the payment sheet and goes straight to payment processing.
IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest,
                       ShippingDelegation) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['shippingAddress'])"));
  EXPECT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "paymentRequestWithOptions({requestShipping: true}, $1)",
                    method_name)));
}

// Since the payment handler can provide the contact information and there is
// only one app, the browser should skip the payment sheet and go straight to
// payment processing.
IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest, ContactDelegation) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);
  EXPECT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          "enableDelegations(['payerName', 'payerEmail', 'payerPhone'])"));
  EXPECT_EQ("success",
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace(
                    "paymentRequestWithOptions({requestPayerName: true, "
                    "requestPayerEmail: true, requestPayerPhone: true}, $1)",
                    method_name)));
}

// Since the payment handler can provide the shipping address and contact
// information and there is only one app, the browser should skip the payment
// sheet and go straight to payment processing.
IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest, AllDelegations) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['shippingAddress', "
                            "'payerName', 'payerEmail', 'payerPhone'])"));
  EXPECT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          content::JsReplace("paymentRequestWithOptions({requestShipping: "
                             "true, requestPayerName: true, requestPayerEmail: "
                             "true, requestPayerPhone: true}, $1)",
                             method_name)));
}

// Shipping section must exist in payment sheet since shipping address is
// requested and won't be provided by the selected payment handler.
IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest,
                       PartialDelegation_ShippingNotSupported) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);
  EXPECT_EQ(
      "success",
      content::EvalJs(
          GetActiveWebContents(),
          "enableDelegations(['payerName', 'payerEmail', 'payerPhone'])"));
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true, requestPayerName: "
          "true, requestPayerEmail: true, requestPayerPhone: true}, $1)",
          method_name),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  WaitForObservedEvent();

  ASSERT_TRUE(test_controller()->is_shipping_section_visible().has_value());
  EXPECT_TRUE(test_controller()->is_shipping_section_visible().value());

  ASSERT_TRUE(test_controller()->is_contact_section_visible().has_value());
  EXPECT_FALSE(test_controller()->is_contact_section_visible().value());
}

// Contact section must exist in payment sheet since payer's name and email are
// requested and won't be provided by the selected payment handler.
IN_PROC_BROWSER_TEST_F(PaymentHandlerEnableDelegationsTest,
                       PartialDelegation_ContactInfoNotSupported) {
  std::string method_name;
  InstallPaymentApp("a.com", "/payment_handler_sw.js", &method_name);
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "enableDelegations(['shippingAddress'])"));
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  ASSERT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace(
          "paymentRequestWithOptions({requestShipping: true, requestPayerName: "
          "true, requestPayerEmail: true}, $1)",
          method_name),
      /*options=*/content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  WaitForObservedEvent();

  ASSERT_TRUE(test_controller()->is_shipping_section_visible().has_value());
  EXPECT_FALSE(test_controller()->is_shipping_section_visible().value());

  ASSERT_TRUE(test_controller()->is_contact_section_visible().has_value());
  EXPECT_TRUE(test_controller()->is_contact_section_visible().value());
}

}  // namespace
}  // namespace payments
