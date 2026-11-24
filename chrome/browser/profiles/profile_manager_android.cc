// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_manager_android.h"

#include <string>
#include <utility>

#include "base/android/callback_android.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_avatar_icon_util.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/profiles/android/jni_headers/ProfileManager_jni.h"

using jni_zero::ScopedJavaLocalRef;

ProfileManagerAndroid::ProfileManagerAndroid(ProfileManager* manager) {
  profile_manager_observation_.Observe(manager);
}

ProfileManagerAndroid::~ProfileManagerAndroid() = default;

void ProfileManagerAndroid::OnProfileAdded(Profile* profile) {
  // profiles::SetLastUsedProfile() can run before a new profile (e.g. from
  // --profile-directory) is in ProfileAttributesStorage, writing the initial
  // profile to SharedPreferences instead. Re-sync here after
  // AddProfileToStorage() has run if this profile matches kProfileLastUsed.
  if (profiles::IsAndroidMultiprofileStartupDataEnabled() &&
      g_browser_process->local_state()->GetFilePath(prefs::kProfileLastUsed) ==
          profile->GetBaseName()) {
    profiles::WriteLastUsedProfileBaseName(
        ProfileManager::GetLastUsedProfileBaseName());
  }
  Java_ProfileManager_onProfileAdded(jni_zero::AttachCurrentThread(),
                                     profile->GetJavaObject());
}

void ProfileManagerAndroid::OnProfileMarkedForPermanentDeletion(
    Profile* profile) {}

// static
static ScopedJavaLocalRef<jobject> JNI_ProfileManager_GetLastUsedRegularProfile(
    JNIEnv* env) {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  if (!profile) {
    NOTREACHED() << "Profile not found.";
  }
  return profile->GetJavaObject();
}

// static
static std::vector<Profile*> JNI_ProfileManager_GetLoadedProfiles(JNIEnv* env) {
  return g_browser_process->profile_manager()->GetLoadedProfiles();
}

// static
static void JNI_ProfileManager_CreateMultiProfileAsync(
    JNIEnv* env,
    const std::u16string& name,
    base::OnceCallback<void(Profile*)> initialized_callback) {
  ProfileManager::CreateMultiProfileAsync(
      name, profiles::GetPlaceholderAvatarIndex(), /* is_hidden= */ false,
      std::move(initialized_callback));
}

// static
static void JNI_ProfileManager_OnProfileActivated(JNIEnv* env,
                                                  Profile* profile) {
  if (!profile) {
    return;
  }
  g_browser_process->profile_manager()->SetProfileAsLastUsed(profile);
}

// static
static void JNI_ProfileManager_DestroyWhenAppropriate(JNIEnv* env,
                                                      Profile* profile) {
  CHECK(profile) << "Attempting to destroy a null profile.";
  CHECK(profile->IsOffTheRecord())
      << "Only OTR profiles can be destroyed from Java as regular profiles are "
         "owned by the C++ ProfileManager.";
  // Don't delete the Profile directly because the corresponding
  // RenderViewHost might not be deleted yet.
  ProfileDestroyer::DestroyOTRProfileWhenAppropriate(profile);
}

DEFINE_JNI(ProfileManager)
