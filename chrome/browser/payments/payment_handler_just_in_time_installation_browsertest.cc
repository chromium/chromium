// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/features.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

class PaymentHandlerJustInTimeInstallationTest
    : public PaymentRequestPlatformBrowserTestBase {
 protected:
  PaymentHandlerJustInTimeInstallationTest()
      : kylepay_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        henrypay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  ~PaymentHandlerJustInTimeInstallationTest() override = default;

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    kylepay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/kylepay.com/");
    ASSERT_TRUE(kylepay_server_.Start());

    henrypay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/henrypay.com");
    ASSERT_TRUE(henrypay_server_.Start());

    NavigateTo("/payment_request_bobpay_and_cards_test.html");

    // Set up test manifest downloader that knows how to fake origin.
    const std::string kyle_method_name = "kylepay.com";
    const std::string henry_method_name = "henrypay.com";
    SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        {{kyle_method_name, &kylepay_server_},
         {henry_method_name, &henrypay_server_}});
  }

 private:
  net::EmbeddedTestServer kylepay_server_;
  net::EmbeddedTestServer henrypay_server_;
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
      "\"https://henrypay.com/webpay\" is not supported.\n\n\n\n\n");
}

using PaymentHandlerSkipSheetTest = PaymentHandlerJustInTimeInstallationTest;

IN_PROC_BROWSER_TEST_F(PaymentHandlerSkipSheetTest, SkipWithUserGesture) {
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "testPaymentMethods([ "
                      " {supportedMethods: 'https://kylepay.com/webpay'}])"));
  WaitForObservedEvent();
  ExpectBodyContains("kylepay.com/webpay");

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
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "testPaymentMethods([ "
                      " {supportedMethods: 'https://kylepay.com/webpay'}])",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE |
                          content::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  WaitForObservedEvent();
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());

  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER);
}

class SecurePaymentConfirmationSkipSheetTest
    : public PaymentHandlerJustInTimeInstallationTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    PaymentHandlerJustInTimeInstallationTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }
};

// TODO(crbug.com/825270): Remove this special case user gesture exception is
// removed.
IN_PROC_BROWSER_TEST_F(SecurePaymentConfirmationSkipSheetTest,
                       SkipWithoutUserGesture) {
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "testPaymentMethods([ "
                      " {supportedMethods: 'https://kylepay.com/webpay'}])",
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  WaitForObservedEvent();
  ExpectBodyContains("kylepay.com/webpay");

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SKIPPED_SHOW);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_SHOWN);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_SELECTED_OTHER);
}

class AlwaysAllowJustInTimePaymentAppTest
    : public PaymentHandlerJustInTimeInstallationTest,
      public testing::WithParamInterface<bool> {
 protected:
  AlwaysAllowJustInTimePaymentAppTest() {
    scoped_feature_list_.InitWithFeatureState(
        features::kAlwaysAllowJustInTimePaymentApp, GetParam());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(AlwaysAllowJustInTimePaymentAppTest,
                       HybridRequest_NoCreditCard) {
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSingleEvent(GetParam() ? TestEvent::kPaymentCompleted
                                            : TestEvent::kAppListReady);
  content::ExecuteScriptAsync(GetActiveWebContents(), R"(
    testPaymentMethods([
      {supportedMethods: 'basic-card'},
      {supportedMethods: 'https://kylepay.com/webpay'}
    ]);
  )");
  WaitForObservedEvent();

  if (GetParam()) {
    // If AlwaysAllowJIT is enabled, kylepay should be installed just-in-time
    // and used for testing.
    ExpectBodyContains("kylepay.com/webpay");
  } else {
    // With AlwaysJIT disabled, the request is expected to stop at the payment
    // sheet waiting for user action.
    EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));
  }

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_EQ(GetParam(),
            (buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER) > 0);
}

IN_PROC_BROWSER_TEST_P(AlwaysAllowJustInTimePaymentAppTest,
                       HybridRequest_HasCompleteCreditCard) {
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

  // Regardless whether AlwaysJIT is disabled, beceause there is a complete
  // basic card, the request is expected to stop at the payment sheet waiting
  // for user action.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_EQ(GetParam(),
            (buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER) > 0);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AlwaysAllowJustInTimePaymentAppTest,
                         ::testing::Values(true, false));

}  // namespace payments
