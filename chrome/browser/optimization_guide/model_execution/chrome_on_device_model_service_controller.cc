// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/optimization_guide/model_execution/chrome_on_device_model_service_controller.h"

#include "chrome/browser/browser_process.h"
#include "components/optimization_guide/core/model_execution/on_device_model_access_controller.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "content/public/browser/service_process_host.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace optimization_guide {

namespace {

ChromeOnDeviceModelServiceController* g_instance = nullptr;

void LaunchService(
    mojo::PendingReceiver<on_device_model::mojom::OnDeviceModelService>
        pending_receiver) {
  CHECK(features::CanLaunchOnDeviceModelService());
  content::ServiceProcessHost::Launch<
      on_device_model::mojom::OnDeviceModelService>(
      std::move(pending_receiver),
      content::ServiceProcessHost::Options()
          .WithDisplayName("On-Device Model Service")
          .Pass());
}

}  // namespace

ChromeOnDeviceModelServiceController::ChromeOnDeviceModelServiceController(
    base::WeakPtr<OnDeviceModelComponentStateManager>
        on_device_component_state_manager)
    : OnDeviceModelServiceController(
          std::make_unique<OnDeviceModelAccessController>(
              *g_browser_process->local_state()),
          std::move(on_device_component_state_manager),
          base::BindRepeating(&LaunchService)) {
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

}  // namespace optimization_guide
