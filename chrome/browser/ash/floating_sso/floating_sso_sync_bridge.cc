// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_sso/floating_sso_sync_bridge.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check_deref.h"
#include "base/logging.h"
#include "chrome/browser/ash/floating_sso/cookie_sync_conversions.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/cookie_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"
#include "net/cookies/canonical_cookie.h"

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
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory create_store_callback)
    : syncer::DataTypeSyncBridge(std::move(change_processor)) {
  StoreWithCache::CreateAndLoad(
      std::move(create_store_callback), syncer::COOKIES,
      base::BindOnce(&FloatingSsoSyncBridge::OnStoreCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

FloatingSsoSyncBridge::~FloatingSsoSyncBridge() {
  if (!deferred_cookie_additions_.empty() ||
      !deferred_cookie_deletions_.empty()) {
    DVLOG(1) << "Non-empty event queue at shutdown.";
  }
}

std::unique_ptr<syncer::MetadataChangeList>
FloatingSsoSyncBridge::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
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
  std::optional<syncer::ModelError> result = ApplyIncrementalSyncChanges(
      std::move(metadata_change_list), std::move(remote_entities));
  OnMergeFullSyncDataFinished();
  return result;
}

std::optional<syncer::ModelError>
FloatingSsoSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::vector<net::CanonicalCookie> added_or_updated;
  std::vector<net::CanonicalCookie> deleted;
  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    switch (change->type()) {
      case syncer::EntityChange::ACTION_ADD:
      case syncer::EntityChange::ACTION_UPDATE: {
        const sync_pb::CookieSpecifics& specifics =
            change->data().specifics.cookie();
        // We save `specifics` locally only when we don't fail to convert it to
        // a `cookie` here. Alternatively we could still store `specifics` in
        // the store and then try to create a cookie again in case of a Chrome
        // update. We don't do this because: (1) in the targeted enterprise
        // use case we expect affected devices to be on the same Chrome version
        // and (2) the disparity between client-side and server-side states will
        // not last long due to short TTL for cookies in Sync.
        if (std::unique_ptr<net::CanonicalCookie> cookie =
                FromSyncProto(specifics);
            cookie) {
          added_or_updated.push_back(*cookie);
          batch->WriteData(change->storage_key(), specifics);
        }
        break;
      }
      case syncer::EntityChange::ACTION_DELETE: {
        const CookieSpecificsEntries& in_memory_data = store_->in_memory_data();
        auto it = in_memory_data.find(change->storage_key());
        if (it == in_memory_data.end()) {
          // Nothing to delete in the local store.
          break;
        }
        batch->DeleteData(change->storage_key());
        if (std::unique_ptr<net::CanonicalCookie> cookie =
                FromSyncProto(it->second);
            cookie) {
          deleted.push_back(*cookie);
        }
        break;
      }
    }
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  CommitToStore(std::move(batch));

  for (Observer& observer : observers_) {
    // No need to notify about empty lists of changes.
    if (!added_or_updated.empty()) {
      observer.OnCookiesAddedOrUpdatedRemotely(added_or_updated);
    }
    if (!deleted.empty()) {
      observer.OnCookiesRemovedRemotely(deleted);
    }
  }

  return {};
}

std::string FloatingSsoSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return GetClientTag(entity_data);
}

std::string FloatingSsoSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
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
  if (keep_local_cookie_keys_.contains(storage_key)) {
    return syncer::ConflictResolution::kUseLocal;
  }
  return syncer::DataTypeSyncBridge::ResolveConflict(storage_key, remote_data);
}

const FloatingSsoSyncBridge::CookieSpecificsEntries&
FloatingSsoSyncBridge::CookieSpecificsInStore() const {
  return CHECK_DEREF(store_.get()).in_memory_data();
}

bool FloatingSsoSyncBridge::IsInitialDataReadFinishedForTest() const {
  return is_initial_data_read_finished_;
}

void FloatingSsoSyncBridge::SetOnStoreCommitCallbackForTest(
    base::RepeatingClosure callback) {
  on_store_commit_callback_for_test_ = std::move(callback);
}

void FloatingSsoSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<StoreWithCache> store,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    deferred_cookie_additions_.clear();
    deferred_cookie_deletions_.clear();
    return;
  }
  CHECK(store);
  store_ = std::move(store);
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  is_initial_data_read_finished_ = true;
  ProcessQueuedCookies();
}

void FloatingSsoSyncBridge::ProcessQueuedCookies() {
  // Add all new cookies. The two queues should not overlap.
  for (const auto& [key, cookie] : deferred_cookie_additions_) {
    if (deferred_cookie_deletions_.contains(key)) {
      DVLOG(1) << "Cookie present in both addition and deletion queues. Will "
                  "perform delete.";
    } else {
      AddOrUpdateCookie(cookie);
    }
  }
  // Delete queued cookies.
  for (const auto& storage_key : deferred_cookie_deletions_) {
    DeleteCookieWithKey(storage_key);
  }
  deferred_cookie_additions_.clear();
  deferred_cookie_deletions_.clear();
}

void FloatingSsoSyncBridge::OnStoreCommit(
    const std::optional<syncer::ModelError>& error) {
  if (on_store_commit_callback_for_test_) {
    on_store_commit_callback_for_test_.Run();
  }
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void FloatingSsoSyncBridge::CommitToStore(
    std::unique_ptr<StoreWithCache::WriteBatch> batch) {
  store_->CommitWriteBatch(std::move(batch),
                           base::BindOnce(&FloatingSsoSyncBridge::OnStoreCommit,
                                          weak_ptr_factory_.GetWeakPtr()));
}

bool FloatingSsoSyncBridge::IsCookieInStore(
    const std::string& storage_key) const {
  return store_->in_memory_data().contains(storage_key);
}

void FloatingSsoSyncBridge::AddOrUpdateCookie(
    const net::CanonicalCookie& cookie) {
  std::optional<std::string> serialization_result = SerializedKey(cookie);
  if (!serialization_result.has_value()) {
    return;
  }
  const std::string& storage_key = serialization_result.value();

  if (!is_initial_data_read_finished_) {
    deferred_cookie_additions_[storage_key] = cookie;
    deferred_cookie_deletions_.erase(storage_key);
    return;
  }

  std::optional<sync_pb::CookieSpecifics> specifics = ToSyncProto(cookie);
  if (!specifics.has_value()) {
    return;
  }

  // Check if an identical cookie already exists in the store, to avoid sending
  // no-op changes to Sync.
  const CookieSpecificsEntries& in_store_specifics = CookieSpecificsInStore();
  if (auto it = in_store_specifics.find(specifics->unique_key());
      it != in_store_specifics.end()) {
    const sync_pb::CookieSpecifics& local_specifics = it->second;
    std::unique_ptr<net::CanonicalCookie> in_store_cookie =
        FromSyncProto(local_specifics);
    if (in_store_cookie && in_store_cookie->HasEquivalentDataMembers(cookie)) {
      return;
    }
  }

  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();

  // Add/update this entry to the store and model.
  change_processor()->Put(storage_key, CreateEntityData(specifics.value()),
                          batch->GetMetadataChangeList());
  batch->WriteData(storage_key, specifics.value());

  CommitToStore(std::move(batch));
}

void FloatingSsoSyncBridge::DeleteCookie(const net::CanonicalCookie& cookie) {
  std::optional<std::string> serialization_result = SerializedKey(cookie);
  if (!serialization_result.has_value()) {
    return;
  }
  const std::string& storage_key = serialization_result.value();

  if (!is_initial_data_read_finished_) {
    deferred_cookie_deletions_.insert(storage_key);
    deferred_cookie_additions_.erase(storage_key);
    return;
  }

  DeleteCookieWithKey(storage_key);
}

void FloatingSsoSyncBridge::DeleteCookieWithKey(
    const std::string& storage_key) {
  // Check if the key is present in the store, to avoid sending no-op changes to
  // Sync.
  if (!IsCookieInStore(storage_key)) {
    return;
  }

  std::unique_ptr<StoreWithCache::WriteBatch> batch =
      store_->CreateWriteBatch();
  change_processor()->Delete(storage_key, syncer::DeletionOrigin::Unspecified(),
                             batch->GetMetadataChangeList());
  batch->DeleteData(storage_key);

  CommitToStore(std::move(batch));
}

void FloatingSsoSyncBridge::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FloatingSsoSyncBridge::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void FloatingSsoSyncBridge::AddToLocallyPreferredCookies(
    const std::string& storage_key) {
  keep_local_cookie_keys_.insert(storage_key);
}

void FloatingSsoSyncBridge::SetOnMergeFullSyncDataCallback(
    base::OnceClosure callback) {
  if (merge_full_sync_data_finished_) {
    std::move(callback).Run();
    return;
  }
  on_merge_full_sync_data_callback_ = std::move(callback);
}

void FloatingSsoSyncBridge::OnMergeFullSyncDataFinished() {
  if (on_merge_full_sync_data_callback_) {
    std::move(on_merge_full_sync_data_callback_).Run();
  }
  merge_full_sync_data_finished_ = true;
}

}  // namespace ash::floating_sso
