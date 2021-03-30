// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_manager_private.h"

namespace file_manager {
namespace file_manager_private = extensions::api::file_manager_private;

namespace {

file_manager_private::TransferState ConvertItemEventState(
    drivefs::mojom::ItemEvent::State state) {
  switch (state) {
    case drivefs::mojom::ItemEvent::State::kQueued:
    case drivefs::mojom::ItemEvent::State::kInProgress:
      return file_manager_private::TRANSFER_STATE_IN_PROGRESS;
    case drivefs::mojom::ItemEvent::State::kCompleted:
      return file_manager_private::TRANSFER_STATE_COMPLETED;
    case drivefs::mojom::ItemEvent::State::kFailed:
      return file_manager_private::TRANSFER_STATE_FAILED;
  }
}

bool IsItemEventCompleted(drivefs::mojom::ItemEvent::State state) {
  switch (state) {
    case drivefs::mojom::ItemEvent::State::kQueued:
    case drivefs::mojom::ItemEvent::State::kInProgress:
      return false;
    case drivefs::mojom::ItemEvent::State::kCompleted:
    case drivefs::mojom::ItemEvent::State::kFailed:
      return true;
  }
  return false;
}

file_manager_private::DriveConfirmDialogType ConvertDialogReasonType(
    drivefs::mojom::DialogReason::Type type) {
  switch (type) {
    case drivefs::mojom::DialogReason::Type::kEnableDocsOffline:
      return file_manager_private::
          DRIVE_CONFIRM_DIALOG_TYPE_ENABLE_DOCS_OFFLINE;
  }
}

}  // namespace

DriveFsEventRouter::DriveFsEventRouter() = default;
DriveFsEventRouter::~DriveFsEventRouter() = default;

DriveFsEventRouter::SyncingStatusState::SyncingStatusState() = default;
DriveFsEventRouter::SyncingStatusState::~SyncingStatusState() = default;

void DriveFsEventRouter::OnUnmounted() {
  sync_status_state_.completed_bytes = 0;
  sync_status_state_.group_id_to_bytes_to_transfer.clear();
  pin_status_state_.completed_bytes = 0;
  pin_status_state_.group_id_to_bytes_to_transfer.clear();

  // Ensure any existing sync progress indicator is cleared.
  file_manager_private::FileTransferStatus sync_status;
  sync_status.transfer_state = file_manager_private::TRANSFER_STATE_FAILED;
  sync_status.hide_when_zero_jobs = true;
  file_manager_private::FileTransferStatus pin_status;
  pin_status.transfer_state = file_manager_private::TRANSFER_STATE_FAILED;
  pin_status.hide_when_zero_jobs = true;

  DispatchOnFileTransfersUpdatedEvent(sync_status);
  DispatchOnPinTransfersUpdatedEvent(pin_status);

  dialog_callback_.Reset();
}

file_manager_private::FileTransferStatus
DriveFsEventRouter::CreateFileTransferStatus(
    const std::vector<drivefs::mojom::ItemEvent*>& item_events,
    SyncingStatusState* state) {
  int64_t total_bytes_transferred = 0;
  int64_t total_bytes_to_transfer = 0;
  int num_files_syncing = 0;
  bool any_in_progress = false;
  for (const auto* item : item_events) {
    if (IsItemEventCompleted(item->state)) {
      auto it = state->group_id_to_bytes_to_transfer.find(item->group_id);
      if (it != state->group_id_to_bytes_to_transfer.end()) {
        state->completed_bytes += it->second;
        state->group_id_to_bytes_to_transfer.erase(it);
      }
    } else {
      total_bytes_transferred += item->bytes_transferred;
      total_bytes_to_transfer += item->bytes_to_transfer;
      ++num_files_syncing;
      if (item->state == drivefs::mojom::ItemEvent::State::kInProgress) {
        any_in_progress = true;
      }
      if (item->bytes_to_transfer) {
        state->group_id_to_bytes_to_transfer[item->group_id] =
            item->bytes_to_transfer;
      }
    }
  }
  auto completed_bytes = state->completed_bytes;
  if (num_files_syncing == 0) {
    state->completed_bytes = 0;
    state->group_id_to_bytes_to_transfer.clear();
  }

  file_manager_private::FileTransferStatus status;
  status.hide_when_zero_jobs = true;

  if ((completed_bytes == 0 && !any_in_progress) || item_events.empty()) {
    status.transfer_state = file_manager_private::TRANSFER_STATE_COMPLETED;
    return status;
  }

  total_bytes_transferred += completed_bytes;
  total_bytes_to_transfer += completed_bytes;

  status.num_total_jobs = num_files_syncing;
  status.processed = total_bytes_transferred;
  status.total = total_bytes_to_transfer;
  return status;
}

void DriveFsEventRouter::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  std::vector<drivefs::mojom::ItemEvent*> sync_events, pin_events;
  for (const auto& item : syncing_status.item_events) {
    if (item->reason == drivefs::mojom::ItemEventReason::kPin) {
      pin_events.push_back(item.get());
    } else {
      sync_events.push_back(item.get());
    }
  }
  auto sync_status = CreateFileTransferStatus(sync_events, &sync_status_state_);
  auto pin_status = CreateFileTransferStatus(pin_events, &pin_status_state_);

  auto extension_ids = GetEventListenerExtensionIds(
      file_manager_private::OnFileTransfersUpdated::kEventName);

  if (sync_status.total == 0) {
    DispatchOnFileTransfersUpdatedEvent(sync_status);
  } else {
    for (const auto* item : sync_events) {
      sync_status.transfer_state = ConvertItemEventState(item->state);
      base::FilePath path(item->path);
      for (const auto& extension_id : extension_ids) {
        sync_status.file_url =
            ConvertDrivePathToFileSystemUrl(path, extension_id).spec();
        DispatchOnFileTransfersUpdatedEventToExtension(extension_id,
                                                       sync_status);
      }
    }
  }

  if (pin_status.total == 0) {
    DispatchOnPinTransfersUpdatedEvent(pin_status);
  } else {
    for (const auto* item : pin_events) {
      pin_status.transfer_state = ConvertItemEventState(item->state);
      base::FilePath path(item->path);
      for (const auto& extension_id : extension_ids) {
        pin_status.file_url =
            ConvertDrivePathToFileSystemUrl(path, extension_id).spec();
        DispatchOnPinTransfersUpdatedEventToExtension(extension_id, pin_status);
      }
    }
  }
}

void DriveFsEventRouter::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  // Maps from parent directory to event for that directory.
  std::map<base::FilePath,
           extensions::api::file_manager_private::FileWatchEvent>
      events;
  for (const auto& extension_id : GetEventListenerExtensionIds(
           file_manager_private::OnDirectoryChanged::kEventName)) {
    for (const auto& change : changes) {
      auto& event = events[change.path.DirName()];
      if (!event.changed_files) {
        event.event_type = extensions::api::file_manager_private::
            FILE_WATCH_EVENT_TYPE_CHANGED;
        event.changed_files = std::make_unique<
            std::vector<extensions::api::file_manager_private::FileChange>>();
        event.entry.additional_properties.SetString(
            "fileSystemRoot", base::StrCat({ConvertDrivePathToFileSystemUrl(
                                                base::FilePath(), extension_id)
                                                .spec(),
                                            "/"}));
        event.entry.additional_properties.SetString("fileSystemName",
                                                    GetDriveFileSystemName());
        event.entry.additional_properties.SetString(
            "fileFullPath", change.path.DirName().value());
        event.entry.additional_properties.SetBoolean("fileIsDirectory", true);
      }
      event.changed_files->emplace_back();
      auto& file_manager_change = event.changed_files->back();
      file_manager_change.url =
          ConvertDrivePathToFileSystemUrl(change.path, extension_id).spec();
      file_manager_change.changes.push_back(
          change.type == drivefs::mojom::FileChange::Type::kDelete
              ? extensions::api::file_manager_private::CHANGE_TYPE_DELETE
              : extensions::api::file_manager_private::
                    CHANGE_TYPE_ADD_OR_UPDATE);
    }
    for (auto& event : events) {
      DispatchOnDirectoryChangedEventToExtension(extension_id, event.first,
                                                 event.second);
    }
  }
}

void DriveFsEventRouter::OnError(const drivefs::mojom::DriveError& error) {
  file_manager_private::DriveSyncErrorEvent event;
  switch (error.type) {
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFull:
      event.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE;
      break;
    case drivefs::mojom::DriveError::Type::kPinningFailedDiskFull:
      event.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_LOCAL_SPACE;
      break;
  }
  for (const auto& extension_id : GetEventListenerExtensionIds(
           file_manager_private::OnDriveSyncError::kEventName)) {
    event.file_url =
        ConvertDrivePathToFileSystemUrl(error.path, extension_id).spec();
    DispatchEventToExtension(
        extension_id,
        extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
        file_manager_private::OnDriveSyncError::kEventName,
        file_manager_private::OnDriveSyncError::Create(event));
  }
}

void DriveFsEventRouter::DisplayConfirmDialog(
    const drivefs::mojom::DialogReason& reason,
    base::OnceCallback<void(drivefs::mojom::DialogResult)> callback) {
  if (dialog_callback_) {
    std::move(callback).Run(drivefs::mojom::DialogResult::kNotDisplayed);
    return;
  }
  auto extension_ids = GetEventListenerExtensionIds(
      file_manager_private::OnDriveConfirmDialog::kEventName);
  if (extension_ids.empty()) {
    std::move(callback).Run(drivefs::mojom::DialogResult::kNotDisplayed);
    return;
  }
  dialog_callback_ = std::move(callback);

  file_manager_private::DriveConfirmDialogEvent event;
  event.type = ConvertDialogReasonType(reason.type);
  for (const auto& extension_id : extension_ids) {
    event.file_url =
        ConvertDrivePathToFileSystemUrl(reason.path, extension_id).spec();
    DispatchEventToExtension(
        extension_id,
        extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_CONFIRM_DIALOG,
        file_manager_private::OnDriveConfirmDialog::kEventName,
        file_manager_private::OnDriveConfirmDialog::Create(event));
  }
}

void DriveFsEventRouter::OnDialogResult(drivefs::mojom::DialogResult result) {
  if (dialog_callback_) {
    std::move(dialog_callback_).Run(result);
  }
}

void DriveFsEventRouter::DispatchOnFileTransfersUpdatedEvent(
    const extensions::api::file_manager_private::FileTransferStatus& status) {
  for (const auto& extension_id : GetEventListenerExtensionIds(
           file_manager_private::OnFileTransfersUpdated::kEventName)) {
    DispatchOnFileTransfersUpdatedEventToExtension(extension_id, status);
  }
}

void DriveFsEventRouter::DispatchOnFileTransfersUpdatedEventToExtension(
    const std::string& extension_id,
    const extensions::api::file_manager_private::FileTransferStatus& status) {
  DispatchEventToExtension(
      extension_id,
      extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED,
      file_manager_private::OnFileTransfersUpdated::kEventName,
      file_manager_private::OnFileTransfersUpdated::Create(status));
}

void DriveFsEventRouter::DispatchOnPinTransfersUpdatedEvent(
    const extensions::api::file_manager_private::FileTransferStatus& status) {
  for (const auto& extension_id : GetEventListenerExtensionIds(
           file_manager_private::OnPinTransfersUpdated::kEventName)) {
    DispatchOnPinTransfersUpdatedEventToExtension(extension_id, status);
  }
}

void DriveFsEventRouter::DispatchOnPinTransfersUpdatedEventToExtension(
    const std::string& extension_id,
    const extensions::api::file_manager_private::FileTransferStatus& status) {
  DispatchEventToExtension(
      extension_id,
      extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED,
      file_manager_private::OnPinTransfersUpdated::kEventName,
      file_manager_private::OnPinTransfersUpdated::Create(status));
}

void DriveFsEventRouter::DispatchOnDirectoryChangedEventToExtension(
    const std::string& extension_id,
    const base::FilePath& directory,
    const extensions::api::file_manager_private::FileWatchEvent& event) {
  if (!IsPathWatched(directory)) {
    return;
  }
  DispatchEventToExtension(
      extension_id,
      extensions::events::FILE_MANAGER_PRIVATE_ON_DIRECTORY_CHANGED,
      file_manager_private::OnDirectoryChanged::kEventName,
      file_manager_private::OnDirectoryChanged::Create(event));
}

}  // namespace file_manager
