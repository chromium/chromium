// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_TEST_UTIL_H_
#define CHROME_BROWSER_PROFILES_PROFILE_TEST_UTIL_H_

#include "base/files/file_path.h"
#include "build/build_config.h"

class Profile;
class ProfileManager;

namespace profiles::testing {

// Helper to call `ProfileManager::CreateProfileAsync` synchronously during
// tests. Returns the created `Profile`.
Profile* CreateProfileSync(ProfileManager* profile_manager,
                           const base::FilePath& path);

#if !BUILDFLAG(IS_ANDROID)
// Helper to call `::profiles::SwitchToProfile()` synchronously during tests.
void SwitchToProfileSync(const base::FilePath& path, bool always_create = true);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace profiles::testing
#endif  // CHROME_BROWSER_PROFILES_PROFILE_TEST_UTIL_H_
