// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/android_locale.h"

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {

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

}  // namespace base::i18n
