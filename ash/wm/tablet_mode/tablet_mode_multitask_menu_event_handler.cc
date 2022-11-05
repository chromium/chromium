// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/bind.h"
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

void TabletModeMultitaskMenuEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSEWHEEL)
    return;

  // Note that connecting a mouse normally puts the device in clamshell mode
  // unless a developer switch is enabled.
  if (!debug::DeveloperAcceleratorsEnabled())
    return;

  const float y_offset = event->AsMouseWheelEvent()->y_offset();
  if (y_offset == 0.f)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());

  // Close the multitask menu if it is the target and we have a upwards scroll.
  if (y_offset > 0.f && multitask_menu_ &&
      target == multitask_menu_->widget()->GetNativeWindow()) {
    multitask_menu_->AnimateClose();
    return;
  }

  if (multitask_menu_)
    return;

  aura::Window* active_window = window_util::GetActiveWindow();
  if (!active_window || !active_window->Contains(target) ||
      !WindowState::Get(active_window)->CanMaximize()) {
    return;
  }

  // Show the multitask menu if it is in the top quarter of the target and is a
  // downwards scroll.
  if (y_offset < 0.f &&
      event->location_f().y() < target->bounds().height() / 4.f) {
    multitask_menu_ = std::make_unique<TabletModeMultitaskMenu>(
        this, active_window,
        base::BindRepeating(
            &TabletModeMultitaskMenuEventHandler::ResetMultitaskMenu,
            base::Unretained(this)));
  }
}

void TabletModeMultitaskMenuEventHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* active_window = window_util::GetActiveWindow();
  auto* window_state = WindowState::Get(active_window);
  // No-op if there is no active window and no multitask menu, which might be
  // the active window. If the multitask menu is the active window, we still
  // want to handle events that might close the menu.
  if (!window_state ||
      (!multitask_menu_ &&
       (!active_window ||
        !active_window->Contains(static_cast<aura::Window*>(event->target())) ||
        window_state->IsFloated() || !window_state->CanMaximize()))) {
    return;
  }

  gfx::PointF screen_location = event->location_f();
  wm::ConvertPointToScreen(active_window, &screen_location);

  // If no drag is in process and the menu is open, only handle events inside
  // the menu.
  if (!is_drag_to_open_ && multitask_menu_ &&
      !gfx::RectF(multitask_menu_->widget()->GetWindowBoundsInScreen())
           .Contains(screen_location)) {
    return;
  }

  // If no drag is in process and the menu is closed, only handle events inside
  // the target area.
  if (!is_drag_to_open_ && !multitask_menu_ &&
      !gfx::RectF(active_window->GetBoundsInScreen().CenterPoint().x() -
                      kTargetAreaWidth / 2,
                  active_window->GetBoundsInScreen().y(), kTargetAreaWidth,
                  kTargetAreaHeight)
           .Contains(screen_location)) {
    return;
  }

  if (ProcessBeginFlingOrSwipe(*event)) {
    event->SetHandled();
    return;
  }

  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_TYPE_END:
    case ui::ET_GESTURE_END:
      if (is_drag_to_open_) {
        if (is_drag_to_open_.value()) {
          multitask_menu_ = std::make_unique<TabletModeMultitaskMenu>(
              this, active_window,
              base::BindRepeating(
                  &TabletModeMultitaskMenuEventHandler::ResetMultitaskMenu,
                  base::Unretained(this)));
        } else {
          // TODO(crbug.com/1363818): Handle drag direction changes if animation
          // is in progress.
          DCHECK(multitask_menu_);
          multitask_menu_->AnimateClose();
        }
        event->SetHandled();
        is_drag_to_open_.reset();
      }
      break;
    default:
      break;
  }
}

bool TabletModeMultitaskMenuEventHandler::ProcessBeginFlingOrSwipe(
    const ui::GestureEvent& event) {
  float detail_x, detail_y = 0.f;
  switch (event.type()) {
    case ui::ET_GESTURE_SCROLL_BEGIN:
      detail_x = event.details().scroll_x_hint();
      detail_y = event.details().scroll_y_hint();
      break;
    case ui::ET_SCROLL_FLING_START:
      detail_x = event.details().velocity_x();
      detail_y = event.details().velocity_y();
      break;
    case ui::ET_GESTURE_SWIPE: {
      const bool vertical =
          event.details().swipe_down() || event.details().swipe_down();
      detail_x = vertical ? 0.f : 1.f;
      if (vertical)
        detail_y = event.details().swipe_down() ? 1.f : -1.f;
      else
        detail_y = 0.f;
      break;
    }
    case ui::ET_GESTURE_SCROLL_UPDATE:
      detail_x = event.details().scroll_x();
      detail_y = event.details().scroll_y();
      break;
    default:
      return false;
  }

  // Do not handle horizontal gestures.
  if (std::fabs(detail_x) > std::fabs(detail_y)) {
    return false;
  }

  // Do not handle up events if menu is not shown.
  if (!multitask_menu_ && detail_y < 0.f)
    return false;

  // Do not handle down events if menu is shown.
  if (multitask_menu_ && detail_y > 0.f)
    return false;

  is_drag_to_open_.emplace(detail_y > 0);
  return true;
}

void TabletModeMultitaskMenuEventHandler::ResetMultitaskMenu() {
  multitask_menu_.reset();
}

}  // namespace ash
