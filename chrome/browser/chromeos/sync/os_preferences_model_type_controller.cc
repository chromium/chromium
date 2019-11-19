// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/os_preferences_model_type_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"

OsPreferencesModelTypeController::OsPreferencesModelTypeController(
    syncer::ModelType type,
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : syncer::SyncableServiceBasedModelTypeController(type,
                                                      std::move(store_factory),
                                                      syncable_service,
                                                      dump_stack),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  DCHECK(type == syncer::OS_PREFERENCES ||
         type == syncer::OS_PRIORITY_PREFERENCES);
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      syncer::prefs::kOsSyncFeatureEnabled,
      base::BindRepeating(&OsPreferencesModelTypeController::OnUserPrefChanged,
                          base::Unretained(this)));
}

OsPreferencesModelTypeController::~OsPreferencesModelTypeController() = default;

syncer::DataTypeController::PreconditionState
OsPreferencesModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void OsPreferencesModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
