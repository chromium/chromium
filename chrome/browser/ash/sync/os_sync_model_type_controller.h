// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_OS_SYNC_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_ASH_SYNC_OS_SYNC_MODEL_TYPE_CONTROLLER_H_

#include <memory>

#include "components/prefs/pref_change_registrar.h"
#include "components/sync/driver/model_type_controller.h"

class PrefService;

namespace syncer {
class ModelTypeControllerDelegate;
class SyncService;
}  // namespace syncer

// Controls sync of Chrome OS ModelTypes that can run in transport-mode.
// TODO(https://crbug.com/1274802): Remove this.
class OsSyncModelTypeController : public syncer::ModelTypeController {
 public:
  OsSyncModelTypeController(syncer::ModelType type,
                            std::unique_ptr<syncer::ModelTypeControllerDelegate>
                                delegate_for_full_sync_mode,
                            std::unique_ptr<syncer::ModelTypeControllerDelegate>
                                delegate_for_transport_mode,
                            PrefService* pref_service,
                            syncer::SyncService* sync_service);
  ~OsSyncModelTypeController() override;

  OsSyncModelTypeController(const OsSyncModelTypeController&) = delete;
  OsSyncModelTypeController& operator=(const OsSyncModelTypeController&) =
      delete;

 private:
  // Callback for changes to the OS sync feature enabled pref.
  void OnUserPrefChanged();

  PrefService* const pref_service_;
  syncer::SyncService* const sync_service_;
};

#endif  // CHROME_BROWSER_ASH_SYNC_OS_SYNC_MODEL_TYPE_CONTROLLER_H_
