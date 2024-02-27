// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_METRICS_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_METRICS_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

enum class GameDashboardMainMenuToggleMethod {
  kGameDashboardButton,
  kSearchPlusG,
  kEsc,
  kActivateNewFeature,
  kOverview,
  // Includes clicking outside of the menu, clicking on the game window close
  // button, and game window closing unspecified.
  kOthers,
  kMaxValue = kOthers,
};

enum class GameDashboardHistogramCategory {
  kToggleMainMenu,
  kMaxValue = kToggleMainMenu,
};

ASH_EXPORT std::string BuildGameDashboardHistogramName(
    GameDashboardHistogramCategory category,
    bool is_on);

void RecordGameDashboardToggleMainMenu(
    GameDashboardMainMenuToggleMethod toggle_method,
    bool toggled_on);

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_METRICS_H_
