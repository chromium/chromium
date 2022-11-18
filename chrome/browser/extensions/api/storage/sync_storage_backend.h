// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_STORAGE_BACKEND_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_STORAGE_BACKEND_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/values.h"
#include "components/sync/model/syncable_service.h"
#include "components/value_store/value_store_factory.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"

namespace syncer {
class SyncErrorFactory;
}

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

class SettingsSyncProcessor;
class SyncableSettingsStorage;

// Manages ValueStore objects for extensions, including routing
// changes from sync to them.
// Lives entirely on the FILE thread.
class SyncStorageBackend : public syncer::SyncableService {
 public:
  // |storage_factory| is use to create leveldb storage areas.
  // |observers| is the list of observers to settings changes.
  SyncStorageBackend(
      scoped_refptr<value_store::ValueStoreFactory> storage_factory,
      const SettingsStorageQuotaEnforcer::Limits& quota,
      SequenceBoundSettingsChangedCallback observer,
      syncer::ModelType sync_type,
      const syncer::SyncableService::StartSyncFlare& flare);

  SyncStorageBackend(const SyncStorageBackend&) = delete;
  SyncStorageBackend& operator=(const SyncStorageBackend&) = delete;

  ~SyncStorageBackend() override;

  virtual value_store::ValueStore* GetStorage(const std::string& extension_id);
  virtual void DeleteStorage(const std::string& extension_id);

  // syncer::SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncDataList GetAllSyncDataForTesting(syncer::ModelType type) const;
  absl::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
      std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) override;
  absl::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  void StopSyncing(syncer::ModelType type) override;

 private:
  // Gets a weak reference to the storage area for a given extension,
  // initializing sync with some initial data if sync enabled.
  SyncableSettingsStorage* GetOrCreateStorageWithSyncData(
      const std::string& extension_id,
      base::Value::Dict sync_data) const;

  // Creates a new SettingsSyncProcessor for an extension.
  std::unique_ptr<SettingsSyncProcessor> CreateSettingsSyncProcessor(
      const std::string& extension_id) const;

  // The Factory to use for creating new ValueStores.
  const scoped_refptr<value_store::ValueStoreFactory> storage_factory_;

  // Quota limits (see SettingsStorageQuotaEnforcer).
  const SettingsStorageQuotaEnforcer::Limits quota_;

  // Observer to settings changes.
  SequenceBoundSettingsChangedCallback observer_;

  // A cache of ValueStore objects that have already been created.
  // Ensure that there is only ever one created per extension.
  using StorageObjMap =
      std::map<std::string, std::unique_ptr<SyncableSettingsStorage>>;
  mutable StorageObjMap storage_objs_;

  // Current sync model type. Either EXTENSION_SETTINGS or APP_SETTINGS.
  syncer::ModelType sync_type_;

  // Current sync processor, if any.
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  // Current sync error handler if any.
  std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory_;

  syncer::SyncableService::StartSyncFlare flare_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_STORAGE_BACKEND_H_
