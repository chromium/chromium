// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/login/existing_user_controller.h"
#include "chrome/browser/ash/login/signin_specifics.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/extensions/login_screen/login/login_api_lock_handler.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/extensions/api/login.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/key.h"
#include "chromeos/login/auth/user_context.h"
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
const char kAuthenticationFailed[] = "Authentication failed";

}  // namespace login_api_errors

LoginLaunchManagedGuestSessionFunction::
    LoginLaunchManagedGuestSessionFunction() = default;
LoginLaunchManagedGuestSessionFunction::
    ~LoginLaunchManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchManagedGuestSessionFunction::Run() {
  ui::UserActivityDetector::Get()->HandleExternalUserActivity();

  std::unique_ptr<api::login::LaunchManagedGuestSession::Params> parameters =
      api::login::LaunchManagedGuestSession::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters);

  if (session_manager::SessionManager::Get()->session_state() !=
      session_manager::SessionState::LOGIN_PRIMARY) {
    return RespondNow(Error(login_api_errors::kAlreadyActiveSession));
  }

  chromeos::ExistingUserController* existing_user_controller =
      chromeos::ExistingUserController::current_controller();
  if (existing_user_controller->IsSigninInProgress()) {
    return RespondNow(Error(login_api_errors::kAnotherLoginAttemptInProgress));
  }

  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  for (const user_manager::User* user : user_manager->GetUsers()) {
    if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      continue;
    chromeos::UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                  user->GetAccountId());
    if (parameters->password) {
      context.SetKey(chromeos::Key(*parameters->password));
      context.SetManagedGuestSessionLaunchExtensionId(extension_id());
    }

    existing_user_controller->Login(context, chromeos::SigninSpecifics());
    return RespondNow(NoArguments());
  }
  return RespondNow(Error(login_api_errors::kNoManagedGuestSessionAccounts));
}

LoginExitCurrentSessionFunction::LoginExitCurrentSessionFunction() = default;
LoginExitCurrentSessionFunction::~LoginExitCurrentSessionFunction() = default;

ExtensionFunction::ResponseAction LoginExitCurrentSessionFunction::Run() {
  std::unique_ptr<api::login::ExitCurrentSession::Params> parameters =
      api::login::ExitCurrentSession::Params::Create(*args_);
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

  std::unique_ptr<api::login::UnlockManagedGuestSession::Params> parameters =
      api::login::UnlockManagedGuestSession::Params::Create(*args_);
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

  chromeos::UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                active_user->GetAccountId());
  context.SetKey(chromeos::Key(parameters->password));
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

}  // namespace extensions
