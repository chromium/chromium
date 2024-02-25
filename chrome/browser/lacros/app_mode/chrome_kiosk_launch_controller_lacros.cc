// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/chrome_kiosk_launch_controller_lacros.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/notreached.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"

ChromeKioskLaunchControllerLacros::ChromeKioskLaunchControllerLacros(
    Profile& profile)
    : profile_(profile) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::ChromeAppKioskService>()) {
    return;
  }

  service->GetRemote<crosapi::mojom::ChromeAppKioskService>()
      ->BindLaunchController(
          controller_receiver_.BindNewPipeAndPassRemoteWithVersion());
}

ChromeKioskLaunchControllerLacros::~ChromeKioskLaunchControllerLacros() =
    default;

void ChromeKioskLaunchControllerLacros::InstallKioskApp(
    AppInstallParamsPtr params,
    InstallKioskAppCallback callback) {
  installer_ =
      std::make_unique<chromeos::ChromeKioskAppInstaller>(&*profile_, *params);
  installer_->BeginInstall(std::move(callback));
}

void ChromeKioskLaunchControllerLacros::LaunchKioskApp(
    const std::string& app_id,
    bool is_network_ready,
    LaunchKioskAppCallback callback) {
  launcher_ = std::make_unique<chromeos::ChromeKioskAppLauncher>(
      &*profile_, app_id, is_network_ready);
  launcher_->LaunchApp(std::move(callback));
}
