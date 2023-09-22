// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/preferences/android/shared_preferences_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "chrome/browser/preferences/jni_headers/SharedPreferencesManager_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;

namespace android::shared_preferences {

const SharedPreferencesManager GetChromeSharedPreferences() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jshared_prefs_manager =
      Java_SharedPreferencesManager_getInstance(env);
  return SharedPreferencesManager(jshared_prefs_manager, env);
}

SharedPreferencesManager::SharedPreferencesManager(const JavaRef<jobject>& jobj,
                                                   JNIEnv* env)
    : java_obj_(jobj), env_(env) {}

SharedPreferencesManager::SharedPreferencesManager(
    const SharedPreferencesManager& other)
    : java_obj_(other.java_obj_), env_(other.env_) {}

SharedPreferencesManager::~SharedPreferencesManager() {}

void SharedPreferencesManager::RemoveKey(
    const std::string& shared_preference_key) {
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env_, shared_preference_key);
  Java_SharedPreferencesManager_removeKey(env_, java_obj_, jkey);
}

bool SharedPreferencesManager::ContainsKey(
    const std::string& shared_preference_key) {
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env_, shared_preference_key);
  return Java_SharedPreferencesManager_contains(env_, java_obj_, jkey);
}

bool SharedPreferencesManager::ReadBoolean(
    const std::string& shared_preference_key,
    bool default_value) {
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env_, shared_preference_key);
  return Java_SharedPreferencesManager_readBoolean(env_, java_obj_, jkey,
                                                   default_value);
}

int SharedPreferencesManager::ReadInt(const std::string& shared_preference_key,
                                      int default_value) {
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env_, shared_preference_key);
  return Java_SharedPreferencesManager_readInt(env_, java_obj_, jkey,
                                               default_value);
}

std::string SharedPreferencesManager::ReadString(
    const std::string& shared_preference_key,
    const std::string& default_value) {
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env_, shared_preference_key);
  ScopedJavaLocalRef<jstring> jdefault_value =
      ConvertUTF8ToJavaString(env_, default_value);
  ScopedJavaLocalRef<jstring> java_result =
      Java_SharedPreferencesManager_readString(env_, java_obj_, jkey,
                                               jdefault_value);
  return ConvertJavaStringToUTF8(java_result);
}

void SharedPreferencesManager::WriteString(
    const std::string& shared_preference_key,
    const std::string& value) {
  ScopedJavaLocalRef<jstring> jkey =
      ConvertUTF8ToJavaString(env_, shared_preference_key);
  ScopedJavaLocalRef<jstring> jvalue = ConvertUTF8ToJavaString(env_, value);
  Java_SharedPreferencesManager_writeString(env_, java_obj_, jkey, jvalue);
}

}  // namespace android::shared_preferences
