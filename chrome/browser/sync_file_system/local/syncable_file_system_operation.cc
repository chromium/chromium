// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/local/syncable_file_system_operation.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "chrome/browser/sync_file_system/local/local_file_sync_context.h"
#include "chrome/browser/sync_file_system/local/sync_file_system_backend.h"
#include "chrome/browser/sync_file_system/local/syncable_file_operation_runner.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "content/public/browser/browser_thread.h"
#include "net/url_request/url_request.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/file_writer_delegate.h"

using storage::FileSystemURL;

namespace sync_file_system {

namespace {

void WriteCallbackAdapter(
    const SyncableFileSystemOperation::WriteCallback& callback,
    base::File::Error status) {
  callback.Run(status, 0, true);
}

}  // namespace

class SyncableFileSystemOperation::QueueableTask
    : public SyncableFileOperationRunner::Task {
 public:
  QueueableTask(base::WeakPtr<SyncableFileSystemOperation> operation,
                const base::Closure& task)
      : operation_(operation),
        task_(task),
        target_paths_(operation->target_paths_) {}

  ~QueueableTask() override { DCHECK(!operation_); }

  void Run() override {
    if (!operation_)
      return;
    DCHECK(!task_.is_null());
    task_.Run();
    operation_.reset();
  }

  void Cancel() override {
    DCHECK(!task_.is_null());
    if (operation_)
      operation_->OnCancelled();
    task_.Reset();
    operation_.reset();
  }

  const std::vector<FileSystemURL>& target_paths() const override {
    return target_paths_;
  }

 private:
  base::WeakPtr<SyncableFileSystemOperation> operation_;
  base::Closure task_;
  const std::vector<FileSystemURL> target_paths_;
  DISALLOW_COPY_AND_ASSIGN(QueueableTask);
};

SyncableFileSystemOperation::~SyncableFileSystemOperation() {}

void SyncableFileSystemOperation::CreateFile(const FileSystemURL& url,
                                             bool exclusive,
                                             StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = std::move(callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::CreateFile,
                 base::Unretained(impl_.get()), url, exclusive,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::CreateDirectory(const FileSystemURL& url,
                                                  bool exclusive,
                                                  bool recursive,
                                                  StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = std::move(callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::CreateDirectory,
                 base::Unretained(impl_.get()), url, exclusive, recursive,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::Copy(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    ErrorBehavior error_behavior,
    const CopyProgressCallback& progress_callback,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(dest_url);
  completion_callback_ = std::move(callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Copy, base::Unretained(impl_.get()),
                 src_url, dest_url, option, error_behavior, progress_callback,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::Move(const FileSystemURL& src_url,
                                       const FileSystemURL& dest_url,
                                       CopyOrMoveOption option,
                                       StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(src_url);
  target_paths_.push_back(dest_url);
  completion_callback_ = std::move(callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Move, base::Unretained(impl_.get()),
                 src_url, dest_url, option,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::DirectoryExists(const FileSystemURL& url,
                                                  StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->DirectoryExists(url, std::move(callback));
}

void SyncableFileSystemOperation::FileExists(const FileSystemURL& url,
                                             StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->FileExists(url, std::move(callback));
}

void SyncableFileSystemOperation::GetMetadata(const FileSystemURL& url,
                                              int fields,
                                              GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->GetMetadata(url, fields, std::move(callback));
}

void SyncableFileSystemOperation::ReadDirectory(
    const FileSystemURL& url,
    const ReadDirectoryCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // This is a read operation and there'd be no hard to let it go even if
  // directory operation is disabled. (And we should allow this if it's made
  // on the root directory)
  impl_->ReadDirectory(url, callback);
}

void SyncableFileSystemOperation::Remove(const FileSystemURL& url,
                                         bool recursive,
                                         StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = std::move(callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Remove, base::Unretained(impl_.get()),
                 url, recursive,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::WriteBlob(
    const FileSystemURL& url,
    std::unique_ptr<storage::FileWriterDelegate> writer_delegate,
    std::unique_ptr<storage::BlobReader> blob_reader,
    const WriteCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, 0, true);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = base::BindOnce(&WriteCallbackAdapter, callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(
          &FileSystemOperation::WriteBlob, base::Unretained(impl_.get()), url,
          base::Passed(&writer_delegate), base::Passed(&blob_reader),
          base::Bind(&self::DidWrite, weak_factory_.GetWeakPtr(), callback))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::Write(
    const FileSystemURL& url,
    std::unique_ptr<storage::FileWriterDelegate> writer_delegate,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    const WriteCallback& callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    callback.Run(base::File::FILE_ERROR_NOT_FOUND, 0, true);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = base::BindOnce(&WriteCallbackAdapter, callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(
          &FileSystemOperation::Write, base::Unretained(impl_.get()), url,
          base::Passed(&writer_delegate), base::Passed(&data_pipe),
          base::Bind(&self::DidWrite, weak_factory_.GetWeakPtr(), callback))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::Truncate(const FileSystemURL& url,
                                           int64_t length,
                                           StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(url);
  completion_callback_ = std::move(callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::Truncate, base::Unretained(impl_.get()),
                 url, length,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::TouchFile(
    const FileSystemURL& url,
    const base::Time& last_access_time,
    const base::Time& last_modified_time,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->TouchFile(url, last_access_time, last_modified_time,
                   std::move(callback));
}

void SyncableFileSystemOperation::OpenFile(const FileSystemURL& url,
                                           int file_flags,
                                           OpenFileCallback callback) {
  NOTREACHED();
}

void SyncableFileSystemOperation::Cancel(StatusCallback cancel_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->Cancel(std::move(cancel_callback));
}

void SyncableFileSystemOperation::CreateSnapshotFile(
    const FileSystemURL& path,
    SnapshotFileCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->CreateSnapshotFile(path, std::move(callback));
}

void SyncableFileSystemOperation::CopyInForeignFile(
    const base::FilePath& src_local_disk_path,
    const FileSystemURL& dest_url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!operation_runner_.get()) {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
    return;
  }
  DCHECK(operation_runner_.get());
  target_paths_.push_back(dest_url);
  completion_callback_ = std::move(callback);
  std::unique_ptr<SyncableFileOperationRunner::Task> task(new QueueableTask(
      weak_factory_.GetWeakPtr(),
      base::Bind(&FileSystemOperation::CopyInForeignFile,
                 base::Unretained(impl_.get()), src_local_disk_path, dest_url,
                 base::Bind(&self::DidFinish, weak_factory_.GetWeakPtr()))));
  operation_runner_->PostOperationTask(std::move(task));
}

void SyncableFileSystemOperation::RemoveFile(const FileSystemURL& url,
                                             StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->RemoveFile(url, std::move(callback));
}

void SyncableFileSystemOperation::RemoveDirectory(const FileSystemURL& url,
                                                  StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->RemoveDirectory(url, std::move(callback));
}

void SyncableFileSystemOperation::CopyFileLocal(
    const FileSystemURL& src_url,
    const FileSystemURL& dest_url,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->CopyFileLocal(src_url, dest_url, option, progress_callback,
                       std::move(callback));
}

void SyncableFileSystemOperation::MoveFileLocal(const FileSystemURL& src_url,
                                                const FileSystemURL& dest_url,
                                                CopyOrMoveOption option,
                                                StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  impl_->MoveFileLocal(src_url, dest_url, option, std::move(callback));
}

base::File::Error SyncableFileSystemOperation::SyncGetPlatformPath(
    const FileSystemURL& url,
    base::FilePath* platform_path) {
  return impl_->SyncGetPlatformPath(url, platform_path);
}

SyncableFileSystemOperation::SyncableFileSystemOperation(
    const FileSystemURL& url,
    storage::FileSystemContext* file_system_context,
    std::unique_ptr<storage::FileSystemOperationContext> operation_context)
    : url_(url) {
  DCHECK(file_system_context);
  SyncFileSystemBackend* backend =
      SyncFileSystemBackend::GetBackend(file_system_context);
  DCHECK(backend);
  if (!backend->sync_context()) {
    // Syncable FileSystem is opened in a file system context which doesn't
    // support (or is not initialized for) the API.
    // Returning here to leave operation_runner_ as NULL.
    return;
  }
  impl_.reset(storage::FileSystemOperation::Create(
      url_, file_system_context, std::move(operation_context)));
  operation_runner_ = backend->sync_context()->operation_runner();
}

void SyncableFileSystemOperation::DidFinish(base::File::Error status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!completion_callback_.is_null());
  if (operation_runner_.get())
    operation_runner_->OnOperationCompleted(target_paths_);
  std::move(completion_callback_).Run(status);
}

void SyncableFileSystemOperation::DidWrite(const WriteCallback& callback,
                                           base::File::Error result,
                                           int64_t bytes,
                                           bool complete) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!complete) {
    callback.Run(result, bytes, complete);
    return;
  }
  if (operation_runner_.get())
    operation_runner_->OnOperationCompleted(target_paths_);
  callback.Run(result, bytes, complete);
}

void SyncableFileSystemOperation::OnCancelled() {
  DCHECK(!completion_callback_.is_null());
  std::move(completion_callback_).Run(base::File::FILE_ERROR_ABORT);
}

}  // namespace sync_file_system
