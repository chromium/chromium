// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ANDROID_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/compiler_specific.h"
#include "base/supports_user_data.h"

class Profile;

// Android wrapper around profile that provides safer passage from java and
// back to native.
class ProfileAndroid : public base::SupportsUserData::Data {
 public:
  static ProfileAndroid* FromProfile(Profile* profile);
  static Profile* FromProfileAndroid(
      const base::android::JavaRef<jobject>& obj);

  static base::android::ScopedJavaLocalRef<jobject> GetLastUsedProfile(
      JNIEnv* env);

  // Destroys this Profile when possible.
  void DestroyWhenAppropriate(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  // Return the original profile.
  base::android::ScopedJavaLocalRef<jobject> GetOriginalProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Return the incognito profile.
  //
  // WARNING: This will create the OffTheRecord profile if it doesn't already
  // exist. If this isn't what you want, you need to check
  // HasOffTheRecordProfile() first.
  base::android::ScopedJavaLocalRef<jobject> GetOffTheRecordProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Return whether an off the record profile exists.
  jboolean HasOffTheRecordProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetProfileKey(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Whether this profile is off the record.
  jboolean IsOffTheRecord(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Whether this profile is signed in to a child account.
  jboolean IsChild(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

  void Wipe(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  explicit ProfileAndroid(Profile* profile);
  ~ProfileAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  Profile* profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> obj_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ANDROID_H_
