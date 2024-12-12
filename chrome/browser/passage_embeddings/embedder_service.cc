// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/embedder_service.h"

#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace passage_embeddings {

EmbedderService::EmbedderService(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    PassageEmbeddingsServiceController* service_controller)
    : model_provider_(model_provider), service_controller_(service_controller) {
  // TODO(383380610): Observe Optimization Target Model if `model_provider_` is
  //  provided.
}

EmbedderService::~EmbedderService() = default;

void EmbedderService::ComputePassagesEmbeddings(
    mojom::PassagePriority priority,
    std::vector<std::string> passages,
    ComputePassagesEmbeddingsCallback callback) {
  ChromePassageEmbeddingsServiceController::Get()->GetEmbeddings(
      std::move(passages), priority,
      base::BindOnce(
          [](ComputePassagesEmbeddingsCallback callback,
             std::vector<mojom::PassageEmbeddingsResultPtr> results,
             ComputeEmbeddingsStatus status) {
            std::vector<std::string> passages;
            std::vector<Embedding> embeddings;
            for (auto& result : results) {
              passages.emplace_back(result->passage);
              embeddings.emplace_back(result->embeddings);
            }
            std::move(callback).Run(std::move(passages), std::move(embeddings),
                                    status);
          },
          std::move(callback)));
}

void EmbedderService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  // TODO(383380610): Update `service_controller_` with the model info.
}

}  // namespace passage_embeddings
