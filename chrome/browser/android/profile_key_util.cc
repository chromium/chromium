// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/profile_key_util.h"

#include "base/android/scoped_java_ref.h"
#include "chrome/browser/android/profile_key_startup_accessor.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/profiles/android/jni_headers/ProfileKeyUtil_jni.h"

using base::android::ScopedJavaLocalRef;

namespace android {
namespace {

Profile* GetProfile() {
  Profile* profile = ProfileManager::GetLastUsedProfile();
  DCHECK(profile);
  return profile;
}

}  // namespace

ProfileKey* GetLastUsedRegularProfileKey() {
  ProfileKey* key = ProfileKeyStartupAccessor::GetInstance()->profile_key();
  if (!key)
    key = GetProfile()->GetProfileKey();
  DCHECK(key && !key->IsOffTheRecord());
  return key;
}

}  // namespace android

// static
ScopedJavaLocalRef<jobject> JNI_ProfileKeyUtil_GetLastUsedRegularProfileKey(
    JNIEnv* env) {
  ProfileKey* key = ::android::GetLastUsedRegularProfileKey();
  if (!key) {
    NOTREACHED_IN_MIGRATION() << "ProfileKey not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  ProfileKeyAndroid* profile_key_android = key->GetProfileKeyAndroid();
  if (!profile_key_android) {
    NOTREACHED_IN_MIGRATION() << "ProfileKeyAndroid not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  return profile_key_android->GetJavaObject();
}
