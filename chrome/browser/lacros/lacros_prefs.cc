// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/lacros_prefs.h"

#include "chrome/common/pref_names.h"
#include "chromeos/components/disks/disks_prefs.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace lacros_prefs {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // The preferences on external storages are used in imageWriterPrivate
  // extension api implementation code, which are supported in Lacros.
  disks::prefs::RegisterProfilePrefs(registry);
}

void RegisterExtensionControlledAshPrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // These settings are used by extensions when the feature itself is controlled
  // by a pref in ash. In lacros, these prefs hold the computed value across all
  // extensions (and also the value set by each individual extension in lacros),
  // and the final value is sent to ash.
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityFocusHighlightEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosDockedMagnifierEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityAutoclickEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityCaretHighlightEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityCursorColorEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityCursorHighlightEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityDictationEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityHighContrastEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityLargeCursorEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityScreenMagnifierEnabled, false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilitySelectToSpeakEnabled, false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilitySpokenFeedbackEnabled, false);
  registry->RegisterBooleanPref(::prefs::kLacrosAccessibilityStickyKeysEnabled,
                                false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilitySwitchAccessEnabled, false);
  registry->RegisterBooleanPref(
      ::prefs::kLacrosAccessibilityVirtualKeyboardEnabled, false);
}

}  // namespace lacros_prefs
