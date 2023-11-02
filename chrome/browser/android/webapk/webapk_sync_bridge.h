// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/time/clock.h"
#include "chrome/browser/android/webapk/webapk_database.h"
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
      std::unique_ptr<base::Clock> clock);
  WebApkSyncBridge(const WebApkSyncBridge&) = delete;
  WebApkSyncBridge& operator=(const WebApkSyncBridge&) = delete;
  ~WebApkSyncBridge() override;

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
  void PrepareRegistryUpdateFromInstalledAndSyncApps(
      const std::vector<const sync_pb::WebApkSpecifics*>&
          sync_update_from_installed,
      const syncer::EntityChangeList& sync_changes,
      RegistryUpdateData* registry_update_from_installed_and_sync) const;

 private:
  void OnDatabaseOpened(base::OnceClosure callback,
                        Registry registry,
                        std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void ReportErrorToChangeProcessor(const syncer::ModelError& error);

  std::unique_ptr<WebApkDatabase> database_;
  Registry registry_;
  std::unique_ptr<base::Clock> clock_;

  base::WeakPtrFactory<WebApkSyncBridge> weak_ptr_factory_{this};
};

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(
    const WebApkProto& app);
webapps::AppId ManifestIdStrToAppId(const std::string& manifest_id);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
