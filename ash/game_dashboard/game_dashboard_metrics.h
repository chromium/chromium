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

inline constexpr char kGameDashboardToolbarClickToExpandStateHistogram[] =
    "ToolbarClickToExpandState";
inline constexpr char kGameDashboardToolbarNewLocationHistogram[] =
    "ToolbarNewLocation";
inline constexpr char kGameDashboardFunctionTriggeredHistogram[] =
    "FunctionTriggered";
inline constexpr char
    kGameDashboardWelcomeDialogNotificationToggleStateHistogram[] =
        "WelcomeDialogNotificationToggleState";
inline constexpr char kGameDashboardControlsHintToggleSourceHistogram[] =
    "ControlsHintToggleSource";
inline constexpr char kGameDashboardControlsFeatureToggleStateHistogram[] =
    "ControlsFeatureToggleState";

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

// Indicator for the 4 quadrants that the toolbar is able to be placed.
// This enum should be kept in sync with the `GameDashboardToolbarPosition`
// in tools/metrics/histograms/enums.xml.
enum class GameDashboardToolbarSnapLocation {
  kTopLeft,
  kTopRight,
  kBottomRight,
  kBottomLeft,
  kMaxValue = kBottomLeft,
};

// Enumeration of the various functions accessible from the game dashboard.
// This enum should be kept in sync with the `GameDashboardFunction`
// in tools/metrics/histograms/enums.xml.
enum class GameDashboardFunction {
  kFeedback,
  kHelp,
  kSetting,
  kSettingBack,
  kScreenSize,
  kGameControlsSetupOrEdit,
  kMaxValue = kGameControlsSetupOrEdit,
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

void RecordGameDashboardToolbarClickToExpandState(bool is_expanded);

void RecordGameDashboardToolbarNewLocation(
    GameDashboardToolbarSnapLocation location);

void RecordGameDashboardFunctionTriggered(GameDashboardFunction function);

void RecordGameDashboardWelcomeDialogNotificationToggleState(bool toggled_on);

void RecordGameDashboardControlsHintToggleSource(GameDashboardMenu menu,
                                                 bool toggled_on);
void RecordGameDashboardControlsFeatureToggleState(bool toggled_on);
}  // namespace ash

#endif  // ASH_GAME_DASHBOARD_GAME_DASHBOARD_METRICS_H_
