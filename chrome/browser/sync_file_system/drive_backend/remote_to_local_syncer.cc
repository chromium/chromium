// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"

#include <stdint.h>

#include <limits>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/drive_backend/task_dependency_manager.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/service/drive_service_interface.h"
#include "extensions/common/extension.h"
#include "google_apis/drive/drive_api_parser.h"
#include "storage/common/file_system/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

bool BuildFileSystemURL(MetadataDatabase* metadata_database,
                        const FileTracker& tracker,
                        storage::FileSystemURL* url) {
  base::FilePath path;
  if (!metadata_database->BuildPathForTracker(
          tracker.tracker_id(), &path))
    return false;

  GURL origin =
      extensions::Extension::GetBaseURLFromExtensionId(tracker.app_id());
  *url = sync_file_system::CreateSyncableFileSystemURL(origin, path);

  return true;
}

bool HasFolderAsParent(const FileDetails& details,
                       const std::string& folder_id) {
  for (int i = 0; i < details.parent_folder_ids_size(); ++i) {
    if (details.parent_folder_ids(i) == folder_id)
      return true;
  }
  return false;
}

bool HasDisabledAppRoot(MetadataDatabase* database,
                        const FileTracker& tracker) {
  DCHECK(tracker.active());
  FileTracker app_root_tracker;
  if (database->FindAppRootTracker(tracker.app_id(), &app_root_tracker)) {
    DCHECK(app_root_tracker.tracker_kind() == TRACKER_KIND_APP_ROOT ||
           app_root_tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT);
    return app_root_tracker.tracker_kind() == TRACKER_KIND_DISABLED_APP_ROOT;
  }
  return false;
}

std::unique_ptr<FileMetadata> GetFileMetadata(MetadataDatabase* database,
                                              const std::string& file_id) {
  std::unique_ptr<FileMetadata> metadata(new FileMetadata);
  if (!database->FindFileByFileID(file_id, metadata.get()))
    metadata.reset();
  return metadata;
}

// Creates a temporary file in |dir_path|.  This must be called on an
// IO-allowed task runner, and the runner must be given as |file_task_runner|.
storage::ScopedFile CreateTemporaryFile(
    const scoped_refptr<base::TaskRunner>& file_task_runner) {
  base::FilePath temp_file_path;
  if (!base::CreateTemporaryFile(&temp_file_path))
    return storage::ScopedFile();

  return storage::ScopedFile(temp_file_path,
                             storage::ScopedFile::DELETE_ON_SCOPE_OUT,
                             file_task_runner);
}

}  // namespace

RemoteToLocalSyncer::RemoteToLocalSyncer(SyncEngineContext* sync_context)
    : sync_context_(sync_context),
      file_type_(SYNC_FILE_TYPE_UNKNOWN),
      sync_action_(SYNC_ACTION_NONE),
      prepared_(false),
      sync_root_deletion_(false) {}

RemoteToLocalSyncer::~RemoteToLocalSyncer() {
}

void RemoteToLocalSyncer::RunPreflight(std::unique_ptr<SyncTaskToken> token) {
  token->InitializeTaskLog("Remote -> Local");

  if (!drive_service() || !metadata_database() || !remote_change_processor()) {
    token->RecordLog("Context not ready.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  dirty_tracker_ = base::WrapUnique(new FileTracker);
  if (metadata_database()->GetDirtyTracker(dirty_tracker_.get())) {
    token->RecordLog(base::StringPrintf(
        "Start: tracker_id=%" PRId64, dirty_tracker_->tracker_id()));
    metadata_database()->DemoteTracker(dirty_tracker_->tracker_id());
    ResolveRemoteChange(std::move(token));
    return;
  }

  token->RecordLog("Nothing to do.");
  SyncTaskManager::NotifyTaskDone(std::move(token),
                                  SYNC_STATUS_NO_CHANGE_TO_SYNC);
}

void RemoteToLocalSyncer::ResolveRemoteChange(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);
  remote_metadata_ = GetFileMetadata(
      metadata_database(), dirty_tracker_->file_id());

  if (!remote_metadata_ || !remote_metadata_->has_details()) {
    if (remote_metadata_ && !remote_metadata_->has_details()) {
      token->RecordLog(
          "Missing details of a remote file: " + remote_metadata_->file_id());
      NOTREACHED();
    }
    token->RecordLog("Missing remote metadata case.");

    MoveToBackground(
        std::move(token),
        base::Bind(&RemoteToLocalSyncer::HandleMissingRemoteMetadata,
                   weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  const FileDetails& remote_details = remote_metadata_->details();

  if (!dirty_tracker_->active() ||
      HasDisabledAppRoot(metadata_database(), *dirty_tracker_)) {
    // Handle inactive tracker in SyncCompleted.
    token->RecordLog("Inactive tracker case.");
    SyncCompleted(std::move(token), SYNC_STATUS_OK);
    return;
  }

  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));

  if (!dirty_tracker_->has_synced_details()) {
    token->RecordLog(base::StringPrintf(
        "Missing synced_details of an active tracker: %" PRId64,
        dirty_tracker_->tracker_id()));
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  DCHECK(dirty_tracker_->has_synced_details());
  const FileDetails& synced_details = dirty_tracker_->synced_details();

  if (dirty_tracker_->tracker_id() ==
      metadata_database()->GetSyncRootTrackerID()) {
    if (remote_details.missing() ||
        synced_details.title() != remote_details.title() ||
        remote_details.parent_folder_ids_size()) {
      token->RecordLog("Sync-root deletion.");
      sync_root_deletion_ = true;
      SyncCompleted(std::move(token), SYNC_STATUS_OK);
      return;
    }
    token->RecordLog("Trivial sync-root change.");
    SyncCompleted(std::move(token), SYNC_STATUS_OK);
    return;
  }

  DCHECK_NE(dirty_tracker_->tracker_id(),
            metadata_database()->GetSyncRootTrackerID());

  if (!BuildFileSystemURL(metadata_database(), *dirty_tracker_, &url_)) {
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  DCHECK(url_.is_valid());

  if (remote_details.missing()) {
    if (!synced_details.missing()) {
      token->RecordLog("Remote file deletion.");
      MoveToBackground(std::move(token),
                       base::Bind(&RemoteToLocalSyncer::HandleDeletion,
                                  weak_ptr_factory_.GetWeakPtr()));
      return;
    }

    DCHECK(synced_details.missing());
    token->RecordLog("Found a stray missing tracker: " +
                     dirty_tracker_->file_id());
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_OK);
    return;
  }

  // Most of remote_details field is valid from here.
  DCHECK(!remote_details.missing());

  if (synced_details.file_kind() != remote_details.file_kind()) {
    token->RecordLog(base::StringPrintf(
        "Found type mismatch between remote and local file: %s"
        " type: (local) %d vs (remote) %d",
        dirty_tracker_->file_id().c_str(),
        synced_details.file_kind(),
        remote_details.file_kind()));
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }
  DCHECK_EQ(synced_details.file_kind(), remote_details.file_kind());

  if (synced_details.file_kind() == FILE_KIND_UNSUPPORTED) {
    token->RecordLog("Found an unsupported active file: " +
                     remote_metadata_->file_id());
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }
  DCHECK(remote_details.file_kind() == FILE_KIND_FILE ||
         remote_details.file_kind() == FILE_KIND_FOLDER);

  if (synced_details.title() != remote_details.title()) {
    // Handle rename as deletion + addition.
    token->RecordLog("Detected file rename.");

    MoveToBackground(std::move(token),
                     base::Bind(&RemoteToLocalSyncer::HandleFileMove,
                                weak_ptr_factory_.GetWeakPtr()));
    return;
  }
  DCHECK_EQ(synced_details.title(), remote_details.title());

  FileTracker parent_tracker;
  if (!metadata_database()->FindTrackerByTrackerID(
          dirty_tracker_->parent_tracker_id(), &parent_tracker)) {
    token->RecordLog("Missing parent tracker for a non sync-root tracker: "
                     + dirty_tracker_->file_id());
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  if (!HasFolderAsParent(remote_details, parent_tracker.file_id())) {
    // Handle reorganize as deletion + addition.
    token->RecordLog("Detected file reorganize.");

    MoveToBackground(std::move(token),
                     base::Bind(&RemoteToLocalSyncer::HandleFileMove,
                                weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (synced_details.file_kind() == FILE_KIND_FILE) {
    if (synced_details.md5() != remote_details.md5()) {
      token->RecordLog("Detected file content update.");
      MoveToBackground(std::move(token),
                       base::Bind(&RemoteToLocalSyncer::HandleContentUpdate,
                                  weak_ptr_factory_.GetWeakPtr()));
      return;
    }
  } else {
    DCHECK_EQ(FILE_KIND_FOLDER, synced_details.file_kind());
    if (synced_details.missing()) {
      token->RecordLog("Detected folder update.");
      MoveToBackground(std::move(token),
                       base::Bind(&RemoteToLocalSyncer::HandleFolderUpdate,
                                  weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    if (dirty_tracker_->needs_folder_listing()) {
      token->RecordLog("Needs listing folder.");
      MoveToBackground(std::move(token),
                       base::Bind(&RemoteToLocalSyncer::ListFolderContent,
                                  weak_ptr_factory_.GetWeakPtr()));
      return;
    }
    SyncCompleted(std::move(token), SYNC_STATUS_OK);
    return;
  }

  token->RecordLog("Trivial file change.");
  SyncCompleted(std::move(token), SYNC_STATUS_OK);
}

void RemoteToLocalSyncer::MoveToBackground(std::unique_ptr<SyncTaskToken> token,
                                           const Continuation& continuation) {
  DCHECK(dirty_tracker_);

  std::unique_ptr<TaskBlocker> blocker(new TaskBlocker);
  blocker->app_id = dirty_tracker_->app_id();
  if (url_.is_valid())
    blocker->paths.push_back(url_.path());
  blocker->file_ids.push_back(dirty_tracker_->file_id());
  blocker->tracker_ids.push_back(dirty_tracker_->tracker_id());

  SyncTaskManager::UpdateTaskBlocker(
      std::move(token), std::move(blocker),
      base::Bind(&RemoteToLocalSyncer::ContinueAsBackgroundTask,
                 weak_ptr_factory_.GetWeakPtr(), continuation));
}

void RemoteToLocalSyncer::ContinueAsBackgroundTask(
    const Continuation& continuation,
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);

  // The SyncTask runs as a background task beyond this point.
  // Not that any task can run between MoveToBackground() and
  // ContinueAsBackgroundTask(), so we need to make sure other tasks didn't
  // affect to the current RemoteToLocalSyncer task.
  //
  // - For LocalToRemoteSyncer, it may update or delete any of FileTracker and
  //   FileMetadata. When it updates FileMetadata or FileDetails in FileTracker,
  //   it also updates |change_id|.  So, ensure the target FileTracker and
  //   FileMetadata exist and their |change_id|s are not updated.
  // - For ListChangesTask, it may update FileMetadata together with |change_id|
  //   and may delete FileTracker.
  // - For UninstallAppTask, it may delete FileMetadata and FileTracker.
  //   Check if FileTracker still exists.
  // - For other RemoteToLocalSyncer, it may delete the FileTracker of parent.
  //   Note that since RemoteToLocalSyncer demotes the target FileTracker first,
  //   any other RemoteToLocalSyncer does not run for current |dirty_tracker_|.
  // - Others, SyncEngineInitializer and RegisterAppTask doesn't affect to

  FileTracker latest_dirty_tracker;
  if (!metadata_database()->FindTrackerByTrackerID(
          dirty_tracker_->tracker_id(), &latest_dirty_tracker) ||
      dirty_tracker_->active() != latest_dirty_tracker.active() ||
      !latest_dirty_tracker.dirty()) {
    SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
    return;
  }

  int64_t current_change_id = std::numeric_limits<int64_t>::min();
  int64_t latest_change_id = std::numeric_limits<int64_t>::min();
  if (dirty_tracker_->has_synced_details())
    current_change_id = dirty_tracker_->synced_details().change_id();
  if (latest_dirty_tracker.has_synced_details())
    latest_change_id = latest_dirty_tracker.synced_details().change_id();
  if (current_change_id != latest_change_id) {
    SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
    return;
  }

  FileMetadata latest_file_metadata;
  if (metadata_database()->FindFileByFileID(dirty_tracker_->file_id(),
                                            &latest_file_metadata)) {
    if (!remote_metadata_) {
      SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
      return;
    }

    int64_t change_id = remote_metadata_->details().change_id();
    int64_t latest_change_id = latest_file_metadata.details().change_id();
    if (change_id != latest_change_id) {
      SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
      return;
    }
  } else {
    if (remote_metadata_) {
      SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
      return;
    }
  }
  continuation.Run(std::move(token));
}

void RemoteToLocalSyncer::HandleMissingRemoteMetadata(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);

  drive_service()->GetFileResource(
      dirty_tracker_->file_id(),
      base::Bind(&RemoteToLocalSyncer::DidGetRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token)));
}

void RemoteToLocalSyncer::DidGetRemoteMetadata(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::FileResource> entry) {
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksInCurrentSequence());

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK &&
      error != google_apis::HTTP_NOT_FOUND) {
    SyncCompleted(std::move(token), status);
    return;
  }

  if (error == google_apis::HTTP_NOT_FOUND) {
    status = metadata_database()->UpdateByDeletedRemoteFile(
        dirty_tracker_->file_id());
    SyncCompleted(std::move(token), status);
    return;
  }

  if (!entry) {
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  status = metadata_database()->UpdateByFileResource(*entry);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  metadata_database()->PromoteDemotedTracker(dirty_tracker_->tracker_id());

  // Do not update |dirty_tracker_|.
  SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
}

void RemoteToLocalSyncer::DidPrepareForAddOrUpdateFile(
    std::unique_ptr<SyncTaskToken> token,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  DCHECK(url_.is_valid());
  DCHECK(local_metadata_);
  DCHECK(local_changes_);

  // Check if the local file exists.
  if (local_metadata_->file_type == SYNC_FILE_TYPE_UNKNOWN ||
      (!local_changes_->empty() && local_changes_->back().IsDelete())) {
    file_type_ = SYNC_FILE_TYPE_FILE;
    sync_action_ = SYNC_ACTION_ADDED;
    // Missing local file case.
    // Download the file and add it to local as a new file.
    DownloadFile(std::move(token));
    return;
  }

  DCHECK(local_changes_->empty() || local_changes_->back().IsAddOrUpdate());
  if (local_changes_->empty()) {
    if (local_metadata_->file_type == SYNC_FILE_TYPE_FILE) {
      file_type_ = SYNC_FILE_TYPE_FILE;
      sync_action_ = SYNC_ACTION_UPDATED;
      // Download the file and overwrite the existing local file.
      DownloadFile(std::move(token));
      return;
    }

    DCHECK_EQ(SYNC_FILE_TYPE_DIRECTORY, local_metadata_->file_type);

    // Got a remote regular file modification for existing local folder.
    // Our policy prioritize folders in this case.
    // Let local-to-remote sync phase process this change.
    remote_change_processor()->RecordFakeLocalChange(
        url_, FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                         local_metadata_->file_type),
        SyncCompletedCallback(std::move(token)));
    return;
  }

  DCHECK(local_changes_->back().IsAddOrUpdate());
  // Conflict case.
  // Do nothing for the change now, and handle this in LocalToRemoteSync phase.
  SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
}

void RemoteToLocalSyncer::HandleFolderUpdate(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().missing());
  DCHECK_EQ(FILE_KIND_FOLDER, remote_metadata_->details().file_kind());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForFolderUpdate,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&token)));
}

void RemoteToLocalSyncer::DidPrepareForFolderUpdate(
    std::unique_ptr<SyncTaskToken> token,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  DCHECK(url_.is_valid());
  DCHECK(local_metadata_);
  DCHECK(local_changes_);

  // Check if the local file exists.
  if (local_metadata_->file_type == SYNC_FILE_TYPE_UNKNOWN ||
      (!local_changes_->empty() && local_changes_->back().IsDelete())) {
    file_type_ = SYNC_FILE_TYPE_DIRECTORY;
    sync_action_ = SYNC_ACTION_ADDED;
    // No local file exists at the path.
    CreateFolder(std::move(token));
    return;
  }

  if (local_metadata_->file_type == SYNC_FILE_TYPE_DIRECTORY) {
    // There already exists a folder, nothing left to do.
    if (dirty_tracker_->needs_folder_listing() &&
        !dirty_tracker_->synced_details().missing()) {
      ListFolderContent(std::move(token));
    } else {
      SyncCompleted(std::move(token), SYNC_STATUS_OK);
    }
    return;
  }

  DCHECK_EQ(SYNC_FILE_TYPE_FILE, local_metadata_->file_type);
  file_type_ = SYNC_FILE_TYPE_DIRECTORY;
  sync_action_ = SYNC_ACTION_ADDED;
  // Got a remote folder for existing local file.
  // Our policy prioritize folders in this case.
  CreateFolder(std::move(token));
}

void RemoteToLocalSyncer::HandleDeletion(std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().missing());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(remote_metadata_->details().missing());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForDeletion,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&token)));
}

void RemoteToLocalSyncer::HandleFileMove(std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().missing());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForDeletion,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&token)));
}

void RemoteToLocalSyncer::DidPrepareForDeletion(
    std::unique_ptr<SyncTaskToken> token,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  DCHECK(url_.is_valid());
  DCHECK(local_metadata_);
  DCHECK(local_changes_);

  // Check if the local file exists.
  if (local_metadata_->file_type == SYNC_FILE_TYPE_UNKNOWN ||
      (!local_changes_->empty() && local_changes_->back().IsDelete())) {
    // No local file exists at the path.
    SyncCompleted(std::move(token), SYNC_STATUS_OK);
    return;
  }

  DCHECK(local_changes_->empty() || local_changes_->back().IsAddOrUpdate());
  if (local_changes_->empty()) {
    file_type_ = local_metadata_->file_type;
    sync_action_ = SYNC_ACTION_DELETED;
    DeleteLocalFile(std::move(token));
    return;
  }

  DCHECK(local_changes_->back().IsAddOrUpdate());
  // File is remotely deleted and locally updated.
  // Ignore the remote deletion and handle it as if applied successfully.
  SyncCompleted(std::move(token), SYNC_STATUS_OK);
}

void RemoteToLocalSyncer::HandleContentUpdate(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK_EQ(FILE_KIND_FILE, dirty_tracker_->synced_details().file_kind());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().missing());

  DCHECK_NE(dirty_tracker_->synced_details().md5(),
            remote_metadata_->details().md5());

  Prepare(base::Bind(&RemoteToLocalSyncer::DidPrepareForAddOrUpdateFile,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
}

void RemoteToLocalSyncer::ListFolderContent(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(dirty_tracker_);
  DCHECK(dirty_tracker_->active());
  DCHECK(!HasDisabledAppRoot(metadata_database(), *dirty_tracker_));
  DCHECK(dirty_tracker_->has_synced_details());
  DCHECK(!dirty_tracker_->synced_details().missing());
  DCHECK_EQ(FILE_KIND_FOLDER, dirty_tracker_->synced_details().file_kind());
  DCHECK(dirty_tracker_->needs_folder_listing());

  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());
  DCHECK(!remote_metadata_->details().missing());

  // TODO(tzik): Replace this call with ChildList version.
  drive_service()->GetFileListInDirectory(
      dirty_tracker_->file_id(),
      base::Bind(&RemoteToLocalSyncer::DidListFolderContent,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token),
                 base::Passed(base::WrapUnique(new FileIDList))));
}

void RemoteToLocalSyncer::DidListFolderContent(
    std::unique_ptr<SyncTaskToken> token,
    std::unique_ptr<FileIDList> children,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::FileList> file_list) {
  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  if (!file_list) {
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  children->reserve(children->size() + file_list->items().size());
  for (const auto& file_resource : file_list->items()) {
    children->push_back(file_resource->file_id());
  }

  if (!file_list->next_link().is_empty()) {
    drive_service()->GetRemainingFileList(
        file_list->next_link(),
        base::Bind(&RemoteToLocalSyncer::DidListFolderContent,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&token), base::Passed(&children)));
    return;
  }

  status = metadata_database()->PopulateFolderByChildList(
      dirty_tracker_->file_id(), *children);
  SyncCompleted(std::move(token), status);
}

void RemoteToLocalSyncer::SyncCompleted(std::unique_ptr<SyncTaskToken> token,
                                        SyncStatusCode status) {
  token->RecordLog(base::StringPrintf(
      "[Remote -> Local]: Finished: action=%s, tracker=%" PRId64 " status=%s",
      SyncActionToString(sync_action_), dirty_tracker_->tracker_id(),
      SyncStatusCodeToString(status)));

  if (sync_root_deletion_) {
    FinalizeSync(std::move(token), SYNC_STATUS_OK);
    return;
  }

  if (status == SYNC_STATUS_RETRY) {
    FinalizeSync(std::move(token), SYNC_STATUS_OK);
    return;
  }

  if (status != SYNC_STATUS_OK) {
    FinalizeSync(std::move(token), status);
    return;
  }

  DCHECK(dirty_tracker_);
  DCHECK(remote_metadata_);
  DCHECK(remote_metadata_->has_details());

  FileDetails updated_details = remote_metadata_->details();
  if (!dirty_tracker_->active() ||
      HasDisabledAppRoot(metadata_database(), *dirty_tracker_)) {
    // Operations for an inactive tracker don't update file content.
    if (dirty_tracker_->has_synced_details())
      updated_details.set_md5(dirty_tracker_->synced_details().md5());
    if (!dirty_tracker_->active()) {
      // Keep missing true, as the change hasn't been synced to local.
      updated_details.clear_md5();
      updated_details.set_missing(true);
    }
  }

  status = metadata_database()->UpdateTracker(
      dirty_tracker_->tracker_id(), updated_details);
  FinalizeSync(std::move(token), status);
}

void RemoteToLocalSyncer::FinalizeSync(std::unique_ptr<SyncTaskToken> token,
                                       SyncStatusCode status) {
  if (prepared_) {
    remote_change_processor()->FinalizeRemoteSync(
        url_, false /* clear_local_change */,
        base::Bind(SyncTaskManager::NotifyTaskDone,
                   base::Passed(&token), status));
    return;
  }

  SyncTaskManager::NotifyTaskDone(std::move(token), status);
}

void RemoteToLocalSyncer::Prepare(const SyncStatusCallback& callback) {
  DCHECK(url_.is_valid());
  remote_change_processor()->PrepareForProcessRemoteChange(
      url_,
      base::Bind(&RemoteToLocalSyncer::DidPrepare,
                 weak_ptr_factory_.GetWeakPtr(),
                 callback));
}

void RemoteToLocalSyncer::DidPrepare(const SyncStatusCallback& callback,
                                     SyncStatusCode status,
                                     const SyncFileMetadata& local_metadata,
                                     const FileChangeList& local_changes) {
  if (status != SYNC_STATUS_OK) {
    callback.Run(status);
    return;
  }
  prepared_ = true;

  local_metadata_.reset(new SyncFileMetadata(local_metadata));
  local_changes_.reset(new FileChangeList(local_changes));

  callback.Run(status);
}

void RemoteToLocalSyncer::DeleteLocalFile(
    std::unique_ptr<SyncTaskToken> token) {
  remote_change_processor()->ApplyRemoteChange(
      FileChange(FileChange::FILE_CHANGE_DELETE, SYNC_FILE_TYPE_UNKNOWN),
      base::FilePath(), url_, SyncCompletedCallback(std::move(token)));
}

void RemoteToLocalSyncer::DownloadFile(std::unique_ptr<SyncTaskToken> token) {
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksInCurrentSequence());

  storage::ScopedFile file = CreateTemporaryFile(
      base::WrapRefCounted(sync_context_->GetWorkerTaskRunner()));

  base::FilePath path = file.path();
  drive_service()->DownloadFile(
      path, remote_metadata_->file_id(),
      base::Bind(&RemoteToLocalSyncer::DidDownloadFile,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token), base::Passed(&file)),
      google_apis::GetContentCallback(),
      google_apis::ProgressCallback());
}

void RemoteToLocalSyncer::DidDownloadFile(std::unique_ptr<SyncTaskToken> token,
                                          storage::ScopedFile file,
                                          google_apis::DriveApiErrorCode error,
                                          const base::FilePath&) {
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksInCurrentSequence());

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  base::FilePath path = file.path();
  const std::string md5 = drive::util::GetMd5Digest(path, nullptr);
  if (md5.empty()) {
    SyncCompleted(std::move(token), SYNC_FILE_ERROR_NOT_FOUND);
    return;
  }

  if (md5 != remote_metadata_->details().md5()) {
    // File has been modified since last metadata retrieval.
    SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
    return;
  }

  remote_change_processor()->ApplyRemoteChange(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE, SYNC_FILE_TYPE_FILE),
      path, url_,
      base::Bind(&RemoteToLocalSyncer::DidApplyDownload,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token), base::Passed(&file)));
}

void RemoteToLocalSyncer::DidApplyDownload(std::unique_ptr<SyncTaskToken> token,
                                           storage::ScopedFile,
                                           SyncStatusCode status) {
  SyncCompleted(std::move(token), status);
}

void RemoteToLocalSyncer::CreateFolder(std::unique_ptr<SyncTaskToken> token) {
  remote_change_processor()->ApplyRemoteChange(
      FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                 SYNC_FILE_TYPE_DIRECTORY),
      base::FilePath(), url_, SyncCompletedCallback(std::move(token)));
}

drive::DriveServiceInterface* RemoteToLocalSyncer::drive_service() {
  return sync_context_->GetDriveService();
}

MetadataDatabase* RemoteToLocalSyncer::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

RemoteChangeProcessor* RemoteToLocalSyncer::remote_change_processor() {
  DCHECK(sync_context_->GetRemoteChangeProcessor());
  return sync_context_->GetRemoteChangeProcessor();
}

SyncStatusCallback RemoteToLocalSyncer::SyncCompletedCallback(
    std::unique_ptr<SyncTaskToken> token) {
  return base::Bind(&RemoteToLocalSyncer::SyncCompleted,
                    weak_ptr_factory_.GetWeakPtr(),
                    base::Passed(&token));
}

}  // namespace drive_backend
}  // namespace sync_file_system
