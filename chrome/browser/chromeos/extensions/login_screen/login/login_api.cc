// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login/login_api.h"

#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_ash.h"
#include "chrome/common/extensions/api/login.h"
#include "google_apis/gaia/gaia_id.h"

namespace extensions {

namespace {
crosapi::LoginAsh* GetLoginApi() {
  return crosapi::CrosapiManager::Get()->crosapi_ash()->login_ash();
}
}  // namespace

ExtensionFunctionWithOptionalErrorResult::
    ~ExtensionFunctionWithOptionalErrorResult() = default;

void ExtensionFunctionWithOptionalErrorResult::OnResult(
    const std::optional<std::string>& error) {
  if (error) {
    Respond(Error(*error));
    return;
  }

  return Respond(NoArguments());
}

ExtensionFunctionWithStringResult::~ExtensionFunctionWithStringResult() =
    default;

void ExtensionFunctionWithStringResult::OnResult(const std::string& result) {
  Respond(WithArguments(result));
}

ExtensionFunctionWithVoidResult::~ExtensionFunctionWithVoidResult() = default;

void ExtensionFunctionWithVoidResult::OnResult() {
  Respond(NoArguments());
}

LoginLaunchManagedGuestSessionFunction::
    LoginLaunchManagedGuestSessionFunction() = default;
LoginLaunchManagedGuestSessionFunction::
    ~LoginLaunchManagedGuestSessionFunction() = default;

ExtensionFunction::ResponseAction
LoginLaunchManagedGuestSessionFunction::Run() {
  auto parameters =
      api::login::LaunchManagedGuestSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginLaunchManagedGuestSessionFunction::OnResult, this);

  std::optional<std::string> password;
  if (parameters->password) {
    password = std::move(*parameters->password);
  }
  GetLoginApi()->LaunchManagedGuestSession(password, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginExitCurrentSessionFunction::LoginExitCurrentSessionFunction() = default;
LoginExitCurrentSessionFunction::~LoginExitCurrentSessionFunction() = default;

ExtensionFunction::ResponseAction LoginExitCurrentSessionFunction::Run() {
  auto parameters = api::login::ExitCurrentSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginExitCurrentSessionFunction::OnResult, this);

  std::optional<std::string> data_for_next_login_attempt;
  if (parameters->data_for_next_login_attempt) {
    data_for_next_login_attempt =
        std::move(*parameters->data_for_next_login_attempt);
  }
  GetLoginApi()->ExitCurrentSession(data_for_next_login_attempt,
                                    std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginFetchDataForNextLoginAttemptFunction::
    LoginFetchDataForNextLoginAttemptFunction() = default;
LoginFetchDataForNextLoginAttemptFunction::
    ~LoginFetchDataForNextLoginAttemptFunction() = default;

ExtensionFunction::ResponseAction
LoginFetchDataForNextLoginAttemptFunction::Run() {
  auto callback = base::BindOnce(
      &LoginFetchDataForNextLoginAttemptFunction::OnResult, this);

  GetLoginApi()->FetchDataForNextLoginAttempt(std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginLockManagedGuestSessionFunction::LoginLockManagedGuestSessionFunction() =
    default;
LoginLockManagedGuestSessionFunction::~LoginLockManagedGuestSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginLockManagedGuestSessionFunction::Run() {
  auto callback =
      base::BindOnce(&LoginLockManagedGuestSessionFunction::OnResult, this);

  GetLoginApi()->LockManagedGuestSession(std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
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

  auto callback =
      base::BindOnce(&LoginUnlockManagedGuestSessionFunction::OnResult, this);

  GetLoginApi()->UnlockManagedGuestSession(parameters->password,
                                           std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginLockCurrentSessionFunction::LoginLockCurrentSessionFunction() = default;
LoginLockCurrentSessionFunction::~LoginLockCurrentSessionFunction() = default;

ExtensionFunction::ResponseAction LoginLockCurrentSessionFunction::Run() {
  auto callback =
      base::BindOnce(&LoginLockCurrentSessionFunction::OnResult, this);

  GetLoginApi()->LockCurrentSession(std::move(callback));
  return RespondLater();
}

LoginUnlockCurrentSessionFunction::LoginUnlockCurrentSessionFunction() =
    default;
LoginUnlockCurrentSessionFunction::~LoginUnlockCurrentSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginUnlockCurrentSessionFunction::Run() {
  auto parameters = api::login::UnlockCurrentSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginUnlockCurrentSessionFunction::OnResult, this);

  GetLoginApi()->UnlockCurrentSession(parameters->password,
                                      std::move(callback));
  return RespondLater();
}

LoginLaunchSamlUserSessionFunction::LoginLaunchSamlUserSessionFunction() =
    default;
LoginLaunchSamlUserSessionFunction::~LoginLaunchSamlUserSessionFunction() =
    default;

ExtensionFunction::ResponseAction LoginLaunchSamlUserSessionFunction::Run() {
  auto parameters = api::login::LaunchSamlUserSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginLaunchSamlUserSessionFunction::OnResult, this);

  GetLoginApi()->LaunchSamlUserSession(
      parameters->properties.email, GaiaId(parameters->properties.gaia_id),
      parameters->properties.password, parameters->properties.oauth_code,
      std::move(callback));
  return RespondLater();
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

  auto callback = base::BindOnce(
      &LoginLaunchSharedManagedGuestSessionFunction::OnResult, this);

  GetLoginApi()->LaunchSharedManagedGuestSession(parameters->password,
                                                 std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginEnterSharedSessionFunction::LoginEnterSharedSessionFunction() = default;
LoginEnterSharedSessionFunction::~LoginEnterSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEnterSharedSessionFunction::Run() {
  auto parameters = api::login::EnterSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginEnterSharedSessionFunction::OnResult, this);

  GetLoginApi()->EnterSharedSession(parameters->password, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginUnlockSharedSessionFunction::LoginUnlockSharedSessionFunction() = default;
LoginUnlockSharedSessionFunction::~LoginUnlockSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginUnlockSharedSessionFunction::Run() {
  auto parameters = api::login::EnterSharedSession::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  auto callback =
      base::BindOnce(&LoginUnlockSharedSessionFunction::OnResult, this);

  GetLoginApi()->UnlockSharedSession(parameters->password, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginEndSharedSessionFunction::LoginEndSharedSessionFunction() = default;
LoginEndSharedSessionFunction::~LoginEndSharedSessionFunction() = default;

ExtensionFunction::ResponseAction LoginEndSharedSessionFunction::Run() {
  auto callback =
      base::BindOnce(&LoginEndSharedSessionFunction::OnResult, this);

  GetLoginApi()->EndSharedSession(std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
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

  auto callback =
      base::BindOnce(&LoginSetDataForNextLoginAttemptFunction::OnResult, this);

  GetLoginApi()->SetDataForNextLoginAttempt(
      parameters->data_for_next_login_attempt, std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

LoginRequestExternalLogoutFunction::LoginRequestExternalLogoutFunction() =
    default;
LoginRequestExternalLogoutFunction::~LoginRequestExternalLogoutFunction() =
    default;

ExtensionFunction::ResponseAction LoginRequestExternalLogoutFunction::Run() {
  GetLoginApi()->NotifyOnRequestExternalLogout();
  return RespondNow(NoArguments());
}

LoginNotifyExternalLogoutDoneFunction::LoginNotifyExternalLogoutDoneFunction() =
    default;
LoginNotifyExternalLogoutDoneFunction::
    ~LoginNotifyExternalLogoutDoneFunction() = default;

ExtensionFunction::ResponseAction LoginNotifyExternalLogoutDoneFunction::Run() {
  GetLoginApi()->NotifyOnExternalLogoutDone();
  return RespondNow(NoArguments());
}

}  // namespace extensions
