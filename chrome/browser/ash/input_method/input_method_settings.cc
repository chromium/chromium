// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_settings.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/services/ime/public/mojom/japanese_settings.mojom-shared.h"
#include "chromeos/ash/services/ime/public/mojom/japanese_settings.mojom.h"

namespace ash {
namespace input_method {

namespace {

namespace mojom = ::ash::ime::mojom;

// The Japanese engine. This is the key for the settings object which lets us
// know where to store the settings info.
constexpr char kJapaneseEngineId[] = "nacl_mozc_jp";

// This should be kept in sync with the values on the settings page's
// InputMethodOptions. This should match
// https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js;l=71-88;drc=6c88edbfe6096489ccac66b3ef5c84d479892181.
constexpr char kJapaneseAutomaticallySwitchToHalfwidth[] =
    "AutomaticallySwitchToHalfwidth";
constexpr char kJapaneseShiftKeyModeStyle[] = "ShiftKeyModeStyle";
constexpr char kJapaneseUseInputHistory[] = "UseInputHistory";
constexpr char kJapaneseUseSystemDictionary[] = "UseSystemDictionary";
constexpr char kJapaneseNumberOfSuggestions[] = "numberOfSuggestions";
constexpr char kJapaneseInputMode[] = "JapaneseInputMode";
constexpr char kJapanesePunctuationStyle[] = "JapanesePunctuationStyle";
constexpr char kJapaneseSymbolStyle[] = "JapaneseSymbolStyle";
constexpr char kJapaneseSpaceInputStyle[] = "JapaneseSpaceInputStyle";
constexpr char kJapaneseSelectionShortcut[] = "JapaneseSectionShortcut";
constexpr char kJapaneseKeymapStyle[] = "JapaneseKeymapStyle";
constexpr char kJapaneseDisablePersonalizedSuggestions[] =
    "JapaneseDisableSuggestions";
constexpr char kJapaneseAutomaticallySendStatisticsToGoogle[] =
    "AutomaticallySendStatisticsToGoogle";

// This should match the strings listed here:
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

// The values here should be kept in sync with
// chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js
// Although these strings look like UI strings, they are the actual internal
// values stored inside prefs. Therefore, it is important to make sure these
// strings match the settings page exactly.
constexpr char kKoreanPrefsLayoutDubeolsik[] = "2 Set / 두벌식";
constexpr char kKoreanPrefsLayoutDubeolsikOldHangeul[] =
    "2 Set (Old Hangul) / 두벌식 (옛글)";
constexpr char kKoreanPrefsLayoutSebeolsik390[] = "3 Set (390) / 세벌식 (390)";
constexpr char kKoreanPrefsLayoutSebeolsikFinal[] =
    "3 Set (Final) / 세벌식 (최종)";
constexpr char kKoreanPrefsLayoutSebeolsikNoShift[] =
    "3 Set (No Shift) / 세벌식 (순아래)";
constexpr char kKoreanPrefsLayoutSebeolsikOldHangeul[] =
    "3 Set (Old Hangul) / 세벌식 (옛글)";

// The values here should be kept in sync with
// chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js
constexpr char kPinyinPrefsLayoutUsQwerty[] = "US";
constexpr char kPinyinPrefsLayoutDvorak[] = "Dvorak";
constexpr char kPinyinPrefsLayoutColemak[] = "Colemak";

// The values here should be kept in sync with
// chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js
constexpr char kZhuyinPrefsLayoutStandard[] = "Default";
constexpr char kZhuyinPrefsLayoutIbm[] = "IBM";
constexpr char kZhuyinPrefsLayoutEten[] = "Eten";

// The values here should be kept in sync with
// chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js
constexpr char kZhuyinPrefsSelectionKeys1234567890[] = "1234567890";
constexpr char kZhuyinPrefsSelectionKeysAsdfghjkl[] = "asdfghjkl;";
constexpr char kZhuyinPrefsSelectionKeysAsdfzxcv89[] = "asdfzxcv89";
constexpr char kZhuyinPrefsSelectionKeysAsdfjkl789[] = "asdfjkl789";
constexpr char kZhuyinPrefsSelectionKeys1234Qweras[] = "1234qweras";

// The values here should be kept in sync with
// chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js
constexpr char kZhuyinPrefsPageSize10[] = "10";
constexpr char kZhuyinPrefsPageSize9[] = "9";
constexpr char kZhuyinPrefsPageSize8[] = "8";

constexpr char kJapaneseMigrationCompleteKey[] = "is_migration_complete";

const base::Value::Dict* GetJapaneseInputMethodSpecificSettings(
    const PrefService& prefs) {
  const base::Value::Dict& all_input_method_prefs =
      prefs.GetDict(::prefs::kLanguageInputMethodSpecificSettings);
  return all_input_method_prefs.FindDict(kJapaneseEngineId);
}

std::string ValueOrEmpty(const std::string* str) {
  return str ? *str : "";
}

bool IsUsEnglishEngine(const std::string& engine_id) {
  return engine_id == "xkb:us::eng";
}

bool IsFstEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE) ||
         base::StartsWith(engine_id, "experimental_",
                          base::CompareCase::SENSITIVE);
}

bool IsKoreanEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "ko-", base::CompareCase::SENSITIVE);
}

bool IsPinyinEngine(const std::string& engine_id) {
  return engine_id == "zh-t-i0-pinyin" || engine_id == "zh-hant-t-i0-pinyin";
}

bool IsZhuyinEngine(const std::string& engine_id) {
  return engine_id == "zh-hant-t-i0-und";
}

mojom::LatinSettingsPtr CreateLatinSettings(
    const base::Value::Dict& input_method_specific_pref,
    const PrefService& prefs,
    const std::string& engine_id) {
  auto settings = mojom::LatinSettings::New();
  auto autocorrect_pref = GetPhysicalKeyboardAutocorrectPref(prefs, engine_id);
  settings->autocorrect =
      base::StartsWith(engine_id, "experimental_",
                       base::CompareCase::SENSITIVE) ||
      base::FeatureList::IsEnabled(features::kAutocorrectParamsTuning) ||
      autocorrect_pref == AutocorrectPreference::kEnabled;
  settings->predictive_writing =
      features::IsAssistiveMultiWordEnabled() &&
      prefs.GetBoolean(prefs::kAssistPredictiveWritingEnabled) &&
      IsUsEnglishEngine(engine_id);
  return settings;
}

mojom::KoreanLayout KoreanLayoutToMojom(const std::string& layout) {
  if (layout == kKoreanPrefsLayoutDubeolsik)
    return mojom::KoreanLayout::kDubeolsik;
  if (layout == kKoreanPrefsLayoutDubeolsikOldHangeul)
    return mojom::KoreanLayout::kDubeolsikOldHangeul;
  if (layout == kKoreanPrefsLayoutSebeolsik390)
    return mojom::KoreanLayout::kSebeolsik390;
  if (layout == kKoreanPrefsLayoutSebeolsikFinal)
    return mojom::KoreanLayout::kSebeolsikFinal;
  if (layout == kKoreanPrefsLayoutSebeolsikNoShift)
    return mojom::KoreanLayout::kSebeolsikNoShift;
  if (layout == kKoreanPrefsLayoutSebeolsikOldHangeul)
    return mojom::KoreanLayout::kSebeolsikOldHangeul;
  return mojom::KoreanLayout::kDubeolsik;
}

mojom::KoreanSettingsPtr CreateKoreanSettings(
    const base::Value::Dict& input_method_specific_pref) {
  auto settings = mojom::KoreanSettings::New();
  settings->input_multiple_syllables =
      !input_method_specific_pref.FindBool("koreanEnableSyllableInput")
           .value_or(true);
  const std::string* prefs_layout =
      input_method_specific_pref.FindString("koreanKeyboardLayout");
  settings->layout = prefs_layout ? KoreanLayoutToMojom(*prefs_layout)
                                  : mojom::KoreanLayout::kDubeolsik;
  return settings;
}

mojom::FuzzyPinyinSettingsPtr CreateFuzzyPinyinSettings(
    const base::Value::Dict& pref) {
  auto settings = mojom::FuzzyPinyinSettings::New();
  settings->an_ang = pref.FindBool("an:ang").value_or(false);
  settings->en_eng = pref.FindBool("en:eng").value_or(false);
  settings->ian_iang = pref.FindBool("ian:iang").value_or(false);
  settings->k_g = pref.FindBool("k:g").value_or(false);
  settings->r_l = pref.FindBool("r:l").value_or(false);
  settings->uan_uang = pref.FindBool("uan:uang").value_or(false);
  settings->c_ch = pref.FindBool("c:ch").value_or(false);
  settings->f_h = pref.FindBool("f:h").value_or(false);
  settings->in_ing = pref.FindBool("in:ing").value_or(false);
  settings->l_n = pref.FindBool("l:n").value_or(false);
  settings->s_sh = pref.FindBool("s:sh").value_or(false);
  settings->z_zh = pref.FindBool("z:zh").value_or(false);
  return settings;
}

mojom::PinyinLayout PinyinLayoutToMojom(const std::string& layout) {
  if (layout == kPinyinPrefsLayoutUsQwerty)
    return mojom::PinyinLayout::kUsQwerty;
  if (layout == kPinyinPrefsLayoutDvorak)
    return mojom::PinyinLayout::kDvorak;
  if (layout == kPinyinPrefsLayoutColemak)
    return mojom::PinyinLayout::kColemak;
  return mojom::PinyinLayout::kUsQwerty;
}

mojom::PinyinSettingsPtr CreatePinyinSettings(
    const base::Value::Dict& input_method_specific_pref) {
  auto settings = mojom::PinyinSettings::New();
  settings->fuzzy_pinyin =
      CreateFuzzyPinyinSettings(input_method_specific_pref);
  const std::string* prefs_layout =
      input_method_specific_pref.FindString("xkbLayout");
  settings->layout = prefs_layout ? PinyinLayoutToMojom(*prefs_layout)
                                  : mojom::PinyinLayout::kUsQwerty;
  settings->use_hyphen_and_equals_to_page_candidates =
      input_method_specific_pref.FindBool("pinyinEnableUpperPaging")
          .value_or(true);
  settings->use_comma_and_period_to_page_candidates =
      input_method_specific_pref.FindBool("pinyinEnableLowerPaging")
          .value_or(true);
  settings->default_to_chinese =
      input_method_specific_pref.FindBool("pinyinDefaultChinese")
          .value_or(true);
  settings->default_to_full_width_characters =
      input_method_specific_pref.FindBool("pinyinFullWidthCharacter")
          .value_or(false);
  settings->default_to_full_width_punctuation =
      input_method_specific_pref.FindBool("pinyinChinesePunctuation")
          .value_or(true);
  return settings;
}

mojom::ZhuyinLayout ZhuyinLayoutToMojom(const std::string& layout) {
  if (layout == kZhuyinPrefsLayoutStandard)
    return mojom::ZhuyinLayout::kStandard;
  if (layout == kZhuyinPrefsLayoutIbm)
    return mojom::ZhuyinLayout::kIbm;
  if (layout == kZhuyinPrefsLayoutEten)
    return mojom::ZhuyinLayout::kEten;
  return mojom::ZhuyinLayout::kStandard;
}

mojom::ZhuyinSelectionKeys ZhuyinSelectionKeysToMojom(
    const std::string& selection_keys) {
  if (selection_keys == kZhuyinPrefsSelectionKeys1234567890)
    return mojom::ZhuyinSelectionKeys::k1234567890;
  if (selection_keys == kZhuyinPrefsSelectionKeysAsdfghjkl)
    return mojom::ZhuyinSelectionKeys::kAsdfghjkl;
  if (selection_keys == kZhuyinPrefsSelectionKeysAsdfzxcv89)
    return mojom::ZhuyinSelectionKeys::kAsdfzxcv89;
  if (selection_keys == kZhuyinPrefsSelectionKeysAsdfjkl789)
    return mojom::ZhuyinSelectionKeys::kAsdfjkl789;
  if (selection_keys == kZhuyinPrefsSelectionKeys1234Qweras)
    return mojom::ZhuyinSelectionKeys::k1234Qweras;
  return mojom::ZhuyinSelectionKeys::k1234567890;
}

uint32_t ZhuyinPageSizeToInt(const std::string& page_size) {
  if (page_size == kZhuyinPrefsPageSize10)
    return 10;
  if (page_size == kZhuyinPrefsPageSize9)
    return 9;
  if (page_size == kZhuyinPrefsPageSize8)
    return 8;
  return 10;
}

mojom::ZhuyinSettingsPtr CreateZhuyinSettings(
    const base::Value::Dict& input_method_specific_pref) {
  auto settings = mojom::ZhuyinSettings::New();
  settings->layout = ZhuyinLayoutToMojom(ValueOrEmpty(
      input_method_specific_pref.FindString("zhuyinKeyboardLayout")));
  settings->selection_keys = ZhuyinSelectionKeysToMojom(
      ValueOrEmpty(input_method_specific_pref.FindString("zhuyinSelectKeys")));
  settings->page_size = ZhuyinPageSizeToInt(
      ValueOrEmpty(input_method_specific_pref.FindString("zhuyinPageSize")));
  return settings;
}

const base::Value::Dict& GetPrefsDictionaryForEngineId(
    const PrefService& prefs,
    const std::string& engine_id,
    const base::Value::Dict& fallback_dictionary) {
  // All input method settings are stored in a single pref whose value is a
  // dictionary.
  const base::Value::Dict& all_input_method_pref =
      prefs.GetDict(::prefs::kLanguageInputMethodSpecificSettings);

  // For each input method, the dictionary contains an entry, with the key being
  // a string that identifies the input method, and the value being a
  // subdictionary with the specific settings for that input method.  The
  // subdictionary structure depends on the type of input method it's for.  The
  // subdictionary may be null if the user hasn't changed any settings for that
  // input method.
  const base::Value::Dict* input_method_specific_pref_or_null =
      all_input_method_pref.FindDict(engine_id);

  // For convenience, pass an empty dictionary if there are no settings for this
  // input method yet.
  return input_method_specific_pref_or_null
             ? *input_method_specific_pref_or_null
             : fallback_dictionary;
}

// Port the Prefs settings onto a Dict object for setting the user Prefs.
// This converts the code in the corresponding pages:
// https://crsrc.org/c/chrome/browser/resources/settings/chromeos/os_languages_page/input_method_util.js;drc=6c88edbfe6096489ccac66b3ef5c84d479892181;l=72
// https://crsrc.org/c/chromeos/ash/services/ime/public/mojom/japanese_settings.mojom;drc=e250164fc5bdefca32cb94157e9835ff8c2c9ee6;l=73
base::Value::Dict ConvertConfigToJapaneseSettings(
    const ime::mojom::JapaneseConfig config) {
  base::Value::Dict japanese_settings;
  switch (config.input_mode) {
    case (mojom::InputMode::kRomaji):
      japanese_settings.Set(kJapaneseInputMode,
                            std::string(kJapaneseInputModeRomaji));
      break;
    case (mojom::InputMode::kKana):
      japanese_settings.Set(kJapaneseInputMode,
                            std::string(kJapaneseInputModeKana));
      break;
  }
  switch (config.punctuation_style) {
    case (mojom::PunctuationStyle::kKutenTouten):
      japanese_settings.Set(kJapanesePunctuationStyle,
                            std::string(kJapanesePunctuationStyleKutenTouten));
      break;
    case (mojom::PunctuationStyle::kCommaTouten):
      japanese_settings.Set(kJapanesePunctuationStyle,
                            std::string(kJapanesePunctuationStyleCommaTouten));
      break;
    case (mojom::PunctuationStyle::kKutenPeriod):
      japanese_settings.Set(kJapanesePunctuationStyle,
                            std::string(kJapanesePunctuationStyleKutenPeriod));
      break;
    case (mojom::PunctuationStyle::kCommaPeriod):
      japanese_settings.Set(kJapanesePunctuationStyle,
                            std::string(kJapanesePunctuationStyleCommaPeriod));
      break;
  }
  switch (config.symbol_style) {
    case (mojom::SymbolStyle::kCornerBracketMiddleDot):
      japanese_settings.Set(
          kJapaneseSymbolStyle,
          std::string(kJapaneseSymbolStyleCornerBracketMiddleDot));
      break;
    case (mojom::SymbolStyle::kCornerBracketSlash):
      japanese_settings.Set(
          kJapaneseSymbolStyle,
          std::string(kJapaneseSymbolStyleCornerBracketSlash));
      break;
    case (mojom::SymbolStyle::kSquareBracketSlash):
      japanese_settings.Set(
          kJapaneseSymbolStyle,
          std::string(kJapaneseSymbolStyleSquareBracketSlash));
      break;
    case (mojom::SymbolStyle::kSquareBracketMiddleDot):
      japanese_settings.Set(
          kJapaneseSymbolStyle,
          std::string(kJapaneseSymbolStyleSquareBracketMiddleDot));
      break;
  }

  switch (config.space_input_style) {
    case (mojom::SpaceInputStyle::kInputMode):
      japanese_settings.Set(kJapaneseSpaceInputStyle,
                            std::string(kJapaneseSpaceInputStyleInputMode));
      break;
    case (mojom::SpaceInputStyle::kHalfwidth):
      japanese_settings.Set(kJapaneseSpaceInputStyle,
                            std::string(kJapaneseSpaceInputStyleHalfwidth));
      break;
    case (mojom::SpaceInputStyle::kFullwidth):
      japanese_settings.Set(kJapaneseSpaceInputStyle,
                            std::string(kJapaneseSpaceInputStyleFullwidth));
      break;
  }

  switch (config.selection_shortcut) {
    case (mojom::SelectionShortcut::kNoShortcut):
      japanese_settings.Set(kJapaneseSelectionShortcut,
                            std::string(kJapaneseSelectionShortcutNoShortcut));
      break;
    case (mojom::SelectionShortcut::kAsdfghjkl):
      japanese_settings.Set(kJapaneseSelectionShortcut,
                            std::string(kJapaneseSelectionShortcutAsdfghjkl));
      break;
    case (mojom::SelectionShortcut::kDigits123456789):
      japanese_settings.Set(
          kJapaneseSelectionShortcut,
          std::string(kJapaneseSelectionShortcutDigits123456789));
      break;
  }

  switch (config.keymap_style) {
    case (mojom::KeymapStyle::kCustom):
      japanese_settings.Set(kJapaneseKeymapStyle,
                            std::string(kJapaneseKeymapStyleCustom));
      break;
    case (mojom::KeymapStyle::kAtok):
      japanese_settings.Set(kJapaneseKeymapStyle,
                            std::string(kJapaneseKeymapStyleAtok));
      break;
    case (mojom::KeymapStyle::kMsIme):
      japanese_settings.Set(kJapaneseKeymapStyle,
                            std::string(kJapaneseKeymapStyleMsIme));
      break;
    case (mojom::KeymapStyle::kKotoeri):
      japanese_settings.Set(kJapaneseKeymapStyle,
                            std::string(kJapaneseKeymapStyleKotoeri));
      break;
    case (mojom::KeymapStyle::kMobile):
      japanese_settings.Set(kJapaneseKeymapStyle,
                            std::string(kJapaneseKeymapStyleMobile));
      break;
    case (mojom::KeymapStyle::kChromeOs):
      japanese_settings.Set(kJapaneseKeymapStyle,
                            std::string(kJapaneseKeymapStyleChromeOs));
      break;
    case (mojom::KeymapStyle::kNone):
      // Note: For None type, we just default to MsIme. That is the default,
      // since None seems to be unused.
      japanese_settings.Set(kJapaneseKeymapStyle,
                            std::string(kJapaneseKeymapStyleMsIme));
      break;
  }

  japanese_settings.Set(kJapaneseAutomaticallySwitchToHalfwidth,
                        config.automatically_switch_to_halfwidth);

  switch (config.shift_key_mode_switch) {
    case (mojom::ShiftKeyModeSwitch::kOff):
      japanese_settings.Set(kJapaneseShiftKeyModeStyle,
                            std::string(kJapaneseShiftKeyModeStyleOff));
      break;
    case (mojom::ShiftKeyModeSwitch::kAlphanumeric):
      japanese_settings.Set(
          kJapaneseShiftKeyModeStyle,
          std::string(kJapaneseShiftKeyModeStyleAlphanumeric));
      break;
    case (mojom::ShiftKeyModeSwitch::kKatakana):
      japanese_settings.Set(kJapaneseShiftKeyModeStyle,
                            std::string(kJapaneseShiftKeyModeStyleKatakana));
      break;
  }

  japanese_settings.Set(kJapaneseUseInputHistory, config.use_input_history);
  japanese_settings.Set(kJapaneseUseSystemDictionary,
                        config.use_system_dictionary);
  japanese_settings.Set(kJapaneseNumberOfSuggestions,
                        static_cast<int>(config.number_of_suggestions));
  japanese_settings.Set(kJapaneseDisablePersonalizedSuggestions,
                        config.disable_personalized_suggestions);
  japanese_settings.Set(kJapaneseAutomaticallySendStatisticsToGoogle,
                        config.send_statistics_to_google);
  return japanese_settings;
}

}  // namespace

mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const PrefService& prefs,
    const std::string& engine_id) {
  base::Value::Dict empty_dictionary;
  const auto& input_method_specific_pref =
      GetPrefsDictionaryForEngineId(prefs, engine_id, empty_dictionary);

  if (IsFstEngine(engine_id)) {
    return mojom::InputMethodSettings::NewLatinSettings(
        CreateLatinSettings(input_method_specific_pref, prefs, engine_id));
  }
  if (IsKoreanEngine(engine_id)) {
    return mojom::InputMethodSettings::NewKoreanSettings(
        CreateKoreanSettings(input_method_specific_pref));
  }
  if (IsPinyinEngine(engine_id)) {
    return mojom::InputMethodSettings::NewPinyinSettings(
        CreatePinyinSettings(input_method_specific_pref));
  }
  if (IsZhuyinEngine(engine_id)) {
    return mojom::InputMethodSettings::NewZhuyinSettings(
        CreateZhuyinSettings(input_method_specific_pref));
  }
  // TODO(b/232341104): Add the code to send the Japanese settings to
  // the engine if the engine_id is nacl_mozc_jp or nacl_mozc_us.
  // This will do the inverse of ConvertConfigToJapaneseSettings.
  // This will be something like InputMethodSettings::NewJapaneseSettings(...)

  return nullptr;
}

bool IsJapaneseSettingsMigrationComplete(const PrefService& prefs) {
  const base::Value::Dict* input_method_specific_pref_or_null =
      GetJapaneseInputMethodSpecificSettings(prefs);
  const base::Value::Dict empty_value;
  const base::Value::Dict& input_method_specific_pref =
      input_method_specific_pref_or_null ? *input_method_specific_pref_or_null
                                         : empty_value;
  const absl::optional<bool> value =
      input_method_specific_pref.FindBool(kJapaneseMigrationCompleteKey);
  return value.has_value() && *value;
}

void SetJapaneseSettingsMigrationComplete(PrefService& prefs, bool value) {
  // To set just the migration flag, this copies the whole prefs object
  // to change one entry - is_migrated, then re-set the whole
  // InputMethodSpecificPrefs object.  Maybe there is a better way to do this?
  const base::Value::Dict* input_method_specific_pref_or_null =
      GetJapaneseInputMethodSpecificSettings(prefs);

  base::Value::Dict japanese_settings =
      (input_method_specific_pref_or_null != nullptr)
          ? input_method_specific_pref_or_null->Clone()
          : base::Value::Dict();
  japanese_settings.Set(kJapaneseMigrationCompleteKey, value);

  base::Value::Dict all_input_method_prefs =
      prefs.GetDict(::prefs::kLanguageInputMethodSpecificSettings).Clone();
  all_input_method_prefs.Set(kJapaneseEngineId, std::move(japanese_settings));

  prefs.SetDict(::prefs::kLanguageInputMethodSpecificSettings,
                std::move(all_input_method_prefs));
}

// Migrate the settings to the prefs service and mark the migration as
// completed.
void MigrateJapaneseSettingsToPrefs(PrefService& prefs,
                                    ime::mojom::JapaneseConfig config) {
  const base::Value::Dict* input_method_specific_pref_or_null =
      GetJapaneseInputMethodSpecificSettings(prefs);
  base::Value::Dict japanese_settings =
      (input_method_specific_pref_or_null != nullptr)
          ? input_method_specific_pref_or_null->Clone()
          : base::Value::Dict();

  // Health check. This code should never be called if the migration has already
  // happened. This should fail if the returned optional is either
  // nullopt or if it is false - indicating that it was unset due to the
  // migration being cancelled.
  CHECK(!japanese_settings.FindBool(kJapaneseMigrationCompleteKey)
             .value_or(false));

  japanese_settings.Merge(ConvertConfigToJapaneseSettings(config));

  // Mark the Migration as completed.
  japanese_settings.Set(kJapaneseMigrationCompleteKey, true);

  // Set the config
  base::Value::Dict all_input_method_prefs =
      prefs.GetDict(::prefs::kLanguageInputMethodSpecificSettings).Clone();
  all_input_method_prefs.Set(kJapaneseEngineId, std::move(japanese_settings));

  prefs.SetDict(::prefs::kLanguageInputMethodSpecificSettings,
                std::move(all_input_method_prefs));
}

bool IsAutocorrectSupported(const std::string& engine_id) {
  static const base::NoDestructor<base::flat_set<std::string>>
      enabledInputMethods({
          "xkb:be::fra",        "xkb:be::ger",
          "xkb:be::nld",        "xkb:br::por",
          "xkb:ca::fra",        "xkb:ca:eng:eng",
          "xkb:ca:multix:fra",  "xkb:ch::ger",
          "xkb:ch:fr:fra",      "xkb:de::ger",
          "xkb:de:neo:ger",     "xkb:dk::dan",
          "xkb:es::spa",        "xkb:fi::fin",
          "xkb:fr::fra",        "xkb:fr:bepo:fra",
          "xkb:gb:dvorak:eng",  "xkb:gb:extd:eng",
          "xkb:it::ita",        "xkb:latam::spa",
          "xkb:no::nob",        "xkb:pl::pol",
          "xkb:pt::por",        "xkb:se::swe",
          "xkb:tr::tur",        "xkb:tr:f:tur",
          "xkb:us:intl:nld",    "xkb:us:intl:por",
          "xkb:us:intl_pc:nld", "xkb:us:intl_pc:por",
          "xkb:us::eng",        "xkb:us:altgr-intl:eng",
          "xkb:us:colemak:eng", "xkb:us:dvorak:eng",
          "xkb:us:dvp:eng",     "xkb:us:intl:eng",
          "xkb:us:intl_pc:eng", "xkb:us:workman-intl:eng",
          "xkb:us:workman:eng",
      });

  return enabledInputMethods->find(engine_id) != enabledInputMethods->end();
}

}  // namespace input_method
}  // namespace ash
