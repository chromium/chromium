// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_io_task.h"

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/thread_pool.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

namespace file_manager {
namespace io_task {

namespace {

// Obtains metadata of a URL. Used to get the filesize of the copied files.
void GetFileMetadataOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& url,
    int fields,
    storage::FileSystemOperation::GetMetadataCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  file_system_context->operation_runner()->GetMetadata(url, fields,
                                                       std::move(callback));
}

// Starts the copy operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    const storage::FileSystemOperation::CopyOrMoveProgressCallback&
        progress_callback,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Copy(
      source_url, destination_url,
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT, progress_callback,
      std::move(complete_callback));
}

}  // namespace

CopyIOTask::CopyIOTask(
    std::vector<storage::FileSystemURL> source_urls,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context)
    : profile_(profile), file_system_context_(file_system_context) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kCopy;
  progress_.destination_folder = std::move(destination_folder);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : source_urls) {
    progress_.sources.emplace_back(url, absl::nullopt);
  }

  source_sizes_.reserve(source_urls.size());
}

CopyIOTask::~CopyIOTask() {
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

void CopyIOTask::Execute(IOTask::ProgressCallback progress_callback,
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

void CopyIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void CopyIOTask::Complete(State state) {
  progress_.state = state;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

// Computes the total size of all source files and stores it in
// |progress_.total_bytes|.
void CopyIOTask::GetFileSize(size_t idx) {
  DCHECK(idx < progress_.sources.size());
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &GetFileMetadataOnIOThread, file_system_context_,
          progress_.sources[idx].url,
          storage::FileSystemOperation::GET_METADATA_FIELD_SIZE |
              storage::FileSystemOperation::GET_METADATA_FIELD_TOTAL_SIZE,
          google_apis::CreateRelayCallback(base::BindOnce(
              &CopyIOTask::GotFileSize, weak_ptr_factory_.GetWeakPtr(), idx))));
}

// Helper function to GetFileSize() that is called when the metadata for a file
// is retrieved.
void CopyIOTask::GotFileSize(size_t idx,
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
    if (util::IsNonNativeFileSystemType(progress_.destination_folder.type())) {
      // Destination is a virtual filesystem, so skip checking free space.
      GenerateDestinationURL(0);
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                         progress_.destination_folder.path()),
          base::BindOnce(&CopyIOTask::GotFreeDiskSpace,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

// Ensures that there is enough free space on the destination volume.
void CopyIOTask::GotFreeDiskSpace(int64_t free_space) {
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (progress_.destination_folder.filesystem_id() ==
          util::GetDownloadsMountPointName(profile_) ||
      (drive_integration_service &&
       drive_integration_service->GetMountPointPath().IsParent(
           progress_.destination_folder.path()))) {
    free_space -= cryptohome::kMinFreeSpaceInBytes;
  }
  if (progress_.total_bytes > free_space) {
    progress_.outputs.emplace_back(progress_.destination_folder,
                                   base::File::FILE_ERROR_NO_SPACE);
    Complete(State::kError);
    return;
  }

  GenerateDestinationURL(0);
}

// Tries to find an unused filename in the destination folder for a specific
// entry being copied.
void CopyIOTask::GenerateDestinationURL(size_t idx) {
  DCHECK(idx < progress_.sources.size());
  util::GenerateUnusedFilename(
      progress_.destination_folder,
      progress_.sources[idx].url.path().BaseName(), file_system_context_,
      base::BindOnce(&CopyIOTask::CopyFile, weak_ptr_factory_.GetWeakPtr(),
                     idx));
}

// Starts the underlying copy operation.
void CopyIOTask::CopyFile(
    size_t idx,
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  DCHECK(idx < progress_.sources.size());
  if (destination_result.is_error()) {
    progress_.outputs.emplace_back(progress_.destination_folder, absl::nullopt);
    OnCopyComplete(idx, destination_result.error());
    return;
  }
  progress_.outputs.emplace_back(destination_result.value(), absl::nullopt);

  last_progress_size_ = 0;

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &StartCopyOnIOThread, file_system_context_,
          progress_.sources[idx].url, destination_result.value(),
          google_apis::CreateRelayCallback(base::BindRepeating(
              &CopyIOTask::OnCopyProgress, weak_ptr_factory_.GetWeakPtr())),
          google_apis::CreateRelayCallback(
              base::BindOnce(&CopyIOTask::OnCopyComplete,
                             weak_ptr_factory_.GetWeakPtr(), idx))),
      base::BindOnce(&CopyIOTask::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CopyIOTask::OnCopyProgress(
    storage::FileSystemOperation::CopyOrMoveProgressType type,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    int64_t size) {
  // |size| is only valid for kProgress.
  if (type != storage::FileSystemOperation::CopyOrMoveProgressType::kProgress)
    return;

  progress_.bytes_transferred += size - last_progress_size_;
  last_progress_size_ = size;
  progress_callback_.Run(progress_);
}

void CopyIOTask::OnCopyComplete(size_t idx, base::File::Error error) {
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

void CopyIOTask::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

}  // namespace io_task
}  // namespace file_manager
