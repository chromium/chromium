// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/fast_ink/fast_ink_pointer_controller.h"

#include "ash/public/cpp/stylus_utils.h"
#include "ui/aura/window.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace fast_ink {
namespace {

// The amount of time used to estimate time from VSYNC event to when
// visible light can be noticed by the user.
const int kPresentationDelayMs = 18;

}  // namespace

// TODO(llin): Update |enabled_for_mouse_event_| based on whether the user has
// been interacted with a stylus.
FastInkPointerController::FastInkPointerController()
    : presentation_delay_(
          base::TimeDelta::FromMilliseconds(kPresentationDelayMs)),
      enabled_for_mouse_event_(!ash::stylus_utils::HasStylusInput()) {
  input_device_event_observation_.Observe(ui::DeviceDataManager::GetInstance());
}

FastInkPointerController::~FastInkPointerController() {}

void FastInkPointerController::SetEnabled(bool enabled) {
  enabled_ = enabled;
  // Not calling DestroyPointerView when disabling, leaving the control over
  // the pointer view lifetime to the specific controller implementation.
  // For instance, a controller might prefer to keep the pointer view around
  // while it is being animated away.
}

void FastInkPointerController::AddExcludedWindow(aura::Window* window) {
  DCHECK(window);
  excluded_windows_.Add(window);
}

bool FastInkPointerController::CanStartNewGesture(ui::LocatedEvent* event) {
  if (IsPointerInExcludedWindows(event))
    return false;

  // 1) The stylus/finger is pressed.
  // 2) The stylus/finger is moving, but the pointer session has not started yet
  // (most likely because the preceding press event was consumed by another
  // handler).
  bool can_start_on_touch_event =
      event->type() == ui::ET_TOUCH_PRESSED ||
      (event->type() == ui::ET_TOUCH_MOVED && !GetPointerView());
  if (can_start_on_touch_event)
    return true;

  // 1) The mouse is pressed.
  // 2) The mouse is moving, but the pointer session has not started yet
  // (most likely because the preceding press event was consumed by another
  // handler).
  bool can_start_on_mouse_event =
      event->type() == ui::ET_MOUSE_PRESSED ||
      (event->type() == ui::ET_MOUSE_MOVED && !GetPointerView());
  if (can_start_on_mouse_event)
    return true;

  return false;
}

bool FastInkPointerController::ShouldProcessEvent(ui::LocatedEvent* event) {
  return event->type() == ui::ET_TOUCH_RELEASED ||
         event->type() == ui::ET_TOUCH_MOVED ||
         event->type() == ui::ET_TOUCH_PRESSED ||
         event->type() == ui::ET_MOUSE_PRESSED ||
         event->type() == ui::ET_MOUSE_RELEASED ||
         event->type() == ui::ET_MOUSE_MOVED;
}

bool FastInkPointerController::IsPointerInExcludedWindows(
    ui::LocatedEvent* event) {
  gfx::Point screen_location = event->location();
  aura::Window* event_target = static_cast<aura::Window*>(event->target());
  wm::ConvertPointToScreen(event_target, &screen_location);

  for (const auto* excluded_window : excluded_windows_.windows()) {
    if (excluded_window->GetBoundsInScreen().Contains(screen_location)) {
      return true;
    }
  }

  return false;
}

bool FastInkPointerController::MaybeCreatePointerView(
    ui::LocatedEvent* event,
    bool can_start_new_gesture) {
  aura::Window* root_window =
      static_cast<aura::Window*>(event->target())->GetRootWindow();
  if (can_start_new_gesture) {
    DestroyPointerView();
    CreatePointerView(presentation_delay_, root_window);
  } else {
    views::View* pointer_view = GetPointerView();
    if (!pointer_view)
      return false;
    views::Widget* widget = pointer_view->GetWidget();
    if (widget->IsClosed() ||
        widget->GetNativeWindow()->GetRootWindow() != root_window) {
      // The pointer widget is no longer valid, end the current pointer session.
      DestroyPointerView();
      return false;
    }
  }

  return true;
}

void FastInkPointerController::OnTouchEvent(ui::TouchEvent* event) {
  const int touch_id = event->pointer_details().id;
  // Keep track of touch point count.
  if (event->type() == ui::ET_TOUCH_PRESSED)
    touch_ids_.insert(touch_id);
  if (event->type() == ui::ET_TOUCH_RELEASED ||
      event->type() == ui::ET_TOUCH_CANCELLED) {
    auto iter = touch_ids_.find(touch_id);

    // Can happen if this object is constructed while fingers were down.
    if (iter == touch_ids_.end())
      return;

    touch_ids_.erase(touch_id);
  }

  if (!enabled_)
    return;

  // Disable on touch events if the device has stylus.
  if (ash::stylus_utils::HasStylusInput() &&
      event->pointer_details().pointer_type != ui::EventPointerType::kPen) {
    return;
  }

  // Disable for multiple fingers touch.
  if (touch_ids_.size() > 1) {
    DestroyPointerView();
    return;
  }

  if (!ShouldProcessEvent(event))
    return;

  // Update pointer view and stop event propagation if pointer view is
  // available.
  if (MaybeCreatePointerView(event, CanStartNewGesture(event))) {
    UpdatePointerView(event);
    event->StopPropagation();
  }
}

void FastInkPointerController::OnMouseEvent(ui::MouseEvent* event) {
  if (!enabled_ || !enabled_for_mouse_event_ || !ShouldProcessEvent(event))
    return;

  // Update pointer view and stop event propagation if pointer view is
  // available.
  if (MaybeCreatePointerView(event, CanStartNewGesture(event))) {
    UpdatePointerView(event);
    event->StopPropagation();
  }
}

void FastInkPointerController::OnDeviceListsComplete() {
  enabled_for_mouse_event_ = !ash::stylus_utils::HasStylusInput();
}

}  // namespace fast_ink
