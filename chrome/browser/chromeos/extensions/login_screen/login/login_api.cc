// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>

#include "ash/components/login/auth/key.h"
#include "ash/components/login/auth/user_context.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/shared_session_handler.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/extensions/api/login.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace extensions {

namespace {

std::string GetLaunchExtensionIdPrefValue(const user_manager::User* user) {
  Profile* profile = chromeos::ProfileHelper::Get()->GetProfileByUser(user);
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  return prefs->GetString(prefs::kLoginExtensionApiLaunchExtensionId);
}

}  // namespace

namespace login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                               "");
}

}  // namespace login_api

namespace login_api_errors {

const char kAlreadyActiveSession[] = "There is already an active session";
const char kLoginScreenIsNotActive[] = "Login screen is not active";
const char kAnotherLoginAttemptInProgress[] =
    "Another login attempt is in progress";
const char kNoManagedGuestSessionAccounts[] =
    "No managed guest session accounts";
const char kNoPermissionToLock[] =
    "The extension does not have permission to lock this session";
const char kSessionIsNotActive[] = "Session is not active";
const char kNoPermissionToUnlock[] =
    "The extension does not have permission to unlock this session";
const char kSessionIsNotLocked[] = "Session is not locked";
const char kAnotherUnlockAttemptInProgress[] =
    "Another unlock attempt is in progress";
const char kSharedMGSAlreadyLaunched[] =
    "Shared Managed Guest Session has already been launched";
const char kAuthenticationFailed[] = "Authentication failed";
const char kNoSharedMGSFound[] = "No shared Managed Guest Session found";
const char kSharedSessionIsNotActive[] = "Shared session is not active";
const char kSharedSessionAlreadyLaunched[] =
    "Another shared session has already been launched";
const char kScryptFailure[] = "Scrypt failed";
const char kCleanupInProgress[] = "Cleanup is already in progress";
const char kUnlockFailure[] = "Managed Guest Session unlock failed";
const char kNoPermissionToUseApi[] =
    "The extension does not have permission to use this API";

}  // namespace login_api_errors

LoginLaunchManagedGuestSessionFunction::
    LoginLaunchManagedGuestSessionFunction() = default;
LoginLaunchManagedGuestSessionFunction::
    ~LoginLaunchManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchManagedGuestSessionFunction::Run() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  auto parameters =
      api::login::LaunchManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    return RespondNow(Error(login_api_errors::kAlreadyActiveSession));
  }

  auto* existing_user_controller =
      ash::ExistingUserController::current_controller();
  if (existing_user_controller->IsSigninInProgress()) {
    return RespondNow(Error(login_api_errors::kAnotherLoginAttemptInProgress));
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  for (const user_manager::User* user : user_manager->GetUsers()) {
    if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      continue;
    ash::UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                             user->GetAccountId());
    if (parameters->password) {
      context.SetKey(ash::Key(*parameters->password));
      context.SetManagedGuestSessionLaunchExtensionId(extension_id());
    }

    existing_user_controller->Login(context, ash::SigninSpecifics());
    return RespondNow(NoArguments());
  }
  return RespondNow(Error(login_api_errors::kNoManagedGuestSessionAccounts));
}

LoginExitCurrentSessionFunction::LoginExitCurrentSessionFunction() = default;
LoginExitCurrentSessionFunction::~LoginExitCurrentSessionFunction() = default;

ExtensionFunction::ResponseAction LoginExitCurrentSessionFunction::Run() {
  auto parameters = api::login::ExitCurrentSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);

  if (parameters->data_for_next_login_attempt) {
    local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                           *parameters->data_for_next_login_attempt);
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
  DCHECK(local_state);
  std::string data_for_next_login_attempt =
      local_state->GetString(prefs::kLoginExtensionApiDataForNextLoginAttempt);
  local_state->ClearPref(prefs::kLoginExtensionApiDataForNextLoginAttempt);

  return RespondNow(OneArgument(base::Value(data_for_next_login_attempt)));
}

LoginLockManagedGuestSessionFunction::LoginLockManagedGuestSessionFunction() =
    default;
LoginLockManagedGuestSessionFunction::~LoginLockManagedGuestSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginLockManagedGuestSessionFunction::Run() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  const user_manager::UserManager* user_manager =
      user_manager::UserManager::Get();
  const user_manager::User* active_user = user_manager->GetActiveUser();
  if (!active_user ||
      active_user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      !user_manager->CanCurrentUserLock()) {
    return RespondNow(Error(login_api_errors::kNoPermissionToLock));
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::ACTIVE) {
    return RespondNow(Error(login_api_errors::kSessionIsNotActive));
  }

  chromeos::LoginApiLockHandler::Get()->RequestLockScreen();
  return RespondNow(NoArguments());
}

LoginUnlockManagedGuestSessionFunction::
    LoginUnlockManagedGuestSessionFunction() = default;
LoginUnlockManagedGuestSessionFunction::
    ~LoginUnlockManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginUnlockManagedGuestSessionFunction::Run() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  auto parameters =
      api::login::UnlockManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user ||
      active_user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT ||
      extension_id() != GetLaunchExtensionIdPrefValue(active_user)) {
    return RespondNow(Error(login_api_errors::kNoPermissionToUnlock));
  }

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOCKED) {
    return RespondNow(Error(login_api_errors::kSessionIsNotLocked));
  }

  chromeos::LoginApiLockHandler* handler = chromeos::LoginApiLockHandler::Get();
  if (handler->IsUnlockInProgress()) {
    return RespondNow(Error(login_api_errors::kAnotherUnlockAttemptInProgress));
  }

  ash::UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                           active_user->GetAccountId());
  context.SetKey(ash::Key(parameters->password));
  handler->Authenticate(
      context,
      base::BindOnce(
          &LoginUnlockManagedGuestSessionFunction::OnAuthenticationComplete,
          this));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void LoginUnlockManagedGuestSessionFunction::OnAuthenticationComplete(
    bool success) {
  if (!success) {
    Respond(Error(login_api_errors::kAuthenticationFailed));
    return;
  }

  Respond(NoArguments());
}

LoginLaunchSharedManagedGuestSessionFunction::
    LoginLaunchSharedManagedGuestSessionFunction() = default;
LoginLaunchSharedManagedGuestSessionFunction::
    ~LoginLaunchSharedManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchSharedManagedGuestSessionFunction::Run() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  auto parameters =
      api::login::LaunchSharedManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  absl::optional<std::string> error =
      chromeos::SharedSessionHandler::Get()->LaunchSharedManagedGuestSession(
          extension_id(), parameters->password);
  if (error) {
    return RespondNow(Error(*error));
  }

  return RespondNow(NoArguments());
}

LoginEnterSharedSessionFunction::LoginEnterSharedSessionFunction() = default;
LoginEnterSharedSessionFunction::~LoginEnterSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEnterSharedSessionFunction::Run() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  auto parameters = api::login::EnterSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  chromeos::SharedSessionHandler::Get()->EnterSharedSession(
      parameters->password,
      base::BindOnce(
          &LoginEnterSharedSessionFunction::OnEnterSharedSessionComplete,
          this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void LoginEnterSharedSessionFunction::OnEnterSharedSessionComplete(
    absl::optional<std::string> error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  Respond(NoArguments());
}

LoginUnlockSharedSessionFunction::LoginUnlockSharedSessionFunction() = default;
LoginUnlockSharedSessionFunction::~LoginUnlockSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginUnlockSharedSessionFunction::Run() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  auto parameters = api::login::UnlockSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const user_manager::User* active_user =
      user_manager::UserManager::Get()->GetActiveUser();
  if (!active_user ||
      extension_id() != GetLaunchExtensionIdPrefValue(active_user)) {
    return RespondNow(Error(login_api_errors::kNoPermissionToUnlock));
  }

  chromeos::SharedSessionHandler::Get()->UnlockSharedSession(
      parameters->password,
      base::BindOnce(
          &LoginUnlockSharedSessionFunction::OnUnlockSharedSessionComplete,
          this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void LoginUnlockSharedSessionFunction::OnUnlockSharedSessionComplete(
    absl::optional<std::string> error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  Respond(NoArguments());
}

LoginEndSharedSessionFunction::LoginEndSharedSessionFunction() = default;
LoginEndSharedSessionFunction::~LoginEndSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEndSharedSessionFunction::Run() {
  chromeos::SharedSessionHandler::Get()->EndSharedSession(base::BindOnce(
      &LoginEndSharedSessionFunction::OnEndSharedSessionComplete, this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void LoginEndSharedSessionFunction::OnEndSharedSessionComplete(
    absl::optional<std::string> error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  Respond(NoArguments());
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
  DCHECK(local_state);
  local_state->SetString(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                         parameters->data_for_next_login_attempt);

  return RespondNow(NoArguments());
}

}  // namespace extensions
