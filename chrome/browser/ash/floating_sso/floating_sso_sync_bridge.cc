// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_with_in_memory_cache.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace ash::floating_sso {

namespace {

std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::CookieSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->specifics.mutable_cookie()->CopyFrom(specifics);
  entity_data->name = specifics.unique_key();
  return entity_data;
}

}  // namespace

FloatingSsoSyncBridge::FloatingSsoSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory create_store_callback)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  StoreWithCache::CreateAndLoad(
      std::move(create_store_callback), syncer::COOKIES,
      base::BindOnce(&FloatingSsoSyncBridge::OnStoreCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

FloatingSsoSyncBridge::~FloatingSsoSyncBridge() = default;

std::unique_ptr<syncer::MetadataChangeList>
FloatingSsoSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> FloatingSsoSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList remote_entities) {
  const CookieSpecificsEntries& in_memory_data = store_->in_memory_data();
  std::set<std::string> local_keys_to_upload;
  for (const auto& [key, specifics] : in_memory_data) {
    local_keys_to_upload.insert(key);
  }
  // Go through `remote_entities` and filter out entities conflicting with local
  // data if the local data should be preferred according to `ResolveConflict`.
  // When remote data should be preferred, remove corresponding key from
  // `local_keys_to_upload`.
  std::erase_if(remote_entities,
                [&](const std::unique_ptr<syncer::EntityChange>& change) {
                  auto it = local_keys_to_upload.find(change->storage_key());
                  if (it != local_keys_to_upload.end()) {
                    syncer::ConflictResolution result =
                        ResolveConflict(*it, change->data());
                    if (result == syncer::ConflictResolution::kUseLocal) {
                      return true;
                    } else {
                      // TODO: b/354202235 - revisit this CHECK once we have a
                      // non-default implementation of `ResolveConflict`.
                      CHECK_EQ(result, syncer::ConflictResolution::kUseRemote);
                      local_keys_to_upload.erase(it);
                      return false;
                    }
                  }
                  return false;
                });

  // Send entities corresponding to `local_keys_to_upload` to Sync server.
  for (const std::string& storage_key : local_keys_to_upload) {
    change_processor()->Put(storage_key,
                            CreateEntityData(in_memory_data.at(storage_key)),
                            metadata_change_list.get());
  }

  // Add remote entities to local data.
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(remote_entities));
}

std::optional<syncer::ModelError>
FloatingSsoSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // TODO: b/353225533 - send notifications about new and updated cookies, so
  // that the browser can add them to the cookie jar.
  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const sync_pb::CookieSpecifics& specifics =
        change->data().specifics.cookie();
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE:
        batch->WriteData(change->storage_key(), specifics);
        break;
      case syncer::EntityChange::ACTION_DELETE:
        batch->DeleteData(change->storage_key());
        break;
    }
  }
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&FloatingSsoSyncBridge::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
  return {};
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
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  const CookieSpecificsEntries& in_memory_data = store_->in_memory_data();
  for (const std::string& storage_key : storage_keys) {
    auto it = in_memory_data.find(storage_key);
    if (it != in_memory_data.end()) {
      batch->Put(it->first, CreateEntityData(it->second));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
FloatingSsoSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& entry : store_->in_memory_data()) {
    batch->Put(entry.first, CreateEntityData(entry.second));
  }
  return batch;
}

syncer::ConflictResolution FloatingSsoSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const syncer::EntityData& remote_data) const {
  // TODO: b/353222478 - prefer local SAML cookies if they were acquired
  // during the most recent ChromeOS sign-in.
  return syncer::ModelTypeSyncBridge::ResolveConflict(storage_key, remote_data);
}

const FloatingSsoSyncBridge::CookieSpecificsEntries&
FloatingSsoSyncBridge::CookieSpecificsEntriesForTest() const {
  return CHECK_DEREF(store_.get()).in_memory_data();
}

bool FloatingSsoSyncBridge::IsInitialDataReadFinishedForTest() const {
  return is_initial_data_read_finished_;
}

void FloatingSsoSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<StoreWithCache> store,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  CHECK(store);
  store_ = std::move(store);
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  is_initial_data_read_finished_ = true;
}

void FloatingSsoSyncBridge::OnStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}

}  // namespace ash::floating_sso
