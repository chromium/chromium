// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTING_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTING_SYNC_DATA_H_

#include <memory>
#include <optional>
#include <string>

#include "base/values.h"
#include "components/sync/model/sync_change.h"
#include "extensions/common/extension_id.h"

namespace syncer {
class SyncData;
}

namespace extensions {

// Container for data interpreted from sync data/changes for an extension or
// app setting. Safe and efficient to copy.
class SettingSyncData {
 public:
  // Creates from a sync change.
  explicit SettingSyncData(const syncer::SyncChange& sync_change);

  // Creates from sync data. |change_type| will be std::nullopt.
  explicit SettingSyncData(const syncer::SyncData& sync_data);

  // Creates explicitly.
  SettingSyncData(syncer::SyncChange::SyncChangeType change_type,
                  const ExtensionId& extension_id,
                  const std::string& key,
                  base::Value value);

  SettingSyncData(const SettingSyncData&) = delete;
  SettingSyncData& operator=(const SettingSyncData&) = delete;

  ~SettingSyncData();

  // May return std::nullopt if this object represents sync data that isn't
  // associated with a sync operation.
  const std::optional<syncer::SyncChange::SyncChangeType>& change_type() const {
    return change_type_;
  }
  const ExtensionId& extension_id() const { return extension_id_; }
  const std::string& key() const { return key_; }
  // value() cannot be called if ExtractValue() has been called.
  const base::Value& value() const { return *value_; }

  // Releases ownership of the value to the caller. Neither value() nor
  // ExtractValue() can be after this.
  base::Value ExtractValue();

 private:
  // Populates the extension ID, key, and value from |sync_data|. This will be
  // either an extension or app settings data type.
  void ExtractSyncData(const syncer::SyncData& sync_data);

  std::optional<syncer::SyncChange::SyncChangeType> change_type_;
  ExtensionId extension_id_;
  std::string key_;
  std::optional<base::Value> value_;
};

using SettingSyncDataList = std::vector<std::unique_ptr<SettingSyncData>>;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTING_SYNC_DATA_H_
