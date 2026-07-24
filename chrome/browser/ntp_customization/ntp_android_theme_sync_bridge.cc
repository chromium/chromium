// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ntp_customization/ntp_android_theme_sync_bridge.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "ui/webui/buildflags.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/entity_data.h"

namespace ntp_customization {

namespace {

// Fixed storage key for the single Android theme entity.
constexpr char kAndroidThemeStorageKey[] = "current_android_theme";
constexpr char kAndroidThemeClientTag[] = "android_theme_tag";
constexpr char kAndroidThemeEntityName[] = "Android Theme";

// Helper function to create an EntityData object for ThemeAndroidSpecifics.
std::unique_ptr<syncer::EntityData> CreateEntityData(
    const sync_pb::ThemeAndroidSpecifics& specifics) {
  auto entity_data = std::make_unique<syncer::EntityData>();
  *entity_data->specifics.mutable_theme_android() = specifics;
  entity_data->name = kAndroidThemeEntityName;
  return entity_data;
}

}  // namespace

NtpAndroidThemeSyncBridge::NtpAndroidThemeSyncBridge(
    std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
    syncer::OnceDataTypeStoreFactory store_factory,
    ThemeChangedCallback theme_changed_callback)
    : syncer::DataTypeSyncBridge(std::move(change_processor)),
      theme_changed_callback_(std::move(theme_changed_callback)) {
  CHECK(!theme_changed_callback_.is_null());
  std::move(store_factory)
      .Run(syncer::THEMES_ANDROID,
           base::BindOnce(&NtpAndroidThemeSyncBridge::OnStoreCreated,
                          weak_factory_.GetWeakPtr()));
}

NtpAndroidThemeSyncBridge::~NtpAndroidThemeSyncBridge() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void NtpAndroidThemeSyncBridge::UpdateTheme(
    const sync_pb::ThemeAndroidSpecifics& specifics) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  NOTREACHED();
#else
  current_theme_ = specifics;

  if (!store_) {
    // In the unlikely event that a local theme update occurs before the
    // DataTypeStore has finished initializing, the update is dropped from the
    // sync commit queue.
    return;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch();
  write_batch->WriteData(kAndroidThemeStorageKey,
                         specifics.SerializeAsString());

  change_processor()->Put(kAndroidThemeStorageKey, CreateEntityData(specifics),
                          write_batch->GetMetadataChangeList());

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&NtpAndroidThemeSyncBridge::OnCommitError,
                     weak_factory_.GetWeakPtr()));
#endif
}

std::unique_ptr<syncer::MetadataChangeList>
NtpAndroidThemeSyncBridge::CreateMetadataChangeList() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return syncer::DataTypeStore::WriteBatch::CreateMetadataChangeList();
}

std::optional<syncer::ModelError> NtpAndroidThemeSyncBridge::MergeFullSyncData(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  NOTREACHED();
#else
  if (entity_data.size() > 1) {
    return syncer::ModelError(FROM_HERE,
                              syncer::ModelError::Type::kThemeTooManySpecifics);
  }
  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  if (!entity_data.empty()) {
    ApplyThemeSpecifics(entity_data.front()->data().specifics.theme_android(),
                        write_batch.get());
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&NtpAndroidThemeSyncBridge::OnCommitError,
                     weak_factory_.GetWeakPtr()));

  return std::nullopt;
#endif
}

std::optional<syncer::ModelError>
NtpAndroidThemeSyncBridge::ApplyIncrementalSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
    syncer::EntityChangeList entity_changes) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
#if BUILDFLAG(ENABLE_WEBUI_NTP)
  NOTREACHED();
#else
  if (entity_changes.size() > 1) {
    return syncer::ModelError(FROM_HERE,
                              syncer::ModelError::Type::kThemeTooManyChanges);
  }

  // TODO(crbug.com/488439751): Consider checking if the active local theme is an
  // unsyncable user-uploaded photo or managed by enterprise policy before
  // applying incremental remote sync changes, matching iOS/Desktop protections.

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch(std::move(metadata_change_list));

  for (const auto& change : entity_changes) {
    if (change->type() == syncer::EntityChange::ACTION_DELETE) {
      current_theme_.reset();
      write_batch->DeleteData(kAndroidThemeStorageKey);
      theme_changed_callback_.Run(sync_pb::ThemeAndroidSpecifics());
    } else {
      ApplyThemeSpecifics(change->data().specifics.theme_android(),
                          write_batch.get());
    }
  }

  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&NtpAndroidThemeSyncBridge::OnCommitError,
                     weak_factory_.GetWeakPtr()));

  return std::nullopt;
#endif
}

void NtpAndroidThemeSyncBridge::ApplyDisableSyncChanges(
    std::unique_ptr<syncer::MetadataChangeList> delete_metadata_change_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset the sync bridge's in-memory cache and delete local sync store data.
  // Note: `theme_changed_callback_` is intentionally NOT called here because on
  // Android, signing out or disabling sync should preserve the current local
  // background rather than resetting it back to default.
  current_theme_.reset();

  if (!store_) {
    return;
  }

  std::unique_ptr<syncer::DataTypeStore::WriteBatch> write_batch =
      store_->CreateWriteBatch(std::move(delete_metadata_change_list));
  write_batch->DeleteData(kAndroidThemeStorageKey);
  store_->CommitWriteBatch(
      std::move(write_batch),
      base::BindOnce(&NtpAndroidThemeSyncBridge::OnCommitError,
                     weak_factory_.GetWeakPtr()));
}

std::unique_ptr<syncer::DataBatch> NtpAndroidThemeSyncBridge::GetDataForCommit(
    StorageKeyList storage_keys) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto batch = std::make_unique<syncer::MutableDataBatch>();
  for (const std::string& key : storage_keys) {
    if (key == kAndroidThemeStorageKey && current_theme_.has_value()) {
      batch->Put(key, CreateEntityData(*current_theme_));
    }
  }
  return batch;
}

std::unique_ptr<syncer::DataBatch>
NtpAndroidThemeSyncBridge::GetAllDataForDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetDataForCommit({kAndroidThemeStorageKey});
}

bool NtpAndroidThemeSyncBridge::IsEntityDataValid(
    const syncer::EntityData& entity_data) const {
  return entity_data.specifics.has_theme_android();
}

std::string NtpAndroidThemeSyncBridge::GetClientTag(
    const syncer::EntityData& entity_data) const {
  return kAndroidThemeClientTag;
}

std::string NtpAndroidThemeSyncBridge::GetStorageKey(
    const syncer::EntityData& entity_data) const {
  return kAndroidThemeStorageKey;
}

sync_pb::EntitySpecifics
NtpAndroidThemeSyncBridge::TrimAllSupportedFieldsFromRemoteSpecifics(
    const sync_pb::EntitySpecifics& entity_specifics) const {
  sync_pb::EntitySpecifics trimmed_specifics = entity_specifics;
  trimmed_specifics.clear_theme_android();
  return trimmed_specifics;
}

void NtpAndroidThemeSyncBridge::OnStoreCreated(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore> store) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }
  store_ = std::move(store);
  store_->ReadAllDataAndMetadata(base::BindOnce(
      &NtpAndroidThemeSyncBridge::OnDataLoaded, weak_factory_.GetWeakPtr()));
}

void NtpAndroidThemeSyncBridge::OnDataLoaded(
    const std::optional<syncer::ModelError>& error,
    std::unique_ptr<syncer::DataTypeStore::RecordList> data,
    std::unique_ptr<syncer::MetadataBatch> metadata_batch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
    return;
  }

  for (const auto& record : *data) {
    if (record.id == kAndroidThemeStorageKey) {
      sync_pb::ThemeAndroidSpecifics specifics;
      if (specifics.ParseFromString(record.value)) {
        current_theme_ = specifics;
        break;
      } else {
        DLOG(ERROR) << "Failed to deserialize ThemeAndroidSpecifics.";
        continue;
      }
    }
  }

  change_processor()->ModelReadyToSync(std::move(metadata_batch));
}

void NtpAndroidThemeSyncBridge::OnCommitError(
    const std::optional<syncer::ModelError>& error) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error) {
    change_processor()->ReportError(*error);
  }
}

void NtpAndroidThemeSyncBridge::ApplyThemeSpecifics(
    const sync_pb::ThemeAndroidSpecifics& specifics,
    syncer::DataTypeStore::WriteBatch* write_batch) {
  current_theme_ = specifics;
  write_batch->WriteData(kAndroidThemeStorageKey,
                         specifics.SerializeAsString());
  theme_changed_callback_.Run(specifics);
}

}  // namespace ntp_customization
