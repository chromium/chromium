// Copyright 2022 The Chromium Authors. All rights reserved.
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

namespace language {
std::vector<std::string> LanguageBridge::GetULPLanguages(
    std::string account_name) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jstring> account_name_java =
      base::android::ConvertUTF8ToJavaString(env, account_name);
  base::android::ScopedJavaLocalRef<jobjectArray> languages_java =
      Java_LanguageBridge_getULPLanguages(env, account_name_java);

  const int num_langs = (*env).GetArrayLength(languages_java.obj());
  std::vector<std::string> languages;
  for (int i = 0; i < num_langs; i++) {
    jstring language_name_java =
        (jstring)(*env).GetObjectArrayElement(languages_java.obj(), i);
    languages.emplace_back(
        base::android::ConvertJavaStringToUTF8(env, language_name_java));
  }

  return languages;
}
}  // namespace language

static void JNI_LanguageBridge_GetULPModelLanguages(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& set) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(profile)
          ->GetLanguageModel(language::LanguageModelManager::ModelType::ULP);
  if (!language_model)
    return;

  std::vector<language::LanguageModel::LanguageDetails> languageDetails =
      language_model->GetLanguages();

  std::vector<std::string> languages;
  for (const auto& details : languageDetails) {
    languages.push_back(details.lang_code);
  }
  Java_LanguageBridge_copyStringArrayToCollection(
      env, set, base::android::ToJavaArrayOfStrings(env, languages));
}
