// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace companion::visual_search {

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

}  // namespace

VisualSearchSuggestionsService::VisualSearchSuggestionsService(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : model_provider_(model_provider),
      background_task_runner_(background_task_runner) {
  if (model_provider_) {
    model_provider_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
        /*model_metadata=*/absl::nullopt, this);
  }
}

VisualSearchSuggestionsService::~VisualSearchSuggestionsService() {
  if (model_provider_) {
    model_provider_->RemoveObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
        this);
    model_provider_ = nullptr;
  }
}

void VisualSearchSuggestionsService::Shutdown() {
  if (model_file_) {
    // If the model file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseModelFile, std::move(*model_file_)));
  }
}

void VisualSearchSuggestionsService::OnModelFileLoaded(base::File model_file) {
  if (!model_file.IsValid()) {
    return;
  }

  if (model_file_) {
    // If the model file is already loaded, it should be closed on a
    // background thread.
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&CloseModelFile, std::move(*model_file_)));
  }
  model_file_ = std::move(model_file);
}

void VisualSearchSuggestionsService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  if (optimization_target !=
      optimization_guide::proto::
          OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION) {
    return;
  }
  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadModelFile, model_info.GetModelFilePath()),
      base::BindOnce(&VisualSearchSuggestionsService::OnModelFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::File VisualSearchSuggestionsService::GetModelFile() {
  if (model_file_) {
    return model_file_->Duplicate();
  }
  return base::File();
}

}  // namespace companion::visual_search
