// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_navigation_data.h"
#include "chrome/browser/optimization_guide/optimization_guide_permissions_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_fetcher.h"
#include "chrome/browser/optimization_guide/prediction/remote_decision_tree_predictor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/services/machine_learning/public/cpp/service_connection.h"
#include "chrome/services/machine_learning/public/mojom/decision_tree.mojom.h"
#include "chrome/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "components/leveldb_proto/public/proto_database_provider.h"
#include "components/optimization_guide/optimization_guide_constants.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/optimization_guide/optimization_guide_store.h"
#include "components/optimization_guide/optimization_guide_switches.h"
#include "components/optimization_guide/optimization_guide_test_util.h"
#include "components/optimization_guide/optimization_guide_util.h"
#include "components/optimization_guide/prediction_model.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/store_update_data.h"
#include "components/optimization_guide/top_host_provider.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace {

// Returns true if |optimization_target_decision| reflects that the model had
// already been evaluated.
bool ShouldUseCurrentOptimizationTargetDecision(
    optimization_guide::OptimizationTargetDecision
        optimization_target_decision) {
  switch (optimization_target_decision) {
    case optimization_guide::OptimizationTargetDecision::kPageLoadMatches:
    case optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch:
    case optimization_guide::OptimizationTargetDecision::
        kModelPredictionHoldback:
      return true;
    case optimization_guide::OptimizationTargetDecision::
        kModelNotAvailableOnClient:
    case optimization_guide::OptimizationTargetDecision::kUnknown:
    case optimization_guide::OptimizationTargetDecision::kDeciderNotInitialized:
      return false;
  }
}

// Delay between retries on failed fetch and store of prediction models and
// host model features from the remote Optimization Guide Service.
constexpr base::TimeDelta kFetchRetryDelay = base::TimeDelta::FromMinutes(16);

// The amount of time to wait after a successful fetch of models and host model
// features before requesting an update from the remote Optimization Guide
// Service.
constexpr base::TimeDelta kUpdateModelsAndFeaturesDelay =
    base::TimeDelta::FromHours(24);

// Provide a random time delta in seconds before fetching models and host model
// features.
base::TimeDelta RandomFetchDelay() {
  return base::TimeDelta::FromSeconds(base::RandInt(
      optimization_guide::features::PredictionModelFetchRandomMinDelaySecs(),
      optimization_guide::features::PredictionModelFetchRandomMaxDelaySecs()));
}

// Util class for recording the state of a prediction model. The result is
// recorded when it goes out of scope and its destructor is called.
class ScopedPredictionManagerModelStatusRecorder {
 public:
  explicit ScopedPredictionManagerModelStatusRecorder(
      optimization_guide::proto::OptimizationTarget optimization_target)
      : status_(optimization_guide::PredictionManagerModelStatus::kUnknown),
        optimization_target_(optimization_target) {}

  ~ScopedPredictionManagerModelStatusRecorder() {
    DCHECK_NE(status_,
              optimization_guide::PredictionManagerModelStatus::kUnknown);
    base::UmaHistogramEnumeration(
        "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus",
        status_);

    base::UmaHistogramEnumeration(
        "OptimizationGuide.ShouldTargetNavigation.PredictionModelStatus." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        status_);
  }

  void set_status(optimization_guide::PredictionManagerModelStatus status) {
    status_ = status;
  }

 private:
  optimization_guide::PredictionManagerModelStatus status_;
  const optimization_guide::proto::OptimizationTarget optimization_target_;
};

// Util class for recording the construction and validation of a prediction
// model. The result is recorded when it goes out of scope and its destructor is
// called.
class ScopedPredictionModelConstructionAndValidationRecorder {
 public:
  explicit ScopedPredictionModelConstructionAndValidationRecorder(
      optimization_guide::proto::OptimizationTarget optimization_target)
      : validation_start_time_(base::TimeTicks::Now()),
        optimization_target_(optimization_target) {}

  ~ScopedPredictionModelConstructionAndValidationRecorder() {
    base::UmaHistogramBoolean("OptimizationGuide.IsPredictionModelValid",
                              is_valid_);
    base::UmaHistogramBoolean(
        "OptimizationGuide.IsPredictionModelValid." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target_),
        is_valid_);

    // Only record the timing if the model is valid and was able to be
    // constructed.
    if (is_valid_) {
      base::TimeDelta validation_latency =
          base::TimeTicks::Now() - validation_start_time_;
      base::UmaHistogramTimes(
          "OptimizationGuide.PredictionModelValidationLatency",
          validation_latency);
      base::UmaHistogramTimes(
          "OptimizationGuide.PredictionModelValidationLatency." +
              optimization_guide::GetStringNameForOptimizationTarget(
                  optimization_target_),
          validation_latency);
    }
  }

  void set_is_valid(bool is_valid) { is_valid_ = is_valid; }

 private:
  bool is_valid_ = true;
  const base::TimeTicks validation_start_time_;
  const optimization_guide::proto::OptimizationTarget optimization_target_;
};

}  // namespace

namespace optimization_guide {

struct PredictionDecisionParams {
  PredictionDecisionParams(
      base::WeakPtr<OptimizationGuideNavigationData> navigation_data,
      proto::OptimizationTarget optimization_target,
      OptimizationTargetDecisionCallback callback,
      int64_t version,
      base::TimeTicks model_evaluation_start_time)
      : navigation_data(navigation_data),
        optimization_target(optimization_target),
        callback(std::move(callback)),
        version(version),
        model_evaluation_start_time(model_evaluation_start_time) {}

  ~PredictionDecisionParams() = default;

  PredictionDecisionParams(const PredictionDecisionParams&) = delete;
  PredictionDecisionParams& operator=(const PredictionDecisionParams&) = delete;

  // Will store relevant prediction results, if not null.
  base::WeakPtr<OptimizationGuideNavigationData> navigation_data;
  // Target of the prediction.
  proto::OptimizationTarget optimization_target;
  // Callback to be invoked once a OptimizationTargetDecision is made.
  OptimizationTargetDecisionCallback callback;
  // Model version.
  int64_t version;
  // Time when the model evaluation is initiated.
  base::TimeTicks model_evaluation_start_time;
};

PredictionManager::PredictionManager(
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets_at_initialization,
    const base::FilePath& profile_path,
    leveldb_proto::ProtoDatabaseProvider* database_provider,
    TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    Profile* profile)
    : PredictionManager(
          optimization_targets_at_initialization,
          std::make_unique<OptimizationGuideStore>(
              database_provider,
              profile_path.AddExtensionASCII(
                  optimization_guide::
                      kOptimizationGuidePredictionModelAndFeaturesStore),
              base::ThreadPool::CreateSequencedTaskRunner(
                  {base::MayBlock(), base::TaskPriority::BEST_EFFORT})),
          top_host_provider,
          url_loader_factory,
          pref_service,
          profile) {}

PredictionManager::PredictionManager(
    const std::vector<optimization_guide::proto::OptimizationTarget>&
        optimization_targets_at_initialization,
    std::unique_ptr<OptimizationGuideStore> model_and_features_store,
    TopHostProvider* top_host_provider,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    Profile* profile)
    : host_model_features_cache_(
          std::max(features::MaxHostModelFeaturesCacheSize(), size_t(1))),
      session_fcp_(),
      top_host_provider_(top_host_provider),
      model_and_features_store_(std::move(model_and_features_store)),
      url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      profile_(profile),
      clock_(base::DefaultClock::GetInstance()) {
  Initialize(optimization_targets_at_initialization);
}

PredictionManager::~PredictionManager() {
  g_browser_process->network_quality_tracker()
      ->RemoveEffectiveConnectionTypeObserver(this);
}

void PredictionManager::Initialize(const std::vector<proto::OptimizationTarget>&
                                       optimization_targets_at_initialization) {
  RegisterOptimizationTargets(optimization_targets_at_initialization);
  g_browser_process->network_quality_tracker()
      ->AddEffectiveConnectionTypeObserver(this);
  model_and_features_store_->Initialize(
      switches::ShouldPurgeModelAndFeaturesStoreOnStartup(),
      base::BindOnce(&PredictionManager::OnStoreInitialized,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::UpdateFCPSessionStatistics(base::TimeDelta fcp) {
  previous_load_fcp_ms_ = fcp.InMillisecondsF();
  session_fcp_.AddSample(*previous_load_fcp_ms_);
  pref_service_->SetDouble(prefs::kSessionStatisticFCPMean,
                           session_fcp_.GetMean());
  pref_service_->SetDouble(prefs::kSessionStatisticFCPStdDev,
                           session_fcp_.GetStdDev());
}

void PredictionManager::RegisterOptimizationTargets(
    const std::vector<proto::OptimizationTarget>& optimization_targets) {
  SEQUENCE_CHECKER(sequence_checker_);

  if (optimization_targets.size() == 0)
    return;

  base::flat_set<proto::OptimizationTarget> new_optimization_targets;
  for (const auto& optimization_target : optimization_targets) {
    if (optimization_target == proto::OPTIMIZATION_TARGET_UNKNOWN)
      continue;
    if (registered_optimization_targets_.find(optimization_target) !=
        registered_optimization_targets_.end()) {
      continue;
    }
    registered_optimization_targets_.insert(optimization_target);
    new_optimization_targets.insert(optimization_target);
  }

  // Before loading/fetching models and features, the store must be ready.
  if (!store_is_ready_)
    return;

  // Only proceed if there are newly registered targets to load/fetch models and
  // features for. Otherwise, the registered targets will have models loaded
  // when the store was initialized.
  if (new_optimization_targets.size() == 0)
    return;

  // Start loading the host model features if they are not already.
  if (!host_model_features_loaded_) {
    LoadHostModelFeatures();
    return;
  }
  // Otherwise, the host model features are loaded, so load prediction models
  // for any newly registered targets.
  LoadPredictionModels(new_optimization_targets);
}

base::Optional<float> PredictionManager::GetValueForClientFeature(
    const std::string& model_feature,
    content::NavigationHandle* navigation_handle,
    const base::flat_map<proto::ClientModelFeature, float>&
        override_client_model_feature_values) const {
  SEQUENCE_CHECKER(sequence_checker_);

  proto::ClientModelFeature client_model_feature;
  if (!proto::ClientModelFeature_Parse(model_feature, &client_model_feature))
    return base::nullopt;

  auto cmf_value_it =
      override_client_model_feature_values.find(client_model_feature);
  if (cmf_value_it != override_client_model_feature_values.end())
    return cmf_value_it->second;

  base::Optional<float> value;

  switch (client_model_feature) {
    case proto::CLIENT_MODEL_FEATURE_UNKNOWN: {
      return base::nullopt;
    }
    case proto::CLIENT_MODEL_FEATURE_EFFECTIVE_CONNECTION_TYPE: {
      value = static_cast<float>(current_effective_connection_type_);
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_PAGE_TRANSITION: {
      value = static_cast<float>(navigation_handle->GetPageTransition());
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_SITE_ENGAGEMENT_SCORE: {
      Profile* profile = Profile::FromBrowserContext(
          navigation_handle->GetWebContents()->GetBrowserContext());
      SiteEngagementService* engagement_service =
          SiteEngagementService::Get(profile);
      // Precision loss is acceptable/expected for prediction models.
      value = static_cast<float>(
          engagement_service->GetScore(navigation_handle->GetURL()));
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_SAME_ORIGIN_NAVIGATION: {
      OptimizationGuideNavigationData* nav_data =
          OptimizationGuideNavigationData::GetFromNavigationHandle(
              navigation_handle);

      bool is_same_origin = nav_data && nav_data->is_same_origin_navigation();

      LOCAL_HISTOGRAM_BOOLEAN(
          "OptimizationGuide.PredictionManager.IsSameOrigin", is_same_origin);

      value = static_cast<float>(is_same_origin);
      break;
    }
    case proto::CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_MEAN: {
      value = session_fcp_.GetNumberOfSamples() == 0
                  ? static_cast<float>(pref_service_->GetDouble(
                        prefs::kSessionStatisticFCPMean))
                  : session_fcp_.GetMean();
      break;
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_SESSION_STANDARD_DEVIATION: {
      value = session_fcp_.GetNumberOfSamples() == 0
                  ? static_cast<float>(pref_service_->GetDouble(
                        prefs::kSessionStatisticFCPStdDev))
                  : session_fcp_.GetStdDev();
      break;
    }
    case proto::
        CLIENT_MODEL_FEATURE_FIRST_CONTENTFUL_PAINT_PREVIOUS_PAGE_LOAD: {
      value = previous_load_fcp_ms_.value_or(static_cast<float>(
          pref_service_->GetDouble(prefs::kSessionStatisticFCPMean)));
      break;
    }
    default: {
      return base::nullopt;
    }
  }

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (value && navigation_data) {
    navigation_data->SetValueForModelFeature(client_model_feature, *value);
    return value;
  }
  return base::nullopt;
}

base::flat_map<std::string, float> PredictionManager::BuildFeatureMap(
    content::NavigationHandle* navigation_handle,
    const base::flat_set<std::string>& model_features,
    const base::flat_map<proto::ClientModelFeature, float>&
        override_client_model_feature_values) {
  SEQUENCE_CHECKER(sequence_checker_);
  base::flat_map<std::string, float> feature_map;
  if (model_features.size() == 0)
    return feature_map;

  const base::flat_map<std::string, float>* host_model_features = nullptr;

  std::string host = navigation_handle->GetURL().host();
  auto it = host_model_features_cache_.Get(host);
  if (it != host_model_features_cache_.end())
    host_model_features = &(it->second);

  UMA_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HasHostModelFeaturesForHost",
      host_model_features != nullptr);

  // If the feature is not implemented by the client, it is assumed that it is a
  // host model feature we have in the map. If it is not in either, a default is
  // created for it. This ensures that the prediction model will have values for
  // every feature that it requires to be evaluated.
  for (const auto& model_feature : model_features) {
    base::Optional<float> value = GetValueForClientFeature(
        model_feature, navigation_handle, override_client_model_feature_values);
    if (value) {
      feature_map[model_feature] = *value;
      continue;
    }
    if (!host_model_features || !host_model_features->contains(model_feature)) {
      feature_map[model_feature] = -1.0;
      continue;
    }
    feature_map[model_feature] =
        host_model_features->find(model_feature)->second;
  }
  return feature_map;
}

OptimizationTargetDecision PredictionManager::ShouldTargetNavigation(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationTarget optimization_target,
    const base::flat_map<proto::ClientModelFeature, float>&
        override_client_model_feature_values) {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (navigation_data) {
    base::Optional<optimization_guide::OptimizationTargetDecision>
        optimization_target_decision =
            navigation_data->GetDecisionForOptimizationTarget(
                optimization_target);
    if (optimization_target_decision.has_value() &&
        ShouldUseCurrentOptimizationTargetDecision(
            *optimization_target_decision)) {
      return *optimization_target_decision;
    }
  }
  if (!registered_optimization_targets_.contains(optimization_target))
    return OptimizationTargetDecision::kUnknown;

  ScopedPredictionManagerModelStatusRecorder model_status_recorder(
      optimization_target);
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it == optimization_target_prediction_model_map_.end()) {
    if (store_is_ready_ && model_and_features_store_) {
      OptimizationGuideStore::EntryKey model_entry_key;
      if (model_and_features_store_->FindPredictionModelEntryKey(
              optimization_target, &model_entry_key)) {
        model_status_recorder.set_status(
            PredictionManagerModelStatus::kStoreAvailableModelNotLoaded);
      } else {
        model_status_recorder.set_status(
            PredictionManagerModelStatus::kStoreAvailableNoModelForTarget);
      }
    } else {
      model_status_recorder.set_status(
          PredictionManagerModelStatus::kStoreUnavailableModelUnknown);
    }
    return OptimizationTargetDecision::kModelNotAvailableOnClient;
  }
  model_status_recorder.set_status(
      PredictionManagerModelStatus::kModelAvailable);
  PredictionModel* prediction_model = it->second.get();

  base::flat_map<std::string, float> feature_map =
      BuildFeatureMap(navigation_handle, prediction_model->GetModelFeatures(),
                      override_client_model_feature_values);

  base::TimeTicks model_evaluation_start_time = base::TimeTicks::Now();
  double prediction_score = 0.0;
  optimization_guide::OptimizationTargetDecision target_decision =
      prediction_model->Predict(feature_map, &prediction_score);
  if (target_decision != OptimizationTargetDecision::kUnknown) {
    UmaHistogramTimes(
        "OptimizationGuide.PredictionModelEvaluationLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                optimization_target),
        base::TimeTicks::Now() - model_evaluation_start_time);
  }

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

void PredictionManager::ShouldTargetNavigationAsync(
    content::NavigationHandle* navigation_handle,
    proto::OptimizationTarget optimization_target,
    const base::flat_map<proto::ClientModelFeature, float>&
        override_client_model_feature_values,
    OptimizationTargetDecisionCallback callback) {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(navigation_handle->GetURL().SchemeIsHTTPOrHTTPS());

  OptimizationGuideNavigationData* navigation_data =
      OptimizationGuideNavigationData::GetFromNavigationHandle(
          navigation_handle);
  if (navigation_data) {
    base::Optional<optimization_guide::OptimizationTargetDecision>
        optimization_target_decision =
            navigation_data->GetDecisionForOptimizationTarget(
                optimization_target);
    if (optimization_target_decision.has_value() &&
        ShouldUseCurrentOptimizationTargetDecision(
            *optimization_target_decision)) {
      std::move(callback).Run(*optimization_target_decision);
      return;
    }
  }

  if (!registered_optimization_targets_.contains(optimization_target)) {
    std::move(callback).Run(OptimizationTargetDecision::kUnknown);
    return;
  }

  // Use the synchronous code path if ML Service is not enabled.
  if (!features::ShouldUseMLServiceForPrediction()) {
    std::move(callback).Run(
        ShouldTargetNavigation(navigation_handle, optimization_target,
                               override_client_model_feature_values));
    return;
  }

  ScopedPredictionManagerModelStatusRecorder model_status_recorder(
      optimization_target);
  auto it =
      optimization_target_remote_model_predictor_map_.find(optimization_target);
  if (it == optimization_target_remote_model_predictor_map_.end()) {
    if (store_is_ready_ && model_and_features_store_) {
      OptimizationGuideStore::EntryKey model_entry_key;
      if (model_and_features_store_->FindPredictionModelEntryKey(
              optimization_target, &model_entry_key)) {
        model_status_recorder.set_status(
            PredictionManagerModelStatus::kStoreAvailableModelNotLoaded);
      } else {
        model_status_recorder.set_status(
            PredictionManagerModelStatus::kStoreAvailableNoModelForTarget);
      }
    } else {
      model_status_recorder.set_status(
          PredictionManagerModelStatus::kStoreUnavailableModelUnknown);
    }
    std::move(callback).Run(
        OptimizationTargetDecision::kModelNotAvailableOnClient);
    return;
  }

  RemoteDecisionTreePredictor* predictor = it->second.get();

  if (!predictor->Get() || !predictor->IsConnected()) {
    // Connection to remote model is no longer valid.
    model_status_recorder.set_status(
        optimization_guide::PredictionManagerModelStatus::
            kStoreAvailableModelNotLoaded);
    optimization_target_remote_model_predictor_map_.erase(it);
    std::move(callback).Run(
        OptimizationTargetDecision::kModelNotAvailableOnClient);
    return;
  }

  model_status_recorder.set_status(
      PredictionManagerModelStatus::kModelAvailable);

  base::flat_map<std::string, float> feature_map =
      BuildFeatureMap(navigation_handle, predictor->model_features(),
                      override_client_model_feature_values);

  predictor->Get()->Predict(
      feature_map,
      base::BindOnce(&PredictionManager::OnModelEvaluated,
                     ui_weak_ptr_factory_.GetWeakPtr(),
                     std::make_unique<PredictionDecisionParams>(
                         navigation_data->GetWeakPtr(), optimization_target,
                         std::move(callback), predictor->version(),
                         base::TimeTicks::Now())));
}

void PredictionManager::OnModelEvaluated(
    std::unique_ptr<PredictionDecisionParams> params,
    machine_learning::mojom::DecisionTreePredictionResult result,
    double prediction_score) {
  SEQUENCE_CHECKER(sequence_checker_);

  if (result !=
      machine_learning::mojom::DecisionTreePredictionResult::kUnknown) {
    UmaHistogramTimes(
        "OptimizationGuide.PredictionModelEvaluationLatency." +
            optimization_guide::GetStringNameForOptimizationTarget(
                params->optimization_target),
        base::TimeTicks::Now() - params->model_evaluation_start_time);
  }

  if (params->navigation_data) {
    params->navigation_data->SetModelVersionForOptimizationTarget(
        params->optimization_target, params->version);
    params->navigation_data->SetModelPredictionScoreForOptimizationTarget(
        params->optimization_target, prediction_score);
  }

  if (optimization_guide::features::
          ShouldOverrideOptimizationTargetDecisionForMetricsPurposes(
              params->optimization_target)) {
    std::move(params->callback)
        .Run(optimization_guide::OptimizationTargetDecision::
                 kModelPredictionHoldback);
    return;
  }

  optimization_guide::OptimizationTargetDecision target_decision;
  switch (result) {
    case machine_learning::mojom::DecisionTreePredictionResult::kTrue:
      target_decision =
          optimization_guide::OptimizationTargetDecision::kPageLoadMatches;
      break;
    case machine_learning::mojom::DecisionTreePredictionResult::kFalse:
      target_decision =
          optimization_guide::OptimizationTargetDecision::kPageLoadDoesNotMatch;
      break;
    case machine_learning::mojom::DecisionTreePredictionResult::kUnknown:
      target_decision =
          optimization_guide::OptimizationTargetDecision::kUnknown;
      break;
  }

  std::move(params->callback).Run(target_decision);
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

RemoteDecisionTreePredictor*
PredictionManager::GetRemoteDecisionTreePredictorForTesting(
    proto::OptimizationTarget optimization_target) const {
  auto it =
      optimization_target_remote_model_predictor_map_.find(optimization_target);
  if (it != optimization_target_remote_model_predictor_map_.end())
    return it->second.get();
  return nullptr;
}

const HostModelFeaturesMRUCache*
PredictionManager::GetHostModelFeaturesForTesting() const {
  return &host_model_features_cache_;
}

void PredictionManager::SetPredictionModelFetcherForTesting(
    std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher) {
  prediction_model_fetcher_ = std::move(prediction_model_fetcher);
}

void PredictionManager::FetchModelsAndHostModelFeatures() {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!IsUserPermittedToFetchFromRemoteOptimizationGuide(profile_))
    return;

  ScheduleModelsAndHostModelFeaturesFetch();

  // Models and host model features should not be fetched if there are no
  // optimization targets registered.
  if (registered_optimization_targets_.size() == 0)
    return;

  std::vector<std::string> top_hosts;
  // If the top host provider is not available, the user has likely not seen the
  // Lite mode infobar, so top hosts cannot be provided. However, prediction
  // models are allowed to be fetched.
  if (top_host_provider_) {
    top_hosts = top_host_provider_->GetTopHosts();

    // Remove hosts that are already available in the host model features cache.
    // The request should still be made in case there is a new model or a model
    // that does not rely on host model features to be fetched.
    auto it = top_hosts.begin();
    while (it != top_hosts.end()) {
      if (host_model_features_cache_.Peek(*it) !=
          host_model_features_cache_.end()) {
        it = top_hosts.erase(it);
        continue;
      }
      ++it;
    }
  }

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
  SEQUENCE_CHECKER(sequence_checker_);
  if (!get_models_response_data)
    return;

  // Update host model features, even if empty so the store metadata
  // that contains the update time for new models and features to be fetched
  // from the remote Optimization Guide Service is updated.
  UpdateHostModelFeatures((*get_models_response_data)->host_model_features());

  if ((*get_models_response_data)->models_size() > 0) {
    // Stash the response so the models can be stored once the host
    // model features are stored.
    get_models_response_data_to_store_ = std::move(*get_models_response_data);
  }

  fetch_timer_.Stop();
  fetch_timer_.Start(
      FROM_HERE, kUpdateModelsAndFeaturesDelay, this,
      &PredictionManager::ScheduleModelsAndHostModelFeaturesFetch);
}

void PredictionManager::UpdateHostModelFeatures(
    const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
        host_model_features) {
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<StoreUpdateData> host_model_features_update_data =
      StoreUpdateData::CreateHostModelFeaturesStoreUpdateData(
          /*update_time=*/clock_->Now() + kUpdateModelsAndFeaturesDelay,
          /*expiry_time=*/clock_->Now() +
              features::StoredHostModelFeaturesFreshnessDuration());
  for (const auto& host_model_features : host_model_features) {
    if (ProcessAndStoreHostModelFeatures(host_model_features)) {
      host_model_features_update_data->CopyHostModelFeaturesIntoUpdateData(
          host_model_features);
    }
  }

  model_and_features_store_->UpdateHostModelFeatures(
      std::move(host_model_features_update_data),
      base::BindOnce(&PredictionManager::OnHostModelFeaturesStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<PredictionModel> PredictionManager::CreatePredictionModel(
    const proto::PredictionModel& model) const {
  SEQUENCE_CHECKER(sequence_checker_);
  return PredictionModel::Create(
      std::make_unique<proto::PredictionModel>(model));
}

void PredictionManager::UpdatePredictionModels(
    const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
        prediction_models) {
  SEQUENCE_CHECKER(sequence_checker_);
  std::unique_ptr<StoreUpdateData> prediction_model_update_data =
      StoreUpdateData::CreatePredictionModelStoreUpdateData();
  bool has_models_to_update = false;
  for (const auto& model : prediction_models) {
    if (model.has_model() && !model.model().download_url().empty()) {
      // Skip over models that have a download URL since they will be updated
      // out-of-band.

      // TODO(crbug/1146151): Download model from URL.
      continue;
    }

    has_models_to_update = true;
    // Storing the model regardless of whether the model is valid or not. Model
    // will be removed from store if it fails to load.
    prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
    base::UmaHistogramSparse(
        "OptimizationGuide.PredictionModelUpdateVersion." +
            optimization_guide::GetStringNameForOptimizationTarget(
                model.model_info().optimization_target()),
        model.model_info().version());
    OnLoadPredictionModel(std::make_unique<proto::PredictionModel>(model));
  }

  if (has_models_to_update) {
    model_and_features_store_->UpdatePredictionModels(
        std::move(prediction_model_update_data),
        base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnPredictionModelsStored() {
  SEQUENCE_CHECKER(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true);
}

void PredictionManager::OnHostModelFeaturesStored() {
  SEQUENCE_CHECKER(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true);

  if (get_models_response_data_to_store_ &&
      get_models_response_data_to_store_->models_size() > 0) {
    UpdatePredictionModels(get_models_response_data_to_store_->models());
  }
  // Clear any data remaining in the stored get models response.
  get_models_response_data_to_store_.reset();

  // Purge any expired host model features from the store.
  model_and_features_store_->PurgeExpiredHostModelFeatures();

  fetch_timer_.Stop();
  ScheduleModelsAndHostModelFeaturesFetch();
}

void PredictionManager::OnStoreInitialized() {
  SEQUENCE_CHECKER(sequence_checker_);
  store_is_ready_ = true;

  // Only load host model features if there are optimization targets registered.
  if (registered_optimization_targets_.size() == 0)
    return;

  // The store is ready so start loading host model features and the models for
  // the registered optimization targets.  Once the host model features are
  // loaded, prediction models for the registered optimization targets will be
  // loaded.
  LoadHostModelFeatures();

  MaybeScheduleModelAndHostModelFeaturesFetch();
}

void PredictionManager::LoadHostModelFeatures() {
  SEQUENCE_CHECKER(sequence_checker_);
  // Load the host model features first, each prediction model requires the set
  // of host model features to be known before creation.
  model_and_features_store_->LoadAllHostModelFeatures(
      base::BindOnce(&PredictionManager::OnLoadHostModelFeatures,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::OnLoadHostModelFeatures(
    std::unique_ptr<std::vector<proto::HostModelFeatures>>
        all_host_model_features) {
  SEQUENCE_CHECKER(sequence_checker_);
  // If the store returns an empty vector of host model features, the store
  // contains no host model features. However, the load is otherwise complete
  // and prediction models can be loaded but they will require no host model
  // feature information.
  host_model_features_loaded_ = true;
  if (all_host_model_features) {
    for (const auto& host_model_features : *all_host_model_features)
      ProcessAndStoreHostModelFeatures(host_model_features);
  }
  UMA_HISTOGRAM_COUNTS_1000(
      "OptimizationGuide.PredictionManager.HostModelFeaturesMapSize",
      host_model_features_cache_.size());

  // Load the prediction models for all the registered optimization targets now
  // that it is not blocked by loading the host model features.
  LoadPredictionModels(registered_optimization_targets_);
}

void PredictionManager::LoadPredictionModels(
    const base::flat_set<proto::OptimizationTarget>& optimization_targets) {
  SEQUENCE_CHECKER(sequence_checker_);
  DCHECK(host_model_features_loaded_);

  OptimizationGuideStore::EntryKey model_entry_key;
  for (const auto& optimization_target : optimization_targets) {
    // The prediction model for this optimization target has already been
    // loaded.
    if (features::ShouldUseMLServiceForPrediction() &&
        optimization_target_remote_model_predictor_map_.contains(
            optimization_target)) {
      continue;
    }

    if (!features::ShouldUseMLServiceForPrediction() &&
        optimization_target_prediction_model_map_.contains(
            optimization_target)) {
      continue;
    }
    if (!model_and_features_store_->FindPredictionModelEntryKey(
            optimization_target, &model_entry_key)) {
      continue;
    }
    model_and_features_store_->LoadPredictionModel(
        model_entry_key,
        base::BindOnce(&PredictionManager::OnLoadPredictionModel,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnLoadPredictionModel(
    std::unique_ptr<proto::PredictionModel> model) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!model)
    return;

  if (features::ShouldUseMLServiceForPrediction()) {
    SendPredictionModelToMLService(
        std::move(model),
        base::BindOnce(&PredictionManager::OnProcessOrSendPredictionModel,
                       ui_weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  bool success = ProcessAndStorePredictionModel(*model);
  OnProcessOrSendPredictionModel(std::move(model), success);
}

void PredictionManager::OnProcessOrSendPredictionModel(
    std::unique_ptr<proto::PredictionModel> model,
    bool success) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (success) {
    base::UmaHistogramSparse(
        "OptimizationGuide.PredictionModelLoadedVersion." +
            optimization_guide::GetStringNameForOptimizationTarget(
                model->model_info().optimization_target()),
        model->model_info().version());
    return;
  }

  // Remove model from store if it exists.
  OptimizationGuideStore::EntryKey model_entry_key;
  if (model_and_features_store_->FindPredictionModelEntryKey(
          model->model_info().optimization_target(), &model_entry_key)) {
    model_and_features_store_->RemovePredictionModelFromEntryKey(
        model_entry_key);
  }
}

bool PredictionManager::ProcessAndStorePredictionModel(
    const proto::PredictionModel& model) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!model.model_info().has_optimization_target())
    return false;
  if (!model.has_model())
    return false;
  if (!registered_optimization_targets_.contains(
          model.model_info().optimization_target())) {
    return false;
  }

  ScopedPredictionModelConstructionAndValidationRecorder
      prediction_model_recorder(model.model_info().optimization_target());
  std::unique_ptr<PredictionModel> prediction_model =
      CreatePredictionModel(model);
  if (!prediction_model) {
    prediction_model_recorder.set_is_valid(false);
    return false;
  }

  auto it = optimization_target_prediction_model_map_.find(
      model.model_info().optimization_target());
  if (it == optimization_target_prediction_model_map_.end()) {
    optimization_target_prediction_model_map_.emplace(
        model.model_info().optimization_target(), std::move(prediction_model));
    return true;
  }
  if (it->second->GetVersion() != prediction_model->GetVersion()) {
    it->second = std::move(prediction_model);
    return true;
  }
  return false;
}

bool PredictionManager::SendPredictionModelToMLService(
    std::unique_ptr<proto::PredictionModel> model,
    PostModelLoadCallback callback) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!model->model_info().has_optimization_target())
    return false;
  if (!model->has_model())
    return false;
  if (!registered_optimization_targets_.contains(
          model->model_info().optimization_target())) {
    return false;
  }

  // The Decision Tree model type is currently the only supported model type.
  if (model->model_info().supported_model_types(0) ==
      optimization_guide::proto::ModelType::MODEL_TYPE_DECISION_TREE) {
    auto predictor_handle =
        std::make_unique<RemoteDecisionTreePredictor>(*model);

    auto* service_connection =
        machine_learning::ServiceConnection::GetInstance();
    auto pending_receiver = predictor_handle->BindNewPipeAndPassReceiver();
    std::string model_string = model->SerializeAsString();
    service_connection->LoadDecisionTreeModel(
        machine_learning::mojom::DecisionTreeModelSpec::New(model_string),
        std::move(pending_receiver),
        base::BindOnce(&PredictionManager::OnPredictionModelSentToMLService,
                       ui_weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(model), std::move(predictor_handle)));
    return true;
  }
  return false;
}

void PredictionManager::OnPredictionModelSentToMLService(
    PostModelLoadCallback callback,
    std::unique_ptr<proto::PredictionModel> model,
    std::unique_ptr<RemoteDecisionTreePredictor> predictor_handle,
    machine_learning::mojom::LoadModelResult result) {
  SEQUENCE_CHECKER(sequence_checker_);
  proto::OptimizationTarget target = model->model_info().optimization_target();
  ScopedPredictionModelConstructionAndValidationRecorder
      prediction_model_recorder(target);

  if (result != machine_learning::mojom::LoadModelResult::kOk) {
    prediction_model_recorder.set_is_valid(false);
    std::move(callback).Run(std::move(model), false);
    return;
  }

  auto it = optimization_target_remote_model_predictor_map_.find(target);
  if (it == optimization_target_remote_model_predictor_map_.end()) {
    optimization_target_remote_model_predictor_map_.emplace(
        target, std::move(predictor_handle));
  } else if (it->second->version() != model->model_info().version()) {
    it->second = std::move(predictor_handle);
  } else {
    return;
  }

  std::move(callback).Run(std::move(model), true);
}

bool PredictionManager::ProcessAndStoreHostModelFeatures(
    const proto::HostModelFeatures& host_model_features) {
  SEQUENCE_CHECKER(sequence_checker_);
  if (!host_model_features.has_host())
    return false;
  if (host_model_features.model_features_size() == 0)
    return false;

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
    return false;
  host_model_features_cache_.Put(host_model_features.host(),
                                 model_features_for_host);
  return true;
}

void PredictionManager::MaybeScheduleModelAndHostModelFeaturesFetch() {
  if (!IsUserPermittedToFetchFromRemoteOptimizationGuide(profile_))
    return;

  if (optimization_guide::switches::
          ShouldOverrideFetchModelsAndFeaturesTimer()) {
    SetLastModelAndFeaturesFetchAttemptTime(clock_->Now());
    fetch_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(1), this,
                       &PredictionManager::FetchModelsAndHostModelFeatures);
  } else {
    ScheduleModelsAndHostModelFeaturesFetch();
  }
}

base::Time PredictionManager::GetLastFetchAttemptTime() const {
  SEQUENCE_CHECKER(squence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(
          pref_service_->GetInt64(prefs::kModelAndFeaturesLastFetchAttempt)));
}

void PredictionManager::ScheduleModelsAndHostModelFeaturesFetch() {
  DCHECK(!fetch_timer_.IsRunning());
  DCHECK(store_is_ready_);
  const base::TimeDelta time_until_update_time =
      model_and_features_store_->GetHostModelFeaturesUpdateTime() -
      clock_->Now();
  const base::TimeDelta time_until_retry =
      GetLastFetchAttemptTime() + kFetchRetryDelay - clock_->Now();
  base::TimeDelta fetcher_delay =
      std::max(time_until_update_time, time_until_retry);
  if (fetcher_delay <= base::TimeDelta()) {
    SetLastModelAndFeaturesFetchAttemptTime(clock_->Now());
    fetch_timer_.Start(FROM_HERE, RandomFetchDelay(), this,
                       &PredictionManager::FetchModelsAndHostModelFeatures);
    return;
  }
  fetch_timer_.Start(
      FROM_HERE, fetcher_delay, this,
      &PredictionManager::ScheduleModelsAndHostModelFeaturesFetch);
}

void PredictionManager::SetLastModelAndFeaturesFetchAttemptTime(
    base::Time last_attempt_time) {
  SEQUENCE_CHECKER(sequence_checker_);
  pref_service_->SetInt64(
      prefs::kModelAndFeaturesLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void PredictionManager::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void PredictionManager::ClearHostModelFeatures() {
  host_model_features_cache_.Clear();
  if (model_and_features_store_)
    model_and_features_store_->ClearHostModelFeaturesFromDatabase();
}

base::Optional<base::flat_map<std::string, float>>
PredictionManager::GetHostModelFeaturesForHost(const std::string& host) const {
  auto it = host_model_features_cache_.Peek(host);
  if (it == host_model_features_cache_.end())
    return base::nullopt;
  return it->second;
}

void PredictionManager::OverrideTargetDecisionForTesting(
    proto::OptimizationTarget optimization_target,
    OptimizationGuideDecision optimization_guide_decision) {
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it != optimization_target_prediction_model_map_.end())
    optimization_target_prediction_model_map_.erase(it);

  // No model for |kUnknown|. This will make |ShouldTargetNavigation|
  // return an |OptimizationTargetDecision::kModelNotAvailableOnClient|,
  // which in turn yields an |OptimizationGuideDecision::kUnknown| in
  // |OptimizationGuideKeyedService::ShouldTargetNavigation|.
  if (optimization_guide_decision == OptimizationGuideDecision::kUnknown)
    return;

  // Construct a simple model that will return the provided
  // |optimization_guide_decision|.
  const double threshold = 5.0;
  const double weight = 1.0;
  double leaf_value =
      (optimization_guide_decision == OptimizationGuideDecision::kTrue)
          ? threshold + 1.0  // Value is greater than |threshold| to get |kTrue|
          : threshold - 1.0;  // Value is less than |threshold| to get |kFalse|

  std::unique_ptr<proto::PredictionModel> prediction_model =
      GetSingleLeafDecisionTreePredictionModel(threshold, weight,
                                               leaf_value / weight);

  proto::ModelInfo* model_info = prediction_model->mutable_model_info();

  model_info->set_version(1);
  model_info->set_optimization_target(optimization_target);
  model_info->add_supported_model_types(proto::MODEL_TYPE_DECISION_TREE);

  optimization_target_prediction_model_map_.emplace(
      optimization_target, CreatePredictionModel(*prediction_model));
}

}  // namespace optimization_guide
