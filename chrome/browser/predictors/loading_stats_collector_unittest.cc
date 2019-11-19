// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_stats_collector.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/network_isolation_key.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace predictors {

namespace {
const char kInitialUrl[] = "http://www.google.com/cats";
const char kRedirectedUrl[] = "http://www.google.fr/chats";
const char kRedirectedUrl2[] = "http://www.google.de/katzen";
}

using RedirectStatus = ResourcePrefetchPredictor::RedirectStatus;

class LoadingStatsCollectorTest : public testing::Test {
 public:
  LoadingStatsCollectorTest();
  ~LoadingStatsCollectorTest() override;
  void SetUp() override;

  void TestRedirectStatusHistogram(const std::string& initial_url,
                                   const std::string& prediction_url,
                                   const std::string& navigation_url,
                                   RedirectStatus expected_status);

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<StrictMock<MockResourcePrefetchPredictor>> mock_predictor_;
  std::unique_ptr<LoadingStatsCollector> stats_collector_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

LoadingStatsCollectorTest::LoadingStatsCollectorTest() = default;

LoadingStatsCollectorTest::~LoadingStatsCollectorTest() = default;

void LoadingStatsCollectorTest::SetUp() {
  LoadingPredictorConfig config;
  PopulateTestConfig(&config);
  profile_ = std::make_unique<TestingProfile>();
  content::RunAllTasksUntilIdle();
  mock_predictor_ = std::make_unique<StrictMock<MockResourcePrefetchPredictor>>(
      config, profile_.get());
  stats_collector_ =
      std::make_unique<LoadingStatsCollector>(mock_predictor_.get(), config);
  histogram_tester_ = std::make_unique<base::HistogramTester>();
  content::RunAllTasksUntilIdle();
}

void LoadingStatsCollectorTest::TestRedirectStatusHistogram(
    const std::string& initial_url,
    const std::string& prediction_url,
    const std::string& navigation_url,
    RedirectStatus expected_status) {
  // Prediction setting.
  // We need at least one resource for prediction.
  const std::string& script_url = "https://cdn.google.com/script.js";
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      GURL(prediction_url).host(), initial_url != prediction_url,
      {{url::Origin::Create(GURL(script_url)), 1, net::NetworkIsolationKey()}});
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(GURL(initial_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));

  // Navigation simulation.
  std::vector<content::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(
      CreateResourceLoadInfoWithRedirects({initial_url, navigation_url}));
  resources.push_back(
      CreateResourceLoadInfo(script_url, content::ResourceType::kScript));
  PageRequestSummary summary =
      CreatePageRequestSummary(navigation_url, initial_url, resources);

  stats_collector_->RecordPageRequestSummary(summary);

  // Histogram check.
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectLearningRedirectStatus,
      static_cast<int>(expected_status), 1);
}

TEST_F(LoadingStatsCollectorTest, TestPreconnectPrecisionRecallHistograms) {
  const std::string main_frame_url = "http://google.com/?query=cats";
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };

  // Predicts 4 origins: 2 useful, 2 useless.
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      GURL(main_frame_url).host(), false,
      {{url::Origin::Create(GURL(main_frame_url)), 1,
        net::NetworkIsolationKey()},
       {url::Origin::Create(GURL(gen(1))), 1, net::NetworkIsolationKey()},
       {url::Origin::Create(GURL(gen(2))), 1, net::NetworkIsolationKey()},
       {url::Origin::Create(GURL(gen(3))), 0, net::NetworkIsolationKey()}});
  EXPECT_CALL(*mock_predictor_,
              PredictPreconnectOrigins(GURL(main_frame_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));

  // Simulate a page load with 2 resources, one we know, one we don't, plus we
  // know the main frame origin.
  std::vector<content::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo(main_frame_url));
  resources.push_back(
      CreateResourceLoadInfo(gen(1), content::ResourceType::kScript));
  resources.push_back(
      CreateResourceLoadInfo(gen(100), content::ResourceType::kScript));
  PageRequestSummary summary =
      CreatePageRequestSummary(main_frame_url, main_frame_url, resources);

  stats_collector_->RecordPageRequestSummary(summary);

  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectLearningRecall, 66, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectLearningPrecision, 50, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectLearningCount, 4, 1);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusNoRedirect) {
  TestRedirectStatusHistogram(kInitialUrl, kInitialUrl, kInitialUrl,
                              RedirectStatus::NO_REDIRECT);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusNoRedirectButPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kRedirectedUrl, kInitialUrl,
                              RedirectStatus::NO_REDIRECT_BUT_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusRedirectNotPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kInitialUrl, kRedirectedUrl,
                              RedirectStatus::REDIRECT_NOT_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest, TestRedirectStatusRedirectWrongPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kRedirectedUrl, kRedirectedUrl2,
                              RedirectStatus::REDIRECT_WRONG_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest,
       TestRedirectStatusRedirectCorrectlyPredicted) {
  TestRedirectStatusHistogram(kInitialUrl, kRedirectedUrl, kRedirectedUrl,
                              RedirectStatus::REDIRECT_CORRECTLY_PREDICTED);
}

TEST_F(LoadingStatsCollectorTest, TestPreconnectHistograms) {
  const std::string main_frame_url("http://google.com/?query=cats");
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };
  EXPECT_CALL(*mock_predictor_,
              PredictPreconnectOrigins(GURL(main_frame_url), _))
      .WillOnce(Return(false));

  {
    // Initialize PreconnectStats.

    // These two are hits.
    PreconnectedRequestStats origin1(url::Origin::Create(GURL(gen(1))), true);
    PreconnectedRequestStats origin2(url::Origin::Create(GURL(gen(2))), false);
    // And these two are misses.
    PreconnectedRequestStats origin3(url::Origin::Create(GURL(gen(3))), false);
    PreconnectedRequestStats origin4(url::Origin::Create(GURL(gen(4))), true);

    auto stats = std::make_unique<PreconnectStats>(GURL(main_frame_url));
    stats->requests_stats = {origin1, origin2, origin3, origin4};

    stats_collector_->RecordPreconnectStats(std::move(stats));
  }

  {
    // Simulate a page load with 3 origins.
    std::vector<content::mojom::ResourceLoadInfoPtr> resources;
    resources.push_back(CreateResourceLoadInfo(main_frame_url));
    resources.push_back(
        CreateResourceLoadInfo(gen(1), content::ResourceType::kScript));
    resources.push_back(
        CreateResourceLoadInfo(gen(2), content::ResourceType::kScript));
    resources.push_back(
        CreateResourceLoadInfo(gen(100), content::ResourceType::kScript));
    PageRequestSummary summary =
        CreatePageRequestSummary(main_frame_url, main_frame_url, resources);

    stats_collector_->RecordPageRequestSummary(summary);
  }

  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreresolveHitsPercentage, 50, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectHitsPercentage, 50, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreresolveCount, 4, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectCount, 2, 1);
}

// Tests that preconnect histograms won't be recorded if preconnect stats are
// empty.
TEST_F(LoadingStatsCollectorTest, TestPreconnectHistogramsEmpty) {
  const std::string main_frame_url = "http://google.com";
  auto stats = std::make_unique<PreconnectStats>(GURL(main_frame_url));
  stats_collector_->RecordPreconnectStats(std::move(stats));

  EXPECT_CALL(*mock_predictor_,
              PredictPreconnectOrigins(GURL(main_frame_url), _))
      .WillOnce(Return(false));

  std::vector<content::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo(main_frame_url));
  resources.push_back(CreateResourceLoadInfo("http://cdn.google.com/script.js",
                                             content::ResourceType::kScript));
  PageRequestSummary summary =
      CreatePageRequestSummary(main_frame_url, main_frame_url, resources);
  stats_collector_->RecordPageRequestSummary(summary);

  // No histograms should be recorded.
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreresolveHitsPercentage, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreconnectHitsPercentage, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreresolveCount, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreconnectCount, 0);
}

// Tests that the preconnect won't divide by zero if preconnect stats contain
// preresolve attempts only.
TEST_F(LoadingStatsCollectorTest, TestPreconnectHistogramsPreresolvesOnly) {
  const std::string main_frame_url("http://google.com/?query=cats");
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };
  EXPECT_CALL(*mock_predictor_,
              PredictPreconnectOrigins(GURL(main_frame_url), _))
      .WillOnce(Return(false));

  {
    // Initialize PreconnectStats.

    // These two are hits.
    PreconnectedRequestStats origin1(url::Origin::Create(GURL(gen(1))), false);
    PreconnectedRequestStats origin2(url::Origin::Create(GURL(gen(2))), false);
    // And these two are misses.
    PreconnectedRequestStats origin3(url::Origin::Create(GURL(gen(3))), false);
    PreconnectedRequestStats origin4(url::Origin::Create(GURL(gen(4))), false);

    auto stats = std::make_unique<PreconnectStats>(GURL(main_frame_url));
    stats->requests_stats = {origin1, origin2, origin3, origin4};

    stats_collector_->RecordPreconnectStats(std::move(stats));
  }

  {
    // Simulate a page load with 3 origins.
    std::vector<content::mojom::ResourceLoadInfoPtr> resources;
    resources.push_back(CreateResourceLoadInfo(main_frame_url));
    resources.push_back(
        CreateResourceLoadInfo(gen(1), content::ResourceType::kScript));
    resources.push_back(
        CreateResourceLoadInfo(gen(2), content::ResourceType::kScript));
    resources.push_back(
        CreateResourceLoadInfo(gen(100), content::ResourceType::kScript));
    PageRequestSummary summary =
        CreatePageRequestSummary(main_frame_url, main_frame_url, resources);

    stats_collector_->RecordPageRequestSummary(summary);
  }

  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreresolveHitsPercentage, 50, 1);
  // Can't really report a hits percentage if there were no events.
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreconnectHitsPercentage, 0);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreresolveCount, 4, 1);
  histogram_tester_->ExpectUniqueSample(
      internal::kLoadingPredictorPreconnectCount, 0, 1);
}

}  // namespace predictors
