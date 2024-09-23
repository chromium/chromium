// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_stats_collector.h"

#include <vector>

#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/predictors/loading_test_util.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/test/base/testing_profile.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_utils.h"
#include "net/base/network_isolation_key.h"
#include "services/metrics/public/cpp/ukm_builders.h"
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
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  const net::NetworkAnonymizationKey network_anonymization_key_ =
      net::NetworkAnonymizationKey::CreateTransient();
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
  ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
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
      {{url::Origin::Create(GURL(script_url)), 1, network_anonymization_key_}});
  EXPECT_CALL(*mock_predictor_, PredictPreconnectOrigins(GURL(initial_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));

  // Navigation simulation.
  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(
      CreateResourceLoadInfoWithRedirects({initial_url, navigation_url}));
  resources.push_back(CreateResourceLoadInfo(
      script_url, network::mojom::RequestDestination::kScript));
  PageRequestSummary summary =
      CreatePageRequestSummary(navigation_url, initial_url, resources);

  stats_collector_->RecordPageRequestSummary(summary, std::nullopt);

  // Histogram check.
  histogram_tester_->ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningRedirectStatus.Navigation",
      static_cast<int>(expected_status), 1);
}

TEST_F(LoadingStatsCollectorTest, TestPreconnectPrecisionRecallMetrics) {
  const std::string main_frame_url = "http://google.com/?query=cats";
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };

  // Predicts 4 origins: 2 useful, 2 useless.
  PreconnectPrediction prediction = CreatePreconnectPrediction(
      GURL(main_frame_url).host(), false,
      {{url::Origin::Create(GURL(main_frame_url)), 1,
        network_anonymization_key_},
       {url::Origin::Create(GURL(gen(1))), 1, network_anonymization_key_},
       {url::Origin::Create(GURL(gen(2))), 1, network_anonymization_key_},
       {url::Origin::Create(GURL(gen(3))), 0, network_anonymization_key_}});
  EXPECT_CALL(*mock_predictor_,
              PredictPreconnectOrigins(GURL(main_frame_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(prediction), Return(true)));

  // Simulate a page load with 2 resources, one we know, one we don't, plus we
  // know the main frame origin.
  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo(main_frame_url));
  resources.push_back(CreateResourceLoadInfo(
      gen(1), network::mojom::RequestDestination::kScript));
  resources.push_back(CreateResourceLoadInfo(
      gen(100), network::mojom::RequestDestination::kScript));
  base::TimeTicks now = base::TimeTicks::Now();
  PageRequestSummary summary =
      CreatePageRequestSummary(main_frame_url, main_frame_url, resources, now);
  summary.navigation_committed = now + base::Milliseconds(3);
  summary.preconnect_origins = {
      url::Origin::Create(GURL(gen(1))),
      url::Origin::Create(GURL(gen(2))),
      url::Origin::Create(GURL(gen(3))),
  };

  stats_collector_->RecordPageRequestSummary(summary, std::nullopt);

  histogram_tester_->ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningRecall.Navigation", 66, 1);
  histogram_tester_->ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningPrecision.Navigation", 50, 1);
  histogram_tester_->ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningCount.Navigation", 4, 1);
  histogram_tester_->ExpectTotalCount(
      "LoadingPredictor.PreconnectLearningRecall.OptimizationGuide", 0);
  histogram_tester_->ExpectTotalCount(
      "LoadingPredictor.PreconnectLearningPrecision.OptimizationGuide", 0);
  histogram_tester_->ExpectTotalCount(
      "LoadingPredictor.PreconnectLearningCount.OptimizationGuide", 0);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::LoadingPredictor::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder_->ExpectEntryMetric(
      entry, ukm::builders::LoadingPredictor::kLocalPredictionOriginsName, 4);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kLocalPredictionCorrectlyPredictedOriginsName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::kNavigationStartToNavigationCommitName,
      3);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kSubresourceOriginPreconnectsInitiatedName,
      3);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kCorrectSubresourceOriginPreconnectsInitiatedName,
      1);
  // Make sure optimization guide metrics are not recorded.
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kOptimizationGuidePredictionDecisionName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kOptimizationGuidePredictionOriginsName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kOptimizationGuidePredictionCorrectlyPredictedOriginsName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kOptimizationGuidePredictionSubresourcesName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedSubresourcesName));
  // Make sure prefetch metrics are not recorded since no prefetches were
  // initiated.
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kNavigationStartToFirstSubresourcePrefetchInitiatedName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry,
      ukm::builders::LoadingPredictor::kSubresourcePrefetchesInitiatedName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kCorrectSubresourcePrefetchesInitiatedName));
}

TEST_F(LoadingStatsCollectorTest,
       TestPrecisionRecallMetricsWithOptimizationGuide) {
  const std::string main_frame_url = "http://google.com/?query=cats";
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };
  const std::string same_origin_subresource_url = "http://google.com/script.js";

  PreconnectPrediction local_prediction;
  EXPECT_CALL(*mock_predictor_,
              PredictPreconnectOrigins(GURL(main_frame_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(local_prediction), Return(false)));

  // Optimization Guide predicts 4 origins: 2 useful, 2 useless.
  std::optional<OptimizationGuidePrediction> optimization_guide_prediction =
      OptimizationGuidePrediction();
  optimization_guide_prediction->decision =
      optimization_guide::OptimizationGuideDecision::kTrue;
  base::TimeTicks now = base::TimeTicks::Now();
  optimization_guide_prediction->optimization_guide_prediction_arrived =
      now + base::Milliseconds(3);
  optimization_guide_prediction->preconnect_prediction =
      CreatePreconnectPrediction(
          GURL(main_frame_url).host(), false,
          {{url::Origin::Create(GURL(main_frame_url)), 1,
            network_anonymization_key_},
           {url::Origin::Create(GURL(gen(1))), 1, network_anonymization_key_},
           {url::Origin::Create(GURL(gen(2))), 1, network_anonymization_key_},
           {url::Origin::Create(GURL(gen(3))), 0, network_anonymization_key_}});
  optimization_guide_prediction->predicted_subresources = {
      GURL(same_origin_subresource_url), GURL(gen(1)), GURL(gen(2)),
      GURL(gen(3)), GURL(gen(4))};

  // Simulate a page load with 2 resources, one we know, one we don't, plus we
  // know the main frame origin.
  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo(main_frame_url));
  resources.push_back(
      CreateResourceLoadInfo(same_origin_subresource_url,
                             network::mojom::RequestDestination::kScript));
  resources.push_back(CreateResourceLoadInfo(
      gen(1), network::mojom::RequestDestination::kScript));
  resources.push_back(CreateResourceLoadInfo(
      gen(100), network::mojom::RequestDestination::kScript));
  PageRequestSummary summary =
      CreatePageRequestSummary(main_frame_url, main_frame_url, resources, now);
  summary.prefetch_urls = {
      GURL(gen(1)),
      GURL(gen(2)),
      GURL(gen(3)),
  };
  summary.first_prefetch_initiated = now + base::Milliseconds(1);

  stats_collector_->RecordPageRequestSummary(summary,
                                             optimization_guide_prediction);

  histogram_tester_->ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningRecall.OptimizationGuide", 66, 1);
  histogram_tester_->ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningPrecision.OptimizationGuide", 50, 1);
  histogram_tester_->ExpectUniqueSample(
      "LoadingPredictor.PreconnectLearningCount.OptimizationGuide", 4, 1);
  histogram_tester_->ExpectTotalCount(
      "LoadingPredictor.PreconnectLearningRecall.Navigation", 0);
  histogram_tester_->ExpectTotalCount(
      "LoadingPredictor.PreconnectLearningPrecision.Navigation", 0);
  histogram_tester_->ExpectTotalCount(
      "LoadingPredictor.PreconnectLearningCount.Navigation", 0);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::LoadingPredictor::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::kSubresourcePrefetchesInitiatedName, 3);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kCorrectSubresourcePrefetchesInitiatedName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kNavigationStartToFirstSubresourcePrefetchInitiatedName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::kOptimizationGuidePredictionDecisionName,
      static_cast<int64_t>(
          optimization_guide::OptimizationGuideDecision::kTrue));
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kNavigationStartToOptimizationGuidePredictionArrivedName,
      3);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::kOptimizationGuidePredictionOriginsName,
      4);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedOriginsName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedLowPriorityOriginsName,
      0);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionSubresourcesName,
      5);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionSubresources_CrossOriginName,
      4);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedSubresourcesName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedSubresources_CrossOriginName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedLowPrioritySubresourcesName,
      0);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedLowPrioritySubresources_CrossOriginName,
      0);
  // Make sure local metrics are not recorded since there was not a local
  // prediction.
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::kLocalPredictionOriginsName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kLocalPredictionCorrectlyPredictedOriginsName));
  // Make sure preconnect metrics are not recorded since no preconnects were
  // initiated.
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kSubresourceOriginPreconnectsInitiatedName));
  EXPECT_FALSE(ukm_recorder_->EntryHasMetric(
      entry, ukm::builders::LoadingPredictor::
                 kCorrectSubresourceOriginPreconnectsInitiatedName));
}

TEST_F(LoadingStatsCollectorTest,
       TestOptimizationGuideCorrectPredictionsPostLoad) {
  const std::string main_frame_url = "http://google.com/?query=cats";
  const std::string same_origin_subresource_url = "http://google.com/script.js";
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };

  PreconnectPrediction local_prediction;
  EXPECT_CALL(*mock_predictor_,
              PredictPreconnectOrigins(GURL(main_frame_url), _))
      .WillOnce(DoAll(SetArgPointee<1>(local_prediction), Return(false)));

  std::optional<OptimizationGuidePrediction> optimization_guide_prediction =
      OptimizationGuidePrediction();
  optimization_guide_prediction->decision =
      optimization_guide::OptimizationGuideDecision::kTrue;
  base::TimeTicks now = base::TimeTicks::Now();
  optimization_guide_prediction->optimization_guide_prediction_arrived =
      now + base::Milliseconds(3);
  optimization_guide_prediction->preconnect_prediction =
      CreatePreconnectPrediction(
          GURL(main_frame_url).host(), false,
          {{url::Origin::Create(GURL(main_frame_url)), 1,
            network_anonymization_key_},
           {url::Origin::Create(GURL(gen(1))), 1, network_anonymization_key_},
           {url::Origin::Create(GURL(gen(2))), 1, network_anonymization_key_},
           {url::Origin::Create(GURL(gen(3))), 0, network_anonymization_key_}});
  optimization_guide_prediction->predicted_subresources = {
      GURL(same_origin_subresource_url), GURL(gen(1)), GURL(gen(2)),
      GURL(gen(3)), GURL(gen(4))};

  // Simulate a page load with 3 resources (we know all 3). The 3rd resource
  // is fetched after the page finishes loading.
  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo(main_frame_url));
  resources.push_back(CreateResourceLoadInfo(
      gen(1), network::mojom::RequestDestination::kScript));
  resources.push_back(CreateResourceLoadInfo(
      gen(2), network::mojom::RequestDestination::kScript));
  resources.push_back(CreateResourceLoadInfo(
      gen(3), network::mojom::RequestDestination::kScript));
  resources.push_back(
      CreateResourceLoadInfo(same_origin_subresource_url,
                             network::mojom::RequestDestination::kScript));
  PageRequestSummary summary =
      CreatePageRequestSummary(main_frame_url, main_frame_url, {}, now,
                               /*main_frame_load_complete=*/false);
  summary.UpdateOrAddResource(*resources[0]);
  summary.UpdateOrAddResource(*resources[1]);
  summary.UpdateOrAddResource(*resources[2]);
  summary.MainFrameLoadComplete();
  summary.UpdateOrAddResource(*resources[3]);
  summary.UpdateOrAddResource(*resources[2]);
  summary.UpdateOrAddResource(*resources[4]);
  summary.prefetch_urls = {GURL(gen(1)), GURL(gen(2)), GURL(gen(3)),
                           GURL(gen(4))};
  summary.first_prefetch_initiated = now + base::Milliseconds(1);

  stats_collector_->RecordPageRequestSummary(summary,
                                             optimization_guide_prediction);
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::LoadingPredictor::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::kSubresourcePrefetchesInitiatedName, 4);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kCorrectSubresourcePrefetchesInitiatedName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kNavigationStartToFirstSubresourcePrefetchInitiatedName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::kOptimizationGuidePredictionDecisionName,
      static_cast<int64_t>(
          optimization_guide::OptimizationGuideDecision::kTrue));
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kNavigationStartToOptimizationGuidePredictionArrivedName,
      3);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::kOptimizationGuidePredictionOriginsName,
      4);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedOriginsName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedLowPriorityOriginsName,
      1);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionSubresourcesName,
      5);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionSubresources_CrossOriginName,
      4);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedSubresourcesName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedSubresources_CrossOriginName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedLowPrioritySubresourcesName,
      2);
  ukm_recorder_->ExpectEntryMetric(
      entry,
      ukm::builders::LoadingPredictor::
          kOptimizationGuidePredictionCorrectlyPredictedLowPrioritySubresources_CrossOriginName,
      1);
}

TEST_F(LoadingStatsCollectorTest, TestNoMetricsRecordedBeforeLoadComplete) {
  const std::string main_frame_url = "http://google.com/?query=cats";
  auto gen = [](int index) {
    return base::StringPrintf("http://cdn%d.google.com/script.js", index);
  };

  std::optional<OptimizationGuidePrediction> optimization_guide_prediction =
      OptimizationGuidePrediction();
  optimization_guide_prediction->decision =
      optimization_guide::OptimizationGuideDecision::kTrue;
  base::TimeTicks now = base::TimeTicks::Now();
  optimization_guide_prediction->optimization_guide_prediction_arrived =
      now + base::Milliseconds(3);
  optimization_guide_prediction->preconnect_prediction =
      CreatePreconnectPrediction(GURL(main_frame_url).host(), false, {});
  optimization_guide_prediction->predicted_subresources = {GURL(gen(1))};

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo(main_frame_url));
  resources.push_back(CreateResourceLoadInfo(
      gen(1), network::mojom::RequestDestination::kScript));
  PageRequestSummary summary =
      CreatePageRequestSummary(main_frame_url, main_frame_url, {}, now,
                               /*main_frame_load_complete=*/false);
  summary.UpdateOrAddResource(*resources[0]);
  summary.UpdateOrAddResource(*resources[1]);
  stats_collector_->RecordPageRequestSummary(summary,
                                             optimization_guide_prediction);

  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::LoadingPredictor::kEntryName);
  EXPECT_EQ(0u, entries.size());
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
    std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
    resources.push_back(CreateResourceLoadInfo(main_frame_url));
    resources.push_back(CreateResourceLoadInfo(
        gen(1), network::mojom::RequestDestination::kScript));
    resources.push_back(CreateResourceLoadInfo(
        gen(2), network::mojom::RequestDestination::kScript));
    resources.push_back(CreateResourceLoadInfo(
        gen(100), network::mojom::RequestDestination::kScript));
    PageRequestSummary summary =
        CreatePageRequestSummary(main_frame_url, main_frame_url, resources);

    stats_collector_->RecordPageRequestSummary(summary, std::nullopt);
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

  std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
  resources.push_back(CreateResourceLoadInfo(main_frame_url));
  resources.push_back(
      CreateResourceLoadInfo("http://cdn.google.com/script.js",
                             network::mojom::RequestDestination::kScript));
  base::TimeTicks now = base::TimeTicks::Now();
  PageRequestSummary summary =
      CreatePageRequestSummary(main_frame_url, main_frame_url, resources, now);
  summary.navigation_committed = now + base::Milliseconds(3);
  stats_collector_->RecordPageRequestSummary(summary, std::nullopt);

  // No histograms should be recorded.
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreresolveHitsPercentage, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreconnectHitsPercentage, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreresolveCount, 0);
  histogram_tester_->ExpectTotalCount(
      internal::kLoadingPredictorPreconnectCount, 0);
  // UKM should not be recorded since we did not have predictions for the
  // navigation.
  auto entries = ukm_recorder_->GetEntriesByName(
      ukm::builders::LoadingPredictor::kEntryName);
  EXPECT_EQ(0u, entries.size());
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
    std::vector<blink::mojom::ResourceLoadInfoPtr> resources;
    resources.push_back(CreateResourceLoadInfo(main_frame_url));
    resources.push_back(CreateResourceLoadInfo(
        gen(1), network::mojom::RequestDestination::kScript));
    resources.push_back(CreateResourceLoadInfo(
        gen(2), network::mojom::RequestDestination::kScript));
    resources.push_back(CreateResourceLoadInfo(
        gen(100), network::mojom::RequestDestination::kScript));
    PageRequestSummary summary =
        CreatePageRequestSummary(main_frame_url, main_frame_url, resources);

    stats_collector_->RecordPageRequestSummary(summary, std::nullopt);
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
