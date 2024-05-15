// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/sync_file_system_service.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/extension_sync_event_observer.h"
#include "chrome/browser/apps/platform_apps/api/sync_file_system/sync_file_system_api_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_direction.h"
#include "chrome/browser/sync_file_system/sync_event_observer.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_file_status.h"
#include "chrome/browser/sync_file_system/sync_process_runner.h"
#include "chrome/browser/sync_file_system/sync_status_code.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/unloaded_extension_reason.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "storage/browser/file_system/file_system_context.h"
#include "url/gurl.h"

using content::BrowserThread;
using extensions::Extension;
using extensions::ExtensionPrefs;
using extensions::ExtensionRegistry;
using storage::FileSystemURL;
using storage::FileSystemURLSet;

namespace sync_file_system {

namespace {

const char kLocalSyncName[] = "Local sync";
const char kRemoteSyncName[] = "Remote sync";

SyncServiceState RemoteStateToSyncServiceState(
    RemoteServiceState state) {
  switch (state) {
    case REMOTE_SERVICE_OK:
      return SYNC_SERVICE_RUNNING;
    case REMOTE_SERVICE_TEMPORARY_UNAVAILABLE:
      return SYNC_SERVICE_TEMPORARY_UNAVAILABLE;
    case REMOTE_SERVICE_AUTHENTICATION_REQUIRED:
      return SYNC_SERVICE_AUTHENTICATION_REQUIRED;
    case REMOTE_SERVICE_ACCESS_FORBIDDEN:
      return SYNC_SERVICE_TEMPORARY_UNAVAILABLE;
    case REMOTE_SERVICE_DISABLED:
      return SYNC_SERVICE_DISABLED;
    case REMOTE_SERVICE_STATE_MAX:
      NOTREACHED_IN_MIGRATION();
  }
  NOTREACHED_IN_MIGRATION() << "Unknown remote service state: " << state;
  return SYNC_SERVICE_DISABLED;
}

void DidHandleUninstalledEvent(const GURL& origin, SyncStatusCode code) {
  if (code != SYNC_STATUS_OK && code != SYNC_STATUS_UNKNOWN_ORIGIN) {
    util::Log(logging::LOGGING_WARNING, FROM_HERE,
              "Failed to uninstall origin for uninstall event: %s",
              origin.spec().c_str());
  }
}

void DidHandleUnloadedEvent(const GURL& origin, SyncStatusCode code) {
  if (code != SYNC_STATUS_OK && code != SYNC_STATUS_UNKNOWN_ORIGIN) {
    util::Log(logging::LOGGING_WARNING, FROM_HERE,
              "Failed to disable origin for unload event: %s",
              origin.spec().c_str());
  }
}

void DidHandleLoadEvent(
    const GURL& origin,
    SyncStatusCode code) {
  if (code != SYNC_STATUS_OK) {
    util::Log(logging::LOGGING_WARNING, FROM_HERE,
              "Failed to enable origin for load event: %s",
              origin.spec().c_str());
  }
}

std::string SyncFileStatusToString(SyncFileStatus sync_file_status) {
  return chrome_apps::api::sync_file_system::ToString(
      chrome_apps::api::SyncFileStatusToExtensionEnum(sync_file_status));
}

// We need this indirection because WeakPtr can only be bound to methods
// without a return value.
LocalChangeProcessor* GetLocalChangeProcessorAdapter(
    base::WeakPtr<SyncFileSystemService> service,
    const GURL& origin) {
  if (!service)
    return nullptr;
  return service->GetLocalChangeProcessor(origin);
}

void SetFileStatusFromSyncStatus(base::Value::Dict* file,
                                 SyncStatusCode,
                                 SyncFileStatus status) {
  file->Set("status", SyncFileStatusToString(status));
}

}  // namespace

//---------------------------------------------------------------------------
// SyncProcessRunner's.

// SyncProcessRunner implementation for LocalSync.
class LocalSyncRunner : public SyncProcessRunner,
                        public LocalFileSyncService::Observer {
 public:
  LocalSyncRunner(const std::string& name, SyncFileSystemService* sync_service)
      : SyncProcessRunner(name,
                          sync_service,
                          nullptr, /* timer_helper */
                          1 /* max_parallel_task */) {}

  LocalSyncRunner(const LocalSyncRunner&) = delete;
  LocalSyncRunner& operator=(const LocalSyncRunner&) = delete;

  void StartSync(SyncStatusCallback callback) override {
    GetSyncService()->local_service_->ProcessLocalChange(
        base::BindOnce(&LocalSyncRunner::DidProcessLocalChange,
                       factory_.GetWeakPtr(), std::move(callback)));
  }

  // LocalFileSyncService::Observer overrides.
  void OnLocalChangeAvailable(int64_t pending_changes) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    OnChangesUpdated(pending_changes);

    // Kick other sync runners just in case they're not running.
    GetSyncService()->RunForEachSyncRunners(&SyncProcessRunner::Schedule);
  }

 private:
  void DidProcessLocalChange(SyncStatusCallback callback,
                             SyncStatusCode status,
                             const FileSystemURL& url) {
    util::Log(logging::LOGGING_VERBOSE, FROM_HERE,
              "ProcessLocalChange finished with status=%d (%s) for url=%s",
              status, SyncStatusCodeToString(status),
              url.DebugString().c_str());
    std::move(callback).Run(status);
  }

  base::WeakPtrFactory<LocalSyncRunner> factory_{this};
};

// SyncProcessRunner implementation for RemoteSync.
class RemoteSyncRunner : public SyncProcessRunner,
                         public RemoteFileSyncService::Observer {
 public:
  RemoteSyncRunner(const std::string& name,
                   SyncFileSystemService* sync_service,
                   RemoteFileSyncService* remote_service)
      : SyncProcessRunner(name,
                          sync_service,
                          nullptr, /* timer_helper */
                          1 /* max_parallel_task */),
        remote_service_(remote_service),
        last_state_(REMOTE_SERVICE_OK) {}

  RemoteSyncRunner(const RemoteSyncRunner&) = delete;
  RemoteSyncRunner& operator=(const RemoteSyncRunner&) = delete;

  void StartSync(SyncStatusCallback callback) override {
    remote_service_->ProcessRemoteChange(
        base::BindOnce(&RemoteSyncRunner::DidProcessRemoteChange,
                       factory_.GetWeakPtr(), std::move(callback)));
  }

  SyncServiceState GetServiceState() override {
    return RemoteStateToSyncServiceState(last_state_);
  }

  // RemoteFileSyncService::Observer overrides.
  void OnRemoteChangeQueueUpdated(int64_t pending_changes) override {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);

    OnChangesUpdated(pending_changes);

    // Kick other sync runners just in case they're not running.
    GetSyncService()->RunForEachSyncRunners(&SyncProcessRunner::Schedule);
  }

  void OnRemoteServiceStateUpdated(RemoteServiceState state,
                                   const std::string& description) override {
    // Just forward to SyncFileSystemService.
    GetSyncService()->OnRemoteServiceStateUpdated(state, description);
    last_state_ = state;
  }

 private:
  void DidProcessRemoteChange(SyncStatusCallback callback,
                              SyncStatusCode status,
                              const FileSystemURL& url) {
    util::Log(logging::LOGGING_VERBOSE, FROM_HERE,
              "ProcessRemoteChange finished with status=%d (%s) for url=%s",
              status, SyncStatusCodeToString(status),
              url.DebugString().c_str());

    if (status == SYNC_STATUS_FILE_BUSY) {
      GetSyncService()->local_service_->RegisterURLForWaitingSync(
          url,
          base::BindOnce(&RemoteSyncRunner::Schedule, factory_.GetWeakPtr()));
    }
    std::move(callback).Run(status);
  }

  raw_ptr<RemoteFileSyncService> remote_service_;
  RemoteServiceState last_state_;
  base::WeakPtrFactory<RemoteSyncRunner> factory_{this};
};

//-----------------------------------------------------------------------------
// SyncFileSystemService

void SyncFileSystemService::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  local_sync_runners_.clear();
  remote_sync_runners_.clear();

  local_service_->Shutdown();
  local_service_.reset();

  remote_service_.reset();

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service)
    sync_service->RemoveObserver(this);

  ExtensionRegistry::Get(profile_)->RemoveObserver(this);

  profile_ = nullptr;
}

SyncFileSystemService::~SyncFileSystemService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!profile_);
}

void SyncFileSystemService::InitializeForApp(
    storage::FileSystemContext* file_system_context,
    const GURL& app_origin,
    SyncStatusCallback callback) {
  DCHECK(local_service_);
  DCHECK(remote_service_);
  DCHECK(app_origin == app_origin.DeprecatedGetOriginAsURL());

  util::Log(logging::LOGGING_VERBOSE, FROM_HERE, "Initializing for App: %s",
            app_origin.spec().c_str());

  local_service_->MaybeInitializeFileSystemContext(
      app_origin, file_system_context,
      base::BindOnce(&SyncFileSystemService::DidInitializeFileSystem,
                     weak_ptr_factory_.GetWeakPtr(), app_origin,
                     std::move(callback)));
}

void SyncFileSystemService::GetExtensionStatusMap(
    ExtensionStatusMapCallback callback) {
  remote_service_->GetOriginStatusMap(
      base::BindOnce(&SyncFileSystemService::DidGetExtensionStatusMap,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SyncFileSystemService::DumpFiles(
    content::StoragePartition* storage_partition,
    const GURL& origin,
    DumpFilesCallback callback) {
  DCHECK(!origin.is_empty());

  storage::FileSystemContext* file_system_context =
      storage_partition->GetFileSystemContext();
  local_service_->MaybeInitializeFileSystemContext(
      origin, file_system_context,
      base::BindOnce(&SyncFileSystemService::DidInitializeFileSystemForDump,
                     weak_ptr_factory_.GetWeakPtr(), origin,
                     std::move(callback)));
}

void SyncFileSystemService::DumpDatabase(DumpFilesCallback callback) {
  remote_service_->DumpDatabase(
      base::BindOnce(&SyncFileSystemService::DidDumpDatabase,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SyncFileSystemService::GetFileSyncStatus(const FileSystemURL& url,
                                              SyncFileStatusCallback callback) {
  DCHECK(local_service_);
  DCHECK(remote_service_);

  // It's possible to get an invalid FileEntry.
  if (!url.is_valid()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback), SYNC_FILE_ERROR_INVALID_URL,
                       SYNC_FILE_STATUS_UNKNOWN));
    return;
  }

  local_service_->HasPendingLocalChanges(
      url, base::BindOnce(&SyncFileSystemService::DidGetLocalChangeStatus,
                          weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SyncFileSystemService::AddSyncEventObserver(SyncEventObserver* observer) {
  observers_.AddObserver(observer);
}

void SyncFileSystemService::RemoveSyncEventObserver(
    SyncEventObserver* observer) {
  observers_.RemoveObserver(observer);
}

LocalChangeProcessor* SyncFileSystemService::GetLocalChangeProcessor(
    const GURL& origin) {
  return remote_service_->GetLocalChangeProcessor();
}

void SyncFileSystemService::OnSyncIdle() {
  if (promoting_demoted_changes_)
    return;
  promoting_demoted_changes_ = true;

  int* job_count = new int(1);
  base::RepeatingClosure promote_completion_callback = base::BindRepeating(
      &SyncFileSystemService::OnPromotionCompleted,
      weak_ptr_factory_.GetWeakPtr(), base::Owned(job_count));

  int64_t remote_changes = 0;
  for (size_t i = 0; i < remote_sync_runners_.size(); ++i)
    remote_changes += remote_sync_runners_[i]->pending_changes();
  if (remote_changes == 0) {
    ++*job_count;
    local_service_->PromoteDemotedChanges(promote_completion_callback);
  }

  int64_t local_changes = 0;
  for (size_t i = 0; i < local_sync_runners_.size(); ++i)
    local_changes += local_sync_runners_[i]->pending_changes();
  if (local_changes == 0) {
    ++*job_count;
    remote_service_->PromoteDemotedChanges(promote_completion_callback);
  }

  promote_completion_callback.Run();
}

void SyncFileSystemService::OnPromotionCompleted(int* count) {
  if (--*count != 0)
    return;
  promoting_demoted_changes_ = false;
  CheckIfIdle();
}

void SyncFileSystemService::CheckIfIdle() {
  if (promoting_demoted_changes_)
    return;

  for (size_t i = 0; i < remote_sync_runners_.size(); ++i) {
    SyncServiceState service_state = remote_sync_runners_[i]->GetServiceState();
    if (service_state != SYNC_SERVICE_RUNNING)
      continue;

    if (remote_sync_runners_[i]->pending_changes())
      return;
  }

  for (size_t i = 0; i < local_sync_runners_.size(); ++i) {
    SyncServiceState service_state = local_sync_runners_[i]->GetServiceState();
    if (service_state != SYNC_SERVICE_RUNNING)
      continue;

    if (local_sync_runners_[i]->pending_changes())
      return;
  }

  if (idle_callback_.is_null())
    return;

  std::move(idle_callback_).Run();
}

SyncServiceState SyncFileSystemService::GetSyncServiceState() {
  // For now we always query the state from the main RemoteFileSyncService.
  return RemoteStateToSyncServiceState(remote_service_->GetCurrentState());
}

SyncFileSystemService* SyncFileSystemService::GetSyncService() {
  return this;
}

void SyncFileSystemService::CallOnIdleForTesting(base::OnceClosure callback) {
  DCHECK(idle_callback_.is_null());
  idle_callback_ = std::move(callback);
  CheckIfIdle();
}

SyncFileSystemService::SyncFileSystemService(Profile* profile)
    : profile_(profile),
      sync_enabled_(true),
      promoting_demoted_changes_(false) {
}

void SyncFileSystemService::Initialize(
    std::unique_ptr<LocalFileSyncService> local_service,
    std::unique_ptr<RemoteFileSyncService> remote_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(local_service);
  DCHECK(remote_service);
  DCHECK(profile_);

  local_service_ = std::move(local_service);
  remote_service_ = std::move(remote_service);

  auto local_syncer = std::make_unique<LocalSyncRunner>(kLocalSyncName, this);
  auto remote_syncer = std::make_unique<RemoteSyncRunner>(
      kRemoteSyncName, this, remote_service_.get());

  local_service_->AddChangeObserver(local_syncer.get());
  local_service_->SetLocalChangeProcessorCallback(base::BindRepeating(
      &GetLocalChangeProcessorAdapter, weak_ptr_factory_.GetWeakPtr()));

  remote_service_->AddServiceObserver(remote_syncer.get());
  remote_service_->AddFileStatusObserver(this);
  remote_service_->SetRemoteChangeProcessor(local_service_.get());

  local_sync_runners_.push_back(std::move(local_syncer));
  remote_sync_runners_.push_back(std::move(remote_syncer));

  syncer::SyncService* sync_service =
      SyncServiceFactory::GetForProfile(profile_);
  if (sync_service) {
    UpdateSyncEnabledStatus(sync_service);
    sync_service->AddObserver(this);
  }

  ExtensionRegistry::Get(profile_)->AddObserver(this);
}

void SyncFileSystemService::DidInitializeFileSystem(const GURL& app_origin,
                                                    SyncStatusCallback callback,
                                                    SyncStatusCode status) {
  DVLOG(1) << "DidInitializeFileSystem: "
           << app_origin.spec() << " " << status;

  if (status != SYNC_STATUS_OK) {
    std::move(callback).Run(status);
    return;
  }

  // Local side of initialization for the app is done.
  // Continue on initializing the remote side.
  if (!remote_service_) {
    std::move(callback).Run(SYNC_STATUS_ABORT);
    return;
  }

  remote_service_->RegisterOrigin(
      app_origin, base::BindOnce(&SyncFileSystemService::DidRegisterOrigin,
                                 weak_ptr_factory_.GetWeakPtr(), app_origin,
                                 std::move(callback)));
}

void SyncFileSystemService::DidRegisterOrigin(const GURL& app_origin,
                                              SyncStatusCallback callback,
                                              SyncStatusCode status) {
  util::Log(logging::LOGGING_VERBOSE, FROM_HERE,
            "DidInitializeForApp (registered the origin): %s: %s",
            app_origin.spec().c_str(), SyncStatusCodeToString(status));

  if (!remote_service_) {
    std::move(callback).Run(SYNC_STATUS_ABORT);
    return;
  }

  if (status == SYNC_STATUS_FAILED) {
    // If we got generic error return the service status information.
    switch (remote_service_->GetCurrentState()) {
      case REMOTE_SERVICE_AUTHENTICATION_REQUIRED:
        std::move(callback).Run(SYNC_STATUS_AUTHENTICATION_FAILED);
        return;
      case REMOTE_SERVICE_TEMPORARY_UNAVAILABLE:
        std::move(callback).Run(SYNC_STATUS_SERVICE_TEMPORARILY_UNAVAILABLE);
        return;
      default:
        break;
    }
  }

  std::move(callback).Run(status);
}

void SyncFileSystemService::DidInitializeFileSystemForDump(
    const GURL& origin,
    DumpFilesCallback callback,
    SyncStatusCode status) {
  DCHECK(!origin.is_empty());

  if (status != SYNC_STATUS_OK || !remote_service_) {
    std::move(callback).Run(base::Value::List());
    return;
  }

  remote_service_->DumpFiles(
      origin, base::BindOnce(&SyncFileSystemService::DidDumpFiles,
                             weak_ptr_factory_.GetWeakPtr(), origin,
                             std::move(callback)));
}

void SyncFileSystemService::DidDumpFiles(const GURL& origin,
                                         DumpFilesCallback callback,
                                         base::Value::List dump_files) {
  if (dump_files.empty() || !local_service_ || !remote_service_) {
    std::move(callback).Run(base::Value::List());
    return;
  }

  // Place `dump_files` onto the heap to ensure pointer stability.
  auto files = std::make_unique<base::Value::List>(std::move(dump_files));
  auto* unowned_files = files.get();

  base::RepeatingClosure accumulate_callback = base::BarrierClosure(
      unowned_files->size(), base::BindOnce(
                                 [](DumpFilesCallback callback,
                                    std::unique_ptr<base::Value::List> files) {
                                   std::move(callback).Run(std::move(*files));
                                 },
                                 std::move(callback), std::move(files)));

  // After all metadata loaded, sync status can be added to each entry.
  for (base::Value& file : *unowned_files) {
    const auto& path = CHECK_DEREF(file.GetDict().FindString("path"));
    base::FilePath file_path = base::FilePath::FromUTF8Unsafe(path);
    FileSystemURL url = CreateSyncableFileSystemURL(origin, file_path);
    // `file` pointer is safe to pass here as its lifetime is bound to the
    // `callback` above.
    GetFileSyncStatus(
        url, base::BindOnce(&SetFileStatusFromSyncStatus, &file.GetDict())
                 .Then(accumulate_callback));
  }
}

void SyncFileSystemService::DidDumpDatabase(DumpFilesCallback callback,
                                            base::Value::List list) {
  std::move(callback).Run(std::move(list));
}

void SyncFileSystemService::DidGetExtensionStatusMap(
    ExtensionStatusMapCallback callback,
    std::unique_ptr<RemoteFileSyncService::OriginStatusMap> status_map) {
  if (!status_map)
    status_map = std::make_unique<RemoteFileSyncService::OriginStatusMap>();
  std::move(callback).Run(*status_map);
}

void SyncFileSystemService::SetSyncEnabledForTesting(bool enabled) {
  sync_enabled_ = enabled;
  remote_service_->SetSyncEnabled(sync_enabled_);
}

void SyncFileSystemService::DidGetLocalChangeStatus(
    SyncFileStatusCallback callback,
    SyncStatusCode status,
    bool has_pending_local_changes) {
  std::move(callback).Run(status, has_pending_local_changes
                                      ? SYNC_FILE_STATUS_HAS_PENDING_CHANGES
                                      : SYNC_FILE_STATUS_SYNCED);
}

void SyncFileSystemService::OnRemoteServiceStateUpdated(
    RemoteServiceState state,
    const std::string& description) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  util::Log(logging::LOGGING_VERBOSE, FROM_HERE,
            "OnRemoteServiceStateChanged: %d %s", state, description.c_str());

  for (auto& observer : observers_) {
    observer.OnSyncStateUpdated(GURL(), RemoteStateToSyncServiceState(state),
                                description);
  }

  RunForEachSyncRunners(&SyncProcessRunner::Schedule);
}

void SyncFileSystemService::OnExtensionInstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    bool is_update) {
  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for INSTALLED: " << app_origin;
  // NOTE: When an app is uninstalled and re-installed in a sequence,
  // |local_service_| may still keeps |app_origin| as disabled origin.
  local_service_->SetOriginEnabled(app_origin, true);
}

void SyncFileSystemService::OnExtensionUnloaded(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UnloadedExtensionReason reason) {
  if (reason != extensions::UnloadedExtensionReason::DISABLE)
    return;

  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  int disable_reasons =
      ExtensionPrefs::Get(profile_)->GetDisableReasons(extension->id());
  if (disable_reasons & extensions::disable_reason::DISABLE_RELOAD) {
    // Bypass disabling the origin since the app will be re-enabled soon.
    // NOTE: If re-enabling the app fails, the app is disabled while it is
    // handled as enabled origin in the SyncFS. This should be safe and will be
    // recovered when the user re-enables the app manually or the sync service
    // restarts.
    DVLOG(1) << "Handle extension notification for UNLOAD(DISABLE_RELOAD): "
             << app_origin;
    return;
  }

  DVLOG(1) << "Handle extension notification for UNLOAD(DISABLE): "
           << app_origin;
  remote_service_->DisableOrigin(
      app_origin, base::BindOnce(&DidHandleUnloadedEvent, app_origin));
  local_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::OnExtensionUninstalled(
    content::BrowserContext* browser_context,
    const Extension* extension,
    extensions::UninstallReason reason) {
  RemoteFileSyncService::UninstallFlag flag =
      RemoteFileSyncService::UNINSTALL_AND_PURGE_REMOTE;
  // If it's loaded from an unpacked package and with key: field,
  // the uninstall will not be sync'ed and the user might be using the
  // same app key in other installs, so avoid purging the remote folder.
  if (extensions::Manifest::IsUnpackedLocation(extension->location()) &&
      extension->manifest()->FindKey(extensions::manifest_keys::kKey)) {
    flag = RemoteFileSyncService::UNINSTALL_AND_KEEP_REMOTE;
  }

  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for UNINSTALLED: "
           << app_origin;
  remote_service_->UninstallOrigin(
      app_origin, flag, base::BindOnce(&DidHandleUninstalledEvent, app_origin));
  local_service_->SetOriginEnabled(app_origin, false);
}

void SyncFileSystemService::OnExtensionLoaded(
    content::BrowserContext* browser_context,
    const Extension* extension) {
  GURL app_origin = Extension::GetBaseURLFromExtensionId(extension->id());
  DVLOG(1) << "Handle extension notification for LOADED: " << app_origin;
  remote_service_->EnableOrigin(
      app_origin, base::BindOnce(&DidHandleLoadEvent, app_origin));
  local_service_->SetOriginEnabled(app_origin, true);
}

void SyncFileSystemService::OnStateChanged(syncer::SyncService* sync) {
  UpdateSyncEnabledStatus(sync);
}

void SyncFileSystemService::OnFileStatusChanged(
    const FileSystemURL& url,
    SyncFileType file_type,
    SyncFileStatus sync_status,
    SyncAction action_taken,
    SyncDirection direction) {
  for (auto& observer : observers_)
    observer.OnFileSynced(url, file_type, sync_status, action_taken, direction);
}

void SyncFileSystemService::UpdateSyncEnabledStatus(
    syncer::SyncService* sync_service) {
  if (!sync_service->GetUserSettings()->IsInitialSyncFeatureSetupComplete()) {
    return;
  }
  bool old_sync_enabled = sync_enabled_;
  sync_enabled_ = sync_service->GetActiveDataTypes().Has(syncer::APPS);
  remote_service_->SetSyncEnabled(sync_enabled_);
  if (!old_sync_enabled && sync_enabled_)
    RunForEachSyncRunners(&SyncProcessRunner::Schedule);
}

void SyncFileSystemService::RunForEachSyncRunners(
    void(SyncProcessRunner::*method)()) {
  for (auto iter = local_sync_runners_.begin();
       iter != local_sync_runners_.end(); ++iter)
    (iter->get()->*method)();
  for (auto iter = remote_sync_runners_.begin();
       iter != remote_sync_runners_.end(); ++iter)
    (iter->get()->*method)();
}

}  // namespace sync_file_system
