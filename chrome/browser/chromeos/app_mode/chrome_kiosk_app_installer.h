// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_INSTALLER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_INSTALLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/extensions/forced_extensions/install_stage_tracker.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "chromeos/crosapi/mojom/chrome_app_kiosk_service.mojom.h"
#include "extensions/common/extension_id.h"

class Profile;

namespace chromeos {

class ChromeKioskAppInstaller
    : private extensions::InstallObserver,
      public extensions::InstallStageTracker::Observer {
 public:
  using InstallResult = crosapi::mojom::ChromeKioskInstallResult;
  using AppInstallParams = crosapi::mojom::AppInstallParams;
  using InstallCallback =
      crosapi::mojom::ChromeKioskLaunchController::InstallKioskAppCallback;

  ChromeKioskAppInstaller(Profile* profile,
                          const AppInstallParams& install_data);
  ChromeKioskAppInstaller(const ChromeKioskAppInstaller&) = delete;
  ChromeKioskAppInstaller& operator=(const ChromeKioskAppInstaller&) = delete;
  ~ChromeKioskAppInstaller() override;

  void BeginInstall(InstallCallback callback);

 private:
  void MaybeInstallSecondaryApps(const extensions::Extension& primary_app);
  void MaybeCheckExtensionUpdate();
  void OnExtensionUpdateCheckFinished(bool update_found);
  void FinalizeAppInstall();

  // extensions::InstallObserver overrides.
  void OnFinishCrxInstall(content::BrowserContext* context,
                          const base::FilePath& source_file,
                          const std::string& extension_id,
                          const extensions::Extension* extension,
                          bool success) override;

  // extensions::InstallStageTracker::Observer overrides.
  void OnExtensionInstallationFailed(
      const extensions::ExtensionId& id,
      extensions::InstallStageTracker::FailureReason reason) override;

  void ReportInstallSuccess();
  void ReportInstallFailure(InstallResult result);

  // Observes `InstallTracker` until the given `ids` finish installing.
  void ObserveInstallations(const std::vector<extensions::ExtensionId>& ids);

  const extensions::ExtensionId& primary_app_id() const {
    return primary_app_install_data_.id;
  }

  raw_ref<Profile> profile_;
  AppInstallParams primary_app_install_data_;

  // The set of extension IDs to wait for in `OnFinishCrxInstall`. This includes
  // the primary Chrome app, its secondary apps, as well as any shared modules
  // they import.
  base::flat_set<extensions::ExtensionId> waiting_ids_;

  InstallCallback on_ready_callback_;

  bool install_complete_ = false;
  bool secondary_apps_installing_ = false;

  // Will be true if an update (not an install) of the primary app fails.
  bool primary_app_update_failed_ = false;
  // Will be true if an update (not an install) of a secondary app fails.
  bool secondary_app_update_failed_ = false;

  base::Time extension_update_start_time_;

  // Used to run extension update checks for primary app's imports and
  // secondary extensions.
  std::unique_ptr<StartupAppLauncherUpdateChecker> update_checker_;

  base::ScopedObservation<extensions::InstallTracker,
                          extensions::InstallObserver>
      install_observation_{this};
  base::ScopedObservation<extensions::InstallStageTracker,
                          extensions::InstallStageTracker::Observer>
      install_stage_observation_{this};
  base::WeakPtrFactory<ChromeKioskAppInstaller> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_CHROME_KIOSK_APP_INSTALLER_H_
