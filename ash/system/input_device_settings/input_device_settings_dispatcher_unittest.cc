// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_dispatcher.h"

#include "ash/constants/ash_features.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/haptic_touchpad_effects.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/stylus_state.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
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
              (std::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetTouchpadScrollSensitivity,
              (std::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetTouchpadHapticFeedback,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTouchpadHapticClickSensitivity,
              (std::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetTapToClick,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTapDragging,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetNaturalScroll,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetMouseSensitivity,
              (std::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetMouseScrollSensitivity,
              (std::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetMouseReverseScroll,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetMouseAcceleration,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetMouseScrollAcceleration,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetPointingStickSensitivity,
              (std::optional<int> device_id, int value),
              (override));
  MOCK_METHOD(void,
              SetPointingStickAcceleration,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTouchpadAcceleration,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetTouchpadScrollAcceleration,
              (std::optional<int> device_id, bool enabled),
              (override));
  MOCK_METHOD(void,
              SetPrimaryButtonRight,
              (std::optional<int> device_id, bool right),
              (override));
  MOCK_METHOD(void,
              SetPointingStickPrimaryButtonRight,
              (std::optional<int> device_id, bool right),
              (override));
  MOCK_METHOD(void,
              BlockModifiersOnDevices,
              (std::vector<int> device_ids),
              (override));

  MOCK_METHOD(std::unique_ptr<ui::ScopedDisableInputDevices>,
              DisableInputDevices,
              (),
              (override));
  MOCK_METHOD(bool, AreInputDevicesEnabled, (), (const override));

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
  bool AreAnyKeysPressed() override { return false; }
  void DisableKeyboardImposterCheck() override {}
};

const ui::InputDevice CreateInputDevice(int id,
                                        uint16_t vendor,
                                        uint16_t product) {
  return ui::InputDevice(id, ui::INPUT_DEVICE_USB, "kDeviceName", "",
                         base::FilePath(), vendor, product, 0);
}

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
    scoped_feature_list_.InitWithFeatures({features::kInputDeviceSettingsSplit,
                                           features::kPeripheralCustomization},
                                          {});

    AshTestBase::SetUp();
    Shell::Get()->event_rewriter_controller()->Initialize(nullptr, nullptr);
    controller_ = std::make_unique<MockInputController>();
    dispatcher_ =
        std::make_unique<InputDeviceSettingsDispatcher>(controller_.get());

    ON_CALL(*controller_, BlockModifiersOnDevices)
        .WillByDefault(testing::Invoke([&](std::vector<int> device_ids) {
          device_ids_to_block_modifiers_ = std::move(device_ids);
        }));
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
  base::test::ScopedFeatureList scoped_feature_list_;

  std::vector<int> device_ids_to_block_modifiers_;
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

  constexpr std::optional<int> mouse_id = kMouseId;
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

  constexpr std::optional<int> pointing_stick_id = kPointingStickId;
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

  constexpr std::optional<int> touchpad_id = kTouchpadId;
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

TEST_F(InputDeviceSettingsDispatcherTest, DuplicateIdsBlockModifiers) {
  // Use VID/PID for mouse with modifier blocking issue.
  auto duplicate_1_1 = CreateInputDevice(0, 0x046d, 0xb019);
  auto duplicate_1_2 = CreateInputDevice(1, 0x046d, 0xb019);
  auto duplicate_1_3 = CreateInputDevice(2, 0x046d, 0xb019);

  // Use VID/PID without modifier blocking issue.
  auto duplicate_2_1 = CreateInputDevice(3, 0x046d, 0xb034);
  auto duplicate_2_2 = CreateInputDevice(4, 0x046d, 0xb034);
  auto duplicate_2_3 = CreateInputDevice(5, 0x046d, 0xb034);

  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {duplicate_1_1, duplicate_2_1});
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {duplicate_1_2, duplicate_2_2});
  ui::DeviceDataManagerTestApi().SetUncategorizedDevices({duplicate_1_3});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {ui::KeyboardDevice(duplicate_2_3)});

  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());

  auto* input_device_settings_controller =
      Shell::Get()->input_device_settings_controller();

  // After starting to observe, all device ids with same vid/pid as
  // `duplicate_1_1` should be blocked.
  input_device_settings_controller->StartObservingButtons(duplicate_1_1.id);
  EXPECT_EQ(
      (std::vector<int>{duplicate_1_1.id, duplicate_1_2.id, duplicate_1_3.id}),
      device_ids_to_block_modifiers_);

  // Add mouse button to configure.
  input_device_settings_controller->OnMouseButtonPressed(
      duplicate_1_1.id, *mojom::Button::NewVkey(ui::VKEY_TAB));

  input_device_settings_controller->StopObservingButtons();
  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());

  auto mouse_settings =
      input_device_settings_controller->GetMouseSettings(duplicate_1_1.id)
          ->Clone();
  mouse_settings->button_remappings.back()->remapping_action =
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kDisable);
  input_device_settings_controller->SetMouseSettings(duplicate_1_1.id,
                                                     std::move(mouse_settings));

  // After configuring the button to an action, block modifiers from the device.
  EXPECT_EQ(
      (std::vector<int>{duplicate_1_1.id, duplicate_1_2.id, duplicate_1_3.id}),
      device_ids_to_block_modifiers_);

  mouse_settings =
      input_device_settings_controller->GetMouseSettings(duplicate_1_1.id)
          ->Clone();
  mouse_settings->button_remappings.back()->remapping_action = nullptr;
  input_device_settings_controller->SetMouseSettings(duplicate_1_1.id,
                                                     std::move(mouse_settings));

  // After settings back to no remapping action, modifiers should not be
  // blocked.
  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());
}

TEST_F(InputDeviceSettingsDispatcherTest, DuplicateIdsDontBlockModifiers) {
  // Use VID/PID for mouse with modifier blocking issue.
  auto duplicate_1_1 = CreateInputDevice(0, 0x046d, 0xb019);
  auto duplicate_1_2 = CreateInputDevice(1, 0x046d, 0xb019);
  auto duplicate_1_3 = CreateInputDevice(2, 0x046d, 0xb019);

  // Use VID/PID without modifier blocking issue.
  auto duplicate_2_1 = CreateInputDevice(3, 0x046d, 0xb034);
  auto duplicate_2_2 = CreateInputDevice(4, 0x046d, 0xb034);
  auto duplicate_2_3 = CreateInputDevice(5, 0x046d, 0xb034);

  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {duplicate_1_1, duplicate_2_1});
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {duplicate_1_2, duplicate_2_2});
  ui::DeviceDataManagerTestApi().SetUncategorizedDevices({duplicate_1_3});
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {ui::KeyboardDevice(duplicate_2_3)});

  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());

  auto* input_device_settings_controller =
      Shell::Get()->input_device_settings_controller();

  // After starting to observe, nothing should be blocked as `duplicate_2_1`
  // does not require modifier blocking.
  input_device_settings_controller->StartObservingButtons(duplicate_2_1.id);
  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());

  // Add mouse button to configure.
  input_device_settings_controller->OnMouseButtonPressed(
      duplicate_2_1.id, *mojom::Button::NewVkey(ui::VKEY_TAB));

  input_device_settings_controller->StopObservingButtons();
  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());

  auto mouse_settings =
      input_device_settings_controller->GetMouseSettings(duplicate_2_1.id)
          ->Clone();
  mouse_settings->button_remappings.back()->remapping_action =
      mojom::RemappingAction::NewStaticShortcutAction(
          mojom::StaticShortcutAction::kDisable);
  input_device_settings_controller->SetMouseSettings(duplicate_2_1.id,
                                                     std::move(mouse_settings));

  // After configuring the button to an action, no modifiers should be blocked
  // still.
  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());

  mouse_settings =
      input_device_settings_controller->GetMouseSettings(duplicate_2_1.id)
          ->Clone();
  mouse_settings->button_remappings.back()->remapping_action = nullptr;
  input_device_settings_controller->SetMouseSettings(duplicate_2_1.id,
                                                     std::move(mouse_settings));

  // After settings back to no remapping action, modifiers should not be
  // blocked.
  EXPECT_TRUE(device_ids_to_block_modifiers_.empty());
}

}  // namespace ash
