// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"

#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/folder_creator.h"
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
#include "net/base/mime_util.h"
#include "storage/common/file_system/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

std::unique_ptr<FileTracker> FindTrackerByID(
    MetadataDatabase* metadata_database,
    int64_t tracker_id) {
  std::unique_ptr<FileTracker> tracker(new FileTracker);
  if (metadata_database->FindTrackerByTrackerID(tracker_id, tracker.get()))
    return tracker;
  return std::unique_ptr<FileTracker>();
}

bool GetKnownChangeID(MetadataDatabase* metadata_database,
                      const std::string& file_id,
                      int64_t* change_id) {
  FileMetadata remote_file_metadata;
  if (!metadata_database->FindFileByFileID(file_id, &remote_file_metadata))
    return false;
  *change_id = remote_file_metadata.details().change_id();
  return true;
}

bool IsLocalFileMissing(const SyncFileMetadata& local_metadata,
                        const FileChange& local_change) {
  return local_metadata.file_type == SYNC_FILE_TYPE_UNKNOWN ||
         local_change.IsDelete();
}

std::string GetMimeTypeFromTitle(const base::FilePath& title) {
  base::FilePath::StringType extension = title.Extension();
  std::string mime_type;
  if (extension.empty() ||
      !net::GetWellKnownMimeTypeFromExtension(extension.substr(1), &mime_type))
    return kMimeTypeOctetStream;
  return mime_type;
}

}  // namespace

LocalToRemoteSyncer::LocalToRemoteSyncer(SyncEngineContext* sync_context,
                                         const SyncFileMetadata& local_metadata,
                                         const FileChange& local_change,
                                         const base::FilePath& local_path,
                                         const storage::FileSystemURL& url)
    : sync_context_(sync_context),
      local_change_(local_change),
      local_is_missing_(IsLocalFileMissing(local_metadata, local_change)),
      local_path_(local_path),
      url_(url),
      file_type_(SYNC_FILE_TYPE_UNKNOWN),
      sync_action_(SYNC_ACTION_NONE),
      remote_file_change_id_(0),
      retry_on_success_(false),
      needs_remote_change_listing_(false) {
  DCHECK(local_is_missing_ ||
         local_change.file_type() == local_metadata.file_type)
      << local_change.DebugString() << " metadata:" << local_metadata.file_type;
}

LocalToRemoteSyncer::~LocalToRemoteSyncer() {
}

void LocalToRemoteSyncer::RunPreflight(std::unique_ptr<SyncTaskToken> token) {
  token->InitializeTaskLog("Local -> Remote");

  if (!IsContextReady()) {
    token->RecordLog("Context not ready.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  token->RecordLog(base::StringPrintf(
      "Start: %s on %s@%s %s",
      local_change_.DebugString().c_str(),
      url_.path().AsUTF8Unsafe().c_str(),
      url_.origin().host().c_str(),
      local_is_missing_ ? "(missing)" : ""));

  if (local_is_missing_ && !local_change_.IsDelete()) {
    // Stray file, we can just return.
    token->RecordLog("Missing file for non-delete change.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_OK);
    return;
  }

  std::string app_id = url_.origin().host();
  base::FilePath path = url_.path();

  std::unique_ptr<FileTracker> active_ancestor_tracker(new FileTracker);
  base::FilePath active_ancestor_path;
  if (!metadata_database()->FindNearestActiveAncestor(
          app_id, path,
          active_ancestor_tracker.get(), &active_ancestor_path)) {
    // The app is disabled or not registered.
    token->RecordLog("App is disabled or not registered");
    SyncTaskManager::NotifyTaskDone(std::move(token),
                                    SYNC_STATUS_UNKNOWN_ORIGIN);
    return;
  }
  DCHECK(active_ancestor_tracker->active());
  DCHECK(active_ancestor_tracker->has_synced_details());
  const FileDetails& active_ancestor_details =
      active_ancestor_tracker->synced_details();

  // TODO(tzik): Consider handling
  // active_ancestor_tracker->synced_details().missing() case.

  DCHECK(active_ancestor_details.file_kind() == FILE_KIND_FILE ||
         active_ancestor_details.file_kind() == FILE_KIND_FOLDER);

  base::FilePath missing_entries;
  if (active_ancestor_path.empty()) {
    missing_entries = path;
  } else if (active_ancestor_path != path) {
    if (!active_ancestor_path.AppendRelativePath(path, &missing_entries)) {
      NOTREACHED();
      token->RecordLog(
          base::StringPrintf("Detected invalid ancestor: %" PRFilePath,
                             active_ancestor_path.value().c_str()));
      SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
      return;
    }
  }

  std::vector<base::FilePath::StringType> missing_components =
      storage::VirtualPath::GetComponents(missing_entries);

  if (!missing_components.empty()) {
    if (local_is_missing_) {
      token->RecordLog("Both local and remote are marked missing");
      // !IsDelete() but SYNC_FILE_TYPE_UNKNOWN could happen when a file is
      // deleted by recursive deletion (which is not recorded by tracker)
      // but there're remaining changes for the same file in the tracker.

      // Local file is deleted and remote file is missing, already deleted or
      // not yet synced.  There is nothing to do for the file.
      SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_OK);
      return;
    }
  }

  if (missing_components.size() > 1) {
    // The original target doesn't have remote file and parent.
    // Try creating the parent first.
    if (active_ancestor_details.file_kind() == FILE_KIND_FOLDER) {
      remote_parent_folder_tracker_ = std::move(active_ancestor_tracker);
      target_path_ = active_ancestor_path.Append(missing_components[0]);
      token->RecordLog("Detected missing parent folder.");

      retry_on_success_ = true;
      MoveToBackground(base::Bind(&LocalToRemoteSyncer::CreateRemoteFolder,
                                  weak_ptr_factory_.GetWeakPtr()),
                       std::move(token));
      return;
    }

    DCHECK(active_ancestor_details.file_kind() == FILE_KIND_FILE);
    remote_parent_folder_tracker_ =
        FindTrackerByID(metadata_database(),
                        active_ancestor_tracker->parent_tracker_id());
    remote_file_tracker_ = std::move(active_ancestor_tracker);
    target_path_ = active_ancestor_path;
    token->RecordLog("Detected non-folder file in its path.");

    retry_on_success_ = true;
    MoveToBackground(base::Bind(&LocalToRemoteSyncer::DeleteRemoteFile,
                                weak_ptr_factory_.GetWeakPtr()),
                     std::move(token));
    return;
  }

  if (missing_components.empty()) {
    // The original target has remote active file/folder.
    remote_parent_folder_tracker_ =
        FindTrackerByID(metadata_database(),
                        active_ancestor_tracker->parent_tracker_id());
    remote_file_tracker_ = std::move(active_ancestor_tracker);
    target_path_ = url_.path();
    DCHECK(target_path_ == active_ancestor_path);

    if (remote_file_tracker_->dirty()) {
      token->RecordLog(base::StringPrintf(
          "Detected conflicting dirty tracker:%" PRId64,
           remote_file_tracker_->tracker_id()));
      // Both local and remote file has pending modification.
      HandleConflict(std::move(token));
      return;
    }

    // Non-conflicting file/folder update case.
    HandleExistingRemoteFile(std::move(token));
    return;
  }

  DCHECK(local_change_.IsAddOrUpdate());
  DCHECK_EQ(1u, missing_components.size());
  // The original target has remote parent folder and doesn't have remote active
  // file.
  // Upload the file as a new file or create a folder.
  remote_parent_folder_tracker_ = std::move(active_ancestor_tracker);
  target_path_ = url_.path();
  DCHECK(target_path_ == active_ancestor_path.Append(missing_components[0]));
  if (local_change_.file_type() == SYNC_FILE_TYPE_FILE) {
    token->RecordLog("Detected a new file.");
    MoveToBackground(base::Bind(&LocalToRemoteSyncer::UploadNewFile,
                                weak_ptr_factory_.GetWeakPtr()),
                     std::move(token));
    return;
  }

  token->RecordLog("Detected a new folder.");
  MoveToBackground(base::Bind(&LocalToRemoteSyncer::CreateRemoteFolder,
                              weak_ptr_factory_.GetWeakPtr()),
                   std::move(token));
}

void LocalToRemoteSyncer::MoveToBackground(
    const Continuation& continuation,
    std::unique_ptr<SyncTaskToken> token) {
  std::unique_ptr<TaskBlocker> blocker(new TaskBlocker);
  blocker->app_id = url_.origin().host();
  blocker->paths.push_back(target_path_);

  if (remote_file_tracker_) {
    if (!GetKnownChangeID(metadata_database(),
                          remote_file_tracker_->file_id(),
                          &remote_file_change_id_)) {
      NOTREACHED();
      SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
      return;
    }

    blocker->tracker_ids.push_back(remote_file_tracker_->tracker_id());
    blocker->file_ids.push_back(remote_file_tracker_->file_id());
  }

  // Run current task as a background task with |blocker|.
  // After the invocation of ContinueAsBackgroundTask
  SyncTaskManager::UpdateTaskBlocker(
      std::move(token), std::move(blocker),
      base::Bind(&LocalToRemoteSyncer::ContinueAsBackgroundTask,
                 weak_ptr_factory_.GetWeakPtr(), continuation));
}

void LocalToRemoteSyncer::ContinueAsBackgroundTask(
    const Continuation& continuation,
    std::unique_ptr<SyncTaskToken> token) {
  // The SyncTask runs as a background task beyond this point.
  // Note that any task can run between MoveToBackground() and
  // ContinueAsBackgroundTask(), so we need to make sure other tasks didn't
  // affect to the current LocalToRemoteSyncer task.
  //
  // - For RemoteToLocalSyncer, it doesn't actually run beyond
  //   PrepareForProcessRemoteChange() since it should be blocked in
  //   LocalFileSyncService.
  // - For ListChangesTask, it may update FileMetatada together with |change_id|
  //   and may delete FileTracker.  So, ensure |change_id| is not changed and
  //   check if FileTracker still exists.
  // - For UninstallAppTask, it may also delete FileMetadata and FileTracker.
  //   Check if FileTracker still exists.
  // - Others, SyncEngineInitializer and RegisterAppTask doesn't affect to
  //   LocalToRemoteSyncer.
  if (remote_file_tracker_) {
    int64_t latest_change_id = 0;
    if (!GetKnownChangeID(metadata_database(),
                          remote_file_tracker_->file_id(),
                          &latest_change_id) ||
        latest_change_id > remote_file_change_id_) {
      SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
      return;
    }

    if (!metadata_database()->FindTrackerByTrackerID(
            remote_file_tracker_->tracker_id(), nullptr)) {
      SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
      return;
    }
  }

  continuation.Run(std::move(token));
}

void LocalToRemoteSyncer::SyncCompleted(std::unique_ptr<SyncTaskToken> token,
                                        SyncStatusCode status) {
  if (status == SYNC_STATUS_OK && retry_on_success_)
    status = SYNC_STATUS_RETRY;

  if (needs_remote_change_listing_)
    status = SYNC_STATUS_FILE_BUSY;

  token->RecordLog(base::StringPrintf(
      "Finished: action=%s, status=%s for %s@%s",
      SyncActionToString(sync_action_),
      SyncStatusCodeToString(status),
      target_path_.AsUTF8Unsafe().c_str(),
      url_.origin().host().c_str()));

  SyncTaskManager::NotifyTaskDone(std::move(token), status);
}

void LocalToRemoteSyncer::HandleConflict(std::unique_ptr<SyncTaskToken> token) {
  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->has_synced_details());
  DCHECK(remote_file_tracker_->active());
  DCHECK(remote_file_tracker_->dirty());

  if (local_is_missing_) {
    SyncCompleted(std::move(token), SYNC_STATUS_OK);
    return;
  }

  if (local_change_.IsFile()) {
    // Upload the conflicting file as a new file and let ConflictResolver
    // resolve it.
    MoveToBackground(base::Bind(&LocalToRemoteSyncer::UploadNewFile,
                                weak_ptr_factory_.GetWeakPtr()),
                     std::move(token));
    return;
  }

  DCHECK(local_change_.IsDirectory());
  // Check if we can reuse the remote folder.
  FileMetadata remote_file_metadata;
  if (!metadata_database()->FindFileByFileID(
          remote_file_tracker_->file_id(), &remote_file_metadata)) {
    NOTREACHED();
    MoveToBackground(base::Bind(&LocalToRemoteSyncer::CreateRemoteFolder,
                                weak_ptr_factory_.GetWeakPtr()),
                     std::move(token));
    return;
  }

  const FileDetails& remote_details = remote_file_metadata.details();
  base::FilePath title = storage::VirtualPath::BaseName(target_path_);
  if (!remote_details.missing() &&
      remote_details.file_kind() == FILE_KIND_FOLDER &&
      remote_details.title() == title.AsUTF8Unsafe() &&
      HasFileAsParent(remote_details,
                      remote_parent_folder_tracker_->file_id())) {
    MoveToBackground(
        base::Bind(&LocalToRemoteSyncer::UpdateTrackerForReusedFolder,
                   weak_ptr_factory_.GetWeakPtr(), remote_details),
        std::move(token));
    return;
  }

  MoveToBackground(base::Bind(&LocalToRemoteSyncer::CreateRemoteFolder,
                              weak_ptr_factory_.GetWeakPtr()),
                   std::move(token));
}

void LocalToRemoteSyncer::UpdateTrackerForReusedFolder(
    const FileDetails& details,
    std::unique_ptr<SyncTaskToken> token) {
  SyncStatusCode status = metadata_database()->UpdateTracker(
      remote_file_tracker_->tracker_id(), details);
  SyncCompleted(std::move(token), status);
}

void LocalToRemoteSyncer::HandleExistingRemoteFile(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(remote_file_tracker_);
  DCHECK(!remote_file_tracker_->dirty());
  DCHECK(remote_file_tracker_->active());
  DCHECK(remote_file_tracker_->has_synced_details());

  if (local_is_missing_) {
    // Local file deletion for existing remote file.
    MoveToBackground(base::Bind(&LocalToRemoteSyncer::DeleteRemoteFile,
                                weak_ptr_factory_.GetWeakPtr()),
                     std::move(token));
    return;
  }

  DCHECK(local_change_.IsAddOrUpdate());
  DCHECK(local_change_.IsFile() || local_change_.IsDirectory());

  const FileDetails& synced_details = remote_file_tracker_->synced_details();
  DCHECK(synced_details.file_kind() == FILE_KIND_FILE ||
         synced_details.file_kind() == FILE_KIND_FOLDER);
  if (local_change_.IsFile()) {
    if (synced_details.file_kind() == FILE_KIND_FILE) {
      // Non-conflicting local file update to existing remote regular file.
      MoveToBackground(base::Bind(&LocalToRemoteSyncer::UploadExistingFile,
                                  weak_ptr_factory_.GetWeakPtr()),
                       std::move(token));
      return;
    }

    DCHECK_EQ(FILE_KIND_FOLDER, synced_details.file_kind());
    // Non-conflicting local file update to existing remote *folder*.
    // Assuming this case as local folder deletion + local file creation, delete
    // the remote folder and upload the file.
    retry_on_success_ = true;
    MoveToBackground(base::Bind(&LocalToRemoteSyncer::DeleteRemoteFile,
                                weak_ptr_factory_.GetWeakPtr()),
                     std::move(token));
    return;
  }

  DCHECK(local_change_.IsDirectory());
  if (synced_details.file_kind() == FILE_KIND_FILE) {
    // Non-conflicting local folder creation to existing remote *file*.
    // Assuming this case as local file deletion + local folder creation, delete
    // the remote file and create a remote folder.
    retry_on_success_ = true;
    MoveToBackground(base::Bind(&LocalToRemoteSyncer::DeleteRemoteFile,
                                weak_ptr_factory_.GetWeakPtr()),
                     std::move(token));
    return;
  }

  // Non-conflicting local folder creation to existing remote folder.
  DCHECK_EQ(FILE_KIND_FOLDER, synced_details.file_kind());
  SyncCompleted(std::move(token), SYNC_STATUS_OK);
}

void LocalToRemoteSyncer::DeleteRemoteFile(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->has_synced_details());

  switch (remote_file_tracker_->synced_details().file_kind()) {
    case FILE_KIND_UNSUPPORTED:
      NOTREACHED();
      file_type_ = SYNC_FILE_TYPE_UNKNOWN;
      break;
    case FILE_KIND_FILE:
      file_type_ = SYNC_FILE_TYPE_FILE;
      break;
    case FILE_KIND_FOLDER:
      file_type_ = SYNC_FILE_TYPE_DIRECTORY;
      break;
  }
  sync_action_ = SYNC_ACTION_DELETED;
  drive_service()->DeleteResource(
      remote_file_tracker_->file_id(),
      remote_file_tracker_->synced_details().etag(),
      base::Bind(&LocalToRemoteSyncer::DidDeleteRemoteFile,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
}

void LocalToRemoteSyncer::DidDeleteRemoteFile(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error) {
  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK &&
      error != google_apis::HTTP_NOT_FOUND &&
      error != google_apis::HTTP_PRECONDITION &&
      error != google_apis::HTTP_CONFLICT) {
    SyncCompleted(std::move(token), status);
    return;
  }

  // Handle NOT_FOUND case as SUCCESS case.
  // For PRECONDITION / CONFLICT case, the remote file is modified since the
  // last sync completed.  As our policy for deletion-modification conflict
  // resolution, ignore the local deletion.
  if (status == SYNC_STATUS_OK ||
      error == google_apis::HTTP_NOT_FOUND) {
    SyncStatusCode status = metadata_database()->UpdateByDeletedRemoteFile(
        remote_file_tracker_->file_id());
    SyncCompleted(std::move(token), status);
    return;
  }

  SyncCompleted(std::move(token), SYNC_STATUS_OK);
}

void LocalToRemoteSyncer::UploadExistingFile(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(remote_file_tracker_);
  DCHECK(remote_file_tracker_->has_synced_details());
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksInCurrentSequence());

  const std::string local_file_md5 = drive::util::GetMd5Digest(local_path_,
                                                               nullptr);
  if (local_file_md5 == remote_file_tracker_->synced_details().md5()) {
    // Local file is not changed.
    SyncCompleted(std::move(token), SYNC_STATUS_OK);
    return;
  }

  file_type_ = SYNC_FILE_TYPE_FILE;
  sync_action_ = SYNC_ACTION_UPDATED;

  drive::UploadExistingFileOptions options;
  options.etag = remote_file_tracker_->synced_details().etag();
  drive_uploader()->UploadExistingFile(
      remote_file_tracker_->file_id(),
      local_path_,
      "application/octet_stream",
      options,
      base::Bind(&LocalToRemoteSyncer::DidUploadExistingFile,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)),
      google_apis::ProgressCallback());
}

void LocalToRemoteSyncer::DidUploadExistingFile(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    const GURL&,
    std::unique_ptr<google_apis::FileResource> entry) {
  if (error == google_apis::HTTP_PRECONDITION ||
      error == google_apis::HTTP_CONFLICT ||
      error == google_apis::HTTP_NOT_FOUND) {
    // The remote file has unfetched remote change.  Fetch latest metadata and
    // update database with it.
    // TODO(tzik): Consider adding local side low-priority dirtiness handling to
    // handle this as ListChangesTask.

    needs_remote_change_listing_ = true;
    UpdateRemoteMetadata(remote_file_tracker_->file_id(), std::move(token));
    return;
  }

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  if (!entry) {
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  DCHECK(entry);
  status = metadata_database()->UpdateByFileResource(*entry);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  FileMetadata file;
  if (!metadata_database()->FindFileByFileID(
          remote_file_tracker_->file_id(), &file)) {
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  const FileDetails& details = file.details();
  base::FilePath title = storage::VirtualPath::BaseName(target_path_);
  if (!details.missing() &&
      details.file_kind() == FILE_KIND_FILE &&
      details.title() == title.AsUTF8Unsafe() &&
      HasFileAsParent(details,
                      remote_parent_folder_tracker_->file_id())) {
    SyncStatusCode status = metadata_database()->UpdateTracker(
        remote_file_tracker_->tracker_id(), file.details());
    SyncCompleted(std::move(token), status);
    return;
  }

  SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
}

void LocalToRemoteSyncer::UpdateRemoteMetadata(
    const std::string& file_id,
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(remote_file_tracker_);

  drive_service()->GetFileResource(
      file_id,
      base::Bind(&LocalToRemoteSyncer::DidGetRemoteMetadata,
                 weak_ptr_factory_.GetWeakPtr(),
                 file_id, base::Passed(&token)));
}

void LocalToRemoteSyncer::DidGetRemoteMetadata(
    const std::string& file_id,
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::FileResource> entry) {
  DCHECK(sync_context_->GetWorkerTaskRunner()->RunsTasksInCurrentSequence());

  if (error == google_apis::HTTP_NOT_FOUND) {
    retry_on_success_ = true;
    SyncStatusCode status =
        metadata_database()->UpdateByDeletedRemoteFile(file_id);
    SyncCompleted(std::move(token), status);
    return;
  }

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  if (!entry) {
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  retry_on_success_ = true;
  status = metadata_database()->UpdateByFileResource(*entry);
  SyncCompleted(std::move(token), status);
}

void LocalToRemoteSyncer::UploadNewFile(std::unique_ptr<SyncTaskToken> token) {
  DCHECK(remote_parent_folder_tracker_);

  file_type_ = SYNC_FILE_TYPE_FILE;
  sync_action_ = SYNC_ACTION_ADDED;
  base::FilePath title = storage::VirtualPath::BaseName(target_path_);
  drive_uploader()->UploadNewFile(
      remote_parent_folder_tracker_->file_id(), local_path_,
      title.AsUTF8Unsafe(), GetMimeTypeFromTitle(title),
      drive::UploadNewFileOptions(),
      base::Bind(&LocalToRemoteSyncer::DidUploadNewFile,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)),
      google_apis::ProgressCallback());
}

void LocalToRemoteSyncer::DidUploadNewFile(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    const GURL& upload_location,
    std::unique_ptr<google_apis::FileResource> entry) {
  if (error == google_apis::HTTP_NOT_FOUND)
    needs_remote_change_listing_ = true;

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  if (!entry) {
    NOTREACHED();
    SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  status = metadata_database()->ReplaceActiveTrackerWithNewResource(
      remote_parent_folder_tracker_->tracker_id(), *entry);
  SyncCompleted(std::move(token), status);
}

void LocalToRemoteSyncer::CreateRemoteFolder(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(remote_parent_folder_tracker_);

  base::FilePath title = storage::VirtualPath::BaseName(target_path_);
  file_type_ = SYNC_FILE_TYPE_DIRECTORY;
  sync_action_ = SYNC_ACTION_ADDED;

  DCHECK(!folder_creator_);
  folder_creator_.reset(new FolderCreator(
      drive_service(), metadata_database(),
      remote_parent_folder_tracker_->file_id(),
      title.AsUTF8Unsafe()));
  folder_creator_->Run(base::Bind(
      &LocalToRemoteSyncer::DidCreateRemoteFolder,
      weak_ptr_factory_.GetWeakPtr(),
      base::Passed(&token)));
}

void LocalToRemoteSyncer::DidCreateRemoteFolder(
    std::unique_ptr<SyncTaskToken> token,
    const std::string& file_id,
    SyncStatusCode status) {
  if (status == SYNC_FILE_ERROR_NOT_FOUND)
    needs_remote_change_listing_ = true;

  std::unique_ptr<FolderCreator> deleter = std::move(folder_creator_);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  status = SYNC_STATUS_FAILED;
  MetadataDatabase::ActivationStatus activation_status =
      metadata_database()->TryActivateTracker(
          remote_parent_folder_tracker_->tracker_id(),
          file_id, &status);
  switch (activation_status) {
    case MetadataDatabase::ACTIVATION_PENDING:
      SyncCompleted(std::move(token), status);
      return;
    case MetadataDatabase::ACTIVATION_FAILED_ANOTHER_ACTIVE_TRACKER:
      // The activation failed due to another tracker that has another parent.
      // Detach the folder from the current parent to avoid using this folder as
      // active folder.
      drive_service()->RemoveResourceFromDirectory(
          remote_parent_folder_tracker_->file_id(), file_id,
          base::Bind(&LocalToRemoteSyncer::DidDetachResourceForCreationConflict,
                     weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
      return;
  }

  NOTREACHED();
  SyncCompleted(std::move(token), SYNC_STATUS_FAILED);
  return;
}

void LocalToRemoteSyncer::DidDetachResourceForCreationConflict(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error) {
  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    SyncCompleted(std::move(token), status);
    return;
  }

  SyncCompleted(std::move(token), SYNC_STATUS_RETRY);
}

bool LocalToRemoteSyncer::IsContextReady() {
  return sync_context_->GetDriveService() &&
      sync_context_->GetDriveUploader() &&
      sync_context_->GetMetadataDatabase();
}

drive::DriveServiceInterface* LocalToRemoteSyncer::drive_service() {
  set_used_network(true);
  return sync_context_->GetDriveService();
}

drive::DriveUploaderInterface* LocalToRemoteSyncer::drive_uploader() {
  set_used_network(true);
  return sync_context_->GetDriveUploader();
}

MetadataDatabase* LocalToRemoteSyncer::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

}  // namespace drive_backend
}  // namespace sync_file_system
