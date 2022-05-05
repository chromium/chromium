// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/chrome_kiosk_launch_controller_lacros.h"
#include <memory>

#include "base/bind.h"
#include "base/notreached.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"

ChromeKioskLaunchControllerLacros::ChromeKioskLaunchControllerLacros() {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::ChromeAppKioskService>())
    return;

  service->GetRemote<crosapi::mojom::ChromeAppKioskService>()
      ->BindLaunchController(
          controller_receiver_.BindNewPipeAndPassRemoteWithVersion());
}

ChromeKioskLaunchControllerLacros::~ChromeKioskLaunchControllerLacros() {}

void ChromeKioskLaunchControllerLacros::InstallKioskApp(
    AppInstallParamsPtr params,
    InstallKioskAppCallback callback) {
  NOTIMPLEMENTED();
}

void ChromeKioskLaunchControllerLacros::LaunchKioskApp(
    const std::string& app_id,
    bool is_network_ready,
    LaunchKioskAppCallback callback) {
  NOTIMPLEMENTED();
}
