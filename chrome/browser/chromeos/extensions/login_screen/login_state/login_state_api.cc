// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"

#include "base/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/login_state.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#else
#include "chrome/browser/ash/crosapi/crosapi_ash.h"
#include "chrome/browser/ash/crosapi/crosapi_manager.h"
#include "chrome/browser/ash/crosapi/login_state_ash.h"
#include "chrome/common/chrome_constants.h"
#endif

namespace {

bool IsSigninProfile(const Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return profile && profile->GetBaseName().value() == chrome::kInitialProfile;
#else
  return false;
#endif
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
const char kUnsupportedByAsh[] = "Not implemented.";

// Performs common crosapi validation. These errors are not caused by the
// extension so they are considered recoverable. Returns an error message on
// error, or nullopt on success.
absl::optional<std::string> ValidateCrosapi() {
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::LoginState>()) {
    return kUnsupportedByAsh;
  }
  return absl::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

namespace extensions {

api::login_state::SessionState ToApiEnum(crosapi::mojom::SessionState state) {
  switch (state) {
    case crosapi::mojom::SessionState::kUnknown:
      return api::login_state::SessionState::SESSION_STATE_UNKNOWN;
    case crosapi::mojom::SessionState::kInOobeScreen:
      return api::login_state::SessionState::SESSION_STATE_IN_OOBE_SCREEN;
    case crosapi::mojom::SessionState::kInLoginScreen:
      return api::login_state::SessionState::SESSION_STATE_IN_LOGIN_SCREEN;
    case crosapi::mojom::SessionState::kInSession:
      return api::login_state::SessionState::SESSION_STATE_IN_SESSION;
    case crosapi::mojom::SessionState::kInLockScreen:
      return api::login_state::SessionState::SESSION_STATE_IN_LOCK_SCREEN;
    case crosapi::mojom::SessionState::kInRmaScreen:
      return api::login_state::SessionState::SESSION_STATE_IN_RMA_SCREEN;
  }
  NOTREACHED();
  return api::login_state::SessionState::SESSION_STATE_UNKNOWN;
}

crosapi::mojom::LoginState* GetLoginStateApi() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  return chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::LoginState>()
      .get();
#else
  return crosapi::CrosapiManager::Get()->crosapi_ash()->login_state_ash();
#endif
}

ExtensionFunction::ResponseAction LoginStateGetProfileTypeFunction::Run() {
  bool is_signin_profile =
      IsSigninProfile(Profile::FromBrowserContext(browser_context()));
  api::login_state::ProfileType profile_type =
      is_signin_profile
          ? api::login_state::ProfileType::PROFILE_TYPE_SIGNIN_PROFILE
          : api::login_state::ProfileType::PROFILE_TYPE_USER_PROFILE;
  return RespondNow(WithArguments(api::login_state::ToString(profile_type)));
}

ExtensionFunction::ResponseAction LoginStateGetSessionStateFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  absl::optional<std::string> error = ValidateCrosapi();
  if (error.has_value()) {
    return RespondNow(Error(error.value()));
  }
#endif

  auto callback =
      base::BindOnce(&LoginStateGetSessionStateFunction::OnResult, this);

  GetLoginStateApi()->GetSessionState(std::move(callback));
  return did_respond() ? AlreadyResponded() : RespondLater();
}

void LoginStateGetSessionStateFunction::OnResult(
    crosapi::mojom::GetSessionStateResultPtr result) {
  using Result = crosapi::mojom::GetSessionStateResult;
  switch (result->which()) {
    case Result::Tag::kErrorMessage:
      Respond(Error(result->get_error_message()));
      return;
    case Result::Tag::kSessionState:
      api::login_state::SessionState session_state =
          ToApiEnum(result->get_session_state());
      Respond(WithArguments(api::login_state::ToString(session_state)));
      return;
  }
}

}  // namespace extensions
