// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_consent_store.h"

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "base/types/cxx23_to_underlying.h"

namespace ash::input_method {
namespace {

constexpr int kConsentWindowDisplayUpperLimit = 3;

ConsentStatus GetConsentStatusFromInteger(int consent_status) {
  switch (consent_status) {
    case base::to_underlying(ConsentStatus::kUnset):
      return ConsentStatus::kUnset;
    case base::to_underlying(ConsentStatus::kApproved):
      return ConsentStatus::kApproved;
    case base::to_underlying(ConsentStatus::kDeclined):
      return ConsentStatus::kDeclined;
    case base::to_underlying(ConsentStatus::kImplicitlyDeclined):
      return ConsentStatus::kImplicitlyDeclined;
    case base::to_underlying(ConsentStatus::kPending):
      return ConsentStatus::kPending;
    default:
      LOG(ERROR) << "Invalid consent status: " << consent_status;
      return ConsentStatus::kInvalid;
  }
}

}  // namespace

EditorConsentStore::EditorConsentStore(PrefService* pref_service)
    : pref_service_(pref_service) {}

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
      return;
    }

    if (consent_action == ConsentAction::kDismissed) {
      SetConsentStatus(ConsentStatus::kPending);
      IncrementConsentWindowDismissCount();
    }

    if (GetConsentWindowDismissCount() >= kConsentWindowDisplayUpperLimit) {
      SetConsentStatus(ConsentStatus::kImplicitlyDeclined);
    }
    return;
  }
}

int EditorConsentStore::GetConsentWindowDismissCount() {
  return pref_service_->GetInteger(prefs::kOrcaConsentWindowDismissCount);
}

void EditorConsentStore::IncrementConsentWindowDismissCount() {
  pref_service_->SetInteger(prefs::kOrcaConsentWindowDismissCount,
                            GetConsentWindowDismissCount() + 1);
}
}  // namespace ash::input_method
