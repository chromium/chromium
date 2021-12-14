// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

#include "android_webview/common/aw_features.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

static const base::TickClock* g_clock;

class TestClient : public VisibilityMetricsLogger::Client {
 public:
  explicit TestClient(VisibilityMetricsLogger* logger) : logger_(logger) {
    logger_->AddClient(this);
  }

  virtual ~TestClient() { logger_->RemoveClient(this); }

  void SetViewAttached(bool view_attached) {
    visibility_info_.view_attached = view_attached;
    logger_->ClientVisibilityChanged(this);
  }

  void SetViewVisible(bool view_visible) {
    visibility_info_.view_visible = view_visible;
    logger_->ClientVisibilityChanged(this);
  }

  void SetWindowVisible(bool window_visible) {
    visibility_info_.window_visible = window_visible;
    logger_->ClientVisibilityChanged(this);
  }

  void SetScheme(VisibilityMetricsLogger::Scheme scheme) {
    visibility_info_.scheme = scheme;
    logger_->ClientVisibilityChanged(this);
  }

  // VisibilityMetricsLogger::Client implementation
  VisibilityMetricsLogger::VisibilityInfo GetVisibilityInfo() override {
    return visibility_info_;
  }

 private:
  raw_ptr<VisibilityMetricsLogger> logger_;
  VisibilityMetricsLogger::VisibilityInfo visibility_info_;
};

class VisibilityMetricsLoggerTest : public testing::Test {
 public:
  VisibilityMetricsLoggerTest()
      : task_environment_(
            content::BrowserTaskEnvironment::TimeSource::MOCK_TIME) {
    g_clock = task_environment_.GetMockTickClock();
  }

  ~VisibilityMetricsLoggerTest() override { g_clock = nullptr; }

  VisibilityMetricsLogger* logger() { return logger_.get(); }

  content::BrowserTaskEnvironment& task_environment() {
    return task_environment_;
  }

 protected:
  // testing::Test.
  void SetUp() override {
    logger_ = std::make_unique<VisibilityMetricsLogger>();
  }

  void TearDown() override {}

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<VisibilityMetricsLogger> logger_;
};

TEST_F(VisibilityMetricsLoggerTest, TestFractionalSecondAccumulation) {
  base::HistogramTester histogram_tester;

  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Milliseconds(500));

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 0);

  task_environment().FastForwardBy(base::Milliseconds(500));

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 1);

  client.reset();
}

TEST_F(VisibilityMetricsLoggerTest, TestSingleVisibleClient) {
  base::HistogramTester histogram_tester;

  task_environment().FastForwardBy(base::Seconds(10));
  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(30));
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);
  client->SetScheme(VisibilityMetricsLogger::Scheme::kHttp);

  task_environment().FastForwardBy(base::Seconds(10));
  client->SetWindowVisible(false);

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 10);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 40);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                                     VisibilityMetricsLogger::Scheme::kHttp,
                                     10);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                                     VisibilityMetricsLogger::Scheme::kData, 0);

  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);
  client->SetScheme(VisibilityMetricsLogger::Scheme::kData);
  task_environment().FastForwardBy(base::Seconds(90));

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 100);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 40);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                                     VisibilityMetricsLogger::Scheme::kHttp,
                                     10);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                                     VisibilityMetricsLogger::Scheme::kData,
                                     90);

  client.reset();
}

TEST_F(VisibilityMetricsLoggerTest, TestLongDurationVisibleClient) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(300));
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Seconds(50));
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Seconds(50));
  client2.reset();

  task_environment().FastForwardBy(base::Seconds(50));
  client1.reset();

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 150);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 300);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kVisible, 200);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kNotVisible, 650);
}

TEST_F(VisibilityMetricsLoggerTest, TestTwoVisibleClients) {
  // t=0: client1 created
  // t=10: client2 created
  // t=40: client1 visible, recording scheduled for t+60s
  // t=50: client2 visible
  // t=60: client1 invisible
  // t=70: client2 invisible
  // t=100: clients deleted.

  // Time with any client visible: 70 - 40 = 30
  // Time with no visible client: 100 - 30 = 70
  // Time x visible clients: (50-40) * 1 + (60-50) * 2 + (70-60) * 1 = 40
  // Time x hidden clients: 100 + 90 - 40 = 150
  base::HistogramTester histogram_tester;
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(10));
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(30));
  // This queues delayed recording after 60 seconds (test-defined)
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Seconds(10));
  // No additional task is queued
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Seconds(10));
  // This does not cause metrics to be recorded because one client remains
  // visible.
  client1->SetWindowVisible(false);

  task_environment().FastForwardBy(base::Seconds(10));
  // The last client becoming invisible triggers immediate recording and the
  // cancellation of the queued task.
  client2->SetWindowVisible(false);

  task_environment().FastForwardBy(base::Seconds(30));
  client1.reset();
  client2.reset();

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 30);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 70);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kVisible, 40);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kNotVisible, 150);
}

TEST_F(VisibilityMetricsLoggerTest, TestTwoVisibleWebContentClients) {
  // t=0: client1 created
  // t=10: client2 created
  // t=40: client1 visible with empty scheme
  // t=50: client1 navigates to http scheme
  // t=60: client2 visible and navigates to http scheme
  // t=70: client2 invisible
  // t=80: client1 invisible
  // t=100: clients deleted

  // Any client visible: 40
  // No client visible: 60
  // Per client visible: 40 (client1) + 10 (client2) = 50
  // Per client existing but invisible: 100 (client1) + 90 (client2) - 50 = 140

  // Any client visible with empty scheme: 10
  // Any client visible with http scheme: 30
  // Per client visible with empty scheme: 10
  // Per client visible with http scheme: 30 (client1) + 10 (client2) = 40

  base::HistogramTester histogram_tester;
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(10));
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(30));
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Seconds(10));
  client1->SetScheme(VisibilityMetricsLogger::Scheme::kHttp);

  task_environment().FastForwardBy(base::Seconds(10));
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);
  client2->SetScheme(VisibilityMetricsLogger::Scheme::kHttp);

  task_environment().FastForwardBy(base::Seconds(10));
  client1->SetWindowVisible(false);

  task_environment().FastForwardBy(base::Seconds(10));
  client2->SetWindowVisible(false);

  task_environment().FastForwardBy(base::Seconds(20));
  client1.reset();
  client2.reset();

  logger()->RecordMetrics();
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 40);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 60);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kVisible, 50);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.PerWebView",
      VisibilityMetricsLogger::Visibility::kNotVisible, 140);

  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                                     VisibilityMetricsLogger::Scheme::kEmpty,
                                     10);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                                     VisibilityMetricsLogger::Scheme::kHttp,
                                     30);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.PerWebView",
                                     VisibilityMetricsLogger::Scheme::kEmpty,
                                     10);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.PerWebView",
                                     VisibilityMetricsLogger::Scheme::kHttp,
                                     40);
}

TEST_F(VisibilityMetricsLoggerTest, TestScreenPortion) {
  // t=0: client created
  // t=10: client visible, navigates to web content, screen percentage 0%
  // t=30: 7% screen percentage
  // t=35: 42% screen percentage
  // t=45: 100% screen percentage
  // t=60: client invisible
  // t=70: client visible
  // t=95: client navigates away from web content
  // t=100: client deleted

  // Time with no visible client: 10 + 10 = 20
  // Time with client visible: 20 + 5 + 10 + 15 + 25 + 5 = 80
  // Time with client displaying web content: 20 + 5 + 10 + 15 + 25 = 75
  // Time displaying web content with portion kExactlyZeroPercent: 20
  // Time displaying web content with portion kZeroPercent: 5
  // Time displaying web content with portion kFortyPercent: 10
  // Time displaying web content with portion kOneHundredPercent: 15 + 25 = 40

  base::HistogramTester histogram_tester;
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      android_webview::features::kWebViewMeasureScreenCoverage);
  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(10));
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);
  client->SetScheme(VisibilityMetricsLogger::Scheme::kHttp);
  // If pixels is 0 then time spent is logged under kExactlyZeroPercent,
  // otherwise the screen portion is calculated as percentage / 10.
  logger()->UpdateOpenWebScreenArea(/*pixels=*/0, /*percentage=*/0);

  task_environment().FastForwardBy(base::Seconds(20));
  logger()->UpdateOpenWebScreenArea(/*pixels=*/14, /*percentage=*/7);

  task_environment().FastForwardBy(base::Seconds(5));
  logger()->UpdateOpenWebScreenArea(/*pixels=*/84, /*percentage=*/42);

  task_environment().FastForwardBy(base::Seconds(10));
  logger()->UpdateOpenWebScreenArea(/*pixels=*/200, /*percentage=*/100);

  task_environment().FastForwardBy(base::Seconds(15));
  client->SetViewVisible(false);

  task_environment().FastForwardBy(base::Seconds(10));
  client->SetViewVisible(true);

  task_environment().FastForwardBy(base::Seconds(25));
  client->SetScheme(VisibilityMetricsLogger::Scheme::kData);

  task_environment().FastForwardBy(base::Seconds(5));
  logger()->RecordMetrics();
  client.reset();

  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kNotVisible, 20);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.Visibility.Global",
      VisibilityMetricsLogger::Visibility::kVisible, 80);
  histogram_tester.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                                     VisibilityMetricsLogger::Scheme::kHttp,
                                     75);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.WebViewOpenWebVisible.ScreenPortion2",
      VisibilityMetricsLogger::WebViewOpenWebScreenPortion::kExactlyZeroPercent,
      20);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.WebViewOpenWebVisible.ScreenPortion2",
      VisibilityMetricsLogger::WebViewOpenWebScreenPortion::kZeroPercent, 5);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.WebViewOpenWebVisible.ScreenPortion2",
      VisibilityMetricsLogger::WebViewOpenWebScreenPortion::kFortyPercent, 10);
  histogram_tester.ExpectBucketCount(
      "Android.WebView.WebViewOpenWebVisible.ScreenPortion2",
      VisibilityMetricsLogger::WebViewOpenWebScreenPortion::kOneHundredPercent,
      40);
}

}  // namespace android_webview
