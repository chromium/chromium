// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {
constexpr char kExternalKeyboardId[] = "test:external";
constexpr char kExternalChromeOSKeyboardId[] = "test:chromeos";
constexpr char kInternalKeyboardDeviceKey[] = "test:internal";
constexpr char kExternalMouseId[] = "test:mouse";
constexpr char kPointingStickId[] = "test:pointingstick";
constexpr char kExternalTouchpadId[] = "test:touchpad-external";
constexpr char kGraphicsTabletId[] = "test:graphics-tablet";
constexpr int kSampleMinSensitivity = 1;
constexpr int kSampleSensitivity = 3;
constexpr int kSampleMaxSensitivity = 5;

constexpr char kUser1[] = "user1@gmail.com";
constexpr char kUser2[] = "user2@gmail.com";

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr int kKeyboardInternalId = 1;

}  // namespace

class InputDeviceSettingsMetricsManagerTest : public AshTestBase {
 public:
  InputDeviceSettingsMetricsManagerTest() = default;
  InputDeviceSettingsMetricsManagerTest(
      const InputDeviceSettingsMetricsManagerTest&) = delete;
  InputDeviceSettingsMetricsManagerTest& operator=(
      const InputDeviceSettingsMetricsManagerTest&) = delete;
  ~InputDeviceSettingsMetricsManagerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    manager_ = std::make_unique<InputDeviceSettingsMetricsManager>();
  }

  void TearDown() override {
    manager_.reset();
    AshTestBase::TearDown();
  }

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeKeyboard(const ui::KeyboardDevice& fake_keyboard) {
    fake_keyboard_devices_.push_back(fake_keyboard);

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = kKbdTopRowLayout1Tag;
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/absl::nullopt,
                             /*devtype=*/absl::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({fake_keyboard});
  }

 protected:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<InputDeviceSettingsMetricsManager> manager_;
};

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordsKeyboardSettings) {
  scoped_feature_list_.InitWithFeatures(
      {
          features::kInputDeviceSettingsSplit,
          features::kAltClickAndSixPackCustomization,
          ::features::kSupportF11AndF12KeyShortcuts,
      },
      /*disabled_features=*/{});
  mojom::Keyboard keyboard_external;
  keyboard_external.device_key = kExternalKeyboardId;
  keyboard_external.is_external = true;
  keyboard_external.meta_key = mojom::MetaKey::kCommand;
  keyboard_external.settings = mojom::KeyboardSettings::New();
  auto& settings_external = *keyboard_external.settings;
  settings_external.top_row_are_fkeys = true;
  settings_external.modifier_remappings = {
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kControl},
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kCapsLock}};
  settings_external.six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  mojom::Keyboard keyboard_external_chromeos;
  keyboard_external_chromeos.device_key = kExternalChromeOSKeyboardId;
  keyboard_external_chromeos.is_external = true;
  keyboard_external_chromeos.meta_key = mojom::MetaKey::kSearch;
  keyboard_external_chromeos.settings = mojom::KeyboardSettings::New();
  auto& settings_external_chromeos = *keyboard_external_chromeos.settings;
  settings_external_chromeos.top_row_are_fkeys = false;
  settings_external_chromeos.six_pack_key_remappings =
      mojom::SixPackKeyInfo::New();

  mojom::Keyboard keyboard_internal;
  const auto kb = ui::KeyboardDevice(
      1, ui::InputDeviceType::INPUT_DEVICE_INTERNAL, "fake_kb");

  keyboard_internal.device_key = kInternalKeyboardDeviceKey;
  keyboard_internal.is_external = false;
  keyboard_internal.id = kKeyboardInternalId;
  AddFakeKeyboard(ui::KeyboardDevice(kKeyboardInternalId,
                                     ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                                     "keyboard_internal"));
  keyboard_internal.settings = mojom::KeyboardSettings::New();
  auto& settings_internal = *keyboard_internal.settings;
  settings_internal.top_row_are_fkeys = true;
  settings_internal.six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  settings_internal.f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  settings_internal.f12 = ui::mojom::ExtendedFkeysModifier::kShift;

  // Initially expect no user preferences recorded.
  base::HistogramTester histogram_tester;
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_external);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/0u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/0u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Keyboard.External.Modifiers."
      "NumberOfRemappedKeysOnStart",
      /*sample=*/3u, /*expected_bucket_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.SixPackKeys.Insert.Initial",
      /*expected_count=*/1u);

  manager_.get()->RecordKeyboardInitialMetrics(keyboard_external_chromeos);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/0u);

  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.F11.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.F12.Initial",
      /*expected_count=*/1u);

  // Call RecordKeyboardChangedMetrics with the same settings, metrics will
  // not be recoreded.
  const mojom::KeyboardSettingsPtr old_settings =
      keyboard_internal.settings.Clone();
  manager_.get()->RecordKeyboardChangedMetrics(keyboard_internal,
                                               *old_settings);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Changed",
      /*expected_count=*/0);

  // Call RecordKeyboardChangedMetrics with different settings, metrics will
  // be recoreded.
  keyboard_internal.settings->top_row_are_fkeys =
      !keyboard_internal.settings->top_row_are_fkeys;
  keyboard_internal.settings->six_pack_key_remappings->del =
      ui::mojom::SixPackShortcutModifier::kAlt;
  keyboard_internal.id = kKeyboardInternalId;
  keyboard_internal.settings->f11 = ui::mojom::ExtendedFkeysModifier::kDisabled;
  keyboard_internal.settings->f12 = ui::mojom::ExtendedFkeysModifier::kDisabled;
  manager_.get()->RecordKeyboardChangedMetrics(keyboard_internal,
                                               *old_settings);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.SixPackKeys.Delete.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.F11.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.F12.Changed",
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordMetricOncePerKeyboard) {
  mojom::Keyboard keyboard_external;
  keyboard_external.device_key = kExternalKeyboardId;
  keyboard_external.is_external = true;
  keyboard_external.meta_key = mojom::MetaKey::kCommand;
  keyboard_external.settings = mojom::KeyboardSettings::New();
  auto& settings_external = *keyboard_external.settings;
  settings_external.top_row_are_fkeys = true;

  mojom::Keyboard keyboard_internal;
  keyboard_internal.device_key = kInternalKeyboardDeviceKey;
  keyboard_internal.is_external = false;
  keyboard_internal.settings = mojom::KeyboardSettings::New();
  auto& settings_internal = *keyboard_internal.settings;
  settings_internal.top_row_are_fkeys = true;

  base::HistogramTester histogram_tester;
  SimulateUserLogin(kUser1);
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_external);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.External.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);

  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);

  // Call RecordKeyboardInitialMetrics with the same user and same keyboard,
  // ExpectTotalCount for Internal metric won't increase.
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/1u);

  // Call RecordKeyboardInitialMetrics with the different user but same
  // keyboard, ExpectTotalCount for Internal metric will increase.
  SimulateUserLogin(kUser2);
  manager_.get()->RecordKeyboardInitialMetrics(keyboard_internal);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.TopRowAreFKeys.Initial",
      /*expected_count=*/2u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordMouseSettings) {
  mojom::Mouse mouse;
  mouse.device_key = kExternalMouseId;
  mouse.settings = mojom::MouseSettings::New();
  mouse.settings->sensitivity = kSampleSensitivity;
  mouse.settings->button_remappings.push_back(
      mojom::ButtonRemapping::New("my-vkey", mojom::Button::NewVkey(ui::VKEY_A),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kBrightnessDown)));
  mouse.settings->button_remappings.push_back(mojom::ButtonRemapping::New(
      "middle-button",
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      mojom::RemappingAction::NewAcceleratorAction(
          AcceleratorAction::kMediaPlay)));

  base::HistogramTester histogram_tester;
  SimulateUserLogin(kUser1);
  manager_.get()->RecordMouseInitialMetrics(mouse);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Initial",
      /*expected_count=*/1u);

  // Call RecordMouseInitialMetrics with the same user and same mouse,
  // ExpectTotalCount for mouse metric won't increase.
  manager_.get()->RecordMouseInitialMetrics(mouse);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Initial",
      /*expected_count=*/1u);

  // Call RecordMouseInitialMetrics with the different user but same
  // mouse, ExpectTotalCount for mouse metric will increase.
  SimulateUserLogin(kUser2);
  manager_.get()->RecordMouseInitialMetrics(mouse);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Initial",
      /*expected_count=*/2u);

  // Call record changed settings metrics.
  const auto old_setting = mouse.settings->Clone();
  mouse.settings->sensitivity = kSampleMinSensitivity;
  mouse.settings->reverse_scrolling = !mouse.settings->reverse_scrolling;
  mouse.settings->button_remappings.at(0)->name = "renamed vkey";
  mouse.settings->button_remappings.at(1)->name = "renamed customizable button";
  manager_.get()->RecordMouseChangedMetrics(mouse, *old_setting);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.SwapPrimaryButtons.Changed",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.ReverseScrolling.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Increase",
      /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Mouse.Sensitivity.Decrease",
      /*sample=*/2u,
      /*expected_bucket_count=*/1u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.Name.Changed.Vkey",
      /*sample=*/ui::KeyboardCode::VKEY_A,
      /*expected_bucket_count=*/1u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.Name.Changed."
      "CustomizableButton",
      /*sample=*/mojom::CustomizableButton::kMiddle,
      /*expected_count=*/1u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.Name.Changed."
      "CustomizableButton",
      /*sample=*/mojom::CustomizableButton::kLeft,
      /*expected_count=*/0u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordPointingStickSettings) {
  mojom::PointingStick pointing_stick;
  pointing_stick.device_key = kPointingStickId;
  pointing_stick.settings = mojom::PointingStickSettings::New();
  pointing_stick.settings->sensitivity = kSampleSensitivity;

  base::HistogramTester histogram_tester;
  SimulateUserLogin(kUser1);
  manager_.get()->RecordPointingStickInitialMetrics(pointing_stick);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Initial",
      /*expected_count=*/1u);

  // Call RecordPointingStickInitialMetrics with the same user and same
  // pointing stick, expectTotalCount for the metric won't increase.
  manager_.get()->RecordPointingStickInitialMetrics(pointing_stick);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Initial",
      /*expected_count=*/1u);

  // Call RecordPointingStickInitialMetrics with the different user but
  // same pointing stick, expectTotalCount for the metric will increase.
  SimulateUserLogin(kUser2);
  manager_.get()->RecordPointingStickInitialMetrics(pointing_stick);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Initial",
      /*expected_count=*/2u);

  // Call record changed settings metrics.
  const auto old_setting = pointing_stick.settings->Clone();
  pointing_stick.settings->sensitivity = kSampleMaxSensitivity;
  pointing_stick.settings->swap_right = !pointing_stick.settings->swap_right;
  manager_.get()->RecordPointingStickChangedMetrics(pointing_stick,
                                                    *old_setting);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.PointingStick.AccelerationEnabled.Changed",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.PointingStick.SwapPrimaryButtons.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Increase",
      /*sample=*/2u,
      /*expected_bucket_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.PointingStick.Sensitivity.Decrease",
      /*expected_count=*/0);
}

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordTouchpadSettings) {
  scoped_feature_list_.InitWithFeatures(
      {features::kInputDeviceSettingsSplit,
       features::kAltClickAndSixPackCustomization},
      /*disabled_features=*/{});
  mojom::Touchpad touchpad_external;
  touchpad_external.device_key = kExternalTouchpadId;
  touchpad_external.is_external = true;
  touchpad_external.is_haptic = true;
  touchpad_external.settings = mojom::TouchpadSettings::New();
  touchpad_external.settings->sensitivity = kSampleSensitivity;
  touchpad_external.settings->haptic_sensitivity = kSampleSensitivity;

  base::HistogramTester histogram_tester;
  SimulateUserLogin(kUser1);
  manager_.get()->RecordTouchpadInitialMetrics(touchpad_external);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.Sensitivity.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.HapticEnabled.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.SimulateRightClick.Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Touchpad.External.HapticSensitivity.Initial",
      /*sample=*/3u, /*expected_bucket_count=*/1u);

  // Call RecordTouchpadInitialMetrics with the same user and same touchpad,
  // ExpectTotalCount for Internal metric won't increase.
  manager_.get()->RecordTouchpadInitialMetrics(touchpad_external);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.Sensitivity.Initial",
      /*expected_count=*/1u);

  // Call RecordTouchpadInitialMetrics with the different user but same
  // touchpad, ExpectTotalCount for external touchpad metric will increase.
  SimulateUserLogin(kUser2);
  manager_.get()->RecordTouchpadInitialMetrics(touchpad_external);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.Sensitivity.Initial",
      /*expected_count=*/2u);

  // Call record changed settings metrics.
  const auto old_setting = touchpad_external.settings->Clone();
  touchpad_external.settings->sensitivity = kSampleMaxSensitivity;
  touchpad_external.settings->reverse_scrolling =
      !touchpad_external.settings->reverse_scrolling;
  touchpad_external.settings->tap_dragging_enabled =
      !touchpad_external.settings->tap_dragging_enabled;
  touchpad_external.settings->tap_to_click_enabled =
      !touchpad_external.settings->tap_to_click_enabled;
  touchpad_external.settings->haptic_sensitivity = kSampleMinSensitivity;
  touchpad_external.settings->simulate_right_click =
      ui::mojom::SimulateRightClickModifier::kSearch;

  manager_.get()->RecordTouchpadChangedMetrics(touchpad_external, *old_setting);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.AccelerationEnabled.Changed",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.HapticEnabled.Changed",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.ReverseScrolling.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.Sensitivity.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.SimulateRightClick.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Touchpad.External.Sensitivity.Increase",
      /*sample=*/2u,
      /*expected_bucket_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.Sensitivity.Decrease",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.TapDragging.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.TapToClick.Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Touchpad.External.HapticSensitivity.Changed",
      /*sample=*/1u, /*expected_bucket_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.External.HapticSensitivity.Increase",
      /*expected_count=*/0);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Touchpad.External.HapticSensitivity.Decrease",
      /*sample=*/2u,
      /*expected_bucket_count=*/1u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordGraphicsTabletSettings) {
  mojom::GraphicsTablet graphics_tablet;
  graphics_tablet.device_key = kGraphicsTabletId;
  graphics_tablet.settings = mojom::GraphicsTabletSettings::New();
  graphics_tablet.settings->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New("pen-vkey",
                                  mojom::Button::NewVkey(ui::VKEY_C),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kBrightnessDown)));
  graphics_tablet.settings->pen_button_remappings.push_back(
      mojom::ButtonRemapping::New("pen-middle-button",
                                  mojom::Button::NewCustomizableButton(
                                      mojom::CustomizableButton::kMiddle),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kMediaPlay)));
  graphics_tablet.settings->tablet_button_remappings.push_back(
      mojom::ButtonRemapping::New("tablet-vkey",
                                  mojom::Button::NewVkey(ui::VKEY_B),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kBrightnessDown)));
  graphics_tablet.settings->tablet_button_remappings.push_back(
      mojom::ButtonRemapping::New("tablet-right-button",
                                  mojom::Button::NewCustomizableButton(
                                      mojom::CustomizableButton::kRight),
                                  mojom::RemappingAction::NewAcceleratorAction(
                                      AcceleratorAction::kMediaPlay)));

  base::HistogramTester histogram_tester;
  SimulateUserLogin(kUser1);
  manager_.get()->RecordGraphicsTabletInitialMetrics(graphics_tablet);
  // TODO(cambickel): Add tests for graphics tablet initial metrics.

  // Call record changed settings metrics.
  const auto old_setting = graphics_tablet.settings->Clone();
  graphics_tablet.settings->pen_button_remappings.at(0)->name = "renamed vkey";
  graphics_tablet.settings->pen_button_remappings.at(1)->name =
      "renamed customizable button";
  graphics_tablet.settings->tablet_button_remappings.at(0)->name =
      "renamed vkey";
  graphics_tablet.settings->tablet_button_remappings.at(1)->name =
      "renamed customizable button";
  manager_.get()->RecordGraphicsTabletChangedMetrics(graphics_tablet,
                                                     *old_setting);
  // Test pen button remappings.
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.GraphicsTabletPen.ButtonRemapping.Name.Changed."
      "Vkey",
      /*sample=*/ui::KeyboardCode::VKEY_C,
      /*expected_bucket_count=*/1u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.GraphicsTabletPen.ButtonRemapping.Name.Changed."
      "CustomizableButton",
      /*sample=*/mojom::CustomizableButton::kMiddle,
      /*expected_count=*/1u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.GraphicsTabletPen.ButtonRemapping.Name."
      "Changed."
      "CustomizableButton",
      /*sample=*/mojom::CustomizableButton::kRight,
      /*expected_count=*/0u);

  // Test tablet button remappings.
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.GraphicsTablet.ButtonRemapping.Name.Changed."
      "Vkey",
      /*sample=*/ui::KeyboardCode::VKEY_B,
      /*expected_bucket_count=*/1u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.GraphicsTablet.ButtonRemapping.Name.Changed."
      "CustomizableButton",
      /*sample=*/mojom::CustomizableButton::kRight,
      /*expected_count=*/1u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.GraphicsTablet.ButtonRemapping.Name.Changed."
      "CustomizableButton",
      /*sample=*/mojom::CustomizableButton::kMiddle,
      /*expected_count=*/0u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest, RecordModifierRemappingMetrics) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kExternalKeyboardId;
  keyboard.is_external = false;
  keyboard.modifier_keys = {
      ui::mojom::ModifierKey::kAlt,
      ui::mojom::ModifierKey::kMeta,
      ui::mojom::ModifierKey::kAssistant,
  };
  keyboard.settings = mojom::KeyboardSettings::New();
  keyboard.settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kControl},
  };
  base::HistogramTester histogram_tester;
  manager_.get()->RecordKeyboardInitialMetrics(keyboard);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.AltRemappedTo."
      "Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.MetaRemappedTo."
      "Initial",
      /*expected_count=*/1u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers."
      "NumberOfRemappedKeysOnStart",
      /*sample=*/1u, /*expected_bucket_count=*/1u);

  const auto old_settings = std::move(keyboard.settings);
  keyboard.settings = mojom::KeyboardSettings::New();
  keyboard.settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kControl},
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kCapsLock},
  };
  manager_.get()->RecordKeyboardChangedMetrics(keyboard, *old_settings);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.AltRemappedTo."
      "Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.MetaRemappedTo."
      "Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers."
      "AssistantRemappedTo.Changed",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers."
      "NumberOfRemappedKeysOnStart",
      /*expected_count*/ 1u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest,
       RecordModifierRemappingHashMetrics) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kExternalKeyboardId;
  keyboard.is_external = false;
  keyboard.settings = mojom::KeyboardSettings::New();
  keyboard.modifier_keys = {
      ui::mojom::ModifierKey::kMeta,      ui::mojom::ModifierKey::kControl,
      ui::mojom::ModifierKey::kAlt,       ui::mojom::ModifierKey::kCapsLock,
      ui::mojom::ModifierKey::kEscape,    ui::mojom::ModifierKey::kBackspace,
      ui::mojom::ModifierKey::kAssistant,
  };
  keyboard.settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kEscape},
      {ui::mojom::ModifierKey::kControl, ui::mojom::ModifierKey::kEscape},
  };
  base::HistogramTester histogram_tester;

  SimulateUserLogin(kUser1);

  manager_.get()->RecordKeyboardInitialMetrics(keyboard);
  // Test the hash code is correct with manually computed value.
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.Hash", 0x7654255,
      1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.Hash", 1u);

  keyboard.settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kControl},
      {ui::mojom::ModifierKey::kControl, ui::mojom::ModifierKey::kMeta},
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kEscape},
      {ui::mojom::ModifierKey::kCapsLock, ui::mojom::ModifierKey::kAssistant},
      {ui::mojom::ModifierKey::kEscape, ui::mojom::ModifierKey::kCapsLock},
      {ui::mojom::ModifierKey::kBackspace, ui::mojom::ModifierKey::kAssistant},
      {ui::mojom::ModifierKey::kAssistant, ui::mojom::ModifierKey::kVoid},
  };

  SimulateUserLogin(kUser2);
  manager_.get()->RecordKeyboardInitialMetrics(keyboard);

  // Test the hash code is correct with manually computed value.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.Hash", 0x3747501,
      1);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.Hash", 2u);
}

TEST_F(InputDeviceSettingsMetricsManagerTest,
       ResetKeyboardModifierRemappingsMetrics) {
  mojom::KeyboardPtr keyboard = mojom::Keyboard::New();
  keyboard->device_key = kExternalKeyboardId;
  keyboard->is_external = true;
  keyboard->meta_key = mojom::MetaKey::kCommand;
  keyboard->modifier_keys = {
      ui::mojom::ModifierKey::kMeta,      ui::mojom::ModifierKey::kControl,
      ui::mojom::ModifierKey::kAlt,       ui::mojom::ModifierKey::kCapsLock,
      ui::mojom::ModifierKey::kEscape,    ui::mojom::ModifierKey::kBackspace,
      ui::mojom::ModifierKey::kAssistant,
  };
  keyboard->settings = mojom::KeyboardSettings::New();
  keyboard->settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kCapsLock},
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kAssistant}};

  base::HistogramTester histogram_tester;
  const auto default_settings = mojom::KeyboardSettings::New();
  default_settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kControl, ui::mojom::ModifierKey::kMeta},
      {ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kControl}};
  SimulateUserLogin(kUser1);
  manager_.get()->RecordKeyboardNumberOfKeysReset(*keyboard, *default_settings);
  // Test the number of reset keys is correct.
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Keyboard.External.Modifiers.NumberOfKeysReset",
      /*sample=*/3u, /*expected_bucket_count=*/1u);
}

}  // namespace ash