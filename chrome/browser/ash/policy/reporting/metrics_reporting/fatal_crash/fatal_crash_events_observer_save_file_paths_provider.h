// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_SAVE_FILE_PATHS_PROVIDER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_SAVE_FILE_PATHS_PROVIDER_H_

#include "chrome/browser/ash/policy/reporting/metrics_reporting/fatal_crash/fatal_crash_events_observer.h"

#include "base/files/file_path.h"

namespace reporting {

class FatalCrashEventsObserver::SaveFilePathsProviderInterface {
 public:
  // Gets the save file path for saving reported local IDs.
  virtual base::FilePath GetReportedLocalIdSaveFilePath() const = 0;

  // Gets the save file path for saving uploaded crash info.
  virtual base::FilePath GetUploadedCrashInfoSaveFilePath() const = 0;

 protected:
  // The `SaveFilePathsProviderInterface` singleton. Use a raw pointer
  // instead of `unique_ptr` or `raw_ptr` here, because we must avoid
  // destructing it at exit time. `unique_ptr` would trigger the error of
  // "declaration requires an exit-time destructor".
  static const SaveFilePathsProviderInterface*
      g_default_save_file_paths_provider_;
};

class FatalCrashEventsObserver::DefaultSaveFilePathsProvider final
    : public FatalCrashEventsObserver::SaveFilePathsProviderInterface {
 public:
  static const SaveFilePathsProviderInterface& Get();
  DefaultSaveFilePathsProvider(const DefaultSaveFilePathsProvider&) = delete;
  DefaultSaveFilePathsProvider& operator=(const DefaultSaveFilePathsProvider&) =
      delete;
  ~DefaultSaveFilePathsProvider();

 private:
  DefaultSaveFilePathsProvider();

  // SaveFilePathsProviderInterface:
  base::FilePath GetReportedLocalIdSaveFilePath() const override;
  base::FilePath GetUploadedCrashInfoSaveFilePath() const override;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_FATAL_CRASH_FATAL_CRASH_EVENTS_OBSERVER_SAVE_FILE_PATHS_PROVIDER_H_
