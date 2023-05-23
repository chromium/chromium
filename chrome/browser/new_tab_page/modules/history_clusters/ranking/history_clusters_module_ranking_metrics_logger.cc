// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_metrics_logger.h"

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"

HistoryClustersModuleRankingMetricsLogger::
    HistoryClustersModuleRankingMetricsLogger(ukm::SourceId ukm_source_id)
    : ukm_source_id_(ukm_source_id) {}

HistoryClustersModuleRankingMetricsLogger::
    ~HistoryClustersModuleRankingMetricsLogger() = default;

void HistoryClustersModuleRankingMetricsLogger::AddSignals(
    base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
        ranking_signals) {
  for (const auto& ranking_signal : ranking_signals) {
    ranking_metrics_infos_[ranking_signal.first].ranking_signals =
        ranking_signal.second;
  }
}

void HistoryClustersModuleRankingMetricsLogger::SetClicked(int64_t cluster_id) {
  if (ranking_metrics_infos_.contains(cluster_id)) {
    ranking_metrics_infos_[cluster_id].clicked = true;
  }
}

void HistoryClustersModuleRankingMetricsLogger::SetLayoutTypeShown(
    ntp::history_clusters::mojom::LayoutType layout_type,
    int64_t cluster_id) {
  if (ranking_metrics_infos_.contains(cluster_id)) {
    ranking_metrics_infos_[cluster_id].layout_type = layout_type;
  }
}

void HistoryClustersModuleRankingMetricsLogger::RecordUkm(
    bool record_in_cluster_id_order) {
  if (!record_in_cluster_id_order) {
    // Record in whatever order if we just care to log the signals.
    for (auto& ranking_metrics_info : ranking_metrics_infos_) {
      MaybeRecordRankingMetricsInfo(ranking_metrics_info.second);
    }
    return;
  }

  // Otherwise, sort by cluster ID before logging the records.
  std::vector<std::tuple<int64_t, RankingMetricsInfo>>
      ranking_metrics_info_vector;
  ranking_metrics_info_vector.reserve(ranking_metrics_infos_.size());
  for (const auto& ranking_metrics_info : ranking_metrics_infos_) {
    ranking_metrics_info_vector.emplace_back(ranking_metrics_info.first,
                                             ranking_metrics_info.second);
  }
  base::ranges::stable_sort(
      ranking_metrics_info_vector, [&](const auto& rs1, const auto& rs2) {
        return std::get<int64_t>(rs1) < std::get<int64_t>(rs2);
      });
  for (const auto& ranking_metrics_info : ranking_metrics_info_vector) {
    MaybeRecordRankingMetricsInfo(
        std::get<RankingMetricsInfo>(ranking_metrics_info));
  }
}

void HistoryClustersModuleRankingMetricsLogger::MaybeRecordRankingMetricsInfo(
    const RankingMetricsInfo& ranking_metrics_info) {
  if (ranking_metrics_info.layout_type ==
      ntp::history_clusters::mojom::LayoutType::kNone) {
    return;
  }

  ukm::builders::NewTabPage_HistoryClusters builder(ukm_source_id_);
  ranking_metrics_info.ranking_signals.PopulateUkmEntry(&builder);
  builder.SetDidEngageWithModule(ranking_metrics_info.clicked);
  builder.SetLayoutTypeShown(
      static_cast<int64_t>(ranking_metrics_info.layout_type));
  builder.Record(ukm::UkmRecorder::Get());
}
