// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/accessibility_prefs/android/accessibility_prefs_controller.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "ui/accessibility/accessibility_prefs.h"

namespace accessibility {

// static
void AccessibilityPrefsController::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
#if BUILDFLAG(IS_ANDROID)
  registry->RegisterBooleanPref(
      prefs::kAccessibilityPerformanceFilteringAllowed, true);
#endif
}

AccessibilityPrefsController::AccessibilityPrefsController(
    PrefService* local_state_prefs)
    : local_state_prefs_(local_state_prefs) {
#if BUILDFLAG(IS_ANDROID)
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(local_state_prefs_);

  pref_change_registrar_->Add(
      prefs::kAccessibilityPerformanceFilteringAllowed,
      base::BindRepeating(&AccessibilityPrefsController::
                              OnAccessibilityPerformanceFilteringAllowedChanged,
                          base::Unretained(this)));
#endif

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&AccessibilityPrefsController::Init,
                                base::Unretained(this)));
}

AccessibilityPrefsController::~AccessibilityPrefsController() = default;

void AccessibilityPrefsController::Init() {
  if (g_browser_process && g_browser_process->profile_manager()) {
    profile_manager_observation_.Observe(g_browser_process->profile_manager());
    for (Profile* profile :
         g_browser_process->profile_manager()->GetLoadedProfiles()) {
      OnProfileAdded(profile);
    }
  }
#if BUILDFLAG(IS_ANDROID)
  OnAccessibilityPerformanceFilteringAllowedChanged();
#endif
  OnRendererAccessibilityEnabledChanged();
}

void AccessibilityPrefsController::OnProfileAdded(Profile* profile) {
  if (profile && profile->GetPrefs() && !profile_pref_change_registrar_) {
    profile_pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    profile_pref_change_registrar_->Init(profile->GetPrefs());
    profile_pref_change_registrar_->Add(
        prefs::kRendererAccessibilityEnabled,
        base::BindRepeating(&AccessibilityPrefsController::
                                OnRendererAccessibilityEnabledChanged,
                            base::Unretained(this)));
    OnRendererAccessibilityEnabledChanged();
  }
}

void AccessibilityPrefsController::OnProfileManagerDestroying() {
  profile_manager_observation_.Reset();
}

#if BUILDFLAG(IS_ANDROID)
void AccessibilityPrefsController::
    OnAccessibilityPerformanceFilteringAllowedChanged() {
  bool new_state = local_state_prefs_->GetBoolean(
      prefs::kAccessibilityPerformanceFilteringAllowed);
  content::BrowserAccessibilityState::GetInstance()
      ->SetPerformanceFilteringAllowed(new_state);
}
#endif

void AccessibilityPrefsController::OnRendererAccessibilityEnabledChanged() {
  bool enabled = (profile_pref_change_registrar_ &&
                  profile_pref_change_registrar_->prefs())
                     ? profile_pref_change_registrar_->prefs()->GetBoolean(
                           prefs::kRendererAccessibilityEnabled)
                     : true;
  content::BrowserAccessibilityState::GetInstance()->SetAXModeChangeAllowed(
      enabled);
}

}  // namespace accessibility
