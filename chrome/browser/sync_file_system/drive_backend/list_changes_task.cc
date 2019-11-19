// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"

#include <stddef.h>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/location.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_util.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_token.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/drive/service/drive_service_interface.h"
#include "google_apis/drive/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

ListChangesTask::ListChangesTask(SyncEngineContext* sync_context)
    : sync_context_(sync_context) {}

ListChangesTask::~ListChangesTask() {
}

void ListChangesTask::RunPreflight(std::unique_ptr<SyncTaskToken> token) {
  token->InitializeTaskLog("List Changes");

  if (!IsContextReady()) {
    token->RecordLog("Failed to get required service.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  SyncTaskManager::UpdateTaskBlocker(
      std::move(token), std::unique_ptr<TaskBlocker>(new TaskBlocker),
      base::Bind(&ListChangesTask::StartListing,
                 weak_ptr_factory_.GetWeakPtr()));
}

void ListChangesTask::StartListing(std::unique_ptr<SyncTaskToken> token) {
  drive_service()->GetChangeList(
      metadata_database()->GetLargestFetchedChangeID() + 1,
      base::Bind(&ListChangesTask::DidListChanges,
                 weak_ptr_factory_.GetWeakPtr(), base::Passed(&token)));
}

void ListChangesTask::DidListChanges(
    std::unique_ptr<SyncTaskToken> token,
    google_apis::DriveApiErrorCode error,
    std::unique_ptr<google_apis::ChangeList> change_list) {
  SyncStatusCode status = DriveApiErrorCodeToSyncStatusCode(error);
  if (status != SYNC_STATUS_OK) {
    token->RecordLog("Failed to fetch change list.");
    SyncTaskManager::NotifyTaskDone(std::move(token),
                                    SYNC_STATUS_NETWORK_ERROR);
    return;
  }

  if (!change_list) {
    NOTREACHED();
    token->RecordLog("Got invalid change list.");
    SyncTaskManager::NotifyTaskDone(std::move(token), SYNC_STATUS_FAILED);
    return;
  }

  auto* mutable_items = change_list->mutable_items();

  // google_apis::ChangeList can contain both FileResource and TeamDriveResource
  // entries. We only care about FileResource entries, so filter out any entries
  // that are TeamDriveReasource.
  base::EraseIf(*mutable_items, [](const auto& change_resource) {
    return change_resource->type() ==
           google_apis::ChangeResource::ChangeType::TEAM_DRIVE;
  });

  change_list_.reserve(change_list_.size() + mutable_items->size());

  std::move(mutable_items->begin(), mutable_items->end(),
            std::back_inserter(change_list_));
  change_list->mutable_items()->clear();

  if (!change_list->next_link().is_empty()) {
    drive_service()->GetRemainingChangeList(
        change_list->next_link(),
        base::Bind(
            &ListChangesTask::DidListChanges,
            weak_ptr_factory_.GetWeakPtr(),
            base::Passed(&token)));
    return;
  }

  if (change_list_.empty()) {
    token->RecordLog("Got no change.");
    SyncTaskManager::NotifyTaskDone(std::move(token),
                                    SYNC_STATUS_NO_CHANGE_TO_SYNC);
    return;
  }

  std::unique_ptr<TaskBlocker> task_blocker(new TaskBlocker);
  task_blocker->exclusive = true;
  SyncTaskManager::UpdateTaskBlocker(
      std::move(token), std::move(task_blocker),
      base::Bind(&ListChangesTask::CheckInChangeList,
                 weak_ptr_factory_.GetWeakPtr(),
                 change_list->largest_change_id()));
}

void ListChangesTask::CheckInChangeList(int64_t largest_change_id,
                                        std::unique_ptr<SyncTaskToken> token) {
  token->RecordLog(base::StringPrintf(
      "Got %" PRIuS " changes, updating MetadataDatabase.",
      change_list_.size()));

  DCHECK(file_ids_.empty());
  file_ids_.reserve(change_list_.size());
  for (size_t i = 0; i < change_list_.size(); ++i)
    file_ids_.push_back(change_list_[i]->file_id());

  SyncStatusCode status = metadata_database()->UpdateByChangeList(
      largest_change_id, std::move(change_list_));
  if (status != SYNC_STATUS_OK) {
    SyncTaskManager::NotifyTaskDone(std::move(token), status);
    return;
  }

  status = metadata_database()->SweepDirtyTrackers(file_ids_);
  SyncTaskManager::NotifyTaskDone(std::move(token), status);
}

bool ListChangesTask::IsContextReady() {
  return sync_context_->GetMetadataDatabase() &&
      sync_context_->GetDriveService();
}

MetadataDatabase* ListChangesTask::metadata_database() {
  return sync_context_->GetMetadataDatabase();
}

drive::DriveServiceInterface* ListChangesTask::drive_service() {
  set_used_network(true);
  return sync_context_->GetDriveService();
}

}  // namespace drive_backend
}  // namespace sync_file_system
