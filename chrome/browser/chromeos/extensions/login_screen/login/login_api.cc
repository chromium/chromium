// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/errors.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/shared_session_handler.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/common/extensions/api/login.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "extensions/browser/event_router.h"
#include "google_apis/gaia/gaia_id.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace extensions {

namespace {

base::expected<void, std::string> LockSession(
    std::optional<user_manager::UserType> user_type) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  const auto* user_manager = user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user || !active_user->CanLock() ||
      (user_type && active_user->GetType() != user_type)) {
    return base::unexpected(extensions::login_api_errors::kNoLockableSession);
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return base::unexpected(extensions::login_api_errors::kSessionIsNotActive);
  }

  chromeos::LoginApiLockHandler::Get()->RequestLockScreen();
  return base::ok();
}

void UnlockSession(
    std::string password,
    std::optional<user_manager::UserType> user_type,
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  const auto* user_manager = user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user || !active_user->CanLock() ||
      (user_type && active_user->GetType() != user_type)) {
    std::move(callback).Run(
        base::unexpected(extensions::login_api_errors::kNoUnlockableSession));
    return;
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    std::move(callback).Run(
        base::unexpected(extensions::login_api_errors::kSessionIsNotLocked));
    return;
  }

  auto* handler = chromeos::LoginApiLockHandler::Get();
  if (handler->IsUnlockInProgress()) {
    std::move(callback).Run(base::unexpected(
        extensions::login_api_errors::kAnotherUnlockAttemptInProgress));
    return;
  }

  ash::UserContext context(active_user->GetType(), active_user->GetAccountId());
  context.SetKey(ash::Key(std::move(password)));

  handler->Authenticate(
      context,
      base::BindOnce([](bool success) -> base::expected<void, std::string> {
        if (!success) {
          return base::unexpected(
              extensions::login_api_errors::kAuthenticationFailed);
        }
        return base::ok();
      }).Then(std::move(callback)));
}

base::OnceCallback<void(const std::optional<std::string>&)>
AdaptOptionalErrorCallback(
    base::OnceCallback<void(base::expected<void, std::string>)> callback) {
  return base::BindOnce([](const std::optional<std::string>& result)
                            -> base::expected<void, std::string> {
           if (result.has_value()) {
             return base::unexpected(result.value());
           }
           return base::ok();
         })
      .Then(std::move(callback));
}

base::expected<void, std::string> CanLaunchSession() {
  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    return base::unexpected(
        extensions::login_api_errors::kAlreadyActiveSession);
  }

  auto* existing_user_controller =
      ash::ExistingUserController::current_controller();
  if (existing_user_controller->IsSigninInProgress()) {
    return base::unexpected(
        extensions::login_api_errors::kAnotherLoginAttemptInProgress);
  }

  return base::ok();
}

user_manager::User* FindPublicAccountUser() {
  for (user_manager::User* user :
       user_manager::UserManager::Get()->GetPersistedUsers()) {
    CHECK(user);
    if (user->GetType() == user_manager::UserType::kPublicAccount) {
      return user;
    }
  }
  return nullptr;
}

}  // namespace

namespace internal {

LoginAsyncFunctionBase::~LoginAsyncFunctionBase() = default;

void LoginAsyncFunctionBase::OnResult(
    base::expected<void, std::string> result) {
  Respond(result.has_value() ? NoArguments()
                             : Error(std::move(result.error())));
}

ExtensionFunction::ResponseAction LoginAsyncFunctionBase::MaybeResponded() {
  return did_respond() ? AlreadyResponded() : RespondLater();
}

}  // namespace internal

LoginLaunchManagedGuestSessionFunction::
    LoginLaunchManagedGuestSessionFunction() = default;
LoginLaunchManagedGuestSessionFunction::
    ~LoginLaunchManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchManagedGuestSessionFunction::Run() {
  auto parameters =
      api::login::LaunchManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  if (auto can_launch = CanLaunchSession(); !can_launch.has_value()) {
    return RespondNow(Error(std::move(can_launch.error())));
  }

  const user_manager::User* user = FindPublicAccountUser();
  if (!user) {
    return RespondNow(
        Error(extensions::login_api_errors::kNoManagedGuestSessionAccounts));
  }

  ash::UserContext context(user_manager::UserType::kPublicAccount,
                           user->GetAccountId());
  if (parameters->password) {
    context.SetKey(ash::Key(*parameters->password));
    context.SetSamlPassword(ash::SamlPassword{*parameters->password});
    context.SetCanLockManagedGuestSession(true);
  }

  auto* existing_user_controller =
      ash::ExistingUserController::current_controller();
  existing_user_controller->Login(context, ash::SigninSpecifics());
  return RespondNow(NoArguments());
}

LoginExitCurrentSessionFunction::LoginExitCurrentSessionFunction() = default;
LoginExitCurrentSessionFunction::~LoginExitCurrentSessionFunction() = default;

ExtensionFunction::ResponseAction LoginExitCurrentSessionFunction::Run() {
  auto parameters = api::login::ExitCurrentSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PrefService* local_state = g_browser_process->local_state();
  CHECK(local_state);

  if (parameters->data_for_next_login_attempt) {
    local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                           std::move(*parameters->data_for_next_login_attempt));
  } else {
    local_state->ClearPref(prefs::kLoginExtensionApiDataForNextLoginAttempt);
  }
  chrome::AttemptUserExit();
  return RespondNow(NoArguments());
}

LoginFetchDataForNextLoginAttemptFunction::
    LoginFetchDataForNextLoginAttemptFunction() = default;
LoginFetchDataForNextLoginAttemptFunction::
    ~LoginFetchDataForNextLoginAttemptFunction() = default;

ExtensionFunction::ResponseAction
LoginFetchDataForNextLoginAttemptFunction::Run() {
  PrefService* local_state = g_browser_process->local_state();
  CHECK(local_state);

  std::string data_for_next_login_attempt =
      local_state->GetString(prefs::kLoginExtensionApiDataForNextLoginAttempt);
  local_state->ClearPref(prefs::kLoginExtensionApiDataForNextLoginAttempt);
  return RespondNow(WithArguments(std::move(data_for_next_login_attempt)));
}

LoginLockManagedGuestSessionFunction::LoginLockManagedGuestSessionFunction() =
    default;
LoginLockManagedGuestSessionFunction::~LoginLockManagedGuestSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginLockManagedGuestSessionFunction::Run() {
  auto result = LockSession(user_manager::UserType::kPublicAccount);
  return RespondNow(result.has_value() ? NoArguments()
                                       : Error(std::move(result.error())));
}

LoginUnlockManagedGuestSessionFunction::
    LoginUnlockManagedGuestSessionFunction() = default;
LoginUnlockManagedGuestSessionFunction::
    ~LoginUnlockManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginUnlockManagedGuestSessionFunction::Run() {
  auto parameters =
      api::login::UnlockManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  UnlockSession(
      parameters->password, user_manager::UserType::kPublicAccount,
      base::BindOnce(&LoginUnlockManagedGuestSessionFunction::OnResult, this));
  return MaybeResponded();
}

LoginLockCurrentSessionFunction::LoginLockCurrentSessionFunction() = default;
LoginLockCurrentSessionFunction::~LoginLockCurrentSessionFunction() = default;

ExtensionFunction::ResponseAction LoginLockCurrentSessionFunction::Run() {
  auto result = LockSession(std::nullopt);
  return RespondNow(result.has_value() ? NoArguments()
                                       : Error(std::move(result.error())));
}

LoginUnlockCurrentSessionFunction::LoginUnlockCurrentSessionFunction() =
    default;
LoginUnlockCurrentSessionFunction::~LoginUnlockCurrentSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginUnlockCurrentSessionFunction::Run() {
  auto parameters = api::login::UnlockCurrentSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  UnlockSession(
      parameters->password, std::nullopt,
      base::BindOnce(&LoginUnlockCurrentSessionFunction::OnResult, this));
  return MaybeResponded();
}

LoginLaunchSamlUserSessionFunction::LoginLaunchSamlUserSessionFunction() =
    default;
LoginLaunchSamlUserSessionFunction::~LoginLaunchSamlUserSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginLaunchSamlUserSessionFunction::Run() {
  auto parameters = api::login::LaunchSamlUserSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  ui::UserActivityDetector::Get()->HandleExternalUserActivity();
  if (auto can_launch = CanLaunchSession(); !can_launch.has_value()) {
    return RespondNow(Error(std::move(can_launch.error())));
  }

  ash::UserContext context(
      user_manager::UserType::kRegular,
      AccountId::FromUserEmailGaiaId(parameters->properties.email,
                                     GaiaId(parameters->properties.gaia_id)));
  ash::Key key(parameters->properties.password);
  key.SetLabel(ash::kCryptohomeGaiaKeyLabel);
  context.SetKey(key);
  context.SetSamlPassword(ash::SamlPassword{parameters->properties.password});
  context.SetPasswordKey(ash::Key(parameters->properties.password));
  context.SetAuthFlow(ash::UserContext::AUTH_FLOW_GAIA_WITH_SAML);
  context.SetIsUsingSamlPrincipalsApi(false);
  context.SetAuthCode(parameters->properties.oauth_code);

  ash::LoginDisplayHost::default_host()->CompleteLogin(context);
  return RespondNow(NoArguments());
}

LoginLaunchSharedManagedGuestSessionFunction::
    LoginLaunchSharedManagedGuestSessionFunction() = default;
LoginLaunchSharedManagedGuestSessionFunction::
    ~LoginLaunchSharedManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchSharedManagedGuestSessionFunction::Run() {
  auto parameters =
      api::login::LaunchSharedManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  std::optional<std::string> result =
      chromeos::SharedSessionHandler::Get()->LaunchSharedManagedGuestSession(
          parameters->password);
  return RespondNow(!result.has_value() ? NoArguments()
                                        : Error(std::move(result.value())));
}

LoginEnterSharedSessionFunction::LoginEnterSharedSessionFunction() = default;
LoginEnterSharedSessionFunction::~LoginEnterSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEnterSharedSessionFunction::Run() {
  auto parameters = api::login::EnterSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  chromeos::SharedSessionHandler::Get()->EnterSharedSession(
      parameters->password,
      AdaptOptionalErrorCallback(
          base::BindOnce(&LoginEnterSharedSessionFunction::OnResult, this)));
  return MaybeResponded();
}

LoginUnlockSharedSessionFunction::LoginUnlockSharedSessionFunction() = default;
LoginUnlockSharedSessionFunction::~LoginUnlockSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginUnlockSharedSessionFunction::Run() {
  auto parameters = api::login::EnterSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  const auto* user_manager = user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user ||
      active_user->GetType() != user_manager::UserType::kPublicAccount ||
      !active_user->CanLock()) {
    return RespondNow(
        Error(extensions::login_api_errors::kNoUnlockableSession));
  }

  chromeos::SharedSessionHandler::Get()->UnlockSharedSession(
      parameters->password,
      AdaptOptionalErrorCallback(
          base::BindOnce(&LoginUnlockSharedSessionFunction::OnResult, this)));
  return MaybeResponded();
}

LoginEndSharedSessionFunction::LoginEndSharedSessionFunction() = default;
LoginEndSharedSessionFunction::~LoginEndSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEndSharedSessionFunction::Run() {
  chromeos::SharedSessionHandler::Get()->EndSharedSession(
      AdaptOptionalErrorCallback(
          base::BindOnce(&LoginEndSharedSessionFunction::OnResult, this)));
  return MaybeResponded();
}

LoginSetDataForNextLoginAttemptFunction::
    LoginSetDataForNextLoginAttemptFunction() = default;
LoginSetDataForNextLoginAttemptFunction::
    ~LoginSetDataForNextLoginAttemptFunction() = default;

ExtensionFunction::ResponseAction
LoginSetDataForNextLoginAttemptFunction::Run() {
  auto parameters =
      api::login::SetDataForNextLoginAttempt::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PrefService* local_state = g_browser_process->local_state();
  CHECK(local_state);
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         parameters->data_for_next_login_attempt);
  return RespondNow(NoArguments());
}

LoginRequestExternalLogoutFunction::LoginRequestExternalLogoutFunction() =
    default;
LoginRequestExternalLogoutFunction::~LoginRequestExternalLogoutFunction() =
    default;

ExtensionFunction::ResponseAction LoginRequestExternalLogoutFunction::Run() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (Profile* profile : profile_manager->GetLoadedProfiles()) {
    EventRouter::Get(profile)->BroadcastEvent(
        std::make_unique<Event>(events::LOGIN_ON_REQUEST_EXTERNAL_LOGOUT,
                                api::login::OnRequestExternalLogout::kEventName,
                                api::login::OnRequestExternalLogout::Create()));
  }
  return RespondNow(NoArguments());
}

LoginNotifyExternalLogoutDoneFunction::LoginNotifyExternalLogoutDoneFunction() =
    default;
LoginNotifyExternalLogoutDoneFunction::
    ~LoginNotifyExternalLogoutDoneFunction() = default;

ExtensionFunction::ResponseAction LoginNotifyExternalLogoutDoneFunction::Run() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  for (Profile* profile : profile_manager->GetLoadedProfiles()) {
    EventRouter::Get(profile)->BroadcastEvent(
        std::make_unique<Event>(events::LOGIN_ON_EXTERNAL_LOGOUT_DONE,
                                api::login::OnExternalLogoutDone::kEventName,
                                api::login::OnExternalLogoutDone::Create()));
  }
  return RespondNow(NoArguments());
}

}  // namespace extensions
