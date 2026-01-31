// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_SETTINGS_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_SETTINGS_H_
#include "chromeos/ash/services/ime/public/mojom/input_method.mojom.h"

namespace ash::input_method {

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
  kToutenKuten = 0,
  kCommaPeriod = 1,
  kToutenPeriod = 2,
  kCommaKuten = 3,
  kMaxValue = kCommaKuten,
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

ash::ime::mojom::JapaneseSettingsPtr ToMojomInputMethodSettings(
    const base::DictValue& prefs_dict);

void RecordJapaneseSettingsMetrics(
    const ash::ime::mojom::JapaneseSettings& settings);

}  // namespace ash::input_method

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_JAPANESE_SETTINGS_H_
