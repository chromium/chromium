// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cached_metrics_profile.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace metrics {

CachedMetricsProfile::CachedMetricsProfile() = default;

CachedMetricsProfile::~CachedMetricsProfile() = default;

Profile* CachedMetricsProfile::GetMetricsProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;

  // If there is a cached profile, reuse that.  However, check that it is still
  // valid first.
  if (cached_profile_ && profile_manager->IsValidProfile(cached_profile_))
    return cached_profile_;

  // Find a suitable profile to use, and cache it so that we continue to report
  // statistics on the same profile.  We would simply use
  // ProfileManager::GetLastUsedProfile(), except that that has the side effect
  // of creating a profile if it does not yet exist.
  cached_profile_ = profile_manager->GetProfileByPath(
      profile_manager->GetLastUsedProfileDir(profile_manager->user_data_dir()));
  if (cached_profile_) {
    // Ensure that the returned profile is not an incognito profile.
    cached_profile_ = cached_profile_->GetOriginalProfile();
  }
  return cached_profile_;
}

}  // namespace metrics
