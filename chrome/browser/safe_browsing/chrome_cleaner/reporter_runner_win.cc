// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_runner_win.h"

#include <stdint.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/task_runner_util.h"
#include "base/win/registry.h"
#include "base/win/scoped_handle.h"
#include "base/win/windows_version.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_dialog_controller_impl_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_fetcher_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_client_info_win.h"
#include "chrome/browser/safe_browsing/chrome_cleaner/srt_field_trial_win.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace safe_browsing {

namespace {

// Used to send UMA information about missing start and end time registry
// values for the reporter. Replicated in the histograms.xml file, so the order
// MUST NOT CHANGE.
enum SwReporterRunningTimeRegistryError {
  REPORTER_RUNNING_TIME_ERROR_NO_ERROR = 0,
  REPORTER_RUNNING_TIME_ERROR_REGISTRY_KEY_INVALID = 1,
  REPORTER_RUNNING_TIME_ERROR_MISSING_START_TIME = 2,
  REPORTER_RUNNING_TIME_ERROR_MISSING_END_TIME = 3,
  REPORTER_RUNNING_TIME_ERROR_MISSING_BOTH_TIMES = 4,
  REPORTER_RUNNING_TIME_ERROR_MAX,
};

// Used to send UMA information about the progress of the SwReporter launch and
// prompt sequence. Replicated in the histograms.xml file, so the order MUST
// NOT CHANGE.
enum SwReporterUmaValue {
  DEPRECATED_SW_REPORTER_EXPLICIT_REQUEST = 0,
  DEPRECATED_SW_REPORTER_STARTUP_RETRY = 1,
  DEPRECATED_SW_REPORTER_RETRIED_TOO_MANY_TIMES = 2,
  SW_REPORTER_START_EXECUTION = 3,
  SW_REPORTER_FAILED_TO_START = 4,
  DEPRECATED_SW_REPORTER_REGISTRY_EXIT_CODE = 5,
  DEPRECATED_SW_REPORTER_RESET_RETRIES = 6,
  DEPRECATED_SW_REPORTER_DOWNLOAD_START = 7,
  SW_REPORTER_NO_BROWSER = 8,
  DEPRECATED_SW_REPORTER_NO_LOCAL_STATE = 9,
  SW_REPORTER_NO_PROMPT_NEEDED = 10,
  SW_REPORTER_NO_PROMPT_FIELD_TRIAL = 11,
  SW_REPORTER_ALREADY_PROMPTED = 12,
  DEPRECATED_SW_REPORTER_RAN_DAILY = 13,
  DEPRECATED_SW_REPORTER_ADDED_TO_MENU = 14,

  SW_REPORTER_MAX,
};

// Used to send UMA information showing whether uploading of Software Reporter
// logs is enabled, or the reason why not.
// Replicated in the histograms.xml file, so the order MUST NOT CHANGE.
enum SwReporterLogsUploadsEnabled {
  REPORTER_LOGS_UPLOADS_SBER_ENABLED = 0,
  REPORTER_LOGS_UPLOADS_SBER_DISABLED = 1,
  REPORTER_LOGS_UPLOADS_RECENTLY_SENT_LOGS = 2,
  REPORTER_LOGS_UPLOADS_DISABLED_BY_USER = 3,
  REPORTER_LOGS_UPLOADS_ENABLED_BY_USER = 4,
  REPORTER_LOGS_UPLOADS_DISABLED_BY_POLICY = 5,
  REPORTER_LOGS_UPLOADS_MAX,
};

// Used to send UMA information about missing logs upload result in the registry
// for the reporter. Replicated in the histograms.xml file, so the order
// MUST NOT CHANGE.
enum SwReporterLogsUploadResultRegistryError {
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_NO_ERROR = 0,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_REGISTRY_KEY_INVALID = 1,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_NOT_FOUND = 2,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_OUT_OF_BOUNDS = 3,
  REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX,
};

const char kRunningTimeErrorMetricName[] =
    "SoftwareReporter.RunningTimeRegistryError";

internal::SwReporterTestingDelegate* g_testing_delegate_ = nullptr;

const char kFoundUwsMetricName[] = "SoftwareReporter.FoundUwS";
const char kFoundUwsReadErrorMetricName[] =
    "SoftwareReporter.FoundUwSReadError";
const char kScanTimesMetricName[] = "SoftwareReporter.UwSScanTimes";
const char kMemoryUsedMetricName[] = "SoftwareReporter.MemoryUsed";
const char kStepMetricName[] = "SoftwareReporter.Step";
const char kLogsUploadEnabledMetricName[] =
    "SoftwareReporter.LogsUploadEnabled";
const char kLogsUploadResultMetricName[] = "SoftwareReporter.LogsUploadResult";
const char kLogsUploadResultRegistryErrorMetricName[] =
    "SoftwareReporter.LogsUploadResultRegistryError";
const char kExitCodeMetricName[] = "SoftwareReporter.ExitCodeFromRegistry";
const char kEngineErrorCodeMetricName[] = "SoftwareReporter.EngineErrorCode";

// The max value for histogram SoftwareReporter.LogsUploadResult, which is used
// to send UMA information about the result of Software Reporter's attempt to
// upload logs, when logs are enabled. This value must be consistent with the
// SoftwareReporterLogsUploadResult enum defined in the histograms.xml file.
const int kSwReporterLogsUploadResultMax = 30;

// Reports metrics about the software reporter via UMA (and sometimes Rappor).
class UMAHistogramReporter {
 public:
  UMAHistogramReporter() : UMAHistogramReporter(std::string()) {}

  explicit UMAHistogramReporter(const std::string& suffix)
      : suffix_(suffix),
        registry_key_(suffix.empty()
                          ? chrome_cleaner::kSoftwareRemovalToolRegistryKey
                          : base::StringPrintf(
                                L"%ls\\%ls",
                                chrome_cleaner::kSoftwareRemovalToolRegistryKey,
                                base::UTF8ToUTF16(suffix).c_str())) {}

  // Reports the software reporter tool's version via UMA.
  void ReportVersion(const base::Version& version) const {
    DCHECK(!version.components().empty());
    // The minor version is the 2nd last component of the version,
    // or just the first component if there is only 1.
    uint32_t minor_version = 0;
    if (version.components().size() > 1)
      minor_version = version.components()[version.components().size() - 2];
    else
      minor_version = version.components()[0];
    RecordSparseHistogram("SoftwareReporter.MinorVersion", minor_version);

    // The major version for X.Y.Z is X*256^3+Y*256+Z. If there are additional
    // components, only the first three count, and if there are less than 3, the
    // missing values are just replaced by zero. So 1 is equivalent 1.0.0.
    DCHECK_LT(version.components()[0], 0x100U);
    uint32_t major_version = 0x1000000 * version.components()[0];
    if (version.components().size() >= 2) {
      DCHECK_LT(version.components()[1], 0x10000U);
      major_version += 0x100 * version.components()[1];
    }
    if (version.components().size() >= 3) {
      DCHECK_LT(version.components()[2], 0x100U);
      major_version += version.components()[2];
    }
    RecordSparseHistogram("SoftwareReporter.MajorVersion", major_version);
  }

  void ReportExitCode(int exit_code) const {
    RecordSparseHistogram("SoftwareReporter.ExitCode", exit_code);

    // Also report the exit code that the reporter writes to the registry.
    base::win::RegKey reporter_key;
    DWORD exit_code_in_registry;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValueDW(chrome_cleaner::kExitCodeValueName,
                                 &exit_code_in_registry) != ERROR_SUCCESS) {
      return;
    }

    RecordSparseHistogram(kExitCodeMetricName, exit_code_in_registry);
    reporter_key.DeleteValue(chrome_cleaner::kExitCodeValueName);
  }

  void ReportEngineErrorCode() const {
    base::win::RegKey reporter_key;
    DWORD engine_error_code;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValueDW(chrome_cleaner::kEngineErrorCodeValueName,
                                 &engine_error_code) != ERROR_SUCCESS) {
      return;
    }

    RecordSparseHistogram(kEngineErrorCodeMetricName, engine_error_code);
    reporter_key.DeleteValue(chrome_cleaner::kEngineErrorCodeValueName);
  }

  // Reports UwS found by the software reporter tool via UMA and RAPPOR.
  void ReportFoundUwS() const {
    base::win::RegKey reporter_key;
    std::vector<base::string16> found_uws_strings;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValues(chrome_cleaner::kFoundUwsValueName,
                                &found_uws_strings) != ERROR_SUCCESS) {
      return;
    }

    bool parse_error = false;
    for (const base::string16& uws_string : found_uws_strings) {
      // All UwS ids are expected to be integers.
      uint32_t uws_id = 0;
      if (base::StringToUint(uws_string, &uws_id)) {
        RecordSparseHistogram(kFoundUwsMetricName, uws_id);
      } else {
        parse_error = true;
      }
    }

    // Clean up the old value.
    reporter_key.DeleteValue(chrome_cleaner::kFoundUwsValueName);
    RecordBooleanHistogram(kFoundUwsReadErrorMetricName, parse_error);
  }

  // Reports to UMA the memory usage of the software reporter tool as reported
  // by the tool itself in the Windows registry.
  void ReportMemoryUsage() const {
    base::win::RegKey reporter_key;
    DWORD memory_used = 0;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
        reporter_key.ReadValueDW(chrome_cleaner::kMemoryUsedValueName,
                                 &memory_used) != ERROR_SUCCESS) {
      return;
    }
    RecordMemoryKBHistogram(kMemoryUsedMetricName, memory_used);
    reporter_key.DeleteValue(chrome_cleaner::kMemoryUsedValueName);
  }

  // Reports the SwReporter run time with UMA both as reported by the tool via
  // the registry and as measured by |ReporterRunner|.
  void ReportRuntime(const base::TimeDelta& reporter_running_time) const {
    RecordLongTimesHistogram("SoftwareReporter.RunningTimeAccordingToChrome",
                             reporter_running_time);

    // TODO(b/641081): This should only have KEY_QUERY_VALUE and KEY_SET_VALUE.
    base::win::RegKey reporter_key;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_ALL_ACCESS) != ERROR_SUCCESS) {
      RecordEnumerationHistogram(
          kRunningTimeErrorMetricName,
          REPORTER_RUNNING_TIME_ERROR_REGISTRY_KEY_INVALID,
          REPORTER_RUNNING_TIME_ERROR_MAX);
      return;
    }

    bool has_start_time = false;
    int64_t start_time_value = 0;
    if (reporter_key.HasValue(chrome_cleaner::kStartTimeValueName) &&
        reporter_key.ReadInt64(chrome_cleaner::kStartTimeValueName,
                               &start_time_value) == ERROR_SUCCESS) {
      has_start_time = true;
      reporter_key.DeleteValue(chrome_cleaner::kStartTimeValueName);
    }

    bool has_end_time = false;
    int64_t end_time_value = 0;
    if (reporter_key.HasValue(chrome_cleaner::kEndTimeValueName) &&
        reporter_key.ReadInt64(chrome_cleaner::kEndTimeValueName,
                               &end_time_value) == ERROR_SUCCESS) {
      has_end_time = true;
      reporter_key.DeleteValue(chrome_cleaner::kEndTimeValueName);
    }

    if (has_start_time && has_end_time) {
      base::TimeDelta registry_run_time =
          base::Time::FromInternalValue(end_time_value) -
          base::Time::FromInternalValue(start_time_value);
      RecordLongTimesHistogram("SoftwareReporter.RunningTime",
                               registry_run_time);
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_NO_ERROR,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    } else if (!has_start_time && !has_end_time) {
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_MISSING_BOTH_TIMES,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    } else if (!has_start_time) {
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_MISSING_START_TIME,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    } else {
      DCHECK(!has_end_time);
      RecordEnumerationHistogram(kRunningTimeErrorMetricName,
                                 REPORTER_RUNNING_TIME_ERROR_MISSING_END_TIME,
                                 REPORTER_RUNNING_TIME_ERROR_MAX);
    }
  }

  // Reports the UwS scan times of the software reporter tool via UMA.
  void ReportScanTimes() const {
    base::string16 scan_times_key_path = base::StringPrintf(
        L"%ls\\%ls", registry_key_.c_str(), chrome_cleaner::kScanTimesSubKey);
    // TODO(b/641081): This should only have KEY_QUERY_VALUE and KEY_SET_VALUE.
    base::win::RegKey scan_times_key;
    if (scan_times_key.Open(HKEY_CURRENT_USER, scan_times_key_path.c_str(),
                            KEY_ALL_ACCESS) != ERROR_SUCCESS) {
      return;
    }

    base::string16 value_name;
    int uws_id = 0;
    int64_t raw_scan_time = 0;
    int num_scan_times = scan_times_key.GetValueCount();
    for (int i = 0; i < num_scan_times; ++i) {
      if (scan_times_key.GetValueNameAt(i, &value_name) == ERROR_SUCCESS &&
          base::StringToInt(value_name, &uws_id) &&
          scan_times_key.ReadInt64(value_name.c_str(), &raw_scan_time) ==
              ERROR_SUCCESS) {
        base::TimeDelta scan_time =
            base::TimeDelta::FromInternalValue(raw_scan_time);
        // We report the number of seconds plus one because it can take less
        // than one second to scan some UwS and the count passed to |AddCount|
        // must be at least one.
        RecordSparseHistogramCount(kScanTimesMetricName, uws_id,
                                   scan_time.InSeconds() + 1);
      }
    }
    // Clean up by deleting the scan times key, which is a subkey of the main
    // reporter key.
    scan_times_key.Close();
    base::win::RegKey reporter_key;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_ENUMERATE_SUB_KEYS) == ERROR_SUCCESS) {
      reporter_key.DeleteKey(chrome_cleaner::kScanTimesSubKey);
    }
  }

  void RecordReporterStep(SwReporterUmaValue value) {
    RecordEnumerationHistogram(kStepMetricName, value, SW_REPORTER_MAX);
  }

  void RecordLogsUploadEnabled(SwReporterLogsUploadsEnabled value) {
    RecordEnumerationHistogram(kLogsUploadEnabledMetricName, value,
                               REPORTER_LOGS_UPLOADS_MAX);
  }

  void RecordLogsUploadResult() {
    base::win::RegKey reporter_key;
    DWORD logs_upload_result = 0;
    if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                          KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS) {
      RecordEnumerationHistogram(
          kLogsUploadResultRegistryErrorMetricName,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_REGISTRY_KEY_INVALID,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
      return;
    }

    if (reporter_key.ReadValueDW(chrome_cleaner::kLogsUploadResultValueName,
                                 &logs_upload_result) != ERROR_SUCCESS) {
      RecordEnumerationHistogram(
          kLogsUploadResultRegistryErrorMetricName,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_NOT_FOUND,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
      return;
    }

    if (logs_upload_result >= kSwReporterLogsUploadResultMax) {
      RecordEnumerationHistogram(
          kLogsUploadResultRegistryErrorMetricName,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_VALUE_OUT_OF_BOUNDS,
          REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
      return;
    }

    RecordEnumerationHistogram(kLogsUploadResultMetricName,
                               static_cast<Sample>(logs_upload_result),
                               kSwReporterLogsUploadResultMax);
    reporter_key.DeleteValue(chrome_cleaner::kLogsUploadResultValueName);
    RecordEnumerationHistogram(kLogsUploadResultRegistryErrorMetricName,
                               REPORTER_LOGS_UPLOAD_RESULT_ERROR_NO_ERROR,
                               REPORTER_LOGS_UPLOAD_RESULT_ERROR_MAX);
  }

 private:
  using Sample = base::HistogramBase::Sample;

  static constexpr base::HistogramBase::Flags kUmaHistogramFlag =
      base::HistogramBase::kUmaTargetedHistogramFlag;

  // Helper functions to record histograms with an optional suffix added to the
  // histogram name. The UMA_HISTOGRAM macros can't be used because they
  // require a constant string.

  std::string FullName(const std::string& name) const {
    if (suffix_.empty())
      return name;
    return base::StringPrintf("%s_%s", name.c_str(), suffix_.c_str());
  }

  void RecordBooleanHistogram(const std::string& name, bool sample) const {
    auto* histogram =
        base::BooleanHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
    if (histogram)
      histogram->AddBoolean(sample);
  }

  void RecordEnumerationHistogram(const std::string& name,
                                  Sample sample,
                                  Sample boundary) const {
    // See HISTOGRAM_ENUMERATION_WITH_FLAG for the parameters to |FactoryGet|.
    auto* histogram = base::LinearHistogram::FactoryGet(
        FullName(name), 1, boundary, boundary + 1, kUmaHistogramFlag);
    if (histogram)
      histogram->Add(sample);
  }

  void RecordLongTimesHistogram(const std::string& name,
                                const base::TimeDelta& sample) const {
    // See UMA_HISTOGRAM_LONG_TIMES for the parameters to |FactoryTimeGet|.
    auto* histogram = base::Histogram::FactoryTimeGet(
        FullName(name), base::TimeDelta::FromMilliseconds(1),
        base::TimeDelta::FromHours(1), 100, kUmaHistogramFlag);
    if (histogram)
      histogram->AddTime(sample);
  }

  void RecordMemoryKBHistogram(const std::string& name, Sample sample) const {
    // See UMA_HISTOGRAM_MEMORY_KB for the parameters to |FactoryGet|.
    auto* histogram = base::Histogram::FactoryGet(FullName(name), 1000, 500000,
                                                  50, kUmaHistogramFlag);
    if (histogram)
      histogram->Add(sample);
  }

  void RecordSparseHistogram(const std::string& name, Sample sample) const {
    auto* histogram =
        base::SparseHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
    if (histogram)
      histogram->Add(sample);
  }

  void RecordSparseHistogramCount(const std::string& name,
                                  Sample sample,
                                  int count) const {
    auto* histogram =
        base::SparseHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
    if (histogram)
      histogram->AddCount(sample, count);
  }

  const std::string suffix_;
  const std::wstring registry_key_;
};

// Records the reporter step without a suffix. (For steps that are never run by
// the experimental reporter.)
void RecordReporterStepHistogram(SwReporterUmaValue value) {
  UMAHistogramReporter uma;
  uma.RecordReporterStep(value);
}

ChromeCleanerController* GetCleanerController() {
  return g_testing_delegate_ ? g_testing_delegate_->GetCleanerController()
                             : ChromeCleanerController::GetInstance();
}

SwReporterInvocationResult ExitCodeToInvocationResult(int exit_code) {
  switch (exit_code) {
    case chrome_cleaner::kSwReporterCleanupNeeded:
      // This should only be used if a cleanup will not be offered. If a
      // cleanup is offered, the controller will get notified with a
      // kCleanupToBeOffered signal, and this one will be ignored.
      // Do not accept reboot required or post-reboot exit codes, since they
      // should not be sent out by the reporter, and should be treated as
      // errors.
      return SwReporterInvocationResult::kCleanupNotOffered;

    case chrome_cleaner::kSwReporterNothingFound:
    case chrome_cleaner::kSwReporterNonRemovableOnly:
    case chrome_cleaner::kSwReporterSuspiciousOnly:
      return SwReporterInvocationResult::kNothingFound;

    case chrome_cleaner::kSwReporterTimeoutWithUwS:
    case chrome_cleaner::kSwReporterTimeoutWithoutUwS:
      return SwReporterInvocationResult::kTimedOut;
  }

  return SwReporterInvocationResult::kGeneralFailure;
}

void CreateChromeCleanerDialogController() {
  if (g_testing_delegate_) {
    g_testing_delegate_->CreateChromeCleanerDialogController();
    return;
  }

  // The dialog controller manages its own lifetime. If the controller enters
  // the kInfected state, the dialog controller will show the chrome cleaner
  // dialog to the user.
  new ChromeCleanerDialogControllerImpl(GetCleanerController());
}

bool ShouldShowPromptForPeriodicRun() {
  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    RecordReporterStepHistogram(SW_REPORTER_NO_BROWSER);
    return false;
  }

  Profile* profile = browser->profile();
  DCHECK(profile);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs);

  // Don't show the prompt again if it's been shown before for this profile and
  // for the current variations seed. The seed preference will be updated once
  // the prompt is shown.
  const std::string incoming_seed = GetIncomingSRTSeed();
  const std::string old_seed = prefs->GetString(prefs::kSwReporterPromptSeed);
  if (!incoming_seed.empty() && incoming_seed == old_seed) {
    RecordReporterStepHistogram(SW_REPORTER_ALREADY_PROMPTED);
    RecordPromptNotShownWithReasonHistogram(NO_PROMPT_REASON_ALREADY_PROMPTED);
    return false;
  }

  return true;
}

base::Time Now() {
  return g_testing_delegate_ ? g_testing_delegate_->Now() : base::Time::Now();
}

}  // namespace

namespace internal {

// This class tries to run a queue of reporters and react to their exit codes.
// It schedules subsequent runs of the queue as needed, or retries as soon as a
// browser is available when none is on first try.
//
// This can't be in the anonymous namespace because it's a friend of
// ChromeMetricsServiceAccessor.
class ReporterRunner {
 public:
  // Tries to run |invocations| immediately. This must be called on the UI
  // thread.
  static void MaybeStartInvocations(
      SwReporterInvocationType invocation_type,
      SwReporterInvocationSequence&& invocations) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    DCHECK_NE(SwReporterInvocationType::kUnspecified, invocation_type);

    // Ensures that any component waiting for the reporter sequence result will
    // be notified if |invocations| doesn't get scheduled.
    base::ScopedClosureRunner scoped_runner(
        base::BindOnce(&ChromeCleanerController::OnReporterSequenceDone,
                       base::Unretained(GetCleanerController()),
                       SwReporterInvocationResult::kNotScheduled));

    PrefService* local_state = g_browser_process->local_state();
    if (!invocations.version().IsValid() || !local_state)
      return;

    // By this point the reporter should be allowed to run.
    DCHECK(SwReporterIsAllowedByPolicy());

    ReporterRunTimeInfo time_info(local_state);

    // The UI should block a new user-initiated run if the reporter is
    // currently running. New periodic runs can be simply ignored.
    if (instance_)
      return;

    // A periodic run will only start if no run is currently happening and
    // if no sequence have been triggered in the last
    // |kDaysBetweenSuccessfulSwReporterRuns| days.
    if (!IsUserInitiated(invocation_type) && !time_info.ShouldRun())
      return;

    // The reporter runner object manages its own lifetime and will delete
    // itself once all invocations finish.
    instance_ = new ReporterRunner(invocation_type, std::move(invocations),
                                   std::move(time_info));
    GetCleanerController()->OnReporterSequenceStarted();
    instance_->PostNextInvocation();

    // The reporter sequence has been scheduled to run, so don't notify that
    // it has not been scheduled.
    ignore_result(scoped_runner.Release());
  }

 private:
  // Keeps track of last and upcoming reporter runs and logs uploading.
  //
  // Periodic runs are allowed to start if the last time the reporter ran was
  // more than |kDaysBetweenSuccessfulSwReporterRuns| days ago. As a safety
  // measure for failure recovery, also allow to run if
  // |prefs::kSwReporterLastTimeTriggered| is set in the future. This is to
  // prevent from no longer running the reporter if a time in the future is
  // accidentally written.
  //
  // Logs uploading is allowed if logs have never been uploaded for this user,
  // or if logs have been sent at least |kSwReporterLastTimeSentReport| days
  // ago. As a safety measure for failure recovery, also allow sending logs if
  // the last upload time in local state is incorrectly set to the future.
  class ReporterRunTimeInfo {
   public:
    explicit ReporterRunTimeInfo(PrefService* local_state) {
      DCHECK(local_state);

      base::Time now = Now();
      if (local_state->HasPrefPath(prefs::kSwReporterLastTimeTriggered)) {
        base::Time last_time_triggered =
            base::Time() +
            base::TimeDelta::FromMicroseconds(
                local_state->GetInt64(prefs::kSwReporterLastTimeTriggered));
        base::Time next_trigger =
            last_time_triggered +
            base::TimeDelta::FromDays(kDaysBetweenSuccessfulSwReporterRuns);
        should_run_ = next_trigger <= now || last_time_triggered > now;
      } else {
        should_run_ = true;
      }

      if (local_state->HasPrefPath(prefs::kSwReporterLastTimeSentReport)) {
        base::Time last_time_sent_logs =
            base::Time() +
            base::TimeDelta::FromMicroseconds(
                local_state->GetInt64(prefs::kSwReporterLastTimeSentReport));
        base::Time next_time_send_logs =
            last_time_sent_logs +
            base::TimeDelta::FromDays(kDaysBetweenReporterLogsSent);
        in_logs_upload_period_ =
            next_time_send_logs <= now || last_time_sent_logs > now;
      } else {
        in_logs_upload_period_ = true;
      }
    }

    bool ShouldRun() const { return should_run_; }

    bool InLogsUploadPeriod() const { return in_logs_upload_period_; }

    bool should_run_ = false;
    bool in_logs_upload_period_ = false;
  };

  ReporterRunner(SwReporterInvocationType invocation_type,
                 SwReporterInvocationSequence&& invocations,
                 ReporterRunTimeInfo&& time_info)
      : invocation_type_(invocation_type),
        invocations_(std::move(invocations)),
        on_sequence_done_(
            base::BindOnce(&ChromeCleanerController::OnReporterSequenceDone,
                           base::Unretained(GetCleanerController()))),
        time_info_(std::move(time_info)) {}

  ~ReporterRunner() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    instance_ = nullptr;
  }

  // Launches the command line at the head of the queue.
  void PostNextInvocation() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    DCHECK(!invocations_.container().empty());
    SwReporterInvocation next_invocation = invocations_.container().front();
    invocations_.mutable_container().pop();

    AppendInvocationSpecificSwitches(&next_invocation);

    base::TaskRunner* task_runner =
        g_testing_delegate_ ? g_testing_delegate_->BlockingTaskRunner()
                            : blocking_task_runner_.get();
    auto launch_and_wait = base::Bind(&LaunchAndWaitForExit, next_invocation);
    auto reporter_done =
        base::Bind(&ReporterRunner::ReporterDone, base::Unretained(this), Now(),
                   next_invocation);
    base::PostTaskAndReplyWithResult(task_runner, FROM_HERE,
                                     std::move(launch_and_wait),
                                     std::move(reporter_done));
  }

  // This method is called on the UI thread when an invocation of the reporter
  // has completed. This is run as a task posted from an interruptible worker
  // thread so should be resilient to unexpected shutdown.
  void ReporterDone(const base::Time& reporter_start_time,
                    SwReporterInvocation finished_invocation,
                    int exit_code) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    // Ensures finalization if there are no further invocations to run. This
    // scoped runner may be released later on if there are other invocations to
    // start.
    base::ScopedClosureRunner scoped_runner(base::BindOnce(
        &ReporterRunner::SendResultAndDeleteSelf, base::Unretained(this),
        ExitCodeToInvocationResult(exit_code)));

    // Don't continue the current queue of reporters if one failed to launch.
    // If the reporter failed to launch, do not process the results. (The exit
    // code itself doesn't need to be logged in this case because
    // SW_REPORTER_FAILED_TO_START is logged in
    // |LaunchAndWaitForExit|.)
    if (exit_code == kReporterNotLaunchedExitCode) {
      NotifySequenceDone(SwReporterInvocationResult::kProcessFailedToLaunch);
      return;
    }

    base::Time now = Now();
    base::TimeDelta reporter_running_time = now - reporter_start_time;

    // Tries to run the next invocation in the queue.
    if (!invocations_.container().empty()) {
      // If there are other invocations to start, then we shouldn't finalize
      // this object. ScopedClosureRunner::Release requires its return value to
      // be used, so simply ignore_result it, since it will not be needed.
      ignore_result(scoped_runner.Release());
      PostNextInvocation();
    }

    UMAHistogramReporter uma(finished_invocation.suffix());
    uma.ReportVersion(invocations_.version());
    uma.ReportExitCode(exit_code);
    uma.ReportEngineErrorCode();
    uma.ReportFoundUwS();

    PrefService* local_state = g_browser_process->local_state();
    if (local_state) {
      if (finished_invocation.BehaviourIsSupported(
              SwReporterInvocation::BEHAVIOUR_LOG_EXIT_CODE_TO_PREFS)) {
        local_state->SetInteger(prefs::kSwReporterLastExitCode, exit_code);
      }
      local_state->SetInt64(prefs::kSwReporterLastTimeTriggered,
                            now.ToInternalValue());
    }
    uma.ReportRuntime(reporter_running_time);
    uma.ReportScanTimes();
    uma.ReportMemoryUsage();
    if (finished_invocation.reporter_logs_upload_enabled())
      uma.RecordLogsUploadResult();

    if (!finished_invocation.BehaviourIsSupported(
            SwReporterInvocation::BEHAVIOUR_TRIGGER_PROMPT)) {
      RecordPromptNotShownWithReasonHistogram(
          NO_PROMPT_REASON_BEHAVIOUR_NOT_SUPPORTED);
      return;
    }

    if (!IsUserInitiated(invocation_type_) && !IsSRTPromptFeatureEnabled()) {
      // Knowing about disabled field trial is more important than reporter not
      // finding anything to remove, so check this case first.
      RecordReporterStepHistogram(SW_REPORTER_NO_PROMPT_FIELD_TRIAL);
      RecordPromptNotShownWithReasonHistogram(
          NO_PROMPT_REASON_FEATURE_NOT_ENABLED);
      return;
    }

    // Do not accept reboot required or post-reboot exit codes, since they
    // should not be sent out by the reporter.
    if (exit_code != chrome_cleaner::kSwReporterCleanupNeeded) {
      RecordReporterStepHistogram(SW_REPORTER_NO_PROMPT_NEEDED);
      RecordPromptNotShownWithReasonHistogram(NO_PROMPT_REASON_NOTHING_FOUND);
      return;
    }

    ChromeCleanerController* cleaner_controller = GetCleanerController();

    // The most common state for the controller at this moment is
    // kReporterRunning, set before this invocation sequence started. However,
    // it's possible that the reporter starts running again during the prompt
    // routine (for example, a new reporter version became available while the
    // cleaner prompt is presented to the user). In that case, only prompt if
    // the controller has returned to the idle state.
    if (cleaner_controller->state() != ChromeCleanerController::State::kIdle &&
        cleaner_controller->state() !=
            ChromeCleanerController::State::kReporterRunning) {
      RecordPromptNotShownWithReasonHistogram(
          NO_PROMPT_REASON_NOT_ON_IDLE_STATE);
      return;
    }

    if (!IsUserInitiated(invocation_type_) &&
        !ShouldShowPromptForPeriodicRun()) {
      return;
    }

    finished_invocation.set_cleaner_logs_upload_enabled(
        invocation_type_ ==
        SwReporterInvocationType::kUserInitiatedWithLogsAllowed);

    finished_invocation.set_chrome_prompt(
        IsUserInitiated(invocation_type_)
            ? chrome_cleaner::ChromePromptValue::kUserInitiated
            : chrome_cleaner::ChromePromptValue::kPrompted);

    NotifySequenceDone(SwReporterInvocationResult::kCleanupToBeOffered);
    cleaner_controller->Scan(finished_invocation);

    // If this is a periodic reporter run, then create the dialog controller, so
    // that the user may eventually be prompted. Otherwise, all interaction
    // should be driven from the Settings page.
    if (!IsUserInitiated(invocation_type_))
      CreateChromeCleanerDialogController();
  }

  // Returns true if the experiment to send reporter logs is enabled, the user
  // opted into Safe Browsing extended reporting, and this queue of invocations
  // started during the logs upload interval.
  bool ShouldSendReporterLogs(const std::string& suffix) {
    UMAHistogramReporter uma(suffix);

    Browser* browser = chrome::FindLastActive();
    if (!browser) {
      return false;
    }

    Profile* profile = browser->profile();
    DCHECK(profile);
    PrefService* prefs = profile->GetPrefs();
    DCHECK(prefs);

    // The enterprise policy overrides all other choices.
    if (!SwReporterReportingIsAllowedByPolicy(profile)) {
      uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_DISABLED_BY_POLICY);
      return false;
    }

    switch (invocation_type_) {
      case SwReporterInvocationType::kUnspecified:
      case SwReporterInvocationType::kMax:
        NOTREACHED();
        return false;

      case SwReporterInvocationType::kUserInitiatedWithLogsDisallowed:
        uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_DISABLED_BY_USER);
        return false;

      case SwReporterInvocationType::kUserInitiatedWithLogsAllowed:
        uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_ENABLED_BY_USER);
        return true;

      case SwReporterInvocationType::kPeriodicRun:
        if (!SafeBrowsingExtendedReportingEnabled()) {
          uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_SBER_DISABLED);
          return false;
        }

        if (!time_info_.InLogsUploadPeriod()) {
          uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_RECENTLY_SENT_LOGS);
          return false;
        }

        uma.RecordLogsUploadEnabled(REPORTER_LOGS_UPLOADS_SBER_ENABLED);
        return true;
    }

    NOTREACHED();
    return false;
  }

  // Appends switches to the next invocation that depend on the user current
  // state with respect to opting into extended Safe Browsing reporting and
  // metrics and crash reporting.
  void AppendInvocationSpecificSwitches(SwReporterInvocation* invocation) {
    // Add switches for users who opted into extended Safe Browsing reporting.
    PrefService* local_state = g_browser_process->local_state();
    if (local_state && ShouldSendReporterLogs(invocation->suffix())) {
      invocation->set_reporter_logs_upload_enabled(true);
      AddSwitchesForExtendedReportingUser(invocation);
      // Set the local state value before the first attempt to run the
      // reporter, because we only want to upload logs once in the window
      // defined by |kDaysBetweenReporterLogsSent|. If we set with other local
      // state values after the reporter runs, we could send logs again too
      // quickly (for example, if Chrome stops before the reporter finishes).
      local_state->SetInt64(prefs::kSwReporterLastTimeSentReport,
                            Now().ToInternalValue());
    } else if (invocation->reporter_logs_upload_enabled()) {
      // Security measure in case this has been changed to true somewhere else,
      // so that reporter logs are not accidentally uploaded without user
      // consent.
      NOTREACHED();
      invocation->set_reporter_logs_upload_enabled(false);
    }

    if (ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled()) {
      invocation->mutable_command_line().AppendSwitch(
          chrome_cleaner::kEnableCrashReportingSwitch);
    }

    const std::string group_name = GetSRTPromptGroupName();
    if (!group_name.empty()) {
      invocation->mutable_command_line().AppendSwitchASCII(
          chrome_cleaner::kSRTPromptFieldTrialGroupNameSwitch, group_name);
    }
  }

  // Adds switches to be sent to the Software Reporter when the user opted into
  // extended Safe Browsing reporting and is not incognito.
  void AddSwitchesForExtendedReportingUser(SwReporterInvocation* invocation) {
    invocation->mutable_command_line().AppendSwitch(
        chrome_cleaner::kExtendedSafeBrowsingEnabledSwitch);
    invocation->mutable_command_line().AppendSwitchASCII(
        chrome_cleaner::kChromeVersionSwitch, version_info::GetVersionNumber());
    invocation->mutable_command_line().AppendSwitchNative(
        chrome_cleaner::kChromeChannelSwitch,
        base::NumberToString16(ChannelAsInt()));
  }

  void SendResultAndDeleteSelf(SwReporterInvocationResult result) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK_EQ(instance_, this);

    NotifySequenceDone(result);

    delete this;
  }

  void NotifySequenceDone(SwReporterInvocationResult result) {
    if (on_sequence_done_)
      std::move(on_sequence_done_).Run(result);
  }

  SwReporterInvocationType invocation_type() const { return invocation_type_; }

  // Not null if there is a sequence currently running.
  static ReporterRunner* instance_;

  scoped_refptr<base::TaskRunner> blocking_task_runner_ =
      base::CreateTaskRunner(
          // LaunchAndWaitForExit creates (MayBlock()) and joins
          // (WithBaseSyncPrimitives()) a process.
          {base::ThreadPool(), base::MayBlock(), base::WithBaseSyncPrimitives(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  SwReporterInvocationType invocation_type_;

  // The queue of invocations that are currently running.
  SwReporterInvocationSequence invocations_;

  // Invoked once when the |invocations_| sequence run finishes or when it's
  // aborted with an error, whichever comes first.
  base::OnceCallback<void(SwReporterInvocationResult result)> on_sequence_done_;

  // Last and upcoming reporter runs and logs uploading.
  ReporterRunTimeInfo time_info_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ReporterRunner);
};

// static
ReporterRunner* ReporterRunner::instance_ = nullptr;

void SetSwReporterTestingDelegate(SwReporterTestingDelegate* delegate) {
  g_testing_delegate_ = delegate;
}

bool ReporterTerminatesOnBrowserExit() {
  // Windows 7 does not allow nested job objects, and the process may already
  // be in a job (for example when running under a debugger or in Terminal
  // Server) so only enable this on Windows 8+. The reporter will finish its
  // scan and upload reports if the user has opted in, but not be able to
  // prompt for cleanup if UwS is found.
  return base::win::GetVersion() >= base::win::Version::WIN8;
}

// This function is called from a worker thread to launch the SwReporter and
// wait for termination to collect its exit code. This task could be
// interrupted by a shutdown at any time, so it shouldn't depend on anything
// external that could be shut down beforehand.
int LaunchAndWaitForExit(const SwReporterInvocation& invocation) {
  TRACE_EVENT0("safe_browsing", "ReporterRunner::LaunchAndWaitForExit");

  // This exit code is used to identify that a reporter run didn't happen, so
  // the result should be ignored and a rerun scheduled for the usual delay.
  int exit_code = kReporterNotLaunchedExitCode;

  UMAHistogramReporter uma(invocation.suffix());

  base::FilePath tmpdir;
  if (!base::GetTempDir(&tmpdir)) {
    uma.RecordReporterStep(SW_REPORTER_FAILED_TO_START);
    return exit_code;
  }

  // The reporter runs from the system tmp directory. This is to avoid
  // unnecessarily holding on to the installation directory while running as it
  // prevents uninstallation of chrome.
  base::LaunchOptions launch_options;
  launch_options.current_directory = tmpdir;

  // Assign the reporter process to a job. If the browser exits before the
  // reporter, the OS will close the job handle and the reporter process.
  base::win::ScopedHandle job;
  if (ReporterTerminatesOnBrowserExit()) {
    job.Set(::CreateJobObject(nullptr, nullptr));
  }
  if (job.IsValid()) {
    base::SetJobObjectLimitFlags(job.Get(), JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE);
    launch_options.job_handle = job.Get();
  } else {
    PLOG(WARNING) << "The Chrome Cleanup Tool's reporter process is not "
                     "attached to a job and may outlive the browser.";
  }

  base::Process reporter_process =
      g_testing_delegate_
          ? g_testing_delegate_->LaunchReporterProcess(invocation,
                                                       launch_options)
          : base::LaunchProcess(invocation.command_line(), launch_options);

  if (!reporter_process.IsValid()) {
    uma.RecordReporterStep(SW_REPORTER_FAILED_TO_START);
    return exit_code;
  }

  uma.RecordReporterStep(SW_REPORTER_START_EXECUTION);

  if (g_testing_delegate_) {
    exit_code = g_testing_delegate_->WaitForReporterExit(reporter_process);
  } else {
    bool success = reporter_process.WaitForExit(&exit_code);
    DCHECK(success);
  }

  // After the reporter process has exited the job object is no longer needed.
  // It will be closed when it goes out of scope here.
  return exit_code;
}

}  // namespace internal

bool IsUserInitiated(SwReporterInvocationType invocation_type) {
  return invocation_type ==
             SwReporterInvocationType::kUserInitiatedWithLogsAllowed ||
         invocation_type ==
             SwReporterInvocationType::kUserInitiatedWithLogsDisallowed;
}

void MaybeStartSwReporter(SwReporterInvocationType invocation_type,
                          SwReporterInvocationSequence&& invocations) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!invocations.container().empty());

  internal::ReporterRunner::MaybeStartInvocations(invocation_type,
                                                  std::move(invocations));
}

bool SwReporterIsAllowedByPolicy() {
  static auto is_allowed = []() {
    PrefService* local_state = g_browser_process->local_state();
    return !local_state ||
           !local_state->IsManagedPreference(prefs::kSwReporterEnabled) ||
           local_state->GetBoolean(prefs::kSwReporterEnabled);
  }();
  return is_allowed;
}

bool SwReporterReportingIsAllowedByPolicy(Profile* profile) {
  // Reporting is allowed when cleanup is enabled by policy and when the
  // specific reporting policy is allowed.  While the former policy is not
  // dynamic, the latter one is.
  bool is_allowed = SwReporterIsAllowedByPolicy();
  if (is_allowed) {
    PrefService* profile_prefs = profile->GetPrefs();
    DCHECK(profile_prefs);

    is_allowed = !profile_prefs ||
                 !profile_prefs->IsManagedPreference(
                     prefs::kSwReporterReportingEnabled) ||
                 profile_prefs->GetBoolean(prefs::kSwReporterReportingEnabled);
  }
  return is_allowed;
}

}  // namespace safe_browsing
