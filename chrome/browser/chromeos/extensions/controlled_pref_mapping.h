// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTROLLED_PREF_MAPPING_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTROLLED_PREF_MAPPING_H_

#include <stddef.h>

namespace chromeos {
namespace prefs {

// These constants are used with extension controlled prefs where the underlying
// preference being controlled live in ash. In lacros they map to a pref used
// to hold the value of the pref of all extensions, which is shipped to ash.
// in ash, they map to the actual pref controlling the feature.

// FocusHighlight is special as the feature exists on several platforms.
// However, extensions can only set the ash-value.
extern const char* kAccessibilityFocusHighlightEnabled;

extern const char* kAccessibilityAutoclickEnabled;
extern const char* kAccessibilityCaretHighlightEnabled;
extern const char* kAccessibilityCursorColorEnabled;
extern const char* kAccessibilityCursorHighlightEnabled;
extern const char* kAccessibilityDictationEnabled;
extern const char* kAccessibilityHighContrastEnabled;
extern const char* kAccessibilityLargeCursorEnabled;
extern const char* kAccessibilityScreenMagnifierEnabled;
extern const char* kAccessibilitySelectToSpeakEnabled;
extern const char* kAccessibilitySpokenFeedbackEnabled;
extern const char* kAccessibilityStickyKeysEnabled;
extern const char* kAccessibilitySwitchAccessEnabled;
extern const char* kAccessibilityVirtualKeyboardEnabled;
extern const char* kDockedMagnifierEnabled;

}  // namespace prefs
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_CONTROLLED_PREF_MAPPING_H_
