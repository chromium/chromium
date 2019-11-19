// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REGISTER_APP_TASK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REGISTER_APP_TASK_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "google_apis/drive/drive_api_error_codes.h"

namespace drive {
class DriveServiceInterface;
}

namespace sync_file_system {
namespace drive_backend {

class FileTracker;
class FolderCreator;
class MetadataDatabase;
class SyncEngineContext;
class TrackerIDSet;

class RegisterAppTask : public ExclusiveTask {
 public:
  RegisterAppTask(SyncEngineContext* sync_context, const std::string& app_id);
  ~RegisterAppTask() override;

  bool CanFinishImmediately();
  void RunExclusive(const SyncStatusCallback& callback) override;

 private:
  void CreateAppRootFolder(const SyncStatusCallback& callback);
  void DidCreateAppRootFolder(const SyncStatusCallback& callback,
                              const std::string& file_id,
                              SyncStatusCode status);
  bool FilterCandidates(const TrackerIDSet& trackers,
                        FileTracker* candidate);
  void RegisterAppIntoDatabase(const FileTracker& tracker,
                               const SyncStatusCallback& callback);

  MetadataDatabase* metadata_database();
  drive::DriveServiceInterface* drive_service();

  SyncEngineContext* sync_context_;  // Not owned.

  int create_folder_retry_count_;
  std::string app_id_;

  std::unique_ptr<FolderCreator> folder_creator_;

  base::WeakPtrFactory<RegisterAppTask> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RegisterAppTask);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_REGISTER_APP_TASK_H_
