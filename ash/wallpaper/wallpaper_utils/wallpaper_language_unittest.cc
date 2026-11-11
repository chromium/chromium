// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_utils/wallpaper_language.h"

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/test/scoped_icu_locale.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using ::base::i18n::GetKnownLanguageTag;
using ::base::i18n::GetLanguageTagFromString;
using ::base::i18n::ScopedDefaultIcuLocale;

}  // namespace

TEST(WallpaperLanguageTest, AustraliaLocale) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("en-AU"));

  EXPECT_EQ("en-AU", ash::GetLanguageTag().tag_string());
}

TEST(WallpaperLanguageTest, ChineseLocale) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("zh-CN"));

  EXPECT_EQ("zh-CN", ash::GetLanguageTag().tag_string());
}

TEST(WallpaperLanguageTest, FakeLocale) {
  ScopedDefaultIcuLocale scoped_locale(
      GetLanguageTagFromString("en-Zzzz").value());

  EXPECT_EQ("en-Zzzz", ash::GetLanguageTag().tag_string());
}

TEST(WallpaperLanguageTest, OnlyLanguage) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("en"));

  EXPECT_EQ("en", ash::GetLanguageTag().tag_string());
}

TEST(WallpaperLanguageTest, NoLocale) {
  ScopedDefaultIcuLocale scoped_locale(GetKnownLanguageTag("und"));

  // Language is undefined.
  EXPECT_EQ("und", ash::GetLanguageTag().tag_string());
}

TEST(WallpaperLanguageTest, FakeLanguage) {
  ScopedDefaultIcuLocale scoped_locale(GetLanguageTagFromString("zr").value());

  EXPECT_EQ("zr", ash::GetLanguageTag().tag_string());
}

TEST(WallpaperLanguageTest, FakeLanguageFakeLocale) {
  ScopedDefaultIcuLocale scoped_locale(
      GetLanguageTagFromString("zr-ZZ").value());

  EXPECT_EQ("zr-ZZ", ash::GetLanguageTag().tag_string());
}

}  // namespace ash
