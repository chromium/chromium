// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager_android.h"

#include "chrome/android/public/profiles/jni_headers/ProfileManager_jni.h"
#include "chrome/browser/profiles/profile_android.h"

ProfileManagerAndroid::ProfileManagerAndroid() = default;

ProfileManagerAndroid::~ProfileManagerAndroid() = default;

void ProfileManagerAndroid::OnProfileAdded(Profile* profile) {
  Java_ProfileManager_onProfileAdded(
      base::android::AttachCurrentThread(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
}

void ProfileManagerAndroid::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {}
