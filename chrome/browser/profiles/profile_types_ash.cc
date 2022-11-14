// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_types_ash.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"

namespace {

bool IsSigninProfilePath(const base::FilePath& profile_path) {
  return profile_path.value() == chrome::kInitialProfile;
}

bool IsLockScreenAppProfilePath(const base::FilePath& profile_path) {
  return profile_path.value() == chrome::kLockScreenAppProfile;
}

bool IsLockScreenProfilePath(const base::FilePath& profile_path) {
  return profile_path.value() == chrome::kLockScreenProfile;
}

}  // namespace

bool IsUserProfile(const Profile* profile) {
  return !IsSigninProfile(profile) && !IsLockScreenAppProfile(profile) &&
         !IsLockScreenProfile(profile);
}

bool IsUserProfilePath(const base::FilePath& profile_path) {
  return !IsSigninProfilePath(profile_path) &&
         !IsLockScreenAppProfilePath(profile_path) &&
         !IsLockScreenProfilePath(profile_path);
}

bool IsLockScreenProfile(const Profile* profile) {
  return profile && IsLockScreenProfilePath(profile->GetBaseName());
}

bool IsLockScreenAppProfile(const Profile* profile) {
  return profile && IsLockScreenAppProfilePath(profile->GetBaseName());
}

bool IsSigninProfile(const Profile* profile) {
  return profile && IsSigninProfilePath(profile->GetBaseName());
}
