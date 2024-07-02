// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_

#include <memory>
#include <optional>
#include <queue>
#include <string>

#include "base/containers/flat_set.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation_traits.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/mojo_service_events_observer_base.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"

namespace reporting {

// TODO(b/336316592): Turn this into a base class and separate the kernel/ec
// specific parts into a separate observer like
// `ChromeFatalCrashEventsObserver`.

// Observes fatal kernel and embedded controller crash events. Due to the IO on
// the save files, this class should only have one instance.
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

  // Interface for observing reported fatal crash events for event based log
  // upload. The observer lifetime is managed by `EventBasedLogManager`.
  class FatalCrashEventLogObserver : public base::CheckedObserver {
   public:
    // Called when fatal crash event is reported with the generated `upload_id`
    // for the log upload. Only the crashes with uploaded crash report will be
    // notified.
    virtual void OnFatalCrashEvent(const std::string& upload_id) = 0;
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

  void AddEventLogObserver(FatalCrashEventLogObserver* observer);

  void RemoveEventLogObserver(FatalCrashEventLogObserver* observer);

 protected:
  // Get allowed crash types.
  virtual const base::flat_set<
      ::ash::cros_healthd::mojom::CrashEventInfo::CrashType>&
  GetAllowedCrashTypes() const;

  virtual FatalCrashTelemetry::CrashType GetFatalCrashTelemetryCrashType(
      ::ash::cros_healthd::mojom::CrashEventInfo::CrashType crash_type) const;

  // This constructor enables the test code and subclasses to use non-default
  // values of the input parameters to accommodate the test environment or
  // subclass requirements.
  FatalCrashEventsObserver(
      const base::FilePath& reported_local_id_save_file_path,
      const base::FilePath& uploaded_crash_info_save_file_path,
      scoped_refptr<base::SequencedTaskRunner> reported_local_id_io_task_runner,
      scoped_refptr<base::SequencedTaskRunner>
          uploaded_crash_info_io_task_runner);

 private:
  // Give `TestEnvironment` the access to the private constructor that
  // specifies the path for the save file.
  friend class FatalCrashEventsObserver::TestEnvironment;

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

  MetricData FillFatalCrashTelemetry(
      const ::ash::cros_healthd::mojom::CrashEventInfoPtr& info,
      std::optional<std::string> event_based_log_upload_id);

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

  // Notifies `event_log_observers_` about the fatal crash event. Returns upload
  // ID generated for event based log upload if there's observers that exists.
  std::optional<std::string> NotifyFatalCrashEventLog();

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

  // Note that the observer list will be empty if LogUploadEnabled policy is
  // disabled for the device.
  base::ObserverList<FatalCrashEventLogObserver> event_log_observers_;

  // Callbacks and variables used for test only.
  std::unique_ptr<SettingsForTest> settings_for_test_ GUARDED_BY_CONTEXT(
      sequence_checker_){std::make_unique<SettingsForTest>()};

  base::WeakPtrFactory<FatalCrashEventsObserver> weak_factory_{this};
};
}  // namespace reporting

namespace base {

template <>
struct ScopedObservationTraits<
    reporting::FatalCrashEventsObserver,
    reporting::FatalCrashEventsObserver::FatalCrashEventLogObserver> {
  static void AddObserver(
      reporting::FatalCrashEventsObserver* source,
      reporting::FatalCrashEventsObserver::FatalCrashEventLogObserver*
          observer) {
    source->AddEventLogObserver(observer);
  }
  static void RemoveObserver(
      reporting::FatalCrashEventsObserver* source,
      reporting::FatalCrashEventsObserver::FatalCrashEventLogObserver*
          observer) {
    source->RemoveEventLogObserver(observer);
  }
};

}  // namespace base

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_H_
