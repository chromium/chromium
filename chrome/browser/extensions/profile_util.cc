// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/profile_util.h"

#include "base/check_is_test.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
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
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetPrimaryUser();

  // `user` can be a nullptr value in tests.
  if (!user) {
    CHECK_IS_TEST();
    return ProfileManager::GetPrimaryUserProfile();
  }
  return Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
}

Profile* GetActiveUserProfile() {
  const user_manager::User* user =
      user_manager::UserManager::Get()->GetActiveUser();

  // `user` can be a nullptr value in tests.
  if (!user) {
    CHECK_IS_TEST();
    return ProfileManager::GetActiveUserProfile();
  }
  return Profile::FromBrowserContext(
      ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
}

bool IsActiveProfile(Profile* profile) {
  return profile->IsSameOrParent(GetActiveUserProfile());
}
#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions::profile_util
