// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_UPLOADING_SYSTEM_LOG_UPLOADER_H_
#define CHROME_BROWSER_ASH_POLICY_UPLOADING_SYSTEM_LOG_UPLOADER_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/uploading/upload_job.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "components/policy/core/common/remote_commands/remote_command_job.h"

namespace base {
class SequencedTaskRunner;
}

namespace redaction {
class RedactionTool;
}

namespace policy {

// Class responsible for periodically uploading system logs, it handles the
// server responses by UploadJob:Delegate callbacks.
class SystemLogUploader : public UploadJob::Delegate {
 public:
  // Structure that stores the system log files as pairs: (file name, loaded
  // from the disk binary file data).
  using SystemLogs = std::vector<std::pair<std::string, std::string>>;

  // Remove lines from |data| that contain common PII (IP addresses, BSSIDs,
  // SSIDs, URLs, e-mail addresses).
  static std::string RemoveSensitiveData(redaction::RedactionTool* redactor,
                                         const std::string& data);

  // Refresh constants.
  static const int64_t kDefaultUploadDelayMs;
  static const int64_t kErrorUploadDelayMs;

  static const int64_t kLogThrottleCount;
  static const base::TimeDelta kLogThrottleWindowDuration;

  // Http header constants to upload zipped logs.
  static const char* const kCommandIdHeaderName;
  static const char* const kFileTypeHeaderName;
  static const char* const kFileTypeZippedLogFile;
  static const char* const kZippedLogsName;
  static const char* const kZippedLogsFileName;
  static const char* const kContentTypeOctetStream;

  // A delegate interface used by SystemLogUploader to read the system logs
  // from the disk and create an upload job.
  class Delegate {
   public:
    using LogUploadCallback =
        base::OnceCallback<void(std::unique_ptr<SystemLogs> system_logs)>;

    using ZippedLogUploadCallback =
        base::OnceCallback<void(std::string zipped_system_logs)>;

    virtual ~Delegate() {}

    // Returns current policy dump in JSON format.
    virtual std::string GetPolicyAsJSON() = 0;

    // Loads system logs and invokes |upload_callback|.
    virtual void LoadSystemLogs(LogUploadCallback upload_callback) = 0;

    // Creates a new fully configured instance of an UploadJob. This method
    // will be called exactly once per every system log upload.
    virtual std::unique_ptr<UploadJob> CreateUploadJob(
        const GURL& upload_url,
        UploadJob::Delegate* delegate) = 0;

    // Zips system logs in a single zip archive and invokes |upload_callback|.
    virtual void ZipSystemLogs(std::unique_ptr<SystemLogs> system_logs,
                               ZippedLogUploadCallback upload_callback) = 0;
  };

  // Constructor. Callers can inject their own Delegate. A nullptr can be passed
  // for |syslog_delegate| to use the default implementation.
  SystemLogUploader(
      std::unique_ptr<Delegate> syslog_delegate,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  SystemLogUploader(const SystemLogUploader&) = delete;
  SystemLogUploader& operator=(const SystemLogUploader&) = delete;

  ~SystemLogUploader() override;

  // Returns the time of the last upload attempt, or Time(0) if no upload has
  // ever happened.
  base::Time last_upload_attempt() const { return last_upload_attempt_; }

  // Schedules the next upload immediately. Is triggered in case of a remote
  // command to upload logs. Attaches `command_id` to the file upload metadata.
  void ScheduleNextSystemLogUploadImmediately(
      RemoteCommandJob::UniqueIDType command_id);

  // Removes the log upload times before the particular time window ( which were
  // uploaded before kLogThrottleWindowDuration time from now), add the latest
  // log upload time if any and return the oldest log upload time in the
  // particular time window.
  base::Time UpdateLocalStateForLogs();

  // UploadJob::Delegate:
  // Callbacks handle success and failure results of upload, destroy the
  // upload job.
  void OnSuccess() override;
  void OnFailure(UploadJob::ErrorCode error_code) override;

  bool upload_enabled() const { return upload_enabled_; }

 private:
  // Updates the system log upload enabled field from settings.
  void RefreshUploadSettings();

  // Starts the system log loading process.
  void StartLogUpload(std::optional<RemoteCommandJob::UniqueIDType> command_id);

  // The callback is invoked by the Delegate if system logs have been loaded
  // from disk, adds policy dump and calls UploadSystemLogs.
  void OnSystemLogsLoaded(
      std::optional<RemoteCommandJob::UniqueIDType> command_id,
      std::unique_ptr<SystemLogs> system_logs);

  // Uploads zipped system logs.
  void UploadZippedSystemLogs(
      std::optional<RemoteCommandJob::UniqueIDType> command_id,
      std::string zipped_system_logs);

  // Helper method that figures out when the next system log upload should
  // be scheduled.
  void ScheduleNextSystemLogUpload(
      base::TimeDelta frequency,
      std::optional<RemoteCommandJob::UniqueIDType> command_id);

  // The number of consequent retries after the failed uploads.
  int retry_count_;

  // How long to wait between system log uploads.
  base::TimeDelta upload_frequency_;

  // The time the last upload attempt was performed.
  base::Time last_upload_attempt_;

  // TaskRunner used for scheduling upload tasks.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // The upload job that is re-created on every system log upload.
  std::unique_ptr<UploadJob> upload_job_;

  // The Delegate is used to load system logs and create UploadJobs.
  std::unique_ptr<Delegate> syslog_delegate_;

  // True if system log upload is enabled. Kept cached in this object because
  // CrosSettings can switch to an unstrusted state temporarily, and we want to
  // use the last-known trusted values.
  bool upload_enabled_;

  // Subscription for callback on changes in system log upload settings.
  base::CallbackListSubscription upload_enabled_subscription_;

  base::ThreadChecker thread_checker_;

  // Used to prevent a race condition where two log uploads are being executed
  // in parallel.
  bool log_upload_in_progress_ = false;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<SystemLogUploader> weak_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_UPLOADING_SYSTEM_LOG_UPLOADER_H_
