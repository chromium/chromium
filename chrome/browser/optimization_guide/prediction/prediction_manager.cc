// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/top_host_provider.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace optimization_guide {

PredictionManager::PredictionManager(
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets_at_initialization,
    TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : session_fcp_(),
      top_host_provider_(top_host_provider),
      url_loader_factory_(url_loader_factory) {
  RegisterOptimizationTargets(optimization_targets_at_initialization);
  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);
}

PredictionManager::~PredictionManager() {
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
}

void PredictionManager::UpdateFCPSessionStatistics(base::TimeDelta fcp) {
  previous_load_fcp_ms_ = static_cast<float>(fcp.InMilliseconds());
  session_fcp_.AddSample(*previous_load_fcp_ms_);
}

void PredictionManager::RegisterOptimizationTargets(
    const std::vector<proto::OptimizationTarget>& optimization_targets) {
  SEQUENCE_CHECKER(sequence_checker_);

  for (const auto& optimization_target : optimization_targets) {
    if (optimization_target == proto::OPTIMIZATION_TARGET_UNKNOWN)
      continue;
    if (registered_optimization_targets_.find(optimization_target) !=
        registered_optimization_targets_.end()) {
      continue;
    }
    registered_optimization_targets_.insert(optimization_target);
  }
  // TODO(crbug/1001194): If the OptimizationGuideStore is available/ready, ask
  // it to start loading the registered models. Scheduling for model host model
  // fetch will wait until the store is ready.

  // TODO(crbug/1001194): Create a schedule for fetching updates for models and
  // for additional/fresh host model features.
  FetchModelsAndHostModelFeatures();
}

base::Optional<float> PredictionManager::GetValueForClientFeature(
    const std::string& model_feature,
    content::NavigationHandle* navigation_handle) const {
  SEQUENCE_CHECKER(sequence_checker_);

  proto::ClientModelFeature client_model_feature;
  if (!proto::ClientModelFeature_Parse(model_feature, &client_model_feature))
    return base::nullopt;

  switch (client_model_feature) {
    case proto::CLIENT_MODEL_FEATURE_UNKNOWN: {
      return base::nullopt;
    }
    case proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE: {
      return static_cast<float>(current_effective_connection_type_);
    }
    case proto::CLIENT_MODEL_FEATURE_PAGE_TRANSITION: {
      return static_cast<float>(navigation_handle->GetPageTransition());
    }
    case proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE: {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
      SiteEngagementService* engagement_service =
          SiteEngagementService::Get(profile);
      // Precision loss is acceptable/expected for prediction models.
      return static_cast<float>(
          engagement_service->GetScore(navigation_handle->GetURL()));
    }
    case proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION: {
      return static_cast<float>(url::IsSameOriginWith(
          navigation_handle->GetURL(), navigation_handle->GetPreviousURL()));
    }
    case proto::CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN: {
      return session_fcp_.GetMean();
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION: {
      return session_fcp_.GetStdDev();
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD: {
      return previous_load_fcp_ms_.value_or(0.0);
    }
    default: {
      return base::nullopt;
    }
  }
}

base::flat_map<std::string, float> PredictionManager::BuildFeatureMap(
    content::NavigationHandle* navigation_handle,
    const base::flat_set<std::string>& model_features) const {
  SEQUENCE_CHECKER(sequence_checker_);
  base::flat_map<std::string, float> feature_map;
  if (model_features.size() == 0)
    return feature_map;

  const base::flat_map<std::string, float>* host_model_features = nullptr;

  std::string host = navigation_handle->GetURL().host();
  auto it = host_model_features_map_.find(host);
  if (it != host_model_features_map_.end())
    host_model_features = &(it->second);

  UMA_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost",
      host_model_features != nullptr);

  // If the feature is not implemented by the client, it is assumed that it is a
  // host model feature we have in the map. If it is not in either, a default is
  // created for it. This ensures that the prediction model will have values for
  // every feature that it requires to be evaluated.
  for (const auto& model_feature : model_features) {
    base::Optional<float> value =
        GetValueForClientFeature(model_feature, navigation_handle);
    if (value) {
      feature_map[model_feature] = *value;
      continue;
    }
    if (!host_model_features || !host_model_features->contains(model_feature)) {
      feature_map[model_feature] = 0.0;
      continue;
    }
    feature_map[model_feature] =
        host_model_features->find(model_feature)->second;
  }
  return feature_map;
}

OptimizationTargetDecision PredictionManager::ShouldTargetNavigation(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationTarget optimization_target) const {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  // TODO(crbug/1001194): Add histogram to record that the optimization target
  // was not registered but was requested.
  if (!registered_optimization_targets_.contains(optimization_target))
    return OptimizationTargetDecision::kUnknown;

  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it == optimization_target_prediction_model_map_.end()) {
    // TODO(crbug/1001194): Check the store to see if there is a model
    // available. There will also be a check with metrics on if the model was
    // available in the but not loaded.
    return OptimizationTargetDecision::kModelNotAvailableOnClient;
  }
  PredictionModel* prediction_model = it->second.get();

  base::flat_map<std::string, float> feature_map =
      BuildFeatureMap(navigation_handle, prediction_model->GetModelFeatures());

  double prediction_score = 0.0;
  optimization_guide::OptimizationTargetDecision target_decision =
      prediction_model->Predict(feature_map, &prediction_score);

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (navigation_data) {
    navigation_data->SetModelVersionForOptimizationTarget(
        optimization_target, prediction_model->GetVersion());
    navigation_data->SetModelPredictionScoreForOptimizationTarget(
        optimization_target, prediction_score);
  }

  if (optimization_guide::features::
          ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
              optimization_target)) {
    return optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback;
  }

  return target_decision;
}

void PredictionManager::OnEffectiveConnectionTypeChanged(
    net::EffectiveConnectionType effective_connection_type) {
  SEQUENCE_CHECKER(sequence_checker_);
  current_effective_connection_type_ = effective_connection_type;
}

PredictionModel* PredictionManager::GetPredictionModelForTesting(
    proto::OptimizationTarget optimization_target) const {
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it != optimization_target_prediction_model_map_.end())
    return it->second.get();
  return nullptr;
}

base::flat_map<std::string, base::flat_map<std::string, float>>
PredictionManager::GetHostModelFeaturesForTesting() const {
  return host_model_features_map_;
}

void PredictionManager::SetPredictionModelFetcherForTesting(
    std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher) {
  prediction_model_fetcher_ = std::move(prediction_model_fetcher);
}

void PredictionManager::FetchModelsAndHostModelFeatures() {
  DCHECK(top_host_provider_);

  // Models and host model features should not be fetched if there are no
  // optimization targets registered.
  if (registered_optimization_targets_.size() == 0)
    return;

  std::vector<std::string> top_hosts = top_host_provider_->GetTopHosts();

  if (!prediction_model_fetcher_) {
    prediction_model_fetcher_ = std::make_unique<PredictionModelFetcher>(
        url_loader_factory_,
        features::GetOptimizationGuideServiceGetModelsURL());
  }

  std::vector<proto::ModelInfo> models_info = std::vector<proto::ModelInfo>();

  proto::ModelInfo base_model_info;
  for (auto client_model_feature = proto::ClientModelFeature_MIN + 1;
       client_model_feature <= proto::ClientModelFeature_MAX;
       client_model_feature++) {
    if (proto::ClientModelFeature_IsValid(client_model_feature)) {
      base_model_info.add_supported_model_features(
          static_cast<proto::ClientModelFeature>(client_model_feature));
    }
  }
  // Only Decision Trees are currently supported.
  base_model_info.add_supported_model_types(proto::MODEL_TYPE_DECISION_TREE);

  // For now, we will fetch for all registered optimization targets.
  for (const auto& optimization_target : registered_optimization_targets_) {
    proto::ModelInfo model_info(base_model_info);
    model_info.set_optimization_target(optimization_target);

    auto it =
        optimization_target_prediction_model_map_.find(optimization_target);
    if (it != optimization_target_prediction_model_map_.end())
      model_info.set_version(it->second.get()->GetVersion());

    models_info.push_back(model_info);
  }

  prediction_model_fetcher_->FetchOptimizationGuideServiceModels(
      models_info, top_hosts, optimization_guide::proto::CONTEXT_BATCH_UPDATE,
      base::BindOnce(&PredictionManager::OnModelsAndHostFeaturesFetched,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::OnModelsAndHostFeaturesFetched(
    base::Optional<std::unique_ptr<proto::GetModelsResponse>>
        get_models_response_data) {
  if (!get_models_response_data)
    return;
  // TODO(crbug/1001194): Asynchronously store the models and host model
  // features within the persistent store.

  // The set of host model features that the models in
  // |get_models_response_data| required in order to be evaluated. Every host
  // model feature returned should contain all the features for the models
  // currently supported.
  base::flat_set<std::string> host_model_features;
  if ((*get_models_response_data)->host_model_features_size() > 0) {
    UpdateHostModelFeatures((*get_models_response_data)->host_model_features());

    host_model_features.reserve((*get_models_response_data)
                                    ->host_model_features(0)
                                    .model_features_size());
    for (const auto& model_feature :
         (*get_models_response_data)->host_model_features(0).model_features()) {
      if (model_feature.has_feature_name())
        host_model_features.insert(model_feature.feature_name());
    }
  }

  if ((*get_models_response_data)->models_size() > 0) {
    UpdatePredictionModels((*get_models_response_data)->mutable_models(),
                           host_model_features);
  }
}

void PredictionManager::UpdateHostModelFeatures(
    const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
        host_model_features) {
  for (const auto& host_model_features : host_model_features) {
    if (!host_model_features.has_host())
      continue;
    if (host_model_features.model_features_size() == 0)
      continue;
    base::flat_map<std::string, float> model_features_for_host;
    model_features_for_host.reserve(host_model_features.model_features_size());
    for (const auto& model_feature : host_model_features.model_features()) {
      if (!model_feature.has_feature_name())
        continue;
      switch (model_feature.feature_value_case()) {
        case proto::ModelFeature::kDoubleValue:
          // Loss of precision from double is acceptable for features supported
          // by the prediction models.
          model_features_for_host.emplace(
              model_feature.feature_name(),
              static_cast<float>(model_feature.double_value()));
          break;
        case proto::ModelFeature::kInt64Value:
          model_features_for_host.emplace(
              model_feature.feature_name(),
              static_cast<float>(model_feature.int64_value()));
          break;
        case proto::ModelFeature::FEATURE_VALUE_NOT_SET:
          NOTREACHED();
          break;
      }
    }
    if (model_features_for_host.size() == 0)
      continue;
    auto it = host_model_features_map_.find(host_model_features.host());
    if (it != host_model_features_map_.end()) {
      it->second = model_features_for_host;
      continue;
    }
    host_model_features_map_.emplace(host_model_features.host(),
                                     model_features_for_host);
  }
}

std::unique_ptr<PredictionModel> PredictionManager::CreatePredictionModel(
    const proto::PredictionModel& model,
    const base::flat_set<std::string>& host_model_features) const {
  return PredictionModel::Create(
      std::make_unique<proto::PredictionModel>(model), host_model_features);
}

void PredictionManager::UpdatePredictionModels(
    google::protobuf::RepeatedPtrField<proto::PredictionModel>*
        prediction_models,
    const base::flat_set<std::string>& host_model_features) {
  std::unique_ptr<PredictionModel> prediction_model;
  for (auto& model : *prediction_models) {
    if (!model.model_info().has_optimization_target())
      continue;
    if (!registered_optimization_targets_.contains(
            model.model_info().optimization_target())) {
      continue;
    }

    prediction_model = CreatePredictionModel(model, host_model_features);
    if (!prediction_model) {
      continue;
    }
    auto it = optimization_target_prediction_model_map_.find(
        model.model_info().optimization_target());
    if (it == optimization_target_prediction_model_map_.end()) {
      optimization_target_prediction_model_map_.emplace(
          model.model_info().optimization_target(),
          std::move(prediction_model));
      continue;
    }
    if (it->second->GetVersion() != model.model_info().version())
      it->second = std::move(prediction_model);
  }
}

}  // namespace optimization_guide
