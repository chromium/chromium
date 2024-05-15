// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"

#include <memory>
#include <utility>

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_origin_change_observer.h"
#include "chrome/browser/sync_file_system/local/root_delete_helper.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/local/syncable_file_operation_runner.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "storage/browser/blob/scoped_file.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/origin.h"

using storage::FileSystemContext;
using storage::FileSystemFileUtil;
using storage::FileSystemOperation;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;

namespace sync_file_system {

namespace {

const int kMaxConcurrentSyncableOperation = 3;
const int kNotifyChangesDurationInSec = 1;
const int kMaxURLsToFetchForLocalSync = 5;

const base::FilePath::CharType kSnapshotDir[] = FILE_PATH_LITERAL("snapshots");

}  // namespace

LocalFileSyncContext::LocalFileSyncContext(
    const base::FilePath& base_path,
    leveldb::Env* env_override,
    base::SingleThreadTaskRunner* ui_task_runner,
    base::SingleThreadTaskRunner* io_task_runner)
    : local_base_path_(base_path.Append(FILE_PATH_LITERAL("local"))),
      env_override_(env_override),
      ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      shutdown_on_ui_(false),
      shutdown_on_io_(false),
      mock_notify_changes_duration_in_sec_(-1) {
  DCHECK(base_path.IsAbsolute());
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
}

void LocalFileSyncContext::MaybeInitializeFileSystemContext(
    const GURL& source_url,
    FileSystemContext* file_system_context,
    SyncStatusCallback callback) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  if (base::Contains(file_system_contexts_, file_system_context)) {
    // The context has been already initialized. Just dispatch the callback
    // with SYNC_STATUS_OK.
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), SYNC_STATUS_OK));
    return;
  }

  StatusCallbackQueue& callback_queue =
      pending_initialize_callbacks_[file_system_context];
  callback_queue.push_back(std::move(callback));
  if (callback_queue.size() > 1)
    return;

  // The sync service always expects the origin (app) is initialized
  // for writable way (even when MaybeInitializeFileSystemContext is called
  // from read-only OpenFileSystem), so open the filesystem with
  // CREATE_IF_NONEXISTENT here.
  storage::FileSystemBackend::ResolveURLCallback open_filesystem_callback =
      base::BindOnce(
          &LocalFileSyncContext::InitializeFileSystemContextOnIOThread, this,
          source_url, base::RetainedRef(file_system_context));
  const blink::StorageKey storage_key =
      blink::StorageKey::CreateFirstParty(url::Origin::Create(source_url));
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&storage::SandboxFileSystemBackendDelegate::OpenFileSystem,
                     base::Unretained(file_system_context->sandbox_delegate()),
                     storage::BucketLocator::ForDefaultBucket(storage_key),
                     storage::kFileSystemTypeSyncable,
                     storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
                     std::move(open_filesystem_callback), GURL()));
}

void LocalFileSyncContext::ShutdownOnUIThread() {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  shutdown_on_ui_ = true;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalFileSyncContext::ShutdownOnIOThread, this));
}

void LocalFileSyncContext::GetFileForLocalSync(
    FileSystemContext* file_system_context,
    LocalFileSyncInfoCallback callback) {
  DCHECK(file_system_context);
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());

  file_system_context->default_file_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&LocalFileSyncContext::GetNextURLsForSyncOnFileThread,
                     this, base::RetainedRef(file_system_context)),
      base::BindOnce(&LocalFileSyncContext::TryPrepareForLocalSync, this,
                     base::RetainedRef(file_system_context),
                     std::move(callback)));
}

void LocalFileSyncContext::ClearChangesForURL(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    base::OnceClosure done_callback) {
  // This is initially called on UI thread and to be relayed to FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&LocalFileSyncContext::ClearChangesForURL,
                                  this, base::RetainedRef(file_system_context),
                                  url, std::move(done_callback)));
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  backend->change_tracker()->ClearChangesForURL(url);

  // Call the completion callback on UI thread.
  ui_task_runner_->PostTask(FROM_HERE, std::move(done_callback));
}

void LocalFileSyncContext::FinalizeSnapshotSync(
    storage::FileSystemContext* file_system_context,
    const storage::FileSystemURL& url,
    SyncStatusCode sync_finish_status,
    base::OnceClosure done_callback) {
  DCHECK(file_system_context);
  DCHECK(url.is_valid());
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::FinalizeSnapshotSync, this,
                       base::RetainedRef(file_system_context), url,
                       sync_finish_status, std::move(done_callback)));
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());

  if (sync_finish_status == SYNC_STATUS_OK ||
      sync_finish_status == SYNC_STATUS_HAS_CONFLICT) {
    // Commit the in-memory mirror change.
    backend->change_tracker()->ResetToMirrorAndCommitChangesForURL(url);
  } else {
    // Abort in-memory mirror change.
    backend->change_tracker()->RemoveMirrorAndCommitChangesForURL(url);
  }

  // We've been keeping it in writing mode, so clear the writing counter
  // to unblock sync activities.
  io_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalFileSyncContext::FinalizeSnapshotSyncOnIOThread,
                     this, url));

  // Call the completion callback on UI thread.
  ui_task_runner_->PostTask(FROM_HERE, std::move(done_callback));
}

void LocalFileSyncContext::FinalizeExclusiveSync(
    storage::FileSystemContext* file_system_context,
    const storage::FileSystemURL& url,
    bool clear_local_changes,
    base::OnceClosure done_callback) {
  DCHECK(file_system_context);
  if (!url.is_valid()) {
    std::move(done_callback).Run();
    return;
  }

  if (clear_local_changes) {
    ClearChangesForURL(
        file_system_context, url,
        base::BindOnce(&LocalFileSyncContext::FinalizeExclusiveSync, this,
                       base::RetainedRef(file_system_context), url, false,
                       std::move(done_callback)));
    return;
  }

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalFileSyncContext::ClearSyncFlagOnIOThread,
                                this, url, false /* for_snapshot_sync */));

  std::move(done_callback).Run();
}

void LocalFileSyncContext::PrepareForSync(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    SyncMode sync_mode,
    LocalFileSyncInfoCallback callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalFileSyncContext::PrepareForSync, this,
                                  base::RetainedRef(file_system_context), url,
                                  sync_mode, std::move(callback)));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  const bool syncable = sync_status()->IsSyncable(url);
  // Disable writing if it's ready to be synced.
  if (syncable)
    sync_status()->StartSyncing(url);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&LocalFileSyncContext::DidGetWritingStatusForSync, this,
                     base::RetainedRef(file_system_context),
                     syncable ? SYNC_STATUS_OK : SYNC_STATUS_FILE_BUSY, url,
                     sync_mode, std::move(callback)));
}

void LocalFileSyncContext::RegisterURLForWaitingSync(
    const FileSystemURL& url,
    base::OnceClosure on_syncable_callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::RegisterURLForWaitingSync, this,
                       url, std::move(on_syncable_callback)));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    return;
  if (sync_status()->IsSyncable(url)) {
    // No need to register; fire the callback now.
    ui_task_runner_->PostTask(FROM_HERE, std::move(on_syncable_callback));
    return;
  }
  url_waiting_sync_on_io_ = url;
  url_syncable_callback_ = std::move(on_syncable_callback);
}

void LocalFileSyncContext::ApplyRemoteChange(
    FileSystemContext* file_system_context,
    const FileChange& change,
    const base::FilePath& local_path,
    const FileSystemURL& url,
    SyncStatusCallback callback) {
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::ApplyRemoteChange, this,
                       base::RetainedRef(file_system_context), change,
                       local_path, url, std::move(callback)));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!sync_status()->IsWritable(url));
  DCHECK(!sync_status()->IsWriting(url));

  FileSystemOperation::StatusCallback operation_callback;
  switch (change.change()) {
    case FileChange::FILE_CHANGE_DELETE:
      HandleRemoteDelete(file_system_context, url, std::move(callback));
      return;
    case FileChange::FILE_CHANGE_ADD_OR_UPDATE:
      HandleRemoteAddOrUpdate(file_system_context, change, local_path, url,
                              std::move(callback));
      return;
  }
  NOTREACHED_IN_MIGRATION();
  std::move(callback).Run(SYNC_STATUS_FAILED);
}

void LocalFileSyncContext::HandleRemoteDelete(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    SyncStatusCallback callback) {
  FileSystemURL url_for_sync = CreateSyncableFileSystemURLForSync(
      file_system_context, url);

  // Handle root directory case differently.
  if (storage::VirtualPath::IsRootPath(url.path())) {
    DCHECK(!root_delete_helper_);
    root_delete_helper_ = std::make_unique<RootDeleteHelper>(
        file_system_context, sync_status(), url,
        base::BindOnce(&LocalFileSyncContext::DidApplyRemoteChange, this, url,
                       std::move(callback)));
    root_delete_helper_->Run();
    return;
  }

  file_system_context->operation_runner()->Remove(
      url_for_sync, true /* recursive */,
      base::BindOnce(&LocalFileSyncContext::DidApplyRemoteChange, this, url,
                     std::move(callback)));
}

void LocalFileSyncContext::HandleRemoteAddOrUpdate(
    FileSystemContext* file_system_context,
    const FileChange& change,
    const base::FilePath& local_path,
    const FileSystemURL& url,
    SyncStatusCallback callback) {
  FileSystemURL url_for_sync = CreateSyncableFileSystemURLForSync(
      file_system_context, url);

  if (storage::VirtualPath::IsRootPath(url.path())) {
    DidApplyRemoteChange(url, std::move(callback), base::File::FILE_OK);
    return;
  }

  file_system_context->operation_runner()->Remove(
      url_for_sync, true /* recursive */,
      base::BindOnce(
          &LocalFileSyncContext::DidRemoveExistingEntryForRemoteAddOrUpdate,
          this, base::RetainedRef(file_system_context), change, local_path, url,
          std::move(callback)));
}

void LocalFileSyncContext::DidRemoveExistingEntryForRemoteAddOrUpdate(
    FileSystemContext* file_system_context,
    const FileChange& change,
    const base::FilePath& local_path,
    const FileSystemURL& url,
    SyncStatusCallback callback,
    base::File::Error error) {
  // Remove() may fail if the target entry does not exist (which is ok),
  // so we ignore |error| here.

  if (shutdown_on_io_) {
    std::move(callback).Run(SYNC_FILE_ERROR_ABORT);
    return;
  }

  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!sync_status()->IsWritable(url));
  DCHECK(!sync_status()->IsWriting(url));

  FileSystemURL url_for_sync = CreateSyncableFileSystemURLForSync(
      file_system_context, url);
  FileSystemOperation::StatusCallback operation_callback =
      base::BindOnce(&LocalFileSyncContext::DidApplyRemoteChange, this, url,
                     std::move(callback));

  DCHECK_EQ(FileChange::FILE_CHANGE_ADD_OR_UPDATE, change.change());
  switch (change.file_type()) {
    case SYNC_FILE_TYPE_FILE: {
      DCHECK(!local_path.empty());
      base::FilePath dir_path = storage::VirtualPath::DirName(url.path());
      if (dir_path.empty() ||
          storage::VirtualPath::DirName(dir_path) == dir_path) {
        // Copying into the root directory.
        file_system_context->operation_runner()->CopyInForeignFile(
            local_path, url_for_sync, std::move(operation_callback));
      } else {
        FileSystemURL dir_url = file_system_context->CreateCrackedFileSystemURL(
            url_for_sync.storage_key(), url_for_sync.mount_type(),
            storage::VirtualPath::DirName(url_for_sync.virtual_path()));
        file_system_context->operation_runner()->CreateDirectory(
            dir_url, false /* exclusive */, true /* recursive */,
            base::BindOnce(&LocalFileSyncContext::DidCreateDirectoryForCopyIn,
                           this, base::RetainedRef(file_system_context),
                           local_path, url, std::move(operation_callback)));
      }
      break;
    }
    case SYNC_FILE_TYPE_DIRECTORY:
      file_system_context->operation_runner()->CreateDirectory(
          url_for_sync, false /* exclusive */, true /* recursive */,
          std::move(operation_callback));
      break;
    case SYNC_FILE_TYPE_UNKNOWN:
      NOTREACHED_IN_MIGRATION() << "File type unknown for ADD_OR_UPDATE change";
  }
}

void LocalFileSyncContext::RecordFakeLocalChange(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    const FileChange& change,
    SyncStatusCallback callback) {
  // This is called on UI thread and to be relayed to FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&LocalFileSyncContext::RecordFakeLocalChange,
                                  this, base::RetainedRef(file_system_context),
                                  url, change, std::move(callback)));
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  backend->change_tracker()->MarkDirtyOnDatabase(url);
  backend->change_tracker()->RecordChange(url, change);

  // Fire the callback on UI thread.
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), SYNC_STATUS_OK));
}

void LocalFileSyncContext::GetFileMetadata(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    SyncFileMetadataCallback callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    io_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&LocalFileSyncContext::GetFileMetadata, this,
                                  base::RetainedRef(file_system_context), url,
                                  std::move(callback)));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());

  FileSystemURL url_for_sync = CreateSyncableFileSystemURLForSync(
      file_system_context, url);
  file_system_context->operation_runner()->GetMetadata(
      url_for_sync,
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kLastModified},
      base::BindOnce(&LocalFileSyncContext::DidGetFileMetadata, this,
                     std::move(callback)));
}

void LocalFileSyncContext::HasPendingLocalChanges(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    HasPendingLocalChangeCallback callback) {
  // This gets called on UI thread and relays the task on FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&LocalFileSyncContext::HasPendingLocalChanges,
                                  this, base::RetainedRef(file_system_context),
                                  url, std::move(callback)));
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  FileChangeList changes;
  backend->change_tracker()->GetChangesForURL(url, &changes);

  // Fire the callback on UI thread.
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), SYNC_STATUS_OK, !changes.empty()));
}

void LocalFileSyncContext::PromoteDemotedChanges(
    const GURL& origin,
    storage::FileSystemContext* file_system_context,
    base::OnceClosure callback) {
  // This is initially called on UI thread and to be relayed to FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::PromoteDemotedChanges, this,
                       origin, base::RetainedRef(file_system_context),
                       std::move(callback)));
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  if (!backend->change_tracker()->PromoteDemotedChanges()) {
    ui_task_runner_->PostTask(FROM_HERE, std::move(callback));
    return;
  }

  io_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalFileSyncContext::UpdateChangesForOrigin,
                                this, origin, std::move(callback)));
}

void LocalFileSyncContext::UpdateChangesForOrigin(const GURL& origin,
                                                  base::OnceClosure callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    return;
  origins_with_pending_changes_.insert(origin);
  ScheduleNotifyChangesUpdatedOnIOThread(std::move(callback));
}

void LocalFileSyncContext::AddOriginChangeObserver(
    LocalOriginChangeObserver* observer) {
  origin_change_observers_.AddObserver(observer);
}

void LocalFileSyncContext::RemoveOriginChangeObserver(
    LocalOriginChangeObserver* observer) {
  origin_change_observers_.RemoveObserver(observer);
}

base::WeakPtr<SyncableFileOperationRunner>
LocalFileSyncContext::operation_runner() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (operation_runner_)
    return operation_runner_->AsWeakPtr();
  return base::WeakPtr<SyncableFileOperationRunner>();
}

LocalFileSyncStatus* LocalFileSyncContext::sync_status() const {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  return sync_status_.get();
}

void LocalFileSyncContext::OnSyncEnabled(const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    return;
  UpdateChangesForOrigin(url.origin().GetURL(), base::DoNothing());
  if (url_syncable_callback_.is_null() ||
      sync_status()->IsWriting(url_waiting_sync_on_io_)) {
    return;
  }
  // TODO(kinuko): may want to check how many pending tasks we have.
  ui_task_runner_->PostTask(FROM_HERE, std::move(url_syncable_callback_));
}

void LocalFileSyncContext::OnWriteEnabled(const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  // Nothing to do for now.
}

LocalFileSyncContext::~LocalFileSyncContext() {
}

void LocalFileSyncContext::ScheduleNotifyChangesUpdatedOnIOThread(
    base::OnceClosure callback) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    return;
  pending_completion_callbacks_.push_back(std::move(callback));
  if (base::Time::Now() > last_notified_changes_ + NotifyChangesDuration()) {
    NotifyAvailableChangesOnIOThread();
  } else if (!timer_on_io_->IsRunning()) {
    timer_on_io_->Start(
        FROM_HERE, NotifyChangesDuration(),
        base::BindOnce(&LocalFileSyncContext::NotifyAvailableChangesOnIOThread,
                       base::Unretained(this)));
  }
}

void LocalFileSyncContext::NotifyAvailableChangesOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    return;

  std::vector<base::OnceClosure> completion_callbacks;
  completion_callbacks.swap(pending_completion_callbacks_);

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&LocalFileSyncContext::NotifyAvailableChanges,
                                this, origins_with_pending_changes_,
                                std::move(completion_callbacks)));
  last_notified_changes_ = base::Time::Now();
  origins_with_pending_changes_.clear();
}

void LocalFileSyncContext::NotifyAvailableChanges(
    const std::set<GURL>& origins,
    std::vector<base::OnceClosure> callbacks) {
  for (auto& observer : origin_change_observers_)
    observer.OnChangesAvailableInOrigins(origins);
  for (auto& callback : callbacks)
    std::move(callback).Run();
}

void LocalFileSyncContext::ShutdownOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  shutdown_on_io_ = true;
  operation_runner_.reset();
  root_delete_helper_.reset();
  sync_status_.reset();
  timer_on_io_.reset();
}

void LocalFileSyncContext::InitializeFileSystemContextOnIOThread(
    const GURL& source_url,
    FileSystemContext* file_system_context,
    const GURL& /* root */,
    const std::string& /* name */,
    base::File::Error error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    error = base::File::FILE_ERROR_ABORT;
  if (error != base::File::FILE_OK) {
    DidInitialize(source_url, file_system_context,
                  FileErrorToSyncStatusCode(error));
    return;
  }
  DCHECK(file_system_context);
  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  if (!backend->change_tracker()) {
    // Create and initialize LocalFileChangeTracker and call back this method
    // later again.
    std::set<GURL>* origins_with_changes = new std::set<GURL>;
    std::unique_ptr<LocalFileChangeTracker>* tracker_ptr(
        new std::unique_ptr<LocalFileChangeTracker>);
    file_system_context->default_file_task_runner()->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            &LocalFileSyncContext::InitializeChangeTrackerOnFileThread, this,
            tracker_ptr, base::RetainedRef(file_system_context),
            origins_with_changes),
        base::BindOnce(
            &LocalFileSyncContext::DidInitializeChangeTrackerOnIOThread, this,
            base::Owned(tracker_ptr), source_url,
            base::RetainedRef(file_system_context),
            base::Owned(origins_with_changes)));
    return;
  }
  if (!operation_runner_) {
    DCHECK(!sync_status_);
    DCHECK(!timer_on_io_);
    sync_status_ = std::make_unique<LocalFileSyncStatus>();
    timer_on_io_ = std::make_unique<base::OneShotTimer>();
    operation_runner_ = std::make_unique<SyncableFileOperationRunner>(
        kMaxConcurrentSyncableOperation, sync_status_.get());
    sync_status_->AddObserver(this);
  }
  backend->set_sync_context(this);
  DidInitialize(source_url, file_system_context,
                SYNC_STATUS_OK);
}

SyncStatusCode LocalFileSyncContext::InitializeChangeTrackerOnFileThread(
    std::unique_ptr<LocalFileChangeTracker>* tracker_ptr,
    FileSystemContext* file_system_context,
    std::set<GURL>* origins_with_changes) {
  DCHECK(file_system_context);
  DCHECK(tracker_ptr);
  DCHECK(origins_with_changes);
  *tracker_ptr = std::make_unique<LocalFileChangeTracker>(
      file_system_context->partition_path(), env_override_,
      file_system_context->default_file_task_runner());
  const SyncStatusCode status = (*tracker_ptr)->Initialize(file_system_context);
  if (status != SYNC_STATUS_OK)
    return status;

  // Get all origins that have pending changes.
  FileSystemURLQueue urls;
  (*tracker_ptr)->GetNextChangedURLs(&urls, 0);
  for (FileSystemURLQueue::iterator iter = urls.begin();
       iter != urls.end(); ++iter) {
    origins_with_changes->insert(iter->origin().GetURL());
  }

  // Creates snapshot directory.
  base::CreateDirectory(local_base_path_.Append(kSnapshotDir));

  return status;
}

void LocalFileSyncContext::DidInitializeChangeTrackerOnIOThread(
    std::unique_ptr<LocalFileChangeTracker>* tracker_ptr,
    const GURL& source_url,
    FileSystemContext* file_system_context,
    std::set<GURL>* origins_with_changes,
    SyncStatusCode status) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(file_system_context);
  DCHECK(origins_with_changes);
  if (shutdown_on_io_)
    status = SYNC_STATUS_ABORT;
  if (status != SYNC_STATUS_OK) {
    DidInitialize(source_url, file_system_context, status);
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  backend->SetLocalFileChangeTracker(std::move(*tracker_ptr));

  origins_with_pending_changes_.insert(origins_with_changes->begin(),
                                       origins_with_changes->end());
  ScheduleNotifyChangesUpdatedOnIOThread(base::DoNothing());

  InitializeFileSystemContextOnIOThread(source_url, file_system_context,
                                        GURL(), std::string(),
                                        base::File::FILE_OK);
}

void LocalFileSyncContext::DidInitialize(
    const GURL& source_url,
    FileSystemContext* file_system_context,
    SyncStatusCode status) {
  if (!ui_task_runner_->RunsTasksInCurrentSequence()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::DidInitialize, this, source_url,
                       base::RetainedRef(file_system_context), status));
    return;
  }
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(!base::Contains(file_system_contexts_, file_system_context));
  DCHECK(base::Contains(pending_initialize_callbacks_, file_system_context));

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());

  file_system_contexts_.insert(file_system_context);

  StatusCallbackQueue& callback_queue =
      pending_initialize_callbacks_[file_system_context];
  while (!callback_queue.empty()) {
    ui_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_queue.front()), status));
    callback_queue.pop_front();
  }
  pending_initialize_callbacks_.erase(file_system_context);
}

std::unique_ptr<LocalFileSyncContext::FileSystemURLQueue>
LocalFileSyncContext::GetNextURLsForSyncOnFileThread(
    FileSystemContext* file_system_context) {
  DCHECK(file_system_context);
  DCHECK(file_system_context->default_file_task_runner()->
             RunsTasksInCurrentSequence());
  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  std::unique_ptr<FileSystemURLQueue> urls(new FileSystemURLQueue);
  backend->change_tracker()->GetNextChangedURLs(
      urls.get(), kMaxURLsToFetchForLocalSync);
  for (FileSystemURLQueue::iterator iter = urls->begin();
       iter != urls->end(); ++iter)
    backend->change_tracker()->DemoteChangesForURL(*iter);

  return urls;
}

void LocalFileSyncContext::TryPrepareForLocalSync(
    FileSystemContext* file_system_context,
    LocalFileSyncInfoCallback callback,
    std::unique_ptr<FileSystemURLQueue> urls) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  DCHECK(urls);

  if (shutdown_on_ui_) {
    std::move(callback).Run(SYNC_STATUS_ABORT, LocalFileSyncInfo(),
                            storage::ScopedFile());
    return;
  }

  if (urls->empty()) {
    std::move(callback).Run(SYNC_STATUS_NO_CHANGE_TO_SYNC, LocalFileSyncInfo(),
                            storage::ScopedFile());
    return;
  }

  const FileSystemURL url = urls->front();
  urls->pop_front();

  PrepareForSync(
      file_system_context, url, SYNC_SNAPSHOT,
      base::BindOnce(&LocalFileSyncContext::DidTryPrepareForLocalSync, this,
                     base::RetainedRef(file_system_context), std::move(urls),
                     std::move(callback)));
}

void LocalFileSyncContext::DidTryPrepareForLocalSync(
    FileSystemContext* file_system_context,
    std::unique_ptr<FileSystemURLQueue> remaining_urls,
    LocalFileSyncInfoCallback callback,
    SyncStatusCode status,
    const LocalFileSyncInfo& sync_file_info,
    storage::ScopedFile snapshot) {
  DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  if (status != SYNC_STATUS_FILE_BUSY) {
    PromoteDemotedChangesForURLs(file_system_context,
                                 std::move(remaining_urls));
    std::move(callback).Run(status, sync_file_info, std::move(snapshot));
    return;
  }

  PromoteDemotedChangesForURL(file_system_context, sync_file_info.url);

  // Recursively call TryPrepareForLocalSync with remaining_urls.
  TryPrepareForLocalSync(file_system_context, std::move(callback),
                         std::move(remaining_urls));
}

void LocalFileSyncContext::PromoteDemotedChangesForURL(
    FileSystemContext* file_system_context,
    const FileSystemURL& url) {
  DCHECK(file_system_context);
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    if (shutdown_on_ui_)
      return;
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::PromoteDemotedChangesForURL, this,
                       base::RetainedRef(file_system_context), url));
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  backend->change_tracker()->PromoteDemotedChangesForURL(url);
}

void LocalFileSyncContext::PromoteDemotedChangesForURLs(
    FileSystemContext* file_system_context,
    std::unique_ptr<FileSystemURLQueue> urls) {
  DCHECK(file_system_context);
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    if (shutdown_on_ui_)
      return;
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::PromoteDemotedChangesForURLs,
                       this, base::RetainedRef(file_system_context),
                       std::move(urls)));
    return;
  }

  for (FileSystemURLQueue::iterator iter = urls->begin();
       iter != urls->end(); ++iter)
    PromoteDemotedChangesForURL(file_system_context, *iter);
}

void LocalFileSyncContext::DidGetWritingStatusForSync(
    FileSystemContext* file_system_context,
    SyncStatusCode status,
    const FileSystemURL& url,
    SyncMode sync_mode,
    LocalFileSyncInfoCallback callback) {
  // This gets called on UI thread and relays the task on FILE thread.
  DCHECK(file_system_context);
  if (!file_system_context->default_file_task_runner()->
          RunsTasksInCurrentSequence()) {
    DCHECK(ui_task_runner_->RunsTasksInCurrentSequence());
    if (shutdown_on_ui_) {
      std::move(callback).Run(SYNC_STATUS_ABORT, LocalFileSyncInfo(),
                              storage::ScopedFile());
      return;
    }
    file_system_context->default_file_task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::DidGetWritingStatusForSync, this,
                       base::RetainedRef(file_system_context), status, url,
                       sync_mode, std::move(callback)));
    return;
  }

  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  FileChangeList changes;
  backend->change_tracker()->GetChangesForURL(url, &changes);

  base::FilePath platform_path;
  base::File::Info file_info;
  FileSystemFileUtil* file_util =
      file_system_context->sandbox_delegate()->sync_file_util();
  DCHECK(file_util);

  base::File::Error file_error = file_util->GetFileInfo(
      std::make_unique<FileSystemOperationContext>(file_system_context).get(),
      url, &file_info, &platform_path);

  storage::ScopedFile snapshot;
  if (file_error == base::File::FILE_OK && sync_mode == SYNC_SNAPSHOT) {
    base::FilePath snapshot_path;
    base::CreateTemporaryFileInDir(local_base_path_.Append(kSnapshotDir),
                                   &snapshot_path);
    if (base::CopyFile(platform_path, snapshot_path)) {
      platform_path = snapshot_path;
      snapshot =
          storage::ScopedFile(snapshot_path,
                              storage::ScopedFile::DELETE_ON_SCOPE_OUT,
                              file_system_context->default_file_task_runner());
    }
  }

  if (status == SYNC_STATUS_OK &&
      file_error != base::File::FILE_OK &&
      file_error != base::File::FILE_ERROR_NOT_FOUND) {
    status = FileErrorToSyncStatusCode(file_error);
  }

  DCHECK(!file_info.is_symbolic_link);

  SyncFileType file_type = SYNC_FILE_TYPE_FILE;
  if (file_error == base::File::FILE_ERROR_NOT_FOUND)
    file_type = SYNC_FILE_TYPE_UNKNOWN;
  else if (file_info.is_directory)
    file_type = SYNC_FILE_TYPE_DIRECTORY;

  LocalFileSyncInfo sync_file_info;
  sync_file_info.url = url;
  sync_file_info.local_file_path = platform_path;
  sync_file_info.metadata.file_type = file_type;
  sync_file_info.metadata.size = file_info.size;
  sync_file_info.metadata.last_modified = file_info.last_modified;
  sync_file_info.changes = changes;

  if (status == SYNC_STATUS_OK && sync_mode == SYNC_SNAPSHOT) {
    if (!changes.empty()) {
      // Now we create an empty mirror change record for URL (and we record
      // changes to both mirror and original records during sync), so that
      // we can reset to the mirror when the sync succeeds.
      backend->change_tracker()->CreateFreshMirrorForURL(url);
    }

    // 'Unlock' the file for snapshot sync.
    // (But keep it in writing status so that no other sync starts on
    // the same URL)
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&LocalFileSyncContext::ClearSyncFlagOnIOThread, this,
                       url, true /* for_snapshot_sync */));
  }

  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), status, sync_file_info,
                                std::move(snapshot)));
}

void LocalFileSyncContext::ClearSyncFlagOnIOThread(
    const FileSystemURL& url,
    bool for_snapshot_sync) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    return;
  sync_status()->EndSyncing(url);

  if (for_snapshot_sync) {
    // The caller will hold shared lock on this one.
    sync_status()->StartWriting(url);
    return;
  }

  // Since a sync has finished the number of changes must have been updated.
  UpdateChangesForOrigin(url.origin().GetURL(), base::DoNothing());
}

void LocalFileSyncContext::FinalizeSnapshotSyncOnIOThread(
    const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  if (shutdown_on_io_)
    return;
  sync_status()->EndWriting(url);

  // Since a sync has finished the number of changes must have been updated.
  UpdateChangesForOrigin(url.origin().GetURL(), base::DoNothing());
}

void LocalFileSyncContext::DidApplyRemoteChange(
    const FileSystemURL& url,
    SyncStatusCallback callback_on_ui,
    base::File::Error file_error) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  root_delete_helper_.reset();
  ui_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback_on_ui),
                                FileErrorToSyncStatusCode(file_error)));
}

void LocalFileSyncContext::DidGetFileMetadata(
    SyncFileMetadataCallback callback,
    base::File::Error file_error,
    const base::File::Info& file_info) {
  DCHECK(io_task_runner_->RunsTasksInCurrentSequence());
  SyncFileMetadata metadata;
  if (file_error == base::File::FILE_OK) {
    metadata.file_type = file_info.is_directory ?
        SYNC_FILE_TYPE_DIRECTORY : SYNC_FILE_TYPE_FILE;
    metadata.size = file_info.size;
    metadata.last_modified = file_info.last_modified;
  }
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), FileErrorToSyncStatusCode(file_error),
                     metadata));
}

base::TimeDelta LocalFileSyncContext::NotifyChangesDuration() {
  if (mock_notify_changes_duration_in_sec_ >= 0)
    return base::Seconds(mock_notify_changes_duration_in_sec_);
  return base::Seconds(kNotifyChangesDurationInSec);
}

void LocalFileSyncContext::DidCreateDirectoryForCopyIn(
    FileSystemContext* file_system_context,
    const base::FilePath& local_path,
    const FileSystemURL& dest_url,
    StatusCallback callback,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    std::move(callback).Run(error);
    return;
  }

  FileSystemURL url_for_sync = CreateSyncableFileSystemURLForSync(
      file_system_context, dest_url);
  file_system_context->operation_runner()->CopyInForeignFile(
      local_path, url_for_sync, std::move(callback));
}

}  // namespace sync_file_system
