// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_service_on_worker.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_service_wrapper.h"
#include "google_apis/drive/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

DriveServiceOnWorker::DriveServiceOnWorker(
    const base::WeakPtr<DriveServiceWrapper>& wrapper,
    base::SingleThreadTaskRunner* ui_task_runner,
    base::SequencedTaskRunner* worker_task_runner)
    : wrapper_(wrapper),
      ui_task_runner_(ui_task_runner),
      worker_task_runner_(worker_task_runner) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

DriveServiceOnWorker::~DriveServiceOnWorker() {}

google_apis::CancelCallbackOnce DriveServiceOnWorker::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const drive::AddNewDirectoryOptions& options,
    google_apis::FileResourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DriveServiceWrapper::AddNewDirectory, wrapper_,
                                parent_resource_id, directory_title, options,
                                RelayCallbackToTaskRunner(
                                    worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    google_apis::EntryActionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::DeleteResource, wrapper_, resource_id, etag,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    google_apis::DownloadActionCallback download_action_callback,
    const google_apis::GetContentCallback& get_content_callback,
    google_apis::ProgressCallback progress_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::DownloadFile, wrapper_, local_cache_path,
          resource_id,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(download_action_callback)),
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    get_content_callback),
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    progress_callback)));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetAboutResource(
    google_apis::AboutResourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::GetAboutResource, wrapper_,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetStartPageToken(
    const std::string& team_drive_id,
    google_apis::StartPageTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::GetStartPageToken, wrapper_, team_drive_id,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetChangeList(
    int64_t start_changestamp,
    google_apis::ChangeListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::GetChangeList, wrapper_, start_changestamp,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetChangeListByToken(
    const std::string& team_drive_id,
    const std::string& start_page_token,
    google_apis::ChangeListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DriveServiceWrapper::GetChangeListByToken,
                                wrapper_, team_drive_id, start_page_token,
                                RelayCallbackToTaskRunner(
                                    worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetRemainingChangeList(
    const GURL& next_link,
    google_apis::ChangeListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::GetRemainingChangeList, wrapper_, next_link,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

std::string DriveServiceOnWorker::GetRootResourceId() const {
  NOTREACHED_IN_MIGRATION();
  // This method is expected to be called only on unit tests.
  return "root";
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetRemainingTeamDriveList(
    const std::string& page_token,
    google_apis::TeamDriveListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::GetRemainingTeamDriveList, wrapper_, page_token,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetRemainingFileList(
    const GURL& next_link,
    google_apis::FileListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::GetRemainingFileList, wrapper_, next_link,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetFileResource(
    const std::string& resource_id,
    google_apis::FileResourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::GetFileResource, wrapper_, resource_id,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetFileListInDirectory(
    const std::string& directory_resource_id,
    google_apis::FileListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DriveServiceWrapper::GetFileListInDirectory,
                                wrapper_, directory_resource_id,
                                RelayCallbackToTaskRunner(
                                    worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce
DriveServiceOnWorker::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    google_apis::EntryActionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::RemoveResourceFromDirectory, wrapper_,
          parent_resource_id, resource_id,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    google_apis::FileListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&DriveServiceWrapper::SearchByTitle, wrapper_,
                                title, directory_resource_id,
                                RelayCallbackToTaskRunner(
                                    worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

bool DriveServiceOnWorker::HasRefreshToken() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void DriveServiceOnWorker::Initialize(const CoreAccountId& account_id) {
  NOTREACHED_IN_MIGRATION();
}

void DriveServiceOnWorker::AddObserver(drive::DriveServiceObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void DriveServiceOnWorker::RemoveObserver(
    drive::DriveServiceObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

bool DriveServiceOnWorker::CanSendRequest() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool DriveServiceOnWorker::HasAccessToken() const {
  NOTREACHED_IN_MIGRATION();
  return false;
}

void DriveServiceOnWorker::RequestAccessToken(
    google_apis::AuthStatusCallback callback) {
  NOTREACHED_IN_MIGRATION();
}

void DriveServiceOnWorker::ClearAccessToken() {
  NOTREACHED_IN_MIGRATION();
}

void DriveServiceOnWorker::ClearRefreshToken() {
  NOTREACHED_IN_MIGRATION();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetAllTeamDriveList(
    google_apis::TeamDriveListCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetAllFileList(
    const std::string& team_drive_id,
    google_apis::FileListCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::Search(
    const std::string& search_query,
    google_apis::FileListCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::TrashResource(
    const std::string& resource_id,
    google_apis::EntryActionCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    google_apis::FileResourceCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::drive::Properties& properties,
    google_apis::FileResourceCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    google_apis::EntryActionCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::InitiateUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const drive::UploadNewFileOptions& options,
    google_apis::InitiateUploadCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce
DriveServiceOnWorker::InitiateUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const drive::UploadExistingFileOptions& options,
    google_apis::InitiateUploadCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::ResumeUpload(
    const GURL& upload_url,
    int64_t start_position,
    int64_t end_position,
    int64_t content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    google_apis::drive::UploadRangeCallback callback,
    google_apis::ProgressCallback progress_callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::GetUploadStatus(
    const GURL& upload_url,
    int64_t content_length,
    google_apis::drive::UploadRangeCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::MultipartUploadNewFile(
    const std::string& content_type,
    std::optional<std::string_view> converted_mime_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const drive::UploadNewFileOptions& options,
    google_apis::FileResourceCallback callback,
    google_apis::ProgressCallback progress_callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce
DriveServiceOnWorker::MultipartUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const base::FilePath& local_file_path,
    const drive::UploadExistingFileOptions& options,
    google_apis::FileResourceCallback callback,
    google_apis::ProgressCallback progress_callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

std::unique_ptr<drive::BatchRequestConfiguratorInterface>
DriveServiceOnWorker::StartBatchRequest() {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

google_apis::CancelCallbackOnce DriveServiceOnWorker::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    google_apis::EntryActionCallback callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

}  // namespace drive_backend
}  // namespace sync_file_system
