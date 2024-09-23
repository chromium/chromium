// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_settings.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/containers/fixed_flat_set.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/input_method/assistive_prefs.h"
#include "chrome/browser/ash/input_method/autocorrect_enums.h"
#include "chrome/browser/ash/input_method/autocorrect_prefs.h"
#include "chrome/browser/ash/input_method/japanese/japanese_settings.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace ash {
namespace input_method {

namespace {

namespace mojom = ::ash::ime::mojom;

constexpr std::string_view kJapaneseEngineId = "nacl_mozc_jp";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
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
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
constexpr char kPinyinPrefsLayoutUsQwerty[] = "US";
constexpr char kPinyinPrefsLayoutDvorak[] = "Dvorak";
constexpr char kPinyinPrefsLayoutColemak[] = "Colemak";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
constexpr char kZhuyinPrefsLayoutStandard[] = "Default";
constexpr char kZhuyinPrefsLayoutIbm[] = "IBM";
constexpr char kZhuyinPrefsLayoutEten[] = "Eten";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
constexpr char kZhuyinPrefsSelectionKeys1234567890[] = "1234567890";
constexpr char kZhuyinPrefsSelectionKeysAsdfghjkl[] = "asdfghjkl;";
constexpr char kZhuyinPrefsSelectionKeysAsdfzxcv89[] = "asdfzxcv89";
constexpr char kZhuyinPrefsSelectionKeysAsdfjkl789[] = "asdfjkl789";
constexpr char kZhuyinPrefsSelectionKeys1234Qweras[] = "1234qweras";

// The values here should be kept in sync with
// chrome/browser/resources/ash/settings/os_languages_page/input_method_util.js
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

bool IsVietnameseTelexEngine(std::string_view engine_id) {
  return engine_id == "vkd_vi_telex";
}

bool IsVietnameseVniEngine(std::string_view engine_id) {
  return engine_id == "vkd_vi_vni";
}

void RecordSettingsMetrics(const mojom::VietnameseTelexSettings& settings) {
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseTelex.FlexibleTyping",
      settings.allow_flexible_diacritics);
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseTelex.ModernToneMark",
      settings.new_style_tone_mark_placement);
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseTelex.UODoubleHorn",
      settings.enable_insert_double_horn_on_uo);
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseTelex.WForUHorn",
      settings.enable_w_for_u_horn_shortcut);
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseTelex.ShowUnderline",
      settings.show_underline_for_composition_text);
}

void RecordSettingsMetrics(const mojom::VietnameseVniSettings& settings) {
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseVNI.FlexibleTyping",
      settings.allow_flexible_diacritics);
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseVNI.ModernToneMark",
      settings.new_style_tone_mark_placement);
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseVNI.UODoubleHorn",
      settings.enable_insert_double_horn_on_uo);
  base::UmaHistogramBoolean(
      "InputMethod.PhysicalKeyboard.VietnameseVNI.ShowUnderline",
      settings.show_underline_for_composition_text);
}

mojom::VietnameseVniSettingsPtr CreateVietnameseVniSettings(
    const base::Value::Dict& input_method_specific_pref) {
  auto settings = mojom::VietnameseVniSettings::New();
  settings->allow_flexible_diacritics =
      input_method_specific_pref
          .FindBool("vietnameseVniAllowFlexibleDiacritics")
          .value_or(true);
  settings->new_style_tone_mark_placement =
      input_method_specific_pref
          .FindBool("vietnameseVniNewStyleToneMarkPlacement")
          .value_or(false);
  settings->enable_insert_double_horn_on_uo =
      input_method_specific_pref.FindBool("vietnameseVniInsertDoubleHornOnUo")
          .value_or(false);
  settings->show_underline_for_composition_text =
      input_method_specific_pref.FindBool("vietnameseVniShowUnderline")
          .value_or(true);
  RecordSettingsMetrics(*settings);
  return settings;
}

mojom::VietnameseTelexSettingsPtr CreateVietnameseTelexSettings(
    const base::Value::Dict& input_method_specific_pref) {
  auto settings = mojom::VietnameseTelexSettings::New();
  settings->allow_flexible_diacritics =
      input_method_specific_pref
          .FindBool("vietnameseTelexAllowFlexibleDiacritics")
          .value_or(true);
  settings->new_style_tone_mark_placement =
      input_method_specific_pref
          .FindBool("vietnameseTelexNewStyleToneMarkPlacement")
          .value_or(false);
  settings->enable_insert_double_horn_on_uo =
      input_method_specific_pref.FindBool("vietnameseTelexInsertDoubleHornOnUo")
          .value_or(false);
  settings->enable_w_for_u_horn_shortcut =
      input_method_specific_pref.FindBool("vietnameseTelexInsertUHornOnW")
          .value_or(true);
  settings->show_underline_for_composition_text =
      input_method_specific_pref.FindBool("vietnameseTelexShowUnderline")
          .value_or(true);
  RecordSettingsMetrics(*settings);
  return settings;
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
      base::FeatureList::IsEnabled(features::kAssistMultiWord) &&
      IsUsEnglishEngine(engine_id) &&
      IsPredictiveWritingPrefEnabled(prefs, engine_id);

  return settings;
}

mojom::KoreanLayout KoreanLayoutToMojom(const std::string& layout) {
  if (layout == kKoreanPrefsLayoutDubeolsik) {
    return mojom::KoreanLayout::kDubeolsik;
  }
  if (layout == kKoreanPrefsLayoutDubeolsikOldHangeul) {
    return mojom::KoreanLayout::kDubeolsikOldHangeul;
  }
  if (layout == kKoreanPrefsLayoutSebeolsik390) {
    return mojom::KoreanLayout::kSebeolsik390;
  }
  if (layout == kKoreanPrefsLayoutSebeolsikFinal) {
    return mojom::KoreanLayout::kSebeolsikFinal;
  }
  if (layout == kKoreanPrefsLayoutSebeolsikNoShift) {
    return mojom::KoreanLayout::kSebeolsikNoShift;
  }
  if (layout == kKoreanPrefsLayoutSebeolsikOldHangeul) {
    return mojom::KoreanLayout::kSebeolsikOldHangeul;
  }
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
  if (layout == kPinyinPrefsLayoutUsQwerty) {
    return mojom::PinyinLayout::kUsQwerty;
  }
  if (layout == kPinyinPrefsLayoutDvorak) {
    return mojom::PinyinLayout::kDvorak;
  }
  if (layout == kPinyinPrefsLayoutColemak) {
    return mojom::PinyinLayout::kColemak;
  }
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
  if (layout == kZhuyinPrefsLayoutStandard) {
    return mojom::ZhuyinLayout::kStandard;
  }
  if (layout == kZhuyinPrefsLayoutIbm) {
    return mojom::ZhuyinLayout::kIbm;
  }
  if (layout == kZhuyinPrefsLayoutEten) {
    return mojom::ZhuyinLayout::kEten;
  }
  return mojom::ZhuyinLayout::kStandard;
}

mojom::ZhuyinSelectionKeys ZhuyinSelectionKeysToMojom(
    const std::string& selection_keys) {
  if (selection_keys == kZhuyinPrefsSelectionKeys1234567890) {
    return mojom::ZhuyinSelectionKeys::k1234567890;
  }
  if (selection_keys == kZhuyinPrefsSelectionKeysAsdfghjkl) {
    return mojom::ZhuyinSelectionKeys::kAsdfghjkl;
  }
  if (selection_keys == kZhuyinPrefsSelectionKeysAsdfzxcv89) {
    return mojom::ZhuyinSelectionKeys::kAsdfzxcv89;
  }
  if (selection_keys == kZhuyinPrefsSelectionKeysAsdfjkl789) {
    return mojom::ZhuyinSelectionKeys::kAsdfjkl789;
  }
  if (selection_keys == kZhuyinPrefsSelectionKeys1234Qweras) {
    return mojom::ZhuyinSelectionKeys::k1234Qweras;
  }
  return mojom::ZhuyinSelectionKeys::k1234567890;
}

uint32_t ZhuyinPageSizeToInt(const std::string& page_size) {
  if (page_size == kZhuyinPrefsPageSize10) {
    return 10;
  }
  if (page_size == kZhuyinPrefsPageSize9) {
    return 9;
  }
  if (page_size == kZhuyinPrefsPageSize8) {
    return 8;
  }
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
}  // namespace

mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const PrefService& prefs,
    const std::string& engine_id) {
  // All input method settings are stored in a single pref whose value is a
  // dictionary.
  // For each input method, the dictionary contains an entry, with the key being
  // a string that identifies the input method, and the value being a
  // subdictionary with the specific settings for that input method.  The
  // subdictionary structure depends on the type of input method it's for.  The
  // subdictionary may be null if the user hasn't changed any settings for that
  // input method.
  const base::Value::Dict* ime_prefs_ptr =
      prefs.GetDict(::prefs::kLanguageInputMethodSpecificSettings)
          .FindDict(engine_id);

  base::Value::Dict default_dict;
  const base::Value::Dict& input_method_specific_pref =
      ime_prefs_ptr == nullptr ? default_dict : *ime_prefs_ptr;

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
  if (IsVietnameseTelexEngine(engine_id)) {
    return mojom::InputMethodSettings::NewVietnameseTelexSettings(
        CreateVietnameseTelexSettings(input_method_specific_pref));
  }
  if (IsVietnameseVniEngine(engine_id)) {
    return mojom::InputMethodSettings::NewVietnameseVniSettings(
        CreateVietnameseVniSettings(input_method_specific_pref));
  }
  if (engine_id == kJapaneseEngineId) {
    return mojom::InputMethodSettings::NewJapaneseSettings(
        ToMojomInputMethodSettings(input_method_specific_pref));
  }
  // TODO(b/232341104): Add the code to send the Japanese settings to
  // the engine if the engine_id is nacl_mozc_jp or nacl_mozc_us.
  // This will do the inverse of ConvertConfigToJapaneseSettings.
  // This will be something like InputMethodSettings::NewJapaneseSettings(...)

  return nullptr;
}

const base::Value* GetLanguageInputMethodSpecificSetting(
    PrefService& prefs,
    const std::string& engine_id,
    const std::string& preference_name) {
  return prefs.GetDict(::prefs::kLanguageInputMethodSpecificSettings)
      .FindByDottedPath(base::StrCat({engine_id, ".", preference_name}));
}

void SetLanguageInputMethodSpecificSetting(PrefService& prefs,
                                           const std::string& engine_id,
                                           const base::Value::Dict& values) {
  // This creates a dictionary where any changes to the dictionary will notify
  // the prefs service (and its observers).
  ScopedDictPrefUpdate update(&prefs,
                              ::prefs::kLanguageInputMethodSpecificSettings);

  // The "update" dictionary contains nested dictionaries of engine_id -> Dict.
  // This partial dictionary contains all the new updated files set up in the
  // same schema so it can be merged.
  base::Value::Dict partial_dict;
  partial_dict.Set(engine_id, values.Clone());

  // Does a nested dictionary merge to the "update" dictionary. This does not
  // modify any existing values that are not inside the partial_dict.
  update->Merge(std::move(partial_dict));
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

bool IsPhysicalKeyboardAutocorrectAllowed(const PrefService& prefs) {
  if (!prefs.FindPreference(
          prefs::kManagedPhysicalKeyboardAutocorrectAllowed)) {
    return true;
  }
  return prefs.GetBoolean(prefs::kManagedPhysicalKeyboardAutocorrectAllowed);
}

bool IsPhysicalKeyboardPredictiveWritingAllowed(const PrefService& prefs) {
  if (!prefs.FindPreference(
          prefs::kManagedPhysicalKeyboardPredictiveWritingAllowed)) {
    return true;
  }
  return prefs.GetBoolean(
      prefs::kManagedPhysicalKeyboardPredictiveWritingAllowed);
}

}  // namespace input_method
}  // namespace ash
