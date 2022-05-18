// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_io_task.h"

#include "base/callback.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time_to_iso8601.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

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

DirectoryInfo::DirectoryInfo(int64_t supplied_free_space)
    : free_space(supplied_free_space) {}
DirectoryInfo::~DirectoryInfo() = default;

DirectoryInfo::DirectoryInfo(DirectoryInfo&& other) = default;
DirectoryInfo& DirectoryInfo::operator=(DirectoryInfo&& other) = default;

}  // namespace

TrashIOTask::TrashIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath base_path)
    : profile_(profile),
      file_system_context_(file_system_context),
      base_path_(base_path) {
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

  progress_.state = State::kInProgress;

  UpdateTrashEntry(0);
}

// Calls the completion callback for the task. `progress_` should not be
// accessed after calling this.
void TrashIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void TrashIOTask::UpdateTrashEntry(size_t idx) {
  base::FilePath source_path = progress_.sources[idx].url.path();

  if (!base_path_.empty() && !source_path.IsAbsolute()) {
    source_path = base_path_.Append(source_path);
  }

  const base::FilePath my_files_path =
      util::GetMyFilesFolderForProfile(profile_);
  const base::FilePath downloads_path =
      util::GetDownloadsFolderForProfile(profile_);

  base::FilePath trash_parent_path;
  if (downloads_path.IsParent(source_path)) {
    trash_parent_path = downloads_path;
  } else if (my_files_path.IsParent(source_path)) {
    trash_parent_path = my_files_path;
  } else {
    // The `source_path` is not parented at a supported Trash location, bail
    // out completely.
    // TODO(b/231830211): This may be better handled more gracefully by
    // continuing with the remaining files to see if others can be trashed.
    progress_.sources[idx].error = base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  std::string relative_restore_path = source_path.value();
  if (!file_manager::util::ReplacePrefix(&relative_restore_path,
                                         trash_parent_path.value(), "")) {
    // If we can't update the trash entry, update the source error and finish
    // with an error.
    progress_.sources[idx].error = base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  TrashEntry& entry = trash_entries_[idx];
  entry.trash_path = trash_parent_path.Append(kTrashFolderName);
  entry.trash_info_contents =
      GenerateTrashInfoContents(relative_restore_path, entry.deletion_time);

  auto it = free_space_map_.find(trash_parent_path);
  if (it == free_space_map_.end()) {
    GetFreeDiskSpace(idx, trash_parent_path);
    return;
  }

  ValidateAndDecrementFreeSpace(idx, it);
}

void TrashIOTask::ValidateAndDecrementFreeSpace(size_t idx,
                                                FreeSpaceMap::iterator& it) {
  size_t trash_contents_size = trash_entries_[idx].trash_info_contents.size();
  progress_.total_bytes += trash_contents_size;

  if (trash_contents_size > it->second.free_space) {
    // TODO(b/231830211): We probably don't have to bail out here, we can check
    // if an error is set on `progress_.sources` before trashing. This will
    // enable trashes with mixed sources (some no space, some with space) to
    // finish.
    progress_.sources[idx].error = base::File::FILE_ERROR_NO_SPACE;
    Complete(State::kError);
    return;
  }

  it->second.free_space -= trash_contents_size;
  GetFileSize(idx);
}

// Computes the total size of all source files and stores it in
// `progress_.total_bytes`.
void TrashIOTask::GetFileSize(size_t idx) {
  DCHECK(idx < progress_.sources.size());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetFileMetadataOnIOThread, file_system_context_,
          progress_.sources[idx].url,
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
              storage::FileSystemOperation::GET_METADATA_FIELD_TOTAL_SIZE,
          google_apis::CreateRelayCallback(
              base::BindOnce(&TrashIOTask::GotFileSize,
                             weak_ptr_factory_.GetWeakPtr(), idx))));
}

// Helper function to GetFileSize() that is called when the metadata for a file
// is retrieved.
void TrashIOTask::GotFileSize(size_t idx,
                              base::File::Error error,
                              const base::File::Info& file_info) {
  DCHECK(idx < progress_.sources.size());
  if (error != base::File::FILE_OK) {
    progress_.sources[idx].error = error;
    Complete(State::kError);
    return;
  }

  progress_.total_bytes += file_info.size;
  trash_entries_[idx].source_file_size = file_info.size;

  if (idx < progress_.sources.size() - 1) {
    UpdateTrashEntry(idx + 1);
    return;
  }

  // TODO(b/231830211): Update to ensure the trash directory is properly
  // initialised.
  Complete(State::kSuccess);
}

void TrashIOTask::GetFreeDiskSpace(size_t idx,
                                   const base::FilePath& trash_parent_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, trash_parent_path),
      base::BindOnce(&TrashIOTask::GotFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr(), idx, trash_parent_path));
}

void TrashIOTask::GotFreeDiskSpace(size_t idx,
                                   const base::FilePath& trash_parent_path,
                                   int64_t free_space) {
  auto it =
      free_space_map_.try_emplace(trash_parent_path, DirectoryInfo(free_space));
  ValidateAndDecrementFreeSpace(idx, it.first);
}

void TrashIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

}  // namespace io_task
}  // namespace file_manager
