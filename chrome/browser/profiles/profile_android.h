// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_ANDROID_H_
#define CHROME_BROWSER_PROFILES_PROFILE_ANDROID_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/supports_user_data.h"
#include "chrome/browser/profiles/profile.h"

class Profile;

// Android wrapper around profile that provides safer passage from java and
// back to native.
class ProfileAndroid : public base::SupportsUserData::Data {
 public:
  static ProfileAndroid* FromProfile(Profile* profile);
  static Profile* FromProfileAndroid(
      const base::android::JavaRef<jobject>& obj);

  static base::android::ScopedJavaLocalRef<jobject> GetLastUsedRegularProfile(
      JNIEnv* env);

  // Destroys this Profile when possible.
  void DestroyWhenAppropriate(JNIEnv* env,
                              const base::android::JavaParamRef<jobject>& obj);

  // Return the original profile.
  base::android::ScopedJavaLocalRef<jobject> GetOriginalProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Return the OffTheRecord profile.
  //
  base::android::ScopedJavaLocalRef<jobject> GetOffTheRecordProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_otr_profile_id,
      const jboolean j_create_if_needed);

  // Return primary OffTheRecord profile.
  base::android::ScopedJavaLocalRef<jobject> GetPrimaryOTRProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const jboolean j_create_if_needed);

  // Return whether an OffTheRecord profile with given OTRProfileID exists.
  jboolean HasOffTheRecordProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj,
      const base::android::JavaParamRef<jobject>& j_otr_profile_id);

  // Returns if the primary OffTheRecord profile exists.
  jboolean HasPrimaryOTRProfile(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetOTRProfileID(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  base::android::ScopedJavaLocalRef<jobject> GetProfileKey(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& obj);

  // Whether this profile is off the record.
  jboolean IsOffTheRecord(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& obj);

  // Whether this profile is primary off the record profile.
  jboolean IsPrimaryOTRProfile(JNIEnv* env,
                               const base::android::JavaParamRef<jobject>& obj);

  // Whether this profile is signed in to a child account.
  jboolean IsChild(JNIEnv* env,
                   const base::android::JavaParamRef<jobject>& obj);

  void Wipe(JNIEnv* env, const base::android::JavaParamRef<jobject>& obj);

  jlong GetBrowserContextPointer(JNIEnv* env);

  explicit ProfileAndroid(Profile* profile);
  ~ProfileAndroid() override;

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  raw_ptr<Profile> profile_;  // weak
  base::android::ScopedJavaGlobalRef<jobject> obj_;
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_ANDROID_H_
