// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"

#include "base/test/task_environment.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HistoryClustersModuleRankingSignalsTest : public testing::Test {
 public:
  HistoryClustersModuleRankingSignalsTest() = default;
  ~HistoryClustersModuleRankingSignalsTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HistoryClustersModuleRankingSignalsTest, ConstructorNoCartsNoBoost) {
  history::Cluster cluster;
  cluster.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://github.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {{"category1", 90},
                                                            {"boosted", 84}};
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
  cluster.visits = {history_clusters::testing::CreateClusterVisit(
                        visit, /*normalized_url=*/absl::nullopt, 1.0),
                    history_clusters::testing::CreateClusterVisit(
                        visit2, /*normalized_url=*/absl::nullopt, 1.0),
                    history_clusters::testing::CreateClusterVisit(
                        visit4, /*normalized_url=*/absl::nullopt, 0.3)};

  HistoryClustersModuleRankingSignals signals(
      /*active_carts=*/{}, /*category_boostlist=*/{}, cluster);
  EXPECT_GT(signals.duration_since_most_recent_visit.InMinutes(), 0);
  // Even though it says boosted, there is no passed-in boostlist so it's false.
  EXPECT_FALSE(signals.belongs_to_boosted_category);
  EXPECT_EQ(signals.num_visits_with_image, 2u);
  EXPECT_EQ(signals.num_total_visits, 3u);
  // github.com and search.com
  EXPECT_EQ(signals.num_unique_hosts, 2u);
  EXPECT_EQ(signals.num_abandoned_carts, 0u);

  // Verify UKM.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ukm::builders::NewTabPage_HistoryClusters builder(ukm::NoURLSourceId());
  signals.PopulateUkmEntry(&builder);
  builder.Record(ukm::UkmRecorder::Get());

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0];
  test_ukm_recorder.EntryHasMetric(entry,
                                   ukm::builders::NewTabPage_HistoryClusters::
                                       kMinutesSinceMostRecentVisitName);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::NewTabPage_HistoryClusters::kBelongsToBoostedCategoryName,
      0);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumVisitsWithImageName,
      2);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumTotalVisitsName, 3);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumUniqueHostsName, 2);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumAbandonedCartsName,
      0);
}

TEST_F(HistoryClustersModuleRankingSignalsTest, ConstructorHasCartsAndBoost) {
  history::Cluster cluster;
  cluster.cluster_id = 1;
  history::AnnotatedVisit visit =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          1, GURL("https://m.merchant.com/"));
  visit.visit_row.is_known_to_sync = true;
  visit.content_annotations.has_url_keyed_image = true;
  visit.content_annotations.model_annotations.categories = {{"category1", 90},
                                                            {"boosted", 84}};
  history::AnnotatedVisit visit2 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          2, GURL("https://search.com/"));
  visit2.visit_row.visit_time = base::Time::FromTimeT(100);
  visit2.content_annotations.search_terms = u"search";
  visit2.content_annotations.related_searches = {"relsearch1", "relsearch2"};
  history::AnnotatedVisit visit4 =
      history_clusters::testing::CreateDefaultAnnotatedVisit(
          4, GURL("https://www.merchant.com/2"));
  visit4.content_annotations.model_annotations.categories = {{"category1", 85},
                                                             {"category3", 82}};
  visit4.content_annotations.has_url_keyed_image = true;
  visit4.visit_row.is_known_to_sync = true;
  cluster.visits = {history_clusters::testing::CreateClusterVisit(
                        visit, /*normalized_url=*/absl::nullopt, 1.0),
                    history_clusters::testing::CreateClusterVisit(
                        visit2, /*normalized_url=*/absl::nullopt, 1.0),
                    history_clusters::testing::CreateClusterVisit(
                        visit4, /*normalized_url=*/absl::nullopt, 0.3)};

  std::vector<CartDB::KeyAndValue> active_carts = {
      {"merchant.com", cart_db::ChromeCartContentProto::default_instance()},
  };
  base::flat_set<std::string> category_boostlist = {"boosted"};
  HistoryClustersModuleRankingSignals signals(active_carts, category_boostlist,
                                              cluster);
  EXPECT_GT(signals.duration_since_most_recent_visit.InMinutes(), 0);
  EXPECT_TRUE(signals.belongs_to_boosted_category);
  EXPECT_EQ(signals.num_visits_with_image, 2u);
  EXPECT_EQ(signals.num_total_visits, 3u);
  // m.merchant.com, www.merchant.com, and search.com
  EXPECT_EQ(signals.num_unique_hosts, 3u);
  // m.merchant.com and www.merchant.com should both match to merchant.com.
  EXPECT_EQ(signals.num_abandoned_carts, 1u);

  // Verify UKM.
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  ukm::builders::NewTabPage_HistoryClusters builder(ukm::NoURLSourceId());
  signals.PopulateUkmEntry(&builder);
  builder.Record(ukm::UkmRecorder::Get());

  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 1u);
  auto* entry = entries[0];
  test_ukm_recorder.EntryHasMetric(entry,
                                   ukm::builders::NewTabPage_HistoryClusters::
                                       kMinutesSinceMostRecentVisitName);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::NewTabPage_HistoryClusters::kBelongsToBoostedCategoryName,
      1);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumVisitsWithImageName,
      2);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumTotalVisitsName, 3);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumUniqueHostsName, 3);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumAbandonedCartsName,
      1);
}

}  // namespace
