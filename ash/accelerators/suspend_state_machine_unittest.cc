// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/suspend_state_machine.h"

#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/types/event_type.h"
#include "ui/ozone/public/input_controller.h"

namespace ash::accelerators {

namespace {

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

// Key representation in test cases.
struct TestKeyEvent {
  ui::EventType type;
  ui::DomCode code;
  ui::DomKey key;
  ui::KeyboardCode keycode;
  ui::EventFlags flags = ui::EF_NONE;

  std::string ToString() const;
};

// Factory template of TestKeyEvents just to reduce a lot of code/data
// duplication.
template <ui::DomCode code,
          ui::DomKey::Base key,
          ui::KeyboardCode keycode,
          ui::EventFlags modifier_flag = ui::EF_NONE,
          ui::DomKey::Base shifted_key = key>
struct TestKey {
  // Returns press key event.
  static constexpr TestKeyEvent Pressed(ui::EventFlags flags = ui::EF_NONE) {
    return {ui::EventType::kKeyPressed, code,
            (flags & ui::EF_SHIFT_DOWN) ? shifted_key : key, keycode,
            flags | modifier_flag};
  }

  // Returns release key event.
  static constexpr TestKeyEvent Released(ui::EventFlags flags = ui::EF_NONE) {
    // Note: modifier flag should not be present on release events.
    return {ui::EventType::kKeyReleased, code,
            (flags & ui::EF_SHIFT_DOWN) ? shifted_key : key, keycode, flags};
  }
};

// Short cut of TestKey construction for Character keys.
template <ui::DomCode code,
          char key,
          ui::KeyboardCode keycode,
          char shifted_key = key>
using TestCharKey = TestKey<code,
                            ui::DomKey::FromCharacter(key),
                            keycode,
                            ui::EF_NONE,
                            ui::DomKey::FromCharacter(shifted_key)>;

// Modifier keys.
using KeyLShift = TestKey<ui::DomCode::SHIFT_LEFT,
                          ui::DomKey::SHIFT,
                          ui::VKEY_SHIFT,
                          ui::EF_SHIFT_DOWN>;
using KeyRShift = TestKey<ui::DomCode::SHIFT_RIGHT,
                          ui::DomKey::SHIFT,
                          ui::VKEY_SHIFT,
                          ui::EF_SHIFT_DOWN>;
using KeyLMeta = TestKey<ui::DomCode::META_LEFT,
                         ui::DomKey::META,
                         ui::VKEY_LWIN,
                         ui::EF_COMMAND_DOWN>;
using KeyRMeta = TestKey<ui::DomCode::META_RIGHT,
                         ui::DomKey::META,
                         ui::VKEY_RWIN,
                         ui::EF_COMMAND_DOWN>;
using KeyLControl = TestKey<ui::DomCode::CONTROL_LEFT,
                            ui::DomKey::CONTROL,
                            ui::VKEY_CONTROL,
                            ui::EF_CONTROL_DOWN>;
using KeyRControl = TestKey<ui::DomCode::CONTROL_RIGHT,
                            ui::DomKey::CONTROL,
                            ui::VKEY_CONTROL,
                            ui::EF_CONTROL_DOWN>;
using KeyLAlt = TestKey<ui::DomCode::ALT_LEFT,
                        ui::DomKey::ALT,
                        ui::VKEY_MENU,
                        ui::EF_ALT_DOWN>;
using KeyRAlt = TestKey<ui::DomCode::ALT_RIGHT,
                        ui::DomKey::ALT,
                        ui::VKEY_MENU,
                        ui::EF_ALT_DOWN>;

// Character keys. Shift chars are based on US layout.
using KeyA = TestCharKey<ui::DomCode::US_A, 'a', ui::VKEY_A, 'A'>;
using KeyB = TestCharKey<ui::DomCode::US_B, 'b', ui::VKEY_B, 'B'>;
using KeyC = TestCharKey<ui::DomCode::US_C, 'c', ui::VKEY_C, 'C'>;

const bool kKeysPressed = true;
const bool kNoKeysPressed = false;

using EventTypeVariant = std::variant<TestKeyEvent, bool>;
using SuspendStateMachineEvent = SuspendStateMachine::SuspendStateMachineEvent;

}  // namespace

class SuspendStateMachineTest
    : public AshTestBase,
      public testing::WithParamInterface<
          std::tuple<ui::Accelerator, std::vector<EventTypeVariant>>> {
 public:
  void SetUp() override {
    AshTestBase::SetUp();

    std::tie(trigger_accelerator_, events_) = GetParam();
    input_controller_ = std::make_unique<MockInputController>();
    suspend_state_machine_ =
        std::make_unique<SuspendStateMachine>(input_controller_.get());
  }

  void TearDown() override {
    suspend_state_machine_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<SuspendStateMachine> suspend_state_machine_;
  std::unique_ptr<MockInputController> input_controller_;
  std::vector<EventTypeVariant> events_;
  ui::Accelerator trigger_accelerator_;
};

class SuccessfulSuspendStateMachineTest : public SuspendStateMachineTest {};

INSTANTIATE_TEST_SUITE_P(
    All,
    SuccessfulSuspendStateMachineTest,
    testing::ValuesIn(
        std::vector<std::tuple<ui::Accelerator, std::vector<EventTypeVariant>>>{
            // Standard activations of the suspend.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyLMeta::Released(), kNoKeysPressed,
              KeyA::Released()}},
            {{ui::VKEY_B, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyLMeta::Released(), kNoKeysPressed,
              KeyB::Released()}},
            {{ui::VKEY_C, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
             {kKeysPressed, KeyLControl::Released(), KeyLAlt::Released(),
              kNoKeysPressed, KeyC::Released()}},

            // Reversed ordering of releases, checks that ordering does not
            // matter.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyA::Released(), kNoKeysPressed,
              KeyLMeta::Released()}},
            {{ui::VKEY_B, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyB::Released(), kNoKeysPressed,
              KeyLMeta::Released()}},
            {{ui::VKEY_C, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN},
             {kKeysPressed, KeyC::Released(), KeyLAlt::Released(),
              kNoKeysPressed, KeyLControl::Released()}},

            // Left vs right modifiers does not matter.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyRMeta::Released(), KeyLMeta::Released(),
              kNoKeysPressed, KeyA::Released()}},
            {{ui::VKEY_A, ui::EF_ALT_DOWN},
             {kKeysPressed, KeyRAlt::Released(), KeyLAlt::Released(),
              kNoKeysPressed, KeyA::Released()}},
            {{ui::VKEY_A, ui::EF_CONTROL_DOWN},
             {kKeysPressed, KeyLControl::Released(), KeyRControl::Released(),
              kNoKeysPressed, KeyA::Released()}},
            {{ui::VKEY_A, ui::EF_SHIFT_DOWN},
             {kKeysPressed, KeyLShift::Released(), KeyRShift::Released(),
              kNoKeysPressed, KeyA::Released()}},

            // All modifiers in one accelerator.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN |
                              ui::EF_SHIFT_DOWN | ui::EF_CONTROL_DOWN},
             {kKeysPressed, KeyLControl::Released(), KeyLShift::Released(),
              KeyLMeta::Released(), KeyRAlt::Released(), kNoKeysPressed,
              KeyA::Released()}},

            // No modifiers in accelerator.
            {{ui::VKEY_A, ui::EF_NONE}, {kNoKeysPressed, KeyA::Released()}},

            // Other keys currently held down, not triggered until all keys
            // released.
            {{ui::VKEY_A, ui::EF_ALT_DOWN},
             {kKeysPressed, KeyLAlt::Released(), KeyA::Released(),
              kNoKeysPressed, KeyRAlt::Released()}},

        }));

TEST_P(SuccessfulSuspendStateMachineTest, SuspendTriggered) {
  base::HistogramTester histogram_tester;

  suspend_state_machine_->StartObservingToTriggerSuspend(trigger_accelerator_);
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.SuspendStateMachine",
                                     SuspendStateMachineEvent::kTriggered, 1);
  for (const auto& event : events_) {
    ASSERT_EQ(0, power_manager_client()->num_request_suspend_calls());
    if (std::holds_alternative<bool>(event)) {
      ON_CALL(*input_controller_, AreAnyKeysPressed())
          .WillByDefault(testing::Return(std::get<bool>(event)));
      continue;
    } else {
      const TestKeyEvent& test_event = std::get<TestKeyEvent>(event);
      ui::KeyEvent key_event(test_event.type, test_event.keycode,
                             test_event.code, test_event.flags, test_event.key,
                             ui::EventTimeForNow());
      suspend_state_machine_->OnEvent(&key_event);
    }
  }

  EXPECT_EQ(1, power_manager_client()->num_request_suspend_calls());
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.SuspendStateMachine",
                                     SuspendStateMachineEvent::kSuspended, 1);
}

class CancelledSuspendStateMachineTest : public SuspendStateMachineTest {};

INSTANTIATE_TEST_SUITE_P(
    All,
    CancelledSuspendStateMachineTest,
    testing::ValuesIn(
        std::vector<std::tuple<ui::Accelerator, std::vector<EventTypeVariant>>>{
            // Release a key not in the original accelerator.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyLMeta::Released(), KeyB::Released(),
              kNoKeysPressed, KeyA::Released()}},

            // Release a modifier not in the original accelerator.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyLControl::Released(), kNoKeysPressed,
              KeyA::Released()}},

            // Press a modifier again in the original accelerator.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyLMeta::Released(), KeyLMeta::Pressed(),
              KeyLMeta::Released(), kNoKeysPressed, KeyA::Released()}},

            // Press a key not in the original accelerator.
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyLMeta::Released(), KeyB::Pressed(),
              KeyB::Released(), kNoKeysPressed, KeyA::Released()}},
            {{ui::VKEY_A, ui::EF_COMMAND_DOWN},
             {kKeysPressed, KeyLMeta::Released(), KeyB::Pressed(),
              kNoKeysPressed, KeyA::Released()}},
        }));

TEST_P(CancelledSuspendStateMachineTest, SuspendNotTriggered) {
  base::HistogramTester histogram_tester;

  suspend_state_machine_->StartObservingToTriggerSuspend(trigger_accelerator_);
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.SuspendStateMachine",
                                     SuspendStateMachineEvent::kTriggered, 1);
  for (const auto& event : events_) {
    ASSERT_EQ(0, power_manager_client()->num_request_suspend_calls());
    if (std::holds_alternative<bool>(event)) {
      ON_CALL(*input_controller_, AreAnyKeysPressed())
          .WillByDefault(testing::Return(std::get<bool>(event)));
      continue;
    } else {
      const TestKeyEvent& test_event = std::get<TestKeyEvent>(event);
      ui::KeyEvent key_event(test_event.type, test_event.keycode,
                             test_event.code, test_event.flags, test_event.key,
                             ui::EventTimeForNow());
      suspend_state_machine_->OnEvent(&key_event);
    }
  }

  EXPECT_EQ(0, power_manager_client()->num_request_suspend_calls());
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.SuspendStateMachine",
                                     SuspendStateMachineEvent::kCancelled, 1);
}

}  // namespace ash::accelerators
