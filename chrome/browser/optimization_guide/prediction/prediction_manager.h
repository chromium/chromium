// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/optimization_guide/optimization_guide_session_statistic.h"
#include "components/optimization_guide/optimization_guide_enums.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "url/origin.h"

namespace content {
class NavigationHandle;
}  // namespace content

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

namespace optimization_guide {

enum class OptimizationGuideDecision;
class PredictionModel;
class PredictionModelFetcher;
class TopHostProvider;

// A PredictionManager supported by the optimization guide that makes an
// OptimizationTargetDecision by evaluating the corresponding prediction model
// for an OptimizationTarget.
class PredictionManager
    : public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  PredictionManager(
      const std::vector<optimization_guide::proto::OptimizationTarget>&
          optimization_targets_at_initialization,
      TopHostProvider* top_host_provider,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory);
  ~PredictionManager() override;

  // Registers the optimization targets that may have ShouldTargetNavigtation
  // requested by consumers of the Optimization Guide.
  void RegisterOptimizationTargets(
      const std::vector<proto::OptimizationTarget>& optimization_targets);

  // Determines if the navigation matches the critieria for
  // |optimization_target|. Returns kUnknown if a PredictionModel for the
  // optimization target is not registered and kModelNotAvailableOnClient if the
  // if model for the optimization target is not currently on the client.
  OptimizationTargetDecision ShouldTargetNavigation(
      content::NavigationHandle* navigation_handle,
      proto::OptimizationTarget optimization_target) const;

  // Updates |session_fcp_| and |previous_fcp_| with |fcp|.
  void UpdateFCPSessionStatistics(base::TimeDelta fcp);

  OptimizationGuideSessionStatistic* GetFCPSessionStatisticsForTesting() const {
    return const_cast<OptimizationGuideSessionStatistic*>(&session_fcp_);
  }

  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver
  // implementation:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  // Sets the prediction model fetcher for testing.
  void SetPredictionModelFetcherForTesting(
      std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher);

  PredictionModelFetcher* prediction_model_fetcher() const {
    return prediction_model_fetcher_.get();
  }

 protected:

  // Returns the prediction model for the optimization target used by this
  // PredictionManager for testing.
  PredictionModel* GetPredictionModelForTesting(
      proto::OptimizationTarget optimization_target) const;

  // Returns the host model features for all hosts used by this
  // PredictionManager for testing.
  base::flat_map<std::string, base::flat_map<std::string, float>>
  GetHostModelFeaturesForTesting() const;

  // Creates a PredictionModel, virtual for testing.
  virtual std::unique_ptr<PredictionModel> CreatePredictionModel(
      const proto::PredictionModel& model,
      const base::flat_set<std::string>& host_model_features) const;

  // Processes |host_model_features| to be stored in |host_model_features_map|.
  void UpdateHostModelFeatures(
      const google::protobuf::RepeatedPtrField<proto::HostModelFeatures>&
          host_model_features);

  // Processes |prediction_models| to be stored in
  // |optimization_target_prediction_model_map_|.
  void UpdatePredictionModels(
      google::protobuf::RepeatedPtrField<proto::PredictionModel>*
          prediction_models,
      const base::flat_set<std::string>& host_model_features);

 private:
  // Constructs and returns  a map containing the current feature values for the
  // requested set of model features.
  base::flat_map<std::string, float> BuildFeatureMap(
      content::NavigationHandle* navigation_handle,
      const base::flat_set<std::string>& model_features) const;

  // Calculates and returns the current value for the client feature specified
  // by |model_feature|. Returns nullopt if the client does not support the
  // model feature.
  base::Optional<float> GetValueForClientFeature(
      const std::string& model_feature,
      content::NavigationHandle* navigation_handle) const;

  // Called to make a request to fetch models and host model features from the
  // remote Optimization Guide Service. Used to fetch models for the registered
  // optimization targets as well as the host model features for top hosts
  // needed to evaluate these models.
  void FetchModelsAndHostModelFeatures();

  // Called when the models and host model features have been fetched from the
  // remote Optimization Guide Service and are ready for parsing. Processes the
  // prediction models and the host model features in the response and stores
  // them for use.
  void OnModelsAndHostFeaturesFetched(
      base::Optional<std::unique_ptr<proto::GetModelsResponse>>
          get_models_response_data);

  // A map of optimization target to the prediction model capable of making
  // an optimization target decision for it.
  base::flat_map<proto::OptimizationTarget, std::unique_ptr<PredictionModel>>
      optimization_target_prediction_model_map_;

  // The set of optimization targets that have been registered with the
  // prediction manager.
  base::flat_set<proto::OptimizationTarget> registered_optimization_targets_;

  // A map of host to host model features known to the prediction manager.
  //
  // TODO(crbug/1001194): When loading features for the map, the size should be
  // restricted.
  base::flat_map<std::string, base::flat_map<std::string, float>>
      host_model_features_map_;

  // The current session's FCP statistics for HTTP/HTTPS navigations.
  OptimizationGuideSessionStatistic session_fcp_;

  // A float representation of the time to FCP of the previous HTTP/HTTPS page
  // load. This is nullopt when no previous page load exists (the first page
  // load of a session).
  base::Optional<float> previous_load_fcp_ms_;

  // The fetcher than handles making requests to update the models and host
  // model features from the remote Optimization Guide Service.
  std::unique_ptr<PredictionModelFetcher> prediction_model_fetcher_;

  // The top host provider that can be queried. Not owned.
  TopHostProvider* top_host_provider_ = nullptr;

  // The URL loader factory used for fetching model and host feature updates
  // from the remote Optimization Guide Service.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // The current estimate of the EffectiveConnectionType.
  net::EffectiveConnectionType current_effective_connection_type_ =
      net::EffectiveConnectionType::EFFECTIVE_CONNECTION_TYPE_UNKNOWN;

  SEQUENCE_CHECKER(sequence_checker_);

  // Used to get |weak_ptr_| to self on the UI thread.
  base::WeakPtrFactory<PredictionManager> ui_weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(PredictionManager);
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_PREDICTION_PREDICTION_MANAGER_H_
