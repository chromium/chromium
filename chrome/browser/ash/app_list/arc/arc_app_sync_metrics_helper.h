// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_SYNC_METRICS_HELPER_H_
#define CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_SYNC_METRICS_HELPER_H_

#include <optional>

#include "base/time/time.h"

namespace arc {

// Handles metrics for app sync.
class ArcAppSyncMetricsHelper {
 public:
  ArcAppSyncMetricsHelper();
  ~ArcAppSyncMetricsHelper();
  ArcAppSyncMetricsHelper(const ArcAppSyncMetricsHelper& other) = delete;
  ArcAppSyncMetricsHelper& operator=(const ArcAppSyncMetricsHelper&) = delete;

  // Sets `time_sync_started_` to current time.
  void SetTimeSyncStarted();

  // When an app is installed, count of installed apps is incremented,
  // the current time is saved, and app size is recorded in UMA.
  void OnAppInstalled(std::optional<uint64_t> app_size_in_bytes);

  // Sets `num_expected_apps_` and records the count in UMA.
  void SetAndRecordNumExpectedApps(uint64_t num_expected_apps);

  // Records the remaining metrics in UMA.
  void RecordMetrics();

 private:
  base::TimeTicks time_sync_started_;
  base::TimeTicks time_last_install_finished_;
  uint64_t num_installed_apps_ = 0;
  uint64_t num_expected_apps_ = 0;
};

}  // namespace arc

#endif  // CHROME_BROWSER_ASH_APP_LIST_ARC_ARC_APP_SYNC_METRICS_HELPER_H_
