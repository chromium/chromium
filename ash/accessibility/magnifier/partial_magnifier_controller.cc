// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/partial_magnifier_controller.h"

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

}  // namespace

PartialMagnifierController::PartialMagnifierController() {
  Shell::Get()->AddPreTargetHandler(this);

  MagnifierGlass::Params params = {kMagnificationScale, kMagnifierRadius};
  magnifier_glass_ = std::make_unique<MagnifierGlass>(std::move(params));
}

PartialMagnifierController::~PartialMagnifierController() {
  Shell::Get()->RemovePreTargetHandler(this);
}

void PartialMagnifierController::SetEnabled(bool enabled) {
  if (is_enabled_ == enabled)
    return;

  is_enabled_ = enabled;
  SetActive(false);
}

void PartialMagnifierController::SwitchTargetRootWindowIfNeeded(
    aura::Window* new_root_window) {
  if (new_root_window == current_root_window_)
    return;
  current_root_window_ = new_root_window;

  // |new_root_window| could be null, e.g. current mouse/touch point is beyond
  // the bounds of the displays. Deactivate and return.
  if (!current_root_window_) {
    SetActive(false);
    return;
  }

  DCHECK(current_root_window_->IsRootWindow());

  // Arbitrarily place the magnifier at the center of the new root window.
  // This function's caller is expected to call |ShowFor| afterwards with
  // more appropriate location if possible.
  if (is_enabled_ && is_active_) {
    magnifier_glass_->ShowFor(current_root_window_,
                              current_root_window_->bounds().CenterPoint());
  }
}

void PartialMagnifierController::OnTouchEvent(ui::TouchEvent* event) {
  OnLocatedEvent(event, event->pointer_details());
}

void PartialMagnifierController::SetActive(bool active) {
  // Fail if we're trying to activate while disabled.
  DCHECK(is_enabled_ || !active);

  if (is_active_ == active)
    return;

  is_active_ = active;
  if (!is_active_)
    magnifier_glass_->Close();
}

void PartialMagnifierController::OnLocatedEvent(
    ui::LocatedEvent* event,
    const ui::PointerDetails& pointer_details) {
  if (!is_enabled_)
    return;

  if (pointer_details.pointer_type != ui::EventPointerType::kPen) {
    return;
  }

  // Compute the event location in screen space.
  aura::Window* target = static_cast<aura::Window*>(event->target());
  aura::Window* event_root = target->GetRootWindow();
  gfx::Point screen_point = event->root_location();
  wm::ConvertPointToScreen(event_root, &screen_point);

  SwitchTargetRootWindowIfNeeded(event_root);

  const bool palette_contains_point =
      palette_utils::PaletteContainsPointInScreen(screen_point);
  // If the stylus is pressed on the palette icon or widget, do not activate.
  if (event->type() == ui::EventType::kTouchPressed &&
      !palette_contains_point) {
    SetActive(true);
  }

  if (event->type() == ui::EventType::kTouchReleased ||
      event->type() == ui::EventType::kTouchCancelled) {
    SetActive(false);
  }

  if (!is_active_)
    return;

  magnifier_glass_->ShowFor(current_root_window_, event->root_location());

  // If the stylus is over the palette icon or widget, do not consume the event.
  if (!palette_contains_point) {
    event->StopPropagation();
  }
}

}  // namespace ash
