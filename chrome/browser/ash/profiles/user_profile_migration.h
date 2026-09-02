// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PROFILES_USER_PROFILE_MIGRATION_H_
#define CHROME_BROWSER_ASH_PROFILES_USER_PROFILE_MIGRATION_H_

#include "base/types/expected.h"

class Profile;

namespace ash {

enum NonUserRepresentativeProfileReason {
  kNullProfile,
  kAshSignInProfile,
  kAshLockScreenProfile,
  kAshShimlessRmaAppProfile,
  kMissingAccountId,
  kMissingUser,
  kNonPrimaryOTRGuestProfile,
  kOTRRegularProfile,
};

constexpr inline std::string_view NonUserRepresentativeProfileReasonToString(
    NonUserRepresentativeProfileReason reason) {
  switch (reason) {
#define CASE_IMPL(key)                          \
  case NonUserRepresentativeProfileReason::key: \
    return #key
    CASE_IMPL(kNullProfile);
    CASE_IMPL(kAshSignInProfile);
    CASE_IMPL(kAshLockScreenProfile);
    CASE_IMPL(kAshShimlessRmaAppProfile);
    CASE_IMPL(kMissingAccountId);
    CASE_IMPL(kMissingUser);
    CASE_IMPL(kNonPrimaryOTRGuestProfile);
    CASE_IMPL(kOTRRegularProfile);
#undef CASE_IMPL
  }
}

// While it is often considered simple 1:1 mapping between ChromeOS User and
// browser Profile, actually there are edge cases.
// 1) We have Profiles without User
//   - SigninProfile
//   - LockscreenProfile
//   - ShimlessRmaProfile.
// 2) Representative Profile of Guest user is the Incognito Profile
//   i.e. Primary Off the record Profile.
// 3) For non Guest Users, profiles may be the one for off the record,
//   such as Incognito.
// Specifically, using ProfileKeyedService/BrowserContextKeyedService for each
// user session, what Profile is used needs to be carefully handled, but in
// practice, sometimes not due to historical reasons unfortunately.
// This function helps to check such cases. Returns base::ok if the given
// profile is a representative Profile of a user, i.e.
// - If the user is Guest one, the profile should be the primary off the record
//   profile.
// - If the user is not guest, the profile should be the original profile.
// Otherwise, including special profiles listed 1) above, returns
// base::unexpected with an error reason.
base::expected<void, NonUserRepresentativeProfileReason>
IsUserRepresentativeProfileForMigration(Profile* profile);

// Returns:
// 1) If the given profile is a one not tied to a user, e.g. signin profile,
//   returns nullptr.
// 2) If the given profile is tied to a guest user, returns the incognito
//   profile for the original profile of the given profile.
// 3) Otherwise, i.e. if the given profile is tied to non-guest user,
//   returns the original profile of the given profile.
Profile* GetUserRepresentativeProfileForMigration(Profile* profile);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PROFILES_USER_PROFILE_MIGRATION_H_
