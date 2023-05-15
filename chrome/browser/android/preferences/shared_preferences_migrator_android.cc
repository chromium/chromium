// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/preferences/shared_preferences_migrator_android.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "chrome/browser/preferences/jni_headers/SharedPreferencesManager_jni.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;

namespace android::shared_preferences {

void ClearKey(const std::string& shared_preference_key) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jshared_prefs_manager =
      Java_SharedPreferencesManager_getInstance(env);

  DCHECK(!jshared_prefs_manager.is_null());

  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env, shared_preference_key);
  Java_SharedPreferencesManager_removeKey(env, jshared_prefs_manager, jkey);
}

absl::optional<bool> GetAndClearBoolean(
    const std::string& shared_preference_key) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jshared_prefs_manager =
      Java_SharedPreferencesManager_getInstance(env);

  DCHECK(!jshared_prefs_manager.is_null());

  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env, shared_preference_key);
  if (!Java_SharedPreferencesManager_contains(env, jshared_prefs_manager,
                                              jkey)) {
    return absl::nullopt;
  }

  bool result = Java_SharedPreferencesManager_readBoolean(
      env, jshared_prefs_manager, jkey, /*defaultValue=*/false);
  Java_SharedPreferencesManager_removeKey(env, jshared_prefs_manager, jkey);
  return result;
}

absl::optional<int> GetAndClearInt(const std::string& shared_preference_key) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jshared_prefs_manager =
      Java_SharedPreferencesManager_getInstance(env);
  DCHECK(!jshared_prefs_manager.is_null());
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env, shared_preference_key);
  if (!Java_SharedPreferencesManager_contains(env, jshared_prefs_manager,
                                              jkey)) {
    return absl::nullopt;
  }
  int result = Java_SharedPreferencesManager_readInt(env, jshared_prefs_manager,
                                                     jkey, /*defaultValue=*/0);
  Java_SharedPreferencesManager_removeKey(env, jshared_prefs_manager, jkey);
  return result;
}

std::string GetString(const std::string& shared_preference_key,
                      const std::string& default_value) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jshared_prefs_manager =
      Java_SharedPreferencesManager_getInstance(env);

  DCHECK(!jshared_prefs_manager.is_null());
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env, shared_preference_key);
  ScopedJavaLocalRef<jstring> jdefault =
      ConvertUTF8ToJavaString(env, default_value);
  return ConvertJavaStringToUTF8(
      env, Java_SharedPreferencesManager_readString(env, jshared_prefs_manager,
                                                    jkey, jdefault));
}

void SetString(const std::string& shared_preference_key,
               const std::string& value) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jshared_prefs_manager =
      Java_SharedPreferencesManager_getInstance(env);

  DCHECK(!jshared_prefs_manager.is_null());
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env, shared_preference_key);
  ScopedJavaLocalRef<jstring> jvalue = ConvertUTF8ToJavaString(env, value);
  Java_SharedPreferencesManager_writeString(env, jshared_prefs_manager, jkey,
                                            jvalue);
}

}  // namespace android::shared_preferences
