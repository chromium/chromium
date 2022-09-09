// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LIST_CHANGES_TASK_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LIST_CHANGES_TASK_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "google_apis/common/api_error_codes.h"

namespace drive {
class DriveServiceInterface;
}

namespace google_apis {
class ChangeList;
class ChangeResource;
}  // namespace google_apis

namespace sync_file_system {
namespace drive_backend {

class MetadataDatabase;
class SyncEngineContext;

class ListChangesTask : public SyncTask {
 public:
  explicit ListChangesTask(SyncEngineContext* sync_context);

  ListChangesTask(const ListChangesTask&) = delete;
  ListChangesTask& operator=(const ListChangesTask&) = delete;

  ~ListChangesTask() override;

  void RunPreflight(std::unique_ptr<SyncTaskToken> token) override;

 private:
  void StartListing(std::unique_ptr<SyncTaskToken> token);
  void DidListChanges(std::unique_ptr<SyncTaskToken> token,
                      google_apis::ApiErrorCode error,
                      std::unique_ptr<google_apis::ChangeList> change_list);
  void CheckInChangeList(int64_t largest_change_id,
                         std::unique_ptr<SyncTaskToken> token);

  bool IsContextReady();
  MetadataDatabase* metadata_database();
  drive::DriveServiceInterface* drive_service();

  raw_ptr<SyncEngineContext> sync_context_;
  std::vector<std::unique_ptr<google_apis::ChangeResource>> change_list_;

  std::vector<std::string> file_ids_;

  base::WeakPtrFactory<ListChangesTask> weak_ptr_factory_{this};
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LIST_CHANGES_TASK_H_
