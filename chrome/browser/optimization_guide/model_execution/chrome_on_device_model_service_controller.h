// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include "base/memory/scoped_refptr.h"
#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"

namespace optimization_guide {
class OnDeviceModelComponentStateManager;

// Chrome specialization of service controller, binding launch fn and recording
// synthetic trials for performance class.
class ChromeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  explicit ChromeOnDeviceModelServiceController(
      base::WeakPtr<OnDeviceModelComponentStateManager>
          on_device_component_state_manager);

  void RegisterPerformanceClassSyntheticTrial(
      OnDeviceModelPerformanceClass perf_class) override;

 protected:
 private:
  ~ChromeOnDeviceModelServiceController() override;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
