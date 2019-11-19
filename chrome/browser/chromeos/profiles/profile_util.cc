// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/profiles/profile_util.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/login/login_state/login_state.h"
#include "components/user_manager/user.h"

namespace chromeos {

bool IsProfileAssociatedWithGaiaAccount(Profile* profile) {
  // TODO(crbug.com/942937): This code can likely be simplified.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableGaiaServices)) {
    return false;
  }
  if (!chromeos::LoginState::IsInitialized())
    return false;
  if (!chromeos::LoginState::Get()->IsUserGaiaAuthenticated())
    return false;
  if (profile->IsOffTheRecord())
    return false;

  // Using ProfileHelper::GetSigninProfile() here would lead to an infinite loop
  // when this method is called during the creation of the sign-in profile
  // itself. Using ProfileHelper::GetSigninProfileDir() is safe because it does
  // not try to access the sign-in profile.
  if (profile->GetPath() == ProfileHelper::GetSigninProfileDir())
    return false;
  if (profile->GetPath() == ProfileHelper::GetLockScreenAppProfilePath())
    return false;
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  if (user && user->GetType() == user_manager::USER_TYPE_ACTIVE_DIRECTORY) {
    return false;
  }

  return true;
}

}  // namespace chromeos
