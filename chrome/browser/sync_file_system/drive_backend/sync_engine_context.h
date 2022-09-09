// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
}

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace sync_file_system {

class RemoteChangeProcessor;
class TaskLogger;

namespace drive_backend {

class MetadataDatabase;

class SyncEngineContext {
 public:
  SyncEngineContext(
      std::unique_ptr<drive::DriveServiceInterface> drive_service,
      std::unique_ptr<drive::DriveUploaderInterface> drive_uploader,
      TaskLogger* task_logger,
      const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner);

  SyncEngineContext(const SyncEngineContext&) = delete;
  SyncEngineContext& operator=(const SyncEngineContext&) = delete;

  ~SyncEngineContext();

  void SetMetadataDatabase(std::unique_ptr<MetadataDatabase> metadata_database);
  void SetRemoteChangeProcessor(
      RemoteChangeProcessor* remote_change_processor);

  drive::DriveServiceInterface* GetDriveService();
  drive::DriveUploaderInterface* GetDriveUploader();
  base::WeakPtr<TaskLogger> GetTaskLogger();
  MetadataDatabase* GetMetadataDatabase();
  RemoteChangeProcessor* GetRemoteChangeProcessor();
  base::SingleThreadTaskRunner* GetUITaskRunner();
  base::SequencedTaskRunner* GetWorkerTaskRunner();

  std::unique_ptr<MetadataDatabase> PassMetadataDatabase();

  void DetachFromSequence();

 private:
  friend class DriveBackendSyncTest;

  std::unique_ptr<drive::DriveServiceInterface> drive_service_;
  std::unique_ptr<drive::DriveUploaderInterface> drive_uploader_;
  base::WeakPtr<TaskLogger> task_logger_;
  raw_ptr<RemoteChangeProcessor> remote_change_processor_;  // Not owned.

  std::unique_ptr<MetadataDatabase> metadata_database_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_CONTEXT_H_
