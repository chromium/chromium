// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_METRICS_LOGGER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_METRICS_LOGGER_H_

#include "base/containers/flat_map.h"

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
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

  // Sets that the module showing cluster with `cluster_id` was clicked. Virtual
  // for testing.
  virtual void SetClicked(int64_t cluster_id);

  // Sets that the module showing cluster with `cluster_id` was disabled.
  // Virtual for testing.
  virtual void SetDisabled(int64_t cluster_id);

  // Sets that the module showing cluster with `cluster_id` was dismissed.
  // Virtual for testing.
  virtual void SetDismissed(int64_t cluster_id);

  // Sets that the module showing cluster with `cluster_id` was displayed using
  // `layout_type`. Virtual for testing.
  virtual void SetLayoutTypeShown(
      ntp::history_clusters::mojom::LayoutType layout_type,
      int64_t cluster_id);

  // Record metrics stored by `this` attached to `ukm_source_id_`.
  void RecordUkm(bool record_in_cluster_id_order);

 private:
  // The UKM source ID associated with this logger.
  const ukm::SourceId ukm_source_id_;

  struct RankingMetricsInfo {
    HistoryClustersModuleRankingSignals ranking_signals;
    bool disabled = false;
    bool dismissed = false;
    bool clicked = false;
    ntp::history_clusters::mojom::LayoutType layout_type =
        ntp::history_clusters::mojom::LayoutType::kNone;
  };

  // Attaches a UKM event for `ranking_metrics_info` to `ukm_source_id_` if
  // `ranking_metrics_info` indicates the cluster was shown to the user.
  void MaybeRecordRankingMetricsInfo(
      const RankingMetricsInfo& ranking_metrics_info);

  // A map from cluster ID to information required to record cluster ranking
  // metrics.
  base::flat_map<int64_t, RankingMetricsInfo> ranking_metrics_infos_;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_METRICS_LOGGER_H_
