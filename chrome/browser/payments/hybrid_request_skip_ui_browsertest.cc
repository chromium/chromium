// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

enum SkipToGPayMode {
  ALWAYS_SKIP_TO_GPAY,
  SKIP_TO_GPAY_IF_NO_CARD,
};

enum SkipToGPayTestConfig {
  TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT,
  TEST_INCOMPLETE_AUTOFILL_INSTRUMENT,
  TEST_NO_AUTOFILL_INSTRUMENT,
};

// Parameterized test fixture that tests the two skip-to-GPay modes (i.e.
// always-skip and skip-if-no-card) for all three cases of:
// - user has a complete autofill instrument
// - user has an incomplete autofill instrument
// - user doesn't have any autofill instrument
class HybridRequestSkipUITest
    : public PaymentRequestPlatformBrowserTestBase,
      public testing::WithParamInterface<
          std::tuple<SkipToGPayMode, SkipToGPayTestConfig>> {
 public:
  HybridRequestSkipUITest()
      : gpay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    if (GetTestMode() == ALWAYS_SKIP_TO_GPAY) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kPaymentRequestSkipToGPay);
    } else {
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kPaymentRequestSkipToGPayIfNoCard},
          /*disabled_features=*/{features::kPaymentRequestSkipToGPay});
    }
  }

  ~HybridRequestSkipUITest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();

    gpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/google.com/");
    ASSERT_TRUE(gpay_server_.Start());

    NavigateTo("/hybrid_request_skip_ui_test.html");

    // Set up test manifest downloader that knows how to fake origin.
    const std::string method_name = "google.com";
    SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        {{method_name, &gpay_server_}});

    // Inject autofill instrument based on test config.
    if (GetTestConfig() == TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT) {
      CreateAndAddCreditCardForProfile(CreateAndAddAutofillProfile());
    } else if (GetTestConfig() == TEST_INCOMPLETE_AUTOFILL_INSTRUMENT) {
      AddCreditCard(autofill::test::GetCreditCard());
    }
  }

  // Convenience methods for accessing the test parameterization.
  SkipToGPayMode GetTestMode() const { return std::get<0>(GetParam()); }
  SkipToGPayTestConfig GetTestConfig() const { return std::get<1>(GetParam()); }

  // Runs a single test case and checks that |expected_result| is returned.
  void RunTest(const char* js_snippet, const char* expected_result) {
    if (GetTestMode() == SKIP_TO_GPAY_IF_NO_CARD &&
        GetTestConfig() == TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT) {
      // Skip-to-GPay is not activated in this combination because user has a
      // usable autofill instrument. Just verify that the payment.show() is
      // called and both credit card and Gpay payment methods are available.
      ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
      content::ExecuteScriptAsync(GetActiveWebContents(), js_snippet);
      WaitForObservedEvent();
      EXPECT_EQ(2u, test_controller()->app_descriptions().size());
      return;
    }

    // Skip-to-GPay should have been activated. Verify result.
    EXPECT_EQ(expected_result,
              content::EvalJs(GetActiveWebContents(), js_snippet));
  }

 protected:
  net::EmbeddedTestServer gpay_server_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, NothingRequested) {
  RunTest("buy({apiVersion: 1})",
          "{\"details\":{\"apiVersion\":1},\"shippingAddress\":null,"
          "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":null,"
          "\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, ShippingRequested_V1) {
  RunTest(
      "buy({apiVersion: 1, requestShipping: true})",
      "{\"details\":{\"apiVersion\":1},\"shippingAddress\":{\"country\":\"CA\","
      "\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, ShippingRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestShipping: true})",
      "{\"details\":{\"apiVersion\":2},\"shippingAddress\":{\"country\":\"CA\","
      "\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, EmailRequested_V1) {
  RunTest("buy({apiVersion: 1, requestEmail: true})",
          "{\"details\":{\"apiVersion\":1},\"shippingAddress\":null,"
          "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":"
          "\"paymentrequest@chromium.org\",\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, EmailRequested_V2) {
  RunTest("buy({apiVersion: 2, requestEmail: true})",
          "{\"details\":{\"apiVersion\":2},\"shippingAddress\":null,"
          "\"shippingOption\":null,\"payerName\":null,\"payerEmail\":"
          "\"paymentrequest@chromium.org\",\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, NameRequested_V1) {
  RunTest("buy({apiVersion: 1, requestName: true})",
          "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":"
          "null,\"shippingOption\":null,\"payerName\":\"Browser "
          "Test\",\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, NameRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestName: true})",
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":null,\"shippingOption\":null,"
      "\"payerName\":\"BrowserTest\",\"payerEmail\":null,\"payerPhone\":null}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, PhoneRequested_V1) {
  RunTest("buy({apiVersion: 1, requestPhone: true})",
          "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":"
          "null,\"shippingOption\":null,\"payerName\":null,\"payerEmail\":null,"
          "\"payerPhone\":\"+1 234-567-8900\"}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, PhoneRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestPhone: true})",
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":null,\"shippingOption\":null,"
      "\"payerName\":null,\"payerEmail\":null,\"payerPhone\":\"+1 "
      "234-567-8900\"}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, AllRequested_V1) {
  RunTest(
      "buy({apiVersion: 1, requestShipping: true, requestEmail: "
      "true, requestName: true, requestPhone: true})",
      "{\"details\":{\"apiVersion\":1,\"cardInfo\":{}},\"shippingAddress\":{"
      "\"country\":\"CA\",\"addressLine\":[\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":\"Browser "
      "Test\",\"payerEmail\":\"paymentrequest@chromium.org\",\"payerPhone\":\"+"
      "1 234-567-8900\"}");
}

IN_PROC_BROWSER_TEST_P(HybridRequestSkipUITest, AllRequested_V2) {
  RunTest(
      "buy({apiVersion: 2, requestShipping: true, requestEmail: "
      "true, requestName: true, requestPhone: true})",
      "{\"details\":{\"apiVersion\":2,\"paymentMethodData\":{\"type\":\"CARD\","
      "\"info\":{}}},\"shippingAddress\":{\"country\":\"CA\",\"addressLine\":["
      "\"123 Main Street\",\"Unit "
      "A\"],\"region\":\"ON\",\"city\":\"Toronto\",\"dependentLocality\":\"\","
      "\"postalCode\":\"12345\",\"sortingCode\":\"\",\"organization\":\"\","
      "\"recipient\":\"Browser "
      "Test\",\"phone\":\"\"},\"shippingOption\":\"free-shipping\","
      "\"payerName\":\"BrowserTest\",\"payerEmail\":\"paymentrequest@chromium."
      "org\",\"payerPhone\":\"+1 234-567-8900\"}");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    HybridRequestSkipUITest,
    ::testing::Combine(::testing::Values(ALWAYS_SKIP_TO_GPAY,
                                         SKIP_TO_GPAY_IF_NO_CARD),
                       ::testing::Values(TEST_HAS_COMPLETE_AUTOFILL_INSTRUMENT,
                                         TEST_INCOMPLETE_AUTOFILL_INSTRUMENT,
                                         TEST_NO_AUTOFILL_INSTRUMENT)));

}  // namespace payments
