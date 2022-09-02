// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The dimensions of the area that can activate the multitask menu.
constexpr int kTargetAreaWidth = 510;
constexpr int kTargetAreaHeight = 113;

}  // namespace

TabletModeMultitaskMenuEventHandler::TabletModeMultitaskMenuEventHandler() {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeMultitaskMenuEventHandler::~TabletModeMultitaskMenuEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void TabletModeMultitaskMenuEventHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* active_window = window_util::GetActiveWindow();
  if (!multitask_menu_ &&
      (!active_window ||
       !active_window->Contains(static_cast<aura::Window*>(event->target())) ||
       !WindowState::Get(active_window)->CanMaximize())) {
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN: {
      // Only vertical scrolls can open the menu.
      if (std::fabs(event->details().scroll_y_hint()) <=
          std::fabs(event->details().scroll_x_hint())) {
        break;
      }

      gfx::Point screen_location = event->location();
      wm::ConvertPointToScreen(active_window, &screen_location);
      float y_delta = event->details().scroll_y_hint();
      if (multitask_menu_ && y_delta < 0.f &&
          multitask_menu_->multitask_menu_widget()
              ->GetWindowBoundsInScreen()
              .Contains(screen_location)) {
        // If menu is open, we should close on a swipe up.
        is_drag_down_.emplace(false);
        event->SetHandled();
      } else if (!multitask_menu_ && y_delta > 0.f &&
                 gfx::Rect(
                     active_window->GetBoundsInScreen().CenterPoint().x() -
                         kTargetAreaWidth / 2,
                     0, kTargetAreaWidth, kTargetAreaHeight)
                     .Contains(screen_location)) {
        // Otherwise, we should show the menu on a swipe.
        is_drag_down_.emplace(true);
        event->SetHandled();
      }
      break;
    }
    case ui::ET_SCROLL_FLING_START:
      if (event->details().velocity_y() > event->details().velocity_x()) {
        is_drag_down_.emplace(event->details().velocity_y() > 0);
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE: {
      if (is_drag_down_) {
        is_drag_down_.emplace(event->details().scroll_y() > 0);
        event->SetHandled();
      }
      break;
    }
    case ui::ET_GESTURE_SWIPE: {
      // If a touch is released at high velocity, the scroll gesture
      // is "converted" to a swipe gesture. ET_GESTURE_END is still
      // sent after, and ET_GESTURE_SCROLL_END is sometimes sent after this.
      if (event->details().swipe_down() || event->details().swipe_up()) {
        is_drag_down_.emplace(event->details().swipe_down());
        event->SetHandled();
      }
      break;
    }
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_TYPE_END:
    case ui::ET_GESTURE_END:
      if (is_drag_down_.has_value()) {
        if (is_drag_down_.value()) {
          ShowMultitaskMenu(active_window);
        } else {
          CloseMultitaskMenu();
        }
        event->SetHandled();
        is_drag_down_.reset();
      }
      break;
    default:
      break;
  }
}

void TabletModeMultitaskMenuEventHandler::ShowMultitaskMenu(
    aura::Window* active_window) {
  multitask_menu_ =
      std::make_unique<TabletModeMultitaskMenu>(this, active_window);
  multitask_menu_->Show();
}

void TabletModeMultitaskMenuEventHandler::CloseMultitaskMenu() {
  multitask_menu_.reset();
}

}  // namespace ash