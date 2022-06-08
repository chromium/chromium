// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_io_task.h"

#include "base/callback.h"
#include "base/files/file_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace io_task {

RestoreIOTask::RestoreIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath base_path)
    : file_system_context_(file_system_context),
      profile_(profile),
      base_path_(base_path) {
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

  progress_.state = State::kInProgress;

  ValidateTrashInfo(0);
}

// Calls the completion callback for the task. `progress_` should not be
// accessed after calling this.
void RestoreIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void RestoreIOTask::ValidateTrashInfo(size_t idx) {
  const base::FilePath& file_path = progress_.sources[idx].url.path();
  if (file_path.FinalExtension() != ".trashinfo") {
    progress_.sources[idx].error = base::File::FILE_ERROR_INVALID_URL;
    Complete(State::kError);
    return;
  }

  // TODO(b/231830250): Add in logic to ensure the trash location is parented at
  // an enabled trash location.

  ParseTrashInfoFile(idx, file_path);
}

void RestoreIOTask::ParseTrashInfoFile(
    size_t idx,
    const base::FilePath& trash_info_file_path) {
  // TODO(b/231830250): Add parsing logic here on a base::MayBlock thread to
  // enable restoration.
  Complete(State::kSuccess);
}

void RestoreIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

}  // namespace io_task
}  // namespace file_manager
