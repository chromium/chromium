// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_SERVICE_HELPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_SERVICE_HELPER_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "components/drive/drive_uploader.h"
#include "components/drive/service/fake_drive_service.h"

namespace base {
class FilePath;
}

namespace sync_file_system {
namespace drive_backend {

class FakeDriveServiceHelper {
 public:
  FakeDriveServiceHelper(drive::FakeDriveService* fake_drive_service,
                         drive::DriveUploaderInterface* drive_uploader,
                         const std::string& sync_root_folder_title);
  virtual ~FakeDriveServiceHelper();

  google_apis::DriveApiErrorCode AddOrphanedFolder(
      const std::string& title,
      std::string* folder_id);
  google_apis::DriveApiErrorCode AddFolder(
      const std::string& parent_folder_id,
      const std::string& title,
      std::string* folder_id);
  google_apis::DriveApiErrorCode AddFile(
      const std::string& parent_folder_id,
      const std::string& title,
      const std::string& content,
      std::string* file_id);
  google_apis::DriveApiErrorCode UpdateFile(
      const std::string& file_id,
      const std::string& content);
  google_apis::DriveApiErrorCode DeleteResource(
      const std::string& file_id);
  google_apis::DriveApiErrorCode TrashResource(
      const std::string& file_id);
  google_apis::DriveApiErrorCode UpdateModificationTime(
      const std::string& file_id,
      const base::Time& modification_time);
  google_apis::DriveApiErrorCode RenameResource(
      const std::string& file_id,
      const std::string& new_title);
  google_apis::DriveApiErrorCode AddResourceToDirectory(
      const std::string& parent_folder_id,
      const std::string& file_id);
  google_apis::DriveApiErrorCode RemoveResourceFromDirectory(
      const std::string& parent_folder_id,
      const std::string& file_id);
  google_apis::DriveApiErrorCode GetSyncRootFolderID(
      std::string* sync_root_folder_id);
  google_apis::DriveApiErrorCode ListFilesInFolder(
      const std::string& folder_id,
      std::vector<std::unique_ptr<google_apis::FileResource>>* entries);
  google_apis::DriveApiErrorCode SearchByTitle(
      const std::string& folder_id,
      const std::string& title,
      std::vector<std::unique_ptr<google_apis::FileResource>>* entries);

  google_apis::DriveApiErrorCode GetFileResource(
      const std::string& file_id,
      std::unique_ptr<google_apis::FileResource>* entry);
  google_apis::DriveApiErrorCode GetFileVisibility(
      const std::string& file_id,
      google_apis::drive::FileVisibility* visiblity);
  google_apis::DriveApiErrorCode ReadFile(
      const std::string& file_id,
      std::string* file_content);
  google_apis::DriveApiErrorCode GetAboutResource(
      std::unique_ptr<google_apis::AboutResource>* about_resource);

  void AddTeamDrive(const std::string& team_drive_id,
                    const std::string& team_drive_name);

  base::FilePath base_dir_path() { return base_dir_.GetPath(); }

 private:
  google_apis::DriveApiErrorCode CompleteListing(
      std::unique_ptr<google_apis::FileList> list,
      std::vector<std::unique_ptr<google_apis::FileResource>>* entries);

  void Initialize();

  base::FilePath WriteToTempFile(const std::string& content);

  base::ScopedTempDir base_dir_;
  base::FilePath temp_dir_;

  // Not own.
  drive::FakeDriveService* fake_drive_service_;
  drive::DriveUploaderInterface* drive_uploader_;

  std::string sync_root_folder_title_;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_SERVICE_HELPER_H_
