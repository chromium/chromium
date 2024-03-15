// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GAME_DASHBOARD_GAME_DASHBOARD_METRICS_H_
#define ASH_GAME_DASHBOARD_GAME_DASHBOARD_METRICS_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

inline constexpr char kGameDashboardToggleMainMenuHistogram[] =
    "ToggleMainMenu";
inline constexpr char kGameDashboardToolbarToggleStateHistogram[] =
    "ToolbarToggleState";
inline constexpr char kGameDashboardRecordingStartSourceHistogram[] =
    "RecordingStartSource";
inline constexpr char kGameDashboardScreenshotTakeSourceHistogram[] =
    "ScreenshotTakeSource";
inline constexpr char kGameDashboardEditControlsWithEmptyStateHistogram[] =
    "EditControlsWithEmptyState";

// Used to build histogram name with on or off state.
inline constexpr char kGameDashboardHistogramOn[] = "On";
inline constexpr char kGameDashboardHistogramOff[] = "Off";

// Used to build histogram name.
inline constexpr char kGameDashboardHistogramSeparator[] = ".";

// This enum should be kept in sync with the `GameDashboardMainMenuToggleMethod`
// in tools/metrics/histograms/enums.xml.
enum class GameDashboardMainMenuToggleMethod {
  kGameDashboardButton,
  kSearchPlusG,
  kEsc,
  kActivateNewFeature,
  kOverview,
  // Includes clicking outside of the menu, clicking on the game window close
  // button, and game window closing unspecified.
  kOthers,
  kTabletMode,
  kMaxValue = kTabletMode,
};

// This enum should be kept in sync with the `GameDashboardMenu` in
// tools/metrics/histograms/enums.xml.
enum class GameDashboardMenu {
  kMainMenu,
  kToolbar,
  kMaxValue = kToolbar,
};

ASH_EXPORT std::string BuildGameDashboardHistogramName(const std::string& name);

void RecordGameDashboardToggleMainMenu(
    const std::string& app_id,
    GameDashboardMainMenuToggleMethod toggle_method,
    bool toggled_on);

void RecordGameDashboardToolbarToggleState(const std::string& app_id,
                                           bool toggled_on);

void RecordGameDashboardRecordingStartSource(const std::string& app_id,
                                             GameDashboardMenu menu);

void RecordGameDashboardScreenshotTakeSource(const std::string& app_id,
                                             GameDashboardMenu menu);

void RecordGameDashboardEditControlsWithEmptyState(const std::string& app_id,
                                                   bool is_setup);

}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_METRICS_H_
