// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/safe_browsing/chrome_cleaner/reporter_histogram_recorder.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions_win.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "components/chrome_cleaner/public/constants/constants.h"

namespace safe_browsing {

namespace {

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

// The max value for histogram SoftwareReporter.LogsUploadResult, which is used
// to send UMA information about the result of Software Reporter's attempt to
// upload logs, when logs are enabled. This value must be consistent with the
// SoftwareReporterLogsUploadResult enum defined in the histograms.xml file.
const int kSwReporterLogsUploadResultMax = 30;

// TODO(crbug.com/1335637): Should use the UmaHistogram functions instead
// of relying on this histogram implementation detail.
constexpr base::HistogramBase::Flags kUmaHistogramFlag =
    base::HistogramBase::kUmaTargetedHistogramFlag;

}  // namespace

ReporterHistogramRecorder::ReporterHistogramRecorder(const std::string& suffix)
    : suffix_(suffix),
      registry_key_(suffix.empty()
                        ? chrome_cleaner::kSoftwareRemovalToolRegistryKey
                        : base::StringPrintf(
                              L"%ls\\%ls",
                              chrome_cleaner::kSoftwareRemovalToolRegistryKey,
                              base::UTF8ToUTF16(suffix).c_str())) {}

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

  RecordSparseHistogram(kExitCodeMetricName, exit_code_in_registry);
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

  RecordSparseHistogram(kEngineErrorCodeMetricName, engine_error_code);
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
      RecordSparseHistogram(kFoundUwsMetricName, uws_id);
    } else {
      parse_error = true;
    }
  }

  // Clean up the old value.
  reporter_key.DeleteValue(chrome_cleaner::kFoundUwsValueName);
  RecordBooleanHistogram(kFoundUwsReadErrorMetricName, parse_error);
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
  RecordMemoryKBHistogram(kMemoryUsedMetricName, memory_used);
  reporter_key.DeleteValue(chrome_cleaner::kMemoryUsedValueName);
}

void ReporterHistogramRecorder::RecordRuntime(
    const base::TimeDelta& reporter_running_time,
    const base::TimeDelta& running_time_without_sleep) const {
  RecordLongTimesHistogram("SoftwareReporter.RunningTimeAccordingToChrome2",
                           reporter_running_time);
  RecordLongTimesHistogram("SoftwareReporter.RunningTimeWithoutSleep2",
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
  RecordEnumerationHistogram(kLogsUploadEnabledMetricName, value,
                             REPORTER_LOGS_UPLOADS_MAX);
}

void ReporterHistogramRecorder::RecordLogsUploadResult() const {
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

void ReporterHistogramRecorder::RecordCreateJobResult(DWORD result) const {
  RecordSparseHistogram("SoftwareReporter.CreateJobResult", result);
}

std::string ReporterHistogramRecorder::FullName(const std::string& name) const {
  if (suffix_.empty())
    return name;
  return base::StringPrintf("%s_%s", name.c_str(), suffix_.c_str());
}

void ReporterHistogramRecorder::RecordBooleanHistogram(const std::string& name,
                                                       bool sample) const {
  auto* histogram =
      base::BooleanHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
  if (histogram)
    histogram->AddBoolean(sample);
}

void ReporterHistogramRecorder::RecordEnumerationHistogram(
    const std::string& name,
    Sample sample,
    Sample boundary) const {
  // See HISTOGRAM_ENUMERATION_WITH_FLAG for the parameters to |FactoryGet|.
  auto* histogram = base::LinearHistogram::FactoryGet(
      FullName(name), 1, boundary, boundary + 1, kUmaHistogramFlag);
  if (histogram)
    histogram->Add(sample);
}

void ReporterHistogramRecorder::RecordLongTimesHistogram(
    const std::string& name,
    const base::TimeDelta& sample) const {
  // See UMA_HISTOGRAM_LONG_TIMES for the parameters to |FactoryTimeGet|.
  auto* histogram =
      base::Histogram::FactoryTimeGet(FullName(name), base::Milliseconds(1),
                                      base::Hours(1), 100, kUmaHistogramFlag);
  if (histogram)
    histogram->AddTime(sample);
}

void ReporterHistogramRecorder::RecordMemoryKBHistogram(const std::string& name,
                                                        Sample sample) const {
  // See UMA_HISTOGRAM_MEMORY_KB for the parameters to |FactoryGet|.
  auto* histogram = base::Histogram::FactoryGet(FullName(name), 1000, 500000,
                                                50, kUmaHistogramFlag);
  if (histogram)
    histogram->Add(sample);
}

void ReporterHistogramRecorder::RecordSparseHistogram(const std::string& name,
                                                      Sample sample) const {
  auto* histogram =
      base::SparseHistogram::FactoryGet(FullName(name), kUmaHistogramFlag);
  if (histogram)
    histogram->Add(sample);
}

}  // namespace safe_browsing
