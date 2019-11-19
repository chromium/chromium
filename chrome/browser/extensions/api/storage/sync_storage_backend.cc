// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/sync_storage_backend.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/api/storage/settings_sync_processor.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "chrome/browser/extensions/api/storage/syncable_settings_storage.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_error_factory.h"
#include "extensions/browser/api/storage/backend_task_runner.h"

namespace extensions {

namespace {

void AddAllSyncData(const std::string& extension_id,
                    const base::DictionaryValue& src,
                    syncer::ModelType type,
                    syncer::SyncDataList* dst) {
  for (base::DictionaryValue::Iterator it(src); !it.IsAtEnd(); it.Advance()) {
    dst->push_back(settings_sync_util::CreateData(
        extension_id, it.key(), it.value(), type));
  }
}

std::unique_ptr<base::DictionaryValue> EmptyDictionaryValue() {
  return std::make_unique<base::DictionaryValue>();
}

ValueStoreFactory::ModelType ToFactoryModelType(syncer::ModelType sync_type) {
  switch (sync_type) {
    case syncer::APP_SETTINGS:
      return ValueStoreFactory::ModelType::APP;
    case syncer::EXTENSION_SETTINGS:
      return ValueStoreFactory::ModelType::EXTENSION;
    default:
      NOTREACHED();
  }
  return ValueStoreFactory::ModelType::EXTENSION;
}

}  // namespace

SyncStorageBackend::SyncStorageBackend(
    scoped_refptr<ValueStoreFactory> storage_factory,
    const SettingsStorageQuotaEnforcer::Limits& quota,
    scoped_refptr<SettingsObserverList> observers,
    syncer::ModelType sync_type,
    const syncer::SyncableService::StartSyncFlare& flare)
    : storage_factory_(std::move(storage_factory)),
      quota_(quota),
      observers_(std::move(observers)),
      sync_type_(sync_type),
      flare_(flare) {
  DCHECK(IsOnBackendSequence());
  DCHECK(sync_type_ == syncer::EXTENSION_SETTINGS ||
         sync_type_ == syncer::APP_SETTINGS);
}

SyncStorageBackend::~SyncStorageBackend() {}

ValueStore* SyncStorageBackend::GetStorage(const std::string& extension_id) {
  DCHECK(IsOnBackendSequence());
  return GetOrCreateStorageWithSyncData(extension_id, EmptyDictionaryValue());
}

SyncableSettingsStorage* SyncStorageBackend::GetOrCreateStorageWithSyncData(
    const std::string& extension_id,
    std::unique_ptr<base::DictionaryValue> sync_data) const {
  DCHECK(IsOnBackendSequence());

  auto maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage != storage_objs_.end()) {
    return maybe_storage->second.get();
  }

  std::unique_ptr<SettingsStorageQuotaEnforcer> settings_storage(
      new SettingsStorageQuotaEnforcer(
          quota_, storage_factory_->CreateSettingsStore(
                      settings_namespace::SYNC, ToFactoryModelType(sync_type_),
                      extension_id)));

  // It's fine to create the quota enforcer underneath the sync layer, since
  // sync will only go ahead if each underlying storage operation succeeds.
  std::unique_ptr<SyncableSettingsStorage> syncable_storage(
      new SyncableSettingsStorage(observers_, extension_id,
                                  settings_storage.release(), sync_type_,
                                  flare_));
  SyncableSettingsStorage* raw_syncable_storage = syncable_storage.get();
  storage_objs_[extension_id] = std::move(syncable_storage);

  if (sync_processor_.get()) {
    syncer::SyncError error = raw_syncable_storage->StartSyncing(
        std::move(sync_data), CreateSettingsSyncProcessor(extension_id));
    if (error.IsSet())
      raw_syncable_storage->StopSyncing();
  }
  return raw_syncable_storage;
}

void SyncStorageBackend::DeleteStorage(const std::string& extension_id) {
  DCHECK(IsOnBackendSequence());

  // Clear settings when the extension is uninstalled.  Leveldb implementations
  // will also delete the database from disk when the object is destroyed as a
  // result of being removed from |storage_objs_|.
  //
  // TODO(kalman): always GetStorage here (rather than only clearing if it
  // exists) since the storage area may have been unloaded, but we still want
  // to clear the data from disk.
  // However, this triggers http://crbug.com/111072.
  auto maybe_storage = storage_objs_.find(extension_id);
  if (maybe_storage == storage_objs_.end())
    return;
  maybe_storage->second->Clear();
  storage_objs_.erase(extension_id);
}

std::set<std::string> SyncStorageBackend::GetKnownExtensionIDs(
    ValueStoreFactory::ModelType model_type) const {
  DCHECK(IsOnBackendSequence());
  std::set<std::string> result;

  // Storage areas can be in-memory as well as on disk. |storage_objs_| will
  // contain all that are in-memory.
  for (const auto& storage_obj : storage_objs_) {
    result.insert(storage_obj.first);
  }

  std::set<std::string> disk_ids = storage_factory_->GetKnownExtensionIDs(
      settings_namespace::SYNC, model_type);
  result.insert(disk_ids.begin(), disk_ids.end());

  return result;
}

void SyncStorageBackend::WaitUntilReadyToSync(base::OnceClosure done) {
  DCHECK(IsOnBackendSequence());
  // This class is ready to sync immediately upon construction.
  std::move(done).Run();
}

syncer::SyncDataList SyncStorageBackend::GetAllSyncData(syncer::ModelType type)
    const {
  DCHECK(IsOnBackendSequence());
  // For all extensions, get all their settings.  This has the effect
  // of bringing in the entire state of extension settings in memory; sad.
  syncer::SyncDataList all_sync_data;
  std::set<std::string> known_extension_ids(
      GetKnownExtensionIDs(ToFactoryModelType(type)));

  for (auto it = known_extension_ids.cbegin(); it != known_extension_ids.cend();
       ++it) {
    ValueStore::ReadResult maybe_settings =
        GetOrCreateStorageWithSyncData(*it, EmptyDictionaryValue())->Get();
    if (!maybe_settings.status().ok()) {
      LOG(WARNING) << "Failed to get settings for " << *it << ": "
                   << maybe_settings.status().message;
      continue;
    }
    AddAllSyncData(*it, maybe_settings.settings(), type, &all_sync_data);
  }

  return all_sync_data;
}

syncer::SyncMergeResult SyncStorageBackend::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK(IsOnBackendSequence());
  DCHECK_EQ(sync_type_, type);
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(sync_error_factory.get());

  sync_processor_ = std::move(sync_processor);
  sync_error_factory_ = std::move(sync_error_factory);

  // Group the initial sync data by extension id.
  // The raw pointers are safe because ownership of each item is passed to
  // storage->StartSyncing or GetOrCreateStorageWithSyncData.
  std::map<std::string, base::DictionaryValue*> grouped_sync_data;

  for (const syncer::SyncData& sync_data : initial_sync_data) {
    SettingSyncData data(sync_data);
    // Yes this really is a reference to a pointer.
    base::DictionaryValue*& settings = grouped_sync_data[data.extension_id()];
    if (!settings)
      settings = new base::DictionaryValue();
    DCHECK(!settings->HasKey(data.key())) << "Duplicate settings for "
                                          << data.extension_id() << "/"
                                          << data.key();
    settings->SetWithoutPathExpansion(data.key(), data.PassValue());
  }

  // Start syncing all existing storage areas.  Any storage areas created in
  // the future will start being synced as part of the creation process.
  for (const auto& storage_obj : storage_objs_) {
    const std::string& extension_id = storage_obj.first;
    SyncableSettingsStorage* storage = storage_obj.second.get();

    auto group = grouped_sync_data.find(extension_id);
    syncer::SyncError error;
    if (group != grouped_sync_data.end()) {
      error = storage->StartSyncing(base::WrapUnique(group->second),
                                    CreateSettingsSyncProcessor(extension_id));
      grouped_sync_data.erase(group);
    } else {
      error = storage->StartSyncing(EmptyDictionaryValue(),
                                    CreateSettingsSyncProcessor(extension_id));
    }

    if (error.IsSet())
      storage->StopSyncing();
  }

  // Eagerly create and init the rest of the storage areas that have sync data.
  // Under normal circumstances (i.e. not first-time sync) this will be all of
  // them.
  for (const auto& group : grouped_sync_data) {
    GetOrCreateStorageWithSyncData(group.first, base::WrapUnique(group.second));
  }

  return syncer::SyncMergeResult(type);
}

syncer::SyncError SyncStorageBackend::ProcessSyncChanges(
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
        GetOrCreateStorageWithSyncData(group.first, EmptyDictionaryValue());
    syncer::SyncError error =
        storage->ProcessSyncChanges(base::WrapUnique(group.second));
    if (error.IsSet())
      storage->StopSyncing();
  }

  return syncer::SyncError();
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
  sync_error_factory_.reset();
}

std::unique_ptr<SettingsSyncProcessor>
SyncStorageBackend::CreateSettingsSyncProcessor(
    const std::string& extension_id) const {
  CHECK(sync_processor_.get());
  return std::unique_ptr<SettingsSyncProcessor>(new SettingsSyncProcessor(
      extension_id, sync_type_, sync_processor_.get()));
}

}  // namespace extensions
