// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/privacy_sandbox/profile_bucket_metrics.h"

#include <string>

#include "base/strings/strcat.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"

constexpr int kMaxProfileBucket = 10;

namespace privacy_sandbox {

namespace {

std::optional<int> GetProfileBucket(Profile* profile) {
  // Guest and incognito profiles should not be included in any of the profile
  // level related histograms.
  if (!profile->IsRegularProfile()) {
    return std::nullopt;
  }

  // Can be null in unit tests.
  if (!g_browser_process->profile_manager()) {
    return std::nullopt;
  }

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());

  // This can happen if the profile is deleted.
  if (!entry) {
    return std::nullopt;
  }
  return entry->GetMetricsBucketIndex();
}

}  // namespace

std::string GetProfileBucketName(Profile* profile) {
  std::optional<int> profile_num = GetProfileBucket(profile);
  if (!profile_num.has_value()) {
    return "";
  }

  if (profile_num.value() > kMaxProfileBucket) {
    return "Profile_11+";
  }
  return base::StrCat({"Profile_", base::ToString(profile_num.value())});
}

std::optional<ProfileEnabledState> GetProfileEnabledState(Profile* profile,
                                                          bool enabled) {
  std::optional<int> profile_num = GetProfileBucket(profile);
  if (!profile_num) {
    return std::nullopt;
  }
  switch (profile_num.value()) {
    case 1:
      return enabled ? ProfileEnabledState::kPSProfileOneEnabled
                     : ProfileEnabledState::kPSProfileOneDisabled;
    case 2:
      return enabled ? ProfileEnabledState::kPSProfileTwoEnabled
                     : ProfileEnabledState::kPSProfileTwoDisabled;
    case 3:
      return enabled ? ProfileEnabledState::kPSProfileThreeEnabled
                     : ProfileEnabledState::kPSProfileThreeDisabled;
    case 4:
      return enabled ? ProfileEnabledState::kPSProfileFourEnabled
                     : ProfileEnabledState::kPSProfileFourDisabled;
    default:
      return enabled ? ProfileEnabledState::kPSProfileFivePlusEnabled
                     : ProfileEnabledState::kPSProfileFivePlusDisabled;
  }
}

}  // namespace privacy_sandbox
