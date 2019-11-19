// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYNC_OS_PREFERENCES_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_SYNC_OS_PREFERENCES_MODEL_TYPE_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"
#include "components/sync/model/model_type_store.h"

class PrefService;

namespace syncer {
class SyncService;
}

// Controls syncing of ModelTypes OS_PREFERENCES and OS_PRIORITY_PREFERENCES.
class OsPreferencesModelTypeController
    : public syncer::SyncableServiceBasedModelTypeController {
 public:
  OsPreferencesModelTypeController(
      syncer::ModelType type,
      syncer::OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      PrefService* pref_service,
      syncer::SyncService* sync_service);
  ~OsPreferencesModelTypeController() override;

  OsPreferencesModelTypeController(const OsPreferencesModelTypeController&) =
      delete;
  OsPreferencesModelTypeController& operator=(
      const OsPreferencesModelTypeController&) = delete;

  // DataTypeController:
  PreconditionState GetPreconditionState() const override;

 private:
  // Callback for changes to the OS sync feature enabled pref.
  void OnUserPrefChanged();

  PrefService* const pref_service_;
  syncer::SyncService* const sync_service_;

  PrefChangeRegistrar pref_registrar_;
};

#endif  // CHROME_BROWSER_CHROMEOS_SYNC_OS_PREFERENCES_MODEL_TYPE_CONTROLLER_H_
