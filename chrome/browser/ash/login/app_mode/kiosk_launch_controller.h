// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_
#define CHROME_BROWSER_ASH_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_

#include <memory>

#include "ash/public/cpp/login_accelerators.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/kiosk_app_launcher.h"
#include "chrome/browser/ash/app_mode/kiosk_app_types.h"
#include "chrome/browser/ash/app_mode/kiosk_profile_loader.h"
#include "chrome/browser/ash/login/app_mode/force_install_observer.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"

namespace app_mode {
class ForceInstallObserver;
}  // namespace app_mode

namespace ash {
class LoginDisplayHost;
class OobeUI;

extern const char kKioskLaunchStateCrashKey[];

// Kiosk launch state for crash key.
enum class KioskLaunchState {
  kAttemptToLaunch,
  kStartLaunch,
  kLauncherStarted,
  kLaunchFailed,
  kAppWindowCreated,
};

std::string KioskLaunchStateToString(KioskLaunchState state);

// Sets crash key for kiosk launch state.
void SetKioskLaunchStateCrashKey(KioskLaunchState state);

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
class KioskLaunchController : public KioskProfileLoader::Delegate,
                              public AppLaunchSplashScreenView::Delegate,
                              public KioskAppLauncher::Delegate {
 public:
  class KioskProfileLoadFailedObserver : public base::CheckedObserver {
   public:
    ~KioskProfileLoadFailedObserver() override = default;
    virtual void OnKioskProfileLoadFailed() = 0;
  };

  using ReturnBoolCallback = base::RepeatingCallback<bool()>;

  explicit KioskLaunchController(OobeUI* oobe_ui);
  KioskLaunchController(const KioskLaunchController&) = delete;
  KioskLaunchController& operator=(const KioskLaunchController&) = delete;
  ~KioskLaunchController() override;

  [[nodiscard]] static std::unique_ptr<base::AutoReset<bool>>
  DisableWaitTimerAndLoginOperationsForTesting();
  [[nodiscard]] static std::unique_ptr<base::AutoReset<bool>>
  SkipSplashScreenWaitForTesting();
  [[nodiscard]] static std::unique_ptr<base::AutoReset<base::TimeDelta>>
  SetNetworkWaitForTesting(base::TimeDelta wait_time);
  [[nodiscard]] static std::unique_ptr<base::AutoReset<bool>>
  BlockAppLaunchForTesting();
  static void SetNetworkTimeoutCallbackForTesting(base::OnceClosure* callback);
  static void SetCanConfigureNetworkCallbackForTesting(
      ReturnBoolCallback* callback);
  static void SetNeedOwnerAuthToConfigureNetworkCallbackForTesting(
      ReturnBoolCallback* callback);

  static std::unique_ptr<KioskLaunchController> CreateForTesting(
      AppLaunchSplashScreenView* view,
      std::unique_ptr<KioskAppLauncher> app_launcher);

  bool waiting_for_network() const {
    return app_state_ == AppState::kInitNetwork;
  }
  bool network_wait_timedout() const { return network_wait_timedout_; }
  bool showing_network_dialog() const {
    return network_ui_state_ == NetworkUIState::kShowing;
  }

  void Start(const KioskAppId& kiosk_app_id, bool auto_launch);

  void AddKioskProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer);

  void RemoveKioskProfileLoadFailedObserver(
      KioskProfileLoadFailedObserver* observer);

  bool HandleAccelerator(LoginAcceleratorAction action);

 private:
  friend class KioskLaunchControllerTest;

  enum AppState {
    kCreatingProfile = 0,   // Profile is being created.
    kInitNetwork,           // Waiting for the network to initialize.
    kInstallingApp,         // App is being installed.
    kInstallingExtensions,  // Force-installed extensions are being installed.
    kInstalled,  // Everything is installed, waiting for the splash screen timer
                 // to fire.
    kLaunched    // App is being launched.
  };

  enum NetworkUIState {
    kNotShowing = 0,  // Network configure UI is not being shown.
    kNeedToShow,      // We need to show the UI as soon as we can.
    kShowing          // Network configure UI is being shown.
  };

  KioskLaunchController();

  void OnCancelAppLaunch();
  void OnNetworkConfigRequested();

  // AppLaunchSplashScreenView::Delegate:
  void OnConfigureNetwork() override;
  void OnDeletingSplashScreenView() override;
  void OnNetworkConfigFinished() override;
  void OnNetworkStateChanged(bool online) override;
  KioskAppManagerBase::App GetAppData() override;
  bool IsNetworkRequired() override;

  // KioskAppLauncher::Delegate:
  void InitializeNetwork() override;
  bool IsNetworkReady() const override;
  bool IsShowingNetworkConfigScreen() const override;
  void OnLaunchFailed(KioskAppLaunchError::Error error) override;
  void OnAppInstalling() override;
  void OnAppPrepared() override;
  void OnAppLaunched() override;
  void OnAppDataUpdated() override;
  void OnAppWindowCreated() override;

  // KioskProfileLoader::Delegate:
  void OnProfileLoaded(Profile* profile) override;
  void OnProfileLoadFailed(KioskAppLaunchError::Error error) override;
  void OnOldEncryptionDetected(
      std::unique_ptr<UserContext> user_context) override;

  void OnOwnerSigninSuccess();

  // Whether the network could be configured during launching.
  bool CanConfigureNetwork();
  // Whether the owner password is needed to configure network.
  bool NeedOwnerAuthToConfigureNetwork();
  // Shows network configuration dialog if kiosk profile was already created or
  // postpones the display upon creation.
  void MaybeShowNetworkConfigureUI();
  // Shows the network configuration dialog.
  void ShowNetworkConfigureUI();
  void CloseNetworkConfigureScreenIfOnline();

  void HandleWebAppInstallFailed();

  // Continues launching after forced extensions are installed if required.
  // If it times out waiting for extensions to install, logs metrics via UMA.
  void FinishForcedExtensionsInstall(
      app_mode::ForceInstallObserver::Result result);

  void OnNetworkWaitTimedOut();
  void OnTimerFire();
  void CloseSplashScreen();
  void CleanUp();
  void LaunchApp();

  bool auto_launch_ = false;  // Whether current app is being auto-launched.

  // Current state of the controller.
  AppState app_state_ = AppState::kCreatingProfile;
  // Current state of network configure dialog.
  NetworkUIState network_ui_state_ = NetworkUIState::kNotShowing;

  LoginDisplayHost* const host_;  // Not owned, destructed upon shutdown.
  AppLaunchSplashScreenView* splash_screen_view_ = nullptr;  // Owned by OobeUI.
  KioskAppId kiosk_app_id_;                                  // Current app.
  Profile* profile_ = nullptr;                               // Not owned.

  // Whether app should be launched as soon as it is ready.
  bool launch_on_install_ = false;
  bool network_wait_timedout_ = false;
  // Whether the network is required for the installation.
  bool network_required_ = false;

  // Whether the controller has already been cleaned-up.
  bool cleaned_up_ = false;

  // Used to login into kiosk user profile.
  std::unique_ptr<KioskProfileLoader> kiosk_profile_loader_;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;

  // Used to prepare and launch the actual kiosk app, is created after
  // profile initialization. Is nullptr for arc kiosks.
  std::unique_ptr<KioskAppLauncher> app_launcher_;

  // A timer that fires when the network was not prepared and we require user
  // network configuration to continue.
  base::OneShotTimer network_wait_timer_;

  // Tracks the moment when Kiosk launcher is started.
  base::Time launcher_start_time_;

  std::unique_ptr<app_mode::ForceInstallObserver> force_install_observer_;

  base::ObserverList<KioskProfileLoadFailedObserver>
      profile_load_failed_observers_;

  base::WeakPtrFactory<KioskLaunchController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_LOGIN_APP_MODE_KIOSK_LAUNCH_CONTROLLER_H_
