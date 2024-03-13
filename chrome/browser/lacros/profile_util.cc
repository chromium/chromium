// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/profile_util.h"

#include "base/containers/span.h"
#include "base/hash/legacy_hash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

uint64_t HashProfilePathToProfileId(const base::FilePath& profile_path) {
  // The result is defined as taking the 63 upper bits from CityHash64 and
  // setting the lowest bit to 1. This gives us two properties:
  //  1. We can use the value 0 as a natural invalid/unset indicator.
  //  2. Any other even number is reserved for future use.
  return 1 | base::legacy::CityHash64(
                 base::as_bytes(base::make_span(profile_path.value())));
}

ProfileAttributesEntry* GetProfileAttributesWithProfileId(uint64_t profile_id) {
  std::vector<ProfileAttributesEntry*> entries =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetAllProfilesAttributesSortedByNameWithCheck();

  for (auto* entry : entries) {
    if (HashProfilePathToProfileId(entry->GetPath()) == profile_id) {
      return entry;
    }
  }

  return nullptr;
}

// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfile() {
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(profiles, &Profile::IsMainProfile);
  if (main_it == profiles.end()) {
    return nullptr;
  }
  return *main_it;
}
