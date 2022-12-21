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
constexpr gfx::SizeF kTargetAreaSize(510.f, 113.f);

}  // namespace

TabletModeMultitaskMenuEventHandler::TabletModeMultitaskMenuEventHandler() {
  Shell::Get()->AddPreTargetHandler(this);
}

TabletModeMultitaskMenuEventHandler::~TabletModeMultitaskMenuEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void TabletModeMultitaskMenuEventHandler::MaybeCreateMultitaskMenu(
    aura::Window* active_window) {
  if (!multitask_menu_) {
    multitask_menu_ =
        std::make_unique<TabletModeMultitaskMenu>(this, active_window);
  }
}

void TabletModeMultitaskMenuEventHandler::ResetMultitaskMenu() {
  multitask_menu_.reset();
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
    multitask_menu_->Animate(/*show=*/false);
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
    MaybeCreateMultitaskMenu(active_window);
    multitask_menu_->Animate(/*show=*/true);
  }
}

void TabletModeMultitaskMenuEventHandler::OnGestureEvent(
    ui::GestureEvent* event) {
  aura::Window* active_window = window_util::GetActiveWindow();
  auto* window_state = WindowState::Get(active_window);
  // No-op if there is no multitask menu and no active window that can open the
  // menu. If the menu is open, the checks in the second condition do not apply
  // since the menu is the active window.
  if (!multitask_menu_ && (!active_window || window_state->IsFloated() ||
                           !window_state->CanMaximize())) {
    return;
  }

  aura::Window* target = static_cast<aura::Window*>(event->target());
  gfx::PointF screen_location = event->location_f();
  wm::ConvertPointToScreen(target, &screen_location);

  // If the menu is closed, only handle events inside the target area that might
  // open the menu.
  const gfx::RectF window_bounds(active_window->GetBoundsInScreen());
  gfx::RectF target_area(window_bounds);
  target_area.ClampToCenteredSize(kTargetAreaSize);
  target_area.set_y(window_bounds.y());
  if (!multitask_menu_ && !target_area.Contains(screen_location)) {
    return;
  }

  // Save the window coordinates to pass to the menu.
  gfx::PointF window_location = event->location_f();
  aura::Window::ConvertPointToTarget(target, active_window, &window_location);

  const ui::GestureEventDetails details = event->details();
  switch (event->type()) {
    case ui::ET_GESTURE_SWIPE:
      if (!details.swipe_up() && !details.swipe_down())
        return;
      MaybeCreateMultitaskMenu(active_window);
      multitask_menu_->Animate(/*show=*/details.swipe_down());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (std::fabs(details.scroll_y_hint()) <=
          std::fabs(details.scroll_x_hint())) {
        return;
      }
      MaybeCreateMultitaskMenu(active_window);
      multitask_menu_->BeginDrag(window_location.y());
      event->SetHandled();
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      if (multitask_menu_) {
        multitask_menu_->UpdateDrag(window_location.y());
        event->SetHandled();
      }
      break;
    case ui::ET_SCROLL_FLING_CANCEL:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_GESTURE_END:
      if (multitask_menu_) {
        multitask_menu_->EndDrag();
        event->SetHandled();
      }
      break;
    default:
      break;
  }
}

}  // namespace ash
