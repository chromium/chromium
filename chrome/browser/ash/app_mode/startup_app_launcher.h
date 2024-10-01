// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_
#define CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager_observer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_installer.h"
#include "chrome/browser/chromeos/app_mode/chrome_kiosk_app_launcher.h"

class Profile;

namespace ash {

// Responsible for the startup of the app for Chrome App kiosk.
class StartupAppLauncher : public KioskAppLauncher,
                           public KioskAppManagerObserver {
 public:
  StartupAppLauncher(Profile* profile,
                     const std::string& app_id,
                     bool should_skip_install,
                     NetworkDelegate* network_delegate);
  StartupAppLauncher(const StartupAppLauncher&) = delete;
  StartupAppLauncher& operator=(const StartupAppLauncher&) = delete;
  ~StartupAppLauncher() override;

 private:
  // Launch state of the kiosk application
  enum class LaunchState {
    kNotStarted,
    kInitializingNetwork,
    kWaitingForCache,
    kInstallingApp,
    kReadyToLaunch,
    kWaitingForWindow,
    kLaunchSucceeded,
    kLaunchFailed
  };

  // `KioskAppLauncher`:
  void AddObserver(KioskAppLauncher::Observer* observer) override;
  void RemoveObserver(KioskAppLauncher::Observer* observer) override;
  void Initialize() override;
  void ContinueWithNetworkReady() override;
  void LaunchApp() override;

  void BeginInstall();
  void OnInstallComplete(
      chromeos::ChromeKioskAppInstaller::InstallResult result);
  void OnInstallSuccess();

  void OnLaunchComplete(chromeos::ChromeKioskAppLauncher::LaunchResult result);

  void OnLaunchSuccess();
  void OnLaunchFailure(KioskAppLaunchError::Error error);

  bool RetryWhenNetworkIsAvailable();
  void OnKioskAppDataLoadStatusChanged(const std::string& app_id);

  // KioskAppManagerObserver overrides.
  void OnKioskExtensionLoadedInCache(const std::string& app_id) override;
  void OnKioskExtensionDownloadFailed(const std::string& app_id) override;

  const raw_ptr<Profile> profile_;
  const std::string app_id_;
  const bool should_skip_install_;

  int launch_attempt_ = 0;
  LaunchState state_ = LaunchState::kNotStarted;

  KioskAppLauncher::ObserverList observers_;
  std::unique_ptr<chromeos::ChromeKioskAppInstaller> installer_;
  std::unique_ptr<chromeos::ChromeKioskAppLauncher> launcher_;

  base::ScopedObservation<KioskAppManagerBase, KioskAppManagerObserver>
      kiosk_app_manager_observation_{this};

  base::WeakPtrFactory<StartupAppLauncher> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_STARTUP_APP_LAUNCHER_H_
