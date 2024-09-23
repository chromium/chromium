// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"

#include <stdint.h>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/folder_creator.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/tracker_id_set.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/drive/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

bool CompareOnCTime(const FileTracker& left,
                    const FileTracker& right) {
  return left.synced_details().creation_time() <
      right.synced_details().creation_time();
}

}  // namespace

RegisterAppTask::RegisterAppTask(SyncEngineContext* sync_context,
                                 const std::string& app_id)
    : sync_context_(sync_context),
      create_folder_retry_count_(0),
      app_id_(app_id) {}

RegisterAppTask::~RegisterAppTask() {
}

bool RegisterAppTask::CanFinishImmediately() {
  return metadata_database() &&
         metadata_database()->FindAppRootTracker(app_id_, nullptr);
}

void RegisterAppTask::RunExclusive(SyncStatusCallback callback) {
  if (create_folder_retry_count_++ >= kMaxRetry) {
    std::move(callback).Run(SYNC_STATUS_FAILED);
    return;
  }

  if (!metadata_database() || !drive_service()) {
    std::move(callback).Run(SYNC_STATUS_FAILED);
    return;
  }

  int64_t sync_root = metadata_database()->GetSyncRootTrackerID();
  TrackerIDSet trackers;
  if (!metadata_database()->FindTrackersByParentAndTitle(
          sync_root, app_id_, &trackers)) {
    CreateAppRootFolder(std::move(callback));
    return;
  }

  FileTracker candidate;
  if (!FilterCandidates(trackers, &candidate)) {
    CreateAppRootFolder(std::move(callback));
    return;
  }

  if (candidate.active()) {
    DCHECK(candidate.tracker_kind() == TRACKER_KIND_APP_ROOT ||
           candidate.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT);
    std::move(callback).Run(SYNC_STATUS_OK);
    return;
  }

  RegisterAppIntoDatabase(candidate, std::move(callback));
}

void RegisterAppTask::CreateAppRootFolder(SyncStatusCallback callback) {
  int64_t sync_root_tracker_id = metadata_database()->GetSyncRootTrackerID();
  FileTracker sync_root_tracker;
  bool should_success = metadata_database()->FindTrackerByTrackerID(
      sync_root_tracker_id,
      &sync_root_tracker);
  DCHECK(should_success);

  DCHECK(!folder_creator_);
  folder_creator_ =
      std::make_unique<FolderCreator>(drive_service(), metadata_database(),
                                      sync_root_tracker.file_id(), app_id_);
  folder_creator_->Run(base::BindOnce(&RegisterAppTask::DidCreateAppRootFolder,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      std::move(callback)));
}

void RegisterAppTask::DidCreateAppRootFolder(SyncStatusCallback callback,
                                             const std::string& folder_id,
                                             SyncStatusCode status) {
  std::unique_ptr<FolderCreator> deleter = std::move(folder_creator_);
  if (status != SYNC_STATUS_OK) {
    std::move(callback).Run(status);
    return;
  }

  RunExclusive(std::move(callback));
}

bool RegisterAppTask::FilterCandidates(const TrackerIDSet& trackers,
                                       FileTracker* candidate) {
  DCHECK(candidate);
  if (trackers.has_active()) {
    if (metadata_database()->FindTrackerByTrackerID(
            trackers.active_tracker(), candidate)) {
      return true;
    }
    NOTREACHED_IN_MIGRATION();
  }

  std::unique_ptr<FileTracker> oldest_tracker;
  for (auto itr = trackers.begin(); itr != trackers.end(); ++itr) {
    std::unique_ptr<FileTracker> tracker(new FileTracker);
    if (!metadata_database()->FindTrackerByTrackerID(
            *itr, tracker.get())) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    FileMetadata file;
    DCHECK(!tracker->active());
    DCHECK(tracker->has_synced_details());

    if (!metadata_database()->FindFileByFileID(tracker->file_id(), &file)) {
      NOTREACHED_IN_MIGRATION();
      // The parent folder is sync-root, whose contents are fetched in
      // initialization sequence.
      // So at this point, direct children of sync-root should have
      // FileMetadata.
      continue;
    }

    if (file.details().file_kind() != FILE_KIND_FOLDER)
      continue;

    if (file.details().missing())
      continue;

    if (oldest_tracker && CompareOnCTime(*oldest_tracker, *tracker))
      continue;

    oldest_tracker = std::move(tracker);
  }

  if (!oldest_tracker)
    return false;

  *candidate = *oldest_tracker;
  return true;
}

void RegisterAppTask::RegisterAppIntoDatabase(const FileTracker& tracker,
                                              SyncStatusCallback callback) {
  SyncStatusCode status =
      metadata_database()->RegisterApp(app_id_, tracker.file_id());
  std::move(callback).Run(status);
}

MetadataDatabase* RegisterAppTask::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

drive::DriveServiceInterface* RegisterAppTask::drive_service() {
  return sync_context_->GetDriveService();
}

}  // namespace drive_backend
}  // namespace sync_file_system
