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
#include "base/strings/utf_string_conversions.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_helpers.h"
#include "chrome/browser/android/webapk/webapk_registry_update.h"
#include "chrome/browser/android/webapk/webapk_restore_task.h"
#include "chrome/browser/android/webapk/webapk_specifics_fetcher.h"
#include "chrome/common/channel_info.h"
#include "components/sync/base/data_type.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_store_service.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"
#include "components/webapps/browser/android/shortcut_info.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
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
  if (!manifest_id_gurl.is_valid()) {
    LOG(ERROR) << "Invalid manifest_id: " << manifest_id;
    return "";
  }
  return GenerateAppIdFromManifestId(manifest_id_gurl.GetWithoutRef());
}

namespace {

constexpr base::TimeDelta kRecentAppMaxAge = base::Days(30);
constexpr char kSyncedWebApkAdditionHistogramName[] =
    "WebApk.Sync.SyncedWebApkAddition";

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

// Returns true if the specifics' timestamp is at most kRecentAppMaxAge before
// |time|. In other words, if |time| is Now, then this returns whether the
// specifics is at most kRecentAppMaxAge old.
bool AppWasUsedRecentlyComparedTo(const sync_pb::WebApkSpecifics* specifics,
                                  const base::Time time) {
  base::Time app_last_used = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(specifics->last_used_time_windows_epoch_micros()));
  return time - app_last_used < kRecentAppMaxAge;
}

// Create a |webapps::ShortcutInfo| from the synced |webapk_specifics|.
// If the data is invalid, returns nullptr.
std::unique_ptr<webapps::ShortcutInfo> CreateShortcutInfoFromSpecifics(
    const sync_pb::WebApkSpecifics& webapk_specifics) {
  GURL start_url(GURL(webapk_specifics.start_url()));
  if (!start_url.is_valid()) {
    return nullptr;
  }
  auto shortcut_info = std::make_unique<webapps::ShortcutInfo>(start_url);
  GURL manifest_id(webapk_specifics.manifest_id());
  if (manifest_id.is_valid()) {
    shortcut_info->manifest_id = manifest_id;
  }
  GURL scope(webapk_specifics.scope());
  if (scope.is_valid()) {
    shortcut_info->scope = scope;
  }
  std::u16string name = base::UTF8ToUTF16(webapk_specifics.name());
  shortcut_info->user_title = name;
  shortcut_info->name = name;
  shortcut_info->short_name = name;
  if (webapk_specifics.icon_infos().size() > 0) {
    shortcut_info->best_primary_icon_url =
        GURL(webapk_specifics.icon_infos(0).url());
    shortcut_info->is_primary_icon_maskable =
        webapps::WebappsIconUtils::DoesAndroidSupportMaskableIcons() &&
        webapk_specifics.icon_infos(0).purpose() ==
            sync_pb::WebApkIconInfo_Purpose_MASKABLE;
  } else {
    // If there is no icon url in sync data, put |start_url| as a place holder
    // for primary icon url. Download icon will fallback to generated icon.
    shortcut_info->best_primary_icon_url = start_url;
  }
  return shortcut_info;
}

// Legacy (pre-manifest-id) WebAPKs can have empty manifest_ids. These, in turn,
// get translated into empty app_ids via ManifestIdStrToAppId(). If we end up
// with an empty app_id, generally we need to abort Sync-handling and ignore
// that WebAPK for Sync purposes.
bool IsLegacyAppId(webapps::AppId app_id) {
  return app_id.empty();
}

}  // anonymous namespace

WebApkSyncBridge::WebApkSyncBridge(
    syncer::DataTypeStoreService* data_type_store_service,
    base::OnceClosure on_initialized)
    : WebApkSyncBridge(
          data_type_store_service,
          std::move(on_initialized),
          std::make_unique<syncer::ClientTagBasedDataTypeProcessor>(
              syncer::WEB_APKS,
              base::BindRepeating(&syncer::ReportUnrecoverableError,
                                  chrome::GetChannel())),
          std::make_unique<base::DefaultClock>(),
          std::make_unique<WebApkSpecificsFetcher>()) {}

WebApkSyncBridge::WebApkSyncBridge(
    syncer::DataTypeStoreService* data_type_store_service,
    base::OnceClosure on_initialized,
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    std::unique_ptr<base::Clock> clock,
    std::unique_ptr<AbstractWebApkSpecificsFetcher> specifics_fetcher)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      database_(
          data_type_store_service,
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
  for (auto& task : init_done_callback_) {
    std::move(task).Run(/* initialized= */ true);
  }
}

std::unique_ptr<syncer::MetadataChangeList>
WebApkSyncBridge::CreateMetadataChangeList() {
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

bool WebApkSyncBridge::AppWasUsedRecently(
    const sync_pb::WebApkSpecifics* specifics) const {
  return AppWasUsedRecentlyComparedTo(specifics, clock_->Now());
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
    if (IsLegacyAppId(app_id)) {
      continue;
    }

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
    if (!IsLegacyAppId(app_id)) {
      sync_update_from_installed_set.insert(app_id);
    }
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

  WebappRegistry::SetNeedsPwaRestore(
      SyncDataContainsNewApps(installed_apps, entity_changes));

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

  init_done_callback_.push_back(std::move(init_done_callback));
}

void WebApkSyncBridge::MergeSyncDataForTesting(
    std::vector<std::vector<std::string>> app_vector,
    std::vector<int> last_used_days_vector) {
  CHECK(database_.is_opened());
  CHECK(app_vector.size() == last_used_days_vector.size());

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
  std::unique_ptr<webapk::RegistryUpdateData> registry_update =
      std::make_unique<webapk::RegistryUpdateData>();

  int i = 0;
  for (auto const& app : app_vector) {
    std::unique_ptr<sync_pb::WebApkSpecifics> specifics =
        std::make_unique<sync_pb::WebApkSpecifics>();
    specifics->set_start_url(app[0]);
    specifics->set_manifest_id(app[0]);
    specifics->set_name(app[1]);

    const std::string icon_url = app[2];
    const int32_t icon_size_in_px = 256;
    const sync_pb::WebApkIconInfo_Purpose icon_purpose =
        sync_pb::WebApkIconInfo_Purpose_ANY;
    sync_pb::WebApkIconInfo* icon_info = specifics->add_icon_infos();
    icon_info->set_size_in_px(icon_size_in_px);
    icon_info->set_url(icon_url);
    icon_info->set_purpose(icon_purpose);

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
    std::unique_ptr<sync_pb::WebApkSpecifics> app_specifics,
    bool is_install) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  AddOrModifyAppInSync(
      WebApkProtoFromSpecifics(app_specifics.get(), true /* installed */),
      is_install);
}

void WebApkSyncBridge::OnWebApkUninstalled(const std::string& manifest_id) {
  if (!change_processor()->IsTrackingMetadata()) {
    return;
  }

  webapps::AppId app_id = ManifestIdStrToAppId(manifest_id);
  if (IsLegacyAppId(app_id)) {
    return;
  }
  WebApkProto* app = GetAppByIdMutable(registry_, app_id);

  if (app == nullptr) {
    return;
  }

  if (!AppWasUsedRecently(&app->sync_data())) {
    DeleteAppsFromSync(std::vector<webapps::AppId>{app_id},
                       database_.is_opened());
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
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList(),
      base::BindOnce(&WebApkSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));
}

std::vector<WebApkRestoreData> WebApkSyncBridge::GetRestorableAppsShortcutInfo()
    const {
  std::vector<WebApkRestoreData> results;
  for (auto const& [appId, proto] : registry_) {
    if (!proto->is_locally_installed() &&
        AppWasUsedRecently(&proto->sync_data())) {
      auto restore_info = CreateShortcutInfoFromSpecifics(proto->sync_data());
      if (restore_info) {
        results.emplace_back(WebApkRestoreData(
            appId, std::move(restore_info),
            base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(
                proto->sync_data().last_used_time_windows_epoch_micros()))));
      }
    }
  }
  return results;
}

const WebApkProto* WebApkSyncBridge::GetWebApkByAppId(
    webapps::AppId app_id) const {
  return GetAppById(registry_, app_id);
}

std::unique_ptr<syncer::DataBatch> WebApkSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const webapps::AppId& app_id : storage_keys) {
    const WebApkProto* app = GetAppById(registry_, app_id);
    if (app) {
      data_batch->Put(app_id, CreateSyncEntityData(*app));
    }
  }

  return data_batch;
}

std::unique_ptr<syncer::DataBatch> WebApkSyncBridge::GetAllDataForDebugging() {
  auto data_batch = std::make_unique<syncer::MutableDataBatch>();

  for (const auto& appListing : registry_) {
    const webapps::AppId app_id = appListing.first;
    const WebApkProto& app = *appListing.second;
    data_batch->Put(app_id, CreateSyncEntityData(app));
  }

  return data_batch;
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

void WebApkSyncBridge::RemoveOldWebAPKsFromSync(
    int64_t current_time_ms_since_unix_epoch) {
  std::vector<webapps::AppId> app_ids;
  for (const auto& appListing : registry_) {
    const webapps::AppId app_id = appListing.first;
    const WebApkProto& app = *appListing.second;
    if (!AppWasUsedRecentlyComparedTo(
            &app.sync_data(), base::Time::FromMillisecondsSinceUnixEpoch(
                                  current_time_ms_since_unix_epoch))) {
      app_ids.push_back(app_id);
    }
  }

  RegisterDoneInitializingCallback(
      base::BindOnce(&WebApkSyncBridge::DeleteAppsFromSync,
                     weak_ptr_factory_.GetWeakPtr(), app_ids));
}

void WebApkSyncBridge::AddOrModifyAppInSync(std::unique_ptr<WebApkProto> app,
                                            bool is_install) {
  webapps::AppId app_id = ManifestIdStrToAppId(app->sync_data().manifest_id());
  if (IsLegacyAppId(app_id)) {
    return;
  }
  RecordSyncedWebApkAdditionHistogram(is_install, registry_.count(app_id) > 0);

  std::unique_ptr<syncer::EntityData> entity_data =
      CreateSyncEntityDataFromSpecifics(app->sync_data());

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
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

void WebApkSyncBridge::DeleteAppsFromSync(
    const std::vector<webapps::AppId>& app_ids,
    bool database_opened) {
  if (app_ids.size() == 0 || !database_opened) {
    return;
  }

  RecordSyncedWebApkRemovalCountHistogram(app_ids.size());

  std::unique_ptr<syncer::MetadataChangeList> metadata_change_list =
      syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
  std::unique_ptr<RegistryUpdateData> registry_update =
      std::make_unique<RegistryUpdateData>();

  for (const webapps::AppId& app_id : app_ids) {
    change_processor()->Delete(app_id, syncer::DeletionOrigin::Unspecified(),
                               metadata_change_list.get());
    registry_update->apps_to_delete.push_back(app_id);
  }

  database_.Write(
      *registry_update, std::move(metadata_change_list),
      base::BindOnce(&WebApkSyncBridge::OnDataWritten,
                     weak_ptr_factory_.GetWeakPtr(), base::DoNothing()));

  ApplyIncrementalSyncChangesToRegistry(std::move(registry_update));
}

void WebApkSyncBridge::SetClockForTesting(std::unique_ptr<base::Clock> clock) {
  clock_ = std::move(clock);
}

const Registry& WebApkSyncBridge::GetRegistryForTesting() const {
  return registry_;
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
WebApkSyncBridge::GetDataTypeControllerDelegate() {
  return change_processor()->GetControllerDelegate();
}

void WebApkSyncBridge::RecordSyncedWebApkAdditionHistogram(
    bool is_install,
    bool already_exists_in_sync) const {
  if (is_install && !already_exists_in_sync) {
    base::UmaHistogramEnumeration(
        kSyncedWebApkAdditionHistogramName,
        AddOrModifyType::kNewInstallOnDeviceAndNewAddToSync);
  } else if (is_install && already_exists_in_sync) {
    base::UmaHistogramEnumeration(
        kSyncedWebApkAdditionHistogramName,
        AddOrModifyType::kNewInstallOnDeviceAndModificationToSync);
  } else if (!is_install && !already_exists_in_sync) {
    base::UmaHistogramEnumeration(
        kSyncedWebApkAdditionHistogramName,
        AddOrModifyType::kLaunchOnDeviceAndNewAddToSync);
  } else {
    base::UmaHistogramEnumeration(
        kSyncedWebApkAdditionHistogramName,
        AddOrModifyType::kLaunchOnDeviceAndModificationToSync);
  }
}

void WebApkSyncBridge::RecordSyncedWebApkRemovalCountHistogram(
    int num_web_apks_removed) const {
  base::UmaHistogramExactLinear("WebApk.Sync.SyncedWebApkRemovalCount",
                                num_web_apks_removed, 51 /* max_count */);
}

}  // namespace webapk
