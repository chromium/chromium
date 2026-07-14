// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_UPLOADING_SYSTEM_LOG_UPLOADER_DELEGATE_H_
#define CHROME_BROWSER_ASH_POLICY_UPLOADING_SYSTEM_LOG_UPLOADER_DELEGATE_H_

#include <memory>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ash/policy/uploading/system_log_uploader.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

// An implementation of the `SystemLogUploader::Delegate`, that is used to
// create an upload job and load system logs from the disk.
class SystemLogUploaderDelegate : public SystemLogUploader::Delegate {
 public:
  // `task_runner` is used for scheduling the upload task.
  explicit SystemLogUploaderDelegate(
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  SystemLogUploaderDelegate(const SystemLogUploaderDelegate&) = delete;
  SystemLogUploaderDelegate& operator=(const SystemLogUploaderDelegate&) =
      delete;
  ~SystemLogUploaderDelegate() override;

  // SystemLogUploader::Delegate:
  std::string GetPolicyAsJSON() override;
  void LoadSystemLogs(LogUploadCallback upload_callback) override;
  std::unique_ptr<UploadJob> CreateUploadJob(
      const GURL& upload_url,
      UploadJob::Delegate* delegate) override;
  void ZipSystemLogs(std::unique_ptr<SystemLogUploader::SystemLogs> system_logs,
                     ZippedLogUploadCallback upload_callback) override;

 private:
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_UPLOADING_SYSTEM_LOG_UPLOADER_DELEGATE_H_
