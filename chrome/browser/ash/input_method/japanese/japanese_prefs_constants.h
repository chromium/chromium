// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_CONSTANTS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_CONSTANTS_H_

namespace ash::input_method {

// Japanese Prefs should be should be set only the nacl_mozc_jp, and shared
// across both "nacl_mozc_jp" and "nacl_mozc_us"
static constexpr std::string_view kJpPrefsEngineId = "nacl_mozc_jp";

// Top level option categories:
// LINT.IfChange(JpOptionCategories)
static constexpr std::string_view kJpPrefAutomaticallySwitchToHalfwidth =
    "AutomaticallySwitchToHalfwidth";
static constexpr std::string_view kJpPrefShiftKeyModeStyle =
    "ShiftKeyModeStyle";
static constexpr std::string_view kJpPrefUseInputHistory = "UseInputHistory";
static constexpr std::string_view kJpPrefUseSystemDictionary =
    "UseSystemDictionary";
static constexpr std::string_view kJpPrefNumberOfSuggestions =
    "numberOfSuggestions";
static constexpr std::string_view kJpPrefInputMode = "JapaneseInputMode";
static constexpr std::string_view kJpPrefPunctuationStyle =
    "JapanesePunctuationStyle";
static constexpr std::string_view kJpPrefSymbolStyle = "JapaneseSymbolStyle";
static constexpr std::string_view kJpPrefSpaceInputStyle =
    "JapaneseSpaceInputStyle";
static constexpr std::string_view kJpPrefSelectionShortcut =
    "JapaneseSectionShortcut";
static constexpr std::string_view kJpPrefKeymapStyle = "JapaneseKeymapStyle";
static constexpr std::string_view kJpPrefDisablePersonalizedSuggestions =
    "JapaneseDisableSuggestions";
static constexpr std::string_view kJpPrefAutomaticallySendStatisticsToGoogle =
    "AutomaticallySendStatisticsToGoogle";
// LINT.ThenChange(/chrome/browser/resources/ash/settings/os_languages_page/input_method_util.ts:JpOptionCategories)
// Options values for the above option categories:
// LINT.IfChange(JpOptionValues)
static constexpr std::string_view kJpPrefInputModeKana = "Kana";
static constexpr std::string_view kJpPrefInputModeRomaji = "Romaji";
static constexpr std::string_view kJpPrefPunctuationStyleKutenTouten =
    "KutenTouten";
static constexpr std::string_view kJpPrefPunctuationStyleCommaPeriod =
    "CommaPeriod";
static constexpr std::string_view kJpPrefPunctuationStyleKutenPeriod =
    "KutenPeriod";
static constexpr std::string_view kJpPrefPunctuationStyleCommaTouten =
    "CommaTouten";
static constexpr std::string_view kJpPrefSymbolStyleCornerBracketMiddleDot =
    "CornerBracketMiddleDot";
static constexpr std::string_view kJpPrefSymbolStyleSquareBracketSlash =
    "SquareBracketSlash";
static constexpr std::string_view kJpPrefSymbolStyleCornerBracketSlash =
    "CornerBracketSlash";
static constexpr std::string_view kJpPrefSymbolStyleSquareBracketMiddleDot =
    "SquareBracketMiddleDot";
static constexpr std::string_view kJpPrefSpaceInputStyleInputMode = "InputMode";
static constexpr std::string_view kJpPrefSpaceInputStyleFullwidth = "Fullwidth";
static constexpr std::string_view kJpPrefSpaceInputStyleHalfwidth = "Halfwidth";
static constexpr std::string_view kJpPrefSelectionShortcutNoShortcut =
    "NoShortcut";
static constexpr std::string_view kJpPrefSelectionShortcutDigits123456789 =
    "Digits123456789";
static constexpr std::string_view kJpPrefSelectionShortcutAsdfghjkl =
    "ASDFGHJKL";
static constexpr std::string_view kJpPrefKeymapStyleCustom = "Custom";
static constexpr std::string_view kJpPrefKeymapStyleAtok = "Atok";
static constexpr std::string_view kJpPrefKeymapStyleMsIme = "MsIme";
static constexpr std::string_view kJpPrefKeymapStyleKotoeri = "Kotoeri";
static constexpr std::string_view kJpPrefKeymapStyleMobile = "Mobile";
static constexpr std::string_view kJpPrefKeymapStyleChromeOs = "ChromeOs";
static constexpr std::string_view kJpPrefShiftKeyModeStyleOff = "Off";
static constexpr std::string_view kJpPrefShiftKeyModeStyleAlphanumeric =
    "Alphanumeric";
static constexpr std::string_view kJpPrefShiftKeyModeStyleKatakana = "Katakana";
// LINT.ThenChange(/chrome/browser/resources/ash/settings/os_languages_page/input_method_types.ts:JpOptionValues)

// Pref key and values related to the "source of truth" for the options data.
// These are not accessible via the OsSettings app and is only used as a way
// to detect when configuration data needs to be copied over from one source to
// another.
static constexpr std::string_view kJpPrefMetadataOptionsSource =
    "Metadata-OptionsSource";
// This is the special "legacy" configuration file that is used directly by the
// extension on disk to set configurations.
static constexpr std::string_view kJpPrefMetadataOptionsSourceLegacyConfig1Db =
    "LegacyConfig1Db";
// In SystemPK Japanese, the source of truth is the chromeos PrefService like
// the rest of the IMEs.
static constexpr std::string_view kJpPrefMetadataOptionsSourcePrefService =
    "PrefService";

// All the enums below correspond to UMA histograms enum values.
// LINT.IfChange(jp_settings_hist_enums)
enum class HistInputMode {
  kRomaji = 0,
  kKana = 1,
  kMaxValue = kKana,
};

enum class HistKeymapStyle {
  kCustom = 0,
  kAtok = 1,
  kMsime = 2,
  kKotoeri = 3,
  kMobile = 4,
  kChromeos = 5,
  kMaxValue = kChromeos,
};

enum class HistPunctuationStyle {
  kKutenTouten = 0,
  kCommaPeriod = 1,
  kKutenPeriod = 2,
  kCommaTouten = 3,
  kMaxValue = kCommaTouten,
};

enum class HistSelectionShortcut {
  kDigits123456789 = 0,
  kAsdfghjkl = 1,
  kNoShortcut = 2,
  kMaxValue = kNoShortcut,
};

enum class HistShiftKeyModeStyle {
  kOff = 0,
  kAlphanumeric = 1,
  kKatakana = 2,
  kMaxValue = kKatakana,
};

enum class HistSpaceInputStyle {
  kInputMode = 0,
  kFullWidth = 1,
  kHalfWidth = 2,
  kMaxValue = kHalfWidth,
};

enum class HistSymbolStyle {
  kCornerBracketMiddleDot = 0,
  kSquareBracketSlash = 1,
  kCornerBracketSlash = 2,
  kSquareBracketMiddleDot = 3,
  kMaxValue = kSquareBracketMiddleDot,
};

// LINT.ThenChange(/tools/metrics/histograms/metadata/input/enums.xml:jp_settings_hist_enums)

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_CONSTANTS_H_
