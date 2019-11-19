// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_ARC_KIOSK_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_ARC_KIOSK_CONTROLLER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/app_mode/arc/arc_kiosk_app_service.h"
#include "chrome/browser/chromeos/login/session/user_session_manager.h"
#include "chromeos/login/auth/login_performer.h"

class AccountId;
class Profile;

namespace base {
class OneShotTimer;
}

namespace chromeos {

class ArcKioskSplashScreenView;
class LoginDisplayHost;
class OobeUI;
class UserContext;

// Controller for the ARC kiosk launch process, responsible for
// coordinating loading the ARC kiosk profile, and
// updating the splash screen UI.
class ArcKioskController : public LoginPerformer::Delegate,
                           public UserSessionManagerDelegate,
                           public ArcKioskAppService::Delegate {
 public:
  ArcKioskController(LoginDisplayHost* host, OobeUI* oobe_ui);

  ~ArcKioskController() override;

  // Starts ARC kiosk splash screen.
  void StartArcKiosk(const AccountId& account_id);

  // Invoked when the launch bailout shortcut key is pressed.
  void OnCancelArcKioskLaunch();
  // Invoked when the splash screen view gets being deleted.
  void OnDeletingSplashScreenView();

 private:
  void CleanUp();
  void CloseSplashScreen();

  // LoginPerformer::Delegate implementation:
  void OnAuthFailure(const AuthFailure& error) override;
  void OnAuthSuccess(const UserContext& user_context) override;
  void WhiteListCheckFailed(const std::string& email) override;
  void PolicyLoadFailed() override;
  void SetAuthFlowOffline(bool offline) override;
  void OnOldEncryptionDetected(const UserContext& user_context,
                               bool has_incomplete_migration) override;

  // UserSessionManagerDelegate implementation:
  void OnProfilePrepared(Profile* profile, bool browser_launched) override;

  // ArcKioskAppService::Delegate implementation:
  void OnAppStarted() override;
  void OnAppWindowLaunched() override;

  // LoginDisplayHost owns itself.
  LoginDisplayHost* const host_;
  // Owned by OobeUI.
  ArcKioskSplashScreenView* arc_kiosk_splash_screen_view_;
  // Not owning here.
  Profile* profile_ = nullptr;

  // Used to execute login operations.
  std::unique_ptr<LoginPerformer> login_performer_ = nullptr;

  // A timer to ensure the app splash is shown for a minimum amount of time.
  base::OneShotTimer splash_wait_timer_;
  bool launched_ = false;
  base::WeakPtrFactory<ArcKioskController> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ArcKioskController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_ARC_KIOSK_CONTROLLER_H_
