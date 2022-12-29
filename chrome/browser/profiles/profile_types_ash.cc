// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_types_ash.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"

bool IsUserProfile(const Profile* profile) {
  return !IsSigninProfile(profile) && !IsLockScreenAppProfile(profile) &&
         !IsLockScreenProfile(profile);
}

bool IsUserProfilePath(const base::FilePath& profile_path) {
  const auto& value = profile_path.value();
  return value != ash::BrowserContextHelper::kSigninBrowserContextBaseName &&
         value !=
             ash::BrowserContextHelper::kLockScreenAppBrowserContextBaseName &&
         value != ash::BrowserContextHelper::kLockScreenBrowserContextBaseName;
}

bool IsLockScreenProfile(const Profile* profile) {
  return profile &&
         profile->GetBaseName().value() ==
             ash::BrowserContextHelper::kLockScreenBrowserContextBaseName;
}

bool IsLockScreenAppProfile(const Profile* profile) {
  return profile &&
         profile->GetBaseName().value() ==
             ash::BrowserContextHelper::kLockScreenAppBrowserContextBaseName;
}

bool IsSigninProfile(const Profile* profile) {
  return profile &&
         profile->GetBaseName().value() ==
             ash::BrowserContextHelper::kSigninBrowserContextBaseName;
}
