// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

namespace file_manager {
namespace io_task {

namespace {

// Starts the copy operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    const FileManagerCopyOrMoveHookDelegate::ProgressCallback&
        progress_callback,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  // TODO(crbug.com/1312336): Replace FileManagerCopyOrMoveHookDelegate with new
  // class.
  return file_system_context->operation_runner()->Copy(
      source_url, destination_url, options,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<FileManagerCopyOrMoveHookDelegate>(progress_callback),
      std::move(complete_callback));
}

// Starts the move operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartMoveOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    const FileManagerCopyOrMoveHookDelegate::ProgressCallback&
        progress_callback,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Move(
      source_url, destination_url, options,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      std::make_unique<FileManagerCopyOrMoveHookDelegate>(progress_callback),
      std::move(complete_callback));
}

}  // namespace

CopyOrMoveIOTask::CopyOrMoveIOTask(
    OperationType type,
    std::vector<storage::FileSystemURL> source_urls,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : profile_(profile), file_system_context_(file_system_context) {
  DCHECK(type == OperationType::kCopy || type == OperationType::kMove);
  progress_.state = State::kQueued;
  progress_.type = type;
  progress_.destination_folder = std::move(destination_folder);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : source_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
  }

  source_sizes_.reserve(source_urls.size());
}

CopyOrMoveIOTask::~CopyOrMoveIOTask() {
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

bool CopyOrMoveIOTask::IsCrossFileSystemForTesting(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  return IsCrossFileSystem(source_url, destination_url);
}

// Helper function for copy or move tasks that determines whether or not
// entries identified by their URLs should be considered as being on the
// different file systems or not. The entries are seen as being on different
// filesystems if either:
// - the entries are not on the same volume OR
// - one entry is in My files, and the other one in Downloads.
// crbug.com/1200251
bool CopyOrMoveIOTask::IsCrossFileSystem(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile_);

  base::WeakPtr<file_manager::Volume> source_volume =
      volume_manager->FindVolumeFromPath(source_url.path());
  base::WeakPtr<file_manager::Volume> destination_volume =
      volume_manager->FindVolumeFromPath(destination_url.path());

  if (!(source_volume && destination_volume)) {
    // When either volume is unavailable, fallback to only checking the
    // filesystem_id, which uniquely maps a URL to its ExternalMountPoints
    // instance. NOTE: different volumes (e.g. for removables), might share the
    // same ExternalMountPoints. NOTE 2: if either volume is unavailable, the
    // operation itself is likely to fail.
    return source_url.filesystem_id() != destination_url.filesystem_id();
  }

  if (source_volume->volume_id() != destination_volume->volume_id()) {
    return true;
  }

  // On volumes other than DOWNLOADS, I/O operations within volumes that have
  // the same ID are considered same-filesystem.
  if (source_volume->type() != file_manager::VOLUME_TYPE_DOWNLOADS_DIRECTORY) {
    return false;
  }

  // The Downloads folder being bind mounted in My files, I/O operations within
  // My files may need to be considered cross-filesystem (if one path is in
  // Downloads and the other is not).
  base::FilePath my_files_path =
      file_manager::util::GetMyFilesFolderForProfile(profile_);
  base::FilePath downloads_path = my_files_path.Append("Downloads");

  bool source_in_downloads = downloads_path.IsParent(source_url.path());
  // The destination_url can be the destination folder, so Downloads is a valid
  // destination.
  bool destination_in_downloads =
      downloads_path == destination_url.path() ||
      downloads_path.IsParent(destination_url.path());
  return source_in_downloads != destination_in_downloads;
}

void CopyOrMoveIOTask::Execute(IOTask::ProgressCallback progress_callback,
                               IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  if (progress_.sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }
  progress_.state = State::kInProgress;

  GetFileSize(0);
}

void CopyOrMoveIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void CopyOrMoveIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

// Computes the total size of all source files and stores it in
// |progress_.total_bytes|.
void CopyOrMoveIOTask::GetFileSize(size_t idx) {
  DCHECK(idx < progress_.sources.size());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetFileMetadataOnIOThread, file_system_context_,
          progress_.sources[idx].url,
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
              storage::FileSystemOperation::GET_METADATA_FIELD_TOTAL_SIZE,
          google_apis::CreateRelayCallback(
              base::BindOnce(&CopyOrMoveIOTask::GotFileSize,
                             weak_ptr_factory_.GetWeakPtr(), idx))));
}

// Helper function to GetFileSize() that is called when the metadata for a file
// is retrieved.
void CopyOrMoveIOTask::GotFileSize(size_t idx,
                                   base::File::Error error,
                                   const base::File::Info& file_info) {
  DCHECK(idx < progress_.sources.size());
  if (error != base::File::FILE_OK) {
    progress_.sources[idx].error = error;
    Complete(State::kError);
    return;
  }

  progress_.total_bytes += file_info.size;
  source_sizes_.push_back(file_info.size);
  if (idx < progress_.sources.size() - 1) {
    GetFileSize(idx + 1);
  } else {
    speedometer_.SetTotalBytes(progress_.total_bytes);
    if (util::IsNonNativeFileSystemType(progress_.destination_folder.type())) {
      // Destination is a virtual filesystem, so skip checking free space.
      GenerateDestinationURL(0);
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                         progress_.destination_folder.path()),
          base::BindOnce(&CopyOrMoveIOTask::GotFreeDiskSpace,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

// Ensures that there is enough free space on the destination volume.
void CopyOrMoveIOTask::GotFreeDiskSpace(int64_t free_space) {
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (progress_.destination_folder.filesystem_id() ==
          util::GetDownloadsMountPointName(profile_) ||
      (drive_integration_service &&
       drive_integration_service->GetMountPointPath().IsParent(
           progress_.destination_folder.path()))) {
    free_space -= cryptohome::kMinFreeSpaceInBytes;
  }

  int64_t required_bytes = progress_.total_bytes;

  // Move operations that are same-filesystem do not require disk space.
  if (progress_.type == OperationType::kMove) {
    for (size_t i = 0; i < source_sizes_.size(); i++) {
      if (!IsCrossFileSystem(progress_.sources[i].url,
                             progress_.destination_folder)) {
        required_bytes -= source_sizes_[i];
      }
    }
  }

  if (required_bytes > free_space) {
    progress_.outputs.emplace_back(progress_.destination_folder,
                                   base::File::FILE_ERROR_NO_SPACE);
    Complete(State::kError);
    return;
  }

  GenerateDestinationURL(0);
}

// Tries to find an unused filename in the destination folder for a specific
// entry being transferred.
void CopyOrMoveIOTask::GenerateDestinationURL(size_t idx) {
  DCHECK(idx < progress_.sources.size());
  util::GenerateUnusedFilename(
      progress_.destination_folder,
      progress_.sources[idx].url.path().BaseName(), file_system_context_,
      base::BindOnce(&CopyOrMoveIOTask::CopyOrMoveFile,
                     weak_ptr_factory_.GetWeakPtr(), idx));
}

// Starts the underlying copy or move operation.
void CopyOrMoveIOTask::CopyOrMoveFile(
    size_t idx,
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  DCHECK(idx < progress_.sources.size());
  if (destination_result.is_error()) {
    progress_.outputs.emplace_back(progress_.destination_folder, absl::nullopt);
    OnCopyOrMoveComplete(idx, destination_result.error());
    return;
  }
  progress_.outputs.emplace_back(destination_result.value(), absl::nullopt);

  last_progress_size_ = 0;

  const storage::FileSystemURL& source_url = progress_.sources[idx].url;
  const storage::FileSystemURL& destination_url = destination_result.value();

  // File browsers generally default to preserving mtimes on copy/move so we
  // should do the same.
  storage::FileSystemOperation::CopyOrMoveOptionSet options =
      storage::FileSystemOperation::CopyOrMoveOptionSet(
          storage::FileSystemOperation::CopyOrMoveOption::kPreserveLastModified,
          storage::FileSystemOperation::CopyOrMoveOption::
              kRemovePartiallyCopiedFilesOnError);
  // To ensure progress updates, force cross-filesystem I/O operations when the
  // source and the destination are on different volumes, or between My files
  // and Downloads.
  if (IsCrossFileSystem(source_url, destination_url)) {
    options.Put(
        storage::FileSystemOperation::CopyOrMoveOption::kForceCrossFilesystem);
  }

  auto* transfer_function = progress_.type == OperationType::kCopy
                                ? &StartCopyOnIOThread
                                : &StartMoveOnIOThread;

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(transfer_function, file_system_context_, source_url,
                     destination_url, options,
                     google_apis::CreateRelayCallback(base::BindRepeating(
                         &CopyOrMoveIOTask::OnCopyOrMoveProgress,
                         weak_ptr_factory_.GetWeakPtr())),
                     google_apis::CreateRelayCallback(
                         base::BindOnce(&CopyOrMoveIOTask::OnCopyOrMoveComplete,
                                        weak_ptr_factory_.GetWeakPtr(), idx))),
      base::BindOnce(&CopyOrMoveIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CopyOrMoveIOTask::OnCopyOrMoveProgress(
    FileManagerCopyOrMoveHookDelegate::ProgressType type,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    int64_t size) {
  // |size| is only valid for kProgress.
  if (type != FileManagerCopyOrMoveHookDelegate::ProgressType::kProgress)
    return;

  progress_.bytes_transferred += size - last_progress_size_;
  speedometer_.Update(progress_.bytes_transferred);
  const double remaining_seconds = speedometer_.GetRemainingSeconds();

  // Speedometer can produce infinite result which can't be serialized to JSON
  // when sending the status via private API.
  if (std::isfinite(remaining_seconds)) {
    progress_.remaining_seconds = remaining_seconds;
  }

  last_progress_size_ = size;
  progress_callback_.Run(progress_);
}

void CopyOrMoveIOTask::OnCopyOrMoveComplete(size_t idx,
                                            base::File::Error error) {
  DCHECK(idx < progress_.sources.size());
  DCHECK(idx < progress_.outputs.size());
  operation_id_.reset();
  progress_.sources[idx].error = error;
  progress_.outputs[idx].error = error;
  progress_.bytes_transferred += source_sizes_[idx] - last_progress_size_;

  if (idx < progress_.sources.size() - 1) {
    progress_callback_.Run(progress_);
    GenerateDestinationURL(idx + 1);
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

void CopyOrMoveIOTask::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

}  // namespace io_task
}  // namespace file_manager
