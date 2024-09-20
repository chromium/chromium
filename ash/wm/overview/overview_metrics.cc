// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_metrics.h"

#include <string>

#include "ash/public/cpp/window_properties.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "ui/aura/window.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/screen.h"

namespace ash {
namespace {

const ui::PresentationTimeRecorder::BucketParams&
GetLegacyPresentationTimeBucketParams() {
  static const ui::PresentationTimeRecorder::BucketParams kParams(
      base::Milliseconds(1), base::Seconds(2), 50);
  return kParams;
}

int GetNumWindowsOnAllDesks() {
  int num_windows_found = 0;
  for (const auto& desk : DesksController::Get()->desks()) {
    for (const auto& window : desk->GetAllAssociatedWindows()) {
      if (!window->GetProperty(kHideInDeskMiniViewKey)) {
        ++num_windows_found;
      }
    }
  }
  for (const auto& root_window : Shell::Get()->GetAllRootWindows()) {
    num_windows_found += DesksController::Get()
                             ->GetVisibleOnAllDesksWindowsOnRoot(root_window)
                             .size();
  }
  return num_windows_found;
}

std::string GetPresentationTimeMetricNameWithDeskBar(
    int num_windows,
    std::string_view enter_exit_label) {
  constexpr int kNumWindowsOverflowBoundary = 10;
  const std::string num_windows_suffix =
      num_windows > kNumWindowsOverflowBoundary
          ? "MoreThan" + base::NumberToString(kNumWindowsOverflowBoundary)
          : base::NumberToString(num_windows);
  return base::StrCat({"Ash.Overview.", enter_exit_label,
                       ".PresentationTime.WithDeskBarAndNumWindows",
                       num_windows_suffix});
}

void RecordPresentationTimeMetricsWithDeskBar(
    std::unique_ptr<ui::PresentationTimeRecorder> enter_recorder,
    std::unique_ptr<ui::PresentationTimeRecorder> exit_recorder,
    DeskBarVisibility desk_bar_visibility,
    const std::string& enter_metric_name,
    const std::string& exit_metric_name) {
  // Only record for `kShownImmediately` because that's the only case where the
  // desk bar's rendering was a part of the first frame's overall presentation
  // latency when entering overview.
  if (desk_bar_visibility == DeskBarVisibility::kShownImmediately) {
    const std::optional<base::TimeDelta> enter_latency =
        enter_recorder->GetAverageLatency();
    if (enter_latency) {
      base::UmaHistogramCustomTimes(
          enter_metric_name, *enter_latency,
          GetLegacyPresentationTimeBucketParams().min_latency,
          GetLegacyPresentationTimeBucketParams().max_latency,
          GetLegacyPresentationTimeBucketParams().num_buckets);
    }
  }

  // If the desk bar is present, that means it plays a factor in the latency
  // when exiting overview, regardless of whether the desk bar was rendered
  // immediately or after the enter animation completed.
  if (desk_bar_visibility != DeskBarVisibility::kNotShown) {
    const std::optional<base::TimeDelta> exit_latency =
        exit_recorder->GetAverageLatency();
    if (exit_latency) {
      base::UmaHistogramCustomTimes(
          exit_metric_name, *exit_latency,
          GetLegacyPresentationTimeBucketParams().min_latency,
          GetLegacyPresentationTimeBucketParams().max_latency,
          GetLegacyPresentationTimeBucketParams().num_buckets);
    }
  }
}

}  // namespace

constexpr char kOverviewStartActionHistogram[] = "Ash.Overview.StartAction";
constexpr char kOverviewEndActionHistogram[] = "Ash.Overview.EndAction";

void RecordOverviewStartAction(OverviewStartAction type) {
  UMA_HISTOGRAM_ENUMERATION(kOverviewStartActionHistogram, type);
}

void RecordOverviewEndAction(OverviewEndAction type) {
  UMA_HISTOGRAM_ENUMERATION(kOverviewEndActionHistogram, type);
}

const ui::PresentationTimeRecorder::BucketParams&
GetOverviewPresentationTimeBucketParams() {
  static const ui::PresentationTimeRecorder::BucketParams kParams(
      base::Milliseconds(20), base::Seconds(30), 100);
  return kParams;
}

void SchedulePresentationTimeMetricsWithDeskBar(
    std::unique_ptr<ui::PresentationTimeRecorder> enter_recorder,
    std::unique_ptr<ui::PresentationTimeRecorder> exit_recorder,
    DeskBarVisibility desk_bar_visibility) {
  const size_t num_windows = GetNumWindowsOnAllDesks();
  // This function is currently called when exiting overview mode (before the
  // presentation time for the exit is known). Rather than seeking the exact
  // moment the exit-overview frame is presented, it's simpler to just record
  // the metric after the maximum expected frame latency since metric recording
  // is not an urgent operation.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &RecordPresentationTimeMetricsWithDeskBar, std::move(enter_recorder),
          std::move(exit_recorder), desk_bar_visibility,
          GetPresentationTimeMetricNameWithDeskBar(num_windows, "Enter"),
          GetPresentationTimeMetricNameWithDeskBar(num_windows, "Exit")),
      // For safety, wait twice the maximum expected latency before reading the
      // measurement. Anything longer than
      // `kOverviewEnterExitPresentationMaxLatency` goes in the overflow bucket
      // anyways.
      GetLegacyPresentationTimeBucketParams().max_latency * 2);
}

void RecordOverviewEnterPresentationTimeWithReason(
    OverviewStartAction start_action,
    base::TimeDelta presentation_time) {
  const char* suffix = nullptr;
  switch (start_action) {
    case OverviewStartAction::kDevTools:
    case OverviewStartAction::kTests:
    case OverviewStartAction::kBentoBar_DEPRECATED:
      suffix = "Other";
      break;
    case OverviewStartAction::kPine:
      suffix = "InformedRestore";
      break;
    case OverviewStartAction::kSplitView:
    case OverviewStartAction::kAccelerator:
    case OverviewStartAction::kDragWindowFromShelf:
    case OverviewStartAction::kExitHomeLauncher:
    case OverviewStartAction::kOverviewButton:
    case OverviewStartAction::kOverviewButtonLongPress:
    case OverviewStartAction::k3FingerVerticalScroll:
    case OverviewStartAction::kWallpaper:
    case OverviewStartAction::kOverviewDeskSwitch:
    case OverviewStartAction::kDeskButton:
    case OverviewStartAction::kFasterSplitScreenSetup:
      suffix = display::Screen::GetScreen()->InTabletMode()
                   ? "UserInitiatedTablet"
                   : "UserInitiatedClamshell";
      break;
  }
  base::UmaHistogramCustomTimes(
      base::StrCat({kEnterOverviewPresentationHistogram, ".", suffix}),
      presentation_time, GetOverviewPresentationTimeBucketParams().min_latency,
      GetOverviewPresentationTimeBucketParams().max_latency,
      GetOverviewPresentationTimeBucketParams().num_buckets);
}

}  // namespace ash
