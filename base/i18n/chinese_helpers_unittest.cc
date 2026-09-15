// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/chinese_helpers.h"

#include <string_view>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {
namespace {

LanguageTag ParseTag(std::string_view str) {
  std::optional<LanguageTag> tag = GetLanguageTagFromString(str);
  CHECK(tag.has_value()) << "Failed to parse tag: " << str;
  return tag.value_or(GetKnownLanguageTag("und"));
}

TEST(ChineseHelpersTest, IsChinese) {
  // Exact match for 'zh' language subtag
  EXPECT_TRUE(IsChinese(GetKnownLanguageTag("zh")));
  EXPECT_TRUE(IsChinese(GetKnownLanguageTag("zh-Hans")));
  EXPECT_TRUE(IsChinese(GetKnownLanguageTag("zh-Hant")));
  EXPECT_TRUE(IsChinese(GetKnownLanguageTag("zh-CN")));
  EXPECT_TRUE(IsChinese(GetKnownLanguageTag("zh-TW")));
  EXPECT_TRUE(IsChinese(GetKnownLanguageTag("zh-HK")));
  EXPECT_TRUE(IsChinese(ParseTag("zh-SG")));
  EXPECT_TRUE(IsChinese(ParseTag("zh-SG")));
  EXPECT_TRUE(IsChinese(ParseTag("zh-MO")));
  EXPECT_TRUE(IsChinese(ParseTag("zh-Hans-CN")));
  EXPECT_TRUE(IsChinese(ParseTag("zh-Hant-TW")));
  EXPECT_TRUE(IsChinese(ParseTag("cmn-SG")));
  EXPECT_TRUE(IsChinese(ParseTag("cmn-SG")));
  EXPECT_TRUE(IsChinese(ParseTag("cmn-MO")));
  EXPECT_TRUE(IsChinese(ParseTag("cmn-Hans-CN")));
  EXPECT_TRUE(IsChinese(ParseTag("cmn-Hant-TW")));

  // Non-Chinese languages
  EXPECT_FALSE(IsChinese(GetKnownLanguageTag("en")));
  EXPECT_FALSE(IsChinese(GetKnownLanguageTag("en-US")));
  EXPECT_FALSE(IsChinese(GetKnownLanguageTag("ja-JP")));
  EXPECT_FALSE(IsChinese(GetKnownLanguageTag("de")));
  EXPECT_FALSE(IsChinese(GetKnownLanguageTag("fr-FR")));
}

TEST(ChineseHelpersTest, IsSimplifiedChinese) {
  // Simplified Chinese tags
  EXPECT_TRUE(IsSimplifiedChinese(GetKnownLanguageTag("zh-Hans")));
  EXPECT_TRUE(
      IsSimplifiedChinese(GetKnownLanguageTag("zh-CN")));  // Defaults to Hans
  EXPECT_TRUE(IsSimplifiedChinese(ParseTag("zh-SG")));     // Defaults to Hans
  EXPECT_TRUE(IsSimplifiedChinese(ParseTag("zh-Hans-CN")));
  EXPECT_TRUE(IsSimplifiedChinese(ParseTag("cmn-Hans")));

  // Traditional Chinese tags
  EXPECT_FALSE(IsSimplifiedChinese(GetKnownLanguageTag("zh-Hant")));
  EXPECT_FALSE(IsSimplifiedChinese(GetKnownLanguageTag("zh-TW")));
  EXPECT_FALSE(IsSimplifiedChinese(GetKnownLanguageTag("zh-HK")));
  EXPECT_FALSE(IsSimplifiedChinese(ParseTag("zh-MO")));
  EXPECT_FALSE(IsSimplifiedChinese(ParseTag("zh-Hant-TW")));
  EXPECT_FALSE(IsSimplifiedChinese(ParseTag("cmn-MO")));
  EXPECT_FALSE(IsSimplifiedChinese(ParseTag("cmn-Hant-TW")));

  // Non-Chinese language tags
  EXPECT_FALSE(IsSimplifiedChinese(GetKnownLanguageTag("en-US")));
  EXPECT_FALSE(IsSimplifiedChinese(GetKnownLanguageTag("ja-JP")));
}

TEST(ChineseHelpersTest, IsTraditionalChinese) {
  // Traditional Chinese tags
  EXPECT_TRUE(IsTraditionalChinese(GetKnownLanguageTag("zh-Hant")));
  EXPECT_TRUE(
      IsTraditionalChinese(GetKnownLanguageTag("zh-TW")));  // Defaults to Hant
  EXPECT_TRUE(
      IsTraditionalChinese(GetKnownLanguageTag("zh-HK")));  // Defaults to Hant
  EXPECT_TRUE(IsTraditionalChinese(ParseTag("zh-MO")));     // Defaults to Hant
  EXPECT_TRUE(IsTraditionalChinese(ParseTag("cmn-MO")));    // Defaults to Hant
  EXPECT_TRUE(IsTraditionalChinese(ParseTag("zh-Hant-TW")));
  EXPECT_TRUE(IsTraditionalChinese(ParseTag("cmn-Hant-TW")));

  // Simplified Chinese tags
  EXPECT_FALSE(IsTraditionalChinese(GetKnownLanguageTag("zh-Hans")));
  EXPECT_FALSE(IsTraditionalChinese(GetKnownLanguageTag("zh-CN")));
  EXPECT_FALSE(IsTraditionalChinese(ParseTag("zh-SG")));
  EXPECT_FALSE(IsTraditionalChinese(ParseTag("zh-Hans-CN")));
  EXPECT_FALSE(IsTraditionalChinese(ParseTag("cmn-SG")));
  EXPECT_FALSE(IsTraditionalChinese(ParseTag("cmn-Hans-CN")));

  // Non-Chinese language tags
  EXPECT_FALSE(IsTraditionalChinese(GetKnownLanguageTag("en-US")));
  EXPECT_FALSE(IsTraditionalChinese(GetKnownLanguageTag("ja-JP")));
}

}  // namespace
}  // namespace base::i18n
