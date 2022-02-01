// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/login_ash.h"

#include "ash/components/login/auth/user_context.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/errors.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/shared_session_handler.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/pref_names.h"
#include "chromeos/crosapi/mojom/login.mojom.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace crosapi {

LoginAsh::LoginAsh() = default;
LoginAsh::~LoginAsh() = default;

void LoginAsh::BindReceiver(mojo::PendingReceiver<mojom::Login> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LoginAsh::LaunchManagedGuestSession(
    const absl::optional<std::string>& password,
    LaunchManagedGuestSessionCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    std::move(callback).Run(
        extensions::login_api_errors::kAlreadyActiveSession);
    return;
  }

  auto* existing_user_controller =
      ash::ExistingUserController::current_controller();
  if (existing_user_controller->IsSigninInProgress()) {
    std::move(callback).Run(
        extensions::login_api_errors::kAnotherLoginAttemptInProgress);
    return;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  for (const user_manager::User* user : user_manager->GetUsers()) {
    if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      continue;
    ash::UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                             user->GetAccountId());
    if (password) {
      context.SetKey(chromeos::Key(*password));
      context.SetCanLockManagedGuestSession(true);
    }

    existing_user_controller->Login(context, ash::SigninSpecifics());
    std::move(callback).Run(absl::nullopt);
    return;
  }
  std::move(callback).Run(
      extensions::login_api_errors::kNoManagedGuestSessionAccounts);
}

void LoginAsh::ExitCurrentSession(
    const absl::optional<std::string>& data_for_next_login_attempt,
    ExitCurrentSessionCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  if (data_for_next_login_attempt) {
    local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                           *data_for_next_login_attempt);
  } else {
    local_state->ClearPref(prefs::kLoginExtensionApiDataForNextLoginAttempt);
  }

  chrome::AttemptUserExit();
  std::move(callback).Run(absl::nullopt);
}

void LoginAsh::FetchDataForNextLoginAttempt(
    FetchDataForNextLoginAttemptCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  std::string data_for_next_login_attempt =
      local_state->GetString(prefs::kLoginExtensionApiDataForNextLoginAttempt);
  local_state->ClearPref(prefs::kLoginExtensionApiDataForNextLoginAttempt);

  std::move(callback).Run(data_for_next_login_attempt);
}

void LoginAsh::LockManagedGuestSession(
    LockManagedGuestSessionCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user ||
      active_user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      !user_manager->CanCurrentUserLock()) {
    std::move(callback).Run(
        extensions::login_api_errors::kNoLockableManagedGuestSession);
    return;
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    std::move(callback).Run(extensions::login_api_errors::kSessionIsNotActive);
    return;
  }

  chromeos::LoginApiLockHandler::Get()->RequestLockScreen();
  std::move(callback).Run(absl::nullopt);
}

void LoginAsh::UnlockManagedGuestSession(
    const std::string& password,
    UnlockManagedGuestSessionCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user ||
      active_user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      !user_manager->CanCurrentUserLock()) {
    std::move(callback).Run(
        extensions::login_api_errors::kNoUnlockableManagedGuestSession);
    return;
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    std::move(callback).Run(extensions::login_api_errors::kSessionIsNotLocked);
    return;
  }

  chromeos::LoginApiLockHandler* handler = chromeos::LoginApiLockHandler::Get();
  if (handler->IsUnlockInProgress()) {
    std::move(callback).Run(
        extensions::login_api_errors::kAnotherUnlockAttemptInProgress);
    return;
  }

  ash::UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                           active_user->GetAccountId());
  context.SetKey(chromeos::Key(password));
  handler->Authenticate(
      context, base::BindOnce(&LoginAsh::OnScreenLockerAuthenticate,
                              weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginAsh::LaunchSharedManagedGuestSession(
    const std::string& password,
    LaunchSharedManagedGuestSessionCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  absl::optional<std::string> error =
      chromeos::SharedSessionHandler::Get()->LaunchSharedManagedGuestSession(
          password);
  if (error) {
    std::move(callback).Run(error);
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

void LoginAsh::EnterSharedSession(const std::string& password,
                                  EnterSharedSessionCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  chromeos::SharedSessionHandler::Get()->EnterSharedSession(
      password,
      base::BindOnce(&LoginAsh::OnOptionalErrorCallbackComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginAsh::UnlockSharedSession(const std::string& password,
                                   UnlockSharedSessionCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user ||
      active_user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      !user_manager->CanCurrentUserLock()) {
    std::move(callback).Run(
        extensions::login_api_errors::kNoUnlockableManagedGuestSession);
    return;
  }

  chromeos::SharedSessionHandler::Get()->UnlockSharedSession(
      password,
      base::BindOnce(&LoginAsh::OnOptionalErrorCallbackComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginAsh::EndSharedSession(EndSharedSessionCallback callback) {
  chromeos::SharedSessionHandler::Get()->EndSharedSession(
      base::BindOnce(&LoginAsh::OnOptionalErrorCallbackComplete,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void LoginAsh::SetDataForNextLoginAttempt(
    const std::string& data_for_next_login_attempt,
    SetDataForNextLoginAttemptCallback callback) {
  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         data_for_next_login_attempt);

  std::move(callback).Run();
}

void LoginAsh::OnScreenLockerAuthenticate(
    base::OnceCallback<void(const absl::optional<std::string>&)> callback,
    bool success) {
  if (!success) {
    std::move(callback).Run(
        extensions::login_api_errors::kAuthenticationFailed);
    return;
  }

  std::move(callback).Run(absl::nullopt);
}

void LoginAsh::OnOptionalErrorCallbackComplete(
    base::OnceCallback<void(const absl::optional<std::string>&)> callback,
    absl::optional<std::string> error) {
  std::move(callback).Run(error);
}

}  // namespace crosapi
