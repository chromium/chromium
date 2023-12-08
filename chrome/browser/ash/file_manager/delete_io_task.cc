// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/delete_io_task.h"

#include <memory>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"

namespace file_manager {
namespace io_task {

DeleteIOTask::DeleteIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : IOTask(show_notification), file_system_context_(file_system_context) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kDelete;
  progress_.bytes_transferred = 0;
  progress_.total_bytes = file_urls.size();

  for (const auto& url : file_urls) {
    progress_.sources.emplace_back(url, std::nullopt);
  }
}

DeleteIOTask::~DeleteIOTask() {
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

void DeleteIOTask::Execute(IOTask::ProgressCallback progress_callback,
                           IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  if (progress_.sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }
  progress_.state = State::kInProgress;
  DeleteFile(0);
}

void DeleteIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void DeleteIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void DeleteIOTask::DeleteFile(size_t idx) {
  DCHECK(idx < progress_.sources.size());
  const storage::FileSystemURL file_url = progress_.sources[idx].url;
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartDeleteOnIOThread, file_system_context_, file_url,
                     google_apis::CreateRelayCallback(
                         base::BindOnce(&DeleteIOTask::OnDeleteComplete,
                                        weak_ptr_factory_.GetWeakPtr(), idx))),
      base::BindOnce(&DeleteIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void DeleteIOTask::OnDeleteComplete(size_t idx, base::File::Error error) {
  DCHECK(idx < progress_.sources.size());
  operation_id_.reset();
  progress_.sources[idx].error = error;
  progress_.bytes_transferred += 1;

  if (idx < progress_.sources.size() - 1) {
    progress_callback_.Run(progress_);
    DeleteFile(idx + 1);
  } else {
    for (const auto& source : progress_.sources) {
      if (source.error != base::File::FILE_OK) {
        Complete(State::kError);
        return;
      }
    }
    Complete(State::kSuccess);
  }
}

void DeleteIOTask::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

}  // namespace io_task
}  // namespace file_manager
