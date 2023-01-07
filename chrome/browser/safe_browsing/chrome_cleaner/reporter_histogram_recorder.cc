// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_histogram_recorder.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/strcat_win.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace safe_browsing {

namespace {

// Used to send UMA information about missing logs upload result in the registry
// for the reporter. Matches SoftwareReporterLogsUploadResultRegistryError in
// enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LogsUploadResultRegistryError {
  NO_ERROR = 0,
  REGISTRY_KEY_INVALID = 1,
  VALUE_NOT_FOUND = 2,
  VALUE_OUT_OF_BOUNDS = 3,
  kMaxValue = VALUE_OUT_OF_BOUNDS,
};

// Network errors encountered by the reporter when uploading logs, written by
// the reporter to the registry. Matches SoftwareReporterLogsUploadResult in
// enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class LogsUploadResult {
  kSuccess = 0,
  kRequestFailed = 1,
  kInvalidResponse = 2,
  kTimedOut = 3,
  kInternalError = 4,
  kReportTooLarge = 5,
  kNoNetwork = 6,
  kMaxValue = kNoNetwork,
};

const char kFoundUwsMetricName[] = "SoftwareReporter.FoundUwS";
const char kFoundUwsReadErrorMetricName[] =
    "SoftwareReporter.FoundUwSReadError";
const char kMemoryUsedMetricName[] = "SoftwareReporter.MemoryUsed";
const char kLogsUploadEnabledMetricName[] =
    "SoftwareReporter.LogsUploadEnabled";
const char kLogsUploadResultMetricName[] = "SoftwareReporter.LogsUploadResult";
const char kLogsUploadResultRegistryErrorMetricName[] =
    "SoftwareReporter.LogsUploadResultRegistryError";
const char kExitCodeMetricName[] = "SoftwareReporter.ExitCodeFromRegistry";
const char kEngineErrorCodeMetricName[] = "SoftwareReporter.EngineErrorCode";

}  // namespace

ReporterHistogramRecorder::ReporterHistogramRecorder(const std::string& suffix)
    : suffix_(suffix),
      registry_key_(
          suffix.empty()
              ? chrome_cleaner::kSoftwareRemovalToolRegistryKey
              : base::StrCat({chrome_cleaner::kSoftwareRemovalToolRegistryKey,
                              L"\\", base::UTF8ToWide(suffix)})) {}

void ReporterHistogramRecorder::RecordVersion(
    const base::Version& version) const {
  DCHECK(!version.components().empty());
  // The minor version is the 2nd last component of the version,
  // or just the first component if there is only 1.
  uint32_t minor_version = 0;
  if (version.components().size() > 1)
    minor_version = version.components()[version.components().size() - 2];
  else
    minor_version = version.components()[0];
  base::UmaHistogramSparse(FullName("SoftwareReporter.MinorVersion"),
                           minor_version);

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
  base::UmaHistogramSparse(FullName("SoftwareReporter.MajorVersion"),
                           major_version);
}

void ReporterHistogramRecorder::RecordExitCode(int exit_code) const {
  // Also report the exit code that the reporter writes to the registry.
  base::win::RegKey reporter_key;
  DWORD exit_code_in_registry;
  if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                        KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
      reporter_key.ReadValueDW(chrome_cleaner::kExitCodeValueName,
                               &exit_code_in_registry) != ERROR_SUCCESS) {
    return;
  }

  base::UmaHistogramSparse(FullName(kExitCodeMetricName),
                           exit_code_in_registry);
  reporter_key.DeleteValue(chrome_cleaner::kExitCodeValueName);
}

void ReporterHistogramRecorder::RecordEngineErrorCode() const {
  base::win::RegKey reporter_key;
  DWORD engine_error_code;
  if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                        KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
      reporter_key.ReadValueDW(chrome_cleaner::kEngineErrorCodeValueName,
                               &engine_error_code) != ERROR_SUCCESS) {
    return;
  }

  base::UmaHistogramSparse(FullName(kEngineErrorCodeMetricName),
                           engine_error_code);
  reporter_key.DeleteValue(chrome_cleaner::kEngineErrorCodeValueName);
}

void ReporterHistogramRecorder::RecordFoundUwS() const {
  base::win::RegKey reporter_key;
  std::vector<std::wstring> found_uws_strings;
  if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                        KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
      reporter_key.ReadValues(chrome_cleaner::kFoundUwsValueName,
                              &found_uws_strings) != ERROR_SUCCESS) {
    return;
  }

  bool parse_error = false;
  for (const auto& uws_string : found_uws_strings) {
    // All UwS ids are expected to be integers.
    uint32_t uws_id = 0;
    if (base::StringToUint(uws_string, &uws_id)) {
      base::UmaHistogramSparse(FullName(kFoundUwsMetricName), uws_id);
    } else {
      parse_error = true;
    }
  }

  // Clean up the old value.
  reporter_key.DeleteValue(chrome_cleaner::kFoundUwsValueName);
  base::UmaHistogramBoolean(FullName(kFoundUwsReadErrorMetricName),
                            parse_error);
}

void ReporterHistogramRecorder::RecordMemoryUsage() const {
  base::win::RegKey reporter_key;
  DWORD memory_used = 0;
  if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                        KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS ||
      reporter_key.ReadValueDW(chrome_cleaner::kMemoryUsedValueName,
                               &memory_used) != ERROR_SUCCESS) {
    return;
  }
  base::UmaHistogramMemoryKB(FullName(kMemoryUsedMetricName), memory_used);
  reporter_key.DeleteValue(chrome_cleaner::kMemoryUsedValueName);
}

void ReporterHistogramRecorder::RecordRuntime(
    const base::TimeDelta& reporter_running_time,
    const base::TimeDelta& running_time_without_sleep) const {
  base::UmaHistogramLongTimes(
      FullName("SoftwareReporter.RunningTimeAccordingToChrome2"),
      reporter_running_time);
  base::UmaHistogramLongTimes(
      FullName("SoftwareReporter.RunningTimeWithoutSleep2"),
      running_time_without_sleep);

  // TODO(b/641081): This should only have KEY_QUERY_VALUE and KEY_SET_VALUE.
  base::win::RegKey reporter_key;
  if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                        KEY_ALL_ACCESS) != ERROR_SUCCESS) {
    return;
  }

  // Clean up obsolete registry values, if necessary.
  reporter_key.DeleteValue(chrome_cleaner::kStartTimeValueName);
  reporter_key.DeleteValue(chrome_cleaner::kEndTimeValueName);
}

void ReporterHistogramRecorder::RecordLogsUploadEnabled(
    SwReporterLogsUploadsEnabled value) const {
  base::UmaHistogramEnumeration(FullName(kLogsUploadEnabledMetricName), value);
}

void ReporterHistogramRecorder::RecordLogsUploadResult() const {
  base::win::RegKey reporter_key;
  DWORD logs_upload_result = 0;
  if (reporter_key.Open(HKEY_CURRENT_USER, registry_key_.c_str(),
                        KEY_QUERY_VALUE | KEY_SET_VALUE) != ERROR_SUCCESS) {
    base::UmaHistogramEnumeration(
        FullName(kLogsUploadResultRegistryErrorMetricName),
        LogsUploadResultRegistryError::REGISTRY_KEY_INVALID);
    return;
  }

  if (reporter_key.ReadValueDW(chrome_cleaner::kLogsUploadResultValueName,
                               &logs_upload_result) != ERROR_SUCCESS) {
    base::UmaHistogramEnumeration(
        FullName(kLogsUploadResultRegistryErrorMetricName),
        LogsUploadResultRegistryError::VALUE_NOT_FOUND);
    return;
  }

  if (logs_upload_result < 0 ||
      logs_upload_result >= static_cast<DWORD>(LogsUploadResult::kMaxValue)) {
    base::UmaHistogramEnumeration(
        FullName(kLogsUploadResultRegistryErrorMetricName),
        LogsUploadResultRegistryError::VALUE_OUT_OF_BOUNDS);
    return;
  }

  base::UmaHistogramEnumeration(
      FullName(kLogsUploadResultMetricName),
      static_cast<LogsUploadResult>(logs_upload_result));
  reporter_key.DeleteValue(chrome_cleaner::kLogsUploadResultValueName);
  base::UmaHistogramEnumeration(
      FullName(kLogsUploadResultRegistryErrorMetricName),
      LogsUploadResultRegistryError::NO_ERROR);
}

std::string ReporterHistogramRecorder::FullName(const std::string& name) const {
  if (suffix_.empty())
    return name;
  return base::StrCat({name, "_", suffix_});
}

}  // namespace safe_browsing
