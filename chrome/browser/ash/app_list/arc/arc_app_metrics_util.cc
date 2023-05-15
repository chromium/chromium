// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/arc/arc_app_metrics_util.h"

#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/ash/app_list/arc/arc_app_metrics_data.h"

namespace {

constexpr char kManualInstallHistogramBase[] =
    "Arc.AppInstall.Manual.InitialSession.";

constexpr char kPolicyInstallHistogramBase[] =
    "Arc.AppInstall.Policy.InitialSession.";

}  // namespace

namespace arc {

ArcAppMetricsUtil::ArcAppMetricsUtil() {
  manual_install_data_ =
      std::make_unique<ArcAppMetricsData>(kManualInstallHistogramBase);
  policy_install_data_ =
      std::make_unique<ArcAppMetricsData>(kPolicyInstallHistogramBase);
}

ArcAppMetricsUtil::~ArcAppMetricsUtil() = default;

void ArcAppMetricsUtil::recordAppInstallStartTime(
    const std::string& app_name,
    bool is_controlled_by_policy) {
  if (is_controlled_by_policy) {
    policy_install_data_->recordAppInstallStartTime(app_name);
  } else {
    manual_install_data_->recordAppInstallStartTime(app_name);
  }
}

void ArcAppMetricsUtil::maybeReportInstallTimeDelta(
    const std::string& app_name,
    bool is_controlled_by_policy) {
  if (is_controlled_by_policy) {
    policy_install_data_->maybeReportInstallTimeDelta(app_name);
  } else {
    manual_install_data_->maybeReportInstallTimeDelta(app_name);
  }
}

void ArcAppMetricsUtil::reportMetrics() {
  policy_install_data_->reportMetrics();
  manual_install_data_->reportMetrics();
}

}  // namespace arc
