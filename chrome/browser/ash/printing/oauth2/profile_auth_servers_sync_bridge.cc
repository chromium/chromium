// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/common/channel_info.h"
#include "chromeos/printing/uri.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_base.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/printers_authorization_server_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::printing::oauth2 {

namespace {

sync_pb::PrintersAuthorizationServerSpecifics ToSpecifics(
    const std::string& uri) {
  sync_pb::PrintersAuthorizationServerSpecifics specifics;
  specifics.set_uri(uri);
  return specifics;
}

std::unique_ptr<syncer::EntityData> ToEntityDataPtr(const std::string& uri) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_printers_authorization_server() =
      ToSpecifics(uri);
  entity_data->name = uri;
  return entity_data;
}

}  // namespace

std::unique_ptr<ProfileAuthServersSyncBridge>
ProfileAuthServersSyncBridge::Create(
    Observer* observer,
    syncer::OnceModelTypeStoreFactory store_factory) {
  DCHECK(observer);
  return base::WrapUnique(new ProfileAuthServersSyncBridge(
      std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
          syncer::PRINTERS_AUTHORIZATION_SERVERS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel())),
      std::move(store_factory), observer));
}

std::unique_ptr<ProfileAuthServersSyncBridge>
ProfileAuthServersSyncBridge::CreateForTesting(
    Observer* observer,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory store_factory) {
  DCHECK(observer);
  DCHECK(change_processor);
  return base::WrapUnique(new ProfileAuthServersSyncBridge(
      std::move(change_processor), std::move(store_factory), observer));
}

ProfileAuthServersSyncBridge::~ProfileAuthServersSyncBridge() = default;

void ProfileAuthServersSyncBridge::AddAuthorizationServer(
    const chromeos::Uri& server_uri) {
  DCHECK(initialization_completed_);
  const std::string key = server_uri.GetNormalized(/*always_print_port=*/false);
  servers_uris_.insert(key);
  auto batch = store_->CreateWriteBatch();
  batch->WriteData(key, ToSpecifics(key).SerializeAsString());
  if (change_processor()->IsTrackingMetadata()) {
    change_processor()->Put(key, ToEntityDataPtr(key),
                            batch->GetMetadataChangeList());
  }
  store_->CommitWriteBatch(
      std::move(batch), base::BindOnce(&ProfileAuthServersSyncBridge::OnCommit,
                                       weak_ptr_factory_.GetWeakPtr()));
}

ProfileAuthServersSyncBridge::ProfileAuthServersSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    syncer::OnceModelTypeStoreFactory store_factory,
    Observer* observer)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)),
      observer_(observer) {
  std::move(store_factory)
      .Run(syncer::PRINTERS_AUTHORIZATION_SERVERS,
           base::BindOnce(&ProfileAuthServersSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ProfileAuthServersSyncBridge::OnStoreCreated(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore> store) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  store_ = std::move(store);
  store_->ReadAllData(
      base::BindOnce(&ProfileAuthServersSyncBridge::OnReadAllData,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProfileAuthServersSyncBridge::OnReadAllData(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::ModelTypeStore::RecordList> record_list) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::ModelTypeStore::Record& r : *record_list) {
    sync_pb::PrintersAuthorizationServerSpecifics specifics;
    if (!specifics.ParseFromString(r.value)) {
      change_processor()->ReportError(
          {FROM_HERE, "Failed to deserialize all specifics."});
      return;
    }
    servers_uris_.insert(specifics.uri());
  }

  // Data loaded. Load metadata.
  store_->ReadAllMetadata(
      base::BindOnce(&ProfileAuthServersSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProfileAuthServersSyncBridge::OnReadAllMetadata(
    const absl::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  change_processor()->ModelReadyToSync(std::move(metadata_batch));
  initialization_completed_ = true;
  observer_->OnProfileAuthorizationServersInitialized();
}

std::unique_ptr<syncer::MetadataChangeList>
ProfileAuthServersSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> ProfileAuthServersSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // Every local URI is considered unsynced until the contrary is proven, i.e.
  // until the same URI is seen in the incoming |entity_data|.
  std::set<std::string> unsynced_local_uris = servers_uris_;
  std::unique_ptr<syncer::ModelTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    const sync_pb::PrintersAuthorizationServerSpecifics& specifics =
        change->data().specifics.printers_authorization_server();
    const std::string remote_uri = specifics.uri();
    DCHECK_EQ(change->storage_key(), remote_uri);
    auto [unused, is_new] = servers_uris_.insert(remote_uri);
    if (is_new) {
      batch->WriteData(remote_uri, specifics.SerializeAsString());
    } else {
      unsynced_local_uris.erase(remote_uri);
    }
  }

  for (const std::string& uri : unsynced_local_uris) {
    change_processor()->Put(uri, ToEntityDataPtr(uri),
                            metadata_change_list.get());
  }

  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch), base::BindOnce(&ProfileAuthServersSyncBridge::OnCommit,
                                       weak_ptr_factory_.GetWeakPtr()));

  return absl::nullopt;
}

absl::optional<syncer::ModelError>
ProfileAuthServersSyncBridge::ApplySyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  return absl::nullopt;
}

void ProfileAuthServersSyncBridge::GetData(StorageKeyList storage_keys,
                                           DataCallback callback) {}

void ProfileAuthServersSyncBridge::GetAllDataForDebugging(
    DataCallback callback) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& uri : servers_uris_) {
    batch->Put(uri, ToEntityDataPtr(uri));
  }
  std::move(callback).Run(std::move(batch));
}

std::string ProfileAuthServersSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string ProfileAuthServersSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_printers_authorization_server());
  return entity_data.specifics.printers_authorization_server().uri();
}

void ProfileAuthServersSyncBridge::OnCommit(
    const absl::optional<syncer::ModelError>& error) {
  if (error) {
    LOG(WARNING) << "Failed to commit operation to store in "
                    "ProfileAuthServersSyncBridge";
    change_processor()->ReportError(*error);
  }
}

}  // namespace ash::printing::oauth2
