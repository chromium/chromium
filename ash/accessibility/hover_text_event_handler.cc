// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/hover_text_event_handler.h"

#include "ash/accessibility/accessibility_event_handler_manager.h"
#include "ash/accessibility/hover_text_event_handler_delegate.h"
#include "ash/shell.h"

namespace ash {

HoverTextEventHandler::HoverTextEventHandler(
    HoverTextEventHandlerDelegate* delegate)
    : delegate_(delegate) {
  Shell::Get()->AddAccessibilityEventHandler(
      this, AccessibilityEventHandlerManager::HandlerType::kHoverText);
}

HoverTextEventHandler::~HoverTextEventHandler() {
  if (IsActive()) {
    delegate_->OnHoverTextKeyEvent(false, /*stop_speech=*/true);
  }
  Shell::Get()->RemoveAccessibilityEventHandler(this);
}

bool HoverTextEventHandler::IsActive() const {
  return activation_key_held_ || tray_toggled_active_;
}

void HoverTextEventHandler::OnTrayToggled() {
  tray_toggled_active_ = !tray_toggled_active_;
  const bool active = IsActive();
  delegate_->OnHoverTextKeyEvent(active, /*stop_speech=*/!active);
}

void HoverTextEventHandler::OnHoverTextBubbleShown() {
  // TODO(b/568813139): Record that a bubble was shown while Search is held.
}

void HoverTextEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  // TODO(b/568813139): Forward rate-limited mouse move events.
}

void HoverTextEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  // TODO(b/568813139): Stop raw touch propagation and force gesture
  // recognition for touches pressed while active.
}

void HoverTextEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  // TODO(b/568813139): Forward and consume gestures synthesized from touches
  // pressed while active.
}

void HoverTextEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  // TODO(b/568813139): Dispatch key events to helper methods.
}

void HoverTextEventHandler::HandleSearchKeyEvent(ui::KeyEvent* event) {
  // TODO(b/568813139): Handle Search key press and release activation state.
}

void HoverTextEventHandler::HandleControlKeyEvent(const ui::KeyEvent& event) {
  // TODO(b/568813139): Stop active speech on Control key press.
}

void HoverTextEventHandler::CancelActivationChord() {
  // TODO(b/568813139): Cancel Search key activation chord when another key is
  // pressed.
}

void HoverTextEventHandler::SetActivationKeyHeld(bool is_key_held,
                                                 bool stop_speech) {
  activation_key_held_ = is_key_held;
  delegate_->OnHoverTextKeyEvent(IsActive(), stop_speech);
}

}  // namespace ash
