// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_METRICS_LOGGER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_METRICS_LOGGER_H_

#include "base/containers/flat_map.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class HistoryClustersModuleRankingSignals;

// Class responsible for logging metrics needed to rank clusters to show in the
// History Clusters module.
class HistoryClustersModuleRankingMetricsLogger {
 public:
  explicit HistoryClustersModuleRankingMetricsLogger(
      ukm::SourceId ukm_source_id);
  virtual ~HistoryClustersModuleRankingMetricsLogger();

  // Adds `ranking_signals` to be logged.
  void AddSignals(base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
                      ranking_signals);

  // Record metrics stored by `this` attached to `ukm_source_id_`.
  void RecordUkm(bool record_in_cluster_id_order);

 private:
  // The UKM source ID associated with this logger.
  const ukm::SourceId ukm_source_id_;

  // A map from cluster ID to ranking signals for that cluster.
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals> ranking_signals_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_METRICS_LOGGER_H_
