// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_io_task.h"

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager::io_task {

namespace {

base::File::Error CreateNestedPath(
    const base::FilePath& absolute_restore_path) {
  if (base::PathExists(absolute_restore_path)) {
    return base::File::FILE_OK;
  }
  base::File::Error status;
  if (!base::CreateDirectoryAndGetError(absolute_restore_path, &status)) {
    return status;
  }
  return base::File::FILE_OK;
}

}  // namespace

RestoreIOTask::RestoreIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath base_path,
    bool show_notification)
    : IOTask(show_notification),
      file_system_context_(file_system_context),
      profile_(profile),
      base_path_(base_path) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kRestore;
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : file_urls) {
    progress_.sources.emplace_back(url, std::nullopt);
  }

  if (file_urls.size() > 0) {
    base::FilePath source_path =
        util::GetDisplayablePath(profile_, file_urls.front())
            .value_or(base::FilePath())
            .BaseName();

    if (source_path.MatchesFinalExtension(trash::kTrashInfoExtension)) {
      source_path = source_path.RemoveFinalExtension();
    }

    progress_.source_name = source_path.value();
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

  if (progress_.sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }

  progress_.state = State::kInProgress;
  validator_ =
      std::make_unique<trash::TrashInfoValidator>(profile_, base_path_);
  validator_->SetDisconnectHandler(base::BindOnce(
      &RestoreIOTask::Complete, weak_ptr_factory_.GetWeakPtr(), State::kError));

  ValidateTrashInfo(0);
}

// Calls the completion callback for the task. `progress_` should not be
// accessed after calling this. If the `trash_service_` is disconnected, it will
// end up here so avoid accessing `trash_service_` here.
void RestoreIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void RestoreIOTask::ValidateTrashInfo(size_t idx) {
  const base::FilePath& trash_info =
      (base_path_.empty())
          ? progress_.sources[idx].url.path()
          : base_path_.Append(progress_.sources[idx].url.path());

  auto on_parsed_callback =
      base::BindOnce(&RestoreIOTask::EnsureParentRestorePathExists,
                     weak_ptr_factory_.GetWeakPtr(), idx);

  validator_->ValidateAndParseTrashInfo(std::move(trash_info),
                                        std::move(on_parsed_callback));
}

void RestoreIOTask::EnsureParentRestorePathExists(
    size_t idx,
    trash::ParsedTrashInfoDataOrError parsed_data_or_error) {
  if (!parsed_data_or_error.has_value()) {
    progress_.sources[idx].error =
        trash::ValidationErrorToFileError(parsed_data_or_error.error());
    Complete(State::kError);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          &CreateNestedPath,
          parsed_data_or_error.value().absolute_restore_path.DirName()),
      base::BindOnce(&RestoreIOTask::OnParentRestorePathExists,
                     weak_ptr_factory_.GetWeakPtr(), idx,
                     parsed_data_or_error.value().trashed_file_path,
                     parsed_data_or_error.value().absolute_restore_path));
}

void RestoreIOTask::OnParentRestorePathExists(
    size_t idx,
    const base::FilePath& trashed_file_location,
    const base::FilePath& absolute_restore_path,
    base::File::Error status) {
  if (status != base::File::FILE_OK) {
    progress_.sources[idx].error = status;
    Complete(State::kError);
    return;
  }

  GenerateDestinationURL(idx, trashed_file_location, absolute_restore_path);
}

void RestoreIOTask::GenerateDestinationURL(
    size_t idx,
    const base::FilePath& trashed_file_location,
    const base::FilePath& absolute_restore_path) {
  const storage::FileSystemURL item_folder_location = CreateFileSystemURL(
      progress_.sources[idx].url,
      MakeRelativeFromBasePath(absolute_restore_path.DirName()));
  util::GenerateUnusedFilename(item_folder_location,
                               absolute_restore_path.BaseName(),
                               file_system_context_,
                               base::BindOnce(&RestoreIOTask::RestoreItem,
                                              weak_ptr_factory_.GetWeakPtr(),
                                              idx, trashed_file_location));
}

void RestoreIOTask::RestoreItem(
    size_t idx,
    base::FilePath trashed_file_location,
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  storage::FileSystemURL source_url =
      CreateFileSystemURL(progress_.sources[idx].url,
                          MakeRelativeFromBasePath(trashed_file_location));
  if (!destination_result.has_value()) {
    progress_.outputs.emplace_back(source_url, std::nullopt);
    OnRestoreItem(idx, destination_result.error());
    return;
  }
  progress_.outputs.emplace_back(destination_result.value(), std::nullopt);

  // File browsers generally default to preserving mtimes on copy/move so we
  // should do the same.
  storage::FileSystemOperation::CopyOrMoveOptionSet options = {
      storage::FileSystemOperation::CopyOrMoveOption::kPreserveLastModified};

  auto complete_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &RestoreIOTask::OnRestoreItem, weak_ptr_factory_.GetWeakPtr(), idx));

  // For move operations that occur on the same file system, the progress
  // callback is never invoked.
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartMoveFileLocalOnIOThread, file_system_context_,
                     source_url, destination_result.value(), options,
                     std::move(complete_callback)),
      base::BindOnce(&RestoreIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RestoreIOTask::OnRestoreItem(size_t idx, base::File::Error error) {
  if (error != base::File::FILE_OK) {
    RestoreComplete(idx, error);
    return;
  }

  auto complete_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &RestoreIOTask::RestoreComplete, weak_ptr_factory_.GetWeakPtr(), idx));

  // On successful file restore, there is a dangling trashinfo file, remove this
  // before restoration is considered complete.
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartDeleteOnIOThread, file_system_context_,
                     progress_.sources[idx].url, std::move(complete_callback)),
      base::BindOnce(&RestoreIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RestoreIOTask::RestoreComplete(size_t idx, base::File::Error error) {
  DCHECK(idx < progress_.sources.size());
  DCHECK(idx < progress_.outputs.size());
  operation_id_.reset();
  progress_.sources[idx].error = error;
  progress_.outputs[idx].error = error;

  if (idx < progress_.sources.size() - 1) {
    progress_callback_.Run(progress_);
    ValidateTrashInfo(idx + 1);
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

const storage::FileSystemURL RestoreIOTask::CreateFileSystemURL(
    const storage::FileSystemURL& original_url,
    const base::FilePath& path) {
  return file_system_context_->CreateCrackedFileSystemURL(
      original_url.storage_key(), original_url.type(), path);
}

base::FilePath RestoreIOTask::MakeRelativeFromBasePath(
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

void RestoreIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

void RestoreIOTask::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

}  // namespace file_manager::io_task
