// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/login_screen/login_state/login_state_api.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/extensions/api/login_state.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/browser/browser_context.h"

namespace extensions {

api::login_state::SessionState ToApiEnum(session_manager::SessionState state) {
  switch (state) {
    case session_manager::SessionState::UNKNOWN:
      return api::login_state::SessionState::kUnknown;
    case session_manager::SessionState::OOBE:
      return api::login_state::SessionState::kInOobeScreen;
    case session_manager::SessionState::LOGIN_PRIMARY:
    case session_manager::SessionState::LOGIN_SECONDARY:
    case session_manager::SessionState::LOGGED_IN_NOT_ACTIVE:
      return api::login_state::SessionState::kInLoginScreen;
    case session_manager::SessionState::ACTIVE:
      return api::login_state::SessionState::kInSession;
    case session_manager::SessionState::LOCKED:
      return api::login_state::SessionState::kInLockScreen;
    case session_manager::SessionState::RMA:
      return api::login_state::SessionState::kInRmaScreen;
  }

  NOTREACHED();
}

ExtensionFunction::ResponseAction LoginStateGetProfileTypeFunction::Run() {
  const Profile* profile = Profile::FromBrowserContext(browser_context());
  api::login_state::ProfileType profile_type;

  if (ash::IsSigninBrowserContext(profile)) {
    profile_type = api::login_state::ProfileType::kSigninProfile;
  } else if (ash::IsLockScreenBrowserContext(profile)) {
    profile_type = api::login_state::ProfileType::kLockProfile;
  } else {
    profile_type = api::login_state::ProfileType::kUserProfile;
  }
  return RespondNow(WithArguments(api::login_state::ToString(profile_type)));
}

ExtensionFunction::ResponseAction LoginStateGetSessionStateFunction::Run() {
  return RespondNow(WithArguments(api::login_state::ToString(
      ToApiEnum(session_manager::SessionManager::Get()->session_state()))));
}

}  // namespace extensions
