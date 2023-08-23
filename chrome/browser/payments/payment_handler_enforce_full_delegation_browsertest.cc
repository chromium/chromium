// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"

namespace payments {
namespace {

enum EnforceFullDelegationFlag {
  DISABLED = 0,
  ENABLED,
};

class PaymentHandlerEnforceFullDelegationTest
    : public PaymentRequestPlatformBrowserTestBase,
      public testing::WithParamInterface<EnforceFullDelegationFlag> {
 public:
  PaymentHandlerEnforceFullDelegationTest() {
    if (GetParam() == ENABLED) {
      scoped_feature_list_.InitAndEnableFeature(
          payments::features::kEnforceFullDelegation);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          payments::features::kEnforceFullDelegation);
    }
  }
  ~PaymentHandlerEnforceFullDelegationTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerEnforceFullDelegationTest,
                       ShowPaymentSheetWhenOnlySomeAppsAreSkipped) {
  std::string expected = "success";

  std::string method_name1;
  NavigateTo("a.com", "/enforce_full_delegation.test/index.html");
  InstallPaymentApp("a.com", "/enforce_full_delegation.test/app.js",
                    &method_name1);
  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(),
                                      "enableDelegations(['payerName'])"));

  std::string method_name2;
  NavigateTo("b.com", "/enforce_full_delegation.test/index.html");
  InstallPaymentApp("b.com", "/enforce_full_delegation.test/app.js",
                    &method_name2);
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(), "enableDelegations([])"));

  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("addSupportedMethods([$1, $2])",
                                               method_name1, method_name2)));
  EXPECT_EQ(expected,
            content::EvalJs(
                GetActiveWebContents(),
                "createPaymentRequestWithOptions({requestPayerName: true})"));

  // When enforcing full delegation: although b.com app is skipped for partial
  // delegation, a.com app is still expected to appear in the payment sheet.
  // When not enforcing: both apps are expected to appear in the sheet. So the
  // sheet appears in both enabled and disabled cases.
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "show()"));
  WaitForObservedEvent();

  if (GetParam() == ENABLED) {
    EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  } else {
    EXPECT_EQ(2u, test_controller()->app_descriptions().size());
  }
}

// crbug.com/1468262: Flaky test on Android.
// crbug.com/1473404: Flaky test on ChromeOS and Windows.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#define MAYBE_WhenEnabled_ShowPaymentSheet_WhenDisabled_Reject \
  DISABLED_WhenEnabled_ShowPaymentSheet_WhenDisabled_Reject
#else
#define MAYBE_WhenEnabled_ShowPaymentSheet_WhenDisabled_Reject \
  WhenEnabled_ShowPaymentSheet_WhenDisabled_Reject
#endif
IN_PROC_BROWSER_TEST_P(PaymentHandlerEnforceFullDelegationTest,
                       MAYBE_WhenEnabled_ShowPaymentSheet_WhenDisabled_Reject) {
  NavigateTo("a.com", "/enforce_full_delegation.test/index.html");

  std::string method_name;
  InstallPaymentApp("a.com", "/enforce_full_delegation.test/app.js",
                    &method_name);

  std::string expected = "success";
  EXPECT_EQ(expected,
            content::EvalJs(
                GetActiveWebContents(),
                content::JsReplace("addSupportedMethods([$1])", method_name)));
  EXPECT_EQ(expected,
            content::EvalJs(GetActiveWebContents(), "enableDelegations([])"));
  EXPECT_EQ(expected,
            content::EvalJs(
                GetActiveWebContents(),
                "createPaymentRequestWithOptions({requestPayerName: true})"));

  if (GetParam() == ENABLED) {
    ResetEventWaiterForSingleEvent(TestEvent::kNotSupportedError);
  } else {
    ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  }

  EXPECT_EQ(expected, content::EvalJs(GetActiveWebContents(), "show()"));
  WaitForObservedEvent();

  if (GetParam() == ENABLED) {
    EXPECT_EQ(0u, test_controller()->app_descriptions().size());
    ExpectBodyContains(
        "Skipping \"Test App Name\" for not providing all of the requested "
        "PaymentOptions.");
  } else {
    EXPECT_EQ(1u, test_controller()->app_descriptions().size());
  }
}

// Run all tests with both values for
// features::kEnforceFullDelegation.
INSTANTIATE_TEST_SUITE_P(All,
                         PaymentHandlerEnforceFullDelegationTest,
                         ::testing::Values(DISABLED, ENABLED));
}  // namespace
}  // namespace payments
