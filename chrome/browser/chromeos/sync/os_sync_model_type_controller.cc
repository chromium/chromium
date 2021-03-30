// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/os_sync_model_type_controller.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_controller_delegate.h"

using syncer::ModelTypeControllerDelegate;

OsSyncModelTypeController::OsSyncModelTypeController(
    syncer::ModelType type,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_full_sync_mode,
    std::unique_ptr<ModelTypeControllerDelegate> delegate_for_transport_mode,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : syncer::ModelTypeController(type,
                                  std::move(delegate_for_full_sync_mode),
                                  std::move(delegate_for_transport_mode)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      syncer::prefs::kOsSyncFeatureEnabled,
      base::BindRepeating(&OsSyncModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

OsSyncModelTypeController::~OsSyncModelTypeController() = default;

syncer::DataTypeController::PreconditionState
OsSyncModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void OsSyncModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
