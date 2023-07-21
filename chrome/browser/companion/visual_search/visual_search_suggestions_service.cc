// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/companion/visual_search/visual_search_suggestions_service.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/task/sequenced_task_runner.h"
#include "components/optimization_guide/core/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/optimization_guide_util.h"
#include "components/optimization_guide/proto/models.pb.h"

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

// Extracts the model string value from the model metadata.
// The model string is expected to be a serialized string of the
// |EligibilitySpec| proto.
std::string GetModelSpec(ModelMetadata& metadata) {
  std::string model_spec;
  if (metadata.has_value() && metadata->has_eligibility_spec()) {
    metadata->eligibility_spec().SerializeToString(&model_spec);
  }
  return model_spec;
}

}  // namespace

VisualSearchSuggestionsService::VisualSearchSuggestionsService(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    const scoped_refptr<base::SequencedTaskRunner>& background_task_runner)
    : model_provider_(model_provider),
      background_task_runner_(background_task_runner) {
  if (model_provider_) {
    // Prepare metadata for requesting a specific version of the config proto.
    optimization_guide::proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/lens.prime.csc.VisualSearchModelMetadata");
    optimization_guide::proto::VisualSearchModelMetadata vs_model_metadata;
    // Version 2 includes the new additions to the proto spec for
    // sorting and for z score handling. There is no explicitly named
    // version 1. The version before version 2 is only the base features.
    vs_model_metadata.set_metadata_version(2);
    vs_model_metadata.SerializeToString(any_metadata.mutable_value());

    model_provider_->AddObserverForOptimizationTargetModel(
        optimization_guide::proto::
            OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION,
        any_metadata, this);
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
  for (auto& callback : model_callbacks_) {
    std::move(callback).Run(model_file_->Duplicate(),
                            GetModelSpec(model_metadata_));
  }
  model_callbacks_.clear();
}

void VisualSearchSuggestionsService::OnModelUpdated(
    optimization_guide::proto::OptimizationTarget optimization_target,
    const optimization_guide::ModelInfo& model_info) {
  if (optimization_target !=
      optimization_guide::proto::
          OPTIMIZATION_TARGET_VISUAL_SEARCH_CLASSIFICATION) {
    return;
  }

  const absl::optional<optimization_guide::proto::Any>& metadata =
      model_info.GetModelMetadata();

  if (metadata.has_value()) {
    model_metadata_ = optimization_guide::ParsedAnyMetadata<
        optimization_guide::proto::VisualSearchModelMetadata>(metadata.value());
  }

  background_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&LoadModelFile, model_info.GetModelFilePath()),
      base::BindOnce(&VisualSearchSuggestionsService::OnModelFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void VisualSearchSuggestionsService::SetModelUpdateCallback(
    ModelUpdateCallback callback) {
  if (model_file_) {
    std::move(callback).Run(model_file_->Duplicate(),
                            GetModelSpec(model_metadata_));
    return;
  }
  model_callbacks_.emplace_back(std::move(callback));
}

}  // namespace companion::visual_search
