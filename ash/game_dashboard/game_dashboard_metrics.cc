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
constexpr char kGameDashboardToggleMainMenu[] = "ToggleMainMenu";

constexpr char kOn[] = "On";
constexpr char kOff[] = "Off";
constexpr char kSeparator[] = ".";

}  // namespace

ASH_EXPORT std::string BuildGameDashboardHistogramName(
    GameDashboardHistogramCategory category,
    bool is_on) {
  switch (category) {
    case GameDashboardHistogramCategory::kToggleMainMenu:
      return base::JoinString(
          std::vector<std::string>{kGameDashboardHistogramNameRoot,
                                   kGameDashboardToggleMainMenu,
                                   is_on ? kOn : kOff},
          kSeparator);
    default:
      NOTREACHED();
  }
}

void RecordGameDashboardToggleMainMenu(
    GameDashboardMainMenuToggleMethod toggled_method,
    bool toggled_on) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(
          GameDashboardHistogramCategory::kToggleMainMenu, toggled_on),
      toggled_method);
}

}  // namespace ash
