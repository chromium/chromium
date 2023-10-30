// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_WRAPPER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_WRAPPER_H_

#include <stdint.h>

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/drive/service/drive_service_interface.h"

namespace sync_file_system {
namespace drive_backend {

// This class wraps a part of DriveServiceInterface class to support weak
// pointer.  Each method wraps corresponding name method of
// DriveServiceInterface.  See comments in drive_service_interface.h
// for details.
class DriveServiceWrapper {
 public:
  explicit DriveServiceWrapper(drive::DriveServiceInterface* drive_service);
  ~DriveServiceWrapper();

  DriveServiceWrapper(const DriveServiceWrapper&) = delete;
  DriveServiceWrapper& operator=(const DriveServiceWrapper&) = delete;

  void AddNewDirectory(const std::string& parent_resource_id,
                       const std::string& directory_title,
                       const drive::AddNewDirectoryOptions& options,
                       google_apis::FileResourceCallback callback);

  void DeleteResource(const std::string& resource_id,
                      const std::string& etag,
                      google_apis::EntryActionCallback callback);

  void DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      google_apis::DownloadActionCallback download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      google_apis::ProgressCallback progress_callback);

  void GetAboutResource(google_apis::AboutResourceCallback callback);

  void GetStartPageToken(const std::string& team_drive_id,
                         google_apis::StartPageTokenCallback callback);

  void GetChangeList(int64_t start_changestamp,
                     google_apis::ChangeListCallback callback);

  void GetChangeListByToken(const std::string& team_drive_id,
                            const std::string& start_page_token,
                            google_apis::ChangeListCallback callback);

  void GetRemainingChangeList(const GURL& next_link,
                              google_apis::ChangeListCallback callback);

  void GetRemainingTeamDriveList(const std::string& page_token,
                                 google_apis::TeamDriveListCallback callback);

  void GetRemainingFileList(const GURL& next_link,
                            google_apis::FileListCallback callback);

  void GetFileResource(const std::string& resource_id,
                       google_apis::FileResourceCallback callback);

  void GetFileListInDirectory(const std::string& directory_resource_id,
                              google_apis::FileListCallback callback);

  void RemoveResourceFromDirectory(const std::string& parent_resource_id,
                                   const std::string& resource_id,
                                   google_apis::EntryActionCallback callback);

  void SearchByTitle(const std::string& title,
                     const std::string& directory_resource_id,
                     google_apis::FileListCallback callback);

  base::WeakPtr<DriveServiceWrapper> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  raw_ptr<drive::DriveServiceInterface> drive_service_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<DriveServiceWrapper> weak_ptr_factory_{this};
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_WRAPPER_H_
