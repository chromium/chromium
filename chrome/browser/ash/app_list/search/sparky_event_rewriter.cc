// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/sparky_event_rewriter.h"

#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"

namespace app_list {

ui::EventDispatchDetails SparkyEventRewriter::RewriteEvent(
    const ui::Event& event,
    const Continuation continuation) {
  // Observe control key press and control key release action.
  if (event.type() == ui::EventType::kKeyPressed &&
      event.AsKeyEvent()->key_code() == ui::VKEY_SHIFT) {
    shift_pressed_ = true;
  } else if (event.type() == ui::EventType::kKeyReleased &&
             event.AsKeyEvent()->key_code() == ui::VKEY_SHIFT) {
    shift_pressed_ = false;
  }

  // Accepts the supplied event without change.
  return SendEvent(continuation, &event);
}

}  // namespace app_list
