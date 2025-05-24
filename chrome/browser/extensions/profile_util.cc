// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/profile_util.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace extensions::profile_util {

bool ProfileCanUseNonComponentExtensions(const Profile* profile) {
#if BUILDFLAG(IS_CHROMEOS)
  if (!profile || !ash::ProfileHelper::IsUserProfile(profile)) {
    return false;
  }

  const user_manager::User* user =
      ash::ProfileHelper::Get()->GetUserByProfile(profile);
  if (!user) {
    return false;
  }

  // ChromeOS has special irregular profiles that must also be filtered
  // out in addition to `ProfileHelper::IsUserProfile()`. `IsUserProfile()`
  // includes guest and public users (which cannot use non-component
  // extensions) so instead only look for those user types that can use them.
  switch (user->GetType()) {
    case user_manager::UserType::kRegular:
    case user_manager::UserType::kChild:
      return true;

    case user_manager::UserType::kGuest:
    case user_manager::UserType::kPublicAccount:
    case user_manager::UserType::kKioskApp:
    case user_manager::UserType::kWebKioskApp:
    case user_manager::UserType::kKioskIWA:
      return false;
  }
#else
  if (!profile) {
    return false;
  }
  return profile->IsRegularProfile();
#endif  // BUILDFLAG(IS_CHROMEOS)
}

Profile* GetLastUsedProfile() {
  return ProfileManager::GetLastUsedProfile();
}

size_t GetNumberOfProfiles() {
  ProfileManager* const manager = GetProfileManager();
  return !manager ? 0 : manager->GetNumberOfProfiles();
}

ProfileManager* GetProfileManager() {
  return g_browser_process->profile_manager();
}

#if BUILDFLAG(IS_CHROMEOS)
Profile* GetPrimaryUserProfile() {
  return ProfileManager::GetPrimaryUserProfile();
}

Profile* GetActiveUserProfile() {
  return ProfileManager::GetActiveUserProfile();
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions::profile_util
