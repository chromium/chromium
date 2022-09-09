// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FOLDER_CREATOR_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FOLDER_CREATOR_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "google_apis/common/api_error_codes.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class FileList;
class FileResource;
}  // namespace google_apis

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;

class FolderCreator {
 public:
  using FileIDCallback = base::OnceCallback<void(const std::string& file_id,
                                                 SyncStatusCode status)>;

  FolderCreator(drive::DriveServiceInterface* drive_service,
                MetadataDatabase* metadata_database,
                const std::string& parent_folder_id,
                const std::string& title);

  FolderCreator(const FolderCreator&) = delete;
  FolderCreator& operator=(const FolderCreator&) = delete;

  ~FolderCreator();

  void Run(FileIDCallback callback);

 private:
  void DidCreateFolder(FileIDCallback callback,
                       google_apis::ApiErrorCode error,
                       std::unique_ptr<google_apis::FileResource> entry);
  void DidListFolders(
      FileIDCallback callback,
      std::vector<std::unique_ptr<google_apis::FileResource>> candidates,
      google_apis::ApiErrorCode error,
      std::unique_ptr<google_apis::FileList> file_list);

  raw_ptr<drive::DriveServiceInterface> drive_service_;
  raw_ptr<MetadataDatabase> metadata_database_;

  const std::string parent_folder_id_;
  const std::string title_;

  base::WeakPtrFactory<FolderCreator> weak_ptr_factory_{this};
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FOLDER_CREATOR_H_
