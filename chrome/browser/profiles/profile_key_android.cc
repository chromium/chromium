// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_key_android.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "chrome/android/public/profiles/jni_headers/ProfileKey_jni.h"
#include "chrome/browser/android/profile_key_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_manager.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

ProfileKeyAndroid::ProfileKeyAndroid(ProfileKey* key) : key_(key) {
  JNIEnv* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jkey =
      Java_ProfileKey_create(env, reinterpret_cast<intptr_t>(this));
  obj_.Reset(env, jkey.obj());
}

ProfileKeyAndroid::~ProfileKeyAndroid() {
  Java_ProfileKey_onNativeDestroyed(AttachCurrentThread(), obj_);
}

// static
ProfileKey* ProfileKeyAndroid::FromProfileKeyAndroid(
    const JavaRef<jobject>& obj) {
  if (obj.is_null())
    return nullptr;

  ProfileKeyAndroid* profile_key_android = reinterpret_cast<ProfileKeyAndroid*>(
      Java_ProfileKey_getNativePointer(AttachCurrentThread(), obj));
  if (!profile_key_android)
    return nullptr;
  return profile_key_android->key_;
}

// static
ScopedJavaLocalRef<jobject> ProfileKeyAndroid::GetLastUsedProfileKey(
    JNIEnv* env) {
  ProfileKey* key = ::android::GetLastUsedProfileKey();
  if (key == nullptr) {
    NOTREACHED() << "ProfileKey not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  ProfileKeyAndroid* profile_key_android = key->GetProfileKeyAndroid();
  if (profile_key_android == nullptr) {
    NOTREACHED() << "ProfileKeyAndroid not found.";
    return ScopedJavaLocalRef<jobject>();
  }

  return ScopedJavaLocalRef<jobject>(profile_key_android->obj_);
}

ScopedJavaLocalRef<jobject> ProfileKeyAndroid::GetOriginalKey(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  ProfileKeyAndroid* original_key =
      key_->GetOriginalKey()->GetProfileKeyAndroid();
  DCHECK(original_key);
  return original_key->GetJavaObject();
}

jboolean ProfileKeyAndroid::IsOffTheRecord(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  return key_->IsOffTheRecord();
}

// static
ScopedJavaLocalRef<jobject> JNI_ProfileKey_GetLastUsedProfileKey(JNIEnv* env) {
  return ProfileKeyAndroid::GetLastUsedProfileKey(env);
}

ScopedJavaLocalRef<jobject> ProfileKeyAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(obj_);
}
