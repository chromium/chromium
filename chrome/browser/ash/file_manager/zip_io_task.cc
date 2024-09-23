// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/zip_io_task.h"

#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/file_util_service.h"
#include "chrome/services/file_util/public/cpp/zip_file_creator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager {
namespace io_task {

namespace {

int64_t ComputeSize(base::FilePath src_dir,
                    std::vector<base::FilePath> src_files) {
  VLOG(1) << ">>> Computing total size of " << src_files.size() << " items...";
  int64_t total_bytes = 0;
  base::File::Info info;
  for (const base::FilePath& relative_path : src_files) {
    const base::FilePath absolute_path = src_dir.Append(relative_path);

    if (base::GetFileInfo(absolute_path, &info)) {
      total_bytes += info.is_directory
                         ? base::ComputeDirectorySize(absolute_path)
                         : info.size;
    }
  }
  VLOG(1) << "<<< Total size is " << total_bytes << " bytes";
  return total_bytes;
}

}  // namespace

ZipIOTask::ZipIOTask(
    std::vector<storage::FileSystemURL> source_urls,
    storage::FileSystemURL parent_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : IOTask(show_notification),
      profile_(profile),
      file_system_context_(file_system_context) {
  progress_.state = State::kQueued;
  progress_.type = OperationType::kZip;
  progress_.SetDestinationFolder(std::move(parent_folder), profile);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (auto& url : source_urls) {
    progress_.sources.emplace_back(std::move(url), std::nullopt);
  }
}

ZipIOTask::~ZipIOTask() {
  if (zip_file_creator_) {
    zip_file_creator_->Stop();
  }
}

void ZipIOTask::Execute(IOTask::ProgressCallback progress_callback,
                        IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  start_time_ = base::TimeTicks::Now();

  if (progress_.sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }
  progress_.state = State::kInProgress;

  // Convert the destination folder URL to absolute path.
  source_dir_ = progress_.GetDestinationFolder().path();
  if (!ash::FileSystemBackend::CanHandleURL(progress_.GetDestinationFolder()) ||
      source_dir_.empty()) {
    progress_.outputs.emplace_back(progress_.GetDestinationFolder(),
                                   base::File::FILE_ERROR_NOT_FOUND);
    Complete(State::kError);
    return;
  }

  // Convert source file URLs to relative paths.
  for (EntryStatus& source : progress_.sources) {
    const base::FilePath absolute_path = source.url.path();
    if (!ash::FileSystemBackend::CanHandleURL(source.url) ||
        absolute_path.empty()) {
      source.error = base::File::FILE_ERROR_NOT_FOUND;
      Complete(State::kError);
      return;
    }

    base::FilePath relative_path;
    if (!source_dir_.AppendRelativePath(absolute_path, &relative_path)) {
      source.error = base::File::FILE_ERROR_INVALID_OPERATION;
      Complete(State::kError);
      return;
    }
    source_relative_paths_.push_back(std::move(relative_path));

    if (file_manager::util::IsDriveLocalPath(profile_, absolute_path) &&
        file_manager::file_tasks::IsOfficeFile(absolute_path)) {
      UMA_HISTOGRAM_ENUMERATION(
          file_manager::file_tasks::kUseOutsideDriveMetricName,
          file_manager::file_tasks::OfficeFilesUseOutsideDriveHook::ZIP);
      auto* drive_service =
          drive::util::GetIntegrationServiceByProfile(profile_);
      if (drive_service) {
        drive_service->ForceReSyncFile(
            absolute_path, base::BindOnce(&ZipIOTask::OnFilePreprocessed,
                                          weak_ptr_factory_.GetWeakPtr()));
        continue;
      }
    }
    OnFilePreprocessed();
  }
}

void ZipIOTask::OnFilePreprocessed() {
  DCHECK_LT(files_preprocessed_, progress_.sources.size());
  files_preprocessed_++;
  if (files_preprocessed_ < progress_.sources.size()) {
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ComputeSize, source_dir_, source_relative_paths_),
      base::BindOnce(&ZipIOTask::GenerateZipNameAfterGotTotalBytes,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ZipIOTask::Cancel() {
  progress_.state = State::kCancelled;
  // Any inflight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void ZipIOTask::Complete(State state) {
  progress_.state = state;
  if (state == State::kSuccess) {
    base::UmaHistogramTimes("FileBrowser.ZipTask.Time",
                            base::TimeTicks::Now() - start_time_);
  }
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

// Generates the destination url for the ZIP file.
void ZipIOTask::GenerateZipNameAfterGotTotalBytes(int64_t total_bytes) {
  progress_.total_bytes = total_bytes;
  speedometer_.SetTotalBytes(progress_.total_bytes);

  // TODO(crbug.com/1238237) Localize the name.
  base::FilePath zip_name("Archive.zip");
  if (source_relative_paths_.size() == 1) {
    zip_name = source_relative_paths_[0].BaseName().ReplaceExtension("zip");
  }
  util::GenerateUnusedFilename(
      progress_.GetDestinationFolder(), zip_name, file_system_context_,
      base::BindOnce(&ZipIOTask::ZipItems, weak_ptr_factory_.GetWeakPtr()));
}

// Starts the zip operation.
void ZipIOTask::ZipItems(
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  if (!destination_result.has_value()) {
    progress_.outputs.emplace_back(progress_.GetDestinationFolder(),
                                   destination_result.error());
    Complete(State::kError);
    return;
  }
  progress_.outputs.emplace_back(destination_result.value(), std::nullopt);
  progress_callback_.Run(progress_);

  zip_file_creator_ = base::MakeRefCounted<ZipFileCreator>(
      std::move(source_dir_), std::move(source_relative_paths_),
      std::move(destination_result->path()));
  zip_file_creator_->SetProgressCallback(base::BindOnce(
      &ZipIOTask::OnZipProgress, weak_ptr_factory_.GetWeakPtr()));
  zip_file_creator_->SetCompletionCallback(
      BindPostTaskToCurrentDefault(base::BindOnce(
          &ZipIOTask::OnZipComplete, weak_ptr_factory_.GetWeakPtr())));
  zip_file_creator_->Start(LaunchFileUtilService());
}

void ZipIOTask::OnZipProgress() {
  DCHECK(zip_file_creator_);
  progress_.bytes_transferred = zip_file_creator_->GetProgress().bytes;
  if (speedometer_.Update(progress_.bytes_transferred)) {
    const base::TimeDelta remaining_time = speedometer_.GetRemainingTime();

    // Speedometer can produce infinite result which can't be serialized to JSON
    // when sending the status via private API.
    if (!remaining_time.is_inf()) {
      progress_.remaining_seconds = remaining_time.InSecondsF();
    }
  }

  progress_callback_.Run(progress_);
  if (zip_file_creator_->GetResult() == ZipFileCreator::kInProgress) {
    zip_file_creator_->SetProgressCallback(base::BindOnce(
        &ZipIOTask::OnZipProgress, weak_ptr_factory_.GetWeakPtr()));
  }
}

void ZipIOTask::OnZipComplete() {
  DCHECK(zip_file_creator_);
  progress_.bytes_transferred = zip_file_creator_->GetProgress().bytes;
  switch (zip_file_creator_->GetResult()) {
    case ZipFileCreator::kSuccess:
      progress_.outputs.back().error = base::File::FILE_OK;
      Complete(State::kSuccess);
      break;
    case ZipFileCreator::kError:
      progress_.outputs.back().error = base::File::FILE_ERROR_FAILED;
      LOG(ERROR) << "Cannot create Zip archive: "
                 << zip_file_creator_->GetResult();
      Complete(State::kError);
      break;
    case ZipFileCreator::kCancelled:
      // Cancelled state already gets reported so don't call Complete().
      break;
    case ZipFileCreator::kInProgress:
      NOTREACHED_IN_MIGRATION();
  }
  zip_file_creator_.reset();
}

}  // namespace io_task
}  // namespace file_manager
