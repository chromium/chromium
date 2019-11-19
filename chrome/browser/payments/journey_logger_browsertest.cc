// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/payments/personal_data_manager_test_util.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/payments/payment_request_test_controller.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_event_waiter.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/payments/content/service_worker_payment_app_factory.h"
#include "components/payments/core/journey_logger.h"
#include "components/payments/core/test_payment_manifest_downloader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/chrome_feature_list.h"
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif

namespace payments {
namespace {

autofill::CreditCard GetCardWithBillingAddress(
    const autofill::AutofillProfile& profile) {
  autofill::CreditCard card = autofill::test::GetCreditCard();
  card.set_billing_address_id(profile.guid());
  return card;
}

}  // namespace

class JourneyLoggerTest : public PlatformBrowserTest,
                          public PaymentRequestTestObserver {
 public:
  // PaymentRequestTestObserver events that can be waited on by the EventWaiter.
  enum TestEvent : int {
    SHOW_APPS_READY,
  };

  JourneyLoggerTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        gpay_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    test_controller_.SetObserver(this);
  }

  ~JourneyLoggerTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed to
    // load pages from the fake "google.com" without an interstitial.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    // Map all out-going DNS lookups to the local server. This must be used in
    // conjunction with switches::kIgnoreCertificateErrors to work.
    host_resolver()->AddRule("*", "127.0.0.1");

    gpay_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments/google.com/");
    ASSERT_TRUE(gpay_server_.Start());

    // Set up test manifest downloader that knows how to fake origin.
    content::BrowserContext* context =
        GetActiveWebContents()->GetBrowserContext();
    auto downloader = std::make_unique<TestDownloader>(
        content::BrowserContext::GetDefaultStoragePartition(context)
            ->GetURLLoaderFactoryForBrowserProcess());
    downloader->AddTestServerURL("https://google.com/",
                                 gpay_server_.GetURL("google.com", "/"));
    ServiceWorkerPaymentAppFactory::GetInstance()
        ->SetDownloaderAndIgnorePortInOriginComparisonForTesting(
            std::move(downloader));

    https_server_.ServeFilesFromSourceDirectory(
        "components/test/data/payments");
    ASSERT_TRUE(https_server_.Start());

    ASSERT_TRUE(content::NavigateToURL(
        GetActiveWebContents(),
        https_server_.GetURL("/journey_logger_test.html")));

    test_controller_.SetUpOnMainThread();
    PlatformBrowserTest::SetUpOnMainThread();
  }

  content::WebContents* GetActiveWebContents() {
    return chrome_test_utils::GetActiveWebContents(this);
  }

  // PaymentRequestTestObserver implementation.
  void OnShowAppsReady() override {
    if (event_waiter_)
      event_waiter_->OnEvent(TestEvent::SHOW_APPS_READY);
  }

  void ResetEventWaiterForSequence(std::list<TestEvent> event_sequence) {
    event_waiter_ = std::make_unique<autofill::EventWaiter<TestEvent>>(
        std::move(event_sequence));
  }

  void WaitForObservedEvent() { event_waiter_->Wait(); }

 protected:
  net::EmbeddedTestServer https_server_;

 private:
  PaymentRequestTestController test_controller_;
  net::EmbeddedTestServer gpay_server_;
  std::unique_ptr<autofill::EventWaiter<TestEvent>> event_waiter_;

  DISALLOW_COPY_AND_ASSIGN(JourneyLoggerTest);
};

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, NoPaymentMethodSupported) {
  base::HistogramTester histogram_tester;

  ResetEventWaiterForSequence({TestEvent::SHOW_APPS_READY});
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "testBasicCard()"));
  WaitForObservedEvent();

  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
}

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, BasicCardOnly) {
  autofill::AutofillProfile address = autofill::test::GetFullProfile();
  test::AddAutofillProfile(GetActiveWebContents()->GetBrowserContext(),
                           address);
  test::AddCreditCard(GetActiveWebContents()->GetBrowserContext(),
                      GetCardWithBillingAddress(address));

  base::HistogramTester histogram_tester;

  ResetEventWaiterForSequence({TestEvent::SHOW_APPS_READY});
  EXPECT_TRUE(content::ExecJs(GetActiveWebContents(), "testBasicCard()"));
  WaitForObservedEvent();

  EXPECT_EQ(true, content::EvalJs(GetActiveWebContents(), "abort()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_TRUE(buckets[0].min &
              JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
}

IN_PROC_BROWSER_TEST_F(JourneyLoggerTest, GooglePaymentApp) {
  base::HistogramTester histogram_tester;

  EXPECT_EQ("{\"apiVersion\":1}",
            content::EvalJs(GetActiveWebContents(), "testGPay()"));

  std::vector<base::Bucket> buckets =
      histogram_tester.GetAllSamples("PaymentRequest.Events");
  ASSERT_EQ(1U, buckets.size());
  EXPECT_FALSE(buckets[0].min &
               JourneyLogger::EVENT_AVAILABLE_METHOD_BASIC_CARD);
  EXPECT_TRUE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_GOOGLE);
  EXPECT_FALSE(buckets[0].min & JourneyLogger::EVENT_AVAILABLE_METHOD_OTHER);
}

}  // namespace payments
