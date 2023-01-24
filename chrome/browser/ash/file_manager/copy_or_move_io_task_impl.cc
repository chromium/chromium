// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
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
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

namespace file_manager::io_task {

namespace {

// Starts the copy operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    storage::FileSystemOperation::ErrorBehavior error_behavior,
    std::unique_ptr<storage::CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Copy(
      source_url, destination_url, options, error_behavior,
      std::move(copy_or_move_hook_delegate), std::move(complete_callback));
}

// Starts the move operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartMoveOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    storage::FileSystemOperation::ErrorBehavior error_behavior,
    std::unique_ptr<storage::CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Move(
      source_url, destination_url, options, error_behavior,
      std::move(copy_or_move_hook_delegate), std::move(complete_callback));
}

// Helper function for copy or move tasks that determines whether or not
// entries identified by their URLs should be considered as being on the
// different file systems or not. The entries are seen as being on different
// filesystems if either:
// - the entries are not on the same volume OR
// - one entry is in My files, and the other one in Downloads.
// crbug.com/1200251
bool IsCrossFileSystem(Profile* profile,
                       const storage::FileSystemURL& source_url,
                       const storage::FileSystemURL& destination_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);

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
      file_manager::util::GetMyFilesFolderForProfile(profile);
  base::FilePath downloads_path = my_files_path.Append("Downloads");

  bool source_in_downloads = downloads_path.IsParent(source_url.path());
  // The destination_url can be the destination folder, so Downloads is a valid
  // destination.
  bool destination_in_downloads =
      downloads_path == destination_url.path() ||
      downloads_path.IsParent(destination_url.path());
  return source_in_downloads != destination_in_downloads;
}

}  // namespace

ItemProgress::ItemProgress() = default;
ItemProgress::~ItemProgress() = default;

CopyOrMoveIOTaskImpl::CopyOrMoveIOTaskImpl(
    OperationType type,
    ProgressStatus& progress,
    std::vector<base::FilePath> destination_file_names,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : progress_(progress),
      profile_(profile),
      file_system_context_(file_system_context),
      source_sizes_(progress_.sources.size()),
      item_progresses(progress_.sources.size()) {
  DCHECK(type == OperationType::kCopy || type == OperationType::kMove);
  if (!destination_file_names.empty()) {
    DCHECK_EQ(progress_.sources.size(), destination_file_names.size());
  }
  destination_file_names_ = std::move(destination_file_names);
}

CopyOrMoveIOTaskImpl::~CopyOrMoveIOTaskImpl() {
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

// static
bool CopyOrMoveIOTaskImpl::IsCrossFileSystemForTesting(
    Profile* profile,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  return IsCrossFileSystem(profile, source_url, destination_url);
}

void CopyOrMoveIOTaskImpl::Execute(IOTask::ProgressCallback progress_callback,
                                   IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  if (progress_.sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }

  VerifyTransfer();
}

void CopyOrMoveIOTaskImpl::VerifyTransfer() {
  // No checks, just start the transfer.
  StartTransfer();
}

void CopyOrMoveIOTaskImpl::StartTransfer() {
  progress_.state = State::kInProgress;

  // Start the transfer by getting the file size.
  for (size_t i = 0; i < progress_.sources.size(); i++) {
    GetFileSize(i);
  }
}

void CopyOrMoveIOTaskImpl::Cancel() {
  progress_.state = State::kCancelled;
  // Any in-flight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void CopyOrMoveIOTaskImpl::Complete(State state) {
  completed_ = true;
  progress_.state = state;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

// Computes the total size of all source files and stores it in
// |progress_.total_bytes|.
void CopyOrMoveIOTaskImpl::GetFileSize(size_t idx) {
  DCHECK(idx < progress_.sources.size());

  const base::FilePath& source = progress_.sources[idx].url.path();
  const base::FilePath& destination = progress_.destination_folder.path();

  constexpr auto metadata_fields =
      storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
      storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
      storage::FileSystemOperation::GET_METADATA_FIELD_TOTAL_SIZE;

  auto get_metadata_callback =
      base::BindOnce(&GetFileMetadataOnIOThread, file_system_context_,
                     progress_.sources[idx].url, metadata_fields,
                     google_apis::CreateRelayCallback(
                         base::BindOnce(&CopyOrMoveIOTaskImpl::GotFileSize,
                                        weak_ptr_factory_.GetWeakPtr(), idx)));

  if (file_manager::util::IsDriveLocalPath(profile_, source) &&
      file_manager::file_tasks::IsOfficeFile(source) &&
      !file_manager::util::IsDriveLocalPath(profile_, destination)) {
    if (progress_.type == OperationType::kCopy) {
      UMA_HISTOGRAM_ENUMERATION(
          file_manager::file_tasks::kUseOutsideDriveMetricName,
          file_manager::file_tasks::OfficeFilesUseOutsideDriveHook::COPY);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          file_manager::file_tasks::kUseOutsideDriveMetricName,
          file_manager::file_tasks::OfficeFilesUseOutsideDriveHook::MOVE);
    }
    auto* drive_service = drive::util::GetIntegrationServiceByProfile(profile_);
    if (drive_service) {
      drive_service->ForceReSyncFile(
          source,
          base::BindPostTask(content::GetIOThreadTaskRunner({}),
                             std::move(get_metadata_callback), FROM_HERE));
      return;
    }
    // If there is no Drive connection, we should continue as normal.
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(get_metadata_callback));
}

// Helper function to GetFileSize() that is called when the metadata for a file
// is retrieved.
void CopyOrMoveIOTaskImpl::GotFileSize(size_t idx,
                                       base::File::Error error,
                                       const base::File::Info& file_info) {
  if (completed_) {
    // If Complete() has been called (e.g. due to an error), |progress_| is no
    // longer valid, so return immediately.
    return;
  }

  DCHECK(idx < progress_.sources.size());
  if (error != base::File::FILE_OK) {
    progress_.sources[idx].error = error;
    LOG(ERROR) << "Could not get size of source file: error " << error << " "
               << base::File::ErrorToString(error);
    Complete(State::kError);
    return;
  }

  progress_.total_bytes += file_info.size;
  source_sizes_[idx] = file_info.size;
  progress_.sources[idx].is_directory = file_info.is_directory;

  // Return early if we didn't yet get the file size for all files.
  DCHECK_LT(files_preprocessed_, progress_.sources.size());
  if (++files_preprocessed_ < progress_.sources.size()) {
    return;
  }

  // Got file size for all files at this point!
  speedometer_.SetTotalBytes(progress_.total_bytes);

  if (util::IsNonNativeFileSystemType(progress_.destination_folder.type())) {
    // Destination is a virtual filesystem, so skip checking free space.
    GenerateDestinationURL(0);
  } else {
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                       progress_.destination_folder.path()),
        base::BindOnce(&CopyOrMoveIOTaskImpl::GotFreeDiskSpace,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

// Ensures that there is enough free space on the destination volume.
void CopyOrMoveIOTaskImpl::GotFreeDiskSpace(int64_t free_space) {
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
      if (!IsCrossFileSystem(profile_, progress_.sources[i].url,
                             progress_.destination_folder)) {
        required_bytes -= source_sizes_[i];
      }
    }
  }

  if (required_bytes > free_space) {
    progress_.outputs.emplace_back(progress_.destination_folder,
                                   base::File::FILE_ERROR_NO_SPACE);
    LOG(ERROR) << "Insufficient free space in destination";
    Complete(State::kError);
    return;
  }

  GenerateDestinationURL(0);
}

// Tries to find an unused filename in the destination folder for a specific
// entry being transferred.
void CopyOrMoveIOTaskImpl::GenerateDestinationURL(size_t idx) {
  DCHECK(idx < progress_.sources.size());

  // In the event no `destination_file_names_` exist, fall back to the
  // `BaseName` from the source URL.
  const auto destination_file_name =
      (destination_file_names_.size() == progress_.sources.size())
          ? destination_file_names_[idx]
          : progress_.sources[idx].url.path().BaseName();

  util::GenerateUnusedFilename(
      progress_.destination_folder, destination_file_name, file_system_context_,
      base::BindOnce(&CopyOrMoveIOTaskImpl::CopyOrMoveFile,
                     weak_ptr_factory_.GetWeakPtr(), idx));
}

// Starts the underlying copy or move operation.
void CopyOrMoveIOTaskImpl::CopyOrMoveFile(
    size_t idx,
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  DCHECK(idx < progress_.sources.size());

  if (!destination_result.has_value()) {
    progress_.outputs.emplace_back(progress_.destination_folder, absl::nullopt);
    OnCopyOrMoveComplete(idx, destination_result.error());
    return;
  }

  progress_.outputs.emplace_back(destination_result.value(), absl::nullopt);
  DCHECK_EQ(idx + 1, progress_.outputs.size());

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
  if (IsCrossFileSystem(profile_, source_url, destination_url)) {
    options.Put(
        storage::FileSystemOperation::CopyOrMoveOption::kForceCrossFilesystem);
  }

  auto* transfer_function = progress_.type == OperationType::kCopy
                                ? &StartCopyOnIOThread
                                : &StartMoveOnIOThread;

  // Using CreateRelayCallback to ensure that the callbacks are executed on the
  // current thread.
  auto complete_callback = google_apis::CreateRelayCallback(
      base::BindOnce(&CopyOrMoveIOTaskImpl::OnCopyOrMoveComplete,
                     weak_ptr_factory_.GetWeakPtr(), idx));

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(transfer_function, file_system_context_, source_url,
                     destination_url, options, GetErrorBehavior(),
                     GetHookDelegate(idx), std::move(complete_callback)),
      base::BindOnce(&CopyOrMoveIOTaskImpl::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

storage::FileSystemOperation::ErrorBehavior
CopyOrMoveIOTaskImpl::GetErrorBehavior() {
  return storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT;
}

std::unique_ptr<storage::CopyOrMoveHookDelegate>
CopyOrMoveIOTaskImpl::GetHookDelegate(size_t idx) {
  // Using CreateRelayCallback to ensure that the callbacks are executed on the
  // current thread.
  auto progress_callback = google_apis::CreateRelayCallback(
      base::BindRepeating(&CopyOrMoveIOTaskImpl::OnCopyOrMoveProgress,
                          weak_ptr_factory_.GetWeakPtr(), idx));
  return std::make_unique<FileManagerCopyOrMoveHookDelegate>(progress_callback);
}

void CopyOrMoveIOTaskImpl::OnCopyOrMoveProgress(
    size_t idx,
    FileManagerCopyOrMoveHookDelegate::ProgressType type,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    int64_t size) {
  const std::string destination_path = destination_url.path().AsUTF8Unsafe();
  auto& [individual_progress, aggregate_progress] = item_progresses[idx];

  const auto log_progress = [&]() {
    VLOG(1) << type << "\ncopy_move_src" << source_url.path()
            << "\ncopy_move_des " << destination_url.path();
  };

  using ProgressType = FileManagerCopyOrMoveHookDelegate::ProgressType;
  if (type != ProgressType::kProgress) {
    switch (type) {
      case ProgressType::kBegin:
        log_progress();
        individual_progress[destination_path] = 0;
        return;
      case ProgressType::kEndCopy:
        log_progress();
        individual_progress.erase(destination_path);
        return;
      case ProgressType::kEndMove:
        log_progress();
        individual_progress.erase(destination_path);
        return;
      case ProgressType::kEndRemoveSource:
        log_progress();
        return;
      case ProgressType::kError:
        log_progress();
        return;
      default:
        NOTREACHED() << "Unknown ProgressType:" << int(type);
        return;
    }
  }

  // The |size| is only valid for ProgressType::kProgress.
  DCHECK_EQ(type, ProgressType::kProgress);
  int64_t& last_size = individual_progress.at(destination_path);
  int64_t delta = size - last_size;
  last_size = size;

  aggregate_progress += delta;
  progress_.bytes_transferred += delta;
  speedometer_.Update(progress_.bytes_transferred);

  // Speedometer can produce infinite result which can't be serialized to JSON
  // when sending the status via private API.
  double remaining_seconds = speedometer_.GetRemainingSeconds();
  if (std::isfinite(remaining_seconds)) {
    progress_.remaining_seconds = remaining_seconds;
  }

  progress_callback_.Run(progress_);
}

void CopyOrMoveIOTaskImpl::OnCopyOrMoveComplete(size_t idx,
                                                base::File::Error error) {
  DCHECK(idx < progress_.sources.size());
  DCHECK(idx < progress_.outputs.size());

  operation_id_.reset();

  progress_.sources[idx].error = error;
  progress_.outputs[idx].error = error;

  auto& [individual_progress, aggregate_progress] = item_progresses[idx];
  individual_progress.clear();

  // Some copy and move operations (depending on the source and destination
  // filesystems) don't support progress reporting yet, so we rely on setting
  // bytes_transferred only when each item completes. By also deducting
  // `aggregate_progress` from bytes_transferred, we ensure that both operations
  // that report progress and those that don't are supported.
  progress_.bytes_transferred += source_sizes_[idx] - aggregate_progress;

  if (idx < progress_.sources.size() - 1) {
    progress_callback_.Run(progress_);
    GenerateDestinationURL(idx + 1);
    return;
  }

  // Complete: assume State::kSuccess.
  file_manager::io_task::State complete_state = State::kSuccess;

  // Look for source errors and set the complete state to State::Error if any
  // source errors are found.
  for (const auto& source : progress_.sources) {
    DCHECK(source.error.has_value());
    if (source.error != base::File::FILE_OK) {
      LOG(ERROR) << "Error on complete: error " << source.error.value() << " "
                 << base::File::ErrorToString(source.error.value());
      complete_state = State::kError;
      break;
    }
  }

  Complete(complete_state);
}

void CopyOrMoveIOTaskImpl::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

}  // namespace file_manager::io_task
