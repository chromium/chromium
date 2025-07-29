// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/chrome_prediction_model_store.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"

namespace optimization_guide {

// This holds the ModelBrokerState and other common objects shared between
// profiles. Since some of the membersit hold raw_ptr to browser process level
// objects, such as local state prefs, profile manager, it must not outlive the
// browser process, so each profile holds a ref to it in
// OptimizationGuideKeyedService to keep it alive until all profiles are
// destroyed.
class OptimizationGuideGlobalState final
    : public base::RefCounted<OptimizationGuideGlobalState> {
 public:
  // Retrieves or creates the instance.
  static scoped_refptr<OptimizationGuideGlobalState> CreateOrGet();

  OnDeviceModelComponentStateManager& component_state_manager() {
    return model_broker_state_.component_state_manager();
  }

  OnDeviceModelServiceController& service_controller() {
    return model_broker_state_.service_controller();
  }

  ChromePredictionModelStore& prediction_model_store() {
    return prediction_model_store_;
  }

  // Create a new asset manager to provide extra models/configs to the broker.
  std::unique_ptr<OnDeviceAssetManager> CreateAssetManager(
      OptimizationGuideModelProvider* provider) {
    return model_broker_state_.CreateAssetManager(provider);
  }

  void EnsurePerformanceClassAvailable(base::OnceClosure complete) {
    model_broker_state_.performance_classifier()
        .EnsurePerformanceClassAvailable(std::move(complete));
  }

  on_device_model::Capabilities GetPossibleOnDeviceCapabilities() const {
    return model_broker_state_.GetPossibleOnDeviceCapabilities();
  }

 private:
  friend base::RefCounted<OptimizationGuideGlobalState>;
  OptimizationGuideGlobalState();
  ~OptimizationGuideGlobalState();

  ModelBrokerState model_broker_state_;

  ChromePredictionModelStore prediction_model_store_;

  base::WeakPtrFactory<OptimizationGuideGlobalState> weak_ptr_factory_{this};
};

// Chrome uses a single shared instance of ModelBrokerState.
// This retrieves it, or creates it if it doesn't exist yet.
OptimizationGuideGlobalState& GetOrCreateChromeModelBrokerState();

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
