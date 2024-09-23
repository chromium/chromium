// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile_key_android.h"

#include "base/android/jni_android.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/profiles/android/jni_headers/ProfileKey_jni.h"

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

ScopedJavaLocalRef<jobject> ProfileKeyAndroid::GetOriginalKey(JNIEnv* env) {
  ProfileKeyAndroid* original_key =
      key_->GetOriginalKey()->GetProfileKeyAndroid();
  DCHECK(original_key);
  return original_key->GetJavaObject();
}

jboolean ProfileKeyAndroid::IsOffTheRecord(JNIEnv* env) {
  return key_->IsOffTheRecord();
}

jlong ProfileKeyAndroid::GetSimpleFactoryKeyPointer(JNIEnv* env) {
  return reinterpret_cast<jlong>(static_cast<SimpleFactoryKey*>(key_));
}

ScopedJavaLocalRef<jobject> ProfileKeyAndroid::GetJavaObject() {
  return ScopedJavaLocalRef<jobject>(obj_);
}
