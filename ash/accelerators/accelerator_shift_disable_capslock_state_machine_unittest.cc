// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_shift_disable_capslock_state_machine.h"

#include <variant>

#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/stub_input_controller.h"

namespace ash::accelerators {

namespace {

class MockInputController : public ui::StubInputController {
 public:
  MOCK_METHOD(bool, AreAnyKeysPressed, (), (override));
};

const bool kKeysPressed = true;
const bool kNoKeysPressed = false;

using EventTypeVariant = std::variant<ui::MouseEvent, ui::KeyEvent, bool>;
using ShiftDisableState =
    AcceleratorShiftDisableCapslockStateMachine::ShiftDisableState;

ui::MouseEvent MousePress() {
  return ui::MouseEvent(ui::EventType::kMousePressed,
                        /*location=*/gfx::PointF{},
                        /*root_location=*/gfx::PointF{},
                        /*time_stamp=*/{}, ui::EF_LEFT_MOUSE_BUTTON,
                        ui::EF_LEFT_MOUSE_BUTTON);
}

ui::MouseEvent MouseRelease() {
  return ui::MouseEvent(ui::EventType::kMouseReleased,
                        /*location=*/gfx::PointF{},
                        /*root_location=*/gfx::PointF{},
                        /*time_stamp=*/{}, ui::EF_NONE,
                        ui::EF_LEFT_MOUSE_BUTTON);
}

ui::KeyEvent KeyPress(ui::KeyboardCode key_code) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, key_code, ui::EF_NONE);
}

ui::KeyEvent KeyRelease(ui::KeyboardCode key_code) {
  return ui::KeyEvent(ui::EventType::kKeyReleased, key_code, ui::EF_NONE);
}

ui::Event& GetEventFromVariant(EventTypeVariant& event) {
  if (std::holds_alternative<ui::MouseEvent>(event)) {
    return std::get<ui::MouseEvent>(event);
  } else {
    return std::get<ui::KeyEvent>(event);
  }
}

}  // namespace

class AcceleratorShiftDisableCapslockStateMachineTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<std::vector<EventTypeVariant>, ShiftDisableState>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    std::tie(events_, expected_state_) = GetParam();

    input_controller_ = std::make_unique<MockInputController>();
    shift_disable_state_machine_ =
        std::make_unique<AcceleratorShiftDisableCapslockStateMachine>(
            input_controller_.get());
  }

  void TearDown() override {
    shift_disable_state_machine_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<AcceleratorShiftDisableCapslockStateMachine>
      shift_disable_state_machine_;
  std::unique_ptr<MockInputController> input_controller_;
  std::vector<EventTypeVariant> events_;
  ShiftDisableState expected_state_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AcceleratorShiftDisableCapslockStateMachineTest,
    testing::ValuesIn(std::vector<std::tuple<std::vector<EventTypeVariant>,
                                             ShiftDisableState>>{
        // Shift -> Release Shift allows the disable to trigger.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), kNoKeysPressed,
          KeyRelease(ui::VKEY_SHIFT)},
         ShiftDisableState::kTrigger},

        // A -> A release goes back to starting state.
        {{kKeysPressed, KeyPress(ui::VKEY_A), kNoKeysPressed,
          KeyRelease(ui::VKEY_A)},
         ShiftDisableState::kStart},

        // Pressing A moves to suppress state.
        {{kKeysPressed, KeyPress(ui::VKEY_A)}, ShiftDisableState::kSuppress},

        // A -> Shift -> Release shift keeps us in suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_A), KeyPress(ui::VKEY_SHIFT),
          KeyRelease(ui::VKEY_SHIFT)},
         ShiftDisableState::kSuppress},

        // If any key is released, stay in starting state.
        {{KeyRelease(ui::VKEY_A)}, ShiftDisableState::kStart},

        // If shift is presesd twice, stay in primed state.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), KeyPress(ui::VKEY_RSHIFT)},
         ShiftDisableState::kPrimed},

        // If any key is pressed once in primed state, go to suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_LSHIFT), KeyPress(ui::VKEY_A)},
         ShiftDisableState::kSuppress},

        // If any key is released once in primed state, go to suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_LSHIFT), KeyRelease(ui::VKEY_0)},
         ShiftDisableState::kSuppress},

        // If mouse is pressed in primed state, go to suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), MousePress()},
         ShiftDisableState::kSuppress},

        // If mouse is pressed and released, ensure we end up back in starting
        // state.
        {{MousePress(), MouseRelease()}, ShiftDisableState::kStart},
    }));

TEST_P(AcceleratorShiftDisableCapslockStateMachineTest, StateTest) {
  for (auto& event : events_) {
    if (std::holds_alternative<bool>(event)) {
      ON_CALL(*input_controller_, AreAnyKeysPressed())
          .WillByDefault(testing::Return(std::get<bool>(event)));
      continue;
    }

    shift_disable_state_machine_->OnEvent(&GetEventFromVariant(event));
  }

  EXPECT_EQ(expected_state_, shift_disable_state_machine_->current_state());
}

}  // namespace ash::accelerators
