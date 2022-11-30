// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/chrome_app_kiosk_service_ash.h"

#include "base/logging.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"

namespace crosapi {

ChromeAppKioskServiceAsh::ChromeAppKioskServiceAsh() = default;
ChromeAppKioskServiceAsh::~ChromeAppKioskServiceAsh() = default;

void ChromeAppKioskServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::ChromeAppKioskService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void ChromeAppKioskServiceAsh::BindLaunchController(
    mojo::PendingRemote<mojom::ChromeKioskLaunchController> launch_controller) {
  launch_controllers_.Add(std::move(launch_controller));
}

void ChromeAppKioskServiceAsh::InstallKioskApp(
    const mojom::AppInstallParams& params,
    mojom::ChromeKioskLaunchController::InstallKioskAppCallback callback) {
  if (!GetController()) {
    std::move(callback).Run(mojom::ChromeKioskInstallResult::kUnknown);
    return;
  }

  GetController()->InstallKioskApp(params.Clone(), std::move(callback));
}

void ChromeAppKioskServiceAsh::LaunchKioskApp(
    std::string app_id,
    bool is_network_ready,
    mojom::ChromeKioskLaunchController::LaunchKioskAppCallback callback) {
  if (!GetController()) {
    std::move(callback).Run(mojom::ChromeKioskLaunchResult::kUnknown);
    return;
  }

  GetController()->LaunchKioskApp(app_id, is_network_ready,
                                  std::move(callback));
}

mojom::ChromeKioskLaunchController* ChromeAppKioskServiceAsh::GetController() {
  if (launch_controllers_.empty()) {
    LOG(WARNING) << "Lacros installer has not been bound";
    return nullptr;
  }

  return launch_controllers_.begin()->get();
}

}  // namespace crosapi
