// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/restore_io_task.h"

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace io_task {

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
    std::vector<std::string> restore_paths,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath base_path)
    : file_system_context_(file_system_context),
      profile_(profile),
      base_path_(base_path),
      restore_paths_(restore_paths) {
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

  // The list of restore paths is passed as metadata to the RestoreIOTask and
  // thus is a 1:1 mapping of storage::FileSystemURL to the final restore path.
  // This path still has to be validated and represents a relative (to the
  // source filesystem) path to move the file to.
  if (restore_paths_.size() != progress_.sources.size()) {
    Complete(State::kError);
    return;
  }

  enabled_trash_locations_ =
      GenerateEnabledTrashLocationsForProfile(profile_, base_path_);
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
  const base::FilePath& trash_info =
      (base_path_.empty())
          ? progress_.sources[idx].url.path()
          : base_path_.Append(progress_.sources[idx].url.path());
  if (trash_info.FinalExtension() != kTrashInfoExtension) {
    progress_.sources[idx].error = base::File::FILE_ERROR_INVALID_URL;
    Complete(State::kError);
    return;
  }

  // Ensures the trash location is parented at an enabled trash location.
  base::FilePath trash_parent_path;
  base::FilePath trash_relative_folder_path;
  for (const auto& [parent_path, info] : enabled_trash_locations_) {
    if (parent_path.Append(info.relative_folder_path).IsParent(trash_info)) {
      trash_parent_path = parent_path;
      trash_relative_folder_path = info.relative_folder_path;
      break;
    }
  }

  if (trash_parent_path.empty() || trash_relative_folder_path.empty()) {
    progress_.sources[idx].error = base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  // Ensure the corresponding file that this metadata file refers to actually
  // exists.
  base::FilePath trashed_file_location =
      trash_parent_path.Append(trash_relative_folder_path)
          .Append(kFilesFolderName)
          .Append(trash_info.BaseName().RemoveFinalExtension());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::PathExists, trashed_file_location),
      base::BindOnce(&RestoreIOTask::OnTrashedFileExists,
                     weak_ptr_factory_.GetWeakPtr(), idx, trash_parent_path,
                     trashed_file_location));
}

void RestoreIOTask::OnTrashedFileExists(
    size_t idx,
    const base::FilePath& trash_parent_path,
    const base::FilePath& trashed_file_location,
    bool exists) {
  if (!exists) {
    progress_.sources[idx].error = base::File::FILE_ERROR_NOT_FOUND;
    Complete(State::kError);
    return;
  }

  // The leading character is "/", which needs to be removed to ensure it can be
  // appended to the parent path.
  // TODO(b/231830250): Remove this once the UI trashing logic has been enabled
  // to conform to having no leading "/" character.
  if (!restore_paths_[idx].empty() &&
      base::StartsWith(restore_paths_[idx], "/",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    restore_paths_[idx].erase(0, 1);
  }

  // The path to restore is expected to be relative to the source filesystem
  // (the previous condition enforces this) and not have any references to it's
  // parent (i.e. no ".." path traversals).
  base::FilePath restore_path(restore_paths_[idx]);
  if (restore_path.empty() || restore_path.IsAbsolute() ||
      restore_path.ReferencesParent()) {
    progress_.sources[idx].error = base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  base::FilePath absolute_restore_path = trash_parent_path.Append(restore_path);
  EnsureParentRestorePathExists(idx, trashed_file_location,
                                absolute_restore_path);
}

void RestoreIOTask::EnsureParentRestorePathExists(
    size_t idx,
    const base::FilePath& trashed_file_location,
    const base::FilePath& absolute_restore_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&CreateNestedPath, absolute_restore_path.DirName()),
      base::BindOnce(&RestoreIOTask::OnParentRestorePathExists,
                     weak_ptr_factory_.GetWeakPtr(), idx, trashed_file_location,
                     absolute_restore_path));
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
  if (destination_result.is_error()) {
    progress_.outputs.emplace_back(source_url, absl::nullopt);
    OnRestoreItem(idx, destination_result.error());
    return;
  }
  progress_.outputs.emplace_back(destination_result.value(), absl::nullopt);

  // File browsers generally default to preserving mtimes on copy/move so we
  // should do the same.
  storage::FileSystemOperation::CopyOrMoveOptionSet options(
      storage::FileSystemOperation::CopyOrMoveOption::kPreserveLastModified);

  auto complete_callback =
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         base::BindOnce(&RestoreIOTask::OnRestoreItem,
                                        weak_ptr_factory_.GetWeakPtr(), idx));

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

  auto complete_callback =
      base::BindPostTask(base::SequencedTaskRunnerHandle::Get(),
                         base::BindOnce(&RestoreIOTask::RestoreComplete,
                                        weak_ptr_factory_.GetWeakPtr(), idx));

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

}  // namespace io_task
}  // namespace file_manager
