// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/android_locale.h"

#include <string>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

namespace {

using base::android::ConvertUTF8ToJavaString;
using base::android::MethodID;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;

// Sets java.util.Locale's default to `new Locale(language, country)` for the
// lifetime of this object, restoring the previous default on destruction.
class ScopedJavaDefaultLocale {
 public:
  ScopedJavaDefaultLocale(const std::string& language,
                          const std::string& country) {
    JNIEnv* env = base::android::AttachCurrentThread();
    locale_class_.Reset(base::android::GetClass(env, "java/util/Locale"));

    jmethodID get_default = MethodID::Get<MethodID::TYPE_STATIC>(
        env, locale_class_.obj(), "getDefault", "()Ljava/util/Locale;");
    original_locale_.Reset(jni_zero::AdoptRef(
        env, env->CallStaticObjectMethod(locale_class_.obj(), get_default)));

    jmethodID ctor = MethodID::Get<MethodID::TYPE_INSTANCE>(
        env, locale_class_.obj(), "<init>",
        "(Ljava/lang/String;Ljava/lang/String;)V");
    ScopedJavaLocalRef<jstring> j_language =
        ConvertUTF8ToJavaString(env, language);
    ScopedJavaLocalRef<jstring> j_country =
        ConvertUTF8ToJavaString(env, country);
    ScopedJavaLocalRef<jobject> new_locale = jni_zero::AdoptRef(
        env, env->NewObject(locale_class_.obj(), ctor, j_language.obj(),
                            j_country.obj()));
    SetDefault(env, new_locale.obj());
  }

  ScopedJavaDefaultLocale(const ScopedJavaDefaultLocale&) = delete;
  ScopedJavaDefaultLocale& operator=(const ScopedJavaDefaultLocale&) = delete;

  ~ScopedJavaDefaultLocale() {
    SetDefault(base::android::AttachCurrentThread(), original_locale_.obj());
  }

 private:
  void SetDefault(JNIEnv* env, jobject locale) {
    jmethodID set_default = MethodID::Get<MethodID::TYPE_STATIC>(
        env, locale_class_.obj(), "setDefault", "(Ljava/util/Locale;)V");
    env->CallStaticVoidMethod(locale_class_.obj(), set_default, locale);
  }

  ScopedJavaGlobalRef<jclass> locale_class_;
  ScopedJavaGlobalRef<jobject> original_locale_;
};

}  // namespace

TEST(AndroidLocaleTest, GetAndroidDefaultCountryCode) {
  std::string country_code = GetAndroidDefaultCountryCode();
  // Device country code should be non-empty (e.g. "US").
  EXPECT_FALSE(country_code.empty());
}

TEST(AndroidLocaleTest, GetAndroidDefaultLocale) {
  LanguageTag default_locale = GetAndroidDefaultLocale();
  // Device default locale should be valid and non-empty (e.g., "en-US").
  EXPECT_FALSE(default_locale.tag_string().empty());
}

TEST(AndroidLocaleTest, GetAndroidDefaultLocaleValid) {
  ScopedJavaDefaultLocale scoped_locale("fr", "CA");
  EXPECT_EQ(GetAndroidDefaultLocale().tag_string(), "fr-CA");
}

// Locale.ROOT-like locales map to an empty string.
TEST(AndroidLocaleTest, GetAndroidDefaultLocaleEmpty) {
  ScopedJavaDefaultLocale scoped_locale("", "");
  EXPECT_EQ(GetAndroidDefaultLocale().tag_string(), "en-US");
}

// A locale with a country but no language maps to "-US", which is not a valid
// BCP 47 tag.
TEST(AndroidLocaleTest, GetAndroidDefaultLocaleEmptyLanguage) {
  ScopedJavaDefaultLocale scoped_locale("", "US");
  EXPECT_EQ(GetAndroidDefaultLocale().tag_string(), "en-US");
}

// An ill-formed region should fall back to the language subtag.
TEST(AndroidLocaleTest, GetAndroidDefaultLocaleInvalidRegion) {
  ScopedJavaDefaultLocale scoped_locale("de", "DEU");
  EXPECT_EQ(GetAndroidDefaultLocale().tag_string(), "de");
}

// An ill-formed language should fall back to "en-US".
TEST(AndroidLocaleTest, GetAndroidDefaultLocaleInvalidLanguage) {
  ScopedJavaDefaultLocale scoped_locale("e1", "US");
  EXPECT_EQ(GetAndroidDefaultLocale().tag_string(), "en-US");
}

}  // namespace base::i18n
