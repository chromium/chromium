// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_metrics_controller.h"

#include <vector>

#include "ash/root_window_controller.h"
#include "ash/root_window_settings.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/switchable_windows.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_metrics.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/env.h"
#include "ui/display/screen.h"
#include "ui/display/tablet_state.h"
#include "ui/wm/public/activation_client.h"

namespace ash {

namespace {

// Histogram of the device UI mode when entering split view.
constexpr char kSplitViewEntryPointDeviceUIModeHistogram[] =
    "Ash.SplitView.EntryPoint.DeviceUIMode";
// Histogram of the device orientation when entering split view.
constexpr char kSplitViewEntryPointDeviceOrientationHistogram[] =
    "Ash.SplitView.EntryPoint.DeviceOrientation";
// Histogram of the device orientation when using split view.
constexpr char kSplitViewDeviceOrientationPrefix[] =
    "Ash.SplitView.DeviceOrientation";
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
base::TimeTicks g_clamshell_multi_display_split_view_start_time;
base::TimeTicks g_tablet_multi_display_split_view_start_time;

// An accumulator of clamshell multi-display split view engagement time. When
// the clamshell split view with two windows snapped on both sides is paused
// (the definition of "pause" is defined in the comments at the beginning of the
// the header file), accumulate the current engagement time period.
int64_t g_clamshell_multi_display_split_view_time_ms;

bool IsRecordingClamshellMultiDisplaySplitView() {
  return g_clamshell_multi_display_split_view_start_time != kInvalidTime;
}

bool IsRecordingTabletMultiDisplaySplitView() {
  return g_tablet_multi_display_split_view_start_time != kInvalidTime;
}

// Number of root windows in split view.
int NumRootWindowsInSplitViewRecording() {
  auto root_windows = Shell::GetAllRootWindows();
  return base::ranges::count_if(root_windows, [](aura::Window* root_window) {
    return SplitViewController::Get(root_window)
        ->split_view_metrics_controller()
        ->in_split_view_recording();
  });
}

bool InTabletMode() {
  return display::Screen::GetScreen()->InTabletMode();
}

bool TopTwoVisibleWindowsBothSnapped(
    const std::vector<raw_ptr<aura::Window, VectorExperimental>>& windows) {
  int windows_size = windows.size();
  if (windows_size < 2)
    return false;

  // Check if there are snapped windows on both sides without hidden by other
  // windows. The topmost window is at the end of the list.
  WindowState* top_snap_window_state = WindowState::Get(windows.back());
  if (!top_snap_window_state->IsSnapped())
    return false;

  for (aura::Window* window : base::Reversed(windows)) {
    // Skip the top one.
    if (window == windows.back())
      continue;

    auto* window_state = WindowState::Get(window);
    // Skip the invisible windows.
    if (!window->IsVisible())
      continue;
    if (!window_state->IsSnapped())
      return false;
    if (window_state->GetStateType() == top_snap_window_state->GetStateType()) {
      continue;
    } else {
      return true;
    }
  }
  return false;
}

// Appends the proper suffix to |prefix| based on whether the device is in
// tablet mode or not.
std::string GetHistogramNameWithDeviceUIMode(std::string prefix) {
  return prefix.append(InTabletMode() ? ".TabletMode" : ".ClamshellMode");
}

SplitViewMetricsController::DeviceOrientation GetDeviceOrientation(
    const display::Display& display) {
  return display.is_landscape()
             ? SplitViewMetricsController::DeviceOrientation::kLandscape
             : SplitViewMetricsController::DeviceOrientation::kPortrait;
}

// Records the pref value of `kSnapWindowSuggestions` at the time a window is
// snapped.
void MaybeRecordSnapWindowSuggestions(
    WindowSnapActionSource snap_action_source) {
  if (!CanSnapActionSourceStartFasterSplitView(snap_action_source)) {
    return;
  }
  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  if (!pref_service) {
    return;
  }
  base::UmaHistogramBoolean(
      BuildSnapWindowSuggestionsHistogramName(snap_action_source),
      pref_service->GetBoolean(prefs::kSnapWindowSuggestions));
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
  Shell::Get()->activation_client()->AddObserver(this);

  auto* desks_controller = Shell::Get()->desks_controller();
  desks_controller->AddObserver(this);
  current_desk_ = desks_controller->active_desk();

  aura::Env::GetInstance()->AddObserver(this);

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          split_view_controller->root_window());
  orientation_ = GetDeviceOrientation(display);
  ResetTimeAndCounter();
}

SplitViewMetricsController::~SplitViewMetricsController() {
  ClearObservedWindows();
  split_view_controller_->RemoveObserver(this);
  Shell::Get()->activation_client()->RemoveObserver(this);
  Shell::Get()->desks_controller()->RemoveObserver(this);
  aura::Env::GetInstance()->RemoveObserver(this);
}

void SplitViewMetricsController::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  if (previous_state == state)
    return;

  if (previous_state == SplitViewController::State::kNoSnap)
    StartRecordSplitViewMetrics();
  else if (state == SplitViewController::State::kNoSnap)
    StopRecordSplitViewMetrics();
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
  if (!(changed_metrics &
        display::DisplayObserver::DisplayMetric::DISPLAY_METRIC_ROTATION)) {
    return;
  }

  // Do nothing if the split view does not belongs to the modified display.
  if (GetRootWindowSettings(split_view_controller_->root_window())
          ->display_id != display.id()) {
    return;
  }

  const DeviceOrientation orientation = GetDeviceOrientation(display);
  if (orientation_ == orientation)
    return;
  orientation_ = orientation;

  // Reports change of the display orientation.
  if (in_split_view_recording_) {
    base::UmaHistogramEnumeration(kOrientationInSplitViewHistogram,
                                  orientation_);
    ReportDeviceUIModeAndOrientationHistogram();
  }
}

void SplitViewMetricsController::OnDisplayTabletStateChanged(
    display::TabletState state) {
  switch (state) {
    case display::TabletState::kEnteringTabletMode:
    case display::TabletState::kExitingTabletMode:
      break;
    case display::TabletState::kInTabletMode:
      OnTabletModeStarted();
      break;
    case display::TabletState::kInClamshellMode:
      OnTabletModeEnded();
      break;
  }
}

void SplitViewMetricsController::OnWindowParentChanged(aura::Window* window,
                                                       aura::Window* parent) {
  // Stop observing the window when it is moved to another desk. If the restored
  // window is parented to current desk, observe its window state change.
  if (parent && desks_util::IsDeskContainer(parent)) {
    if (parent->GetId() != current_desk_->container_id()) {
      RemoveObservedWindow(window);
    } else if (base::Contains(no_state_observed_windows_, window)) {
      WindowState::Get(window)->AddObserver(this);
      no_state_observed_windows_.erase(window);
    }

    // If the top two windows snapped on both sides in clamshell mode, moving
    // one of the snapped windows to another desk may end the split view. If
    // there are two windows snapped on both sides with another unsnapped window
    // on top in clamshell mode, moving the unsnapped window to another desk may
    // start split view.
    MaybeStartOrEndRecordBothSnappedClamshellSplitView();
  }
}

void SplitViewMetricsController::OnResizeLoopEnded(aura::Window* window) {
  // Only report window resizing if it is in the split view with two windows
  // snapped on both sides.
  if (!in_split_view_recording_ || split_view_controller_->InSplitViewMode())
    return;

  clamshell_resize_count_ += 1;
}

void SplitViewMetricsController::OnWindowDestroyed(aura::Window* window) {
  RemoveObservedWindow(window);

  // If the top two windows snapped on both sides in clamshell mode,
  // destroying one of the snapped windows may end the split view. If there
  // are two windows snapped on both sides with another unsnapped window
  // on top in clamshell mode, destroying the unsnapped window may start split
  // view.
  MaybeStartOrEndRecordBothSnappedClamshellSplitView();
}

void SplitViewMetricsController::OnWindowRemovingFromRootWindow(
    aura::Window* window,
    aura::Window* new_root) {
  // Stop observing the window if it is removing from current root window.
  RemoveObservedWindow(window);

  // If the top two windows snapped on both sides in clamshell mode,
  // moving one of the snapped windows to another display may end the split
  // view. If there are two windows snapped on both sides with another unsnapped
  // window on top in clamshell mode, moving the unsnapped window to another
  // display may start split view.
  MaybeStartOrEndRecordBothSnappedClamshellSplitView();

  // Add the window to the new root window's split view metrics controller. It
  // may make the new root window start or end split view metrics recording.
  if (new_root) {
    auto* target_split_view_metrics_controller =
        SplitViewController::Get(new_root)->split_view_metrics_controller();
    DCHECK(target_split_view_metrics_controller);
    if (!target_split_view_metrics_controller->IsObservingWindow(window)) {
      target_split_view_metrics_controller->AddObservedWindow(window);
      target_split_view_metrics_controller
          ->MaybeStartOrEndRecordBothSnappedClamshellSplitView();
    }
  }
}

void SplitViewMetricsController::OnPostWindowStateTypeChange(
    WindowState* window_state,
    chromeos::WindowStateType old_type) {
  MaybeStartOrEndRecordSnapTwoWindowsDuration(window_state);
  MaybeStartOrEndRecordMinimizeTwoWindowsDuration(window_state, old_type);

  // We only care if a window is snapped or unsnapped.
  bool is_snapped = window_state->IsSnapped();
  if (is_snapped) {
    MaybeRecordSnapWindowSuggestions(
        window_state->snap_action_source().value_or(
            WindowSnapActionSource::kNotSpecified));
  }
  bool was_snapped = chromeos::IsSnappedWindowStateType(old_type);
  if (is_snapped == was_snapped)
    return;

  if (was_snapped &&
      chromeos::IsSnappedWindowStateType(first_closed_state_type_) &&
      old_type != first_closed_state_type_) {
    // If a window in the opposite side of `first_closed_state_type_` gets
    // unsnapped, record the max duration to indicate a second snapped window
    // was never closed after the first window.
    RecordCloseTwoWindowsDuration(kSequentialSnapActionMaxTime);
  }

  MaybeStartOrEndRecordBothSnappedClamshellSplitView();
}

void SplitViewMetricsController::OnWindowActivated(ActivationReason reason,
                                                   aura::Window* gained_active,
                                                   aura::Window* lost_active) {
  // Reorder the observed windows.
  AddOrStackWindowOnTop(gained_active);
  MaybeStartOrEndRecordBothSnappedClamshellSplitView();
}

void SplitViewMetricsController::OnDeskActivationChanged(
    const Desk* activated,
    const Desk* deactivated) {
  // When switching desks, ends the split view and updates observed windows.
  StopRecordSplitViewMetrics();
  current_desk_ = Shell::Get()->desks_controller()->active_desk();
  InitObservedWindowsOnActiveDesk();

  // Check if the new desk is in clamshell split view with two windows snapped
  // on both sides.
  MaybeStartOrEndRecordBothSnappedClamshellSplitView();
}

void SplitViewMetricsController::OnWindowInitialized(aura::Window* window) {
  int32_t* activation_index =
      window->GetProperty(app_restore::kActivationIndexKey);
  if (!activation_index)
    return;

  app_restore::WindowInfo* window_info =
      window->GetProperty(app_restore::kWindowInfoKey);
  if (!window_info)
    return;

  // Check if the recovered window belongs to the same root window.
  // Note: The display id saved in window_info has no value. Need to use the
  // restore bounds/
  if (!window_info->current_bounds.has_value() ||
      !display::Screen::GetScreen()
           ->GetDisplayNearestWindow(split_view_controller_->root_window())
           .work_area()
           .Contains(window_info->current_bounds.value())) {
    return;
  }

  // Check if the recovered window is in the current desk.
  if (!window_info->desk_guid.is_valid() ||
      window_info->desk_guid != current_desk_->uuid()) {
    return;
  }

  // Insert the window in the `observed_windows_` list according to its
  // activation index key. Since the window is not parented at this stage, the
  // `WindowStateObserver` will be added later in `OnWindowParentChanged`.
  window->AddObserver(this);
  no_state_observed_windows_.insert(window);
  observed_windows_.insert(WindowRestoreController::GetWindowToInsertBefore(
                               window, observed_windows_),
                           window);
}

void SplitViewMetricsController::StartRecordSplitViewMetrics() {
  if (in_split_view_recording_)
    return;

  // If the split view is started with a snapped window next to the overview,
  // and there is only one window (no overview items), the overview will end
  // immediately so does the split view. We won't record this case.
  if (split_view_controller_->InClamshellSplitViewMode() &&
      Shell::Get()
              ->mru_window_tracker()
              ->BuildMruWindowList(DesksMruType::kActiveDesk)
              .size() == 1) {
    return;
  }

  bool in_clamshell = !InTabletMode();
  if (in_clamshell)
    StartRecordClamshellSplitView();
  else
    StartRecordTabletSplitView();

  in_split_view_recording_ = true;
  // The starting of this split view makes the number of root windows in split
  // view become two, which means the multi-display split view just started.
  if (NumRootWindowsInSplitViewRecording() == 2) {
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

void SplitViewMetricsController::StopRecordSplitViewMetrics() {
  if (!in_split_view_recording_)
    return;

  bool is_recording_clamshell_metrics = IsRecordingClamshellMetrics();
  if (is_recording_clamshell_metrics) {
    // If the split view is started with a snapped window next to the overview,
    // the the user activate a window by clicking the overview item, the window
    // will snap to the other side. Therefore, stacks the window on top.
    if (auto* to_be_activated_window =
            split_view_controller_->to_be_activated_window()) {
      AddOrStackWindowOnTop(to_be_activated_window);
    }

    // Do not end if there are still two windows snapped on both sides on top.
    if (TopTwoVisibleWindowsBothSnapped(observed_windows_))
      return;

    StopRecordClamshellSplitView();
  } else {
    StopRecordTabletSplitView();
  }

  base::UmaHistogramCounts100(kSplitViewSwapWindowCountHistogram, swap_count_);

  in_split_view_recording_ = false;

  // The ending of this split view makes the number of root windows in split
  // view become one, which means the multi-display split view just ended.
  if (NumRootWindowsInSplitViewRecording() == 1) {
    if (is_recording_clamshell_metrics)
      StopRecordClamshellMultiDisplaySplitView();
    else
      StopRecordTabletMultiDisplaySplitView();
  }
  ResetTimeAndCounter();
}

bool SplitViewMetricsController::IsObservingWindow(aura::Window* window) const {
  return base::Contains(observed_windows_, window);
}

void SplitViewMetricsController::AddObservedWindow(aura::Window* window) {
  window->AddObserver(this);
  WindowState::Get(window)->AddObserver(this);
  observed_windows_.emplace_back(window);
}

void SplitViewMetricsController::RemoveObservedWindow(aura::Window* window) {
  if (window->is_destroying()) {
    MaybeStartOrEndRecordCloseTwoWindowsDuration(window);
  }

  if (window == first_snapped_window_) {
    if (window->is_destroying()) {
      // If `first_snapped_window_` was destroyed, record the max duration to
      // indicate a second window was never snapped on the opposite side.
      RecordSnapTwoWindowsDuration(kSequentialSnapActionMaxTime);
    }
    first_snapped_window_ = nullptr;
  }
  if (first_minimized_window_state_ &&
      window == first_minimized_window_state_->window()) {
    if (window->is_destroying()) {
      RecordMinimizeTwoWindowsDuration(kSequentialSnapActionMaxTime);
    }
    first_minimized_window_state_ = nullptr;
  }
  if (std::erase(observed_windows_, window)) {
    WindowState::Get(window)->RemoveObserver(this);
    window->RemoveObserver(this);
  }
}

void SplitViewMetricsController::AddOrStackWindowOnTop(aura::Window* window) {
  // We only observe the activable windows on active desk of the root window
  // attached by |split_view_controller_|.
  if (!window)
    return;

  if (window->GetRootWindow() != split_view_controller_->root_window())
    return;

  aura::Window* parent = window->parent();
  if (!parent || !IsSwitchableContainer(parent))
    return;

  const int parent_id = parent->GetId();
  if (desks_util::IsDeskContainerId(parent_id) &&
      parent_id != current_desk_->container_id()) {
    return;
  }

  if (!CanIncludeWindowInMruList(window))
    return;

  auto iter = base::ranges::find(observed_windows_, window);
  if (iter == observed_windows_.end()) {
    AddObservedWindow(window);
  } else {
    observed_windows_.erase(iter);
    observed_windows_.emplace_back(window);
  }
}

void SplitViewMetricsController::InitObservedWindowsOnActiveDesk() {
  ClearObservedWindows();

  auto windows =
      current_desk_
          ->GetDeskContainerForRoot(split_view_controller_->root_window())
          ->children();
  for (aura::Window* window : windows) {
    if (!CanIncludeWindowInMruList(window))
      continue;
    AddObservedWindow(window);
  }
}

void SplitViewMetricsController::ClearObservedWindows() {
  while (!observed_windows_.empty()) {
    RemoveObservedWindow(observed_windows_.back());
  }
}

void SplitViewMetricsController::
    MaybeStartOrEndRecordBothSnappedClamshellSplitView() {
  if (InTabletMode() || split_view_controller_->InSplitViewMode()) {
    return;
  }

  bool both_snapped = TopTwoVisibleWindowsBothSnapped(observed_windows_);
  if (!in_split_view_recording_ && both_snapped) {
    StartRecordSplitViewMetrics();
  } else if (in_split_view_recording_ && !both_snapped) {
    StopRecordSplitViewMetrics();
  }
}

bool SplitViewMetricsController::
    MaybePauseRecordBothSnappedClamshellSplitView() {
  if (InTabletMode() || split_view_controller_->InSplitViewMode()) {
    return false;
  }

  if (observed_windows_.size() < 3)
    return false;
  // Find the topmost unsnapped visible window.
  auto iter = observed_windows_.end() - 1;
  auto begin_iter = observed_windows_.begin();
  for (; iter != begin_iter; iter--) {
    if (!(*iter)->IsVisible())
      continue;
    if (WindowState::Get(*iter)->IsSnapped())
      return false;
    else
      break;
  }

  if (iter == begin_iter)
    return false;

  return TopTwoVisibleWindowsBothSnapped(
      std::vector<raw_ptr<aura::Window, VectorExperimental>>(begin_iter, iter));
}

void SplitViewMetricsController::RecordSnapTwoWindowsDuration(
    const base::TimeDelta& elapsed_time) {
  base::UmaHistogramCustomTimes(kSnapTwoWindowsDurationHistogramName,
                                /*sample=*/elapsed_time,
                                kSequentialSnapActionMinTime,
                                kSequentialSnapActionMaxTime, /*buckets=*/100);
  first_snapped_window_ = nullptr;
  first_snapped_time_ = base::TimeTicks();
}

void SplitViewMetricsController::RecordMinimizeTwoWindowsDuration(
    const base::TimeDelta& elapsed_time) {
  base::UmaHistogramCustomTimes(kMinimizeTwoWindowsDurationHistogramName,
                                /*sample=*/elapsed_time,
                                kSequentialSnapActionMinTime,
                                kSequentialSnapActionMaxTime, /*buckets=*/100);
  first_minimized_window_state_ = nullptr;
  first_minimized_time_ = base::TimeTicks();
}

void SplitViewMetricsController::RecordCloseTwoWindowsDuration(
    const base::TimeDelta& elapsed_time) {
  base::UmaHistogramCustomTimes(kCloseTwoWindowsDurationHistogramName,
                                /*sample=*/elapsed_time,
                                kSequentialSnapActionMinTime,
                                kSequentialSnapActionMaxTime, /*buckets=*/100);
  // Reset `first_closed_state_type_` to kDefault to stop recording.
  first_closed_state_type_ = chromeos::WindowStateType::kDefault;
  first_closed_time_ = base::TimeTicks();
}

void SplitViewMetricsController::MaybeStartOrEndRecordSnapTwoWindowsDuration(
    WindowState* window_state) {
  // If `first_snapped_window_` is no longer snapped, record the max duration to
  // indicate a second window was never snapped on the opposite side.
  if (first_snapped_window_ &&
      !WindowState::Get(first_snapped_window_)->IsSnapped()) {
    // Any state type change can change `first_snapped_window_`'s state type
    // (i.e. float). This must be reset before we check `first_snapped_window_`
    // below.
    RecordSnapTwoWindowsDuration(kSequentialSnapActionMaxTime);
  }
  if (window_state->IsSnapped()) {
    if (first_snapped_window_ && !first_snapped_time_.is_null() &&
        window_state->window() != first_snapped_window_ &&
        window_state->GetStateType() ==
            ToWindowStateType(GetOppositeSnapType(first_snapped_window_))) {
      // If this is a different window that got snapped on the opposite side,
      // record the duration since `first_snapped_time_`.
      RecordSnapTwoWindowsDuration(base::TimeTicks::Now() -
                                   first_snapped_time_);
      return;
    }
    // Else start recording. If the same window gets snapped again, this will
    // restart recording.
    first_snapped_window_ = window_state->window();
    first_snapped_time_ = base::TimeTicks::Now();
    return;
  }
}

void SplitViewMetricsController::
    MaybeStartOrEndRecordMinimizeTwoWindowsDuration(
        WindowState* window_state,
        chromeos::WindowStateType old_type) {
  const bool is_minimized = window_state->IsMinimized();
  if (is_minimized && chromeos::IsSnappedWindowStateType(old_type)) {
    if (first_minimized_window_state_ && !first_minimized_time_.is_null()) {
      // No need to check if `first_minimized_window_state_` is the same as
      // `window_state`, since if it changes state it would no longer be
      // minimized, and would fall through to record the max duration below.
      RecordMinimizeTwoWindowsDuration(base::TimeTicks::Now() -
                                       first_minimized_time_);
      return;
    }
    first_minimized_window_state_ = window_state;
    first_minimized_time_ = base::TimeTicks::Now();
    return;
  }
  if (window_state == first_minimized_window_state_ && !is_minimized &&
      !window_state->IsSnapped()) {
    // If the first window is no longer minimized or snapped, record the max
    // duration to indicate no other window was snapped then minimized.
    RecordMinimizeTwoWindowsDuration(kSequentialSnapActionMaxTime);
  }
}

void SplitViewMetricsController::MaybeStartOrEndRecordCloseTwoWindowsDuration(
    aura::Window* window) {
  if (auto* window_state = WindowState::Get(window);
      window_state && window_state->IsSnapped()) {
    if (!chromeos::IsSnappedWindowStateType(first_closed_state_type_)) {
      // If `first_closed_state_type_` is reset to kDefault, start recording.
      first_closed_state_type_ = window_state->GetStateType();
      first_closed_time_ = base::TimeTicks::Now();
      return;
    }
    // If `window` has the opposite state type of `first_closed_state_type_`,
    // record the duration.
    if (ToWindowStateType(GetOppositeSnapType(window)) ==
            first_closed_state_type_ &&
        !first_closed_time_.is_null()) {
      RecordCloseTwoWindowsDuration(base::TimeTicks::Now() -
                                    first_closed_time_);
    }
  }
}

void SplitViewMetricsController::ResetTimeAndCounter() {
  clamshell_split_view_start_time_ = kInvalidTime;
  tablet_split_view_start_time_ = kInvalidTime;
  clamshell_resize_count_ = 0;
  tablet_resize_count_ = 0;
  swap_count_ = 0;
}

void SplitViewMetricsController::OnTabletModeStarted() {
  // If it has been in split view and recording clamshell mode metrics, stop
  // recording clamshell mode metrics and start to record tablet mode metrics.
  if (in_split_view_recording_ && IsRecordingClamshellMetrics()) {
    StopRecordClamshellSplitView();
    StartRecordTabletSplitView();
    if (NumRootWindowsInSplitViewRecording() > 1 &&
        IsRecordingClamshellMultiDisplaySplitView()) {
      StopRecordClamshellMultiDisplaySplitView();
      StartRecordTabletMultiDisplaySplitView();
    }
  }
}

void SplitViewMetricsController::OnTabletModeEnded() {
  // If it has been in split view and recording tablet mode metrics, stop
  // recording tablet mode metrics and start to record clamshell mode metrics.
  if (in_split_view_recording_ && IsRecordingTabletMetrics()) {
    StopRecordTabletSplitView();
    StartRecordClamshellSplitView();
    if (NumRootWindowsInSplitViewRecording() > 1 &&
        IsRecordingTabletMultiDisplaySplitView()) {
      StopRecordTabletMultiDisplaySplitView();
      StartRecordClamshellMultiDisplaySplitView();
    }
  }
}

bool SplitViewMetricsController::IsRecordingClamshellMetrics() const {
  return clamshell_split_view_start_time_ != kInvalidTime;
}
bool SplitViewMetricsController::IsRecordingTabletMetrics() const {
  return tablet_split_view_start_time_ != kInvalidTime;
}

void SplitViewMetricsController::StartRecordClamshellSplitView() {
  clamshell_split_view_start_time_ = base::TimeTicks::Now();
  ReportDeviceUIModeAndOrientationHistogram();
}

void SplitViewMetricsController::StopRecordClamshellSplitView() {
  DCHECK_NE(clamshell_split_view_start_time_, kInvalidTime);

  // Accumulate the engagement time.
  clamshell_split_view_time_ +=
      (base::TimeTicks::Now() - clamshell_split_view_start_time_)
          .InMicroseconds();
  clamshell_split_view_start_time_ = kInvalidTime;

  // If pauses, do not emit the records.
  if (MaybePauseRecordBothSnappedClamshellSplitView())
    return;

  base::UmaHistogramLongTimes(kTimeInSplitScreenClamshellHistogram,
                              base::Milliseconds(clamshell_split_view_time_));
  base::UmaHistogramCounts100(kSplitViewResizeWindowCountClamshellHistogram,
                              clamshell_resize_count_);
  clamshell_split_view_time_ = 0;
  clamshell_resize_count_ = 0;
}

void SplitViewMetricsController::StartRecordTabletSplitView() {
  tablet_split_view_start_time_ = base::TimeTicks::Now();
  ReportDeviceUIModeAndOrientationHistogram();
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
  ReportDeviceUIModeAndOrientationHistogram();
}

void SplitViewMetricsController::StopRecordClamshellMultiDisplaySplitView() {
  DCHECK_NE(g_clamshell_multi_display_split_view_start_time, kInvalidTime);

  // Accumulate the engagement time.
  g_clamshell_multi_display_split_view_time_ms =
      (base::TimeTicks::Now() - g_clamshell_multi_display_split_view_start_time)
          .InMilliseconds();
  g_clamshell_multi_display_split_view_start_time = kInvalidTime;

  // If pauses, do not emit the records.
  if (MaybePauseRecordBothSnappedClamshellSplitView())
    return;

  base::UmaHistogramLongTimes(
      kTimeInMultiDisplaySplitScreenClamshellHistogram,
      base::Milliseconds(g_clamshell_multi_display_split_view_time_ms));
  g_clamshell_multi_display_split_view_time_ms = 0;
}

void SplitViewMetricsController::StartRecordTabletMultiDisplaySplitView() {
  g_tablet_multi_display_split_view_start_time = base::TimeTicks::Now();
  ReportDeviceUIModeAndOrientationHistogram();
}

void SplitViewMetricsController::StopRecordTabletMultiDisplaySplitView() {
  DCHECK_NE(g_tablet_multi_display_split_view_start_time, kInvalidTime);

  base::UmaHistogramLongTimes(
      kTimeInMultiDisplaySplitScreenTabletHistogram,
      base::TimeTicks::Now() - g_tablet_multi_display_split_view_start_time);
  g_tablet_multi_display_split_view_start_time = kInvalidTime;
}

void SplitViewMetricsController::ReportDeviceUIModeAndOrientationHistogram() {
  base::UmaHistogramEnumeration(
      GetHistogramNameWithDeviceUIMode(kSplitViewDeviceOrientationPrefix),
      orientation_);
}

}  // namespace ash
