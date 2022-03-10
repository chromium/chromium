// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentHandlerJustInTimeInstallationTest
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentHandlerJustInTimeInstallationTest()
      : kylepay_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        henrypay_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        harry_example_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~PaymentHandlerJustInTimeInstallationTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    kylepay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/kylepay.com/");
    ASSERT_TRUE(kylepay_server_.Start());

    henrypay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/henrypay.com");
    ASSERT_TRUE(henrypay_server_.Start());

    harry_example_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/harry.example.com");
    ASSERT_TRUE(harry_example_server_.Start());

    NavigateTo("/payment_request_bobpay_and_cards_test.html");

    // Set up test manifest downloader that knows how to fake origin.
    const std::string kyle_hostname = "kylepay.com";
    const std::string henry_hostname = "henrypay.com";
    const std::string harry_hostname = "harry.example.com";
    SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        {{kyle_hostname, &kylepay_server_},
         {henry_hostname, &henrypay_server_},
         {harry_hostname, &harry_example_server_}});
  }

 private:
  net::EmbeddedTestServer kylepay_server_;
  net::EmbeddedTestServer henrypay_server_;
  net::EmbeddedTestServer harry_example_server_;
};

// kylepay.com hosts an installable payment app which handles both shipping
// address and payer's contact information.
IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       InstallPaymentAppAndPay) {
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.com/webpay'}], "
      "false/*= requestShippingContact */);"));
  WaitForObservedEvent();

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains("kylepay.com/webpay");
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       InstallPaymentAppAndPayWithDelegation) {
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.com/webpay'}], "
      "true/*= requestShippingContact */);"));
  WaitForObservedEvent();

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains("kylepay.com/webpay");
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       DoNotInstallPaymentAppWithInvalidPaymentMethod) {
  ResetEventWaiterForSingleEvent(TestEvent::kNotSupportedError);
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "testPaymentMethods([{supportedMethods: "
                              "'https://henrypay.com/webpay'}])"));
  WaitForObservedEvent();

  // show() should get rejected since the JIT installable app
  // that uses invalid payment method.
  ExpectBodyContains(
      "\nBob Pay and Cards Test\nInstallable App\nThe payment method "
      "\"https://henrypay.com/webpay\" is not supported.\n\n\n");
}

using PaymentHandlerSkipSheetTest = PaymentHandlerJustInTimeInstallationTest;

IN_PROC_BROWSER_TEST_F(PaymentHandlerSkipSheetTest, SkipWithUserGesture) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 0);
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "testPaymentMethods([ "
                      " {supportedMethods: 'https://kylepay.com/webpay'}])"));
  WaitForObservedEvent();
  ExpectBodyContains("kylepay.com/webpay");

  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", true, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", false, 0);

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER);
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerSkipSheetTest, NoSkipWithoutUserGesture) {
  // When shipping is not requested, kylepay.com cannot leverage skip-the-sheet
  // for being the only payment app that can fulfill all delegation requests. So
  // if there is no user gesture, the request should stop at the payment sheet
  // waiting for user action.
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 0);
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "testPaymentMethods([ "
                      " {supportedMethods: 'https://kylepay.com/webpay'}])",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE |
                          content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  WaitForObservedEvent();
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));

  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 0);

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());

  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER);
}

class PaymentHandlerJustInTimeInstallationTestWithParam
    : public PaymentHandlerJustInTimeInstallationTest,
      public ::testing::WithParamInterface<bool> {
 protected:
  PaymentHandlerJustInTimeInstallationTestWithParam() {
    scoped_feature_list_.InitWithFeatureState(
        ::features::kPaymentRequestBasicCard, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(PaymentHandlerJustInTimeInstallationTestWithParam,
                       MultiplePaymentMethods) {
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  content::ExecuteScriptAsync(GetActiveWebContents(), R"(
    testPaymentMethods([
      {supportedMethods: 'basic-card'},
      {supportedMethods: 'https://harry.example.com/webpay'},
      {supportedMethods: 'https://kylepay.com/webpay'}
    ]);
  )");
  WaitForObservedEvent();

  // The request is expected to stop at the payment sheet waiting for user
  // action.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());

  // No cards were added to Autofill, so no basic-card payment instruments were
  // available for the user to select.
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);

  // The merchant did not request https://google.com/pay or
  // https://android.com/pay payment methods and no payment apps were added to
  // Chrome with these payment method identifiers, so no Google payment
  // instruments were available for the user to select.
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);

  // - When the merchant requests basic-card and that feature is enabled
  //   (GetParam() == true), then Chrome skips looking up JIT installable
  //   payment handlers, so these "other" payment instruments are not available
  //   for the user to select.
  // - When the basic-card feature is disabled (GetParam() == false), then
  //   Chrome looks up the JIT installable payment handlers, so these "other"
  //   payment instruments are available for the user to select.
  EXPECT_EQ(!GetParam(),
            (buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER) > 0);
}

IN_PROC_BROWSER_TEST_P(PaymentHandlerJustInTimeInstallationTestWithParam,
                       HybridRequest_HasCompleteCreditCard) {
  if (!GetParam())
    return;

  CreateAndAddCreditCardForProfile(CreateAndAddAutofillProfile());

  base::HistogramTester histogram_tester;
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);

  content::ExecuteScriptAsync(GetActiveWebContents(), R"(
    testPaymentMethods([
      {supportedMethods: 'basic-card'},
      {supportedMethods: 'https://kylepay.com/webpay'}
    ]);
  )");
  WaitForObservedEvent();

  // Because there is a complete basic card, the request is expected to stop at
  // the payment sheet waiting for user action.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());

  // A card was added to Autofill, so a basic-card payment instrument is
  // available for the user to select.
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);

  // The merchant did not request https://google.com/pay or
  // https://android.com/pay payment methods and no payment apps were added to
  // Chrome with these payment method identifiers, so no Google payment
  // instruments were available for the user to select.
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);

  // Chrome skips looking up JIT installable payment handlers when merchant
  // requests basic-card payment method, so the "other" payment handlers are not
  // available for the user to select.
  EXPECT_FALSE((buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER) >
               0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PaymentHandlerJustInTimeInstallationTestWithParam,
                         ::testing::Values(false, true));

}  // namespace payments
