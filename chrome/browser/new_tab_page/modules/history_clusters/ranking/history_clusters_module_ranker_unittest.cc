// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search/ntp_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_handler.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#endif

namespace {

using ::testing::ElementsAre;

class HistoryClustersModuleRankerTest : public testing::Test {
 public:
  HistoryClustersModuleRankerTest() = default;
  ~HistoryClustersModuleRankerTest() override = default;

  std::vector<history::Cluster> RankClusters(
      HistoryClustersModuleRanker* ranker,
      std::vector<history::Cluster> in_clusters) {
    // Within each cluster, sort visits.
    for (auto& cluster : in_clusters) {
      history_clusters::StableSortVisits(cluster.visits);
    }

    std::vector<history::Cluster> clusters;
    base::RunLoop run_loop;
    ranker->RankClusters(std::move(in_clusters),
                         base::BindOnce(
                             [](base::RunLoop* run_loop,
                                std::vector<history::Cluster>* out_clusters,
                                std::vector<history::Cluster> clusters) {
                               *out_clusters = std::move(clusters);
                               run_loop->Quit();
                             },
                             &run_loop, &clusters));

    run_loop.Run();
    return clusters;
  }

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HistoryClustersModuleRankerTest, RecencyOnly) {
  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://github.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {{"category1", 90},
                                                            {"category2", 84}};
  history::AnnotatedVisit visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://search.com/"));
  visit2.visit_row.visit_time = base::Time::FromTimeT(3);
  visit2.content_annotations.search_terms = u"search";
  visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit visit4 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          4, GURL("https://github.com/2"));
  visit4.content_annotations.model_annotations.categories = {{"category1", 85},
                                                             {"category3", 82}};
  visit4.content_annotations.has_url_keyed_image = true;
  visit4.visit_row.is_known_to_sync = true;
  cluster1.visits = {history_clusters::testing::CreateClusterVisit(
                         visit, /*normalized_url=*/absl::nullopt, 0.1),
                     history_clusters::testing::CreateClusterVisit(
                         visit2, /*normalized_url=*/absl::nullopt, 1.0),
                     history_clusters::testing::CreateClusterVisit(
                         visit4, /*normalized_url=*/absl::nullopt, 0.3)};

  history::Cluster cluster2 = cluster1;
  // Make the visit time before the first cluster and the first visit have a
  // different visit ID so we can differentiate the two clusters.
  cluster2.visits[1].annotated_visit.visit_row.visit_id = 123;
  cluster2.visits[1].annotated_visit.visit_row.visit_time =
      base::Time::FromTimeT(10);

  base::flat_set<std::string> boost = {};
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      /*optimization_guide_model_provider=*/nullptr, boost);
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2});

  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(history_clusters::testing::VisitResult(
                                  123, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(4, 0.3),
                              history_clusters::testing::VisitResult(1, 0.1)),
                  ElementsAre(history_clusters::testing::VisitResult(2, 1.0, {},
                                                                     u"search"),
                              history_clusters::testing::VisitResult(4, 0.3),
                              history_clusters::testing::VisitResult(1, 0.1))));
}

TEST_F(HistoryClustersModuleRankerTest, WithCategoryBoosting) {
  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://github.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {
      {"category1", 90}, {"boostedbuthidden", 84}};
  history::AnnotatedVisit visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://search.com/"));
  visit2.visit_row.visit_time = base::Time::FromTimeT(100);
  visit2.content_annotations.search_terms = u"search";
  visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit visit4 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          4, GURL("https://github.com/2"));
  visit4.content_annotations.model_annotations.categories = {{"category1", 85},
                                                             {"category3", 82}};
  visit4.content_annotations.has_url_keyed_image = true;
  visit4.visit_row.is_known_to_sync = true;
  cluster1.visits = {history_clusters::testing::CreateClusterVisit(
                         visit, /*normalized_url=*/absl::nullopt, 0.0),
                     history_clusters::testing::CreateClusterVisit(
                         visit2, /*normalized_url=*/absl::nullopt, 1.0),
                     history_clusters::testing::CreateClusterVisit(
                         visit4, /*normalized_url=*/absl::nullopt, 0.3)};

  history::Cluster cluster2;
  cluster2.cluster_id = 2;
  history::AnnotatedVisit c2_visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          111, GURL("https://github.com/"));
  c2_visit.visit_row.is_known_to_sync = true;
  c2_visit.content_annotations.has_url_keyed_image = true;
  c2_visit.content_annotations.model_annotations.categories = {
      {"category1", 90}, {"boosted", 84}};
  history::AnnotatedVisit c2_visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          222, GURL("https://search.com/"));
  c2_visit2.visit_row.visit_time = base::Time::FromTimeT(3);
  c2_visit2.content_annotations.search_terms = u"search";
  c2_visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit c2_visit4 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          444, GURL("https://github.com/2"));
  c2_visit4.content_annotations.model_annotations.categories = {
      {"category1", 85}, {"category3", 82}};
  c2_visit4.content_annotations.has_url_keyed_image = true;
  c2_visit4.visit_row.is_known_to_sync = true;
  cluster2.visits = {history_clusters::testing::CreateClusterVisit(
                         c2_visit, /*normalized_url=*/absl::nullopt, 0.8),
                     history_clusters::testing::CreateClusterVisit(
                         c2_visit2, /*normalized_url=*/absl::nullopt, 1.0),
                     history_clusters::testing::CreateClusterVisit(
                         c2_visit4, /*normalized_url=*/absl::nullopt, 0.6)};

  history::Cluster cluster3 = cluster2;
  cluster3.cluster_id = 3;
  for (auto& cluster_visit : cluster3.visits) {
    // Increment the visits to differentiate the cluster.
    cluster_visit.annotated_visit.visit_row.visit_id++;
    // Change the time to be earlier.
    cluster_visit.annotated_visit.visit_row.visit_time =
        base::Time::FromTimeT(1);
  }

  base::flat_set<std::string> boost = {"boosted", "boostedbuthidden"};
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      /*optimization_guide_model_provider=*/nullptr, boost);
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2, cluster3});

  // The second and third clusters should be picked since it contains a boosted
  // category even though they were earlier than the first cluster and the
  // visits should be sorted according to score. Tiebreaker between multiple
  // clusters is still time.
  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(history_clusters::testing::VisitResult(
                                  222, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(111, 0.8),
                              history_clusters::testing::VisitResult(444, 0.6)),
                  ElementsAre(history_clusters::testing::VisitResult(
                                  223, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(112, 0.8),
                              history_clusters::testing::VisitResult(445, 0.6)),
                  ElementsAre(history_clusters::testing::VisitResult(2, 1.0, {},
                                                                     u"search"),
                              history_clusters::testing::VisitResult(4, 0.3),
                              history_clusters::testing::VisitResult(1, 0.0))));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)

class FakeModelHandler : public HistoryClustersModuleRankingModelHandler {
 public:
  explicit FakeModelHandler(
      optimization_guide::OptimizationGuideModelProvider* provider)
      : HistoryClustersModuleRankingModelHandler(provider) {}
  ~FakeModelHandler() override = default;

  bool CanExecuteAvailableModel() override { return true; }

  void ExecuteBatch(
      const std::vector<HistoryClustersModuleRankingSignals>& inputs,
      ExecuteBatchCallback callback) override {
    std::vector<float> outputs;
    outputs.reserve(inputs.size());
    for (const auto& input : inputs) {
      float score = input.belongs_to_boosted_category ? 1 : 0;
      outputs.push_back(
          score + static_cast<float>(
                      input.duration_since_most_recent_visit.InMinutes()));
      ;
    }
    std::move(callback).Run(std::move(outputs));
  }
};

class HistoryClustersModuleRankerWithModelTest
    : public HistoryClustersModuleRankerTest {
 public:
  HistoryClustersModuleRankerWithModelTest() {
    scoped_feature_list_.InitAndEnableFeature(
        ntp_features::kNtpHistoryClustersModuleUseModelRanking);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HistoryClustersModuleRankerWithModelTest,
       ModelNotAvailableUsesFallback) {
  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://github.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {{"category1", 90},
                                                            {"category2", 84}};
  history::AnnotatedVisit visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://search.com/"));
  visit2.visit_row.visit_time = base::Time::FromTimeT(3);
  visit2.content_annotations.search_terms = u"search";
  visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit visit4 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          4, GURL("https://github.com/2"));
  visit4.content_annotations.model_annotations.categories = {{"category1", 85},
                                                             {"category3", 82}};
  visit4.content_annotations.has_url_keyed_image = true;
  visit4.visit_row.is_known_to_sync = true;
  cluster1.visits = {history_clusters::testing::CreateClusterVisit(
                         visit, /*normalized_url=*/absl::nullopt, 0.1),
                     history_clusters::testing::CreateClusterVisit(
                         visit2, /*normalized_url=*/absl::nullopt, 1.0),
                     history_clusters::testing::CreateClusterVisit(
                         visit4, /*normalized_url=*/absl::nullopt, 0.3)};

  history::Cluster cluster2 = cluster1;
  // Make the visit time before the first cluster and the first visit have a
  // different visit ID so we can differentiate the two clusters.
  cluster2.visits[1].annotated_visit.visit_row.visit_id = 123;
  cluster2.visits[1].annotated_visit.visit_row.visit_time =
      base::Time::FromTimeT(10);

  auto model_provider = std::make_unique<
      optimization_guide::TestOptimizationGuideModelProvider>();
  base::flat_set<std::string> boost = {};
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      model_provider.get(), boost);
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2});

  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(history_clusters::testing::VisitResult(
                                  123, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(4, 0.3),
                              history_clusters::testing::VisitResult(1, 0.1)),
                  ElementsAre(history_clusters::testing::VisitResult(2, 1.0, {},
                                                                     u"search"),
                              history_clusters::testing::VisitResult(4, 0.3),
                              history_clusters::testing::VisitResult(1, 0.1))));
}

TEST_F(HistoryClustersModuleRankerWithModelTest, ModelAvailable) {
  base::Time now = base::Time::Now();

  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://github.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {
      {"category1", 90}, {"boostedbuthidden", 84}};
  history::AnnotatedVisit visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://search.com/"));
  visit2.visit_row.visit_time = base::Time::FromTimeT(100);
  visit2.content_annotations.search_terms = u"search";
  visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit visit4 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          4, GURL("https://github.com/2"));
  visit4.content_annotations.model_annotations.categories = {{"category1", 85},
                                                             {"category3", 82}};
  visit4.content_annotations.has_url_keyed_image = true;
  visit4.visit_row.is_known_to_sync = true;
  cluster1.visits = {history_clusters::testing::CreateClusterVisit(
                         visit, /*normalized_url=*/absl::nullopt, 0.0),
                     history_clusters::testing::CreateClusterVisit(
                         visit2, /*normalized_url=*/absl::nullopt, 1.0),
                     history_clusters::testing::CreateClusterVisit(
                         visit4, /*normalized_url=*/absl::nullopt, 0.3)};

  history::Cluster cluster2;
  cluster2.cluster_id = 2;
  history::AnnotatedVisit c2_visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          111, GURL("https://github.com/"));
  c2_visit.visit_row.is_known_to_sync = true;
  c2_visit.content_annotations.has_url_keyed_image = true;
  c2_visit.content_annotations.model_annotations.categories = {
      {"category1", 90}, {"boosted", 84}};
  history::AnnotatedVisit c2_visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          222, GURL("https://search.com/"));
  // Pick a time that is 3 minutes ago.
  c2_visit2.visit_row.visit_time = now - base::Minutes(3);
  c2_visit2.content_annotations.search_terms = u"search";
  c2_visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit c2_visit4 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          444, GURL("https://github.com/2"));
  c2_visit4.content_annotations.model_annotations.categories = {
      {"category1", 85}, {"category3", 82}};
  c2_visit4.content_annotations.has_url_keyed_image = true;
  c2_visit4.visit_row.is_known_to_sync = true;
  cluster2.visits = {history_clusters::testing::CreateClusterVisit(
                         c2_visit, /*normalized_url=*/absl::nullopt, 0.8),
                     history_clusters::testing::CreateClusterVisit(
                         c2_visit2, /*normalized_url=*/absl::nullopt, 1.0),
                     history_clusters::testing::CreateClusterVisit(
                         c2_visit4, /*normalized_url=*/absl::nullopt, 0.6)};

  history::Cluster cluster3 = cluster2;
  cluster3.cluster_id = 3;
  for (auto& cluster_visit : cluster3.visits) {
    // Increment the visits to differentiate the cluster.
    cluster_visit.annotated_visit.visit_row.visit_id++;
    // Change the time to be 1 minute ago.
    cluster_visit.annotated_visit.visit_row.visit_time = now - base::Minutes(1);
  }

  base::flat_set<std::string> boost = {"boosted", "boostedbuthidden"};
  auto model_provider = std::make_unique<
      optimization_guide::TestOptimizationGuideModelProvider>();
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      model_provider.get(), boost);
  auto model_handler = std::make_unique<FakeModelHandler>(model_provider.get());
  module_ranker->OverrideModelHandlerForTesting(std::move(model_handler));
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2, cluster3});

  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(history_clusters::testing::VisitResult(
                                  223, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(112, 0.8),
                              history_clusters::testing::VisitResult(445, 0.6)),
                  ElementsAre(history_clusters::testing::VisitResult(
                                  222, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(111, 0.8),
                              history_clusters::testing::VisitResult(444, 0.6)),
                  ElementsAre(history_clusters::testing::VisitResult(2, 1.0, {},
                                                                     u"search"),
                              history_clusters::testing::VisitResult(4, 0.3),
                              history_clusters::testing::VisitResult(1, 0.0))));
}

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

}  // namespace
