// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/views/picker_key_event_handler.h"

#include "ash/picker/views/picker_pseudo_focus_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/event_type.h"

namespace ash {
namespace {

ui::KeyEvent CreateKeyEvent(ui::KeyboardCode key_code,
                            int flags = ui::EF_NONE) {
  return ui::KeyEvent(ui::ET_KEY_PRESSED, key_code, ui::DomCode::NONE, flags,
                      ui::EventTimeForNow());
}

class MockPseudoFocusHandler : public PickerPseudoFocusHandler {
 public:
  MockPseudoFocusHandler() = default;
  MockPseudoFocusHandler(const MockPseudoFocusHandler&) = delete;
  MockPseudoFocusHandler& operator=(const MockPseudoFocusHandler&) = delete;
  ~MockPseudoFocusHandler() override = default;

  // PickerPseudoFocusHandler:
  bool DoPseudoFocusedAction() override { return true; }
  bool MovePseudoFocusUp() override { return true; }
  bool MovePseudoFocusDown() override { return true; }
  bool MovePseudoFocusLeft() override { return true; }
  bool MovePseudoFocusRight() override { return true; }
  bool AdvancePseudoFocus(PseudoFocusDirection direction) override {
    return true;
  }
  bool GainPseudoFocus(PseudoFocusDirection direction) override { return true; }
  void LosePseudoFocus() override { return; }
};

TEST(PickerKeyEventHandlerTest,
     DoesNotHandleKeyEventyWithoutPseudoFocusHandler) {
  PickerKeyEventHandler key_event_handler;

  EXPECT_FALSE(
      key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RETURN)));
}

TEST(PickerKeyEventHandlerTest, HandlesKeyEventWithPseudoFocusHandler) {
  PickerKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(
      key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_RETURN)));
}

TEST(PickerKeyEventHandlerTest, HandlesUnmodifedArrowKeyEvent) {
  PickerKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_UP)));
}

TEST(PickerKeyEventHandlerTest, DoesNotHandleModifiedArrowKeyEvent) {
  PickerKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_FALSE(key_event_handler.HandleKeyEvent(
      CreateKeyEvent(ui::VKEY_UP, ui::EF_SHIFT_DOWN)));
}

TEST(PickerKeyEventHandlerTest, HandlesTabKeyEvent) {
  PickerKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(CreateKeyEvent(ui::VKEY_TAB)));
}

TEST(PickerKeyEventHandlerTest, HandlesShiftTabKeyEvent) {
  PickerKeyEventHandler key_event_handler;
  MockPseudoFocusHandler pseudo_focus_handler;
  key_event_handler.SetActivePseudoFocusHandler(&pseudo_focus_handler);

  EXPECT_TRUE(key_event_handler.HandleKeyEvent(
      CreateKeyEvent(ui::VKEY_TAB, ui::EF_SHIFT_DOWN)));
}

}  // namespace
}  // namespace ash
