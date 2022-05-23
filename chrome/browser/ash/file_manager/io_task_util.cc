// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/io_task_util.h"

#include <memory>

#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace io_task {

storage::FileSystemOperationRunner::OperationID StartMoveOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    const FileManagerCopyOrMoveHookDelegate::ProgressCallback&
        progress_callback,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Move(
      source_url, destination_url, options,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<FileManagerCopyOrMoveHookDelegate>(progress_callback),
      std::move(complete_callback));
}

void GetFileMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    int fields,
    storage::FileSystemOperation::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(url, fields,
                                                       std::move(callback));
}

storage::FileSystemOperationRunner::OperationID StartDeleteOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& file_url,
    storage::FileSystemOperation::StatusCallback status_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Remove(
      file_url, /*recursive=*/true, std::move(status_callback));
}

}  // namespace io_task
}  // namespace file_manager
