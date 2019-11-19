// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/drive/service/drive_service_interface.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

namespace sync_file_system {
namespace drive_backend {

class DriveServiceWrapper;

// This class wraps a part of DriveServiceInterface class to post actual
// tasks to DriveServiceWrapper which lives in another thread.
// Each method wraps corresponding name method of DriveServiceInterface.
// See comments in drive_service_interface.h for details.
class DriveServiceOnWorker : public drive::DriveServiceInterface {
 public:
  DriveServiceOnWorker(
      const base::WeakPtr<DriveServiceWrapper>& wrapper,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner);
  ~DriveServiceOnWorker() override;

  google_apis::CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const drive::AddNewDirectoryOptions& options,
      const google_apis::FileResourceCallback& callback) override;

  google_apis::CancelCallback DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      const google_apis::EntryActionCallback& callback) override;

  google_apis::CancelCallback DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      const google_apis::DownloadActionCallback& download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      const google_apis::ProgressCallback& progress_callback) override;

  google_apis::CancelCallback GetAboutResource(
      const google_apis::AboutResourceCallback& callback) override;

  google_apis::CancelCallback GetStartPageToken(
      const std::string& team_drive_id,
      const google_apis::StartPageTokenCallback& callback) override;

  google_apis::CancelCallback GetChangeList(
      int64_t start_changestamp,
      const google_apis::ChangeListCallback& callback) override;

  google_apis::CancelCallback GetChangeListByToken(
      const std::string& team_drive_id,
      const std::string& start_page_token,
      const google_apis::ChangeListCallback& callback) override;

  google_apis::CancelCallback GetRemainingChangeList(
      const GURL& next_link,
      const google_apis::ChangeListCallback& callback) override;

  std::string GetRootResourceId() const override;

  google_apis::CancelCallback GetRemainingTeamDriveList(
      const std::string& page_token,
      const google_apis::TeamDriveListCallback& callback) override;

  google_apis::CancelCallback GetRemainingFileList(
      const GURL& next_link,
      const google_apis::FileListCallback& callback) override;

  google_apis::CancelCallback GetFileResource(
      const std::string& resource_id,
      const google_apis::FileResourceCallback& callback) override;

  google_apis::CancelCallback GetFileListInDirectory(
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) override;

  google_apis::CancelCallback RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) override;

  google_apis::CancelCallback SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      const google_apis::FileListCallback& callback) override;

  bool HasRefreshToken() const override;

  // Following virtual methods are expected not to be accessed at all.
  void Initialize(const CoreAccountId& account_id) override;
  void AddObserver(drive::DriveServiceObserver* observer) override;
  void RemoveObserver(drive::DriveServiceObserver* observer) override;
  bool CanSendRequest() const override;
  bool HasAccessToken() const override;
  void RequestAccessToken(
      const google_apis::AuthStatusCallback& callback) override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;
  google_apis::CancelCallback GetAllTeamDriveList(
      const google_apis::TeamDriveListCallback& callback) override;
  google_apis::CancelCallback GetAllFileList(
      const std::string& team_drive_id,
      const google_apis::FileListCallback& callback) override;
  google_apis::CancelCallback Search(
      const std::string& search_query,
      const google_apis::FileListCallback& callback) override;
  google_apis::CancelCallback TrashResource(
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) override;
  google_apis::CancelCallback CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const google_apis::FileResourceCallback& callback) override;
  google_apis::CancelCallback UpdateResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const base::Time& last_viewed_by_me,
      const google_apis::drive::Properties& properties,
      const google_apis::FileResourceCallback& callback) override;
  google_apis::CancelCallback AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      const google_apis::EntryActionCallback& callback) override;
  google_apis::CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const drive::UploadNewFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) override;
  google_apis::CancelCallback InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const drive::UploadExistingFileOptions& options,
      const google_apis::InitiateUploadCallback& callback) override;
  google_apis::CancelCallback ResumeUpload(
      const GURL& upload_url,
      int64_t start_position,
      int64_t end_position,
      int64_t content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const google_apis::drive::UploadRangeCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback GetUploadStatus(
      const GURL& upload_url,
      int64_t content_length,
      const google_apis::drive::UploadRangeCallback& callback) override;
  google_apis::CancelCallback MultipartUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const base::FilePath& local_file_path,
      const drive::UploadNewFileOptions& options,
      const google_apis::FileResourceCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  google_apis::CancelCallback MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const drive::UploadExistingFileOptions& options,
      const google_apis::FileResourceCallback& callback,
      const google_apis::ProgressCallback& progress_callback) override;
  std::unique_ptr<drive::BatchRequestConfiguratorInterface> StartBatchRequest()
      override;
  google_apis::CancelCallback AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      const google_apis::EntryActionCallback& callback) override;

 private:
  base::WeakPtr<DriveServiceWrapper> wrapper_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  base::SequenceChecker sequence_checker_;

  DISALLOW_COPY_AND_ASSIGN(DriveServiceOnWorker);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_
