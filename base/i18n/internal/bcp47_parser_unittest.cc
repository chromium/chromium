// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/internal/bcp47_parser.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/test/gmock_expected_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n_internal {

TEST(Bcp47ParserTest, Constexprness) {
  // Constexpr checks for individual subtag types.
  static_assert(!IsLanguageSubtag(""));
  static_assert(IsLanguageSubtag("en"));
  static_assert(IsLanguageSubtag("eng"));
  static_assert(!IsLanguageSubtag("e"));
  static_assert(!IsLanguageSubtag("123"));

  static_assert(!IsScriptSubtag(""));
  static_assert(IsScriptSubtag("Latn"));
  static_assert(!IsScriptSubtag("Lat"));
  static_assert(!IsScriptSubtag("Latin"));

  static_assert(!IsRegionSubtag(""));
  static_assert(IsRegionSubtag("US"));
  static_assert(IsRegionSubtag("001"));
  static_assert(!IsRegionSubtag("USA"));

  static_assert(!IsVariantSubtag(""));
  static_assert(IsVariantSubtag("nedis"));
  static_assert(IsVariantSubtag("1901"));
  static_assert(!IsVariantSubtag("abc"));

  // Constexpr check for full tag parsing from a string.
  static_assert([] {
    std::optional<ParsedBcp47Tag> parsed = ParseBcp47Tag("en-Latn-US");
    return parsed.has_value() && parsed->language == "en" &&
           parsed->script == "Latn" && parsed->region == "US" &&
           parsed->variants.empty();
  }());

  // Constexpr check for AreSubtagsKnown.
  static_assert([] {
    std::optional<ParsedBcp47Tag> parsed = ParseBcp47Tag("en-Latn-US");
    return parsed.has_value() && AreSubtagsKnown(*parsed);
  }());

  static_assert([] {
    std::optional<ParsedBcp47Tag> parsed = ParseBcp47Tag("xx-Latn-US");
    // ParseBcp47Tag only checks if language subtag is well-formed, not if it is
    // known. "xx" is 2 alpha, so it is a valid language subtag.
    return parsed.has_value() && !AreSubtagsKnown(*parsed);
  }());
}

TEST(Bcp47ParserTest, IsLanguageSubtag) {
  EXPECT_FALSE(IsLanguageSubtag(""));
  EXPECT_TRUE(IsLanguageSubtag("en"));
  EXPECT_TRUE(IsLanguageSubtag("eng"));
  EXPECT_FALSE(IsLanguageSubtag("e"));
  EXPECT_FALSE(IsLanguageSubtag("123"));
}

TEST(Bcp47ParserTest, IsScriptSubtag) {
  EXPECT_FALSE(IsScriptSubtag(""));
  EXPECT_TRUE(IsScriptSubtag("Latn"));
  EXPECT_TRUE(IsScriptSubtag("Cyrl"));
  EXPECT_FALSE(IsScriptSubtag("Lat"));
  EXPECT_FALSE(IsScriptSubtag("Latin"));
}

TEST(Bcp47ParserTest, IsRegionSubtag) {
  EXPECT_FALSE(IsRegionSubtag(""));
  EXPECT_TRUE(IsRegionSubtag("US"));
  EXPECT_TRUE(IsRegionSubtag("001"));
  EXPECT_FALSE(IsRegionSubtag("U"));
  EXPECT_FALSE(IsRegionSubtag("USA"));
}

TEST(Bcp47ParserTest, IsVariantSubtag) {
  EXPECT_FALSE(IsVariantSubtag(""));
  EXPECT_TRUE(IsVariantSubtag("nedis"));
  EXPECT_TRUE(IsVariantSubtag("1901"));
  EXPECT_TRUE(IsVariantSubtag("12345678"));
  EXPECT_FALSE(IsVariantSubtag("abc"));
}

TEST(Bcp47ParserTest, ParseBcp47TagFromSpan) {
  ASSERT_OK_AND_ASSIGN(
      ParsedBcp47Tag parsed,
      ParseBcp47Tag(base::span<const std::string_view>({"en", "US"})));
  EXPECT_EQ(parsed.language, "en");
  EXPECT_EQ(parsed.region, "US");
  EXPECT_TRUE(parsed.script.empty());
  EXPECT_TRUE(parsed.variants.empty());
}

TEST(Bcp47ParserTest, ParseBcp47TagFromSpanEmpty) {
  const std::optional<ParsedBcp47Tag> parsed =
      ParseBcp47Tag(base::span<const std::string_view>({}));
  EXPECT_FALSE(parsed.has_value());
}

TEST(Bcp47ParserTest, ParseBcp47TagFromStringEmpty) {
  const std::optional<ParsedBcp47Tag> parsed = ParseBcp47Tag("");
  EXPECT_FALSE(parsed.has_value());
}

TEST(Bcp47ParserTest, ParseBcp47TagFromString) {
  {
    ASSERT_OK_AND_ASSIGN(ParsedBcp47Tag parsed, ParseBcp47Tag("zh-Hant-TW"));
    EXPECT_EQ(parsed.language, "zh");
    EXPECT_EQ(parsed.script, "Hant");
    EXPECT_EQ(parsed.region, "TW");
    EXPECT_TRUE(parsed.variants.empty());
  }

  {
    ASSERT_OK_AND_ASSIGN(ParsedBcp47Tag parsed,
                         ParseBcp47Tag("en-scouse-nedis"));
    EXPECT_EQ(parsed.language, "en");
    EXPECT_TRUE(parsed.script.empty());
    EXPECT_TRUE(parsed.region.empty());
    ASSERT_EQ(parsed.variants.size(), 2u);
    EXPECT_EQ(parsed.variants[0], "scouse");
    EXPECT_EQ(parsed.variants[1], "nedis");
  }
}

}  // namespace base::i18n_internal
