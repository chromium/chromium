// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/curtain/input_event_filter.h"

#include "ash/shell.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_rewriter.h"

namespace ash::curtain {

InputEventFilter::InputEventFilter(aura::Window* root_window,
                                   EventFilter filter)
    : root_window_(*root_window), filter_(filter) {
  root_window_->GetHost()->GetEventSource()->AddEventRewriter(this);
}

InputEventFilter::~InputEventFilter() {
  root_window_->GetHost()->GetEventSource()->RemoveEventRewriter(this);
}

ui::EventDispatchDetails InputEventFilter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  switch (filter_.Run(event)) {
    case FilterResult::kKeepEvent:
      return SendEvent(continuation, &event);
    case FilterResult::kSuppressEvent:
      return DiscardEvent(continuation);
  }
}

}  // namespace ash::curtain
