// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_LAUNCHER_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_service_launcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "extensions/common/extension.h"

namespace chromeos {

class ChromeKioskAppLauncher : public extensions::AppWindowRegistry::Observer {
 public:
  using LaunchResult = crosapi::mojom::ChromeKioskLaunchResult;
  using LaunchCallback =
      crosapi::mojom::ChromeKioskLaunchController::LaunchKioskAppCallback;

  ChromeKioskAppLauncher(Profile* profile,
                         const std::string& app_id,
                         bool network_available);
  ChromeKioskAppLauncher(const ChromeKioskAppLauncher&) = delete;
  ChromeKioskAppLauncher& operator=(const ChromeKioskAppLauncher&) = delete;
  ~ChromeKioskAppLauncher() override;

  void LaunchApp(LaunchCallback callback);

 private:
  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;

  // `KioskAppServiceLauncher` callback.
  void OnAppServiceAppLaunched(bool success);

  void WaitForAppWindow();

  void ReportLaunchSuccess();
  void ReportLaunchFailure(LaunchResult result);

  const extensions::Extension* GetPrimaryAppExtension() const;

  bool PrimaryAppHasPendingUpdate() const;

  // Returns true if all secondary apps have been installed.
  bool AreSecondaryAppsInstalled() const;

  void MaybeUpdateAppData();
  void SetSecondaryAppsEnabledState(const extensions::Extension* primary_app);
  void SetAppEnabledState(const extensions::ExtensionId& id,
                          bool should_be_enabled);

  const raw_ptr<Profile> profile_;
  std::string app_id_;
  bool network_available_ = false;

  base::ScopedObservation<extensions::AppWindowRegistry,
                          extensions::AppWindowRegistry::Observer>
      app_window_observation_{this};

  std::unique_ptr<KioskAppServiceLauncher> app_service_launcher_;

  LaunchCallback on_ready_callback_;

  base::WeakPtrFactory<ChromeKioskAppLauncher> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_LAUNCHER_H_
