// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/language_tag_matcher.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/i18n/tags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

using ::testing::Eq;
using ::testing::Optional;

LanguageTag LanguageTagOrDie(std::string_view tag) {
  return *LanguageTagConverter::GetInstance().FromString(tag);
}

TEST(LanguageTagMatcherTest, ExactMatch) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("en-US"),
                                        LanguageTagOrDie("fr-FR")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageTagOrDie("en-US")),
              Optional(LanguageTagOrDie("en-US")));
}

TEST(LanguageTagMatcherTest, FallbackMatch) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("en"),
                                        LanguageTagOrDie("fr")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  // en-US should fallback to en
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("en-US")),
              Optional(LanguageTagOrDie("en")));
}

TEST(LanguageTagMatcherTest, NoMatch) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("en-US")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageTagOrDie("fr-FR")), Eq(std::nullopt));
}

TEST(LanguageTagMatcherTest, SpanishLatinAmerica) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("es-419"),
                                        LanguageTagOrDie("es")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(language_tags::SPANISH_ARGENTINA()),
              Eq(LanguageTagOrDie("es-419")));
}

TEST(LanguageTagMatcherTest, SpanishMexico) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("es-CL"),
                                        LanguageTagOrDie("es-MX")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageTagOrDie("es-AR")),
              Optional(LanguageTagOrDie("es-MX")));
}

TEST(LanguageTagMatcherTest, PortugueseBrazil) {
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("pt-BR"),
                                          LanguageTagOrDie("pt-PT")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageTagOrDie("pt")),
                Optional(LanguageTagOrDie("pt-BR")));
  }
  // Checks that changing the order does not affect the result.
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("pt-PT"),
                                          LanguageTagOrDie("pt-BR")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageTagOrDie("pt")),
                Optional(LanguageTagOrDie("pt-BR")));
  }
}

TEST(LanguageTagMatcherTest, MultipleEnglishLocales) {
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("en-US"),
                                          LanguageTagOrDie("en-GB"),
                                          LanguageTagOrDie("en-AU")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageTagOrDie("en")),
                Optional(LanguageTagOrDie("en-US")));
  }
  // Checks that changing the order does not affect the result.
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("en-AU"),
                                          LanguageTagOrDie("en-US")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageTagOrDie("en")),
                Optional(LanguageTagOrDie("en-US")));
  }
  // Checks that changing the order does not affect the result.
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("en-AU"),
                                          LanguageTagOrDie("en-GB")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageTagOrDie("en")),
                Optional(LanguageTagOrDie("en-GB")));
  }
}

TEST(LanguageTagMatcherTest, ChineseLanguages) {
  std::vector<LanguageTag> supported = {language_tags::TAIWAN_CHINESE(),
                                        language_tags::CHINA_CHINESE()};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(language_tags::CHINESE()),
              Optional(language_tags::CHINA_CHINESE()));
}

TEST(LanguageTagMatcherTest, TaiwanChineseDoesNotMatchChinese) {
  std::vector<LanguageTag> supported = {language_tags::TAIWAN_CHINESE()};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(language_tags::CHINESE()), Eq(std::nullopt));
}

TEST(LanguageTagMatcherTest, ChineseRegions_ZhTwSupported) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-TW")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  // zh-HK should match zh-TW because both use Traditional script (Hant).
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-HK")),
              Optional(LanguageTagOrDie("zh-TW")));

  // zh-MO (Macau) should also match zh-TW.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-MO")),
              Optional(LanguageTagOrDie("zh-TW")));

  // zh-Hant (Generic Traditional) should match zh-TW.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-Hant")),
              Optional(LanguageTagOrDie("zh-TW")));
  // Chinese should not match to Taiwan Chinese.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh")), Eq(std::nullopt));
}

TEST(LanguageTagMatcherTest, ChineseRegions_ZhCnSupported) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-CN")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  // zh-SG (Singapore) should match zh-CN because both use Simplified script
  // (Hans).
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-SG")),
              Optional(LanguageTagOrDie("zh-CN")));

  // zh-MY (Malaysia) should also match zh-CN.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-MY")),
              Optional(LanguageTagOrDie("zh-CN")));

  // zh-Hans (Generic Simplified) should match zh-CN.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-Hans")),
              Optional(LanguageTagOrDie("zh-CN")));
}

TEST(LanguageTagMatcherTest, ChineseRegions_ScriptAffinity) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-CN"),
                                        LanguageTagOrDie("zh-TW")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  // zh-HK prefers zh-TW over zh-CN.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-HK")),
              Optional(LanguageTagOrDie("zh-TW")));

  // zh-SG prefers zh-CN over zh-TW.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-SG")),
              Optional(LanguageTagOrDie("zh-CN")));
}

TEST(LanguageTagMatcherTest, ChineseScriptMatchesRegion) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-HK")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  // zh-Hant (Generic Traditional) should match zh-HK if it's the only
  // Traditional option.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-Hant")),
              Optional(LanguageTagOrDie("zh-HK")));

  // zh-TW should also match zh-HK if only zh-HK is supported.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-TW")),
              Optional(LanguageTagOrDie("zh-HK")));
}

TEST(LanguageTagMatcherTest, MultipleChineseRegions) {
  std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-SG"),
                                        LanguageTagOrDie("zh-CN")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  // zh-CN should win over zh-SG.
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh")),
              Optional(LanguageTagOrDie("zh-CN")));
}

TEST(LanguageTagMatcherTest, ComplexFallback) {
  // zh-Hans-CN should fallback to zh (which is usually zh-Hans)
  {
    std::vector<LanguageTag> supported = {language_tags::CHINESE()};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    LanguageTag lc_zh_hans_cn = LanguageTagOrDie("zh-Hans-CN");
    EXPECT_THAT(matcher.Match(lc_zh_hans_cn),
                Optional(language_tags::CHINESE()));
  }

  // zh-Hant-HK should fallback to zh-Hant
  {
    LanguageTag lc_zh_hant = LanguageTagOrDie("zh-Hant");
    std::vector<LanguageTag> supported = {lc_zh_hant};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);
    LanguageTag lc_zh_hant_hk = LanguageTagOrDie("zh-Hant-HK");
    EXPECT_THAT(matcher.Match(lc_zh_hant_hk), Optional(lc_zh_hant));
  }
}

TEST(LanguageTagMatcherTest, LikelySubtagsMatch) {
  // If "en" is preferred, and "en-US" is supported.
  // "en" fallback chain is ["en", "und"]. No match.
  // "en" with likely subtags is "en-Latn-US".
  // "en-Latn-US" fallback chain includes "en-US".
  std::vector<LanguageTag> supported = {LanguageTagOrDie("en-US")};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

  EXPECT_THAT(matcher.Match(LanguageTagOrDie("en")),
              Optional(LanguageTagOrDie("en-US")));
}

TEST(LanguageTagMatcherTest, LikelySubtagsNoMatch) {
  std::vector<LanguageTag> supported = {language_tags::ENGLISH_UK()};
  LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);
  EXPECT_THAT(matcher.Match(LanguageTagOrDie("en")),
              Optional(language_tags::ENGLISH_UK()));
}

TEST(LanguageTagMatcherTest, SerboCroatian) {
  // Serbian (sr) usually defaults to Cyrillic script.
  // Serbo-Croatian (sh) is an older macro-language tag, often mapped to
  // sr-Latn.

  // Test 1: Preferred "sh" (Serbo-Croatian), Supported "sr-Latn".
  // "sh" is canonicalized to "sr-Latn" by LanguageTagConverter/Rust bridge.
  {
    std::vector<LanguageTag> supported = {language_tags::SERBO_CROATIAN()};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);
    LanguageTag lc_sh = LanguageTagOrDie("sh");
    // "sh" matches "sr-Latn" because LanguageTagConverter canonicalizes it.
    EXPECT_THAT(matcher.Match(lc_sh), Optional(LanguageTagOrDie("sr-Latn")));
  }

  // Test 2: Preferred "sr-Latn" (Serbo-Croatian), Supported "hr" (Croatian).
  // These are distinct languages in ICU/CLDR, so they should NOT match
  // via fallback or maximization.
  {
    std::vector<LanguageTag> supported = {language_tags::CROATIAN()};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);
    EXPECT_THAT(matcher.Match(language_tags::SERBO_CROATIAN()),
                Eq(std::nullopt));
  }

  // Test 3: Preferred "sr-HR" (Serbian as spoken in Croatia), Supported
  // "sr-Latn" (Serbian). "sr-HR" fallsback to "sr", but it is not able to
  // matcch "sr-Latn".
  {
    LanguageTagMatcher matcher =
        LanguageTagMatcher::Create({language_tags::SERBO_CROATIAN()});
    EXPECT_THAT(matcher.Match(LanguageTagOrDie("sr-HR")), Eq(std::nullopt));
  }
}

TEST(LanguageTagMatcherTest, ChineseVariantsAndExtensions) {
  // Case 1: Locale with variant matching supported base language.
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-Latn")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    // zh-Latn-pinyin should match zh-Latn.
    EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-Latn-pinyin")),
                Optional(LanguageTagOrDie("zh-Latn")));
  }

  // Case 2: Locale with extension matching supported base language.
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-Hans")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    // zh-Hans-u-co-pinyin should match zh-Hans.
    EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-Hans-u-co-pinyin")),
                Optional(LanguageTagOrDie("zh-Hans")));
  }

  // Case 3: Locale with both variant and region.
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-CN")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    // zh-Hans-CN-pinyin (simplified as zh-CN-pinyin by some) should match
    // zh-CN. Note: pinyin is usually a variant for Latn script, but testing
    // fallback here.
    EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-CN-pinyin")),
                Optional(LanguageTagOrDie("zh-CN")));
  }

  // Case 4: Locale with unicode extension.
  {
    std::vector<LanguageTag> supported = {LanguageTagOrDie("zh")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    // zh-u-em-emoji should match zh.
    EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-u-em-emoji")),
                Optional(LanguageTagOrDie("zh")));
  }

  // Case 5: Complex variant fallback.
  {
    // If zh-Latn-pinyin is supported, it should be matched exactly.
    std::vector<LanguageTag> supported = {LanguageTagOrDie("zh-Latn-pinyin"),
                                          LanguageTagOrDie("zh-Latn")};
    LanguageTagMatcher matcher = LanguageTagMatcher::Create(supported);

    EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-Latn-pinyin")),
                Optional(LanguageTagOrDie("zh-Latn-pinyin")));

    // zh-Latn-wadegile should match zh-Latn as it's the closest ancestor.
    EXPECT_THAT(matcher.Match(LanguageTagOrDie("zh-Latn-wadegile")),
                Optional(LanguageTagOrDie("zh-Latn")));
  }
}

}  // namespace
}  // namespace base
