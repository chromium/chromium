// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_type_store.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/web_app_specifics.pb.h"
#include "components/webapps/common/web_app_id.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace webapk {

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(
    const WebApkProto& app) {
  // The Sync System doesn't allow empty entity_data name.
  DCHECK(!app.sync_data().name().empty());

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = app.sync_data().name();

  *(entity_data->specifics.mutable_web_apk()) = app.sync_data();
  return entity_data;
}

namespace {

const WebApkProto* GetAppById(const Registry& registry,
                              const webapps::AppId& app_id) {
  auto it = registry.find(app_id);
  if (it != registry.end()) {
    return it->second.get();
  }

  return nullptr;
}

}  // anonymous namespace

WebApkSyncBridge::WebApkSyncBridge(
    AbstractWebApkDatabaseFactory* database_factory,
    base::OnceClosure on_initialized)
    : WebApkSyncBridge(
          database_factory,
          std::move(on_initialized),
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEB_APPS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel()))) {}

WebApkSyncBridge::WebApkSyncBridge(
    AbstractWebApkDatabaseFactory* database_factory,
    base::OnceClosure on_initialized,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {
  CHECK(database_factory);
  database_ = std::make_unique<WebApkDatabase>(
      database_factory,
      base::BindRepeating(&WebApkSyncBridge::ReportErrorToChangeProcessor,
                          base::Unretained(this)));

  // TODO(hartmanng): we may need to query installed WebAPKs on the device here
  // too.
  database_->OpenDatabase(base::BindOnce(&WebApkSyncBridge::OnDatabaseOpened,
                                         weak_ptr_factory_.GetWeakPtr(),
                                         std::move(on_initialized)));
}

WebApkSyncBridge::~WebApkSyncBridge() = default;

void WebApkSyncBridge::ReportErrorToChangeProcessor(
    const syncer::ModelError& error) {
  change_processor()->ReportError(error);
}

void WebApkSyncBridge::OnDatabaseOpened(
    base::OnceClosure callback,
    Registry registry,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK(database_->is_opened());

  // Provide sync metadata to the processor _before_ any local changes occur.
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  registry_ = std::move(registry);
  std::move(callback).Run();
}

std::unique_ptr<syncer::MetadataChangeList>
WebApkSyncBridge::CreateMetadataChangeList() {
  return syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
}

absl::optional<syncer::ModelError> WebApkSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // TODO(hartmanng): implement
  return absl::nullopt;
}

absl::optional<syncer::ModelError>
WebApkSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  // TODO(hartmanng): implement
  return absl::nullopt;
}

void WebApkSyncBridge::GetData(StorageKeyList storage_keys,
                               DataCallback callback) {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const webapps::AppId& app_id : storage_keys) {
    const WebApkProto* app = GetAppById(registry_, app_id);
    if (app) {
      data_batch->Put(app_id, CreateSyncEntityData(*app));
    }
  }

  std::move(callback).Run(std::move(data_batch));
}

void WebApkSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const auto& appListing : registry_) {
    const webapps::AppId app_id = appListing.first;
    const WebApkProto& app = *appListing.second;
    data_batch->Put(app_id, CreateSyncEntityData(app));
  }

  std::move(callback).Run(std::move(data_batch));
}

// GetClientTag and GetStorageKey must return the same thing for a given AppId
// as the dPWA implementation in
// chrome/browser/web_applications/web_app_sync_bridge.cc's
// WebAppSyncBridge::GetClientTag().
std::string WebApkSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_web_apk());

  const std::string& manifest_id =
      entity_data.specifics.web_apk().manifest_id();
  GURL manifest_id_gurl(manifest_id);
  CHECK(manifest_id_gurl.is_valid()) << "manifest_id: " << manifest_id;
  return GenerateAppIdFromManifestId(manifest_id_gurl.GetWithoutRef());
}

std::string WebApkSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetClientTag(entity_data);
}

}  // namespace webapk
