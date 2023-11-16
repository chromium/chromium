// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_

#include <memory>
#include <queue>
#include <string>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
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
  // Callbacks and other variables created solely for test purposes. Fit the
  // Chromium code style to not name it "TestSettings" because this struct is
  // also compiled in production code.
  struct SettingsForTest;

  // RAII class that set up the environment for testing this class.
  class TestEnvironment;

  // An entry corresponds to a crash that is saved in the save file of reported
  // local IDs.
  struct LocalIdEntry {
    std::string local_id;
    int64_t capture_timestamp_us;
  };

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

 private:
  // Give `TestEnvironment` the access to the private constructor that
  // specifies the path for the save file.
  friend class FatalCrashEventsObserver::TestEnvironment;

  // Manages default save file paths. The defaults are changed in browser tests.
  class SaveFilePathsProviderInterface;
  // Production implementation of `SaveFilePathsProviderInterface`.
  class DefaultSaveFilePathsProvider;

  // For `OnEvent`. Not let `TestEnvironment` be a proxy of `OnEvent` because it
  // is an exception to allow `SlowFileLoadingFieldsPassedThrough` to call
  // `OnEvent` directly. Using `TestEnvironment` as a proxy would expose
  // `OnEvent` to all tests and weaken access checks.
  FRIEND_TEST_ALL_PREFIXES(FatalCrashEventsObserverTest,
                           SlowFileLoadingFieldsPassedThrough);

  // Manages the local IDs corresponding to reported unuploaded crashes. Once
  // a crash is uploaded, it is outside the purview of this class.
  class ReportedLocalIdManager;

  // Manages uploaded crash info, namely the creation time and offset of
  // uploads.log since last report.
  class UploadedCrashInfoManager;

  FatalCrashEventsObserver();
  // This constructor enables the test code to use non-default values of the
  // input parameters to accommodate the test environment. In production code,
  // they are always the default value specified in the default constructor.
  FatalCrashEventsObserver(
      const SaveFilePathsProviderInterface& save_file_paths_provider,
      scoped_refptr<base::SequencedTaskRunner> reported_local_id_io_task_runner,
      scoped_refptr<base::SequencedTaskRunner>
          uploaded_crash_info_io_task_runner);

  MetricData FillFatalCrashTelemetry(
      const ::ash::cros_healthd::mojom::CrashEventInfoPtr& info);

  // ash::cros_healthd::mojom::EventObserver:
  void OnEvent(ash::cros_healthd::mojom::EventInfoPtr info) override;

  // CrosHealthdEventsObserverBase
  void AddObserver() override;

  // Processes an event received via `OnEvent`.
  void ProcessEvent(ash::cros_healthd::mojom::EventInfoPtr info);

  // Processes an unuploaded crash event received via `OnEvent`.
  void ProcessUnuploadedCrashEvent(
      ash::cros_healthd::mojom::CrashEventInfoPtr crash_event_info);

  // Processes an uploaded crash event received via `OnEvent`.
  void ProcessUploadedCrashEvent(
      ash::cros_healthd::mojom::CrashEventInfoPtr crash_event_info);

  // Indicates whether the save files have been loaded.
  bool AreSaveFilesLoaded() const;

  // Processes events that was received before the save files have been loaded.
  void ProcessEventsBeforeSaveFilesLoaded();

  SEQUENCE_CHECKER(sequence_checker_);

  // Manages saved local IDs for reported unuploaded crashes.
  std::unique_ptr<ReportedLocalIdManager> reported_local_id_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Manages the state of uploaded crash info.
  std::unique_ptr<UploadedCrashInfoManager> uploaded_crash_info_manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Intermediate queue that tracks events received from croshealthd before save
  // files are loaded. Recorded events are processed in the order that they are
  // received once save files are loaded.
  std::queue<ash::cros_healthd::mojom::EventInfoPtr>
      event_queue_before_save_files_loaded_
          GUARDED_BY_CONTEXT(sequence_checker_);

  // Callbacks and variables used for test only.
  std::unique_ptr<SettingsForTest> settings_for_test_ GUARDED_BY_CONTEXT(
      sequence_checker_){std::make_unique<SettingsForTest>()};

  base::WeakPtrFactory<FatalCrashEventsObserver> weak_factory_{this};
};
}  // namespace reporting
#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
