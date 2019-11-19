// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/existing_user_controller.h"
#include "chrome/browser/chromeos/login/signin_specifics.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/common/extensions/api/login.h"
#include "chrome/common/pref_names.h"
#include "chromeos/login/auth/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace extensions {

namespace {

const char kErrorNoManagedGuestSessionAccounts[] =
    "No managed guest session accounts";

}  // namespace

namespace login_api {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kLoginExtensionApiDataForNextLoginAttempt,
                               "");
}

}  // namespace login_api

LoginLaunchManagedGuestSessionFunction::
    LoginLaunchManagedGuestSessionFunction() = default;
LoginLaunchManagedGuestSessionFunction::
    ~LoginLaunchManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchManagedGuestSessionFunction::Run() {
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  for (const user_manager::User* user : user_manager->GetUsers()) {
    if (!user || user->GetType() != user_manager::USER_TYPE_PUBLIC_ACCOUNT)
      continue;
    chromeos::UserContext context(user_manager::USER_TYPE_PUBLIC_ACCOUNT,
                                  user->GetAccountId());
    chromeos::ExistingUserController::current_controller()->Login(
        context, chromeos::SigninSpecifics());
    return RespondNow(NoArguments());
  }
  return RespondNow(Error(kErrorNoManagedGuestSessionAccounts));
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

  return RespondNow(
      OneArgument(std::make_unique<base::Value>(data_for_next_login_attempt)));
}

}  // namespace extensions
