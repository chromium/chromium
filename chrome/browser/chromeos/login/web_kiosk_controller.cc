// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chrome/browser/chromeos/login/web_kiosk_controller.h>

#include "base/syslog_logging.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager_base.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_data.h"
#include "chrome/browser/chromeos/app_mode/web_app/web_kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/auth/chrome_login_performer.h"
#include "chrome/browser/chromeos/login/screens/encryption_migration_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/encryption_migration_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chromeos/login/auth/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"

namespace chromeos {

// Web Kiosk splash screen minimum show time.
constexpr base::TimeDelta kWebKioskSplashScreenMinTime =
    base::TimeDelta::FromSeconds(10);

WebKioskController::WebKioskController(LoginDisplayHost* host, OobeUI* oobe_ui)
    : host_(host),
      web_kiosk_splash_screen_view_(
          oobe_ui->GetView<AppLaunchSplashScreenHandler>()) {}

WebKioskController::~WebKioskController() {
  if (web_kiosk_splash_screen_view_)
    web_kiosk_splash_screen_view_->SetDelegate(nullptr);
}

void WebKioskController::StartWebKiosk(const AccountId& account_id) {
  account_id_ = account_id;
  web_kiosk_splash_screen_view_->SetDelegate(this);
  web_kiosk_splash_screen_view_->Show();
  splash_wait_timer_.Start(FROM_HERE, kWebKioskSplashScreenMinTime,
                           base::Bind(&WebKioskController::OnTimerFire,
                                      weak_ptr_factory_.GetWeakPtr()));

  login_performer_ = std::make_unique<ChromeLoginPerformer>(this);
  login_performer_->LoginAsWebKioskAccount(account_id_);
}

KioskAppManagerBase::App WebKioskController::GetAppData() {
  const WebKioskAppData* app =
      WebKioskAppManager::Get()->GetAppByAccountId(account_id_);
  DCHECK(app);
  return KioskAppManagerBase::App(*app);
}

void WebKioskController::OnTimerFire() {
  // Start launching now.
  if (app_prepared_) {
    app_launcher_->LaunchApp();
  } else {
    launch_on_install_ = true;
  }
}

void WebKioskController::OnCancelAppLaunch() {
  KioskAppLaunchError::Save(KioskAppLaunchError::USER_CANCEL);
  CleanUp();
  chrome::AttemptUserExit();
}

void WebKioskController::OnNetworkConfigRequested() {
  // TODO(crbug.com/1006230): Implement when app launch logic is done.
}

void WebKioskController::OnNetworkConfigFinished() {
  // TODO(crbug.com/1006230): Implement when app launch logic is done.
}

void WebKioskController::OnNetworkStateChanged(bool online) {
  // TODO(crbug.com/1006230): Implement when app launch logic is done.
}

void WebKioskController::OnDeletingSplashScreenView() {
  web_kiosk_splash_screen_view_ = nullptr;
}

void WebKioskController::CleanUp() {
  splash_wait_timer_.Stop();
}

void WebKioskController::CloseSplashScreen() {
  CleanUp();
  host_->Finalize(base::OnceClosure());
}

void WebKioskController::OnAuthFailure(const AuthFailure& error) {
  SYSLOG(ERROR) << "Web Kiosk launch failed. Will now shut down, error="
                << error.GetErrorString();
  KioskAppLaunchError::SaveCryptohomeFailure(error);
  CleanUp();
  chrome::AttemptUserExit();
}

void WebKioskController::OnAuthSuccess(const UserContext& user_context) {
  // LoginPerformer instance will delete itself in case of successful auth.
  login_performer_->set_delegate(nullptr);
  ignore_result(login_performer_.release());

  UserSessionManager::GetInstance()->StartSession(
      user_context, UserSessionManager::PRIMARY_USER_SESSION,
      false,  // has_auth_cookies
      false,  // Start session for user.
      this);
}

void WebKioskController::WhiteListCheckFailed(const std::string& email) {
  NOTREACHED();
}

void WebKioskController::PolicyLoadFailed() {
  SYSLOG(ERROR) << "Policy load failed. Will now shut down";
  KioskAppLaunchError::Save(KioskAppLaunchError::POLICY_LOAD_FAILED);
  CleanUp();
  chrome::AttemptUserExit();
}

void WebKioskController::SetAuthFlowOffline(bool offline) {
  NOTREACHED();
}

void WebKioskController::OnOldEncryptionDetected(
    const UserContext& user_context,
    bool has_incomplete_migration) {
  NOTREACHED();
}

void WebKioskController::OnProfilePrepared(Profile* profile,
                                           bool browser_launched) {
  DVLOG(1) << "Profile loaded... Starting app launch.";
  // This object could be deleted any time after successfully reporting
  // a profile load, so invalidate the delegate now.
  UserSessionManager::GetInstance()->DelegateDeleted(this);

  // This is needed to trigger input method extensions being loaded.
  profile->InitChromeOSPreferences();

  // Reset virtual keyboard to use IME engines in app profile early.
  ChromeKeyboardControllerClient::Get()->RebuildKeyboardIfEnabled();

  // We need to change the session state so we are able to create browser
  // windows.
  session_manager::SessionManager::Get()->SetSessionState(
      session_manager::SessionState::LOGGED_IN_NOT_ACTIVE);

  app_launcher_.reset(new WebKioskAppLauncher(profile, this));
  app_launcher_->Initialize(account_id_);
}

void WebKioskController::InitializeNetwork() {
  if (!web_kiosk_splash_screen_view_)
    return;

  // TODO(crbug.com/1006230): Implement network dialog flow.
  app_launcher_->ContinueWithNetworkReady();
}

void WebKioskController::OnAppStartedInstalling() {
  if (!web_kiosk_splash_screen_view_)
    return;
  web_kiosk_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_INSTALLING_APPLICATION);
  web_kiosk_splash_screen_view_->Show();
}

void WebKioskController::OnAppPrepared() {
  app_prepared_ = true;
  if (!web_kiosk_splash_screen_view_)
    return;
  web_kiosk_splash_screen_view_->UpdateAppLaunchState(
      AppLaunchSplashScreenView::AppLaunchState::
          APP_LAUNCH_STATE_WAITING_APP_WINDOW);
  web_kiosk_splash_screen_view_->Show();
  if (launch_on_install_) {
    app_launcher_->LaunchApp();
  }
}

void WebKioskController::OnAppLaunched() {
  SYSLOG(INFO) << "Kiosk launch succeeded, wait for app window.";
  session_manager::SessionManager::Get()->SessionStarted();
  CloseSplashScreen();
}

void WebKioskController::OnAppLaunchFailed() {
  SYSLOG(ERROR) << "App launch failed. Will now shut down";
  KioskAppLaunchError::Save(KioskAppLaunchError::UNABLE_TO_LAUNCH);
  CleanUp();
  chrome::AttemptUserExit();
}

}  // namespace chromeos
