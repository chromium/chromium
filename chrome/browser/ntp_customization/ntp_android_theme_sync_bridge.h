// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_THEME_SYNC_BRIDGE_H_
#define CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_THEME_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/model/data_type_store.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/protocol/theme_android_specifics.pb.h"

namespace syncer {
class DataTypeLocalChangeProcessor;
}  // namespace syncer

namespace ntp_customization {

// DataTypeSyncBridge for THEMES_ANDROID data type.
// It syncs the custom background and color for Android (Clank) devices.
class NtpAndroidThemeSyncBridge : public syncer::DataTypeSyncBridge {
 public:
  using ThemeChangedCallback =
      base::RepeatingCallback<void(const sync_pb::ThemeAndroidSpecifics&)>;

  NtpAndroidThemeSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      syncer::OnceDataTypeStoreFactory store_factory,
      ThemeChangedCallback theme_changed_callback);
  ~NtpAndroidThemeSyncBridge() override;

  NtpAndroidThemeSyncBridge(const NtpAndroidThemeSyncBridge&) = delete;
  NtpAndroidThemeSyncBridge& operator=(const NtpAndroidThemeSyncBridge&) =
      delete;

  // Updates the local theme data and commits the change to Sync.
  void UpdateTheme(const sync_pb::ThemeAndroidSpecifics& specifics);

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

 private:
  // Callback invoked when the DataTypeStore creation is finished.
  void OnStoreCreated(const std::optional<syncer::ModelError>& error,
                      std::unique_ptr<syncer::DataTypeStore> store);

  // Callback invoked when local entity records and metadata are loaded from storage.
  void OnDataLoaded(const std::optional<syncer::ModelError>& error,
                    std::unique_ptr<syncer::DataTypeStore::RecordList> data,
                    std::unique_ptr<syncer::MetadataBatch> metadata_batch);

  // Callback invoked when a DataTypeStore commit operation finishes.
  void OnCommitError(const std::optional<syncer::ModelError>& error);

  // Helper to update the in-memory cache, write to batch, and notify downstream callback.
  void ApplyThemeSpecifics(const sync_pb::ThemeAndroidSpecifics& specifics,
                           syncer::DataTypeStore::WriteBatch* write_batch);

  // The local LevelDB storage instance used for persisting entity data and metadata.
  std::unique_ptr<syncer::DataTypeStore> store_;

  // In-memory cached active Android theme specifics.
  std::optional<sync_pb::ThemeAndroidSpecifics> current_theme_;

  // Callback invoked when incoming sync changes arrive to notify downstream services.
  ThemeChangedCallback theme_changed_callback_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<NtpAndroidThemeSyncBridge> weak_factory_{this};
};

}  // namespace ntp_customization

#endif  // CHROME_BROWSER_NTP_CUSTOMIZATION_NTP_ANDROID_THEME_SYNC_BRIDGE_H_
