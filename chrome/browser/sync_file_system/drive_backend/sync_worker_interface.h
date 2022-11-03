// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_INTERFACE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_INTERFACE_H_

#include <memory>
#include <string>

#include "base/values.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "net/base/network_change_notifier.h"

class GURL;

namespace base {
class FilePath;
}

namespace storage {
class FileSystemURL;
}

namespace sync_file_system {

class FileChange;
class SyncFileMetadata;

namespace drive_backend {

class RemoteChangeProcessorOnWorker;
class SyncEngineContext;

class SyncWorkerInterface {
 public:
  class Observer {
   public:
    virtual void OnPendingFileListUpdated(int item_count) = 0;
    virtual void OnFileStatusChanged(const storage::FileSystemURL& url,
                                     SyncFileType file_type,
                                     SyncFileStatus file_status,
                                     SyncAction sync_action,
                                     SyncDirection direction) = 0;
    virtual void UpdateServiceState(RemoteServiceState state,
                                    const std::string& description) = 0;

   protected:
    virtual ~Observer() {}
  };

  SyncWorkerInterface() {}

  SyncWorkerInterface(const SyncWorkerInterface&) = delete;
  SyncWorkerInterface& operator=(const SyncWorkerInterface&) = delete;

  virtual ~SyncWorkerInterface() {}

  // Initializes SyncWorkerInterface after constructions of some member classes.
  virtual void Initialize(
      std::unique_ptr<SyncEngineContext> sync_engine_context) = 0;

  // See RemoteFileSyncService for the details.
  virtual void RegisterOrigin(const GURL& origin,
                              SyncStatusCallback callback) = 0;
  virtual void EnableOrigin(const GURL& origin,
                            SyncStatusCallback callback) = 0;
  virtual void DisableOrigin(const GURL& origin,
                             SyncStatusCallback callback) = 0;
  virtual void UninstallOrigin(const GURL& origin,
                               RemoteFileSyncService::UninstallFlag flag,
                               SyncStatusCallback callback) = 0;
  virtual void ProcessRemoteChange(SyncFileCallback callback) = 0;
  virtual void SetRemoteChangeProcessor(
      RemoteChangeProcessorOnWorker* remote_change_processor_on_worker) = 0;
  virtual RemoteServiceState GetCurrentState() const = 0;
  virtual void GetOriginStatusMap(
      RemoteFileSyncService::StatusMapCallback callback) = 0;
  virtual base::Value::List DumpFiles(const GURL& origin) = 0;
  virtual base::Value::List DumpDatabase() = 0;
  virtual void SetSyncEnabled(bool enabled) = 0;
  virtual void PromoteDemotedChanges(base::OnceClosure callback) = 0;

  // See LocalChangeProcessor for the details.
  virtual void ApplyLocalChange(const FileChange& local_change,
                                const base::FilePath& local_path,
                                const SyncFileMetadata& local_metadata,
                                const storage::FileSystemURL& url,
                                SyncStatusCallback callback) = 0;

  virtual void ActivateService(RemoteServiceState service_state,
                               const std::string& description) = 0;
  virtual void DeactivateService(const std::string& description) = 0;

  virtual void DetachFromSequence() = 0;

  virtual void AddObserver(Observer* observer) = 0;

 private:
  friend class SyncEngineTest;
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_INTERFACE_H_
