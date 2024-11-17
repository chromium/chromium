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

constexpr int kMaxProfileBucket = 20;

namespace privacy_sandbox {

std::string GetProfileBucketName(Profile* profile) {
  if (!profile->IsRegularProfile()) {
    return "";
  }

  // Can be null in unit tests.
  if (!g_browser_process->profile_manager()) {
    return "";
  }

  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  // This can happen if the profile is deleted.
  if (!entry) {
    return "";
  }
  int profile_num = entry->GetMetricsBucketIndex();
  if (profile_num > kMaxProfileBucket) {
    return "Profile_21+";
  }
  return base::StrCat({"Profile_", base::ToString(profile_num)});
}

}  // namespace privacy_sandbox
