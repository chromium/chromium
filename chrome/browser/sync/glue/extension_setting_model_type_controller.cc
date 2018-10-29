// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_model_type_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/extension_system.h"

namespace browser_sync {

ExtensionSettingModelTypeController::ExtensionSettingModelTypeController(
    syncer::ModelType type,
    syncer::OnceModelTypeStoreFactory store_factory,
    SyncableServiceProvider syncable_service_provider,
    const base::RepeatingClosure& dump_stack,
    Profile* profile)
    : NonUiSyncableServiceBasedModelTypeController(
          type,
          std::move(store_factory),
          std::move(syncable_service_provider),
          dump_stack,
          extensions::GetBackendTaskRunner()),
      profile_(profile) {
  DCHECK(profile_);
  DCHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
}

ExtensionSettingModelTypeController::~ExtensionSettingModelTypeController() {}

void ExtensionSettingModelTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(
      /*extensions_enabled=*/true);
  NonUiSyncableServiceBasedModelTypeController::LoadModels(configure_context,
                                                           model_load_callback);
}

}  // namespace browser_sync
