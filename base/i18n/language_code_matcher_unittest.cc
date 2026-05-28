// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_code_matcher.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/i18n/language_code.h"
#include "base/i18n/language_code_builder.h"
#include "base/i18n/language_codes.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

using ::testing::Eq;
using ::testing::Optional;

LanguageCode LanguageCodeOrDie(std::string_view code) {
  return *LanguageCodeBuilder::GetInstance().FromString(code);
}

TEST(LanguageCodeMatcherTest, ExactMatch) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("en-US"),
                                         LanguageCodeOrDie("fr-FR")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("en-US")),
              Optional(LanguageCodeOrDie("en-US")));
}

TEST(LanguageCodeMatcherTest, FallbackMatch) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("en"),
                                         LanguageCodeOrDie("fr")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  // en-US should fallback to en
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("en-US")),
              Optional(LanguageCodeOrDie("en")));
}

TEST(LanguageCodeMatcherTest, NoMatch) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("en-US")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("fr-FR")), Eq(std::nullopt));
}

TEST(LanguageCodeMatcherTest, SpanishLatinAmerica) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("es-419"),
                                         LanguageCodeOrDie("es")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(language_codes::SPANISH_ARGENTINA()),
              Eq(LanguageCodeOrDie("es-419")));
}

TEST(LanguageCodeMatcherTest, SpanishMexico) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("es-CL"),
                                         LanguageCodeOrDie("es-MX")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("es-AR")),
              Optional(LanguageCodeOrDie("es-MX")));
}

TEST(LanguageCodeMatcherTest, PortugueseBrazil) {
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("pt-BR"),
                                           LanguageCodeOrDie("pt-PT")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("pt")),
                Optional(LanguageCodeOrDie("pt-BR")));
  }
  // Checks that changing the order does not affect the result.
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("pt-PT"),
                                           LanguageCodeOrDie("pt-BR")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("pt")),
                Optional(LanguageCodeOrDie("pt-BR")));
  }
}

TEST(LanguageCodeMatcherTest, MultipleEnglishLocales) {
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("en-US"),
                                           LanguageCodeOrDie("en-GB"),
                                           LanguageCodeOrDie("en-AU")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("en")),
                Optional(LanguageCodeOrDie("en-US")));
  }
  // Checks that changing the order does not affect the result.
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("en-AU"),
                                           LanguageCodeOrDie("en-US")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("en")),
                Optional(LanguageCodeOrDie("en-US")));
  }
  // Checks that changing the order does not affect the result.
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("en-AU"),
                                           LanguageCodeOrDie("en-GB")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("en")),
                Optional(LanguageCodeOrDie("en-GB")));
  }
}

TEST(LanguageCodeMatcherTest, ChineseLanguages) {
  std::vector<LanguageCode> supported = {language_codes::TAIWAN_CHINESE(),
                                         language_codes::CHINA_CHINESE()};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(language_codes::CHINESE()),
              Optional(language_codes::CHINA_CHINESE()));
}

TEST(LanguageCodeMatcherTest, TaiwanChineseDoesNotMatchChinese) {
  std::vector<LanguageCode> supported = {language_codes::TAIWAN_CHINESE()};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(language_codes::CHINESE()), Eq(std::nullopt));
}

TEST(LanguageCodeMatcherTest, ChineseRegions_ZhTwSupported) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-TW")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  // zh-HK should match zh-TW because both use Traditional script (Hant).
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-HK")),
              Optional(LanguageCodeOrDie("zh-TW")));

  // zh-MO (Macau) should also match zh-TW.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-MO")),
              Optional(LanguageCodeOrDie("zh-TW")));

  // zh-Hant (Generic Traditional) should match zh-TW.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-Hant")),
              Optional(LanguageCodeOrDie("zh-TW")));
  // Chinese should not match to Taiwan Chinese.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh")), Eq(std::nullopt));
}

TEST(LanguageCodeMatcherTest, ChineseRegions_ZhCnSupported) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-CN")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  // zh-SG (Singapore) should match zh-CN because both use Simplified script
  // (Hans).
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-SG")),
              Optional(LanguageCodeOrDie("zh-CN")));

  // zh-MY (Malaysia) should also match zh-CN.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-MY")),
              Optional(LanguageCodeOrDie("zh-CN")));

  // zh-Hans (Generic Simplified) should match zh-CN.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-Hans")),
              Optional(LanguageCodeOrDie("zh-CN")));
}

TEST(LanguageCodeMatcherTest, ChineseRegions_ScriptAffinity) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-CN"),
                                         LanguageCodeOrDie("zh-TW")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  // zh-HK prefers zh-TW over zh-CN.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-HK")),
              Optional(LanguageCodeOrDie("zh-TW")));

  // zh-SG prefers zh-CN over zh-TW.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-SG")),
              Optional(LanguageCodeOrDie("zh-CN")));
}

TEST(LanguageCodeMatcherTest, ChineseScriptMatchesRegion) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-HK")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  // zh-Hant (Generic Traditional) should match zh-HK if it's the only
  // Traditional option.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-Hant")),
              Optional(LanguageCodeOrDie("zh-HK")));

  // zh-TW should also match zh-HK if only zh-HK is supported.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-TW")),
              Optional(LanguageCodeOrDie("zh-HK")));
}

TEST(LanguageCodeMatcherTest, MultipleChineseRegions) {
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-SG"),
                                         LanguageCodeOrDie("zh-CN")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  // zh-CN should win over zh-SG.
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh")),
              Optional(LanguageCodeOrDie("zh-CN")));
}

TEST(LanguageCodeMatcherTest, ComplexFallback) {
  // zh-Hans-CN should fallback to zh (which is usually zh-Hans)
  {
    std::vector<LanguageCode> supported = {language_codes::CHINESE()};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    LanguageCode lc_zh_hans_cn = LanguageCodeOrDie("zh-Hans-CN");
    EXPECT_THAT(matcher.Match(lc_zh_hans_cn),
                Optional(language_codes::CHINESE()));
  }

  // zh-Hant-HK should fallback to zh-Hant
  {
    LanguageCode lc_zh_hant = LanguageCodeOrDie("zh-Hant");
    std::vector<LanguageCode> supported = {lc_zh_hant};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);
    LanguageCode lc_zh_hant_hk = LanguageCodeOrDie("zh-Hant-HK");
    EXPECT_THAT(matcher.Match(lc_zh_hant_hk), Optional(lc_zh_hant));
  }
}

TEST(LanguageCodeMatcherTest, LikelySubtagsMatch) {
  // If "en" is preferred, and "en-US" is supported.
  // "en" fallback chain is ["en", "und"]. No match.
  // "en" with likely subtags is "en-Latn-US".
  // "en-Latn-US" fallback chain includes "en-US".
  std::vector<LanguageCode> supported = {LanguageCodeOrDie("en-US")};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("en")),
              Optional(LanguageCodeOrDie("en-US")));
}

TEST(LanguageCodeMatcherTest, LikelySubtagsNoMatch) {
  std::vector<LanguageCode> supported = {language_codes::ENGLISH_UK()};
  LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);
  EXPECT_THAT(matcher.Match(LanguageCodeOrDie("en")),
              Optional(language_codes::ENGLISH_UK()));
}

TEST(LanguageCodeMatcherTest, SerboCroatian) {
  // Serbian (sr) usually defaults to Cyrillic script.
  // Serbo-Croatian (sh) is an older macro-language tag, often mapped to
  // sr-Latn.

  // Test 1: Preferred "sh" (Serbo-Croatian), Supported "sr-Latn".
  // "sh" is canonicalized to "sr-Latn" by LanguageCodeBuilder/Rust bridge.
  {
    std::vector<LanguageCode> supported = {language_codes::SERBO_CROATIAN()};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);
    LanguageCode lc_sh = LanguageCodeOrDie("sh");
    // "sh" matches "sr-Latn" because LanguageCodeBuilder canonicalizes it.
    EXPECT_THAT(matcher.Match(lc_sh), Optional(LanguageCodeOrDie("sr-Latn")));
  }

  // Test 2: Preferred "sr-Latn" (Serbo-Croatian), Supported "hr" (Croatian).
  // These are distinct languages in ICU/CLDR, so they should NOT match
  // via fallback or maximization.
  {
    std::vector<LanguageCode> supported = {language_codes::CROATIAN()};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);
    EXPECT_THAT(matcher.Match(language_codes::SERBO_CROATIAN()),
                Eq(std::nullopt));
  }

  // Test 3: Preferred "sr-HR" (Serbian as spoken in Croatia), Supported
  // "sr-Latn" (Serbian). "sr-HR" fallsback to "sr", but it is not able to
  // matcch "sr-Latn".
  {
    LanguageCodeMatcher matcher =
        LanguageCodeMatcher::Create({language_codes::SERBO_CROATIAN()});
    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("sr-HR")), Eq(std::nullopt));
  }
}

TEST(LanguageCodeMatcherTest, ChineseVariantsAndExtensions) {
  // Case 1: Locale with variant matching supported base language.
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-Latn")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    // zh-Latn-pinyin should match zh-Latn.
    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-Latn-pinyin")),
                Optional(LanguageCodeOrDie("zh-Latn")));
  }

  // Case 2: Locale with extension matching supported base language.
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-Hans")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    // zh-Hans-u-co-pinyin should match zh-Hans.
    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-Hans-u-co-pinyin")),
                Optional(LanguageCodeOrDie("zh-Hans")));
  }

  // Case 3: Locale with both variant and region.
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-CN")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    // zh-Hans-CN-pinyin (simplified as zh-CN-pinyin by some) should match
    // zh-CN. Note: pinyin is usually a variant for Latn script, but testing
    // fallback here.
    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-CN-pinyin")),
                Optional(LanguageCodeOrDie("zh-CN")));
  }

  // Case 4: Locale with unicode extension.
  {
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    // zh-u-em-emoji should match zh.
    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-u-em-emoji")),
                Optional(LanguageCodeOrDie("zh")));
  }

  // Case 5: Complex variant fallback.
  {
    // If zh-Latn-pinyin is supported, it should be matched exactly.
    std::vector<LanguageCode> supported = {LanguageCodeOrDie("zh-Latn-pinyin"),
                                           LanguageCodeOrDie("zh-Latn")};
    LanguageCodeMatcher matcher = LanguageCodeMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-Latn-pinyin")),
                Optional(LanguageCodeOrDie("zh-Latn-pinyin")));

    // zh-Latn-wadegile should match zh-Latn as it's the closest ancestor.
    EXPECT_THAT(matcher.Match(LanguageCodeOrDie("zh-Latn-wadegile")),
                Optional(LanguageCodeOrDie("zh-Latn")));
  }
}

}  // namespace
}  // namespace base
