// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_SERVICE_H_
#define CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/passage_embeddings/passage_embeddings_types.h"
#include "services/passage_embeddings/public/mojom/passage_embeddings.mojom.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace passage_embeddings {

class PassageEmbeddingsServiceController;

// Computes passage embeddings using PassageEmbeddingsServiceController.
// Encapsulates queuing and scheduling logic.
class EmbedderService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  // `model_provider` may be nullptr. If provided, it is guaranteed to
  // outlive `this` since EmbedderServiceFactory depends on
  // OptimizationGuideKeyedServiceFactory. `service_controller` is a singleton
  // and never nullptr.
  EmbedderService(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      PassageEmbeddingsServiceController* service_controller);
  ~EmbedderService() override;

  // Calls `service_controller_` to compute the embeddings and calls `callback`
  // with the results. It is guaranteed that the `embeddings` will have the same
  // number of elements as `passages` when all embedding executions succeed.
  // Otherwise, will return an empty vector and a failure status.
  using Embedding = std::vector<float>;
  using ComputePassagesEmbeddingsCallback =
      base::OnceCallback<void(std::vector<std::string> passages,
                              std::vector<Embedding> embeddings,
                              ComputeEmbeddingsStatus status)>;
  void ComputePassagesEmbeddings(mojom::PassagePriority priority,
                                 std::vector<std::string> passages,
                                 ComputePassagesEmbeddingsCallback callback);

 private:
  // OptimizationTargetModelObserver:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // The provider of the embeddings model.
  // May be nullptr. Otherwiseit is guaranteed to outlive `this`.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_;

  // The controller used to interact with the PassageEmbeddingsService.
  // It is a singleton and guaranteed not to be nullptr and to outlive `this`.
  raw_ptr<PassageEmbeddingsServiceController> service_controller_;
};

}  // namespace passage_embeddings

#endif  // CHROME_BROWSER_PASSAGE_EMBEDDINGS_EMBEDDER_SERVICE_H_
