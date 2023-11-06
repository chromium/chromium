// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

namespace optimization_guide {

class ChromeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  ChromeOnDeviceModelServiceController();
  ~ChromeOnDeviceModelServiceController() override;

 private:
  // OnDeviceModelServiceController implementation:
  void LaunchService() override;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
