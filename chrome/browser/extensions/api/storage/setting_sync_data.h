// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTING_SYNC_DATA_H_
#define CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTING_SYNC_DATA_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/values.h"
#include "components/sync/model/sync_change.h"

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

  // Creates from sync data. |change_type| will be base::nullopt.
  explicit SettingSyncData(const syncer::SyncData& sync_data);

  // Creates explicitly.
  SettingSyncData(syncer::SyncChange::SyncChangeType change_type,
                  const std::string& extension_id,
                  const std::string& key,
                  std::unique_ptr<base::Value> value);

  ~SettingSyncData();

  // May return base::nullopt if this object represents sync data that isn't
  // associated with a sync operation.
  const base::Optional<syncer::SyncChange::SyncChangeType>& change_type()
      const {
    return change_type_;
  }
  const std::string& extension_id() const { return extension_id_; }
  const std::string& key() const { return key_; }
  // value() cannot be called if PassValue() has been called.
  const base::Value& value() const { return *value_; }

  // Releases ownership of the value to the caller. Neither value() nor
  // PassValue() can be after this.
  std::unique_ptr<base::Value> PassValue();

 private:
  // Populates the extension ID, key, and value from |sync_data|. This will be
  // either an extension or app settings data type.
  void ExtractSyncData(const syncer::SyncData& sync_data);

  base::Optional<syncer::SyncChange::SyncChangeType> change_type_;
  std::string extension_id_;
  std::string key_;
  std::unique_ptr<base::Value> value_;

  DISALLOW_COPY_AND_ASSIGN(SettingSyncData);
};

using SettingSyncDataList = std::vector<std::unique_ptr<SettingSyncData>>;

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_STORAGE_SETTING_SYNC_DATA_H_
