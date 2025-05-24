// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_launcher_state_machine.h"

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

using EventTypeVariant = std::variant<ui::MouseEvent, ui::KeyEvent, bool>;
using LauncherState = AcceleratorLauncherStateMachine::LauncherState;

const bool kKeysPressed = true;
const bool kNoKeysPressed = false;

class MockInputController : public ui::StubInputController {
 public:
  MOCK_METHOD(bool, AreAnyKeysPressed, (), (override));
};

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

class AcceleratorLauncherStateMachineTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<std::vector<EventTypeVariant>, LauncherState>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    std::tie(events_, expected_state_) = GetParam();

    input_controller_ = std::make_unique<MockInputController>();
    launcher_state_machine_ = std::make_unique<AcceleratorLauncherStateMachine>(
        input_controller_.get());
  }

  void TearDown() override {
    launcher_state_machine_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<AcceleratorLauncherStateMachine> launcher_state_machine_;
  std::unique_ptr<MockInputController> input_controller_;
  std::vector<EventTypeVariant> events_;
  LauncherState expected_state_;
};

INSTANTIATE_TEST_SUITE_P(
    Works,
    AcceleratorLauncherStateMachineTest,
    testing::ValuesIn(std::vector<
                      std::tuple<std::vector<EventTypeVariant>, LauncherState>>{
        // Pressing and releasing Meta key allows launcher to trigger.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), kNoKeysPressed,
          KeyRelease(ui::VKEY_LWIN)},
         LauncherState::kTrigger},

        // Pressing Shift -> Meta allows launcher to open.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), KeyPress(ui::VKEY_LWIN),
          KeyRelease(ui::VKEY_LWIN)},
         LauncherState::kTrigger},

        // Getting into a suppressed state and then releasing all keys resets
        // and allows you to trigger the launcher via Meta press and release.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyPress(ui::VKEY_A),
          KeyRelease(ui::VKEY_A), kNoKeysPressed, KeyRelease(ui::VKEY_LWIN),
          kKeysPressed, KeyPress(ui::VKEY_LWIN), kNoKeysPressed,
          KeyRelease(ui::VKEY_LWIN)},
         LauncherState::kTrigger},

        // Meta -> Shift -> Shift release -> Meta release does not trigger and
        // instead puts us back at the start.
        {{kKeysPressed, KeyPress(ui::VKEY_LWIN), KeyPress(ui::VKEY_SHIFT),
          KeyRelease(ui::VKEY_SHIFT), kNoKeysPressed,
          KeyRelease(ui::VKEY_LWIN)},
         LauncherState::kStart},

        // Shift -> Meta leaves us in kPrimed.
        {{kKeysPressed, KeyPress(ui::VKEY_SHIFT), KeyPress(ui::VKEY_LWIN)},
         LauncherState::kPrimed},

        // Pressing any key besides shift or meta leaves us in kSuppress.
        {{kKeysPressed, KeyPress(ui::VKEY_A)}, LauncherState::kSuppress},

        // Pressing any key besides shift or meta and releasing resets us back
        // to kStart.
        {{kKeysPressed, KeyPress(ui::VKEY_A), kNoKeysPressed,
          KeyRelease(ui::VKEY_A)},
         LauncherState::kStart},

        // Pressing A -> Press and release Meta leaves us still in kSuppress and
        // does not allow the launcher to open.
        {{kKeysPressed, KeyPress(ui::VKEY_A), KeyPress(ui::VKEY_LWIN),
          KeyRelease(ui::VKEY_LWIN)},
         LauncherState::kSuppress},

        // Press A -> Press B -> Release A leaves us still in kSuppress as B is
        // still being held down.
        {{kKeysPressed, KeyPress(ui::VKEY_A), KeyPress(ui::VKEY_B),
          KeyRelease(ui::VKEY_A)},
         LauncherState::kSuppress},
    }));

TEST_P(AcceleratorLauncherStateMachineTest, StateTest) {
  for (auto& event : events_) {
    if (std::holds_alternative<bool>(event)) {
      ON_CALL(*input_controller_, AreAnyKeysPressed())
          .WillByDefault(testing::Return(std::get<bool>(event)));
      continue;
    }

    launcher_state_machine_->OnEvent(&GetEventFromVariant(event));
  }

  EXPECT_EQ(expected_state_, launcher_state_machine_->current_state());
}

}  // namespace ash::accelerators
