// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/filter_keys_event_rewriter.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"

namespace ash {

FilterKeysEventRewriter::FilterKeysEventRewriter() {
  Shell::Get()->accessibility_controller()->SetFilterKeysEventRewriter(this);
}

FilterKeysEventRewriter::~FilterKeysEventRewriter() {
  Shell::Get()->accessibility_controller()->SetFilterKeysEventRewriter(nullptr);
}

void FilterKeysEventRewriter::SetBounceKeysEnabled(bool enabled) {
  bounce_keys_enabled_ = enabled;
}

bool FilterKeysEventRewriter::IsBounceKeysEnabled() {
  return bounce_keys_enabled_;
}

ui::EventDispatchDetails FilterKeysEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (bounce_keys_enabled_) {
    // TODO(b/324272616): implement logic to ignore repeat key events within a
    // duration.
  }
  return SendEvent(continuation, &event);
}

}  // namespace ash
