// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_TYPE_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"

class Profile;

namespace browser_sync {

// Controller with custom logic to start the extensions machinery when sync is
// starting.
class ExtensionModelTypeController
    : public syncer::SyncableServiceBasedModelTypeController {
 public:
  ExtensionModelTypeController(
      syncer::ModelType type,
      syncer::OnceModelTypeStoreFactory store_factory,
      base::WeakPtr<syncer::SyncableService> syncable_service,
      const base::RepeatingClosure& dump_stack,
      DelegateMode delegate_mode,
      Profile* profile);

  ExtensionModelTypeController(const ExtensionModelTypeController&) = delete;
  ExtensionModelTypeController& operator=(const ExtensionModelTypeController&) =
      delete;

  ~ExtensionModelTypeController() override;

  // DataTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;

 private:
  const raw_ptr<Profile> profile_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_MODEL_TYPE_CONTROLLER_H_
