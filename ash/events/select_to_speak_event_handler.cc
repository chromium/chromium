// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/select_to_speak_event_handler.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/select_to_speak_event_handler_delegate.h"
#include "ash/shell.h"

namespace ash {

const ui::KeyboardCode kSpeakSelectionKey = ui::VKEY_S;

SelectToSpeakEventHandler::SelectToSpeakEventHandler(
    SelectToSpeakEventHandlerDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  Shell::Get()->AddPreTargetHandler(this,
                                    ui::EventTarget::Priority::kAccessibility);
}

SelectToSpeakEventHandler::~SelectToSpeakEventHandler() {
  Shell::Get()->RemovePreTargetHandler(this);
}

bool SelectToSpeakEventHandler::IsSelectToSpeakEnabled() {
  return Shell::Get()->accessibility_controller()->select_to_speak_enabled();
}

void SelectToSpeakEventHandler::SetSelectToSpeakStateSelecting(
    bool is_selecting) {
  if (is_selecting && state_ == INACTIVE) {
    // The extension has requested that it enter SELECTING state, and we
    // aren't already in a SELECTING state. Prepare to start capturing events
    // from stylus, mouse or touch.
    // If we are already in any state besides INACTIVE then there is no
    // work that needs to be done.
    state_ = SELECTION_REQUESTED;
  } else if (!is_selecting) {
    // If we were using search + mouse, continue to wait for the search key
    // up event by not resetting the state to INACTIVE.
    if (state_ != MOUSE_RELEASED)
      state_ = INACTIVE;
    touch_id_ = ui::kPointerIdUnknown;
    touch_type_ = ui::EventPointerType::POINTER_TYPE_UNKNOWN;
  }
}

void SelectToSpeakEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(IsSelectToSpeakEnabled());
  DCHECK(event);

  ui::KeyboardCode key_code = event->key_code();
  bool cancel_event = false;

  // Update the state when pressing and releasing the Search key (VKEY_LWIN).
  if (key_code == ui::VKEY_LWIN) {
    if (event->type() == ui::ET_KEY_PRESSED && state_ == INACTIVE) {
      state_ = SEARCH_DOWN;
    } else if (event->type() == ui::ET_KEY_RELEASED) {
      if (state_ == CAPTURING_MOUSE) {
        cancel_event = true;
        state_ = WAIT_FOR_MOUSE_RELEASE;
      } else if (state_ == MOUSE_RELEASED) {
        cancel_event = true;
        state_ = INACTIVE;
      } else if (state_ == CAPTURING_SPEAK_SELECTION_KEY) {
        cancel_event = true;
        state_ = WAIT_FOR_SPEAK_SELECTION_KEY_RELEASE;
      } else if (state_ == SPEAK_SELECTION_KEY_RELEASED) {
        cancel_event = true;
        state_ = INACTIVE;
      } else if (state_ == SEARCH_DOWN) {
        // They just tapped the search key without clicking the mouse.
        // Don't cancel this event -- the search key may still be used
        // by another part of Chrome, and we didn't use it here.
        state_ = INACTIVE;
      }
    }
  } else if (key_code == kSpeakSelectionKey) {
    if (event->type() == ui::ET_KEY_PRESSED &&
        (state_ == SEARCH_DOWN || state_ == SPEAK_SELECTION_KEY_RELEASED)) {
      // They pressed the S key while search was down.
      // It's possible to press the selection key multiple times to read
      // the same region over and over, so state S_RELEASED can become state
      // CAPTURING_SPEAK_SELECTION_KEY if the search key is not lifted.
      cancel_event = true;
      state_ = CAPTURING_SPEAK_SELECTION_KEY;
    } else if (event->type() == ui::ET_KEY_RELEASED) {
      if (state_ == CAPTURING_SPEAK_SELECTION_KEY) {
        // They released the speak selection key while it was being captured.
        cancel_event = true;
        state_ = SPEAK_SELECTION_KEY_RELEASED;
      } else if (state_ == WAIT_FOR_SPEAK_SELECTION_KEY_RELEASE) {
        // They have already released the search key
        cancel_event = true;
        state_ = INACTIVE;
      }
    }
  } else if (state_ == SEARCH_DOWN) {
    state_ = INACTIVE;
  }

  // Forward the key to the chrome process for the extension.
  delegate_->DispatchKeyEvent(*event);

  if (cancel_event)
    CancelEvent(event);
}

void SelectToSpeakEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(IsSelectToSpeakEnabled());
  DCHECK(event);
  if (state_ == INACTIVE)
    return;

  if (event->type() == ui::ET_MOUSE_PRESSED) {
    if (state_ == SEARCH_DOWN || state_ == MOUSE_RELEASED)
      state_ = CAPTURING_MOUSE;
    else if (state_ == SELECTION_REQUESTED)
      state_ = CAPTURING_MOUSE_ONLY;
  }

  if (state_ == WAIT_FOR_MOUSE_RELEASE &&
      event->type() == ui::ET_MOUSE_RELEASED) {
    state_ = INACTIVE;
    return;
  }

  // Only forward the event to the extension if we are capturing mouse
  // events.
  if (state_ != CAPTURING_MOUSE && state_ != CAPTURING_MOUSE_ONLY)
    return;

  if (event->type() == ui::ET_MOUSE_RELEASED) {
    if (state_ == CAPTURING_MOUSE)
      state_ = MOUSE_RELEASED;
    else if (state_ == CAPTURING_MOUSE_ONLY)
      state_ = INACTIVE;
  }

  delegate_->DispatchMouseEvent(*event);

  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_RELEASED)
    CancelEvent(event);
}

void SelectToSpeakEventHandler::OnTouchEvent(ui::TouchEvent* event) {
  DCHECK(IsSelectToSpeakEnabled());
  DCHECK(event);
  // Only capture touch events if selection was requested or we are capturing
  // touch events already.
  if (state_ != SELECTION_REQUESTED && state_ != CAPTURING_TOUCH_ONLY)
    return;

  // On a touch-down event, if selection was requested, we begin capturing
  // touch events.
  if (event->type() == ui::ET_TOUCH_PRESSED && state_ == SELECTION_REQUESTED &&
      touch_id_ == ui::kPointerIdUnknown) {
    state_ = CAPTURING_TOUCH_ONLY;
    touch_id_ = event->pointer_details().id;
    touch_type_ = event->pointer_details().pointer_type;
  }

  if (touch_id_ != event->pointer_details().id ||
      touch_type_ != event->pointer_details().pointer_type) {
    // If this was a different pointer, cancel the event and return early.
    // We only want to track one touch pointer at a time.
    CancelEvent(event);
    return;
  }

  // On a touch-up event, we go back to inactive state, but still forward the
  // event to the extension.
  if (event->type() == ui::ET_TOUCH_RELEASED &&
      state_ == CAPTURING_TOUCH_ONLY) {
    state_ = INACTIVE;
    touch_id_ = ui::kPointerIdUnknown;
    touch_type_ = ui::EventPointerType::POINTER_TYPE_UNKNOWN;
  }

  // Create a mouse event to send to the extension, describing the touch.
  // This is done because there is no RenderWidgetHost::ForwardTouchEvent,
  // and we already have mouse event plumbing in place for Select-to-Speak.
  ui::EventType type;
  switch (event->type()) {
    case ui::ET_TOUCH_PRESSED:
      type = ui::ET_MOUSE_PRESSED;
      break;
    case ui::ET_TOUCH_RELEASED:
    case ui::ET_TOUCH_CANCELLED:
      type = ui::ET_MOUSE_RELEASED;
      break;
    case ui::ET_TOUCH_MOVED:
      type = ui::ET_MOUSE_DRAGGED;
      break;
    default:
      return;
  }
  int flags = ui::EF_LEFT_MOUSE_BUTTON;
  ui::MouseEvent event_to_send(type, event->location(), event->root_location(),
                               event->time_stamp(), flags, flags);

  delegate_->DispatchMouseEvent(event_to_send);

  if (event->type() != ui::ET_TOUCH_MOVED) {
    // Don't cancel move events in case focus needs to change.
    CancelEvent(event);
  }
}

void SelectToSpeakEventHandler::CancelEvent(ui::Event* event) {
  DCHECK(event);
  if (event->cancelable()) {
    event->SetHandled();
    event->StopPropagation();
  }
}

}  // namespace ash
