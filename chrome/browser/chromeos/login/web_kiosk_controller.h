// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_WEB_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_WEB_KIOSK_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_launcher.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chromeos/login/auth/login_performer.h"

class AccountId;
class Profile;

namespace base {
class OneShotTimer;
}

namespace chromeos {

class LoginDisplayHost;
class OobeUI;
class UserContext;

// Controller for the web kiosk launch process, responsible for loading the web
// kiosk profile, and updating the splash screen UI.
class WebKioskController : public LoginPerformer::Delegate,
                           public UserSessionManagerDelegate,
                           public AppLaunchSplashScreenView::Delegate,
                           public WebKioskAppLauncher::Delegate {
 public:
  WebKioskController(LoginDisplayHost* host, OobeUI* oobe_ui);
  ~WebKioskController() override;

  void StartWebKiosk(const AccountId& account_id);

 private:
  // LoginPerformer::Delegate:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void WhiteListCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;
  void SetAuthFlowOffline(bool offline) override;
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;

  // UserSessionManagerDelegate:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  // AppLaunchSplashScreenView::Delegate:
  void OnCancelAppLaunch() override;
  void OnNetworkConfigRequested() override;
  void OnNetworkConfigFinished() override;
  void OnNetworkStateChanged(bool online) override;
  void OnDeletingSplashScreenView() override;
  KioskAppManagerBase::App GetAppData() override;

  // WebKioskAppLauncher:
  void InitializeNetwork() override;
  void OnAppStartedInstalling() override;
  void OnAppPrepared() override;
  void OnAppLaunched() override;
  void OnAppLaunchFailed() override;

  void CleanUp();
  void OnTimerFire();
  void CloseSplashScreen();

  LoginDisplayHost* const host_;  // Not owned, destructed upon shutdown.
  AppLaunchSplashScreenView* web_kiosk_splash_screen_view_;  // Owned by OobeUI.

  AccountId account_id_;

  bool app_prepared_ = false;
  bool launch_on_install_ = false;

  // Used to prepare and launch the actual web kiosk app, is created after
  // profile initialization.
  std::unique_ptr<WebKioskAppLauncher> app_launcher_;
  // Used to execute login operations.
  std::unique_ptr<LoginPerformer> login_performer_;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;

  base::WeakPtrFactory<WebKioskController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(WebKioskController);
};
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_WEB_KIOSK_CONTROLLER_H_
