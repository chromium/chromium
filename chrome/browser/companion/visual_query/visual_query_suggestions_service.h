// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_SUGGESTIONS_SERVICE_H_
#define CHROME_BROWSER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_SUGGESTIONS_SERVICE_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "chrome/common/companion/visual_query.mojom.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/visual_search_model_metadata.pb.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace companion::visual_query {

using ModelMetadata =
    absl::optional<optimization_guide::proto::VisualSearchModelMetadata>;

class VisualQuerySuggestionsService
    : public KeyedService,
      public mojom::VisualSuggestionsModelProvider,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using ModelUpdateCallback =
      base::OnceCallback<void(base::File, const std::string&)>;

  VisualQuerySuggestionsService(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  VisualQuerySuggestionsService(const VisualQuerySuggestionsService&) =
      delete;
  VisualQuerySuggestionsService& operator=(
      const VisualQuerySuggestionsService&) = delete;
  ~VisualQuerySuggestionsService() override;

  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // mojom::VisualSuggestionsModelProvider implementation:
  void GetModelWithMetadata(GetModelWithMetadataCallback callback) override;

  // Registers a callback used when model file is available or updated.
  void RegisterModelUpdateCallback(ModelUpdateCallback callback);

  // Allows renderer clients to bind to the implementation of model provider.
  void BindModelReceiver(
      mojo::PendingReceiver<mojom::VisualSuggestionsModelProvider> receiver);

 private:
  // Unloads the model in background task.
  void UnloadModelFile();

  // Notifies the model update to observers, and clears the observer list.
  void NotifyModelUpdatesAndClear();

  void OnModelFileLoaded(base::File model_file);

  // Maintain list of callbacks for observers of model updates.
  std::vector<ModelUpdateCallback> model_callbacks_;

  // Represents the model that we send to the classifier agent.
  absl::optional<base::File> model_file_;

  // Used to store the model metadata returned from model provider.
  ModelMetadata model_metadata_;

  // Pointer to the model provider that we use to fetch classifier models.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_;

  // Set of receivers from renderers used to acquire model and metadata.
  mojo::ReceiverSet<mojom::VisualSuggestionsModelProvider> model_receivers_;

  // Background task runner needed to perform I/O operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Pointer factory necessary for scheduling tasks on different threads.
  base::WeakPtrFactory<VisualQuerySuggestionsService> weak_ptr_factory_{this};
};

}  // namespace companion::visual_query

#endif  // CHROME_BROWSER_COMPANION_VISUAL_QUERY_VISUAL_QUERY_SUGGESTIONS_SERVICE_H_
