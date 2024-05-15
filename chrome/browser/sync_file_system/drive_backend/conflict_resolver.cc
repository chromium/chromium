// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"

#include <stdint.h>
#include <utility>

#include "base/check_op.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/drive/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

ConflictResolver::ConflictResolver(SyncEngineContext* sync_context)
    : sync_context_(sync_context) {}

ConflictResolver::~ConflictResolver() {}

void ConflictResolver::RunPreflight(std::unique_ptr<SyncTaskToken> token) {
  token->InitializeTaskLog("Conflict Resolution");

  std::unique_ptr<TaskBlocker> task_blocker(new TaskBlocker);
  task_blocker->exclusive = true;
  SyncTaskManager::UpdateTaskBlocker(
      std::move(token), std::move(task_blocker),
      base::BindOnce(&ConflictResolver::RunExclusive,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ConflictResolver::RunExclusive(std::unique_ptr<SyncTaskToken> token) {
  if (!IsContextReady()) {
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  // Conflict resolution should be invoked on clean tree.
  if (metadata_database()->HasDirtyTracker()) {
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_RETRY);
    return;
  }

  TrackerIDSet trackers;
  if (metadata_database()->GetMultiParentFileTrackers(&target_file_id_,
                                                      &trackers)) {
    DCHECK_LT(1u, trackers.size());
    if (!trackers.has_active()) {
      NOTREACHED_IN_MIGRATION();
      SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
      return;
    }

    token->RecordLog(base::StringPrintf(
        "Detected multi-parent trackers (active tracker_id=%" PRId64 ")",
        trackers.active_tracker()));

    DCHECK(trackers.has_active());
    for (auto itr = trackers.begin(); itr != trackers.end(); ++itr) {
      FileTracker tracker;
      if (!metadata_database()->FindTrackerByTrackerID(*itr, &tracker)) {
        NOTREACHED_IN_MIGRATION();
        continue;
      }

      if (tracker.active())
        continue;

      FileTracker parent_tracker;
      bool should_success = metadata_database()->FindTrackerByTrackerID(
          tracker.parent_tracker_id(), &parent_tracker);
      if (!should_success) {
        NOTREACHED_IN_MIGRATION();
        SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
        return;
      }
      parents_to_remove_.push_back(parent_tracker.file_id());
    }
    DetachFromNonPrimaryParents(std::move(token));
    return;
  }

  if (metadata_database()->GetConflictingTrackers(&trackers)) {
    target_file_id_ = PickPrimaryFile(trackers);
    DCHECK(!target_file_id_.empty());
    int64_t primary_tracker_id = -1;
    for (auto itr = trackers.begin(); itr != trackers.end(); ++itr) {
      FileTracker tracker;
      if (!metadata_database()->FindTrackerByTrackerID(*itr, &tracker)) {
        NOTREACHED_IN_MIGRATION();
        continue;
      }
      if (tracker.file_id() != target_file_id_) {
        non_primary_file_ids_.push_back(
            std::make_pair(tracker.file_id(), tracker.synced_details().etag()));
      } else {
        primary_tracker_id = tracker.tracker_id();
      }
    }

    token->RecordLog(
        base::StringPrintf("Detected %" PRIuS " conflicting trackers "
                           "(primary tracker_id=%" PRId64 ")",
                           non_primary_file_ids_.size(), primary_tracker_id));

    RemoveNonPrimaryFiles(std::move(token));
    return;
  }

  SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_NO_CONFLICT);
}

void ConflictResolver::DetachFromNonPrimaryParents(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(!parents_to_remove_.empty());

  // TODO(tzik): Check if ETag match is available for
  // RemoteResourceFromDirectory.
  std::string parent_folder_id = parents_to_remove_.back();
  parents_to_remove_.pop_back();

  token->RecordLog(base::StringPrintf(
      "Detach %s from %s", target_file_id_.c_str(), parent_folder_id.c_str()));

  drive_service()->RemoveResourceFromDirectory(
      parent_folder_id, target_file_id_,
      base::BindOnce(&ConflictResolver::DidDetachFromParent,
                     weak_ptr_factory_.GetWeakPtr(), std::move(token)));
}

void ConflictResolver::DidDetachFromParent(std::unique_ptr<SyncTaskToken> token,
                                           google_apis::ApiErrorCode error) {
  SyncStatusCode status = ApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  if (!parents_to_remove_.empty()) {
    DetachFromNonPrimaryParents(std::move(token));
    return;
  }

  SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_OK);
}

std::string ConflictResolver::PickPrimaryFile(const TrackerIDSet& trackers) {
  std::unique_ptr<FileMetadata> primary;
  for (auto itr = trackers.begin(); itr != trackers.end(); ++itr) {
    FileTracker tracker;
    if (!metadata_database()->FindTrackerByTrackerID(*itr, &tracker)) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    std::unique_ptr<FileMetadata> file_metadata(new FileMetadata);
    if (!metadata_database()->FindFileByFileID(tracker.file_id(),
                                               file_metadata.get())) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    if (!primary) {
      primary = std::move(file_metadata);
      continue;
    }

    DCHECK(primary->details().file_kind() == FILE_KIND_FILE ||
           primary->details().file_kind() == FILE_KIND_FOLDER);
    DCHECK(file_metadata->details().file_kind() == FILE_KIND_FILE ||
           file_metadata->details().file_kind() == FILE_KIND_FOLDER);

    if (primary->details().file_kind() == FILE_KIND_FILE) {
      if (file_metadata->details().file_kind() == FILE_KIND_FOLDER) {
        // Prioritize folders over regular files.
        primary = std::move(file_metadata);
        continue;
      }

      DCHECK(file_metadata->details().file_kind() == FILE_KIND_FILE);
      if (primary->details().modification_time() <
          file_metadata->details().modification_time()) {
        // Prioritize last write for regular files.
        primary = std::move(file_metadata);
        continue;
      }

      continue;
    }

    DCHECK(primary->details().file_kind() == FILE_KIND_FOLDER);
    if (file_metadata->details().file_kind() == FILE_KIND_FILE) {
      // Prioritize folders over regular files.
      continue;
    }

    DCHECK(file_metadata->details().file_kind() == FILE_KIND_FOLDER);
    if (primary->details().creation_time() >
        file_metadata->details().creation_time()) {
      // Prioritize first create for folders.
      primary = std::move(file_metadata);
      continue;
    }
  }

  if (primary)
    return primary->file_id();
  return std::string();
}

void ConflictResolver::RemoveNonPrimaryFiles(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(!non_primary_file_ids_.empty());

  std::string file_id = non_primary_file_ids_.back().first;
  std::string etag = non_primary_file_ids_.back().second;
  non_primary_file_ids_.pop_back();

  DCHECK_NE(target_file_id_, file_id);

  token->RecordLog(
      base::StringPrintf("Remove non-primary file %s", file_id.c_str()));

  // TODO(tzik): Check if the file is a folder, and merge its contents into
  // the folder identified by |target_file_id_|.
  drive_service()->DeleteResource(
      file_id, etag,
      base::BindOnce(&ConflictResolver::DidRemoveFile,
                     weak_ptr_factory_.GetWeakPtr(), std::move(token),
                     file_id));
}

void ConflictResolver::DidRemoveFile(std::unique_ptr<SyncTaskToken> token,
                                     const std::string& file_id,
                                     google_apis::ApiErrorCode error) {
  if (error == google_apis::HTTP_PRECONDITION ||
      error == google_apis::HTTP_CONFLICT) {
    UpdateFileMetadata(file_id, std::move(token));
    return;
  }

  SyncStatusCode status = ApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK && error != google_apis::HTTP_NOT_FOUND) {
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  deleted_file_ids_.push_back(file_id);
  if (!non_primary_file_ids_.empty()) {
    RemoveNonPrimaryFiles(std::move(token));
    return;
  }

  status =
      metadata_database()->UpdateByDeletedRemoteFileList(deleted_file_ids_);
  SyncTaskManager::NotifyTaskDone(std::move(token), status);
}

bool ConflictResolver::IsContextReady() {
  return sync_context_->GetDriveService() &&
         sync_context_->GetMetadataDatabase();
}

void ConflictResolver::UpdateFileMetadata(
    const std::string& file_id,
    std::unique_ptr<SyncTaskToken> token) {
  drive_service()->GetFileResource(
      file_id, base::BindOnce(&ConflictResolver::DidGetRemoteMetadata,
                              weak_ptr_factory_.GetWeakPtr(), file_id,
                              std::move(token)));
}

void ConflictResolver::DidGetRemoteMetadata(
    const std::string& file_id,
    std::unique_ptr<SyncTaskToken> token,
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::FileResource> entry) {
  SyncStatusCode status = ApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK && error != google_apis::HTTP_NOT_FOUND) {
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  if (error != google_apis::HTTP_NOT_FOUND) {
    status = metadata_database()->UpdateByDeletedRemoteFile(file_id);
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  if (!entry) {
    NOTREACHED_IN_MIGRATION();
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  status = metadata_database()->UpdateByFileResource(*entry);
  SyncTaskManager::NotifyTaskDone(std::move(token), status);
}

drive::DriveServiceInterface* ConflictResolver::drive_service() {
  set_used_network(true);
  return sync_context_->GetDriveService();
}

MetadataDatabase* ConflictResolver::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

}  // namespace drive_backend
}  // namespace sync_file_system
