// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEY_ANDROID_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEY_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"

class ProfileKey;

// Android wrapper around ProfileKey that provides safe passage from java and
// native.
class ProfileKeyAndroid {
 public:
  explicit ProfileKeyAndroid(ProfileKey* key);
  ~ProfileKeyAndroid();

  static ProfileKey* FromProfileKeyAndroid(
      const base::android::JavaRef<jobject>& obj);

  static base::android::ScopedJavaLocalRef<jobject> GetLastUsedProfileKey(
      JNIEnv* env);

  // Return the original profile key.
  base::android::ScopedJavaLocalRef<jobject> GetOriginalKey(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Whether this profile is off the record.
  jboolean IsOffTheRecord(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  ProfileKey* key_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEY_ANDROID_H_
