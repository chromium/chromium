// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_MANAGER_H_

#include <memory>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"

class Profile;

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace policy {

// Ties together collection, storage and upload of app install event logs. The
// app refers to extension or ARC++ app.
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
class InstallEventLogManagerBase {
 public:
  // Helper that returns a |base::SequencedTaskRunner| for background operations
  // on an event log. All background operations relating to a given log file,
  // whether by an |InstallEventLogManagerBase| or any other class, must use the
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
  };

  // All accesses to the |profile|'s app install event log file must use
  // the same |log_task_runner_wrapper| to ensure correct I/O serialization.
  InstallEventLogManagerBase(LogTaskRunnerWrapper* log_task_runner_wrapper,
                             Profile* profile);
  ~InstallEventLogManagerBase();

  // The current size of the log, returned by each operation on the log store.
  struct LogSize {
    // The total number of log entries, across all apps.
    int total_size;
    // The maximum number of log entries for a single app.
    int max_size;
  };

  // Once created, |InstallLog| runs in the background and must be accessed and
  // eventually destroyed via |log_task_runner_|.  |T| specifies the event type
  // and |C| specifies the type of type of event log class.
  template <typename T, class C>
  class InstallLog {
   public:
    InstallLog();
    InstallLog(const InstallLog<T, C>& install_log) = delete;
    InstallLog<T, C> operator=(const InstallLog<T, C>& install_log) = delete;
    virtual ~InstallLog();

    // Loads the log from disk or creates an empty log if the log file does not
    // exist. Must be called before any other methods, including the destructor.
    LogSize Init(const base::FilePath& file_path);

    // Adds an identical log entry for each app in |ids|.
    LogSize Add(const std::set<std::string>& ids, const T& event);

    // Stores the log to disk.
    void Store();

    // Clears log entries that were previously serialized and stores the
    // resulting log to disk.
    LogSize ClearSerializedAndStore();

   protected:
    // Returns the current size of the log.
    LogSize GetSize() const;

    // The actual log store.
    std::unique_ptr<C> log_;

    // Ensures that methods are not called from the wrong thread.
    SEQUENCE_CHECKER(sequence_checker_);
  };

  // Helper class that manages the storing and uploading of logs.
  class LogUpload {
   public:
    LogUpload();
    virtual ~LogUpload() = 0;

    // Callback invoked by |InstallLog::Init()|. Schedules the first log upload.
    void OnLogInit(const LogSize& log_size);

    // Callback invoked by all other operations on |InstallLog| that may change
    // its contents. (Re-)schedules log upload and log storage to disk.
    void OnLogChange(const LogSize& log_size);

    // Stores the log to disk.
    virtual void StoreLog() = 0;

    // Ensure that an upload is either already requested or scheduled for the
    // future. If |expedited| is |true|, ensures that a scheduled upload lies no
    // more than fifteen minutes in the future.
    void EnsureUpload(bool expedited);

    // Requests that uploader upload the log to the server.
    void RequestUpload();

    virtual void RequestUploadForUploader() = 0;

    template <typename T, typename C>
    void OnSerializeLogDone(T callback, std::unique_ptr<C> log);

    // The current size of the log.
    LogSize log_size_;

    // Any change to the log contents causes a task to be scheduled that will
    // store the log contents to disk five seconds later. Changes during this
    // five second window will be picked up by the scheduled store and do not
    // require another store to be scheduled.
    bool store_scheduled_ = false;

    // Whether an upload request has been sent to the uploader already. If
    // so, no further uploads are scheduled until the current request is
    // successful. The uploader retries indefinitely on errors.
    bool upload_requested_ = false;

    // Whether an upload has been scheduled for some time in the future.
    bool upload_scheduled_ = false;

    // Whether a scheduled upload is expedited (fifteen minute delay) instead of
    // regular (three hour delay).
    bool expedited_upload_scheduled_ = false;

    // After successful upload, uploaded log entries are cleared and the log is
    // stored to disk. If a store task is scheduled, this factory's weak
    // pointers are invalidated to cancel it and avoid unnecessary I/O.
    base::WeakPtrFactory<LogUpload> store_weak_factory_{this};

    // Invalidated to cancel a pending upload when the log becomes empty after
    // upload or an expedited upload is needed instead of a previously scheduled
    // regular upload.
    base::WeakPtrFactory<LogUpload> upload_weak_factory_{this};

    // Used by log store owner to access |this|. Invalidated when |this| is
    // destroyed as log store owner outlives it.
    base::WeakPtrFactory<LogUpload> log_weak_factory_{this};
  };

  // Task runner via which log store owner is accessed.
  const scoped_refptr<base::SequencedTaskRunner> log_task_runner_;
};

// Implementation details.
template <typename T, class C>
InstallEventLogManagerBase::InstallLog<T, C>::InstallLog() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

template <typename T, class C>
InstallEventLogManagerBase::InstallLog<T, C>::~InstallLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  log_->Store();
}

template <typename T, class C>
InstallEventLogManagerBase::LogSize
InstallEventLogManagerBase::InstallLog<T, C>::Init(
    const base::FilePath& file_path) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!log_);
  log_ = std::make_unique<C>(file_path);
  return GetSize();
}

template <typename T, class C>
InstallEventLogManagerBase::LogSize
InstallEventLogManagerBase::InstallLog<T, C>::Add(
    const std::set<std::string>& ids,
    const T& event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  for (const auto& id : ids) {
    log_->Add(id, event);
  }
  return GetSize();
}

template <typename T, class C>
void InstallEventLogManagerBase::InstallLog<T, C>::Store() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  log_->Store();
}

template <typename T, class C>
InstallEventLogManagerBase::LogSize
InstallEventLogManagerBase::InstallLog<T, C>::ClearSerializedAndStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(log_);
  log_->ClearSerialized();
  log_->Store();
  return GetSize();
}

template <typename T, class C>
InstallEventLogManagerBase::LogSize
InstallEventLogManagerBase::InstallLog<T, C>::GetSize() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LogSize size;
  size.total_size = log_->total_size();
  size.max_size = log_->max_size();
  return size;
}

template <typename T, typename C>
void InstallEventLogManagerBase::LogUpload::OnSerializeLogDone(
    T callback,
    std::unique_ptr<C> log) {
  std::move(callback).Run(log.get());
}

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_INSTALL_EVENT_LOG_MANAGER_H_
