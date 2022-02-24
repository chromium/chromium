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

namespace mojom = ::ash::ime::mojom;

constexpr char kUsEnglishEngineId[] = "xkb:us::eng";
constexpr char kKoreanEngineId[] = "ko-t-i0-und";
constexpr char kPinyinEngineId[] = "zh-t-i0-pinyin";
constexpr char kZhuyinEngineId[] = "zh-hant-t-i0-und";

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

  const auto settings =
      CreateSettingsFromPrefs(prefs, kUsEnglishEngineId, InputFieldContext{});

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_FALSE(latin_settings.autocorrect);
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, CreateLatinSettings) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAssistMultiWord}, {});
  TestingPrefServiceSimple prefs;
  base::DictionaryValue dict;
  dict.SetIntPath(base::StrCat({kUsEnglishEngineId,
                                ".physicalKeyboardAutoCorrectionLevel"}),
                  1);
  RegisterTestingPrefs(prefs, dict);
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

  const auto settings =
      CreateSettingsFromPrefs(prefs, kUsEnglishEngineId, InputFieldContext{});

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_TRUE(latin_settings.autocorrect);
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest,
     PredictiveWritingEnabledWhenMultiWordAllowedAndEnabledAndLacrosDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAssistMultiWord}, {});
  TestingPrefServiceSimple prefs;
  base::DictionaryValue dict;
  RegisterTestingPrefs(prefs, dict);
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId,
                                                InputFieldContext{
                                                    .lacros_enabled = false,
                                                    .multiword_enabled = true,
                                                    .multiword_allowed = true,
                                                });

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_TRUE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, PredictiveWritingDisabledWhenLacrosEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAssistMultiWord}, {});
  TestingPrefServiceSimple prefs;
  base::DictionaryValue dict;
  RegisterTestingPrefs(prefs, dict);
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId,
                                                InputFieldContext{
                                                    .lacros_enabled = true,
                                                    .multiword_enabled = true,
                                                    .multiword_allowed = true,
                                                });

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest,
     PredictiveWritingDisabledWhenMultiwordNotAllowed) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAssistMultiWord}, {});
  TestingPrefServiceSimple prefs;
  base::DictionaryValue dict;
  RegisterTestingPrefs(prefs, dict);
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId,
                                                InputFieldContext{
                                                    .lacros_enabled = false,
                                                    .multiword_enabled = true,
                                                    .multiword_allowed = false,
                                                });

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest,
     PredictiveWritingDisabledWhenMultiwordDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAssistMultiWord}, {});
  TestingPrefServiceSimple prefs;
  base::DictionaryValue dict;
  RegisterTestingPrefs(prefs, dict);
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId,
                                                InputFieldContext{
                                                    .lacros_enabled = false,
                                                    .multiword_enabled = false,
                                                    .multiword_allowed = true,
                                                });

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, CreateKoreanSettingsDefault) {
  base::DictionaryValue dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings =
      CreateSettingsFromPrefs(prefs, kKoreanEngineId, InputFieldContext{});

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

  const auto settings =
      CreateSettingsFromPrefs(prefs, kKoreanEngineId, InputFieldContext{});

  ASSERT_TRUE(settings->is_korean_settings());
  const auto& korean_settings = *settings->get_korean_settings();
  EXPECT_EQ(korean_settings.layout, mojom::KoreanLayout::kSebeolsik390);
  EXPECT_TRUE(korean_settings.input_multiple_syllables);
}

TEST(CreateSettingsFromPrefsTest, CreatePinyinSettingsDefault) {
  base::DictionaryValue dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings =
      CreateSettingsFromPrefs(prefs, kPinyinEngineId, InputFieldContext{});

  ASSERT_TRUE(settings->is_pinyin_settings());
  const auto& pinyin_settings = *settings->get_pinyin_settings();
  ASSERT_TRUE(pinyin_settings.fuzzy_pinyin);
  const auto& fuzzy_pinyin = *pinyin_settings.fuzzy_pinyin;
  EXPECT_FALSE(fuzzy_pinyin.an_ang);
  EXPECT_FALSE(fuzzy_pinyin.en_eng);
  EXPECT_FALSE(fuzzy_pinyin.ian_iang);
  EXPECT_FALSE(fuzzy_pinyin.k_g);
  EXPECT_FALSE(fuzzy_pinyin.r_l);
  EXPECT_FALSE(fuzzy_pinyin.uan_uang);
  EXPECT_FALSE(fuzzy_pinyin.c_ch);
  EXPECT_FALSE(fuzzy_pinyin.f_h);
  EXPECT_FALSE(fuzzy_pinyin.in_ing);
  EXPECT_FALSE(fuzzy_pinyin.l_n);
  EXPECT_FALSE(fuzzy_pinyin.s_sh);
  EXPECT_FALSE(fuzzy_pinyin.z_zh);
  EXPECT_EQ(pinyin_settings.layout, mojom::PinyinLayout::kUsQwerty);
  EXPECT_TRUE(pinyin_settings.use_hyphen_and_equals_to_page_candidates);
  EXPECT_TRUE(pinyin_settings.use_comma_and_period_to_page_candidates);
  EXPECT_TRUE(pinyin_settings.default_to_chinese);
  EXPECT_FALSE(pinyin_settings.default_to_full_width_characters);
  EXPECT_TRUE(pinyin_settings.default_to_full_width_punctuation);
}

TEST(CreateSettingsFromPrefsTest, CreatePinyinSettings) {
  base::DictionaryValue dict;
  dict.SetBoolPath("pinyin.en:eng", true);
  dict.SetBoolPath("pinyin.k:g", true);
  dict.SetBoolPath("pinyin.in:ing", true);
  dict.SetStringPath("pinyin.xkbLayout", "Colemak");
  dict.SetBoolPath("pinyin.pinyinEnableLowerPaging", false);
  dict.SetBoolPath("pinyin.pinyinEnableUpperPaging", false);
  dict.SetBoolPath("pinyin.pinyinDefaultChinese", false);
  dict.SetBoolPath("pinyin.pinyinFullWidthCharacter", true);
  dict.SetBoolPath("pinyin.pinyinChinesePunctuation", false);
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings =
      CreateSettingsFromPrefs(prefs, kPinyinEngineId, InputFieldContext{});

  ASSERT_TRUE(settings->is_pinyin_settings());
  const auto& pinyin_settings = *settings->get_pinyin_settings();
  ASSERT_TRUE(pinyin_settings.fuzzy_pinyin);
  const auto& fuzzy_pinyin = *pinyin_settings.fuzzy_pinyin;
  EXPECT_FALSE(fuzzy_pinyin.an_ang);
  EXPECT_TRUE(fuzzy_pinyin.en_eng);
  EXPECT_FALSE(fuzzy_pinyin.ian_iang);
  EXPECT_TRUE(fuzzy_pinyin.k_g);
  EXPECT_FALSE(fuzzy_pinyin.r_l);
  EXPECT_FALSE(fuzzy_pinyin.uan_uang);
  EXPECT_FALSE(fuzzy_pinyin.c_ch);
  EXPECT_FALSE(fuzzy_pinyin.f_h);
  EXPECT_TRUE(fuzzy_pinyin.in_ing);
  EXPECT_FALSE(fuzzy_pinyin.l_n);
  EXPECT_FALSE(fuzzy_pinyin.s_sh);
  EXPECT_FALSE(fuzzy_pinyin.z_zh);
  EXPECT_EQ(pinyin_settings.layout, mojom::PinyinLayout::kColemak);
  EXPECT_FALSE(pinyin_settings.use_comma_and_period_to_page_candidates);
  EXPECT_FALSE(pinyin_settings.use_hyphen_and_equals_to_page_candidates);
  EXPECT_FALSE(pinyin_settings.default_to_chinese);
  EXPECT_TRUE(pinyin_settings.default_to_full_width_characters);
  EXPECT_FALSE(pinyin_settings.default_to_full_width_punctuation);
}

TEST(CreateSettingsFromPrefsTest, CreateZhuyinSettingsDefault) {
  base::DictionaryValue dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings =
      CreateSettingsFromPrefs(prefs, kZhuyinEngineId, InputFieldContext{});

  ASSERT_TRUE(settings->is_zhuyin_settings());
  const auto& zhuyin_settings = *settings->get_zhuyin_settings();
  EXPECT_EQ(zhuyin_settings.layout, mojom::ZhuyinLayout::kStandard);
  EXPECT_EQ(zhuyin_settings.selection_keys,
            mojom::ZhuyinSelectionKeys::k1234567890);
  EXPECT_EQ(zhuyin_settings.page_size, 10u);
}

TEST(CreateSettingsFromPrefsTest, CreateZhuyinSettings) {
  base::DictionaryValue dict;
  dict.SetStringPath("zhuyin.zhuyinKeyboardLayout", "IBM");
  dict.SetStringPath("zhuyin.zhuyinSelectKeys", "asdfghjkl;");
  dict.SetStringPath("zhuyin.zhuyinPageSize", "8");
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings =
      CreateSettingsFromPrefs(prefs, kZhuyinEngineId, InputFieldContext{});

  ASSERT_TRUE(settings->is_zhuyin_settings());
  const auto& zhuyin_settings = *settings->get_zhuyin_settings();
  EXPECT_EQ(zhuyin_settings.layout, mojom::ZhuyinLayout::kIbm);
  EXPECT_EQ(zhuyin_settings.selection_keys,
            mojom::ZhuyinSelectionKeys::kAsdfghjkl);
  EXPECT_EQ(zhuyin_settings.page_size, 8u);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
