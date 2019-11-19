// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/arc_kiosk_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_kiosk_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

// ARC Kiosk splash screen minimum show time.
constexpr base::TimeDelta kArcKioskSplashScreenMinTime =
    base::TimeDelta::FromSeconds(10);

ArcKioskController::ArcKioskController(LoginDisplayHost* host, OobeUI* oobe_ui)
    : host_(host),
      arc_kiosk_splash_screen_view_(
          oobe_ui->GetView<ArcKioskSplashScreenHandler>()) {}

ArcKioskController::~ArcKioskController() {
  if (arc_kiosk_splash_screen_view_)
    arc_kiosk_splash_screen_view_->SetDelegate(nullptr);
}

void ArcKioskController::StartArcKiosk(const AccountId& account_id) {
  DVLOG(1) << "Starting ARC Kiosk for account: " << account_id.GetUserEmail();

  host_->GetLoginDisplay()->SetUIEnabled(true);

  arc_kiosk_splash_screen_view_->SetDelegate(this);
  arc_kiosk_splash_screen_view_->Show();
  splash_wait_timer_.Start(FROM_HERE, kArcKioskSplashScreenMinTime,
                           base::Bind(&ArcKioskController::CloseSplashScreen,
                                      weak_ptr_factory_.GetWeakPtr()));

  login_performer_ = std::make_unique<ChromeLoginPerformer>(this);
  login_performer_->LoginAsArcKioskAccount(account_id);
}

void ArcKioskController::OnCancelArcKioskLaunch() {
  KioskAppLaunchError::Save(KioskAppLaunchError::USER_CANCEL);
  CleanUp();
  chrome::AttemptUserExit();
}

void ArcKioskController::OnDeletingSplashScreenView() {
  arc_kiosk_splash_screen_view_ = nullptr;
}

void ArcKioskController::CleanUp() {
  splash_wait_timer_.Stop();
  // Delegate is registered only when |profile_| is set.
  if (profile_)
    ArcKioskAppService::Get(profile_)->SetDelegate(nullptr);
}

void ArcKioskController::CloseSplashScreen() {
  if (!launched_)
    return;
  CleanUp();
  host_->Finalize(base::OnceClosure());
  session_manager::SessionManager::Get()->SessionStarted();
}

void ArcKioskController::OnAuthFailure(const AuthFailure& error) {
  LOG(ERROR) << "ARC Kiosk launch failed. Will now shut down, error="
             << error.GetErrorString();
  KioskAppLaunchError::Save(KioskAppLaunchError::ARC_AUTH_FAILED);
  CleanUp();
  chrome::AttemptUserExit();
}

void ArcKioskController::OnAuthSuccess(const UserContext& user_context) {
  // LoginPerformer instance will delete itself in case of successful auth.
  login_performer_->set_delegate(nullptr);
  ignore_result(login_performer_.release());

  UserSessionManager::GetInstance()->StartSession(
      user_context, UserSessionManager::PRIMARY_USER_SESSION,
      false,  // has_auth_cookies
      false,  // Start session for user.
      this);
}

void ArcKioskController::WhiteListCheckFailed(const std::string& email) {
  NOTREACHED();
}

void ArcKioskController::PolicyLoadFailed() {
  LOG(ERROR) << "Policy load failed. Will now shut down";
  KioskAppLaunchError::Save(KioskAppLaunchError::POLICY_LOAD_FAILED);
  CleanUp();
  chrome::AttemptUserExit();
}

void ArcKioskController::SetAuthFlowOffline(bool offline) {
  NOTREACHED();
}

void ArcKioskController::OnOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  host_->StartWizard(EncryptionMigrationScreenView::kScreenId);

  EncryptionMigrationScreen* migration_screen =
      static_cast<EncryptionMigrationScreen*>(
          host_->GetWizardController()->current_screen());
  DCHECK(migration_screen);
  migration_screen->SetUserContext(user_context);
  migration_screen->SetupInitialView();
}

void ArcKioskController::OnProfilePrepared(Profile* profile,
                                           bool browser_launched) {
  DVLOG(1) << "Profile loaded... Starting app launch.";
  profile_ = profile;
  // This object could be deleted any time after successfully reporting
  // a profile load, so invalidate the delegate now.
  UserSessionManager::GetInstance()->DelegateDeleted(this);
  ArcKioskAppService::Get(profile_)->SetDelegate(this);
  if (arc_kiosk_splash_screen_view_) {
    arc_kiosk_splash_screen_view_->UpdateArcKioskState(
        ArcKioskSplashScreenView::ArcKioskState::WAITING_APP_LAUNCH);
  }
}

void ArcKioskController::OnAppStarted() {
  DVLOG(1) << "ARC Kiosk launch succeeded, wait for app window.";

  if (arc_kiosk_splash_screen_view_) {
    arc_kiosk_splash_screen_view_->UpdateArcKioskState(
        ArcKioskSplashScreenView::ArcKioskState::WAITING_APP_WINDOW);
  }
}

void ArcKioskController::OnAppWindowLaunched() {
  DVLOG(1) << "App window created, closing splash screen.";
  launched_ = true;
  // If timer is running, do not remove splash screen for a few
  // more seconds to give the user ability to exit ARC kiosk.
  if (splash_wait_timer_.IsRunning())
    return;
  CloseSplashScreen();
}

}  // namespace chromeos
