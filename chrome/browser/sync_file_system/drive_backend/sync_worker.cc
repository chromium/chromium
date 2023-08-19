// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/sync_worker.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/observer_list.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/conflict_resolver.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_backend_constants.h"
#include "chrome/browser/sync_file_system/drive_backend/list_changes_task.h"
#include "chrome/browser/sync_file_system/drive_backend/local_to_remote_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"
#include "chrome/browser/sync_file_system/drive_backend/register_app_task.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_change_processor_on_worker.h"
#include "chrome/browser/sync_file_system/drive_backend/remote_to_local_syncer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_context.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_engine_initializer.h"
#include "chrome/browser/sync_file_system/drive_backend/sync_task.h"
#include "chrome/browser/sync_file_system/drive_backend/uninstall_app_task.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/drive/service/drive_service_interface.h"
#include "extensions/browser/extension_registry.h"
#include "storage/common/file_system/file_system_util.h"

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

namespace {

void InvokeIdleCallback(base::RepeatingClosure idle_callback,
                        SyncStatusCallback callback) {
  idle_callback.Run();
  std::move(callback).Run(SYNC_STATUS_OK);
}

}  // namespace

SyncWorker::SyncWorker(
    const base::FilePath& base_dir,
    const base::WeakPtr<extensions::ExtensionServiceInterface>&
        extension_service,
    extensions::ExtensionRegistry* extension_registry,
    leveldb::Env* env_override)
    : base_dir_(base_dir),
      env_override_(env_override),
      service_state_(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE),
      should_check_conflict_(true),
      should_check_remote_change_(true),
      listing_remote_changes_(false),
      sync_enabled_(false),
      extension_service_(extension_service),
      extension_registry_(extension_registry) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(base_dir_.IsAbsolute());
}

SyncWorker::~SyncWorker() {
  observers_.Clear();
}

void SyncWorker::Initialize(std::unique_ptr<SyncEngineContext> context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!task_manager_);

  context_ = std::move(context);

  task_manager_ = std::make_unique<SyncTaskManager>(
      weak_ptr_factory_.GetWeakPtr(), 0 /* maximum_background_task */,
      context_->GetWorkerTaskRunner());
  task_manager_->Initialize(SYNC_STATUS_OK);

  PostInitializeTask();
}

void SyncWorker::RegisterOrigin(const GURL& origin,
                                SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!GetMetadataDatabase())
    PostInitializeTask();

  std::unique_ptr<RegisterAppTask> task(
      new RegisterAppTask(context_.get(), origin.host()));
  if (task->CanFinishImmediately()) {
    std::move(callback).Run(SYNC_STATUS_OK);
    return;
  }

  task_manager_->ScheduleSyncTask(FROM_HERE, std::move(task),
                                  SyncTaskManager::PRIORITY_HIGH,
                                  std::move(callback));
}

void SyncWorker::EnableOrigin(const GURL& origin, SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_manager_->ScheduleTask(
      FROM_HERE,
      base::BindOnce(&SyncWorker::DoEnableApp, weak_ptr_factory_.GetWeakPtr(),
                     origin.host()),
      SyncTaskManager::PRIORITY_HIGH, std::move(callback));
}

void SyncWorker::DisableOrigin(const GURL& origin,
                               SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_manager_->ScheduleTask(
      FROM_HERE,
      base::BindOnce(&SyncWorker::DoDisableApp, weak_ptr_factory_.GetWeakPtr(),
                     origin.host()),
      SyncTaskManager::PRIORITY_HIGH, std::move(callback));
}

void SyncWorker::UninstallOrigin(const GURL& origin,
                                 RemoteFileSyncService::UninstallFlag flag,
                                 SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  task_manager_->ScheduleSyncTask(
      FROM_HERE,
      std::make_unique<UninstallAppTask>(context_.get(), origin.host(), flag),
      SyncTaskManager::PRIORITY_HIGH, std::move(callback));
}

void SyncWorker::ProcessRemoteChange(SyncFileCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoteToLocalSyncer* syncer = new RemoteToLocalSyncer(context_.get());
  task_manager_->ScheduleSyncTask(
      FROM_HERE, std::unique_ptr<SyncTask>(syncer),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorker::DidProcessRemoteChange,
                     weak_ptr_factory_.GetWeakPtr(), syncer,
                     std::move(callback)));
}

void SyncWorker::SetRemoteChangeProcessor(
    RemoteChangeProcessorOnWorker* remote_change_processor_on_worker) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  context_->SetRemoteChangeProcessor(remote_change_processor_on_worker);
}

RemoteServiceState SyncWorker::GetCurrentState() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sync_enabled_)
    return REMOTE_SERVICE_DISABLED;
  return service_state_;
}

void SyncWorker::GetOriginStatusMap(
    RemoteFileSyncService::StatusMapCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!GetMetadataDatabase())
    return;

  std::vector<std::string> app_ids;
  GetMetadataDatabase()->GetRegisteredAppIDs(&app_ids);

  std::unique_ptr<RemoteFileSyncService::OriginStatusMap> status_map(
      new RemoteFileSyncService::OriginStatusMap);
  for (std::vector<std::string>::const_iterator itr = app_ids.begin();
       itr != app_ids.end(); ++itr) {
    const std::string& app_id = *itr;
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);
    (*status_map)[origin] =
        GetMetadataDatabase()->IsAppEnabled(app_id) ? "Enabled" : "Disabled";
  }

  std::move(callback).Run(std::move(status_map));
}

base::Value::List SyncWorker::DumpFiles(const GURL& origin) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!GetMetadataDatabase())
    return base::Value::List();
  return GetMetadataDatabase()->DumpFiles(origin.host());
}

base::Value::List SyncWorker::DumpDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!GetMetadataDatabase())
    return base::Value::List();
  return GetMetadataDatabase()->DumpDatabase();
}

void SyncWorker::SetSyncEnabled(bool enabled) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sync_enabled_ == enabled)
    return;

  RemoteServiceState old_state = GetCurrentState();
  sync_enabled_ = enabled;
  if (old_state == GetCurrentState())
    return;

  for (auto& observer : observers_) {
    observer.UpdateServiceState(
        GetCurrentState(), enabled ? "Sync is enabled" : "Sync is disabled");
  }
}

void SyncWorker::PromoteDemotedChanges(base::OnceClosure callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MetadataDatabase* metadata_db = GetMetadataDatabase();
  if (metadata_db && metadata_db->HasDemotedDirtyTracker()) {
    metadata_db->PromoteDemotedTrackers();
    for (auto& observer : observers_)
      observer.OnPendingFileListUpdated(metadata_db->CountDirtyTracker());
  }
  std::move(callback).Run();
}

void SyncWorker::ApplyLocalChange(const FileChange& local_change,
                                  const base::FilePath& local_path,
                                  const SyncFileMetadata& local_metadata,
                                  const storage::FileSystemURL& url,
                                  SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  LocalToRemoteSyncer* syncer = new LocalToRemoteSyncer(
      context_.get(), local_metadata, local_change, local_path, url);
  task_manager_->ScheduleSyncTask(
      FROM_HERE, std::unique_ptr<SyncTask>(syncer),
      SyncTaskManager::PRIORITY_MED,
      base::BindOnce(&SyncWorker::DidApplyLocalChange,
                     weak_ptr_factory_.GetWeakPtr(), syncer,
                     std::move(callback)));
}

void SyncWorker::MaybeScheduleNextTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return;

  // TODO(tzik): Notify observer of OnRemoteChangeQueueUpdated.
  // TODO(tzik): Add an interface to get the number of dirty trackers to
  // MetadataDatabase.

  if (MaybeStartFetchChanges())
    return;

  if (!call_on_idle_callback_.is_null()) {
    // Consumes the callback.
    std::move(call_on_idle_callback_).Run();
  }
}

void SyncWorker::NotifyLastOperationStatus(
    SyncStatusCode status,
    bool used_network) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  UpdateServiceStateFromSyncStatusCode(status, used_network);

  if (GetMetadataDatabase()) {
    for (auto& observer : observers_) {
      observer.OnPendingFileListUpdated(
          GetMetadataDatabase()->CountDirtyTracker());
    }
  }
}

void SyncWorker::RecordTaskLog(std::unique_ptr<TaskLogger::TaskLog> task_log) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  context_->GetUITaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&TaskLogger::RecordLog, context_->GetTaskLogger(),
                     std::move(task_log)));
}

void SyncWorker::ActivateService(RemoteServiceState service_state,
                                 const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateServiceState(service_state, description);
  if (!GetMetadataDatabase()) {
    PostInitializeTask();
    return;
  }

  should_check_remote_change_ = true;
  MaybeScheduleNextTask();
}

void SyncWorker::DeactivateService(const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE, description);
}

void SyncWorker::DetachFromSequence() {
  task_manager_->DetachFromSequence();
  context_->DetachFromSequence();
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void SyncWorker::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SyncWorker::DoDisableApp(const std::string& app_id,
                              SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!GetMetadataDatabase()) {
    std::move(callback).Run(SYNC_STATUS_OK);
    return;
  }

  SyncStatusCode status = GetMetadataDatabase()->DisableApp(app_id);
  std::move(callback).Run(status);
}

void SyncWorker::DoEnableApp(const std::string& app_id,
                             SyncStatusCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!GetMetadataDatabase()) {
    std::move(callback).Run(SYNC_STATUS_OK);
    return;
  }

  SyncStatusCode status = GetMetadataDatabase()->EnableApp(app_id);
  std::move(callback).Run(status);
}

void SyncWorker::PostInitializeTask() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!GetMetadataDatabase());

  // This initializer task may not run if MetadataDatabase in context_ is
  // already initialized when it runs.
  SyncEngineInitializer* initializer =
      new SyncEngineInitializer(context_.get(),
                                base_dir_.Append(kDatabaseName),
                                env_override_);
  task_manager_->ScheduleSyncTask(
      FROM_HERE, std::unique_ptr<SyncTask>(initializer),
      SyncTaskManager::PRIORITY_HIGH,
      base::BindOnce(&SyncWorker::DidInitialize, weak_ptr_factory_.GetWeakPtr(),
                     initializer));
}

void SyncWorker::DidInitialize(SyncEngineInitializer* initializer,
                               SyncStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == SYNC_STATUS_ACCESS_FORBIDDEN) {
    UpdateServiceState(REMOTE_SERVICE_ACCESS_FORBIDDEN, "Access forbidden");
    return;
  }
  if (status != SYNC_STATUS_OK) {
    UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
                       "Could not initialize remote service");
    return;
  }

  std::unique_ptr<MetadataDatabase> metadata_database =
      initializer->PassMetadataDatabase();
  if (metadata_database) {
    context_->SetMetadataDatabase(std::move(metadata_database));
    return;
  }

  UpdateServiceState(REMOTE_SERVICE_OK, std::string());
  UpdateRegisteredApps();
}

void SyncWorker::UpdateRegisteredApps() {
  MetadataDatabase* metadata_db = GetMetadataDatabase();
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(metadata_db);

  std::unique_ptr<std::vector<std::string>> app_ids(
      new std::vector<std::string>);
  metadata_db->GetRegisteredAppIDs(app_ids.get());

  AppStatusMap* app_status = new AppStatusMap;
  base::RepeatingClosure callback = base::BindRepeating(
      &SyncWorker::DidQueryAppStatus, weak_ptr_factory_.GetWeakPtr(),
      base::Owned(app_status));

  context_->GetUITaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &SyncWorker::QueryAppStatusOnUIThread, extension_service_,
          // This is protected by checking the extension_service_
          // weak pointer, since the underlying ExtensionService
          // also relies on the ExtensionRegistry.
          base::Unretained(extension_registry_), base::Owned(app_ids.release()),
          app_status,
          RelayCallbackToTaskRunner(context_->GetWorkerTaskRunner(), FROM_HERE,
                                    std::move(callback))));
}

void SyncWorker::QueryAppStatusOnUIThread(
    const base::WeakPtr<extensions::ExtensionServiceInterface>&
        extension_service_ptr,
    extensions::ExtensionRegistry* extension_registry,
    const std::vector<std::string>* app_ids,
    AppStatusMap* status,
    base::OnceClosure callback) {
  extensions::ExtensionServiceInterface* extension_service =
      extension_service_ptr.get();
  if (!extension_service) {
    std::move(callback).Run();
    return;
  }

  for (auto itr = app_ids->begin(); itr != app_ids->end(); ++itr) {
    const std::string& app_id = *itr;
    if (!extension_registry->GetInstalledExtension(app_id))
      (*status)[app_id] = APP_STATUS_UNINSTALLED;
    else if (!extension_service->IsExtensionEnabled(app_id))
      (*status)[app_id] = APP_STATUS_DISABLED;
    else
      (*status)[app_id] = APP_STATUS_ENABLED;
  }

  std::move(callback).Run();
}

void SyncWorker::DidQueryAppStatus(const AppStatusMap* app_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MetadataDatabase* metadata_db = GetMetadataDatabase();
  DCHECK(metadata_db);

  // Update the status of every origin using status from ExtensionService.
  for (auto itr = app_status->begin(); itr != app_status->end(); ++itr) {
    const std::string& app_id = itr->first;
    GURL origin = extensions::Extension::GetBaseURLFromExtensionId(app_id);

    if (itr->second == APP_STATUS_UNINSTALLED) {
      // Extension has been uninstalled.
      // (At this stage we can't know if it was unpacked extension or not,
      // so just purge the remote folder.)
      UninstallOrigin(origin, RemoteFileSyncService::UNINSTALL_AND_PURGE_REMOTE,
                      base::DoNothing());
      continue;
    }

    FileTracker tracker;
    if (!metadata_db->FindAppRootTracker(app_id, &tracker)) {
      // App will register itself on first run.
      continue;
    }

    DCHECK(itr->second == APP_STATUS_ENABLED ||
           itr->second == APP_STATUS_DISABLED);
    bool is_app_enabled = (itr->second == APP_STATUS_ENABLED);
    bool is_app_root_tracker_enabled =
        (tracker.tracker_kind() == TRACKER_KIND_APP_ROOT);
    if (is_app_enabled && !is_app_root_tracker_enabled)
      EnableOrigin(origin, base::DoNothing());
    else if (!is_app_enabled && is_app_root_tracker_enabled)
      DisableOrigin(origin, base::DoNothing());
  }
}

void SyncWorker::DidProcessRemoteChange(RemoteToLocalSyncer* syncer,
                                        SyncFileCallback callback,
                                        SyncStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (syncer->is_sync_root_deletion()) {
    MetadataDatabase::ClearDatabase(context_->PassMetadataDatabase());
    PostInitializeTask();
    std::move(callback).Run(status, syncer->url());
    return;
  }

  if (status == SYNC_STATUS_OK) {
    if (syncer->sync_action() != SYNC_ACTION_NONE &&
        syncer->url().is_valid()) {
      for (auto& observer : observers_) {
        observer.OnFileStatusChanged(
            syncer->url(), syncer->file_type(), SYNC_FILE_STATUS_SYNCED,
            syncer->sync_action(), SYNC_DIRECTION_REMOTE_TO_LOCAL);
      }
    }

    if (syncer->sync_action() == SYNC_ACTION_DELETED &&
        syncer->url().is_valid() &&
        storage::VirtualPath::IsRootPath(syncer->url().path())) {
      RegisterOrigin(syncer->url().origin().GetURL(), base::DoNothing());
    }
    should_check_conflict_ = true;
  }
  std::move(callback).Run(status, syncer->url());
}

void SyncWorker::DidApplyLocalChange(LocalToRemoteSyncer* syncer,
                                     SyncStatusCallback callback,
                                     SyncStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if ((status == SYNC_STATUS_OK || status == SYNC_STATUS_RETRY) &&
      syncer->url().is_valid() &&
      syncer->sync_action() != SYNC_ACTION_NONE) {
    storage::FileSystemURL updated_url = syncer->url();
    if (!syncer->target_path().empty()) {
      updated_url = CreateSyncableFileSystemURL(syncer->url().origin().GetURL(),
                                                syncer->target_path());
    }
    for (auto& observer : observers_) {
      observer.OnFileStatusChanged(
          updated_url, syncer->file_type(), SYNC_FILE_STATUS_SYNCED,
          syncer->sync_action(), SYNC_DIRECTION_LOCAL_TO_REMOTE);
    }
  }

  if (status == SYNC_STATUS_UNKNOWN_ORIGIN && syncer->url().is_valid())
    RegisterOrigin(syncer->url().origin().GetURL(), base::DoNothing());

  if (syncer->needs_remote_change_listing() &&
      !listing_remote_changes_) {
    task_manager_->ScheduleSyncTask(
        FROM_HERE,
        std::unique_ptr<SyncTask>(new ListChangesTask(context_.get())),
        SyncTaskManager::PRIORITY_HIGH,
        base::BindOnce(&SyncWorker::DidFetchChanges,
                       weak_ptr_factory_.GetWeakPtr()));
    should_check_remote_change_ = false;
    listing_remote_changes_ = true;
    time_to_check_changes_ =
        base::TimeTicks::Now() + base::Seconds(kListChangesRetryDelaySeconds);
  }

  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;

  std::move(callback).Run(status);
}

bool SyncWorker::MaybeStartFetchChanges() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (GetCurrentState() == REMOTE_SERVICE_DISABLED)
    return false;

  if (!GetMetadataDatabase())
    return false;

  if (listing_remote_changes_)
    return false;

  base::TimeTicks now = base::TimeTicks::Now();
  if (!should_check_remote_change_ && now < time_to_check_changes_) {
    if (!GetMetadataDatabase()->HasDirtyTracker() &&
        should_check_conflict_) {
      should_check_conflict_ = false;
      return task_manager_->ScheduleSyncTaskIfIdle(
          FROM_HERE,
          std::unique_ptr<SyncTask>(new ConflictResolver(context_.get())),
          base::BindOnce(&SyncWorker::DidResolveConflict,
                         weak_ptr_factory_.GetWeakPtr()));
    }
    return false;
  }

  if (task_manager_->ScheduleSyncTaskIfIdle(
          FROM_HERE,
          std::unique_ptr<SyncTask>(new ListChangesTask(context_.get())),
          base::BindOnce(&SyncWorker::DidFetchChanges,
                         weak_ptr_factory_.GetWeakPtr()))) {
    should_check_remote_change_ = false;
    listing_remote_changes_ = true;
    time_to_check_changes_ = now + base::Seconds(kListChangesRetryDelaySeconds);
    return true;
  }
  return false;
}

void SyncWorker::DidResolveConflict(SyncStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == SYNC_STATUS_OK || status == SYNC_STATUS_RETRY)
    should_check_conflict_ = true;
}

void SyncWorker::DidFetchChanges(SyncStatusCode status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (status == SYNC_STATUS_OK)
    should_check_conflict_ = true;
  listing_remote_changes_ = false;
}

void SyncWorker::UpdateServiceStateFromSyncStatusCode(
    SyncStatusCode status,
    bool used_network) {
  switch (status) {
    case SYNC_STATUS_OK:
      if (used_network)
        UpdateServiceState(REMOTE_SERVICE_OK, std::string());
      break;

    // Authentication error.
    case SYNC_STATUS_AUTHENTICATION_FAILED:
      UpdateServiceState(REMOTE_SERVICE_AUTHENTICATION_REQUIRED,
                         "Authentication required");
      break;

    // OAuth token error.
    case SYNC_STATUS_ACCESS_FORBIDDEN:
      UpdateServiceState(REMOTE_SERVICE_ACCESS_FORBIDDEN,
                         "Access forbidden");
      break;

    // Errors which could make the service temporarily unavailable.
    case SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE:
    case SYNC_STATUS_NETWORK_ERROR:
    case SYNC_STATUS_ABORT:
    case SYNC_STATUS_FAILED:
      UpdateServiceState(REMOTE_SERVICE_TEMPORARY_UNAVAILABLE,
                         "Network or temporary service error.");
      break;

    // Errors which would require manual user intervention to resolve.
    case SYNC_DATABASE_ERROR_CORRUPTION:
    case SYNC_DATABASE_ERROR_IO_ERROR:
    case SYNC_DATABASE_ERROR_FAILED:
      UpdateServiceState(REMOTE_SERVICE_DISABLED,
                         "Unrecoverable database error");
      break;

    default:
      // Other errors don't affect service state
      break;
  }
}

void SyncWorker::UpdateServiceState(RemoteServiceState state,
                                    const std::string& description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  RemoteServiceState old_state = GetCurrentState();
  service_state_ = state;

  if (old_state == GetCurrentState())
    return;

  util::Log(logging::LOGGING_VERBOSE, FROM_HERE,
            "Service state changed: %d->%d: %s", old_state, GetCurrentState(),
            description.c_str());

  for (auto& observer : observers_)
    observer.UpdateServiceState(GetCurrentState(), description);
}

void SyncWorker::CallOnIdleForTesting(const base::RepeatingClosure& callback) {
  if (task_manager_->ScheduleTaskIfIdle(
          FROM_HERE, base::BindOnce(&InvokeIdleCallback, callback),
          base::DoNothing()))
    return;
  call_on_idle_callback_ =
      base::BindOnce(&SyncWorker::CallOnIdleForTesting,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback));
}

drive::DriveServiceInterface* SyncWorker::GetDriveService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_->GetDriveService();
}

drive::DriveUploaderInterface* SyncWorker::GetDriveUploader() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_->GetDriveUploader();
}

MetadataDatabase* SyncWorker::GetMetadataDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return context_->GetMetadataDatabase();
}

}  // namespace drive_backend
}  // namespace sync_file_system
