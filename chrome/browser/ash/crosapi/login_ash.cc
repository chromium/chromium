// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/login_ash.h"

#include <optional>

#include "base/notimplemented.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/errors.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/shared_session_handler.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/public/auth_types.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace crosapi {

LoginAsh::LoginAsh() = default;
LoginAsh::~LoginAsh() = default;

void LoginAsh::BindReceiver(mojo::PendingReceiver<mojom::Login> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void LoginAsh::LaunchManagedGuestSession(
    const std::optional<std::string>& password,
    OptionalErrorCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  std::optional<std::string> error = CanLaunchSession();
  if (error) {
    std::move(callback).Run(error);
    return;
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  for (const user_manager::User* user : user_manager->GetPersistedUsers()) {
    if (!user || user->GetType() != user_manager::UserType::kPublicAccount) {
      continue;
    }
    ash::UserContext context(user_manager::UserType::kPublicAccount,
                             user->GetAccountId());
    if (password) {
      context.SetKey(ash::Key(*password));
      context.SetSamlPassword(ash::SamlPassword{*password});
      context.SetCanLockManagedGuestSession(true);
    }

    auto* existing_user_controller =
        ash::ExistingUserController::current_controller();
    existing_user_controller->Login(context, ash::SigninSpecifics());
    std::move(callback).Run(std::nullopt);
    return;
  }
  std::move(callback).Run(
      extensions::login_api_errors::kNoManagedGuestSessionAccounts);
}

void LoginAsh::LaunchSamlUserSession(const std::string& email,
                                     const GaiaId& gaia_id,
                                     const std::string& password,
                                     const std::string& oauth_code,
                                     OptionalErrorCallback callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  std::optional<std::string> error = CanLaunchSession();
  if (error) {
    std::move(callback).Run(error);
    return;
  }

  ash::UserContext context(user_manager::UserType::kRegular,
                           AccountId::FromUserEmailGaiaId(email, gaia_id));
  ash::Key key(password);
  key.SetLabel(ash::kCryptohomeGaiaKeyLabel);
  context.SetKey(key);
  context.SetSamlPassword(ash::SamlPassword{password});
  context.SetPasswordKey(ash::Key(password));
  context.SetAuthFlow(ash::UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  context.SetIsUsingSamlPrincipalsApi(false);
  context.SetAuthCode(oauth_code);

  ash::LoginDisplayHost::default_host()->CompleteLogin(context);
  std::move(callback).Run(std::nullopt);
}

void LoginAsh::AddExternalLogoutRequestObserver(
    mojo::PendingRemote<mojom::ExternalLogoutRequestObserver> observer) {
  mojo::Remote<mojom::ExternalLogoutRequestObserver> remote(
      std::move(observer));
  external_logout_request_observers_.Add(std::move(remote));
}

void LoginAsh::AddExternalLogoutDoneObserver(
    ExternalLogoutDoneObserver* observer) {
  external_logout_done_observers_.AddObserver(observer);
}

void LoginAsh::RemoveExternalLogoutDoneObserver(
    ExternalLogoutDoneObserver* observer) {
  external_logout_done_observers_.RemoveObserver(observer);
}

void LoginAsh::NotifyOnRequestExternalLogout() {
  for (auto& observer : external_logout_request_observers_) {
    observer->OnRequestExternalLogout();
  }
}

void LoginAsh::NotifyOnExternalLogoutDone() {
  for (auto& observer : external_logout_done_observers_) {
    observer.OnExternalLogoutDone();
  }
}

void LoginAsh::REMOVED_0(const std::optional<std::string>& password,
                         REMOVED_0Callback callback) {
  NOTIMPLEMENTED();
}

void LoginAsh::REMOVED_4(const std::string& password,
                         REMOVED_4Callback callback) {
  NOTIMPLEMENTED();
}

void LoginAsh::REMOVED_5(const std::string& password,
                         REMOVED_5Callback callback) {
  NOTIMPLEMENTED();
}

void LoginAsh::REMOVED_6(const std::string& password,
                         REMOVED_6Callback callback) {
  NOTIMPLEMENTED();
}

void LoginAsh::REMOVED_7(const std::string& password,
                         REMOVED_7Callback callback) {
  NOTIMPLEMENTED();
}

void LoginAsh::REMOVED_10(mojom::SamlUserSessionPropertiesPtr properties,
                          REMOVED_10Callback callback) {
  NOTIMPLEMENTED();
}

void LoginAsh::REMOVED_12(const std::string& password,
                          REMOVED_12Callback callback) {
  NOTIMPLEMENTED();
}

void LoginAsh::OnScreenLockerAuthenticate(OptionalErrorCallback callback,
                                          bool success) {
  if (!success) {
    std::move(callback).Run(
        extensions::login_api_errors::kAuthenticationFailed);
    return;
  }

  std::move(callback).Run(std::nullopt);
}

void LoginAsh::OnOptionalErrorCallbackComplete(
    OptionalErrorCallback callback,
    const std::optional<std::string>& error) {
  std::move(callback).Run(error);
}

std::optional<std::string> LoginAsh::CanLaunchSession() {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    return extensions::login_api_errors::kAlreadyActiveSession;
  }

  auto* existing_user_controller =
      ash::ExistingUserController::current_controller();
  if (existing_user_controller->IsSigninInProgress())
    return extensions::login_api_errors::kAnotherLoginAttemptInProgress;

  return std::nullopt;
}

}  // namespace crosapi
