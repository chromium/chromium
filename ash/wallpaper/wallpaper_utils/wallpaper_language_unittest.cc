// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_language.h"

#include "base/logging.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace ash {

namespace {

void SetIcuLocale(const char* locale_name) {
  UErrorCode error_code = U_ZERO_ERROR;
  icu::Locale locale(locale_name);
  icu::Locale::setDefault(locale, error_code);
}

}  // namespace

TEST(WallpaperLanguageTest, AustraliaLocale) {
  SetIcuLocale("en_AU");

  EXPECT_EQ("en-AU", ash::GetLanguageTag());
}

TEST(WallpaperLanguageTest, ChineseLocale) {
  SetIcuLocale("zh_CN");

  EXPECT_EQ("zh-CN", ash::GetLanguageTag());
}

TEST(WallpaperLanguageTest, IllFormedLanguage) {
  SetIcuLocale("123-123");

  // Language is undefined.
  EXPECT_EQ("und-123", ash::GetLanguageTag());
}

TEST(WallpaperLanguageTest, FakeLocale) {
  SetIcuLocale("en-Zzzz");

  EXPECT_EQ("en-Zzzz", ash::GetLanguageTag());
}

TEST(WallpaperLanguageTest, OnlyLanguage) {
  SetIcuLocale("en");

  EXPECT_EQ("en", ash::GetLanguageTag());
}

TEST(WallpaperLanguageTest, NoLocale) {
  SetIcuLocale("");

  // Language is undefined.
  EXPECT_EQ("und", ash::GetLanguageTag());
}

TEST(WallpaperLanguageTest, FakeLanguage) {
  SetIcuLocale("zr");

  EXPECT_EQ("zr", ash::GetLanguageTag());
}

TEST(WallpaperLanguageTest, FakeLanguageFakeLocale) {
  SetIcuLocale("zr-ZZ");

  EXPECT_EQ("zr-ZZ", ash::GetLanguageTag());
}

}  // namespace ash
