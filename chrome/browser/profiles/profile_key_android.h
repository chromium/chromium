// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEY_ANDROID_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEY_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

class ProfileKey;

// Android wrapper around ProfileKey that provides safe passage from java and
// native.
class ProfileKeyAndroid {
 public:
  explicit ProfileKeyAndroid(ProfileKey* key);
  ~ProfileKeyAndroid();

  static ProfileKey* FromProfileKeyAndroid(
      const base::android::JavaRef<jobject>& obj);

  // Return the original profile key.
  base::android::ScopedJavaLocalRef<jobject> GetOriginalKey(JNIEnv* env);

  // Whether this profile is off the record.
  jboolean IsOffTheRecord(JNIEnv* env);

  jlong GetSimpleFactoryKeyPointer(JNIEnv* env);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  raw_ptr<ProfileKey> key_;
  base::android::ScopedJavaGlobalRef<jobject> obj_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_KEY_ANDROID_H_
