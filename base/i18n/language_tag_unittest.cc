// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag.h"

#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/icu4c_tag_converter.h"
#include "base/i18n/language_tag_value_converters.h"
#include "base/i18n/tag_converters.h"
#include "base/test/gmock_expected_support.h"
#include "base/values.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base::i18n {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Optional;
using ::testing::Property;

MATCHER_P(OptionalToString, expected, "") {
  return ExplainMatchResult(
      Optional(Property(&LanguageTag::tag_string, Eq(expected))), arg,
      result_listener);
}

TEST(LanguageTagTest, CompileTimeTags) {
  // Verifies that multi-subtag tags can be created at compile-time.
  static_assert(GetKnownLanguageTag("ja-JP").tag_string() == "ja-JP");
  static_assert(GetKnownLanguageTag("en-US").tag_string() == "en-US");

  // Verifies GetKnownLanguageTag wrapper.
  static_assert(GetKnownLanguageTag("en-US").tag_string() == "en-US");
  constexpr LanguageTag ja_jp = GetKnownLanguageTag("ja-JP");
  static_assert(ja_jp.tag_string() == "ja-JP");
}

TEST(LanguageTagTest, ParseAndSubtagsString) {
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
              Optional(Property(&Extension::SubtagsString, Eq("foo"))));
  EXPECT_THAT(
      lc.GetExtension(bcp47_extensions::unicode()),
      Optional(Property(&UnicodeExtension::SubtagsString, Eq("ca-gregory"))));
  EXPECT_THAT(
      lc.GetExtension(bcp47_extensions::priv()),
      Optional(Property(&PrivateUseSubtags::SubtagsString, Eq("private"))));
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
        Optional(Property(&PrivateUseSubtags::SubtagsString, Eq("private"))));
  }
  {
    // Single-char private use subtags.
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US-x-a"))
    EXPECT_EQ(lc.tag_string(), "en-US-x-a");
    EXPECT_THAT(lc.GetExtension(bcp47_extensions::priv()),
                Optional(Property(&PrivateUseSubtags::SubtagsString, Eq("a"))));
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
        Optional(Property(&PrivateUseSubtags::SubtagsString, Eq("12345678"))));
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

  // Deprecated tags: "tl" -> "fil"
  EXPECT_THAT(LanguageTagConverter::GetInstance().FromString("tl"),
              Optional(GetKnownLanguageTag("fil")));
}

TEST(LanguageTagTest, LegacyLanguages) {
  // "sh" should NOT be canonicalized.
  auto sh_tag = LanguageTagConverter::GetInstance().FromString("sh");
  ASSERT_TRUE(sh_tag.has_value());
  EXPECT_EQ(sh_tag->tag_string(), "sh");

  // Case insensitivity check
  auto sh_upper = LanguageTagConverter::GetInstance().FromString("SH");
  ASSERT_TRUE(sh_upper.has_value());
  EXPECT_EQ(sh_upper->tag_string(), "sh");  // Still lowercased by tag_string()
                                            // but not canonicalized to sr-Latn
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
  EXPECT_EQ(lt_en_us.language_subtag(), "en");
  EXPECT_EQ(lt_en_us.WithLanguageSubtagOnly(), GetKnownLanguageTag("en"));

  // Undefined case.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag und_us,
      LanguageTagConverter::GetInstance().FromString("und-US"));
  EXPECT_EQ(und_us.language_subtag(), "und");
  EXPECT_EQ(und_us.WithLanguageSubtagOnly(), GetKnownLanguageTag("und"));

  // Chinese case.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag zh_cn,
      LanguageTagConverter::GetInstance().FromString("zh-Hans-CN"));
  EXPECT_EQ(zh_cn.language_subtag(), "zh");
  EXPECT_EQ(zh_cn.WithLanguageSubtagOnly(), GetKnownLanguageTag("zh"));
}

TEST(LanguageTagTest, GetRegionSubtag) {
  // Simple case.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_en_us,
                       LanguageTagConverter::GetInstance().FromString("en-US"));
  EXPECT_EQ(lt_en_us.region_subtag(), "US");

  // No region.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_en,
                       LanguageTagConverter::GetInstance().FromString("en"));
  EXPECT_TRUE(lt_en.region_subtag().empty());

  // Language, Script, Region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_zh_hant_tw,
      LanguageTagConverter::GetInstance().FromString("zh-Hant-TW"));
  EXPECT_EQ(lt_zh_hant_tw.region_subtag(), "TW");

  // Numeric region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_es_419,
      LanguageTagConverter::GetInstance().FromString("es-419"));
  EXPECT_EQ(lt_es_419.region_subtag(), "419");

  // Script but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_sr_latn,
      LanguageTagConverter::GetInstance().FromString("sr-Latn"));
  EXPECT_TRUE(lt_sr_latn.region_subtag().empty());

  // Complex case with extensions.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_complex,
      LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory"));
  EXPECT_EQ(lt_complex.region_subtag(), "US");

  // Extension but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_ext_no_region,
      LanguageTagConverter::GetInstance().FromString("en-u-ca-gregory"));
  EXPECT_TRUE(lt_ext_no_region.region_subtag().empty());

  // Script + Extension but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_script_ext_no_region,
      LanguageTagConverter::GetInstance().FromString("sr-Latn-u-ca-gregory"));
  EXPECT_TRUE(lt_script_ext_no_region.region_subtag().empty());
}

TEST(LanguageTagTest, GetScriptSubtag) {
  // Simple case with script.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_zh_hant_tw,
      LanguageTagConverter::GetInstance().FromString("zh-Hant-TW"));
  EXPECT_EQ(lt_zh_hant_tw.script_subtag(), "Hant");

  // Script but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_sr_latn,
      LanguageTagConverter::GetInstance().FromString("sr-Latn"));
  EXPECT_EQ(lt_sr_latn.script_subtag(), "Latn");

  // No script.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_en_us,
                       LanguageTagConverter::GetInstance().FromString("en-US"));
  EXPECT_TRUE(lt_en_us.script_subtag().empty());

  // Complex case with extensions and script.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_complex,
      LanguageTagConverter::GetInstance().FromString("sr-Latn-u-ca-gregory"));
  EXPECT_EQ(lt_complex.script_subtag(), "Latn");

  // Undefined language with script.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_und_latn,
      LanguageTagConverter::GetInstance().FromString("und-Latn"));
  EXPECT_EQ(lt_und_latn.script_subtag(), "Latn");
}

TEST(LanguageTagTest, GetVariantSubtags) {
  // No variants.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_en_us,
                       LanguageTagConverter::GetInstance().FromString("en-US"));
  EXPECT_TRUE(lt_en_us.variant_subtags().empty());

  // Single variant.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_oxendict,
      LanguageTagConverter::GetInstance().FromString("en-GB-oxendict"));
  EXPECT_THAT(lt_oxendict.variant_subtags(), ElementsAre("oxendict"));

  // Numeric variant.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_de_1996,
      LanguageTagConverter::GetInstance().FromString("de-1996"));
  EXPECT_THAT(lt_de_1996.variant_subtags(), ElementsAre("1996"));

  // Multiple variants.
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt_multiple,
      LanguageTagConverter::GetInstance().FromString("sl-IT-rozaj-biske"));
  // Variants are sorted.
  EXPECT_THAT(lt_multiple.variant_subtags(), ElementsAre("biske", "rozaj"));

  // Complex tag with variants and extension.
  ASSERT_OK_AND_ASSIGN(LanguageTag lt_complex,
                       LanguageTagConverter::GetInstance().FromString(
                           "en-GB-oxendict-u-ca-gregory"));
  EXPECT_THAT(lt_complex.variant_subtags(), ElementsAre("oxendict"));
}

TEST(LanguageTagTest, GetParentEnUs) {
  ASSERT_OK_AND_ASSIGN(LanguageTag lt,
                       LanguageTagConverter::GetInstance().FromString("en-US"));
  EXPECT_THAT(lt.GetParentTag(), OptionalToString("en"));
  EXPECT_EQ(lt.GetParentTag()->GetParentTag(), std::nullopt);
}

TEST(LanguageTagTest, GetParentSrLatnRs) {
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt,
      LanguageTagConverter::GetInstance().FromString("sr-Latn-RS"));
  ASSERT_OK_AND_ASSIGN(LanguageTag parent1, lt.GetParentTag());
  EXPECT_EQ(parent1.tag_string(), "sr-Latn");
  ASSERT_OK_AND_ASSIGN(LanguageTag parent2, parent1.GetParentTag());
  EXPECT_EQ(parent2.tag_string(), "sr");
  EXPECT_EQ(parent2.GetParentTag(), std::nullopt);
}

TEST(LanguageTagTest, GetParentWithExtension) {
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt,
      LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory"));
  EXPECT_THAT(lt.GetParentTag(), OptionalToString("en-US"));
}

TEST(LanguageTagTest, GetParentWithMultipleExtensions) {
  ASSERT_OK_AND_ASSIGN(LanguageTag lt,
                       LanguageTagConverter::GetInstance().FromString(
                           "en-US-a-abcdef-u-ca-gregory"));
  EXPECT_THAT(lt.GetParentTag(), OptionalToString("en-US"));
}

TEST(LanguageTagTest, GetParentWithVariantsAndExtension) {
  ASSERT_OK_AND_ASSIGN(LanguageTag lt,
                       LanguageTagConverter::GetInstance().FromString(
                           "en-GB-oxendict-u-ca-gregory"));
  EXPECT_THAT(lt.GetParentTag(), OptionalToString("en-GB-oxendict"));
  EXPECT_THAT(lt.GetParentTag()->GetParentTag(), OptionalToString("en-GB"));
  EXPECT_THAT(lt.GetParentTag()->GetParentTag()->GetParentTag(),
              OptionalToString("en"));
}

TEST(LanguageTagTest, GetParentConstexpr) {
  static_assert(GetKnownLanguageTag("es-MX").GetParentTag().value() ==
                GetKnownLanguageTag("es"));
  static_assert(!GetKnownLanguageTag("es").GetParentTag().has_value());
}

TEST(LanguageTagTest, GetParentWithPrivateUseSubtags) {
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt,
      LanguageTagConverter::GetInstance().FromString("en-US-x-test"));
  EXPECT_THAT(lt.GetParentTag(), OptionalToString("en-US"));
}

TEST(LanguageTagTest, GetLineage) {
  ASSERT_OK_AND_ASSIGN(
      LanguageTag lt,
      LanguageTagConverter::GetInstance().FromString("sr-Latn-RS"));
  EXPECT_THAT(lt.GetLineage(), ElementsAre(GetKnownLanguageTag("sr-Latn-RS"),
                                           GetKnownLanguageTag("sr-Latn"),
                                           GetKnownLanguageTag("sr")));
}

TEST(LanguageTagTest, GetLineageNoParents) {
  ASSERT_OK_AND_ASSIGN(LanguageTag lt,
                       LanguageTagConverter::GetInstance().FromString("en"));
  EXPECT_THAT(lt.GetLineage(), ElementsAre(GetKnownLanguageTag("en")));
}

TEST(LanguageTagTest, ExtensionMutation) {
  // 1. GetExtension (Unicode) - returning std::nullopt when not present, and
  // WithExtension.
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US"));
    std::optional<UnicodeExtension> u_ext =
        lc.GetExtension(bcp47_extensions::unicode());
    EXPECT_FALSE(u_ext.has_value());

    std::optional<UnicodeExtension> new_u_ext =
        UnicodeExtension::FromString("u-ca-gregory");
    ASSERT_TRUE(new_u_ext.has_value());
    LanguageTag mutated = lc.WithExtension(*new_u_ext);
    EXPECT_EQ(mutated.tag_string(), "en-US-u-ca-gregory");
  }

  // 2. GetExtension (Unicode) - modifying existing.
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory"));
    std::optional<UnicodeExtension> u_ext =
        lc.GetExtension(bcp47_extensions::unicode());
    ASSERT_TRUE(u_ext.has_value());
    EXPECT_THAT(u_ext->GetKeywordValue("ca"), Optional(Eq("gregory")));
    EXPECT_TRUE(u_ext->SetKeyword("ca", "buddhist"));
    LanguageTag mutated = lc.WithExtension(*u_ext);
    EXPECT_EQ(mutated.tag_string(), "en-US-u-ca-buddhist");
  }

  // 3. PrivateUseSubtags mutation.
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US"));
    std::optional<PrivateUseSubtags> x_ext =
        lc.GetExtension(bcp47_extensions::priv());
    EXPECT_FALSE(x_ext.has_value());

    std::optional<PrivateUseSubtags> new_x_ext =
        PrivateUseSubtags::FromString("x-private");
    ASSERT_TRUE(new_x_ext.has_value());
    EXPECT_TRUE(new_x_ext->AddSubtag("stuff"));
    LanguageTag mutated = lc.WithExtension(*new_x_ext);
    EXPECT_EQ(mutated.tag_string(), "en-US-x-private-stuff");
  }

  // 4. Generic Extension mutation.
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US"));
    std::optional<Extension> a_ext =
        lc.GetExtension(bcp47_extensions::ext<'a'>());
    EXPECT_FALSE(a_ext.has_value());

    std::optional<Extension> new_a_ext = Extension::FromString("a-myext");
    ASSERT_TRUE(new_a_ext.has_value());
    EXPECT_TRUE(new_a_ext->AddSubtag("other"));
    LanguageTag mutated = lc.WithExtension(*new_a_ext);
    EXPECT_EQ(mutated.tag_string(), "en-US-a-myext-other");
  }
}

TEST(LanguageTagTest, WithExtensionRemoved) {
  // Removing an extension from a tag with no extensions should return the same
  // tag.
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US"));
    EXPECT_EQ(lc.WithExtensionRemoved('a').tag_string(), "en-US");
    EXPECT_EQ(lc.WithExtensionRemoved('x').tag_string(), "en-US");
  }

  // Removing an existing Unicode extension ('u').
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory"));
    EXPECT_EQ(lc.WithExtensionRemoved('u').tag_string(), "en-US");
    // Also test case insensitivity for 'U'.
    EXPECT_EQ(lc.WithExtensionRemoved('U').tag_string(), "en-US");
  }

  // Removing an existing generic extension ('a').
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US-a-foo-bar"));
    EXPECT_EQ(lc.WithExtensionRemoved('a').tag_string(), "en-US");
    // Also test case insensitivity for 'A'.
    EXPECT_EQ(lc.WithExtensionRemoved('A').tag_string(), "en-US");
  }

  // Removing an existing private use extension ('x').
  {
    ASSERT_OK_AND_ASSIGN(
        LanguageTag lc,
        LanguageTagConverter::GetInstance().FromString("en-US-x-private"));
    EXPECT_EQ(lc.WithExtensionRemoved('x').tag_string(), "en-US");
    // Also test 'X'.
    EXPECT_EQ(lc.WithExtensionRemoved('X').tag_string(), "en-US");
  }

  // Removing an extension from a tag with multiple extensions.
  {
    ASSERT_OK_AND_ASSIGN(LanguageTag lc,
                         LanguageTagConverter::GetInstance().FromString(
                             "en-US-a-foo-u-ca-gregory-x-private"));
    EXPECT_EQ(lc.WithExtensionRemoved('a').tag_string(),
              "en-US-u-ca-gregory-x-private");
    EXPECT_EQ(lc.WithExtensionRemoved('u').tag_string(),
              "en-US-a-foo-x-private");
    EXPECT_EQ(lc.WithExtensionRemoved('x').tag_string(),
              "en-US-a-foo-u-ca-gregory");
  }
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

namespace {

auto GetTestData() {
  static constexpr auto kTestData = std::to_array<LanguageTestData>({
#define IMPL_LANGUAGECODE_TAG_NAME(tag, name) \
  {tag, #name, GetKnownLanguageTag(tag)},
#include "base/i18n/internal/canonical_language_tags.inc"
#undef IMPL_LANGUAGECODE_TAG_NAME
  });
  return kTestData;
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(
    All,
    LanguageTagAllCodesTest,
    testing::ValuesIn(GetTestData()),
    [](const testing::TestParamInfo<LanguageTestData>& info) {
      return std::string(info.param.name);
    });

TEST(IcuLocaleConverterTest, FromLanguageTag) {
  const IcuLocaleConverter& converter = IcuLocaleConverter::GetInstance();

  // Test simple locale conversion
  std::optional<LanguageTag> en_us =
      LanguageTagConverter::GetInstance().FromString("en-US");
  ASSERT_TRUE(en_us.has_value());
  icu::Locale locale_en_us = converter.FromLanguageTag(*en_us);
  EXPECT_STREQ("en_US", locale_en_us.getName());
  EXPECT_STREQ("en", locale_en_us.getLanguage());
  EXPECT_STREQ("US", locale_en_us.getCountry());

  // Test ja-JP locale conversion
  std::optional<LanguageTag> ja_jp =
      LanguageTagConverter::GetInstance().FromString("ja-JP");
  ASSERT_TRUE(ja_jp.has_value());
  icu::Locale locale_ja_jp = converter.FromLanguageTag(*ja_jp);
  EXPECT_STREQ("ja_JP", locale_ja_jp.getName());
  EXPECT_STREQ("ja", locale_ja_jp.getLanguage());
  EXPECT_STREQ("JP", locale_ja_jp.getCountry());

  // Test language tag with script: zh-Hans-CN
  std::optional<LanguageTag> zh_hans_cn =
      LanguageTagConverter::GetInstance().FromString("zh-Hans-CN");
  ASSERT_TRUE(zh_hans_cn.has_value());
  icu::Locale locale_zh_hans_cn = converter.FromLanguageTag(*zh_hans_cn);
  EXPECT_STREQ("zh_Hans_CN", locale_zh_hans_cn.getName());
  EXPECT_STREQ("zh", locale_zh_hans_cn.getLanguage());
  EXPECT_STREQ("Hans", locale_zh_hans_cn.getScript());
  EXPECT_STREQ("CN", locale_zh_hans_cn.getCountry());

  // Test undefined language tag: und
  std::optional<LanguageTag> und =
      LanguageTagConverter::GetInstance().FromString("und");
  ASSERT_TRUE(und.has_value());
  icu::Locale locale_und = converter.FromLanguageTag(*und);
  EXPECT_STREQ("", locale_und.getName());

  // Test custom/dynamic language tag (not in the cache) fallback path:
  // en-US-u-ca-gregory
  std::optional<LanguageTag> dynamic_tag =
      LanguageTagConverter::GetInstance().FromString("en-US-u-ca-gregory");
  ASSERT_TRUE(dynamic_tag.has_value());
  icu::Locale locale_dynamic = converter.FromLanguageTag(*dynamic_tag);
  EXPECT_STREQ("en_US@calendar=gregorian", locale_dynamic.getName());
}

TEST(IcuLocaleConverterTest, ToLanguageTag) {
  const IcuLocaleConverter& converter = IcuLocaleConverter::GetInstance();

  // Test simple locale conversion
  UErrorCode status = U_ZERO_ERROR;
  icu::Locale locale_en_us = icu::Locale::forLanguageTag("en-US", status);
  ASSERT_TRUE(U_SUCCESS(status));
  LanguageTag en_us = converter.ToLanguageTag(locale_en_us);
  EXPECT_EQ("en-US", en_us.tag_string());

  // Test custom/dynamic locale conversion
  status = U_ZERO_ERROR;
  icu::Locale locale_dynamic =
      icu::Locale::forLanguageTag("en-US-u-ca-gregory", status);
  ASSERT_TRUE(U_SUCCESS(status));
  LanguageTag dynamic_tag = converter.ToLanguageTag(locale_dynamic);
  EXPECT_EQ("en-US-u-ca-gregory", dynamic_tag.tag_string());

  // Test fallback/failure or undefined
  icu::Locale locale_und = icu::Locale::getRoot();
  LanguageTag und = converter.ToLanguageTag(locale_und);
  EXPECT_EQ("und", und.tag_string());
}

}  // namespace
}  // namespace base::i18n
