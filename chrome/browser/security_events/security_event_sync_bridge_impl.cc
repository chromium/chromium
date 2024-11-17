// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/security_events/security_event_sync_bridge_impl.h"

#include <array>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/security_event_specifics.pb.h"

namespace {

std::string GetStorageKeyFromSpecifics(
    const sync_pb::SecurityEventSpecifics& specifics) {
  // Force Big Endian, this means newly created keys are last in sort order,
  // which allows leveldb to append new writes, which it is best at.
  // TODO(markusheintz): Until we force |event_time_usec| to never conflict,
  // this has the potential for errors.
  std::array<uint8_t, 8> key;
  base::span(key).copy_from(base::U64ToBigEndian(
      base::checked_cast<uint64_t>(specifics.event_time_usec())));
  return std::string(key.begin(), key.end());
}

std::unique_ptr<syncer::EntityData> ToEntityData(
    sync_pb::SecurityEventSpecifics specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = base::NumberToString(specifics.event_time_usec());
  entity_data->specifics.mutable_security_event()->Swap(&specifics);
  return entity_data;
}

}  // namespace

SecurityEventSyncBridgeImpl::SecurityEventSyncBridgeImpl(
    syncer::OnceDataTypeStoreFactory store_factory,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  StoreWithCache::CreateAndLoad(
      std::move(store_factory), syncer::SECURITY_EVENTS,
      base::BindOnce(&SecurityEventSyncBridgeImpl::OnStoreLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

SecurityEventSyncBridgeImpl::~SecurityEventSyncBridgeImpl() {
  // TODO(crbug.com/362428820): Remove logging once investigation is complete.
  if (store_) {
    VLOG(1) << "SecurityEvents during destruction: "
            << store_->in_memory_data().size();
  }
}

void SecurityEventSyncBridgeImpl::RecordSecurityEvent(
    sync_pb::SecurityEventSpecifics specifics) {
  if (!store_) {
    return;
  }
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  std::string storage_key = GetStorageKeyFromSpecifics(specifics);

  std::unique_ptr<StoreWithCache::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  write_batch->WriteData(storage_key, specifics);

  change_processor()->Put(storage_key, ToEntityData(std::move(specifics)),
                          write_batch->GetMetadataChangeList());

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SecurityEventSyncBridgeImpl::OnStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
SecurityEventSyncBridgeImpl::GetControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}

std::unique_ptr<syncer::MetadataChangeList>
SecurityEventSyncBridgeImpl::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError>
SecurityEventSyncBridgeImpl::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(entity_data.empty());
  DCHECK(change_processor()->IsTrackingMetadata());
  DCHECK(!change_processor()->TrackedAccountId().empty());
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_data));
}

std::optional<syncer::ModelError>
SecurityEventSyncBridgeImpl::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<StoreWithCache::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    DCHECK_EQ(syncer::EntityChange::ACTION_DELETE, change->type());
    write_batch->DeleteData(change->storage_key());
  }

  write_batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&SecurityEventSyncBridgeImpl::OnStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
  return {};
}

std::unique_ptr<syncer::DataBatch>
SecurityEventSyncBridgeImpl::GetDataForCommit(StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  const std::map<std::string, sync_pb::SecurityEventSpecifics>& in_memory_data =
      store_->in_memory_data();
  for (const std::string& storage_key : storage_keys) {
    auto it = in_memory_data.find(storage_key);
    if (it != in_memory_data.end()) {
      batch->Put(it->first, ToEntityData(it->second));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
SecurityEventSyncBridgeImpl::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& [storage_key, specifics] : store_->in_memory_data()) {
    batch->Put(storage_key, ToEntityData(specifics));
  }
  return batch;
}

std::string SecurityEventSyncBridgeImpl::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string SecurityEventSyncBridgeImpl::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetStorageKeyFromSpecifics(entity_data.specifics.security_event());
}

void SecurityEventSyncBridgeImpl::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  store_->DeleteAllDataAndMetadata(
      base::BindOnce(&SecurityEventSyncBridgeImpl::OnStoreCommit,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SecurityEventSyncBridgeImpl::OnStoreLoaded(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<StoreWithCache> store,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("sync", "syncer::SecurityEventSyncBridgeImpl::OnStoreLoaded");

  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void SecurityEventSyncBridgeImpl::OnStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    change_processor()->ReportError(*error);
  }
}
