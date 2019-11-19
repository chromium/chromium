// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/chromeos/policy/app_install_event_log_uploader.h"
#include "chrome/browser/chromeos/policy/app_install_event_logger.h"

class Profile;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace enterprise_management {
class AppInstallReportLogEvent;
class AppInstallReportRequest;
}  // namespace enterprise_management

namespace policy {

class AppInstallEventLog;

// Ties together collection, storage and upload of app push-install event logs.
// Owns an |AppInstallEventLog| for log storage and an |AppInstallEventLogger|
// for log collection. The |AppInstallEventUploader| is passed to the
// constructor and must outlive |this|.
//
// Newly added log entries are held in memory first and stored to disk no more
// than five seconds later. The log is also written to disk every time it has
// been successfully uploaded to the server and on logout.
//
// Uploads to the server are scheduled as follows:
// * The first upload happens fifteen minutes after |this| is instantiated. This
//   ensures that initial activity in short-lived, ephemeral sessions is not
//   lost.
// * Subsequent uploads are scheduled three hours after the last successful
//   upload and suspended if the log becomes empty.
// * If the log is getting full, the next upload is expedited from three hours
//   to fifteen minutes delay.
class AppInstallEventLogManager : public AppInstallEventLogger::Delegate,
                                  public AppInstallEventLogUploader::Delegate {
 public:
  // Helper that returns a |base::SequencedTaskRunner| for background operations
  // on an event log. All background operations relating to a given log file,
  // whether by an |AppInstallEventLogManager| or any other class, must use the
  // same |base::SequencedTaskRunner| returned by a |LogTaskRunnerWrapper|
  // instance to ensure correct serialization.
  class LogTaskRunnerWrapper {
   public:
    LogTaskRunnerWrapper();
    virtual ~LogTaskRunnerWrapper();

    // Returns a |base::SequencedTaskRunner| that executes tasks in order and
    // runs any pending tasks on shutdown (to ensure the log is stored to disk).
    // Virtual for testing.
    virtual scoped_refptr<base::SequencedTaskRunner> GetTaskRunner();

   private:
    scoped_refptr<base::SequencedTaskRunner> task_runner_;

    DISALLOW_COPY_AND_ASSIGN(LogTaskRunnerWrapper);
  };

  // All accesses to the |profile|'s app push-install event log file must use
  // the same |log_task_runner_wrapper| to ensure correct I/O serialization.
  // |uploader| must outlive |this|.
  AppInstallEventLogManager(LogTaskRunnerWrapper* log_task_runner_wrapper,
                            AppInstallEventLogUploader* uploader,
                            Profile* profile);

  // Posts a task to |log_task_runner_| that stores the log to file and destroys
  // |log_|. |log_| thus outlives |this| but any pending callbacks are canceled
  // by invalidating weak pointers.
  ~AppInstallEventLogManager() override;

  // Clears all data related to the app-install event log for |profile|. Must
  // not be called while an |AppInstallEventLogManager| exists for |profile|.
  // This method and any other accesses to the |profile|'s app push-install
  // event log must use the same |log_task_runner_wrapper| to ensure correct I/O
  // serialization.
  static void Clear(LogTaskRunnerWrapper* log_task_runner_wrapper,
                    Profile* profile);

  // AppInstallEventLogger::Delegate:
  void Add(
      const std::set<std::string>& packages,
      const enterprise_management::AppInstallReportLogEvent& event) override;

  // AppInstallEventLogUploader::Delegate:
  void SerializeForUpload(
      AppInstallEventLogUploader::Delegate::SerializationCallback callback)
      override;
  void OnUploadSuccess() override;

 private:
  // The current size of the log, returned by each operation on the log store.
  struct LogSize {
    // The total number of log entries, across all apps.
    int total_size;
    // The maximum number of log entries for a single app.
    int max_size;
  };

  // Once created, |Log| runs in the background and must be accessed and
  // eventually destroyed via |log_task_runner_|. |Log| outlives its parent and
  // stores the current log to disk in its destructor.
  class Log {
   public:
    Log();

    // Stores the current log to disk.
    ~Log();

    // Loads the log from disk or creates an empty log if the log file does not
    // exist. Must be called before any other methods, including the destructor.
    LogSize Init(const base::FilePath& file_path);

    // Adds an identical log entry for app in |packages|.
    LogSize Add(const std::set<std::string>& packages,
                const enterprise_management::AppInstallReportLogEvent& event);

    // Stores the log to disk.
    void Store();

    // Serializes the log to a protobuf for upload.
    std::unique_ptr<enterprise_management::AppInstallReportRequest> Serialize();

    // Clears log entries that were previously serialized and stores the
    // resulting log to disk.
    LogSize ClearSerializedAndStore();

   private:
    // Returns the current size of the log.
    LogSize GetSize() const;

    // The actual log store.
    std::unique_ptr<AppInstallEventLog> log_;

    // Ensures that methods are not called from the wrong thread.
    SEQUENCE_CHECKER(sequence_checker_);

    DISALLOW_COPY_AND_ASSIGN(Log);
  };

  // Callback invoked by |Log::Init()|. Schedules the first log upload.
  void OnLogInit(const LogSize& log_size);

  // Callback invoked by all other operations on |Log| that may change its
  // contents. (Re-)schedules log upload and log storage to disk.
  void OnLogChange(const LogSize& log_size);

  // Callback invoked by |Log::Serialize()|. Forwards the log contents received
  // in |log| to |callback| for upload to the server.
  void OnSerializeLogDone(
      AppInstallEventLogUploader::Delegate::SerializationCallback callback,
      std::unique_ptr<enterprise_management::AppInstallReportRequest> log);

  // Stores the log to disk.
  void StoreLog();

  // Ensure that an upload is either already requested or scheduled for the
  // future. If |expedited| is |true|, ensures that a scheduled upload lies no
  // more than fifteen minutes in the future.
  void EnsureUpload(bool expedited);

  // Requests that |uploader_| upload the log to the server.
  void RequestUpload();

  // Task runner via which |log_| is accessed.
  const scoped_refptr<base::SequencedTaskRunner> log_task_runner_;

  // Uploads logs to the server.
  AppInstallEventLogUploader* const uploader_;

  // Helper that owns the log store. Once created, must only be accessed via
  // |log_task_runner_|. Outlives |this| and ensures the log is stored to disk
  // in its destructor.
  std::unique_ptr<Log> log_;

  // Collects log events and passes them to |this|.
  std::unique_ptr<AppInstallEventLogger> logger_;

  // The current size of the log.
  LogSize log_size_;

  // Any change to the log contents causes a task to be scheduled that will
  // store the log contents to disk five seconds later. Changes during this five
  // second window will be picked up by the scheduled store and do not require
  // another store to be scheduled.
  bool store_scheduled_ = false;

  // Whether an upload request has been sent to the |uploader_| already. If so,
  // no further uploads are scheduled until the current request is successful.
  // The |uploader_| retries indefinitely on errors.
  bool upload_requested_ = false;

  // Whether an upload has been scheduled for some time in the future.
  bool upload_scheduled_ = false;

  // Whether a scheduled upload is expedited (fifteen minute delay) instead of
  // regular (three hour delay).
  bool expedited_upload_scheduled_ = false;

  // After successful upload, uploaded log entries are cleared and the log is
  // stored to disk. If a store task is scheduled, this factory's weak pointers
  // are invalidated to cancel it and avoid unnecessary I/O.
  base::WeakPtrFactory<AppInstallEventLogManager> store_weak_factory_{this};

  // Invalidated to cancel a pending upload when the log becomes empty after
  // upload or an expedited upload is needed instead of a previously scheduled
  // regular upload.
  base::WeakPtrFactory<AppInstallEventLogManager> upload_weak_factory_{this};

  // Used by |log_| to access |this|. Invalidated when |this| is destroyed as
  // |log_| outlives it.
  base::WeakPtrFactory<AppInstallEventLogManager> log_weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AppInstallEventLogManager);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_APP_INSTALL_EVENT_LOG_MANAGER_H_
