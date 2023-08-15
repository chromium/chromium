// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_consent_store.h"

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "base/types/cxx23_to_underlying.h"

namespace ash::input_method {
namespace {

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

}  // namespace ash::input_method
