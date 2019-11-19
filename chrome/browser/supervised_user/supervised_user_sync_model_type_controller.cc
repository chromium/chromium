// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/supervised_user/supervised_user_sync_model_type_controller.h"

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_sync/browser_sync_client.h"
#include "components/sync/model/model_type_store_service.h"

SupervisedUserSyncModelTypeController::SupervisedUserSyncModelTypeController(
    syncer::ModelType type,
    const Profile* profile,
    const base::RepeatingClosure& dump_stack,
    browser_sync::BrowserSyncClient* sync_client)
    : SyncableServiceBasedModelTypeController(
          type,
          sync_client->GetModelTypeStoreService()->GetStoreFactory(),
          sync_client->GetSyncableServiceForType(type),
          dump_stack),
      profile_(profile) {
  DCHECK(type == syncer::SUPERVISED_USER_SETTINGS ||
         type == syncer::SUPERVISED_USER_WHITELISTS);
}

SupervisedUserSyncModelTypeController::
    ~SupervisedUserSyncModelTypeController() {}

syncer::DataTypeController::PreconditionState
SupervisedUserSyncModelTypeController::GetPreconditionState() const {
  DCHECK(CalledOnValidThread());
  return profile_->IsSupervised() ? PreconditionState::kPreconditionsMet
                                  : PreconditionState::kMustStopAndClearData;
}
