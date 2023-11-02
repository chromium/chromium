// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_EXTERNAL_LOADER_BROKER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_EXTERNAL_LOADER_BROKER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/values.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

// Singleton broker that stands in the middle between the
// ChromeKioskAppInstaller and the KioskAppExternalLoader. The external
// loader registers here when created and will be triggered by the app installer
// when the apps to be installed are known.
class ChromeKioskExternalLoaderBroker {
 public:
  using InstallDataChangeCallback =
      base::RepeatingCallback<void(base::Value::Dict)>;

  static ChromeKioskExternalLoaderBroker* Get();

  static void Shutdown();

  ChromeKioskExternalLoaderBroker();
  ChromeKioskExternalLoaderBroker(const ChromeKioskExternalLoaderBroker&) =
      delete;
  ChromeKioskExternalLoaderBroker& operator=(
      const ChromeKioskExternalLoaderBroker&) = delete;
  ~ChromeKioskExternalLoaderBroker();

  void RegisterPrimaryAppInstallDataObserver(
      InstallDataChangeCallback callback);
  void RegisterSecondaryAppInstallDataObserver(
      InstallDataChangeCallback callback);

  void TriggerPrimaryAppInstall(
      const crosapi::mojom::AppInstallParams& install_data);
  void TriggerSecondaryAppInstall(std::vector<std::string> secondary_app_ids);

 private:
  base::Value::Dict CreatePrimaryAppLoaderPrefs() const;
  base::Value::Dict CreateSecondaryAppLoaderPrefs() const;

  absl::optional<crosapi::mojom::AppInstallParams> primary_app_install_data_;
  absl::optional<std::vector<std::string>> secondary_app_ids_;

  // Handle to the primary app external loader.
  InstallDataChangeCallback primary_app_changed_handler_;

  // Handle to the secondary app external loader.
  InstallDataChangeCallback secondary_apps_changed_handler_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_EXTERNAL_LOADER_BROKER_H_
