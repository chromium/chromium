// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/sync/os_syncable_service_model_type_controller.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/model_type_controller_delegate.h"
#include "components/sync/model/syncable_service_based_bridge.h"

using syncer::ClientTagBasedModelTypeProcessor;
using syncer::ForwardingModelTypeControllerDelegate;
using syncer::SyncableServiceBasedBridge;

OsSyncableServiceModelTypeController::OsSyncableServiceModelTypeController(
    syncer::ModelType type,
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    PrefService* pref_service,
    syncer::SyncService* sync_service)
    : ModelTypeController(type),
      bridge_(std::make_unique<SyncableServiceBasedBridge>(
          type,
          std::move(store_factory),
          std::make_unique<ClientTagBasedModelTypeProcessor>(type, dump_stack),
          syncable_service.get())),
      pref_service_(pref_service),
      sync_service_(sync_service) {
  DCHECK(chromeos::features::IsSplitSettingsSyncEnabled());
  DCHECK(type == syncer::APP_LIST || type == syncer::OS_PREFERENCES ||
         type == syncer::OS_PRIORITY_PREFERENCES);
  DCHECK(pref_service_);
  DCHECK(sync_service_);
  syncer::ModelTypeControllerDelegate* delegate =
      bridge_->change_processor()->GetControllerDelegate().get();
  // Runs in transport-mode and full-sync mode, sharing the bridge's delegate.
  InitModelTypeController(
      /*delegate_for_full_sync_mode=*/
      std::make_unique<ForwardingModelTypeControllerDelegate>(delegate),
      /*delegate_for_transport_mode=*/
      std::make_unique<ForwardingModelTypeControllerDelegate>(delegate));

  pref_registrar_.Init(pref_service_);
  pref_registrar_.Add(
      syncer::prefs::kOsSyncFeatureEnabled,
      base::BindRepeating(
          &OsSyncableServiceModelTypeController::OnUserPrefChanged,
          base::Unretained(this)));
}

OsSyncableServiceModelTypeController::~OsSyncableServiceModelTypeController() =
    default;

syncer::DataTypeController::PreconditionState
OsSyncableServiceModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return pref_service_->GetBoolean(syncer::prefs::kOsSyncFeatureEnabled)
             ? PreconditionState::kPreconditionsMet
             : PreconditionState::kMustStopAndClearData;
}

void OsSyncableServiceModelTypeController::OnUserPrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
