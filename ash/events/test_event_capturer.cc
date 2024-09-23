// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/test_event_capturer.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_utils.h"

namespace ash {

TestEventCapturer::TestEventCapturer() {}
TestEventCapturer::~TestEventCapturer() {}

void TestEventCapturer::ClearEvents() {
  key_events_.clear();
  mouse_events_.clear();
  touch_events_.clear();
  wheel_events_.clear();
}

void TestEventCapturer::OnKeyEvent(ui::KeyEvent* event) {
  key_events_.push_back(*event);

  // If there is a possibility that we're in an infinite loop, we should
  // exit early with a sensible error rather than letting the test time out.
  ASSERT_LT(key_events_.size(), 100u);
}

void TestEventCapturer::OnMouseEvent(ui::MouseEvent* event) {
  bool save_event = false;
  bool stop_event = false;
  ui::EventType type = event->type();
  if (type == ui::EventType::kMousePressed ||
      type == ui::EventType::kMouseReleased) {
    // Only track left and right mouse button events, ensuring that we get
    // left-click, right-click and double-click.
    if (!(event->flags() & ui::EF_LEFT_MOUSE_BUTTON) &&
        (!(event->flags() & ui::EF_RIGHT_MOUSE_BUTTON))) {
      return;
    }
    save_event = true;
    // Stop event propagation so we don't click on random stuff that
    // might break test assumptions.
    stop_event = true;
  } else if (type == ui::EventType::kMouseDragged ||
             (capture_mouse_move_ && type == ui::EventType::kMouseMoved) ||
             (capture_mouse_enter_exit_ &&
              (type == ui::EventType::kMouseEntered ||
               type == ui::EventType::kMouseExited))) {
    save_event = true;
    stop_event = false;
  } else if (type == ui::EventType::kMousewheel) {
    // Save it immediately as a MouseWheelEvent.
    wheel_events_.push_back(ui::MouseWheelEvent(
        event->AsMouseWheelEvent()->offset(), event->location(),
        event->root_location(), ui::EventTimeForNow(), event->flags(),
        event->changed_button_flags()));
  }

  if (save_event) {
    mouse_events_.push_back(ui::MouseEvent(
        event->type(), event->location(), event->root_location(),
        ui::EventTimeForNow(), event->flags(), event->changed_button_flags()));
  }

  if (stop_event) {
    event->StopPropagation();
  }

  // If there is a possibility that we're in an infinite loop, we should
  // exit early with a sensible error rather than letting the test time out.
  ASSERT_LT(mouse_events_.size(), 100u);
  ASSERT_LT(wheel_events_.size(), 100u);
}

void TestEventCapturer::OnTouchEvent(ui::TouchEvent* event) {
  touch_events_.push_back(*event);

  // If there is a possibility that we're in an infinite loop, we should
  // exit early with a sensible error rather than letting the test time out.
  ASSERT_LT(touch_events_.size(), 100u);
}

ui::KeyEvent* TestEventCapturer::LastKeyEvent() {
  return key_events_.empty() ? nullptr : &key_events_.back();
}

ui::MouseEvent* TestEventCapturer::LastMouseEvent() {
  return mouse_events_.empty() ? nullptr : &mouse_events_.back();
}

ui::TouchEvent* TestEventCapturer::LastTouchEvent() {
  return touch_events_.empty() ? nullptr : &touch_events_.back();
}

std::string_view TestEventCapturer::GetLogContext() const {
  return "TestEventCapturer";
}

}  // namespace ash
