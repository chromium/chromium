// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_INIT_PARAMS_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_INIT_PARAMS_H_

#include <string>

#include "base/files/file_path.h"
#include "components/account_id/account_id.h"

// Move-only struct holding the parameters of a profile being added into
// ProfileAttributesStorage.
struct ProfileAttributesInitParams {
  ProfileAttributesInitParams();
  ~ProfileAttributesInitParams();

  // Move operations are allowed.
  ProfileAttributesInitParams(ProfileAttributesInitParams&&);
  ProfileAttributesInitParams& operator=(ProfileAttributesInitParams&&);

  // Copy operations are forbidden.
  ProfileAttributesInitParams(const ProfileAttributesInitParams&) = delete;
  ProfileAttributesInitParams& operator=(const ProfileAttributesInitParams&) =
      delete;

  // Full path to the profile.
  base::FilePath profile_path;
  // Local profile name displayed to the user.
  std::u16string profile_name;
  // GAIA id of the user signed into the profile. Empty string if the
  // user is not signed in.
  std::string gaia_id;
  // Email address of the user signed into the profile. Empty string if the user
  // is not signed in.
  std::u16string user_name;
  // Whether or not the profile has a primary account with the Sync consent.
  bool is_consented_primary_account = false;
  // Index of the default avatar icon used by the profile.
  size_t icon_index = 0;
  // ID of the supervisor user for this profile. Empty string if the profile is
  // not supervised.
  std::string supervised_user_id;
  // `AccountId` of the user of the profile. Empty if the profile doesn't have
  // any associated `user_manager::User`.
  AccountId account_id{EmptyAccountId()};
  // Whether the profile is ephemeral. Ephemeral profiles are cleaned up on
  // every Chrome restart.
  bool is_ephemeral = false;
  // Whether the profile is omitted from all UIs displaying multi-profile
  // choice.
  bool is_omitted = false;
  // Whether the profile was signed in with information from a credential
  // provider.
  bool is_signed_in_with_credential_provider = false;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ATTRIBUTES_INIT_PARAMS_H_
