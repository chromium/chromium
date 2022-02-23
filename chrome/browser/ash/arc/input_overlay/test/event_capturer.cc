// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/event_capturer.h"

namespace arc {
namespace input_overlay {
namespace test {
EventCapturer::EventCapturer() = default;
EventCapturer::~EventCapturer() = default;

void EventCapturer::Clear() {
  key_events_.clear();
  touch_events_.clear();
  mouse_events_.clear();
}

void EventCapturer::OnKeyEvent(ui::KeyEvent* event) {
  key_events_.emplace_back(std::make_unique<ui::KeyEvent>(*event));
}

void EventCapturer::OnTouchEvent(ui::TouchEvent* event) {
  touch_events_.emplace_back(std::make_unique<ui::TouchEvent>(*event));
}

void EventCapturer::OnMouseEvent(ui::MouseEvent* event) {
  mouse_events_.emplace_back(std::make_unique<ui::MouseEvent>(*event));
}

}  // namespace test
}  // namespace input_overlay
}  // namespace arc
