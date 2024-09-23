// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_methods_by_language.h"

#include <utility>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

using ::testing::TestWithParam;

using LanguageCategoryTestCase = std::pair<std::string, LanguageCategory>;
using MapToLanguageCategoryTest = TestWithParam<LanguageCategoryTestCase>;

INSTANTIATE_TEST_SUITE_P(
    InputMethodsByLanguage,
    MapToLanguageCategoryTest,
    testing::ValuesIn<LanguageCategoryTestCase>({
        // Danish
        {"xkb:dk::dan", LanguageCategory::kDanish},
        // Dutch
        {"xkb:be::nld", LanguageCategory::kDutch},
        {"xkb:us:intl_pc:nld", LanguageCategory::kDutch},
        {"xkb:us:intl:nld", LanguageCategory::kDutch},
        // Finnish
        {"xkb:fi::fin", LanguageCategory::kFinnish},
        // English
        {"xkb:ca:eng:eng", LanguageCategory::kEnglish},
        {"xkb:gb::eng", LanguageCategory::kEnglish},
        {"xkb:gb:extd:eng", LanguageCategory::kEnglish},
        {"xkb:gb:dvorak:eng", LanguageCategory::kEnglish},
        {"xkb:in::eng", LanguageCategory::kEnglish},
        {"xkb:pk::eng", LanguageCategory::kEnglish},
        {"xkb:us:altgr-intl:eng", LanguageCategory::kEnglish},
        {"xkb:us:colemak:eng", LanguageCategory::kEnglish},
        {"xkb:us:dvorak:eng", LanguageCategory::kEnglish},
        {"xkb:us:dvp:eng", LanguageCategory::kEnglish},
        {"xkb:us:intl_pc:eng", LanguageCategory::kEnglish},
        {"xkb:us:intl:eng", LanguageCategory::kEnglish},
        {"xkb:us:workman-intl:eng", LanguageCategory::kEnglish},
        {"xkb:us:workman:eng", LanguageCategory::kEnglish},
        {"xkb:us::eng", LanguageCategory::kEnglish},
        {"xkb:za:gb:eng", LanguageCategory::kEnglish},
        // French
        {"xkb:be::fra", LanguageCategory::kFrench},
        {"xkb:ca::fra", LanguageCategory::kFrench},
        {"xkb:ca:multix:fra", LanguageCategory::kFrench},
        {"xkb:fr::fra", LanguageCategory::kFrench},
        {"xkb:fr:bepo:fra", LanguageCategory::kFrench},
        {"xkb:ch:fr:fra", LanguageCategory::kFrench},
        // German
        {"xkb:be::ger", LanguageCategory::kGerman},
        {"xkb:de::ger", LanguageCategory::kGerman},
        {"xkb:de:neo:ger", LanguageCategory::kGerman},
        {"xkb:ch::ger", LanguageCategory::kGerman},
        // Italian
        {"xkb:it::ita", LanguageCategory::kItalian},
        // Japanese
        {"xkb:jp::jpn", LanguageCategory::kJapanese},
        {"nacl_mozc_us", LanguageCategory::kJapanese},
        {"nacl_mozc_jp", LanguageCategory::kJapanese},
        // Norwegian
        {"xkb:no::nob", LanguageCategory::kNorwegian},
        // Portugese
        {"xkb:br::por", LanguageCategory::kPortugese},
        {"xkb:pt::por", LanguageCategory::kPortugese},
        {"xkb:us:intl_pc:por", LanguageCategory::kPortugese},
        {"xkb:us:intl:por", LanguageCategory::kPortugese},
        // Spanish
        {"xkb:latam::spa", LanguageCategory::kSpanish},
        {"xkb:es::spa", LanguageCategory::kSpanish},
        // Swedish
        {"xkb:se::swe", LanguageCategory::kSwedish},
    }));

TEST_P(MapToLanguageCategoryTest, IsCorrectCategory) {
  const auto& [input_method, category] = GetParam();

  EXPECT_EQ(InputMethodToLanguageCategory(input_method), category);
}
}  // namespace
}  // namespace ash::input_method
