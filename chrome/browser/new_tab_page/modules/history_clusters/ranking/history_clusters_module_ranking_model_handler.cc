// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_handler.h"

#include "base/barrier_closure.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_executor.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_metadata.pb.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "components/optimization_guide/proto/models.pb.h"

namespace {

// Constructs the input vector needed for the model according to
// `model_metadata` and `signals`.
std::vector<float> ConstructInputVector(
    const new_tab_page::proto::HistoryClustersModuleRankingModelMetadata&
        model_metadata,
    const HistoryClustersModuleRankingSignals& signals) {
  std::vector<float> input_vector;
  input_vector.reserve(model_metadata.signals_size());
  for (const auto signal : model_metadata.signals()) {
    switch (signal) {
      case new_tab_page::proto::
          HISTORY_CLUSTERS_MODULE_RANKING_MINUTES_SINCE_MOST_RECENT_VISIT:
        input_vector.push_back(static_cast<float>(
            signals.duration_since_most_recent_visit.InMinutes()));
        break;
      case new_tab_page::proto::
          HISTORY_CLUSTERS_MODULE_RANKING_BELONGS_TO_BOOSTED_CATEGORY:
        input_vector.push_back(signals.belongs_to_boosted_category ? 1 : 0);
        break;
      case new_tab_page::proto::
          HISTORY_CLUSTERS_MODULE_RANKING_NUM_VISITS_WITH_IMAGE:
        input_vector.push_back(
            static_cast<float>(signals.num_visits_with_image));
        break;
      case new_tab_page::proto::
          HISTORY_CLUSTERS_MODULE_RANKING_NUM_TOTAL_VISITS:
        input_vector.push_back(static_cast<float>(signals.num_total_visits));
        break;
      case new_tab_page::proto::
          HISTORY_CLUSTERS_MODULE_RANKING_NUM_UNIQUE_HOSTS:
        input_vector.push_back(static_cast<float>(signals.num_unique_hosts));
        break;
      case new_tab_page::proto::
          HISTORY_CLUSTERS_MODULE_RANKING_NUM_ABANDONED_CARTS:
        input_vector.push_back(static_cast<float>(signals.num_abandoned_carts));
        break;
      default:
        NOTREACHED();
    }
  }
  return input_vector;
}

void OnSingleExecutionComplete(std::vector<float>* outputs,
                               base::OnceClosure closure,
                               const absl::optional<float>& output) {
  outputs->push_back(output.value_or(0.0));
  std::move(closure).Run();
}

}  // namespace

HistoryClustersModuleRankingModelHandler::
    HistoryClustersModuleRankingModelHandler(
        optimization_guide::OptimizationGuideModelProvider* model_provider)
    : ModelHandler<float, const std::vector<float>&>(
          model_provider,
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE}),
          std::make_unique<HistoryClustersModuleRankingModelExecutor>(),
          /*model_inference_timeout=*/absl::nullopt,
          optimization_guide::proto::OptimizationTarget::
              OPTIMIZATION_TARGET_NEW_TAB_PAGE_HISTORY_CLUSTERS_MODULE_RANKING,
          /*model_metadata=*/absl::nullopt) {
  SetShouldUnloadModelOnComplete(false);
}

HistoryClustersModuleRankingModelHandler::
    ~HistoryClustersModuleRankingModelHandler() = default;

bool HistoryClustersModuleRankingModelHandler::CanExecuteAvailableModel() {
  auto model_metadata = ParsedSupportedFeaturesForLoadedModel<
      new_tab_page::proto::HistoryClustersModuleRankingModelMetadata>();
  if (!model_metadata) {
    return false;
  }
  return model_metadata->version() <=
         HistoryClustersModuleRankingSignals::kClientVersion;
}

void HistoryClustersModuleRankingModelHandler::ExecuteBatch(
    std::vector<HistoryClustersModuleRankingSignals>* inputs,
    ExecuteBatchCallback callback) {
  CHECK(inputs);

  // Validate model.
  auto model_metadata = ParsedSupportedFeaturesForLoadedModel<
      new_tab_page::proto::HistoryClustersModuleRankingModelMetadata>();
  if (!model_metadata ||
      model_metadata->version() >
          HistoryClustersModuleRankingSignals::kClientVersion) {
    std::vector<float> outputs(inputs->size());
    std::move(callback).Run(std::move(outputs));
    return;
  }

  // Execute batch.
  auto batch_job = std::make_unique<BatchModelOutput>();
  batch_job->reserve(inputs->size());
  BatchModelOutput* batch_job_ptr = batch_job.get();
  pending_jobs_.insert(std::move(batch_job));
  auto barrier_closure = base::BarrierClosure(
      inputs->size(),
      base::BindOnce(&HistoryClustersModuleRankingModelHandler::OnBatchExecuted,
                     weak_ptr_factory_.GetWeakPtr(), batch_job_ptr,
                     std::move(callback)));
  for (const auto& input : *inputs) {
    std::vector<float> input_vector =
        ConstructInputVector(*model_metadata, input);
    ExecuteModelWithInput(base::BindOnce(&OnSingleExecutionComplete,
                                         batch_job_ptr, barrier_closure),
                          input_vector);
  }
}

void HistoryClustersModuleRankingModelHandler::OnBatchExecuted(
    BatchModelOutput* job,
    ExecuteBatchCallback callback) {
  std::move(callback).Run(std::move(*job));

  // Clean up and maybe unload model.
  auto it = pending_jobs_.find(job);
  if (it != pending_jobs_.end()) {
    pending_jobs_.erase(it);
  }
  if (pending_jobs_.empty()) {
    UnloadModel();
  }
}
