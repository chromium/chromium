// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code.h"

#include "base/i18n/language_code_builder.h"
#include "base/i18n/language_codes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(LanguageCodeTest, ParseAndToString) {
  auto lc = LanguageCodeBuilder::GetInstance().FromString("en-US");
  ASSERT_TRUE(lc.has_value());
  EXPECT_EQ(lc, language_codes::ENGLISH_US());

  auto lc_norm = LanguageCodeBuilder::GetInstance().FromString("EN-us");
  ASSERT_TRUE(lc_norm.has_value());
  EXPECT_EQ(lc_norm->ToString(), "en-US");
}

TEST(LanguageCodeTest, InvalidLocales) {
  EXPECT_FALSE(LanguageCodeBuilder::GetInstance().FromString("").has_value());
  EXPECT_FALSE(LanguageCodeBuilder::GetInstance()
                   .FromString("toolonglanguagecode")
                   .has_value());
  EXPECT_FALSE(LanguageCodeBuilder::GetInstance()
                   .FromString("pt-longscript-BR")
                   .has_value());
  EXPECT_FALSE(LanguageCodeBuilder::GetInstance()
                   .FromString("pt-BRA-Brazil")
                   .has_value());
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
  auto lc_zh = LanguageCodeBuilder::GetInstance().FromString("zh-Hant-HK");
  ASSERT_TRUE(lc_zh.has_value());
  EXPECT_EQ(lc_zh->ToString(), "zh-Hant-HK");

  auto lc_sr = LanguageCodeBuilder::GetInstance().FromString("sr-Latn-RS");
  ASSERT_TRUE(lc_sr.has_value());
  EXPECT_EQ(lc_sr->ToString(), "sr-Latn-RS");
}

TEST(LanguageCodeTest, NumericRegions) {
  // Locales with numeric regions.
  auto lc_es = LanguageCodeBuilder::GetInstance().FromString("es-419");
  ASSERT_TRUE(lc_es.has_value());
  EXPECT_EQ(lc_es, language_codes::SPANISH_LATIN_AMERICAN());
}

TEST(LanguageCodeTest, ThreeLetterLanguages) {
  // 3-letter language codes.
  auto lc_fil = LanguageCodeBuilder::GetInstance().FromString("fil-PH");
  ASSERT_TRUE(lc_fil.has_value());
  EXPECT_EQ(lc_fil->ToString(), "fil-PH");

  // Asturian (ast).
  auto lc_ast = LanguageCodeBuilder::GetInstance().FromString("ast-ES");
  ASSERT_TRUE(lc_ast.has_value());
  EXPECT_EQ(lc_ast->ToString(), "ast-ES");
}

TEST(LanguageCodeTest, Variants) {
  // Locales with variants.
  auto lc_gb_scuse =
      LanguageCodeBuilder::GetInstance().FromString("en-GB-scuse");
  ASSERT_TRUE(lc_gb_scuse.has_value());
  EXPECT_EQ(lc_gb_scuse->ToString(), "en-GB-scuse");

  auto lc_gb_oxendict =
      LanguageCodeBuilder::GetInstance().FromString("en-GB-oxendict");
  ASSERT_TRUE(lc_gb_oxendict.has_value());
  EXPECT_EQ(lc_gb_oxendict->ToString(), "en-GB-oxendict");

  // German with orthography variant.
  auto lc_de_1996 = LanguageCodeBuilder::GetInstance().FromString("de-1996");
  ASSERT_TRUE(lc_de_1996.has_value());
  EXPECT_EQ(lc_de_1996->ToString(), "de-1996");
}

TEST(LanguageCodeTest, Extensions) {
  // Locales with extensions.
  auto lc_us_gregory =
      LanguageCodeBuilder::GetInstance().FromString("en-US-u-ca-gregory");
  ASSERT_TRUE(lc_us_gregory.has_value());
  EXPECT_EQ(lc_us_gregory->ToString(), "en-US-u-ca-gregory");

  auto lc_us_posix =
      LanguageCodeBuilder::GetInstance().FromString("en-US-u-va-posix");
  ASSERT_TRUE(lc_us_posix.has_value());
  EXPECT_EQ(lc_us_posix->ToString(), "en-US-u-va-posix");

  // Extension with multiple keywords.
  auto lc_complex_ext = LanguageCodeBuilder::GetInstance().FromString(
      "en-US-u-ca-gregory-co-emoji");
  ASSERT_TRUE(lc_complex_ext.has_value());
  EXPECT_EQ(lc_complex_ext->ToString(), "en-US-u-ca-gregory-co-emoji");

  // Private use extensions.
  auto lc_private =
      LanguageCodeBuilder::GetInstance().FromString("en-US-x-private");
  ASSERT_TRUE(lc_private.has_value());
  EXPECT_EQ(lc_private->ToString(), "en-US-x-private");
}

TEST(LanguageCodeTest, LongBcp47Codes) {
  // A long but valid BCP47 code that should trigger heap allocation (> 12
  // chars). Azerbaijani in Cyrillic script as spoken in Russia with a variant
  // and extensions. "az-Cyrl-RU-variant-u-ca-gregory-co-phonebk"
  const std::string long_code = "az-Cyrl-RU-variant-u-ca-gregory-co-phonebk";
  auto lc_long = LanguageCodeBuilder::GetInstance().FromString(long_code);
  ASSERT_TRUE(lc_long.has_value());
  EXPECT_EQ(lc_long->ToString(), long_code);

  // Another long one: "en-US-u-ca-gregory-co-emoji-kb-true-hc-h24"
  // Note: LanguageCodeBuilder canonicalizes the extensions.
  const std::string very_long_code =
      "en-US-u-ca-gregory-co-emoji-kb-true-hc-h24";
  const std::string very_long_code_canonical =
      "en-US-u-ca-gregory-co-emoji-hc-h24-kb";
  auto lc_very_long =
      LanguageCodeBuilder::GetInstance().FromString(very_long_code);
  ASSERT_TRUE(lc_very_long.has_value());
  EXPECT_EQ(lc_very_long->ToString(), very_long_code_canonical);
}

TEST(LanguageCodeTest, PrivateUseTags) {
  // Basic private use tag.
  auto lc_x_simple = LanguageCodeBuilder::GetInstance().FromString("en-x-test");
  ASSERT_TRUE(lc_x_simple.has_value());
  EXPECT_EQ(lc_x_simple->ToString(), "en-x-test");

  // Long private use tag.
  const std::string long_x = "en-US-x-this-is-a-very-long-private-use-tag";
  auto lc_x_long = LanguageCodeBuilder::GetInstance().FromString(long_x);
  ASSERT_TRUE(lc_x_long.has_value());
  EXPECT_EQ(lc_x_long->ToString(), long_x);

  // Private use only not allowed.
  auto lc_x_only = LanguageCodeBuilder::GetInstance().FromString("x-private");
  EXPECT_FALSE(lc_x_only.has_value());
}

TEST(LanguageCodeTest, CopyAndMove) {
  auto lc_original = LanguageCodeBuilder::GetInstance().FromString("en-US");
  ASSERT_TRUE(lc_original.has_value());

  // Copy constructor
  LanguageCode lc_copy(*lc_original);
  EXPECT_EQ(lc_copy.ToString(), "en-US");
  EXPECT_EQ(lc_copy, *lc_original);

  // Copy assignment
  LanguageCode lc_copy_assign = *lc_original;
  lc_copy_assign = *lc_original;
  EXPECT_EQ(lc_copy_assign.ToString(), "en-US");
  EXPECT_EQ(lc_copy_assign, *lc_original);

  // Move constructor
  LanguageCode lc_move(std::move(lc_copy));
  EXPECT_EQ(lc_move.ToString(), "en-US");
  EXPECT_EQ(lc_move, *lc_original);

  // Move assignment
  LanguageCode lc_move_assign = *lc_original;
  lc_move_assign = std::move(lc_move);
  EXPECT_EQ(lc_move_assign.ToString(), "en-US");
  EXPECT_EQ(lc_move_assign, *lc_original);
}

TEST(LanguageCodeTest, Canonicalize) {
  // Deprecated tags: "iw" -> "he"
  auto lc_iw = LanguageCodeBuilder::GetInstance().FromString("iw");
  ASSERT_TRUE(lc_iw.has_value());
  EXPECT_EQ(lc_iw, language_codes::HEBREW());

  // Deprecated tags: "cmn" -> "zh"
  auto lc_cmn = LanguageCodeBuilder::GetInstance().FromString("cmn");
  ASSERT_TRUE(lc_cmn.has_value());
  EXPECT_EQ(lc_cmn, language_codes::CHINESE());
}

TEST(LanguageCodeTest, GetRegionCode) {
  // Simple case.
  auto lc_en_us = LanguageCodeBuilder::GetInstance().FromString("en-US");
  ASSERT_TRUE(lc_en_us.has_value());
  auto region_en_us = lc_en_us->GetRegionCode();
  ASSERT_TRUE(region_en_us.has_value());
  EXPECT_EQ(region_en_us->ToString(), "US");

  // No region.
  auto lc_en = LanguageCodeBuilder::GetInstance().FromString("en");
  ASSERT_TRUE(lc_en.has_value());
  EXPECT_FALSE(lc_en->GetRegionCode().has_value());

  // Language, Script, Region.
  auto lc_zh_hant_tw =
      LanguageCodeBuilder::GetInstance().FromString("zh-Hant-TW");
  ASSERT_TRUE(lc_zh_hant_tw.has_value());
  auto region_zh_hant_tw = lc_zh_hant_tw->GetRegionCode();
  ASSERT_TRUE(region_zh_hant_tw.has_value());
  EXPECT_EQ(region_zh_hant_tw->ToString(), "TW");

  // Numeric region.
  auto lc_es_419 = LanguageCodeBuilder::GetInstance().FromString("es-419");
  ASSERT_TRUE(lc_es_419.has_value());
  auto region_es_419 = lc_es_419->GetRegionCode();
  ASSERT_TRUE(region_es_419.has_value());
  EXPECT_EQ(region_es_419->ToString(), "419");

  // Script but no region.
  auto lc_sr_latn = LanguageCodeBuilder::GetInstance().FromString("sr-Latn");
  ASSERT_TRUE(lc_sr_latn.has_value());
  EXPECT_FALSE(lc_sr_latn->GetRegionCode().has_value());

  // Complex case with extensions.
  auto lc_complex =
      LanguageCodeBuilder::GetInstance().FromString("en-US-u-ca-gregory");
  ASSERT_TRUE(lc_complex.has_value());
  auto region_complex = lc_complex->GetRegionCode();
  ASSERT_TRUE(region_complex.has_value());
  EXPECT_EQ(region_complex->ToString(), "US");

  // Extension but no region.
  auto lc_ext_no_region =
      LanguageCodeBuilder::GetInstance().FromString("en-u-ca-gregory");
  ASSERT_TRUE(lc_ext_no_region.has_value());
  EXPECT_FALSE(lc_ext_no_region->GetRegionCode().has_value());

  // Script + Extension but no region.
  auto lc_script_ext_no_region =
      LanguageCodeBuilder::GetInstance().FromString("sr-Latn-u-ca-gregory");
  ASSERT_TRUE(lc_script_ext_no_region.has_value());
  EXPECT_FALSE(lc_script_ext_no_region->GetRegionCode().has_value());
}

struct LanguageTestData {
  std::string tag;
  std::string name;
  const LanguageCode& (*get_code)();
};

class LanguageCodeAllCodesTest
    : public testing::TestWithParam<LanguageTestData> {};

// This test ensures that the code that generates all the constant functions
// work as expected.
TEST_P(LanguageCodeAllCodesTest, VerifyAllLangCodeFunctions) {
  const LanguageTestData& param = GetParam();
  auto lc = LanguageCodeBuilder::GetInstance().FromString(param.tag);
  ASSERT_TRUE(lc.has_value());
  EXPECT_EQ(lc->ToString(), param.tag);
  EXPECT_EQ(*lc, param.get_code());
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
      return info.param.name;
    });

}  // namespace base
