// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/language/android/language_bridge.h"

#include "base/android/jni_string.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/browser/language_prefs.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "chrome/browser/language/android/jni_headers/LanguageBridge_jni.h"

namespace language {
std::vector<base::i18n::LanguageTag> LanguageBridge::GetULPLanguagesFromDevice(
    std::string account_name) {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  std::vector<std::string> locales =
      Java_LanguageBridge_getULPLanguagesFromDevice(env, account_name);
  std::vector<base::i18n::LanguageTag> tags;
  tags.reserve(locales.size());
  for (const std::string& locale : locales) {
    std::optional<base::i18n::LanguageTag> tag =
        base::i18n::LanguageTagConverter::GetInstance().FromString(locale);
    if (tag) {
      tags.push_back(*tag);
    }
  }
  return tags;
}
}  // namespace language

// Gets the ULP languages from the Android only preference.
static std::vector<std::string> JNI_LanguageBridge_GetULPFromPreference(
    JNIEnv* env,
    Profile* profile) {
  return language::LanguagePrefs(profile->GetPrefs()).GetULPLanguages();
}

DEFINE_JNI(LanguageBridge)
