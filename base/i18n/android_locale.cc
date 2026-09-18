// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/android_locale.h"

#include <optional>
#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "base/base_jni/LocaleUtils_jni.h"

namespace base::i18n {

std::string GetAndroidDefaultCountryCode() {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_LocaleUtils_getDefaultCountryCode(env);
}

LanguageTag GetAndroidDefaultLocale() {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string locale_str = Java_LocaleUtils_getDefaultLocaleString(env);
  std::optional<LanguageTag> tag = GetLanguageTagFromString(locale_str);
  CHECK(tag);
  return *tag;
}

}  // namespace base::i18n

DEFINE_JNI(LocaleUtils)
