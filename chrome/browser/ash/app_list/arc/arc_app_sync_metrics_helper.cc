// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_sync_metrics_helper.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

constexpr char kHistogramNameBase[] = "Arc.AppSync.InitialSession.";
constexpr int kAppCountUmaExclusiveMax = 101;
constexpr int kUmaNumBuckets = 50;

}  // namespace

namespace arc {

ArcAppSyncMetricsHelper::ArcAppSyncMetricsHelper() = default;

ArcAppSyncMetricsHelper::~ArcAppSyncMetricsHelper() = default;

void ArcAppSyncMetricsHelper::SetTimeSyncStarted() {
  time_sync_started_ = base::TimeTicks::Now();
  time_last_install_finished_ = time_sync_started_;
}

void ArcAppSyncMetricsHelper::OnAppInstalled(
    std::optional<uint64_t> app_size_in_bytes) {
  time_last_install_finished_ = base::TimeTicks::Now();
  num_installed_apps_++;

  if (app_size_in_bytes.has_value()) {
    base::UmaHistogramCustomCounts(
        base::StrCat({kHistogramNameBase, "InstalledAppSize"}),
        /*convert to MB*/ app_size_in_bytes.value() / (1000 * 1000), /*min=*/1,
        /*exclusive_max=4GB*/ 4 * 1000, kUmaNumBuckets);
  }
}

void ArcAppSyncMetricsHelper::SetAndRecordNumExpectedApps(
    uint64_t num_expected_apps) {
  num_expected_apps_ = num_expected_apps;
  base::UmaHistogramExactLinear(
      base::StrCat({kHistogramNameBase, "NumAppsExpected"}), num_expected_apps_,
      kAppCountUmaExclusiveMax);
}

void ArcAppSyncMetricsHelper::RecordMetrics() {
  if (time_sync_started_ != time_last_install_finished_) {
    const base::TimeDelta latency =
        time_last_install_finished_ - time_sync_started_;
    // Min is set to 30s since app installs typically take longer and an
    // underflow bucket will be created
    base::UmaHistogramCustomCounts(
        base::StrCat({kHistogramNameBase, "Latency"}), latency.InSeconds(),
        /*min=*/30, /*exclusive_max=*/base::Hours(3).InSeconds(),
        kUmaNumBuckets);
  }

  base::UmaHistogramExactLinear(
      base::StrCat({kHistogramNameBase, "NumAppsInstalled"}),
      num_installed_apps_, kAppCountUmaExclusiveMax);
  base::UmaHistogramExactLinear(
      base::StrCat({kHistogramNameBase, "NumAppsNotInstalled"}),
      num_expected_apps_ - num_installed_apps_, kAppCountUmaExclusiveMax);
}

}  // namespace arc
