// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_bridge.h"

#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/browser/android/webapk/webapk_registry_update.h"
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

std::unique_ptr<syncer::EntityData> CreateSyncEntityDataFromSpecifics(
    const sync_pb::WebApkSpecifics& app) {
  // The Sync System doesn't allow empty entity_data name.
  CHECK(!app.name().empty());

  auto entity_data = std::make_unique<syncer::EntityData>();
  entity_data->name = app.name();

  *(entity_data->specifics.mutable_web_apk()) = app;
  return entity_data;
}

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(
    const WebApkProto& app) {
  return CreateSyncEntityDataFromSpecifics(app.sync_data());
}

webapps::AppId ManifestIdStrToAppId(const std::string& manifest_id) {
  GURL manifest_id_gurl(manifest_id);
  CHECK(manifest_id_gurl.is_valid()) << "manifest_id: " << manifest_id;
  return GenerateAppIdFromManifestId(manifest_id_gurl.GetWithoutRef());
}

namespace {

constexpr base::TimeDelta kRecentAppMaxAge = base::Days(30);

const WebApkProto* GetAppById(const Registry& registry,
                              const webapps::AppId& app_id) {
  auto it = registry.find(app_id);
  if (it != registry.end()) {
    return it->second.get();
  }

  return nullptr;
}

WebApkProto* GetAppByIdMutable(const Registry& registry,
                               const webapps::AppId& app_id) {
  return const_cast<WebApkProto*>(GetAppById(registry, app_id));
}

std::unique_ptr<WebApkProto> WebApkProtoFromSpecifics(
    const sync_pb::WebApkSpecifics* app,
    bool installed) {
  std::unique_ptr<WebApkProto> app_proto = std::make_unique<WebApkProto>();
  app_proto->set_is_locally_installed(installed);

  sync_pb::WebApkSpecifics* mutable_specifics = app_proto->mutable_sync_data();
  *mutable_specifics = *app;

  return app_proto;
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
                                  chrome::GetChannel())),
          std::make_unique<base::DefaultClock>()) {}

WebApkSyncBridge::WebApkSyncBridge(
    AbstractWebApkDatabaseFactory* database_factory,
    base::OnceClosure on_initialized,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    std::unique_ptr<base::Clock> clock)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)),
      clock_(std::move(clock)) {
  CHECK(database_factory);
  database_ = std::make_unique<WebApkDatabase>(
      database_factory,
      base::BindRepeating(&WebApkSyncBridge::ReportErrorToChangeProcessor,
                          base::Unretained(this)));

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

bool WebApkSyncBridge::AppWasUsedRecently(
    const sync_pb::WebApkSpecifics* specifics) const {
  base::Time app_last_used = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics->last_used_time_windows_epoch_micros()));
  return clock_->Now() - app_last_used < kRecentAppMaxAge;
}

void WebApkSyncBridge::PrepareSyncUpdateFromInstalledApps(
    const std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>&
        installed_apps,
    const syncer::EntityChangeList& sync_changes,
    std::vector<const sync_pb::WebApkSpecifics*>* sync_update_from_installed)
    const {
  std::map<webapps::AppId, const std::unique_ptr<syncer::EntityChange>&>
      sync_changes_map;
  for (const auto& sync_change : sync_changes) {
    sync_changes_map.insert(
        std::pair<webapps::AppId, const std::unique_ptr<syncer::EntityChange>&>(
            sync_change->storage_key(), sync_change));
  }

  for (const auto& installed_app : installed_apps) {
    if (!AppWasUsedRecently(installed_app.get())) {
      continue;
    }

    webapps::AppId app_id = ManifestIdStrToAppId(installed_app->manifest_id());
    auto it = sync_changes_map.find(app_id);
    if (it == sync_changes_map.end()) {
      sync_update_from_installed->push_back(installed_app.get());
      continue;
    }

    const std::unique_ptr<syncer::EntityChange>& sync_change = it->second;
    if (sync_change->type() == syncer::EntityChange::ACTION_DELETE) {
      sync_update_from_installed->push_back(installed_app.get());
      continue;
    }

    CHECK(sync_change->data().specifics.has_web_apk());
    if (installed_app->last_used_time_windows_epoch_micros() >=
        sync_change->data()
            .specifics.web_apk()
            .last_used_time_windows_epoch_micros()) {
      sync_update_from_installed->push_back(installed_app.get());
    }
  }
}

void WebApkSyncBridge::PrepareRegistryUpdateFromInstalledAndSyncApps(
    const std::vector<const sync_pb::WebApkSpecifics*>&
        sync_update_from_installed,
    const syncer::EntityChangeList& sync_changes,
    RegistryUpdateData* registry_update_from_installed_and_sync) const {
  std::set<webapps::AppId> sync_update_from_installed_set;
  for (const sync_pb::WebApkSpecifics* sync_update :
       sync_update_from_installed) {
    webapps::AppId app_id = ManifestIdStrToAppId(sync_update->manifest_id());
    sync_update_from_installed_set.insert(app_id);

    registry_update_from_installed_and_sync->apps_to_create.push_back(
        WebApkProtoFromSpecifics(sync_update, true /* installed */));
  }

  for (const auto& sync_change : sync_changes) {
    if (sync_update_from_installed_set.count(sync_change->storage_key()) != 0) {
      continue;
    }

    if (sync_change->type() == syncer::EntityChange::ACTION_DELETE) {
      // There's no need to queue up a deletion if the app doesn't exist in the
      // registry in the first place.
      if (GetAppByIdMutable(registry_, sync_change->storage_key()) != nullptr) {
        registry_update_from_installed_and_sync->apps_to_delete.push_back(
            sync_change->storage_key());
      }
      continue;
    }

    CHECK(sync_change->data().specifics.has_web_apk());
    registry_update_from_installed_and_sync->apps_to_create.push_back(
        WebApkProtoFromSpecifics(&sync_change->data().specifics.web_apk(),
                                 false /* installed */));
  }
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

  return ManifestIdStrToAppId(entity_data.specifics.web_apk().manifest_id());
}

std::string WebApkSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) {
  return GetClientTag(entity_data);
}

}  // namespace webapk
