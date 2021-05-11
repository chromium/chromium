// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/partial_magnification_controller.h"

#include "ash/accessibility/magnifier/magnifier_glass.h"
#include "ash/shell.h"
#include "ash/system/palette/palette_utils.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/types/event_type.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// Ratio of magnifier scale.
constexpr float kMagnificationScale = 2.f;
// Radius of the magnifying glass in DIP. This does not include the thickness
// of the magnifying glass shadow and border.
constexpr int kMagnifierRadius = 188;

aura::Window* GetCurrentRootWindow() {
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  for (aura::Window* root_window : root_windows) {
    if (root_window->ContainsPointInRoot(
            root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot()))
      return root_window;
  }
  return nullptr;
}

}  // namespace

PartialMagnificationController::PartialMagnificationController() {
  Shell::Get()->AddPreTargetHandler(this);

  MagnifierGlass::Params params = {kMagnificationScale, kMagnifierRadius};
  magnifier_glass_ = std::make_unique<MagnifierGlass>(std::move(params));
}

PartialMagnificationController::~PartialMagnificationController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void PartialMagnificationController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void PartialMagnificationController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void PartialMagnificationController::SetEnabled(bool enabled) {
  if (is_enabled_ == enabled)
    return;

  is_enabled_ = enabled;
  SetActive(false);
  for (auto& observer : observers_)
    observer.OnPartialMagnificationStateChanged(enabled);
}

void PartialMagnificationController::SwitchTargetRootWindowIfNeeded(
    aura::Window* new_root_window) {
  if (!new_root_window)
    new_root_window = GetCurrentRootWindow();

  if (is_enabled_ && is_active_) {
    magnifier_glass_->ShowFor(
        new_root_window,
        new_root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());
  }
}

void PartialMagnificationController::OnTouchEvent(ui::TouchEvent* event) {
  OnLocatedEvent(event, event->pointer_details());
}

void PartialMagnificationController::OnMouseEvent(ui::MouseEvent* event) {
  OnLocatedEvent(event, event->pointer_details());
}

void PartialMagnificationController::SetActive(bool active) {
  // Fail if we're trying to activate while disabled.
  DCHECK(is_enabled_ || !active);

  is_active_ = active;
  if (is_active_) {
    aura::Window* root_window = GetCurrentRootWindow();
    magnifier_glass_->ShowFor(
        root_window,
        root_window->GetHost()->dispatcher()->GetLastMouseLocationInRoot());
  } else {
    magnifier_glass_->Close();
  }
}

void PartialMagnificationController::OnLocatedEvent(
    ui::LocatedEvent* event,
    const ui::PointerDetails& pointer_details) {
  if (!is_enabled_)
    return;

  const bool is_mouse_event =
      pointer_details.pointer_type == ui::EventPointerType::kMouse;

  if (is_mouse_event && !allow_mouse_following_)
    return;

  if (pointer_details.pointer_type != ui::EventPointerType::kPen &&
      !is_mouse_event) {
    return;
  }

  // Compute the event location in screen space.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &screen_point);
  const bool palette_contains_point =
      palette_utils::PaletteContainsPointInScreen(screen_point);

  // If the stylus is pressed on the palette icon or widget, do not activate.
  if ((event->type() == ui::ET_TOUCH_PRESSED && !palette_contains_point) ||
      is_mouse_event) {
    SetActive(true);
  }

  if (event->type() == ui::ET_TOUCH_RELEASED)
    SetActive(false);

  if (!is_active_)
    return;

  aura::Window* root_window = GetCurrentRootWindow();
  // |root_window| could be null, e.g. current mouse/touch point is beyond the
  // bounds of the displays. Deactivate and return.
  if (!root_window) {
    SetActive(false);
    return;
  }

  // Remap point from where it was captured to the display it is actually on.
  gfx::Point point = event->root_location();
  aura::Window::ConvertPointToTarget(event_root, root_window, &point);
  magnifier_glass_->ShowFor(root_window, point);

  // If the stylus is over the palette icon or widget or if the magnifier is
  // following the mouse, do not consume the event.
  if (!palette_contains_point && !is_mouse_event)
    event->StopPropagation();
}

}  // namespace ash
