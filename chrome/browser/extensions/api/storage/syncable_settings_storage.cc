// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/syncable_settings_storage.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/extensions/api/storage/settings_sync_processor.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_change.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/storage_area_namespace.h"
#include "extensions/common/extension_id.h"

using value_store::ValueStore;

namespace extensions {

SyncableSettingsStorage::SyncableSettingsStorage(
    SequenceBoundSettingsChangedCallback observer,
    const ExtensionId& extension_id,
    ValueStore* delegate,
    syncer::DataType sync_type,
    const syncer::SyncableService::StartSyncFlare& flare)
    : observer_(std::move(observer)),
      extension_id_(extension_id),
      delegate_(delegate),
      sync_type_(sync_type),
      flare_(flare) {
  DCHECK(IsOnBackendSequence());
}

SyncableSettingsStorage::~SyncableSettingsStorage() {
  DCHECK(IsOnBackendSequence());
}

size_t SyncableSettingsStorage::GetBytesInUse(const std::string& key) {
  DCHECK(IsOnBackendSequence());
  return delegate_->GetBytesInUse(key);
}

size_t SyncableSettingsStorage::GetBytesInUse(
    const std::vector<std::string>& keys) {
  DCHECK(IsOnBackendSequence());
  return delegate_->GetBytesInUse(keys);
}

size_t SyncableSettingsStorage::GetBytesInUse() {
  DCHECK(IsOnBackendSequence());
  return delegate_->GetBytesInUse();
}

template <class T>
T SyncableSettingsStorage::HandleResult(T result) {
  if (result.status().restore_status != RESTORE_NONE) {
    // If we're syncing, stop - we don't want to push the deletion of any data.
    // At next startup, when we start up the sync service, we'll get back any
    // data which was stored intact on Sync.
    // TODO(devlin): Investigate if there's a way we can trigger
    // MergeDataAndStartSyncing() to immediately get back any data we can, and
    // continue syncing.
    StopSyncing();
  }
  return result;
}

ValueStore::ReadResult SyncableSettingsStorage::Get(
    const std::string& key) {
  DCHECK(IsOnBackendSequence());
  return HandleResult(delegate_->Get(key));
}

ValueStore::ReadResult SyncableSettingsStorage::Get(
    const std::vector<std::string>& keys) {
  DCHECK(IsOnBackendSequence());
  return HandleResult(delegate_->Get(keys));
}

ValueStore::ReadResult SyncableSettingsStorage::Get() {
  DCHECK(IsOnBackendSequence());
  return HandleResult(delegate_->Get());
}

ValueStore::WriteResult SyncableSettingsStorage::Set(
    WriteOptions options, const std::string& key, const base::Value& value) {
  DCHECK(IsOnBackendSequence());
  WriteResult result = HandleResult(delegate_->Set(options, key, value));
  if (!result.status().ok())
    return result;
  SyncResultIfEnabled(result);
  return result;
}

ValueStore::WriteResult SyncableSettingsStorage::Set(
    WriteOptions options,
    const base::Value::Dict& values) {
  DCHECK(IsOnBackendSequence());
  WriteResult result = HandleResult(delegate_->Set(options, values));
  if (!result.status().ok())
    return result;
  SyncResultIfEnabled(result);
  return result;
}

ValueStore::WriteResult SyncableSettingsStorage::Remove(
    const std::string& key) {
  DCHECK(IsOnBackendSequence());
  WriteResult result = HandleResult(delegate_->Remove(key));
  if (!result.status().ok())
    return result;
  SyncResultIfEnabled(result);
  return result;
}

ValueStore::WriteResult SyncableSettingsStorage::Remove(
    const std::vector<std::string>& keys) {
  DCHECK(IsOnBackendSequence());
  WriteResult result = HandleResult(delegate_->Remove(keys));
  if (!result.status().ok())
    return result;
  SyncResultIfEnabled(result);
  return result;
}

ValueStore::WriteResult SyncableSettingsStorage::Clear() {
  DCHECK(IsOnBackendSequence());
  WriteResult result = HandleResult(delegate_->Clear());
  if (!result.status().ok())
    return result;
  SyncResultIfEnabled(result);
  return result;
}

void SyncableSettingsStorage::SyncResultIfEnabled(
    const ValueStore::WriteResult& result) {
  if (result.changes().empty())
    return;

  if (sync_processor_.get()) {
    std::optional<syncer::ModelError> error =
        sync_processor_->SendChanges(result.changes());
    if (error.has_value())
      StopSyncing();
  } else {
    // Tell sync to try and start soon, because syncable changes to sync_type_
    // have started happening. This will cause sync to call us back
    // asynchronously via StartSyncing(...) as soon as possible.
    flare_.Run(sync_type_);
  }
}

// Sync-related methods.

std::optional<syncer::ModelError> SyncableSettingsStorage::StartSyncing(
    base::Value::Dict sync_state,
    std::unique_ptr<SettingsSyncProcessor> sync_processor) {
  DCHECK(IsOnBackendSequence());
  DCHECK(!sync_processor_.get());

  sync_processor_ = std::move(sync_processor);
  sync_processor_->Init(sync_state);

  ReadResult maybe_settings = delegate_->Get();
  if (!maybe_settings.status().ok()) {
    return syncer::ModelError(
        FROM_HERE, base::StringPrintf("Failed to get settings: %s",
                                      maybe_settings.status().message.c_str()));
  }

  base::Value::Dict current_settings = maybe_settings.PassSettings();
  return sync_state.empty()
             ? SendLocalSettingsToSync(std::move(current_settings))
             : OverwriteLocalSettingsWithSync(std::move(sync_state),
                                              std::move(current_settings));
}

std::optional<syncer::ModelError>
SyncableSettingsStorage::SendLocalSettingsToSync(
    base::Value::Dict local_state) {
  DCHECK(IsOnBackendSequence());

  if (local_state.empty())
    return std::nullopt;

  // Transform the current settings into a list of sync changes.
  value_store::ValueStoreChangeList changes;
  for (auto pair : local_state) {
    changes.push_back(value_store::ValueStoreChange(pair.first, std::nullopt,
                                                    std::move(pair.second)));
  }

  std::optional<syncer::ModelError> error =
      sync_processor_->SendChanges(std::move(changes));
  if (error.has_value())
    StopSyncing();
  return error;
}

std::optional<syncer::ModelError>
SyncableSettingsStorage::OverwriteLocalSettingsWithSync(
    base::Value::Dict sync_state,
    base::Value::Dict local_state) {
  DCHECK(IsOnBackendSequence());
  // This is implemented by building up a list of sync changes then sending
  // those to ProcessSyncChanges. This generates events like onStorageChanged.
  auto changes = std::make_unique<SettingSyncDataList>();

  for (auto it : local_state) {
    std::optional<base::Value> sync_value = sync_state.Extract(it.first);
    if (sync_value.has_value()) {
      // If the sync value is different, update local setting with new value.
      if (*sync_value != it.second) {
        changes->push_back(std::make_unique<SettingSyncData>(
            syncer::SyncChange::ACTION_UPDATE, extension_id_, it.first,
            std::move(*sync_value)));
      }
    } else {
      // Not synced, delete local setting.
      changes->push_back(std::make_unique<SettingSyncData>(
          syncer::SyncChange::ACTION_DELETE, extension_id_, it.first,
          base::Value(base::Value::Dict())));
    }
  }

  // Add all new settings to local settings.
  for (auto pair : sync_state) {
    changes->push_back(std::make_unique<SettingSyncData>(
        syncer::SyncChange::ACTION_ADD, extension_id_, pair.first,
        std::move(pair.second)));
  }

  if (changes->empty())
    return std::nullopt;
  return ProcessSyncChanges(std::move(changes));
}

void SyncableSettingsStorage::StopSyncing() {
  DCHECK(IsOnBackendSequence());
  sync_processor_.reset();
}

std::optional<syncer::ModelError> SyncableSettingsStorage::ProcessSyncChanges(
    std::unique_ptr<SettingSyncDataList> sync_changes) {
  DCHECK(IsOnBackendSequence());
  DCHECK(!sync_changes->empty()) << "No sync changes for " << extension_id_;

  if (!sync_processor_.get()) {
    return syncer::ModelError(
        FROM_HERE, std::string("Sync is inactive for ") + extension_id_);
  }

  std::vector<syncer::ModelError> errors;
  value_store::ValueStoreChangeList changes;

  for (const std::unique_ptr<SettingSyncData>& sync_change : *sync_changes) {
    DCHECK_EQ(extension_id_, sync_change->extension_id());
    const std::string& key = sync_change->key();
    base::Value change_value = sync_change->ExtractValue();

    std::optional<base::Value> current_value;
    {
      ReadResult maybe_settings = Get(key);
      if (!maybe_settings.status().ok()) {
        errors.emplace_back(
            FROM_HERE,
            base::StringPrintf("Error getting current sync state for %s/%s: %s",
                               extension_id_.c_str(), key.c_str(),
                               maybe_settings.status().message.c_str()));
        continue;
      }
      current_value = maybe_settings.settings().Extract(key);
    }

    std::optional<syncer::ModelError> error;

    DCHECK(sync_change->change_type().has_value());

    switch (*sync_change->change_type()) {
      case syncer::SyncChange::ACTION_ADD:
        if (!current_value) {
          error = OnSyncAdd(key, std::move(change_value), &changes);
        } else {
          // Already a value; hopefully a local change has beaten sync in a
          // race and change's not a bug, so pretend change's an update.
          LOG(WARNING) << "Got add from sync for existing setting " <<
              extension_id_ << "/" << key;
          error = OnSyncUpdate(key, std::move(*current_value),
                               std::move(change_value), &changes);
        }
        break;

      case syncer::SyncChange::ACTION_UPDATE:
        if (current_value.has_value()) {
          error = OnSyncUpdate(key, std::move(*current_value),
                               std::move(change_value), &changes);
        } else {
          // Similarly, pretend change's an add.
          LOG(WARNING) << "Got update from sync for nonexistent setting" <<
              extension_id_ << "/" << key;
          error = OnSyncAdd(key, std::move(change_value), &changes);
        }
        break;

      case syncer::SyncChange::ACTION_DELETE:
        if (current_value.has_value()) {
          error = OnSyncDelete(key, std::move(*current_value), &changes);
        } else {
          // Similarly, ignore change.
          LOG(WARNING) << "Got delete from sync for nonexistent setting " <<
              extension_id_ << "/" << key;
        }
        break;
    }

    if (error) {
      errors.push_back(*error);
    }
  }

  sync_processor_->NotifyChanges(changes);

  observer_->Run(extension_id_, StorageAreaNamespace::kSync,
                 /*session_access_level=*/std::nullopt,
                 value_store::ValueStoreChange::ToValue(std::move(changes)));

  // TODO(kalman): Something sensible with multiple errors.
  if (errors.empty())
    return std::nullopt;
  return errors[0];
}

std::optional<syncer::ModelError> SyncableSettingsStorage::OnSyncAdd(
    const std::string& key,
    base::Value new_value,
    value_store::ValueStoreChangeList* changes) {
  WriteResult result =
      HandleResult(delegate_->Set(IGNORE_QUOTA, key, new_value));
  if (!result.status().ok()) {
    return syncer::ModelError(
        FROM_HERE,
        base::StringPrintf("Error pushing sync add to local settings: %s",
                           result.status().message.c_str()));
  }
  changes->push_back(
      value_store::ValueStoreChange(key, std::nullopt, std::move(new_value)));
  return std::nullopt;
}

std::optional<syncer::ModelError> SyncableSettingsStorage::OnSyncUpdate(
    const std::string& key,
    base::Value old_value,
    base::Value new_value,
    value_store::ValueStoreChangeList* changes) {
  WriteResult result =
      HandleResult(delegate_->Set(IGNORE_QUOTA, key, new_value));
  if (!result.status().ok()) {
    return syncer::ModelError(
        FROM_HERE,
        base::StringPrintf("Error pushing sync update to local settings: %s",
                           result.status().message.c_str()));
  }
  changes->push_back(value_store::ValueStoreChange(key, std::move(old_value),
                                                   std::move(new_value)));
  return std::nullopt;
}

std::optional<syncer::ModelError> SyncableSettingsStorage::OnSyncDelete(
    const std::string& key,
    base::Value old_value,
    value_store::ValueStoreChangeList* changes) {
  WriteResult result = HandleResult(delegate_->Remove(key));
  if (!result.status().ok()) {
    return syncer::ModelError(
        FROM_HERE,
        base::StringPrintf("Error pushing sync remove to local settings: %s",
                           result.status().message.c_str()));
  }
  changes->push_back(
      value_store::ValueStoreChange(key, std::move(old_value), std::nullopt));
  return std::nullopt;
}

}  // namespace extensions
