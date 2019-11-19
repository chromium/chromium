// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_webui.h"

#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/login/lock/screen_locker.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/signin_screen_controller.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/user_adding_screen.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/grit/generated_resources.h"
#include "components/account_id/account_id.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_names.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/user_activity/user_activity_detector.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

// LoginDisplayWebUI, public: --------------------------------------------------

LoginDisplayWebUI::~LoginDisplayWebUI() {
  if (webui_handler_)
    webui_handler_->ResetSigninScreenHandlerDelegate();
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
  SignInScreenController::Get()->Init(users);
  show_guest_ = show_guest;
  show_users_changed_ = (show_users_ != show_users);
  show_users_ = show_users;
  allow_new_user_changed_ = (allow_new_user_ != allow_new_user);
  allow_new_user_ = allow_new_user;

  ui::UserActivityDetector* activity_detector = ui::UserActivityDetector::Get();
  if (activity_detector && !activity_detector->HasObserver(this))
    activity_detector->AddObserver(this);
}

// ---- Common methods

// ---- User selection screen methods

void LoginDisplayWebUI::HandleGetUsers() {
  SignInScreenController::Get()->SendUserList();
}

void LoginDisplayWebUI::CheckUserStatus(const AccountId& account_id) {
  SignInScreenController::Get()->CheckUserStatus(account_id);
}

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

void LoginDisplayWebUI::ShowError(int error_msg_id,
                                  int login_attempts,
                                  HelpAppLauncher::HelpTopic help_topic_id) {
  VLOG(1) << "Show error, error_id: " << error_msg_id
          << ", attempts:" << login_attempts
          << ", help_topic_id: " << help_topic_id;
  if (!webui_handler_)
    return;

  std::string error_text;
  switch (error_msg_id) {
    case IDS_LOGIN_ERROR_CAPTIVE_PORTAL:
      error_text = l10n_util::GetStringFUTF8(
          error_msg_id, delegate()->GetConnectedNetworkName());
      break;
    default:
      error_text = l10n_util::GetStringUTF8(error_msg_id);
      break;
  }

  // Only display hints about keyboard layout if the error is authentication-
  // related.
  if (error_msg_id != IDS_LOGIN_ERROR_WHITELIST &&
      error_msg_id != IDS_ENTERPRISE_LOGIN_ERROR_WHITELIST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_KEY_LOST &&
      error_msg_id != IDS_LOGIN_ERROR_OWNER_REQUIRED &&
      error_msg_id != IDS_LOGIN_ERROR_GOOGLE_ACCOUNT_NOT_ALLOWED) {
    input_method::InputMethodManager* ime_manager =
        input_method::InputMethodManager::Get();

    // Display a hint to switch keyboards if there are other active input
    // methods.
    if (ime_manager->GetActiveIMEState()->GetNumActiveInputMethods() > 1) {
      error_text +=
          "\n" + l10n_util::GetStringUTF8(IDS_LOGIN_ERROR_KEYBOARD_SWITCH_HINT);
    }
  }

  std::string help_link;
  if (login_attempts > 1)
    help_link = l10n_util::GetStringUTF8(IDS_LEARN_MORE);

  webui_handler_->ShowError(login_attempts, error_text, help_link,
                            help_topic_id);
}

void LoginDisplayWebUI::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  VLOG(1) << "Show error screen, error_id: " << error_id;
  if (!webui_handler_)
    return;
  webui_handler_->ShowErrorScreen(error_id);
}

void LoginDisplayWebUI::ShowPasswordChangedDialog(bool show_password_error,
                                                  const std::string& email) {
  if (webui_handler_)
    webui_handler_->ShowPasswordChangedDialog(show_password_error, email);
}

void LoginDisplayWebUI::ShowSigninUI(const std::string& email) {
  if (webui_handler_)
    webui_handler_->ShowSigninUI(email);
}

void LoginDisplayWebUI::ShowWhitelistCheckFailedError() {
  if (webui_handler_)
    webui_handler_->ShowWhitelistCheckFailedError();
}

// LoginDisplayWebUI, NativeWindowDelegate implementation: ---------------------
gfx::NativeWindow LoginDisplayWebUI::GetNativeWindow() const {
  return parent_window();
}

// LoginDisplayWebUI, SigninScreenHandlerDelegate implementation: --------------
void LoginDisplayWebUI::CancelUserAdding() {
  if (!UserAddingScreen::Get()->IsRunning()) {
    LOG(ERROR) << "User adding screen not running.";
    return;
  }
  UserAddingScreen::Get()->Cancel();
}
void LoginDisplayWebUI::Login(const UserContext& user_context,
                              const SigninSpecifics& specifics) {
  DCHECK(delegate_);
  if (delegate_)
    delegate_->Login(user_context, specifics);
}

void LoginDisplayWebUI::OnSigninScreenReady() {
  SignInScreenController::Get()->OnSigninScreenReady();

  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void LoginDisplayWebUI::RemoveUser(const AccountId& account_id) {
  SignInScreenController::Get()->RemoveUser(account_id);
}

void LoginDisplayWebUI::ShowEnterpriseEnrollmentScreen() {
  if (delegate_)
    delegate_->OnStartEnterpriseEnrollment();
}

void LoginDisplayWebUI::ShowEnableDebuggingScreen() {
  if (delegate_)
    delegate_->OnStartEnableDebuggingScreen();
}

void LoginDisplayWebUI::ShowKioskEnableScreen() {
  if (delegate_)
    delegate_->OnStartKioskEnableScreen();
}

void LoginDisplayWebUI::ShowKioskAutolaunchScreen() {
  if (delegate_)
    delegate_->OnStartKioskAutolaunchScreen();
}

void LoginDisplayWebUI::ShowUpdateRequiredScreen() {
  if (delegate_)
    delegate_->ShowUpdateRequiredScreen();
}

void LoginDisplayWebUI::ShowWrongHWIDScreen() {
  if (delegate_)
    delegate_->ShowWrongHWIDScreen();
}

void LoginDisplayWebUI::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
  SignInScreenController::Get()->SetWebUIHandler(webui_handler_);
}

bool LoginDisplayWebUI::IsShowGuest() const {
  return show_guest_;
}

bool LoginDisplayWebUI::IsShowUsers() const {
  return show_users_;
}

bool LoginDisplayWebUI::ShowUsersHasChanged() const {
  return show_users_changed_;
}

bool LoginDisplayWebUI::IsAllowNewUser() const {
  return allow_new_user_;
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

void LoginDisplayWebUI::Signout() {
  delegate_->Signout();
}

void LoginDisplayWebUI::OnUserActivity(const ui::Event* event) {
  if (delegate_)
    delegate_->ResetAutoLoginTimer();
}

}  // namespace chromeos
