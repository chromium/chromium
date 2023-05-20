// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"

#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/test/base/testing_profile.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/optimization_guide/core/test_optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search/ntp_features.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_handler.h"
#endif

namespace {

using ::testing::ElementsAre;

class MockCartService : public CartService {
 public:
  explicit MockCartService(Profile* profile) : CartService(profile) {}

  MOCK_METHOD1(LoadAllActiveCarts, void(CartDB::LoadCallback callback));
};

class HistoryClustersModuleRankerTest : public testing::Test {
 public:
  HistoryClustersModuleRankerTest() = default;
  ~HistoryClustersModuleRankerTest() override = default;

  void SetUp() override {
    testing::Test::SetUp();

    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        HistoryServiceFactory::GetInstance(),
        HistoryServiceFactory::GetDefaultFactory());
    testing_profile_ = profile_builder.Build();

    mock_cart_service_ =
        std::make_unique<MockCartService>(testing_profile_.get());
  }

  std::vector<history::Cluster> RankClusters(
      HistoryClustersModuleRanker* ranker,
      std::vector<history::Cluster> in_clusters,
      base::flat_map<int64_t, HistoryClustersModuleRankingSignals>*
          out_ranking_signals) {
    // Within each cluster, sort visits.
    for (auto& cluster : in_clusters) {
      history_clusters::StableSortVisits(cluster.visits);
    }

    std::vector<history::Cluster> clusters;
    base::RunLoop run_loop;
    ranker->RankClusters(
        std::move(in_clusters),
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::vector<history::Cluster>* out_clusters,
               base::flat_map<int64_t, HistoryClustersModuleRankingSignals>*
                   out_ranking_signals,
               std::vector<history::Cluster> clusters,
               base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
                   ranking_signals) {
              *out_clusters = std::move(clusters);
              *out_ranking_signals = std::move(ranking_signals);
              run_loop->Quit();
            },
            &run_loop, &clusters, out_ranking_signals));

    run_loop.Run();
    return clusters;
  }

  MockCartService& mock_cart_service() { return *mock_cart_service_; }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> testing_profile_;
  std::unique_ptr<MockCartService> mock_cart_service_;
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
  cluster2.cluster_id = 2;
  // Make the visit time before the first cluster and the first visit have a
  // different visit ID so we can differentiate the two clusters.
  cluster2.visits[1].annotated_visit.visit_row.visit_id = 123;
  cluster2.visits[1].annotated_visit.visit_row.visit_time =
      base::Time::FromTimeT(10);
  // Drop all images to differentiate.
  for (auto& cluster_visit : cluster2.visits) {
    cluster_visit.annotated_visit.visit_row.is_known_to_sync = false;
    cluster_visit.annotated_visit.content_annotations.has_url_keyed_image =
        false;
  }

  base::flat_set<std::string> boost = {};
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      /*optimization_guide_model_provider=*/nullptr, /*cart_service=*/nullptr,
      boost);
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals;
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2}, &ranking_signals);

  // Order is [2, 1].
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

  ASSERT_EQ(ranking_signals.size(), 2u);
  EXPECT_TRUE(ranking_signals.contains(1));
  EXPECT_TRUE(ranking_signals.contains(2));
  // Just check the visits with image since that is the differentiator in
  // signals between the two clusters.
  // Cluster ID 1 has 2 images.
  EXPECT_EQ(ranking_signals[1].num_visits_with_image, 2u);
  // Cluster ID 2 has 0 images.
  EXPECT_EQ(ranking_signals[2].num_visits_with_image, 0u);
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
    // Drop images.
    cluster_visit.annotated_visit.visit_row.is_known_to_sync = false;
    cluster_visit.annotated_visit.content_annotations.has_url_keyed_image =
        false;
  }

  base::flat_set<std::string> boost = {"boosted", "boostedbuthidden"};
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      /*optimization_guide_model_provider=*/nullptr, /*cart_service=*/nullptr,
      boost);
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals;
  std::vector<history::Cluster> clusters = RankClusters(
      module_ranker.get(), {cluster1, cluster2, cluster3}, &ranking_signals);

  // The second and third clusters should be picked since it contains a boosted
  // category even though they were earlier than the first cluster and the
  // visits should be sorted according to score. Tiebreaker between multiple
  // clusters is still time.
  // Ordered as [2, 3, 1].
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

  ASSERT_EQ(ranking_signals.size(), 3u);
  ASSERT_TRUE(ranking_signals.contains(1));
  ASSERT_TRUE(ranking_signals.contains(2));
  ASSERT_TRUE(ranking_signals.contains(3));
  // Just check the differentiators between each of the clusters.
  // Cluster ID 1 does not have boosted category.
  EXPECT_FALSE(ranking_signals[1].belongs_to_boosted_category);
  // Cluster ID 2 has 2 images and has boosted category.
  EXPECT_EQ(ranking_signals[2].num_visits_with_image, 2u);
  EXPECT_TRUE(ranking_signals[2].belongs_to_boosted_category);
  // Cluster ID 3 has boosted category and 0 images.
  EXPECT_EQ(ranking_signals[3].num_visits_with_image, 0u);
  EXPECT_TRUE(ranking_signals[3].belongs_to_boosted_category);
}

class HistoryClustersModuleRankerCartTest
    : public HistoryClustersModuleRankerTest {
 public:
  HistoryClustersModuleRankerCartTest() {
    scoped_feature_list_.InitWithFeatures({ntp_features::kNtpChromeCartModule},
                                          {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(HistoryClustersModuleRankerCartTest,
       TestCartMetricsRecordedWithoutModel) {
  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://amazon.com/"));
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
  auto& cart_service = mock_cart_service();
  cart_db::ChromeCartContentProto cart_proto;
  std::vector<CartDB::KeyAndValue> carts = {{"amazon.com", cart_proto}};
  EXPECT_CALL(cart_service, LoadAllActiveCarts(base::test::IsNotNullCallback()))
      .WillOnce(testing::WithArgs<0>(
          testing::Invoke([&carts](CartDB::LoadCallback callback) -> void {
            std::move(callback).Run(true, carts);
          })));
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals;
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      /*optimization_guide_model_provider=*/nullptr, &cart_service, boost);
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2}, &ranking_signals);

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

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.HistoryClusters.CartAssociationStatus",
      commerce::CartHistoryClusterAssociationStatus::kAssociatedWithTopCluster,
      1);
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)

class FakeModelHandler : public HistoryClustersModuleRankingModelHandler {
 public:
  explicit FakeModelHandler(
      optimization_guide::OptimizationGuideModelProvider* provider)
      : HistoryClustersModuleRankingModelHandler(provider) {}
  ~FakeModelHandler() override = default;

  bool CanExecuteAvailableModel() override { return true; }

  void ExecuteBatch(std::vector<HistoryClustersModuleRankingSignals>* inputs,
                    ExecuteBatchCallback callback) override {
    CHECK(inputs);
    std::vector<float> outputs;
    outputs.reserve(inputs->size());
    for (const auto& input : *inputs) {
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
    scoped_feature_list_.InitWithFeatures(
        {ntp_features::kNtpHistoryClustersModuleUseModelRanking,
         ntp_features::kNtpChromeCartModule},
        {});
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
  cluster2.cluster_id = 2;
  // Make the visit time before the first cluster and the first visit have a
  // different visit ID so we can differentiate the two clusters.
  cluster2.visits[1].annotated_visit.visit_row.visit_id = 123;
  cluster2.visits[1].annotated_visit.visit_row.visit_time =
      base::Time::FromTimeT(10);
  // Drop all images to differentiate.
  for (auto& cluster_visit : cluster2.visits) {
    cluster_visit.annotated_visit.visit_row.is_known_to_sync = false;
    cluster_visit.annotated_visit.content_annotations.has_url_keyed_image =
        false;
  }

  auto model_provider = std::make_unique<
      optimization_guide::TestOptimizationGuideModelProvider>();
  base::flat_set<std::string> boost = {};
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      model_provider.get(), /*cart_service=*/nullptr, boost);
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals;
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2}, &ranking_signals);

  // Ordered as [2, 1].
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

  ASSERT_EQ(ranking_signals.size(), 2u);
  EXPECT_TRUE(ranking_signals.contains(1));
  EXPECT_TRUE(ranking_signals.contains(2));
  // Just check the visits with image since that is the differentiator in
  // signals between the two clusters.
  // Cluster ID 1 has 2 images.
  EXPECT_EQ(ranking_signals[1].num_visits_with_image, 2u);
  // Cluster ID 2 has 0 images.
  EXPECT_EQ(ranking_signals[2].num_visits_with_image, 0u);
}

TEST_F(HistoryClustersModuleRankerWithModelTest, ModelAvailable) {
  base::Time now = base::Time::Now();

  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://amazon.com/"));
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
    // Drop images.
    cluster_visit.annotated_visit.visit_row.is_known_to_sync = false;
    cluster_visit.annotated_visit.content_annotations.has_url_keyed_image =
        false;
  }

  base::flat_set<std::string> boost = {"boosted", "boostedbuthidden"};
  auto model_provider = std::make_unique<
      optimization_guide::TestOptimizationGuideModelProvider>();
  auto& cart_service = mock_cart_service();
  cart_db::ChromeCartContentProto cart_proto;
  std::vector<CartDB::KeyAndValue> carts = {{"amazon.com", cart_proto}};
  EXPECT_CALL(cart_service, LoadAllActiveCarts(base::test::IsNotNullCallback()))
      .WillOnce(testing::WithArgs<0>(
          testing::Invoke([&carts](CartDB::LoadCallback callback) -> void {
            std::move(callback).Run(true, carts);
          })));
  auto module_ranker = std::make_unique<HistoryClustersModuleRanker>(
      model_provider.get(), &cart_service, boost);
  auto model_handler = std::make_unique<FakeModelHandler>(model_provider.get());
  module_ranker->OverrideModelHandlerForTesting(std::move(model_handler));
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals;
  std::vector<history::Cluster> clusters = RankClusters(
      module_ranker.get(), {cluster1, cluster2, cluster3}, &ranking_signals);

  // Ordered as [3, 2, 1].
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

  histogram_tester_.ExpectBucketCount(
      "NewTabPage.HistoryClusters.CartAssociationStatus",
      commerce::CartHistoryClusterAssociationStatus::
          kAssociatedWithNonTopCluster,
      1);

  ASSERT_EQ(ranking_signals.size(), 3u);
  ASSERT_TRUE(ranking_signals.contains(1));
  ASSERT_TRUE(ranking_signals.contains(2));
  ASSERT_TRUE(ranking_signals.contains(3));
  // Just check the differentiators between each of the clusters.
  // Cluster ID 1 does not have boosted category.
  EXPECT_FALSE(ranking_signals[1].belongs_to_boosted_category);
  // Cluster ID 2 has 2 images and has boosted category.
  EXPECT_EQ(ranking_signals[2].num_visits_with_image, 2u);
  EXPECT_TRUE(ranking_signals[2].belongs_to_boosted_category);
  // Cluster ID 3 has boosted category and 0 images.
  EXPECT_EQ(ranking_signals[3].num_visits_with_image, 0u);
  EXPECT_TRUE(ranking_signals[3].belongs_to_boosted_category);
}

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

}  // namespace
