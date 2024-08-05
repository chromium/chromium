// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/printing/printers_sync_bridge.h"

#include <set>
#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/printing/specifics_translation.h"
#include "chromeos/printing/printer_configuration.h"
#include "components/sync/base/deletion_origin.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/model/client_tag_based_data_type_processor.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/printer_specifics.pb.h"

namespace ash {

namespace {

using syncer::ClientTagBasedDataTypeProcessor;
using syncer::ConflictResolution;
using syncer::DataTypeLocalChangeProcessor;
using syncer::DataTypeStore;
using syncer::EntityChange;
using syncer::EntityChangeList;
using syncer::EntityData;
using syncer::MetadataChangeList;

std::unique_ptr<EntityData> CopyToEntityData(
    const sync_pb::PrinterSpecifics& specifics) {
  auto entity_data = std::make_unique<EntityData>();
  *entity_data->specifics.mutable_printer() = specifics;
  entity_data->name =
      specifics.display_name().empty() ? "PRINTER" : specifics.display_name();
  return entity_data;
}

// Computes the make_and_model field for old |specifics| where it is missing.
// Returns true if an update was made.  make_and_model is computed from the
// manufacturer and model strings.
bool MigrateMakeAndModel(sync_pb::PrinterSpecifics* specifics) {
  if (specifics->has_make_and_model()) {
    return false;
  }

  specifics->set_make_and_model(
      MakeAndModel(specifics->manufacturer(), specifics->model()));
  return true;
}

// If |specifics|'s PPD reference has both autoconf and another option selected,
// we strip the autoconf flag and return true, false otherwise.
bool ResolveInvalidPpdReference(sync_pb::PrinterSpecifics* specifics) {
  auto* ppd_ref = specifics->mutable_ppd_reference();

  if (!ppd_ref->autoconf())
    return false;

  if (!ppd_ref->has_user_supplied_ppd_url() &&
      !ppd_ref->has_effective_make_and_model()) {
    return false;
  }

  ppd_ref->clear_autoconf();
  return true;
}

}  // namespace

// Delegate class which helps to manage the DataTypeStore.
class PrintersSyncBridge::StoreProxy {
 public:
  StoreProxy(PrintersSyncBridge* owner,
             syncer::OnceDataTypeStoreFactory callback)
      : owner_(owner) {
    std::move(callback).Run(syncer::PRINTERS,
                            base::BindOnce(&StoreProxy::OnStoreCreated,
                                           weak_ptr_factory_.GetWeakPtr()));
  }

  // Returns true if the store has been initialized.
  bool Ready() { return store_.get() != nullptr; }

  // Returns a new WriteBatch.
  std::unique_ptr<DataTypeStore::WriteBatch> CreateWriteBatch() {
    DCHECK(store_);
    return store_->CreateWriteBatch();
  }

  // Commits writes to the database and updates metadata.
  void Commit(std::unique_ptr<DataTypeStore::WriteBatch> batch) {
    DCHECK(store_);
    store_->CommitWriteBatch(
        std::move(batch),
        base::BindOnce(&StoreProxy::OnCommit, weak_ptr_factory_.GetWeakPtr()));
    owner_->NotifyPrintersUpdated();
  }

 private:
  // Callback for DataTypeStore initialization.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<DataTypeStore> store) {
    if (error) {
      owner_->change_processor()->ReportError(*error);
      return;
    }

    store_ = std::move(store);
    store_->ReadAllData(base::BindOnce(&StoreProxy::OnReadAllData,
                                       weak_ptr_factory_.GetWeakPtr()));
  }

  void OnReadAllData(const std::optional<syncer::ModelError>& error,
                     std::unique_ptr<DataTypeStore::RecordList> record_list) {
    if (error) {
      owner_->change_processor()->ReportError(*error);
      return;
    }

    bool parse_error = false;
    {
      base::AutoLock lock(owner_->data_lock_);
      for (const DataTypeStore::Record& r : *record_list) {
        auto specifics = std::make_unique<sync_pb::PrinterSpecifics>();
        if (specifics->ParseFromString(r.value)) {
          auto& dest = owner_->all_data_[specifics->id()];
          dest = std::move(specifics);
        } else {
          parse_error = true;
        }
      }
    }
    owner_->NotifyPrintersUpdated();

    if (parse_error) {
      owner_->change_processor()->ReportError(
          {FROM_HERE, "Failed to deserialize all specifics."});
      return;
    }

    // Data loaded.  Load metadata.
    store_->ReadAllMetadata(base::BindOnce(&StoreProxy::OnReadAllMetadata,
                                           weak_ptr_factory_.GetWeakPtr()));
  }

  // Callback to handle commit errors.
  void OnCommit(const std::optional<syncer::ModelError>& error) {
    if (error) {
      LOG(WARNING) << "Failed to commit operation to store";
      owner_->change_processor()->ReportError(*error);
      return;
    }
  }

  void OnReadAllMetadata(
      const std::optional<syncer::ModelError>& error,
      std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
    TRACE_EVENT0(
        "ui",
        "ash::{anonympus}::PrintersSyncBridge::StoreProxy::OnReadAllMetadata");
    if (error) {
      owner_->change_processor()->ReportError(*error);
      return;
    }

    owner_->change_processor()->ModelReadyToSync(std::move(metadata_batch));
  }

  raw_ptr<PrintersSyncBridge> owner_;

  std::unique_ptr<DataTypeStore> store_;
  base::WeakPtrFactory<StoreProxy> weak_ptr_factory_{this};
};

PrintersSyncBridge::PrintersSyncBridge(
    syncer::OnceDataTypeStoreFactory callback,
    base::RepeatingClosure error_callback)
    : DataTypeSyncBridge(std::make_unique<ClientTagBasedDataTypeProcessor>(
          syncer::PRINTERS,
          std::move(error_callback))),
      store_delegate_(std::make_unique<StoreProxy>(this, std::move(callback))),
      observers_(new base::ObserverListThreadSafe<Observer>()) {}

PrintersSyncBridge::~PrintersSyncBridge() = default;

std::unique_ptr<MetadataChangeList>
PrintersSyncBridge::CreateMetadataChangeList() {
  return DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> PrintersSyncBridge::MergeFullSyncData(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK(change_processor()->IsTrackingMetadata());

  std::unique_ptr<DataTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  std::set<std::string> sync_entity_ids;
  {
    base::AutoLock lock(data_lock_);
    // Store the new data locally.
    for (const auto& change : entity_data) {
      const sync_pb::PrinterSpecifics& specifics =
          change->data().specifics.printer();

      DCHECK_EQ(change->storage_key(), specifics.id());
      sync_entity_ids.insert(specifics.id());

      // Write the update to local storage even if we already have it.
      StoreSpecifics(std::make_unique<sync_pb::PrinterSpecifics>(specifics),
                     batch.get());
    }

    // Inform the change processor of the new local entities and generate
    // appropriate metadata.
    for (const auto& entry : all_data_) {
      const std::string& local_entity_id = entry.first;

      // Migrate old schema to new combined one (crbug.com/737809).
      bool migrated = MigrateMakeAndModel(entry.second.get());

      // Clean up invalid ppd references (crbug.com/987869).
      bool resolved = ResolveInvalidPpdReference(entry.second.get());

      if (migrated || resolved ||
          !base::Contains(sync_entity_ids, local_entity_id)) {
        // Only local objects which were not updated are uploaded.  Objects for
        // which there was a remote copy are overwritten.
        change_processor()->Put(local_entity_id,
                                CopyToEntityData(*entry.second),
                                metadata_change_list.get());
      }
    }
  }

  NotifyPrintersUpdated();
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));
  store_delegate_->Commit(std::move(batch));
  return {};
}

std::optional<syncer::ModelError>
PrintersSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<MetadataChangeList> metadata_change_list,
    EntityChangeList entity_changes) {
  std::unique_ptr<DataTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  {
    base::AutoLock lock(data_lock_);
    // For all the entities from the server, apply changes.
    for (const std::unique_ptr<EntityChange>& change : entity_changes) {
      // We register the entity's storage key as our printer ids since they're
      // globally unique.
      const std::string& id = change->storage_key();
      if (change->type() == EntityChange::ACTION_DELETE) {
        // Server says delete, try to remove locally.
        DeleteSpecifics(id, batch.get());
      } else {
        // Server says update, overwrite whatever is local.  Conflict resolution
        // guarantees that this will be the newest version of the object.
        const sync_pb::PrinterSpecifics& specifics =
            change->data().specifics.printer();
        DCHECK_EQ(id, specifics.id());
        StoreSpecifics(std::make_unique<sync_pb::PrinterSpecifics>(specifics),
                       batch.get());
      }
    }
  }

  NotifyPrintersUpdated();
  // Update the local database with metadata for the incoming changes.
  batch->TakeMetadataChangesFrom(std::move(metadata_change_list));

  store_delegate_->Commit(std::move(batch));
  return {};
}

std::unique_ptr<syncer::DataBatch> PrintersSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  {
    base::AutoLock lock(data_lock_);
    for (const auto& key : storage_keys) {
      auto found = all_data_.find(key);
      if (found != all_data_.end()) {
        batch->Put(key, CopyToEntityData(*found->second));
      }
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
PrintersSyncBridge::GetAllDataForDebugging() {
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  {
    base::AutoLock lock(data_lock_);
    for (const auto& entry : all_data_) {
      batch->Put(entry.first, CopyToEntityData(*entry.second));
    }
  }
  return batch;
}

std::string PrintersSyncBridge::GetClientTag(const EntityData& entity_data) {
  // Printers were never synced prior to USS so this can match GetStorageKey.
  return GetStorageKey(entity_data);
}

std::string PrintersSyncBridge::GetStorageKey(const EntityData& entity_data) {
  DCHECK(entity_data.specifics.has_printer());
  return entity_data.specifics.printer().id();
}

// Picks the entity with the most recent updated time as the canonical version.
ConflictResolution PrintersSyncBridge::ResolveConflict(
    const std::string& storage_key,
    const EntityData& remote_data) const {
  DCHECK(remote_data.specifics.has_printer());

  auto iter = all_data_.find(storage_key);
  // If the local printer doesn't exist, it must have been deleted. In this
  // case, use the remote one.
  if (iter == all_data_.end()) {
    return ConflictResolution::kUseRemote;
  }
  const sync_pb::PrinterSpecifics& local_printer = *iter->second;

  const sync_pb::PrinterSpecifics& remote_printer =
      remote_data.specifics.printer();

  if (local_printer.updated_timestamp() > remote_printer.updated_timestamp()) {
    return ConflictResolution::kUseLocal;
  }

  return ConflictResolution::kUseRemote;
}

void PrintersSyncBridge::AddPrinter(
    std::unique_ptr<sync_pb::PrinterSpecifics> printer) {
  {
    base::AutoLock lock(data_lock_);
    AddPrinterLocked(std::move(printer));
  }
  NotifyPrintersUpdated();
}

bool PrintersSyncBridge::UpdatePrinter(
    std::unique_ptr<sync_pb::PrinterSpecifics> printer) {
  bool res;
  {
    base::AutoLock lock(data_lock_);
    res = UpdatePrinterLocked(std::move(printer));
  }
  NotifyPrintersUpdated();
  return res;
}

bool PrintersSyncBridge::UpdatePrinterLocked(
    std::unique_ptr<sync_pb::PrinterSpecifics> printer) {
  data_lock_.AssertAcquired();
  DCHECK(printer->has_id());
  auto iter = all_data_.find(printer->id());
  if (iter == all_data_.end()) {
    AddPrinterLocked(std::move(printer));
    return true;
  }

  // Modify the printer in-place then notify the change processor.
  sync_pb::PrinterSpecifics* merged = iter->second.get();
  MergePrinterToSpecifics(*SpecificsToPrinter(*printer), merged);
  merged->set_updated_timestamp(
      base::Time::Now().InMillisecondsSinceUnixEpoch());
  CommitPrinterPut(*merged);

  return false;
}

bool PrintersSyncBridge::RemovePrinter(const std::string& id) {
  DCHECK(store_delegate_->Ready());

  std::unique_ptr<DataTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  {
    base::AutoLock lock(data_lock_);
    if (!DeleteSpecifics(id, batch.get())) {
      LOG(WARNING) << "Could not find printer" << id;
      return false;
    }
  }

  if (change_processor()->IsTrackingMetadata()) {
    change_processor()->Delete(id, syncer::DeletionOrigin::Unspecified(),
                               batch->GetMetadataChangeList());
  }
  store_delegate_->Commit(std::move(batch));

  return true;
}

std::vector<sync_pb::PrinterSpecifics> PrintersSyncBridge::GetAllPrinters()
    const {
  base::AutoLock lock(data_lock_);
  std::vector<sync_pb::PrinterSpecifics> printers;
  for (auto& entry : all_data_) {
    printers.push_back(*entry.second);
  }

  return printers;
}

std::optional<sync_pb::PrinterSpecifics> PrintersSyncBridge::GetPrinter(
    const std::string& id) const {
  base::AutoLock lock(data_lock_);
  auto iter = all_data_.find(id);
  if (iter == all_data_.end()) {
    return {};
  }

  return {*iter->second};
}

bool PrintersSyncBridge::HasPrinter(const std::string& id) const {
  base::AutoLock lock(data_lock_);
  return all_data_.find(id) != all_data_.end();
}

void PrintersSyncBridge::CommitPrinterPut(
    const sync_pb::PrinterSpecifics& printer) {
  std::unique_ptr<DataTypeStore::WriteBatch> batch =
      store_delegate_->CreateWriteBatch();
  if (change_processor()->IsTrackingMetadata()) {
    change_processor()->Put(printer.id(), CopyToEntityData(printer),
                            batch->GetMetadataChangeList());
  }
  batch->WriteData(printer.id(), printer.SerializeAsString());

  store_delegate_->Commit(std::move(batch));
}

void PrintersSyncBridge::AddPrinterLocked(
    std::unique_ptr<sync_pb::PrinterSpecifics> printer) {
  // TODO(skau): Benchmark this code.  Make sure it doesn't hold onto the lock
  // for too long.
  data_lock_.AssertAcquired();
  printer->set_updated_timestamp(
      base::Time::Now().InMillisecondsSinceUnixEpoch());

  CommitPrinterPut(*printer);
  auto& dest = all_data_[printer->id()];
  dest = std::move(printer);
}

void PrintersSyncBridge::StoreSpecifics(
    std::unique_ptr<sync_pb::PrinterSpecifics> specifics,
    DataTypeStore::WriteBatch* batch) {
  data_lock_.AssertAcquired();
  const std::string id = specifics->id();
  batch->WriteData(id, specifics->SerializeAsString());
  all_data_[id] = std::move(specifics);
}

bool PrintersSyncBridge::DeleteSpecifics(const std::string& id,
                                         DataTypeStore::WriteBatch* batch) {
  data_lock_.AssertAcquired();
  auto iter = all_data_.find(id);
  if (iter != all_data_.end()) {
    batch->DeleteData(id);
    all_data_.erase(iter);
    return true;
  }

  return false;
}

void PrintersSyncBridge::AddObserver(Observer* obs) {
  observers_->AddObserver(obs);
}

void PrintersSyncBridge::RemoveObserver(Observer* obs) {
  observers_->RemoveObserver(obs);
}

void PrintersSyncBridge::NotifyPrintersUpdated() {
  observers_->Notify(FROM_HERE,
                     &PrintersSyncBridge::Observer::OnPrintersUpdated);
}

}  // namespace ash
