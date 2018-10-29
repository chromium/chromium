// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNCABLE_SETTINGS_STORAGE_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNCABLE_SETTINGS_STORAGE_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list_threadsafe.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/storage/setting_sync_data.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/model/syncable_service.h"
#include "extensions/browser/api/storage/settings_observer.h"
#include "extensions/browser/value_store/value_store.h"

namespace extensions {

class SettingsSyncProcessor;

// Decorates a ValueStore with sync behaviour.
class SyncableSettingsStorage : public ValueStore {
 public:
  SyncableSettingsStorage(scoped_refptr<SettingsObserverList> observers,
                          const std::string& extension_id,
                          // Ownership taken.
                          ValueStore* delegate,
                          syncer::ModelType sync_type,
                          const syncer::SyncableService::StartSyncFlare& flare);

  ~SyncableSettingsStorage() override;

  // ValueStore implementation.
  size_t GetBytesInUse(const std::string& key) override;
  size_t GetBytesInUse(const std::vector<std::string>& keys) override;
  size_t GetBytesInUse() override;
  ReadResult Get(const std::string& key) override;
  ReadResult Get(const std::vector<std::string>& keys) override;
  ReadResult Get() override;
  WriteResult Set(WriteOptions options,
                  const std::string& key,
                  const base::Value& value) override;
  WriteResult Set(WriteOptions options,
                  const base::DictionaryValue& values) override;
  WriteResult Remove(const std::string& key) override;
  WriteResult Remove(const std::vector<std::string>& keys) override;
  WriteResult Clear() override;

  // Sync-related methods, analogous to those on SyncableService (handled by
  // ExtensionSettings), but with looser guarantees about when the methods
  // can be called.

  // Starts syncing this storage area. Must only be called if sync isn't
  // already active.
  // |sync_state| is the current state of the extension settings in sync.
  // |sync_processor| is used to write out any changes.
  // Returns any error when trying to sync, or an empty error on success.
  syncer::SyncError StartSyncing(
      std::unique_ptr<base::DictionaryValue> sync_state,
      std::unique_ptr<SettingsSyncProcessor> sync_processor);

  // Stops syncing this storage area. May be called at any time (idempotent).
  void StopSyncing();

  // Pushes a list of sync changes into this storage area. May be called at any
  // time, changes will be ignored if sync isn't active.
  // Returns any error when trying to sync, or an empty error on success.
  syncer::SyncError ProcessSyncChanges(
      std::unique_ptr<SettingSyncDataList> sync_changes);

 private:
  // Sends the changes from |result| to sync if it's enabled.
  void SyncResultIfEnabled(const ValueStore::WriteResult& result);

  // Analyze the result returned by a call to the delegate, and take appropriate
  // measures.
  template <class T>
  T HandleResult(T result);

  // Sends all local settings to sync. This assumes that there are no settings
  // in sync yet.
  // Returns any error when trying to sync, or an empty error on success.
  syncer::SyncError SendLocalSettingsToSync(
      std::unique_ptr<base::DictionaryValue> local_state);

  // Overwrites local state with sync state.
  // Returns any error when trying to sync, or an empty error on success.
  syncer::SyncError OverwriteLocalSettingsWithSync(
      std::unique_ptr<base::DictionaryValue> sync_state,
      std::unique_ptr<base::DictionaryValue> local_state);

  // Called when an Add/Update/Remove comes from sync.
  syncer::SyncError OnSyncAdd(const std::string& key,
                              std::unique_ptr<base::Value> new_value,
                              ValueStoreChangeList* changes);
  syncer::SyncError OnSyncUpdate(const std::string& key,
                                 std::unique_ptr<base::Value> old_value,
                                 std::unique_ptr<base::Value> new_value,
                                 ValueStoreChangeList* changes);
  syncer::SyncError OnSyncDelete(const std::string& key,
                                 std::unique_ptr<base::Value> old_value,
                                 ValueStoreChangeList* changes);

  // List of observers to settings changes.
  const scoped_refptr<SettingsObserverList> observers_;

  // Id of the extension these settings are for.
  std::string const extension_id_;

  // Storage area to sync.
  const std::unique_ptr<ValueStore> delegate_;

  // Object which sends changes to sync.
  std::unique_ptr<SettingsSyncProcessor> sync_processor_;

  const syncer::ModelType sync_type_;
  const syncer::SyncableService::StartSyncFlare flare_;

  DISALLOW_COPY_AND_ASSIGN(SyncableSettingsStorage);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SYNCABLE_SETTINGS_STORAGE_H_
