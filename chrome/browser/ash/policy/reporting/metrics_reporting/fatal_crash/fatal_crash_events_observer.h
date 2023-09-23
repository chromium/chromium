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
#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

// Observes fatal crash events.
class FatalCrashEventsObserver
    : public MojoServiceEventsObserverBase<
          ash::cros_healthd::mojom::EventObserver>,
      public ash::cros_healthd::mojom::EventObserver {
 public:
  // A RAII class that setups the environment for testing this class.
  class TestEnvironment;

  struct LocalIdEntry {
    std::string local_id;
    int64_t capture_timestamp_us;
  };

  static std::unique_ptr<FatalCrashEventsObserver> Create();

  FatalCrashEventsObserver(const FatalCrashEventsObserver& other) = delete;
  FatalCrashEventsObserver& operator=(const FatalCrashEventsObserver& other) =
      delete;

  ~FatalCrashEventsObserver() override;

  // Convert a `base::Time` to a timestamp in microseconds.
  static int64_t ConvertTimeToMicroseconds(base::Time t);

  // Sets the callback that is called when a crash is skipped.
  void SetSkippedCrashCallback(
      base::RepeatingCallback<void(LocalIdEntry)> callback);

 private:
  // Give `TestEnvironment` the access to the private constructor that specifies
  // the path for the save file.
  friend class FatalCrashEventsObserver::TestEnvironment;

  // Manages reported local IDs.
  class ReportedLocalIdManager {
   public:
    static std::unique_ptr<ReportedLocalIdManager> Create(
        base::FilePath save_file_path);
    ReportedLocalIdManager(const ReportedLocalIdManager&) = delete;
    ReportedLocalIdManager& operator=(const ReportedLocalIdManager&) = delete;

    virtual ~ReportedLocalIdManager();

    // Returns true unless the local ID is already in the reported Local IDs or
    // the timestamp is no later than the earliest timestamp corresponding to
    // reported local IDs.
    bool ShouldReport(const std::string& local_id,
                      int64_t capture_timestamp_us) const;

    // Updates local ID. Does nothing and returns false if a crash with the
    // given local ID and capture timestamp should not be reported. Otherwise,
    // writes the update to the save file; if there are more than maximum
    // allowed local IDs, remove the oldest one.
    bool UpdateLocalId(const std::string& local_id,
                       int64_t capture_timestamp_us);

   private:
    // Give `TestEnvironment` the access to `kMaxNumOfLocalIds`.
    friend class FatalCrashEventsObserver::TestEnvironment;

    // `LocalIdEntry` comparator for local_id_entries_, based on the timestamp.
    class LocalIdEntryComparator {
     public:
      bool operator()(const LocalIdEntry& a, const LocalIdEntry& b) const;
    };
    // Assert no data member, therefore there's no need in explicitly defining
    // the constructors and destructor of `LocalIdEntryComparator`.
    static_assert(std::is_empty_v<LocalIdEntryComparator>);

    // The maximum number of local IDs to save.
    static constexpr size_t kMaxNumOfLocalIds = 128u;

    explicit ReportedLocalIdManager(base::FilePath save_file_path);

    // Loads save file. Logs and ignores errors. If there is a parsing error,
    // still loads all lines before the line on which the error occurs. Does not
    // clear existing local IDs in RAM.
    void LoadSaveFile();

    // Writes save file based on the currently saved reported local IDs. Ignores
    // and logs any errors encountered. If the device reboots before the write
    // succeeds next time, this may lead to a repeated report of an unuploaded
    // crash, which is, however, better than the opposite,
    // i.e., missing unuploaded crash.
    void WriteSaveFile() const;

    SEQUENCE_CHECKER(sequence_checker_);

    // The file that saves reported local IDs. It is in the CSV format: csv
    // format (Column 0: Local ID, Column 1: capture timestamps).
    const base::FilePath save_file_ GUARDED_BY_CONTEXT(sequence_checker_);

    // The temporary save file that was written to before updating `save_file_`.
    const base::FilePath save_file_tmp_ GUARDED_BY_CONTEXT(sequence_checker_){
        save_file_.AddExtension(".tmp")};

    // The priority queue that makes popping out the oldest crash efficient.
    std::priority_queue<LocalIdEntry,
                        std::vector<LocalIdEntry>,
                        LocalIdEntryComparator>
        local_id_entries_ GUARDED_BY_CONTEXT(sequence_checker_);

    // A map that maps local IDs to their respective capture timestamps in
    // microseconds.
    std::unordered_map<std::string, int64_t> local_ids_
        GUARDED_BY_CONTEXT(sequence_checker_);
  };

  FatalCrashEventsObserver();
  explicit FatalCrashEventsObserver(base::FilePath reported_local_id_save_file);

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

  // Called when a crash is skipped and not reported. Currently only used in
  // tests but production code may also use it in the future.
  base::RepeatingCallback<void(LocalIdEntry)> skipped_callback_
      GUARDED_BY_CONTEXT(sequence_checker_){base::DoNothing()};

  // If true, stop the processing after the event observed callback is called.
  // Only used for testing.
  bool interrupted_after_event_observed_for_test_
      GUARDED_BY_CONTEXT(sequence_checker_){false};
};
}  // namespace reporting
#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
