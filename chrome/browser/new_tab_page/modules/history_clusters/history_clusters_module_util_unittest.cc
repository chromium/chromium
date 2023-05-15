// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"

#include "components/history_clusters/core/clustering_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::ElementsAre;

using HistoryClustersModuleUtilTest = testing::Test;

TEST(HistoryClustersModuleUtilTest, RecencyOnly) {
  history::Cluster cluster1;
  cluster1.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://github.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.visit_row.visit_time = base::Time::Now() - base::Minutes(3);
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {{"category1", 90},
                                                            {"category2", 84}};
  history::AnnotatedVisit visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://search.com/"));
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
  cluster2.visits[0].annotated_visit.visit_row.visit_time =
      base::Time::Now() - base::Minutes(1);
  cluster2.visits[0].annotated_visit.visit_row.visit_id = 123;

  base::flat_set<std::string> boost = {};
  std::vector<history::Cluster> clusters = {cluster1, cluster2};
  SortClustersUsingHeuristic(boost, clusters);

  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(history_clusters::testing::VisitResult(123, 0.1),
                              history_clusters::testing::VisitResult(2, 1.0, {},
                                                                     u"search"),
                              history_clusters::testing::VisitResult(4, 0.3)),
                  ElementsAre(history_clusters::testing::VisitResult(1, 0.1),
                              history_clusters::testing::VisitResult(2, 1.0, {},
                                                                     u"search"),
                              history_clusters::testing::VisitResult(4, 0.3))));
}

TEST(HistoryClustersModuleUtilTest, WithCategoryBoosting) {
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
  c2_visit.visit_row.visit_time = base::Time::Now() - base::Minutes(3);
  c2_visit.content_annotations.has_url_keyed_image = true;
  c2_visit.content_annotations.model_annotations.categories = {
      {"category1", 90}, {"boosted", 84}};
  history::AnnotatedVisit c2_visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          222, GURL("https://search.com/"));
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
        base::Time::Now() - base::Hours(1);
  }

  base::flat_set<std::string> boost = {"boosted", "boostedbuthidden"};
  std::vector<history::Cluster> clusters = {cluster1, cluster2, cluster3};
  SortClustersUsingHeuristic(boost, clusters);

  // The second and third clusters should be picked since it contains a boosted
  // category even though they were earlier than the first cluster and the
  // visits should be sorted according to score. Tiebreaker between multiple
  // clusters is still time.
  EXPECT_THAT(
      history_clusters::testing::ToVisitResults(clusters),
      ElementsAre(ElementsAre(history_clusters::testing::VisitResult(111, 0.8),
                              history_clusters::testing::VisitResult(
                                  222, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(444, 0.6)),
                  ElementsAre(history_clusters::testing::VisitResult(112, 0.8),
                              history_clusters::testing::VisitResult(
                                  223, 1.0, {}, u"search"),
                              history_clusters::testing::VisitResult(445, 0.6)),
                  ElementsAre(history_clusters::testing::VisitResult(1, 0.0),
                              history_clusters::testing::VisitResult(2, 1.0, {},
                                                                     u"search"),
                              history_clusters::testing::VisitResult(4, 0.3))));
}

}  // namespace
