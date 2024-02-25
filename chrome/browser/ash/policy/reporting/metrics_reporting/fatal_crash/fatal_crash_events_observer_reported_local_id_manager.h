// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_REPORTED_LOCAL_ID_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_REPORTED_LOCAL_ID_MANAGER_H_

#include <queue>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

class FatalCrashEventsObserver::ReportedLocalIdManager {
 public:
  // Callback type once the save file is loaded.
  using SaveFileLoadedCallback = base::OnceCallback<void()>;

  // The result of `ShouldReport`.
  enum class ShouldReportResult : uint8_t {
    kYes = 0u,
    kNegativeTimestamp,
    kHasBeenReported,
    kCrashTooOldAndMaxNumOfSavedLocalIdsReached,
    kMaxValue = kCrashTooOldAndMaxNumOfSavedLocalIdsReached
  };

  // Create a `ReportedLocalIdManager` instance.
  //
  // Params:
  //
  // - save_file_path: Path to the save file.
  // - save_file_loaded_callback: The value of `save_file_loaded_callback_`. See
  //   its document.
  // - io_task_runner: The task runner to run IO tasks on. If nullptr, the
  //                   constructor would create a default task runner.
  static std::unique_ptr<ReportedLocalIdManager> Create(
      base::FilePath save_file_path,
      SaveFileLoadedCallback save_file_loaded_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  ReportedLocalIdManager(const ReportedLocalIdManager&) = delete;
  ReportedLocalIdManager& operator=(const ReportedLocalIdManager&) = delete;
  virtual ~ReportedLocalIdManager();

  // Returns true if a crash with the local ID has already been reported.
  bool HasBeenReported(const std::string& local_id) const;

  // Returns a value to indicate that the timestamp is negative, the local ID
  // is already in the reported Local IDs or the timestamp is no later than
  // the earliest timestamp corresponding to reported local IDs. Otherwise,
  // returns kYes.
  //
  // Not a const method because it calls `GetEarliestLocalIdEntry`, which
  // involves cleaning up saved local IDs.
  ShouldReportResult ShouldReport(const std::string& local_id,
                                  int64_t capture_timestamp_us);

  // Updates local ID. Does nothing and returns false if a crash with the
  // given local ID and capture timestamp should not be reported. Otherwise,
  // writes the update to the save file; if there are more than maximum
  // allowed local IDs, remove the oldest one.
  bool UpdateLocalId(const std::string& local_id, int64_t capture_timestamp_us);

  // Same as `UpdateLocalId`, except not writing changes to the save file.
  // Useful when loading from save file.
  bool UpdateLocalIdInRam(const std::string& local_id,
                          int64_t capture_timestamp_us);

  // Adds a local ID and its corresponding capture timestamp. Returns true if
  // the local ID can be saved to memory.
  bool Add(const std::string& local_id, int64_t capture_timestamp_us);

  // Removes a local ID, e.g., it has been reported again as uploaded. If the
  // local ID is not found, does nothing.
  void Remove(const std::string& local_id);

  // Indicates whether the save file has been loaded.
  bool IsSaveFileLoaded() const;

 private:
  // Give `TestEnvironment` the access to `kMaxNumOfLocalIds`.
  friend class FatalCrashEventsObserver::TestEnvironment;

  // `LocalIdEntry` comparator for `local_id_entry_queue_`, based on the
  // timestamp.
  class LocalIdEntryComparator {
   public:
    bool operator()(const LocalIdEntry& a, const LocalIdEntry& b) const;
  };
  // Assert no data member, therefore there's no need in explicitly defining
  // the constructors and destructor of `LocalIdEntryComparator`.
  static_assert(std::is_empty_v<LocalIdEntryComparator>);

  // The maximum number of local IDs to save.
  static constexpr size_t kMaxNumOfLocalIds = 128u;

  // The maximum size of the priority queue before reconstructing it.
  static constexpr size_t kMaxSizeOfLocalIdEntryQueue{kMaxNumOfLocalIds * 10u};

  ReportedLocalIdManager(
      base::FilePath save_file_path,
      SaveFileLoadedCallback save_file_loaded_callback,
      scoped_refptr<base::SequencedTaskRunner> io_task_runner);

  // Loads save file. Logs and ignores errors. If there is a parsing error,
  // still loads all lines before the line on which the error occurs. Does not
  // clear existing local IDs in RAM.
  void LoadSaveFile();

  // Resume loading save file after the IO part is done.
  void ResumeLoadingSaveFile(const std::string& content);

  // Writes save file based on the currently saved reported local IDs. Ignores
  // and logs any errors encountered. If the device reboots before the write
  // succeeds next time, this may lead to a repeated report of an unuploaded
  // crash, which is, however, better than the opposite, i.e., missing
  // unuploaded crash.
  void WriteSaveFile();

  // Cleans up local IDs corresponding to crashes that have been reported
  // again as uploaded in an efficient manner.
  void CleanUpLocalIdEntryQueue();

  // (Re)constructs `local_id_entry_queue_` from `local_ids_`.
  void ReconstructLocalIdEntries();

  // Gets the local ID entry corresponding to the earliest unuploaded crash.
  // Must be used before the earliest local ID is removed by any operations on
  // `local_id_entry_queue_`.
  const LocalIdEntry& GetEarliestLocalIdEntry();

  // Remove the local ID entry corresponding to the earliest unuploaded crash.
  void RemoveEarliestLocalIdEntry();

  // This instance is always on the same sequence as that of the owning
  // `FatalCrashEventsObserver` instance.
  SEQUENCE_CHECKER(sequence_checker_);

  // The file that saves reported local IDs. It is in the CSV format: csv
  // format (Column 0: Local ID, Column 1: capture timestamps).
  const base::FilePath save_file_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The temporary save file that was written to before updating `save_file_`.
  const base::FilePath save_file_tmp_ GUARDED_BY_CONTEXT(sequence_checker_){
      save_file_.AddExtension(".tmp")};

  // A map that maps local IDs to their respective capture timestamps in
  // microseconds. Only local IDs corresponding to crashes that has been
  // reported as unuploaded crashes are here. If a crash has been uploaded
  // then, its local ID would then be removed from this map.
  std::unordered_map</*local_id*/ std::string, /*timestamp*/ int64_t> local_ids_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The priority queue that makes popping out the oldest crash efficient. The
  // local IDs in this priority queue constitute a super set of those in
  // `local_ids_`, which implies that some local IDs corresponding to crashes
  // that have already been reported again as uploaded crashes. This is to
  // avoid removing removing non-top elements from a priority queue as this
  // operation is inefficient.
  std::priority_queue<LocalIdEntry,
                      std::vector<LocalIdEntry>,
                      LocalIdEntryComparator>
      local_id_entry_queue_ GUARDED_BY_CONTEXT(sequence_checker_);

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

  base::WeakPtrFactory<ReportedLocalIdManager> weak_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_REPORTED_LOCAL_ID_MANAGER_H_
