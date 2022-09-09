// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_

#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/remote_change_processor.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "google_apis/common/api_error_codes.h"
#include "storage/browser/file_system/file_system_url.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class FileList;
class FileResource;
}  // namespace google_apis

namespace storage {
class ScopedFile;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;

class RemoteToLocalSyncer : public SyncTask {
 public:
  typedef SyncTaskManager::Continuation Continuation;

  // Conflicting trackers will have low priority for RemoteToLocalSyncer so that
  // it should be resolved by LocatToRemoteSyncer.
  explicit RemoteToLocalSyncer(SyncEngineContext* sync_context);

  RemoteToLocalSyncer(const RemoteToLocalSyncer&) = delete;
  RemoteToLocalSyncer& operator=(const RemoteToLocalSyncer&) = delete;

  ~RemoteToLocalSyncer() override;

  void RunPreflight(std::unique_ptr<SyncTaskToken> token) override;
  void RunExclusive(std::unique_ptr<SyncTaskToken> token);

  const storage::FileSystemURL& url() const { return url_; }
  SyncFileType file_type() const { return file_type_; }
  SyncAction sync_action() const { return sync_action_; }

  bool is_sync_root_deletion() const { return sync_root_deletion_; }

 private:
  typedef std::vector<std::string> FileIDList;

  // TODO(tzik): Update documentation here.
  //
  // Dispatches remote change to handlers or to SyncCompleted() directly.
  // This function uses information only in MetadataDatabase.
  //
  // If the tracker doesn't have remote metadata:
  //   # The file is listed in a folder right before this operation.
  //   - Dispatch to HandleMissingRemoteMetadata to fetch remote metadata.
  // Else, if the tracker is not active or the dominating app-root is disabled:
  //   # Assume the file has remote metadata.
  //   - Update the tracker with |missing| flag and empty |md5|.
  //   Note: MetadataDatabase may activate the tracker if possible.
  // Else, if the tracker doesn't have synced metadata:
  //   # Assume the tracker has remote metadata and the tracker is active.
  //   # The tracker is not yet synced ever.
  //   - If the file is remotely deleted, do nothing to local file and dispatch
  //     directly to SyncCompleted().
  //   - Else, if the file is a regular file, dispatch to HandleNewFile().
  //   - Else, if the file is a folder, dispatch to HandleFolderUpdate().
  //   - Else, the file should be an unsupported active file. This should not
  //     happen.
  // Else, if the remote metadata is marked as deleted:
  //   # Most of the remote metadata is missing including title, kind and md5.
  //   - Dispatch to HandleDeletion().
  // Else, if the tracker has different titles between its synced metadata and
  // remote metadata:
  //   # Assume the tracker is active and has remote metetadata and synced
  //     metadata.
  //   # The file is remotely renamed.
  //   # Maybe, this can be decomposed to delete and update.
  //   - Dispatch to HandleRemoteRename().
  // Else, if the tracker's parent is not a parent of the remote metadata:
  //   # The file has reorganized.
  //   # Maybe, this can be decomposed to delete and update.
  //   - Dispatch to HandreReorganize().
  // Else, if the folder is a regular file and the md5 in remote metadata does
  // not match the md5 in synced metadata:
  //   # The file is modified remotely.
  //   - Dispatch to HandleContentUpdate().
  // Else, if the tracker is a folder and it has needs_folder_listing flag:
  //   - Dispatch to HandleFolderContentListing()
  // Else, there should be no change to sync.
  //   - Dispatch to HandleOfflineSolvable()
  void ResolveRemoteChange(std::unique_ptr<SyncTaskToken> token);

  void MoveToBackground(std::unique_ptr<SyncTaskToken> token,
                        Continuation continuation);
  void ContinueAsBackgroundTask(Continuation continuation,
                                std::unique_ptr<SyncTaskToken> token);

  // Handles missing remote metadata case.
  // Fetches remote metadata and updates MetadataDatabase by that.  The sync
  // operation itself will be deferred to the next sync round.
  // Note: if the file is not found, it should be handled as if deleted.
  void HandleMissingRemoteMetadata(std::unique_ptr<SyncTaskToken> token);
  void DidGetRemoteMetadata(std::unique_ptr<SyncTaskToken> token,
                            google_apis::ApiErrorCode error,
                            std::unique_ptr<google_apis::FileResource> entry);

  // This implements the body of the HandleNewFile and HandleContentUpdate.
  // If the file doesn't have corresponding local file:
  //   - Dispatch to DownloadFile.
  // Else, if the local file doesn't have local change:
  //   - Dispatch to DownloadFile if the local file is a regular file.
  //   - If the local file is a folder, handle this case as a conflict.  Lower
  //     the priority of the tracker, and defer further handling to
  //     local-to-remote change.
  // Else:
  //  # The file has local modification.
  //  - Handle this case as a conflict.  Lower the priority of the tracker, and
  //    defer further handling to local-to-remote change.
  void DidPrepareForAddOrUpdateFile(std::unique_ptr<SyncTaskToken> token,
                                    SyncStatusCode status);

  // Handles remotely added folder.  Needs Prepare() call.
  // TODO(tzik): Write details and implement this.
  void HandleFolderUpdate(std::unique_ptr<SyncTaskToken> token);
  void DidPrepareForFolderUpdate(std::unique_ptr<SyncTaskToken> token,
                                 SyncStatusCode status);

  // Handles deleted remote file.  Needs Prepare() call.
  // If the deleted tracker is the sync-root:
  //  - TODO(tzik): Needs special handling.
  // Else, if the deleted tracker is a app-root:
  //  - TODO(tzik): Needs special handling.
  // Else, if the local file is already deleted:
  //  - Do nothing anymore to the local, call SyncCompleted().
  // Else, if the local file is modified:
  //  - Do nothing to the local file, call SyncCompleted().
  // Else, if the local file is not modified:
  //  - Delete local file.
  //  # Note: if the local file is a folder, delete recursively.
  void HandleDeletion(std::unique_ptr<SyncTaskToken> token);
  void DidPrepareForDeletion(std::unique_ptr<SyncTaskToken> token,
                             SyncStatusCode status);

  void HandleFileMove(std::unique_ptr<SyncTaskToken> token);

  // Handles new file.  Needs Prepare() call.
  void HandleContentUpdate(std::unique_ptr<SyncTaskToken> token);

  void ListFolderContent(std::unique_ptr<SyncTaskToken> token);
  void DidListFolderContent(std::unique_ptr<SyncTaskToken> token,
                            std::unique_ptr<FileIDList> children,
                            google_apis::ApiErrorCode error,
                            std::unique_ptr<google_apis::FileList> file_list);

  void SyncCompleted(std::unique_ptr<SyncTaskToken> token,
                     SyncStatusCode status);
  void FinalizeSync(std::unique_ptr<SyncTaskToken> token,
                    SyncStatusCode status);

  void Prepare(SyncStatusCallback callback);
  void DidPrepare(SyncStatusCallback callback,
                  SyncStatusCode status,
                  const SyncFileMetadata& metadata,
                  const FileChangeList& changes);

  void DeleteLocalFile(std::unique_ptr<SyncTaskToken> token);
  void DownloadFile(std::unique_ptr<SyncTaskToken> token);
  void DidDownloadFile(std::unique_ptr<SyncTaskToken> token,
                       storage::ScopedFile file,
                       google_apis::ApiErrorCode error,
                       const base::FilePath&);
  void DidApplyDownload(std::unique_ptr<SyncTaskToken> token,
                        storage::ScopedFile,
                        SyncStatusCode status);

  void CreateFolder(std::unique_ptr<SyncTaskToken> token);

  // TODO(tzik): After we convert all callbacks to token-passing style,
  // drop this function.
  SyncStatusCallback SyncCompletedCallback(
      std::unique_ptr<SyncTaskToken> token);

  drive::DriveServiceInterface* drive_service();
  MetadataDatabase* metadata_database();
  RemoteChangeProcessor* remote_change_processor();

  raw_ptr<SyncEngineContext> sync_context_;  // Not owned.

  std::unique_ptr<FileTracker> dirty_tracker_;
  std::unique_ptr<FileMetadata> remote_metadata_;

  storage::FileSystemURL url_;
  SyncFileType file_type_;
  SyncAction sync_action_;

  bool prepared_;
  bool sync_root_deletion_;

  std::unique_ptr<SyncFileMetadata> local_metadata_;
  std::unique_ptr<FileChangeList> local_changes_;

  base::WeakPtrFactory<RemoteToLocalSyncer> weak_ptr_factory_{this};
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REMOTE_TO_LOCAL_SYNCER_H_
