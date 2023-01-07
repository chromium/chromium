// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/ui/login_display_mojo.h"

#include "ash/public/cpp/login_screen.h"
#include "ash/public/cpp/login_screen_model.h"
#include "ash/public/cpp/login_types.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_mode/kiosk_app_manager.h"
#include "chrome/browser/ash/login/challenge_response_auth_keys_loader.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/quick_unlock/pin_backend.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/user_selection_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host_mojo.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/login_screen_client_impl.h"
#include "chrome/browser/ui/webui/ash/login/enable_adb_sideloading_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/reset_screen_handler.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/session_manager/core/session_manager.h"

namespace ash {

LoginDisplayMojo::LoginDisplayMojo(LoginDisplayHostMojo* host) : host_(host) {
  user_manager::UserManager::Get()->AddObserver(this);
}

LoginDisplayMojo::~LoginDisplayMojo() {
  user_manager::UserManager::Get()->RemoveObserver(this);
}

void LoginDisplayMojo::UpdatePinKeyboardState(const AccountId& account_id) {
  quick_unlock::PinBackend::GetInstance()->CanAuthenticate(
      // Currently if PIN is cryptohome-based, PinCanAuthenticate always return
      // true if there's a set up PIN, even if the quick unlock policy disables
      // it. And if PIN is pref-based it always returns false regardless of the
      // policy because pref-based PIN doesn't have capability to decrypt the
      // user's cryptohome. So just pass an arbitrary purpose here.
      account_id, quick_unlock::Purpose::kAny,
      base::BindOnce(&LoginDisplayMojo::OnPinCanAuthenticate,
                     weak_factory_.GetWeakPtr(), account_id));
}

void LoginDisplayMojo::UpdateChallengeResponseAuthAvailability(
    const AccountId& account_id) {
  const bool enable_challenge_response =
      ChallengeResponseAuthKeysLoader::CanAuthenticateUser(account_id);
  LoginScreen::Get()->GetModel()->SetChallengeResponseAuthEnabledForUser(
      account_id, enable_challenge_response);
}

void LoginDisplayMojo::Init(const user_manager::UserList& filtered_users,
                            bool show_guest) {
  host_->SetUserCount(filtered_users.size());
  host_->UpdateAddUserButtonStatus();
  auto* client = LoginScreenClientImpl::Get();

  // ExistingUserController::DeviceSettingsChanged and others may initialize the
  // login screen multiple times. Views-login only supports initialization once.
  if (!initialized_) {
    client->SetDelegate(host_);
    LoginScreen::Get()->ShowLoginScreen();
  }

  UserSelectionScreen* user_selection_screen = host_->user_selection_screen();
  user_selection_screen->Init(filtered_users);
  LoginScreen::Get()->GetModel()->SetUserList(
      user_selection_screen->UpdateAndReturnUserListForAsh());
  LoginScreen::Get()->SetAllowLoginAsGuest(show_guest);
  user_selection_screen->SetUsersLoaded(true /*loaded*/);

  if (user_manager::UserManager::IsInitialized()) {
    // Enable pin and challenge-response authentication for any users who can
    // use them.
    for (const user_manager::User* user : filtered_users) {
      if (!user->IsDeviceLocalAccount()) {
        UpdatePinKeyboardState(user->GetAccountId());
        UpdateChallengeResponseAuthAvailability(user->GetAccountId());
      }
    }
  }

  if (!initialized_) {
    initialized_ = true;

    // login-prompt-visible is recorded and tracked to verify boot performance
    // does not regress. Autotests may also depend on it (ie,
    // login_SameSessionTwice).
    VLOG(1) << "Emitting login-prompt-visible";
    SessionManagerClient::Get()->EmitLoginPromptVisible();

    // TODO(crbug.com/1305245) - Remove once the issue is fixed.
    LOG(WARNING) << "LoginDisplayMojo::Init() NotifyLoginOrLockScreenVisible";
    session_manager::SessionManager::Get()->NotifyLoginOrLockScreenVisible();

    // If there no available users exist, delay showing the dialogs until after
    // GAIA dialog is shown (GAIA dialog will check these local state values,
    // too). Login UI will show GAIA dialog if no user are registered, which
    // might hide any UI shown here.
    if (filtered_users.empty())
      return;

    // TODO(crbug.com/1105387): Part of initial screen logic.
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

void LoginDisplayMojo::SetUIEnabled(bool is_enabled) {
  // OOBE UI is null iff we display the user adding screen.
  if (is_enabled && host_->GetOobeUI() != nullptr) {
    host_->GetOobeUI()->ShowOobeUI(false);
  }
}

void LoginDisplayMojo::OnUserImageChanged(const user_manager::User& user) {
  LoginScreen::Get()->GetModel()->SetAvatarForUser(
      user.GetAccountId(),
      UserSelectionScreen::BuildAshUserAvatarForUser(user));
}

void LoginDisplayMojo::ShowOwnerPod(const AccountId& owner) {
  const user_manager::User* device_owner =
      user_manager::UserManager::Get()->FindUser(owner);
  CHECK(device_owner);

  std::vector<LoginUserInfo> user_info_list;
  LoginUserInfo user_info;
  user_info.basic_user_info.type = device_owner->GetType();
  user_info.basic_user_info.account_id = device_owner->GetAccountId();
  user_info.basic_user_info.display_name =
      base::UTF16ToUTF8(device_owner->GetDisplayName());
  user_info.basic_user_info.display_email = device_owner->display_email();
  user_info.basic_user_info.avatar =
      UserSelectionScreen::BuildAshUserAvatarForUser(*device_owner);
  user_info.auth_type = proximity_auth::mojom::AuthType::OFFLINE_PASSWORD;
  user_info.is_signed_in = device_owner->is_logged_in();
  user_info.is_device_owner = true;
  user_info.can_remove = false;
  user_info_list.push_back(user_info);

  LoginScreen::Get()->GetModel()->SetUserList(user_info_list);
  LoginScreen::Get()->SetAllowLoginAsGuest(false);
  LoginScreen::Get()->EnableAddUserButton(false);

  // Disable PIN.
  LoginScreen::Get()->GetModel()->SetPinEnabledForUser(owner,
                                                       /*enabled=*/false);
}

void LoginDisplayMojo::OnPinCanAuthenticate(const AccountId& account_id,
                                            bool can_authenticate) {
  LoginScreen::Get()->GetModel()->SetPinEnabledForUser(account_id,
                                                       can_authenticate);
}

}  // namespace ash
