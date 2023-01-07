// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils_chromeos.h"

#include "components/password_manager/core/browser/password_access_authenticator.h"

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

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

constexpr base::TimeDelta kShowPasswordAuthTokenLifetime =
    password_manager::PasswordAccessAuthenticator::kAuthValidityPeriod;
constexpr base::TimeDelta kExportPasswordsAuthTokenLifetime = base::Seconds(5);

}  // namespace

namespace extensions {

base::TimeDelta GetAuthTokenLifetimeForPurpose(
    password_manager::ReauthPurpose purpose) {
  return (purpose == password_manager::ReauthPurpose::EXPORT)
             ? kExportPasswordsAuthTokenLifetime
             : kShowPasswordAuthTokenLifetime;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsOsReauthAllowedAsh(Profile* profile,
                          base::TimeDelta auth_token_lifetime) {
  const bool user_cannot_manually_enter_password =
      !ash::password_visibility::AccountHasUserFacingPassword(
          g_browser_process->local_state(),
          ash::ProfileHelper::Get()->GetUserByProfile(profile)->GetAccountId());
  if (user_cannot_manually_enter_password)
    return true;

  ash::quick_unlock::QuickUnlockStorage* quick_unlock_storage =
      ash::quick_unlock::QuickUnlockFactory::GetForProfile(profile);
  const ash::quick_unlock::AuthToken* auth_token =
      quick_unlock_storage->GetAuthToken();
  if (!auth_token || !auth_token->GetAge().has_value())
    return false;

  return auth_token->GetAge() <= auth_token_lifetime;
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
void IsOsReauthAllowedLacrosAsync(
    password_manager::ReauthPurpose purpose,
    password_manager::PasswordAccessAuthenticator::AuthResultCallback
        callback) {
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service->IsAvailable<crosapi::mojom::Authentication>()) {
    // Use crosapi to call IsOsReauthAllowedAsh() in Ash, injecting auth token
    // lifetime from Lacros (instead using the values defined in Ash) for more
    // flexibility. Pass |callback| directly since it's compatible.
    lacros_service->GetRemote<crosapi::mojom::Authentication>()
        ->IsOsReauthAllowedForActiveUserProfile(
            GetAuthTokenLifetimeForPurpose(purpose), std::move(callback));
  } else {
    // No crosapi: Fallback to pre-crosapi behavior.
    std::move(callback).Run(true);
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

}  // namespace extensions
