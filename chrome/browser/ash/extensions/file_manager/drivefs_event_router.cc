// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/values.h"
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
constexpr auto& kIndividualPinEventName =
    file_manager_private::OnIndividualPinTransfersUpdated::kEventName;

constexpr extensions::events::HistogramValue kTransferEvent =
    extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED;
constexpr extensions::events::HistogramValue kPinEvent =
    extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED;

std::vector<IndividualFileTransferStatus> CopyIndividualStatuses(
    std::vector<IndividualFileTransferStatus>& statuses) {
  std::vector<IndividualFileTransferStatus> statuses_copy;
  for (const auto& status : statuses) {
    IndividualFileTransferStatus copy;
    copy.transfer_state = status.transfer_state;
    copy.processed = status.processed;
    copy.total = status.total;
    statuses_copy.push_back(std::move(copy));
  }
  return statuses_copy;
}

file_manager_private::TransferState ConvertItemEventState(
    drivefs::mojom::ItemEvent::State state) {
  switch (state) {
    case drivefs::mojom::ItemEvent::State::kQueued:
      return file_manager_private::TRANSFER_STATE_QUEUED;
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

DriveFsEventRouter::DriveFsEventRouter(
    SystemNotificationManager* notification_manager)
    : notification_manager_(notification_manager) {}
DriveFsEventRouter::~DriveFsEventRouter() = default;

DriveFsEventRouter::SyncingStatusState::SyncingStatusState() = default;
DriveFsEventRouter::SyncingStatusState::SyncingStatusState(
    const SyncingStatusState& other) = default;
DriveFsEventRouter::SyncingStatusState::~SyncingStatusState() = default;

void DriveFsEventRouter::OnUnmounted() {
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

  dialog_callback_.Reset();
}

void DriveFsEventRouter::OnSyncingStatusUpdate(
    const drivefs::mojom::SyncingStatus& syncing_status) {
  std::vector<const drivefs::mojom::ItemEvent*> transfer_items;
  std::vector<const drivefs::mojom::ItemEvent*> pin_items;

  for (const auto& item : syncing_status.item_events) {
    if (item->reason == drivefs::mojom::ItemEventReason::kTransfer) {
      transfer_items.push_back(item.get());
    } else {
      pin_items.push_back(item.get());
    }
  }

  if (base::FeatureList::IsEnabled(ash::features::kFilesInlineSyncStatus)) {
    BroadcastIndividualTransferEventsForItems(transfer_items, kTransferEvent,
                                              kIndividualTransferEventName);
    BroadcastIndividualTransferEventsForItems(pin_items, kPinEvent,
                                              kIndividualPinEventName);
    return;
  }

  BroadcastAggregateTransferEventForItems(
      transfer_items, kTransferEvent, kTransferEventName, sync_status_state_);
  BroadcastAggregateTransferEventForItems(pin_items, kPinEvent, kPinEventName,
                                          pin_status_state_);
}

void DriveFsEventRouter::BroadcastIndividualTransferEventsForItems(
    const std::vector<const drivefs::mojom::ItemEvent*>& items,
    const extensions::events::HistogramValue& event_type,
    const std::string& event_name) {
  std::vector<IndividualFileTransferStatus> statuses;
  std::vector<base::FilePath> paths;
  for (const auto* const item : items) {
    IndividualFileTransferStatus status;
    status.transfer_state = ConvertItemEventState(item->state);
    status.processed = item->bytes_transferred;
    status.total = item->bytes_to_transfer;
    statuses.push_back(std::move(status));
    paths.emplace_back(item->path);
  }

  for (const auto& url : GetEventListenerURLs(event_name)) {
    PathsToEntries(paths, url,
                   base::BindOnce(&DriveFsEventRouter::OnEntries,
                                  weak_ptr_factory_.GetWeakPtr(), event_type,
                                  CopyIndividualStatuses(statuses)));
  }
}

void DriveFsEventRouter::OnEntries(
    const extensions::events::HistogramValue& event_type,
    std::vector<IndividualFileTransferStatus> statuses,
    IndividualFileTransferEntries entries) {
  std::vector<IndividualFileTransferStatus> filtered_statuses;

  for (size_t i = 0; i < entries.size(); i++) {
    auto& entry = entries[i];
    auto& status = statuses[i];
    if (!entry.additional_properties.empty()) {
      status.entry = std::move(entry);
      filtered_statuses.push_back(std::move(status));
    }
  }
  BroadcastIndividualTransfersEvent(event_type, filtered_statuses);
}

void DriveFsEventRouter::BroadcastAggregateTransferEventForItems(
    const std::vector<const drivefs::mojom::ItemEvent*>& items,
    const extensions::events::HistogramValue& event_type,
    const std::string& event_name,
    SyncingStatusState& state) {
  std::vector<const drivefs::mojom::ItemEvent*> filtered_items;
  std::vector<const drivefs::mojom::ItemEvent*> ignored_items;
  bool are_any_failed = false;
  bool are_any_in_progress = false;
  int64_t total_bytes_transferred = 0;
  int64_t total_bytes_to_transfer = 0;
  int num_syncing_items = 0;
  const drivefs::mojom::ItemEvent* some_syncing_item = nullptr;

  for (const auto* item : items) {
    if (base::Contains(ignored_file_paths_, base::FilePath(item->path))) {
      ignored_items.push_back(item);
    } else {
      filtered_items.push_back(item);
    }
  }

  for (const auto* const item : filtered_items) {
    if (IsItemEventCompleted(item->state)) {
      auto it = state.group_id_to_bytes_to_transfer.find(item->group_id);
      if (it != state.group_id_to_bytes_to_transfer.end()) {
        state.completed_bytes += it->second;
        state.group_id_to_bytes_to_transfer.erase(it);
      }
      if (item->state == drivefs::mojom::ItemEvent_State::kFailed) {
        are_any_failed = true;
      }
      continue;
    }

    // Any not-completed item will do. It is exclusively used to display
    // notification copy when there's only one last item that is syncing.
    if (!some_syncing_item) {
      some_syncing_item = item;
    }
    if (item->state == drivefs::mojom::ItemEvent_State::kInProgress) {
      are_any_in_progress = true;
    }
    total_bytes_transferred += item->bytes_transferred;
    total_bytes_to_transfer += item->bytes_to_transfer;
    ++num_syncing_items;
    if (item->bytes_to_transfer) {
      state.group_id_to_bytes_to_transfer[item->group_id] =
          item->bytes_to_transfer;
    }
  }

  FileTransferStatus status;
  status.hide_when_zero_jobs = true;

  if (some_syncing_item) {
    status.show_notification = true;
    status.num_total_jobs = num_syncing_items;
    status.processed = total_bytes_transferred + state.completed_bytes;
    status.total = total_bytes_to_transfer + state.completed_bytes;
    status.transfer_state =
        are_any_in_progress ? file_manager_private::TRANSFER_STATE_IN_PROGRESS
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
  switch (event_type) {
    case extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED:
      BroadcastEvent(
          event_type, kIndividualTransferEventName,
          file_manager_private::OnIndividualFileTransfersUpdated::Create(
              status),
          false);
      break;
    case extensions::events::FILE_MANAGER_PRIVATE_ON_PIN_TRANSFERS_UPDATED:
      BroadcastEvent(
          event_type, kIndividualPinEventName,
          file_manager_private::OnIndividualPinTransfersUpdated::Create(status),
          false);
      break;
    default:
      NOTREACHED() << "Event type not handled: " << event_type;
  }
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
