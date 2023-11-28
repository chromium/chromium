// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/android/webapk/webapk_database.h"
#include "chrome/browser/android/webapk/webapk_specifics_fetcher.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/model_type_sync_bridge.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace syncer {
struct EntityData;
class MetadataChangeList;
class ModelError;
class ModelTypeChangeProcessor;
}  // namespace syncer

namespace webapk {

class AbstractWebApkDatabaseFactory;

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
// ModelTypeChangeProcessor and WebApkDatabase (the storage).
class WebApkSyncBridge : public syncer::ModelTypeSyncBridge {
 public:
  WebApkSyncBridge(AbstractWebApkDatabaseFactory* database_factory,
                   base::OnceClosure on_initialized);
  // Tests may inject mocks using this ctor.
  WebApkSyncBridge(
      AbstractWebApkDatabaseFactory* database_factory,
      base::OnceClosure on_initialized,
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      std::unique_ptr<base::Clock> clock,
      std::unique_ptr<AbstractWebApkSpecificsFetcher> specifics_fetcher);
  WebApkSyncBridge(const WebApkSyncBridge&) = delete;
  WebApkSyncBridge& operator=(const WebApkSyncBridge&) = delete;
  ~WebApkSyncBridge() override;

  using CommitCallback = base::OnceCallback<void(bool success)>;

  // syncer::ModelTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  absl::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  absl::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // internal helpers, exposed for testing.
  bool AppWasUsedRecently(const sync_pb::WebApkSpecifics* specifics) const;

  // PrepareSyncUpdateFromInstalledApps compiles a vector of changes that need
  // to be applied to the remote sync data, based on the apps that are installed
  // on the device. This is "Step 1" of MergeFullSyncData() (see
  // https://docs.google.com/document/d/1Pce17EEuIs0dIbw-L1RZVf2HA4H8-Lu8RqVxHGmdJds).
  // We don't need to consider apps from the db/registry yet - that will be
  // covered in "Step 3".
  //
  // Being more specific, this step gathers all the apps that are installed
  // locally that have been used or modified recently enough _except_ the ones
  // that are already in the remote sync data and have been used more recently
  // than the local versions (ie, we want only the latest, most up-to-date
  // version of the metadata).
  //
  // |installed_apps| and |sync_changes| are inputs to this function, and
  // |sync_update_from_installed| is the output.
  void PrepareSyncUpdateFromInstalledApps(
      const std::vector<std::unique_ptr<sync_pb::WebApkSpecifics>>&
          installed_apps,
      const syncer::EntityChangeList& sync_changes,
      std::vector<const sync_pb::WebApkSpecifics*>* sync_update_from_installed)
      const;

  // PrepareRegistryUpdateFromInstalledAndSyncApps creates a collection of apps
  // that need to be added to or removed from the db and registry, based on a
  // combination of the app list gathered in
  // PrepareSyncUpdateFromInstalledApps(), and the changes from Sync. This is
  // "Step 2" of
  // https://docs.google.com/document/d/1Pce17EEuIs0dIbw-L1RZVf2HA4H8-Lu8RqVxHGmdJds.
  //
  // Here, we want to gather all the apps from |sync_update_from_installed|,
  // plus everything from the remote sync data except the ones that have already
  // been covered by |sync_update_from_installed|. Effectively this means that
  // we're gathering everything from Sync, plus all the relevant
  // locally-installed apps, so we can bring the registry up to date with both
  // data sources.
  //
  // |sync_update_from_installed| and |sync_changes| are inputs, and
  // |registry_update_from_installed_and_sync| is the output.
  //
  // The return value indicates whether or not there were new apps from Sync
  // that were not already installed on the device (and therefore are candidates
  // to be Restored from backup).
  bool PrepareRegistryUpdateFromInstalledAndSyncApps(
      const std::vector<const sync_pb::WebApkSpecifics*>&
          sync_update_from_installed,
      const syncer::EntityChangeList& sync_changes,
      RegistryUpdateData* registry_update_from_installed_and_sync) const;

  void OnWebApkUsed(std::unique_ptr<sync_pb::WebApkSpecifics> app_specifics);
  void OnWebApkUninstalled(const std::string& manifest_id);

  const Registry& GetRegistryForTesting() const;

 private:
  void ReportErrorToChangeProcessor(const syncer::ModelError& error);
  void OnDatabaseOpened(base::OnceClosure callback,
                        Registry registry,
                        std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void OnDataWritten(CommitCallback callback, bool success);

  // SendInstalledAndRegistryAppsToSync sends a collection of updates to Sync
  // based on a combination of |registry_| and the app lists gathered in
  // PrepareSyncUpdateFromInstalledApps() and
  // PrepareRegistryUpdateFromInstalledAndSyncApps(). This is "Step 3" of
  // https://docs.google.com/document/d/1Pce17EEuIs0dIbw-L1RZVf2HA4H8-Lu8RqVxHGmdJds.
  //
  // Concretely, we send all the updates from installed apps
  // (|sync_update_from_installed|) as well as everything in |registry_| that
  // isn't already covered in |registry_update_from_installed_and_sync|. In
  // other words, this pushes all relevant updates from installed apps, and
  // anything in the registry that appeared in neither installed nor synced
  // apps.
  //
  // |sync_update_from_installed| and |registry_update_from_installed_and_sync|
  // are inputs, and |metadata_change_list| is appended to as an output.
  void SendInstalledAndRegistryAppsToSync(
      const std::vector<const sync_pb::WebApkSpecifics*>&
          sync_update_from_installed,
      const std::unique_ptr<RegistryUpdateData>&
          registry_update_from_installed_and_sync,
      syncer::MetadataChangeList* metadata_change_list);

  // ApplyIncrementalSyncChangesToRegistry applies the changes in the app list
  // gathered in PrepareRegistryUpdateFromInstalledAndSyncApps() to the
  // registry. This is "Step 5" (the final step) from
  // https://docs.google.com/document/d/1Pce17EEuIs0dIbw-L1RZVf2HA4H8-Lu8RqVxHGmdJds.
  void ApplyIncrementalSyncChangesToRegistry(
      std::unique_ptr<RegistryUpdateData> update_data);
  void PrepareRegistryUpdateFromSyncApps(
      const syncer::EntityChangeList& sync_changes,
      RegistryUpdateData* registry_update_from_sync) const;

  void AddOrModifyAppInSync(std::unique_ptr<WebApkProto> app);
  void DeleteAppFromSync(const webapps::AppId& app_id);

  std::unique_ptr<WebApkDatabase> database_;
  Registry registry_;
  std::unique_ptr<base::Clock> clock_;
  std::unique_ptr<AbstractWebApkSpecificsFetcher> webapk_specifics_fetcher_;

  base::WeakPtrFactory<WebApkSyncBridge> weak_ptr_factory_{this};
};

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(
    const WebApkProto& app);
webapps::AppId ManifestIdStrToAppId(const std::string& manifest_id);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
