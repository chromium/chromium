// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/filter_keys_event_rewriter.h"

#include <optional>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "base/time/time.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/types/event_type.h"

namespace ash {

FilterKeysEventRewriter::FilterKeysEventRewriter() {
  Shell::Get()->accessibility_controller()->SetFilterKeysEventRewriter(this);
}

FilterKeysEventRewriter::~FilterKeysEventRewriter() {
  Shell::Get()->accessibility_controller()->SetFilterKeysEventRewriter(nullptr);
}

void FilterKeysEventRewriter::SetBounceKeysEnabled(bool enabled) {
  bounce_keys_enabled_ = enabled;
  if (!bounce_keys_enabled_) {
    ResetBounceKeysState();
  }
}

bool FilterKeysEventRewriter::IsBounceKeysEnabled() const {
  return bounce_keys_enabled_;
}

void FilterKeysEventRewriter::SetBounceKeysDelay(base::TimeDelta delay) {
  bounce_keys_delay_ = delay;
}

const base::TimeDelta& FilterKeysEventRewriter::GetBounceKeysDelay() const {
  return bounce_keys_delay_;
}

ui::EventDispatchDetails FilterKeysEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  if (!IsBounceKeysEnabled() || !event.IsKeyEvent() || event.IsSynthesized()) {
    return SendEvent(continuation, &event);
  }

  const auto* key_event = event.AsKeyEvent();
  if (key_event->is_repeat()) {
    return SendEvent(continuation, &event);
  }

  const bool is_last_pressed_key =
      last_pressed_key_.has_value() &&
      last_pressed_key_.value() == key_event->code();
  if (!is_last_pressed_key) {
    if (key_event->type() == ui::EventType::kKeyPressed) {
      last_released_time_.reset();
      last_pressed_key_ = key_event->code();
    } else {
      return EraseNextKeyReleaseToDiscard(key_event->code())
                 ? DiscardEvent(continuation)
                 : SendEvent(continuation, &event);
    }
  }

  std::optional<base::TimeTicks> last_released_time = last_released_time_;
  if (key_event->type() == ui::EventType::kKeyReleased) {
    last_released_time_ = key_event->time_stamp();

    if (EraseNextKeyReleaseToDiscard(key_event->code())) {
      return DiscardEvent(continuation);
    }
  }

  if (last_released_time.has_value()) {
    auto elapsed_time = key_event->time_stamp() - last_released_time.value();
    if (elapsed_time < bounce_keys_delay_) {
      if (key_event->type() == ui::EventType::kKeyPressed) {
        next_key_release_to_discard_.insert(key_event->code());
      }
      return DiscardEvent(continuation);
    }
    last_released_time_.reset();
  }

  return SendEvent(continuation, &event);
}

bool FilterKeysEventRewriter::EraseNextKeyReleaseToDiscard(
    ui::DomCode dom_code) {
  return base::EraseIf(next_key_release_to_discard_,
                       [&dom_code](auto code) { return code == dom_code; });
}

void FilterKeysEventRewriter::ResetBounceKeysState() {
  last_pressed_key_.reset();
  last_released_time_.reset();
  next_key_release_to_discard_.clear();
}

}  // namespace ash
