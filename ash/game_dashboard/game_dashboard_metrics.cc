// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"

namespace ash {

namespace {

constexpr char kGameDashboardHistogramNameRoot[] = "Ash.GameDashboard";

}  // namespace

ASH_EXPORT std::string BuildGameDashboardHistogramName(
    const std::string& name) {
  return base::JoinString(
      std::vector<std::string>{kGameDashboardHistogramNameRoot, name},
      kGameDashboardHistogramSeparator);
}

void RecordGameDashboardToggleMainMenu(
    GameDashboardMainMenuToggleMethod toggled_method,
    bool toggled_on) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(kGameDashboardToggleMainMenuHistogram)
          .append(kGameDashboardHistogramSeparator)
          .append(toggled_on ? kGameDashboardHistogramOn
                             : kGameDashboardHistogramOff),
      toggled_method);
}

void RecordGameDashboardToolbarToggleState(bool toggled_on) {
  base::UmaHistogramBoolean(BuildGameDashboardHistogramName(
                                kGameDashboardToolbarToggleStateHistogram),
                            toggled_on);
}

void RecordGameDashboardRecordingStartSource(GameDashboardMenu menu) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(
          kGameDashboardRecordingStartSourceHistogram),
      menu);
}

void RecordGameDashboardScreenshotTakeSource(GameDashboardMenu menu) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(
          kGameDashboardScreenshotTakeSourceHistogram),
      menu);
}

void RecordGameDashboardEditControlsWithEmptyState(bool is_setup) {
  base::UmaHistogramBoolean(
      BuildGameDashboardHistogramName(
          kGameDashboardEditControlsWithEmptyStateHistogram),
      is_setup);
}

}  // namespace ash
