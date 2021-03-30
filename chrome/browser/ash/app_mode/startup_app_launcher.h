// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/extensions/install_observer.h"
#include "chrome/browser/extensions/install_tracker.h"
#include "extensions/browser/app_window/app_window_registry.h"

class Profile;

namespace extensions {
class AppWindowRegistry;
}

namespace ash {

class StartupAppLauncherUpdateChecker;

// Responsible for the startup of the app for Chrome App kiosk.
class StartupAppLauncher : public KioskAppLauncher,
                           public extensions::InstallObserver,
                           public KioskAppManagerObserver,
                           public extensions::AppWindowRegistry::Observer {
 public:
  StartupAppLauncher(Profile* profile,
                     const std::string& app_id,
                     Delegate* delegate);

  ~StartupAppLauncher() override;

 private:
  // Class used to watch for app window creation.
  class AppWindowWatcher;

  // KioskAppLauncher:
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void LaunchApp() override;
  void RestartLauncher() override;

  void OnLaunchSuccess();
  void OnLaunchFailure(KioskAppLaunchError::Error error);

  void BeginInstall();
  void OnReadyToLaunch();
  void MaybeUpdateAppData();

  void MaybeInitializeNetwork();
  void MaybeInstallSecondaryApps();
  void SetSecondaryAppsEnabledState(const extensions::Extension* primary_app);
  void MaybeLaunchApp();

  void MaybeCheckExtensionUpdate();
  void OnExtensionUpdateCheckFinished(bool update_found);

  void OnKioskAppDataLoadStatusChanged(const std::string& app_id);

  // AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;

  // Returns true if any secondary app is pending.
  bool IsAnySecondaryAppPending() const;

  // Returns true if all secondary apps have been installed.
  bool AreSecondaryAppsInstalled() const;

  // Returns true if secondary apps are declared in manifest.
  bool HasSecondaryApps() const;

  // Returns true if the primary app has a pending update.
  bool PrimaryAppHasPendingUpdate() const;

  // Returns true if the app with |id| failed, and it is the primary or one of
  // the secondary apps.
  bool DidPrimaryOrSecondaryAppFailedToInstall(bool success,
                                               const std::string& id) const;

  const extensions::Extension* GetPrimaryAppExtension() const;

  // extensions::InstallObserver overrides.
  void OnFinishCrxInstall(const std::string& extension_id,
                          bool success) override;

  // KioskAppManagerObserver overrides.
  void OnKioskExtensionLoadedInCache(const std::string& app_id) override;
  void OnKioskExtensionDownloadFailed(const std::string& app_id) override;

  Profile* const profile_;
  const std::string app_id_;
  bool network_ready_handled_ = false;
  int launch_attempt_ = 0;
  bool ready_to_launch_ = false;
  bool wait_for_crx_update_ = false;
  bool secondary_apps_installed_ = false;
  bool waiting_for_window_ = false;

  // Used to run extension update checks for primary app's imports and
  // secondary extensions.
  std::unique_ptr<StartupAppLauncherUpdateChecker> update_checker_;

  extensions::AppWindowRegistry* window_registry_;

  ScopedObserver<KioskAppManagerBase, KioskAppManagerObserver>
      kiosk_app_manager_observer_{this};

  ScopedObserver<extensions::InstallTracker, extensions::InstallObserver>
      install_observer_{this};

  base::WeakPtrFactory<StartupAppLauncher> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(StartupAppLauncher);
};

}  // namespace ash

// TODO(https://crbug.com/1164001): remove when the //chrome/browser/chromeos
// migration is finished.
namespace chromeos {
using ::ash::StartupAppLauncher;
}

#endif  // CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_
