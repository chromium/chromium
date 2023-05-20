// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_metrics_logger.h"

#include "base/test/task_environment.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "components/ukm/test_ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class HistoryClustersModuleRankingMetricsLoggerTest : public testing::Test {
 public:
  HistoryClustersModuleRankingMetricsLoggerTest() = default;
  ~HistoryClustersModuleRankingMetricsLoggerTest() override = default;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(HistoryClustersModuleRankingMetricsLoggerTest, E2E) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;

  HistoryClustersModuleRankingSignals signals1;
  signals1.duration_since_most_recent_visit = base::Minutes(2);
  signals1.belongs_to_boosted_category = false;
  signals1.num_visits_with_image = 3;
  signals1.num_total_visits = 4;
  signals1.num_unique_hosts = 2;
  signals1.num_abandoned_carts = 1;

  HistoryClustersModuleRankingSignals signals2;
  signals2.duration_since_most_recent_visit = base::Minutes(5);
  signals2.belongs_to_boosted_category = true;
  signals2.num_visits_with_image = 2;
  signals2.num_total_visits = 10;
  signals2.num_unique_hosts = 3;
  signals2.num_abandoned_carts = 0;

  HistoryClustersModuleRankingSignals should_not_be_logged;
  should_not_be_logged.duration_since_most_recent_visit = base::Minutes(100);
  should_not_be_logged.belongs_to_boosted_category = false;
  should_not_be_logged.num_visits_with_image = 100;
  should_not_be_logged.num_total_visits = 100;
  should_not_be_logged.num_unique_hosts = 300;
  should_not_be_logged.num_abandoned_carts = 100;

  HistoryClustersModuleRankingMetricsLogger logger(ukm::NoURLSourceId());
  logger.AddSignals({{1, signals1}, {2, signals2}, {3, should_not_be_logged}});
  logger.SetClicked(/*cluster_id=*/1);
  logger.SetLayoutTypeShown(ntp::history_clusters::mojom::LayoutType::kLayout1,
                            /*cluster_id=*/1);
  logger.SetLayoutTypeShown(ntp::history_clusters::mojom::LayoutType::kLayout2,
                            /*cluster_id=*/2);
  logger.RecordUkm(/*record_in_cluster_id_order=*/true);

  // There should one entry per shown cluster.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::NewTabPage_HistoryClusters::kEntryName);
  ASSERT_EQ(entries.size(), 2u);
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
      3);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumTotalVisitsName, 4);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumUniqueHostsName, 2);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kNumAbandonedCartsName,
      1);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::NewTabPage_HistoryClusters::kDidEngageWithModuleName, 1);
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::NewTabPage_HistoryClusters::kLayoutTypeShownName,
      1);

  auto* entry2 = entries[1];
  test_ukm_recorder.EntryHasMetric(entry2,
                                   ukm::builders::NewTabPage_HistoryClusters::
                                       kMinutesSinceMostRecentVisitName);
  test_ukm_recorder.ExpectEntryMetric(
      entry2,
      ukm::builders::NewTabPage_HistoryClusters::kBelongsToBoostedCategoryName,
      1);
  test_ukm_recorder.ExpectEntryMetric(
      entry2,
      ukm::builders::NewTabPage_HistoryClusters::kNumVisitsWithImageName, 2);
  test_ukm_recorder.ExpectEntryMetric(
      entry2, ukm::builders::NewTabPage_HistoryClusters::kNumTotalVisitsName,
      10);
  test_ukm_recorder.ExpectEntryMetric(
      entry2, ukm::builders::NewTabPage_HistoryClusters::kNumUniqueHostsName,
      3);
  test_ukm_recorder.ExpectEntryMetric(
      entry2, ukm::builders::NewTabPage_HistoryClusters::kNumAbandonedCartsName,
      0);
  test_ukm_recorder.ExpectEntryMetric(
      entry2,
      ukm::builders::NewTabPage_HistoryClusters::kDidEngageWithModuleName, 0);
  test_ukm_recorder.ExpectEntryMetric(
      entry2, ukm::builders::NewTabPage_HistoryClusters::kLayoutTypeShownName,
      2);
}

}  // namespace
