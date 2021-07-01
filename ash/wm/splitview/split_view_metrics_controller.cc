// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_metrics_controller.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_functions.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"

namespace ash {

namespace {

// Histogram of the device UI mode when entering split view.
constexpr char kSplitViewEntryPointDeviceUIModeHistogram[] =
    "Ash.SplitView.EntryPoint.DeviceUIMode";
// Histogram of the device orientation when entering split view.
constexpr char kSplitViewEntryPointDeviceOrientationHistogram[] =
    "Ash.SplitView.EntryPoint.DeviceOrientation";
// Histogram of the device orientation changes when using split view.
constexpr char kOrientationInSplitViewHistogram[] =
    "Ash.SplitView.OrientationInSplitView";
// Histogram of the engagement time in clamshell split view.
constexpr char kTimeInSplitScreenClamshellHistogram[] =
    "Ash.SplitView.TimeInSplitScreen.ClamshellMode";
// Histogram of the engagement time in tablet split view.
constexpr char kTimeInSplitScreenTabletHistogram[] =
    "Ash.SplitView.TimeInSplitScreen.TabletMode";
// Histogram of the engagement time in multi-display clamshell split view.
constexpr char kTimeInMultiDisplaySplitScreenClamshellHistogram[] =
    "Ash.SplitView.TimeInMultiDisplaySplitScreen.ClamshellMode";
// Histogram of the engagement time in multi-display tablet split view.
constexpr char kTimeInMultiDisplaySplitScreenTabletHistogram[] =
    "Ash.SplitView.TimeInMultiDisplaySplitScreen.TabletMode";
// Histogram of the number of resizing window operations in clamshell split
// view.
constexpr char kSplitViewResizeWindowCountClamshellHistogram[] =
    "Ash.SplitView.ResizeWindowCount.ClamshellMode";
// Histogram of the number of resizing window operations in tablet split view.
constexpr char kSplitViewResizeWindowCountTabletHistogram[] =
    "Ash.SplitView.ResizeWindowCount.TabletMode";
// Histogram of the number of swapping window operations in split view.
constexpr char kSplitViewSwapWindowCountHistogram[] =
    "Ash.SplitView.SwapWindowCount";

constexpr base::TimeTicks kInvalidTime = base::TimeTicks::Max();

// Start time of clamshell and tablet multi-display split view.
base::TimeTicks g_tablet_multi_display_split_view_start_time;
base::TimeTicks g_clamshell_multi_display_split_view_start_time;

// Number of root windows in split view.
int NumRootWindowsInSplitView() {
  auto root_windows = Shell::GetAllRootWindows();
  return std::count_if(
      root_windows.begin(), root_windows.end(), [](aura::Window* root_window) {
        return SplitViewController::Get(root_window)->InSplitViewMode();
      });
}

// Checks if the device is in tablet mode.
bool InTabletMode() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

}  // namespace

// static
SplitViewMetricsController* SplitViewMetricsController::Get(
    aura::Window* window) {
  DCHECK(window);
  DCHECK(window->GetRootWindow());

  auto* root_window_controller = RootWindowController::ForWindow(window);
  DCHECK(root_window_controller);

  return root_window_controller->split_view_controller()
      ->split_view_metrics_controller();
}

SplitViewMetricsController::SplitViewMetricsController(
    SplitViewController* split_view_controller)
    : split_view_controller_(split_view_controller) {
  split_view_controller_->AddObserver(this);
  tablet_mode_controller_observation_.Observe(
      Shell::Get()->tablet_mode_controller());
  Shell::Get()->display_manager()->AddObserver(this);

  orientation_ = SplitViewController::IsLayoutHorizontal()
                     ? DeviceOrientation::kLandscape
                     : DeviceOrientation::kPortrait;

  ResetTimeAndCounter();
}

SplitViewMetricsController::~SplitViewMetricsController() {
  tablet_mode_controller_observation_.Reset();
  split_view_controller_->RemoveObserver(this);
  Shell::Get()->display_manager()->RemoveObserver(this);
}

void SplitViewMetricsController::OnTabletModeStarted() {
  // If it has been in split view and recording clamshell mode metrics, stop
  // recording clamshell mode metrics and start to record tablet mode metrics.
  if (split_view_controller_->InSplitViewMode() &&
      IsRecordingClamshellMetrics()) {
    StopRecordClamshellSplitView();
    StartRecordTabletSplitView();
    if (NumRootWindowsInSplitView() > 1) {
      StopRecordClamshellMultiDisplaySplitView();
      StartRecordTabletMultiDisplaySplitView();
    }
  }
}

void SplitViewMetricsController::OnTabletModeEnded() {
  // If it has been in split view and recording tablet mode metrics, stop
  // recording tablet mode metrics and start to record clamshell mode metrics.
  if (split_view_controller_->InSplitViewMode() && IsRecordingTabletMetrics()) {
    StopRecordTabletSplitView();
    StartRecordClamshellSplitView();
    if (NumRootWindowsInSplitView() > 1) {
      StopRecordTabletMultiDisplaySplitView();
      StartRecordClamshellMultiDisplaySplitView();
    }
  }
}

void SplitViewMetricsController::OnTabletControllerDestroyed() {
  tablet_mode_controller_observation_.Reset();
}

void SplitViewMetricsController::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  if (previous_state == state)
    return;

  if (previous_state == SplitViewController::State::kNoSnap)
    OnSplitViewStarted();
  else if (state == SplitViewController::State::kNoSnap)
    OnSplitViewEnded();
}

void SplitViewMetricsController::OnSplitViewWindowResized() {
  DCHECK(split_view_controller_->InSplitViewMode());

  if (split_view_controller_->InClamshellSplitViewMode())
    clamshell_resize_count_ += 1;
  else
    tablet_resize_count_ += 1;
}

void SplitViewMetricsController::OnSplitViewWindowSwapped() {
  DCHECK(split_view_controller_->InSplitViewMode());

  swap_count_ += 1;

  // Decreases the counter by 2, since swapping windows will trigger resizing
  // window twice.
  if (split_view_controller_->InClamshellSplitViewMode())
    clamshell_resize_count_ -= 2;
  else
    tablet_resize_count_ -= 2;
}

void SplitViewMetricsController::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Checks if the device is in split view.
  if (!split_view_controller_->InSplitViewMode())
    return;

  // Checks if the root window of |split_view_controller_| is in the changed
  // display.
  if (display::Screen::GetScreen()
          ->GetDisplayNearestWindow(split_view_controller_->root_window())
          .id() != display.id()) {
    return;
  }

  // Checks if the display is rotated.
  if (changed_metrics !=
      display::DisplayObserver::DisplayMetric::DISPLAY_METRIC_ROTATION) {
    return;
  }

  // Reports change of the display orientation.
  DeviceOrientation orientation = SplitViewController::IsLayoutHorizontal()
                                      ? DeviceOrientation::kLandscape
                                      : DeviceOrientation::kPortrait;

  if (orientation_ != orientation) {
    orientation_ = orientation;
    base::UmaHistogramEnumeration(kOrientationInSplitViewHistogram,
                                  orientation_);
  }
}

void SplitViewMetricsController::OnSplitViewStarted() {
  bool in_clamshell = !InTabletMode();
  if (in_clamshell)
    StartRecordClamshellSplitView();
  else
    StartRecordTabletSplitView();

  // The starting of this split view makes the number of root windows in split
  // view become two, which means the multi-display split view just started.
  if (NumRootWindowsInSplitView() == 2) {
    if (in_clamshell)
      StartRecordClamshellMultiDisplaySplitView();
    else
      StartRecordTabletMultiDisplaySplitView();
  }

  base::UmaHistogramEnumeration(kSplitViewEntryPointDeviceOrientationHistogram,
                                orientation_);
  base::UmaHistogramEnumeration(
      kSplitViewEntryPointDeviceUIModeHistogram,
      in_clamshell ? DeviceUIMode::kClamshell : DeviceUIMode::kTablet);
}

void SplitViewMetricsController::OnSplitViewEnded() {
  bool is_recording_clamshell_metrics = IsRecordingClamshellMetrics();
  if (is_recording_clamshell_metrics)
    StopRecordClamshellSplitView();
  else
    StopRecordTabletSplitView();

  base::UmaHistogramCounts100(kSplitViewSwapWindowCountHistogram, swap_count_);

  // The ending of this split view makes the number of root windows in split
  // view become one, which means the multi-display split view just ended.
  if (NumRootWindowsInSplitView() == 1) {
    if (is_recording_clamshell_metrics)
      StopRecordClamshellMultiDisplaySplitView();
    else
      StopRecordTabletMultiDisplaySplitView();
  }
  ResetTimeAndCounter();
}

void SplitViewMetricsController::ResetTimeAndCounter() {
  clamshell_split_view_start_time_ = kInvalidTime;
  tablet_split_view_start_time_ = kInvalidTime;
  clamshell_resize_count_ = 0;
  tablet_resize_count_ = 0;
  swap_count_ = 0;
}

bool SplitViewMetricsController::IsRecordingClamshellMetrics() const {
  return clamshell_split_view_start_time_ != kInvalidTime;
}
bool SplitViewMetricsController::IsRecordingTabletMetrics() const {
  return tablet_split_view_start_time_ != kInvalidTime;
}

void SplitViewMetricsController::StartRecordClamshellSplitView() {
  clamshell_split_view_start_time_ = base::TimeTicks::Now();
}

void SplitViewMetricsController::StopRecordClamshellSplitView() {
  DCHECK_NE(clamshell_split_view_start_time_, kInvalidTime);

  base::UmaHistogramLongTimes(
      kTimeInSplitScreenClamshellHistogram,
      base::TimeTicks::Now() - clamshell_split_view_start_time_);
  clamshell_split_view_start_time_ = kInvalidTime;

  base::UmaHistogramCounts100(kSplitViewResizeWindowCountClamshellHistogram,
                              clamshell_resize_count_);
  clamshell_resize_count_ = 0;
}

void SplitViewMetricsController::StartRecordTabletSplitView() {
  tablet_split_view_start_time_ = base::TimeTicks::Now();
}

void SplitViewMetricsController::StopRecordTabletSplitView() {
  DCHECK_NE(tablet_split_view_start_time_, kInvalidTime);

  base::UmaHistogramLongTimes(
      kTimeInSplitScreenTabletHistogram,
      base::TimeTicks::Now() - tablet_split_view_start_time_);
  tablet_split_view_start_time_ = kInvalidTime;

  base::UmaHistogramCounts100(kSplitViewResizeWindowCountTabletHistogram,
                              tablet_resize_count_);
  tablet_resize_count_ = 0;
}

void SplitViewMetricsController::StartRecordClamshellMultiDisplaySplitView() {
  g_clamshell_multi_display_split_view_start_time = base::TimeTicks::Now();
}

void SplitViewMetricsController::StopRecordClamshellMultiDisplaySplitView() {
  DCHECK_NE(g_clamshell_multi_display_split_view_start_time, kInvalidTime);

  base::UmaHistogramLongTimes(
      kTimeInMultiDisplaySplitScreenClamshellHistogram,
      base::TimeTicks::Now() - g_clamshell_multi_display_split_view_start_time);
  g_clamshell_multi_display_split_view_start_time = kInvalidTime;
}

void SplitViewMetricsController::StartRecordTabletMultiDisplaySplitView() {
  g_tablet_multi_display_split_view_start_time = base::TimeTicks::Now();
}

void SplitViewMetricsController::StopRecordTabletMultiDisplaySplitView() {
  DCHECK_NE(g_tablet_multi_display_split_view_start_time, kInvalidTime);

  base::UmaHistogramLongTimes(
      kTimeInMultiDisplaySplitScreenTabletHistogram,
      base::TimeTicks::Now() - g_tablet_multi_display_split_view_start_time);
  g_tablet_multi_display_split_view_start_time = kInvalidTime;
}

}  // namespace ash
