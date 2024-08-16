// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_setting_data_type_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/extension_system.h"

namespace browser_sync {

ExtensionSettingDataTypeController::ExtensionSettingDataTypeController(
    syncer::DataType type,
    syncer::OnceDataTypeStoreFactory store_factory,
    SyncableServiceProvider syncable_service_provider,
    const base::RepeatingClosure& dump_stack,
    DelegateMode delegate_mode,
    Profile* profile)
    : NonUiSyncableServiceBasedDataTypeController(
          type,
          std::move(store_factory),
          std::move(syncable_service_provider),
          dump_stack,
          extensions::GetBackendTaskRunner(),
          delegate_mode),
      profile_(profile) {
  DCHECK(profile_);
  DCHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
}

ExtensionSettingDataTypeController::~ExtensionSettingDataTypeController() =
    default;

void ExtensionSettingDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(
      /*extensions_enabled=*/true);
  NonUiSyncableServiceBasedDataTypeController::LoadModels(configure_context,
                                                           model_load_callback);
}

}  // namespace browser_sync
