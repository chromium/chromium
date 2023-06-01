// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_SERVICE_HELPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_SERVICE_HELPER_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
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

  google_apis::ApiErrorCode AddOrphanedFolder(const std::string& title,
                                              std::string* folder_id);
  google_apis::ApiErrorCode AddFolder(const std::string& parent_folder_id,
                                      const std::string& title,
                                      std::string* folder_id);
  google_apis::ApiErrorCode AddFile(const std::string& parent_folder_id,
                                    const std::string& title,
                                    const std::string& content,
                                    std::string* file_id);
  google_apis::ApiErrorCode UpdateFile(const std::string& file_id,
                                       const std::string& content);
  google_apis::ApiErrorCode DeleteResource(const std::string& file_id);
  google_apis::ApiErrorCode TrashResource(const std::string& file_id);
  google_apis::ApiErrorCode UpdateModificationTime(
      const std::string& file_id,
      const base::Time& modification_time);
  google_apis::ApiErrorCode RenameResource(const std::string& file_id,
                                           const std::string& new_title);
  google_apis::ApiErrorCode AddResourceToDirectory(
      const std::string& parent_folder_id,
      const std::string& file_id);
  google_apis::ApiErrorCode RemoveResourceFromDirectory(
      const std::string& parent_folder_id,
      const std::string& file_id);
  google_apis::ApiErrorCode GetSyncRootFolderID(
      std::string* sync_root_folder_id);
  google_apis::ApiErrorCode ListFilesInFolder(
      const std::string& folder_id,
      std::vector<std::unique_ptr<google_apis::FileResource>>* entries);
  google_apis::ApiErrorCode SearchByTitle(
      const std::string& folder_id,
      const std::string& title,
      std::vector<std::unique_ptr<google_apis::FileResource>>* entries);

  google_apis::ApiErrorCode GetFileResource(
      const std::string& file_id,
      std::unique_ptr<google_apis::FileResource>* entry);
  google_apis::ApiErrorCode GetFileVisibility(
      const std::string& file_id,
      google_apis::drive::FileVisibility* visiblity);
  google_apis::ApiErrorCode ReadFile(const std::string& file_id,
                                     std::string* file_content);
  google_apis::ApiErrorCode GetAboutResource(
      std::unique_ptr<google_apis::AboutResource>* about_resource);

  void AddTeamDrive(const std::string& team_drive_id,
                    const std::string& team_drive_name);

  base::FilePath base_dir_path() { return base_dir_.GetPath(); }

 private:
  google_apis::ApiErrorCode CompleteListing(
      std::unique_ptr<google_apis::FileList> list,
      std::vector<std::unique_ptr<google_apis::FileResource>>* entries);

  void Initialize();

  base::FilePath WriteToTempFile(const std::string& content);

  base::ScopedTempDir base_dir_;
  base::FilePath temp_dir_;

  // Not own.
  raw_ptr<drive::FakeDriveService, DanglingUntriaged> fake_drive_service_;
  raw_ptr<drive::DriveUploaderInterface, DanglingUntriaged> drive_uploader_;

  std::string sync_root_folder_title_;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_FAKE_DRIVE_SERVICE_HELPER_H_
