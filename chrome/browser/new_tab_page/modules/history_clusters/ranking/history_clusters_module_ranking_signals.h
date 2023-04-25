// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_SIGNALS_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_SIGNALS_H_

#include "base/containers/flat_set.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"

// The signals used to rank clusters for the history clusters module.
class HistoryClustersModuleRankingSignals {
 public:
  static constexpr int kClientVersion = 1;

  // Creates signals from `cluster`.
  HistoryClustersModuleRankingSignals(
      const base::flat_set<std::string>& category_boostlist,
      const history::Cluster& cluster);
  HistoryClustersModuleRankingSignals();
  ~HistoryClustersModuleRankingSignals();
  HistoryClustersModuleRankingSignals(
      const HistoryClustersModuleRankingSignals&);

  // Duration since cluster's most recent visit.
  base::TimeDelta duration_since_most_recent_visit;
  // Whether the cluster is of a boosted category.
  bool belongs_to_boosted_category = false;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_SIGNALS_H_
