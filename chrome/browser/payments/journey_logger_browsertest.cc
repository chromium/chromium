// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/payments/payment_request_platform_browsertest_base.h"
#include "components/payments/core/journey_logger.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace payments {

namespace {
using Event2 = payments::JourneyLogger::Event2;
}  // namespace

class JourneyLoggerTest : public PaymentRequestPlatformBrowserTestBase {
 public:
  JourneyLoggerTest() : gpay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  JourneyLoggerTest(const JourneyLoggerTest&) = delete;
  JourneyLoggerTest& operator=(const JourneyLoggerTest&) = delete;

  ~JourneyLoggerTest() override = default;

  void PreRunTestOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::PreRunTestOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpOnMainThread() override {
    PaymentRequestPlatformBrowserTestBase::SetUpOnMainThread();
    main_frame_url_ = https_server()->GetURL("/journey_logger_test.html");
    ASSERT_TRUE(
        content::NavigateToURL(GetActiveWebContents(), main_frame_url_));
  }

  void SetUpForGpay() {
    gpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/google.com/");
    ASSERT_TRUE(gpay_server_.Start());

    // Set up test manifest downloader that knows how to fake origin.
    const std::string method_name = "google.com";
    SetDownloaderAndIgnorePortInOriginComparisonForTesting(
        {{method_name, &gpay_server_}});

    gpay_scope_url_ = gpay_server_.GetURL("google.com", "/");
  }

  const GURL& main_frame_url() const { return main_frame_url_; }
  const GURL& gpay_scope_url() const { return gpay_scope_url_; }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 private:
  net::EmbeddedTestServer gpay_server_;
  GURL main_frame_url_;
  GURL gpay_scope_url_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
};

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, NoPaymentMethodSupported) {
  base::HistogramTester histogram_tester;
  NavigateTo("/payment_handler.html");

  // Launch the payment request without installing the payment app.
  content::EvalJsResult eval_js_result =
      content::EvalJs(GetActiveWebContents(), "launch()");
  ASSERT_TRUE(eval_js_result.error.empty());
  EXPECT_THAT(eval_js_result.ExtractString(),
              testing::StartsWith("NotSupportedError"));

  // Verify recorded checkout steps.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kInitiated, 1U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kShowCalled, 1U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kPaymentRequestTriggered, 0U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kPaymentHandlerInvoked, 0U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kCompleted, 0U);
}

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest,
                       NoPaymentMethodSupportedWithShipping) {
  base::HistogramTester histogram_tester;
  NavigateTo("/payment_request_metrics_test.html");

  // Launch the payment request without installing the payment app.
  content::EvalJsResult eval_js_result =
      content::EvalJs(GetActiveWebContents(), "noSupportedPromise()");
  ASSERT_TRUE(eval_js_result.error.empty());
  EXPECT_THAT(eval_js_result.ExtractString(),
              testing::StartsWith("NotSupportedError"));

  // Make sure the events were logged correctly.
  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events2");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min & static_cast<int>(Event2::kRequestShipping));
  EXPECT_TRUE(buckets[0].min & static_cast<int>(Event2::kRequestMethodOther));
  EXPECT_TRUE(buckets[0].min & static_cast<int>(Event2::kCouldNotShow));
}

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, GooglePaymentApp) {
  base::HistogramTester histogram_tester;
  SetUpForGpay();

  EXPECT_EQ("{\"apiVersion\":1}",
            content::EvalJs(GetActiveWebContents(), "testGPay()"));

  // Verify recorded checkout steps.
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kInitiated, 1U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kShowCalled, 1U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kPaymentRequestTriggered, 1U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kPaymentHandlerInvoked, 1U);
  histogram_tester.ExpectBucketCount(
      "PaymentRequest.CheckoutFunnel",
      JourneyLogger::CheckoutFunnelStep::kCompleted, 1U);
}

// Make sure the UKM was logged correctly.
IN_PROC_BROWSER_TEST_F(JourneyLoggerTest,
                       UKMCheckoutEventsRecordedForAppOrigin) {
  std::string payment_method;
  InstallPaymentApp("payment-app.com", "/payment_handler_sw.js",
                    &payment_method);

  GURL merchant_url = https_server()->GetURL("/payment_handler.html");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), merchant_url));
  ResetEventWaiterForSingleEvent(TestEvent::kPaymentCompleted);
  EXPECT_EQ("success",
            content::EvalJs(GetActiveWebContents(),
                            content::JsReplace("launch($1)", payment_method)));
  WaitForObservedEvent();

  // UKM for merchant's website origin.
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentRequest_CheckoutEvents::kEntryName);
  size_t num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0], merchant_url);

  // UKM for payment app's scope.
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentApp_CheckoutEvents::kEntryName);
  num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(
      entries[0], https_server()->GetURL("payment-app.com", "/"));
}

IN_PROC_BROWSER_TEST_F(
    JourneyLoggerTest,
    UKMCheckoutEventsNotRecordedForAppOriginWhenNoWindowShown) {
  SetUpForGpay();

  EXPECT_EQ("{\"apiVersion\":1}",
            content::EvalJs(GetActiveWebContents(), "testGPay()"));

  // UKM for merchant's website origin.
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentRequest_CheckoutEvents::kEntryName);
  size_t num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0], main_frame_url());

  // No UKM for payment app's scope since the app's origin is not shown inside
  // the PH modal window.
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentApp_CheckoutEvents::kEntryName);
  num_entries = entries.size();
  EXPECT_EQ(0u, num_entries);
}

IN_PROC_BROWSER_TEST_F(
    JourneyLoggerTest,
    UKMCheckoutEventsNotRecordedForAppOriginWhenNoAppInvoked) {
  std::string a_payment_method;
  InstallPaymentApp("a.com", "/nickpay.test/app.js", &a_payment_method);
  std::string b_payment_method;
  InstallPaymentApp("b.com", "/nickpay.test/app.js", &b_payment_method);

  NavigateTo("/journey_logger_test.html");
  ResetEventWaiterForSingleEvent(TestEvent::kAppListReady);
  EXPECT_TRUE(content::ExecJs(
      GetActiveWebContents(),
      content::JsReplace("testPaymentMethods([{supportedMethods:$1},"
                         "{supportedMethods:$2}])",
                         a_payment_method, b_payment_method)));
  WaitForObservedEvent();

  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "abort()"));

  // UKM for merchant's website origin.
  auto entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentRequest_CheckoutEvents::kEntryName);
  size_t num_entries = entries.size();
  EXPECT_EQ(1u, num_entries);
  test_ukm_recorder()->ExpectEntrySourceHasUrl(entries[0], main_frame_url());

  // No UKM for payment app's scope since the request got aborted before
  // invoking a payment app.
  entries = test_ukm_recorder()->GetEntriesByName(
      ukm::builders::PaymentApp_CheckoutEvents::kEntryName);
  num_entries = entries.size();
  EXPECT_EQ(0u, num_entries);
}

}  // namespace payments
