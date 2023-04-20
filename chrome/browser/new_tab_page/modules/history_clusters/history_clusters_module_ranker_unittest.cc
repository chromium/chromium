// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_ranker.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

class HistoryClustersModuleRankerTest : public testing::Test {
 public:
  HistoryClustersModuleRankerTest() = default;
  ~HistoryClustersModuleRankerTest() override = default;

  std::vector<history::Cluster> RankClusters(
      HistoryClustersModuleRanker* ranker,
      std::vector<history::Cluster> in_clusters) {
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

TEST_F(HistoryClustersModuleRankerTest, NoMax) {
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
      /*max_clusters_to_return=*/0u, boost);
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

TEST_F(HistoryClustersModuleRankerTest, MaxClustersAppliedNoCategoryBoosting) {
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
      /*max_clusters_to_return=*/static_cast<size_t>(1), boost);
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2});

  // The second cluster should be picked since it's later and the visits should
  // be sorted according to score.
  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(
          history_clusters::testing::VisitResult(123, 1.0, {}, u"search"),
          history_clusters::testing::VisitResult(4, 0.3),
          history_clusters::testing::VisitResult(1, 0.1))));
}

TEST_F(HistoryClustersModuleRankerTest,
       MaxClustersAppliedWithCategoryBoosting) {
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
      /*max_clusters_to_return=*/2, boost);
  std::vector<history::Cluster> clusters =
      RankClusters(module_ranker.get(), {cluster1, cluster2, cluster3});

  // The second and third clusters should be picked since it contains a boosted
  // category even though they were earlier than the first cluster and the
  // visits should be sorted according to score. Tiebreaker between multiple
  // clusters is still time.
  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(
          ElementsAre(
              history_clusters::testing::VisitResult(222, 1.0, {}, u"search"),
              history_clusters::testing::VisitResult(111, 0.8),
              history_clusters::testing::VisitResult(444, 0.6)),
          ElementsAre(
              history_clusters::testing::VisitResult(223, 1.0, {}, u"search"),
              history_clusters::testing::VisitResult(112, 0.8),
              history_clusters::testing::VisitResult(445, 0.6))));
}
