// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_SIGNALS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_SIGNALS_H_

#include <vector>

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_db.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"
#include "components/history/core/browser/history_types.h"

struct HistoryClustersCategoryMetrics;

namespace ukm::builders {
class NewTabPage_HistoryClusters;
}  // namespace ukm::builders

struct HistoryClusterMetrics;

// The signals used to rank clusters for the history clusters module.
class HistoryClustersModuleRankingSignals {
 public:
  static constexpr int kClientVersion = 2;

  // Creates signals from `cluster`.
  HistoryClustersModuleRankingSignals(
      const std::vector<CartDB::KeyAndValue>& active_carts,
      const base::flat_set<std::string>& category_boostlist,
      const history::Cluster& cluster,
      const HistoryClusterMetrics& metrics,
      const HistoryClustersCategoryMetrics& category_metrics);

  HistoryClustersModuleRankingSignals();
  ~HistoryClustersModuleRankingSignals();
  HistoryClustersModuleRankingSignals(
      const HistoryClustersModuleRankingSignals&);

  // Populates UKM entry with data from `this`.
  void PopulateUkmEntry(
      ukm::builders::NewTabPage_HistoryClusters* ukm_entry_builder) const;

  // Duration since cluster's most recent visit.
  base::TimeDelta duration_since_most_recent_visit;
  // Whether the cluster is of a boosted category.
  bool belongs_to_boosted_category = false;
  // The number of visits that have an image.
  size_t num_visits_with_image = 0;
  // The number of total visits in the cluster including ones that are not
  // necessarily shown in the module.
  size_t num_total_visits = 0;
  // The number of unique hosts represented in the cluster.
  size_t num_unique_hosts = 0;
  // The number of abandoned carts associated with the cluster.
  size_t num_abandoned_carts = 0;
  // The number of times the cluster was seen by the user in the last 24 hours.
  size_t num_times_seen_last_24h = 0;
  // The number of times the cluster was used by the user in the last 24 hours.
  size_t num_times_used_last_24h = 0;
  // The number of categories associated with the cluster.
  size_t num_associated_categories = 0;
  // Whether the cluster is of the most frequently seen category in the last 24
  // hours.
  bool belongs_to_most_seen_category = false;
  // Whether the cluster is of the most frequently engaged category in the last
  // 24 hours.
  bool belongs_to_most_used_category = false;
  // The number of times the most frequently seen category has been seen in the
  // last 24 hours.
  size_t most_frequent_category_seen_count_last_24h = 0;
  // The number of times the most frequentluy used category has been used in the
  // last 24 hours.
  size_t most_frequent_category_used_count_last_24h = 0;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_SIGNALS_H_
