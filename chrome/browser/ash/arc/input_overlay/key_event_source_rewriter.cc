// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/key_event_source_rewriter.h"

#include "ash/shell.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_source.h"

namespace arc::input_overlay {

KeyEventSourceRewriter::KeyEventSourceRewriter(aura::Window* top_level_window)
    : top_level_window_{top_level_window} {
  observation_.Observe(
      ash::Shell::GetPrimaryRootWindow()->GetHost()->GetEventSource());
}

KeyEventSourceRewriter::~KeyEventSourceRewriter() = default;

ui::EventDispatchDetails KeyEventSourceRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!event.IsKeyEvent()) {
    return SendEvent(continuation, &event);
  }
  auto* root_window = top_level_window_->GetRootWindow();
  return root_window->GetHost()->GetEventSource()->SendEventToSink(&event);
}

}  // namespace arc::input_overlay
