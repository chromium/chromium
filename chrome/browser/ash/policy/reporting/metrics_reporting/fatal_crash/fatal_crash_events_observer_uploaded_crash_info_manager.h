// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_UPLOADED_CRASH_INFO_MANAGER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_UPLOADED_CRASH_INFO_MANAGER_H_

#include <memory>
#include <string_view>

#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"
#include "chromeos/ash/services/cros_healthd/public/mojom/cros_healthd_events.mojom.h"
#include "components/reporting/util/status.h"

namespace reporting {
class FatalCrashEventsObserver::UploadedCrashInfoManager {
 public:
  static std::unique_ptr<UploadedCrashInfoManager> Create(
      base::FilePath save_file_path);
  UploadedCrashInfoManager(const UploadedCrashInfoManager&) = delete;
  UploadedCrashInfoManager& operator=(const UploadedCrashInfoManager&) = delete;
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
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_UPLOADED_CRASH_INFO_MANAGER_H_
