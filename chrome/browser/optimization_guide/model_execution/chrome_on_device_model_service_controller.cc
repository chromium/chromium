// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/chrome_on_device_model_service_controller.h"

#include "base/metrics/field_trial_params.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/service_process_host.h"

namespace optimization_guide {

ChromeOnDeviceModelServiceController::ChromeOnDeviceModelServiceController() =
    default;
ChromeOnDeviceModelServiceController::~ChromeOnDeviceModelServiceController() =
    default;

void ChromeOnDeviceModelServiceController::LaunchService() {
  CHECK(
      base::FeatureList::IsEnabled(features::kOptimizationGuideOnDeviceModel));
  if (service_remote_) {
    return;
  }
  content::ServiceProcessHost::Launch<
      on_device_model::mojom::OnDeviceModelService>(
      service_remote_.BindNewPipeAndPassReceiver(),
      content::ServiceProcessHost::Options()
          .WithDisplayName("On-Device Model Service")
          .Pass());
  service_remote_.reset_on_disconnect();
  service_remote_.set_idle_handler(
      features::GetOnDeviceModelIdleTimeout(),
      base::BindRepeating(
          [](mojo::Remote<on_device_model::mojom::OnDeviceModelService>*
                 remote) { remote->reset(); },
          &service_remote_));
}

}  // namespace optimization_guide
