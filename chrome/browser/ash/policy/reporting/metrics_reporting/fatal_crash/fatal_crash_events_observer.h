// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_

#include <memory>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

// Observes fatal crash events. Due to the IO on the save files, this class
// should only have one instance.
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
  class ReportedLocalIdManager;

  // Manages uploaded crash info, namely the creation time and offset of
  // uploads.log since last report.
  class UploadedCrashInfoManager;

  FatalCrashEventsObserver();
  FatalCrashEventsObserver(base::FilePath reported_local_id_save_file,
                           base::FilePath uploaded_crash_info_save_file,
                           base::TimeDelta backoff_time_for_loading);

  MetricData FillFatalCrashTelemetry(
      const ::ash::cros_healthd::mojom::CrashEventInfoPtr& info);

  // ash::cros_healthd::mojom::EventObserver:
  void OnEvent(const ash::cros_healthd::mojom::EventInfoPtr info) override;

  // CrosHealthdEventsObserverBase
  void AddObserver() override;

  // Indicates whether the save files have been loaded.
  bool AreLoaded() const;

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

  const base::TimeDelta backoff_time_for_loading_;

  base::WeakPtrFactory<FatalCrashEventsObserver> weak_factory_{this};
};
}  // namespace reporting
#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
