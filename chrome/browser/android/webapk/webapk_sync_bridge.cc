// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_sync_bridge.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_database_factory.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/browser/android/webapk/webapk_registry_update.h"
#include "chrome/browser/android/webapk/webapk_specifics_fetcher.h"
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

std::unique_ptr<WebApkProto> CloneWebApkProto(const WebApkProto& app) {
  std::unique_ptr<WebApkProto> clone = std::make_unique<WebApkProto>();

  *clone = app;

  sync_pb::WebApkSpecifics* mutable_specifics = clone->mutable_sync_data();
  *mutable_specifics = app.sync_data();

  return clone;
}

}  // anonymous namespace

WebApkSyncBridge::WebApkSyncBridge(
    AbstractWebApkDatabaseFactory* database_factory,
    base::OnceClosure on_initialized)
    : WebApkSyncBridge(
          database_factory,
          std::move(on_initialized),
          std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
              syncer::WEB_APKS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel())),
          std::make_unique<base::DefaultClock>(),
          std::make_unique<WebApkSpecificsFetcher>()) {}

WebApkSyncBridge::WebApkSyncBridge(
    AbstractWebApkDatabaseFactory* database_factory,
    base::OnceClosure on_initialized,
    std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<AbstractWebApkSpecificsFetcher> specifics_fetcher)
    : syncer::ModelTypeSyncBridge(std::move(change_processor)),
      database_(
          database_factory,
          base::BindRepeating(&WebApkSyncBridge::ReportErrorToChangeProcessor,
                              base::Unretained(this))),
      clock_(std::move(clock)),
      webapk_specifics_fetcher_(std::move(specifics_fetcher)) {
  database_.OpenDatabase(base::BindOnce(&WebApkSyncBridge::OnDatabaseOpened,
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
  DCHECK(database_.is_opened());

  // Provide sync metadata to the processor _before_ any local changes occur.
  change_processor()->ModelReadyToSync(std::move(metadata_batch));

  registry_ = std::move(registry);
  std::move(callback).Run();
  if (init_done_callback_) {
    std::move(init_done_callback_).Run(/* initialized= */ true);
  }
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

void WebApkSyncBridge::OnDataWritten(CommitCallback callback, bool success) {
  if (!success) {
    DLOG(ERROR) << "WebApkSyncBridge commit failed";
  }

  base::UmaHistogramBoolean("WebApk.Database.WriteResult", success);
  std::move(callback).Run(success);
}

void WebApkSyncBridge::ApplyIncrementalSyncChangesToRegistry(
    std::unique_ptr<RegistryUpdateData> update_data) {
  if (update_data->isEmpty()) {
    return;
  }

  for (auto& app : update_data->apps_to_create) {
    webapps::AppId app_id =
        ManifestIdStrToAppId(app->sync_data().manifest_id());
    auto it = registry_.find(app_id);
    if (it != registry_.end()) {
      registry_.erase(it);
    }
    registry_.emplace(std::move(app_id), std::move(app));
  }

  for (const webapps::AppId& app_id : update_data->apps_to_delete) {
    auto it = registry_.find(app_id);
    CHECK(it != registry_.end());
    registry_.erase(it);
  }
}

bool WebApkSyncBridge::SyncDataContainsNewApps(
    const std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>&
        installed_apps,
    const syncer::EntityChangeList& sync_changes) const {
  std::set<webapps::AppId> sync_update_from_installed_set;
  for (const std::unique_ptr<sync_pb::WebApkSpecifics>& sync_update :
       installed_apps) {
    webapps::AppId app_id = ManifestIdStrToAppId(sync_update->manifest_id());
    sync_update_from_installed_set.insert(app_id);
  }

  for (const auto& sync_change : sync_changes) {
    if (sync_update_from_installed_set.count(sync_change->storage_key()) != 0) {
      continue;
    }

    if (sync_change->type() == syncer::EntityChange::ACTION_DELETE) {
      continue;
    }

    // There are changes from sync that aren't installed on the device.
    return true;
  }

  return false;
}

std::optional<syncer::ModelError> WebApkSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  CHECK(change_processor()->IsTrackingMetadata());

  const std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>> installed_apps =
      webapk_specifics_fetcher_->GetWebApkSpecifics();
  if (SyncDataContainsNewApps(installed_apps, entity_changes)) {
    // There are apps stored in Sync that aren't currently installed on the
    // device.
    WebappRegistry
        webapp_registry;  // TODO(crbug.com/1497527): WebappRegistry is supposed
                          // to be owned by ChromeBrowsingDataRemoverDelegate.
    webapp_registry.SetNeedsPwaRestore();
  }

  // Since we're using "account-only" semantics for Transport Mode, we just call
  // through to ApplyIncrementalSyncChanges().
  return ApplyIncrementalSyncChanges(std::move(metadata_change_list),
                                     std::move(entity_changes));
}

void WebApkSyncBridge::RegisterDoneInitializingCallback(
    base::OnceCallback<void(bool)> init_done_callback) {
  if (database_.is_opened()) {
    std::move(init_done_callback).Run(/* initialized= */ true);
    return;
  }

  init_done_callback_ = std::move(init_done_callback);
}

void WebApkSyncBridge::MergeSyncDataForTesting(
    std::vector<std::vector<std::string>> app_vector,
    std::vector<int> last_used_days_vector) {
  CHECK(database_.is_opened());
  CHECK(app_vector.size() == last_used_days_vector.size());

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
  std::unique_ptr<webapk::RegistryUpdateData> registry_update =
      std::make_unique<webapk::RegistryUpdateData>();

  int i = 0;
  for (auto const& app : app_vector) {
    std::unique_ptr<sync_pb::WebApkSpecifics> specifics =
        std::make_unique<sync_pb::WebApkSpecifics>();
    specifics->set_manifest_id(app[0]);
    specifics->set_name(app[1]);
    base::Time time = base::Time::Now() - base::Days(last_used_days_vector[i]);
    specifics->set_last_used_time_windows_epoch_micros(
        time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    registry_update->apps_to_create.push_back(
        WebApkProtoFromSpecifics(specifics.get(), false));
    i++;
  }

  database_.Write(
      *registry_update, std::move(metadata_change_list),
      base::BindOnce(&WebApkSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistry(std::move(registry_update));
}

void WebApkSyncBridge::PrepareRegistryUpdateFromSyncApps(
    const syncer::EntityChangeList& sync_changes,
    RegistryUpdateData* registry_update_from_sync) const {
  for (const auto& sync_change : sync_changes) {
    if (sync_change->type() == syncer::EntityChange::ACTION_DELETE) {
      // There's no need to queue up a deletion if the app doesn't exist in the
      // registry in the first place.
      if (GetAppByIdMutable(registry_, sync_change->storage_key()) != nullptr) {
        registry_update_from_sync->apps_to_delete.push_back(
            sync_change->storage_key());
      }
      continue;
    }

    CHECK(sync_change->data().specifics.has_web_apk());
    registry_update_from_sync->apps_to_create.push_back(
        WebApkProtoFromSpecifics(&sync_change->data().specifics.web_apk(),
                                 false /* installed */));
  }
}

std::optional<syncer::ModelError> WebApkSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  std::unique_ptr<RegistryUpdateData> registry_update_from_sync =
      std::make_unique<RegistryUpdateData>();
  PrepareRegistryUpdateFromSyncApps(entity_changes,
                                    registry_update_from_sync.get());

  database_.Write(
      *registry_update_from_sync, std::move(metadata_change_list),
      base::BindOnce(&WebApkSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistry(std::move(registry_update_from_sync));

  return std::nullopt;
}

void WebApkSyncBridge::OnWebApkUsed(
    std::unique_ptr<sync_pb::WebApkSpecifics> app_specifics) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  AddOrModifyAppInSync(
      WebApkProtoFromSpecifics(app_specifics.get(), true /* installed */));
}

void WebApkSyncBridge::OnWebApkUninstalled(const std::string& manifest_id) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  webapps::AppId app_id = ManifestIdStrToAppId(manifest_id);
  WebApkProto* app = GetAppByIdMutable(registry_, app_id);

  if (app == nullptr) {
    return;
  }

  if (!AppWasUsedRecently(&app->sync_data())) {
    DeleteAppFromSync(app_id);
    return;
  }

  // This updates the registry entry directly, so we don't need to call
  // ApplyIncrementalSyncChangesToRegistry() later.
  app->set_is_locally_installed(false);

  // We don't need to update Sync, since this change only affects the
  // non-Specifics part of the proto.
  std::unique_ptr<RegistryUpdateData> registry_update =
      std::make_unique<RegistryUpdateData>();
  registry_update->apps_to_create.push_back(CloneWebApkProto(*app));

  database_.Write(
      *registry_update,
      syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList(),
      base::BindOnce(&WebApkSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
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

void WebApkSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  database_.DeleteAllDataAndMetadata(base::DoNothing());

  registry_.clear();
}

void WebApkSyncBridge::AddOrModifyAppInSync(std::unique_ptr<WebApkProto> app) {
  webapps::AppId app_id = ManifestIdStrToAppId(app->sync_data().manifest_id());
  std::unique_ptr<syncer::EntityData> entity_data =
      CreateSyncEntityDataFromSpecifics(app->sync_data());

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
  change_processor()->Put(app_id, std::move(entity_data),
                          metadata_change_list.get());

  std::unique_ptr<RegistryUpdateData> registry_update =
      std::make_unique<RegistryUpdateData>();
  registry_update->apps_to_create.push_back(std::move(app));

  database_.Write(
      *registry_update, std::move(metadata_change_list),
      base::BindOnce(&WebApkSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistry(std::move(registry_update));
}

void WebApkSyncBridge::DeleteAppFromSync(const webapps::AppId& app_id) {
  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::ModelTypeStore::WriteBatch::CreateMetadataChangeList();
  change_processor()->Delete(app_id, metadata_change_list.get());

  std::unique_ptr<RegistryUpdateData> registry_update =
      std::make_unique<RegistryUpdateData>();
  registry_update->apps_to_delete.push_back(app_id);

  database_.Write(
      *registry_update, std::move(metadata_change_list),
      base::BindOnce(&WebApkSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistry(std::move(registry_update));
}

const Registry& WebApkSyncBridge::GetRegistryForTesting() const {
  return registry_;
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
WebApkSyncBridge::GetModelTypeControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}

}  // namespace webapk
