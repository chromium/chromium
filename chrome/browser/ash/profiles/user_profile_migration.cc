// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/profiles/user_profile_migration.h"

#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/annotated_account_id.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "components/user_manager/user_type.h"

namespace ash {

base::expected<void, NonUserRepresentativeProfileReason>
IsUserRepresentativeProfileForMigration(Profile* profile) {
  if (!profile) {
    return base::unexpected(NonUserRepresentativeProfileReason::kNullProfile);
  }
  // Profiles dedicated for the special use in ChromeOS.
  if (ash::IsSigninBrowserContext(profile)) {
    return base::unexpected(
        NonUserRepresentativeProfileReason::kAshSignInProfile);
  }
  if (ash::IsLockScreenBrowserContext(profile)) {
    return base::unexpected(
        NonUserRepresentativeProfileReason::kAshLockScreenProfile);
  }
  if (ash::IsShimlessRmaAppBrowserContext(profile)) {
    return base::unexpected(
        NonUserRepresentativeProfileReason::kAshShimlessRmaAppProfile);
  }

  // Here, the profile must be somehow tied to a user.
  CHECK(ash::IsUserBrowserContext(profile));

  // The `account_id` must be annotated to the original profile.
  const auto* account_id =
      ash::AnnotatedAccountId::Get(profile->GetOriginalProfile());
  if (!account_id) {
    return base::unexpected(
        NonUserRepresentativeProfileReason::kMissingAccountId);
  }

  // And the User must exist.
  const auto* user = user_manager::UserManager::Get()->FindUser(*account_id);
  if (!user) {
    return base::unexpected(NonUserRepresentativeProfileReason::kMissingUser);
  }

  // If the user type is Guest, it's representative Profile should be
  // the incognito profile, i.e., the primary off the record profile.
  if (user->GetType() == user_manager::UserType::kGuest) {
    CHECK(profile->IsGuestSession());
    if (!profile->IsPrimaryOTRProfile()) {
      return base::unexpected(
          NonUserRepresentativeProfileReason::kNonPrimaryOTRGuestProfile);
    }
    return base::ok();
  }

  // Otherwise, the profile must not be off the record.
  CHECK(!profile->IsGuestSession());
  if (profile->IsOffTheRecord()) {
    return base::unexpected(
        NonUserRepresentativeProfileReason::kOTRRegularProfile);
  }
  return base::ok();
}

Profile* GetUserRepresentativeProfileForMigration(Profile* profile) {
  auto validity = IsUserRepresentativeProfileForMigration(profile);
  if (validity) {
    return profile;
  }

  switch (validity.error()) {
    case NonUserRepresentativeProfileReason::kNullProfile:
    case NonUserRepresentativeProfileReason::kAshSignInProfile:
    case NonUserRepresentativeProfileReason::kAshLockScreenProfile:
    case NonUserRepresentativeProfileReason::kAshShimlessRmaAppProfile:
    case NonUserRepresentativeProfileReason::kMissingAccountId:
    case NonUserRepresentativeProfileReason::kMissingUser:
      // Cannot be fixed up.
      return nullptr;
    case NonUserRepresentativeProfileReason::kNonPrimaryOTRGuestProfile:
      // Returns primary off the record profile for the guest user.
      // It should not need to create, as this should be already created
      // at the very beginning of the profile creation.
      // Though, we set create_if_needed=true for backward compatibility.
      // We may want to revisit here to set it false later.
      return profile->GetPrimaryOTRProfile(/*create_if_needed=*/true);
    case NonUserRepresentativeProfileReason::kOTRRegularProfile:
      // In case the profile is off the record for regular user,
      // return the original one.
      return profile->GetOriginalProfile();
  }
}

}  // namespace ash
