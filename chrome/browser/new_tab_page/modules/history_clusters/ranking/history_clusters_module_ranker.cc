// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"

#include <iterator>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_db.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_cluster_metrics.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_category_metrics.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_metrics_logger.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/search/ntp_features.h"
#include "components/segmentation_platform/embedder/default_model/database_api_clients.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_handler.h"
#endif

const char kHistoryClusterSeenEventName[] = "NewTabPage.HistoryClusters.Seen";
const char kHistoryClusterUsedEventName[] = "NewTabPage.HistoryClusters.Used";
const char kHistoryClustersSeenCategoriesEventName[] =
    "NewTabPage.HistoryClusters.SeenCategories";
const char kHistoryClustersUsedCategoriesEventName[] =
    "NewTabPage.HistoryClusters.UsedCategories";

namespace {

// Returns a pair consisting of the most frequent names (in case of a tie), and
// their associated count.
std::pair<std::set<std::string>, size_t> GetMostFrequent(
    const std::set<std::string>& names,
    const std::vector<float>& counts) {
  CHECK_EQ(names.size(), counts.size());

  int most_frequent_count = 0;
  std::set<std::string> most_frequent;
  int i = 0;
  for (const auto& name : names) {
    int count = floor(counts[i]);
    if (count > most_frequent_count) {
      most_frequent_count = counts[i];
      most_frequent.clear();
    }
    if (count == most_frequent_count) {
      most_frequent.insert(name);
    }
    i++;
  }

  return {std::move(most_frequent), static_cast<size_t>(most_frequent_count)};
}

}  // namespace

HistoryClustersModuleRanker::HistoryClustersModuleRanker(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    segmentation_platform::SegmentationPlatformService*
        segmentation_platform_service,
    history::HistoryService* history_service,
    CartService* cart_service,
    const base::flat_set<std::string>& category_boostlist)
    : segmentation_platform_service_(segmentation_platform_service),
      history_service_(history_service),
      cart_service_(cart_service),
      category_boostlist_(category_boostlist),
      query_day_count_(GetFieldTrialParamByFeatureAsInt(
          ntp_features::kNtpHistoryClustersModuleRankingMetricsQueryDays,
          ntp_features::kNtpHistoryClustersModuleRankingMetricsQueryDaysParam,
          1)),
      threshold_param_((float)GetFieldTrialParamByFeatureAsDouble(
          ntp_features::kNtpHistoryClustersModule,
          ntp_features::kNtpHistoryClustersModuleScoreThresholdParam,
          0)) {
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
  DCHECK(!clusters.empty());

  history::QueryOptions query_options;
  query_options.end_time = base::Time::Now();
  query_options.begin_time =
      query_options.end_time - base::Days(query_day_count_);
  history_service_->GetAnnotatedVisits(
      query_options, false,
      base::BindOnce(&HistoryClustersModuleRanker::OnGotAnnotatedVisits,
                     weak_ptr_factory_.GetWeakPtr(), std::move(clusters),
                     std::move(callback)),
      &task_tracker_);
}

void HistoryClustersModuleRanker::OnGotAnnotatedVisits(
    std::vector<history::Cluster> clusters,
    ClustersCallback callback,
    const std::vector<history::AnnotatedVisit> annotated_visits) {
  auto metrics_ready_callback =
      base::BindOnce(&HistoryClustersModuleRanker::OnMetricsReady,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
  if (annotated_visits.empty()) {
    OnMetricsQueryDataReady(
        std::move(metrics_ready_callback), std::move(clusters),
        /*category_ids=*/{},
        segmentation_platform::DatabaseClient::ResultStatus::kError, {});
    return;
  }
  if (!segmentation_platform_service_) {
    OnMetricsQueryDataReady(
        std::move(metrics_ready_callback), std::move(clusters),
        /*category_ids=*/{},
        segmentation_platform::DatabaseClient::ResultStatus::kError, {});
    return;
  }
  segmentation_platform::DatabaseClient* client =
      segmentation_platform_service_->GetDatabaseClient();
  if (!client) {
    OnMetricsQueryDataReady(
        std::move(metrics_ready_callback), std::move(clusters),
        /*category_ids=*/{},
        segmentation_platform::DatabaseClient::ResultStatus::kError, {});
    return;
  }

  std::set<std::string> cluster_ids;
  std::ranges::transform(clusters,
                         std::inserter(cluster_ids, cluster_ids.end()),
                         [](const history::Cluster& cluster) {
                           return base::NumberToString(cluster.cluster_id);
                         });
  // A non-empty cluster ids set is required for the below query logic to work
  // correctly.
  segmentation_platform::proto::SegmentationModelMetadata metadata;
  segmentation_platform::MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig();
  segmentation_platform::DatabaseApiClients::AddSumGroupQuery(
      writer, kHistoryClusterSeenEventName, cluster_ids, query_day_count_);
  segmentation_platform::DatabaseApiClients::AddSumGroupQuery(
      writer, kHistoryClusterUsedEventName, cluster_ids, query_day_count_);

  std::set<std::string> category_ids;
  for (const auto& annotated_visit : annotated_visits) {
    for (const auto& category :
         annotated_visit.content_annotations.model_annotations.categories) {
      category_ids.insert(category.id);
    }
  }
  if (!category_ids.empty()) {
    segmentation_platform::DatabaseApiClients::AddSumGroupQuery(
        writer, kHistoryClustersSeenCategoriesEventName, category_ids,
        query_day_count_);
    segmentation_platform::DatabaseApiClients::AddSumGroupQuery(
        writer, kHistoryClustersUsedCategoriesEventName, category_ids,
        query_day_count_);
  }

  // The resulting vector of floats produced by the queries specified in the
  // `metadata` above is guaranteed to be of size equal to 2 * the size of the
  // `cluster_ids` set + 2 * the size of the `category_ids` set.
  client->ProcessFeatures(
      metadata, base::Time::Now(),
      base::BindOnce(&HistoryClustersModuleRanker::OnMetricsQueryDataReady,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(metrics_ready_callback), std::move(clusters),
                     std::move(category_ids)));
}

void HistoryClustersModuleRanker::OnMetricsQueryDataReady(
    base::OnceCallback<void(std::vector<history::Cluster>,
                            std::vector<HistoryClusterMetrics>,
                            HistoryClustersCategoryMetrics)> callback,
    std::vector<history::Cluster> clusters,
    const std::set<std::string> category_ids,
    segmentation_platform::DatabaseClient::ResultStatus status,
    const segmentation_platform::ModelProvider::Request& result) {
  if (status != segmentation_platform::DatabaseClient::ResultStatus::kSuccess) {
    std::move(callback).Run(std::move(clusters), {}, {});
    return;
  }

  // The result vector size should match the expected sizes for the 4 queries
  // specified above as part of the metadata. The sum group query is guaranteed
  // to produce as many vector entries as `metric_names` are provided as input.
  CHECK_EQ(result.size(), (2 * clusters.size() + 2 * category_ids.size()));
  // Split the single query results vector into multiple vectors, one
  // for each of the requested queries.
  auto seen_counts =
      std::vector<float>(result.begin(), result.begin() + clusters.size());
  auto used_counts =
      std::vector<float>(result.begin() + clusters.size(), result.end());
  std::vector<HistoryClusterMetrics> metrics;
  for (size_t i = 0; i < clusters.size(); i++) {
    HistoryClusterMetrics cluster_metrics;
    cluster_metrics.num_times_seen = seen_counts.at(i);
    cluster_metrics.num_times_used = used_counts.at(i);
    metrics.push_back(std::move(cluster_metrics));
  }

  if (category_ids.empty()) {
    std::move(callback).Run(std::move(clusters), std::move(metrics), {});
    return;
  }

  auto category_frequencies_it = result.begin() + 2 * clusters.size();
  auto most_frequently_seen = GetMostFrequent(
      category_ids,
      std::vector<float>(category_frequencies_it,
                         category_frequencies_it + category_ids.size()));
  auto most_frequently_used = GetMostFrequent(
      category_ids,
      std::vector<float>(category_frequencies_it + category_ids.size(),
                         result.end()));
  HistoryClustersCategoryMetrics category_metrics = {
      std::move(most_frequently_seen.first),
      most_frequently_seen.second,
      std::move(most_frequently_used.first),
      most_frequently_used.second,
  };

  std::move(callback).Run(std::move(clusters), std::move(metrics),
                          std::move(category_metrics));
}

void HistoryClustersModuleRanker::OnMetricsReady(
    ClustersCallback callback,
    std::vector<history::Cluster> clusters,
    std::vector<HistoryClusterMetrics> cluster_metrics,
    HistoryClustersCategoryMetrics category_metrics) {
  if (IsCartModuleEnabled() && cart_service_) {
    cart_service_->LoadAllActiveCarts(
        base::BindOnce(&HistoryClustersModuleRanker::OnAllSignalsReady,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(clusters), std::move(cluster_metrics),
                       std::move(category_metrics)));
  } else {
    OnAllSignalsReady(std::move(callback), std::move(clusters),
                      std::move(cluster_metrics), std::move(category_metrics),
                      /*success=*/false, /*active_carts=*/{});
  }
}

void HistoryClustersModuleRanker::OnAllSignalsReady(
    ClustersCallback callback,
    std::vector<history::Cluster> clusters,
    const std::vector<HistoryClusterMetrics>& cluster_metrics,
    const HistoryClustersCategoryMetrics& category_metrics,
    bool success,
    std::vector<CartDB::KeyAndValue> active_carts) {
  auto ranking_signals =
      std::make_unique<std::vector<HistoryClustersModuleRankingSignals>>();
  ranking_signals->reserve(clusters.size());
  for (size_t i = 0; i < clusters.size(); i++) {
    ranking_signals->emplace_back(
        active_carts, category_boostlist_, clusters.at(i),
        !cluster_metrics.empty() ? cluster_metrics.at(i)
                                 : HistoryClusterMetrics(),
        category_metrics);
  }

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  if (model_handler_ && model_handler_->CanExecuteAvailableModel()) {
    auto* ranking_signals_ptr = ranking_signals.get();
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

  std::vector<
      std::tuple<history::Cluster, HistoryClustersModuleRankingSignals, float>>
      clusters_with_scores;

  // Filter clusters by model score.
  for (size_t i = 0; i < clusters.size(); i++) {
    if (outputs[i] <= threshold_param_) {
      clusters_with_scores.emplace_back(std::move(clusters[i]),
                                        std::move(ranking_signals->at(i)),
                                        outputs[i]);
    }
  }

  // Sort clusters by model score. This sort function is reversed and so all
  // models account for this by flipping the sign. i.e. -1 is before -0.5.
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
