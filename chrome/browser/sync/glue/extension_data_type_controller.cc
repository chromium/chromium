// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/extension_data_type_controller.h"

#include <utility>

#include "chrome/browser/profiles/profile.h"
#include "extensions/browser/extension_system.h"

namespace browser_sync {

ExtensionDataTypeController::ExtensionDataTypeController(
    syncer::DataType type,
    syncer::OnceDataTypeStoreFactory store_factory,
    base::WeakPtr<syncer::SyncableService> syncable_service,
    const base::RepeatingClosure& dump_stack,
    DelegateMode delegate_mode,
    Profile* profile)
    : SyncableServiceBasedDataTypeController(type,
                                             std::move(store_factory),
                                             syncable_service,
                                             dump_stack,
                                             delegate_mode),
      profile_(profile) {
  DCHECK(type == syncer::EXTENSIONS || type == syncer::APPS ||
         type == syncer::THEMES);
}

ExtensionDataTypeController::~ExtensionDataTypeController() = default;

void ExtensionDataTypeController::LoadModels(
    const syncer::ConfigureContext& configure_context,
    const ModelLoadCallback& model_load_callback) {
  DCHECK(CalledOnValidThread());
  extensions::ExtensionSystem::Get(profile_)->InitForRegularProfile(
      /*extensions_enabled=*/true);
  DataTypeController::LoadModels(configure_context, model_load_callback);
}

}  // namespace browser_sync
