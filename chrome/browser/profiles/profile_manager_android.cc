// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager_android.h"

#include "chrome/browser/profiles/android/jni_headers/ProfileManager_jni.h"
#include "chrome/browser/profiles/profile_android.h"

ProfileManagerAndroid::ProfileManagerAndroid(ProfileManager* manager) {
  profile_manager_observation_.Observe(manager);
}

ProfileManagerAndroid::~ProfileManagerAndroid() = default;

void ProfileManagerAndroid::OnProfileAdded(Profile* profile) {
  Java_ProfileManager_onProfileAdded(
      base::android::AttachCurrentThread(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
}

void ProfileManagerAndroid::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {}
