// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/cookie_sync_data_type_controller.h"

#include "base/functional/bind.h"
#include "chrome/common/pref_names.h"
#include "components/sync/service/sync_service.h"

namespace ash::floating_sso {

CookieSyncDataTypeController::CookieSyncDataTypeController(
    std::unique_ptr<syncer::DataTypeControllerDelegate>
        delegate_for_full_sync_mode,
    syncer::SyncService* sync_service,
    PrefService* prefs)
    : syncer::DataTypeController(syncer::COOKIES,
                                 std::move(delegate_for_full_sync_mode),
                                 /*delegate_for_transport_mode=*/nullptr),
      sync_service_(sync_service),
      prefs_(prefs) {
  pref_change_registrar_.Init(prefs_);
  pref_change_registrar_.Add(
      prefs::kFloatingSsoEnabled,
      base::BindRepeating(
          &CookieSyncDataTypeController::OnFloatingSsoPrefChanged,
          base::Unretained(this)));
}

CookieSyncDataTypeController::~CookieSyncDataTypeController() = default;

syncer::DataTypeController::PreconditionState
CookieSyncDataTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());

  if (!prefs_->GetBoolean(prefs::kFloatingSsoEnabled)) {
    return PreconditionState::kMustStopAndClearData;
  }

  return PreconditionState::kPreconditionsMet;
}

void CookieSyncDataTypeController::OnFloatingSsoPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

}  // namespace ash::floating_sso
