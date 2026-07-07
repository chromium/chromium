// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/tags.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n {
namespace {

using ::testing::Eq;
using ::testing::Optional;
using ::testing::Property;

MATCHER_P(OptionalToString, expected, "") {
  return ExplainMatchResult(
      Optional(Property(&LanguageTag::tag_string, Eq(expected))), arg,
      result_listener);
}

MATCHER_P(OptionalRegionToString, expected, "") {
  return ExplainMatchResult(
      Optional(Property(&RegionSubtag::base_type::subtag_string, Eq(expected))),
      arg, result_listener);
}

MATCHER_P(LanguageSubtagString, expected, "") {
  return ExplainMatchResult(
      Property(&LanguageSubtag::base_type::subtag_string, Eq(expected)), arg,
      result_listener);
}

TEST(LanguageTagTest, ParseAndToString) {
  EXPECT_THAT(GetKnownLanguageTag("en-US"), GetKnownLanguageTag("en-US"));

  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("EN-us"),
              OptionalToString("en-US"));
}

TEST(LanguageTagTest, ValueConversions) {
  const LanguageTag en_tag = GetKnownLanguageTag("en-US");

  // LanguageTagToValue
  base::Value value = LanguageTagToValue(en_tag);
  EXPECT_TRUE(value.is_string());
  EXPECT_EQ(value.GetString(), "en-US");

  // ValueToLanguageTag (valid)
  EXPECT_THAT(ValueToLanguageTag(value), OptionalToString("en-US"));
  EXPECT_THAT(ValueToLanguageTag(&value), OptionalToString("en-US"));

  // ValueToLanguageTag (invalid & non-string inputs)
  EXPECT_EQ(ValueToLanguageTag(nullptr), std::nullopt);
  EXPECT_EQ(ValueToLanguageTag(base::Value(42)), std::nullopt);
  EXPECT_EQ(ValueToLanguageTag(base::Value("invalid---tag")), std::nullopt);
}

TEST(LanguageTagTest, InvalidLocales) {
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString(""), std::nullopt);
  EXPECT_EQ(
      LanguageTagConverter::GetInstance().FromString("toolongLanguageTag"),
      std::nullopt);
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("pt-longscript-BR"),
            std::nullopt);
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("pt-BRA-Brazil"),
            std::nullopt);
}

TEST(LanguageTagTest, ValidButUnknowLocales) {
  // Standard BCP 47 accepts these as well-formed even if they are semantically
  // "unknown".
  EXPECT_TRUE(
      LanguageTagConverter::GetInstance().FromString("xx-YY").has_value());
  EXPECT_TRUE(
      LanguageTagConverter::GetInstance().FromString("pt-YY").has_value());
  EXPECT_TRUE(
      LanguageTagConverter::GetInstance().FromString("zh-Yyyy").has_value());
  EXPECT_TRUE(
      LanguageTagConverter::GetInstance().FromString("zh-Hant-XX").has_value());
}

TEST(LanguageTagTest, ToLegacyICUFormat) {
  EXPECT_EQ(GetKnownLanguageTag("pt-BR").ToLegacyICUFormat(), "pt_BR");

  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lt,
        LanguageTagConverter::GetInstance().FromString("en-US-u-cu-usd"));
    EXPECT_EQ(lt.ToLegacyICUFormat(), "en_US@currency=USD");
  }
  {
    ASSERT_OK_AND_ASSIGN(LanguageTag lt,
                         LanguageTagConverter::GetInstance().FromString(
                             "de-DE-u-ca-gregory-co-phonebk"));
    EXPECT_EQ(lt.ToLegacyICUFormat(),
              "de_DE@calendar=gregorian;collation=phonebook");
  }
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lt,
        LanguageTagConverter::GetInstance().FromString("ca-ES-u-va-valencia"));
    EXPECT_EQ(lt.ToLegacyICUFormat(), "ca_ES@valencia");
  }
  {
    ASSERT_OK_AND_ASSIGN(LanguageTag lt,
                         LanguageTagConverter::GetInstance().FromString(
                             "ja-u-lb-normal-lw-phrase"));
    EXPECT_EQ(lt.ToLegacyICUFormat(), "ja@lb=normal;lw=phrase");
  }
}

TEST(LanguageTagTest, ComplexLocales) {
  // Valid complex locales (lang-script-region) within length limit.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("zh-Hant-HK"),
              OptionalToString("zh-Hant-HK"));

  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("sr-Latn-RS"),
              OptionalToString("sr-Latn-RS"));
}

TEST(LanguageTagTest, NumericRegions) {
  // Locales with numeric regions.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("es-419"),
              Optional(GetKnownLanguageTag("es-419")));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("es-419"),
              Optional(GetKnownLanguageTag("es-419")));
}

TEST(LanguageTagTest, ThreeLetterLanguages) {
  // 3-letter language tags.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("fil-PH"),
              OptionalToString("fil-PH"));

  // Asturian (ast).
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ast-ES"),
              OptionalToString("ast-ES"));
}

TEST(LanguageTagIso639_2Test, German) {
  // German: ISO 639-2/T is "deu", ISO 639-2/B is "ger".
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("deu"),
              Optional(GetKnownLanguageTag("de")));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ger"),
              Optional(GetKnownLanguageTag("de")));
}

TEST(LanguageTagIso639_2Test, Spanish) {
  // Spanish: ISO 639-2/T and /B are both "spa".
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("spa"),
              Optional(GetKnownLanguageTag("es")));
}

TEST(LanguageTagIso639_2Test, Portuguese) {
  // Portuguese: ISO 639-2/T and /B are both "por".
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("por"),
              Optional(GetKnownLanguageTag("pt")));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("por-BR"),
              Optional(GetKnownLanguageTag("pt-BR")));
}

TEST(LanguageTagIso639_2Test, French) {
  // French: ISO 639-2/T is "fra", ISO 639-2/B is "fre".
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("fra"),
              Optional(GetKnownLanguageTag("fr")));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("fre"),
              Optional(GetKnownLanguageTag("fr")));
}

TEST(LanguageTagIso639_2Test, Chinese) {
  // Chinese: ISO 639-2/T is "zho", ISO 639-2/B is "chi".
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("zho"),
              Optional(GetKnownLanguageTag("zh")));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("chi"),
              Optional(GetKnownLanguageTag("zh")));
}

TEST(LanguageTagIso639_2SpecialCodesTest, SpecialCodes) {
  // Special ISO 639-2 codes.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("mis"),
              OptionalToString("mis"));  // Uncoded languages
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("mul"),
              OptionalToString("mul"));  // Multiple languages
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("zxx"),
              OptionalToString("zxx"));  // No linguistic content
}

TEST(LanguageTagIso639_2PrivateUseTest, PrivateUseRanges) {
  // Private use codes (qaa-qtz).
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("qaa"),
              OptionalToString("qaa"));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("qtz"),
              OptionalToString("qtz"));
}

TEST(LanguageTagIso639_2Test, OtherCommonLanguages) {
  // English: ISO 639-2/T and /B are both "eng".
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("eng"),
              Optional(GetKnownLanguageTag("en")));

  // Hawaiian (no 2-letter equivalent).
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("haw"),
              Optional(GetKnownLanguageTag("haw")));

  // Asturian (no 2-letter equivalent).
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ast"),
              Optional(GetKnownLanguageTag("ast")));
}

TEST(LanguageTagTest, Variants) {
  // Locales with variants.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en-GB-scuse"),
              OptionalToString("en-GB-scuse"));

  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en-GB-oxendict"),
              OptionalToString("en-GB-oxendict"));

  // German with orthography variant.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("de-1996"),
              OptionalToString("de-1996"));
}

TEST(LanguageTagTest, Extensions) {
  // Locales with extensions.
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory"),
      OptionalToString("en-US-u-ca-gregory"));

  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en-US-u-va-posix"),
      OptionalToString("en-US-u-va-posix"));

  // Extension with multiple keywords.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString(
                  "en-US-u-ca-gregory-co-emoji"),
              OptionalToString("en-US-u-ca-gregory-co-emoji"));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("und-x-private"),
              OptionalToString("und-x-private"));
}

TEST(LanguageTagTest, ExtensionFormatting) {
  // RFC 5646 Section 2.2.6: single character singleton (except 'x').
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en-a-myext"),
              OptionalToString("en-a-myext"));

  // Singletons are case-insensitive.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en-A-myext"),
              OptionalToString("en-a-myext"));

  // Multiple extensions.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en-a-foo-b-bar"),
              OptionalToString("en-a-foo-b-bar"));

  // Extensions with multiple subtags.
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en-a-foo-bar-baz"),
      OptionalToString("en-a-foo-bar-baz"));
}

TEST(LanguageTagTest, ExtensionBadlyFormed) {
  // RFC 5646 Section 2.2.6: The singleton MUST NOT be the last subtag.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("en-a"),
            std::nullopt);

  // Extension subtags MUST be between two and eight characters in length.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("en-a-b"),
            std::nullopt);
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("en-a-123456789"),
            std::nullopt);

  // Singletons MUST be single character.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("en-aa-foo"),
            std::nullopt);

  // Each singleton can only appear once.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("en-a-foo-a-bar"),
            std::nullopt);
}

TEST(LanguageTagTest, MultipleExtensions) {
  ASSERT_OK_AND_ASSIGN(LanguageTag lc,
                       LanguageTagConverter::GetInstance().FromString(
                           "en-US-a-foo-u-ca-gregory-x-private"))
  EXPECT_EQ(lc.tag_string(), "en-US-a-foo-u-ca-gregory-x-private");
  EXPECT_THAT(lc.GetExtension(bcp47_extensions::ext<'a'>()),
              Optional(Property(&Extension::subtags_string, Eq("foo"))));
  EXPECT_THAT(
      lc.GetExtension(bcp47_extensions::unicode()),
      Optional(Property(&UnicodeExtension::ToString, Eq("ca-gregory"))));
  EXPECT_THAT(
      lc.GetExtension(bcp47_extensions::priv()),
      Optional(Property(&PrivateUseSubtags::subtags_string, Eq("private"))));
}

TEST(LanguageTagTest, PrivateUseSubtags) {
  {
    // Private use subtags.
    ASSERT_OK_AND_ASSIGN(LanguageTag lc,
                         LanguageTagConverter::GetInstance().FromString(
                             "und-u-ca-gregory-x-private"))
    EXPECT_EQ(lc.tag_string(), "und-u-ca-gregory-x-private");
    EXPECT_THAT(
        lc.GetExtension(bcp47_extensions::priv()),
        Optional(Property(&PrivateUseSubtags::subtags_string, Eq("private"))));
  }
  {
    // Single-char private use subtags.
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US-x-a"))
    EXPECT_EQ(lc.tag_string(), "en-US-x-a");
    EXPECT_THAT(
        lc.GetExtension(bcp47_extensions::priv()),
        Optional(Property(&PrivateUseSubtags::subtags_string, Eq("a"))));
  }
  {
    // Long private use subtags.
    // Private use subtags also have to conform with |subtag| <= 8.
    EXPECT_EQ(
        LanguageTagConverter::GetInstance().FromString("en-US-x-123456789"),
        std::nullopt);
    // Checks that |subtag| = 8 is fine.
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US-x-12345678"))
    EXPECT_THAT(
        lc.GetExtension(bcp47_extensions::priv()),
        Optional(Property(&PrivateUseSubtags::subtags_string, Eq("12345678"))));
  }
}

TEST(LanguageTagTest, LongBcp47Tags) {
  // A long but valid BCP47 tag that should trigger heap allocation (> 12
  // chars). Azerbaijani in Cyrillic script as spoken in Russia with a variant
  // and extensions. "az-Cyrl-RU-variant-u-ca-gregory-co-phonebk"
  const std::string long_tag = "az-Cyrl-RU-variant-u-ca-gregory-co-phonebk";
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString(long_tag),
              OptionalToString(long_tag));

  // Another long one: "en-US-u-ca-gregory-co-emoji-kb-true-hc-h24"
  // Note: LanguageTagConverter canonicalizes the extensions.
  const std::string very_long_tag =
      "en-US-u-ca-gregory-co-emoji-kb-true-hc-h24";
  const std::string very_long_tag_canonical =
      "en-US-u-ca-gregory-co-emoji-hc-h24-kb";
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString(very_long_tag),
              OptionalToString(very_long_tag_canonical));
}

TEST(LanguageTagTest, PrivateUseTags) {
  // Basic private use tag.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en-x-test"),
              OptionalToString("en-x-test"));

  // Long private use tag.
  const std::string long_x = "en-US-x-this-is-a-very-long-private-use-tag";
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString(long_x),
              OptionalToString(long_x));

  // Private use only not allowed.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("x-private"),
            std::nullopt);
}

TEST(LanguageTagTest, LocaleWithAtSign) {
  // Locales with keywords after '@' should be converted to BCP47 extensions.
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en-US@currency=USD"),
      OptionalToString("en-US-u-cu-usd"));

  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ca_ES@valencia"),
              OptionalToString("ca-ES-u-va-valencia"));

  // Multiple keywords.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString(
                  "de_DE@calendar=gregorian;collation=phonebook"),
              OptionalToString("de-DE-u-ca-gregory-co-phonebk"));

  // Japanese line break and line word extensions.
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ja@lb=normal"),
              OptionalToString("ja-u-lb-normal"));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ja@lb=strict"),
              OptionalToString("ja-u-lb-strict"));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ja@lb=loose"),
              OptionalToString("ja-u-lb-loose"));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("ja@lw=phrase"),
              OptionalToString("ja-u-lw-phrase"));
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("ja@lb=normal;lw=phrase"),
      OptionalToString("ja-u-lb-normal-lw-phrase"));
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("ja@lb=strict;lw=phrase"),
      OptionalToString("ja-u-lb-strict-lw-phrase"));
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("ja@lb=loose;lw=phrase"),
      OptionalToString("ja-u-lb-loose-lw-phrase"));

  // Collation and attributes.
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("de@collation=phonebook"),
      OptionalToString("de-u-co-phonebk"));
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("el@colCaseFirst=upper"),
      OptionalToString("el-u-kf-upper"));
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("el-u-kf-upper"),
              OptionalToString("el-u-kf-upper"));

  // Timezone aliases.
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en-US@timezone=pst8pdt"),
      OptionalToString("en-US-u-tz-uslax"));
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en-US@timezone=est5edt"),
      OptionalToString("en-US-u-tz-usnyc"));
}

TEST(LanguageTagTest, LegacyIcuIgnoresPosixEncoding) {
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en.UTF8@"),
              OptionalToString("en"));
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("ca.UTF8@valencia"),
      OptionalToString("ca-u-va-valencia"));
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("ca_ES.UTF8@valencia"),
      OptionalToString("ca-ES-u-va-valencia"));
}

TEST(LanguageTagTest, LegacyIcuRobustness) {
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en@"),
              OptionalToString("en"));

  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en@ab=cd;;;"),
              OptionalToString("en-u-ab-cd"));

  // Keywords without values.
  // "en@calendar" -> "en-u-calendar" -> ICU4X might return "en".
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("en@calendar"),
              OptionalToString("en-u-ca"));
}

TEST(LanguageTagTest, LegacyIcuRepeatedKeys) {
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en@calendar;calendar"),
      OptionalToString("en-u-ca"));
}

TEST(LanguageTagTest, LegacyIcuBadlyFormed) {
  // Values with special characters.
  // ICU4X canonicalizer will likely fail on these, returning nullopt, which is
  // sane.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString(
                "en@calendar=../../etc/passwd"),
            std::nullopt);
  EXPECT_EQ(
      LanguageTagConverter::GetInstance().FromString("en@calendar=foo bar"),
      std::nullopt);
}

TEST(LanguageTagTest, LegacyIcuLongTags) {
  // Extremely long input.
  std::string long_string(1000, 'a');
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString(long_string + "_US"),
            std::nullopt);
  // Extremely long keyword value.
  std::string long_value(1000, 'b');
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("en@calendar=" +
                                                           long_value),
            std::nullopt);
}

TEST(LanguageTagTest, LegacyIcuWellFormedTags) {
  // Mixed case keys (mapping is case-insensitive for keys).
  EXPECT_THAT(
      LanguageTagConverter::GetInstance().FromString("en@Calendar=gregorian"),
      OptionalToString("en-u-ca-gregory"));

  // Multiple equal signs.
  // "en@calendar=gregorian=extra" -> "en-u-ca-gregorian=extra"
  // ICU4X will likely reject "gregorian=extra" as a value.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString(
                "en@calendar=gregorian=extra"),
            std::nullopt);

  // Just an @ sign.
  // "@" is < 2 chars, so it returns nullopt immediately.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("@"), std::nullopt);

  // Just an _ sign.
  EXPECT_EQ(LanguageTagConverter::GetInstance().FromString("_"), std::nullopt);
}

TEST(LanguageTagTest, CopyAndMove) {
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_original,
                       LanguageTagConverter::GetInstance().FromString("en-US"));

  // Copy constructor
  LanguageTag lt_copy(lt_original);
  EXPECT_EQ(lt_copy.tag_string(), "en-US");
  EXPECT_EQ(lt_copy, lt_original);

  // Copy assignment
  LanguageTag lt_copy_assign = lt_original;
  lt_copy_assign = lt_original;
  EXPECT_EQ(lt_copy_assign.tag_string(), "en-US");
  EXPECT_EQ(lt_copy_assign, lt_original);

  // Move constructor
  LanguageTag lt_move(std::move(lt_copy));
  EXPECT_EQ(lt_move.tag_string(), "en-US");
  EXPECT_EQ(lt_move, lt_original);

  // Move assignment
  LanguageTag lt_move_assign = lt_original;
  lt_move_assign = std::move(lt_move);
  EXPECT_EQ(lt_move_assign.tag_string(), "en-US");
  EXPECT_EQ(lt_move_assign, lt_original);
}

TEST(LanguageTagTest, Canonicalize) {
  // Deprecated tags: "iw" -> "he"
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("iw"),
              Optional(GetKnownLanguageTag("he")));

  // Deprecated tags: "cmn" -> "zh"
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("cmn"),
              Optional(GetKnownLanguageTag("zh")));
}

TEST(LanguageTagTest, LegacyLanguages) {
  // "sh" and "tl" should NOT be canonicalized.
  auto sh_tag = LanguageTagConverter::GetInstance().FromString("sh");
  ASSERT_TRUE(sh_tag.has_value());
  EXPECT_EQ(sh_tag->tag_string(), "sh");

  auto tl_tag = LanguageTagConverter::GetInstance().FromString("tl");
  ASSERT_TRUE(tl_tag.has_value());
  EXPECT_EQ(tl_tag->tag_string(), "tl");

  // Case insensitivity check
  auto sh_upper = LanguageTagConverter::GetInstance().FromString("SH");
  ASSERT_TRUE(sh_upper.has_value());
  EXPECT_EQ(sh_upper->tag_string(), "sh");  // Still lowercased by tag_string()
                                            // but not canonicalized to sr-Latn

  auto tl_with_region = LanguageTagConverter::GetInstance().FromString("tl-PH");
  ASSERT_TRUE(tl_with_region.has_value());
  EXPECT_EQ(tl_with_region->tag_string(), "tl-PH");
}

TEST(LanguageTagTest, UndefinedLanguageTag) {
  EXPECT_EQ(GetKnownLanguageTag("und").tag_string(), "und");
}

TEST(LanguageTagTest, CanCreateFixedFlatSet) {
  constexpr auto kLanguageTagsSet = base::MakeFixedFlatSet<LanguageTag>({
      GetKnownLanguageTag("en-US"),
      GetKnownLanguageTag("pt-BR"),
  });

  EXPECT_TRUE(kLanguageTagsSet.contains(GetKnownLanguageTag("en-US")));
  EXPECT_TRUE(kLanguageTagsSet.contains(GetKnownLanguageTag("pt-BR")));
}

TEST(LanguageTagTest, GetLanguageSubtag) {
  // Simple case.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_en_us,
                       LanguageTagConverter::GetInstance().FromString("en-US"));
  EXPECT_THAT(lt_en_us.GetLanguageSubtag(), LanguageSubtagString("en"));

  // Undefined case.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag und_us,
      LanguageTagConverter::GetInstance().FromString("und-US"));
  EXPECT_THAT(und_us.GetLanguageSubtag(), LanguageSubtagString("und"));

  // Chinese case.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag zh_cn,
      LanguageTagConverter::GetInstance().FromString("zh-Hans-CN"));
  EXPECT_THAT(zh_cn.GetLanguageSubtag(), LanguageSubtagString("zh"));
}

TEST(LanguageTagTest, GetRegionSubtag) {
  // Simple case.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_en_us,
                       LanguageTagConverter::GetInstance().FromString("en-US"));
  EXPECT_THAT(lt_en_us.GetRegionSubtag(), OptionalRegionToString("US"));

  // No region.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_en,
                       LanguageTagConverter::GetInstance().FromString("en"));
  EXPECT_FALSE(lt_en.GetRegionSubtag().has_value());

  // Language, Script, Region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_zh_hant_tw,
      LanguageTagConverter::GetInstance().FromString("zh-Hant-TW"));
  EXPECT_THAT(lt_zh_hant_tw.GetRegionSubtag(), OptionalRegionToString("TW"));

  // Numeric region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_es_419,
      LanguageTagConverter::GetInstance().FromString("es-419"));
  EXPECT_THAT(lt_es_419.GetRegionSubtag(), OptionalRegionToString("419"));

  // Script but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_sr_latn,
      LanguageTagConverter::GetInstance().FromString("sr-Latn"));
  EXPECT_FALSE(lt_sr_latn.GetRegionSubtag().has_value());

  // Complex case with extensions.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_complex,
      LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory"));
  EXPECT_THAT(lt_complex.GetRegionSubtag(), OptionalRegionToString("US"));

  // Extension but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_ext_no_region,
      LanguageTagConverter::GetInstance().FromString("en-u-ca-gregory"));
  EXPECT_FALSE(lt_ext_no_region.GetRegionSubtag().has_value());

  // Script + Extension but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_script_ext_no_region,
      LanguageTagConverter::GetInstance().FromString("sr-Latn-u-ca-gregory"));
  EXPECT_FALSE(lt_script_ext_no_region.GetRegionSubtag().has_value());
}

struct LanguageTestData {
  std::string_view tag;
  std::string_view name;
  LanguageTag language_tag;
};

class LanguageTagAllCodesTest
    : public testing::TestWithParam<LanguageTestData> {};

// This test ensures that the code that generates all the constant functions
// work as expected.
TEST_P(LanguageTagAllCodesTest, VerifyAllLangCodeFunctions) {
  const LanguageTestData& param = GetParam();
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString(param.tag),
              Optional(param.language_tag));
}
constexpr LanguageTestData kTestData[] = {
#define IMPL_LANGUAGECODE_TAG_NAME(tag, name) \
  {tag, #name, GetKnownLanguageTag(tag)},
#include "base/i18n/internal/canonical_language_tags.inc"
};
#undef IMPL_LANGUAGECODE_TAG_NAME

INSTANTIATE_TEST_SUITE_P(
    All,
    LanguageTagAllCodesTest,
    testing::ValuesIn(kTestData),
    [](const testing::TestParamInfo<LanguageTestData>& info) {
      return std::string(info.param.name);
    });

}  // namespace
}  // namespace base::i18n
