// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/internal/icon_converter.h"
#include "chrome/browser/notifications/scheduler/internal/icon_converter_result.h"
#include "chrome/browser/notifications/scheduler/internal/icon_entry.h"
#include "chrome/browser/notifications/scheduler/public/notification_data.h"
#include "components/leveldb_proto/public/proto_database.h"

// Forward declaration for proto conversion.
namespace leveldb_proto {
void DataToProto(notifications::IconEntry* icon_entry,
                 notifications::proto::Icon* proto);

void ProtoToData(notifications::proto::Icon* proto,
                 notifications::IconEntry* icon_entry);
}  // namespace leveldb_proto

namespace notifications {

// Storage interface used to read/write icon data, each time only one icon can
// be loaded into memory.
class IconStore {
 public:
  using LoadedIconsMap = std::map<std::string /*icons_uuid*/, IconBundle>;
  using IconTypeUuidMap = std::map<IconType, std::string>;
  using IconTypeBundleMap = std::map<IconType, IconBundle>;
  using LoadedIconKeys = std::unique_ptr<std::vector<std::string>>;

  using InitAndLoadKeysCallback =
      base::OnceCallback<void(bool, LoadedIconKeys)>;
  using LoadIconsCallback = base::OnceCallback<void(bool, LoadedIconsMap)>;
  using AddCallback = base::OnceCallback<void(IconTypeUuidMap, bool)>;
  using UpdateCallback = base::OnceCallback<void(bool)>;

  // Initializes the storage, and load all keys.
  virtual void InitAndLoadKeys(InitAndLoadKeysCallback callback) = 0;

  // Loads multiple icons.
  virtual void LoadIcons(const std::vector<std::string>& keys,
                         LoadIconsCallback callback) = 0;

  // Adds multiple icons to storage.
  virtual void AddIcons(IconTypeBundleMap icons, AddCallback callback) = 0;

  // Deletes multiple icons.
  virtual void DeleteIcons(const std::vector<std::string>& keys,
                           UpdateCallback callback) = 0;

  IconStore() = default;
  IconStore(const IconStore&) = delete;
  IconStore& operator=(const IconStore&) = delete;
  virtual ~IconStore() = default;
};

// IconStore implementation backed by a proto database.
class IconProtoDbStore : public IconStore {
 public:
  explicit IconProtoDbStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db,
      std::unique_ptr<IconConverter> icon_converter);
  IconProtoDbStore(const IconProtoDbStore&) = delete;
  IconProtoDbStore& operator=(const IconProtoDbStore&) = delete;
  ~IconProtoDbStore() override;

 private:
  // IconStore implementation.
  void InitAndLoadKeys(InitAndLoadKeysCallback callback) override;
  void LoadIcons(const std::vector<std::string>& keys,
                 LoadIconsCallback callback) override;
  void AddIcons(IconTypeBundleMap icons, AddCallback callback) override;
  void DeleteIcons(const std::vector<std::string>& keys,
                   UpdateCallback callback) override;

  // Called when the proto database is initialized.
  void OnDbInitialized(InitAndLoadKeysCallback callback,
                       leveldb_proto::Enums::InitStatus status);

  // Called when the icon keys are retrieved from the database.
  void OnIconKeysLoaded(InitAndLoadKeysCallback callback,
                        bool success,
                        LoadedIconKeys icon_keys);

  // Called when the icons are retrieved from the database.
  void OnIconEntriesLoaded(
      LoadIconsCallback callback,
      bool success,
      std::unique_ptr<std::map<std::string, IconEntry>> icon_entries);

  // Called when the icons are encoded.
  void OnIconsEncoded(AddCallback callback,
                      std::vector<IconType> icons_type,
                      std::vector<std::string> icons_uuid,
                      std::unique_ptr<EncodeResult> encode_result);

  // Called when the encoded data are decoded.
  void OnIconsDecoded(LoadIconsCallback callback,
                      std::vector<std::string> icons_uuid,
                      std::unique_ptr<DecodeResult> decode_result);

  // The proto database instance that persists data.
  std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon, IconEntry>> db_;

  // Help serializing icons to disk and deserializing encoded data to icons.
  std::unique_ptr<IconConverter> icon_converter_;

  base::WeakPtrFactory<IconProtoDbStore> weak_ptr_factory_{this};
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_INTERNAL_ICON_STORE_H_
