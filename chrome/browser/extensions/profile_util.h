// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_PROFILE_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_PROFILE_UTIL_H_

#include <stddef.h>

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

class Profile;
class ProfileManager;

namespace extensions::profile_util {

bool ProfileCanUseNonComponentExtensions(const Profile* profile);

Profile* GetLastUsedProfile();

size_t GetNumberOfProfiles();

ProfileManager* GetProfileManager();

#if BUILDFLAG(IS_CHROMEOS)
Profile* GetPrimaryUserProfile();

Profile* GetActiveUserProfile();
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions::profile_util

#endif  // CHROME_BROWSER_EXTENSIONS_PROFILE_UTIL_H_
