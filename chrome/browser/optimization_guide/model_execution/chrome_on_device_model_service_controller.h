// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
#define CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_

#include "components/optimization_guide/core/model_execution/on_device_model_service_controller.h"

namespace optimization_guide {

// Chrome uses a single instance of OnDeviceModelServiceController. This is done
// for two reasons:
// . We only want to load the model once, not once per Profile. To do otherwise
//   would consume a significant amount of memory.
// . To ensure we don't double count the number of crashes (if each profile had
//   it's own connection, then the number of crashes would be double what
//   actually happened).
class ChromeOnDeviceModelServiceController
    : public OnDeviceModelServiceController {
 public:
  ChromeOnDeviceModelServiceController();

  // Returns the OnDeviceModelServiceController, null if it one hasn't been
  // created yet.
  static ChromeOnDeviceModelServiceController* GetSingleInstanceMayBeNull();

 private:
  ~ChromeOnDeviceModelServiceController() override;

  // OnDeviceModelServiceController implementation:
  void LaunchService() override;
};

}  // namespace optimization_guide

#endif  // CHROME_BROWSER_OPTIMIZATION_GUIDE_MODEL_EXECUTION_CHROME_ON_DEVICE_MODEL_SERVICE_CONTROLLER_H_
