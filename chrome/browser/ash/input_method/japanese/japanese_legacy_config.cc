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
  if (auto it = kPreedits.find(config->preedit_method); it != kPreedits.end()) {
    dict.Set(kJpPrefInputMode, it->second);
  }
  if (auto it = kPunctuations.find(config->punctuation_method);
      it != kPunctuations.end()) {
    dict.Set(kJpPrefPunctuationStyle, it->second);
  }
  if (auto it = kSymbols.find(config->symbol_method); it != kSymbols.end()) {
    dict.Set(kJpPrefSymbolStyle, it->second);
  }
  if (auto it = kFundamentalCharacterForms.find(config->space_character_form);
      it != kFundamentalCharacterForms.end()) {
    dict.Set(kJpPrefSpaceInputStyle, it->second);
  }
  if (auto it = kSelectionShortcuts.find(config->selection_shortcut);
      it != kSelectionShortcuts.end()) {
    dict.Set(kJpPrefSelectionShortcut, it->second);
  }
  if (auto it = kSessionKeymaps.find(config->session_keymap);
      it != kSessionKeymaps.end()) {
    dict.Set(kJpPrefKeymapStyle, it->second);
  }
  if (auto it = kShiftKeyModeSwitch.find(config->shift_key_mode_switch);
      it != kShiftKeyModeSwitch.end()) {
    dict.Set(kJpPrefShiftKeyModeStyle, it->second);
  }

  dict.Set(kJpPrefAutomaticallySwitchToHalfwidth, config->use_auto_conversion);
  dict.Set(kJpPrefUseInputHistory, config->use_history_suggest);
  dict.Set(kJpPrefUseSystemDictionary, config->use_dictionary_suggest);
  dict.Set(kJpPrefDisablePersonalizedSuggestions, config->incognito_mode);
  dict.Set(kJpPrefAutomaticallySendStatisticsToGoogle,
           config->upload_usage_stats);

  dict.Set(kJpPrefNumberOfSuggestions,
           static_cast<int>(config->suggestion_size));

  return dict;
}

}  // namespace ash::input_method
