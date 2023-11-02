// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_pref_names.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/common/pref_names.h"
#endif

namespace chromeos {
namespace prefs {
#if BUILDFLAG(IS_CHROMEOS_ASH)
const char* kAccessibilityFocusHighlightEnabled =
    ash::prefs::kAccessibilityFocusHighlightEnabled;
const char* kAccessibilityAutoclickEnabled =
    ash::prefs::kAccessibilityAutoclickEnabled;
const char* kAccessibilityCaretHighlightEnabled =
    ash::prefs::kAccessibilityCaretHighlightEnabled;
const char* kAccessibilityCursorColorEnabled =
    ash::prefs::kAccessibilityCursorColorEnabled;
const char* kAccessibilityCursorHighlightEnabled =
    ash::prefs::kAccessibilityCursorHighlightEnabled;
const char* kAccessibilityDictationEnabled =
    ash::prefs::kAccessibilityDictationEnabled;
const char* kAccessibilityHighContrastEnabled =
    ash::prefs::kAccessibilityHighContrastEnabled;
const char* kAccessibilityLargeCursorEnabled =
    ash::prefs::kAccessibilityLargeCursorEnabled;
const char* kAccessibilityScreenMagnifierEnabled =
    ash::prefs::kAccessibilityScreenMagnifierEnabled;
const char* kAccessibilitySelectToSpeakEnabled =
    ash::prefs::kAccessibilitySelectToSpeakEnabled;
const char* kAccessibilitySpokenFeedbackEnabled =
    ash::prefs::kAccessibilitySpokenFeedbackEnabled;
const char* kAccessibilityStickyKeysEnabled =
    ash::prefs::kAccessibilityStickyKeysEnabled;
const char* kAccessibilitySwitchAccessEnabled =
    ash::prefs::kAccessibilitySwitchAccessEnabled;
const char* kAccessibilityVirtualKeyboardEnabled =
    ash::prefs::kAccessibilityVirtualKeyboardEnabled;
const char* kDockedMagnifierEnabled = ash::prefs::kDockedMagnifierEnabled;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
const char* kAccessibilityFocusHighlightEnabled =
    ::prefs::kLacrosAccessibilityFocusHighlightEnabled;
const char* kAccessibilityAutoclickEnabled =
    ::prefs::kLacrosAccessibilityAutoclickEnabled;
const char* kAccessibilityCaretHighlightEnabled =
    ::prefs::kLacrosAccessibilityCaretHighlightEnabled;
const char* kAccessibilityCursorColorEnabled =
    ::prefs::kLacrosAccessibilityCursorColorEnabled;
const char* kAccessibilityCursorHighlightEnabled =
    ::prefs::kLacrosAccessibilityCursorHighlightEnabled;
const char* kAccessibilityDictationEnabled =
    ::prefs::kLacrosAccessibilityDictationEnabled;
const char* kAccessibilityHighContrastEnabled =
    ::prefs::kLacrosAccessibilityHighContrastEnabled;
const char* kAccessibilityLargeCursorEnabled =
    ::prefs::kLacrosAccessibilityLargeCursorEnabled;
const char* kAccessibilityScreenMagnifierEnabled =
    ::prefs::kLacrosAccessibilityScreenMagnifierEnabled;
const char* kAccessibilitySelectToSpeakEnabled =
    ::prefs::kLacrosAccessibilitySelectToSpeakEnabled;
const char* kAccessibilitySpokenFeedbackEnabled =
    ::prefs::kLacrosAccessibilitySpokenFeedbackEnabled;
const char* kAccessibilityStickyKeysEnabled =
    ::prefs::kLacrosAccessibilityStickyKeysEnabled;
const char* kAccessibilitySwitchAccessEnabled =
    ::prefs::kLacrosAccessibilitySwitchAccessEnabled;
const char* kAccessibilityVirtualKeyboardEnabled =
    ::prefs::kLacrosAccessibilityVirtualKeyboardEnabled;
const char* kDockedMagnifierEnabled = ::prefs::kLacrosDockedMagnifierEnabled;
#endif

}  // namespace prefs
}  // namespace chromeos
