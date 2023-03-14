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
#include "chromeos/ash/services/ime/public/mojom/japanese_settings.mojom.h"
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

// This should be kept in sync with the values on the settings page's
// InputMethodOptions. This should match
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js;l=71-88;drc=6c88edbfe6096489ccac66b3ef5c84d479892181.
constexpr char kJapaneseInputMode[] = "JapaneseInputMode";
constexpr char kJapanesePunctuationStyle[] = "JapanesePunctuationStyle";
constexpr char kJapaneseSymbolStyle[] = "JapaneseSymbolStyle";
constexpr char kJapaneseSpaceInputStyle[] = "JapaneseSpaceInputStyle";
constexpr char kJapaneseSelectionShortcut[] = "JapaneseSectionShortcut";
constexpr char kJapaneseKeymapStyle[] = "JapaneseKeymapStyle";
constexpr char kJapaneseUseInputHistory[] = "UseInputHistory";
constexpr char kJapaneseUseSystemDictionary[] = "UseSystemDictionary";
constexpr char kJapaneseNumberOfSuggestions[] = "numberOfSuggestions";
constexpr char kJapaneseAutomaticallySwitchToHalfwidth[] =
    "AutomaticallySwitchToHalfwidth";
constexpr char kJapaneseShiftKeyModeStyle[] = "ShiftKeyModeStyle";
constexpr char kJapaneseDisablePersonalizedSuggestions[] =
    "JapaneseDisableSuggestions";
constexpr char kJapaneseAutomaticallySendStatisticsToGoogle[] =
    "AutomaticallySendStatisticsToGoogle";

// This should match the strings listed here:
//
// https://crsrc.org/c/chrome/browser/resources/settings/chromeos/os_languages_page/input_method_types.js;l=8-71;drc=7df206933530e6ac65a7e17a88757cbb780c829e
// These are possible values for their corresponding enum type.
constexpr char kJapaneseInputModeKana[] = "Kana";
constexpr char kJapaneseInputModeRomaji[] = "Romaji";
constexpr char kJapanesePunctuationStyleKutenTouten[] = "KutenTouten";
constexpr char kJapanesePunctuationStyleCommaPeriod[] = "CommaPeriod";
constexpr char kJapanesePunctuationStyleKutenPeriod[] = "KutenPeriod";
constexpr char kJapanesePunctuationStyleCommaTouten[] = "CommaTouten";
constexpr char kJapaneseSymbolStyleCornerBracketMiddleDot[] =
    "CornerBracketMiddleDot";
constexpr char kJapaneseSymbolStyleSquareBracketSlash[] = "SquareBracketSlash";
constexpr char kJapaneseSymbolStyleCornerBracketSlash[] = "CornerBracketSlash";
constexpr char kJapaneseSymbolStyleSquareBracketMiddleDot[] =
    "SquareBracketMiddleDot";
constexpr char kJapaneseSpaceInputStyleInputMode[] = "InputMode";
constexpr char kJapaneseSpaceInputStyleFullwidth[] = "Fullwidth";
constexpr char kJapaneseSpaceInputStyleHalfwidth[] = "Halfwidth";
constexpr char kJapaneseSelectionShortcutNoShortcut[] = "NoShortcut";
constexpr char kJapaneseSelectionShortcutDigits123456789[] = "Digits123456789";
constexpr char kJapaneseSelectionShortcutAsdfghjkl[] = "ASDFGHJKL";
constexpr char kJapaneseKeymapStyleCustom[] = "Custom";
constexpr char kJapaneseKeymapStyleAtok[] = "Atok";
constexpr char kJapaneseKeymapStyleMsIme[] = "MsIme";
constexpr char kJapaneseKeymapStyleKotoeri[] = "Kotoeri";
constexpr char kJapaneseKeymapStyleMobile[] = "Mobile";
constexpr char kJapaneseKeymapStyleChromeOs[] = "ChromeOs";
constexpr char kJapaneseShiftKeyModeStyleOff[] = "Off";
constexpr char kJapaneseShiftKeyModeStyleAlphanumeric[] = "Alphanumeric";
constexpr char kJapaneseShiftKeyModeStyleKatakana[] = "Katakana";

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
  EXPECT_FALSE(latin_settings.predictive_writing);
}

class SetJapanesePrefsFromSettingsTest : public ::testing::Test {
 protected:
  void SetUp() override { RegisterTestingPrefs(prefs, dict); }

 public:
  const base::Value::Dict* GetJapaneseDict() {
    return prefs.GetDict(::prefs::kLanguageInputMethodSpecificSettings)
        .FindDict(kJapaneseEngineId);
  }

  base::Value::Dict dict;
  TestingPrefServiceSimple prefs;
};

TEST_F(SetJapanesePrefsFromSettingsTest,
       JapaneseMigrationIsNotCompleteByDefault) {
  EXPECT_FALSE(IsJapaneseSettingsMigrationComplete(prefs));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseMigrationDone) {
  SetJapaneseSettingsMigrationComplete(prefs, true);

  EXPECT_TRUE(IsJapaneseSettingsMigrationComplete(prefs));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseMigrationToFalse) {
  SetJapaneseSettingsMigrationComplete(prefs, true);
  SetJapaneseSettingsMigrationComplete(prefs, false);

  EXPECT_FALSE(IsJapaneseSettingsMigrationComplete(prefs));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSettingsIfMigratedCausesDeath) {
  SetJapaneseSettingsMigrationComplete(prefs, true);
  mojom::JapaneseConfig config;

  EXPECT_CHECK_DEATH(MigrateJapaneseSettingsToPrefs(prefs, config));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSettingsWillMarkMigrationAsDone) {
  mojom::JapaneseConfig config;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));

  EXPECT_TRUE(IsJapaneseSettingsMigrationComplete(prefs));
}

// Test Japanese settings persistence
// Test input mode is set.

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseInputModeToRomaji) {
  mojom::JapaneseConfig config;
  config.input_mode = mojom::InputMode::kRomaji;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();

  EXPECT_EQ(*(result_dict->FindString(kJapaneseInputMode)),
            std::string(kJapaneseInputModeRomaji));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseInputModeToKana) {
  mojom::JapaneseConfig config;
  config.input_mode = mojom::InputMode::kKana;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseInputMode)),
            std::string(kJapaneseInputModeKana));
}

// Test Punctuation style is set.
TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapanesePunctuationStyleToKutenTouten) {
  mojom::JapaneseConfig config;
  config.punctuation_style = mojom::PunctuationStyle::kKutenTouten;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapanesePunctuationStyle)),
            std::string(kJapanesePunctuationStyleKutenTouten));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapanesePunctuationStyleToCommaPeriod) {
  mojom::JapaneseConfig config;
  config.punctuation_style = mojom::PunctuationStyle::kCommaPeriod;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapanesePunctuationStyle)),
            std::string(kJapanesePunctuationStyleCommaPeriod));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapanesePunctuationStyleToKutenPeriod) {
  mojom::JapaneseConfig config;
  config.punctuation_style = mojom::PunctuationStyle::kKutenPeriod;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapanesePunctuationStyle)),
            std::string(kJapanesePunctuationStyleKutenPeriod));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapanesePunctuationStyleToCommaTouten) {
  mojom::JapaneseConfig config;
  config.punctuation_style = mojom::PunctuationStyle::kCommaTouten;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapanesePunctuationStyle)),
            std::string(kJapanesePunctuationStyleCommaTouten));
}

// Test symbol style is set.

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSymbolStyleToCornerBracketMiddleDot) {
  mojom::JapaneseConfig config;
  config.symbol_style = mojom::SymbolStyle::kCornerBracketMiddleDot;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSymbolStyle)),
            std::string(kJapaneseSymbolStyleCornerBracketMiddleDot));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSymbolStyleToSquareBracketSlash) {
  mojom::JapaneseConfig config;
  config.symbol_style = mojom::SymbolStyle::kSquareBracketSlash;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSymbolStyle)),
            std::string(kJapaneseSymbolStyleSquareBracketSlash));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSymbolStyleToCornerBracketSlash) {
  mojom::JapaneseConfig config;
  config.symbol_style = mojom::SymbolStyle::kCornerBracketSlash;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSymbolStyle)),
            std::string(kJapaneseSymbolStyleCornerBracketSlash));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSymbolStyleToSquareBracketMiddleDot) {
  mojom::JapaneseConfig config;
  config.symbol_style = mojom::SymbolStyle::kSquareBracketMiddleDot;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSymbolStyle)),
            std::string(kJapaneseSymbolStyleSquareBracketMiddleDot));
}

// Test setting space input style.

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSpaceInputStyleToInputMode) {
  mojom::JapaneseConfig config;
  config.space_input_style = mojom::SpaceInputStyle::kInputMode;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSpaceInputStyle)),
            std::string(kJapaneseSpaceInputStyleInputMode));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSpaceInputStyleToFullwidth) {
  mojom::JapaneseConfig config;
  config.space_input_style = mojom::SpaceInputStyle::kFullwidth;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSpaceInputStyle)),
            std::string(kJapaneseSpaceInputStyleFullwidth));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSpaceInputStyleToHalfwidth) {
  mojom::JapaneseConfig config;
  config.space_input_style = mojom::SpaceInputStyle::kHalfwidth;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSpaceInputStyle)),
            std::string(kJapaneseSpaceInputStyleHalfwidth));
}

// Test Japanese selection shortcut.

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSelectionShortcutToNoShortcut) {
  mojom::JapaneseConfig config;
  config.selection_shortcut = mojom::SelectionShortcut::kNoShortcut;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSelectionShortcut)),
            std::string(kJapaneseSelectionShortcutNoShortcut));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSelectionShortcutToDigits123456789) {
  mojom::JapaneseConfig config;
  config.selection_shortcut = mojom::SelectionShortcut::kDigits123456789;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSelectionShortcut)),
            std::string(kJapaneseSelectionShortcutDigits123456789));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseSelectionShortcutToAsdfghjkl) {
  mojom::JapaneseConfig config;
  config.selection_shortcut = mojom::SelectionShortcut::kAsdfghjkl;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseSelectionShortcut)),
            std::string(kJapaneseSelectionShortcutAsdfghjkl));
}

// Test Japanese keymap style.

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseKeymapStyleToCustom) {
  mojom::JapaneseConfig config;
  config.keymap_style = mojom::KeymapStyle::kCustom;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseKeymapStyle)),
            std::string(kJapaneseKeymapStyleCustom));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseKeymapStyleToAtok) {
  mojom::JapaneseConfig config;
  config.keymap_style = mojom::KeymapStyle::kAtok;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseKeymapStyle)),
            std::string(kJapaneseKeymapStyleAtok));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseKeymapStyleToMsIme) {
  mojom::JapaneseConfig config;
  config.keymap_style = mojom::KeymapStyle::kMsIme;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseKeymapStyle)),
            std::string(kJapaneseKeymapStyleMsIme));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseKeymapStyleToKotoeri) {
  mojom::JapaneseConfig config;
  config.keymap_style = mojom::KeymapStyle::kKotoeri;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseKeymapStyle)),
            std::string(kJapaneseKeymapStyleKotoeri));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseKeymapStyleToMobile) {
  mojom::JapaneseConfig config;
  config.keymap_style = mojom::KeymapStyle::kMobile;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseKeymapStyle)),
            std::string(kJapaneseKeymapStyleMobile));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseKeymapStyleToChromeOs) {
  mojom::JapaneseConfig config;
  config.keymap_style = mojom::KeymapStyle::kChromeOs;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseKeymapStyle)),
            std::string(kJapaneseKeymapStyleChromeOs));
}

// Test automatically switch to halfwidth.

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetAutomaticallySwitchToHalfwitdhToTrue) {
  mojom::JapaneseConfig config;
  config.automatically_switch_to_halfwidth = true;
  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_TRUE(*result_dict->FindBool(kJapaneseAutomaticallySwitchToHalfwidth));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetAutomaticallySwitchToHalfwitdhToFalse) {
  mojom::JapaneseConfig config;
  config.automatically_switch_to_halfwidth = false;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_FALSE(*result_dict->FindBool(kJapaneseAutomaticallySwitchToHalfwidth));
}

// Test Japanese shift key mode switch.

TEST_F(SetJapanesePrefsFromSettingsTest, SetJapaneseShiftKeyModeStyleToOff) {
  mojom::JapaneseConfig config;
  config.shift_key_mode_switch = mojom::ShiftKeyModeSwitch::kOff;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseShiftKeyModeStyle)),
            std::string(kJapaneseShiftKeyModeStyleOff));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseShiftKeyModeStyleToAlphanumeric) {
  mojom::JapaneseConfig config;
  config.shift_key_mode_switch = mojom::ShiftKeyModeSwitch::kAlphanumeric;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseShiftKeyModeStyle)),
            std::string(kJapaneseShiftKeyModeStyleAlphanumeric));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetJapaneseShiftKeyModeStyleToKatakana) {
  mojom::JapaneseConfig config;
  config.shift_key_mode_switch = mojom::ShiftKeyModeSwitch::kKatakana;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*(result_dict->FindString(kJapaneseShiftKeyModeStyle)),
            std::string(kJapaneseShiftKeyModeStyleKatakana));
}

// Test Japanese use input history.

TEST_F(SetJapanesePrefsFromSettingsTest, SetUseInputHistoryToTrue) {
  mojom::JapaneseConfig config;
  config.use_input_history = true;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_TRUE(*result_dict->FindBool(kJapaneseUseInputHistory));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetUseInputHistoryToFalse) {
  mojom::JapaneseConfig config;
  config.use_input_history = false;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_FALSE(*result_dict->FindBool(kJapaneseUseInputHistory));
}

// Test use system dictionary.

TEST_F(SetJapanesePrefsFromSettingsTest, SetUseSystemDictionaryToTrue) {
  mojom::JapaneseConfig config;
  config.use_system_dictionary = true;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_TRUE(*result_dict->FindBool(kJapaneseUseSystemDictionary));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetUseSystemDictionaryToFalse) {
  mojom::JapaneseConfig config;
  config.use_system_dictionary = false;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_FALSE(*result_dict->FindBool(kJapaneseUseSystemDictionary));
}

// Test number of suggestions.

TEST_F(SetJapanesePrefsFromSettingsTest, SetNumberOfSuggestionsTo1) {
  mojom::JapaneseConfig config;
  config.number_of_suggestions = 1;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*result_dict->FindInt(kJapaneseNumberOfSuggestions), 1);
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetNumberOfSuggestionsTo5) {
  mojom::JapaneseConfig config;
  config.number_of_suggestions = 5;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*result_dict->FindInt(kJapaneseNumberOfSuggestions), 5);
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetNumberOfSuggestionsTo9) {
  mojom::JapaneseConfig config;
  config.number_of_suggestions = 9;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_EQ(*result_dict->FindInt(kJapaneseNumberOfSuggestions), 9);
}
// Test disable personalized suggestions.

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetDisablePersonalizedSuggestionsToTrue) {
  mojom::JapaneseConfig config;
  config.disable_personalized_suggestions = true;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_TRUE(*result_dict->FindBool(kJapaneseDisablePersonalizedSuggestions));
}

TEST_F(SetJapanesePrefsFromSettingsTest,
       SetDisablePersonalizedSuggestionsToFalse) {
  mojom::JapaneseConfig config;
  config.disable_personalized_suggestions = false;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_FALSE(*result_dict->FindBool(kJapaneseDisablePersonalizedSuggestions));
}

// Test sending statistics to Google.

TEST_F(SetJapanesePrefsFromSettingsTest, SetSendStatisticsToGoogleToTrue) {
  mojom::JapaneseConfig config;
  config.send_statistics_to_google = true;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_TRUE(
      *result_dict->FindBool(kJapaneseAutomaticallySendStatisticsToGoogle));
}

TEST_F(SetJapanesePrefsFromSettingsTest, SetSendStatisticsToGoogleToFalse) {
  mojom::JapaneseConfig config;
  config.send_statistics_to_google = false;

  MigrateJapaneseSettingsToPrefs(prefs, std::move(config));
  const base::Value::Dict* result_dict = GetJapaneseDict();
  EXPECT_FALSE(
      *result_dict->FindBool(kJapaneseAutomaticallySendStatisticsToGoogle));
}

// End of Japanese tests

TEST(CreateSettingsFromPrefsTest, CreateLatinSettings) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAssistMultiWord}, {});
  TestingPrefServiceSimple prefs;
  base::Value::Dict dict;
  dict.SetByDottedPath(base::StrCat({kUsEnglishEngineId,
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

TEST(CreateSettingsFromPrefsTest,
     PredictiveWritingEnabledWhenMultiWordAllowedAndEnabled) {
  base::test::ScopedFeatureList features;
  features.InitWithFeatures({features::kAssistMultiWord}, {});
  TestingPrefServiceSimple prefs;
  base::Value::Dict dict;
  RegisterTestingPrefs(prefs, dict);
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

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
  prefs.registry()->RegisterBooleanPref(prefs::kAssistPredictiveWritingEnabled,
                                        true);

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

}  // namespace
}  // namespace input_method
}  // namespace ash
