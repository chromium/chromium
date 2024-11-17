// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "ash/public/cpp/login_accelerators.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/app_mode/cancellable_job.h"
#include "chrome/browser/ash/app_mode/kiosk_app.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/load_profile.h"
#include "chrome/browser/ash/login/app_mode/force_install_observer.h"
#include "chrome/browser/ash/login/app_mode/network_ui_controller.h"
#include "chrome/browser/ash/login/screens/app_launch_splash_screen.h"

namespace app_mode {
class ForceInstallObserver;
}  // namespace app_mode

namespace ash {

class KioskProfileLoadFailedObserver;
class KioskTestHelper;
class LoginDisplayHost;

extern const base::TimeDelta kDefaultKioskSplashScreenMinTime;

// Controller for the kiosk launch process, responsible for loading the kiosk
// profile, and updating the splash screen UI.
//
// Splash screen has several specific things the are considered during it's
// implementation:
// 1. There is a timer, which shows the splash screen for at least 10 seconds to
//    allow used to exit the mode.
// 2. User can at any moment open network configuration menu using a shortcut
//    CTRL+ALT+N to set up the network before the app actually starts.
//
// These are taken into consideration while designing the logic of
// KioskLaunchController.
//
// The launch process involves following steps:
// 1. Perform cryptohome login operations and initialize the profile.
// 2. Initialize KioskAppLauncher and wait for it to emit action depending on
//    the current app installation state(e.g. to install the app the
//    launcher will call KioskLaunchController::InitializeNetwork)
// 3. KioskLaunchController waits for network to be ready and when it is, it
//    calls KioskAppLauncher::ContinueWithNetworkReady() to continue the
//    installation.
// 4. After the app is installed, KioskLaunchController is waiting for the
//    splash screen timer to fire. (We can also end up in this state when the
//    app was already installed).
// 5. KioskLaunchController waits until all force-installed extensions are
//    ready. This step will stops if the time runs out.
// 6. KioskLaunchController launches the app by calling
//    KioskAppLauncher::LaunchApp.
//
// At any moment the user can press the shortcut to configure network setup, so
// we need to also include the logic for the configuration menu.
// Besides, we should be always sure that while configuring network we are using
// the correct profile(kiosk app profile, not sign in profile). We will postpone
// the configuration if the shortcut is pressed to early.
//
// It is all encompassed within the combination of two states -- AppState and
// NetworkUI state.
class KioskLaunchController : public KioskAppLauncher::Observer,
                              public NetworkUiController::Observer {
 public:
  class AcceleratorController {
   public:
    virtual ~AcceleratorController() = default;
    virtual void EnableAccelerators() = 0;
    virtual void DisableAccelerators() = 0;
  };

  // Factory class that constructs a `KioskAppLauncher`.
  // The default implementation constructs the correct implementation of
  // `KioskAppLauncher` based on the kiosk type associated with `KioskAppId`.
  using KioskAppLauncherFactory =
      base::RepeatingCallback<std::unique_ptr<KioskAppLauncher>(
          Profile*,
          const KioskAppId&,
          KioskAppLauncher::NetworkDelegate*)>;

  // Callback invoked when the app window is created behind the splash screen,
  // signifying the application has launched and is ready.
  //
  // The overall launch is not complete yet at this stage, the splash screen is
  // still visible potentially for several seconds longer.
  //
  // May not be called in case of errors. If it is called, it is guaranteed to
  // be called before `LaunchCompleteCallback`.
  using AppLaunchedCallback =
      base::OnceCallback<void(const KioskAppId& app,
                              Profile* profile,
                              const std::optional<std::string>& app_name)>;

  // Callback invoked when the launch finished, either successfully or aborted
  // due to an error.
  using LaunchCompleteCallback =
      base::OnceCallback<void(KioskAppLaunchError::Error error)>;

  KioskLaunchController(LoginDisplayHost* host,
                        AppLaunchedCallback app_launched_callback,
                        AppLaunchSplashScreen* splash_screen,
                        LaunchCompleteCallback done_callback);
  KioskLaunchController(
      LoginDisplayHost* host,
      AppLaunchSplashScreen* splash_screen,
      kiosk::LoadProfileCallback profile_loader,
      AppLaunchedCallback app_launched_callback,
      LaunchCompleteCallback done_callback,
      base::OnceClosure attempt_relaunch,
      base::OnceClosure attempt_logout,
      KioskAppLauncherFactory app_launcher_factory,
      std::unique_ptr<NetworkUiController::NetworkMonitor> network_monitor,
      std::unique_ptr<AcceleratorController> accelerator_controller);
  KioskLaunchController(const KioskLaunchController&) = delete;
  KioskLaunchController& operator=(const KioskLaunchController&) = delete;
  ~KioskLaunchController() override;

  void Start(KioskApp app, bool auto_launch);

  void AddKioskProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer);

  void RemoveKioskProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer);

  bool HandleAccelerator(LoginAcceleratorAction action);

  // `NetworkUiController::Observer`:
  void OnNetworkConfigureUiShowing() override;
  void OnNetworkConfigureUiFinished() override;
  void OnNetworkReady() override;
  void OnNetworkLost() override;

  // Currently required for testing
  NetworkUiController::NetworkUIState GetNetworkUiStateForTesting() const {
    return network_ui_controller_->GetNetworkUiStateForTesting();
  }
  NetworkUiController* GetNetworkUiControllerForTesting();

  // Overrides of the launch behavior during testing.
  // Values here can only be modified through the `KioskTestHelper` class.
  class TestOverrides {
   private:
    friend class KioskLaunchController;
    friend class KioskTestHelper;

    // Whether we should skip the wait for minimum screen show time.
    static bool skip_splash_wait;
    static bool block_app_launch;
    // Whether we should prevent Kiosk launcher from exiting when launch fails.
    static bool block_exit_on_failure;
  };

 private:
  friend class KioskLaunchControllerTest;

  class ScopedAcceleratorDisabler;

  enum AppState {
    kCreatingProfile = 0,  // Profile is being created.
    kInitLauncher,          // Launcher is initializing
    kInstallingApp,         // App is being installed.
    kInstallingExtensions,  // Force-installed extensions are being installed.
    kInstalled,  // Everything is installed, waiting for the splash screen timer
                 // to fire.
    kLaunched,   // App is being launched.
    kInitNetwork,  // Waiting for the network to initialize.
  };

  void OnCancelAppLaunch();
  void OnNetworkConfigRequested();
  void InitializeKeyboard();
  void InitializeLauncher();

  // `KioskAppLauncher::Observer`
  void OnLaunchFailed(KioskAppLaunchError::Error error) override;
  void OnAppInstalling() override;
  void OnAppPrepared() override;
  void OnAppLaunched() override;
  void OnAppDataUpdated() override;
  void OnAppWindowCreated(const std::optional<std::string>& app_name) override;

  void StartAppLaunch(Profile& profile);
  void HandleProfileLoadError(KioskAppLaunchError::Error error);

  // Returns the `Data` struct used to populate the splash screen.
  AppLaunchSplashScreen::Data GetSplashScreenAppData();

  // Shows the app launch screen after it's populated with `data`.
  void ShowAppLaunchSplashScreen(AppLaunchSplashScreen::Data data);

  // Updates the app data shown in the app launch splash screen.
  void UpdateSplashScreenData(AppLaunchSplashScreen::Data data);

  // Continues launching after forced extensions are installed if required.
  // If it times out waiting for extensions to install, logs metrics via UMA.
  void FinishForcedExtensionsInstall(
      app_mode::ForceInstallObserver::Result result);

  void OnNetworkOnline();
  void OnNetworkOffline();
  void OnTimerFire();
  void CloseSplashScreen();
  void CleanUp();
  void LaunchApp();

  void FinishLaunchWithSuccess();
  void FinishLaunchWithError(KioskAppLaunchError::Error error);

  const KioskApp& kiosk_app() const;
  const KioskAppId& kiosk_app_id() const;

  bool auto_launch_ = false;  // Whether current app is being auto-launched.

  // Current state of the controller.
  AppState app_state_ = AppState::kCreatingProfile;

  // Not owned, destructed upon shutdown.
  raw_ptr<LoginDisplayHost> host_ = nullptr;
  // Owned by WizardController.
  raw_ptr<AppLaunchSplashScreen> splash_screen_ = nullptr;
  // Current app. Present once `Start` is called.
  std::optional<KioskApp> kiosk_app_;
  // Current app browser window name. Present once the app window is created.
  std::optional<std::string> app_window_name_;
  // Kiosk profile. Non-null after profile load handler has finished.
  raw_ptr<Profile> profile_ = nullptr;
  const KioskAppLauncherFactory app_launcher_factory_;
  std::unique_ptr<NetworkUiController> network_ui_controller_;

  // Whether app should be launched as soon as it is ready.
  bool launch_on_install_ = false;

  // Whether the controller has already been cleaned-up.
  bool cleaned_up_ = false;

  // Callback invoked when the app launched. See also `AppLaunchedCallback`
  // docs.
  AppLaunchedCallback app_launched_callback_;

  // Callback invoked when the launch is complete. The `error` field indicates
  // if the launch was successful or not.
  LaunchCompleteCallback done_callback_;

  // When invoked will attempt to log out and return to the sign-in screen.
  base::OnceClosure attempt_logout_;
  // When invoked will attempt to restart the device.
  base::OnceClosure attempt_relaunch_;

  // Handle to the job returned by `profile_loader_`.
  std::unique_ptr<CancellableJob> profile_loader_handle_;
  // The function used to load the Kiosk profile. Overridable in tests.
  kiosk::LoadProfileCallback profile_loader_;

  std::unique_ptr<AcceleratorController> accelerator_controller_;
  std::unique_ptr<ScopedAcceleratorDisabler> accelerator_disabler_;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;

  // Used to prepare and launch the actual kiosk app, is created after
  // profile initialization.
  std::unique_ptr<KioskAppLauncher> app_launcher_;

  // Tracks the moment when Kiosk launcher is started.
  base::Time launcher_start_time_;

  std::unique_ptr<app_mode::ForceInstallObserver> force_install_observer_;

  base::ObserverList<KioskProfileLoadFailedObserver>
      profile_load_failed_observers_;

  base::ScopedObservation<KioskAppLauncher, KioskAppLauncher::Observer>
      app_launcher_observation_{this};
  base::WeakPtrFactory<KioskLaunchController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_
