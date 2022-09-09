// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_UPLOADER_ON_WORKER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_UPLOADER_ON_WORKER_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/drive/drive_uploader.h"

namespace base {
class SingleThreadTaskRunner;
class SequencedTaskRunner;
}

namespace sync_file_system {
namespace drive_backend {

class DriveUploaderWrapper;

// This class wraps a part of DriveUploaderInterface class to post actual
// tasks to DriveUploaderWrapper which lives in another thread.
// Each method wraps corresponding name method of DriveUploaderInterface.
// See comments in drive_uploader.h for details.
class DriveUploaderOnWorker : public drive::DriveUploaderInterface {
 public:
  DriveUploaderOnWorker(
      const base::WeakPtr<DriveUploaderWrapper>& wrapper,
      base::SingleThreadTaskRunner* ui_task_runner,
      base::SequencedTaskRunner* worker_task_runner);

  DriveUploaderOnWorker(const DriveUploaderOnWorker&) = delete;
  DriveUploaderOnWorker& operator=(const DriveUploaderOnWorker&) = delete;

  ~DriveUploaderOnWorker() override;

  void StartBatchProcessing() override;
  void StopBatchProcessing() override;
  google_apis::CancelCallbackOnce UploadNewFile(
      const std::string& parent_resource_id,
      const base::FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      const drive::UploadNewFileOptions& options,
      drive::UploadCompletionCallback callback,
      google_apis::ProgressCallback progress_callback) override;

  google_apis::CancelCallbackOnce UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const drive::UploadExistingFileOptions& options,
      drive::UploadCompletionCallback callback,
      google_apis::ProgressCallback progress_callback) override;

  // Following method is expected not to be used.
  google_apis::CancelCallbackOnce ResumeUploadFile(
      const GURL& upload_location,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      drive::UploadCompletionCallback callback,
      google_apis::ProgressCallback progress_callback) override;

 private:
  base::WeakPtr<DriveUploaderWrapper> wrapper_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> worker_task_runner_;

  SEQUENCE_CHECKER(sequece_checker_);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_DRIVE_UPLOADER_ON_WORKER_H_
