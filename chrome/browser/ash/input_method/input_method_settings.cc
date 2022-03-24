// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_settings.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "chrome/common/pref_names.h"

namespace ash {
namespace input_method {
namespace {

namespace mojom = ::ash::ime::mojom;

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

std::string GetPrefKeyForEngineId(const std::string& engine_id) {
  if (engine_id == "zh-t-i0-pinyin") {
    return "pinyin";
  }
  if (engine_id == "zh-hant-t-i0-und") {
    return "zhuyin";
  }
  return engine_id;
}

mojom::LatinSettingsPtr CreateLatinSettings(
    const base::Value& input_method_specific_pref,
    const PrefService& prefs,
    const std::string& engine_id,
    const InputFieldContext& context) {
  auto settings = mojom::LatinSettings::New();
  settings->autocorrect =
      base::StartsWith(engine_id, "experimental_",
                       base::CompareCase::SENSITIVE) ||
      base::FeatureList::IsEnabled(features::kAutocorrectParamsTuning) ||
      input_method_specific_pref
              .FindIntKey("physicalKeyboardAutoCorrectionLevel")
              .value_or(0) > 0;
  settings->predictive_writing =
      context.multiword_enabled && context.multiword_allowed &&
      !context.lacros_enabled &&
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
    const base::Value& input_method_specific_pref) {
  auto settings = mojom::KoreanSettings::New();
  settings->input_multiple_syllables =
      !input_method_specific_pref.FindBoolKey("koreanEnableSyllableInput")
           .value_or(true);
  const std::string* prefs_layout =
      input_method_specific_pref.FindStringKey("koreanKeyboardLayout");
  settings->layout = prefs_layout ? KoreanLayoutToMojom(*prefs_layout)
                                  : mojom::KoreanLayout::kDubeolsik;
  return settings;
}

mojom::FuzzyPinyinSettingsPtr CreateFuzzyPinyinSettings(
    const base::Value& pref) {
  auto settings = mojom::FuzzyPinyinSettings::New();
  settings->an_ang = pref.FindBoolKey("an:ang").value_or(false);
  settings->en_eng = pref.FindBoolKey("en:eng").value_or(false);
  settings->ian_iang = pref.FindBoolKey("ian:iang").value_or(false);
  settings->k_g = pref.FindBoolKey("k:g").value_or(false);
  settings->r_l = pref.FindBoolKey("r:l").value_or(false);
  settings->uan_uang = pref.FindBoolKey("uan:uang").value_or(false);
  settings->c_ch = pref.FindBoolKey("c:ch").value_or(false);
  settings->f_h = pref.FindBoolKey("f:h").value_or(false);
  settings->in_ing = pref.FindBoolKey("in:ing").value_or(false);
  settings->l_n = pref.FindBoolKey("l:n").value_or(false);
  settings->s_sh = pref.FindBoolKey("s:sh").value_or(false);
  settings->z_zh = pref.FindBoolKey("z:zh").value_or(false);
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
    const base::Value& input_method_specific_pref) {
  auto settings = mojom::PinyinSettings::New();
  settings->fuzzy_pinyin =
      CreateFuzzyPinyinSettings(input_method_specific_pref);
  const std::string* prefs_layout =
      input_method_specific_pref.FindStringKey("xkbLayout");
  settings->layout = prefs_layout ? PinyinLayoutToMojom(*prefs_layout)
                                  : mojom::PinyinLayout::kUsQwerty;
  settings->use_hyphen_and_equals_to_page_candidates =
      input_method_specific_pref.FindBoolKey("pinyinEnableUpperPaging")
          .value_or(true);
  settings->use_comma_and_period_to_page_candidates =
      input_method_specific_pref.FindBoolKey("pinyinEnableLowerPaging")
          .value_or(true);
  settings->default_to_chinese =
      input_method_specific_pref.FindBoolKey("pinyinDefaultChinese")
          .value_or(true);
  settings->default_to_full_width_characters =
      input_method_specific_pref.FindBoolKey("pinyinFullWidthCharacter")
          .value_or(false);
  settings->default_to_full_width_punctuation =
      input_method_specific_pref.FindBoolKey("pinyinChinesePunctuation")
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
    const base::Value& input_method_specific_pref) {
  auto settings = mojom::ZhuyinSettings::New();
  settings->layout = ZhuyinLayoutToMojom(ValueOrEmpty(
      input_method_specific_pref.FindStringKey("zhuyinKeyboardLayout")));
  settings->selection_keys = ZhuyinSelectionKeysToMojom(ValueOrEmpty(
      input_method_specific_pref.FindStringKey("zhuyinSelectKeys")));
  settings->page_size = ZhuyinPageSizeToInt(
      ValueOrEmpty(input_method_specific_pref.FindStringKey("zhuyinPageSize")));
  return settings;
}

}  // namespace

mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const PrefService& prefs,
    const std::string& engine_id,
    const InputFieldContext& context) {
  // All input method settings are stored in a single pref whose value is a
  // dictionary.
  const base::Value& all_input_method_pref =
      *prefs.GetDictionary(::prefs::kLanguageInputMethodSpecificSettings);

  // For each input method, the dictionary contains an entry, with the key being
  // a string that identifies the input method, and the value being a
  // subdictionary with the specific settings for that input method.
  // The subdictionary structure depends on the type of input method it's for.
  // The subdictionary may be null if the user hasn't changed any settings for
  // that input method.
  const base::Value* input_method_specific_pref_or_null =
      all_input_method_pref.FindDictKey(GetPrefKeyForEngineId(engine_id));

  // For convenience, pass an empty dictionary if there are no settings for this
  // input method yet.
  base::DictionaryValue empty_value;
  const auto& input_method_specific_pref =
      input_method_specific_pref_or_null ? *input_method_specific_pref_or_null
                                         : empty_value;

  if (IsFstEngine(engine_id)) {
    return mojom::InputMethodSettings::NewLatinSettings(CreateLatinSettings(
        input_method_specific_pref, prefs, engine_id, context));
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

  return nullptr;
}

}  // namespace input_method
}  // namespace ash
