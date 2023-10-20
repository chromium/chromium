// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_

#include <memory>
#include <queue>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/util/status.h"

namespace reporting {

// Observes fatal crash events.
class FatalCrashEventsObserver
    : public MojoServiceEventsObserverBase<
          ash::cros_healthd::mojom::EventObserver>,
      public ash::cros_healthd::mojom::EventObserver {
 public:
  // A RAII class that setups the environment for testing this class.
  class TestEnvironment;

  // An entry corresponds to a crash that is saved in the save file of reported
  // local IDs.
  struct LocalIdEntry {
    std::string local_id;
    int64_t capture_timestamp_us;
  };

  using SkippedUnuploadedCrashCallback =
      base::RepeatingCallback<void(LocalIdEntry)>;
  using SkippedUploadedCrashCallback =
      base::RepeatingCallback<void(std::string /* crash_report_id */,
                                   base::Time /* creation_time */,
                                   uint64_t /* offset */)>;

  // UMA name for recording the reason that an unuploaded crash should not be
  // reported.
  static constexpr char kUmaUnuploadedCrashShouldNotReportReason[] =
      "Browser.ERP.UnuploadedCrashShouldNotReportReason";

  // Create a `FatalCrashEventsObserver` instance.
  static std::unique_ptr<FatalCrashEventsObserver> Create();

  FatalCrashEventsObserver(const FatalCrashEventsObserver& other) = delete;
  FatalCrashEventsObserver& operator=(const FatalCrashEventsObserver& other) =
      delete;

  ~FatalCrashEventsObserver() override;

  // Convert a `base::Time` to a timestamp in microseconds.
  static int64_t ConvertTimeToMicroseconds(base::Time t);

  // Sets the callback that is called when an unuploaded crash is skipped.
  void SetSkippedUnuploadedCrashCallback(
      SkippedUnuploadedCrashCallback callback);

  // Sets the callback that is called when an uploaded crash is skipped.
  void SetSkippedUploadedCrashCallback(SkippedUploadedCrashCallback callback);

 private:
  // Give `TestEnvironment` the access to the private constructor that
  // specifies the path for the save file.
  friend class FatalCrashEventsObserver::TestEnvironment;

  // Manages the local IDs corresponding to reported unuploaded crashes. Once
  // a crash is uploaded, it is outside the purview of this class.
  class ReportedLocalIdManager {
   public:
    // The result of `ShouldReport`.
    enum class ShouldReportResult : uint8_t {
      kYes = 0u,
      kNegativeTimestamp,
      kHasBeenReported,
      kCrashTooOldAndMaxNumOfSavedLocalIdsReached,
      kMaxValue = kCrashTooOldAndMaxNumOfSavedLocalIdsReached
    };

    static std::unique_ptr<ReportedLocalIdManager> Create(
        base::FilePath save_file_path);
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
    bool UpdateLocalId(const std::string& local_id,
                       int64_t capture_timestamp_us);

    // Adds a local ID and its corresponding capture timestamp. Returns true if
    // the local ID can be saved to memory.
    bool Add(const std::string& local_id, int64_t capture_timestamp_us);

    // Removes a local ID, e.g., it has been reported again as uploaded. If the
    // local ID is not found, does nothing.
    void Remove(const std::string& local_id);

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
    static constexpr size_t kMaxSizeOfLocalIdEntryQueue{kMaxNumOfLocalIds *
                                                        10u};

    explicit ReportedLocalIdManager(base::FilePath save_file_path);

    // Loads save file. Logs and ignores errors. If there is a parsing error,
    // still loads all lines before the line on which the error occurs. Does not
    // clear existing local IDs in RAM.
    void LoadSaveFile();

    // Writes save file based on the currently saved reported local IDs. Ignores
    // and logs any errors encountered. If the device reboots before the write
    // succeeds next time, this may lead to a repeated report of an unuploaded
    // crash, which is, however, better than the opposite, i.e., missing
    // unuploaded crash.
    void WriteSaveFile() const;

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
    std::unordered_map</*local_id*/ std::string, /*timestamp*/ int64_t>
        local_ids_ GUARDED_BY_CONTEXT(sequence_checker_);

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
  };

  // Manages uploaded crash info, namely the creation time and offset of
  // uploads.log since last report.
  class UploadedCrashInfoManager {
   public:
    static std::unique_ptr<UploadedCrashInfoManager> Create(
        base::FilePath save_file_path);
    UploadedCrashInfoManager(const UploadedCrashInfoManager&) = delete;
    UploadedCrashInfoManager& operator=(const UploadedCrashInfoManager&) =
        delete;
    virtual ~UploadedCrashInfoManager();

    // Tells whether a given crash event should be reported.
    bool ShouldReport(
        const ash::cros_healthd::mojom::CrashUploadInfoPtr& upload_info) const;

    // Updates uploaded crash info if the given info is newer.
    void Update(base::Time uploads_log_creation_time,
                uint64_t uploads_log_offset);

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

    explicit UploadedCrashInfoManager(base::FilePath save_file_path);

    // Loads the save file in JSON format.
    [[nodiscard]] base::expected<ParseResult, Status> LoadSaveFile();
    // Writes the save file in JSON format.
    Status WriteSaveFile() const;
    // Is the given creation time and offset newer than the currently saved.
    bool IsNewer(base::Time uploads_log_creation_time,
                 uint64_t uploads_log_offset) const;

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
  };

  FatalCrashEventsObserver();
  FatalCrashEventsObserver(base::FilePath reported_local_id_save_file,
                           base::FilePath uploaded_crash_info_save_file);

  MetricData FillFatalCrashTelemetry(
      const ::ash::cros_healthd::mojom::CrashEventInfoPtr& info);

  // ash::cros_healthd::mojom::EventObserver:
  void OnEvent(const ash::cros_healthd::mojom::EventInfoPtr info) override;

  // CrosHealthdEventsObserverBase
  void AddObserver() override;

  // Sets whether to continue postprocessing after event observed callback is
  // called. Pass in true to simulate that event observed callback is
  // interrupted right after it's finished.
  void SetInterruptedAfterEventObservedForTest(
      bool interrupted_after_event_observed);

  SEQUENCE_CHECKER(sequence_checker_);

  // Manages saved local IDs for reported unuploaded crashes.
  std::unique_ptr<ReportedLocalIdManager> reported_local_id_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Manages the state of uploaded crash info.
  std::unique_ptr<UploadedCrashInfoManager> uploaded_crash_info_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Called when an unuploaded crash is skipped and not reported. Currently
  // only used in tests but production code may also use it in the future.
  SkippedUnuploadedCrashCallback skipped_unuploaded_callback_
      GUARDED_BY_CONTEXT(sequence_checker_){base::DoNothing()};

  // Called when an uploaded crash is skipped and not reported. Currently only
  // used in tests but production code may also use it in the future.
  SkippedUploadedCrashCallback skipped_uploaded_callback_
      GUARDED_BY_CONTEXT(sequence_checker_){base::DoNothing()};

  // If true, stop the processing after the event observed callback is called.
  // Only used for testing.
  bool interrupted_after_event_observed_for_test_
      GUARDED_BY_CONTEXT(sequence_checker_){false};
};
}  // namespace reporting
#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
