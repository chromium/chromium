// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_TEST_UTIL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/test/test_file_util.h"
#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

namespace reporting {
class FatalCrashEventsObserver::TestEnvironment {
 public:
  TestEnvironment();
  TestEnvironment(const TestEnvironment&) = delete;
  TestEnvironment& operator=(const TestEnvironment&) = delete;
  ~TestEnvironment();

  // Gets the path to the save file.
  const base::FilePath& GetSaveFilePath() const;

  // Creates a `FatalCrashEventsObserver` object that uses `save_file_path_` as
  // the save file and returns the pointer.
  std::unique_ptr<FatalCrashEventsObserver> CreateFatalCrashEventsObserver()
      const;

  // Sets whether to continue postprocessing after event observed callback is
  // called.
  static void SetInterruptedAfterEventObserved(
      FatalCrashEventsObserver& observer,
      bool interrupted_after_event_observed);

  static constexpr size_t kMaxNumOfLocalIds{
      ReportedLocalIdManager::kMaxNumOfLocalIds};

 private:
  base::FilePath save_file_path_{
      base::CreateUniqueTempDirectoryScopedToTest().Append(
          "REPORTED_LOCAL_IDS")};
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_TEST_UTIL_H_
