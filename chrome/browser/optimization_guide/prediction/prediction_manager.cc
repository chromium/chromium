// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/prediction/prediction_manager.h"

#include <memory>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/flat_tree.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/histogram_macros_local.h"
#include "base/path_service.h"
#include "base/rand_util.h"
#include "base/sequence_checker.h"
#include "base/task/post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_download_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/common/chrome_paths.h"
#include "components/optimization_guide/content/browser/optimization_guide_decider.h"
#include "components/optimization_guide/core/model_info.h"
#include "components/optimization_guide/core/model_util.h"
#include "components/optimization_guide/core/optimization_guide_constants.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_permissions_util.h"
#include "components/optimization_guide/core/optimization_guide_prefs.h"
#include "components/optimization_guide/core/optimization_guide_store.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_target_model_observer.h"
#include "components/optimization_guide/core/prediction_model.h"
#include "components/optimization_guide/core/prediction_model_fetcher_impl.h"
#include "components/optimization_guide/core/store_update_data.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/prefs/pref_service.h"
#include "components/site_engagement/content/site_engagement_service.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

// Provide a random time delta in seconds before fetching models and host model
// features.
base::TimeDelta RandomFetchDelay() {
  return base::Seconds(base::RandInt(
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

void RecordModelUpdateVersion(
    const optimization_guide::proto::ModelInfo& model_info) {
  base::UmaHistogramSparse(
      "OptimizationGuide.PredictionModelUpdateVersion." +
          optimization_guide::GetStringNameForOptimizationTarget(
              model_info.optimization_target()),
      model_info.version());
}

void RecordModelTypeChanged(
    optimization_guide::proto::OptimizationTarget optimization_target,
    bool changed) {
  base::UmaHistogramBoolean(
      "OptimizationGuide.PredictionManager.ModelTypeChanged." +
          optimization_guide::GetStringNameForOptimizationTarget(
              optimization_target),
      changed);
}

// Returns whether models and host model features should be fetched from the
// remote Optimization Guide Service.
bool ShouldFetchModels(Profile* profile) {
  return optimization_guide::features::IsRemoteFetchingEnabled() &&
         !profile->IsOffTheRecord();
}

std::unique_ptr<optimization_guide::proto::PredictionModel>
BuildPredictionModelFromCommandLineForOptimizationTarget(
    optimization_guide::proto::OptimizationTarget optimization_target) {
  absl::optional<
      std::pair<std::string, absl::optional<optimization_guide::proto::Any>>>
      model_file_path_and_metadata =
          optimization_guide::GetModelOverrideForOptimizationTarget(
              optimization_target);
  if (!model_file_path_and_metadata)
    return nullptr;

  std::unique_ptr<optimization_guide::proto::PredictionModel> prediction_model =
      std::make_unique<optimization_guide::proto::PredictionModel>();
  prediction_model->mutable_model_info()->set_optimization_target(
      optimization_target);
  prediction_model->mutable_model_info()->set_version(123);
  if (model_file_path_and_metadata->second) {
    *prediction_model->mutable_model_info()->mutable_model_metadata() =
        model_file_path_and_metadata->second.value();
  }
  prediction_model->mutable_model()->set_download_url(
      model_file_path_and_metadata->first);
  return prediction_model;
}

}  // namespace

namespace optimization_guide {

struct PredictionDecisionParams {
  PredictionDecisionParams(proto::OptimizationTarget optimization_target,
                           OptimizationTargetDecisionCallback callback,
                           int64_t version,
                           base::TimeTicks model_evaluation_start_time)
      : optimization_target(optimization_target),
        callback(std::move(callback)),
        version(version),
        model_evaluation_start_time(model_evaluation_start_time) {}

  ~PredictionDecisionParams() = default;

  PredictionDecisionParams(const PredictionDecisionParams&) = delete;
  PredictionDecisionParams& operator=(const PredictionDecisionParams&) = delete;

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
    base::WeakPtr<OptimizationGuideStore> model_and_features_store,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    Profile* profile)
    : host_model_features_cache_(
          std::max(features::MaxHostModelFeaturesCacheSize(), size_t(1))),
      prediction_model_download_manager_(nullptr),
      model_and_features_store_(model_and_features_store),
      url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      profile_(profile),
      clock_(base::DefaultClock::GetInstance()) {
  Initialize();
}

PredictionManager::~PredictionManager() {
  if (prediction_model_download_manager_)
    prediction_model_download_manager_->RemoveObserver(this);
}

void PredictionManager::Initialize() {
  if (model_and_features_store_) {
    model_and_features_store_->Initialize(
        switches::ShouldPurgeModelAndFeaturesStoreOnStartup(),
        base::BindOnce(&PredictionManager::OnStoreInitialized,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::AddObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    const absl::optional<proto::Any>& model_metadata,
    OptimizationTargetModelObserver* observer) {
  DCHECK(registered_observers_for_optimization_targets_.find(
             optimization_target) ==
         registered_observers_for_optimization_targets_.end());

  // As DCHECKS don't run in the wild, just do not register the observer if
  // something is already registered for the type. Otherwise, file reads may
  // blow up.
  if (registered_observers_for_optimization_targets_.find(
          optimization_target) !=
      registered_observers_for_optimization_targets_.end()) {
    DLOG(ERROR) << "Did not add observer for optimization target "
                << static_cast<int>(optimization_target)
                << " since an observer for the target was already registered ";
    return;
  }

  registered_observers_for_optimization_targets_[optimization_target]
      .AddObserver(observer);
  if (switches::IsDebugLogsEnabled()) {
    DVLOG(0) << "OptimizationGuide: Observer added for OptimizationTarget: "
             << proto::OptimizationTarget_Name(optimization_target);
  }

  // Notify observer of existing model file path.
  auto model_it = optimization_target_model_info_map_.find(optimization_target);
  if (model_it != optimization_target_model_info_map_.end()) {
    observer->OnModelUpdated(optimization_target, *model_it->second);
    if (switches::IsDebugLogsEnabled()) {
      std::string debug_msg =
          "OptimizationGuide: OnModelFileUpdated for OptimizationTarget: ";
      debug_msg += proto::OptimizationTarget_Name(optimization_target);
      debug_msg += "\nFile path: ";
      debug_msg += (*model_it->second).GetModelFilePath().AsUTF8Unsafe();
      debug_msg += "\nHas metadata: ";
      debug_msg += (model_metadata ? "True" : "False");
      DVLOG(0) << debug_msg;
    }
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (registered_optimization_targets_and_metadata_.contains(
          optimization_target))
    return;

  registered_optimization_targets_and_metadata_.emplace(optimization_target,
                                                        model_metadata);
  if (switches::IsDebugLogsEnabled()) {
    DVLOG(0) << "OptimizationGuide: Registered new OptimizationTarget: "
             << proto::OptimizationTarget_Name(optimization_target);
  }

  // Before loading/fetching models and features, the store must be ready.
  if (!store_is_ready_)
    return;

  // If no fetch is scheduled, maybe schedule one.
  if (!fetch_timer_.IsRunning())
    MaybeScheduleModelFetch();

  // Start loading the host model features if they are not already.
  if (!host_model_features_loaded_) {
    LoadHostModelFeatures();
    return;
  }
  // Otherwise, the host model features are loaded, so load prediction models
  // for any newly registered targets.
  LoadPredictionModels({optimization_target});
}

void PredictionManager::RemoveObserverForOptimizationTargetModel(
    proto::OptimizationTarget optimization_target,
    OptimizationTargetModelObserver* observer) {
  auto observers_it =
      registered_observers_for_optimization_targets_.find(optimization_target);
  if (observers_it == registered_observers_for_optimization_targets_.end())
    return;

  observers_it->second.RemoveObserver(observer);
}

base::flat_set<proto::OptimizationTarget>
PredictionManager::GetRegisteredOptimizationTargets() const {
  base::flat_set<proto::OptimizationTarget> optimization_targets;
  for (const auto& optimization_target_and_metadata :
       registered_optimization_targets_and_metadata_) {
    optimization_targets.insert(optimization_target_and_metadata.first);
  }
  return optimization_targets;
}

PredictionModel* PredictionManager::GetPredictionModelForTesting(
    proto::OptimizationTarget optimization_target) const {
  auto it = optimization_target_prediction_model_map_.find(optimization_target);
  if (it != optimization_target_prediction_model_map_.end())
    return it->second.get();
  return nullptr;
}

const HostModelFeaturesLRUCache*
PredictionManager::GetHostModelFeaturesForTesting() const {
  return &host_model_features_cache_;
}

void PredictionManager::SetPredictionModelFetcherForTesting(
    std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher) {
  prediction_model_fetcher_ = std::move(prediction_model_fetcher);
}

void PredictionManager::SetPredictionModelDownloadManagerForTesting(
    std::unique_ptr<PredictionModelDownloadManager>
        prediction_model_download_manager) {
  prediction_model_download_manager_ =
      std::move(prediction_model_download_manager);
}

void PredictionManager::FetchModels() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (switches::IsModelOverridePresent())
    return;

  if (!ShouldFetchModels(profile_))
    return;

  // Models and host model features should not be fetched if there are no
  // optimization targets registered.
  if (registered_optimization_targets_and_metadata_.empty())
    return;

  if (prediction_model_download_manager_) {
    bool download_service_available =
        prediction_model_download_manager_->IsAvailableForDownloads();
    base::UmaHistogramBoolean(
        "OptimizationGuide.PredictionManager."
        "DownloadServiceAvailabilityBlockedFetch",
        !download_service_available);
    if (!download_service_available) {
      // We cannot download any models from the server, so don't refresh them.
      return;
    }

    prediction_model_download_manager_->CancelAllPendingDownloads();
  }

  // NOTE: ALL PRECONDITIONS FOR THIS FUNCTION MUST BE CHECKED ABOVE THIS LINE.
  // It is assumed that if we proceed past here, that a fetch will at least be
  // attempted.

  std::vector<proto::FieldTrial> active_field_trials;
  // Active field trials convey some sort of user information, so
  // ensure that the user has opted into the right permissions before adding
  // these fields to the request.
  if (IsUserPermittedToFetchFromRemoteOptimizationGuide(
          profile_->IsOffTheRecord(), pref_service_)) {
    google::protobuf::RepeatedPtrField<proto::FieldTrial> current_field_trials =
        GetActiveFieldTrialsAllowedForFetch();
    active_field_trials = std::vector<proto::FieldTrial>(
        {current_field_trials.begin(), current_field_trials.end()});
  }

  if (!prediction_model_fetcher_) {
    prediction_model_fetcher_ = std::make_unique<PredictionModelFetcherImpl>(
        url_loader_factory_,
        features::GetOptimizationGuideServiceGetModelsURL(),
        content::GetNetworkConnectionTracker());
  }

  std::vector<proto::ModelInfo> models_info = std::vector<proto::ModelInfo>();

  proto::ModelInfo base_model_info;
  if (features::IsModelDownloadingEnabled()) {
    // TODO(crbug/1204614): Tidy these up so only the current version is sent to
    // the server.
    base_model_info.add_supported_model_engine_versions(
        proto::MODEL_ENGINE_VERSION_TFLITE_2_3_0);
    base_model_info.add_supported_model_engine_versions(
        proto::MODEL_ENGINE_VERSION_TFLITE_2_3_0_1);
    base_model_info.add_supported_model_engine_versions(
        proto::MODEL_ENGINE_VERSION_TFLITE_2_4);
    base_model_info.add_supported_model_engine_versions(
        proto::MODEL_ENGINE_VERSION_TFLITE_2_7);
    base_model_info.add_supported_model_engine_versions(
        proto::MODEL_ENGINE_VERSION_TFLITE_2_8);
    base_model_info.add_supported_model_engine_versions(
        proto::MODEL_ENGINE_VERSION_TFLITE_2_9);
  }

  std::string debug_msg;
  // For now, we will fetch for all registered optimization targets.
  for (const auto& optimization_target_and_metadata :
       registered_optimization_targets_and_metadata_) {
    proto::ModelInfo model_info(base_model_info);
    model_info.set_optimization_target(optimization_target_and_metadata.first);
    if (optimization_target_and_metadata.second.has_value()) {
      *model_info.mutable_model_metadata() =
          *optimization_target_and_metadata.second;
    }

    auto it = optimization_target_prediction_model_map_.find(
        optimization_target_and_metadata.first);
    if (it != optimization_target_prediction_model_map_.end())
      model_info.set_version(it->second.get()->GetVersion());

    auto model_it = optimization_target_model_info_map_.find(
        optimization_target_and_metadata.first);
    if (model_it != optimization_target_model_info_map_.end())
      model_info.set_version(model_it->second.get()->GetVersion());

    models_info.push_back(model_info);
    if (switches::IsDebugLogsEnabled()) {
      debug_msg +=
          "\nOptimization Target: " +
          proto::OptimizationTarget_Name(model_info.optimization_target());
    }
  }
  if (switches::IsDebugLogsEnabled() && !debug_msg.empty()) {
    DVLOG(0) << "OptimizationGuide: Fetching models for Optimization Targets: "
             << debug_msg;
  }

  bool fetch_initiated =
      prediction_model_fetcher_->FetchOptimizationGuideServiceModels(
          models_info, active_field_trials, proto::CONTEXT_BATCH_UPDATE_MODELS,
          g_browser_process->GetApplicationLocale(),
          base::BindOnce(&PredictionManager::OnModelsFetched,
                         ui_weak_ptr_factory_.GetWeakPtr()));

  if (fetch_initiated)
    SetLastModelFetchAttemptTime(clock_->Now());
  // Schedule the next fetch regardless since we may not have initiated a fetch
  // due to a network condition and trying in the next minute to see if that is
  // unblocked is only a timer firing and not an actual query to the server.
  ScheduleModelsFetch();
}

void PredictionManager::OnModelsFetched(
    absl::optional<std::unique_ptr<proto::GetModelsResponse>>
        get_models_response_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!get_models_response_data)
    return;

  SetLastModelFetchSuccessTime(clock_->Now());

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
  fetch_timer_.Start(FROM_HERE, features::PredictionModelFetchInterval(), this,
                     &PredictionManager::ScheduleModelsFetch);
}

void PredictionManager::UpdateHostModelFeatures(
    const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
        host_model_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!model_and_features_store_)
    return;

  std::unique_ptr<StoreUpdateData> host_model_features_update_data =
      StoreUpdateData::CreateHostModelFeaturesStoreUpdateData(
          /*host_model_features_update_time=*/clock_->Now() +
              features::PredictionModelFetchInterval(),
          /*expiry_time=*/clock_->Now() +
              features::StoredHostModelFeaturesFreshnessDuration());
  for (const auto& features : host_model_features) {
    if (ProcessAndStoreHostModelFeatures(features)) {
      host_model_features_update_data->CopyHostModelFeaturesIntoUpdateData(
          features);
    }
  }

  model_and_features_store_->UpdateHostModelFeatures(
      std::move(host_model_features_update_data),
      base::BindOnce(&PredictionManager::OnHostModelFeaturesStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

std::unique_ptr<PredictionModel> PredictionManager::CreatePredictionModel(
    const proto::PredictionModel& model) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return PredictionModel::Create(model);
}

void PredictionManager::UpdatePredictionModels(
    const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
        prediction_models) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!model_and_features_store_)
    return;

  std::unique_ptr<StoreUpdateData> prediction_model_update_data =
      StoreUpdateData::CreatePredictionModelStoreUpdateData(
          clock_->Now() + features::StoredModelsValidDuration());
  bool has_models_to_update = false;
  std::string debug_msg;
  for (const auto& model : prediction_models) {
    if (model.has_model() && !model.model().download_url().empty()) {
      if (prediction_model_download_manager_) {
        GURL download_url(model.model().download_url());
        if (download_url.is_valid()) {
          prediction_model_download_manager_->StartDownload(download_url);
        }
        base::UmaHistogramBoolean(
            "OptimizationGuide.PredictionManager.IsDownloadUrlValid." +
                GetStringNameForOptimizationTarget(
                    model.model_info().optimization_target()),
            download_url.is_valid());
        if (switches::IsDebugLogsEnabled() && download_url.is_valid()) {
          debug_msg += "\nOptimization Target: " +
                       proto::OptimizationTarget_Name(
                           model.model_info().optimization_target());
          debug_msg += "\nModel Download Was Required.";
        }
      }

      // Skip over models that have a download URL since they will be updated
      // once the download has completed successfully.
      continue;
    }
    if (!model.has_model()) {
      // We already have this updated model, so don't update in store.
      continue;
    }

    has_models_to_update = true;
    // Storing the model regardless of whether the model is valid or not. Model
    // will be removed from store if it fails to load.
    prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
    RecordModelUpdateVersion(model.model_info());
    OnLoadPredictionModel(std::make_unique<proto::PredictionModel>(model));

    if (switches::IsDebugLogsEnabled()) {
      debug_msg += "\nOptimization Target: " +
                   proto::OptimizationTarget_Name(
                       model.model_info().optimization_target());
      debug_msg += "\nNew Version: " +
                   base::NumberToString(model.model_info().version());
      debug_msg += "\nModel Download Not Required.";
    }
  }

  if (has_models_to_update) {
    if (switches::IsDebugLogsEnabled() && !debug_msg.empty()) {
      DVLOG(0) << "OptimizationGuide: Models Fetched for Optimzation Targets: "
               << debug_msg;
    }
    model_and_features_store_->UpdatePredictionModels(
        std::move(prediction_model_update_data),
        base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                       ui_weak_ptr_factory_.GetWeakPtr()));
  }
}

void PredictionManager::OnModelReady(const proto::PredictionModel& model) {
  if (switches::IsModelOverridePresent())
    return;

  if (!model_and_features_store_)
    return;

  DCHECK(model.model_info().has_version() &&
         model.model_info().has_optimization_target());

  RecordModelUpdateVersion(model.model_info());
  if (switches::IsDebugLogsEnabled()) {
    std::string debug_msg = "Optimization Guide: Model Files Downloaded: ";
    debug_msg += "\nOptimization Target: " +
                 proto::OptimizationTarget_Name(
                     model.model_info().optimization_target());
    debug_msg +=
        "\nNew Version: " + base::NumberToString(model.model_info().version());
    DVLOG(0) << debug_msg;
  }

  // Store the received model in the store.
  std::unique_ptr<StoreUpdateData> prediction_model_update_data =
      StoreUpdateData::CreatePredictionModelStoreUpdateData(
          clock_->Now() + features::StoredModelsValidDuration());
  prediction_model_update_data->CopyPredictionModelIntoUpdateData(model);
  model_and_features_store_->UpdatePredictionModels(
      std::move(prediction_model_update_data),
      base::BindOnce(&PredictionManager::OnPredictionModelsStored,
                     ui_weak_ptr_factory_.GetWeakPtr()));

  if (registered_optimization_targets_and_metadata_.contains(
          model.model_info().optimization_target())) {
    OnLoadPredictionModel(std::make_unique<proto::PredictionModel>(model));
  }
}

void PredictionManager::NotifyObserversOfNewModel(
    proto::OptimizationTarget optimization_target,
    const ModelInfo& model_info) const {
  auto observers_it =
      registered_observers_for_optimization_targets_.find(optimization_target);
  if (observers_it == registered_observers_for_optimization_targets_.end())
    return;

  for (auto& observer : observers_it->second) {
    observer.OnModelUpdated(optimization_target, model_info);
    if (switches::IsDebugLogsEnabled()) {
      std::string debug_msg =
          "OptimizationGuide: OnModelFileUpdated for OptimizationTarget: ";
      debug_msg += proto::OptimizationTarget_Name(optimization_target);
      debug_msg += "\nFile path: ";
      debug_msg += model_info.GetModelFilePath().AsUTF8Unsafe();
      debug_msg += "\nHas metadata: ";
      debug_msg += (model_info.GetModelMetadata() ? "True" : "False");
      DVLOG(0) << debug_msg;
    }
  }
}

void PredictionManager::OnPredictionModelsStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.PredictionModelsStored", true);
}

void PredictionManager::OnHostModelFeaturesStored() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!model_and_features_store_)
    return;

  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.HostModelFeaturesStored", true);

  if (get_models_response_data_to_store_ &&
      get_models_response_data_to_store_->models_size() > 0) {
    UpdatePredictionModels(get_models_response_data_to_store_->models());
  }
  // Clear any data remaining in the stored get models response.
  get_models_response_data_to_store_.reset();

  // Purge any expired host model features and inactive models from the store.
  model_and_features_store_->PurgeExpiredHostModelFeatures();
  model_and_features_store_->PurgeInactiveModels();

  fetch_timer_.Stop();
  ScheduleModelsFetch();
}

void PredictionManager::OnStoreInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  store_is_ready_ = true;
  LOCAL_HISTOGRAM_BOOLEAN(
      "OptimizationGuide.PredictionManager.StoreInitialized", true);

  // Create the download manager here if we are allowed to.
  if (features::IsModelDownloadingEnabled() && !profile_->IsOffTheRecord() &&
      !prediction_model_download_manager_) {
    prediction_model_download_manager_ =
        std::make_unique<PredictionModelDownloadManager>(
            BackgroundDownloadServiceFactory::GetForKey(
                profile_->GetProfileKey()),
            base::ThreadPool::CreateSequencedTaskRunner(
                {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                 base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN}));
    prediction_model_download_manager_->AddObserver(this);
  }

  // Only load host model features if there are optimization targets registered.
  if (registered_optimization_targets_and_metadata_.empty())
    return;

  // The store is ready so start loading host model features and the models for
  // the registered optimization targets.  Once the host model features are
  // loaded, prediction models for the registered optimization targets will be
  // loaded.
  LoadHostModelFeatures();

  MaybeScheduleModelFetch();
}

void PredictionManager::LoadHostModelFeatures() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!model_and_features_store_)
    return;

  // Load the host model features first, each prediction model requires the set
  // of host model features to be known before creation.
  model_and_features_store_->LoadAllHostModelFeatures(
      base::BindOnce(&PredictionManager::OnLoadHostModelFeatures,
                     ui_weak_ptr_factory_.GetWeakPtr()));
}

void PredictionManager::OnLoadHostModelFeatures(
    std::unique_ptr<std::vector<proto::HostModelFeatures>>
        all_host_model_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  LoadPredictionModels(GetRegisteredOptimizationTargets());
}

void PredictionManager::LoadPredictionModels(
    const base::flat_set<proto::OptimizationTarget>& optimization_targets) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(host_model_features_loaded_);

  if (switches::IsModelOverridePresent()) {
    for (proto::OptimizationTarget optimization_target : optimization_targets) {
      std::unique_ptr<proto::PredictionModel> prediction_model =
          BuildPredictionModelFromCommandLineForOptimizationTarget(
              optimization_target);
      OnLoadPredictionModel(std::move(prediction_model));
    }
    return;
  }

  if (!model_and_features_store_)
    return;

  OptimizationGuideStore::EntryKey model_entry_key;
  for (const auto& optimization_target : optimization_targets) {
    // The prediction model for this optimization target has already been
    // loaded.
    if (optimization_target_prediction_model_map_.contains(
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model)
    return;

  bool success = ProcessAndStoreLoadedModel(*model);
  OnProcessLoadedModel(*model, success);
}

void PredictionManager::OnProcessLoadedModel(
    const proto::PredictionModel& model,
    bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (success) {
    base::UmaHistogramSparse(
        "OptimizationGuide.PredictionModelLoadedVersion." +
            optimization_guide::GetStringNameForOptimizationTarget(
                model.model_info().optimization_target()),
        model.model_info().version());
    return;
  }

  // Remove model from store if it exists.
  OptimizationGuideStore::EntryKey model_entry_key;
  if (model_and_features_store_ &&
      model_and_features_store_->FindPredictionModelEntryKey(
          model.model_info().optimization_target(), &model_entry_key)) {
    LOCAL_HISTOGRAM_BOOLEAN(
        "OptimizationGuide.PredictionModelRemoved." +
            optimization_guide::GetStringNameForOptimizationTarget(
                model.model_info().optimization_target()),
        true);
    model_and_features_store_->RemovePredictionModelFromEntryKey(
        model_entry_key);
  }
}

bool PredictionManager::ProcessAndStoreLoadedModel(
    const proto::PredictionModel& model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!model.model_info().has_optimization_target())
    return false;
  if (!model.model_info().has_version())
    return false;
  if (!model.has_model())
    return false;
  if (!registered_optimization_targets_and_metadata_.contains(
          model.model_info().optimization_target())) {
    return false;
  }

  ScopedPredictionModelConstructionAndValidationRecorder
      prediction_model_recorder(model.model_info().optimization_target());
  std::unique_ptr<ModelInfo> model_info = ModelInfo::Create(model);
  std::unique_ptr<PredictionModel> prediction_model =
      model_info ? nullptr : CreatePredictionModel(model);
  if (!model_info && !prediction_model) {
    prediction_model_recorder.set_is_valid(false);
    return false;
  }

  proto::OptimizationTarget optimization_target =
      model.model_info().optimization_target();

  // See if we should update the loaded model.
  if (!ShouldUpdateStoredModelForTarget(optimization_target,
                                        model.model_info().version())) {
    return true;
  }

  // Update prediction model file if that is what we have loaded.
  if (model_info) {
    StoreLoadedModelInfo(optimization_target, std::move(model_info));
  }

  // Update prediction model if that is what we have loaded.
  if (prediction_model) {
    StoreLoadedPredictionModel(optimization_target,
                               std::move(prediction_model));
  }

  return true;
}

bool PredictionManager::ShouldUpdateStoredModelForTarget(
    proto::OptimizationTarget optimization_target,
    int64_t new_version) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto model_meta_it =
      optimization_target_model_info_map_.find(optimization_target);
  if (model_meta_it != optimization_target_model_info_map_.end())
    return model_meta_it->second->GetVersion() != new_version;

  auto model_it =
      optimization_target_prediction_model_map_.find(optimization_target);
  if (model_it != optimization_target_prediction_model_map_.end())
    return model_it->second->GetVersion() != new_version;

  return true;
}

void PredictionManager::StoreLoadedModelInfo(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(model_info);

  bool has_model_for_target =
      optimization_target_prediction_model_map_.contains(optimization_target);
  RecordModelTypeChanged(optimization_target, has_model_for_target);
  if (has_model_for_target) {
    // Remove prediction model if we received the update as a model file. In
    // practice, this shouldn't happen.
    optimization_target_prediction_model_map_.erase(optimization_target);
  }

  // Notify observers of new model file path.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&PredictionManager::NotifyObserversOfNewModel,
                                ui_weak_ptr_factory_.GetWeakPtr(),
                                optimization_target, *model_info));

  optimization_target_model_info_map_.insert_or_assign(optimization_target,
                                                       std::move(model_info));
}

void PredictionManager::StoreLoadedPredictionModel(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<PredictionModel> prediction_model) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool has_model_file_for_target =
      optimization_target_model_info_map_.contains(optimization_target);
  RecordModelTypeChanged(optimization_target, has_model_file_for_target);
  if (has_model_file_for_target) {
    // Remove prediction model file from map if we received the update as a
    // PredictionModel. In practice, this shouldn't happen.
    optimization_target_model_info_map_.erase(optimization_target);
  }
  optimization_target_prediction_model_map_.insert_or_assign(
      optimization_target, std::move(prediction_model));
}

bool PredictionManager::ProcessAndStoreHostModelFeatures(
    const proto::HostModelFeatures& host_model_features) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
  if (model_features_for_host.empty())
    return false;
  host_model_features_cache_.Put(host_model_features.host(),
                                 model_features_for_host);
  return true;
}

void PredictionManager::MaybeScheduleModelFetch() {
  if (!ShouldFetchModels(profile_))
    return;

  if (switches::ShouldOverrideFetchModelsAndFeaturesTimer()) {
    fetch_timer_.Start(FROM_HERE, base::Seconds(1), this,
                       &PredictionManager::FetchModels);
  } else {
    ScheduleModelsFetch();
  }
}

base::Time PredictionManager::GetLastFetchAttemptTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
      pref_service_->GetInt64(prefs::kModelAndFeaturesLastFetchAttempt)));
}

base::Time PredictionManager::GetLastFetchSuccessTime() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
      pref_service_->GetInt64(prefs::kModelLastFetchSuccess)));
}

void PredictionManager::ScheduleModelsFetch() {
  DCHECK(!fetch_timer_.IsRunning());
  DCHECK(store_is_ready_);
  const base::TimeDelta time_until_update_time =
      GetLastFetchSuccessTime() + features::PredictionModelFetchInterval() -
      clock_->Now();
  const base::TimeDelta time_until_retry =
      GetLastFetchAttemptTime() + features::PredictionModelFetchRetryDelay() -
      clock_->Now();
  base::TimeDelta fetcher_delay =
      std::max(time_until_update_time, time_until_retry);
  if (fetcher_delay <= base::TimeDelta()) {
    fetch_timer_.Start(FROM_HERE, RandomFetchDelay(), this,
                       &PredictionManager::FetchModels);
    return;
  }
  fetch_timer_.Start(FROM_HERE, fetcher_delay, this,
                     &PredictionManager::ScheduleModelsFetch);
}

void PredictionManager::SetLastModelFetchAttemptTime(
    base::Time last_attempt_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(
      prefs::kModelAndFeaturesLastFetchAttempt,
      last_attempt_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void PredictionManager::SetLastModelFetchSuccessTime(
    base::Time last_success_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  pref_service_->SetInt64(
      prefs::kModelLastFetchSuccess,
      last_success_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

void PredictionManager::SetClockForTesting(const base::Clock* clock) {
  clock_ = clock;
}

void PredictionManager::ClearHostModelFeatures() {
  host_model_features_cache_.Clear();
  if (model_and_features_store_)
    model_and_features_store_->ClearHostModelFeaturesFromDatabase();
}

absl::optional<base::flat_map<std::string, float>>
PredictionManager::GetHostModelFeaturesForHost(const std::string& host) const {
  auto it = host_model_features_cache_.Peek(host);
  if (it == host_model_features_cache_.end())
    return absl::nullopt;
  return it->second;
}

void PredictionManager::OverrideTargetModelForTesting(
    proto::OptimizationTarget optimization_target,
    std::unique_ptr<ModelInfo> model_info) {
  if (!model_info) {
    return;
  }

  ModelInfo model_info_copy = *model_info;

  optimization_target_model_info_map_.insert_or_assign(optimization_target,
                                                       std::move(model_info));

  NotifyObserversOfNewModel(optimization_target, model_info_copy);
}

}  // namespace optimization_guide
