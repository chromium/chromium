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

using ::testing::ElementsAre;
using ::testing::Pair;

TEST(Bcp47ParserTest, Constexprness) {
  // Constexpr check for full tag parsing from a string.
  static_assert([] {
    std::optional<ParsedBcp47Tag> parsed = ParseBcp47Tag("en-Latn-US");
    return parsed.has_value() && parsed->language == "en" &&
           parsed->script == "Latn" && parsed->region == "US" &&
           parsed->variants.empty();
  }());
  // Constexpr check for tag ending with a "-".
  static_assert([] {
    std::optional<ParsedBcp47Tag> parsed = ParseBcp47Tag("en-US-");
    return !parsed.has_value();
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

  static_assert([] {
    std::optional<ParsedBcp47Tag> parsed = ParseBcp47Tag("en-US-u-ca-gregory");
    // Tags with extensions are not considered "known".
    return parsed.has_value() && !AreSubtagsKnown(*parsed);
  }());
}

TEST(Bcp47ParserTest, ParseBcp47TagFromSpan) {
  ASSERT_OK_AND_ASSIGN(ParsedBcp47Tag parsed, ParseBcp47Tag("en-US"));
  EXPECT_EQ(parsed.language, "en");
  EXPECT_EQ(parsed.region, "US");
  EXPECT_TRUE(parsed.script.empty());
  EXPECT_TRUE(parsed.variants.empty());
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
    ASSERT_OK_AND_ASSIGN(ParsedBcp47Tag parsed, ParseBcp47Tag("Zh-tw"));
    EXPECT_EQ(parsed.language, "Zh");
    EXPECT_EQ(parsed.script, "");
    EXPECT_EQ(parsed.region, "tw");
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

TEST(Bcp47ParserTest, ParseBcp47TagFromStringExtension) {
  ASSERT_OK_AND_ASSIGN(
      ParsedBcp47Tag parsed,
      ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-ca-gregory-co-phonebk"));
  EXPECT_EQ(parsed.language, "zh");
  EXPECT_EQ(parsed.script, "Hant");
  EXPECT_EQ(parsed.region, "TW");
  EXPECT_TRUE(parsed.variants.empty());
  EXPECT_THAT(
      parsed.extensions,
      ElementsAre(Pair('a', ElementsAre("abc", "def")),
                  Pair('u', ElementsAre("ca", "gregory", "co", "phonebk"))));
}

TEST(Bcp47ParserTest, ParseBcp47TagFromStringExtensionAndPrivateUse) {
  ASSERT_OK_AND_ASSIGN(ParsedBcp47Tag parsed,
                       ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-ca-gregory-co-"
                                     "phonebk-x-bla1-bla2-bla3-a-b-c-def"));
  EXPECT_EQ(parsed.language, "zh");
  EXPECT_EQ(parsed.script, "Hant");
  EXPECT_EQ(parsed.region, "TW");
  EXPECT_TRUE(parsed.variants.empty());
  EXPECT_THAT(
      parsed.extensions,
      ElementsAre(Pair('a', ElementsAre("abc", "def")),
                  Pair('u', ElementsAre("ca", "gregory", "co", "phonebk"))));
  EXPECT_THAT(parsed.private_use,
              ElementsAre("bla1", "bla2", "bla3", "a", "b", "c", "def"));
}

TEST(Bcp47ParserTest, ParseBcp47TagFromStringPrivateUseCaseInsensitive) {
  ASSERT_OK_AND_ASSIGN(ParsedBcp47Tag parsed,
                       ParseBcp47Tag("zh-Hant-TW-A-abc-def-U-ca-gregory-co-"
                                     "phonebk-X-bla1-bla2-bla3-a-b-c-def"));
  EXPECT_THAT(
      parsed.extensions,
      ElementsAre(Pair('a', ElementsAre("abc", "def")),
                  Pair('u', ElementsAre("ca", "gregory", "co", "phonebk"))));

  EXPECT_THAT(parsed.private_use,
              ElementsAre("bla1", "bla2", "bla3", "a", "b", "c", "def"));
}

TEST(Bcp47ParserTest, ParseBcp47TagFromStringInvalidExtension) {
  // Too long.
  EXPECT_FALSE(ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-123456789").has_value());
  // Too short.
  EXPECT_FALSE(ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-x").has_value());
  // Space.
  EXPECT_FALSE(ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-1234 -x-abc").has_value());
  // Duplicated singleton case-insensitive.
  EXPECT_FALSE(ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-1234-U-5678").has_value());
}

TEST(Bcp47ParserTest, ParseBcp47TagFromStringInvalidPrivateUse) {
  // Too long.
  EXPECT_FALSE(
      ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-aaa-x-123456789").has_value());
  // Too short.
  EXPECT_FALSE(ParseBcp47Tag("zh-Hant-TW-x-").has_value());
  // Invalid character.
  EXPECT_FALSE(ParseBcp47Tag("zh-Hant-TW-a-abc-def-u-1234-x-$bc").has_value());
}

}  // namespace base::i18n_internal
