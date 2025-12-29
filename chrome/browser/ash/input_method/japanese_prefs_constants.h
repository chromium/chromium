// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_PREFS_CONSTANTS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_PREFS_CONSTANTS_H_

namespace ash::input_method {

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

// "...Section..." in the string value below is a typo, but persisted in CrOS
// Prefs storage so must NOT be fixed unless user data are migrated first.
inline constexpr std::string_view kJpPrefSelectionShortcut =
    "JapaneseSectionShortcut";

inline constexpr std::string_view kJpPrefKeymapStyle = "JapaneseKeymapStyle";
inline constexpr std::string_view kJpPrefDisablePersonalizedSuggestions =
    "JapaneseDisableSuggestions";
// This option does not do anything, as all usage data uses UMA and adheres to
// UMA settings.
inline constexpr std::string_view kJpPrefAutomaticallySendStatisticsToGoogle =
    "AutomaticallySendStatisticsToGoogle";
// LINT.ThenChange(/chrome/browser/resources/ash/settings/os_languages_page/input_method_prefs_consts.ts:JpOptionCategories)
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
// LINT.ThenChange(/chrome/browser/resources/ash/settings/os_languages_page/input_method_prefs_consts.ts:JpOptionValues)

// Obsolete CrOS-Prefs key. Entry with this key was previously persisted to
// CrOS-Prefs for internal use by the now terminated Mozc-to-CrOS-Prefs data
// migration; it was never accessible via CrOS Settings app.
inline constexpr std::string_view kJpPrefMetadataOptionsSource =
    "Metadata-OptionsSource";

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_PREFS_CONSTANTS_H_
