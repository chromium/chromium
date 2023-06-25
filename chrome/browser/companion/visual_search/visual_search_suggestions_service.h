// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_H_
#define CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/proto/common_types.pb.h"
#include "components/optimization_guide/proto/visual_search_model_metadata.pb.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace companion::visual_search {

using ModelMetadata =
    absl::optional<optimization_guide::proto::VisualSearchModelMetadata>;

class VisualSearchSuggestionsService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using ModelUpdateCallback = base::OnceCallback<void(base::File, std::string)>;

  VisualSearchSuggestionsService(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  VisualSearchSuggestionsService(const VisualSearchSuggestionsService&) =
      delete;
  VisualSearchSuggestionsService& operator=(
      const VisualSearchSuggestionsService&) = delete;
  ~VisualSearchSuggestionsService() override;

  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      const optimization_guide::ModelInfo& model_info) override;

  // Registers a callback used when model file is available or updated.
  void SetModelUpdateCallback(ModelUpdateCallback callback);

 private:
  void OnModelFileLoaded(base::File model_file);

  // Maintain list of callbacks for observers of model updates.
  std::vector<ModelUpdateCallback> model_callbacks_;

  // Represents the model that we send to the classifier agent.
  absl::optional<base::File> model_file_;

  // Used to store the model metadata returned from model provider.
  ModelMetadata model_metadata_;

  // Pointer to the model provider that we use to fetch classifier models.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_;

  // Background task runner needed to perform I/O operations.
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  // Pointer factory necessary for scheduling tasks on different threads.
  base::WeakPtrFactory<VisualSearchSuggestionsService> weak_ptr_factory_{this};
};

}  // namespace companion::visual_search

#endif  // CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_H_
