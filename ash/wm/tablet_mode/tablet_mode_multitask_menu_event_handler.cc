// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/events/event.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The dimensions of the region that can activate the multitask menu.
constexpr int kActivateRegionWidth = 300;
constexpr int kActivateRegionHeight = 100;

// Return true if `point` is in the region that can activate the multitask menu.
bool ActivateRegionHitTest(aura::Window* window,
                           const gfx::Point& point_in_screen) {
  const gfx::Rect bounds_in_screen = window->GetBoundsInScreen();
  const gfx::Rect multitask_menu_activate_region(
      bounds_in_screen.CenterPoint().x() - kActivateRegionWidth / 2,
      bounds_in_screen.y(), kActivateRegionWidth, kActivateRegionHeight);
  return multitask_menu_activate_region.Contains(point_in_screen);
}

bool IsVerticalGestureScroll(const ui::GestureEvent& event) {
  return std::fabs(event.details().scroll_y_hint()) >
         std::fabs(event.details().scroll_x_hint());
}

}  // namespace

TabletModeMultitaskMenuEventHandler::TabletModeMultitaskMenuEventHandler() {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeMultitaskMenuEventHandler::~TabletModeMultitaskMenuEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
  if (multitask_menu_)
    multitask_menu_->window()->RemoveObserver(this);
}

void TabletModeMultitaskMenuEventHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window || active_window != event->target() ||
      !WindowState::Get(active_window)->CanResize()) {
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      gfx::Point screen_location = event->location();
      wm::ConvertPointToScreen(active_window, &screen_location);
      // Only consider vertical scrolls in the activate region.
      if (ActivateRegionHitTest(active_window, screen_location) &&
          IsVerticalGestureScroll(*event)) {
        // Start showing the menu if swipe down.
        if (event->details().scroll_y_hint() > 0) {
          swipe_down_started_ = true;
          event->SetHandled();
        } else {
          // TODO(crbug.com/1349918): Start hide animation.
        }
      }
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (swipe_down_started_) {
        // TODO(crbug.com/1349918): Update show/hide animation.
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SWIPE: {
      // If a touch is released at high velocity, the scroll gesture
      // is "converted" to a swipe gesture. ET_GESTURE_END is still
      // sent after, and ET_GESTURE_SCROLL_END is sometimes sent after this.
      if (event->details().swipe_down()) {
        swipe_down_started_ = true;
        event->SetHandled();
      }
      break;
    }
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_END:
      if (swipe_down_started_) {
        ShowMultitaskMenu(active_window);
        event->SetHandled();
        swipe_down_started_ = false;
      }
      break;
    default:
      break;
  }
  // TODO(crbug.com/1349918): Hide multitask menu on swipe up and touches
  // outside the menu bounds.
}

void TabletModeMultitaskMenuEventHandler::OnWindowDestroying(
    aura::Window* window) {
  HideMultitaskMenu();
  if (multitask_menu_) {
    DCHECK_EQ(window, multitask_menu_->window());
    multitask_menu_->window()->RemoveObserver(this);
    multitask_menu_ = nullptr;
  }
}

void TabletModeMultitaskMenuEventHandler::ShowMultitaskMenu(
    aura::Window* active_window) {
  // TODO(sophiewen): Check that `active_window` is still active.
  if (!multitask_menu_) {
    multitask_menu_ = std::make_unique<TabletModeMultitaskMenu>(active_window);
    // TODO(crbug.com/1351464): Move WindowObserver to TabletModeMultitaskMenu.
    active_window->AddObserver(this);
  }
  multitask_menu_->Show();
}

void TabletModeMultitaskMenuEventHandler::HideMultitaskMenu() {
  if (multitask_menu_)
    multitask_menu_->Hide();
}

}  // namespace ash