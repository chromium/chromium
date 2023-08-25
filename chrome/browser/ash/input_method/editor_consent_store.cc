// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_consent_store.h"

#include "ash/constants/ash_pref_names.h"
#include "base/types/cxx23_to_underlying.h"

namespace ash::input_method {
namespace {

constexpr int kConsentWindowDisplayUpperLimit = 3;

}  // namespace

EditorConsentStore::EditorConsentStore(PrefService* pref_service)
    : pref_service_(pref_service),
      pref_change_registrar_(std::make_unique<PrefChangeRegistrar>()) {
  pref_change_registrar_->Init(pref_service_);
  pref_change_registrar_->Add(
      ash::prefs::kOrcaEnabled,
      base::BindRepeating(&EditorConsentStore::OnUserPrefChanged,
                          weak_ptr_factory_.GetWeakPtr()));
}

EditorConsentStore::~EditorConsentStore() = default;

ConsentStatus EditorConsentStore::GetConsentStatus() const {
  return GetConsentStatusFromInteger(
      pref_service_->GetInteger(prefs::kOrcaConsentStatus));
}

void EditorConsentStore::SetConsentStatus(ConsentStatus consent_status) {
  pref_service_->SetInteger(prefs::kOrcaConsentStatus,
                            base::to_underlying(consent_status));
}

void EditorConsentStore::ProcessConsentAction(ConsentAction consent_action) {
  ConsentStatus current_consent_status = GetConsentStatus();
  // The consent action can only affect the consent status if the status is
  // pending, unset or invalid (invalid is treated as unset). If the user
  // already approved or (implicitly) declined the consent, there should not be
  // any response from the consent page because it should stop being shown to
  // the user.
  if (current_consent_status == ConsentStatus::kInvalid ||
      current_consent_status == ConsentStatus::kPending ||
      current_consent_status == ConsentStatus::kUnset) {
    if (consent_action == ConsentAction::kApproved) {
      SetConsentStatus(ConsentStatus::kApproved);
      return;
    }

    if (consent_action == ConsentAction::kDeclined) {
      SetConsentStatus(ConsentStatus::kDeclined);
      OverrideUserPref(/*new_pref_value=*/false);
      return;
    }

    if (consent_action == ConsentAction::kDismissed) {
      SetConsentStatus(ConsentStatus::kPending);
      IncrementConsentWindowDismissCount();
    }

    if (GetConsentWindowDismissCount() >= kConsentWindowDisplayUpperLimit) {
      SetConsentStatus(ConsentStatus::kImplicitlyDeclined);
      OverrideUserPref(/*new_pref_value=*/false);
    }
  }
}

void EditorConsentStore::OnUserPrefChanged() {
  ConsentStatus current_consent_status = GetConsentStatus();
  // If the user has previously (implicitly) declined the consent status and
  // now switches the toggle on, then reset the consent status.
  if (pref_service_->GetBoolean(ash::prefs::kOrcaEnabled) &&
      (current_consent_status == ConsentStatus::kImplicitlyDeclined ||
       current_consent_status == ConsentStatus::kDeclined)) {
    SetConsentStatus(ConsentStatus::kUnset);
    ResetConsentWindowDismissCount();
  }
}

int EditorConsentStore::GetConsentWindowDismissCount() {
  return pref_service_->GetInteger(prefs::kOrcaConsentWindowDismissCount);
}

void EditorConsentStore::IncrementConsentWindowDismissCount() {
  pref_service_->SetInteger(prefs::kOrcaConsentWindowDismissCount,
                            GetConsentWindowDismissCount() + 1);
}

void EditorConsentStore::ResetConsentWindowDismissCount() {
  pref_service_->SetInteger(prefs::kOrcaConsentWindowDismissCount, 0);
}

void EditorConsentStore::OverrideUserPref(bool new_pref_value) {
  pref_service_->SetBoolean(prefs::kOrcaEnabled, new_pref_value);
}

}  // namespace ash::input_method
