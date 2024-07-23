// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_settings.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/strings/strcat.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom-shared.h"
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
constexpr char kJapaneseEngineId[] = "nacl_mozc_jp";

constexpr char kVietnameseVniEngineId[] = "vkd_vi_vni";
constexpr char kVietnameseTelexEngineId[] = "vkd_vi_telex";

void RegisterTestingPrefs(TestingPrefServiceSimple& prefs,
                          const base::Value::Dict& dict) {
  prefs.registry()->RegisterDictionaryPref(
      ::prefs::kLanguageInputMethodSpecificSettings);
  prefs.Set(::prefs::kLanguageInputMethodSpecificSettings,
            base::Value(dict.Clone()));
}

TEST(CreateSettingsFromPrefsTest, CreateLatinSettingsDefault) {
  base::Value::Dict dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId);

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_FALSE(latin_settings.autocorrect);
  EXPECT_TRUE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, CreateLatinSettingsWithMultiwordEnabled) {
  base::test::ScopedFeatureList features;
  TestingPrefServiceSimple prefs;
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kUsEnglishEngineId,
                                     ".physicalKeyboardAutoCorrectionLevel"}),
                       1);
  dict.SetByDottedPath(
      base::StrCat(
          {kUsEnglishEngineId, ".physicalKeyboardEnablePredictiveWriting"}),
      base::Value(true));
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId);

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_TRUE(latin_settings.autocorrect);
  EXPECT_TRUE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, CreateLatinSettingsWithMultiwordDisabled) {
  base::test::ScopedFeatureList features;
  TestingPrefServiceSimple prefs;
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kUsEnglishEngineId,
                                     ".physicalKeyboardAutoCorrectionLevel"}),
                       1);
  dict.SetByDottedPath(
      base::StrCat(
          {kUsEnglishEngineId, ".physicalKeyboardEnablePredictiveWriting"}),
      base::Value(false));
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId);

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_TRUE(latin_settings.autocorrect);
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest,
     PredictiveWritingEnabledWhenMultiWordAllowedAndEnabled) {
  base::test::ScopedFeatureList features;
  TestingPrefServiceSimple prefs;
  base::Value::Dict dict;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId);

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_TRUE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest,
     PredictiveWritingDisabledWhenMultiwordDisabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({}, {features::kAssistMultiWord});
  TestingPrefServiceSimple prefs;
  base::Value::Dict dict;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kUsEnglishEngineId);

  ASSERT_TRUE(settings->is_latin_settings());
  const auto& latin_settings = *settings->get_latin_settings();
  EXPECT_FALSE(latin_settings.predictive_writing);
}

TEST(CreateSettingsFromPrefsTest, CreateKoreanSettingsDefault) {
  base::Value::Dict dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kKoreanEngineId);

  ASSERT_TRUE(settings->is_korean_settings());
  const auto& korean_settings = *settings->get_korean_settings();
  EXPECT_EQ(korean_settings.layout, mojom::KoreanLayout::kDubeolsik);
  EXPECT_FALSE(korean_settings.input_multiple_syllables);
}

TEST(CreateSettingsFromPrefsTest, CreateKoreanSettings) {
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kKoreanEngineId, ".koreanKeyboardLayout"}),
                       "3 Set (390) / 세벌식 (390)");
  dict.SetByDottedPath(
      base::StrCat({kKoreanEngineId, ".koreanEnableSyllableInput"}), false);
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kKoreanEngineId);

  ASSERT_TRUE(settings->is_korean_settings());
  const auto& korean_settings = *settings->get_korean_settings();
  EXPECT_EQ(korean_settings.layout, mojom::KoreanLayout::kSebeolsik390);
  EXPECT_TRUE(korean_settings.input_multiple_syllables);
}

TEST(CreateSettingsFromPrefsTest, CreatePinyinSettingsDefault) {
  base::Value::Dict dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kPinyinEngineId);

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
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kPinyinEngineId, ".en:eng"}), true);
  dict.SetByDottedPath(base::StrCat({kPinyinEngineId, ".k:g"}), true);
  dict.SetByDottedPath(base::StrCat({kPinyinEngineId, ".in:ing"}), true);
  dict.SetByDottedPath(base::StrCat({kPinyinEngineId, ".xkbLayout"}),
                       "Colemak");
  dict.SetByDottedPath(
      base::StrCat({kPinyinEngineId, ".pinyinEnableLowerPaging"}), false);
  dict.SetByDottedPath(
      base::StrCat({kPinyinEngineId, ".pinyinEnableUpperPaging"}), false);
  dict.SetByDottedPath(base::StrCat({kPinyinEngineId, ".pinyinDefaultChinese"}),
                       false);
  dict.SetByDottedPath(
      base::StrCat({kPinyinEngineId, ".pinyinFullWidthCharacter"}), true);
  dict.SetByDottedPath(
      base::StrCat({kPinyinEngineId, ".pinyinChinesePunctuation"}), false);
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kPinyinEngineId);

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
  base::Value::Dict dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kZhuyinEngineId);

  ASSERT_TRUE(settings->is_zhuyin_settings());
  const auto& zhuyin_settings = *settings->get_zhuyin_settings();
  EXPECT_EQ(zhuyin_settings.layout, mojom::ZhuyinLayout::kStandard);
  EXPECT_EQ(zhuyin_settings.selection_keys,
            mojom::ZhuyinSelectionKeys::k1234567890);
  EXPECT_EQ(zhuyin_settings.page_size, 10u);
}

TEST(CreateSettingsFromPrefsTest, CreateVietnameseVniSettings) {
  base::Value::Dict dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kVietnameseVniEngineId);

  ASSERT_TRUE(settings->is_vietnamese_vni_settings());
  const auto& vni_settings = *settings->get_vietnamese_vni_settings();
  EXPECT_TRUE(vni_settings.allow_flexible_diacritics);
}

TEST(CreateSettingsFromPrefsTest, CreateVietnameseTelexSettings) {
  base::Value::Dict dict;
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings =
      CreateSettingsFromPrefs(prefs, kVietnameseTelexEngineId);

  ASSERT_TRUE(settings->is_vietnamese_telex_settings());
  const auto& telex_settings = *settings->get_vietnamese_telex_settings();
  EXPECT_TRUE(telex_settings.allow_flexible_diacritics);
}

TEST(CreateSettingsFromPrefsTest, CreateZhuyinSettings) {
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".zhuyinKeyboardLayout"}),
                       "IBM");
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".zhuyinSelectKeys"}),
                       "asdfghjkl;");
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".zhuyinPageSize"}), "8");
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  const auto settings = CreateSettingsFromPrefs(prefs, kZhuyinEngineId);

  ASSERT_TRUE(settings->is_zhuyin_settings());
  const auto& zhuyin_settings = *settings->get_zhuyin_settings();
  EXPECT_EQ(zhuyin_settings.layout, mojom::ZhuyinLayout::kIbm);
  EXPECT_EQ(zhuyin_settings.selection_keys,
            mojom::ZhuyinSelectionKeys::kAsdfghjkl);
  EXPECT_EQ(zhuyin_settings.page_size, 8u);
}

TEST(CreateSettingsFromPrefsTest, CreateJapaneseSettings) {
  using ::ash::ime::mojom::JapaneseSettings;

  base::Value::Dict jp_prefs;
  jp_prefs.Set("AutomaticallySendStatisticsToGoogle", false);
  jp_prefs.Set("AutomaticallySwitchToHalfwidth", false);
  jp_prefs.Set("JapaneseDisableSuggestions", true);
  jp_prefs.Set("JapaneseInputMode", "Kana");
  jp_prefs.Set("JapaneseKeymapStyle", "ChromeOs");
  jp_prefs.Set("JapanesePunctuationStyle", "CommaPeriod");
  jp_prefs.Set("JapaneseSectionShortcut", "ASDFGHJKL");
  jp_prefs.Set("JapaneseSpaceInputStyle", "Fullwidth");
  jp_prefs.Set("JapaneseSymbolStyle", "SquareBracketMiddleDot");
  jp_prefs.Set("ShiftKeyModeStyle", "Off");
  jp_prefs.Set("UseInputHistory", false);
  jp_prefs.Set("UseSystemDictionary", false);
  jp_prefs.Set("numberOfSuggestions", 5);

  base::Value::Dict full_prefs;
  full_prefs.Set(kJapaneseEngineId, std::move(jp_prefs));
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, full_prefs);

  const mojom::InputMethodSettingsPtr settings =
      CreateSettingsFromPrefs(prefs, kJapaneseEngineId);

  ASSERT_TRUE(settings->is_japanese_settings());
  mojom::JapaneseSettingsPtr expected = mojom::JapaneseSettings::New();
  expected->automatically_send_statistics_to_google = false;
  expected->automatically_switch_to_halfwidth = true;
  expected->disable_personalized_suggestions = true;
  expected->input_mode = JapaneseSettings::InputMode::kKana;
  expected->keymap_style = JapaneseSettings::KeymapStyle::kChromeos;
  expected->punctuation_style =
      JapaneseSettings::PunctuationStyle::kCommaPeriod;
  expected->selection_shortcut =
      JapaneseSettings::SelectionShortcut::kAsdfghjkl;
  expected->space_input_style = JapaneseSettings::SpaceInputStyle::kFullWidth;
  expected->symbol_style =
      JapaneseSettings::SymbolStyle::kSquareBracketMiddleDot;
  expected->shift_key_mode_style = JapaneseSettings::ShiftKeyModeStyle::kOff;
  expected->use_input_history = false;
  expected->use_system_dictionary = false;
  expected->number_of_suggestions = 5;
  EXPECT_EQ(settings->get_japanese_settings(), expected);
}

TEST(CreateSettingsFromPrefsTest, AutocorrectIsSupportedForLatin) {
  ASSERT_TRUE(IsAutocorrectSupported("xkb:ca:multix:fra"));
  ASSERT_TRUE(IsAutocorrectSupported("xkb:de::ger"));
  ASSERT_TRUE(IsAutocorrectSupported("xkb:us::eng"));
  ASSERT_TRUE(IsAutocorrectSupported("xkb:us:intl_pc:por"));
  ASSERT_TRUE(IsAutocorrectSupported("xkb:us:workman-intl:eng"));
}

TEST(CreateSettingsFromPrefsTest, AutocorrectIsNotSupportedForNonLatin) {
  ASSERT_FALSE(IsAutocorrectSupported("ko-t-i0-und"));
  ASSERT_FALSE(IsAutocorrectSupported("nacl_mozc_us"));
  ASSERT_FALSE(IsAutocorrectSupported("xkb:am:phonetic:arm"));
  ASSERT_FALSE(IsAutocorrectSupported("xkb:in::eng"));
  ASSERT_FALSE(IsAutocorrectSupported("zh-hant-t-i0-pinyin"));
  ASSERT_FALSE(IsAutocorrectSupported("zh-hant-t-i0-und"));
  ASSERT_FALSE(IsAutocorrectSupported("zh-t-i0-pinyin"));
}

TEST(InputMethodSettingsTest, GetLanguageSpecificInputMethodSettings) {
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field1"}), "DEFAULT1");
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field2"}), "DEFAULT2");
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field3"}), "DEFAULT3");
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  base::Value::Dict new_prefs;
  new_prefs.Set("field2", "CHANGED");
  EXPECT_EQ(
      *GetLanguageInputMethodSpecificSetting(prefs, kZhuyinEngineId, "field1"),
      "DEFAULT1");
  EXPECT_EQ(
      *GetLanguageInputMethodSpecificSetting(prefs, kZhuyinEngineId, "field2"),
      "DEFAULT2");
  EXPECT_EQ(
      *GetLanguageInputMethodSpecificSetting(prefs, kZhuyinEngineId, "field3"),
      "DEFAULT3");
}

TEST(InputMethodSettingsTest,
     SetLanguageInputMethodSpecificSettingExistingEngine) {
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field1"}), "DEFAULT");
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field2"}), "DEFAULT");
  dict.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field3"}), "DEFAULT");
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  base::Value::Dict new_prefs;
  new_prefs.Set("field2", "CHANGED");
  SetLanguageInputMethodSpecificSetting(prefs, kZhuyinEngineId, new_prefs);

  const base::Value* prefs_val =
      prefs.GetUserPref(::prefs::kLanguageInputMethodSpecificSettings);

  base::Value::Dict expected;
  expected.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field1"}),
                           "DEFAULT");
  expected.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field2"}),
                           "CHANGED");
  expected.SetByDottedPath(base::StrCat({kZhuyinEngineId, ".field3"}),
                           "DEFAULT");

  EXPECT_EQ(*prefs_val->GetIfDict(), expected);
}

TEST(InputMethodSettingsTest, SetLanguageInputMethodSpecificSettingNewEngine) {
  base::Value::Dict dict;
  dict.SetByDottedPath("existing-engine.field1", "DEFAULT");
  TestingPrefServiceSimple prefs;
  RegisterTestingPrefs(prefs, dict);

  base::Value::Dict new_prefs;
  new_prefs.Set("field1", "NEW");
  SetLanguageInputMethodSpecificSetting(prefs, "brand-new-engine", new_prefs);

  const base::Value* prefs_val =
      prefs.GetUserPref(::prefs::kLanguageInputMethodSpecificSettings);

  base::Value::Dict expected;
  expected.SetByDottedPath("existing-engine.field1", "DEFAULT");
  expected.SetByDottedPath("brand-new-engine.field1", "NEW");

  EXPECT_EQ(*prefs_val->GetIfDict(), expected);
}

}  // namespace
}  // namespace input_method
}  // namespace ash
