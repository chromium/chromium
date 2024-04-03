// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/trash_io_task.h"

#include <sys/xattr.h>

#include "ash/metrics/histogram_macros.h"
#include "base/containers/adapters.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/crostini/crostini_manager.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/trash_common_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"

namespace file_manager::io_task {
namespace {

// Generates and updates the `entry` with the standard contents of the
// individual .trashinfo files which contains the files original path (to
// restore to) and the deletion date.
bool UpdateTrashInfoContents(const base::FilePath& original_path,
                             const base::FilePath& trash_parent_path,
                             const base::FilePath& prefix_restore_path,
                             TrashEntry& entry) {
  std::string relative_restore_path = original_path.value();
  if (!file_manager::util::ReplacePrefix(
          &relative_restore_path,
          trash_parent_path.AsEndingWithSeparator().value(), "")) {
    return false;
  }

  base::FilePath prefix = (prefix_restore_path.IsAbsolute())
                              ? prefix_restore_path
                              : base::FilePath("/").Append(prefix_restore_path);

  entry.trash_info_contents =
      base::StrCat({"[Trash Info]\nPath=",
                    base::EscapePath(prefix.AsEndingWithSeparator().value()),
                    base::EscapePath(relative_restore_path), "\nDeletionDate=",
                    base::TimeFormatAsIso8601(entry.deletion_time), "\n"});
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
    PLOG(ERROR) << "Failed to remove existing metadata file";
    return false;
  }
  return base::WriteFile(destination_path, contents);
}

bool SetTrashDirectoryPermissions(const base::FilePath& trash_directory) {
  return base::SetPosixFilePermissions(
      trash_directory, base::FILE_PERMISSION_READ_BY_USER |
                           base::FILE_PERMISSION_WRITE_BY_USER |
                           base::FILE_PERMISSION_EXECUTE_BY_USER |
                           base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                           base::FILE_PERMISSION_EXECUTE_BY_OTHERS);
}

void RecordDirectorySetupMetric(trash::DirectorySetupUmaType type) {
  UMA_HISTOGRAM_ENUMERATION(trash::kDirectorySetupHistogramName, type);
}

void RecordFailedTrashingMetric(trash::FailedTrashingUmaType type) {
  UMA_HISTOGRAM_ENUMERATION(trash::kFailedTrashingHistogramName, type);
}

base::File::Error SetTrackedExtendedAttribute(const base::FilePath& path) {
  auto tracked_name = base::StrCat({"trash_", path.BaseName().value()});
  if (lsetxattr(path.value().c_str(), trash::kTrackedDirectoryName,
                tracked_name.c_str(), tracked_name.size(), 0) < 0) {
    RecordDirectorySetupMetric(trash::DirectorySetupUmaType::FAILED_XATTR);
    PLOG(WARNING) << "Failed to set the xattr " << trash::kTrackedDirectoryName
                  << "=" << tracked_name << " on " << path;
  }
  return base::File::FILE_OK;
}

TrashEntry::TrashEntry() : deletion_time(base::Time::Now()) {}
TrashEntry::~TrashEntry() = default;

TrashEntry::TrashEntry(TrashEntry&& other) = default;
TrashEntry& TrashEntry::operator=(TrashEntry&& other) = default;

}  // namespace

TrashIOTask::TrashIOTask(
    std::vector<storage::FileSystemURL> file_urls,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const base::FilePath base_path,
    bool show_notification)
    : IOTask(show_notification),
      profile_(profile),
      file_system_context_(file_system_context),
      base_path_(base_path) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kTrash;
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : file_urls) {
    progress_.sources.emplace_back(url, std::nullopt);
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

  if (progress_.sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }

  // Build the list of known paths that are enabled, for now Downloads is a bind
  // mount at MyFiles/Downloads so treat them as separate volumes.
  free_space_map_ =
      trash::GenerateEnabledTrashLocationsForProfile(profile_, base_path_);
  progress_.state = State::kInProgress;

  UpdateTrashEntry(0);
}

// Calls the completion callback for the task. `progress_` should not be
// accessed after calling this.
void TrashIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

void TrashIOTask::UpdateTrashEntry(size_t source_idx) {
  base::FilePath source_path = progress_.sources[source_idx].url.path();

  if (!base_path_.empty() && !source_path.IsAbsolute()) {
    source_path = base_path_.Append(source_path);
  }

  // Use a std::map::reverse_iterator because insertions into a std::map are
  // sorted by key. base::FilePath keys will insert in lexicographical order
  // however in the case of nested directories, reverse lexicographical order is
  // preferred to ensure the closer parent path by depth is chosen.
  const trash::TrashPathsMap::reverse_iterator& trash_parent_path_it =
      base::ranges::find_if(base::Reversed(free_space_map_),
                            [&source_path](const auto& it) {
                              return it.first.IsParent(source_path);
                            });

  if (trash_parent_path_it == free_space_map_.rend()) {
    // The `source_path` is not parented at a supported Trash location, bail
    // out completely.
    progress_.sources[source_idx].error =
        base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  trash::TrashLocation& trash_location = trash_parent_path_it->second;
  const base::FilePath trash_parent_path = trash_parent_path_it->first;
  TrashEntry& entry = trash_entries_[source_idx];
  entry.trash_mount_path = trash_parent_path;
  entry.relative_trash_path = trash_location.relative_folder_path;

  if (!UpdateTrashInfoContents(source_path, trash_parent_path,
                               trash_location.prefix_restore_path, entry)) {
    // If we can't update the trash entry, update the source error and finish
    // with an error.
    progress_.sources[source_idx].error =
        base::File::FILE_ERROR_INVALID_OPERATION;
    Complete(State::kError);
    return;
  }

  if (!trash_location.require_setup) {
    GetFreeDiskSpace(source_idx, trash_parent_path_it);
    return;
  }

  ValidateAndDecrementFreeSpace(source_idx, trash_parent_path_it);
}

void TrashIOTask::ValidateAndDecrementFreeSpace(
    size_t source_idx,
    const trash::TrashPathsMap::reverse_iterator& it) {
  int trash_contents_size =
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
          storage::FileSystemOperation::GetMetadataFieldSet(
              {storage::FileSystemOperation::GetMetadataField::kSize,
               storage::FileSystemOperation::GetMetadataField::kRecursiveSize}),
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

void TrashIOTask::GetFreeDiskSpace(
    size_t source_idx,
    const trash::TrashPathsMap::reverse_iterator& it) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                     it->second.mount_point_path),
      base::BindOnce(&TrashIOTask::GotFreeDiskSpace,
                     weak_ptr_factory_.GetWeakPtr(), source_idx,
                     base::OwnedRef(it)));
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

base::FilePath TrashIOTask::MakeRelativePathAbsoluteFromBasePath(
    const base::FilePath& relative_path) {
  if (base_path_.empty() || base_path_.IsParent(relative_path) ||
      relative_path.IsAbsolute()) {
    return relative_path;
  }
  return base_path_.Append(relative_path);
}

void TrashIOTask::GotFreeDiskSpace(
    size_t source_idx,
    const trash::TrashPathsMap::reverse_iterator& it,
    int64_t free_space) {
  trash::TrashLocation& trash_location = it->second;
  const base::FilePath& trash_parent_path = it->first;
  base::FilePath trash_path = MakeRelativeFromBasePath(
      trash_parent_path.Append(trash_location.relative_folder_path));
  trash_location.trash_files =
      CreateFileSystemURL(progress_.sources[source_idx].url,
                          trash_path.Append(trash::kFilesFolderName));
  trash_location.trash_info =
      CreateFileSystemURL(progress_.sources[source_idx].url,
                          trash_path.Append(trash::kInfoFolderName));
  trash_location.free_space = free_space;
  trash_location.require_setup = true;

  ValidateAndDecrementFreeSpace(source_idx, it);
}

void TrashIOTask::SetupSubDirectory(
    trash::TrashPathsMap::const_iterator& it,
    const storage::FileSystemURL trash_subdirectory) {
  // All enabled trash directories exist in the `free_space_map_` however some
  // may not be used for this IO task. Skip the ones that don't require setup.
  if (!it->second.require_setup) {
    it++;
    if (it == free_space_map_.end()) {
      GenerateDestinationURL(/*source_idx=*/0, /*output_idx=*/0);
      return;
    }
    SetupSubDirectory(it, it->second.trash_files);
    return;
  }

  auto on_setup_complete_callback = base::BindOnce(
      &TrashIOTask::OnSetupSubDirectory, weak_ptr_factory_.GetWeakPtr(),
      base::OwnedRef(it), trash_subdirectory);

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartCreateDirectoryOnIOThread, file_system_context_,
                     trash_subdirectory,
                     base::BindPostTaskToCurrentDefault(
                         base::BindOnce(&TrashIOTask::SetDirectoryTracking,
                                        weak_ptr_factory_.GetWeakPtr(),
                                        std::move(on_setup_complete_callback),
                                        MakeRelativePathAbsoluteFromBasePath(
                                            trash_subdirectory.path())),
                         FROM_HERE)),
      base::BindOnce(&TrashIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrashIOTask::SetDirectoryTracking(
    base::OnceCallback<void(base::File::Error)> on_setup_complete_callback,
    const base::FilePath& trash_subdirectory,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    std::move(on_setup_complete_callback).Run(error);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SetTrackedExtendedAttribute,
                     std::move(trash_subdirectory)),
      std::move(on_setup_complete_callback));
}

void TrashIOTask::OnSetupSubDirectory(
    trash::TrashPathsMap::const_iterator& it,
    const storage::FileSystemURL trash_subdirectory,
    base::File::Error error) {
  if (error != base::File::FILE_OK) {
    auto failed_directory_uma_type =
        (trash_subdirectory == it->second.trash_files)
            ? trash::DirectorySetupUmaType::FAILED_FILES_FOLDER
            : trash::DirectorySetupUmaType::FAILED_INFO_FOLDER;
    RecordDirectorySetupMetric(failed_directory_uma_type);
    LOG(ERROR) << "Failed setting up a trash subfolder: "
               << static_cast<int>(failed_directory_uma_type);
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

  // We have to ensure the permission bits are appropriately setup to allow
  // system daemons access to traverse the folder. By default the permissions
  // are setup as 0700 when they should be 0711.
  auto absolute_trash_path =
      MakeRelativePathAbsoluteFromBasePath(trash_subdirectory.path().DirName());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(&SetTrashDirectoryPermissions,
                     std::move(absolute_trash_path)),
      base::BindOnce(&TrashIOTask::OnSetDirectoryPermissions,
                     weak_ptr_factory_.GetWeakPtr(), base::OwnedRef(it)));
}

void TrashIOTask::OnSetDirectoryPermissions(
    trash::TrashPathsMap::const_iterator& it,
    bool set_permissions_success) {
  if (!set_permissions_success) {
    RecordDirectorySetupMetric(
        trash::DirectorySetupUmaType::FAILED_PARENT_FOLDER_PERMISSIONS);
    LOG(ERROR) << "Failed setting directory permissions";
    Complete(State::kError);
    return;
  }

  it++;
  // If we've have no more trash directory to setup, start trashing files.
  if (it == free_space_map_.end()) {
    GenerateDestinationURL(/*source_idx=*/0, /*output_idx=*/0);
    return;
  }

  SetupSubDirectory(it, it->second.trash_files);
}

void TrashIOTask::GenerateDestinationURL(size_t source_idx, size_t output_idx) {
  DCHECK(source_idx < progress_.sources.size());
  DCHECK(source_idx < trash_entries_.size());

  const TrashEntry& entry = trash_entries_[source_idx];
  const auto trash_path = MakeRelativeFromBasePath(
      entry.trash_mount_path.Append(entry.relative_trash_path)
          .Append(trash::kFilesFolderName));

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
  if (!destination_result.has_value()) {
    progress_.outputs.emplace_back(files_folder_location, std::nullopt);
    TrashComplete(source_idx, output_idx, destination_result.error());
    return;
  }
  const base::FilePath absolute_trash_path =
      trash_entries_[source_idx].trash_mount_path.Append(
          trash_entries_[source_idx].relative_trash_path);
  const std::string file_name =
      destination_result.value().path().BaseName().value();

  const base::FilePath destination_path = trash::GenerateTrashPath(
      absolute_trash_path, trash::kInfoFolderName, file_name);
  progress_.outputs.emplace_back(
      CreateFileSystemURL(progress_.sources[source_idx].url, destination_path),
      std::nullopt);

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
    RecordFailedTrashingMetric(
        trash::FailedTrashingUmaType::FAILED_WRITING_METADATA);
    TrashComplete(source_idx, output_idx, base::File::FILE_ERROR_FAILED);
    return;
  }

  last_metadata_url_ = progress_.outputs[output_idx].url;
  progress_.outputs[output_idx].error = base::File::FILE_OK;
  TrashFile(source_idx, output_idx, destination_url);
}

void TrashIOTask::TrashFile(size_t source_idx,
                            size_t output_idx,
                            const storage::FileSystemURL& destination_url) {
  DCHECK(source_idx < progress_.sources.size());
  DCHECK(output_idx < progress_.outputs.size());
  progress_.outputs.emplace_back(destination_url, std::nullopt);

  last_progress_size_ = 0;

  const storage::FileSystemURL& source_url = progress_.sources[source_idx].url;

  // File browsers generally default to preserving mtimes on copy/move so we
  // should do the same.
  storage::FileSystemOperation::CopyOrMoveOptionSet options = {
      storage::FileSystemOperation::CopyOrMoveOption::kPreserveLastModified};

  auto complete_callback = base::BindPostTaskToCurrentDefault(base::BindOnce(
      &TrashIOTask::OnMoveComplete, weak_ptr_factory_.GetWeakPtr(), source_idx,
      output_idx + 1));

  // For move operations that occur on the same file system, the progress
  // callback is never invoked.
  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartMoveFileLocalOnIOThread, file_system_context_,
                     source_url, destination_url, options,
                     std::move(complete_callback)),
      base::BindOnce(&TrashIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void TrashIOTask::OnMoveComplete(size_t source_idx,
                                 size_t output_idx,
                                 base::File::Error error) {
  DCHECK(source_idx < progress_.sources.size());
  DCHECK(output_idx < progress_.outputs.size());
  if (error != base::File::FILE_OK) {
    LOG(ERROR) << "Failed to move the file to trash folder: " << error;
    RecordFailedTrashingMetric(
        trash::FailedTrashingUmaType::FAILED_MOVING_FILE);
    auto complete_callback = base::BindPostTaskToCurrentDefault(
        base::BindOnce(&TrashIOTask::TrashComplete,
                       weak_ptr_factory_.GetWeakPtr(), source_idx, output_idx));

    content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&StartDeleteOnIOThread, file_system_context_,
                       last_metadata_url_, std::move(complete_callback)),
        base::BindOnce(&TrashIOTask::SetCurrentOperationID,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  TrashComplete(source_idx, output_idx, error);
}

void TrashIOTask::TrashComplete(size_t source_idx,
                                size_t output_idx,
                                base::File::Error error) {
  DCHECK(source_idx < progress_.sources.size());
  DCHECK(output_idx < progress_.outputs.size());
  operation_id_.reset();
  progress_.sources[source_idx].error = error;
  progress_.outputs[output_idx].error = error;
  progress_.bytes_transferred +=
      trash_entries_[source_idx].trash_info_contents.size() +
      (trash_entries_[source_idx].source_file_size - last_progress_size_);

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

}  // namespace file_manager::io_task
