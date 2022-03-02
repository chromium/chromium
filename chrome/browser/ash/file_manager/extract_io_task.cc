// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/extract_io_task.h"

#include "components/services/unzip/content/unzip_service.h"

namespace file_manager {
namespace io_task {

ExtractIOTask::ExtractIOTask(
    std::vector<storage::FileSystemURL> source_urls,
    storage::FileSystemURL parent_folder,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : file_system_context_(file_system_context) {
  progress_.type = OperationType::kExtract;
}

ExtractIOTask::~ExtractIOTask() {}

void ExtractIOTask::Execute(IOTask::ProgressCallback progress_callback,
                            IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  VLOG(1) << "Executing EXTRACT_ARCHIVE IO task";
  zip_file_extractor_ = unzip::LaunchUnzipper();

  Complete(State::kSuccess);
}

void ExtractIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void ExtractIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

}  // namespace io_task
}  // namespace file_manager
