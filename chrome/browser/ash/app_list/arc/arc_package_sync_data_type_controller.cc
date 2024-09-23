// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_package_sync_data_type_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/session/arc_session_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/forwarding_data_type_controller_delegate.h"
#include "components/sync/model/syncable_service_based_bridge.h"
#include "components/sync/service/sync_service.h"

using syncer::ClientTagBasedDataTypeProcessor;
using syncer::DataTypeController;
using syncer::DataTypeControllerDelegate;
using syncer::ForwardingDataTypeControllerDelegate;
using syncer::SyncableServiceBasedBridge;

ArcPackageSyncDataTypeController::ArcPackageSyncDataTypeController(
    syncer::OnceDataTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    syncer::SyncService* sync_service,
    Profile* profile)
    : DataTypeController(syncer::ARC_PACKAGE),
      bridge_(std::make_unique<SyncableServiceBasedBridge>(
          syncer::ARC_PACKAGE,
          std::move(store_factory),
          std::make_unique<ClientTagBasedDataTypeProcessor>(syncer::ARC_PACKAGE,
                                                            dump_stack),
          syncable_service.get())),
      sync_service_(sync_service),
      profile_(profile),
      arc_prefs_(ArcAppListPrefs::Get(profile)) {
  DCHECK(arc_prefs_);
  DCHECK(profile_);
  DataTypeControllerDelegate* delegate =
      bridge_->change_processor()->GetControllerDelegate().get();
  auto delegate_for_full_sync_mode =
      std::make_unique<ForwardingDataTypeControllerDelegate>(delegate);

    // Runs in transport-mode and full-sync mode, sharing the bridge's delegate.
  InitDataTypeController(
      std::move(delegate_for_full_sync_mode),
      /*delegate_for_transport_mode=*/
      std::make_unique<ForwardingDataTypeControllerDelegate>(delegate));

  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->AddObserver(this);
  }

  arc_prefs_->AddObserver(this);
}

ArcPackageSyncDataTypeController::~ArcPackageSyncDataTypeController() {
  arc::ArcSessionManager* arc_session_manager = arc::ArcSessionManager::Get();
  if (arc_session_manager) {
    arc_session_manager->RemoveObserver(this);
  }
  arc_prefs_->RemoveObserver(this);
  // The constructor passed a forwarding controller delegate referencing
  // `bridge_` to the base class. They are stored by the base class and need
  // to be destroyed before `bridge_` to avoid dangling pointers.
  ClearDelegateMap();
}

syncer::DataTypeController::PreconditionState
ArcPackageSyncDataTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  if (!arc::IsArcPlayStoreEnabledForProfile(profile_)) {
    return PreconditionState::kMustStopAndClearData;
  }
  // Implementing a wait here in the controller, instead of the regular wait in
  // the SyncableService, allows waiting again after this particular datatype
  // has been disabled and reenabled (since core sync code does not support the
  // notion of a model becoming unready, which effectively is the case here).
  if (!arc_prefs_->package_list_initial_refreshed()) {
    return PreconditionState::kMustStopAndKeepData;
  }
  return PreconditionState::kPreconditionsMet;
}

void ArcPackageSyncDataTypeController::OnArcPlayStoreEnabledChanged(
    bool enabled) {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void ArcPackageSyncDataTypeController::OnArcInitialStart() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void ArcPackageSyncDataTypeController::OnPackageListInitialRefreshed() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}

void ArcPackageSyncDataTypeController::OnOsSyncFeaturePrefChanged() {
  DCHECK(CalledOnValidThread());
  sync_service_->DataTypePreconditionChanged(type());
}
