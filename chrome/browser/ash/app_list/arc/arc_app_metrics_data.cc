// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_metrics_data.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

constexpr int kExclusiveMaxNumBuckets = 51;

}  // namespace

namespace arc {

ArcAppMetricsData::ArcAppMetricsData(const std::string& histogram_base)
    : histogram_base_(histogram_base) {}

ArcAppMetricsData::~ArcAppMetricsData() = default;

void ArcAppMetricsData::recordAppInstallStartTime(const std::string& app_name) {
  install_start_time_map_[app_name] = base::TimeTicks::Now();
  num_requests_++;
}

void ArcAppMetricsData::maybeReportInstallTimeDelta(
    const std::string& app_name) {
  if (install_start_time_map_.count(app_name) == 0) {
    return;
  }

  const base::TimeDelta time_delta =
      base::TimeTicks::Now() - install_start_time_map_[app_name];
  base::UmaHistogramLongTimes(base::StrCat({histogram_base_, "TimeDelta"}),
                              time_delta);
  install_start_time_map_.erase(app_name);
}

void ArcAppMetricsData::reportMetrics() {
  if (num_requests_ > 0) {
    base::UmaHistogramExactLinear(
        base::StrCat({histogram_base_, "NumAppsIncomplete"}),
        install_start_time_map_.size(), kExclusiveMaxNumBuckets);
  }

  base::UmaHistogramExactLinear(
      base::StrCat({histogram_base_, "NumAppsRequested"}), num_requests_,
      kExclusiveMaxNumBuckets);
}

}  // namespace arc
