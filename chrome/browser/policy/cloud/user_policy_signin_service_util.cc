// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/policy/cloud/user_policy_signin_service_util.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace policy {

void UpdateProfileAttributesWhenSignout(Profile* profile,
                                        ProfileManager* profile_manager) {
  if (!profile_manager) {
    // Skip the update there isn't a profile manager which can happen when
    // testing.
    return;
  }

  ProfileAttributesEntry* entry =
      profile_manager->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  if (entry)
    entry->SetUserAcceptedAccountManagement(false);
}

}  // namespace policy
