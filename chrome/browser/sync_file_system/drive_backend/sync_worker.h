// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task_manager.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_worker_interface.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/task_logger.h"

class GURL;

namespace base {
class ListValue;
}

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace extensions {
class ExtensionRegistry;
class ExtensionServiceInterface;
}

namespace storage {
class FileSystemURL;
}

namespace leveldb {
class Env;
}

namespace sync_file_system {

class FileChange;
class SyncFileMetadata;

namespace drive_backend {

class LocalToRemoteSyncer;
class MetadataDatabase;
class RemoteChangeProcessorOnWorker;
class RemoteToLocalSyncer;
class SyncEngineContext;
class SyncEngineInitializer;

class SyncWorker : public SyncWorkerInterface,
                   public SyncTaskManager::Client {
 public:
  SyncWorker(const base::FilePath& base_dir,
             const base::WeakPtr<extensions::ExtensionServiceInterface>&
                 extension_service,
             extensions::ExtensionRegistry* extension_registry,
             leveldb::Env* env_override);

  ~SyncWorker() override;

  void Initialize(std::unique_ptr<SyncEngineContext> context) override;

  // SyncTaskManager::Client overrides
  void MaybeScheduleNextTask() override;
  void NotifyLastOperationStatus(SyncStatusCode sync_status,
                                 bool used_network) override;
  void RecordTaskLog(std::unique_ptr<TaskLogger::TaskLog> task_log) override;

  // SyncWorkerInterface overrides
  void RegisterOrigin(const GURL& origin,
                      const SyncStatusCallback& callback) override;
  void EnableOrigin(const GURL& origin,
                    const SyncStatusCallback& callback) override;
  void DisableOrigin(const GURL& origin,
                     const SyncStatusCallback& callback) override;
  void UninstallOrigin(const GURL& origin,
                       RemoteFileSyncService::UninstallFlag flag,
                       const SyncStatusCallback& callback) override;
  void ProcessRemoteChange(const SyncFileCallback& callback) override;
  void SetRemoteChangeProcessor(RemoteChangeProcessorOnWorker*
                                    remote_change_processor_on_worker) override;
  RemoteServiceState GetCurrentState() const override;
  void GetOriginStatusMap(
      const RemoteFileSyncService::StatusMapCallback& callback) override;
  std::unique_ptr<base::ListValue> DumpFiles(const GURL& origin) override;
  std::unique_ptr<base::ListValue> DumpDatabase() override;
  void SetSyncEnabled(bool enabled) override;
  void PromoteDemotedChanges(const base::Closure& callback) override;
  void ApplyLocalChange(const FileChange& local_change,
                        const base::FilePath& local_path,
                        const SyncFileMetadata& local_metadata,
                        const storage::FileSystemURL& url,
                        const SyncStatusCallback& callback) override;
  void ActivateService(RemoteServiceState service_state,
                       const std::string& description) override;
  void DeactivateService(const std::string& description) override;
  void DetachFromSequence() override;
  void AddObserver(Observer* observer) override;

 private:
  friend class DriveBackendSyncTest;
  friend class SyncWorkerTest;

  enum AppStatus {
    APP_STATUS_ENABLED,
    APP_STATUS_DISABLED,
    APP_STATUS_UNINSTALLED,
  };

  using AppStatusMap = std::unordered_map<std::string, AppStatus>;

  void DoDisableApp(const std::string& app_id,
                    const SyncStatusCallback& callback);
  void DoEnableApp(const std::string& app_id,
                   const SyncStatusCallback& callback);

  void PostInitializeTask();
  void DidInitialize(SyncEngineInitializer* initializer,
                     SyncStatusCode status);
  void UpdateRegisteredApps();
  static void QueryAppStatusOnUIThread(
      const base::WeakPtr<extensions::ExtensionServiceInterface>&
          extension_service_ptr,
      extensions::ExtensionRegistry* extension_registry,
      const std::vector<std::string>* app_ids,
      AppStatusMap* status,
      const base::Closure& callback);
  void DidQueryAppStatus(const AppStatusMap* app_status);
  void DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                              const SyncFileCallback& callback,
                              SyncStatusCode status);
  void DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                           const SyncStatusCallback& callback,
                           SyncStatusCode status);

  // Returns true if a FetchChanges task is scheduled.
  bool MaybeStartFetchChanges();
  void DidResolveConflict(SyncStatusCode status);
  void DidFetchChanges(SyncStatusCode status);

  void UpdateServiceStateFromSyncStatusCode(SyncStatusCode state,
                                            bool used_network);
  void UpdateServiceState(RemoteServiceState state,
                          const std::string& description);

  void CallOnIdleForTesting(const base::Closure& callback);

  drive::DriveServiceInterface* GetDriveService();
  drive::DriveUploaderInterface* GetDriveUploader();
  MetadataDatabase* GetMetadataDatabase();

  base::FilePath base_dir_;

  leveldb::Env* env_override_;

  // Sync with SyncEngine.
  RemoteServiceState service_state_;

  bool should_check_conflict_;
  bool should_check_remote_change_;
  bool listing_remote_changes_;
  base::TimeTicks time_to_check_changes_;

  bool sync_enabled_;
  base::Closure call_on_idle_callback_;

  std::unique_ptr<SyncTaskManager> task_manager_;

  base::WeakPtr<extensions::ExtensionServiceInterface> extension_service_;
  // Only guaranteed to be valid if |extension_service_| is not null.
  extensions::ExtensionRegistry* extension_registry_;

  std::unique_ptr<SyncEngineContext> context_;
  base::ObserverList<Observer>::Unchecked observers_;

  base::SequenceChecker sequence_checker_;

  base::WeakPtrFactory<SyncWorker> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SyncWorker);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_WORKER_H_
