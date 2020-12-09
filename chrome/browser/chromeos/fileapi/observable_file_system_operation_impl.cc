// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/fileapi/observable_file_system_operation_impl.h"

#include "chrome/browser/chromeos/fileapi/file_change_service.h"
#include "chrome/browser/chromeos/fileapi/file_change_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {

namespace {

using StatusCallback = storage::FileSystemOperation::StatusCallback;

// Helpers ---------------------------------------------------------------------

// Returns the `FileChangeService` associated with the given `account_id`.
FileChangeService* GetFileChangeService(const AccountId& account_id) {
  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(account_id);
  return profile ? FileChangeServiceFactory::GetInstance()->GetService(profile)
                 : nullptr;
}

// Notifies the `FileChangeService` associated with the given `account_id` of a
// file being copied from `src` to `dst`. This method may only be called from
// the browser UI thread.
void NotifyFileCopiedOnUiThread(const AccountId& account_id,
                                const storage::FileSystemURL& src,
                                const storage::FileSystemURL& dst) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  FileChangeService* service = GetFileChangeService(account_id);
  if (service)
    service->NotifyFileCopied(src, dst);
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

}  // namespace

// ObservableFileSystemOperationImpl -------------------------------------------

ObservableFileSystemOperationImpl::ObservableFileSystemOperationImpl(
    const AccountId& account_id,
    const storage::FileSystemURL& url,
    storage::FileSystemContext* file_system_context,
    std::unique_ptr<storage::FileSystemOperationContext> operation_context)
    : storage::FileSystemOperationImpl(url,
                                       file_system_context,
                                       std::move(operation_context)),
      account_id_(account_id) {}

ObservableFileSystemOperationImpl::~ObservableFileSystemOperationImpl() =
    default;

void ObservableFileSystemOperationImpl::Copy(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst,
    CopyOrMoveOption option,
    ErrorBehavior error_behavior,
    const CopyProgressCallback& progress_callback,
    StatusCallback callback) {
  storage::FileSystemOperationImpl::Copy(
      src, dst, option, error_behavior, progress_callback,
      RunInOrderCallback(
          RunOnUiThreadOnSuccessCallback(base::BindOnce(
              &NotifyFileCopiedOnUiThread, account_id_, src, dst)),
          std::move(callback)));
}

void ObservableFileSystemOperationImpl::CopyFileLocal(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst,
    CopyOrMoveOption option,
    const CopyFileProgressCallback& progress_callback,
    StatusCallback callback) {
  storage::FileSystemOperationImpl::CopyFileLocal(
      src, dst, option, progress_callback,
      RunInOrderCallback(
          RunOnUiThreadOnSuccessCallback(base::BindOnce(
              &NotifyFileCopiedOnUiThread, account_id_, src, dst)),
          std::move(callback)));
}

void ObservableFileSystemOperationImpl::Move(const storage::FileSystemURL& src,
                                             const storage::FileSystemURL& dst,
                                             CopyOrMoveOption option,
                                             StatusCallback callback) {
  storage::FileSystemOperationImpl::Move(
      src, dst, option,
      RunInOrderCallback(
          RunOnUiThreadOnSuccessCallback(base::BindOnce(
              &NotifyFileMovedOnUiThread, account_id_, src, dst)),
          std::move(callback)));
}

void ObservableFileSystemOperationImpl::MoveFileLocal(
    const storage::FileSystemURL& src,
    const storage::FileSystemURL& dst,
    CopyOrMoveOption option,
    StatusCallback callback) {
  storage::FileSystemOperationImpl::MoveFileLocal(
      src, dst, option,
      RunInOrderCallback(
          RunOnUiThreadOnSuccessCallback(base::BindOnce(
              &NotifyFileMovedOnUiThread, account_id_, src, dst)),
          std::move(callback)));
}

}  // namespace chromeos
