// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/phrase_segmentation/dependency_parser_model_loader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"

namespace {

// Load the model file at the provided file path.
base::File LoadModelFile(const base::FilePath& model_file_path) {
  if (!base::PathExists(model_file_path)) {
    return base::File();
  }

  return base::File(model_file_path,
                    base::File::FLAG_OPEN | base::File::FLAG_READ);
}

// Close the provided model file.
void CloseModelFile(base::File model_file) {
  if (!model_file.IsValid()) {
    return;
  }
  model_file.Close();
}

// Util class for recording the result of loading the dependency parser model.
// The result is recorded when it goes out of scope and its destructor is
// called.
class ScopedModelLoadingResultRecorder {
 public:
  ScopedModelLoadingResultRecorder() = default;
  ~ScopedModelLoadingResultRecorder() {
    UMA_HISTOGRAM_BOOLEAN(
        "Accessibility.DependencyParserModelLoader.DependencyParserModel."
        "WasLoaded",
        was_loaded_);
  }

  void SetLoaded() { was_loaded_ = true; }

 private:
  bool was_loaded_ = false;
};

// The maximum number of pending model requests allowed to be kept
// by the DependencyParserModelLoader.
constexpr int kMaxPendingRequestsAllowed = 100;

}  // namespace

DependencyParserModelLoader::DependencyParserModelLoader(
    optimization_guide::OptimizationGuideModelProvider* opt_guide,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : opt_guide_(opt_guide), background_task_runner_(background_task_runner) {
  opt_guide_->AddObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION,
      /*model_metadata=*/std::nullopt, this);
}

DependencyParserModelLoader::~DependencyParserModelLoader() {
  opt_guide_->RemoveObserverForOptimizationTargetModel(
      optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION, this);
  // Clear any pending requests, no model file is acceptable as shutdown is
  // happening.
  NotifyModelUpdatesAndClear(false);
}

void DependencyParserModelLoader::Shutdown() {
  // This and the optimization guide are keyed services, currently optimization
  // guide is a BrowserContextKeyedService, it will be cleaned first so removing
  // the observer should not be performed.
  UnloadModelFile();
  // Clear any pending requests, no model file is acceptable as shutdown is
  // happening.
  NotifyModelUpdatesAndClear(false);
}

void DependencyParserModelLoader::UnloadModelFile() {
  if (dependency_parser_model_file_) {
    // If the model file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseModelFile,
                                  std::move(*dependency_parser_model_file_)));
  }
}

void DependencyParserModelLoader::NotifyModelUpdatesAndClear(
    bool is_model_available) {
  for (auto& pending_request : pending_model_requests_) {
    std::move(pending_request).Run(is_model_available);
  }
  pending_model_requests_.clear();
}

void DependencyParserModelLoader::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    base::optional_ref<const optimization_guide::ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (optimization_target !=
      optimization_guide::proto::OPTIMIZATION_TARGET_PHRASE_SEGMENTATION) {
    return;
  }
  if (!model_info.has_value()) {
    UnloadModelFile();
    NotifyModelUpdatesAndClear(false);
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadModelFile, model_info->GetModelFilePath()),
      base::BindOnce(&DependencyParserModelLoader::OnModelFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DependencyParserModelLoader::OnModelFileLoaded(base::File model_file) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopedModelLoadingResultRecorder result_recorder;
  if (!model_file.IsValid()) {
    return;
  }

  UnloadModelFile();
  dependency_parser_model_file_ = std::move(model_file);
  result_recorder.SetLoaded();
  NotifyModelUpdatesAndClear(true);
}

base::File DependencyParserModelLoader::GetDependencyParserModelFile() {
  DCHECK(IsModelAvailable());
  if (!dependency_parser_model_file_) {
    return base::File();
  }
  // The model must be valid at this point.
  DCHECK(dependency_parser_model_file_->IsValid());
  return dependency_parser_model_file_->Duplicate();
}

void DependencyParserModelLoader::NotifyOnModelFileAvailable(
    NotifyModelAvailableCallback callback) {
  DCHECK(!IsModelAvailable());
  if (pending_model_requests_.size() < kMaxPendingRequestsAllowed) {
    pending_model_requests_.emplace_back(std::move(callback));
    return;
  }
  std::move(callback).Run(false);
}
