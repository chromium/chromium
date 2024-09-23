// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_LOADER_H_
#define CHROME_BROWSER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_LOADER_H_

#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

// Service that manages models required to support dependency parsing in the
// browser. Currently, the service should only be used in the browser as it
// relies on the Optimization Guide.
class DependencyParserModelLoader
    : public KeyedService,
      public optimization_guide::OptimizationTargetModelObserver {
 public:
  using GetModelCallback = base::OnceCallback<void(base::File)>;
  using NotifyModelAvailableCallback = base::OnceCallback<void(bool)>;

  DependencyParserModelLoader(
      optimization_guide::OptimizationGuideModelProvider* opt_guide,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);
  DependencyParserModelLoader(const DependencyParserModelLoader&) = delete;
  ~DependencyParserModelLoader() override;

  // KeyedService implementation:
  void Shutdown() override;

  // optimization_guide::OptimizationTargetModelObserver implementation:
  void OnModelUpdated(
      optimization_guide::proto::OptimizationTarget optimization_target,
      base::optional_ref<const optimization_guide::ModelInfo> model_info)
      override;

  // Returns the dependency parser model file, should only be called when the
  // model file is already available. See the |NotifyOnModelFileAvailable|
  // for an asynchronous notification of the model being available.
  base::File GetDependencyParserModelFile();

  // Returns whether the dependency parser model is loaded and available to be
  // requested.
  bool IsModelAvailable() { return dependency_parser_model_file_.has_value(); }

  // If the model file is not available, requestors can ask to be notified, via
  // |callback|. This enables a two-step approach to relabily get the model file
  // when it becomes available if the requestor needs the file right when it
  // becomes available. This is to ensure that if the callback becomes empty,
  // only the notification gets dropped, rather than the model file which has to
  // be closed on a background thread.
  void NotifyOnModelFileAvailable(NotifyModelAvailableCallback callback);

 private:
  // Unloads the model in background task.
  void UnloadModelFile();

  // Notifies the model update to observers, and clears the observer list.
  void NotifyModelUpdatesAndClear(bool is_model_available);

  void OnModelFileLoaded(base::File model_file);

  // Optimization Guide Service that provides model files for this service.
  // Optimization Guide Service is a BrowserContextKeyedServiceFactory and
  // should not be used after Shutdown.
  raw_ptr<optimization_guide::OptimizationGuideModelProvider> opt_guide_;

  // The file that contains the dependency parser model. Available when the
  // file path has been provided by the Optimization Guide and has been
  // successfully loaded.
  std::optional<base::File> dependency_parser_model_file_;

  // The set of callbacks associated with requests for the dependency parser
  // model. The callback notifies requesters than the model file is
  // now available and can be safely requested.
  std::vector<NotifyModelAvailableCallback> pending_model_requests_;

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DependencyParserModelLoader> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_ACCESSIBILITY_PHRASE_SEGMENTATION_DEPENDENCY_PARSER_MODEL_LOADER_H_
