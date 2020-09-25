// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include "ash/metrics/task_switch_metrics_recorder.h"
#include "ash/metrics/task_switch_source.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle_event_filter.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"

namespace ash {

namespace {

// Returns the most recently active window from the |window_list| or nullptr
// if the list is empty.
aura::Window* GetActiveWindow(
    const WindowCycleController::WindowList& window_list) {
  return window_list.empty() ? nullptr : window_list[0];
}

void ReportPossibleDesksSwitchStats(int active_desk_container_id_before_cycle) {
  // Report only for users who have 2 or more desks, since we're only interested
  // in seeing how users of Virtual Desks use window cycling.
  auto* desks_controller = DesksController::Get();
  if (!desks_controller)
    return;

  if (desks_controller->desks().size() < 2)
    return;

  // Note that this functions is called while a potential desk switch animation
  // is starting, in this case we want the target active desk (i.e. the soon-to-
  // be active desk after the animation finishes).
  const int active_desk_container_id_after_cycle =
      desks_controller->GetTargetActiveDesk()->container_id();
  DCHECK_NE(active_desk_container_id_before_cycle, kShellWindowId_Invalid);
  DCHECK_NE(active_desk_container_id_after_cycle, kShellWindowId_Invalid);

  // Note that the desks containers IDs are consecutive. See
  // |ash::ShellWindowId|.
  const int desks_switch_distance =
      std::abs(active_desk_container_id_after_cycle -
               active_desk_container_id_before_cycle);
  UMA_HISTOGRAM_EXACT_LINEAR("Ash.WindowCycleController.DesksSwitchDistance",
                             desks_switch_distance,
                             desks_util::kMaxNumberOfDesks);
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() = default;

WindowCycleController::~WindowCycleController() = default;

// static
bool WindowCycleController::CanCycle() {
  return !Shell::Get()->session_controller()->IsScreenLocked() &&
         !Shell::IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned() &&
         !window_util::IsAnyWindowDragged() &&
         !Shell::Get()->desks_controller()->AreDesksBeingModified();
}

void WindowCycleController::HandleCycleWindow(Direction direction) {
  if (!CanCycle())
    return;

  if (!IsCycling())
    StartCycling();

  Step(direction);
}

void WindowCycleController::Scroll(Direction direction) {
  if (!CanCycle())
    return;

  if (!IsCycling())
    StartCycling();

  DCHECK(window_cycle_list_);
  window_cycle_list_->ScrollInDirection(direction);
}

void WindowCycleController::StartCycling() {
  // Close the wallpaper preview if it is open to prevent visual glitches where
  // the window view item for the preview is transparent
  // (http://crbug.com/895265).
  Shell::Get()->wallpaper_controller()->MaybeClosePreviewWallpaper();

  WindowCycleController::WindowList window_list = CreateWindowList();
  SaveCurrentActiveDeskAndWindow(window_list);

  window_cycle_list_ = std::make_unique<WindowCycleList>(window_list);
  event_filter_ = std::make_unique<WindowCycleEventFilter>();
  base::RecordAction(base::UserMetricsAction("WindowCycleController_Cycle"));
  UMA_HISTOGRAM_COUNTS_100("Ash.WindowCycleController.Items",
                           window_list.size());
}

void WindowCycleController::CompleteCycling() {
  window_cycle_list_->set_user_did_accept(true);
  StopCycling();
}

void WindowCycleController::CancelCycling() {
  StopCycling();
}

void WindowCycleController::MaybeResetCycleList() {
  if (!IsCycling())
    return;

  WindowCycleController::WindowList window_list = CreateWindowList();
  SaveCurrentActiveDeskAndWindow(window_list);

  DCHECK(window_cycle_list_);
  window_cycle_list_->ReplaceWindows(window_list);
}

void WindowCycleController::SetFocusedWindow(aura::Window* window) {
  if (!IsCycling())
    return;

  DCHECK(window_cycle_list_);
  window_cycle_list_->SetFocusedWindow(window);
}

bool WindowCycleController::IsEventInCycleView(ui::LocatedEvent* event) {
  return window_cycle_list_ && window_cycle_list_->IsEventInCycleView(event);
}

bool WindowCycleController::IsWindowListVisible() {
  return window_cycle_list_ && window_cycle_list_->ShouldShowUi();
}

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

WindowCycleController::WindowList WindowCycleController::CreateWindowList() {
  WindowCycleController::WindowList window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleWithPipList(
          features::IsAltTabLimitedToActiveDesk() ? kActiveDesk : kAllDesks);
  // Window cycle list windows will handle showing their transient related
  // windows, so if a window in |window_list| has a transient root also in
  // |window_list|, we can remove it as the transient root will handle showing
  // the window.
  window_util::RemoveTransientDescendants(&window_list);
  return window_list;
}

void WindowCycleController::SaveCurrentActiveDeskAndWindow(
    const WindowCycleController::WindowList& window_list) {
  active_desk_container_id_before_cycle_ =
      desks_util::GetActiveDeskContainerId();
  active_window_before_window_cycle_ = GetActiveWindow(window_list);
}

void WindowCycleController::Step(Direction direction) {
  DCHECK(window_cycle_list_);
  window_cycle_list_->Step(direction);
}

void WindowCycleController::StopCycling() {
  window_cycle_list_.reset();

  // We can't use the MRU window list here to get the active window, since
  // cycling can activate a window on a different desk, leading to a desk-switch
  // animation launching. Getting the MRU window list for the active desk now
  // will always be for the current active desk, not the target active desk.
  aura::Window* active_window_after_window_cycle =
      window_util::GetActiveWindow();

  // Remove our key event filter.
  event_filter_.reset();

  if (active_window_after_window_cycle != nullptr &&
      active_window_before_window_cycle_ != active_window_after_window_cycle) {
    Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
        TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);

    ReportPossibleDesksSwitchStats(active_desk_container_id_before_cycle_);
  }

  active_window_before_window_cycle_ = nullptr;
  active_desk_container_id_before_cycle_ = kShellWindowId_Invalid;
}

}  // namespace ash
