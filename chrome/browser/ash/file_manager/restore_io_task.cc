// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_io_task.h"

#include "base/callback.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace io_task {

RestoreIOTask::RestoreIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : file_system_context_(file_system_context) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kRestore;
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : file_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
  }
}

RestoreIOTask::~RestoreIOTask() {
  if (operation_id_) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<storage::FileSystemContext> file_system_context,
               storage::FileSystemOperationRunner::OperationID operation_id) {
              file_system_context->operation_runner()->Cancel(
                  operation_id, base::DoNothing());
            },
            file_system_context_, *operation_id_));
  }
}

void RestoreIOTask::Execute(IOTask::ProgressCallback progress_callback,
                            IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);
}

void RestoreIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

}  // namespace io_task
}  // namespace file_manager
