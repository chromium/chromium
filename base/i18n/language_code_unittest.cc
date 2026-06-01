// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code.h"

#include <string_view>

#include "base/i18n/language_code_builder.h"
#include "base/i18n/language_codes.h"
#include "base/test/gmock_expected_support.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

using ::testing::Eq;
using ::testing::Optional;
using ::testing::Property;

MATCHER_P(OptionalToString, expected, "") {
  return ExplainMatchResult(
      Optional(Property(&LanguageCode::ToString, Eq(expected))), arg,
      result_listener);
}

MATCHER_P(OptionalRegionToString, expected, "") {
  return ExplainMatchResult(
      Optional(Property(&RegionCode::ToString, Eq(expected))), arg,
      result_listener);
}

TEST(LanguageCodeTest, ParseAndToString) {
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en-US"),
              Optional(language_codes::ENGLISH_US()));

  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("EN-us"),
              OptionalToString("en-US"));
}

TEST(LanguageCodeTest, InvalidLocales) {
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString(""), std::nullopt);
  EXPECT_EQ(
      LanguageCodeBuilder::GetInstance().FromString("toolonglanguagecode"),
      std::nullopt);
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString("pt-longscript-BR"),
            std::nullopt);
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString("pt-BRA-Brazil"),
            std::nullopt);
}

TEST(LanguageCodeTest, ValidButUnknowLocales) {
  // Standard BCP 47 accepts these as well-formed even if they are semantically
  // "unknown".
  EXPECT_TRUE(
      LanguageCodeBuilder::GetInstance().FromString("xx-YY").has_value());
  EXPECT_TRUE(
      LanguageCodeBuilder::GetInstance().FromString("pt-YY").has_value());
  EXPECT_TRUE(
      LanguageCodeBuilder::GetInstance().FromString("zh-Yyyy").has_value());
  EXPECT_TRUE(
      LanguageCodeBuilder::GetInstance().FromString("zh-Hant-XX").has_value());
}

TEST(LanguageCodeTest, ToLegacyICUFormat) {
  EXPECT_EQ(language_codes::BRAZILIAN_PORTUGUESE().ToLegacyICUFormat(),
            "pt_BR");
}

TEST(LanguageCodeTest, ComplexLocales) {
  // Valid complex locales (lang-script-region) within length limit.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("zh-Hant-HK"),
              OptionalToString("zh-Hant-HK"));

  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("sr-Latn-RS"),
              OptionalToString("sr-Latn-RS"));
}

TEST(LanguageCodeTest, NumericRegions) {
  // Locales with numeric regions.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("es-419"),
              Optional(language_codes::SPANISH_LATIN_AMERICAN()));
}

TEST(LanguageCodeTest, ThreeLetterLanguages) {
  // 3-letter language codes.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("fil-PH"),
              OptionalToString("fil-PH"));

  // Asturian (ast).
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("ast-ES"),
              OptionalToString("ast-ES"));
}

TEST(LanguageCodeTest, Variants) {
  // Locales with variants.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en-GB-scuse"),
              OptionalToString("en-GB-scuse"));

  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en-GB-oxendict"),
              OptionalToString("en-GB-oxendict"));

  // German with orthography variant.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("de-1996"),
              OptionalToString("de-1996"));
}

TEST(LanguageCodeTest, Extensions) {
  // Locales with extensions.
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("en-US-u-ca-gregory"),
      OptionalToString("en-US-u-ca-gregory"));

  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en-US-u-va-posix"),
              OptionalToString("en-US-u-va-posix"));

  // Extension with multiple keywords.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString(
                  "en-US-u-ca-gregory-co-emoji"),
              OptionalToString("en-US-u-ca-gregory-co-emoji"));

  // Private use extensions.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en-US-x-private"),
              OptionalToString("en-US-x-private"));
}

TEST(LanguageCodeTest, LongBcp47Codes) {
  // A long but valid BCP47 code that should trigger heap allocation (> 12
  // chars). Azerbaijani in Cyrillic script as spoken in Russia with a variant
  // and extensions. "az-Cyrl-RU-variant-u-ca-gregory-co-phonebk"
  const std::string long_code = "az-Cyrl-RU-variant-u-ca-gregory-co-phonebk";
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString(long_code),
              OptionalToString(long_code));

  // Another long one: "en-US-u-ca-gregory-co-emoji-kb-true-hc-h24"
  // Note: LanguageCodeBuilder canonicalizes the extensions.
  const std::string very_long_code =
      "en-US-u-ca-gregory-co-emoji-kb-true-hc-h24";
  const std::string very_long_code_canonical =
      "en-US-u-ca-gregory-co-emoji-hc-h24-kb";
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString(very_long_code),
              OptionalToString(very_long_code_canonical));
}

TEST(LanguageCodeTest, PrivateUseTags) {
  // Basic private use tag.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en-x-test"),
              OptionalToString("en-x-test"));

  // Long private use tag.
  const std::string long_x = "en-US-x-this-is-a-very-long-private-use-tag";
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString(long_x),
              OptionalToString(long_x));

  // Private use only not allowed.
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString("x-private"),
            std::nullopt);
}

TEST(LanguageCodeTest, LocaleWithAtSign) {
  // Locales with keywords after '@' should be converted to BCP47 extensions.
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("en-US@currency=USD"),
      OptionalToString("en-US-u-cu-usd"));

  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("ca_ES@valencia"),
              OptionalToString("ca-ES-u-va-valencia"));

  // Multiple keywords.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString(
                  "de_DE@calendar=gregorian;collation=phonebook"),
              OptionalToString("de-DE-u-ca-gregory-co-phonebk"));

  // Japanese line break and line word extensions.
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("ja@lb=normal"),
              OptionalToString("ja-u-lb-normal"));
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("ja@lb=strict"),
              OptionalToString("ja-u-lb-strict"));
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("ja@lb=loose"),
              OptionalToString("ja-u-lb-loose"));
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("ja@lw=phrase"),
              OptionalToString("ja-u-lw-phrase"));
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("ja@lb=normal;lw=phrase"),
      OptionalToString("ja-u-lb-normal-lw-phrase"));
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("ja@lb=strict;lw=phrase"),
      OptionalToString("ja-u-lb-strict-lw-phrase"));
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("ja@lb=loose;lw=phrase"),
      OptionalToString("ja-u-lb-loose-lw-phrase"));

  // Collation and attributes.
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("de@collation=phonebook"),
      OptionalToString("de-u-co-phonebk"));
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("el@colCaseFirst=upper"),
      OptionalToString("el-u-kf-upper"));
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("el-u-kf-upper"),
              OptionalToString("el-u-kf-upper"));

  // Timezone aliases.
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("en-US@timezone=pst8pdt"),
      OptionalToString("en-US-u-tz-uslax"));
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("en-US@timezone=est5edt"),
      OptionalToString("en-US-u-tz-usnyc"));
}

TEST(LanguageCodeTest, LegacyIcuRobustness) {
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en@"),
              OptionalToString("en"));

  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en@ab=cd;;;"),
              OptionalToString("en-u-ab-cd"));

  // Keywords without values.
  // "en@calendar" -> "en-u-calendar" -> ICU4X might return "en".
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("en@calendar"),
              OptionalToString("en-u-ca"));
}

TEST(LanguageCodeTest, LegacyIcuRepeatedKeys) {
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("en@calendar;calendar"),
      OptionalToString("en-u-ca"));
}

TEST(LanguageCodeTest, LegacyIcuBadlyFormed) {
  // Values with special characters.
  // ICU4X canonicalizer will likely fail on these, returning nullopt, which is
  // sane.
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString(
                "en@calendar=../../etc/passwd"),
            std::nullopt);
  EXPECT_EQ(
      LanguageCodeBuilder::GetInstance().FromString("en@calendar=foo bar"),
      std::nullopt);
}

TEST(LanguageCodeTest, LegacyIcuLongCodes) {
  // Extremely long input.
  std::string long_string(1000, 'a');
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString(long_string + "_US"),
            std::nullopt);
  // Extremely long keyword value.
  std::string long_value(1000, 'b');
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString("en@calendar=" +
                                                          long_value),
            std::nullopt);
}

TEST(LanguageCodeTest, LegacyIcuWellFormedCodes) {
  // Mixed case keys (mapping is case-insensitive for keys).
  EXPECT_THAT(
      LanguageCodeBuilder::GetInstance().FromString("en@Calendar=gregorian"),
      OptionalToString("en-u-ca-gregory"));

  // Multiple equal signs.
  // "en@calendar=gregorian=extra" -> "en-u-ca-gregorian=extra"
  // ICU4X will likely reject "gregorian=extra" as a value.
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString(
                "en@calendar=gregorian=extra"),
            std::nullopt);

  // Just an @ sign.
  // "@" is < 2 chars, so it returns nullopt immediately.
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString("@"), std::nullopt);

  // Just an _ sign.
  EXPECT_EQ(LanguageCodeBuilder::GetInstance().FromString("_"), std::nullopt);
}

TEST(LanguageCodeTest, CopyAndMove) {
  ASSERT_OK_AND_ASSIGN(LanguageCode lc_original,
                       LanguageCodeBuilder::GetInstance().FromString("en-US"));

  // Copy constructor
  LanguageCode lc_copy(lc_original);
  EXPECT_EQ(lc_copy.ToString(), "en-US");
  EXPECT_EQ(lc_copy, lc_original);

  // Copy assignment
  LanguageCode lc_copy_assign = lc_original;
  lc_copy_assign = lc_original;
  EXPECT_EQ(lc_copy_assign.ToString(), "en-US");
  EXPECT_EQ(lc_copy_assign, lc_original);

  // Move constructor
  LanguageCode lc_move(std::move(lc_copy));
  EXPECT_EQ(lc_move.ToString(), "en-US");
  EXPECT_EQ(lc_move, lc_original);

  // Move assignment
  LanguageCode lc_move_assign = lc_original;
  lc_move_assign = std::move(lc_move);
  EXPECT_EQ(lc_move_assign.ToString(), "en-US");
  EXPECT_EQ(lc_move_assign, lc_original);
}

TEST(LanguageCodeTest, Canonicalize) {
  // Deprecated tags: "iw" -> "he"
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("iw"),
              Optional(language_codes::HEBREW()));

  // Deprecated tags: "cmn" -> "zh"
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString("cmn"),
              Optional(language_codes::CHINESE()));
}

TEST(LanguageCodeTest, GetRegionCode) {
  // Simple case.
  ASSERT_OK_AND_ASSIGN(LanguageCode lc_en_us,
                       LanguageCodeBuilder::GetInstance().FromString("en-US"));
  EXPECT_THAT(lc_en_us.GetRegionCode(), OptionalRegionToString("US"));

  // No region.
  ASSERT_OK_AND_ASSIGN(LanguageCode lc_en,
                       LanguageCodeBuilder::GetInstance().FromString("en"));
  EXPECT_FALSE(lc_en.GetRegionCode().has_value());

  // Language, Script, Region.
  ASSERT_OK_AND_ASSIGN(
      LanguageCode lc_zh_hant_tw,
      LanguageCodeBuilder::GetInstance().FromString("zh-Hant-TW"));
  EXPECT_THAT(lc_zh_hant_tw.GetRegionCode(), OptionalRegionToString("TW"));

  // Numeric region.
  ASSERT_OK_AND_ASSIGN(LanguageCode lc_es_419,
                       LanguageCodeBuilder::GetInstance().FromString("es-419"));
  EXPECT_THAT(lc_es_419.GetRegionCode(), OptionalRegionToString("419"));

  // Script but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageCode lc_sr_latn,
      LanguageCodeBuilder::GetInstance().FromString("sr-Latn"));
  EXPECT_FALSE(lc_sr_latn.GetRegionCode().has_value());

  // Complex case with extensions.
  ASSERT_OK_AND_ASSIGN(
      LanguageCode lc_complex,
      LanguageCodeBuilder::GetInstance().FromString("en-US-u-ca-gregory"));
  EXPECT_THAT(lc_complex.GetRegionCode(), OptionalRegionToString("US"));

  // Extension but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageCode lc_ext_no_region,
      LanguageCodeBuilder::GetInstance().FromString("en-u-ca-gregory"));
  EXPECT_FALSE(lc_ext_no_region.GetRegionCode().has_value());

  // Script + Extension but no region.
  ASSERT_OK_AND_ASSIGN(
      LanguageCode lc_script_ext_no_region,
      LanguageCodeBuilder::GetInstance().FromString("sr-Latn-u-ca-gregory"));
  EXPECT_FALSE(lc_script_ext_no_region.GetRegionCode().has_value());
}

struct LanguageTestData {
  std::string_view tag;
  std::string_view name;
  const LanguageCode& (*get_code)();
};

class LanguageCodeAllCodesTest
    : public testing::TestWithParam<LanguageTestData> {};

// This test ensures that the code that generates all the constant functions
// work as expected.
TEST_P(LanguageCodeAllCodesTest, VerifyAllLangCodeFunctions) {
  const LanguageTestData& param = GetParam();
  EXPECT_THAT(LanguageCodeBuilder::GetInstance().FromString(param.tag),
              Optional(param.get_code()));
}

const LanguageTestData kTestData[] = {
#define IMPL_LANGUAGECODE_TAG_NAME(tag, name) \
  {tag, #name, &language_codes::name},
#include "base/i18n/internal/canonical_language_codes.inc"
#undef IMPL_LANGUAGECODE_TAG_NAME
};

INSTANTIATE_TEST_SUITE_P(
    All,
    LanguageCodeAllCodesTest,
    testing::ValuesIn(kTestData),
    [](const testing::TestParamInfo<LanguageTestData>& info) {
      return std::string(info.param.name);
    });

}  // namespace base
