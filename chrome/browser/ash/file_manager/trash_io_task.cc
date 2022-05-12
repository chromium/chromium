// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_io_task.h"

#include "base/callback.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/time/time_to_iso8601.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace file_manager {
namespace io_task {

constexpr char kTrashFolderName[] = ".Trash";

namespace {

const std::string GenerateTrashInfoContents(
    const std::string& relative_restore_path,
    const base::Time& deletion_time) {
  return base::StrCat({"[Trash Info]\nPath=", relative_restore_path,
                       "\nDeletionDate=", base::TimeToISO8601(deletion_time)});
}

TrashEntry::TrashEntry() : deletion_time(base::Time::Now()) {}
TrashEntry::~TrashEntry() = default;

TrashEntry::TrashEntry(TrashEntry&& other) = default;
TrashEntry& TrashEntry::operator=(TrashEntry&& other) = default;

}  // namespace

TrashIOTask::TrashIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : profile_(profile), file_system_context_(file_system_context) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kTrash;
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : file_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
    trash_entries_.emplace_back();
  }
}

TrashIOTask::~TrashIOTask() {
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

void TrashIOTask::Execute(IOTask::ProgressCallback progress_callback,
                          IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  if (!ConstructTrashEntries()) {
    Complete(State::kError);
    return;
  }

  // TODO(b/231250202): Implement verification of free disk space.
  Complete(State::kSuccess);
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void TrashIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

bool TrashIOTask::ConstructTrashEntries() {
  const base::FilePath my_files_path =
      util::GetMyFilesFolderForProfile(profile_);
  const base::FilePath downloads_path =
      util::GetDownloadsFolderForProfile(profile_);

  for (int i = 0; i < progress_.sources.size(); i++) {
    base::FilePath source_path = progress_.sources[i].url.path();
    if (downloads_path.IsParent(source_path) &&
        UpdateTrashEntryAndIncrementRequiredSpace(i, downloads_path)) {
      continue;
    }

    if (my_files_path.IsParent(source_path) &&
        UpdateTrashEntryAndIncrementRequiredSpace(i, my_files_path)) {
      continue;
    }

    // One of the selected files is unable to be trashed.
    return false;
  }

  return true;
}

bool TrashIOTask::UpdateTrashEntryAndIncrementRequiredSpace(
    size_t idx,
    const base::FilePath& trash_parent_path) {
  base::FilePath source_path = progress_.sources[idx].url.path();
  std::string relative_restore_path = source_path.value();
  if (!file_manager::util::ReplacePrefix(&relative_restore_path,
                                         trash_parent_path.value(), "")) {
    return false;
  }

  TrashEntry& entry = trash_entries_[idx];
  entry.trash_path = trash_parent_path.Append(kTrashFolderName);
  entry.trash_info_contents =
      GenerateTrashInfoContents(relative_restore_path, entry.deletion_time);

  size_t trash_contents_size = entry.trash_info_contents.size();
  progress_.total_bytes += trash_contents_size;

  const auto& required_size = required_sizes_.find(trash_parent_path);
  if (required_size != required_sizes_.end()) {
    required_size->second += trash_contents_size;
    return true;
  }

  required_sizes_.emplace(trash_parent_path, trash_contents_size);
  return true;
}

void TrashIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

}  // namespace io_task
}  // namespace file_manager
