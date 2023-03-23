// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/values.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/sync_status_tracker.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace file_manager {
namespace file_manager_private = extensions::api::file_manager_private;

namespace {

constexpr auto& kTransferEventName =
    file_manager_private::OnFileTransfersUpdated::kEventName;
constexpr auto& kPinEventName =
    file_manager_private::OnPinTransfersUpdated::kEventName;
constexpr auto& kIndividualTransferEventName =
    file_manager_private::OnIndividualFileTransfersUpdated::kEventName;

constexpr extensions::events::HistogramValue kTransferEvent =
    extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED;
constexpr extensions::events::HistogramValue kPinEvent =
    extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED;

file_manager_private::DriveConfirmDialogType ConvertDialogReasonType(
    drivefs::mojom::DialogReason::Type type) {
  switch (type) {
    case drivefs::mojom::DialogReason::Type::kEnableDocsOffline:
      return file_manager_private::
          DRIVE_CONFIRM_DIALOG_TYPE_ENABLE_DOCS_OFFLINE;
  }
}

}  // namespace

DriveFsEventRouter::DriveFsEventRouter(
    SystemNotificationManager* notification_manager)
    : notification_manager_(notification_manager) {}
DriveFsEventRouter::~DriveFsEventRouter() = default;

DriveFsEventRouter::SyncingStatusState::SyncingStatusState() = default;
DriveFsEventRouter::SyncingStatusState::SyncingStatusState(
    const SyncingStatusState& other) = default;
DriveFsEventRouter::SyncingStatusState::~SyncingStatusState() = default;

void DriveFsEventRouter::OnUnmounted() {
  if (!base::FeatureList::IsEnabled(ash::features::kFilesInlineSyncStatus)) {
    sync_status_state_.completed_bytes = 0;
    sync_status_state_.group_id_to_bytes_to_transfer.clear();
    pin_status_state_.completed_bytes = 0;
    pin_status_state_.group_id_to_bytes_to_transfer.clear();

    // Ensure any existing sync progress indicator is cleared.
    FileTransferStatus sync_status;
    sync_status.transfer_state = file_manager_private::TRANSFER_STATE_FAILED;
    sync_status.show_notification = true;
    sync_status.hide_when_zero_jobs = true;
    FileTransferStatus pin_status;
    pin_status.transfer_state = file_manager_private::TRANSFER_STATE_FAILED;
    pin_status.show_notification = true;
    pin_status.hide_when_zero_jobs = true;

    BroadcastTransferEvent(kTransferEvent, sync_status);
    BroadcastTransferEvent(kPinEvent, pin_status);
  }

  dialog_callback_.Reset();
}

file_manager_private::SyncStatus ConvertSyncStatus(drivefs::SyncStatus status) {
  switch (status) {
    case drivefs::SyncStatus::kNotFound:
    case drivefs::SyncStatus::kMoved:
      return file_manager_private::SYNC_STATUS_NOT_FOUND;
    case drivefs::SyncStatus::kQueued:
      return file_manager_private::SYNC_STATUS_QUEUED;
    case drivefs::SyncStatus::kInProgress:
      return file_manager_private::SYNC_STATUS_IN_PROGRESS;
    case drivefs::SyncStatus::kCompleted:
      return file_manager_private::SYNC_STATUS_COMPLETED;
    case drivefs::SyncStatus::kError:
      return file_manager_private::SYNC_STATUS_ERROR;
    default:
      NOTREACHED();
      return file_manager_private::SYNC_STATUS_NOT_FOUND;
  }
}

void DriveFsEventRouter::OnIndividualSyncingStatusesDelta(
    const std::vector<const drivefs::SyncState>& sync_states) {
  std::vector<IndividualFileTransferStatus> statuses;
  std::vector<base::FilePath> paths;

  for (const auto& sync_state : sync_states) {
    IndividualFileTransferStatus status;
    status.sync_status = ConvertSyncStatus(sync_state.status);
    status.progress = sync_state.progress;
    statuses.emplace_back(std::move(status));
    paths.emplace_back(sync_state.path);
  }

  for (const GURL& url : GetEventListenerURLs(kIndividualTransferEventName)) {
    const auto file_urls = ConvertPathsToFileSystemUrls(paths, url);
    for (size_t i = 0; i < file_urls.size(); i++) {
      statuses[i].file_url = file_urls[i].spec();
    }
    // Note: Inline Sync Statuses don't need to differentiate between transfer
    // and pin events because they do not display aggregate progress separately
    // for each of those two categories.
    BroadcastIndividualTransfersEvent(kTransferEvent, statuses);
  }
}

void DriveFsEventRouter::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  // These events are not consumed by Files app when InlineSyncStatus is
  // enabled.
  if (base::FeatureList::IsEnabled(ash::features::kFilesInlineSyncStatus)) {
    return;
  }

  std::vector<const drivefs::mojom::ItemEvent*> transfer_items;
  std::vector<const drivefs::mojom::ItemEvent*> pin_items;

  for (const auto& item : syncing_status.item_events) {
    if (item->reason == drivefs::mojom::ItemEventReason::kTransfer) {
      transfer_items.push_back(item.get());
    } else {
      pin_items.push_back(item.get());
    }
  }

  BroadcastAggregateTransferEventForItems(
      transfer_items, kTransferEvent, kTransferEventName, sync_status_state_);
  BroadcastAggregateTransferEventForItems(pin_items, kPinEvent, kPinEventName,
                                          pin_status_state_);
}

void DriveFsEventRouter::BroadcastAggregateTransferEventForItems(
    const std::vector<const drivefs::mojom::ItemEvent*>& items,
    const extensions::events::HistogramValue& event_type,
    const std::string& event_name,
    SyncingStatusState& state) {
  std::vector<const drivefs::mojom::ItemEvent*> filtered_items;
  std::vector<const drivefs::mojom::ItemEvent*> ignored_items;
  bool are_any_failed = false;
  int64_t in_progress_transferred = 0;
  int64_t in_progress_total = 0;
  int64_t num_in_progress_items = 0;
  const drivefs::mojom::ItemEvent* some_syncing_item = nullptr;

  for (const auto* item : items) {
    if (base::Contains(ignored_file_paths_, base::FilePath(item->path))) {
      ignored_items.push_back(item);
      // Stop tracking ignored queued events.
      if (const auto node =
              state.group_id_to_queued_bytes.extract(item->group_id)) {
        state.queued_bytes -= node.mapped();
      }
    } else {
      filtered_items.push_back(item);
    }
  }

  for (const auto* const item : filtered_items) {
    using State = drivefs::mojom::ItemEvent::State;
    const auto queued_it = state.group_id_to_queued_bytes.find(item->group_id);
    const bool was_queued = queued_it != state.group_id_to_queued_bytes.end();
    if (item->state != State::kQueued && was_queued) {
      state.queued_bytes -=
          state.group_id_to_queued_bytes.extract(queued_it).mapped();
    }

    if (item->state == State::kCompleted || item->state == State::kFailed) {
      if (const auto node =
              state.group_id_to_bytes_to_transfer.extract(item->group_id)) {
        state.completed_bytes += node.mapped();
      }
      if (item->state == State::kFailed) {
        are_any_failed = true;
      }
      continue;
    }

    // Any not-completed item will do. It is exclusively used to display
    // notification copy when there's only one last item that is syncing.
    if (!some_syncing_item) {
      some_syncing_item = item;
    }

    state.group_id_to_bytes_to_transfer[item->group_id] =
        item->bytes_to_transfer;

    if (item->state == State::kQueued) {
      if (!was_queued) {
        state.group_id_to_queued_bytes[item->group_id] =
            item->bytes_to_transfer;
        state.queued_bytes += item->bytes_to_transfer;
      }
      continue;
    }

    DCHECK(item->state == State::kInProgress);

    // If reached, item is "in progress".
    num_in_progress_items++;
    in_progress_transferred += item->bytes_transferred;
    in_progress_total += item->bytes_to_transfer;
  }

  FileTransferStatus status;
  status.hide_when_zero_jobs = true;

  if (some_syncing_item) {
    status.show_notification = true;
    status.num_total_jobs =
        num_in_progress_items + state.group_id_to_queued_bytes.size();
    status.processed = in_progress_transferred + state.completed_bytes;
    status.total =
        in_progress_total + state.completed_bytes + state.queued_bytes;
    status.transfer_state =
        num_in_progress_items ? file_manager_private::TRANSFER_STATE_IN_PROGRESS
                              : file_manager_private::TRANSFER_STATE_QUEUED;

    base::FilePath path(some_syncing_item->path);
    for (const auto& url : GetEventListenerURLs(event_name)) {
      status.file_url = ConvertDrivePathToFileSystemUrl(path, url).spec();
      BroadcastTransferEvent(event_type, status);
    }

    return;
  }

  // If no events of this type were filtered in and at least one was
  // filtered out because it was ignored, this means all remaining events of
  // this type are currently ignored. Let's silently hide the notification.
  status.show_notification =
      !(filtered_items.empty() && !ignored_items.empty());
  state.completed_bytes = 0;
  state.group_id_to_bytes_to_transfer.clear();
  state.group_id_to_queued_bytes.clear();
  status.transfer_state = are_any_failed
                              ? file_manager_private::TRANSFER_STATE_FAILED
                              : file_manager_private::TRANSFER_STATE_COMPLETED;
  BroadcastTransferEvent(event_type, status);
}

void DriveFsEventRouter::OnFilesChanged(
    const std::vector<drivefs::mojom::FileChange>& changes) {
  // Maps from parent directory to event for that directory.
  std::map<base::FilePath,
           extensions::api::file_manager_private::FileWatchEvent>
      events;
  for (const auto& listener_url : GetEventListenerURLs(
           file_manager_private::OnDirectoryChanged::kEventName)) {
    for (const auto& change : changes) {
      auto& event = events[change.path.DirName()];
      if (!event.changed_files) {
        event.event_type = extensions::api::file_manager_private::
            FILE_WATCH_EVENT_TYPE_CHANGED;
        event.changed_files.emplace();
        event.entry.additional_properties.Set(
            "fileSystemRoot", base::StrCat({ConvertDrivePathToFileSystemUrl(
                                                base::FilePath(), listener_url)
                                                .spec(),
                                            "/"}));
        event.entry.additional_properties.Set("fileSystemName",
                                              GetDriveFileSystemName());
        event.entry.additional_properties.Set("fileFullPath",
                                              change.path.DirName().value());
        event.entry.additional_properties.Set("fileIsDirectory", true);
      }
      event.changed_files->emplace_back();
      auto& file_manager_change = event.changed_files->back();
      file_manager_change.url =
          ConvertDrivePathToFileSystemUrl(change.path, listener_url).spec();
      file_manager_change.changes.push_back(
          change.type == drivefs::mojom::FileChange::Type::kDelete
              ? extensions::api::file_manager_private::CHANGE_TYPE_DELETE
              : extensions::api::file_manager_private::
                    CHANGE_TYPE_ADD_OR_UPDATE);
    }
    for (auto& event : events) {
      BroadcastOnDirectoryChangedEvent(event.first, event.second);
    }
  }
}

void DriveFsEventRouter::OnError(const drivefs::mojom::DriveError& error) {
  file_manager_private::DriveSyncErrorEvent event;
  switch (error.type) {
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFull:
      event.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE;
      break;
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFullOrganization:
      event.type = file_manager_private::
          DRIVE_SYNC_ERROR_TYPE_NO_SERVER_SPACE_ORGANIZATION;
      break;
    case drivefs::mojom::DriveError::Type::kPinningFailedDiskFull:
      event.type = file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_LOCAL_SPACE;
      break;
    case drivefs::mojom::DriveError::Type::kCantUploadSharedDriveStorageFull:
      event.type =
          file_manager_private::DRIVE_SYNC_ERROR_TYPE_NO_SHARED_DRIVE_SPACE;
      event.shared_drive = error.shared_drive;
      break;
  }
  for (const auto& listener_url : GetEventListenerURLs(
           file_manager_private::OnDriveSyncError::kEventName)) {
    event.file_url =
        ConvertDrivePathToFileSystemUrl(error.path, listener_url).spec();
    BroadcastEvent(extensions::events::FILE_MANAGER_PRIVATE_ON_DRIVE_SYNC_ERROR,
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
  auto urls = GetEventListenerURLs(
      file_manager_private::OnDriveConfirmDialog::kEventName);
  if (urls.empty()) {
    std::move(callback).Run(drivefs::mojom::DialogResult::kNotDisplayed);
    return;
  }
  dialog_callback_ = std::move(callback);

  file_manager_private::DriveConfirmDialogEvent event;
  event.type = ConvertDialogReasonType(reason.type);
  for (const auto& listener_url : urls) {
    event.file_url =
        ConvertDrivePathToFileSystemUrl(reason.path, listener_url).spec();
    BroadcastEvent(
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

void DriveFsEventRouter::SuppressNotificationsForFilePath(
    const base::FilePath& path) {
  ignored_file_paths_.insert(path);
}

void DriveFsEventRouter::RestoreNotificationsForFilePath(
    const base::FilePath& path) {
  if (ignored_file_paths_.erase(path) == 0) {
    LOG(ERROR) << "Provided file path was not in the set of ignored paths";
  }
}

void DriveFsEventRouter::BroadcastTransferEvent(
    const extensions::events::HistogramValue event_type,
    const FileTransferStatus& status) {
  switch (event_type) {
    case extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED:
      BroadcastEvent(
          event_type, kTransferEventName,
          file_manager_private::OnFileTransfersUpdated::Create(status));
      break;
    case extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED:
      BroadcastEvent(
          event_type, kPinEventName,
          file_manager_private::OnPinTransfersUpdated::Create(status));
      break;
    default:
      NOTREACHED() << "Event type not handled: " << event_type;
  }
}

void DriveFsEventRouter::BroadcastIndividualTransfersEvent(
    const extensions::events::HistogramValue event_type,
    const std::vector<IndividualFileTransferStatus>& status) {
  BroadcastEvent(
      event_type, kIndividualTransferEventName,
      file_manager_private::OnIndividualFileTransfersUpdated::Create(status),
      /*dispatch_to_system_notification=*/false);
}

void DriveFsEventRouter::BroadcastOnDirectoryChangedEvent(
    const base::FilePath& directory,
    const extensions::api::file_manager_private::FileWatchEvent& event) {
  if (!IsPathWatched(directory)) {
    return;
  }
  BroadcastEvent(extensions::events::FILE_MANAGER_PRIVATE_ON_DIRECTORY_CHANGED,
                 file_manager_private::OnDirectoryChanged::kEventName,
                 file_manager_private::OnDirectoryChanged::Create(event));
}

}  // namespace file_manager
