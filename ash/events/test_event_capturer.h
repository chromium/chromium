// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_TEST_EVENT_CAPTURER_H_
#define ASH_EVENTS_TEST_EVENT_CAPTURER_H_

#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace ash {

// Used to capture and inspect events in ash_unittests.  By default it captures
// all KeyEvents, MouseEvents, and TouchEvents.
// EventType::kMouseMoved, EventType::kMouseEntered and EventType::kMouseExited
// can be optionally filtered out to make the stored events less noisy.
class TestEventCapturer : public ui::EventHandler {
 public:
  TestEventCapturer();
  TestEventCapturer(const TestEventCapturer&) = delete;
  TestEventCapturer& operator=(const TestEventCapturer&) = delete;
  ~TestEventCapturer() override;

  void ClearEvents();

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  std::string_view GetLogContext() const override;

  ui::KeyEvent* LastKeyEvent();
  ui::MouseEvent* LastMouseEvent();
  ui::TouchEvent* LastTouchEvent();

  void set_capture_mouse_move(bool value) { capture_mouse_move_ = value; }
  void set_capture_mouse_enter_exit(bool value) {
    capture_mouse_enter_exit_ = value;
  }

  const std::vector<ui::KeyEvent>& key_events() const { return key_events_; }
  const std::vector<ui::MouseEvent>& mouse_events() const {
    return mouse_events_;
  }
  const std::vector<ui::TouchEvent>& touch_events() const {
    return touch_events_;
  }
  const std::vector<ui::MouseWheelEvent>& captured_mouse_wheel_events() const {
    return wheel_events_;
  }

 private:
  bool capture_mouse_move_ = true;
  bool capture_mouse_enter_exit_ = true;
  std::vector<ui::KeyEvent> key_events_;
  std::vector<ui::MouseEvent> mouse_events_;
  std::vector<ui::TouchEvent> touch_events_;
  std::vector<ui::MouseWheelEvent> wheel_events_;
};

}  // namespace ash

#endif  // ASH_EVENTS_TEST_EVENT_CAPTURER_H_
