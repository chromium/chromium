// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager_android.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_destroyer.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/profiles/android/jni_headers/ProfileManager_jni.h"

using jni_zero::ScopedJavaLocalRef;

ProfileManagerAndroid::ProfileManagerAndroid(ProfileManager* manager) {
  profile_manager_observation_.Observe(manager);
}

ProfileManagerAndroid::~ProfileManagerAndroid() = default;

void ProfileManagerAndroid::OnProfileAdded(Profile* profile) {
  Java_ProfileManager_onProfileAdded(jni_zero::AttachCurrentThread(),
                                     profile->GetJavaObject());
}

void ProfileManagerAndroid::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {}

// static
ScopedJavaLocalRef<jobject> JNI_ProfileManager_GetLastUsedRegularProfile(
    JNIEnv* env) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile) {
    NOTREACHED_IN_MIGRATION() << "Profile not found.";
    return nullptr;
  }
  return profile->GetJavaObject();
}

// static
std::vector<Profile*> JNI_ProfileManager_GetLoadedProfiles(JNIEnv* env) {
  return g_browser_process->profile_manager()->GetLoadedProfiles();
}

// static
void JNI_ProfileManager_OnProfileActivated(JNIEnv* env, Profile* profile) {
  if (!profile) {
    return;
  }
  g_browser_process->profile_manager()->SetProfileAsLastUsed(profile);
}

// static
void JNI_ProfileManager_DestroyWhenAppropriate(JNIEnv* env, Profile* profile) {
  CHECK(profile) << "Attempting to destroy a null profile.";
  CHECK(profile->IsOffTheRecord())
      << "Only OTR profiles can be destroyed from Java as regular profiles are "
         "owned by the C++ ProfileManager.";
  // Don't delete the Profile directly because the corresponding
  // RenderViewHost might not be deleted yet.
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile);
}
