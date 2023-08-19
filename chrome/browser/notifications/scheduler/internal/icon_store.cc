// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/internal/icon_store.h"

#include <map>
#include <utility>

#include "base/containers/contains.h"
#include "base/uuid.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "chrome/browser/notifications/scheduler/internal/proto_conversion.h"
#include "chrome/browser/notifications/scheduler/internal/stats.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"

namespace leveldb_proto {

void DataToProto(notifications::IconEntry* icon_entry,
                 notifications::proto::Icon* proto) {
  IconEntryToProto(icon_entry, proto);
}

void ProtoToData(notifications::proto::Icon* proto,
                 notifications::IconEntry* icon_entry) {
  IconEntryFromProto(proto, icon_entry);
}

}  // namespace leveldb_proto

namespace notifications {
namespace {
bool HasKeyInDb(const std::vector<std::string>& key_dict,
                const std::string& key) {
  return base::Contains(key_dict, key);
}
}  // namespace

using KeyEntryPair = std::pair<std::string, IconEntry>;
using KeyEntryVector = std::vector<KeyEntryPair>;
using KeyVector = std::vector<std::string>;

IconProtoDbStore::IconProtoDbStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db,
    std::unique_ptr<IconConverter> icon_converter)
    : db_(std::move(db)), icon_converter_(std::move(icon_converter)) {}

IconProtoDbStore::~IconProtoDbStore() = default;

void IconProtoDbStore::InitAndLoadKeys(InitAndLoadKeysCallback callback) {
  db_->Init(base::BindOnce(&IconProtoDbStore::OnDbInitialized,
                           weak_ptr_factory_.GetWeakPtr(),
                           std::move(callback)));
}

void IconProtoDbStore::AddIcons(IconTypeBundleMap icons, AddCallback callback) {
  if (icons.empty()) {
    std::move(callback).Run(IconTypeUuidMap{}, true);
    return;
  }

  std::vector<std::string> icons_uuid;
  for (size_t i = 0; i < icons.size(); i++) {
    icons_uuid.emplace_back(base::Uuid::GenerateRandomV4().AsLowercaseString());
  }

  std::vector<IconType> icons_type;
  std::vector<SkBitmap> icons_bitmap;
  for (auto&& it = icons.begin(); it != icons.end(); ++it) {
    icons_type.emplace_back(it->first);
    icons_bitmap.emplace_back(std::move(it->second.bitmap));
  }

  icon_converter_->ConvertIconToString(
      std::move(icons_bitmap),
      base::BindOnce(&IconProtoDbStore::OnIconsEncoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(icons_type), std::move(icons_uuid)));
}

void IconProtoDbStore::LoadIcons(const std::vector<std::string>& keys,
                                 LoadIconsCallback callback) {
  if (keys.empty()) {
    std::move(callback).Run(true, LoadedIconsMap{});
    return;
  }
  db_->LoadKeysAndEntriesWithFilter(
      base::BindRepeating(&HasKeyInDb, keys),
      base::BindOnce(&IconProtoDbStore::OnIconEntriesLoaded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void IconProtoDbStore::DeleteIcons(const std::vector<std::string>& keys,
                                   UpdateCallback callback) {
  if (keys.empty()) {
    std::move(callback).Run(true);
    return;
  }
  auto keys_to_delete = std::make_unique<KeyVector>();
  for (size_t i = 0; i < keys.size(); i++)
    keys_to_delete->emplace_back(keys[i]);

  db_->UpdateEntries(std::make_unique<KeyEntryVector>() /*keys_to_save*/,
                     std::move(keys_to_delete), std::move(callback));
}

void IconProtoDbStore::OnDbInitialized(
    InitAndLoadKeysCallback callback,
    leveldb_proto::Enums::InitStatus status) {
  bool success = (status == leveldb_proto::Enums::InitStatus::kOK);
  if (!success) {
    std::move(callback).Run(success, nullptr /*LoadedIconKeys*/);
    return;
  }
  db_->LoadKeys(base::BindOnce(&IconProtoDbStore::OnIconKeysLoaded,
                               weak_ptr_factory_.GetWeakPtr(),
                               std::move(callback)));
}

void IconProtoDbStore::OnIconKeysLoaded(InitAndLoadKeysCallback callback,
                                        bool success,
                                        LoadedIconKeys loaded_keys) {
  if (!success) {
    std::move(callback).Run(success, nullptr /*LoadedIconKeys*/);
    return;
  }
  std::move(callback).Run(success, std::move(loaded_keys));
}

void IconProtoDbStore::OnIconEntriesLoaded(
    LoadIconsCallback callback,
    bool success,
    std::unique_ptr<std::map<std::string, IconEntry>> icon_entries) {
  if (!success) {
    std::move(callback).Run(false, LoadedIconsMap{});
    return;
  }

  std::vector<std::string> icons_uuid;
  std::vector<std::string> encoded_icons_data;
  for (auto& entry : *icon_entries.get()) {
    icons_uuid.emplace_back(std::move(entry.first));
    encoded_icons_data.emplace_back(std::move(entry.second.data));
  }

  icon_converter_->ConvertStringToIcon(
      std::move(encoded_icons_data),
      base::BindOnce(&IconProtoDbStore::OnIconsDecoded,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(icons_uuid)));
}

void IconProtoDbStore::OnIconsEncoded(
    AddCallback callback,
    std::vector<IconType> icons_type,
    std::vector<std::string> icons_uuid,
    std::unique_ptr<EncodeResult> encode_result) {
  IconTypeUuidMap icons_uuid_map;
  if (!encode_result->success) {
    std::move(callback).Run(std::move(icons_uuid_map), false);
    return;
  }

  auto entries_to_save = std::make_unique<KeyEntryVector>();
  auto encoded_data = std::move(encode_result->encoded_data);
  for (size_t i = 0; i < encoded_data.size(); i++) {
    IconEntry icon_entry;
    icon_entry.data = std::move(encoded_data[i]);
    entries_to_save->emplace_back(icons_uuid[i], std::move(icon_entry));
    icons_uuid_map.emplace(icons_type[i], std::move(icons_uuid[i]));
  }
  auto add_callback =
      base::BindOnce(std::move(callback), std::move(icons_uuid_map));
  db_->UpdateEntries(std::move(entries_to_save),
                     std::make_unique<KeyVector>() /*entries_to_delete*/,
                     std::move(add_callback));
}

void IconProtoDbStore::OnIconsDecoded(
    LoadIconsCallback callback,
    std::vector<std::string> icons_uuid,
    std::unique_ptr<DecodeResult> decoded_result) {
  if (!decoded_result->success) {
    std::move(callback).Run(false, LoadedIconsMap{});
    return;
  }

  LoadedIconsMap icons_map;
  auto icons = std::move(decoded_result->decoded_icons);
  for (size_t i = 0; i < icons_uuid.size(); i++) {
    IconBundle icon_bundle(std::move(icons[i]));
    icons_map.emplace(std::move(icons_uuid[i]), std::move(icon_bundle));
  }
  std::move(callback).Run(true, std::move(icons_map));
}

}  // namespace notifications
