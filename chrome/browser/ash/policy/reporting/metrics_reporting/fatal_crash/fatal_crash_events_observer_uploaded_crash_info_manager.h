// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_UPLOADED_CRASH_INFO_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_UPLOADED_CRASH_INFO_MANAGER_H_

#include <atomic>
#include <cstddef>
#include <memory>
#include <string_view>

#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"

namespace reporting {

class FatalCrashEventsObserver::UploadedCrashInfoManager {
 public:
  // Callback type once the save file is loaded.
  using SaveFileLoadedCallback = base::OnceCallback<void()>;

  // Create a `UploadedCrashInfoManager` instance.
  //
  // Params:
  //
  // - save_file_path: Path to the save file.
  // - save_file_loaded_callback: The value of `save_file_loaded_callback_`. See
  //   its document.
  // - io_task_runner: The task runner to run IO tasks on. If nullptr, the
  //                   constructor would create a default task runner.
  static std::unique_ptr<UploadedCrashInfoManager> Create(
      base::FilePath save_file_path,
      SaveFileLoadedCallback save_file_loaded_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  UploadedCrashInfoManager(const UploadedCrashInfoManager&) = delete;
  UploadedCrashInfoManager& operator=(const UploadedCrashInfoManager&) = delete;
  virtual ~UploadedCrashInfoManager();

  // Tells whether a given crash event should be reported.
  bool ShouldReport(
      const ash::cros_healthd::mojom::CrashUploadInfoPtr& upload_info) const;

  // Updates uploaded crash info if the given info is newer.
  void Update(base::Time uploads_log_creation_time,
              uint64_t uploads_log_offset);

  // Indicates whether the save file has been loaded.
  bool IsSaveFileLoaded() const;

 private:
  // Give `TestEnvironment` the access to the JSON key strings.
  friend class FatalCrashEventsObserver::TestEnvironment;

  struct ParseResult {
    int64_t uploads_log_creation_timestamp_ms;
    uint64_t uploads_log_offset;
  };

  // Keys of the fields in the save file.
  static constexpr std::string_view kCreationTimestampMsJsonKey =
      "creation_timestamp_ms";
  static constexpr std::string_view kOffsetJsonKey = "offset";

  explicit UploadedCrashInfoManager(
      base::FilePath save_file_path,
      SaveFileLoadedCallback save_file_loaded_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Loads the save file in JSON format.
  void LoadSaveFile();

  // Resume loading save file after the IO part is done.
  void ResumeLoadingSaveFile(const StatusOr<std::string>& content);

  // Writes the save file in JSON format. Returns an OK status if everything
  // other than file writing itself succeeds. If file writing fails, it will be
  // logged from the IO sequence.
  Status WriteSaveFile() const;

  // Is the given creation time and offset newer than the currently saved.
  bool IsNewer(base::Time uploads_log_creation_time,
               uint64_t uploads_log_offset) const;

  // This instance is always on the same sequence as that of the owning
  // `FatalCrashEventsObserver` instance.
  SEQUENCE_CHECKER(sequence_checker_);

  // The JSON file that saves the creation time and offset of uploads.log
  // since last report.
  const base::FilePath save_file_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The temporary save file that was written to before updating `save_file_`.
  const base::FilePath save_file_tmp_ GUARDED_BY_CONTEXT(sequence_checker_){
      save_file_.AddExtension(".tmp")};

  // The creation time of uploads.log of the last reported crash. Initialize
  // this to minimum creation time possible so that the first uploaded crash
  // (which always has a creation time larger than `base::Time::Min()`) would
  // always be reported.
  base::Time uploads_log_creation_time_ GUARDED_BY_CONTEXT(sequence_checker_){
      base::Time::Min()};

  // The offset of uploads.log of the last reported crash.
  uint64_t uploads_log_offset_ GUARDED_BY_CONTEXT(sequence_checker_){0u};

  // Indicates whether loading has finished.
  bool save_file_loaded_ GUARDED_BY_CONTEXT(sequence_checker_){false};

  // Called when the save file is loaded. Should only be called once because the
  // save file is only loaded once throughout the lifetime of this class
  // instance.
  SaveFileLoadedCallback save_file_loaded_callback_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The task runner that performs IO.
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  // The counter that keeps track of the number of IO tasks posted. This is
  // mostly used to avoid duplicate IO tasks, i.e., a later save file writing
  // task is posted and there's no need to executing an earlier file writing
  // task that has not started. It is always deleted on the IO thread so that
  // when an instance of this class is destroyed, this counter remains
  // accessible for all tasks posted to the IO thread.
  const std::unique_ptr<std::atomic<uint64_t>, base::OnTaskRunnerDeleter>
      latest_save_file_writing_task_id_{
          new std::atomic<uint64_t>(0u),
          base::OnTaskRunnerDeleter(io_task_runner_)};

  base::WeakPtrFactory<UploadedCrashInfoManager> weak_factory_{this};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_UPLOADED_CRASH_INFO_MANAGER_H_
