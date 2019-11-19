// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/manifest.h"

#include "ash/public/mojom/assistant_controller.mojom.h"
#include "ash/public/mojom/assistant_volume_control.mojom.h"
#include "ash/public/mojom/constants.mojom.h"
#include "ash/public/mojom/cros_display_config.mojom.h"
#include "ash/public/mojom/ime_controller.mojom.h"
#include "ash/public/mojom/tray_action.mojom.h"
#include "base/no_destructor.h"
#include "chromeos/services/multidevice_setup/public/mojom/constants.mojom.h"
#include "services/content/public/mojom/constants.mojom.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/preferences/public/mojom/preferences.mojom.h"
#include "services/service_manager/public/cpp/manifest_builder.h"

namespace ash {

service_manager::Manifest& GetAmendmentForTesting() {
  static base::NoDestructor<service_manager::Manifest> amendment;
  return *amendment;
}

const service_manager::Manifest& GetManifest() {
  static base::NoDestructor<service_manager::Manifest> manifest{
      service_manager::ManifestBuilder()
          .WithServiceName(mojom::kServiceName)
          .WithDisplayName("Ash Window Manager and Shell")
          .WithOptions(service_manager::ManifestOptionsBuilder()
                           .WithSandboxType("none")
                           .WithInstanceSharingPolicy(
                               service_manager::Manifest::
                                   InstanceSharingPolicy::kSingleton)
                           .Build())
          .ExposeCapability("system_ui",
                            service_manager::Manifest::InterfaceList<
                                mojom::CrosDisplayConfigController,
                                mojom::ImeController, mojom::TrayAction>())
          .RequireCapability("*", "accessibility")
          .RequireCapability("*", "app")
          .RequireCapability(content::mojom::kServiceName, "navigation")
          .RequireCapability(device::mojom::kServiceName,
                             "device:bluetooth_system")
          .RequireCapability(device::mojom::kServiceName, "device:fingerprint")
          .RequireCapability(chromeos::multidevice_setup::mojom::kServiceName,
                             "multidevice_setup")
          .Build()
          .Amend(GetAmendmentForTesting())};
  return *manifest;
}

void AmendManifestForTesting(const service_manager::Manifest& amendment) {
  GetAmendmentForTesting() = amendment;
}

}  // namespace ash
