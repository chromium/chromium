// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_settings.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace input_method {
namespace {

namespace mojom = chromeos::ime::mojom;

constexpr char kUsEnglishEngineId[] = "xkb:us::eng";
constexpr char kKoreanEngineId[] = "ko-t-i0-und";

void RegisterTestingPrefs(TestingPrefServiceSimple& prefs,
                          const base::DictionaryValue& dict) {
  prefs.registry()->RegisterDictionaryPref(
      ::prefs::kLanguageInputMethodSpecificSettings);
  prefs.Set(::prefs::kLanguageInputMethodSpecificSettings, dict);
}

TEST(CreateSettingsFromPrefsTest, CreateLatinSettingsDefault) {
  base::DictionaryValue dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId);

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_FALSE(latin_settings.autocorrect);
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, CreateLatinSettings) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures(
      {features::kImeMojoDecoder, features::kSystemLatinPhysicalTyping,
       features::kAssistMultiWord},
      {});
  TestingPrefServiceSimple prefs;
  base::DictionaryValue dict;
  dict.SetIntPath(base::StrCat({kUsEnglishEngineId,
                                ".physicalKeyboardAutoCorrectionLevel"}),
                  1);
  RegisterTestingPrefs(prefs, dict);
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId);

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_TRUE(latin_settings.autocorrect);
  EXPECT_TRUE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, CreateKoreanSettingsDefault) {
  base::DictionaryValue dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kKoreanEngineId);

  ASSERT_TRUE(settings->is_korean_settings());
  const auto& korean_settings = *settings->get_korean_settings();
  EXPECT_EQ(korean_settings.layout, mojom::KoreanLayout::kDubeolsik);
  EXPECT_FALSE(korean_settings.input_multiple_syllables);
}

TEST(CreateSettingsFromPrefsTest, CreateKoreanSettings) {
  base::DictionaryValue dict;
  dict.SetStringPath(base::StrCat({kKoreanEngineId, ".koreanKeyboardLayout"}),
                     "3 Set (390) / 세벌식 (390)");
  dict.SetBoolPath(
      base::StrCat({kKoreanEngineId, ".koreanEnableSyllableInput"}), false);
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kKoreanEngineId);

  ASSERT_TRUE(settings->is_korean_settings());
  const auto& korean_settings = *settings->get_korean_settings();
  EXPECT_EQ(korean_settings.layout, mojom::KoreanLayout::kSebeolsik390);
  EXPECT_TRUE(korean_settings.input_multiple_syllables);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
