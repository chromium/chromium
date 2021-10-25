// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/input_method_settings.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/common/pref_names.h"

namespace ash {
namespace input_method {
namespace {

namespace mojom = chromeos::ime::mojom;

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

bool IsUsEnglishEngine(const std::string& engine_id) {
  return engine_id == "xkb:us::eng";
}

bool IsFstEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "xkb:", base::CompareCase::SENSITIVE);
}

bool IsKoreanEngine(const std::string& engine_id) {
  return base::StartsWith(engine_id, "ko-", base::CompareCase::SENSITIVE);
}

mojom::LatinSettingsPtr CreateLatinSettings(
    const base::Value& input_method_specific_pref,
    const PrefService& prefs,
    const std::string& engine_id) {
  auto settings = mojom::LatinSettings::New();
  settings->autocorrect = input_method_specific_pref
                              .FindIntKey("physicalKeyboardAutoCorrectionLevel")
                              .value_or(0) > 0;
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

}  // namespace

mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const PrefService& prefs,
    const std::string& engine_id) {
  // All input method settings are stored in a single pref whose value is a
  // dictionary.
  const base::DictionaryValue& all_input_method_pref =
      *prefs.GetDictionary(::prefs::kLanguageInputMethodSpecificSettings);

  // For each input method, the dictionary contains an entry, with the key being
  // a string that identifies the input method, and the value being a
  // subdictionary with the specific settings for that input method.
  // The subdictionary structure depends on the type of input method it's for.
  // The subdictionary may be null if the user hasn't changed any settings for
  // that input method.
  const base::Value* input_method_specific_pref_or_null =
      all_input_method_pref.FindDictKey(engine_id);

  // For convenience, pass an empty dictionary if there are no settings for this
  // input method yet.
  base::DictionaryValue empty_value;
  const auto& input_method_specific_pref =
      input_method_specific_pref_or_null ? *input_method_specific_pref_or_null
                                         : empty_value;

  if (IsFstEngine(engine_id)) {
    return mojom::InputMethodSettings::NewLatinSettings(
        CreateLatinSettings(input_method_specific_pref, prefs, engine_id));
  }
  if (IsKoreanEngine(engine_id)) {
    return mojom::InputMethodSettings::NewKoreanSettings(
        CreateKoreanSettings(input_method_specific_pref));
  }

  return nullptr;
}

}  // namespace input_method
}  // namespace ash
