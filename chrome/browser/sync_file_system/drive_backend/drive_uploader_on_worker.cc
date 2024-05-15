// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/drive_uploader_on_worker.h"

#include <string>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "chrome/browser/sync_file_system/drive_backend/callback_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/drive_uploader_wrapper.h"
#include "google_apis/drive/drive_api_parser.h"

namespace sync_file_system {
namespace drive_backend {

DriveUploaderOnWorker::DriveUploaderOnWorker(
      const base::WeakPtr<DriveUploaderWrapper>& wrapper,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner)
    : wrapper_(wrapper),
      ui_task_runner_(ui_task_runner),
      worker_task_runner_(worker_task_runner) {
  DETACH_FROM_SEQUENCE(sequece_checker_);
}

DriveUploaderOnWorker::~DriveUploaderOnWorker() {}

void DriveUploaderOnWorker::StartBatchProcessing() {
}
void DriveUploaderOnWorker::StopBatchProcessing() {
}

google_apis::CancelCallbackOnce DriveUploaderOnWorker::UploadNewFile(
    const std::string& parent_resource_id,
    const base::FilePath& local_file_path,
    const std::string& title,
    const std::string& content_type,
    const drive::UploadNewFileOptions& options,
    drive::UploadCompletionCallback callback,
    google_apis::ProgressCallback progress_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequece_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveUploaderWrapper::UploadNewFile, wrapper_, parent_resource_id,
          local_file_path, title, content_type, options,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveUploaderOnWorker::UploadExistingFile(
    const std::string& resource_id,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    const drive::UploadExistingFileOptions& options,
    drive::UploadCompletionCallback callback,
    google_apis::ProgressCallback progress_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequece_checker_);

  ui_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &DriveUploaderWrapper::UploadExistingFile, wrapper_, resource_id,
          local_file_path, content_type, options,
          RelayCallbackToTaskRunner(worker_task_runner_.get(), FROM_HERE,
                                    std::move(callback))));

  return google_apis::CancelCallbackOnce();
}

google_apis::CancelCallbackOnce DriveUploaderOnWorker::ResumeUploadFile(
    const GURL& upload_location,
    const base::FilePath& local_file_path,
    const std::string& content_type,
    drive::UploadCompletionCallback callback,
    google_apis::ProgressCallback progress_callback) {
  NOTREACHED_IN_MIGRATION();
  return google_apis::CancelCallbackOnce();
}

}  // namespace drive_backend
}  // namespace sync_file_system
