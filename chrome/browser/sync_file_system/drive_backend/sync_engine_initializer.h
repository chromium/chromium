// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_INITIALIZER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_INITIALIZER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "google_apis/drive/drive_api_error_codes.h"
#include "google_apis/drive/drive_common_callbacks.h"

namespace google_apis {
class AboutResource;
class FileList;
class FileResource;
}

namespace leveldb {
class Env;
}

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;

// This class performs initializion sequence of SyncEngine.
//
// After initialize sequence completed, the Database must have
//  - Largest change ID,
//  - Sync-root folder and its tracker,
//  - All children of sync-root folder that have inactive and non-dirty
//    trackers.
//
// The initialization sequence is:
//  - Open database and load its contents,
//  - If the database is already populated, complete the sequence.
//  - Get AboutResource to get the largest change ID and the Drive root folder
//    ID.
//  - Find the remote sync-root folder, whose title is
//    "Chrome Syncable FileSystem" and has no parent.
//    Note that if the initialization is interrupted by the browser restart or
//    an error, the sequence leaves the folder in the Drive root folder.  So, if
//    we find the folder in the Drive root folder, handle it as the sync-root
//    folder.
//  - Create the remote sync-root folder if we don't have.
//  - Detach the remote sync-root folder from its parent if it has.
//  - Fetch the folder contents of the remote sync-root folder.
//    The contents are likely registered as app-root folders, but handle them
//    as regular inactive folders until they are registered explicitly.
//  - Populate database with the largest change ID, the sync-root folder and
//    its contents.
//
class SyncEngineInitializer : public SyncTask {
 public:
  SyncEngineInitializer(SyncEngineContext* sync_context,
                        const base::FilePath& database_path,
                        leveldb::Env* env_override);
  ~SyncEngineInitializer() override;
  void RunPreflight(std::unique_ptr<SyncTaskToken> token) override;

  std::unique_ptr<MetadataDatabase> PassMetadataDatabase();

 private:
  typedef base::Callback<void(const SyncStatusCallback& callback)> Task;

  void GetAboutResource(std::unique_ptr<SyncTaskToken> token);
  void DidGetAboutResource(
      std::unique_ptr<SyncTaskToken> token,
      google_apis::DriveApiErrorCode error,
      std::unique_ptr<google_apis::AboutResource> about_resource);
  void FindSyncRoot(std::unique_ptr<SyncTaskToken> token);
  void DidFindSyncRoot(std::unique_ptr<SyncTaskToken> token,
                       google_apis::DriveApiErrorCode error,
                       std::unique_ptr<google_apis::FileList> file_list);
  void CreateSyncRoot(std::unique_ptr<SyncTaskToken> token);
  void DidCreateSyncRoot(std::unique_ptr<SyncTaskToken> token,
                         google_apis::DriveApiErrorCode error,
                         std::unique_ptr<google_apis::FileResource> entry);
  void DetachSyncRoot(std::unique_ptr<SyncTaskToken> token);
  void DidDetachSyncRoot(std::unique_ptr<SyncTaskToken> token,
                         google_apis::DriveApiErrorCode error);
  void ListAppRootFolders(std::unique_ptr<SyncTaskToken> token);
  void DidListAppRootFolders(std::unique_ptr<SyncTaskToken> token,
                             google_apis::DriveApiErrorCode error,
                             std::unique_ptr<google_apis::FileList> file_list);
  void PopulateDatabase(std::unique_ptr<SyncTaskToken> token);

  SyncEngineContext* sync_context_;  // Not owned.
  leveldb::Env* env_override_;

  google_apis::CancelCallback cancel_callback_;
  base::FilePath database_path_;

  int find_sync_root_retry_count_;

  std::unique_ptr<MetadataDatabase> metadata_database_;
  std::vector<std::unique_ptr<google_apis::FileResource>> app_root_folders_;

  int64_t largest_change_id_;
  std::string root_folder_id_;

  std::unique_ptr<google_apis::FileResource> sync_root_folder_;

  base::WeakPtrFactory<SyncEngineInitializer> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SyncEngineInitializer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_INITIALIZER_H_
