// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_UTIL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_UTIL_H_

#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager {
namespace io_task {

// Performs a move via the FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartMoveOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    const FileManagerCopyOrMoveHookDelegate::ProgressCallback&
        progress_callback,
    storage::FileSystemOperation::StatusCallback complete_callback);

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_UTIL_H_
