// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_launcher_state_machine.h"

#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/input_controller.h"

namespace ash::accelerators {

namespace {

using EventTypeVariant = absl::variant<ui::MouseEvent, ui::KeyEvent, bool>;
using LauncherState = AcceleratorLauncherStateMachine::LauncherState;

const bool kKeysPressed = true;
const bool kNoKeysPressed = false;

class MockInputController : public ui::InputController {
 public:
  MOCK_METHOD(bool, AreAnyKeysPressed, (), (override));

 private:
  bool HasMouse() override { return false; }
  bool HasPointingStick() override { return false; }
  bool HasTouchpad() override { return false; }
  bool HasHapticTouchpad() override { return false; }
  bool IsCapsLockEnabled() override { return false; }
  void SetCapsLockEnabled(bool enabled) override {}
  void SetNumLockEnabled(bool enabled) override {}
  bool IsAutoRepeatEnabled() override { return true; }
  void SetAutoRepeatEnabled(bool enabled) override {}
  void SetAutoRepeatRate(const base::TimeDelta& delay,
                         const base::TimeDelta& interval) override {}
  void GetAutoRepeatRate(base::TimeDelta* delay,
                         base::TimeDelta* interval) override {}
  void SetCurrentLayoutByName(
      const std::string& layout_name,
      base::OnceCallback<void(bool)> callback) override {}
  void SetKeyboardKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetKeyboardKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTouchEventLoggingEnabled(bool enabled) override {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  void SuspendMouseAcceleration() override {}
  void EndMouseAccelerationSuspension() override {}
  void SetThreeFingerClick(bool enabled) override {}
  void SetGamepadKeyBitsMapping(
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}
  std::vector<uint64_t> GetGamepadKeyBits(int id) override {
    return std::vector<uint64_t>();
  }
  void SetTapToClickPaused(bool state) override {}
  void GetTouchDeviceStatus(GetTouchDeviceStatusReply reply) override {
    std::move(reply).Run(std::string());
  }
  void GetTouchEventLog(const base::FilePath& out_dir,
                        GetTouchEventLogReply reply) override {
    std::move(reply).Run(std::vector<base::FilePath>());
  }
  void DescribeForLog(DescribeForLogReply reply) const override {
    std::move(reply).Run(std::string());
  }
  void SetInternalTouchpadEnabled(bool enabled) override {}
  bool IsInternalTouchpadEnabled() const override { return false; }
  void SetTouchscreensEnabled(bool enabled) override {}
  void GetStylusSwitchState(GetStylusSwitchStateReply reply) override {
    std::move(reply).Run(ui::StylusState::REMOVED);
  }
  void SetInternalKeyboardFilter(
      bool enable_filter,
      std::vector<ui::DomCode> allowed_keys) override {}
  void GetGesturePropertiesService(
      mojo::PendingReceiver<ui::ozone::mojom::GesturePropertiesService>
          receiver) override {}
  void PlayVibrationEffect(int id,
                           uint8_t amplitude,
                           uint16_t duration_millis) override {}
  void StopVibration(int id) override {}
  void PlayHapticTouchpadEffect(
      ui::HapticTouchpadEffect effect_type,
      ui::HapticTouchpadEffectStrength strength) override {}
  void SetHapticTouchpadEffectForNextButtonRelease(
      ui::HapticTouchpadEffect effect_type,
      ui::HapticTouchpadEffectStrength strength) override {}
  void SetTouchpadSensitivity(std::optional<int> device_id,
                              int value) override {}
  void SetTouchpadScrollSensitivity(std::optional<int> device_id,
                                    int value) override {}
  void SetTouchpadHapticFeedback(std::optional<int> device_id,
                                 bool enabled) override {}
  void SetTouchpadHapticClickSensitivity(std::optional<int> device_id,
                                         int value) override {}
  void SetTapToClick(std::optional<int> device_id, bool enabled) override {}
  void SetTapDragging(std::optional<int> device_id, bool enabled) override {}
  void SetNaturalScroll(std::optional<int> device_id, bool enabled) override {}
  void SetMouseSensitivity(std::optional<int> device_id, int value) override {}
  void SetMouseScrollSensitivity(std::optional<int> device_id,
                                 int value) override {}
  void SetMouseReverseScroll(std::optional<int> device_id,
                             bool enabled) override {}
  void SetMouseAcceleration(std::optional<int> device_id,
                            bool enabled) override {}
  void SetMouseScrollAcceleration(std::optional<int> device_id,
                                  bool enabled) override {}
  void SetPointingStickSensitivity(std::optional<int> device_id,
                                   int value) override {}
  void SetPointingStickAcceleration(std::optional<int> device_id,
                                    bool enabled) override {}
  void SetTouchpadAcceleration(std::optional<int> device_id,
                               bool enabled) override {}
  void SetTouchpadScrollAcceleration(std::optional<int> device_id,
                                     bool enabled) override {}
  void SetPrimaryButtonRight(std::optional<int> device_id,
                             bool right) override {}
  void SetPointingStickPrimaryButtonRight(std::optional<int> device_id,
                                          bool right) override {}
  void BlockModifiersOnDevices(std::vector<int> device_ids) override {}
  bool AreInputDevicesEnabled() const override { return true; }
  std::unique_ptr<ui::ScopedDisableInputDevices> DisableInputDevices()
      override {
    return nullptr;
  }
  void DisableKeyboardImposterCheck() override {}
};

ui::KeyEvent KeyPress(ui::KeyboardCode key_code) {
  return ui::KeyEvent(ui::EventType::kKeyPressed, key_code, ui::EF_NONE);
}

ui::KeyEvent KeyRelease(ui::KeyboardCode key_code) {
  return ui::KeyEvent(ui::EventType::kKeyReleased, key_code, ui::EF_NONE);
}

ui::Event& GetEventFromVariant(EventTypeVariant& event) {
  if (absl::holds_alternative<ui::MouseEvent>(event)) {
    return absl::get<ui::MouseEvent>(event);
  } else {
    return absl::get<ui::KeyEvent>(event);
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
    if (absl::holds_alternative<bool>(event)) {
      ON_CALL(*input_controller_, AreAnyKeysPressed())
          .WillByDefault(testing::Return(absl::get<bool>(event)));
      continue;
    }

    launcher_state_machine_->OnEvent(&GetEventFromVariant(event));
  }

  EXPECT_EQ(expected_state_, launcher_state_machine_->current_state());
}

}  // namespace ash::accelerators
