// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/empty_trash_io_task.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager::io_task {
namespace {

storage::FileSystemOperationRunner::OperationID
StartRemoveRecursivelyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL url,
    storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Remove(
      url, /*recursive=*/true, std::move(callback));
}

}  // namespace

EmptyTrashIOTask::EmptyTrashIOTask(
    blink::StorageKey storage_key,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath base_path,
    bool show_notification)
    : IOTask(show_notification),
      file_system_context_(file_system_context),
      storage_key_(storage_key),
      profile_(profile),
      base_path_(base_path) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kEmptyTrash;
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;
}

EmptyTrashIOTask::~EmptyTrashIOTask() {
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

void EmptyTrashIOTask::Execute(IOTask::ProgressCallback progress_callback,
                               IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  enabled_trash_locations_ =
      trash::GenerateEnabledTrashLocationsForProfile(profile_, base_path_);
  progress_.state = State::kInProgress;

  trash::TrashPathsMap::const_iterator it = enabled_trash_locations_.cbegin();
  if (it == enabled_trash_locations_.end()) {
    Complete(State::kSuccess);
    return;
  }

  RemoveTrashSubDirectory(it, trash::kFilesFolderName);
}

void EmptyTrashIOTask::RemoveTrashSubDirectory(
    trash::TrashPathsMap::const_iterator& trash_location,
    const std::string& folder_name_to_remove) {
  const base::FilePath& trash_parent_path = trash_location->first;
  const base::FilePath trash_path =
      trash_parent_path.Append(trash_location->second.relative_folder_path);
  const storage::FileSystemURL trash_url =
      file_system_context_->CreateCrackedFileSystemURL(
          storage_key_, storage::FileSystemType::kFileSystemTypeLocal,
          trash_path.Append(folder_name_to_remove));

  progress_.outputs.emplace_back(trash_url, absl::nullopt);

  auto complete_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &EmptyTrashIOTask::OnRemoveTrashSubDirectory,
      weak_ptr_factory_.GetWeakPtr(), base::OwnedRef(trash_location),
      std::move(folder_name_to_remove)));

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartRemoveRecursivelyOnIOThread, file_system_context_,
                     trash_url, std::move(complete_callback)),
      base::BindOnce(&EmptyTrashIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void EmptyTrashIOTask::OnRemoveTrashSubDirectory(
    trash::TrashPathsMap::const_iterator& it,
    const std::string& removed_folder_name,
    base::File::Error status) {
  progress_.outputs[progress_.outputs.size() - 1].error = status;
  if (status != base::File::FILE_OK) {
    LOG(ERROR) << "Failed to remove trash directory " << status;
    Complete(State::kError);
    return;
  }
  if (removed_folder_name == trash::kFilesFolderName) {
    RemoveTrashSubDirectory(it, trash::kInfoFolderName);
    return;
  }
  it++;
  if (it == enabled_trash_locations_.end()) {
    Complete(State::kSuccess);
    return;
  }

  RemoveTrashSubDirectory(it, trash::kFilesFolderName);
}

// Calls the completion callback for the task. `progress_` should not be
// accessed after calling this.
void EmptyTrashIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void EmptyTrashIOTask::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

void EmptyTrashIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

}  // namespace file_manager::io_task
