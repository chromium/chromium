// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_

#include <memory>
#include <optional>

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_specifics_fetcher.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/protocol/web_apk_specifics.pb.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
class DataTypeStoreService;
struct EntityData;
class MetadataChangeList;
class ModelError;
}  // namespace syncer

namespace webapk {

struct WebApkRestoreData;

// A unified sync and storage controller.
//
// While webapk::Registry is a read-only model, WebApkSyncBridge is a
// controller for that model. WebApkSyncBridge is responsible for:
// - Registry initialization (reading model from a persistent storage like
// LevelDb or prefs).
// - Writing all the registry updates to a persistent store and sync.
//
// WebApkSyncBridge is the key class to support integration with Unified Sync
// and Storage (USS) system. The sync bridge exclusively owns
// DataTypeLocalChangeProcessor and WebApkDatabase (the storage).
class WebApkSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  WebApkSyncBridge(syncer::DataTypeStoreService* data_type_store_service,
                   base::OnceClosure on_initialized);
  // Tests may inject mocks using this ctor.
  WebApkSyncBridge(
      syncer::DataTypeStoreService* data_type_store_service,
      base::OnceClosure on_initialized,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      std::unique_ptr<base::Clock> clock,
      std::unique_ptr<AbstractWebApkSpecificsFetcher> specifics_fetcher);
  WebApkSyncBridge(const WebApkSyncBridge&) = delete;
  WebApkSyncBridge& operator=(const WebApkSyncBridge&) = delete;
  ~WebApkSyncBridge() override;

  using CommitCallback = base::OnceCallback<void(bool success)>;

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

  void RemoveOldWebAPKsFromSync(int64_t current_time_ms_since_unix_epoch);

  void RegisterDoneInitializingCallback(
      base::OnceCallback<void(bool)> init_done_callback);
  void MergeSyncDataForTesting(std::vector<std::vector<std::string>> app_vector,
                               std::vector<int> last_used_days_vector);

  // internal helpers, exposed for testing.
  bool AppWasUsedRecently(const sync_pb::WebApkSpecifics* specifics) const;

  void PrepareRegistryUpdateFromSyncApps(
      const syncer::EntityChangeList& sync_changes,
      RegistryUpdateData* registry_update_from_sync) const;

  bool SyncDataContainsNewApps(
      const std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>&
          installed_apps,
      const syncer::EntityChangeList& sync_changes) const;

  void OnWebApkUsed(std::unique_ptr<sync_pb::WebApkSpecifics> app_specifics,
                    bool is_install);
  void OnWebApkUninstalled(const std::string& manifest_id);

  // Get list of apps that are available to restore.
  std::vector<WebApkRestoreData> GetRestorableAppsShortcutInfo() const;

  const WebApkProto* GetWebApkByAppId(webapps::AppId app_id) const;

  void SetClockForTesting(std::unique_ptr<base::Clock> clock);

  const Registry& GetRegistryForTesting() const;

  base::WeakPtr<syncer::DataTypeControllerDelegate>
  GetDataTypeControllerDelegate();

 private:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class AddOrModifyType {
    // Note that the "Modification" items refer to _any_ modification of the
    // specifics proto - even just a timestamp change. This does not necessarily
    // imply that there is any change in the actual WebAPK.
    kNewInstallOnDeviceAndNewAddToSync = 0,
    kNewInstallOnDeviceAndModificationToSync = 1,
    kLaunchOnDeviceAndNewAddToSync = 2,
    kLaunchOnDeviceAndModificationToSync = 3,
    kMaxValue = kLaunchOnDeviceAndModificationToSync,
  };

  void ReportErrorToChangeProcessor(const syncer::ModelError& error);
  void OnDatabaseOpened(base::OnceClosure callback,
                        Registry registry,
                        std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnDataWritten(CommitCallback callback, bool success);

  // ApplyIncrementalSyncChangesToRegistry applies the changes in the app list
  // gathered in PrepareRegistryUpdateFromSyncApps() to the registry.
  void ApplyIncrementalSyncChangesToRegistry(
      std::unique_ptr<RegistryUpdateData> update_data);

  void AddOrModifyAppInSync(std::unique_ptr<WebApkProto> app, bool is_install);
  void DeleteAppsFromSync(const std::vector<webapps::AppId>& app_ids,
                          bool database_opened);

  void RecordSyncedWebApkAdditionHistogram(bool is_install,
                                           bool already_exists_in_sync) const;
  void RecordSyncedWebApkRemovalCountHistogram(int num_web_apks_removed) const;

  WebApkDatabase database_;
  Registry registry_;
  std::unique_ptr<base::Clock> clock_;
  std::unique_ptr<AbstractWebApkSpecificsFetcher> webapk_specifics_fetcher_;
  std::vector<base::OnceCallback<void(bool)>> init_done_callback_;

  base::WeakPtrFactory<WebApkSyncBridge> weak_ptr_factory_{this};
};

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(
    const WebApkProto& app);
webapps::AppId ManifestIdStrToAppId(const std::string& manifest_id);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
