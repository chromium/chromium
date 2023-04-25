// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search/ntp_features.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_handler.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#endif

HistoryClustersModuleRanker::HistoryClustersModuleRanker(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    const base::flat_set<std::string>& category_boostlist)
    : category_boostlist_(category_boostlist) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (model_provider) {
    model_handler_ = std::make_unique<HistoryClustersModuleRankingModelHandler>(
        model_provider);
  }
#endif
}

HistoryClustersModuleRanker::~HistoryClustersModuleRanker() = default;

void HistoryClustersModuleRanker::RankClusters(
    std::vector<history::Cluster> clusters,
    ClustersCallback callback) {
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (model_handler_ && model_handler_->CanExecuteAvailableModel()) {
    std::vector<HistoryClustersModuleRankingSignals> ranking_signals;
    ranking_signals.reserve(clusters.size());
    for (const auto& cluster : clusters) {
      ranking_signals.emplace_back(category_boostlist_, cluster);
    }
    model_handler_->ExecuteBatch(
        ranking_signals,
        base::BindOnce(
            &HistoryClustersModuleRanker::OnBatchModelExecutionComplete,
            weak_ptr_factory_.GetWeakPtr(), std::move(clusters),
            std::move(callback)));
    return;
  }
#endif

  RunFallbackHeuristic(std::move(clusters), std::move(callback));
}

void HistoryClustersModuleRanker::RunFallbackHeuristic(
    std::vector<history::Cluster> clusters,
    ClustersCallback callback) {
  SortClustersUsingHeuristic(category_boostlist_, clusters);

  std::move(callback).Run(std::move(clusters));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)

void HistoryClustersModuleRanker::OverrideModelHandlerForTesting(
    std::unique_ptr<HistoryClustersModuleRankingModelHandler> model_handler) {
  model_handler_ = std::move(model_handler);
}

void HistoryClustersModuleRanker::OnBatchModelExecutionComplete(
    std::vector<history::Cluster> clusters,
    ClustersCallback callback,
    std::vector<float> outputs) {
  CHECK_EQ(clusters.size(), outputs.size());

  // Sort clusters by model score.
  std::vector<std::tuple<history::Cluster, float>> clusters_with_scores;
  clusters_with_scores.reserve(clusters.size());
  for (size_t i = 0; i < clusters.size(); i++) {
    clusters_with_scores.emplace_back(std::move(clusters[i]), outputs[i]);
  }
  base::ranges::stable_sort(clusters_with_scores,
                            [](const auto& c1, const auto& c2) {
                              return std::get<float>(c1) < std::get<float>(c2);
                            });

  // Cull clusters based on how many we need.
  std::vector<history::Cluster> output_clusters;
  for (auto& cluster_and_score : clusters_with_scores) {
    output_clusters.push_back(
        std::move(std::get<history::Cluster>(cluster_and_score)));
  }
  std::move(callback).Run(std::move(output_clusters));
}

#endif
