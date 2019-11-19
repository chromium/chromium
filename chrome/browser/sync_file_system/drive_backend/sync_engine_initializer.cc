// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"

#include <stddef.h>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "components/drive/service/drive_api_service.h"
#include "google_apis/drive/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

namespace {

////////////////////////////////////////////////////////////////////////////////
// Functions below are for wrapping the access to legacy GData WAPI classes.

bool HasNoParents(const google_apis::FileResource& entry) {
  return entry.parents().empty();
}

bool HasFolderAsParent(const google_apis::FileResource& entry,
                       const std::string& parent_id) {
  for (size_t i = 0; i < entry.parents().size(); ++i) {
    if (entry.parents()[i].file_id() == parent_id)
      return true;
  }
  return false;
}

bool LessOnCreationTime(const google_apis::FileResource& left,
                        const google_apis::FileResource& right) {
  return left.created_date() < right.created_date();
}

// Functions above are for wrapping the access to legacy GData WAPI classes.
////////////////////////////////////////////////////////////////////////////////

}  // namespace

SyncEngineInitializer::SyncEngineInitializer(
    SyncEngineContext* sync_context,
    const base::FilePath& database_path,
    leveldb::Env* env_override)
    : sync_context_(sync_context),
      env_override_(env_override),
      database_path_(database_path),
      find_sync_root_retry_count_(0),
      largest_change_id_(0) {
  DCHECK(sync_context);
}

SyncEngineInitializer::~SyncEngineInitializer() {
  if (!cancel_callback_.is_null())
    cancel_callback_.Run();
}

void SyncEngineInitializer::RunPreflight(std::unique_ptr<SyncTaskToken> token) {
  util::Log(logging::LOG_VERBOSE, FROM_HERE, "[Initialize] Start.");
  DCHECK(sync_context_);
  DCHECK(sync_context_->GetDriveService());

  // The metadata seems to have been already initialized. Just return with OK.
  if (sync_context_->GetMetadataDatabase()) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Already initialized.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_OK);
    return;
  }

  SyncStatusCode status = SYNC_STATUS_FAILED;
  std::unique_ptr<MetadataDatabase> metadata_database =
      MetadataDatabase::Create(database_path_, env_override_, &status);

  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to initialize MetadataDatabase.");
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  DCHECK(metadata_database);
  metadata_database_ = std::move(metadata_database);
  if (metadata_database_->HasSyncRoot() &&
      !metadata_database_->NeedsSyncRootRevalidation()) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Found local cache of sync-root.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_OK);
    return;
  }

  GetAboutResource(std::move(token));
}

std::unique_ptr<MetadataDatabase>
SyncEngineInitializer::PassMetadataDatabase() {
  return std::move(metadata_database_);
}

void SyncEngineInitializer::GetAboutResource(
    std::unique_ptr<SyncTaskToken> token) {
  set_used_network(true);
  sync_context_->GetDriveService()->GetAboutResource(
      base::Bind(&SyncEngineInitializer::DidGetAboutResource,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
}

void SyncEngineInitializer::DidGetAboutResource(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::AboutResource> about_resource) {
  cancel_callback_.Reset();

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to get AboutResource.");
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  DCHECK(about_resource);
  root_folder_id_ = about_resource->root_folder_id();
  largest_change_id_ = about_resource->largest_change_id();

  DCHECK(!root_folder_id_.empty());
  FindSyncRoot(std::move(token));
}

void SyncEngineInitializer::FindSyncRoot(std::unique_ptr<SyncTaskToken> token) {
  if (find_sync_root_retry_count_++ >= kMaxRetry) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Reached max retry count.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  set_used_network(true);
  cancel_callback_ = sync_context_->GetDriveService()->SearchByTitle(
      kSyncRootFolderTitle,
      std::string(),  // parent_folder_id
      base::Bind(&SyncEngineInitializer::DidFindSyncRoot,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Passed(&token)));
}

void SyncEngineInitializer::DidFindSyncRoot(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::FileList> file_list) {
  cancel_callback_.Reset();

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to find sync root.");
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  if (!file_list) {
    NOTREACHED();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Got invalid resource list.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  std::vector<std::unique_ptr<google_apis::FileResource>>* items =
      file_list->mutable_items();
  for (auto& entry : *items) {
    // Ignore deleted folder.
    if (entry->labels().is_trashed())
      continue;

    // Pick an orphaned folder or a direct child of the root folder and
    // ignore others.
    DCHECK(!root_folder_id_.empty());
    if (!HasNoParents(*entry) && !HasFolderAsParent(*entry, root_folder_id_))
      continue;

    if (entry->shared())
      continue;

    if (!sync_root_folder_ || LessOnCreationTime(*entry, *sync_root_folder_)) {
      sync_root_folder_ = std::move(entry);
    }
  }

  set_used_network(true);
  // If there are more results, retrieve them.
  if (!file_list->next_link().is_empty()) {
    cancel_callback_ = sync_context_->GetDriveService()->GetRemainingFileList(
        file_list->next_link(),
        base::Bind(&SyncEngineInitializer::DidFindSyncRoot,
                   weak_ptr_factory_.GetWeakPtr(),
                   base::Passed(&token)));
    return;
  }

  if (!sync_root_folder_) {
    CreateSyncRoot(std::move(token));
    return;
  }

  if (!HasNoParents(*sync_root_folder_)) {
    DetachSyncRoot(std::move(token));
    return;
  }

  ListAppRootFolders(std::move(token));
}

void SyncEngineInitializer::CreateSyncRoot(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(!sync_root_folder_);
  set_used_network(true);
  drive::AddNewDirectoryOptions options;
  options.visibility = google_apis::drive::FILE_VISIBILITY_PRIVATE;
  cancel_callback_ = sync_context_->GetDriveService()->AddNewDirectory(
      root_folder_id_, kSyncRootFolderTitle, options,
      base::Bind(&SyncEngineInitializer::DidCreateSyncRoot,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
}

void SyncEngineInitializer::DidCreateSyncRoot(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::FileResource> entry) {
  DCHECK(!sync_root_folder_);
  cancel_callback_.Reset();

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to create sync root.");
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  FindSyncRoot(std::move(token));
}

void SyncEngineInitializer::DetachSyncRoot(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(sync_root_folder_);
  set_used_network(true);
  cancel_callback_ =
      sync_context_->GetDriveService()->RemoveResourceFromDirectory(
          root_folder_id_,
          sync_root_folder_->file_id(),
          base::Bind(&SyncEngineInitializer::DidDetachSyncRoot,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&token)));
}

void SyncEngineInitializer::DidDetachSyncRoot(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error) {
  cancel_callback_.Reset();

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to detach sync root.");
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  ListAppRootFolders(std::move(token));
}

void SyncEngineInitializer::ListAppRootFolders(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(sync_root_folder_);
  set_used_network(true);
  cancel_callback_ =
      sync_context_->GetDriveService()->GetFileListInDirectory(
          sync_root_folder_->file_id(),
          base::Bind(&SyncEngineInitializer::DidListAppRootFolders,
                     weak_ptr_factory_.GetWeakPtr(),
                     base::Passed(&token)));
}

void SyncEngineInitializer::DidListAppRootFolders(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::FileList> file_list) {
  cancel_callback_.Reset();

  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to get initial app-root folders.");
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  if (!file_list) {
    NOTREACHED();
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Got invalid initial app-root list.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  std::vector<std::unique_ptr<google_apis::FileResource>>* new_entries =
      file_list->mutable_items();
  app_root_folders_.reserve(app_root_folders_.size() + new_entries->size());
  std::move(new_entries->begin(), new_entries->end(),
            std::back_inserter(app_root_folders_));
  new_entries->clear();

  set_used_network(true);
  if (!file_list->next_link().is_empty()) {
    cancel_callback_ =
        sync_context_->GetDriveService()->GetRemainingFileList(
            file_list->next_link(),
            base::Bind(&SyncEngineInitializer::DidListAppRootFolders,
                       weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
    return;
  }

  PopulateDatabase(std::move(token));
}

void SyncEngineInitializer::PopulateDatabase(
    std::unique_ptr<SyncTaskToken> token) {
  DCHECK(sync_root_folder_);
  SyncStatusCode status = metadata_database_->PopulateInitialData(
      largest_change_id_, *sync_root_folder_, app_root_folders_);
  if (status != SYNC_STATUS_OK) {
    util::Log(logging::LOG_VERBOSE, FROM_HERE,
              "[Initialize] Failed to populate initial data"
              " to MetadataDatabase.");
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "[Initialize] Completed successfully.");
  SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_OK);
}

}  // namespace drive_backend
}  // namespace sync_file_system
