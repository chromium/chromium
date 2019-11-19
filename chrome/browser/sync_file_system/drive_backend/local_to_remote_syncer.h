// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "google_apis/drive/drive_api_error_codes.h"

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace google_apis {
class FileResource;
}

namespace sync_file_system {

namespace drive_backend {

class FileDetails;
class FileTracker;
class FolderCreator;
class MetadataDatabase;
class SyncEngineContext;

class LocalToRemoteSyncer : public SyncTask {
 public:
  typedef base::Callback<void(std::unique_ptr<SyncTaskToken>)> Continuation;

  LocalToRemoteSyncer(SyncEngineContext* sync_context,
                      const SyncFileMetadata& local_metadata,
                      const FileChange& local_change,
                      const base::FilePath& local_path,
                      const storage::FileSystemURL& url);
  ~LocalToRemoteSyncer() override;
  void RunPreflight(std::unique_ptr<SyncTaskToken> token) override;

  const storage::FileSystemURL& url() const { return url_; }
  const base::FilePath& target_path() const { return target_path_; }
  SyncFileType file_type() const { return file_type_; }
  SyncAction sync_action() const { return sync_action_; }
  bool needs_remote_change_listing() const {
    return needs_remote_change_listing_;
  }

 private:
  void MoveToBackground(const Continuation& continuation,
                        std::unique_ptr<SyncTaskToken> token);
  void ContinueAsBackgroundTask(const Continuation& continuation,
                                std::unique_ptr<SyncTaskToken> token);
  void SyncCompleted(std::unique_ptr<SyncTaskToken> token,
                     SyncStatusCode status);

  void HandleConflict(std::unique_ptr<SyncTaskToken> token);
  void HandleExistingRemoteFile(std::unique_ptr<SyncTaskToken> token);

  void UpdateTrackerForReusedFolder(const FileDetails& details,
                                    std::unique_ptr<SyncTaskToken> token);

  void DeleteRemoteFile(std::unique_ptr<SyncTaskToken> token);
  void DidDeleteRemoteFile(std::unique_ptr<SyncTaskToken> token,
                           google_apis::DriveApiErrorCode error);

  void UploadExistingFile(std::unique_ptr<SyncTaskToken> token);
  void DidUploadExistingFile(std::unique_ptr<SyncTaskToken> token,
                             google_apis::DriveApiErrorCode error,
                             const GURL&,
                             std::unique_ptr<google_apis::FileResource>);
  void UpdateRemoteMetadata(const std::string& file_id,
                            std::unique_ptr<SyncTaskToken> token);
  void DidGetRemoteMetadata(const std::string& file_id,
                            std::unique_ptr<SyncTaskToken> token,
                            google_apis::DriveApiErrorCode error,
                            std::unique_ptr<google_apis::FileResource> entry);

  void UploadNewFile(std::unique_ptr<SyncTaskToken> token);
  void DidUploadNewFile(std::unique_ptr<SyncTaskToken> token,
                        google_apis::DriveApiErrorCode error,
                        const GURL& upload_location,
                        std::unique_ptr<google_apis::FileResource> entry);

  void CreateRemoteFolder(std::unique_ptr<SyncTaskToken> token);
  void DidCreateRemoteFolder(std::unique_ptr<SyncTaskToken> token,
                             const std::string& file_id,
                             SyncStatusCode status);
  void DidDetachResourceForCreationConflict(
      std::unique_ptr<SyncTaskToken> token,
      google_apis::DriveApiErrorCode error);

  bool IsContextReady();
  drive::DriveServiceInterface* drive_service();
  drive::DriveUploaderInterface* drive_uploader();
  MetadataDatabase* metadata_database();

  SyncEngineContext* sync_context_;  // Not owned.

  FileChange local_change_;
  bool local_is_missing_;
  base::FilePath local_path_;
  storage::FileSystemURL url_;
  SyncFileType file_type_;
  SyncAction sync_action_;

  std::unique_ptr<FileTracker> remote_file_tracker_;
  std::unique_ptr<FileTracker> remote_parent_folder_tracker_;
  base::FilePath target_path_;
  int64_t remote_file_change_id_;

  bool retry_on_success_;
  bool needs_remote_change_listing_;

  std::unique_ptr<FolderCreator> folder_creator_;

  base::WeakPtrFactory<LocalToRemoteSyncer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(LocalToRemoteSyncer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_
