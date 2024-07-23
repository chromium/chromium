// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/compat_mode/compat_mode_button.h"
#include "ash/components/arc/compat_mode/compat_mode_button_controller.h"

namespace arc {

CompatModeButton::CompatModeButton(CompatModeButtonController* controller,
                                   PressedCallback callback)
    : chromeos::FrameCenterButton(std::move(callback)),
      controller_(controller) {}

bool CompatModeButton::OnMousePressed(const ui::MouseEvent& event) {
  controller_->OnButtonPressed();
  return chromeos::FrameCenterButton::OnMousePressed(event);
}

void CompatModeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->type() == ui::EventType::kGestureTapDown) {
    controller_->OnButtonPressed();
  }
  chromeos::FrameCenterButton::OnGestureEvent(event);
}

}  // namespace arc
