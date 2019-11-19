// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/decision_tree_prediction_model.h"

namespace optimization_guide {

// static
std::unique_ptr<PredictionModel> PredictionModel::Create(
    std::unique_ptr<optimization_guide::proto::PredictionModel>
        prediction_model,
    const base::flat_set<std::string>& host_model_features) {
  // TODO(crbug/1009123): Add a histogram to record if the provided model is
  // constructed successfully or not.
  // TODO(crbug/1009123): Adding timing metrics around initialization due to
  // potential validation overhead.
  if (!prediction_model->has_model())
    return nullptr;

  if (!prediction_model->has_model_info())
    return nullptr;

  if (!prediction_model->model_info().has_version())
    return nullptr;

  // Enforce that only one ModelType is specified for the PredictionModel.
  if (prediction_model->model_info().supported_model_types_size() != 1) {
    return nullptr;
  }

  // Check that the client supports this type of model and is not an unknown
  // type.
  if (!optimization_guide::proto::ModelType_IsValid(
          prediction_model->model_info().supported_model_types(0)) ||
      prediction_model->model_info().supported_model_types(0) ==
          optimization_guide::proto::ModelType::MODEL_TYPE_UNKNOWN) {
    return nullptr;
  }

  // Check that the client supports the model features for |prediction model|.
  for (const auto& model_feature :
       prediction_model->model_info().supported_model_features()) {
    if (!optimization_guide::proto::ClientModelFeature_IsValid(model_feature) ||
        model_feature == optimization_guide::proto::ClientModelFeature::
                             CLIENT_MODEL_FEATURE_UNKNOWN)
      return nullptr;
  }

  std::unique_ptr<PredictionModel> model;
  // The Decision Tree model type is currently the only supported model type.
  if (prediction_model->model_info().supported_model_types(0) !=
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE) {
    return nullptr;
  }
  model = std::make_unique<DecisionTreePredictionModel>(
      std::move(prediction_model), host_model_features);

  // Any constructed model must be validated for correctness according to its
  // model type before being returned.
  if (!model->ValidatePredictionModel())
    return nullptr;

  return model;
}

PredictionModel::PredictionModel(
    std::unique_ptr<optimization_guide::proto::PredictionModel>
        prediction_model,
    const base::flat_set<std::string>& host_model_features) {
  version_ = prediction_model->model_info().version();
  model_features_.reserve(
      prediction_model->model_info().supported_model_features_size() +
      host_model_features.size());
  // Insert all the client model features for the owned |model_|.
  for (const auto& client_model_feature :
       prediction_model->model_info().supported_model_features()) {
    model_features_.emplace(optimization_guide::proto::ClientModelFeature_Name(
        client_model_feature));
  }
  // Insert all the host model features for the owned |model_|.
  for (const auto& host_model_feature : host_model_features)
    model_features_.emplace(host_model_feature);
  model_ = std::make_unique<optimization_guide::proto::Model>(
      prediction_model->model());
}

int64_t PredictionModel::GetVersion() const {
  SEQUENCE_CHECKER(sequence_checker_);
  return version_;
}

base::flat_set<std::string> PredictionModel::GetModelFeatures() const {
  SEQUENCE_CHECKER(sequence_checker_);
  return model_features_;
}

PredictionModel::~PredictionModel() = default;

}  // namespace optimization_guide
