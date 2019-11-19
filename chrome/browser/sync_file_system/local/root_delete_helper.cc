// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/root_delete_helper.h"

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "chrome/browser/sync_file_system/local/local_file_change_tracker.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_status.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/common/file_system/file_system_util.h"

namespace sync_file_system {

namespace {

// This runs on FileSystemContext's default_file_task_runner.
void ResetFileChangeTracker(storage::FileSystemContext* file_system_context,
                            const storage::FileSystemURL& url) {
  DCHECK(file_system_context->default_file_task_runner()->
             RunsTasksInCurrentSequence());
  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  DCHECK(backend->change_tracker());
  backend->change_tracker()->ResetForFileSystem(url.origin().GetURL(),
                                                url.type());
}

}  // namespace

RootDeleteHelper::RootDeleteHelper(
    storage::FileSystemContext* file_system_context,
    LocalFileSyncStatus* sync_status,
    const storage::FileSystemURL& url,
    const FileStatusCallback& callback)
    : file_system_context_(file_system_context),
      url_(url),
      callback_(callback),
      sync_status_(sync_status) {
  DCHECK(file_system_context_.get());
  DCHECK(url_.is_valid());
  DCHECK(!callback_.is_null());
  DCHECK(sync_status_);
  // This is expected to run on the filesystem root.
  DCHECK(storage::VirtualPath::IsRootPath(url.path()));
}

RootDeleteHelper::~RootDeleteHelper() {
}

void RootDeleteHelper::Run() {
  util::Log(logging::LOG_VERBOSE, FROM_HERE,
            "Deleting the entire local filesystem for remote root deletion: "
            "%s", url_.DebugString().c_str());

  file_system_context_->DeleteFileSystem(
      url_.origin().GetURL(), url_.type(),
      base::Bind(&RootDeleteHelper::DidDeleteFileSystem,
                 weak_factory_.GetWeakPtr()));
}

void RootDeleteHelper::DidDeleteFileSystem(base::File::Error error) {
  // Ignore errors, no idea how to deal with it.

  DCHECK(!sync_status_->IsWritable(url_));
  DCHECK(!sync_status_->IsWriting(url_));

  // All writes to the entire file system must be now blocked, so we have
  // to be able to safely reset the local changes and sync statuses for it.
  // TODO(kinuko): This should be probably automatically handled in
  // DeleteFileSystem via QuotaUtil::DeleteOriginDataOnFileThread.
  file_system_context_->default_file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ResetFileChangeTracker,
                     base::RetainedRef(file_system_context_), url_),
      base::BindOnce(&RootDeleteHelper::DidResetFileChangeTracker,
                     weak_factory_.GetWeakPtr()));
}

void RootDeleteHelper::DidResetFileChangeTracker() {
  DCHECK(!sync_status_->IsWritable(url_));
  DCHECK(!sync_status_->IsWriting(url_));

  // Reopening the filesystem.
  file_system_context_->sandbox_delegate()->OpenFileSystem(
      url_.origin().GetURL(), url_.type(),
      storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
      base::Bind(&RootDeleteHelper::DidOpenFileSystem,
                 weak_factory_.GetWeakPtr()),
      GURL());
}

void RootDeleteHelper::DidOpenFileSystem(const GURL& /* root */,
                                         const std::string& /* name */,
                                         base::File::Error error) {
  FileStatusCallback callback = callback_;
  callback.Run(error);
}

}  // namespace sync_file_system
