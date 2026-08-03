// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/supported_locales.h"

#include <optional>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_set.h"
#include "base/i18n/icu_util.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/uloc.h"

namespace base::i18n {
namespace {

using ::testing::Contains;
using ::testing::IsSupersetOf;
using ::testing::Not;

constexpr auto kCanonicalTags = std::to_array<LanguageTag>({
#define IMPL_LANGUAGECODE_TAG_NAME(tag, name) GetKnownLanguageTag(tag),
#include "base/i18n/internal/canonical_language_tags.inc"
#undef IMPL_LANGUAGECODE_TAG_NAME
});

constexpr auto kExcludedTags = base::MakeFixedFlatSet<LanguageTag>({
    GetKnownLanguageTag("ar-XB"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("as"),
#endif
    GetKnownLanguageTag("ay"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("bho"),
    GetKnownLanguageTag("bm"),
    GetKnownLanguageTag("ceb"),
    GetKnownLanguageTag("chr"),
#endif
    GetKnownLanguageTag("co"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("doi"),
#endif
    GetKnownLanguageTag("dv"),
    GetKnownLanguageTag("en-001"),
    GetKnownLanguageTag("en-GB-oxendict"),
    GetKnownLanguageTag("en-XA"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("fy"),
    GetKnownLanguageTag("gd"),
#endif
    GetKnownLanguageTag("gn"),
    GetKnownLanguageTag("hmn"),
    GetKnownLanguageTag("ht"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("ia"),
#endif
    GetKnownLanguageTag("ilo"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("jv"),
    GetKnownLanguageTag("kok"),
#endif
    GetKnownLanguageTag("kri"),
    GetKnownLanguageTag("la"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("lb"),
#endif
    GetKnownLanguageTag("lus"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("mai"),
    GetKnownLanguageTag("mi"),
#endif
    GetKnownLanguageTag("mni-Mtei"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("nso"),
#endif
    GetKnownLanguageTag("ny"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("oc"),
    GetKnownLanguageTag("qu"),
    GetKnownLanguageTag("sa"),
    GetKnownLanguageTag("sd"),
#endif
    GetKnownLanguageTag("sh"),
    GetKnownLanguageTag("sm"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("st"),
    GetKnownLanguageTag("su"),
    GetKnownLanguageTag("tk"),
    GetKnownLanguageTag("tn"),
#endif
    GetKnownLanguageTag("ts"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("tt"),
    GetKnownLanguageTag("ug"),
#endif
    GetKnownLanguageTag("und"),
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    GetKnownLanguageTag("wo"),
    GetKnownLanguageTag("xh"),
    GetKnownLanguageTag("yi"),
#endif
    GetKnownLanguageTag("yue"),
    GetKnownLanguageTag("zh-CN"),
    GetKnownLanguageTag("zh-HK"),
    GetKnownLanguageTag("zh-TW"),
});

class SupportedLocalesTest : public testing::Test {
 public:
  void SetUp() override { base::i18n::InitializeICU(); }
};

TEST_F(SupportedLocalesTest, ReturnsNonEmptySupportedLocales) {
  const base::flat_set<LanguageTag>& locales = GetSupportedIcuLocales();
  EXPECT_FALSE(locales.empty());
}

TEST_F(SupportedLocalesTest, ContainsCommonLocales) {
  const base::flat_set<LanguageTag>& locales = GetSupportedIcuLocales();

  // "en" should be supported.
  std::optional<LanguageTag> en_tag = GetLanguageTagFromString("en");
  ASSERT_TRUE(en_tag.has_value());
  EXPECT_TRUE(locales.contains(*en_tag));

  // "fr" should be supported.
  std::optional<LanguageTag> fr_tag = GetLanguageTagFromString("fr");
  ASSERT_TRUE(fr_tag.has_value());
  EXPECT_TRUE(locales.contains(*fr_tag));
}

TEST_F(SupportedLocalesTest, ParentsAreAlsoIncluded) {
  const base::flat_set<LanguageTag>& locales = GetSupportedIcuLocales();

  // If a subtag like "en-US" is in the locales, its parent "en" must be as
  // well.
  std::optional<LanguageTag> en_us_tag = GetLanguageTagFromString("en-US");
  ASSERT_TRUE(en_us_tag.has_value());

  if (locales.contains(*en_us_tag)) {
    std::optional<LanguageTag> parent_tag = en_us_tag->GetParentTag();
    ASSERT_TRUE(parent_tag.has_value());
    EXPECT_TRUE(locales.contains(*parent_tag));
  }
}

TEST_F(SupportedLocalesTest, DoesNotContainInvalidLocales) {
  // English as used in Brazil, it is a valid bcp47 tag, but it does not have
  // ICU data.
  EXPECT_THAT(GetSupportedIcuLocales(),
              Not(Contains(GetKnownLanguageTag("en-BR"))));
}

TEST_F(SupportedLocalesTest, DoesNotContainEnglishGlobal) {
  EXPECT_THAT(GetSupportedIcuLocales(),
              Not(Contains(GetKnownLanguageTag("en-001"))));
}

TEST_F(SupportedLocalesTest, ContainsSpanishLatinAmerica) {
  EXPECT_THAT(GetSupportedIcuLocales(),
              Contains(GetKnownLanguageTag("es-419")));
}

TEST_F(SupportedLocalesTest, MatchesCannonicalLocalesMostly) {
  std::vector<LanguageTag> expected_tags;
  for (const LanguageTag& tag : kCanonicalTags) {
    if (!kExcludedTags.contains(tag)) {
      expected_tags.push_back(tag);
    }
  }

  EXPECT_THAT(GetSupportedIcuLocales(), IsSupersetOf(expected_tags));
}

TEST_F(SupportedLocalesTest, ExludeTagsAreNotReturned) {
  EXPECT_THAT(GetSupportedIcuLocales(),
              Not(IsSupersetOf(base::span(kExcludedTags))));
}

TEST_F(SupportedLocalesTest, DisplayNameFormatAllLocales) {
  const base::flat_set<LanguageTag>& locales = GetSupportedIcuLocales();
  for (const LanguageTag& tag : locales) {
    std::string locale_name = std::string(tag.tag_string());
    std::u16string display_name;
    const int kBufferSize = 1024;
    UErrorCode error = U_ZERO_ERROR;

    int32_t count = uloc_getDisplayName(
        locale_name.c_str(), "en", base::WriteInto(&display_name, kBufferSize),
        kBufferSize - 1, &error);
    EXPECT_TRUE(U_SUCCESS(error))
        << "Failed to get display name for " << locale_name;
    EXPECT_GT(count, 0) << "Got empty display name for " << locale_name;
  }
}

}  // namespace
}  // namespace base::i18n
