// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_bridge.h"

#include "base/logging.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
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

WebApkSyncBridge::WebApkSyncBridge()
    : WebApkSyncBridge(
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEB_APPS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel()))) {}

WebApkSyncBridge::WebApkSyncBridge(
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)) {}

WebApkSyncBridge::~WebApkSyncBridge() = default;

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
  // TODO(hartmanng): implement
}

void WebApkSyncBridge::GetAllDataForDebugging(DataCallback callback) {
  // TODO(hartmanng): implement
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
