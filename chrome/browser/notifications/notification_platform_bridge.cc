// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/notification_platform_bridge.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"

// static
std::string NotificationPlatformBridge::GetProfileId(Profile* profile) {
  if (!profile)
    return std::string();
  const base::FilePath basename = profile->GetBaseName();
  const std::string profile_id = basename.AsUTF8Unsafe();
  // The conversion must be reversible.
  DCHECK_EQ(basename, GetProfileBaseNameFromProfileId(profile_id));
  return profile_id;
}

// static
base::FilePath NotificationPlatformBridge::GetProfileBaseNameFromProfileId(
    const std::string& profile_id) {
  return base::FilePath::FromUTF8Unsafe(profile_id);
}
