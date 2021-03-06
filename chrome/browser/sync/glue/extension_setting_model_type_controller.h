// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_MODEL_TYPE_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_MODEL_TYPE_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/sync/driver/non_ui_syncable_service_based_model_type_controller.h"

class Profile;

namespace browser_sync {

// A ModelTypeController that processes extension data on the extensions
// background thread.
// NOTE: Chrome OS uses a fork of this class for APP_SETTINGS.
class ExtensionSettingModelTypeController
    : public syncer::NonUiSyncableServiceBasedModelTypeController {
 public:
  // |type| must be either EXTENSION_SETTINGS or APP_SETTINGS.
  // |dump_stack| is called when an unrecoverable error occurs.
  // |profile| must not be null.
  ExtensionSettingModelTypeController(
      syncer::ModelType type,
      syncer::OnceModelTypeStoreFactory store_factory,
      SyncableServiceProvider syncable_service_provider,
      const base::RepeatingClosure& dump_stack,
      Profile* profile);
  ~ExtensionSettingModelTypeController() override;

  // DataTypeController overrides.
  void LoadModels(const syncer::ConfigureContext& configure_context,
                  const ModelLoadCallback& model_load_callback) override;

 private:
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionSettingModelTypeController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_GLUE_EXTENSION_SETTING_MODEL_TYPE_CONTROLLER_H_
