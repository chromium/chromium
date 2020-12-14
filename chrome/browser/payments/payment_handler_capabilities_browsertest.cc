// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "chrome/test/payments/test_event_waiter.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "components/payments/content/android/payment_feature_list.h"
#endif  // OS_ANDROID

namespace payments {
namespace {

constexpr char kOriginalPrice[] = "5.00";
constexpr char kDiscountPrice[] = "4.00";

class PaymentHandlerCapabilitiesTest
    : public PaymentRequestPlatformBrowserTestBase {
 public:

  void ExpectAppTotals() {
    EXPECT_EQ(expected_app_totals_.size(),
              test_controller()->app_descriptions().size());
    for (const auto& app : test_controller()->app_descriptions()) {
      auto iter = expected_app_totals_.find(app.sublabel);
      ASSERT_NE(expected_app_totals_.end(), iter)
          << "Origin \"" << app.sublabel << "\" was not expected.";
      EXPECT_EQ(iter->second, app.total)
          << app.sublabel << " should have a total of \"" << iter->second
          << "\", but \"" << app.total << "\" was found instead.";
    }
  }

  void ExpectModifiedPriceForOrigin(const std::string& origin,
                                    const std::string& value) {
    // Android payment apps pre-format the modified value.
#if defined(OS_ANDROID)
    expected_app_totals_[origin] = "$" + value;
#else
    expected_app_totals_[origin] = "USD " + value;
#endif  // OS_ANDROID
  }

  void ExpectPriceNotModifiedForOrigin(const std::string& origin,
                                       const std::string& value) {
    expected_app_totals_[origin] = "USD " + value;
  }

  std::map<std::string, std::string> expected_app_totals_;
  base::test::ScopedFeatureList features_;
};

// Modified price should be displayed for the payment handler with the matching
// capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest, TwoApps) {
  NavigateTo("alicepay.com", "/payment_handler_installer.html");
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "installWithCapabilities('alicepay.com/app1/app.js', "
                      "'basic-card', {supportedNetworks: ['visa']})"));
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "installWithCapabilities('bobpay.com/app1/app.js', "
                      "'basic-card', {supportedNetworks: ['mastercard']})"));

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifiers_test.html");
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(), "visaSupportedNetwork()"));
  WaitForObservedEvent();

  ExpectModifiedPriceForOrigin("alicepay.com", kDiscountPrice);
  ExpectPriceNotModifiedForOrigin("bobpay.com", kOriginalPrice);

  ExpectAppTotals();
}

// A "basic-card" modifier without any networks will apply to a payment handler
// that does not declare its capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest,
                       AllCardsModifierMatchesAppWithoutCapabilities) {
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "installWithCapabilities('bobpay.com/app1/app.js', "
                            "'basic-card', {})"));

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifier_optional_"
             "data_test.html");
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(), "buyWithAllCardsModifier()"));
  WaitForObservedEvent();

  ExpectModifiedPriceForOrigin("bobpay.com", kDiscountPrice);
  ExpectAppTotals();
}

// A "basic-card" modifier without any networks will apply to a payment handler
// with visa capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest,
                       AllCardsModifierMatchesVisaCapabilities) {
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "installWithCapabilities('bobpay.com/app1/app.js', "
                            "'basic-card', {supportedNetworks: ['visa']})"));

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifier_optional_"
             "data_test.html");
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(), "buyWithAllCardsModifier()"));
  WaitForObservedEvent();

  ExpectModifiedPriceForOrigin("bobpay.com", kDiscountPrice);
  ExpectAppTotals();
}

// A "basic-card" modifier with visa network will not apply to a payment handler
// that does not declare its capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest,
                       VisaCardsModifierDoesNotMatchAppWithoutCapabilities) {
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "installWithCapabilities('bobpay.com/app1/app.js', "
                            "'basic-card', {})"));

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifier_optional_"
             "data_test.html");
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "buyWithVisaModifier()"));
  WaitForObservedEvent();

  ExpectPriceNotModifiedForOrigin("bobpay.com", kOriginalPrice);
  ExpectAppTotals();
}

// A "basic-card" modifier with visa network will apply to a payment handler
// with visa capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest,
                       VisaCardsModifierMatchesVisaCapabilities) {
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            "installWithCapabilities('bobpay.com/app1/app.js', "
                            "'basic-card', {supportedNetworks: ['visa']})"));

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifier_optional_"
             "data_test.html");
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "buyWithVisaModifier()"));
  WaitForObservedEvent();

  ExpectModifiedPriceForOrigin("bobpay.com", kDiscountPrice);
  ExpectAppTotals();
}

// A "basic-card" modifier without any networks will apply to a payment handler
// with mastercard capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest,
                       AllCardsModifierMatchesMastercardCapabilities) {
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "installWithCapabilities('bobpay.com/app1/app.js', "
                      "'basic-card', {supportedNetworks: ['mastercard']})"));

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifier_optional_"
             "data_test.html");
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(), "buyWithAllCardsModifier()"));
  WaitForObservedEvent();

  ExpectModifiedPriceForOrigin("bobpay.com", kDiscountPrice);
  ExpectAppTotals();
}

// A "basic-card" modifier with visa network will not apply to a payment handler
// with mastercard capabilities.
IN_PROC_BROWSER_TEST_F(PaymentHandlerCapabilitiesTest,
                       VisaCardsModifierDoesNotMatchMastercardCapabilities) {
  NavigateTo("bobpay.com", "/payment_handler_installer.html");
  EXPECT_EQ(
      "success",
      content::EvalJs(GetActiveWebContents(),
                      "installWithCapabilities('bobpay.com/app1/app.js', "
                      "'basic-card', {supportedNetworks: ['mastercard']})"));

  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  NavigateTo("test.com",
             "/payment_request_bobpay_and_basic_card_with_modifier_optional_"
             "data_test.html");
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "buyWithVisaModifier()"));
  WaitForObservedEvent();

  ExpectPriceNotModifiedForOrigin("bobpay.com", kOriginalPrice);
  ExpectAppTotals();
}

}  // namespace
}  // namespace payments
