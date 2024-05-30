// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/game_dashboard/game_dashboard_metrics.h"

#include <cstdint>

#include "ash/game_dashboard/game_dashboard_controller.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

namespace ash {

namespace {

constexpr char kGameDashboardHistogramNameRoot[] = "Ash.GameDashboard";
constexpr char kGameDashboardHistogramUkmNameRoot[] = "GameDashboard";

}  // namespace

ASH_EXPORT std::string BuildGameDashboardHistogramName(
    const std::string& name) {
  return base::JoinString(
      std::vector<std::string>{kGameDashboardHistogramNameRoot, name},
      kGameDashboardHistogramSeparator);
}

ASH_EXPORT std::string BuildGameDashboardUkmEventName(const std::string& name) {
  return base::JoinString(
      std::vector<std::string>{kGameDashboardHistogramUkmNameRoot, name},
      kGameDashboardHistogramSeparator);
}

void RecordGameDashboardToggleMainMenu(
    const std::string& app_id,
    GameDashboardMainMenuToggleMethod toggled_method,
    bool toggled_on) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(kGameDashboardToggleMainMenuHistogram)
          .append(kGameDashboardHistogramSeparator)
          .append(toggled_on ? kGameDashboardHistogramOn
                             : kGameDashboardHistogramOff),
      toggled_method);
  ukm::builders::GameDashboard_ToggleMainMenu(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetToggleOn(toggled_on)
      .SetToggleMethod(static_cast<int64_t>(toggled_method))
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardToolbarToggleState(const std::string& app_id,
                                           bool toggled_on) {
  base::UmaHistogramBoolean(BuildGameDashboardHistogramName(
                                kGameDashboardToolbarToggleStateHistogram),
                            toggled_on);
  ukm::builders::GameDashboard_ToolbarToggleState(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetToggleOn(toggled_on)
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardRecordingStartSource(const std::string& app_id,
                                             GameDashboardMenu menu) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(
          kGameDashboardRecordingStartSourceHistogram),
      menu);
  ukm::builders::GameDashboard_RecordingStartSource(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetSource(static_cast<int64_t>(menu))
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardScreenshotTakeSource(const std::string& app_id,
                                             GameDashboardMenu menu) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(
          kGameDashboardScreenshotTakeSourceHistogram),
      menu);
  ukm::builders::GameDashboard_ScreenshotTakeSource(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetSource(static_cast<int64_t>(menu))
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardEditControlsWithEmptyState(const std::string& app_id,
                                                   bool is_setup) {
  base::UmaHistogramBoolean(
      BuildGameDashboardHistogramName(
          kGameDashboardEditControlsWithEmptyStateHistogram),
      is_setup);
  ukm::builders::GameDashboard_EditControlsWithEmptyState(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetEmpty(is_setup)
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardToolbarClickToExpandState(const std::string& app_id,
                                                  bool is_expanded) {
  base::UmaHistogramBoolean(
      BuildGameDashboardHistogramName(
          kGameDashboardToolbarClickToExpandStateHistogram),
      is_expanded);
  ukm::builders::GameDashboard_ToolbarClickToExpandState(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetExpanded(is_expanded)
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardToolbarNewLocation(
    const std::string& app_id,
    GameDashboardToolbarSnapLocation location) {
  base::UmaHistogramEnumeration(BuildGameDashboardHistogramName(
                                    kGameDashboardToolbarNewLocationHistogram),
                                location);
  ukm::builders::GameDashboard_ToolbarNewLocation(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetLocation(static_cast<int64_t>(location))
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardFunctionTriggered(const std::string& app_id,
                                          GameDashboardFunction function) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(kGameDashboardFunctionTriggeredHistogram),
      function);
  ukm::builders::GameDashboard_FunctionTriggered(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetFunction(static_cast<int64_t>(function))
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardWelcomeDialogNotificationToggleState(
    const std::string& app_id,
    bool toggled_on) {
  base::UmaHistogramBoolean(
      BuildGameDashboardHistogramName(
          kGameDashboardWelcomeDialogNotificationToggleStateHistogram),
      toggled_on);
  ukm::builders::GameDashboard_WelcomeDialogNotificationToggleState(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetToggleOn(toggled_on)
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardControlsHintToggleSource(const std::string& app_id,
                                                 GameDashboardMenu menu,
                                                 bool toggled_on) {
  base::UmaHistogramEnumeration(
      BuildGameDashboardHistogramName(
          kGameDashboardControlsHintToggleSourceHistogram)
          .append(kGameDashboardHistogramSeparator)
          .append(toggled_on ? kGameDashboardHistogramOn
                             : kGameDashboardHistogramOff),
      menu);
  ukm::builders::GameDashboard_ControlsHintToggleSource(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetToggleOn(toggled_on)
      .SetSource(static_cast<int64_t>(menu))
      .Record(ukm::UkmRecorder::Get());
}

void RecordGameDashboardControlsFeatureToggleState(const std::string& app_id,
                                                   bool toggled_on) {
  base::UmaHistogramBoolean(
      BuildGameDashboardHistogramName(
          kGameDashboardControlsFeatureToggleStateHistogram),
      toggled_on);
  ukm::builders::GameDashboard_ControlsFeatureToggleState(
      GameDashboardController::Get()->GetUkmSourceId(app_id))
      .SetToggleOn(toggled_on)
      .Record(ukm::UkmRecorder::Get());
}

}  // namespace ash
