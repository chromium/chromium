// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/chrome_on_device_model_service_controller.h"

#include "content/public/browser/service_process_host.h"

namespace optimization_guide {

ChromeOnDeviceModelServiceController::ChromeOnDeviceModelServiceController() =
    default;
ChromeOnDeviceModelServiceController::~ChromeOnDeviceModelServiceController() =
    default;

void ChromeOnDeviceModelServiceController::LaunchService() {
  content::ServiceProcessHost::Launch<
      on_device_model::mojom::OnDeviceModelService>(
      service_remote_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("On-Device Model Service")
          .Pass());
}

}  // namespace optimization_guide
