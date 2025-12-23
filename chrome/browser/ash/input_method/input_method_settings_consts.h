// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_CONSTS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_SETTINGS_CONSTS_H_

namespace ash {
namespace input_method {

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
