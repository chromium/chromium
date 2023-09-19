// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_

#include <memory>

#include "base/memory/weak_ptr.h"
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
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor);
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

 private:
  void OnDatabaseOpened(base::OnceClosure callback,
                        Registry registry,
                        std::unique_ptr<syncer::MetadataBatch> metadata_batch);
  void ReportErrorToChangeProcessor(const syncer::ModelError& error);

  std::unique_ptr<WebApkDatabase> database_;
  Registry registry_;

  base::WeakPtrFactory<WebApkSyncBridge> weak_ptr_factory_{this};
};

std::unique_ptr<syncer::EntityData> CreateSyncEntityData(
    const WebApkProto& app);

}  // namespace webapk

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_SYNC_BRIDGE_H_
