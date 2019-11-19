// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/drivefs_event_router.h"

#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/components/drivefs/mojom/drivefs.mojom.h"

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

}  // namespace

DriveFsEventRouter::DriveFsEventRouter() = default;
DriveFsEventRouter::~DriveFsEventRouter() = default;

void DriveFsEventRouter::OnUnmounted() {
  completed_bytes_ = 0;
  group_id_to_bytes_to_transfer_.clear();

  // Ensure any existing sync progress indicator is cleared.
  file_manager_private::FileTransferStatus status;
  status.transfer_state = file_manager_private::TRANSFER_STATE_FAILED;
  status.hide_when_zero_jobs = true;

  DispatchOnFileTransfersUpdatedEvent(status);
}

void DriveFsEventRouter::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  int64_t total_bytes_transferred = 0;
  int64_t total_bytes_to_transfer = 0;
  int num_files_syncing = 0;
  bool any_in_progress = false;
  for (const auto& item : syncing_status.item_events) {
    if (IsItemEventCompleted(item->state)) {
      auto it = group_id_to_bytes_to_transfer_.find(item->group_id);
      if (it != group_id_to_bytes_to_transfer_.end()) {
        completed_bytes_ += it->second;
        group_id_to_bytes_to_transfer_.erase(it);
      }
    } else {
      total_bytes_transferred += item->bytes_transferred;
      total_bytes_to_transfer += item->bytes_to_transfer;
      ++num_files_syncing;
      if (item->state == drivefs::mojom::ItemEvent::State::kInProgress) {
        any_in_progress = true;
      }
      if (item->bytes_to_transfer) {
        group_id_to_bytes_to_transfer_[item->group_id] =
            item->bytes_to_transfer;
      }
    }
  }
  auto completed_bytes = completed_bytes_;
  if (num_files_syncing == 0) {
    completed_bytes_ = 0;
    group_id_to_bytes_to_transfer_.clear();
  }

  file_manager_private::FileTransferStatus status;
  status.hide_when_zero_jobs = true;

  if ((completed_bytes == 0 && !any_in_progress) ||
      syncing_status.item_events.empty()) {
    status.transfer_state = file_manager_private::TRANSFER_STATE_COMPLETED;
    DispatchOnFileTransfersUpdatedEvent(status);
    // If the progress bar is not already visible, don't show it if no sync task
    // has actually started.
    return;
  }

  total_bytes_transferred += completed_bytes;
  total_bytes_to_transfer += completed_bytes;

  status.num_total_jobs = num_files_syncing;
  status.processed = total_bytes_transferred;
  status.total = total_bytes_to_transfer;

  auto extension_ids = GetEventListenerExtensionIds(
      file_manager_private::OnFileTransfersUpdated::kEventName);

  for (const auto& item : syncing_status.item_events) {
    status.transfer_state = ConvertItemEventState(item->state);

    base::FilePath path(item->path);
    for (const auto& extension_id : extension_ids) {
      status.file_url =
          ConvertDrivePathToFileSystemUrl(path, extension_id).spec();
      DispatchOnFileTransfersUpdatedEventToExtension(extension_id, status);
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
