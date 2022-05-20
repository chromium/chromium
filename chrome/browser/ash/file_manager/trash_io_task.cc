// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_io_task.h"

#include "base/callback.h"
#include "base/files/file_util.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/time/time_to_iso8601.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

namespace file_manager {
namespace io_task {

constexpr char kTrashFolderName[] = ".Trash";
constexpr char kInfoFolderName[] = "info";
constexpr char kFilesFolderName[] = "files";

namespace {

// Generates and updates the `entry` with the standard contents of the
// individual .trashinfo files which contains the files original path (to
// restore to) and the deletion date.
bool UpdateTrashInfoContents(const base::FilePath& original_path,
                             const base::FilePath& trash_parent_path,
                             TrashEntry& entry) {
  std::string relative_restore_path = original_path.value();
  if (!file_manager::util::ReplacePrefix(
          &relative_restore_path,
          trash_parent_path.AsEndingWithSeparator().value(), "")) {
    return false;
  }

  entry.trash_info_contents = base::StrCat(
      {"[Trash Info]\nPath=", relative_restore_path,
       "\nDeletionDate=", base::TimeToISO8601(entry.deletion_time)});
  return true;
}

storage::FileSystemOperationRunner::OperationID StartCreateDirectoryOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL url,
    storage::FileSystemOperationRunner::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->CreateDirectory(
      url, /*exclusive=*/false, /*recursive=*/true, std::move(callback));
}

bool WriteMetadataFileOnBlockingThread(const base::FilePath& destination_path,
                                       const std::string& contents) {
  // If the metadata file already exists either a previous copy failed or
  // the file has been tampered with to overwrite. Try to delete the file before
  // proceeding. `DeleteFile` will succeed if the file does not exist.
  if (!base::DeleteFile(destination_path)) {
    LOG(ERROR) << "Failed to remove existing metadata file";
    return false;
  }
  return base::WriteFile(destination_path, contents);
}

TrashEntry::TrashEntry() : deletion_time(base::Time::Now()) {}
TrashEntry::~TrashEntry() = default;

TrashEntry::TrashEntry(TrashEntry&& other) = default;
TrashEntry& TrashEntry::operator=(TrashEntry&& other) = default;

DirectoryInfo::DirectoryInfo(storage::FileSystemURL supplied_trash_files,
                             storage::FileSystemURL supplied_trash_info,
                             int64_t supplied_free_space)
    : trash_files(supplied_trash_files),
      trash_info(supplied_trash_info),
      free_space(supplied_free_space) {}
DirectoryInfo::~DirectoryInfo() = default;

DirectoryInfo::DirectoryInfo(DirectoryInfo&& other) = default;
DirectoryInfo& DirectoryInfo::operator=(DirectoryInfo&& other) = default;

}  // namespace

const base::FilePath GenerateTrashPath(const base::FilePath& trash_path,
                                       const std::string& subdir,
                                       const std::string& file_name) {
  base::FilePath path = trash_path.Append(subdir).Append(file_name);
  // The metadata file in .Trash/info always has the .trashinfo extension.
  if (subdir == kInfoFolderName) {
    path = path.AddExtension(".trashinfo");
  }
  return path;
}

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

void TrashIOTask::UpdateTrashEntry(size_t source_idx) {
  base::FilePath source_path = progress_.sources[source_idx].url.path();

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
    progress_.sources[source_idx].error =
        base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  TrashEntry& entry = trash_entries_[source_idx];
  entry.trash_path = trash_parent_path.Append(kTrashFolderName);

  if (!UpdateTrashInfoContents(source_path, trash_parent_path, entry)) {
    // If we can't update the trash entry, update the source error and finish
    // with an error.
    progress_.sources[source_idx].error =
        base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  auto it = free_space_map_.find(trash_parent_path);
  if (it == free_space_map_.end()) {
    GetFreeDiskSpace(source_idx, trash_parent_path);
    return;
  }

  ValidateAndDecrementFreeSpace(source_idx, it);
}

void TrashIOTask::ValidateAndDecrementFreeSpace(size_t source_idx,
                                                FreeSpaceMap::iterator& it) {
  size_t trash_contents_size =
      trash_entries_[source_idx].trash_info_contents.size();
  progress_.total_bytes += trash_contents_size;

  if (trash_contents_size > it->second.free_space) {
    // TODO(b/231830211): We probably don't have to bail out here, we can check
    // if an error is set on `progress_.sources` before trashing. This will
    // enable trashes with mixed sources (some no space, some with space) to
    // finish.
    progress_.sources[source_idx].error = base::File::FILE_ERROR_NO_SPACE;
    Complete(State::kError);
    return;
  }

  it->second.free_space -= trash_contents_size;
  GetFileSize(source_idx);
}

// Computes the total size of all source files and stores it in
// `progress_.total_bytes`.
void TrashIOTask::GetFileSize(size_t source_idx) {
  DCHECK(source_idx < progress_.sources.size());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetFileMetadataOnIOThread, file_system_context_,
          progress_.sources[source_idx].url,
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
              storage::FileSystemOperation::GET_METADATA_FIELD_TOTAL_SIZE,
          google_apis::CreateRelayCallback(
              base::BindOnce(&TrashIOTask::GotFileSize,
                             weak_ptr_factory_.GetWeakPtr(), source_idx))));
}

// Helper function to GetFileSize() that is called when the metadata for a file
// is retrieved.
void TrashIOTask::GotFileSize(size_t source_idx,
                              base::File::Error error,
                              const base::File::Info& file_info) {
  DCHECK(source_idx < progress_.sources.size());
  if (error != base::File::FILE_OK) {
    progress_.sources[source_idx].error = error;
    Complete(State::kError);
    return;
  }

  progress_.total_bytes += file_info.size;
  trash_entries_[source_idx].source_file_size = file_info.size;

  if (source_idx < progress_.sources.size() - 1) {
    UpdateTrashEntry(source_idx + 1);
    return;
  }

  auto it = free_space_map_.cbegin();
  SetupSubDirectory(it, it->second.trash_files);
}

void TrashIOTask::GetFreeDiskSpace(size_t source_idx,
                                   const base::FilePath& trash_parent_path) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, trash_parent_path),
      base::BindOnce(&TrashIOTask::GotFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr(), source_idx,
                     trash_parent_path));
}

base::FilePath TrashIOTask::MakeRelativeFromBasePath(
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

void TrashIOTask::GotFreeDiskSpace(size_t source_idx,
                                   const base::FilePath& trash_parent_path,
                                   int64_t free_space) {
  base::FilePath trash_path =
      MakeRelativeFromBasePath(trash_parent_path.Append(kTrashFolderName));
  const storage::FileSystemURL files_url = CreateFileSystemURL(
      progress_.sources[source_idx].url, trash_path.Append(kFilesFolderName));
  const storage::FileSystemURL info_url = CreateFileSystemURL(
      progress_.sources[source_idx].url, trash_path.Append(kInfoFolderName));

  auto it = free_space_map_.try_emplace(
      trash_parent_path, DirectoryInfo(files_url, info_url, free_space));
  ValidateAndDecrementFreeSpace(source_idx, it.first);
}

void TrashIOTask::SetupSubDirectory(
    FreeSpaceMap::const_iterator& it,
    const storage::FileSystemURL trash_subdirectory) {
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartCreateDirectoryOnIOThread, file_system_context_,
                     trash_subdirectory,
                     base::BindOnce(&TrashIOTask::OnSetupSubDirectory,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    base::OwnedRef(it), trash_subdirectory)),
      base::BindOnce(&TrashIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrashIOTask::OnSetupSubDirectory(
    FreeSpaceMap::const_iterator& it,
    const storage::FileSystemURL trash_subdirectory,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    // TODO(b/231830211): We can potentially continue if one .Trash directory
    // fails to create, but we should also rollback if the files directory
    // succeeds but info fails.
    Complete(State::kError);
    return;
  }

  // Make sure to setup the .Trash/info directory after the .Trash/files
  // directory.
  if (trash_subdirectory == it->second.trash_files) {
    SetupSubDirectory(it, it->second.trash_info);
    return;
  }

  it++;
  if (it == free_space_map_.end()) {
    GenerateDestinationURL(/*source_idx=*/0, /*output_idx=*/0);
    return;
  }

  SetupSubDirectory(it, it->second.trash_files);
}

void TrashIOTask::GenerateDestinationURL(size_t source_idx, size_t output_idx) {
  DCHECK(source_idx < progress_.sources.size());
  DCHECK(source_idx < trash_entries_.size());

  const auto trash_path = MakeRelativeFromBasePath(
      trash_entries_[source_idx].trash_path.Append(kFilesFolderName));

  const storage::FileSystemURL files_location =
      CreateFileSystemURL(progress_.sources[source_idx].url, trash_path);
  util::GenerateUnusedFilename(
      files_location, progress_.sources[source_idx].url.path().BaseName(),
      file_system_context_,
      base::BindOnce(&TrashIOTask::WriteMetadata,
                     weak_ptr_factory_.GetWeakPtr(), source_idx, output_idx,
                     files_location));
}

void TrashIOTask::WriteMetadata(
    size_t source_idx,
    size_t output_idx,
    const storage::FileSystemURL& files_folder_location,
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  if (destination_result.is_error()) {
    progress_.outputs.emplace_back(files_folder_location, absl::nullopt);
    TrashComplete(source_idx, output_idx, destination_result.error());
    return;
  }
  const base::FilePath destination_path =
      GenerateTrashPath(trash_entries_[source_idx].trash_path, kInfoFolderName,
                        destination_result.value().path().BaseName().value());
  progress_.outputs.emplace_back(
      CreateFileSystemURL(progress_.sources[source_idx].url, destination_path),
      absl::nullopt);

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&WriteMetadataFileOnBlockingThread, destination_path,
                     trash_entries_[source_idx].trash_info_contents),
      base::BindOnce(&TrashIOTask::OnWriteMetadata,
                     weak_ptr_factory_.GetWeakPtr(), source_idx, output_idx,
                     destination_result.value()));
}

void TrashIOTask::OnWriteMetadata(size_t source_idx,
                                  size_t output_idx,
                                  const storage::FileSystemURL& destination_url,
                                  bool success) {
  if (!success) {
    TrashComplete(source_idx, output_idx, base::File::FILE_ERROR_FAILED);
    return;
  }

  // TODO(b/231250202): Move the file to trash but on error remove the metadata
  // file.
  TrashComplete(source_idx, output_idx, base::File::Error::FILE_OK);
}

void TrashIOTask::TrashComplete(size_t source_idx,
                                size_t output_idx,
                                base::File::Error error) {
  DCHECK(source_idx < progress_.sources.size());
  DCHECK(source_idx < trash_entries_.size());
  DCHECK(output_idx < progress_.outputs.size());
  operation_id_.reset();
  progress_.sources[source_idx].error = error;
  progress_.outputs[output_idx].error = error;
  progress_.bytes_transferred +=
      trash_entries_[source_idx].trash_info_contents.size();

  if (source_idx < progress_.sources.size() - 1) {
    progress_callback_.Run(progress_);
    GenerateDestinationURL(source_idx + 1, output_idx + 1);
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

const storage::FileSystemURL TrashIOTask::CreateFileSystemURL(
    const storage::FileSystemURL& original_url,
    const base::FilePath& path) {
  return file_system_context_->CreateCrackedFileSystemURL(
      original_url.storage_key(), original_url.type(), path);
}

void TrashIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

void TrashIOTask::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

}  // namespace io_task
}  // namespace file_manager
