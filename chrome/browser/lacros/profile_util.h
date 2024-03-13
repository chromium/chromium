// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_PROFILE_UTIL_H_
#define CHROME_BROWSER_LACROS_PROFILE_UTIL_H_

#include <cstdint>

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"

// Computes a stable profile identifier based on the profile path. `CityHash64`
// is defined as unchanging.
uint64_t HashProfilePathToProfileId(const base::FilePath& profile_path);

// Returns a `ProfileAttributesEntry` with the data for the profile that matches
// `profile_id`. Returns `nullptr` otherwise. Returned value should not be
// cached because the profile entry may be deleted at any time, then using
// this value would cause use-after-free.
ProfileAttributesEntry* GetProfileAttributesWithProfileId(uint64_t profile_id);

// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfile();

#endif  // CHROME_BROWSER_LACROS_PROFILE_UTIL_H_
