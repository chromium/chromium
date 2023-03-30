// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"

namespace {

constexpr char kManualInstallHistogramBase[] =
    "Arc.AppInstall.Manual.InitialSession.";

}  // namespace

namespace arc {

ArcAppMetricsUtil::ArcAppMetricsUtil() = default;

ArcAppMetricsUtil::~ArcAppMetricsUtil() = default;

void ArcAppMetricsUtil::recordAppInstallStartTime(const std::string& app_name) {
  install_start_time_map_[app_name] = base::TimeTicks::Now();
  installs_requested_ = true;
}

void ArcAppMetricsUtil::maybeReportInstallTimeDelta(
    const std::string& app_name) {
  if (install_start_time_map_.count(app_name) == 1) {
    const base::TimeTicks start_time = install_start_time_map_[app_name];
    const base::TimeDelta time_delta = base::TimeTicks::Now() - start_time;
    base::UmaHistogramLongTimes(
        base::StrCat({kManualInstallHistogramBase, "TimeDelta"}), time_delta);
    install_start_time_map_.erase(app_name);
  }
}

void ArcAppMetricsUtil::reportIncompleteInstalls() {
  if (installs_requested_) {
    base::UmaHistogramExactLinear(
        base::StrCat({kManualInstallHistogramBase, "NumAppsIncomplete"}),
        install_start_time_map_.size(), /*exclusive_max=*/51);
  }
}

}  // namespace arc
