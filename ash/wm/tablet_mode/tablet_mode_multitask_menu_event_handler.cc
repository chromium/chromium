// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_multitask_menu_event_handler.h"

#include "ash/accelerators/debug_commands.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_cue.h"
#include "ash/wm/tablet_mode/tablet_mode_multitask_menu.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/functional/bind.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

// The dimensions of the area that can activate the multitask menu.
constexpr gfx::SizeF kTargetAreaSize(510.f, 113.f);

}  // namespace

TabletModeMultitaskMenuEventHandler::TabletModeMultitaskMenuEventHandler() {
  multitask_cue_ = std::make_unique<TabletModeMultitaskCue>();
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

    multitask_cue_->DismissCue();
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
    case ui::ET_GESTURE_SCROLL_BEGIN:
      if (std::fabs(details.scroll_y_hint()) <
          std::fabs(details.scroll_x_hint())) {
        return;
      }
      if (!multitask_menu_ && details.scroll_y_hint() > 0) {
        MaybeCreateMultitaskMenu(active_window);
        multitask_menu_->BeginDrag(window_location.y(), /*down=*/true);
        event->SetHandled();
      } else if (multitask_menu_ && details.scroll_y_hint() < 0) {
        multitask_menu_->BeginDrag(window_location.y(), /*down=*/false);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_UPDATE:
      // While the menu is open and we are scrolling down, we mark
      // `SetHandled()` even if the event goes out of menu bounds to keep the
      // menu open. If we are scrolling up, we only handle events inside the
      // menu to avoid consuming them before `OnWidgetActivationChanged()`.
      if (multitask_menu_ && details.scroll_y() > 0) {
        multitask_menu_->UpdateDrag(window_location.y(), /*down=*/true);
        event->SetHandled();
      } else if (multitask_menu_ && details.scroll_y() < 0 &&
                 gfx::RectF(
                     multitask_menu_->widget()->GetWindowBoundsInScreen())
                     .Contains(screen_location)) {
        multitask_menu_->UpdateDrag(window_location.y(), /*down=*/false);
        event->SetHandled();
      }
      break;
    case ui::ET_GESTURE_SCROLL_END:
      if (multitask_menu_) {
        multitask_menu_->EndDrag();
        if (multitask_menu_) {
          // If `multitask_menu_` wasn't destroyed, it was dragged to show.
          RecordMultitaskMenuEntryType(
              chromeos::MultitaskMenuEntryType::kGestureScroll);
        }
        event->SetHandled();
      }
      break;
    case ui::ET_SCROLL_FLING_START:
      // Normally ET_GESTURE_SCROLL_BEGIN will fire first and have already
      // created the multitask menu, however occasionally ET_SCROLL_FLING_START
      // may fire first (https://crbug.com/821237).
      MaybeCreateMultitaskMenu(active_window);
      multitask_menu_->Animate(details.velocity_y() > 0);
      if (multitask_menu_) {
        // If `multitask_menu_` wasn't destroyed, it was animated to show.
        RecordMultitaskMenuEntryType(
            chromeos::MultitaskMenuEntryType::kGestureFling);
      }
      event->SetHandled();
      break;
    default:
      break;
  }
}

}  // namespace ash
