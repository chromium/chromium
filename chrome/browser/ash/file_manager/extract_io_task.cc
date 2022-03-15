// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/extract_io_task.h"

#include "chrome/browser/chromeos/fileapi/file_system_backend.h"
#include "components/services/unzip/content/unzip_service.h"

namespace file_manager {
namespace io_task {

ExtractIOTask::ExtractIOTask(
    std::vector<storage::FileSystemURL> source_urls,
    storage::FileSystemURL parent_folder,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : source_urls_(std::move(source_urls)),
      parent_folder_(std::move(parent_folder)),
      file_system_context_(std::move(file_system_context)) {
  progress_.type = OperationType::kExtract;
  progress_.state = State::kQueued;
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;
  // Store all the ZIP files in the selection so we have
  // a proper count of how many need to be extracted.
  for (const storage::FileSystemURL& source_url : source_urls_) {
    const base::FilePath source_path = source_url.path();
    if (source_path.MatchesExtension(".zip") &&
        chromeos::FileSystemBackend::CanHandleURL(source_url)) {
      progress_.sources.emplace_back(source_url, absl::nullopt);
    }
  }
  extractCount_ = progress_.sources.size();
}

ExtractIOTask::~ExtractIOTask() {}

void ExtractIOTask::ZipExtractCallback(bool success) {
  progress_.state = success ? State::kSuccess : State::kError;
  DCHECK_GT(extractCount_, 0);
  if (--extractCount_ == 0) {
    Complete();
  }
}

void ExtractIOTask::Execute(IOTask::ProgressCallback progress_callback,
                            IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  VLOG(1) << "Executing EXTRACT_ARCHIVE IO task";
  progress_.state = State::kInProgress;
  progress_callback_.Run(progress_);
  for (const EntryStatus& source : progress_.sources) {
    const base::FilePath source_file = source.url.path();
    // TODO(crbug.com/953256) Perform this check only once.
    if (chromeos::FileSystemBackend::CanHandleURL(parent_folder_)) {
      const base::FilePath destination_directory = parent_folder_.path();
      unzip::Unzip(unzip::LaunchUnzipper(), source_file, destination_directory,
                   base::BindOnce(&ExtractIOTask::ZipExtractCallback,
                                  weak_ptr_factory_.GetWeakPtr()));
    } else {
      progress_.state = State::kError;
      // We won't get a callback so reduce the count and maybe finalise.
      DCHECK_GT(extractCount_, 0);
      if (--extractCount_ == 0) {
        Complete();
      }
    }
  }
}

void ExtractIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void ExtractIOTask::Complete() {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

}  // namespace io_task
}  // namespace file_manager
