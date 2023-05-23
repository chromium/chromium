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
  ranking_signals_.insert(ranking_signals.begin(), ranking_signals.end());
}

void HistoryClustersModuleRankingMetricsLogger::RecordUkm(
    bool record_in_cluster_id_order) {
  if (!record_in_cluster_id_order) {
    // Record in whatever order if we just care to log the signals.
    for (auto& ranking_signals : ranking_signals_) {
      ukm::builders::NewTabPage_HistoryClusters builder(ukm_source_id_);
      ranking_signals.second.PopulateUkmEntry(&builder);
      builder.Record(ukm::UkmRecorder::Get());
    }
    return;
  }

  // Otherwise, sort by cluster ID before logging the records.
  std::vector<std::tuple<int64_t, HistoryClustersModuleRankingSignals>>
      ranking_signals_vector;
  ranking_signals_vector.reserve(ranking_signals_.size());
  for (const auto& ranking_signals : ranking_signals_) {
    ranking_signals_vector.emplace_back(ranking_signals.first,
                                        ranking_signals.second);
  }
  base::ranges::stable_sort(
      ranking_signals_vector, [&](const auto& rs1, const auto& rs2) {
        return std::get<int64_t>(rs1) < std::get<int64_t>(rs2);
      });
  for (const auto& ranking_signals : ranking_signals_vector) {
    ukm::builders::NewTabPage_HistoryClusters builder(ukm_source_id_);
    std::get<HistoryClustersModuleRankingSignals>(ranking_signals)
        .PopulateUkmEntry(&builder);
    builder.Record(ukm::UkmRecorder::Get());
  }
}
