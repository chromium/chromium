// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_TEST_UTIL_H_

#include <memory>
#include <string_view>

#include "base/files/file_path.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

namespace reporting {

class FatalCrashEventsObserver::TestEnvironment {
 public:
  static constexpr size_t kMaxNumOfLocalIds{
      ReportedLocalIdManager::kMaxNumOfLocalIds};
  static constexpr std::string_view kCreationTimestampMsJsonKey{
      UploadedCrashInfoManager::kCreationTimestampMsJsonKey};
  static constexpr std::string_view kOffsetJsonKey{
      UploadedCrashInfoManager::kOffsetJsonKey};

  TestEnvironment();
  TestEnvironment(const TestEnvironment&) = delete;
  TestEnvironment& operator=(const TestEnvironment&) = delete;
  ~TestEnvironment();

  // Gets the path to the save file that contains reported local IDs.
  const base::FilePath& GetReportedLocalIdSaveFilePath() const;

  // Gets the path to the save file that contains uploaded crash info.
  const base::FilePath& GetUploadedCrashInfoSaveFilePath() const;

  // Creates a `FatalCrashEventsObserver` object that uses `save_file_path_` as
  // the save file and returns the pointer.
  std::unique_ptr<FatalCrashEventsObserver> CreateFatalCrashEventsObserver()
      const;

  // Sets whether to continue postprocessing after event observed callback is
  // called.
  static void SetInterruptedAfterEventObserved(
      FatalCrashEventsObserver& observer,
      bool interrupted_after_event_observed);

 private:
  base::FilePath temp_dir_{base::CreateUniqueTempDirectoryScopedToTest()};
  base::FilePath reported_local_id_save_file_path_{
      temp_dir_.Append("REPORTED_LOCAL_IDS")};
  base::FilePath uploaded_crash_info_save_file_path_{
      temp_dir_.Append("UPLOADED_CRASH_INFO")};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_TEST_UTIL_H_
