// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/oauth2/profile_auth_servers_sync_bridge.h"

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_base.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/printers_authorization_server_specifics.pb.h"
#include "url/gurl.h"

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

std::set<GURL> ToSetOfUris(const std::set<std::string>& strs) {
  std::set<GURL> uris;
  for (const std::string& str : strs) {
    GURL uri(str);
    if (!uri.is_valid()) {
      LOG(WARNING) << "Failed to parse URI in ProfileAuthServersSyncBridge";
      continue;
    }
    uris.insert(std::move(uri));
  }
  return uris;
}

}  // namespace

std::unique_ptr<ProfileAuthServersSyncBridge>
ProfileAuthServersSyncBridge::Create(
    Observer* observer,
    syncer::OnceDataTypeStoreFactory store_factory) {
  DCHECK(observer);
  return base::WrapUnique(new ProfileAuthServersSyncBridge(
      std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
          syncer::PRINTERS_AUTHORIZATION_SERVERS,
          base::BindRepeating(&syncer::ReportUnrecoverableError,
                              chrome::GetChannel())),
      std::move(store_factory), observer));
}

std::unique_ptr<ProfileAuthServersSyncBridge>
ProfileAuthServersSyncBridge::CreateForTesting(
    Observer* observer,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory) {
  DCHECK(observer);
  DCHECK(change_processor);
  return base::WrapUnique(new ProfileAuthServersSyncBridge(
      std::move(change_processor), std::move(store_factory), observer));
}

ProfileAuthServersSyncBridge::~ProfileAuthServersSyncBridge() = default;

void ProfileAuthServersSyncBridge::AddAuthorizationServer(
    const GURL& server_uri) {
  DCHECK(initialization_completed_);
  const std::string key = server_uri.spec();
  DCHECK(!key.empty());
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
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory,
    Observer* observer)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      observer_(observer) {
  std::move(store_factory)
      .Run(syncer::PRINTERS_AUTHORIZATION_SERVERS,
           base::BindOnce(&ProfileAuthServersSyncBridge::OnStoreCreated,
                          weak_ptr_factory_.GetWeakPtr()));
}

void ProfileAuthServersSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
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
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> record_list) {
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const syncer::DataTypeStore::Record& r : *record_list) {
    sync_pb::PrintersAuthorizationServerSpecifics specifics;
    if (!specifics.ParseFromString(r.value)) {
      change_processor()->ReportError(
          {FROM_HERE, "Failed to deserialize all specifics."});
      return;
    }
    servers_uris_.insert(specifics.uri());
  }

  // Data loaded. Notify the observer and load metadata.
  NotifyObserver(servers_uris_, /*deleted=*/{});
  store_->ReadAllMetadata(
      base::BindOnce(&ProfileAuthServersSyncBridge::OnReadAllMetadata,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ProfileAuthServersSyncBridge::OnReadAllMetadata(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  TRACE_EVENT0("ui", "ProfileAuthServersSyncBridge::OnReadAllMetadata");
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
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError>
ProfileAuthServersSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  // Every local URI is considered unsynced until the contrary is proven, i.e.
  // until the same URI is seen in the incoming `entity_data`.
  std::set<std::string> unsynced_local_uris = servers_uris_;
  std::set<std::string> added_local_uris;
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();

  for (const std::unique_ptr<syncer::EntityChange>& change : entity_data) {
    const sync_pb::PrintersAuthorizationServerSpecifics& specifics =
        change->data().specifics.printers_authorization_server();
    const std::string& remote_uri = specifics.uri();
    DCHECK_EQ(change->storage_key(), remote_uri);
    auto [unused, is_new] = servers_uris_.insert(remote_uri);
    if (is_new) {
      added_local_uris.insert(remote_uri);
      batch->WriteData(remote_uri, specifics.SerializeAsString());
    } else {
      unsynced_local_uris.erase(remote_uri);
    }
  }

  // Send unmatched local URIs to the server.
  for (const std::string& uri : unsynced_local_uris) {
    change_processor()->Put(uri, ToEntityDataPtr(uri),
                            metadata_change_list.get());
  }

  // Save new local URIs to the local store.
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch), base::BindOnce(&ProfileAuthServersSyncBridge::OnCommit,
                                       weak_ptr_factory_.GetWeakPtr()));

  NotifyObserver(added_local_uris, /*deleted=*/{});
  return std::nullopt;
}

std::optional<syncer::ModelError>
ProfileAuthServersSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::set<std::string> added_local_uris;
  std::set<std::string> deleted_local_uris;

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> batch =
      store_->CreateWriteBatch();
  for (const std::unique_ptr<syncer::EntityChange>& change : entity_changes) {
    const std::string& uri = change->storage_key();
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      if (servers_uris_.erase(uri)) {
        batch->DeleteData(uri);
        deleted_local_uris.insert(uri);
      }
    } else {
      if (servers_uris_.insert(uri).second) {  // true <=> uri wasn't there
        batch->WriteData(uri, change->data()
                                  .specifics.printers_authorization_server()
                                  .SerializeAsString());
        added_local_uris.insert(uri);
      }
    }
  }
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_->CommitWriteBatch(
      std::move(batch), base::BindOnce(&ProfileAuthServersSyncBridge::OnCommit,
                                       weak_ptr_factory_.GetWeakPtr()));

  NotifyObserver(added_local_uris, deleted_local_uris);
  return std::nullopt;
}

std::unique_ptr<syncer::DataBatch>
ProfileAuthServersSyncBridge::GetDataForCommit(StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    if (base::Contains(servers_uris_, key)) {
      batch->Put(key, ToEntityDataPtr(key));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
ProfileAuthServersSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const auto& uri : servers_uris_) {
    batch->Put(uri, ToEntityDataPtr(uri));
  }
  return batch;
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
    const std::optional<syncer::ModelError>& error) {
  if (error) {
    LOG(WARNING) << "Failed to commit operation to store in "
                    "ProfileAuthServersSyncBridge";
    change_processor()->ReportError(*error);
  }
}

void ProfileAuthServersSyncBridge::NotifyObserver(
    const std::set<std::string>& added,
    const std::set<std::string>& deleted) {
  // Convert std::set<std::string> to std::set<GURL>.
  std::set<GURL> added_uris = ToSetOfUris(added);
  std::set<GURL> deleted_uris = ToSetOfUris(deleted);
  // Call the observer.
  if (!added_uris.empty() || !deleted_uris.empty()) {
    observer_->OnProfileAuthorizationServersUpdate(std::move(added_uris),
                                                   std::move(deleted_uris));
  }
}

}  // namespace ash::printing::oauth2
