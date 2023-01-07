// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_sync_model_type_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "components/sync/model/model_type_store_service.h"

SupervisedUserSyncModelTypeController::SupervisedUserSyncModelTypeController(
    syncer::ModelType type,
    const Profile* profile,
    const base::RepeatingClosure& dump_stack,
    syncer::OnceModelTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service)
    : SyncableServiceBasedModelTypeController(
          type,
          std::move(store_factory),
          syncable_service,
          dump_stack,
          DelegateMode::kTransportModeWithSingleModel),
      profile_(profile) {
  DCHECK(type == syncer::SUPERVISED_USER_SETTINGS);
}

SupervisedUserSyncModelTypeController::
    ~SupervisedUserSyncModelTypeController() = default;

syncer::DataTypeController::PreconditionState
SupervisedUserSyncModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return profile_->IsChild() ? PreconditionState::kPreconditionsMet
                             : PreconditionState::kMustStopAndClearData;
}
