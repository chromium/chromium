// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_CONSTS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_CONSTS_H_

namespace ash {
namespace input_method {

inline constexpr std::string_view kPkAutocorrectLevelPrefName =
    "physicalKeyboardAutoCorrectionLevel";
inline constexpr std::string_view kPkAutocorrectEnabledByDefaultPrefName =
    "physicalKeyboardAutoCorrectionEnabledByDefault";
inline constexpr std::string_view kPkEnablePredictiveWritingPrefName =
    "physicalKeyboardEnablePredictiveWriting";
inline constexpr std::string_view kVkAutocorrectLevelPrefName =
    "virtualKeyboardAutoCorrectionLevel";

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

inline constexpr std::string_view kKrPrefEnableSyllableInput =
    "koreanEnableSyllableInput";
inline constexpr std::string_view kKrPrefKeyboardLayout =
    "koreanKeyboardLayout";

inline constexpr std::string_view kPinyinPrefXkbLayout = "xkbLayout";
inline constexpr std::string_view kPinyinPrefChinesePunctuation =
    "pinyinChinesePunctuation";
inline constexpr std::string_view kPinyinPrefDefaultChinese =
    "pinyinDefaultChinese";
inline constexpr std::string_view kPinyinPrefEnableFuzzy = "pinyinEnableFuzzy";
inline constexpr std::string_view kPinyinPrefEnableLowerPaging =
    "pinyinEnableLowerPaging";
inline constexpr std::string_view kPinyinPrefEnableUpperPaging =
    "pinyinEnableUpperPaging";
inline constexpr std::string_view kPinyinPrefFullWidthCharacter =
    "pinyinFullWidthCharacter";
inline constexpr std::string_view kPinyinPrefFuzzyEnEng = "en:eng";
inline constexpr std::string_view kPinyinPrefFuzzyAnAng = "an:ang";
inline constexpr std::string_view kPinyinPrefFuzzyIanIang = "ian:iang";
inline constexpr std::string_view kPinyinPrefFuzzyKG = "k:g";
inline constexpr std::string_view kPinyinPrefFuzzyRL = "r:l";
inline constexpr std::string_view kPinyinPrefFuzzyUanUang = "uan:uang";
inline constexpr std::string_view kPinyinPrefFuzzyCCh = "c:ch";
inline constexpr std::string_view kPinyinPrefFuzzyFH = "f:h";
inline constexpr std::string_view kPinyinPrefFuzzyInIng = "in:ing";
inline constexpr std::string_view kPinyinPrefFuzzyLN = "l:n";
inline constexpr std::string_view kPinyinPrefFuzzySSh = "s:sh";
inline constexpr std::string_view kPinyinPrefFuzzyZZh = "z:zh";

inline constexpr std::string_view kZhuyinPrefKeyboardLayout =
    "zhuyinKeyboardLayout";
inline constexpr std::string_view kZhuyinPrefPageSize = "zhuyinPageSize";
inline constexpr std::string_view kZhuyinPrefSelectKeys = "zhuyinSelectKeys";

inline constexpr std::string_view kVnPrefVniAllowFlexibleDiacritics =
    "vietnameseVniAllowFlexibleDiacritics";
inline constexpr std::string_view kVnPrefVniNewStyleToneMarkPlacement =
    "vietnameseVniNewStyleToneMarkPlacement";
inline constexpr std::string_view kVnPrefVniInsertDoubleHornOnUo =
    "vietnameseVniInsertDoubleHornOnUo";
inline constexpr std::string_view kVnPrefVniShowUnderline =
    "vietnameseVniShowUnderline";
inline constexpr std::string_view kVnPrefTelexAllowFlexibleDiacritics =
    "vietnameseTelexAllowFlexibleDiacritics";
inline constexpr std::string_view kVnPrefTelexNewStyleToneMarkPlacement =
    "vietnameseTelexNewStyleToneMarkPlacement";
inline constexpr std::string_view kVnPrefTelexInsertDoubleHornOnUo =
    "vietnameseTelexInsertDoubleHornOnUo";
inline constexpr std::string_view kVnPrefTelexInsertUHornOnW =
    "vietnameseTelexInsertUHornOnW";
inline constexpr std::string_view kVnPrefTelexShowUnderline =
    "vietnameseTelexShowUnderline";

// Options values for the above option categories:
// LINT.IfChange(JpOptionValues)
inline constexpr std::string_view kJpPrefInputModeKana = "Kana";
inline constexpr std::string_view kJpPrefInputModeRomaji = "Romaji";

// "KutenTouten" string value is a misnomer originating from Japanese IME Mozc
// lib (where it's now been fixed), but this string is persisted in CrOS Prefs
// storage so must NOT be adapted unless user data are migrated first.
inline constexpr std::string_view kJpPrefPunctuationStyleToutenKuten =
    "KutenTouten";

inline constexpr std::string_view kJpPrefPunctuationStyleCommaPeriod =
    "CommaPeriod";

// "KutenPeriod" string value is a misnomer originating from Japanese IME Mozc
// lib (where it's now been fixed), but this string is persisted in CrOS Prefs
// storage so must NOT be adapted unless user data are migrated first.
inline constexpr std::string_view kJpPrefPunctuationStyleToutenPeriod =
    "KutenPeriod";

// "CommaTouten" string value is a misnomer originating from Japanese IME Mozc
// lib (where it's now been fixed), but this string is persisted in CrOS Prefs
// storage so must NOT be adapted unless user data are migrated first.
inline constexpr std::string_view kJpPrefPunctuationStyleCommaKuten =
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

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
// Although these strings look like UI strings, they are the actual internal
// values stored inside prefs. Therefore, it is important to make sure these
// strings match the settings page exactly.
inline constexpr std::string_view kKoreanPrefsLayoutDubeolsik =
    "2 Set / 두벌식";
inline constexpr std::string_view kKoreanPrefsLayoutDubeolsikOldHangeul =
    "2 Set (Old Hangul) / 두벌식 (옛글)";
inline constexpr std::string_view kKoreanPrefsLayoutSebeolsik390 =
    "3 Set (390) / 세벌식 (390)";
inline constexpr std::string_view kKoreanPrefsLayoutSebeolsikFinal =
    "3 Set (Final) / 세벌식 (최종)";
inline constexpr std::string_view kKoreanPrefsLayoutSebeolsikNoShift =
    "3 Set (No Shift) / 세벌식 (순아래)";
inline constexpr std::string_view kKoreanPrefsLayoutSebeolsikOldHangeul =
    "3 Set (Old Hangul) / 세벌식 (옛글)";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
inline constexpr std::string_view kPinyinPrefsLayoutUsQwerty = "US";
inline constexpr std::string_view kPinyinPrefsLayoutDvorak = "Dvorak";
inline constexpr std::string_view kPinyinPrefsLayoutColemak = "Colemak";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
inline constexpr std::string_view kZhuyinPrefsLayoutStandard = "Default";
inline constexpr std::string_view kZhuyinPrefsLayoutIbm = "IBM";
inline constexpr std::string_view kZhuyinPrefsLayoutEten = "Eten";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
inline constexpr std::string_view kZhuyinPrefsSelectionKeys1234567890 =
    "1234567890";
inline constexpr std::string_view kZhuyinPrefsSelectionKeysAsdfghjkl =
    "asdfghjkl;";
inline constexpr std::string_view kZhuyinPrefsSelectionKeysAsdfzxcv89 =
    "asdfzxcv89";
inline constexpr std::string_view kZhuyinPrefsSelectionKeysAsdfjkl789 =
    "asdfjkl789";
inline constexpr std::string_view kZhuyinPrefsSelectionKeys1234Qweras =
    "1234qweras";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
inline constexpr std::string_view kZhuyinPrefsPageSize10 = "10";
inline constexpr std::string_view kZhuyinPrefsPageSize9 = "9";
inline constexpr std::string_view kZhuyinPrefsPageSize8 = "8";

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_CONSTS_H_
