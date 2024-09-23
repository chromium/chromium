// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"

#include "base/functional/bind.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/login_state.h"
#include "chromeos/crosapi/mojom/login_state.mojom.h"
#include "content/public/browser/browser_context.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include <optional>

#include "chromeos/lacros/lacros_service.h"
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
std::optional<std::string> ValidateCrosapi() {
  if (!chromeos::LacrosService::Get()
           ->IsAvailable<crosapi::mojom::LoginState>()) {
    return kUnsupportedByAsh;
  }
  return std::nullopt;
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace

namespace extensions {

api::login_state::SessionState ToApiEnum(crosapi::mojom::SessionState state) {
  switch (state) {
    case crosapi::mojom::SessionState::kUnknown:
      return api::login_state::SessionState::kUnknown;
    case crosapi::mojom::SessionState::kInOobeScreen:
      return api::login_state::SessionState::kInOobeScreen;
    case crosapi::mojom::SessionState::kInLoginScreen:
      return api::login_state::SessionState::kInLoginScreen;
    case crosapi::mojom::SessionState::kInSession:
      return api::login_state::SessionState::kInSession;
    case crosapi::mojom::SessionState::kInLockScreen:
      return api::login_state::SessionState::kInLockScreen;
    case crosapi::mojom::SessionState::kInRmaScreen:
      return api::login_state::SessionState::kInRmaScreen;
  }
  NOTREACHED_IN_MIGRATION();
  return api::login_state::SessionState::kUnknown;
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
      is_signin_profile ? api::login_state::ProfileType::kSigninProfile
                        : api::login_state::ProfileType::kUserProfile;
  return RespondNow(WithArguments(api::login_state::ToString(profile_type)));
}

ExtensionFunction::ResponseAction LoginStateGetSessionStateFunction::Run() {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  std::optional<std::string> error = ValidateCrosapi();
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
