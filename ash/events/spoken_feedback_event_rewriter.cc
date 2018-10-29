// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/spoken_feedback_event_rewriter.h"

#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event.h"
#include "ui/events/event_sink.h"

namespace ash {

SpokenFeedbackEventRewriter::SpokenFeedbackEventRewriter() = default;

SpokenFeedbackEventRewriter::~SpokenFeedbackEventRewriter() = default;

void SpokenFeedbackEventRewriter::SetDelegate(
    mojom::SpokenFeedbackEventRewriterDelegatePtr delegate) {
  delegate_ = std::move(delegate);
}

void SpokenFeedbackEventRewriter::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) const {
  DCHECK(event->IsKeyEvent()) << "Unexpected unhandled event type";
  // For now, these events are sent directly to the primary display's EventSink.
  // TODO: Pass the event to the original EventSource, not the primary one.
  ui::EventSource* source =
      Shell::GetPrimaryRootWindow()->GetHost()->GetEventSource();
  if (SendEventToEventSource(source, event.get()).dispatcher_destroyed) {
    VLOG(0) << "Undispatched key " << event->AsKeyEvent()->key_code()
            << " due to destroyed dispatcher.";
  }
}

ui::EventRewriteStatus SpokenFeedbackEventRewriter::RewriteEvent(
    const ui::Event& event,
    std::unique_ptr<ui::Event>* new_event) {
  if (!delegate_.is_bound() ||
      !Shell::Get()->accessibility_controller()->IsSpokenFeedbackEnabled())
    return ui::EVENT_REWRITE_CONTINUE;

  if (event.IsKeyEvent()) {
    const ui::KeyEvent* key_event = event.AsKeyEvent();

    bool capture = capture_all_keys_;

    // Always capture the Search key.
    capture |= key_event->IsCommandDown();

    // Don't capture tab as it gets consumed by Blink so never comes back
    // unhandled. In third_party/WebKit/Source/core/input/EventHandler.cpp, a
    // default tab handler consumes tab even when no focusable nodes are found;
    // it sets focus to Chrome and eats the event.
    if (key_event->GetDomKey() == ui::DomKey::TAB)
      capture = false;

    delegate_->DispatchKeyEventToChromeVox(ui::Event::Clone(event), capture);
    return capture ? ui::EVENT_REWRITE_DISCARD : ui::EVENT_REWRITE_CONTINUE;
  }

  if (send_mouse_events_ && event.IsMouseEvent()) {
    delegate_->DispatchMouseEventToChromeVox(ui::Event::Clone(event));
  }

  return ui::EVENT_REWRITE_CONTINUE;
}

ui::EventRewriteStatus SpokenFeedbackEventRewriter::NextDispatchEvent(
    const ui::Event& last_event,
    std::unique_ptr<ui::Event>* new_event) {
  NOTREACHED();
  return ui::EVENT_REWRITE_CONTINUE;
}

}  // namespace ash
