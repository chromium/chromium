// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_capslock_state_machine.h"

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
using CapslockState = AcceleratorCapslockStateMachine::CapslockState;

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

class AcceleratorCapslockStateMachineTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<std::vector<EventTypeVariant>, CapslockState>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    std::tie(events_, expected_state_) = GetParam();

    input_controller_ = std::make_unique<MockInputController>();
    caps_lock_state_machine_ =
        std::make_unique<AcceleratorCapslockStateMachine>(
            input_controller_.get());
  }

  void TearDown() override {
    caps_lock_state_machine_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<AcceleratorCapslockStateMachine> caps_lock_state_machine_;
  std::unique_ptr<MockInputController> input_controller_;
  std::vector<EventTypeVariant> events_;
  CapslockState expected_state_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    AcceleratorCapslockStateMachineTest,
    testing::ValuesIn(std::vector<
                      std::tuple<std::vector<EventTypeVariant>, CapslockState>>{
        // Alt -> Search primes capslock
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyPress(ui::VKEY_LWIN)},
         CapslockState::kPrimed},

        // Alt -> Search -> Alt release allows capslock to be triggered and
        // will prime again when alt is pressed.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyPress(ui::VKEY_LWIN),
          KeyRelease(ui::VKEY_MENU)},
         CapslockState::kTriggerAlt},

        // Alt -> Search -> Search release allows capslock to be triggered and
        // will prime again when Search is pressed.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyPress(ui::VKEY_LWIN),
          KeyRelease(ui::VKEY_LWIN)},
         CapslockState::kTriggerSearch},

        // Search -> Alt -> Search release allows capslock to be triggered and
        // will prime again when Search is pressed.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyPress(ui::VKEY_MENU),
          KeyRelease(ui::VKEY_LWIN)},
         CapslockState::kTriggerSearch},

        // Search -> Alt -> A lands in suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyPress(ui::VKEY_MENU),
          KeyPress(ui::VKEY_A)},
         CapslockState::kSuppress},

        // Search -> A lands in suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyPress(ui::VKEY_A)},
         CapslockState::kSuppress},

        // Search -> Release A lands in suppressed state.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyRelease(ui::VKEY_A)},
         CapslockState::kSuppress},

        // Search -> Search release puts back in starting state.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyRelease(ui::VKEY_LWIN)},
         CapslockState::kStart},

        // Search -> A -> A Release -> Search release puts back in starting
        // state.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyPress(ui::VKEY_A),
          KeyRelease(ui::VKEY_A), kNoKeysPressed, KeyRelease(ui::VKEY_LWIN)},
         CapslockState::kStart},

        // Alt -> Search -> Release Alt -> Press Search ends up still waiting
        // for Alt to be pressed.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyPress(ui::VKEY_LWIN),
          KeyRelease(ui::VKEY_MENU), KeyPress(ui::VKEY_LWIN)},
         CapslockState::kWaitingAlt},

        // Search -> Alt -> Release Search -> Alt results in us still waiting
        // for Search to be pressed.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyPress(ui::VKEY_MENU),
          KeyRelease(ui::VKEY_LWIN), KeyPress(ui::VKEY_MENU)},
         CapslockState::kWaitingSearch},

        // Alt -> Release Alt ends up back at the start.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyRelease(ui::VKEY_MENU)},
         CapslockState::kStart},

        // Alt -> Release another key ends up with us at kSuppress.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyRelease(ui::VKEY_A)},
         CapslockState::kSuppress},

        // Alt -> A ends up with us at kSuppress.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyPress(ui::VKEY_A)},
         CapslockState::kSuppress},

        // Alt -> Search -> Alt keeps us still primed.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), KeyPress(ui::VKEY_LWIN),
          KeyPress(ui::VKEY_MENU)},
         CapslockState::kPrimed},

        // Pressing any key other than alt or search results in suppress.
        {{kKeysPressed, KeyPress(ui::VKEY_A)}, CapslockState::kSuppress},

        // Alt -> Mouse button results in us being suppressed.
        {{kKeysPressed, KeyPress(ui::VKEY_MENU), MousePress()},
         CapslockState::kSuppress},

        // Mouse press -> Mouse release ends up back at kStart.
        {{MousePress(), MouseRelease()}, CapslockState::kStart},
    }));

TEST_P(AcceleratorCapslockStateMachineTest, StateTest) {
  for (auto& event : events_) {
    if (std::holds_alternative<bool>(event)) {
      ON_CALL(*input_controller_, AreAnyKeysPressed())
          .WillByDefault(testing::Return(std::get<bool>(event)));
      continue;
    }

    caps_lock_state_machine_->OnEvent(&GetEventFromVariant(event));
  }

  EXPECT_EQ(expected_state_, caps_lock_state_machine_->current_state());
}

}  // namespace ash::accelerators
