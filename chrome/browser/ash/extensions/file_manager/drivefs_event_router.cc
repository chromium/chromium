// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/file_manager/drivefs_event_router.h"

#include "ash/constants/ash_features.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/extensions/file_manager/private_api_util.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/drivefs_pinning_manager.h"
#include "extensions/browser/extension_event_histogram_value.h"

namespace file_manager {
namespace file_manager_private = extensions::api::file_manager_private;

namespace {

constexpr auto& kIndividualTransferEventName =
    file_manager_private::OnIndividualFileTransfersUpdated::kEventName;

constexpr extensions::events::HistogramValue kTransferEvent =
    extensions::events::FILE_MANAGER_PRIVATE_ON_FILE_TRANSFERS_UPDATED;

file_manager_private::DriveConfirmDialogType ConvertDialogReasonType(
    drivefs::mojom::DialogReason::Type type) {
  switch (type) {
    case drivefs::mojom::DialogReason::Type::kEnableDocsOffline:
      return file_manager_private::DriveConfirmDialogType::kEnableDocsOffline;
  }
}

}  // namespace

// Time interval to check for stale sync status in
// DriveFsEventRouter::path_to_sync_state_.
constexpr auto kSyncStateStaleCheckInterval = base::Seconds(100);

// Time after which a sync status in DriveFsEventRouter::path_to_sync_state_ is
// considered to be stale.
constexpr auto kSyncStateStaleThreshold = base::Seconds(90);

DriveFsEventRouter::DriveFsEventRouter(
    Profile* profile,
    SystemNotificationManager* notification_manager)
    : profile_(profile), notification_manager_(notification_manager) {
  stale_sync_state_cleanup_timer_.Start(
      FROM_HERE, kSyncStateStaleCheckInterval,
      base::BindRepeating(&DriveFsEventRouter::ClearStaleSyncStates,
                          weak_ptr_factory_.GetWeakPtr()));
}

DriveFsEventRouter::~DriveFsEventRouter() = default;

void DriveFsEventRouter::OnUnmounted() {
  stale_sync_state_cleanup_timer_.Stop();
  path_to_sync_state_.clear();
  dialog_callback_.Reset();
}

file_manager_private::SyncStatus ConvertSyncStatus(drivefs::SyncStatus status) {
  switch (status) {
    case drivefs::SyncStatus::kNotFound:
      return file_manager_private::SyncStatus::kNotFound;
    case drivefs::SyncStatus::kQueued:
      return file_manager_private::SyncStatus::kQueued;
    case drivefs::SyncStatus::kInProgress:
      return file_manager_private::SyncStatus::kInProgress;
    case drivefs::SyncStatus::kCompleted:
      return file_manager_private::SyncStatus::kCompleted;
    case drivefs::SyncStatus::kError:
      return file_manager_private::SyncStatus::kError;
    default:
      NOTREACHED_IN_MIGRATION();
      return file_manager_private::SyncStatus::kNotFound;
  }
}

void DriveFsEventRouter::OnItemProgress(
    const drivefs::mojom::ProgressEvent& event) {
  base::FilePath file_path;
  std::string path;

  if (event.file_path.has_value()) {
    file_path = *event.file_path;
    path = file_path.value();
  } else {
    path = event.path;
    file_path = base::FilePath(path);
  }

  drivefs::SyncStatus status;
  if (event.progress == 0) {
    status = drivefs::SyncStatus::kQueued;
  } else if (event.progress == 100) {
    status = drivefs::SyncStatus::kCompleted;
  } else {
    status = drivefs::SyncStatus::kInProgress;
  }

  std::vector<drivefs::SyncState> filtered_states;

  filtered_states.emplace_back(
      drivefs::SyncState{status, static_cast<float>(event.progress) / 100.0f,
                         file_path, base::Time::Now()});

  if (status == drivefs::SyncStatus::kCompleted) {
    const auto previous_state_iter = path_to_sync_state_.find(path);
    const bool was_tracked = previous_state_iter != path_to_sync_state_.end();
    if (was_tracked) {
      // Stop tracking completed events but push it to subscribers.
      path_to_sync_state_.erase(previous_state_iter);
    }
  } else {
    path_to_sync_state_[path] = filtered_states.back();
    if (!stale_sync_state_cleanup_timer_.IsRunning()) {
      stale_sync_state_cleanup_timer_.Reset();
    }
  }

  std::vector<IndividualFileTransferStatus> statuses;
  std::vector<base::FilePath> paths;

  for (const auto& sync_state : filtered_states) {
    IndividualFileTransferStatus individual_status;
    individual_status.sync_status = ConvertSyncStatus(sync_state.status);
    individual_status.progress = sync_state.progress;
    statuses.emplace_back(std::move(individual_status));
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
        event.event_type =
            extensions::api::file_manager_private::FileWatchEventType::kChanged;
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
              ? extensions::api::file_manager_private::ChangeType::kDelete
              : extensions::api::file_manager_private::ChangeType::
                    kAddOrUpdate);
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
      event.type = file_manager_private::DriveSyncErrorType::kNoServerSpace;
      break;
    case drivefs::mojom::DriveError::Type::kCantUploadStorageFullOrganization:
      event.type =
          file_manager_private::DriveSyncErrorType::kNoServerSpaceOrganization;
      break;
    case drivefs::mojom::DriveError::Type::kPinningFailedDiskFull:
      event.type = file_manager_private::DriveSyncErrorType::kNoLocalSpace;
      break;
    case drivefs::mojom::DriveError::Type::kCantUploadSharedDriveStorageFull:
      event.type =
          file_manager_private::DriveSyncErrorType::kNoSharedDriveSpace;
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

void DriveFsEventRouter::Observe(
    drive::DriveIntegrationService* const service) {
  DCHECK(service);
  drive::DriveIntegrationService::Observer::Observe(service);
  drivefs::DriveFsHost* const host = service->GetDriveFsHost();
  drivefs::DriveFsHost::Observer::Observe(host);
  host->set_dialog_handler(
      base::BindRepeating(&DriveFsEventRouter::DisplayConfirmDialog,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DriveFsEventRouter::Reset() {
  if (drivefs::DriveFsHost* const host = GetHost()) {
    host->set_dialog_handler({});
  }
  drivefs::DriveFsHost::Observer::Reset();
  drive::DriveIntegrationService::Observer::Reset();
}

void DriveFsEventRouter::OnDriveIntegrationServiceDestroyed() {
  Reset();
}

void DriveFsEventRouter::OnBulkPinProgress(
    const drivefs::pinning::Progress& progress) {
  BroadcastEvent(extensions::events::FILE_MANAGER_PRIVATE_ON_BULK_PIN_PROGRESS,
                 file_manager_private::OnBulkPinProgress::kEventName,
                 file_manager_private::OnBulkPinProgress::Create(
                     util::BulkPinProgressToJs(progress)));
}

void DriveFsEventRouter::ClearStaleSyncStates() {
  const base::Time now = base::Time::Now();
  for (auto it = path_to_sync_state_.cbegin();
       it != path_to_sync_state_.cend();) {
    const base::Time& last_updated = it->second.last_updated;
    if (now - last_updated > kSyncStateStaleThreshold) {
      it = path_to_sync_state_.erase(it);
    } else {
      ++it;
    }
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

drivefs::SyncState DriveFsEventRouter::GetDriveSyncStateForPath(
    const base::FilePath& drive_path) {
  const auto it = path_to_sync_state_.find(drive_path.AsUTF8Unsafe());
  if (it == path_to_sync_state_.end()) {
    return drivefs::SyncState::CreateNotFound(drive_path);
  }
  return it->second;
}

}  // namespace file_manager
