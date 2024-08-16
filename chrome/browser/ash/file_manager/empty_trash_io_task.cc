// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/empty_trash_io_task.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager::io_task {

EmptyTrashIOTask::EmptyTrashIOTask(
    blink::StorageKey storage_key,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    base::FilePath base_path,
    bool show_notification)
    : IOTask(show_notification),
      file_system_context_(std::move(file_system_context)),
      storage_key_(std::move(storage_key)),
      profile_(profile),
      base_path_(std::move(base_path)) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kEmptyTrash;
}

EmptyTrashIOTask::~EmptyTrashIOTask() {
  LOG_IF(WARNING, in_flight_ > 0)
      << "An EmptyTrashIOTask is getting deleted although it still has "
      << in_flight_ << " ongoing deletion operations in progress";
}

void EmptyTrashIOTask::Execute(IOTask::ProgressCallback /*progress_callback*/,
                               IOTask::CompleteCallback complete_callback) {
  DCHECK(!complete_callback_);
  complete_callback_ = std::move(complete_callback);

  // A map containing paths which are enabled for trashing.
  const trash::TrashPathsMap locations =
      trash::GenerateEnabledTrashLocationsForProfile(profile_, base_path_);

  if (locations.empty()) {
    progress_.state = State::kSuccess;
    Complete();
    return;
  }

  DCHECK_EQ(in_flight_, 0);
  progress_.state = State::kInProgress;
  for (const trash::TrashPathsMap::value_type& location : locations) {
    base::FilePath dir =
        location.first.Append(location.second.relative_folder_path);

    const EntryStatus& entry = progress_.outputs.emplace_back(
        file_system_context_->CreateCrackedFileSystemURL(
            storage_key_, storage::FileSystemType::kFileSystemTypeLocal, dir),
        std::nullopt);
    ++in_flight_;

    VLOG(1) << "Removing " << entry.url.path();

    // Double-check the path to delete.
    CHECK(dir.IsAbsolute()) << " for " << dir;
    CHECK(!dir.ReferencesParent()) << " for " << dir;
    CHECK(dir.BaseName().value().starts_with(trash::kTrashFolderName))
        << " for " << dir;

    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::DeletePathRecursively, std::move(dir)),
        base::BindOnce(&EmptyTrashIOTask::OnRemoved,
                       weak_ptr_factory_.GetWeakPtr(),
                       progress_.outputs.size() - 1));
  }
}

void EmptyTrashIOTask::OnRemoved(const size_t i, const bool ok) {
  DCHECK_LT(i, progress_.outputs.size());
  if (EntryStatus& entry = progress_.outputs[i]; ok) {
    VLOG(1) << "Removed " << entry.url.path();
    entry.error = base::File::FILE_OK;
  } else {
    LOG(ERROR) << "Cannot remove " << entry.url.path();
    entry.error = base::File::FILE_ERROR_FAILED;
  }

  DCHECK_GT(in_flight_, 0);
  if (--in_flight_ > 0) {
    // Still waiting for some deletion tasks to finish.
    return;
  }

  // All the deletion tasks have finished.
  if (progress_.state != State::kCancelled) {
    // If there was no error, then it is a success.
    progress_.state =
        std::all_of(progress_.outputs.cbegin(), progress_.outputs.cend(),
                    [](const EntryStatus& entry) {
                      return entry.error == base::File::FILE_OK;
                    })
            ? State::kSuccess
            : State::kError;
  }

  LOG_IF(ERROR, progress_.state != State::kSuccess)
      << "Cannot empty the trash bin: " << progress_.state;
  Complete();
}

void EmptyTrashIOTask::Complete() {
  DCHECK(complete_callback_);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void EmptyTrashIOTask::Cancel() {
  LOG_IF(WARNING, progress_.state == State::kInProgress)
      << "Cannot cancel the " << in_flight_
      << " operations that are currently emptying the trash bin";
  progress_.state = State::kCancelled;
}

}  // namespace file_manager::io_task
