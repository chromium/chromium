// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_PRINTING_PRINTERS_SYNC_BRIDGE_H_
#define CHROME_BROWSER_ASH_PRINTING_PRINTERS_SYNC_BRIDGE_H_

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/observer_list_threadsafe.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"

namespace sync_pb {
class PrinterSpecifics;
}  // namespace sync_pb

namespace ash {

// Moderates interaction with the backing database and integrates with the User
// Sync Service for printers.
class PrintersSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  PrintersSyncBridge(syncer::OnceDataTypeStoreFactory callback,
                     base::RepeatingClosure error_callback);

  PrintersSyncBridge(const PrintersSyncBridge&) = delete;
  PrintersSyncBridge& operator=(const PrintersSyncBridge&) = delete;

  ~PrintersSyncBridge() override;

  // DataTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  syncer::ConflictResolution ResolveConflict(
      const std::string& storage_key,
      const syncer::EntityData& remote_data) const override;

  // Stores a |printer|.  Overwrites a printer with a matching id if it exists.
  void AddPrinter(std::unique_ptr<sync_pb::PrinterSpecifics> printer);
  // Merges the |printer| with an existing |printer| if their ids match.
  // Otherwise, adds the printer.  Returns true if the printer is new.
  bool UpdatePrinter(std::unique_ptr<sync_pb::PrinterSpecifics> printer);
  // Removes a printer by |id|.
  bool RemovePrinter(const std::string& id);
  // Returns all printers stored in the database and synced.
  std::vector<sync_pb::PrinterSpecifics> GetAllPrinters() const;
  // Returns the printer with |id| from storage if it could be found.
  std::optional<sync_pb::PrinterSpecifics> GetPrinter(
      const std::string& id) const;
  // Returns whether or not the printer with |id| is contained in the storage.
  bool HasPrinter(const std::string& id) const;

  class Observer {
   public:
    virtual void OnPrintersUpdated() = 0;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  class StoreProxy;

  // Notifies the change processor that |printer| has been modified.
  void CommitPrinterPut(const sync_pb::PrinterSpecifics& printer);
  // Performs |printer| store after lock has been acquired.
  void AddPrinterLocked(std::unique_ptr<sync_pb::PrinterSpecifics> printer);
  // Stores |specifics| locally in |all_data_| and record change in |batch|.
  // |data_lock_| must be acquired before calling this funciton.
  void StoreSpecifics(std::unique_ptr<sync_pb::PrinterSpecifics> specifics,
                      syncer::DataTypeStore::WriteBatch* batch);
  // Removes the specific with |id| from |all_data_| and update |batch| with the
  // change. |data_lock_| must be acquired before calling this function.
  bool DeleteSpecifics(const std::string& id,
                       syncer::DataTypeStore::WriteBatch* batch);
  // Merges the |printer| with an existing |printer| if their ids match.
  // Otherwise, adds the printer.  Returns true if the printer is new.
  // |data_lock_| must be acquired before calling this funciton.
  bool UpdatePrinterLocked(std::unique_ptr<sync_pb::PrinterSpecifics> printer);
  // Call OnPrintersUpdated asynchronously on all registered observers.
  void NotifyPrintersUpdated();

  std::unique_ptr<StoreProxy> store_delegate_;
  scoped_refptr<base::ObserverListThreadSafe<Observer>> observers_;

  // Lock over |all_data_|.
  mutable base::Lock data_lock_;
  // In memory cache of printer information. Access to this is synchronized with
  // |data_lock_|.
  std::map<std::string, std::unique_ptr<sync_pb::PrinterSpecifics>> all_data_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_PRINTING_PRINTERS_SYNC_BRIDGE_H_
