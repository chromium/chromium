// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_to_destination_io_task.h"

#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_info_validator.h"

namespace file_manager::io_task {

namespace {

base::FilePath RemapInfoPathToFilePath(const base::FilePath& trash_file_path) {
  if (trash_file_path.DirName().BaseName().value() != trash::kFilesFolderName ||
      trash_file_path.DirName().DirName().empty()) {
    LOG(ERROR) << "Folder path doesn't contain files parent folder";
    return trash_file_path;
  }
  const auto trash_folder_path = trash_file_path.DirName().DirName();
  return trash_folder_path.Append(trash::kInfoFolderName)
      .Append(
          trash_file_path.BaseName().AddExtension(trash::kTrashInfoExtension));
}

}  // namespace

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
  progress_.SetDestinationFolder(std::move(destination_folder), profile);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : file_urls) {
    progress_.sources.emplace_back(url, std::nullopt);
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
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void RestoreToDestinationIOTask::ValidateTrashInfo(size_t idx) {
  base::FilePath trash_info =
      (base_path_.empty())
          ? progress_.sources[idx].url.path()
          : base_path_.Append(progress_.sources[idx].url.path());

  // Supplied URLs can be of items in the .Trash/files or .Trash/info
  // directory, if in the former the URL gets rewritten to the latter to
  // ensure input is appropriate for validation.
  if (trash_info.FinalExtension() != trash::kTrashInfoExtension) {
    trash_info = RemapInfoPathToFilePath(trash_info);
  }

  auto on_parsed_callback =
      base::BindOnce(&RestoreToDestinationIOTask::OnTrashInfoParsed,
                     weak_ptr_factory_.GetWeakPtr(), idx);

  validator_->ValidateAndParseTrashInfo(std::move(trash_info),
                                        std::move(on_parsed_callback));
}

void RestoreToDestinationIOTask::OnTrashInfoParsed(
    size_t idx,
    trash::ParsedTrashInfoDataOrError parsed_data_or_error) {
  if (!parsed_data_or_error.has_value()) {
    progress_.sources[idx].error =
        trash::ValidationErrorToFileError(parsed_data_or_error.error());
    Complete(State::kError);
    return;
  }

  destination_file_names_.emplace_back(
      parsed_data_or_error.value().absolute_restore_path.BaseName());
  source_urls_.push_back(file_system_context_->CreateCrackedFileSystemURL(
      progress_.sources[idx].url.storage_key(),
      progress_.sources[idx].url.type(),
      MakeRelativeFromBasePath(
          parsed_data_or_error.value().trashed_file_path)));

  if (progress_.sources.size() == (idx + 1)) {
    // Make sure to reset the TrashInfoValidator as it is not required anymore.
    validator_.reset();

    // The `RestoreToDestination` task is composed of the `CopyOrMoveIOTask`.
    // All data is passed to the task, but a pointer is maintained to ensure the
    // parent task is tied to the life of the child task.
    move_io_task_ = std::make_unique<CopyOrMoveIOTask>(
        OperationType::kMove, std::move(source_urls_),
        std::move(destination_file_names_), progress_.GetDestinationFolder(),
        profile_, file_system_context_);
    // Set the same ID so that anything trying to pause/resume/cancel the move
    // task would pause/resume/cancel `this`, which will pass it on to the move
    // task.
    move_io_task_->SetTaskID(progress_.task_id);
    // The existing callbacks need to be intercepted to ensure the IOTask
    // progress that is propagated is sent from the `RestoreToDestinationIOTask`
    // instead of the underlying `CopyOrMoveIOTask`.
    auto progress_callback =
        base::BindRepeating(&RestoreToDestinationIOTask::OnProgressCallback,
                            weak_ptr_factory_.GetWeakPtr());
    auto complete_callback =
        base::BindOnce(&RestoreToDestinationIOTask::OnCompleteCallback,
                       weak_ptr_factory_.GetWeakPtr());

    move_io_task_->Execute(std::move(progress_callback),
                           std::move(complete_callback));
    return;
  }

  ValidateTrashInfo(idx + 1);
}

void RestoreToDestinationIOTask::OnProgressCallback(
    const ProgressStatus& status) {
  progress_.state = status.state;

  // The underlying CopyOrMoveIOTask can enter state::PAUSED to resolve file
  // name conflicts. Copy its status.pause_params to our |progress_| to send
  // those pause_params to the files app UI.
  progress_.pause_params = {};
  if (progress_.state == State::kPaused) {
    progress_.pause_params = status.pause_params;
  }

  progress_.bytes_transferred = status.bytes_transferred;
  progress_.total_bytes = status.total_bytes;
  progress_.remaining_seconds = status.remaining_seconds;

  for (size_t i = 0; i < status.outputs.size(); ++i) {
    if (i < progress_.outputs.size() && i < status.outputs.size()) {
      if (progress_.outputs[i].url == status.outputs[i].url &&
          progress_.outputs[i].error == status.outputs[i].error) {
        continue;
      }
    }
    progress_.outputs.emplace_back(status.outputs[i].url,
                                   status.outputs[i].error);
  }

  progress_callback_.Run(progress_);
}

void RestoreToDestinationIOTask::OnCompleteCallback(ProgressStatus status) {
  const auto task_id = progress_.task_id;
  progress_ = std::move(status);
  progress_.task_id = task_id;
  progress_.type = OperationType::kRestoreToDestination;
  Complete(progress_.state);
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

void RestoreToDestinationIOTask::Pause(PauseParams params) {
  if (move_io_task_) {
    // Delegate Pause to the underlying `move_io_task_`.
    move_io_task_->Pause(std::move(params));
  }
}

void RestoreToDestinationIOTask::Resume(ResumeParams params) {
  if (move_io_task_) {
    // Delegate Resume to the underlying `move_io_task_`.
    move_io_task_->Resume(std::move(params));
  }
}

void RestoreToDestinationIOTask::Cancel() {
  progress_.state = State::kCancelled;
  if (move_io_task_) {
    // Delegate Cancel to the underlying `move_io_task_`.
    move_io_task_->Cancel();
  }
}

CopyOrMoveIOTask* RestoreToDestinationIOTask::GetMoveTaskForTesting() {
  if (move_io_task_) {
    return move_io_task_.get();
  }
  return nullptr;
}

}  // namespace file_manager::io_task
