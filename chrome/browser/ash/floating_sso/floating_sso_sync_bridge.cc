// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace ash::floating_sso {

FloatingSsoSyncBridge::FloatingSsoSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  // TODO: b/346354110 - add Sync store creation.
}

FloatingSsoSyncBridge::~FloatingSsoSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
FloatingSsoSyncBridge::CreateMetadataChangeList() {
  // TODO: b/346355106 - implement.
  NOTIMPLEMENTED();
  return nullptr;
}

std::optional<syncer::ModelError> FloatingSsoSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // TODO: b/346354354 - implement.
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::optional<syncer::ModelError>
FloatingSsoSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // TODO: b/346354109 - implement.
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::string FloatingSsoSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetClientTag(entity_data);
}

std::string FloatingSsoSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return entity_data.specifics.cookie().unique_key();
}

std::unique_ptr<syncer::DataBatch> FloatingSsoSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  // TODO: b/346354248 - implement.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<syncer::DataBatch>
FloatingSsoSyncBridge::GetAllDataForDebugging() {
  // TODO: b/346354358 - implement.
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace ash::floating_sso
