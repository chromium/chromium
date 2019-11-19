// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_service_on_worker.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
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
  sequence_checker_.DetachFromSequence();
}

DriveServiceOnWorker::~DriveServiceOnWorker() {}

google_apis::CancelCallback DriveServiceOnWorker::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const drive::AddNewDirectoryOptions& options,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::AddNewDirectory, wrapper_,
                     parent_resource_id, directory_title, options,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::DeleteResource, wrapper_,
                     resource_id, etag,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    const google_apis::DownloadActionCallback& download_action_callback,
    const google_apis::GetContentCallback& get_content_callback,
    const google_apis::ProgressCallback& progress_callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveServiceWrapper::DownloadFile, wrapper_, local_cache_path,
          resource_id,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    download_action_callback),
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    get_content_callback),
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    progress_callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetAboutResource(
    const google_apis::AboutResourceCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetAboutResource, wrapper_,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetStartPageToken(
    const std::string& team_drive_id,
    const google_apis::StartPageTokenCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetStartPageToken, wrapper_,
                     team_drive_id,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetChangeList(
    int64_t start_changestamp,
    const google_apis::ChangeListCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetChangeList, wrapper_,
                     start_changestamp,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetChangeListByToken(
    const std::string& team_drive_id,
    const std::string& start_page_token,
    const google_apis::ChangeListCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetChangeListByToken, wrapper_,
                     team_drive_id, start_page_token,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetRemainingChangeList(
    const GURL& next_link,
    const google_apis::ChangeListCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetRemainingChangeList, wrapper_,
                     next_link,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

std::string DriveServiceOnWorker::GetRootResourceId() const {
  NOTREACHED();
  // This method is expected to be called only on unit tests.
  return "root";
}

google_apis::CancelCallback DriveServiceOnWorker::GetRemainingTeamDriveList(
    const std::string& page_token,
    const google_apis::TeamDriveListCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetRemainingTeamDriveList, wrapper_,
                     page_token,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetRemainingFileList(
    const GURL& next_link,
    const google_apis::FileListCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetRemainingFileList, wrapper_,
                     next_link,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}


google_apis::CancelCallback DriveServiceOnWorker::GetFileResource(
    const std::string& resource_id,
    const google_apis::FileResourceCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetFileResource, wrapper_,
                     resource_id,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetFileListInDirectory(
    const std::string& directory_resource_id,
    const google_apis::FileListCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::GetFileListInDirectory, wrapper_,
                     directory_resource_id,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::RemoveResourceFromDirectory,
                     wrapper_, parent_resource_id, resource_id,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    const google_apis::FileListCallback& callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&DriveServiceWrapper::SearchByTitle, wrapper_, title,
                     directory_resource_id,
                     RelayCallbackToTaskRunner(worker_task_runner_.get(),
                                               FROM_HERE, callback)));

  return google_apis::CancelCallback();
}

bool DriveServiceOnWorker::HasRefreshToken() const {
  NOTREACHED();
  return false;
}

void DriveServiceOnWorker::Initialize(const CoreAccountId& account_id) {
  NOTREACHED();
}

void DriveServiceOnWorker::AddObserver(drive::DriveServiceObserver* observer) {
  NOTREACHED();
}

void DriveServiceOnWorker::RemoveObserver(
    drive::DriveServiceObserver* observer) {
  NOTREACHED();
}

bool DriveServiceOnWorker::CanSendRequest() const {
  NOTREACHED();
  return false;
}

bool DriveServiceOnWorker::HasAccessToken() const {
  NOTREACHED();
  return false;
}

void DriveServiceOnWorker::RequestAccessToken(
    const google_apis::AuthStatusCallback& callback) {
  NOTREACHED();
}

void DriveServiceOnWorker::ClearAccessToken() {
  NOTREACHED();
}

void DriveServiceOnWorker::ClearRefreshToken() {
  NOTREACHED();
}

google_apis::CancelCallback DriveServiceOnWorker::GetAllTeamDriveList(
    const google_apis::TeamDriveListCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetAllFileList(
    const std::string& team_drive_id,
    const google_apis::FileListCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::Search(
    const std::string& search_query,
    const google_apis::FileListCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::TrashResource(
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::CopyResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const google_apis::FileResourceCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::UpdateResource(
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const base::Time& last_modified,
    const base::Time& last_viewed_by_me,
    const google_apis::drive::Properties& properties,
    const google_apis::FileResourceCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::AddResourceToDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const google_apis::EntryActionCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::InitiateUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const drive::UploadNewFileOptions& options,
    const google_apis::InitiateUploadCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::InitiateUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& resource_id,
    const drive::UploadExistingFileOptions& options,
    const google_apis::InitiateUploadCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::ResumeUpload(
    const GURL& upload_url,
    int64_t start_position,
    int64_t end_position,
    int64_t content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const google_apis::drive::UploadRangeCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::GetUploadStatus(
    const GURL& upload_url,
    int64_t content_length,
    const google_apis::drive::UploadRangeCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::MultipartUploadNewFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const base::FilePath& local_file_path,
    const drive::UploadNewFileOptions& options,
    const google_apis::FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

google_apis::CancelCallback DriveServiceOnWorker::MultipartUploadExistingFile(
    const std::string& content_type,
    int64_t content_length,
    const std::string& parent_resource_id,
    const base::FilePath& local_file_path,
    const drive::UploadExistingFileOptions& options,
    const google_apis::FileResourceCallback& callback,
    const google_apis::ProgressCallback& progress_callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

std::unique_ptr<drive::BatchRequestConfiguratorInterface>
DriveServiceOnWorker::StartBatchRequest() {
  NOTREACHED();
  return std::unique_ptr<drive::BatchRequestConfiguratorInterface>();
}

google_apis::CancelCallback DriveServiceOnWorker::AddPermission(
    const std::string& resource_id,
    const std::string& email,
    google_apis::drive::PermissionRole role,
    const google_apis::EntryActionCallback& callback) {
  NOTREACHED();
  return google_apis::CancelCallback();
}

}  // namespace drive_backend
}  // namespace sync_file_system
