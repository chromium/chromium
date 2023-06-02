// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_H_
#define CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace companion::visual_search {

class VisualSearchSuggestionsService
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
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

  // Simple getter to access the model file.
  base::File GetModelFile();

 private:
  void OnModelFileLoaded(base::File model_file);

  absl::optional<base::File> model_file_;
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> model_provider_;
  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  base::WeakPtrFactory<VisualSearchSuggestionsService> weak_ptr_factory_{this};
};

}  // namespace companion::visual_search

#endif  // CHROME_BROWSER_COMPANION_VISUAL_SEARCH_VISUAL_SEARCH_SUGGESTIONS_SERVICE_H_
