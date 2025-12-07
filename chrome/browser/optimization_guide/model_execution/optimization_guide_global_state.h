// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/optimization_guide/prediction/chrome_profile_download_service_tracker.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/delivery/prediction_manager.h"
#include "components/optimization_guide/core/delivery/prediction_model_store.h"
#include "components/optimization_guide/core/model_execution/model_broker_state.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_capability.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "services/on_device_model/public/cpp/buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/optimization_guide/core/model_execution/android/model_broker_android.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace optimization_guide {

class ChromeModelComponentStateManagerObserver;
class OptimizationGuideGlobalFeature;

// Constructs and initializes a PredictionManager with it's dependencies.
class ChromePredictionManager {
 public:
  ChromePredictionManager();
  ~ChromePredictionManager();

  PredictionModelStore& prediction_model_store() {
    return prediction_model_store_;
  }
  PredictionManager& prediction_manager() { return prediction_manager_; }

 private:
  PredictionModelStore prediction_model_store_;
  PredictionManager prediction_manager_;
  ChromeProfileDownloadServiceTracker profile_download_service_tracker_;
};

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

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  // This accessor is mainly for the chrome://on-device-internals page and
  // tests.
  ModelBrokerState& model_broker_state() { return on_device_capability_; }
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

  OnDeviceCapability& on_device_capability() { return on_device_capability_; }

  PredictionModelStore& prediction_model_store() {
    return prediction_manager_.prediction_model_store();
  }
  PredictionManager& prediction_manager() {
    return prediction_manager_.prediction_manager();
  }

 private:
  friend base::RefCounted<OptimizationGuideGlobalState>;

  OptimizationGuideGlobalState();
  ~OptimizationGuideGlobalState();

  ChromePredictionManager prediction_manager_;

#if BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)
  ModelBrokerState on_device_capability_;
  std::unique_ptr<ChromeModelComponentStateManagerObserver>
      component_state_manager_observer_;
#elif BUILDFLAG(IS_ANDROID)
  ModelBrokerAndroid on_device_capability_;
#else
  OnDeviceCapability on_device_capability_;
#endif  // BUILDFLAG(USE_ON_DEVICE_MODEL_SERVICE)

  base::WeakPtrFactory<OptimizationGuideGlobalState> weak_ptr_factory_{this};
};

// This is a wrapper around OptimizationGuideGlobalState that keeps a reference
// to the global state. This is needed for these two reasons:
// 1. Some members of OptimizationGuideGlobalState create task runner, which
// necessitates the unittests to use the full TaskEnvironment instead of
// SingleThreadTaskEnvironment.
// 2. Profiles are destroyed after GlobalFeatures, at least in tests. So the
// OptimizationGuideKeyedService needs to keep a reference to the global state
// to keep it alive.
class OptimizationGuideGlobalFeature {
 public:
  OptimizationGuideGlobalFeature();
  ~OptimizationGuideGlobalFeature();

  OptimizationGuideGlobalState& Get();

  OptimizationGuideModelProvider& GetModelProvider();

 private:
  scoped_refptr<OptimizationGuideGlobalState> global_state_;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_OPTIMIZATION_GUIDE_GLOBAL_STATE_H_
