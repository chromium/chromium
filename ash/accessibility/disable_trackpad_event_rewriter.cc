// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_trackpad_event_rewriter.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"

namespace ash {

namespace {
bool IsFromInternalTrackpad(const ui::Event& event) {
  // TODO(b/354176487): Implement checking for internal trackpad.
  return true;
}
}  // namespace

DisableTrackpadEventRewriter::DisableTrackpadEventRewriter() {
  Shell::Get()->accessibility_controller()->SetDisableTrackpadEventRewriter(
      this);
}

DisableTrackpadEventRewriter::~DisableTrackpadEventRewriter() {
  Shell::Get()->accessibility_controller()->SetDisableTrackpadEventRewriter(
      nullptr);
}

void DisableTrackpadEventRewriter::SetEnabled(bool enabled) {
  enabled_ = enabled;
}

bool DisableTrackpadEventRewriter::IsEnabled() {
  return enabled_;
}

ui::EventDispatchDetails DisableTrackpadEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!IsEnabled()) {
    return SendEvent(continuation, &event);
  }

  if (event.IsKeyEvent()) {
    HandleKeyEvent(event.AsKeyEvent());
  }

  if (event.IsMouseEvent() && IsFromInternalTrackpad(event)) {
    return DiscardEvent(continuation);
  }

  return SendEvent(continuation, &event);
}

void DisableTrackpadEventRewriter::HandleKeyEvent(const ui::KeyEvent* event) {
  // TODO(b/361611253): Make sure to check for control presses within a 10
  // second window.
  if (event->type() == ui::EventType::kKeyPressed) {
    if (event->key_code() == ui::VKEY_CONTROL) {
      ++control_press_count_;
    } else {
      control_press_count_ = 0;
    }
  }

  if (control_press_count_ >= 5) {
    SetEnabled(false);
    control_press_count_ = 0;
  }
}

}  // namespace ash
