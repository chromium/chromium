// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_tracker.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_action.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "components/drive/drive_notification_observer.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace base {
class SequencedTaskRunner;
}

namespace drive {
class DriveServiceInterface;
class DriveNotificationManager;
class DriveUploaderInterface;
}

namespace extensions {
class ExtensionRegistry;
class ExtensionServiceInterface;
}

namespace leveldb {
class Env;
}

namespace network {
class SharedURLLoaderFactory;
}

namespace sync_file_system {

class RemoteChangeProcessor;
class SyncFileSystemTest;

namespace drive_backend {

class DriveServiceWrapper;
class DriveUploaderWrapper;
class RemoteChangeProcessorOnWorker;
class RemoteChangeProcessorWrapper;
class SyncWorkerInterface;

class SyncEngine
    : public RemoteFileSyncService,
      public LocalChangeProcessor,
      public drive::DriveNotificationObserver,
      public drive::DriveServiceObserver,
      public signin::IdentityManager::Observer,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  typedef RemoteFileSyncService::Observer SyncServiceObserver;

  class DriveServiceFactory {
   public:
    DriveServiceFactory() {}
    virtual ~DriveServiceFactory() {}
    virtual std::unique_ptr<drive::DriveServiceInterface> CreateDriveService(
        signin::IdentityManager* identity_manager,
        scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
        base::SequencedTaskRunner* blocking_task_runner);

   private:
    DISALLOW_COPY_AND_ASSIGN(DriveServiceFactory);
  };

  static std::unique_ptr<SyncEngine> CreateForBrowserContext(
      content::BrowserContext* context,
      TaskLogger* task_logger);
  static void AppendDependsOnFactories(
      std::set<BrowserContextKeyedServiceFactory*>* factories);

  ~SyncEngine() override;
  void Reset();

  // Can be called more than once.
  void Initialize();

  void InitializeForTesting(
      std::unique_ptr<drive::DriveServiceInterface> drive_service,
      std::unique_ptr<drive::DriveUploaderInterface> drive_uploader,
      std::unique_ptr<SyncWorkerInterface> sync_worker);
  void InitializeInternal(
      std::unique_ptr<drive::DriveServiceInterface> drive_service,
      std::unique_ptr<drive::DriveUploaderInterface> drive_uploader,
      std::unique_ptr<SyncWorkerInterface> sync_worker);

  // RemoteFileSyncService overrides.
  void AddServiceObserver(SyncServiceObserver* observer) override;
  void AddFileStatusObserver(FileStatusObserver* observer) override;
  void RegisterOrigin(const GURL& origin,
                      const SyncStatusCallback& callback) override;
  void EnableOrigin(const GURL& origin,
                    const SyncStatusCallback& callback) override;
  void DisableOrigin(const GURL& origin,
                     const SyncStatusCallback& callback) override;
  void UninstallOrigin(const GURL& origin,
                       UninstallFlag flag,
                       const SyncStatusCallback& callback) override;
  void ProcessRemoteChange(const SyncFileCallback& callback) override;
  void SetRemoteChangeProcessor(RemoteChangeProcessor* processor) override;
  LocalChangeProcessor* GetLocalChangeProcessor() override;
  RemoteServiceState GetCurrentState() const override;
  void GetOriginStatusMap(const StatusMapCallback& callback) override;
  void DumpFiles(const GURL& origin, const ListCallback& callback) override;
  void DumpDatabase(const ListCallback& callback) override;
  void SetSyncEnabled(bool enabled) override;
  void PromoteDemotedChanges(const base::Closure& callback) override;

  // LocalChangeProcessor overrides.
  void ApplyLocalChange(const FileChange& local_change,
                        const base::FilePath& local_path,
                        const SyncFileMetadata& local_metadata,
                        const storage::FileSystemURL& url,
                        const SyncStatusCallback& callback) override;

  // drive::DriveNotificationObserver overrides.
  void OnNotificationReceived(
      const std::map<std::string, int64_t>& invalidations) override;
  void OnNotificationTimerFired() override;
  void OnPushNotificationEnabled(bool enabled) override;

  // drive::DriveServiceObserver overrides.
  void OnReadyToSendRequests() override;
  void OnRefreshTokenInvalid() override;

  // network::NetworkConnectionTracker::NetworkConnectionObserver overrides.
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // IdentityManager::Observer overrides.
  void OnPrimaryAccountSet(
      const CoreAccountInfo& primary_account_info) override;
  void OnPrimaryAccountCleared(
      const CoreAccountInfo& previous_primary_account_info) override;

 private:
  class WorkerObserver;

  friend class DriveBackendSyncTest;
  friend class SyncEngineTest;
  friend class sync_file_system::SyncFileSystemTest;

  SyncEngine(const scoped_refptr<base::SingleThreadTaskRunner>& ui_task_runner,
             const scoped_refptr<base::SequencedTaskRunner>& worker_task_runner,
             const scoped_refptr<base::SequencedTaskRunner>& drive_task_runner,
             const base::FilePath& sync_file_system_dir,
             TaskLogger* task_logger,
             drive::DriveNotificationManager* notification_manager,
             extensions::ExtensionServiceInterface* extension_service,
             extensions::ExtensionRegistry* extension_registry,
             signin::IdentityManager* identity_manager,
             scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
             std::unique_ptr<DriveServiceFactory> drive_service_factory,
             leveldb::Env* env_override);

  // Called by WorkerObserver.
  void OnPendingFileListUpdated(int item_count);
  void OnFileStatusChanged(const storage::FileSystemURL& url,
                           SyncFileType file_type,
                           SyncFileStatus file_status,
                           SyncAction sync_action,
                           SyncDirection direction);
  void UpdateServiceState(RemoteServiceState state,
                          const std::string& description);

  SyncStatusCallback TrackCallback(const SyncStatusCallback& callback);

  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> drive_task_runner_;

  const base::FilePath sync_file_system_dir_;
  TaskLogger* task_logger_;

  // These external services are not owned by SyncEngine.
  // The owner of the SyncEngine is responsible for their lifetime.
  // I.e. the owner should declare the dependency explicitly by calling
  // KeyedService::DependsOn().
  drive::DriveNotificationManager* notification_manager_;
  extensions::ExtensionServiceInterface* extension_service_;
  extensions::ExtensionRegistry* extension_registry_;
  signin::IdentityManager* identity_manager_;

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  std::unique_ptr<DriveServiceFactory> drive_service_factory_;

  std::unique_ptr<drive::DriveServiceInterface> drive_service_;
  std::unique_ptr<DriveServiceWrapper> drive_service_wrapper_;
  std::unique_ptr<drive::DriveUploaderInterface> drive_uploader_;
  std::unique_ptr<DriveUploaderWrapper> drive_uploader_wrapper_;

  RemoteChangeProcessor* remote_change_processor_;  // Not owned.
  std::unique_ptr<RemoteChangeProcessorWrapper>
      remote_change_processor_wrapper_;
  // Delete this on worker.
  std::unique_ptr<RemoteChangeProcessorOnWorker>
      remote_change_processor_on_worker_;

  RemoteServiceState service_state_;
  bool has_refresh_token_;
  bool network_available_;
  bool sync_enabled_;

  // Delete them on worker.
  std::unique_ptr<WorkerObserver> worker_observer_;
  std::unique_ptr<SyncWorkerInterface> sync_worker_;

  base::ObserverList<SyncServiceObserver>::Unchecked service_observers_;
  base::ObserverList<FileStatusObserver>::Unchecked file_status_observers_;
  leveldb::Env* env_override_;

  CallbackTracker callback_tracker_;

  base::WeakPtrFactory<SyncEngine> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(SyncEngine);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_SYNC_ENGINE_H_
