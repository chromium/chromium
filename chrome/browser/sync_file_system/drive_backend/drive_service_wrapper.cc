// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_service_wrapper.h"

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/drive/service/drive_service_interface.h"

namespace sync_file_system {
namespace drive_backend {

DriveServiceWrapper::DriveServiceWrapper(
    drive::DriveServiceInterface* drive_service)
    : drive_service_(drive_service) {
  DCHECK(drive_service_);
}

DriveServiceWrapper::~DriveServiceWrapper() = default;

void DriveServiceWrapper::AddNewDirectory(
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const drive::AddNewDirectoryOptions& options,
    google_apis::FileResourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->AddNewDirectory(parent_resource_id, directory_title, options,
                                  std::move(callback));
}

void DriveServiceWrapper::DeleteResource(
    const std::string& resource_id,
    const std::string& etag,
    google_apis::EntryActionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->DeleteResource(resource_id, etag, std::move(callback));
}

void DriveServiceWrapper::DownloadFile(
    const base::FilePath& local_cache_path,
    const std::string& resource_id,
    google_apis::DownloadActionCallback download_action_callback,
    const google_apis::GetContentCallback& get_content_callback,
    google_apis::ProgressCallback progress_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->DownloadFile(local_cache_path, resource_id,
                               std::move(download_action_callback),
                               get_content_callback, progress_callback);
}

void DriveServiceWrapper::GetAboutResource(
    google_apis::AboutResourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetAboutResource(std::move(callback));
}

void DriveServiceWrapper::GetStartPageToken(
    const std::string& team_drive_id,
    google_apis::StartPageTokenCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetStartPageToken(team_drive_id, std::move(callback));
}

void DriveServiceWrapper::GetChangeList(
    int64_t start_changestamp,
    google_apis::ChangeListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetChangeList(start_changestamp, std::move(callback));
}

void DriveServiceWrapper::GetChangeListByToken(
    const std::string& team_drive_id,
    const std::string& start_page_token,
    google_apis::ChangeListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetChangeListByToken(team_drive_id, start_page_token,
                                       std::move(callback));
}

void DriveServiceWrapper::GetRemainingChangeList(
    const GURL& next_link,
    google_apis::ChangeListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetRemainingChangeList(next_link, std::move(callback));
}

void DriveServiceWrapper::GetRemainingTeamDriveList(
    const std::string& page_token,
    google_apis::TeamDriveListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetRemainingTeamDriveList(page_token, std::move(callback));
}

void DriveServiceWrapper::GetRemainingFileList(
    const GURL& next_link,
    google_apis::FileListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetRemainingFileList(next_link, std::move(callback));
}

void DriveServiceWrapper::GetFileResource(
    const std::string& resource_id,
    google_apis::FileResourceCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetFileResource(resource_id, std::move(callback));
}

void DriveServiceWrapper::GetFileListInDirectory(
    const std::string& directory_resource_id,
    google_apis::FileListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->GetFileListInDirectory(directory_resource_id,
                                         std::move(callback));
}

void DriveServiceWrapper::RemoveResourceFromDirectory(
    const std::string& parent_resource_id,
    const std::string& resource_id,
    google_apis::EntryActionCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->RemoveResourceFromDirectory(parent_resource_id, resource_id,
                                              std::move(callback));
}

void DriveServiceWrapper::SearchByTitle(
    const std::string& title,
    const std::string& directory_resource_id,
    google_apis::FileListCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  drive_service_->SearchByTitle(title, directory_resource_id,
                                std::move(callback));
}

}  // namespace drive_backend
}  // namespace sync_file_system
