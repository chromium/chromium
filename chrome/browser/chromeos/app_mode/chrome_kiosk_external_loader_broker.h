// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_EXTERNAL_LOADER_BROKER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_EXTERNAL_LOADER_BROKER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/values.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"

namespace chromeos {

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
  void TriggerSecondaryAppInstall(
      const std::vector<std::string>& secondary_app_ids);

 private:
  void CallPrimaryAppObserver();
  void CallSecondaryAppObserver();

  std::optional<crosapi::mojom::AppInstallParams> primary_app_data_;
  std::optional<std::vector<std::string>> secondary_app_ids_;

  // Handle to the primary app external loader.
  InstallDataChangeCallback primary_app_observer_;

  // Handle to the secondary app external loader.
  InstallDataChangeCallback secondary_apps_observer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_EXTERNAL_LOADER_BROKER_H_
