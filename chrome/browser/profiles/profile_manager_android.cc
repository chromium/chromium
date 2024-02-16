// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager_android.h"

#include "chrome/browser/profiles/android/jni_headers/ProfileManager_jni.h"
#include "chrome/browser/profiles/profile_android.h"
#include "chrome/browser/profiles/profile_destroyer.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

ProfileManagerAndroid::ProfileManagerAndroid(ProfileManager* manager) {
  profile_manager_observation_.Observe(manager);
}

ProfileManagerAndroid::~ProfileManagerAndroid() = default;

void ProfileManagerAndroid::OnProfileAdded(Profile* profile) {
  Java_ProfileManager_onProfileAdded(
      jni_zero::AttachCurrentThread(),
      ProfileAndroid::FromProfile(profile)->GetJavaObject());
}

void ProfileManagerAndroid::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {}

// static
ScopedJavaLocalRef<jobject> JNI_ProfileManager_GetLastUsedRegularProfile(
    JNIEnv* env) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile) {
    NOTREACHED() << "Profile not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  ProfileAndroid* profile_android = ProfileAndroid::FromProfile(profile);
  if (!profile_android) {
    NOTREACHED() << "ProfileAndroid not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  return profile_android->GetJavaObject();
}

// static
void JNI_ProfileManager_DestroyWhenAppropriate(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  Profile* profile = ProfileAndroid::FromProfileAndroid(obj);
  CHECK(profile) << "Attempting to destroy a null profile.";
  CHECK(profile->IsOffTheRecord())
      << "Only OTR profiles can be destroyed from Java as regular profiles are "
         "owned by the C++ ProfileManager.";
  // Don't delete the Profile directly because the corresponding
  // RenderViewHost might not be deleted yet.
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile);
}
