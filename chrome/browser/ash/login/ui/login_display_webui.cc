// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_webui.h"

#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/ui/user_adding_screen.h"
#include "chrome/browser/ash/login/ui/webui_login_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/update_required_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "ui/base/ime/ash/ime_keyboard.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/views/widget/widget.h"

namespace ash {

// LoginDisplayWebUI, public: --------------------------------------------------

LoginDisplayWebUI::~LoginDisplayWebUI() {
  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && activity_detector->HasObserver(this))
    activity_detector->RemoveObserver(this);
}

// LoginDisplay implementation: ------------------------------------------------

LoginDisplayWebUI::LoginDisplayWebUI() = default;

void LoginDisplayWebUI::ClearAndEnablePassword() {
  if (webui_handler_)
    webui_handler_->ClearAndEnablePassword();
}

void LoginDisplayWebUI::Init(const user_manager::UserList& users,
                             bool show_guest,
                             bool show_users,
                             bool allow_new_user) {
  // Testing that the delegate has been set.
  DCHECK(delegate_);

  OobeUI* oobe_ui = LoginDisplayHost::default_host()->GetOobeUI();
  const std::string display_type = oobe_ui->display_type();
  allow_new_user_changed_ = (allow_new_user_ != allow_new_user);
  allow_new_user_ = allow_new_user;

  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && !activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

// ---- Common methods

// ---- Gaia screen methods

// ---- Not yet classified methods

void LoginDisplayWebUI::OnPreferencesChanged() {
  if (webui_handler_)
    webui_handler_->OnPreferencesChanged();
}

void LoginDisplayWebUI::SetUIEnabled(bool is_enabled) {
  // TODO(nkostylev): Cleanup this condition,
  // see http://crbug.com/157885 and http://crbug.com/158255.
  // Allow this call only before user sign in or at lock screen.
  // If this call is made after new user signs in but login screen is still
  // around that would trigger a sign in extension refresh.
  if (is_enabled && (!user_manager::UserManager::Get()->IsUserLoggedIn() ||
                     ScreenLocker::default_screen_locker())) {
    ClearAndEnablePassword();
  }

  LoginDisplayHost* host = LoginDisplayHost::default_host();
  if (host && host->GetWebUILoginView())
    host->GetWebUILoginView()->SetUIEnabled(is_enabled);
}

void LoginDisplayWebUI::ShowAllowlistCheckFailedError() {
  if (webui_handler_)
    webui_handler_->ShowAllowlistCheckFailedError();
}

void LoginDisplayWebUI::Login(const UserContext& user_context,
                              const SigninSpecifics& specifics) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->Login(user_context, specifics);
}

void LoginDisplayWebUI::OnSigninScreenReady() {
  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void LoginDisplayWebUI::ShowEnterpriseEnrollmentScreen() {
  if (delegate_)
    delegate_->OnStartEnterpriseEnrollment();
}

void LoginDisplayWebUI::ShowKioskAutolaunchScreen() {
  if (delegate_)
    delegate_->OnStartKioskAutolaunchScreen();
}

void LoginDisplayWebUI::ShowWrongHWIDScreen() {
  LoginDisplayHost::default_host()->StartWizard(WrongHWIDScreenView::kScreenId);
}

void LoginDisplayWebUI::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
}

bool LoginDisplayWebUI::AllowNewUserChanged() const {
  return allow_new_user_changed_;
}

bool LoginDisplayWebUI::IsSigninInProgress() const {
  return delegate_->IsSigninInProgress();
}

bool LoginDisplayWebUI::IsUserSigninCompleted() const {
  return is_signin_completed();
}

void LoginDisplayWebUI::OnUserActivity(const ui::Event* event) {
  if (delegate_)
    delegate_->ResetAutoLoginTimer();
}

}  // namespace ash
