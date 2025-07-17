// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_MODEL_BROKER_STATE_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_MODEL_BROKER_STATE_H_

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/core/model_execution/on_device_asset_manager.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"

namespace optimization_guide {

// This holds the ModelBrokerState shared between profiles.
// Since it holds raw_ptr to local state prefs, it must not outlive the
// browser process, so each profile holds a ref to it in
// OptimizationGuideKeyedService to keep it alive until all profiles are
// destroyed.
class ChromeModelBrokerState final
    : public base::RefCounted<ChromeModelBrokerState> {
 public:
  // Retrieves or creates the instance.
  static scoped_refptr<ChromeModelBrokerState> CreateOrGet();

  OnDeviceModelComponentStateManager& component_state_manager() {
    return *component_state_manager_;
  }

  scoped_refptr<OnDeviceModelServiceController> service_controller() {
    return service_controller_;
  }

  // Create a new asset manager to provide extra models/configs to the broker.
  std::unique_ptr<OnDeviceAssetManager> CreateAssetManager(
      OptimizationGuideModelProvider* provider);

 private:
  friend base::RefCounted<ChromeModelBrokerState>;
  ChromeModelBrokerState();
  ~ChromeModelBrokerState();

  scoped_refptr<OnDeviceModelComponentStateManager> component_state_manager_;
  scoped_refptr<OnDeviceModelServiceController> service_controller_;

  base::WeakPtrFactory<ChromeModelBrokerState> weak_ptr_factory_{this};
};

// Chrome uses a single shared instance of ModelBrokerState.
// This retrieves it, or creates it if it doesn't exist yet.
ChromeModelBrokerState& GetOrCreateChromeModelBrokerState();

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_MODEL_BROKER_STATE_H_
