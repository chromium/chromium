// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/japanese/japanese_legacy_config.h"

#include "base/containers/fixed_flat_map.h"
#include "base/values.h"
#include "chrome/browser/ash/input_method/japanese/japanese_prefs_constants.h"
#include "chromeos/ash/services/ime/public/mojom/user_data_japanese_legacy_config.mojom.h"

namespace ash::input_method {
namespace {
using ::ash::ime::mojom::JapaneseLegacyConfig;
using ::ash::ime::mojom::JapaneseLegacyConfigPtr;
// stuff
constexpr auto kPreedits =
    base::MakeFixedFlatMap<JapaneseLegacyConfig::PreeditMethod,
                           std::string_view>({
        {JapaneseLegacyConfig::PreeditMethod::kRomaji, kJpPrefInputModeRomaji},
        {JapaneseLegacyConfig::PreeditMethod::kKana, kJpPrefInputModeKana},
    });

constexpr auto kPunctuations =
    base::MakeFixedFlatMap<JapaneseLegacyConfig::PunctuationMethod,
                           std::string_view>({
        {JapaneseLegacyConfig::PunctuationMethod::kKutenTouten,
         kJpPrefPunctuationStyleKutenTouten},
        {JapaneseLegacyConfig::PunctuationMethod::kCommaPeriod,
         kJpPrefPunctuationStyleCommaPeriod},
        {JapaneseLegacyConfig::PunctuationMethod::kKutenPeriod,
         kJpPrefPunctuationStyleKutenPeriod},
        {JapaneseLegacyConfig::PunctuationMethod::kCommaTouten,
         kJpPrefPunctuationStyleCommaTouten},
    });

constexpr auto kSymbols =
    base::MakeFixedFlatMap<JapaneseLegacyConfig::SymbolMethod,
                           std::string_view>({
        {JapaneseLegacyConfig::SymbolMethod::kCornerBracketMiddleDot,
         kJpPrefSymbolStyleCornerBracketMiddleDot},
        {JapaneseLegacyConfig::SymbolMethod::kSquareBracketSlash,
         kJpPrefSymbolStyleSquareBracketSlash},
        {JapaneseLegacyConfig::SymbolMethod::kCornerBracketSlash,
         kJpPrefSymbolStyleCornerBracketSlash},
        {JapaneseLegacyConfig::SymbolMethod::kSquareBracketMiddleDot,
         kJpPrefSymbolStyleSquareBracketMiddleDot},
    });

constexpr auto kFundamentalCharacterForms =
    base::MakeFixedFlatMap<JapaneseLegacyConfig::FundamentalCharacterForm,
                           std::string_view>({
        {JapaneseLegacyConfig::FundamentalCharacterForm::kInputMode,
         kJpPrefSpaceInputStyleInputMode},
        {JapaneseLegacyConfig::FundamentalCharacterForm::kFullWidth,
         kJpPrefSpaceInputStyleFullwidth},
        {JapaneseLegacyConfig::FundamentalCharacterForm::kHalfWidth,
         kJpPrefSpaceInputStyleHalfwidth},
    });

constexpr auto kSelectionShortcuts =
    base::MakeFixedFlatMap<JapaneseLegacyConfig::SelectionShortcut,
                           std::string_view>({
        {JapaneseLegacyConfig::SelectionShortcut::k123456789,
         kJpPrefSelectionShortcutDigits123456789},
        {JapaneseLegacyConfig::SelectionShortcut::kAsdfghjkl,
         kJpPrefSelectionShortcutAsdfghjkl},
        {JapaneseLegacyConfig::SelectionShortcut::kNoShortcut,
         kJpPrefSelectionShortcutNoShortcut},
    });

constexpr auto kSessionKeymaps =
    base::MakeFixedFlatMap<JapaneseLegacyConfig::SessionKeymap,
                           std::string_view>({
        {JapaneseLegacyConfig::SessionKeymap::kCustom,
         kJpPrefKeymapStyleCustom},
        {JapaneseLegacyConfig::SessionKeymap::kAtok, kJpPrefKeymapStyleAtok},
        {JapaneseLegacyConfig::SessionKeymap::kMsime, kJpPrefKeymapStyleMsIme},
        {JapaneseLegacyConfig::SessionKeymap::kKotoeri,
         kJpPrefKeymapStyleKotoeri},
        {JapaneseLegacyConfig::SessionKeymap::kMobile,
         kJpPrefKeymapStyleMobile},
        {JapaneseLegacyConfig::SessionKeymap::kChromeos,
         kJpPrefKeymapStyleChromeOs},
    });

constexpr auto kShiftKeyModeSwitch =
    base::MakeFixedFlatMap<JapaneseLegacyConfig::ShiftKeyModeSwitch,
                           std::string_view>({
        {JapaneseLegacyConfig::ShiftKeyModeSwitch::kOff,
         kJpPrefShiftKeyModeStyleOff},
        {JapaneseLegacyConfig::ShiftKeyModeSwitch::kAsciiInputMode,
         kJpPrefShiftKeyModeStyleAlphanumeric},
        {JapaneseLegacyConfig::ShiftKeyModeSwitch::kKatakana,
         kJpPrefShiftKeyModeStyleKatakana},
    });

}  // namespace

base::Value::Dict CreatePrefsDictFromJapaneseLegacyConfig(
    JapaneseLegacyConfigPtr config) {
  base::Value::Dict dict;
  if (const auto& preedit_method = config->preedit_method;
      preedit_method.has_value()) {
    if (auto it = kPreedits.find(*preedit_method); it != kPreedits.end()) {
      dict.Set(kJpPrefInputMode, it->second);
    }
  }
  if (const auto& punctuation_method = config->punctuation_method;
      punctuation_method.has_value()) {
    if (auto it = kPunctuations.find(*punctuation_method);
        it != kPunctuations.end()) {
      dict.Set(kJpPrefPunctuationStyle, it->second);
    }
  }
  if (const auto& symbol_method = config->symbol_method;
      symbol_method.has_value()) {
    if (auto it = kSymbols.find(*symbol_method); it != kSymbols.end()) {
      dict.Set(kJpPrefSymbolStyle, it->second);
    }
  }
  if (const auto& space_character_form = config->space_character_form;
      space_character_form.has_value()) {
    if (auto it = kFundamentalCharacterForms.find(*space_character_form);
        it != kFundamentalCharacterForms.end()) {
      dict.Set(kJpPrefSpaceInputStyle, it->second);
    }
  }
  if (const auto& selection_shortcut = config->selection_shortcut;
      selection_shortcut.has_value()) {
    if (auto it = kSelectionShortcuts.find(*selection_shortcut);
        it != kSelectionShortcuts.end()) {
      dict.Set(kJpPrefSelectionShortcut, it->second);
    }
  }
  if (const auto& session_keymap = config->session_keymap;
      session_keymap.has_value()) {
    if (auto it = kSessionKeymaps.find(*session_keymap);
        it != kSessionKeymaps.end()) {
      dict.Set(kJpPrefKeymapStyle, it->second);
    }
  }
  if (const auto& shift_key_mode_switch = config->shift_key_mode_switch;
      shift_key_mode_switch.has_value()) {
    if (auto it = kShiftKeyModeSwitch.find(*shift_key_mode_switch);
        it != kShiftKeyModeSwitch.end()) {
      dict.Set(kJpPrefShiftKeyModeStyle, it->second);
    }
  }
  if (const auto& use_auto_conversion = config->use_auto_conversion;
      use_auto_conversion.has_value()) {
    dict.Set(kJpPrefAutomaticallySwitchToHalfwidth, *use_auto_conversion);
  }
  if (const auto& use_history_suggest = config->use_history_suggest;
      use_history_suggest.has_value()) {
    dict.Set(kJpPrefUseInputHistory, *use_history_suggest);
  }
  if (const auto& use_dictionary_suggest = config->use_dictionary_suggest;
      use_dictionary_suggest.has_value()) {
    dict.Set(kJpPrefUseSystemDictionary, *use_dictionary_suggest);
  }
  if (const auto& incognito_mode = config->incognito_mode;
      incognito_mode.has_value()) {
    dict.Set(kJpPrefDisablePersonalizedSuggestions, *incognito_mode);
  }
  if (const auto& upload_usage_stats = config->upload_usage_stats;
      upload_usage_stats.has_value()) {
    dict.Set(kJpPrefAutomaticallySendStatisticsToGoogle, *upload_usage_stats);
  }
  if (const auto& suggestion_size = config->suggestion_size;
      suggestion_size.has_value()) {
    dict.Set(kJpPrefNumberOfSuggestions, static_cast<int>(*suggestion_size));
  }
  return dict;
}

}  // namespace ash::input_method
