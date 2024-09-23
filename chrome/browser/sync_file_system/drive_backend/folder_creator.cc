// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/folder_creator.h"

#include <stddef.h>
#include <utility>

#include "base/functional/bind.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "components/drive/drive_api_util.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/drive/drive_api_parser.h"

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}  // namespace drive

namespace sync_file_system {
namespace drive_backend {

FolderCreator::FolderCreator(drive::DriveServiceInterface* drive_service,
                             MetadataDatabase* metadata_database,
                             const std::string& parent_folder_id,
                             const std::string& title)
    : drive_service_(drive_service),
      metadata_database_(metadata_database),
      parent_folder_id_(parent_folder_id),
      title_(title) {}

FolderCreator::~FolderCreator() = default;

void FolderCreator::Run(FileIDCallback callback) {
  drive::AddNewDirectoryOptions options;
  options.visibility = google_apis::drive::FILE_VISIBILITY_PRIVATE;
  drive_service_->AddNewDirectory(
      parent_folder_id_, title_, options,
      base::BindOnce(&FolderCreator::DidCreateFolder,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void FolderCreator::DidCreateFolder(
    FileIDCallback callback,
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::FileResource> entry) {
  SyncStatusCode status = ApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    std::move(callback).Run(std::string(), status);
    return;
  }

  drive_service_->SearchByTitle(
      title_, parent_folder_id_,
      base::BindOnce(
          &FolderCreator::DidListFolders, weak_ptr_factory_.GetWeakPtr(),
          std::move(callback),

          std::vector<std::unique_ptr<google_apis::FileResource>>()));
}

void FolderCreator::DidListFolders(
    FileIDCallback callback,
    std::vector<std::unique_ptr<google_apis::FileResource>> candidates,
    google_apis::ApiErrorCode error,
    std::unique_ptr<google_apis::FileList> file_list) {
  SyncStatusCode status = ApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    std::move(callback).Run(std::string(), status);
    return;
  }

  if (!file_list) {
    NOTREACHED_IN_MIGRATION();
    std::move(callback).Run(std::string(), SYNC_STATUS_FAILED);
    return;
  }

  candidates.reserve(candidates.size() + file_list->items().size());
  std::move(file_list->mutable_items()->begin(),
            file_list->mutable_items()->end(), std::back_inserter(candidates));
  file_list->mutable_items()->clear();

  if (!file_list->next_link().is_empty()) {
    drive_service_->GetRemainingFileList(
        file_list->next_link(),
        base::BindOnce(&FolderCreator::DidListFolders,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                       std::move(candidates)));
    return;
  }

  const google_apis::FileResource* oldest = nullptr;
  for (size_t i = 0; i < candidates.size(); ++i) {
    const google_apis::FileResource& entry = *candidates[i];
    if (!entry.IsDirectory() || entry.labels().is_trashed())
      continue;

    if (!oldest || oldest->created_date() > entry.created_date())
      oldest = &entry;
  }

  if (!oldest) {
    std::move(callback).Run(std::string(), SYNC_FILE_ERROR_NOT_FOUND);
    return;
  }

  std::string file_id = oldest->file_id();

  status = metadata_database_->UpdateByFileResourceList(std::move(candidates));
  if (status != SYNC_STATUS_OK) {
    std::move(callback).Run(std::string(), status);
    return;
  }

  DCHECK(!file_id.empty());
  if (!metadata_database_->FindFileByFileID(file_id, nullptr)) {
    std::move(callback).Run(std::string(), SYNC_FILE_ERROR_NOT_FOUND);
    return;
  }

  std::move(callback).Run(file_id, status);
}

}  // namespace drive_backend
}  // namespace sync_file_system
