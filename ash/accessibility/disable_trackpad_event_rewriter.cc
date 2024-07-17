// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/disable_trackpad_event_rewriter.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"

namespace ash {

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
  // TODO(b/259378560): Implement this method so that it cancels all events
  // from the built-in trackpad.
  return SendEvent(continuation, &event);
}

}  // namespace ash
