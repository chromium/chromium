// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_CHROME_APP_KIOSK_APP_INSTALLER_H_
#define CHROME_BROWSER_ASH_APP_MODE_CHROME_APP_KIOSK_APP_INSTALLER_H_

#include "base/callback_forward.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/startup_app_launcher_update_checker.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"

class Profile;

namespace ash {

using InstallCallback =
    base::OnceCallback<void(KioskAppLaunchError::Error error)>;

class ChromeAppKioskAppInstaller : private extensions::InstallObserver {
 public:
  ChromeAppKioskAppInstaller(Profile* profile,
                             const std::string& app_id,
                             KioskAppLauncher::Delegate* delegate);
  ChromeAppKioskAppInstaller(const ChromeAppKioskAppInstaller&) = delete;
  ChromeAppKioskAppInstaller& operator=(const ChromeAppKioskAppInstaller&) =
      delete;
  ~ChromeAppKioskAppInstaller() override;

  void BeginInstall(InstallCallback callback);

  void SetInstallComplete() { install_complete_ = true; }
  bool install_complete() const { return install_complete_; }

 private:
  void MaybeInstallSecondaryApps();
  void MaybeCheckExtensionUpdate();
  void OnExtensionUpdateCheckFinished(bool update_found);
  void FinalizeAppInstall();

  // extensions::InstallObserver overrides.
  void OnFinishCrxInstall(const std::string& extension_id,
                          bool success) override;

  void ReportInstallFailure(KioskAppLaunchError::Error error);
  void RetryWhenNetworkIsAvailable(base::OnceClosure callback);
  void ObserveActiveInstallations();

  const extensions::Extension* GetPrimaryAppExtension() const;

  // Returns true if all secondary apps have been installed.
  bool AreSecondaryAppsInstalled() const;

  // Returns true if the app with id |id| is pending an install.
  bool IsAppInstallPending(const std::string& id) const;

  // Returns true if any secondary app is pending.
  bool IsAnySecondaryAppPending() const;

  // Returns true if the app with |id| failed, and it is the primary or one of
  // the secondary apps.
  bool DidPrimaryOrSecondaryAppFailedToInstall(bool success,
                                               const std::string& id) const;

  Profile* const profile_;
  const std::string app_id_;
  KioskAppLauncher::Delegate* delegate_;

  InstallCallback on_ready_callback_;

  bool install_complete_ = false;
  bool secondary_apps_installed_ = false;

  // Used to run extension update checks for primary app's imports and
  // secondary extensions.
  std::unique_ptr<StartupAppLauncherUpdateChecker> update_checker_;

  base::ScopedObservation<extensions::InstallTracker,
                          extensions::InstallObserver>
      install_observation_{this};
  base::WeakPtrFactory<ChromeAppKioskAppInstaller> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_CHROME_APP_KIOSK_APP_INSTALLER_H_
