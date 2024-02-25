// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/storage/setting_sync_data.h"

#include <memory>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "components/sync/model/sync_data.h"
#include "components/sync/protocol/app_setting_specifics.pb.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/extension_setting_specifics.pb.h"
#include "extensions/common/extension_id.h"

namespace extensions {

SettingSyncData::SettingSyncData(const syncer::SyncChange& sync_change)
    : change_type_(sync_change.change_type()) {
  ExtractSyncData(sync_change.sync_data());
}

SettingSyncData::SettingSyncData(const syncer::SyncData& sync_data)
    : change_type_(std::nullopt) {
  ExtractSyncData(sync_data);
}

SettingSyncData::SettingSyncData(syncer::SyncChange::SyncChangeType change_type,
                                 const ExtensionId& extension_id,
                                 const std::string& key,
                                 base::Value value)
    : change_type_(change_type),
      extension_id_(extension_id),
      key_(key),
      value_(std::move(value)) {}

SettingSyncData::~SettingSyncData() {}

base::Value SettingSyncData::ExtractValue() {
  DCHECK(value_) << "value has already been Extract()ed";
  base::Value ret_value = std::move(*value_);
  value_.reset();
  return ret_value;
}

void SettingSyncData::ExtractSyncData(const syncer::SyncData& sync_data) {
  sync_pb::EntitySpecifics specifics = sync_data.GetSpecifics();
  // The specifics are exclusively either extension or app settings.
  DCHECK_NE(specifics.has_extension_setting(), specifics.has_app_setting());
  const sync_pb::ExtensionSettingSpecifics& extension_specifics =
      specifics.has_extension_setting()
          ? specifics.extension_setting()
          : specifics.app_setting().extension_setting();

  extension_id_ = extension_specifics.extension_id();
  key_ = extension_specifics.key();
  value_ = base::JSONReader::Read(extension_specifics.value());

  if (!value_) {
    LOG(WARNING) << "Specifics for " << extension_id_ << "/" << key_
                 << " had bad JSON for value: " << extension_specifics.value();
    value_ = base::Value(base::Value::Dict());
  }
}

}  // namespace extensions
