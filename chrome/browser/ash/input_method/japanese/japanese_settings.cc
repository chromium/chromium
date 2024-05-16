// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/japanese/japanese_settings.h"

#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/input_method/japanese/japanese_prefs_constants.h"
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"

namespace ash::input_method {
namespace {

using ::ash::ime::mojom::JapaneseSettings;
using ::ash::ime::mojom::JapaneseSettingsPtr;
using ::base::UmaHistogramBoolean;
using ::base::UmaHistogramCustomCounts;
using ::base::UmaHistogramEnumeration;

constexpr auto kHistValInputModes =
    base::MakeFixedFlatMap<JapaneseSettings::InputMode, HistInputMode>({
        {JapaneseSettings::InputMode::kRomaji, HistInputMode::kRomaji},
        {JapaneseSettings::InputMode::kKana, HistInputMode::kKana},
    });

constexpr auto kHistValPunctuations =
    base::MakeFixedFlatMap<JapaneseSettings::PunctuationStyle,
                           HistPunctuationStyle>({
        {JapaneseSettings::PunctuationStyle::kKutenTouten,
         HistPunctuationStyle::kKutenTouten},
        {JapaneseSettings::PunctuationStyle::kCommaPeriod,
         HistPunctuationStyle::kCommaPeriod},
        {JapaneseSettings::PunctuationStyle::kKutenPeriod,
         HistPunctuationStyle::kKutenPeriod},
        {JapaneseSettings::PunctuationStyle::kCommaTouten,
         HistPunctuationStyle::kCommaTouten},
    });

constexpr auto kHistValSymbols =
    base::MakeFixedFlatMap<JapaneseSettings::SymbolStyle, HistSymbolStyle>({
        {JapaneseSettings::SymbolStyle::kCornerBracketMiddleDot,
         HistSymbolStyle::kCornerBracketMiddleDot},
        {JapaneseSettings::SymbolStyle::kSquareBracketSlash,
         HistSymbolStyle::kSquareBracketSlash},
        {JapaneseSettings::SymbolStyle::kCornerBracketSlash,
         HistSymbolStyle::kCornerBracketSlash},
        {JapaneseSettings::SymbolStyle::kSquareBracketMiddleDot,
         HistSymbolStyle::kSquareBracketMiddleDot},
    });

constexpr auto kHistValSpaceInputStyles =
    base::MakeFixedFlatMap<JapaneseSettings::SpaceInputStyle,
                           HistSpaceInputStyle>({
        {JapaneseSettings::SpaceInputStyle::kInputMode,
         HistSpaceInputStyle::kInputMode},
        {JapaneseSettings::SpaceInputStyle::kFullWidth,
         HistSpaceInputStyle::kFullWidth},
        {JapaneseSettings::SpaceInputStyle::kHalfWidth,
         HistSpaceInputStyle::kHalfWidth},
    });

constexpr auto kHistValSelectionShortcuts =
    base::MakeFixedFlatMap<JapaneseSettings::SelectionShortcut,
                           HistSelectionShortcut>({
        {JapaneseSettings::SelectionShortcut::kDigits123456789,
         HistSelectionShortcut::kDigits123456789},
        {JapaneseSettings::SelectionShortcut::kAsdfghjkl,
         HistSelectionShortcut::kAsdfghjkl},
        {JapaneseSettings::SelectionShortcut::kNoShortcut,
         HistSelectionShortcut::kNoShortcut},
    });

constexpr auto kHistValKeymapStyles =
    base::MakeFixedFlatMap<JapaneseSettings::KeymapStyle, HistKeymapStyle>({
        {JapaneseSettings::KeymapStyle::kCustom, HistKeymapStyle::kCustom},
        {JapaneseSettings::KeymapStyle::kAtok, HistKeymapStyle::kAtok},
        {JapaneseSettings::KeymapStyle::kMsime, HistKeymapStyle::kMsime},
        {JapaneseSettings::KeymapStyle::kKotoeri, HistKeymapStyle::kKotoeri},
        {JapaneseSettings::KeymapStyle::kMobile, HistKeymapStyle::kMobile},
        {JapaneseSettings::KeymapStyle::kChromeos, HistKeymapStyle::kChromeos},
    });

constexpr auto kHistValShiftKeyModeStyles =
    base::MakeFixedFlatMap<JapaneseSettings::ShiftKeyModeStyle,
                           HistShiftKeyModeStyle>({
        {JapaneseSettings::ShiftKeyModeStyle::kOff,
         HistShiftKeyModeStyle::kOff},
        {JapaneseSettings::ShiftKeyModeStyle::kAlphanumeric,
         HistShiftKeyModeStyle::kAlphanumeric},
        {JapaneseSettings::ShiftKeyModeStyle::kKatakana,
         HistShiftKeyModeStyle::kKatakana},
    });

constexpr auto kInputModes =
    base::MakeFixedFlatMap<std::string_view, JapaneseSettings::InputMode>({
        {kJpPrefInputModeRomaji, JapaneseSettings::InputMode::kRomaji},
        {kJpPrefInputModeKana, JapaneseSettings::InputMode::kKana},
    });

constexpr auto kPunctuations =
    base::MakeFixedFlatMap<std::string_view,
                           JapaneseSettings::PunctuationStyle>({
        {kJpPrefPunctuationStyleKutenTouten,
         JapaneseSettings::PunctuationStyle::kKutenTouten},
        {kJpPrefPunctuationStyleCommaPeriod,
         JapaneseSettings::PunctuationStyle::kCommaPeriod},
        {kJpPrefPunctuationStyleKutenPeriod,
         JapaneseSettings::PunctuationStyle::kKutenPeriod},
        {kJpPrefPunctuationStyleCommaTouten,
         JapaneseSettings::PunctuationStyle::kCommaTouten},
    });

constexpr auto kSymbols =
    base::MakeFixedFlatMap<std::string_view, JapaneseSettings::SymbolStyle>({
        {kJpPrefSymbolStyleCornerBracketMiddleDot,
         JapaneseSettings::SymbolStyle::kCornerBracketMiddleDot},
        {kJpPrefSymbolStyleSquareBracketSlash,
         JapaneseSettings::SymbolStyle::kSquareBracketSlash},
        {kJpPrefSymbolStyleCornerBracketSlash,
         JapaneseSettings::SymbolStyle::kCornerBracketSlash},
        {kJpPrefSymbolStyleSquareBracketMiddleDot,
         JapaneseSettings::SymbolStyle::kSquareBracketMiddleDot},
    });

constexpr auto kSpaceInputStyles =
    base::MakeFixedFlatMap<std::string_view, JapaneseSettings::SpaceInputStyle>(
        {
            {kJpPrefSpaceInputStyleInputMode,
             JapaneseSettings::SpaceInputStyle::kInputMode},
            {kJpPrefSpaceInputStyleFullwidth,
             JapaneseSettings::SpaceInputStyle::kFullWidth},
            {kJpPrefSpaceInputStyleHalfwidth,
             JapaneseSettings::SpaceInputStyle::kHalfWidth},
        });

constexpr auto kSelectionShortcuts =
    base::MakeFixedFlatMap<std::string_view,
                           JapaneseSettings::SelectionShortcut>({
        {kJpPrefSelectionShortcutDigits123456789,
         JapaneseSettings::SelectionShortcut::kDigits123456789},
        {kJpPrefSelectionShortcutAsdfghjkl,
         JapaneseSettings::SelectionShortcut::kAsdfghjkl},
        {kJpPrefSelectionShortcutNoShortcut,
         JapaneseSettings::SelectionShortcut::kNoShortcut},
    });

constexpr auto kKeymapStyles =
    base::MakeFixedFlatMap<std::string_view, JapaneseSettings::KeymapStyle>({
        {kJpPrefKeymapStyleCustom, JapaneseSettings::KeymapStyle::kCustom},
        {kJpPrefKeymapStyleAtok, JapaneseSettings::KeymapStyle::kAtok},
        {kJpPrefKeymapStyleMsIme, JapaneseSettings::KeymapStyle::kMsime},
        {kJpPrefKeymapStyleKotoeri, JapaneseSettings::KeymapStyle::kKotoeri},
        {kJpPrefKeymapStyleMobile, JapaneseSettings::KeymapStyle::kMobile},
        {kJpPrefKeymapStyleChromeOs, JapaneseSettings::KeymapStyle::kChromeos},
    });

constexpr auto kShiftKeyModeStyle =
    base::MakeFixedFlatMap<std::string_view,
                           JapaneseSettings::ShiftKeyModeStyle>({
        {kJpPrefShiftKeyModeStyleOff,
         JapaneseSettings::ShiftKeyModeStyle::kOff},
        {kJpPrefShiftKeyModeStyleAlphanumeric,
         JapaneseSettings::ShiftKeyModeStyle::kAlphanumeric},
        {kJpPrefShiftKeyModeStyleKatakana,
         JapaneseSettings::ShiftKeyModeStyle::kKatakana},
    });

// Makes default based on what drop down menu in settings page will show if
// unset.
JapaneseSettingsPtr MakeDefaultJapaneseSettings() {
  JapaneseSettingsPtr response = JapaneseSettings::New();
  // LINT.IfChange(JpPrefDefaults)
  response->automatically_switch_to_halfwidth = true;
  response->shift_key_mode_style =
      JapaneseSettings::ShiftKeyModeStyle::kAlphanumeric;
  response->use_input_history = true;
  response->use_system_dictionary = true;
  response->number_of_suggestions = 3;
  response->input_mode = JapaneseSettings::InputMode::kRomaji;
  response->punctuation_style =
      JapaneseSettings::PunctuationStyle::kKutenTouten;
  response->symbol_style =
      JapaneseSettings::SymbolStyle::kCornerBracketMiddleDot;
  response->space_input_style = JapaneseSettings::SpaceInputStyle::kInputMode;
  response->selection_shortcut =
      JapaneseSettings::SelectionShortcut::kDigits123456789;
  response->keymap_style = JapaneseSettings::KeymapStyle::kCustom;
  response->disable_personalized_suggestions = true;
  response->automatically_send_statistics_to_google = true;
  // LINT.ThenChange(/chrome/browser/resources/ash/settings/os_languages_page/input_method_util.ts:JpPrefDefaults)
  return response;
}

}  // namespace

JapaneseSettingsPtr ToMojomInputMethodSettings(const base::Value::Dict& prefs) {
  JapaneseSettingsPtr response = MakeDefaultJapaneseSettings();
  if (const std::string* val = prefs.FindString(kJpPrefInputMode);
      val != nullptr) {
    if (auto it = kInputModes.find(*val); it != kInputModes.end()) {
      response->input_mode = it->second;
    } else {
      LOG(ERROR) << "Value not found for " << *val;
    }
  }
  if (const std::string* val = prefs.FindString(kJpPrefPunctuationStyle);
      val != nullptr) {
    if (auto it = kPunctuations.find(*val); it != kPunctuations.end()) {
      response->punctuation_style = it->second;
    } else {
      LOG(ERROR) << "Value not found for " << *val;
    }
  }
  if (const std::string* val = prefs.FindString(kJpPrefSymbolStyle);
      val != nullptr) {
    if (auto it = kSymbols.find(*val); it != kSymbols.end()) {
      response->symbol_style = it->second;
    } else {
      LOG(ERROR) << "Value not found for " << *val;
    }
  }
  if (const std::string* val = prefs.FindString(kJpPrefSpaceInputStyle);
      val != nullptr) {
    if (auto it = kSpaceInputStyles.find(*val); it != kSpaceInputStyles.end()) {
      response->space_input_style = it->second;
    } else {
      LOG(ERROR) << "Value not found for " << *val;
    }
  }
  if (const std::string* val = prefs.FindString(kJpPrefSelectionShortcut);
      val != nullptr) {
    if (auto it = kSelectionShortcuts.find(*val);
        it != kSelectionShortcuts.end()) {
      response->selection_shortcut = it->second;
    } else {
      LOG(ERROR) << "Value not found for " << *val;
    }
  }
  if (const std::string* val = prefs.FindString(kJpPrefKeymapStyle);
      val != nullptr) {
    if (auto it = kKeymapStyles.find(*val); it != kKeymapStyles.end()) {
      response->keymap_style = it->second;
    } else {
      LOG(ERROR) << "Value not found for " << *val;
    }
  }
  if (const std::string* val = prefs.FindString(kJpPrefShiftKeyModeStyle);
      val != nullptr) {
    if (auto it = kShiftKeyModeStyle.find(*val);
        it != kShiftKeyModeStyle.end()) {
      response->shift_key_mode_style = it->second;
    } else {
      LOG(ERROR) << "Value not found for " << *val;
    }
  }

  response->use_input_history = prefs.FindBool(kJpPrefUseInputHistory)
                                    .value_or(response->use_input_history);
  response->use_system_dictionary =
      prefs.FindBool(kJpPrefUseSystemDictionary)
          .value_or(response->use_system_dictionary);
  response->number_of_suggestions =
      prefs.FindInt(kJpPrefNumberOfSuggestions)
          .value_or(response->number_of_suggestions);
  response->disable_personalized_suggestions =
      prefs.FindBool(kJpPrefDisablePersonalizedSuggestions)
          .value_or(response->disable_personalized_suggestions);
  response->automatically_send_statistics_to_google =
      prefs.FindBool(kJpPrefAutomaticallySendStatisticsToGoogle)
          .value_or(response->automatically_send_statistics_to_google);
  return response;
}

void RecordJapaneseSettingsMetrics(
    const ash::ime::mojom::JapaneseSettings& settings) {
  if (auto it = kHistValInputModes.find(settings.input_mode);
      it != kHistValInputModes.end()) {
    UmaHistogramEnumeration(
        "InputMethod.PhysicalKeyboard.Japanese.Settings.InputMode", it->second);
  }

  if (auto it = kHistValKeymapStyles.find(settings.keymap_style);
      it != kHistValKeymapStyles.end()) {
    UmaHistogramEnumeration(
        "InputMethod.PhysicalKeyboard.Japanese.Settings.KeymapStyle",
        it->second);
  }

  if (auto it = kHistValPunctuations.find(settings.punctuation_style);
      it != kHistValPunctuations.end()) {
    UmaHistogramEnumeration(
        "InputMethod.PhysicalKeyboard.Japanese.Settings.PunctuationStyle",
        it->second);
  }

  if (auto it = kHistValSelectionShortcuts.find(settings.selection_shortcut);
      it != kHistValSelectionShortcuts.end()) {
    UmaHistogramEnumeration(
        "InputMethod.PhysicalKeyboard.Japanese.Settings.SelectionShortcut",
        it->second);
  }

  if (auto it = kHistValShiftKeyModeStyles.find(settings.shift_key_mode_style);
      it != kHistValShiftKeyModeStyles.end()) {
    UmaHistogramEnumeration(
        "InputMethod.PhysicalKeyboard.Japanese.Settings.ShiftKeyModeStyle",
        it->second);
  }

  if (auto it = kHistValSpaceInputStyles.find(settings.space_input_style);
      it != kHistValSpaceInputStyles.end()) {
    UmaHistogramEnumeration(
        "InputMethod.PhysicalKeyboard.Japanese.Settings.SpaceInputStyle",
        it->second);
  }

  if (auto it = kHistValSymbols.find(settings.symbol_style);
      it != kHistValSymbols.end()) {
    UmaHistogramEnumeration(
        "InputMethod.PhysicalKeyboard.Japanese.Settings.SymbolStyle",
        it->second);
  }

  UmaHistogramCustomCounts(
      /*name*/
      "InputMethod.PhysicalKeyboard.Japanese.Settings.NumberOfSuggestions",
      /*sample=*/settings.number_of_suggestions,
      /*min=*/1,
      /*exclusive_max=*/10,
      /*buckets=*/10);

  UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.Japanese.Settings."
      "AutoSwitchToHalfwidth",
      settings.automatically_switch_to_halfwidth);
  UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.Japanese.Settings."
      "DisablePersonalizedSuggestions",
      settings.disable_personalized_suggestions);
  UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.Japanese.Settings."
      "UseInputHistory",
      settings.use_input_history);
  UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.Japanese.Settings."
      "UseSystemDictionary",
      settings.use_system_dictionary);
}

}  // namespace ash::input_method
