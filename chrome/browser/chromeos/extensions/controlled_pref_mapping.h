// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTROLLED_PREF_MAPPING_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTROLLED_PREF_MAPPING_H_

#include "ash/constants/ash_pref_names.h"

namespace chromeos::prefs {

// These constants are used with extension controlled prefs where the underlying
// preference being controlled live in ash. In lacros they map to a pref used
// to hold the value of the pref of all extensions, which is shipped to ash.
// in ash, they map to the actual pref controlling the feature.

// FocusHighlight is special as the feature exists on several platforms.
// However, extensions can only set the ash-value.

inline constexpr const char* kAccessibilityFocusHighlightEnabled =
    ash::prefs::kAccessibilityFocusHighlightEnabled;
inline constexpr const char* kAccessibilityAutoclickEnabled =
    ash::prefs::kAccessibilityAutoclickEnabled;
inline constexpr const char* kAccessibilityCaretHighlightEnabled =
    ash::prefs::kAccessibilityCaretHighlightEnabled;
inline constexpr const char* kAccessibilityCursorHighlightEnabled =
    ash::prefs::kAccessibilityCursorHighlightEnabled;
inline constexpr const char* kAccessibilityDictationEnabled =
    ash::prefs::kAccessibilityDictationEnabled;
inline constexpr const char* kAccessibilityHighContrastEnabled =
    ash::prefs::kAccessibilityHighContrastEnabled;
inline constexpr const char* kAccessibilityLargeCursorEnabled =
    ash::prefs::kAccessibilityLargeCursorEnabled;
inline constexpr const char* kAccessibilityScreenMagnifierEnabled =
    ash::prefs::kAccessibilityScreenMagnifierEnabled;
inline constexpr const char* kAccessibilitySelectToSpeakEnabled =
    ash::prefs::kAccessibilitySelectToSpeakEnabled;
inline constexpr const char* kAccessibilitySpokenFeedbackEnabled =
    ash::prefs::kAccessibilitySpokenFeedbackEnabled;
inline constexpr const char* kAccessibilityStickyKeysEnabled =
    ash::prefs::kAccessibilityStickyKeysEnabled;
inline constexpr const char* kAccessibilitySwitchAccessEnabled =
    ash::prefs::kAccessibilitySwitchAccessEnabled;
inline constexpr const char* kAccessibilityVirtualKeyboardEnabled =
    ash::prefs::kAccessibilityVirtualKeyboardEnabled;
inline constexpr const char* kDockedMagnifierEnabled =
    ash::prefs::kDockedMagnifierEnabled;

}  // namespace chromeos::prefs

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTROLLED_PREF_MAPPING_H_
