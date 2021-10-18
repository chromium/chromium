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

bool IsPredictiveWritingEnabled(const PrefService& pref_service,
                                const std::string& engine_id) {
  return (features::IsAssistiveMultiWordEnabled() &&
          pref_service.GetBoolean(prefs::kAssistPredictiveWritingEnabled) &&
          IsUsEnglishEngine(engine_id));
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

}  // namespace

mojom::InputMethodSettingsPtr CreateSettingsFromPrefs(
    const PrefService& prefs,
    const std::string& engine_id) {
  const base::DictionaryValue* prefs_settings =
      prefs.GetDictionary(::prefs::kLanguageInputMethodSpecificSettings);

  // TODO(b/151884011): Extend this to other input methods like Pinyin.
  if (IsFstEngine(engine_id)) {
    auto latin_settings = mojom::LatinSettings::New();
    latin_settings->autocorrect =
        prefs_settings
            ->FindIntPath(engine_id + ".physicalKeyboardAutoCorrectionLevel")
            .value_or(0) > 0;
    latin_settings->predictive_writing =
        IsPredictiveWritingEnabled(prefs, engine_id);
    return mojom::InputMethodSettings::NewLatinSettings(
        std::move(latin_settings));
  } else if (IsKoreanEngine(engine_id)) {
    auto korean_settings = mojom::KoreanSettings::New();
    korean_settings->input_multiple_syllables =
        !prefs_settings->FindBoolPath(engine_id + ".koreanEnableSyllableInput")
             .value_or(true);
    const std::string* prefs_layout =
        prefs_settings->FindStringPath(engine_id + ".koreanKeyboardLayout");
    korean_settings->layout = prefs_layout ? KoreanLayoutToMojom(*prefs_layout)
                                           : mojom::KoreanLayout::kDubeolsik;
    return mojom::InputMethodSettings::NewKoreanSettings(
        std::move(korean_settings));
  }

  return nullptr;
}

}  // namespace input_method
}  // namespace ash
