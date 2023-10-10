// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils_chromeos.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/quick_unlock/auth_token.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_factory.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_storage.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/login/auth/password_visibility_utils.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace extensions {

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsOsReauthAllowedAsh(Profile* profile,
                          base::TimeDelta auth_token_lifetime) {
  const bool user_cannot_manually_enter_password =
      !ash::password_visibility::AccountHasUserFacingPassword(
          g_browser_process->local_state(),
          ash::ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId());
  if (user_cannot_manually_enter_password)
    return true;
  // TODO (b/238606050): This code branch does not seem to be used now.
  //  Clean up the code, or add token as a parameter to this method.
  ash::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      ash::quick_unlock::QuickUnlockFactory::GetForProfile(profile);
  const ash::quick_unlock::AuthToken* auth_token =
      quick_unlock_storage->GetAuthToken();
  if (!auth_token || !auth_token->GetAge().has_value())
    return false;

  return auth_token->GetAge() <= auth_token_lifetime;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace extensions
