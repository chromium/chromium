// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/lru_cache.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/time/clock.h"
#include "base/timer/timer.h"
#include "chrome/browser/optimization_guide/prediction/prediction_model_download_observer.h"
#include "components/optimization_guide/core/model_enums.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "url/origin.h"

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

class PrefService;
class Profile;

namespace optimization_guide {

enum class OptimizationGuideDecision;
class OptimizationGuideStore;
class OptimizationTargetModelObserver;
class PredictionModel;
class PredictionModelDownloadManager;
class PredictionModelFetcher;
class ModelInfo;

using HostModelFeaturesLRUCache =
    base::HashingLRUCache<std::string, base::flat_map<std::string, float>>;

using OptimizationTargetDecisionCallback =
    base::OnceCallback<void(optimization_guide::OptimizationTargetDecision)>;

using PostModelLoadCallback =
    base::OnceCallback<void(std::unique_ptr<proto::PredictionModel>, bool)>;

// A PredictionManager supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating the corresponding prediction model
// for an OptimizationTarget.
class PredictionManager : public PredictionModelDownloadObserver {
 public:
  PredictionManager(
      base::WeakPtr<OptimizationGuideStore> model_and_features_store,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefService* pref_service,
      Profile* profile);

  PredictionManager(const PredictionManager&) = delete;
  PredictionManager& operator=(const PredictionManager&) = delete;

  ~PredictionManager() override;

  // Adds an observer for updates to the model for |optimization_target|.
  //
  // It is assumed that any model retrieved this way will be passed to the
  // Machine Learning Service for inference.
  void AddObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      const absl::optional<proto::Any>& model_metadata,
      OptimizationTargetModelObserver* observer);

  // Removes an observer for updates to the model for |optimization_target|.
  //
  // If |observer| is registered for multiple targets, |observer| must be
  // removed for all observed targets for in order for it to be fully
  // removed from receiving any calls.
  void RemoveObserverForOptimizationTargetModel(
      proto::OptimizationTarget optimization_target,
      OptimizationTargetModelObserver* observer);

  // Set the prediction model fetcher for testing.
  void SetPredictionModelFetcherForTesting(
      std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher);

  PredictionModelFetcher* prediction_model_fetcher() const {
    return prediction_model_fetcher_.get();
  }

  // Set the prediction model download manager for testing.
  void SetPredictionModelDownloadManagerForTesting(
      std::unique_ptr<PredictionModelDownloadManager>
          prediction_model_download_manager);

  PredictionModelDownloadManager* prediction_model_download_manager() const {
    return prediction_model_download_manager_.get();
  }

  base::WeakPtr<OptimizationGuideStore> model_and_features_store() const {
    return model_and_features_store_;
  }

  // Return the optimization targets that are registered.
  base::flat_set<proto::OptimizationTarget> GetRegisteredOptimizationTargets()
      const;

  // Override |clock_| for testing.
  void SetClockForTesting(const base::Clock* clock);

  // Clear host model features from the in memory host model features map and
  // from the models and features store.
  void ClearHostModelFeatures();

  // Override the model file returned to observers for |optimization_target|.
  // Use |TestModelInfoBuilder| to construct the model files. For
  // testing purposes only.
  void OverrideTargetModelForTesting(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<ModelInfo> model_info);

  // PredictionModelDownloadObserver:
  void OnModelReady(const proto::PredictionModel& model) override;

 protected:
  // Return the prediction model for the optimization target used by this
  // PredictionManager for testing.
  PredictionModel* GetPredictionModelForTesting(
      proto::OptimizationTarget optimization_target) const;

  // Return the host model features for all hosts used by this
  // PredictionManager for testing.
  const HostModelFeaturesLRUCache* GetHostModelFeaturesForTesting() const;

  // Returns the host model features for a host if available.
  absl::optional<base::flat_map<std::string, float>>
  GetHostModelFeaturesForHost(const std::string& host) const;

  // Return the set of features that each host in |host_model_features_map_|
  // contains for testing.
  base::flat_set<std::string> GetSupportedHostModelFeaturesForTesting() const;

  // Create a PredictionModel, virtual for testing.
  virtual std::unique_ptr<PredictionModel> CreatePredictionModel(
      const proto::PredictionModel& model) const;

  // Process |host_model_features| to be stored in memory in the host model
  // features map for immediate use and asynchronously write them to the model
  // and features store to be persisted.
  void UpdateHostModelFeatures(
      const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
          host_model_features);

  // Process |prediction_models| to be stored in the in memory optimization
  // target prediction model map for immediate use and asynchronously write the
  // models to the model and features store to be persisted.
  void UpdatePredictionModels(
      const google::protobuf::RepeatedPtrField<proto::PredictionModel>&
          prediction_models);

 private:
  friend class PredictionManagerTestBase;

  // Called on construction to initialize the prediction model and host model
  // features store, and register as an observer to the network quality tracker.
  void Initialize();

  // Called to make a request to fetch models from the remote Optimization Guide
  // Service. Used to fetch models for the registered optimization targets.
  void FetchModels();

  // Callback when the models have been fetched from the remote Optimization
  // Guide Service and are ready for parsing. Processes the prediction models in
  // the response and stores them for use. The metadata entry containing the
  // time that updates should be fetched from the remote Optimization Guide
  // Service is updated, even when the response is empty.
  void OnModelsFetched(absl::optional<std::unique_ptr<proto::GetModelsResponse>>
                           get_models_response_data);

  // Callback run after the model and host model features store is fully
  // initialized. The prediction manager can load models from
  // the store for registered optimization targets. |store_is_ready_| is set to
  // true.
  void OnStoreInitialized();

  // Callback run after prediction models are stored in
  // |model_and_features_store_|.
  void OnPredictionModelsStored();

  // Callback run after host model features are stored in
  // |model_and_features_store_|. |fetch_timer_| is stopped and the timer is
  // rescheduled based on when models and host model features should be fetched
  // again.
  void OnHostModelFeaturesStored();

  // Request the store to load all the host model features it contains. This
  // must be completed before any prediction models can be loaded from the
  // store.
  void LoadHostModelFeatures();

  // Callback run after host model features are loaded from the store and are
  // ready to be processed and placed in |host_model_features_map_|.
  // |host_model_features_loaded_| is set to true when called. Prediction models
  // for all registered optimization targets that are not already loaded are
  // requested to be loaded.
  void OnLoadHostModelFeatures(
      std::unique_ptr<std::vector<proto::HostModelFeatures>>
          all_host_model_features);

  // Load models for every target in |optimization_targets| that have not yet
  // been loaded from the store.
  void LoadPredictionModels(
      const base::flat_set<proto::OptimizationTarget>& optimization_targets);

  // Callback run after a prediction model is loaded from the store.
  // |prediction_model| is used to construct a PredictionModel capable of making
  // prediction for the appropriate optimization target.
  void OnLoadPredictionModel(
      std::unique_ptr<proto::PredictionModel> prediction_model);

  // Process loaded |model| into memory. Return true if a prediction
  // model object was created and successfully stored, otherwise false.
  bool ProcessAndStoreLoadedModel(const proto::PredictionModel& model);

  // Return whether the model stored in memory for |optimization_target| should
  // be updated based on what's currently stored and |new_version|.
  bool ShouldUpdateStoredModelForTarget(
      proto::OptimizationTarget optimization_target,
      int64_t new_version) const;

  // Updates the in-memory model file for |optimization_target| to
  // |prediction_model_file|.
  void StoreLoadedModelInfo(proto::OptimizationTarget optimization_target,
                            std::unique_ptr<ModelInfo> prediction_model_file);

  // Updates the in-memory model for |optimization_target| to
  // |prediction_model|.
  void StoreLoadedPredictionModel(
      proto::OptimizationTarget optimization_target,
      std::unique_ptr<PredictionModel> prediction_model);

  // Post-processing callback invoked after processing |model|.
  void OnProcessLoadedModel(const proto::PredictionModel& model, bool success);

  // Process |host_model_features| from the into host model features
  // usable by the PredictionManager. The processed host model features are
  // stored in |host_model_features_map_|. Return true if host model features
  // can be constructed and successfully stored, otherwise, return false.
  bool ProcessAndStoreHostModelFeatures(
      const proto::HostModelFeatures& host_model_features);

  // Return the time when a prediction model fetch was last attempted.
  base::Time GetLastFetchAttemptTime() const;

  // Set the last time when a prediction model fetch was last attempted to
  // |last_attempt_time|.
  void SetLastModelFetchAttemptTime(base::Time last_attempt_time);

  // Return the time when a prediction model fetch was last successfully
  // completed.
  base::Time GetLastFetchSuccessTime() const;

  // Set the last time when a fetch for prediction models last succeeded to
  // |last_success_time|.
  void SetLastModelFetchSuccessTime(base::Time last_success_time);

  // Determine whether to schedule fetching new prediction models or fetch
  // immediately due to override.
  void MaybeScheduleModelFetch();

  // Schedule |fetch_timer_| to fire based on:
  // 1. The update time for models in the store and
  // 2. The last time a fetch attempt was made.
  void ScheduleModelsFetch();

  // Notifies observers of |optimization_target| that the model has been
  // updated.
  void NotifyObserversOfNewModel(proto::OptimizationTarget optimization_target,
                                 const ModelInfo& model_info) const;

  // A map of optimization target to the prediction model capable of making
  // an optimization target decision for it.
  base::flat_map<proto::OptimizationTarget, std::unique_ptr<PredictionModel>>
      optimization_target_prediction_model_map_;

  // A map of optimization target to the model file containing the model for the
  // target.
  base::flat_map<proto::OptimizationTarget, std::unique_ptr<ModelInfo>>
      optimization_target_model_info_map_;

  // The map from optimization targets to feature-provided metadata that have
  // been registered with the prediction manager.
  base::flat_map<proto::OptimizationTarget, absl::optional<proto::Any>>
      registered_optimization_targets_and_metadata_;

  // The map from optimization target to observers that have been registered to
  // receive model updates from the prediction manager.
  std::map<proto::OptimizationTarget,
           base::ObserverList<OptimizationTargetModelObserver>>
      registered_observers_for_optimization_targets_;

  // A LRU cache of host to host model features known to the prediction manager.
  HostModelFeaturesLRUCache host_model_features_cache_;

  // The fetcher that handles making requests to update the models and host
  // model features from the remote Optimization Guide Service.
  std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher_;

  // The downloader that handles making requests to download the prediction
  // models. Can be null if model downloading is disabled.
  std::unique_ptr<PredictionModelDownloadManager>
      prediction_model_download_manager_;

  // TODO(crbug/1183507): Remove host model features store and all relevant
  // code, and deprecate the proto field too.
  // The optimization guide store that contains prediction models and host
  // model features from the remote Optimization Guide Service.
  base::WeakPtr<OptimizationGuideStore> model_and_features_store_;

  // A stored response from a model and host model features fetch used to hold
  // models to be stored once host model features are processed and stored.
  std::unique_ptr<proto::GetModelsResponse> get_models_response_data_to_store_;

  // The URL loader factory used for fetching model and host feature updates
  // from the remote Optimization Guide Service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // A reference to the PrefService for this profile. Not owned.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // A reference to the profile. Not owned.
  raw_ptr<Profile> profile_ = nullptr;

  // The timer used to schedule fetching prediction models and host model
  // features from the remote Optimization Guide Service.
  base::OneShotTimer fetch_timer_;

  // The clock used to schedule fetching from the remote Optimization Guide
  // Service.
  raw_ptr<const base::Clock> clock_;

  // Whether the |model_and_features_store_| is initialized and ready for use.
  bool store_is_ready_ = false;

  // Whether host model features have been loaded from the store and are ready
  // for use.
  bool host_model_features_loaded_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PredictionManager> ui_weak_ptr_factory_{this};
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
