// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/fileapi/observable_file_system_operation_impl.h"

#include "chrome/browser/ash/fileapi/file_change_service.h"
#include "chrome/browser/ash/fileapi/file_change_service_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"

namespace ash {

namespace {

using StatusCallback = storage::FileSystemOperation::StatusCallback;
using WriteCallback = storage::FileSystemOperation::WriteCallback;

// Helpers ---------------------------------------------------------------------

// Returns the `FileChangeService` associated with the given `account_id`.
FileChangeService* GetFileChangeService(const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  return profile ? FileChangeServiceFactory::GetInstance()->GetService(profile)
                 : nullptr;
}

// Notifies the `FileChangeService` associated with the given `account_id` of a
// file being moved form `src` to `dst`. This method may only be called from the
// browser UI thread.
void NotifyFileMovedOnUiThread(const AccountId& account_id,
                               const storage::FileSystemURL& src,
                               const storage::FileSystemURL& dst) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FileChangeService* service = GetFileChangeService(account_id);
  if (service)
    service->NotifyFileMoved(src, dst);
}

// Notifies the `FileChangeService` associated with the given `account_id` of a
// file under `url` getting modified. This method may only be called from the
// browser UI thread.
void NotifyFileModifiedOnUiThread(const AccountId& account_id,
                                  const storage::FileSystemURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FileChangeService* service = GetFileChangeService(account_id);
  if (service)
    service->NotifyFileModified(url);
}

// Returns a `WriteCallback` which runs the specified callbacks in order.
WriteCallback RunInOrderCallback(WriteCallback a, WriteCallback b) {
  return base::BindRepeating(
      [](WriteCallback a, WriteCallback b, base::File::Error result,
         int64_t bytes, bool complete) {
        std::move(a).Run(result, bytes, complete);
        std::move(b).Run(result, bytes, complete);
      },
      std::move(a), std::move(b));
}

// Returns a `StatusCallback` which runs the specified callbacks in order.
StatusCallback RunInOrderCallback(StatusCallback a, StatusCallback b) {
  return base::BindOnce(
      [](StatusCallback a, StatusCallback b, base::File::Error result) {
        std::move(a).Run(result);
        std::move(b).Run(result);
      },
      std::move(a), std::move(b));
}

// Returns a `StatusCallback` which runs the specified `closure` on the browser
// UI thread if `result` indicates success.
StatusCallback RunOnUiThreadOnSuccessCallback(base::OnceClosure closure) {
  return base::BindOnce(
      [](base::OnceClosure closure, base::File::Error result) {
        if (result == base::File::FILE_OK) {
          auto task_runner = content::GetUIThreadTaskRunner({});
          task_runner->PostTask(FROM_HERE, std::move(closure));
        }
      },
      std::move(closure));
}

// Returns a `WriteCallback` which runs the specified `closure` on the browser
// UI thread if `complete` is set.
WriteCallback RunOnUiThreadOnCompleteCallback(
    const base::RepeatingClosure& closure) {
  return base::BindRepeating(
      [](const base::RepeatingClosure& closure, base::File::Error result,
         int64_t bytes, bool complete) {
        if (complete && result == base::File::FILE_OK) {
          auto task_runner = content::GetUIThreadTaskRunner({});
          task_runner->PostTask(FROM_HERE, std::move(closure));
        }
      },
      std::move(closure));
}

}  // namespace

// ObservableFileSystemOperationImpl -------------------------------------------

ObservableFileSystemOperationImpl::ObservableFileSystemOperationImpl(
    const AccountId& account_id,
    storage::OperationType type,
    const storage::FileSystemURL& url,
    storage::FileSystemContext* file_system_context,
    std::unique_ptr<storage::FileSystemOperationContext> operation_context)
    : storage::FileSystemOperationImpl(
          type,
          url,
          file_system_context,
          std::move(operation_context),
          storage::FileSystemOperation::CreatePassKey()),
      account_id_(account_id) {}

ObservableFileSystemOperationImpl::~ObservableFileSystemOperationImpl() =
    default;

void ObservableFileSystemOperationImpl::Move(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst,
    CopyOrMoveOptionSet options,
    ErrorBehavior error_behavior,
    std::unique_ptr<storage::CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    StatusCallback callback) {
  storage::FileSystemOperationImpl::Move(
      src, dst, options, error_behavior, std::move(copy_or_move_hook_delegate),
      RunInOrderCallback(
          RunOnUiThreadOnSuccessCallback(base::BindOnce(
              &NotifyFileMovedOnUiThread, account_id_, src, dst)),
          std::move(callback)));
}

void ObservableFileSystemOperationImpl::MoveFileLocal(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst,
    CopyOrMoveOptionSet options,
    StatusCallback callback) {
  storage::FileSystemOperationImpl::MoveFileLocal(
      src, dst, options,
      RunInOrderCallback(
          RunOnUiThreadOnSuccessCallback(base::BindOnce(
              &NotifyFileMovedOnUiThread, account_id_, src, dst)),
          std::move(callback)));
}

void ObservableFileSystemOperationImpl::WriteBlob(
    const storage::FileSystemURL& url,
    std::unique_ptr<storage::FileWriterDelegate> writer_delegate,
    std::unique_ptr<storage::BlobReader> blob_reader,
    const WriteCallback& callback) {
  storage::FileSystemOperationImpl::WriteBlob(
      url, std::move(writer_delegate), std::move(blob_reader),
      RunInOrderCallback(RunOnUiThreadOnCompleteCallback(base::BindRepeating(
                             &NotifyFileModifiedOnUiThread, account_id_, url)),
                         std::move(callback)));
}

void ObservableFileSystemOperationImpl::Write(
    const storage::FileSystemURL& url,
    std::unique_ptr<storage::FileWriterDelegate> writer_delegate,
    mojo::ScopedDataPipeConsumerHandle data_pipe,
    const WriteCallback& callback) {
  storage::FileSystemOperationImpl::Write(
      url, std::move(writer_delegate), std::move(data_pipe),
      RunInOrderCallback(RunOnUiThreadOnCompleteCallback(base::BindRepeating(
                             &NotifyFileModifiedOnUiThread, account_id_, url)),
                         std::move(callback)));
}

void ObservableFileSystemOperationImpl::Truncate(
    const storage::FileSystemURL& url,
    int64_t length,
    StatusCallback callback) {
  storage::FileSystemOperationImpl::Truncate(
      url, length,
      RunInOrderCallback(RunOnUiThreadOnSuccessCallback(base::BindOnce(
                             &NotifyFileModifiedOnUiThread, account_id_, url)),
                         std::move(callback)));
}

}  // namespace ash
