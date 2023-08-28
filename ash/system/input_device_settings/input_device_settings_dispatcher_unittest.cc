// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_dispatcher.h"

#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/ozone/public/input_controller.h"

namespace ash {
namespace {
constexpr int kMouseId = 1;
constexpr int kPointingStickId = 2;
constexpr int kTouchpadId = 3;

class MockInputController : public ui::InputController {
 public:
  MOCK_METHOD(void,
              SetTouchpadSensitivity,
              (absl::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetTouchpadScrollSensitivity,
              (absl::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetTouchpadHapticFeedback,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTouchpadHapticClickSensitivity,
              (absl::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetTapToClick,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTapDragging,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetNaturalScroll,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetMouseSensitivity,
              (absl::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetMouseScrollSensitivity,
              (absl::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetMouseReverseScroll,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetMouseAcceleration,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetMouseScrollAcceleration,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetPointingStickSensitivity,
              (absl::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetPointingStickAcceleration,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTouchpadAcceleration,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTouchpadScrollAcceleration,
              (absl::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetPrimaryButtonRight,
              (absl::optional<int> device_id, bool right),
              (override));
  MOCK_METHOD(void,
              SetPointingStickPrimaryButtonRight,
              (absl::optional<int> device_id, bool right),
              (override));

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
  void SetCurrentLayoutByName(const std::string& layout_name) override {}
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
  bool AreAnyKeysPressed() override { return false; }
};
}  // namespace

class InputDeviceSettingsDispatcherTest : public AshTestBase {
 public:
  InputDeviceSettingsDispatcherTest() = default;
  InputDeviceSettingsDispatcherTest(const InputDeviceSettingsDispatcherTest&) =
      delete;
  InputDeviceSettingsDispatcherTest& operator=(
      const InputDeviceSettingsDispatcherTest&) = delete;
  ~InputDeviceSettingsDispatcherTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<MockInputController>();
    dispatcher_ =
        std::make_unique<InputDeviceSettingsDispatcher>(controller_.get());
  }

  void TearDown() override {
    // Dispatcher must be destroyed before the mock controller as the dispatcher
    // depends on the controller.
    dispatcher_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

 protected:
  std::unique_ptr<InputDeviceSettingsDispatcher> dispatcher_;
  std::unique_ptr<MockInputController> controller_;
};

TEST_F(InputDeviceSettingsDispatcherTest, MouseTest) {
  mojom::Mouse mouse;
  mouse.id = kMouseId;
  mouse.settings = mojom::MouseSettings::New();

  auto& settings = *mouse.settings;
  settings.acceleration_enabled = false;
  settings.reverse_scrolling = false;
  settings.swap_right = false;
  settings.scroll_sensitivity = 3;
  settings.sensitivity = 3;

  constexpr absl::optional<int> mouse_id = kMouseId;
  EXPECT_CALL(*controller_, SetMouseSensitivity(mouse_id, settings.sensitivity))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetMouseReverseScroll(mouse_id, settings.reverse_scrolling))
      .Times(2);
  EXPECT_CALL(*controller_, SetMouseScrollAcceleration(
                                mouse_id, settings.scroll_acceleration))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetMouseScrollSensitivity(mouse_id, settings.scroll_sensitivity))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetMouseAcceleration(mouse_id, settings.acceleration_enabled))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetPrimaryButtonRight(mouse_id, settings.swap_right))
      .Times(2);

  dispatcher_->OnMouseConnected(mouse);
  dispatcher_->OnMouseSettingsUpdated(mouse);
}

TEST_F(InputDeviceSettingsDispatcherTest, PointingStickTest) {
  mojom::PointingStick pointing_stick;
  pointing_stick.id = kPointingStickId;
  pointing_stick.settings = mojom::PointingStickSettings::New();

  auto& settings = *pointing_stick.settings;
  settings.acceleration_enabled = false;
  settings.swap_right = false;
  settings.sensitivity = 3;

  constexpr absl::optional<int> pointing_stick_id = kPointingStickId;
  EXPECT_CALL(*controller_, SetPointingStickSensitivity(pointing_stick_id,
                                                        settings.sensitivity))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetPointingStickAcceleration(pointing_stick_id,
                                           settings.acceleration_enabled))
      .Times(2);
  EXPECT_CALL(*controller_, SetPointingStickPrimaryButtonRight(
                                pointing_stick_id, settings.swap_right))
      .Times(2);

  dispatcher_->OnPointingStickConnected(pointing_stick);
  dispatcher_->OnPointingStickSettingsUpdated(pointing_stick);
}

TEST_F(InputDeviceSettingsDispatcherTest, TouchpadTest) {
  mojom::Touchpad touchpad;
  touchpad.id = kTouchpadId;
  touchpad.settings = mojom::TouchpadSettings::New();

  auto& settings = *touchpad.settings;
  settings.acceleration_enabled = false;
  settings.haptic_enabled = false;
  settings.haptic_sensitivity = 1;
  settings.scroll_sensitivity = 3;
  settings.reverse_scrolling = false;
  settings.tap_dragging_enabled = false;
  settings.tap_to_click_enabled = false;
  settings.sensitivity = 5;

  constexpr absl::optional<int> touchpad_id = kTouchpadId;
  EXPECT_CALL(*controller_,
              SetTouchpadSensitivity(touchpad_id, settings.sensitivity))
      .Times(2);
  EXPECT_CALL(*controller_, SetTouchpadScrollSensitivity(
                                touchpad_id, settings.scroll_sensitivity))
      .Times(2);
  EXPECT_CALL(*controller_, SetTouchpadAcceleration(
                                touchpad_id, settings.acceleration_enabled))
      .Times(2);
  EXPECT_CALL(*controller_, SetTouchpadScrollAcceleration(
                                touchpad_id, settings.scroll_acceleration))
      .Times(2);
  EXPECT_CALL(*controller_, SetTouchpadHapticClickSensitivity(
                                touchpad_id, settings.haptic_sensitivity))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetTouchpadHapticFeedback(touchpad_id, settings.haptic_enabled))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetTapToClick(touchpad_id, settings.tap_to_click_enabled))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetNaturalScroll(touchpad_id, settings.reverse_scrolling))
      .Times(2);
  EXPECT_CALL(*controller_,
              SetTapDragging(touchpad_id, settings.tap_dragging_enabled))
      .Times(2);

  dispatcher_->OnTouchpadConnected(touchpad);
  dispatcher_->OnTouchpadSettingsUpdated(touchpad);
}

}  // namespace ash
