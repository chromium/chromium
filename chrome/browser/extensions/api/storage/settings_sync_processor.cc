// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/settings_sync_processor.h"

#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/storage/settings_sync_util.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "extensions/browser/api/storage/backend_task_runner.h"
#include "extensions/browser/api/storage/settings_namespace.h"
#include "extensions/common/extension_id.h"

namespace extensions {

SettingsSyncProcessor::SettingsSyncProcessor(
    const ExtensionId& extension_id,
    syncer::DataType type,
    syncer::SyncChangeProcessor* sync_processor)
    : extension_id_(extension_id),
      type_(type),
      sync_processor_(sync_processor),
      initialized_(false) {
  DCHECK(IsOnBackendSequence());
  CHECK(type == syncer::EXTENSION_SETTINGS || type == syncer::APP_SETTINGS);
  CHECK(sync_processor);
}

SettingsSyncProcessor::~SettingsSyncProcessor() {
  DCHECK(IsOnBackendSequence());
}

void SettingsSyncProcessor::Init(const base::Value::Dict& initial_state) {
  DCHECK(IsOnBackendSequence());
  CHECK(!initialized_) << "Init called multiple times";

  for (auto iter : initial_state) {
    synced_keys_.insert(iter.first);
  }

  initialized_ = true;
}

std::optional<syncer::ModelError> SettingsSyncProcessor::SendChanges(
    const value_store::ValueStoreChangeList& changes) {
  DCHECK(IsOnBackendSequence());
  CHECK(initialized_) << "Init not called";

  syncer::SyncChangeList sync_changes;
  std::set<std::string> added_keys;
  std::set<std::string> deleted_keys;

  for (const auto& i : changes) {
    if (i.new_value) {
      if (synced_keys_.count(i.key)) {
        // New value, key is synced; send ACTION_UPDATE.
        sync_changes.push_back(settings_sync_util::CreateUpdate(
            extension_id_, i.key, *i.new_value, type_));
      } else {
        // New value, key is not synced; send ACTION_ADD.
        sync_changes.push_back(settings_sync_util::CreateAdd(
            extension_id_, i.key, *i.new_value, type_));
        added_keys.insert(i.key);
      }
    } else {
      if (synced_keys_.count(i.key)) {
        // Clearing value, key is synced; send ACTION_DELETE.
        sync_changes.push_back(
            settings_sync_util::CreateDelete(extension_id_, i.key, type_));
        deleted_keys.insert(i.key);
      } else {
        LOG(WARNING) << "Deleted " << i.key << " but not in synced_keys_";
      }
    }
  }

  if (sync_changes.empty())
    return std::nullopt;

  std::optional<syncer::ModelError> error =
      sync_processor_->ProcessSyncChanges(FROM_HERE, sync_changes);
  if (error.has_value())
    return error;

  synced_keys_.insert(added_keys.begin(), added_keys.end());
  for (const auto& deleted_key : deleted_keys) {
    synced_keys_.erase(deleted_key);
  }

  return std::nullopt;
}

void SettingsSyncProcessor::NotifyChanges(
    const value_store::ValueStoreChangeList& changes) {
  DCHECK(IsOnBackendSequence());
  CHECK(initialized_) << "Init not called";

  for (const auto& i : changes) {
    if (i.new_value)
      synced_keys_.insert(i.key);
    else
      synced_keys_.erase(i.key);
  }
}

}  // namespace extensions
