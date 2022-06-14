// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_HISTOGRAM_RECORDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_HISTOGRAM_RECORDER_H_

#include <string>

#include "base/metrics/histogram_base.h"
#include "base/time/time.h"
#include "base/version.h"
#include "base/win/windows_types.h"

namespace safe_browsing {

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

// Records metrics about the software reporter to UMA histograms.
class ReporterHistogramRecorder {
 public:
  explicit ReporterHistogramRecorder(const std::string& suffix);

  ~ReporterHistogramRecorder() = default;

  ReporterHistogramRecorder(const ReporterHistogramRecorder&) = delete;
  ReporterHistogramRecorder& operator=(const ReporterHistogramRecorder&) =
      delete;

  // Records the software reporter tool's version.
  void RecordVersion(const base::Version& version) const;

  // Records the exit code of the software reporter tool process.
  void RecordExitCode(int exit_code) const;

  // Records internal error codes from the reporter engine recorded to the
  // Windows registry.
  void RecordEngineErrorCode() const;

  // Records UwS found by the software reporter tool.
  void RecordFoundUwS() const;

  // Records the memory usage of the software reporter tool as reported
  // by the tool itself in the Windows registry.
  void RecordMemoryUsage() const;

  // Records the SwReporter run time, both as reported by the tool via
  // the registry and as measured by ReporterRunner.
  void RecordRuntime(const base::TimeDelta& reporter_running_time,
                     const base::TimeDelta& running_time_without_sleep) const;

  // Records whether logs uploads were enabled when the
  // software reporter tool ran.
  void RecordLogsUploadEnabled(SwReporterLogsUploadsEnabled value) const;

  // Records the result of a logs upload attempt as recorded by the software
  // reporter tool in the Windows registry.
  void RecordLogsUploadResult() const;

  // Records the result of the CreateJob call when launching the software
  // reporter tool. If CreateJob fails, the tool will not be killed if it's
  // still running when Chrome exits.
  void RecordCreateJobResult(DWORD result) const;

 private:
  // TODO(crbug.com/1335637): Should use the UmaHistogram functions instead
  // of relying on this histogram implementation detail.
  using Sample = base::HistogramBase::Sample;

  // Helper functions to record histograms with an optional suffix added to the
  // histogram name. The UMA_HISTOGRAM macros can't be used because they
  // require a constant string.

  std::string FullName(const std::string& name) const;

  void RecordBooleanHistogram(const std::string& name, bool sample) const;

  void RecordEnumerationHistogram(const std::string& name,
                                  Sample sample,
                                  Sample boundary) const;

  void RecordLongTimesHistogram(const std::string& name,
                                const base::TimeDelta& sample) const;

  void RecordMemoryKBHistogram(const std::string& name, Sample sample) const;

  void RecordSparseHistogram(const std::string& name, Sample sample) const;

  const std::string suffix_;
  const std::wstring registry_key_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_HISTOGRAM_RECORDER_H_
