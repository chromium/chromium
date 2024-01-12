// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/local_file_sync_service.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/local_change_processor.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "extensions/common/extension_set.h"
#include "storage/browser/blob/scoped_file.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "url/gurl.h"

using content::BrowserThread;
using storage::FileSystemURL;

namespace sync_file_system {

namespace {

void PrepareForProcessRemoteChangeCallbackAdapter(
    RemoteChangeProcessor::PrepareChangeCallback callback,
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info,
    storage::ScopedFile snapshot) {
  std::move(callback).Run(status, sync_file_info.metadata,
                          sync_file_info.changes);
}

void InvokeCallbackOnNthInvocation(int* count,
                                   base::RepeatingClosure callback) {
  --*count;
  if (*count <= 0)
    callback.Run();
}

}  // namespace

LocalFileSyncService::OriginChangeMap::OriginChangeMap()
    : next_(change_count_map_.end()) {}
LocalFileSyncService::OriginChangeMap::~OriginChangeMap() {}

bool LocalFileSyncService::OriginChangeMap::NextOriginToProcess(GURL* origin) {
  DCHECK(origin);
  if (change_count_map_.empty())
    return false;
  auto begin = next_;
  do {
    if (next_ == change_count_map_.end())
      next_ = change_count_map_.begin();
    DCHECK_NE(0, next_->second);
    *origin = next_++->first;
    if (!base::Contains(disabled_origins_, *origin))
      return true;
  } while (next_ != begin);
  return false;
}

int64_t LocalFileSyncService::OriginChangeMap::GetTotalChangeCount() const {
  int64_t num_changes = 0;
  for (auto iter = change_count_map_.begin(); iter != change_count_map_.end();
       ++iter) {
    if (base::Contains(disabled_origins_, iter->first))
      continue;
    num_changes += iter->second;
  }
  return num_changes;
}

void LocalFileSyncService::OriginChangeMap::SetOriginChangeCount(
    const GURL& origin,
    int64_t changes) {
  if (changes != 0) {
    change_count_map_[origin] = changes;
    return;
  }
  auto found = change_count_map_.find(origin);
  if (found != change_count_map_.end()) {
    if (next_ == found)
      ++next_;
    change_count_map_.erase(found);
  }
}

void LocalFileSyncService::OriginChangeMap::SetOriginEnabled(
    const GURL& origin, bool enabled) {
  if (enabled)
    disabled_origins_.erase(origin);
  else
    disabled_origins_.insert(origin);
}

// LocalFileSyncService -------------------------------------------------------

std::unique_ptr<LocalFileSyncService> LocalFileSyncService::Create(
    Profile* profile) {
  return base::WrapUnique(new LocalFileSyncService(profile, nullptr));
}

std::unique_ptr<LocalFileSyncService> LocalFileSyncService::CreateForTesting(
    Profile* profile,
    leveldb::Env* env) {
  auto sync_service = base::WrapUnique(new LocalFileSyncService(profile, env));
  sync_service->sync_context_->set_mock_notify_changes_duration_in_sec(0);
  return sync_service;
}

LocalFileSyncService::~LocalFileSyncService() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void LocalFileSyncService::Shutdown() {
  sync_context_->RemoveOriginChangeObserver(this);
  sync_context_->ShutdownOnUIThread();
  profile_ = nullptr;
}

void LocalFileSyncService::MaybeInitializeFileSystemContext(
    const GURL& app_origin,
    storage::FileSystemContext* file_system_context,
    SyncStatusCallback callback) {
  sync_context_->MaybeInitializeFileSystemContext(
      app_origin, file_system_context,
      base::BindOnce(&LocalFileSyncService::DidInitializeFileSystemContext,
                     weak_ptr_factory_.GetWeakPtr(), app_origin,
                     base::RetainedRef(file_system_context),
                     std::move(callback)));
}

void LocalFileSyncService::AddChangeObserver(Observer* observer) {
  change_observers_.AddObserver(observer);
}

void LocalFileSyncService::RegisterURLForWaitingSync(
    const FileSystemURL& url,
    base::OnceClosure on_syncable_callback) {
  sync_context_->RegisterURLForWaitingSync(url,
                                           std::move(on_syncable_callback));
}

void LocalFileSyncService::ProcessLocalChange(SyncFileCallback callback) {
  // Pick an origin to process next.
  GURL origin;
  if (!origin_change_map_.NextOriginToProcess(&origin)) {
    std::move(callback).Run(SYNC_STATUS_NO_CHANGE_TO_SYNC, FileSystemURL());
    return;
  }
  DCHECK(!origin.is_empty());
  DCHECK(base::Contains(origin_to_contexts_, origin));

  DVLOG(1) << "Starting ProcessLocalChange";

  sync_context_->GetFileForLocalSync(
      origin_to_contexts_[origin],
      base::BindOnce(&LocalFileSyncService::DidGetFileForLocalSync,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileSyncService::SetLocalChangeProcessor(
    LocalChangeProcessor* local_change_processor) {
  local_change_processor_ = local_change_processor;
}

void LocalFileSyncService::SetLocalChangeProcessorCallback(
    GetLocalChangeProcessorCallback get_local_change_processor) {
  get_local_change_processor_ = std::move(get_local_change_processor);
}

void LocalFileSyncService::HasPendingLocalChanges(
    const FileSystemURL& url,
    HasPendingLocalChangeCallback callback) {
  if (!base::Contains(origin_to_contexts_, url.origin().GetURL())) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  SYNC_FILE_ERROR_INVALID_URL, false));
    return;
  }
  sync_context_->HasPendingLocalChanges(
      origin_to_contexts_[url.origin().GetURL()], url, std::move(callback));
}

void LocalFileSyncService::PromoteDemotedChanges(
    base::RepeatingClosure callback) {
  if (origin_to_contexts_.empty()) {
    callback.Run();
    return;
  }

  base::RepeatingClosure completion_callback =
      base::BindRepeating(&InvokeCallbackOnNthInvocation,
                          base::Owned(new int(origin_to_contexts_.size() + 1)),
                          std::move(callback));
  for (auto iter = origin_to_contexts_.begin();
       iter != origin_to_contexts_.end(); ++iter)
    sync_context_->PromoteDemotedChanges(iter->first, iter->second,
                                         completion_callback);
  completion_callback.Run();
}

void LocalFileSyncService::GetLocalFileMetadata(
    const FileSystemURL& url,
    SyncFileMetadataCallback callback) {
  DCHECK(base::Contains(origin_to_contexts_, url.origin().GetURL()));
  sync_context_->GetFileMetadata(origin_to_contexts_[url.origin().GetURL()],
                                 url, std::move(callback));
}

void LocalFileSyncService::PrepareForProcessRemoteChange(
    const FileSystemURL& url,
    PrepareChangeCallback callback) {
  DVLOG(1) << "PrepareForProcessRemoteChange: " << url.DebugString();

  if (!base::Contains(origin_to_contexts_, url.origin().GetURL())) {
    // This could happen if a remote sync is triggered for the app that hasn't
    // been initialized in this service.
    DCHECK(profile_);
    // The given url.origin() must be for valid installed app.
    const extensions::Extension* extension =
        extensions::ExtensionRegistry::Get(profile_)
            ->enabled_extensions()
            .GetAppByURL(url.origin().GetURL());
    if (!extension) {
      util::Log(
          logging::LOGGING_WARNING, FROM_HERE,
          "PrepareForProcessRemoteChange called for non-existing origin: %s",
          url.origin().GetURL().spec().c_str());

      // The extension has been uninstalled and this method is called
      // before the remote changes for the origin are removed.
      std::move(callback).Run(SYNC_STATUS_NO_CHANGE_TO_SYNC, SyncFileMetadata(),
                              FileChangeList());
      return;
    }
    scoped_refptr<storage::FileSystemContext> file_system_context =
        extensions::util::GetStoragePartitionForExtensionId(extension->id(),
                                                            profile_)
            ->GetFileSystemContext();
    MaybeInitializeFileSystemContext(
        url.origin().GetURL(), file_system_context.get(),
        base::BindOnce(&LocalFileSyncService::DidInitializeForRemoteSync,
                       weak_ptr_factory_.GetWeakPtr(), url,
                       base::RetainedRef(file_system_context),
                       std::move(callback)));
    return;
  }

  DCHECK(base::Contains(origin_to_contexts_, url.origin().GetURL()));
  sync_context_->PrepareForSync(
      origin_to_contexts_[url.origin().GetURL()], url,
      LocalFileSyncContext::SYNC_EXCLUSIVE,
      base::BindOnce(&PrepareForProcessRemoteChangeCallbackAdapter,
                     std::move(callback)));
}

void LocalFileSyncService::ApplyRemoteChange(const FileChange& change,
                                             const base::FilePath& local_path,
                                             const FileSystemURL& url,
                                             SyncStatusCallback callback) {
  DCHECK(base::Contains(origin_to_contexts_, url.origin().GetURL()));
  util::Log(logging::LOGGING_VERBOSE, FROM_HERE,
            "[Remote -> Local] ApplyRemoteChange: %s on %s",
            change.DebugString().c_str(), url.DebugString().c_str());

  sync_context_->ApplyRemoteChange(
      origin_to_contexts_[url.origin().GetURL()], change, local_path, url,
      base::BindOnce(&LocalFileSyncService::DidApplyRemoteChange,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void LocalFileSyncService::FinalizeRemoteSync(
    const FileSystemURL& url,
    bool clear_local_changes,
    base::OnceClosure completion_callback) {
  DCHECK(base::Contains(origin_to_contexts_, url.origin().GetURL()));
  sync_context_->FinalizeExclusiveSync(
      origin_to_contexts_[url.origin().GetURL()], url, clear_local_changes,
      std::move(completion_callback));
}

void LocalFileSyncService::RecordFakeLocalChange(const FileSystemURL& url,
                                                 const FileChange& change,
                                                 SyncStatusCallback callback) {
  DCHECK(base::Contains(origin_to_contexts_, url.origin().GetURL()));
  sync_context_->RecordFakeLocalChange(
      origin_to_contexts_[url.origin().GetURL()], url, change,
      std::move(callback));
}

void LocalFileSyncService::OnChangesAvailableInOrigins(
    const std::set<GURL>& origins) {
  bool need_notification = false;
  for (auto iter = origins.begin(); iter != origins.end(); ++iter) {
    const GURL& origin = *iter;
    if (!base::Contains(origin_to_contexts_, origin)) {
      // This could happen if this is called for apps/origins that haven't
      // been initialized yet, or for apps/origins that are disabled.
      // (Local change tracker could call this for uninitialized origins
      // while it's reading dirty files from the database in the
      // initialization phase.)
      pending_origins_with_changes_.insert(origin);
      continue;
    }
    need_notification = true;
    SyncFileSystemBackend* backend =
        SyncFileSystemBackend::GetBackend(origin_to_contexts_[origin]);
    DCHECK(backend);
    DCHECK(backend->change_tracker());
    origin_change_map_.SetOriginChangeCount(
        origin, backend->change_tracker()->num_changes());
  }
  if (!need_notification)
    return;
  int64_t num_changes = origin_change_map_.GetTotalChangeCount();
  for (auto& observer : change_observers_)
    observer.OnLocalChangeAvailable(num_changes);
}

void LocalFileSyncService::SetOriginEnabled(const GURL& origin, bool enabled) {
  if (!base::Contains(origin_to_contexts_, origin))
    return;
  origin_change_map_.SetOriginEnabled(origin, enabled);
}

LocalFileSyncService::LocalFileSyncService(Profile* profile,
                                           leveldb::Env* env_override)
    : profile_(profile),
      sync_context_(
          new LocalFileSyncContext(profile_->GetPath(),
                                   env_override,
                                   content::GetUIThreadTaskRunner({}).get(),
                                   content::GetIOThreadTaskRunner({}).get())),
      local_change_processor_(nullptr) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  sync_context_->AddOriginChangeObserver(this);
}

void LocalFileSyncService::DidInitializeFileSystemContext(
    const GURL& app_origin,
    storage::FileSystemContext* file_system_context,
    SyncStatusCallback callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    std::move(callback).Run(status);
    return;
  }
  DCHECK(file_system_context);
  origin_to_contexts_[app_origin] = file_system_context;

  if (pending_origins_with_changes_.find(app_origin) !=
      pending_origins_with_changes_.end()) {
    // We have remaining changes for the origin.
    pending_origins_with_changes_.erase(app_origin);
    SyncFileSystemBackend* backend =
        SyncFileSystemBackend::GetBackend(file_system_context);
    DCHECK(backend);
    DCHECK(backend->change_tracker());
    origin_change_map_.SetOriginChangeCount(
        app_origin, backend->change_tracker()->num_changes());
    int64_t num_changes = origin_change_map_.GetTotalChangeCount();
    for (auto& observer : change_observers_)
      observer.OnLocalChangeAvailable(num_changes);
  }
  std::move(callback).Run(status);
}

void LocalFileSyncService::DidInitializeForRemoteSync(
    const FileSystemURL& url,
    storage::FileSystemContext* file_system_context,
    PrepareChangeCallback callback,
    SyncStatusCode status) {
  if (status != SYNC_STATUS_OK) {
    DVLOG(1) << "FileSystemContext initialization failed for remote sync:"
             << url.DebugString() << " status=" << status
             << " (" << SyncStatusCodeToString(status) << ")";
    std::move(callback).Run(status, SyncFileMetadata(), FileChangeList());
    return;
  }
  origin_to_contexts_[url.origin().GetURL()] = file_system_context;
  PrepareForProcessRemoteChange(url, std::move(callback));
}

void LocalFileSyncService::DidApplyRemoteChange(SyncStatusCallback callback,
                                                SyncStatusCode status) {
  util::Log(logging::LOGGING_VERBOSE, FROM_HERE,
            "[Remote -> Local] ApplyRemoteChange finished --> %s",
            SyncStatusCodeToString(status));
  std::move(callback).Run(status);
}

void LocalFileSyncService::DidGetFileForLocalSync(
    SyncFileCallback callback,
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info,
    storage::ScopedFile snapshot) {
  if (status != SYNC_STATUS_OK) {
    std::move(callback).Run(status, sync_file_info.url);
    return;
  }
  if (sync_file_info.changes.empty()) {
    // There's a slight chance this could happen.
    ProcessLocalChange(std::move(callback));
    return;
  }

  FileChange next_change = sync_file_info.changes.front();
  DVLOG(1) << "ProcessLocalChange: " << sync_file_info.url.DebugString()
           << " change:" << next_change.DebugString();

  GetLocalChangeProcessor(sync_file_info.url)
      ->ApplyLocalChange(
          next_change, sync_file_info.local_file_path, sync_file_info.metadata,
          sync_file_info.url,
          base::BindOnce(&LocalFileSyncService::ProcessNextChangeForURL,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                         std::move(snapshot), sync_file_info, next_change,
                         sync_file_info.changes.PopAndGetNewList()));
}

void LocalFileSyncService::ProcessNextChangeForURL(
    SyncFileCallback callback,
    storage::ScopedFile snapshot,
    const LocalFileSyncInfo& sync_file_info,
    const FileChange& processed_change,
    const FileChangeList& changes,
    SyncStatusCode status) {
  DVLOG(1) << "Processed one local change: "
           << sync_file_info.url.DebugString()
           << " change:" << processed_change.DebugString()
           << " status:" << status;

  if (status == SYNC_STATUS_RETRY) {
    GetLocalChangeProcessor(sync_file_info.url)
        ->ApplyLocalChange(
            processed_change, sync_file_info.local_file_path,
            sync_file_info.metadata, sync_file_info.url,
            base::BindOnce(&LocalFileSyncService::ProcessNextChangeForURL,
                           weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                           std::move(snapshot), sync_file_info,
                           processed_change, changes));
    return;
  }

  if (status == SYNC_FILE_ERROR_NOT_FOUND &&
      processed_change.change() == FileChange::FILE_CHANGE_DELETE) {
    // This must be ok (and could happen).
    status = SYNC_STATUS_OK;
  }

  const FileSystemURL& url = sync_file_info.url;
  if (status != SYNC_STATUS_OK || changes.empty()) {
    DCHECK(base::Contains(origin_to_contexts_, url.origin().GetURL()));
    sync_context_->FinalizeSnapshotSync(
        origin_to_contexts_[url.origin().GetURL()], url, status,
        base::BindOnce(std::move(callback), status, url));
    return;
  }

  FileChange next_change = changes.front();
  GetLocalChangeProcessor(url)->ApplyLocalChange(
      changes.front(), sync_file_info.local_file_path, sync_file_info.metadata,
      url,
      base::BindOnce(&LocalFileSyncService::ProcessNextChangeForURL,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     std::move(snapshot), sync_file_info, next_change,
                     changes.PopAndGetNewList()));
}

LocalChangeProcessor* LocalFileSyncService::GetLocalChangeProcessor(
    const FileSystemURL& url) {
  if (!get_local_change_processor_.is_null())
    return get_local_change_processor_.Run(url.origin().GetURL());
  DCHECK(local_change_processor_);
  return local_change_processor_;
}

}  // namespace sync_file_system
