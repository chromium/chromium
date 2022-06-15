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
#include "base/memory/ptr_util.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/model_type_store_base.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/printers_authorization_server_specifics.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash::printing::oauth2 {

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
  observer_->OnProfileAuthorizationServersInitialized();
}

std::unique_ptr<syncer::MetadataChangeList>
ProfileAuthServersSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> ProfileAuthServersSyncBridge::MergeSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
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
    DataCallback callback) {}

std::string ProfileAuthServersSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  return GetStorageKey(entity_data);
}

std::string ProfileAuthServersSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_printers_authorization_server());
  return entity_data.specifics.printers_authorization_server().uri();
}

}  // namespace ash::printing::oauth2
