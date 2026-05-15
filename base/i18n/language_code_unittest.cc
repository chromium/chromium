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
  EXPECT_FALSE(
      LanguageCodeBuilder::GetInstance().FromString("pt-Brazil").has_value());
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

TEST(LanguageCodeTest, UnsupportedVariantsAndExtensions) {
  // Locales with variants are currently rejected by policy.
  EXPECT_FALSE(
      LanguageCodeBuilder::GetInstance().FromString("en-GB-scuse").has_value());
  EXPECT_FALSE(LanguageCodeBuilder::GetInstance()
                   .FromString("en-GB-oxendict")
                   .has_value());

  // Locales with extensions are currently rejected by policy.
  EXPECT_FALSE(LanguageCodeBuilder::GetInstance()
                   .FromString("en-US-u-ca-gregory")
                   .has_value());
  EXPECT_FALSE(LanguageCodeBuilder::GetInstance()
                   .FromString("en-US-u-va-posix")
                   .has_value());
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

struct LanguageTestData {
  const char* tag;
  const char* name;
  const LanguageCode& (*get_code)();
};

class LanguageCodeAllCodesTest
    : public testing::TestWithParam<LanguageTestData> {};

TEST_P(LanguageCodeAllCodesTest, Verify) {
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
