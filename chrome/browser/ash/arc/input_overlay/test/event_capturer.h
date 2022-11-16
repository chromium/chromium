// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_EVENT_CAPTURER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_EVENT_CAPTURER_H_

#include "ui/events/event.h"
#include "ui/events/event_handler.h"

namespace arc::input_overlay {
namespace test {

// EventCapturer captures events of different types for unit tests.
class EventCapturer : public ui::EventHandler {
 public:
  EventCapturer();
  EventCapturer(const EventCapturer&) = delete;
  EventCapturer& operator=(const EventCapturer&) = delete;
  ~EventCapturer() override;

  void Clear();

  std::vector<std::unique_ptr<ui::KeyEvent>>& key_events() {
    return key_events_;
  }
  std::vector<std::unique_ptr<ui::TouchEvent>>& touch_events() {
    return touch_events_;
  }
  std::vector<std::unique_ptr<ui::MouseEvent>>& mouse_events() {
    return mouse_events_;
  }

 private:
  // EventHandler overrides:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

  std::vector<std::unique_ptr<ui::KeyEvent>> key_events_;
  std::vector<std::unique_ptr<ui::TouchEvent>> touch_events_;
  std::vector<std::unique_ptr<ui::MouseEvent>> mouse_events_;
};
}  // namespace test
}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_EVENT_CAPTURER_H_
