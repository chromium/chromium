// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_cycle_controller.h"

#include "ash/metrics/task_switch_metrics_recorder.h"
#include "ash/metrics/task_switch_source.h"
#include "ash/metrics/user_metrics_recorder.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/window_cycle_event_filter.h"
#include "ash/wm/window_cycle_list.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"

namespace ash {

namespace {

// Returns the most recently active window from the |window_list| or nullptr
// if the list is empty.
aura::Window* GetActiveWindow(const WindowCycleList::WindowList& window_list) {
  return window_list.empty() ? nullptr : window_list[0];
}

}  // namespace

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, public:

WindowCycleController::WindowCycleController() = default;

WindowCycleController::~WindowCycleController() = default;

// static
bool WindowCycleController::CanCycle() {
  // Prevent window cycling if the screen is locked or a modal dialog is open.
  return !Shell::Get()->session_controller()->IsScreenLocked() &&
         !Shell::IsSystemModalWindowOpen() &&
         !Shell::Get()->screen_pinning_controller()->IsPinned();
}

void WindowCycleController::HandleCycleWindow(Direction direction) {
  if (!CanCycle())
    return;

  if (!IsCycling())
    StartCycling();

  Step(direction);
}

void WindowCycleController::StartCycling() {
  WindowCycleList::WindowList window_list =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleWithPipList(
          kAllDesks);
  // Window cycle list windows will handle showing their transient related
  // windows, so if a window in |window_list| has a transient root also in
  // |window_list|, we can remove it as the transient root will handle showing
  // the window.
  window_util::RemoveTransientDescendants(&window_list);

  active_window_before_window_cycle_ = GetActiveWindow(window_list);

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

//////////////////////////////////////////////////////////////////////////////
// WindowCycleController, private:

void WindowCycleController::Step(Direction direction) {
  DCHECK(window_cycle_list_.get());
  window_cycle_list_->Step(direction);
}

void WindowCycleController::StopCycling() {
  window_cycle_list_.reset();

  aura::Window* active_window_after_window_cycle = GetActiveWindow(
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));

  // Remove our key event filter.
  event_filter_.reset();

  if (active_window_after_window_cycle != nullptr &&
      active_window_before_window_cycle_ != active_window_after_window_cycle) {
    Shell::Get()->metrics()->task_switch_metrics_recorder().OnTaskSwitch(
        TaskSwitchSource::WINDOW_CYCLE_CONTROLLER);
  }
  active_window_before_window_cycle_ = nullptr;
}

}  // namespace ash
