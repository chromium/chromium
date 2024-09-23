// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/japanese/japanese_settings.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ash/input_method/japanese/japanese_prefs_constants.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {
using ::ash::ime::mojom::JapaneseSettings;

TEST(JapaneseSettingsTest, OnSetPrefsSetsSettingsFromPrefs) {
  base::Value::Dict prefs;
  prefs.Set("AutomaticallySendStatisticsToGoogle", false);
  prefs.Set("AutomaticallySwitchToHalfwidth", false);
  prefs.Set("JapaneseDisableSuggestions", true);
  prefs.Set("JapaneseInputMode", "Kana");
  prefs.Set("JapaneseKeymapStyle", "ChromeOs");
  prefs.Set("JapanesePunctuationStyle", "CommaPeriod");
  prefs.Set("JapaneseSectionShortcut", "ASDFGHJKL");
  prefs.Set("JapaneseSpaceInputStyle", "Fullwidth");
  prefs.Set("JapaneseSymbolStyle", "SquareBracketMiddleDot");
  prefs.Set("ShiftKeyModeStyle", "Off");
  prefs.Set("UseInputHistory", false);
  prefs.Set("UseSystemDictionary", false);
  prefs.Set("numberOfSuggestions", 5);

  ash::ime::mojom::JapaneseSettingsPtr response =
      ToMojomInputMethodSettings(prefs);
  ash::ime::mojom::JapaneseSettingsPtr expected =
      ash::ime::mojom::JapaneseSettings::New();
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

  EXPECT_EQ(response, expected);
}

TEST(JapaneseSettingsTest, OnUnsetPrefsSetsDefault) {
  base::Value::Dict prefs;

  ash::ime::mojom::JapaneseSettingsPtr response =
      ToMojomInputMethodSettings(prefs);

  ash::ime::mojom::JapaneseSettingsPtr expected =
      ash::ime::mojom::JapaneseSettings::New();
  expected->automatically_switch_to_halfwidth = true;
  expected->shift_key_mode_style =
      JapaneseSettings::ShiftKeyModeStyle::kAlphanumeric;
  expected->use_input_history = true;
  expected->use_system_dictionary = true;
  expected->number_of_suggestions = 3;
  expected->input_mode = JapaneseSettings::InputMode::kRomaji;
  expected->punctuation_style =
      JapaneseSettings::PunctuationStyle::kKutenTouten;
  expected->symbol_style =
      JapaneseSettings::SymbolStyle::kCornerBracketMiddleDot;
  expected->space_input_style = JapaneseSettings::SpaceInputStyle::kInputMode;
  expected->selection_shortcut =
      JapaneseSettings::SelectionShortcut::kDigits123456789;
  expected->keymap_style = JapaneseSettings::KeymapStyle::kCustom;
  expected->disable_personalized_suggestions = true;
  expected->automatically_send_statistics_to_google = true;
  EXPECT_EQ(response, expected);
}

TEST(JapaneseSettingsTest, RecordJapaneseSettingsMetrics) {
  ash::ime::mojom::JapaneseSettingsPtr settings =
      ash::ime::mojom::JapaneseSettings::New();
  settings->automatically_switch_to_halfwidth = true;
  settings->disable_personalized_suggestions = true;
  settings->input_mode = JapaneseSettings::InputMode::kRomaji;
  settings->keymap_style = JapaneseSettings::KeymapStyle::kCustom;
  settings->number_of_suggestions = 3;
  settings->punctuation_style =
      JapaneseSettings::PunctuationStyle::kKutenTouten;
  settings->selection_shortcut =
      JapaneseSettings::SelectionShortcut::kDigits123456789;
  settings->shift_key_mode_style =
      JapaneseSettings::ShiftKeyModeStyle::kAlphanumeric;
  settings->space_input_style = JapaneseSettings::SpaceInputStyle::kInputMode;
  settings->symbol_style =
      JapaneseSettings::SymbolStyle::kCornerBracketMiddleDot;
  settings->use_input_history = false;
  settings->use_system_dictionary = false;

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.AutoSwitchToHalfwidth",
      0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings."
      "DisablePersonalizedSuggestions",
      0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.InputMode", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.KeymapStyle", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.NumberOfSuggestions", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.PunctuationStyle", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.SelectionShortcut", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.ShiftKeyModeStyle", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.SpaceInputStyle", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.SymbolStyle", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.UseInputHistory", 0);
  histogram_tester.ExpectTotalCount(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.UseSystemDictionary", 0);

  RecordJapaneseSettingsMetrics(*settings);

  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.AutoSwitchToHalfwidth",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings."
      "DisablePersonalizedSuggestions",
      true, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.InputMode",
      HistInputMode::kRomaji, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.KeymapStyle",
      HistKeymapStyle::kCustom, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.NumberOfSuggestions", 3,
      1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.PunctuationStyle",
      HistPunctuationStyle::kKutenTouten, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.SelectionShortcut",
      HistSelectionShortcut::kDigits123456789, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.ShiftKeyModeStyle",
      HistShiftKeyModeStyle::kAlphanumeric, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.SpaceInputStyle",
      HistSpaceInputStyle::kInputMode, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.SymbolStyle",
      HistSymbolStyle::kCornerBracketMiddleDot, 1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.UseInputHistory", false,
      1);
  histogram_tester.ExpectUniqueSample(
      "InputMethod.PhysicalKeyboard.Japanese.Settings.UseSystemDictionary",
      false, 1);
}
}  // namespace

}  // namespace ash::input_method
