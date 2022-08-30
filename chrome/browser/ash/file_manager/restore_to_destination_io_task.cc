// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_to_destination_io_task.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_manager::io_task {

RestoreToDestinationIOTask::RestoreToDestinationIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath base_path,
    bool show_notification)
    : IOTask(show_notification),
      file_system_context_(file_system_context),
      profile_(profile),
      base_path_(base_path) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kRestoreToDestination;
  progress_.destination_folder = std::move(destination_folder);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : file_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
  }
}

RestoreToDestinationIOTask::~RestoreToDestinationIOTask() = default;

void RestoreToDestinationIOTask::Execute(
    IOTask::ProgressCallback progress_callback,
    IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  if (progress_.sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }

  progress_.state = State::kInProgress;
  validator_ =
      std::make_unique<trash::TrashInfoValidator>(profile_, base_path_);
  validator_->SetDisconnectHandler(
      base::BindOnce(&RestoreToDestinationIOTask::Complete,
                     weak_ptr_factory_.GetWeakPtr(), State::kError));

  ValidateTrashInfo(0);
}

// Calls the completion callback for the task. `progress_` should not be
// accessed after calling this. If the `trash_service_` is disconnected, it will
// end up here so avoid accessing `trash_service_` here.
void RestoreToDestinationIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void RestoreToDestinationIOTask::ValidateTrashInfo(size_t idx) {
  const base::FilePath& trash_info =
      (base_path_.empty())
          ? progress_.sources[idx].url.path()
          : base_path_.Append(progress_.sources[idx].url.path());

  auto on_parsed_callback =
      base::BindOnce(&RestoreToDestinationIOTask::OnTrashInfoParsed,
                     weak_ptr_factory_.GetWeakPtr(), idx);

  validator_->ValidateAndParseTrashInfo(std::move(trash_info),
                                        std::move(on_parsed_callback));
}

void RestoreToDestinationIOTask::OnTrashInfoParsed(
    size_t idx,
    base::FileErrorOr<trash::ParsedTrashInfoData> parsed_data) {
  if (parsed_data.is_error()) {
    progress_.sources[idx].error = parsed_data.error();
    Complete(State::kError);
    return;
  }

  destination_file_names_.emplace_back(
      parsed_data.value().absolute_restore_path.BaseName());
  source_urls_.push_back(file_system_context_->CreateCrackedFileSystemURL(
      progress_.sources[idx].url.storage_key(),
      progress_.sources[idx].url.type(),
      MakeRelativeFromBasePath(parsed_data.value().trashed_file_path)));

  if (progress_.sources.size() == (idx + 1)) {
    // Make sure to reset the TrashInfoValidator as it is not required anymore.
    validator_.reset();

    // The `RestoreToDestination` task is composed of the `CopyOrMoveIOTask`.
    // All data is passed to the task, but a pointer is maintained to ensure the
    // parent task is tied to the life of the child task.
    move_io_task_ = std::make_unique<CopyOrMoveIOTask>(
        OperationType::kMove, std::move(source_urls_),
        std::move(destination_file_names_), progress_.destination_folder,
        profile_, file_system_context_);
    move_io_task_->Execute(std::move(progress_callback_),
                           std::move(complete_callback_));
    return;
  }

  ValidateTrashInfo(idx + 1);
}

base::FilePath RestoreToDestinationIOTask::MakeRelativeFromBasePath(
    const base::FilePath& absolute_path) {
  if (base_path_.empty() || !base_path_.IsParent(absolute_path)) {
    return absolute_path;
  }
  std::string relative_path = absolute_path.value();
  if (!file_manager::util::ReplacePrefix(
          &relative_path, base_path_.AsEndingWithSeparator().value(), "")) {
    LOG(ERROR) << "Failed to make absolute path relative";
    return absolute_path;
  }
  return base::FilePath(relative_path);
}

void RestoreToDestinationIOTask::Cancel() {
  progress_.state = State::kCancelled;
  if (move_io_task_) {
    // Delegate Cancel to the underlying `move_io_task_` if it has been started.
    move_io_task_->Cancel();
  }
}

}  // namespace file_manager::io_task
