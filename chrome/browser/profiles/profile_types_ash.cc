// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_types_ash.h"

#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"

bool IsUserProfile(const Profile* profile) {
  return ash::IsUserBrowserContext(const_cast<Profile*>(profile));
}

bool IsUserProfilePath(const base::FilePath& profile_path) {
  return ash::IsUserBrowserContextBaseName(profile_path);
}

bool IsLockScreenProfile(const Profile* profile) {
  return ash::IsLockScreenBrowserContext(const_cast<Profile*>(profile));
}

bool IsLockScreenAppProfile(const Profile* profile) {
  return ash::IsLockScreenAppBrowserContext(const_cast<Profile*>(profile));
}

bool IsSigninProfile(const Profile* profile) {
  return ash::IsSigninBrowserContext(const_cast<Profile*>(profile));
}
