// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/profile_key_util.h"

#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/profiles/profile_manager.h"

namespace android {
namespace {

Profile* GetProfile() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);
  return profile;
}

}  // namespace

ProfileKey* GetLastUsedProfileKey() {
  ProfileKey* key = ProfileKeyStartupAccessor::GetInstance()->profile_key();
  if (!key)
    key = GetProfile()->GetProfileKey();
  DCHECK(key);
  return key;
}

}  // namespace android
