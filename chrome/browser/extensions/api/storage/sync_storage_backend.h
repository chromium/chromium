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
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "components/sync/model/syncable_service.h"
#include "components/value_store/value_store_factory.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/api/storage/settings_storage_quota_enforcer.h"
#include "extensions/common/extension_id.h"

namespace value_store {
class ValueStoreFactory;
}

namespace extensions {

class SettingsSyncProcessor;
class SyncableSettingsStorage;

// Manages ValueStore objects for extensions, including routing
// changes from sync to them.
// Lives entirely on the FILE thread.
class SyncStorageBackend final : public syncer::SyncableService {
 public:
  // |storage_factory| is use to create leveldb storage areas.
  // |observers| is the list of observers to settings changes.
  SyncStorageBackend(
      scoped_refptr<value_store::ValueStoreFactory> storage_factory,
      const SettingsStorageQuotaEnforcer::Limits& quota,
      SequenceBoundSettingsChangedCallback observer,
      syncer::DataType sync_type,
      const syncer::SyncableService::StartSyncFlare& flare);

  SyncStorageBackend(const SyncStorageBackend&) = delete;
  SyncStorageBackend& operator=(const SyncStorageBackend&) = delete;

  ~SyncStorageBackend() override;

  virtual value_store::ValueStore* GetStorage(const ExtensionId& extension_id);
  virtual void DeleteStorage(const ExtensionId& extension_id);

  // syncer::SyncableService implementation.
  void WaitUntilReadyToSync(base::OnceClosure done) override;
  syncer::SyncDataList GetAllSyncDataForTesting(syncer::DataType type) const;
  std::optional<syncer::ModelError> MergeDataAndStartSyncing(
      syncer::DataType type,
      const syncer::SyncDataList& initial_sync_data,
      std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) override;
  std::optional<syncer::ModelError> ProcessSyncChanges(
      const base::Location& from_here,
      const syncer::SyncChangeList& change_list) override;
  void StopSyncing(syncer::DataType type) override;
  base::WeakPtr<SyncableService> AsWeakPtr() override;

 private:
  // Gets a weak reference to the storage area for a given extension,
  // initializing sync with some initial data if sync enabled.
  SyncableSettingsStorage* GetOrCreateStorageWithSyncData(
      const ExtensionId& extension_id,
      base::Value::Dict sync_data) const;

  // Creates a new SettingsSyncProcessor for an extension.
  std::unique_ptr<SettingsSyncProcessor> CreateSettingsSyncProcessor(
      const ExtensionId& extension_id) const;

  // The Factory to use for creating new ValueStores.
  const scoped_refptr<value_store::ValueStoreFactory> storage_factory_;

  // Quota limits (see SettingsStorageQuotaEnforcer).
  const SettingsStorageQuotaEnforcer::Limits quota_;

  // Observer to settings changes.
  SequenceBoundSettingsChangedCallback observer_;

  // A cache of ValueStore objects that have already been created.
  // Ensure that there is only ever one created per extension.
  using StorageObjMap =
      std::map<ExtensionId, std::unique_ptr<SyncableSettingsStorage>>;
  mutable StorageObjMap storage_objs_;

  // Current sync data type. Either EXTENSION_SETTINGS or APP_SETTINGS.
  syncer::DataType sync_type_;

  // Current sync processor, if any.
  std::unique_ptr<syncer::SyncChangeProcessor> sync_processor_;

  syncer::SyncableService::StartSyncFlare flare_;

  base::WeakPtrFactory<SyncStorageBackend> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNC_STORAGE_BACKEND_H_
