// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_key_event_handler.h"

#include "ash/picker/views/picker_key_event_target.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

ui::KeyEvent CreateKeyEvent(ui::KeyboardCode key_code) {
  return ui::KeyEvent(ui::ET_KEY_PRESSED, key_code, ui::DomCode::NONE,
                      ui::EF_NONE, ui::EventTimeForNow());
}

class MockPickerKeyEventTarget : public PickerKeyEventTarget {
 public:
  MockPickerKeyEventTarget() = default;
  MockPickerKeyEventTarget(const MockPickerKeyEventTarget&) = delete;
  MockPickerKeyEventTarget& operator=(const MockPickerKeyEventTarget&) = delete;
  ~MockPickerKeyEventTarget() override = default;

  // PickerKeyEventTarget:
  bool OnEnterKeyPressed() override { return true; }
};

TEST(PickerKeyEventHandlerTest, DoesNotHandleEnterKeyWithoutKeyEventTarget) {
  PickerKeyEventHandler key_event_handler;

  EXPECT_FALSE(
      key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RETURN)));
}

TEST(PickerKeyEventHandlerTest, HandlesEnterKeyWithKeyEventTarget) {
  PickerKeyEventHandler key_event_handler;
  MockPickerKeyEventTarget key_event_target;
  key_event_handler.SetActiveKeyEventTarget(&key_event_target);

  EXPECT_TRUE(
      key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RETURN)));
}

}  // namespace
}  // namespace ash
