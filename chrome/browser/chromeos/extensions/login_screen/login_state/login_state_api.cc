// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"

#include <memory>

#include "base/values.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

// Not all session states are exposed to the extension. Session states which
// are not exposed will be mapped to the nearest logical state. The mapping is
// as follows:
// UNKNOWN              -> UNKNOWN
// OOBE                 -> IN_OOBE_SCREEN
// LOGIN_PRIMARY        -> IN_LOGIN_SCREEN
// LOGIN_SECONDARY      -> IN_LOGIN_SCREEN
// LOGGED_IN_NOT_ACTIVE -> IN_LOGIN_SCREEN
// ACTIVE               -> IN_SESSION
// LOCKED               -> IN_LOCK_SCREEN
api::login_state::SessionState SessionStateToApiEnum(
    session_manager::SessionState state) {
  switch (state) {
    case session_manager::SessionState::UNKNOWN:
      return api::login_state::SessionState::SESSION_STATE_UNKNOWN;
    case session_manager::SessionState::OOBE:
      return api::login_state::SessionState::SESSION_STATE_IN_OOBE_SCREEN;
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      return api::login_state::SessionState::SESSION_STATE_IN_LOGIN_SCREEN;
    case session_manager::SessionState::ACTIVE:
      return api::login_state::SessionState::SESSION_STATE_IN_SESSION;
    case session_manager::SessionState::LOCKED:
      return api::login_state::SessionState::SESSION_STATE_IN_LOCK_SCREEN;
  }
  NOTREACHED();
  return api::login_state::SessionState::SESSION_STATE_UNKNOWN;
}

ExtensionFunction::ResponseAction LoginStateGetProfileTypeFunction::Run() {
  bool is_signin_profile = chromeos::ProfileHelper::IsSigninProfile(
      Profile::FromBrowserContext(browser_context()));
  api::login_state::ProfileType profile_type =
      is_signin_profile
          ? api::login_state::ProfileType::PROFILE_TYPE_SIGNIN_PROFILE
          : api::login_state::ProfileType::PROFILE_TYPE_USER_PROFILE;
  return RespondNow(ArgumentList(
      api::login_state::GetProfileType::Results::Create(profile_type)));
}

ExtensionFunction::ResponseAction LoginStateGetSessionStateFunction::Run() {
  session_manager::SessionState state =
      session_manager::SessionManager::Get()->session_state();
  return RespondNow(
      ArgumentList(api::login_state::GetSessionState::Results::Create(
          SessionStateToApiEnum(state))));
}

}  // namespace extensions
