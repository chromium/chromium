// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/ui/login_display_mojo.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/bind.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/accessibility/accessibility_manager.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_manager.h"
#include "chrome/browser/chromeos/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/quick_unlock/pin_backend.h"
#include "chrome/browser/chromeos/login/screens/chrome_user_selection_screen.h"
#include "chrome/browser/chromeos/login/screens/user_selection_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host_mojo.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_manager/known_user.h"
#include "content/public/browser/notification_service.h"
#include "ui/base/ime/chromeos/ime_keyboard.h"
#include "ui/base/ime/chromeos/input_method_manager.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

LoginDisplayMojo::LoginDisplayMojo(LoginDisplayHostMojo* host) : host_(host) {
  user_manager::UserManager::Get()->AddObserver(this);
}

LoginDisplayMojo::~LoginDisplayMojo() {
  user_manager::UserManager::Get()->RemoveObserver(this);
}

void LoginDisplayMojo::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      account_id, base::BindOnce(&LoginDisplayMojo::OnPinCanAuthenticate,
                                 weak_factory_.GetWeakPtr(), account_id));
}

void LoginDisplayMojo::UpdateChallengeResponseAuthAvailability(
    const AccountId& account_id) {
  const bool enable_challenge_response =
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id);
  ash::LoginScreen::Get()->GetModel()->SetChallengeResponseAuthEnabledForUser(
      account_id, enable_challenge_response);
}

void LoginDisplayMojo::ClearAndEnablePassword() {}

void LoginDisplayMojo::Init(const user_manager::UserList& filtered_users,
                            bool show_guest,
                            bool show_users,
                            bool show_new_user) {
  host_->SetUsers(filtered_users);
  auto* client = LoginScreenClient::Get();

  // ExistingUserController::DeviceSettingsChanged and others may initialize the
  // login screen multiple times. Views-login only supports initialization once.
  if (!initialized_) {
    client->SetDelegate(host_);
    ash::LoginScreen::Get()->ShowLoginScreen();
  }

  UserSelectionScreen* user_selection_screen = host_->user_selection_screen();
  user_selection_screen->Init(filtered_users);
  ash::LoginScreen::Get()->GetModel()->SetUserList(
      user_selection_screen->UpdateAndReturnUserListForAsh());
  ash::LoginScreen::Get()->SetAllowLoginAsGuest(show_guest);
  user_selection_screen->SetUsersLoaded(true /*loaded*/);

  if (user_manager::UserManager::IsInitialized()) {
    // Enable pin and challenge-response authentication for any users who can
    // use them.
    for (const user_manager::User* user : filtered_users) {
      UpdatePinKeyboardState(user->GetAccountId());
      UpdateChallengeResponseAuthAvailability(user->GetAccountId());
    }
  }

  if (!initialized_) {
    initialized_ = true;

    // login-prompt-visible is recorded and tracked to verify boot performance
    // does not regress. Autotests may also depend on it (ie,
    // login_SameSessionTwice).
    VLOG(1) << "Emitting login-prompt-visible";
    SessionManagerClient::Get()->EmitLoginPromptVisible();

    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_LOGIN_OR_LOCK_WEBUI_VISIBLE,
        content::NotificationService::AllSources(),
        content::NotificationService::NoDetails());

    // If there no available users exist, delay showing the dialogs until after
    // GAIA dialog is shown (GAIA dialog will check these local state values,
    // too). Login UI will show GAIA dialog if no user are registered, which
    // might hide any UI shown here.
    if (filtered_users.empty())
      return;

    // Check whether factory reset or debugging feature have been requested in
    // prior session, and start reset or enable debugging wizard as needed.
    // This has to happen after login-prompt-visible, as some reset dialog
    // features (TPM firmware update) depend on system services running, which
    // is in turn blocked on the 'login-prompt-visible' signal.
    PrefService* local_state = g_browser_process->local_state();
    if (local_state->GetBoolean(prefs::kFactoryResetRequested)) {
      host_->StartWizard(ResetView::kScreenId);
    } else if (local_state->GetBoolean(prefs::kDebuggingFeaturesRequested)) {
      host_->StartWizard(EnableDebuggingScreenView::kScreenId);
    } else if (local_state->GetBoolean(prefs::kEnableAdbSideloadingRequested)) {
      host_->StartWizard(EnableAdbSideloadingScreenView::kScreenId);
    } else if (!KioskAppManager::Get()->GetAutoLaunchApp().empty() &&
               KioskAppManager::Get()->IsAutoLaunchRequested()) {
      VLOG(0) << "Showing auto-launch warning";
      host_->StartWizard(KioskAutolaunchScreenView::kScreenId);
    }
  }
}

void LoginDisplayMojo::OnPreferencesChanged() {
  if (webui_handler_)
    webui_handler_->OnPreferencesChanged();
}

void LoginDisplayMojo::SetUIEnabled(bool is_enabled) {
  if (is_enabled)
    host_->GetOobeUI()->ShowOobeUI(false);
}

void LoginDisplayMojo::ShowError(int error_msg_id,
                                 int login_attempts,
                                 HelpAppLauncher::HelpTopic help_topic_id) {
  // TODO(jdufault): Investigate removing this method once views-based
  // login is fully implemented. Tracking bug at http://crbug/851680.
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

void LoginDisplayMojo::ShowErrorScreen(LoginDisplay::SigninError error_id) {
  host_->ShowErrorScreen(error_id);
}

void LoginDisplayMojo::ShowPasswordChangedDialog(bool show_password_error,
                                                 const std::string& email) {
  host_->ShowPasswordChangedDialog(show_password_error, email);
}

void LoginDisplayMojo::ShowSigninUI(const std::string& email) {
  host_->ShowSigninUI(email);
}

void LoginDisplayMojo::ShowWhitelistCheckFailedError() {
  host_->ShowWhitelistCheckFailedError();
}

void LoginDisplayMojo::Login(const UserContext& user_context,
                             const SigninSpecifics& specifics) {
  if (delegate_)
    delegate_->Login(user_context, specifics);
}

bool LoginDisplayMojo::IsSigninInProgress() const {
  if (delegate_)
    return delegate_->IsSigninInProgress();
  return false;
}

void LoginDisplayMojo::Signout() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::OnSigninScreenReady() {
  if (delegate_)
    delegate_->OnSigninScreenReady();
}

void LoginDisplayMojo::ShowEnterpriseEnrollmentScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowEnableDebuggingScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowKioskEnableScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowKioskAutolaunchScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowWrongHWIDScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::ShowUpdateRequiredScreen() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::CancelUserAdding() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::RemoveUser(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::SetWebUIHandler(
    LoginDisplayWebUIHandler* webui_handler) {
  webui_handler_ = webui_handler;
}

bool LoginDisplayMojo::IsShowGuest() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::IsShowUsers() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::ShowUsersHasChanged() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::IsAllowNewUser() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::AllowNewUserChanged() const {
  NOTIMPLEMENTED();
  return false;
}

bool LoginDisplayMojo::IsUserSigninCompleted() const {
  return is_signin_completed();
}

void LoginDisplayMojo::HandleGetUsers() {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::CheckUserStatus(const AccountId& account_id) {
  NOTIMPLEMENTED();
}

void LoginDisplayMojo::OnUserImageChanged(const user_manager::User& user) {
  ash::LoginScreen::Get()->GetModel()->SetAvatarForUser(
      user.GetAccountId(),
      UserSelectionScreen::BuildAshUserAvatarForUser(user));
}

void LoginDisplayMojo::OnPinCanAuthenticate(const AccountId& account_id,
                                            bool can_authenticate) {
  ash::LoginScreen::Get()->GetModel()->SetPinEnabledForUser(account_id,
                                                            can_authenticate);
}

}  // namespace chromeos
