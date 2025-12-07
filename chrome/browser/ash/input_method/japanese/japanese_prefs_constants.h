// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_CONSTANTS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_JAPANESE_PREFS_CONSTANTS_H_

namespace ash::input_method {

// TODO(crbug.com/203464079): Use distinct CrOS prefs for nacl_mozc_jp
// ("Japanese [for JIS keyboard]") and nacl_mozc_us ("Japanese for US keyboard")
// input methods. Due to singleton constraints in the legacy implementation,
// unlike all other input methods whose settings were distinct from one another,
// these two input methods shared the same settings. Upon migration to CrOS
// prefs, the unintended sharing was intentionally retained until the issue is
// separately addressed outside the scope of the said migration. Thus, as of
// now, Japanese prefs should specially be set only once for ID "nacl_mozc_jp"
// and shared across both "nacl_mozc_jp" and "nacl_mozc_us" input methods.
inline constexpr std::string_view kJpPrefsEngineId = "nacl_mozc_jp";

// Top level option categories:
// LINT.IfChange(JpOptionCategories)
inline constexpr std::string_view kJpPrefAutomaticallySwitchToHalfwidth =
    "AutomaticallySwitchToHalfwidth";
inline constexpr std::string_view kJpPrefShiftKeyModeStyle =
    "ShiftKeyModeStyle";
inline constexpr std::string_view kJpPrefUseInputHistory = "UseInputHistory";
inline constexpr std::string_view kJpPrefUseSystemDictionary =
    "UseSystemDictionary";
inline constexpr std::string_view kJpPrefNumberOfSuggestions =
    "numberOfSuggestions";
inline constexpr std::string_view kJpPrefInputMode = "JapaneseInputMode";
inline constexpr std::string_view kJpPrefPunctuationStyle =
    "JapanesePunctuationStyle";
inline constexpr std::string_view kJpPrefSymbolStyle = "JapaneseSymbolStyle";
inline constexpr std::string_view kJpPrefSpaceInputStyle =
    "JapaneseSpaceInputStyle";
inline constexpr std::string_view kJpPrefSelectionShortcut =
    "JapaneseSectionShortcut";
inline constexpr std::string_view kJpPrefKeymapStyle = "JapaneseKeymapStyle";
inline constexpr std::string_view kJpPrefDisablePersonalizedSuggestions =
    "JapaneseDisableSuggestions";
// This option does not do anything, as all usage data uses UMA and adheres to
// UMA settings.
inline constexpr std::string_view kJpPrefAutomaticallySendStatisticsToGoogle =
    "AutomaticallySendStatisticsToGoogle";
// LINT.ThenChange(/chrome/browser/resources/ash/settings/os_languages_page/input_method_util.ts:JpOptionCategories)
// Options values for the above option categories:
// LINT.IfChange(JpOptionValues)
inline constexpr std::string_view kJpPrefInputModeKana = "Kana";
inline constexpr std::string_view kJpPrefInputModeRomaji = "Romaji";
inline constexpr std::string_view kJpPrefPunctuationStyleKutenTouten =
    "KutenTouten";
inline constexpr std::string_view kJpPrefPunctuationStyleCommaPeriod =
    "CommaPeriod";
inline constexpr std::string_view kJpPrefPunctuationStyleKutenPeriod =
    "KutenPeriod";
inline constexpr std::string_view kJpPrefPunctuationStyleCommaTouten =
    "CommaTouten";
inline constexpr std::string_view kJpPrefSymbolStyleCornerBracketMiddleDot =
    "CornerBracketMiddleDot";
inline constexpr std::string_view kJpPrefSymbolStyleSquareBracketSlash =
    "SquareBracketSlash";
inline constexpr std::string_view kJpPrefSymbolStyleCornerBracketSlash =
    "CornerBracketSlash";
inline constexpr std::string_view kJpPrefSymbolStyleSquareBracketMiddleDot =
    "SquareBracketMiddleDot";
inline constexpr std::string_view kJpPrefSpaceInputStyleInputMode = "InputMode";
inline constexpr std::string_view kJpPrefSpaceInputStyleFullwidth = "Fullwidth";
inline constexpr std::string_view kJpPrefSpaceInputStyleHalfwidth = "Halfwidth";
inline constexpr std::string_view kJpPrefSelectionShortcutNoShortcut =
    "NoShortcut";
inline constexpr std::string_view kJpPrefSelectionShortcutDigits123456789 =
    "Digits123456789";
inline constexpr std::string_view kJpPrefSelectionShortcutAsdfghjkl =
    "ASDFGHJKL";
inline constexpr std::string_view kJpPrefKeymapStyleCustom = "Custom";
inline constexpr std::string_view kJpPrefKeymapStyleAtok = "Atok";
inline constexpr std::string_view kJpPrefKeymapStyleMsIme = "MsIme";
inline constexpr std::string_view kJpPrefKeymapStyleKotoeri = "Kotoeri";
inline constexpr std::string_view kJpPrefKeymapStyleMobile = "Mobile";
inline constexpr std::string_view kJpPrefKeymapStyleChromeOs = "ChromeOs";
inline constexpr std::string_view kJpPrefShiftKeyModeStyleOff = "Off";
inline constexpr std::string_view kJpPrefShiftKeyModeStyleAlphanumeric =
    "Alphanumeric";
inline constexpr std::string_view kJpPrefShiftKeyModeStyleKatakana = "Katakana";
// LINT.ThenChange(/chrome/browser/resources/ash/settings/os_languages_page/input_method_types.ts:JpOptionValues)

// Pref key and values related to the "source of truth" for the options data.
// These are not accessible via the OsSettings app and is only used as a way
// to detect when configuration data needs to be copied over from one source to
// another.
inline constexpr std::string_view kJpPrefMetadataOptionsSource =
    "Metadata-OptionsSource";
// This is the special "legacy" configuration file that is used directly by the
// extension on disk to set configurations.
inline constexpr std::string_view kJpPrefMetadataOptionsSourceLegacyConfig1Db =
    "LegacyConfig1Db";
// In SystemPK Japanese, the source of truth is the chromeos PrefService like
// the rest of the IMEs.
inline constexpr std::string_view kJpPrefMetadataOptionsSourcePrefService =
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
