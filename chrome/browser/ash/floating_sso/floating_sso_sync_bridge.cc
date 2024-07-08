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
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/cookie_specifics.pb.h"

namespace ash::floating_sso {

FloatingSsoSyncBridge::FloatingSsoSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory create_store_callback)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  std::move(create_store_callback)
      .Run(syncer::COOKIES,
           base::BindOnce(&FloatingSsoSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
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

const FloatingSsoSyncBridge::CookieSpecificsEntries&
FloatingSsoSyncBridge::CookieSpecificsEntriesForTest() const {
  return entries_;
}

bool FloatingSsoSyncBridge::IsInitialDataReadFinishedForTest() const {
  return is_initial_data_read_finished_;
}

void FloatingSsoSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(
      base::BindOnce(&FloatingSsoSyncBridge::OnReadAllDataAndMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FloatingSsoSyncBridge::OnReadAllDataAndMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::ModelTypeStore::Record& record : *record_list) {
    sync_pb::CookieSpecifics cookie_specifics;
    if (!cookie_specifics.ParseFromString(record.value)) {
      continue;
    }
    entries_.emplace(cookie_specifics.unique_key(), cookie_specifics);
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  is_initial_data_read_finished_ = true;
}

}  // namespace ash::floating_sso
