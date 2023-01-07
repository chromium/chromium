// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_OS_SYNCABLE_SERVICE_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SYNC_OS_SYNCABLE_SERVICE_MODEL_TYPE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"
#include "components/sync/model/model_type_store.h"

class PrefService;

namespace syncer {
class SyncableService;
class SyncService;
}  // namespace syncer

// Controls syncing of Chrome OS ModelTypes that run in sync transport-only
// mode and are tied to the OS sync feature consent (APP_LIST, OS_PREFERENCES,
// and OS_PRIORITY_PREFERENCES).
// TODO(https://crbug.com/1274802): Remove this.
class OsSyncableServiceModelTypeController
    : public syncer::SyncableServiceBasedModelTypeController {
 public:
  OsSyncableServiceModelTypeController(
      syncer::ModelType type,
      syncer::OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      PrefService* pref_service,
      syncer::SyncService* sync_service);

  ~OsSyncableServiceModelTypeController() override;

  OsSyncableServiceModelTypeController(
      const OsSyncableServiceModelTypeController&) = delete;
  OsSyncableServiceModelTypeController& operator=(
      const OsSyncableServiceModelTypeController&) = delete;

 private:
  PrefService* const pref_service_;
  syncer::SyncService* const sync_service_;
};

#endif  // CHROME_BROWSER_ASH_SYNC_OS_SYNCABLE_SERVICE_MODEL_TYPE_CONTROLLER_H_
