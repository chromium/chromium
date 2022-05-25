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

// Obtains metadata of a URL. Used to get the filesize of the transferred files.
void GetFileMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    int fields,
    storage::FileSystemOperation::GetMetadataCallback callback);

// Starts the delete operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartDeleteOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& file_url,
    storage::FileSystemOperation::StatusCallback status_callback);

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_IO_TASK_UTIL_H_
