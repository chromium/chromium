// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/sync_storage_backend.h"

#include <memory>
#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/api/storage/settings_sync_processor.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/api/storage/syncable_settings_storage.h"
#include "components/sync/model/sync_change_processor.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/value_store_util.h"

namespace extensions {

namespace {

void AddAllSyncData(const std::string& extension_id,
                    const base::Value::Dict& src,
                    syncer::ModelType type,
                    syncer::SyncDataList* dst) {
  for (auto it : src) {
    dst->push_back(settings_sync_util::CreateData(extension_id, it.first,
                                                  it.second, type));
  }
}

base::Value::Dict EmptyDict() {
  return base::Value::Dict();
}

value_store_util::ModelType ToFactoryModelType(syncer::ModelType sync_type) {
  switch (sync_type) {
    case syncer::APP_SETTINGS:
      return value_store_util::ModelType::APP;
    case syncer::EXTENSION_SETTINGS:
      return value_store_util::ModelType::EXTENSION;
    default:
      NOTREACHED();
  }
  return value_store_util::ModelType::EXTENSION;
}

}  // namespace

SyncStorageBackend::SyncStorageBackend(
    scoped_refptr<value_store::ValueStoreFactory> storage_factory,
    const SettingsStorageQuotaEnforcer::Limits& quota,
    SequenceBoundSettingsChangedCallback observer,
    syncer::ModelType sync_type,
    const syncer::SyncableService::StartSyncFlare& flare)
    : storage_factory_(std::move(storage_factory)),
      quota_(quota),
      observer_(std::move(observer)),
      sync_type_(sync_type),
      flare_(flare) {
  DCHECK(IsOnBackendSequence());
  DCHECK(sync_type_ == syncer::EXTENSION_SETTINGS ||
         sync_type_ == syncer::APP_SETTINGS);
}

SyncStorageBackend::~SyncStorageBackend() {}

value_store::ValueStore* SyncStorageBackend::GetStorage(
    const std::string& extension_id) {
  DCHECK(IsOnBackendSequence());
  return GetOrCreateStorageWithSyncData(extension_id, EmptyDict());
}

SyncableSettingsStorage* SyncStorageBackend::GetOrCreateStorageWithSyncData(
    const std::string& extension_id,
    base::Value::Dict sync_data) const {
  DCHECK(IsOnBackendSequence());

  auto maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage != storage_objs_.end()) {
    return maybe_storage->second.get();
  }

  std::unique_ptr<SettingsStorageQuotaEnforcer> settings_storage(
      new SettingsStorageQuotaEnforcer(
          quota_, value_store_util::CreateSettingsStore(
                      settings_namespace::SYNC, ToFactoryModelType(sync_type_),
                      extension_id, storage_factory_)));

  // It's fine to create the quota enforcer underneath the sync layer, since
  // sync will only go ahead if each underlying storage operation succeeds.
  std::unique_ptr<SyncableSettingsStorage> syncable_storage(
      new SyncableSettingsStorage(observer_, extension_id,
                                  settings_storage.release(), sync_type_,
                                  flare_));
  SyncableSettingsStorage* raw_syncable_storage = syncable_storage.get();
  storage_objs_[extension_id] = std::move(syncable_storage);

  if (sync_processor_.get()) {
    absl::optional<syncer::ModelError> error =
        raw_syncable_storage->StartSyncing(
            std::move(sync_data), CreateSettingsSyncProcessor(extension_id));
    if (error.has_value())
      raw_syncable_storage->StopSyncing();
  }
  return raw_syncable_storage;
}

void SyncStorageBackend::DeleteStorage(const std::string& extension_id) {
  DCHECK(IsOnBackendSequence());

  // Clear settings when the extension is uninstalled.  Leveldb implementations
  // will also delete the database from disk when the object is destroyed as a
  // result of being removed from |storage_objs_|. Note that we always
  // GetStorage here (rather than only clearing if it exists) since the storage
  // area may have been unloaded, but we still want to clear the data from disk.
  GetStorage(extension_id)->Clear();
  storage_objs_.erase(extension_id);
}

void SyncStorageBackend::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(IsOnBackendSequence());
  // This class is ready to sync immediately upon construction.
  std::move(done).Run();
}

syncer::SyncDataList SyncStorageBackend::GetAllSyncDataForTesting(
    syncer::ModelType type) const {
  DCHECK(IsOnBackendSequence());
  // For all extensions, get all their settings.  This has the effect
  // of bringing in the entire state of extension settings in memory; sad.
  syncer::SyncDataList all_sync_data;

  // For tests, all storage areas are kept in memory in `storage_objs_`.
  for (const auto& storage_obj : storage_objs_) {
    std::string extension_id = storage_obj.first;

    value_store::ValueStore::ReadResult maybe_settings =
        GetOrCreateStorageWithSyncData(extension_id, EmptyDict())->Get();
    if (!maybe_settings.status().ok()) {
      LOG(WARNING) << "Failed to get settings for " << extension_id << ": "
                   << maybe_settings.status().message;
      continue;
    }
    AddAllSyncData(extension_id, maybe_settings.settings(), type,
                   &all_sync_data);
  }

  return all_sync_data;
}

absl::optional<syncer::ModelError> SyncStorageBackend::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  DCHECK(IsOnBackendSequence());
  DCHECK_EQ(sync_type_, type);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());

  sync_processor_ = std::move(sync_processor);

  // Group the initial sync data by extension id.
  std::map<std::string, base::Value::Dict> grouped_sync_data;

  for (const syncer::SyncData& sync_data : initial_sync_data) {
    SettingSyncData data(sync_data);
    base::Value::Dict& settings = grouped_sync_data[data.extension_id()];
    DCHECK(!settings.Find(data.key()))
        << "Duplicate settings for " << data.extension_id() << "/"
        << data.key();
    settings.Set(data.key(), data.ExtractValue());
  }

  // Start syncing all existing storage areas.  Any storage areas created in
  // the future will start being synced as part of the creation process.
  for (const auto& storage_obj : storage_objs_) {
    const std::string& extension_id = storage_obj.first;
    SyncableSettingsStorage* storage = storage_obj.second.get();

    auto group = grouped_sync_data.find(extension_id);
    absl::optional<syncer::ModelError> error;
    if (group != grouped_sync_data.end()) {
      error = storage->StartSyncing(std::move(group->second),
                                    CreateSettingsSyncProcessor(extension_id));
      grouped_sync_data.erase(group);
    } else {
      error = storage->StartSyncing(EmptyDict(),
                                    CreateSettingsSyncProcessor(extension_id));
    }

    if (error.has_value())
      storage->StopSyncing();
  }

  // Eagerly create and init the rest of the storage areas that have sync data.
  // Under normal circumstances (i.e. not first-time sync) this will be all of
  // them.
  for (auto& group : grouped_sync_data) {
    GetOrCreateStorageWithSyncData(group.first, std::move(group.second));
  }

  return absl::nullopt;
}

absl::optional<syncer::ModelError> SyncStorageBackend::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& sync_changes) {
  DCHECK(IsOnBackendSequence());
  DCHECK(sync_processor_.get());

  // Group changes by extension, to pass all changes in a single method call.
  // The raw pointers are safe because ownership of each item is passed to
  // storage->ProcessSyncChanges.
  std::map<std::string, SettingSyncDataList*> grouped_sync_data;

  for (const syncer::SyncChange& change : sync_changes) {
    std::unique_ptr<SettingSyncData> data(new SettingSyncData(change));
    SettingSyncDataList*& group = grouped_sync_data[data->extension_id()];
    if (!group)
      group = new SettingSyncDataList();
    group->push_back(std::move(data));
  }

  // Create any storage areas that don't exist yet but have sync data.
  for (const auto& group : grouped_sync_data) {
    SyncableSettingsStorage* storage =
        GetOrCreateStorageWithSyncData(group.first, EmptyDict());
    absl::optional<syncer::ModelError> error =
        storage->ProcessSyncChanges(base::WrapUnique(group.second));
    if (error.has_value())
      storage->StopSyncing();
  }

  return absl::nullopt;
}

void SyncStorageBackend::StopSyncing(syncer::ModelType type) {
  DCHECK(IsOnBackendSequence());
  DCHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
  DCHECK_EQ(sync_type_, type);

  for (const auto& storage_obj : storage_objs_) {
    // Some storage areas may have already stopped syncing if they had areas
    // and syncing was disabled, but StopSyncing is safe to call multiple times.
    storage_obj.second->StopSyncing();
  }

  sync_processor_.reset();
}

std::unique_ptr<SettingsSyncProcessor>
SyncStorageBackend::CreateSettingsSyncProcessor(
    const std::string& extension_id) const {
  CHECK(sync_processor_.get());
  return std::make_unique<SettingsSyncProcessor>(extension_id, sync_type_,
                                                 sync_processor_.get());
}

}  // namespace extensions
