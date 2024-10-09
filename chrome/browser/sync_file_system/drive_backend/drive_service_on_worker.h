// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_

#include <stdint.h>

#include <memory>
#include <string>

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

  DriveServiceOnWorker(const DriveServiceOnWorker&) = delete;
  DriveServiceOnWorker& operator=(const DriveServiceOnWorker&) = delete;

  ~DriveServiceOnWorker() override;

  google_apis::CancelCallbackOnce AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_title,
      const drive::AddNewDirectoryOptions& options,
      google_apis::FileResourceCallback callback) override;

  google_apis::CancelCallbackOnce DeleteResource(
      const std::string& resource_id,
      const std::string& etag,
      google_apis::EntryActionCallback callback) override;

  google_apis::CancelCallbackOnce DownloadFile(
      const base::FilePath& local_cache_path,
      const std::string& resource_id,
      google_apis::DownloadActionCallback download_action_callback,
      const google_apis::GetContentCallback& get_content_callback,
      google_apis::ProgressCallback progress_callback) override;

  google_apis::CancelCallbackOnce GetAboutResource(
      google_apis::AboutResourceCallback callback) override;

  google_apis::CancelCallbackOnce GetStartPageToken(
      const std::string& team_drive_id,
      google_apis::StartPageTokenCallback callback) override;

  google_apis::CancelCallbackOnce GetChangeList(
      int64_t start_changestamp,
      google_apis::ChangeListCallback callback) override;

  google_apis::CancelCallbackOnce GetChangeListByToken(
      const std::string& team_drive_id,
      const std::string& start_page_token,
      google_apis::ChangeListCallback callback) override;

  google_apis::CancelCallbackOnce GetRemainingChangeList(
      const GURL& next_link,
      google_apis::ChangeListCallback callback) override;

  std::string GetRootResourceId() const override;

  google_apis::CancelCallbackOnce GetRemainingTeamDriveList(
      const std::string& page_token,
      google_apis::TeamDriveListCallback callback) override;

  google_apis::CancelCallbackOnce GetRemainingFileList(
      const GURL& next_link,
      google_apis::FileListCallback callback) override;

  google_apis::CancelCallbackOnce GetFileResource(
      const std::string& resource_id,
      google_apis::FileResourceCallback callback) override;

  google_apis::CancelCallbackOnce GetFileListInDirectory(
      const std::string& directory_resource_id,
      google_apis::FileListCallback callback) override;

  google_apis::CancelCallbackOnce RemoveResourceFromDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) override;

  google_apis::CancelCallbackOnce SearchByTitle(
      const std::string& title,
      const std::string& directory_resource_id,
      google_apis::FileListCallback callback) override;

  bool HasRefreshToken() const override;

  // Following virtual methods are expected not to be accessed at all.
  void Initialize(const CoreAccountId& account_id) override;
  void AddObserver(drive::DriveServiceObserver* observer) override;
  void RemoveObserver(drive::DriveServiceObserver* observer) override;
  bool CanSendRequest() const override;
  bool HasAccessToken() const override;
  void RequestAccessToken(google_apis::AuthStatusCallback callback) override;
  void ClearAccessToken() override;
  void ClearRefreshToken() override;
  google_apis::CancelCallbackOnce GetAllTeamDriveList(
      google_apis::TeamDriveListCallback callback) override;
  google_apis::CancelCallbackOnce GetAllFileList(
      const std::string& team_drive_id,
      google_apis::FileListCallback callback) override;
  google_apis::CancelCallbackOnce Search(
      const std::string& search_query,
      google_apis::FileListCallback callback) override;
  google_apis::CancelCallbackOnce TrashResource(
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) override;
  google_apis::CancelCallbackOnce CopyResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      google_apis::FileResourceCallback callback) override;
  google_apis::CancelCallbackOnce UpdateResource(
      const std::string& resource_id,
      const std::string& parent_resource_id,
      const std::string& new_title,
      const base::Time& last_modified,
      const base::Time& last_viewed_by_me,
      const google_apis::drive::Properties& properties,
      google_apis::FileResourceCallback callback) override;
  google_apis::CancelCallbackOnce AddResourceToDirectory(
      const std::string& parent_resource_id,
      const std::string& resource_id,
      google_apis::EntryActionCallback callback) override;
  google_apis::CancelCallbackOnce InitiateUploadNewFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const drive::UploadNewFileOptions& options,
      google_apis::InitiateUploadCallback callback) override;
  google_apis::CancelCallbackOnce InitiateUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const drive::UploadExistingFileOptions& options,
      google_apis::InitiateUploadCallback callback) override;
  google_apis::CancelCallbackOnce ResumeUpload(
      const GURL& upload_url,
      int64_t start_position,
      int64_t end_position,
      int64_t content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      google_apis::drive::UploadRangeCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce GetUploadStatus(
      const GURL& upload_url,
      int64_t content_length,
      google_apis::drive::UploadRangeCallback callback) override;
  google_apis::CancelCallbackOnce MultipartUploadNewFile(
      const std::string& content_type,
      std::optional<std::string_view> converted_mime_type,
      int64_t content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const base::FilePath& local_file_path,
      const drive::UploadNewFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  google_apis::CancelCallbackOnce MultipartUploadExistingFile(
      const std::string& content_type,
      int64_t content_length,
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const drive::UploadExistingFileOptions& options,
      google_apis::FileResourceCallback callback,
      google_apis::ProgressCallback progress_callback) override;
  std::unique_ptr<drive::BatchRequestConfiguratorInterface> StartBatchRequest()
      override;
  google_apis::CancelCallbackOnce AddPermission(
      const std::string& resource_id,
      const std::string& email,
      google_apis::drive::PermissionRole role,
      google_apis::EntryActionCallback callback) override;

 private:
  base::WeakPtr<DriveServiceWrapper> wrapper_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_SERVICE_ON_WORKER_H_
