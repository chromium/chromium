// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/printing/printers_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/model_type_controller_delegate.h"

PrintersModelTypeController::PrintersModelTypeController(
    std::unique_ptr<syncer::ModelTypeControllerDelegate> delegate,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : syncer::ModelTypeController(syncer::PRINTERS, std::move(delegate)),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      syncer::prefs::kOsSyncFeatureEnabled,
      base::BindRepeating(&PrintersModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

PrintersModelTypeController::~PrintersModelTypeController() = default;

syncer::DataTypeController::PreconditionState
PrintersModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void PrintersModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
