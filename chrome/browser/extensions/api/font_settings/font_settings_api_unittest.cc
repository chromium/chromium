// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/font_settings/font_settings_api.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

TEST(FontSettingsApiTest, FontNameValidation) {
  EXPECT_TRUE(IsValidFontName(""));
  EXPECT_TRUE(IsValidFontName("Arial"));
  EXPECT_TRUE(IsValidFontName("Times New Roman"));
  EXPECT_TRUE(IsValidFontName("Courier New"));
  EXPECT_TRUE(IsValidFontName("sans-serif"));
  EXPECT_TRUE(IsValidFontName("Osaka-Mono"));
  EXPECT_TRUE(IsValidFontName("M+ 1p"));
  EXPECT_TRUE(IsValidFontName("custom_standard"));
  EXPECT_TRUE(IsValidFontName("宋体"));

  EXPECT_FALSE(IsValidFontName("x'; position: fixed;"));
  EXPECT_FALSE(IsValidFontName("font;"));
  EXPECT_FALSE(IsValidFontName("font\""));
  EXPECT_FALSE(IsValidFontName("font'"));
  EXPECT_FALSE(IsValidFontName("font\\"));
  EXPECT_FALSE(IsValidFontName("font{color:red}"));
  EXPECT_FALSE(IsValidFontName("font:10px"));
  EXPECT_FALSE(IsValidFontName("font/*comment*/"));
  EXPECT_FALSE(IsValidFontName("font\n"));
  EXPECT_FALSE(IsValidFontName("font, serif"));
  EXPECT_FALSE(IsValidFontName(std::string(300, 'a')));
}

}  // namespace extensions
