// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file_path.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_manager::io_task {

CopyOrMoveIOTask::CopyOrMoveIOTask(
    OperationType type,
    std::vector<storage::FileSystemURL> source_urls,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : IOTask(show_notification),
      profile_(profile),
      file_system_context_(file_system_context) {
  DCHECK(type == OperationType::kCopy || type == OperationType::kMove);
  progress_.state = State::kQueued;
  progress_.type = type;
  progress_.destination_folder = std::move(destination_folder);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : source_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
  }

  source_urls_ = std::move(source_urls);
}

CopyOrMoveIOTask::CopyOrMoveIOTask(
    OperationType type,
    std::vector<storage::FileSystemURL> source_urls,
    std::vector<base::FilePath> destination_file_names,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : CopyOrMoveIOTask(type,
                       source_urls,
                       std::move(destination_folder),
                       profile,
                       file_system_context,
                       show_notification) {
  DCHECK_EQ(source_urls.size(), destination_file_names.size());
  destination_file_names_ = std::move(destination_file_names);
}

CopyOrMoveIOTask::~CopyOrMoveIOTask() = default;

void CopyOrMoveIOTask::Execute(IOTask::ProgressCallback progress_callback,
                               IOTask::CompleteCallback complete_callback) {
  // TODO (crbug.com/1339818): check whether scanning is needed and create
  // appropriate impl_.
  impl_ = std::make_unique<CopyOrMoveIOTaskImpl>(
      progress_.type, progress_, std::move(destination_file_names_),
      progress_.destination_folder, profile_, file_system_context_,
      progress_.show_notification);

  impl_->Execute(std::move(progress_callback), std::move(complete_callback));
}

void CopyOrMoveIOTask::Cancel() {
  progress_.state = State::kCancelled;
  if (impl_) {
    impl_->Cancel();
  }
}

}  // namespace file_manager::io_task
