// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cached_metrics_profile.h"

#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace metrics {
namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsLoggedIn() {
  return user_manager::UserManager::IsInitialized() &&
         user_manager::UserManager::Get()->IsUserLoggedIn();
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

}  // namespace

CachedMetricsProfile::CachedMetricsProfile() = default;

CachedMetricsProfile::~CachedMetricsProfile() = default;

Profile* CachedMetricsProfile::GetMetricsProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  // If there is a cached profile, reuse that.  However, check that it is still
  // valid first. This logic is valid for all platforms, including ChromeOS Ash.
  if (cached_profile_ && profile_manager->IsValidProfile(cached_profile_))
    return cached_profile_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Select the primary user profile for ChromeOS.
  if (!IsLoggedIn())
    return nullptr;
  const user_manager::User* primary_user =
      user_manager::UserManager::Get()->GetPrimaryUser();
  if (!primary_user || !primary_user->is_profile_created())
    return nullptr;
  cached_profile_ = ash::ProfileHelper::Get()->GetProfileByUser(primary_user);
#else
  // Find a suitable profile to use, and cache it so that we continue to report
  // statistics on the same profile.
  cached_profile_ = profile_manager->GetLastUsedProfileIfLoaded();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  if (cached_profile_) {
    // Ensure that the returned profile is not an incognito profile.
    cached_profile_ = cached_profile_->GetOriginalProfile();
  }
  return cached_profile_;
}

}  // namespace metrics
