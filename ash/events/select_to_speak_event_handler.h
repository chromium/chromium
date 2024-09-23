// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_SELECT_TO_SPEAK_EVENT_HANDLER_H_
#define ASH_EVENTS_SELECT_TO_SPEAK_EVENT_HANDLER_H_

#include <set>

#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

class SelectToSpeakEventHandlerDelegate;

// SelectToSpeakEventHandler sends touch, mouse and key events to
// the Select-to-Speak extension (via the delegate) when it is enabled.
class ASH_EXPORT SelectToSpeakEventHandler : public ui::EventHandler {
 public:
  explicit SelectToSpeakEventHandler(
      SelectToSpeakEventHandlerDelegate* delegate);

  SelectToSpeakEventHandler(const SelectToSpeakEventHandler&) = delete;
  SelectToSpeakEventHandler& operator=(const SelectToSpeakEventHandler&) =
      delete;

  ~SelectToSpeakEventHandler() override;

  // Called when the Select-to-Speak extension changes state. |is_selecting| is
  // true if the extension wants to begin capturing mouse or touch events, in
  // which case this handler needs to start forwarding those events if it was
  // in an inactive state.
  void SetSelectToSpeakStateSelecting(bool is_selecting);

  bool IsKeyDownForTesting(ui::KeyboardCode code) const;

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  std::string_view GetLogContext() const override;

  // Returns true if Select to Speak is enabled.
  bool IsSelectToSpeakEnabled();

  void CancelEvent(ui::Event* event);

  enum State {
    // The search key is not down, no selection has been requested.
    // No other keys or mouse events are captured.
    INACTIVE,

    // The Search key is down but the mouse button and 'S' key are not.
    SEARCH_DOWN,

    // The user held down Search and clicked the mouse button. We're capturing
    // all mouse events from now on until either Search or the mouse button is
    // released.
    CAPTURING_MOUSE,

    // The mouse was released, but Search is still held down. If the user
    // clicks again, we'll go back to the state CAPTURING. This is different
    // than the state SEARCH_DOWN because we know the user clicked at least
    // once, so when Search is released we'll handle that event too, so as
    // to not trigger opening the Search UI.
    MOUSE_RELEASED,

    // The Search key was released while the mouse was still down, cancelling
    // the Select-to-Speak event. Stay in this mode until the mouse button
    // is released, too.
    WAIT_FOR_MOUSE_RELEASE,

    // The user held down Search and clicked the speak selection key. We're
    // waiting for the event where the speak selection key is released or the
    // search key is released.
    CAPTURING_SPEAK_SELECTION_KEY,

    // The user held down Search and clicked and released the speak selection
    // key. We will wait for the Search key to be released too. This is
    // different than SEARCH_DOWN because we know the user used the speak
    // selection key, so when Search is released we will capture that event to
    // not trigger opening the Search UI.
    SPEAK_SELECTION_KEY_RELEASED,

    // The Search key was released while the selection key was still down. Stay
    // in this mode until the speak selection key is released too.
    WAIT_FOR_SPEAK_SELECTION_KEY_RELEASE,

    // The user has clicked a button in the Chrome UI indicating they want to
    // start capturing mouse or touch events. The next mouse or touch events
    // should be captured.
    SELECTION_REQUESTED,

    // Mouse events are being captured, but do not need to wait for any key
    // events when the mouse is released, unlike WAIT_FOR_MOUSE_RELEASE.
    // Only mouse events will be captured until the mouse released event is
    // received.
    CAPTURING_MOUSE_ONLY,

    // Touch events are being captured, similar to CAPTURING_MOUSE_ONLY.
    CAPTURING_TOUCH_ONLY
  };

  State state_ = INACTIVE;

  ui::PointerId touch_id_ = ui::kPointerIdUnknown;

  ui::EventPointerType touch_type_ = ui::EventPointerType::kUnknown;

  std::set<ui::KeyboardCode> keys_currently_down_;

  // The delegate used to send key events to the Select-to-Speak extension.
  raw_ptr<SelectToSpeakEventHandlerDelegate> delegate_;
};

}  // namespace ash

#endif  // ASH_EVENTS_SELECT_TO_SPEAK_EVENT_HANDLER_H_
