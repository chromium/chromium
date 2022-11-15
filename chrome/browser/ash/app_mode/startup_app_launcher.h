// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"

class Profile;

namespace ash {

class LacrosLauncher;

// Responsible for the startup of the app for Chrome App kiosk.
class StartupAppLauncher : public KioskAppLauncher,
                           public KioskAppManagerObserver {
 public:
  StartupAppLauncher(Profile* profile,
                     const std::string& app_id,
                     bool should_skip_install,
                     Delegate* delegate);
  StartupAppLauncher(const StartupAppLauncher&) = delete;
  StartupAppLauncher& operator=(const StartupAppLauncher&) = delete;
  ~StartupAppLauncher() override;

 private:
  // Class used to watch for app window creation.
  class AppWindowWatcher;

  // Launch state of the kiosk application
  enum class LaunchState {
    kNotStarted,
    kInitializingNetwork,
    kWaitingForCache,
    kWaitingForLacros,
    kInstallingApp,
    kReadyToLaunch,
    kWaitingForWindow,
    kLaunchSucceeded,
    kLaunchFailed
  };

  // KioskAppLauncher:
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void RestartLauncher() override;
  void LaunchApp() override;

  void BeginInstall();
  void InstallAppInAsh();
  void InstallAppInLacros();
  void OnInstallComplete(ChromeKioskAppInstaller::InstallResult result);
  void OnInstallSuccess();

  void OnLaunchComplete(ChromeKioskAppLauncher::LaunchResult result);

  void OnLaunchSuccess();
  void OnLaunchFailure(KioskAppLaunchError::Error error);

  bool RetryWhenNetworkIsAvailable();
  void OnKioskAppDataLoadStatusChanged(const std::string& app_id);

  // KioskAppManagerObserver overrides.
  void OnKioskExtensionLoadedInCache(const std::string& app_id) override;
  void OnKioskExtensionDownloadFailed(const std::string& app_id) override;

  Profile* const profile_;
  const std::string app_id_;
  const bool should_skip_install_;

  int launch_attempt_ = 0;
  LaunchState state_ = LaunchState::kNotStarted;

  std::unique_ptr<ChromeKioskAppInstaller> installer_;
  std::unique_ptr<LacrosLauncher> lacros_launcher_;
  std::unique_ptr<ChromeKioskAppLauncher> launcher_;

  base::ScopedObservation<KioskAppManagerBase, KioskAppManagerObserver>
      kiosk_app_manager_observation_{this};

  base::WeakPtrFactory<StartupAppLauncher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_
