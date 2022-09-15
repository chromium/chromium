// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/android/language_bridge.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/language/android/jni_headers/LanguageBridge_jni.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/language_prefs.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStrings;

namespace {

PrefService* GetPrefService() {
  return ProfileManager::GetActiveUserProfile()
      ->GetOriginalProfile()
      ->GetPrefs();
}

}  // namespace

namespace language {
std::vector<std::string> LanguageBridge::GetULPLanguagesFromDevice(
    std::string account_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> account_name_java =
      ConvertUTF8ToJavaString(env, account_name);
  ScopedJavaLocalRef<jobjectArray> languages_java =
      Java_LanguageBridge_getULPLanguagesFromDevice(env, account_name_java);

  const int num_langs = (*env).GetArrayLength(languages_java.obj());
  std::vector<std::string> languages;
  for (int i = 0; i < num_langs; i++) {
    jstring language_name_java =
        (jstring)(*env).GetObjectArrayElement(languages_java.obj(), i);
    languages.emplace_back(ConvertJavaStringToUTF8(env, language_name_java));
  }

  return languages;
}
}  // namespace language

// Gets the ULP languages from the Android only preference.
static ScopedJavaLocalRef<jobjectArray> JNI_LanguageBridge_GetULPFromPreference(
    JNIEnv* env) {
  return ToJavaArrayOfStrings(
      env, language::LanguagePrefs(GetPrefService()).GetULPLanguages());
}
