// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_prefs.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {
// Get the active user pref service.
PrefService* GetActiveUserPrefService() {
  if (!ash::Shell::HasInstance()) {
    return nullptr;
  }
  CHECK(ash::Shell::Get()->session_controller());
  return ash::Shell::Get()->session_controller()->GetActivePrefService();
}
// True if customization is set by admin, false if unset.
bool IsCustomizationPolicySet() {
  PrefService* pref_service = GetActiveUserPrefService();
  return pref_service && pref_service->IsManagedPreference(
                             ash::prefs::kShortcutCustomizationAllowed);
}

}  // namespace

namespace ash {

AcceleratorPrefs::AcceleratorPrefs(
    std::unique_ptr<AcceleratorPrefsDelegate> delegate)
    : delegate_(std::move(delegate)) {
  if (Shell::HasInstance()) {
    Shell::Get()->session_controller()->AddObserver(this);
  }
}

AcceleratorPrefs::~AcceleratorPrefs() {
  if (Shell::HasInstance()) {
    Shell::Get()->session_controller()->RemoveObserver(this);
  }
  observers_.Clear();
}

// static:
void AcceleratorPrefs::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kShortcutCustomizationAllowed, true);
  registry->RegisterDictionaryPref(
      prefs::kDeprecatedAcceleratorNotificationsShownCounts);
  registry->RegisterDictionaryPref(
      prefs::kDeprecatedAcceleratorNotificationsLastShown);
}

void AcceleratorPrefs::OnActiveUserPrefServiceChanged(PrefService* prefs) {
  DCHECK(prefs);
  pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
  pref_change_registrar_->Init(prefs);
  pref_change_registrar_->Add(
      ash::prefs::kShortcutCustomizationAllowed,
      base::BindRepeating(&AcceleratorPrefs::OnCustomizationPolicyChanged,
                          base::Unretained(this)));

  OnCustomizationPolicyChanged();
}

void AcceleratorPrefs::AddObserver(AcceleratorPrefs::Observer* observer) {
  observers_.AddObserver(observer);
}

void AcceleratorPrefs::RemoveObserver(AcceleratorPrefs::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void AcceleratorPrefs::OnCustomizationPolicyChanged() {
  if (!IsUserEnterpriseManaged()) {
    return;
  }
  for (auto& observer : observers_) {
    observer.OnShortcutPolicyUpdated();
  }
}

bool AcceleratorPrefs::IsUserEnterpriseManaged() {
  return delegate_->IsUserEnterpriseManaged();
}

bool AcceleratorPrefs::IsCustomizationAllowedByPolicy() {
  if (IsCustomizationPolicySet()) {
    return GetActiveUserPrefService()->GetBoolean(
        prefs::kShortcutCustomizationAllowed);
  }
  // If the policy is unset, return default value true.
  return true;
}

bool AcceleratorPrefs::IsCustomizationAllowed() {
  // If user is managed and customization policy is set, check the policy.
  if (IsUserEnterpriseManaged() && IsCustomizationPolicySet()) {
    return IsCustomizationAllowedByPolicy();
  }

  // If user is not managed or the policy is unset, check the flag.
  return ::features::IsShortcutCustomizationEnabled();
}

}  // namespace ash
