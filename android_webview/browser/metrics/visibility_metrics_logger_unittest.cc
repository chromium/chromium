// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/metrics/visibility_metrics_logger.h"

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

namespace {

typedef VisibilityMetricsLogger::Scheme Scheme;
typedef VisibilityMetricsLogger::Visibility Visibility;

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
  base::HistogramTester histograms;

  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Milliseconds(500));

  logger()->RecordMetrics();
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 0);

  task_environment().FastForwardBy(base::Milliseconds(500));

  logger()->RecordMetrics();
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 1);

  client.reset();
}

TEST_F(VisibilityMetricsLoggerTest, TestSingleVisibleClient) {
  base::HistogramTester histograms;

  task_environment().FastForwardBy(base::Seconds(10));
  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(30));
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);
  client->SetScheme(Scheme::kHttp);

  task_environment().FastForwardBy(base::Seconds(10));
  client->SetWindowVisible(false);

  logger()->RecordMetrics();
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 10);
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kNotVisible, 40);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kHttp, 10);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kData, 0);

  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);
  client->SetScheme(Scheme::kData);
  task_environment().FastForwardBy(base::Seconds(90));

  logger()->RecordMetrics();
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 100);
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kNotVisible, 40);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kHttp, 10);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kData, 90);

  client.reset();
}

TEST_F(VisibilityMetricsLoggerTest, TestLongDurationVisibleClient) {
  base::HistogramTester histograms;
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
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 150);
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kNotVisible, 300);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kVisible, 200);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kNotVisible, 650);
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
  base::HistogramTester histograms;
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
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 30);
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kNotVisible, 70);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kVisible, 40);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kNotVisible, 150);
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

  base::HistogramTester histograms;
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(10));
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(30));
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);

  task_environment().FastForwardBy(base::Seconds(10));
  client1->SetScheme(Scheme::kHttp);

  task_environment().FastForwardBy(base::Seconds(10));
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);
  client2->SetScheme(Scheme::kHttp);

  task_environment().FastForwardBy(base::Seconds(10));
  client1->SetWindowVisible(false);

  task_environment().FastForwardBy(base::Seconds(10));
  client2->SetWindowVisible(false);

  task_environment().FastForwardBy(base::Seconds(20));
  client1.reset();
  client2.reset();

  logger()->RecordMetrics();
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 40);
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kNotVisible, 60);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kVisible, 50);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kNotVisible, 140);

  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kEmpty, 10);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kHttp, 30);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.PerWebView",
                               Scheme::kEmpty, 10);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.PerWebView",
                               Scheme::kHttp, 40);
}

TEST_F(VisibilityMetricsLoggerTest, TestScreenCoverage) {
  // t=0: client created
  // t=10: client visible, navigates to http scheme, screen percentage 0%
  // t=30: 7% screen percentage
  // t=35: 42% screen percentage
  // t=45: 100% screen percentage
  // t=60: client invisible
  // t=70: client visible
  // t=95: client navigates to data scheme
  // t=100: client deleted

  // Time with no visible client: 10 + 10 = 20
  // Time with client visible: 20 + 5 + 10 + 15 + 25 + 5 = 80
  // Time displaying http scheme: 20 + 5 + 10 + 15 + 25 = 75
  // Time displaying with 0% coverage: 20
  // Time displaying with 7% coverage: 5
  // Time displaying with 42% coverage: 10
  // Time displaying with 100% coverage: 15 + 25 + 5 = 45
  // Time displaying http scheme with 100% coverage: 40
  // Time displaying data scheme with 100% coverage: 5

  base::HistogramTester histograms;
  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());

  task_environment().FastForwardBy(base::Seconds(10));
  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);
  client->SetScheme(Scheme::kHttp);
  logger()->UpdateScreenCoverage(/*global_percentage=*/0, {Scheme::kHttp},
                                 /*scheme_percentages=*/{0});

  task_environment().FastForwardBy(base::Seconds(20));
  logger()->UpdateScreenCoverage(/*global_percentage=*/7, {Scheme::kHttp},
                                 /*scheme_percentages=*/{7});

  task_environment().FastForwardBy(base::Seconds(5));
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kHttp},
                                 /*scheme_percentages=*/{42});

  task_environment().FastForwardBy(base::Seconds(10));
  logger()->UpdateScreenCoverage(
      /*global_percentage=*/100, {Scheme::kHttp},
      /*scheme_percentages=*/{100});

  task_environment().FastForwardBy(base::Seconds(15));
  client->SetViewVisible(false);
  logger()->UpdateScreenCoverage(/*global_percentage=*/0, {Scheme::kHttp},
                                 /*scheme_percentages=*/{0});

  task_environment().FastForwardBy(base::Seconds(10));
  client->SetViewVisible(true);
  logger()->UpdateScreenCoverage(
      /*global_percentage=*/100, {Scheme::kHttp},
      /*scheme_percentages=*/{100});

  task_environment().FastForwardBy(base::Seconds(25));
  client->SetScheme(Scheme::kData);
  logger()->UpdateScreenCoverage(
      /*global_percentage=*/100, {Scheme::kData},
      /*scheme_percentages=*/{100});

  task_environment().FastForwardBy(base::Seconds(5));
  logger()->RecordMetrics();
  client.reset();

  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kNotVisible, 20);
  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 80);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kHttp, 75);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/0, 20);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/7, 5);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/42, 10);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/100, 45);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.http",
      /*percentage=*/100, 40);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.data",
      /*percentage=*/100, 5);
}

TEST_F(VisibilityMetricsLoggerTest, TestScreenCoverageByScheme) {
  base::HistogramTester histograms;
  std::unique_ptr<TestClient> client = std::make_unique<TestClient>(logger());

  client->SetViewVisible(true);
  client->SetViewAttached(true);
  client->SetWindowVisible(true);

  client->SetScheme(Scheme::kEmpty);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kEmpty},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(100));

  client->SetScheme(Scheme::kUnknown);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kUnknown},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(101));

  client->SetScheme(Scheme::kHttp);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kHttp},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(102));

  client->SetScheme(Scheme::kHttps);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kHttps},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(103));

  client->SetScheme(Scheme::kFile);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kFile},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(104));

  client->SetScheme(Scheme::kFtp);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kFtp},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(105));

  client->SetScheme(Scheme::kData);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kData},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(106));

  client->SetScheme(Scheme::kJavaScript);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42,
                                 {Scheme::kJavaScript},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(107));

  client->SetScheme(Scheme::kAbout);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kAbout},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(108));

  client->SetScheme(Scheme::kChrome);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kChrome},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(109));

  client->SetScheme(Scheme::kBlob);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kBlob},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(110));

  client->SetScheme(Scheme::kContent);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kContent},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(111));

  client->SetScheme(Scheme::kIntent);
  logger()->UpdateScreenCoverage(/*global_percentage=*/42, {Scheme::kIntent},
                                 /*scheme_percentages=*/{42});
  task_environment().FastForwardBy(base::Seconds(112));

  logger()->RecordMetrics();
  client.reset();

  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 1378);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/42, 1378);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView",
      /*percentage=*/42, 1378);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.empty",
      /*percentage=*/42, 100);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.unknown",
      /*percentage=*/42, 101);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.http",
      /*percentage=*/42, 102);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.https",
      /*percentage=*/42, 103);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.file",
      /*percentage=*/42, 104);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.ftp",
      /*percentage=*/42, 105);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.data",
      /*percentage=*/42, 106);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.javascript",
      /*percentage=*/42, 107);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.about",
      /*percentage=*/42, 108);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.chrome",
      /*percentage=*/42, 109);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.blob",
      /*percentage=*/42, 110);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.content",
      /*percentage=*/42, 111);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.intent",
      /*percentage=*/42, 112);
}

TEST_F(VisibilityMetricsLoggerTest, TestScreenCoverageTwoClientsOverlapping) {
  base::HistogramTester histograms;

  // The first client's coverage is 10 percent.
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);
  client1->SetScheme(Scheme::kHttp);
  logger()->UpdateScreenCoverage(/*global_percentage=*/10, {Scheme::kHttp},
                                 /*scheme_percentages=*/{10});
  task_environment().FastForwardBy(base::Seconds(1));

  // The second client overlaps exactly with the first but has a data scheme.
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);
  client2->SetScheme(Scheme::kData);
  logger()->UpdateScreenCoverage(/*global_percentage=*/10,
                                 {Scheme::kHttp, Scheme::kData},
                                 /*scheme_percentages=*/{10, 10});
  task_environment().FastForwardBy(base::Seconds(3));

  client1.reset();
  client2.reset();
  logger()->RecordMetrics();

  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 4);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kVisible, 7);

  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kHttp, 4);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kData, 3);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.PerWebView",
                               Scheme::kHttp, 4);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.PerWebView",
                               Scheme::kData, 3);

  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.http",
      /*percentage=*/10, 4);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.data",
      /*percentage=*/10, 3);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/10, 4);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView",
      /*percentage=*/10, 7);
}

TEST_F(VisibilityMetricsLoggerTest, TestScreenCoverageTwoClientsNoOverlap) {
  base::HistogramTester histograms;

  // The first client's coverage is 10 percent.
  std::unique_ptr<TestClient> client1 = std::make_unique<TestClient>(logger());
  client1->SetViewVisible(true);
  client1->SetViewAttached(true);
  client1->SetWindowVisible(true);
  client1->SetScheme(Scheme::kHttp);
  logger()->UpdateScreenCoverage(/*global_percentage=*/10, {Scheme::kHttp},
                                 /*scheme_percentages=*/{10});
  task_environment().FastForwardBy(base::Seconds(1));

  // The second client is the same size and scheme but does not overlap.
  std::unique_ptr<TestClient> client2 = std::make_unique<TestClient>(logger());
  client2->SetViewVisible(true);
  client2->SetViewAttached(true);
  client2->SetWindowVisible(true);
  client2->SetScheme(Scheme::kHttp);
  logger()->UpdateScreenCoverage(/*global_percentage=*/20,
                                 {Scheme::kHttp, Scheme::kHttp},
                                 /*scheme_percentages=*/{10, 10});
  task_environment().FastForwardBy(base::Seconds(3));

  client1.reset();
  client2.reset();
  logger()->RecordMetrics();

  histograms.ExpectBucketCount("Android.WebView.Visibility.Global",
                               Visibility::kVisible, 4);
  histograms.ExpectBucketCount("Android.WebView.Visibility.PerWebView",
                               Visibility::kVisible, 7);

  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.Global",
                               Scheme::kHttp, 4);
  histograms.ExpectBucketCount("Android.WebView.VisibleScheme.PerWebView",
                               Scheme::kHttp, 7);

  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView.http",
      /*percentage=*/10, 7);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/10, 1);
  histograms.ExpectBucketCount("Android.WebView.VisibleScreenCoverage.Global",
                               /*percentage=*/20, 3);
  histograms.ExpectBucketCount(
      "Android.WebView.VisibleScreenCoverage.PerWebView",
      /*percentage=*/10, 7);
}

}  // namespace

}  // namespace android_webview
