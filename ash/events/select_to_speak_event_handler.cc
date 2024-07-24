// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/select_to_speak_event_handler.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/accessibility_event_handler_manager.h"
#include "ash/public/cpp/select_to_speak_event_handler_delegate.h"
#include "ash/shell.h"
#include "base/containers/contains.h"
#include "ui/events/types/event_type.h"

namespace ash {

const ui::KeyboardCode kSpeakSelectionKey = ui::VKEY_S;

SelectToSpeakEventHandler::SelectToSpeakEventHandler(
    SelectToSpeakEventHandlerDelegate* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);
  Shell::Get()->AddAccessibilityEventHandler(
      this, AccessibilityEventHandlerManager::HandlerType::kSelectToSpeak);
}

SelectToSpeakEventHandler::~SelectToSpeakEventHandler() {
  Shell::Get()->RemoveAccessibilityEventHandler(this);
}

bool SelectToSpeakEventHandler::IsSelectToSpeakEnabled() {
  return Shell::Get()->accessibility_controller()->select_to_speak().enabled();
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
    touch_type_ = ui::EventPointerType::kUnknown;
  }
}

bool SelectToSpeakEventHandler::IsKeyDownForTesting(
    ui::KeyboardCode code) const {
  return keys_currently_down_.contains(code);
}

void SelectToSpeakEventHandler::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(IsSelectToSpeakEnabled());
  DCHECK(event);

  bool pressed = event->type() == ui::EventType::kKeyPressed;
  bool released = event->type() == ui::EventType::kKeyReleased;
  if (!(pressed || released)) {
    return;
  }

  ui::KeyboardCode key_code = event->key_code();
  if (key_code != kSpeakSelectionKey && key_code != ui::VKEY_LWIN &&
      key_code != ui::VKEY_RWIN && key_code != ui::VKEY_CONTROL) {
    // No need to track keys besides search, control and s.
    if (state_ == SEARCH_DOWN) {
      // If some other key was pressed and we were in SEARCH_DOWN state,
      // now we should be in inactive state.
      // Keys currently pressed hasn't changed so no need to dispatch
      // an event.
      state_ = INACTIVE;
    }
    return;
  }
  if (pressed) {
    keys_currently_down_.insert(key_code);
  } else {
    // This would help us catch any time we get out-of-sync between
    // Select to Speak and the actual keyboard. However, it shouldn't be
    // a fatal error since std::set::erase will still work properly
    // if it can't find the key, and STS will not have propagating bad
    // behavior if it missed a key press event.
    DCHECK(base::Contains(keys_currently_down_, key_code));
    keys_currently_down_.erase(key_code);
  }

  bool cancel_event = false;
  // Update the state when pressing and releasing the Search key (VKEY_LWIN).
  if (key_code == ui::VKEY_LWIN || key_code == ui::VKEY_RWIN) {
    if (pressed && state_ == INACTIVE) {
      state_ = SEARCH_DOWN;
    } else if (event->type() == ui::EventType::kKeyReleased) {
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
    if (pressed &&
        (state_ == SEARCH_DOWN || state_ == SPEAK_SELECTION_KEY_RELEASED)) {
      // They pressed the S key while search was down.
      // It's possible to press the selection key multiple times to read
      // the same region over and over, so state S_RELEASED can become state
      // CAPTURING_SPEAK_SELECTION_KEY if the search key is not lifted.
      cancel_event = true;
      state_ = CAPTURING_SPEAK_SELECTION_KEY;
    } else if (event->type() == ui::EventType::kKeyReleased) {
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
  // Rather than dispatching the raw key event, we send the total list of keys
  // currently down. This means the extension cannot get out-of-sync with the
  // state of the OS if a key event doesn't make it to the extension.
  delegate_->DispatchKeysCurrentlyDown(keys_currently_down_);

  if (cancel_event)
    CancelEvent(event);
}

void SelectToSpeakEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  DCHECK(IsSelectToSpeakEnabled());
  DCHECK(event);
  if (state_ == INACTIVE)
    return;

  if (event->type() == ui::EventType::kMousePressed) {
    if (state_ == SEARCH_DOWN || state_ == MOUSE_RELEASED)
      state_ = CAPTURING_MOUSE;
    else if (state_ == SELECTION_REQUESTED)
      state_ = CAPTURING_MOUSE_ONLY;
  }

  // We don't want mouse events to affect underlying UI when user is about to
  // select text to speak. (e.g. we don't want a hoverbox to appear/disappear)
  if (state_ == SELECTION_REQUESTED || state_ == SEARCH_DOWN) {
    CancelEvent(event);
    return;
  }

  if (state_ == WAIT_FOR_MOUSE_RELEASE &&
      event->type() == ui::EventType::kMouseReleased) {
    state_ = INACTIVE;
    return;
  }

  // Only forward the event to the extension if we are capturing mouse
  // events.
  if (state_ != CAPTURING_MOUSE && state_ != CAPTURING_MOUSE_ONLY)
    return;

  if (event->type() == ui::EventType::kMouseReleased) {
    if (state_ == CAPTURING_MOUSE)
      state_ = MOUSE_RELEASED;
    else if (state_ == CAPTURING_MOUSE_ONLY)
      state_ = INACTIVE;
  }

  // Forward the mouse event to the chrome process for the extension.
  delegate_->DispatchMouseEvent(*event);

  if (event->type() == ui::EventType::kMousePressed ||
      event->type() == ui::EventType::kMouseReleased ||
      event->type() == ui::EventType::kMouseDragged) {
    CancelEvent(event);
  }
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
  if (event->type() == ui::EventType::kTouchPressed &&
      state_ == SELECTION_REQUESTED && touch_id_ == ui::kPointerIdUnknown) {
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
  if (event->type() == ui::EventType::kTouchReleased &&
      state_ == CAPTURING_TOUCH_ONLY) {
    state_ = INACTIVE;
    touch_id_ = ui::kPointerIdUnknown;
    touch_type_ = ui::EventPointerType::kUnknown;
  }

  // Create a mouse event to send to the extension, describing the touch.
  // This is done because there is no RenderWidgetHost::ForwardTouchEvent,
  // and we already have mouse event plumbing in place for Select-to-Speak.
  ui::EventType type;
  switch (event->type()) {
    case ui::EventType::kTouchPressed:
      type = ui::EventType::kMousePressed;
      break;
    case ui::EventType::kTouchReleased:
    case ui::EventType::kTouchCancelled:
      type = ui::EventType::kMouseReleased;
      break;
    case ui::EventType::kTouchMoved:
      type = ui::EventType::kMouseDragged;
      break;
    default:
      return;
  }
  int flags = ui::EF_LEFT_MOUSE_BUTTON;

  // Get screen coordinates if available.
  gfx::Point root_location = event->target()
                                 ? event->target()->GetScreenLocation(*event)
                                 : event->root_location();
  ui::MouseEvent event_to_send(type, event->location(), root_location,
                               event->time_stamp(), flags, flags);

  delegate_->DispatchMouseEvent(event_to_send);

  if (event->type() != ui::EventType::kTouchMoved) {
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

std::string_view SelectToSpeakEventHandler::GetLogContext() const {
  return "SelectToSpeakEventHandler";
}

}  // namespace ash
