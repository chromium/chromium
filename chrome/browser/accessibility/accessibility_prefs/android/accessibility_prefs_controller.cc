// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_prefs/android/accessibility_prefs_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/browser_process.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/accessibility_prefs.h"

namespace accessibility {

// static
void AccessibilityPrefsController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(
      prefs::kAccessibilityPerformanceFilteringAllowed, true);
}

AccessibilityPrefsController::AccessibilityPrefsController(
    PrefService* local_state_prefs)
    : local_state_prefs_(local_state_prefs) {
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(local_state_prefs_);

  pref_change_registrar_->Add(
      prefs::kAccessibilityPerformanceFilteringAllowed,
      base::BindRepeating(&AccessibilityPrefsController::
                              OnAccessibilityPerformanceFilteringAllowedChanged,
                          base::Unretained(this)));
}

AccessibilityPrefsController::~AccessibilityPrefsController() = default;

void AccessibilityPrefsController::
    OnAccessibilityPerformanceFilteringAllowedChanged() {
  bool new_state = local_state_prefs_->GetBoolean(
      prefs::kAccessibilityPerformanceFilteringAllowed);
  content::BrowserAccessibilityState::GetInstance()
      ->SetPerformanceFilteringAllowed(new_state);
}

}  // namespace accessibility
