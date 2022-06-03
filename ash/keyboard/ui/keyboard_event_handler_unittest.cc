// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_event_handler.h"

#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"

namespace keyboard {

TEST(KeyboardEventHandlerTest, HandleGestureEvents) {
  // KeyboardEventHandler needs a KeyboardUIController to be present, otherwise
  // handling gesture events will crash.
  KeyboardUIController controller;

  KeyboardEventHandler filter;
  ui::GestureEvent pinch_begin(
      15, 15, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_PINCH_BEGIN));
  ui::GestureEvent pinch_update(
      20, 20, 0, base::TimeTicks(),
      ui::GestureEventDetails(ui::ET_GESTURE_PINCH_UPDATE));
  ui::GestureEvent pinch_end(30, 30, 0, base::TimeTicks(),
                             ui::GestureEventDetails(ui::ET_GESTURE_PINCH_END));
  filter.OnGestureEvent(&pinch_begin);
  filter.OnGestureEvent(&pinch_update);
  filter.OnGestureEvent(&pinch_end);

  EXPECT_TRUE(pinch_begin.stopped_propagation());
  EXPECT_TRUE(pinch_update.stopped_propagation());
  EXPECT_TRUE(pinch_end.stopped_propagation());

  ui::GestureEvent tap(15, 15, 0, base::TimeTicks(),
                       ui::GestureEventDetails(ui::ET_GESTURE_TAP));
  filter.OnGestureEvent(&tap);
  EXPECT_FALSE(tap.stopped_propagation());
}

}  // namespace keyboard
