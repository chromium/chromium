// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/chrome_on_device_model_service_controller.h"

#include "chrome/browser/browser_process.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/service_process_host.h"

namespace optimization_guide {

namespace {

ChromeOnDeviceModelServiceController* g_instance = nullptr;

}  // namespace

ChromeOnDeviceModelServiceController::ChromeOnDeviceModelServiceController(
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : OnDeviceModelServiceController(
          std::make_unique<OnDeviceModelAccessController>(
              *g_browser_process->local_state()),
          std::move(on_device_component_state_manager)) {
  CHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

// static
ChromeOnDeviceModelServiceController*
ChromeOnDeviceModelServiceController::GetSingleInstanceMayBeNull() {
  return g_instance;
}

ChromeOnDeviceModelServiceController::~ChromeOnDeviceModelServiceController() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

void ChromeOnDeviceModelServiceController::LaunchService() {
  CHECK(features::CanLaunchOnDeviceModelService());
  if (service_remote_) {
    return;
  }
  auto receiver = service_remote_.BindNewPipeAndPassReceiver();
  service_remote_.reset_on_disconnect();
  service_remote_.reset_on_idle_timeout(base::TimeDelta());
  content::ServiceProcessHost::Launch<
      on_device_model::mojom::OnDeviceModelService>(
      std::move(receiver), content::ServiceProcessHost::Options()
                               .WithDisplayName("On-Device Model Service")
                               .Pass());
}

}  // namespace optimization_guide
