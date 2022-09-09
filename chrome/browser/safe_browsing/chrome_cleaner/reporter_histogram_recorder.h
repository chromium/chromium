// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_HISTOGRAM_RECORDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_HISTOGRAM_RECORDER_H_

#include <string>

#include "base/time/time.h"
#include "base/version.h"
#include "base/win/windows_types.h"

namespace safe_browsing {

// Used to send UMA information showing whether uploading of Software Reporter
// logs is enabled, or the reason why not. Matches
// SoftwareReporterLogsUploadEnabled in enums.xml.
//
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SwReporterLogsUploadsEnabled {
  SBER_ENABLED = 0,
  SBER_DISABLED = 1,
  RECENTLY_SENT_LOGS = 2,
  DISABLED_BY_USER = 3,
  ENABLED_BY_USER = 4,
  DISABLED_BY_POLICY = 5,
  kMaxValue = DISABLED_BY_POLICY,
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
  // Returns `name` with an optional suffix added.
  std::string FullName(const std::string& name) const;

  const std::string suffix_;
  const std::wstring registry_key_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_REPORTER_HISTOGRAM_RECORDER_H_
