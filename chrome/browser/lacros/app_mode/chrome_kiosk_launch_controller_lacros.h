// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_APP_MODE_CHROME_KIOSK_LAUNCH_CONTROLLER_LACROS_H_
#define CHROME_BROWSER_LACROS_APP_MODE_CHROME_KIOSK_LAUNCH_CONTROLLER_LACROS_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"

using crosapi::mojom::AppInstallParamsPtr;
using InstallKioskAppCallback =
    crosapi::mojom::ChromeKioskLaunchController::InstallKioskAppCallback;
using LaunchKioskAppCallback =
    crosapi::mojom::ChromeKioskLaunchController::LaunchKioskAppCallback;

// Manage the kiosk session and related resources at the lacros side.
class ChromeKioskLaunchControllerLacros
    : public crosapi::mojom::ChromeKioskLaunchController {
 public:
  explicit ChromeKioskLaunchControllerLacros(Profile& profile);
  ChromeKioskLaunchControllerLacros(const ChromeKioskLaunchControllerLacros&) =
      delete;
  ChromeKioskLaunchControllerLacros& operator=(
      const ChromeKioskLaunchControllerLacros&) = delete;
  ~ChromeKioskLaunchControllerLacros() override;

  // crosapi::mojom::ChromeAppKioskInstaller
  void InstallKioskApp(AppInstallParamsPtr params,
                       InstallKioskAppCallback callback) override;
  void LaunchKioskApp(const std::string& app_id,
                      bool is_network_ready,
                      LaunchKioskAppCallback callback) override;

 private:
  const raw_ref<Profile> profile_;
  std::unique_ptr<chromeos::ChromeKioskAppInstaller> installer_;
  std::unique_ptr<chromeos::ChromeKioskAppLauncher> launcher_;

  mojo::Receiver<crosapi::mojom::ChromeKioskLaunchController>
      controller_receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_APP_MODE_CHROME_KIOSK_LAUNCH_CONTROLLER_LACROS_H_
