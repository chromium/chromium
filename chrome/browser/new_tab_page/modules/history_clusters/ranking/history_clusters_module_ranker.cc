// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_metrics_logger.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search/ntp_features.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_handler.h"
#endif

HistoryClustersModuleRanker::HistoryClustersModuleRanker(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    CartService* cart_service,
    const base::flat_set<std::string>& category_boostlist)
    : cart_service_(cart_service), category_boostlist_(category_boostlist) {
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
  if (IsCartModuleEnabled() && cart_service_) {
    cart_service_->LoadAllActiveCarts(
        base::BindOnce(&HistoryClustersModuleRanker::OnAllSignalsReady,
                       weak_ptr_factory_.GetWeakPtr(), std::move(clusters),
                       std::move(callback)));
  } else {
    OnAllSignalsReady(std::move(clusters), std::move(callback),
                      /*success=*/false, /*active_carts=*/{});
  }
}

void HistoryClustersModuleRanker::OnAllSignalsReady(
    std::vector<history::Cluster> clusters,
    ClustersCallback callback,
    bool success,
    std::vector<CartDB::KeyAndValue> active_carts) {
  auto ranking_signals =
      std::make_unique<std::vector<HistoryClustersModuleRankingSignals>>();
  ranking_signals->reserve(clusters.size());
  for (const auto& cluster : clusters) {
    ranking_signals->emplace_back(active_carts, category_boostlist_, cluster);
  }
  auto* ranking_signals_ptr = ranking_signals.get();

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (model_handler_ && model_handler_->CanExecuteAvailableModel()) {
    model_handler_->ExecuteBatch(
        ranking_signals_ptr,
        base::BindOnce(
            &HistoryClustersModuleRanker::OnBatchModelExecutionComplete,
            weak_ptr_factory_.GetWeakPtr(), std::move(clusters),
            std::move(ranking_signals), std::move(active_carts),
            std::move(callback)));
    return;
  }
#endif

  RunFallbackHeuristic(std::move(clusters), std::move(ranking_signals),
                       std::move(active_carts), std::move(callback));
}

void HistoryClustersModuleRanker::RunFallbackHeuristic(
    std::vector<history::Cluster> clusters,
    std::unique_ptr<std::vector<HistoryClustersModuleRankingSignals>>
        ranking_signals,
    std::vector<CartDB::KeyAndValue> active_carts,
    ClustersCallback callback) {
  CHECK_EQ(clusters.size(), ranking_signals->size());

  CartProcessor::RecordCartHistoryClusterAssociationMetrics(active_carts,
                                                            clusters);

  std::vector<std::tuple<history::Cluster, HistoryClustersModuleRankingSignals>>
      ranking_infos;
  ranking_infos.reserve(clusters.size());
  for (size_t i = 0; i < clusters.size(); i++) {
    ranking_infos.emplace_back(std::move(clusters[i]),
                               std::move(ranking_signals->at(i)));
  }
  base::ranges::stable_sort(
      ranking_infos, [this](const auto& c1, const auto& c2) {
        return CompareClustersUsingHeuristic(category_boostlist_,
                                             std::get<history::Cluster>(c1),
                                             std::get<history::Cluster>(c2));
      });

  clusters.clear();
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
      ranking_signals_map;
  for (auto& ranking_info : ranking_infos) {
    auto& cluster = std::get<history::Cluster>(ranking_info);
    ranking_signals_map[cluster.cluster_id] =
        std::move(std::get<HistoryClustersModuleRankingSignals>(ranking_info));
    clusters.emplace_back(std::move(cluster));
  }
  std::move(callback).Run(std::move(clusters), std::move(ranking_signals_map));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)

void HistoryClustersModuleRanker::OverrideModelHandlerForTesting(
    std::unique_ptr<HistoryClustersModuleRankingModelHandler> model_handler) {
  model_handler_ = std::move(model_handler);
}

void HistoryClustersModuleRanker::OnBatchModelExecutionComplete(
    std::vector<history::Cluster> clusters,
    std::unique_ptr<std::vector<HistoryClustersModuleRankingSignals>>
        ranking_signals,
    std::vector<CartDB::KeyAndValue> active_carts,
    ClustersCallback callback,
    std::vector<float> outputs) {
  CHECK_EQ(clusters.size(), ranking_signals->size());
  CHECK_EQ(clusters.size(), outputs.size());

  // Sort clusters by model score.
  std::vector<
      std::tuple<history::Cluster, HistoryClustersModuleRankingSignals, float>>
      clusters_with_scores;
  clusters_with_scores.reserve(clusters.size());
  for (size_t i = 0; i < clusters.size(); i++) {
    clusters_with_scores.emplace_back(
        std::move(clusters[i]), std::move(ranking_signals->at(i)), outputs[i]);
  }
  base::ranges::stable_sort(clusters_with_scores,
                            [](const auto& c1, const auto& c2) {
                              return std::get<float>(c1) < std::get<float>(c2);
                            });

  std::vector<history::Cluster> output_clusters;
  output_clusters.reserve(clusters_with_scores.size());
  base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
      output_ranking_signals;
  output_ranking_signals.reserve(clusters_with_scores.size());
  for (auto& cluster_and_score : clusters_with_scores) {
    auto& cluster = std::get<history::Cluster>(cluster_and_score);
    output_ranking_signals[cluster.cluster_id] = std::move(
        std::get<HistoryClustersModuleRankingSignals>(cluster_and_score));
    output_clusters.push_back(std::move(cluster));
  }

  CartProcessor::RecordCartHistoryClusterAssociationMetrics(active_carts,
                                                            output_clusters);

  std::move(callback).Run(std::move(output_clusters),
                          std::move(output_ranking_signals));
}

#endif
