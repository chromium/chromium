// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/journey_logger.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments {

namespace {
using Event2 = payments::JourneyLogger::Event2;
}  // namespace

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
        "components/test/data/payments/kylepay.test/");
    ASSERT_TRUE(kylepay_server_.Start());

    henrypay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/henrypay.test");
    ASSERT_TRUE(henrypay_server_.Start());

    harry_example_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/harry.example.test");
    ASSERT_TRUE(harry_example_server_.Start());

    NavigateTo("/payment_request_bobpay_and_cards_test.html");

    // Set up test manifest downloader that knows how to fake origin.
    const std::string kyle_hostname = "kylepay.test";
    const std::string henry_hostname = "henrypay.test";
    const std::string harry_hostname = "harry.example.test";
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

// kylepay.test hosts an installable payment app which handles both shipping
// address and payer's contact information.
IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       InstallPaymentAppAndPay) {
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.test/webpay'}], "
      "false/*= requestShippingContact */);"));
  WaitForObservedEvent();

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains("kylepay.test/webpay");
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       InstallPaymentAppAndPayWithDelegation) {
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      "testPaymentMethods([{supportedMethods: 'https://kylepay.test/webpay'}], "
      "true/*= requestShippingContact */);"));
  WaitForObservedEvent();

  // kylepay should be installed just-in-time and used for testing.
  ExpectBodyContains("kylepay.test/webpay");
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       DoNotInstallPaymentAppWithInvalidPaymentMethod) {
  ResetEventWaiterForSingleEvent(TestEvent::kNotSupportedError);
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(),
                              "testPaymentMethods([{supportedMethods: "
                              "'https://henrypay.test/webpay'}])"));
  WaitForObservedEvent();

  // show() should get rejected since the JIT installable app
  // that uses invalid payment method.
  ExpectBodyContains(
      "\nBob Pay and Cards Test\nInstallable App\nThe payment method "
      "\"https://henrypay.test/webpay\" is not supported.\n\n\n");
}

using PaymentHandlerSkipSheetTest = PaymentHandlerJustInTimeInstallationTest;

// TODO(crbug.com/40568495): Now that user-activation is required for show(),
// this test doesn't make much sense (we will always have a user gesture).
// However we should make sure that the codepaths it is testing are covered by
// other skip-the-sheet tests before removing it.
IN_PROC_BROWSER_TEST_F(PaymentHandlerSkipSheetTest, SkipWithUserGesture) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 0);
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_TRUE(
      content::ExecJs(GetActiveWebContents(),
                      "testPaymentMethods([ "
                      " {supportedMethods: 'https://kylepay.test/webpay'}])"));
  WaitForObservedEvent();
  ExpectBodyContains("kylepay.test/webpay");

  histogram_tester.ExpectTotalCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", true, 1);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.PaymentHandlerInstallSuccess", false, 0);

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & static_cast<int>(Event2::kSkippedShow));
  EXPECT_FALSE(buckets[0].min & static_cast<int>(Event2::kShown));
  EXPECT_TRUE(buckets[0].min & static_cast<int>(Event2::kSelectedOther));
}

IN_PROC_BROWSER_TEST_F(PaymentHandlerJustInTimeInstallationTest,
                       MultiplePaymentMethods) {
  base::HistogramTester histogram_tester;
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  content::ExecuteScriptAsync(GetActiveWebContents(), R"(
    testPaymentMethods([
      {supportedMethods: 'https://harry.example.test/webpay'},
      {supportedMethods: 'https://kylepay.test/webpay'}
    ]);
  )");
  WaitForObservedEvent();

  // The request is expected to stop at the payment sheet waiting for user
  // action.
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));
}

}  // namespace payments
