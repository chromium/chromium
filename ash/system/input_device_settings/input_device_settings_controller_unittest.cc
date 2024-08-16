// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/input_device_settings_controller.h"

#include <cstdint>
#include <memory>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/events/event_rewriter_controller_impl.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/peripherals_app_delegate.h"
#include "ash/public/cpp/test/test_image_downloader.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_controller_impl.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_metadata.h"
#include "ash/system/input_device_settings/input_device_settings_metrics_manager.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler.h"
#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/ranges/algorithm.h"
#include "base/ranges/functional.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_simple_task_runner.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/known_user.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/bluetooth_device.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/bluetooth/test/mock_bluetooth_device.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/ash/mojom/extended_fkeys_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/devices/touchpad_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

using DeviceId = InputDeviceSettingsController::DeviceId;

namespace {

const ui::KeyboardDevice kSampleKeyboardInternal(5,
                                                 ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleKeyboardInternal",
                                                 "",
                                                 base::FilePath("path5"),
                                                 0x1111,
                                                 0x1111,
                                                 0);

const ui::KeyboardDevice kSampleKeyboardInternal2(4,
                                                  ui::INPUT_DEVICE_INTERNAL,
                                                  "kSampleKeyboardInternal2",
                                                  "",
                                                  base::FilePath("path4"),
                                                  0x1111,
                                                  0x1111,
                                                  0);

const ui::KeyboardDevice kSampleKeyboardBluetooth(10,
                                                  ui::INPUT_DEVICE_BLUETOOTH,
                                                  "kSampleKeyboardBluetooth");
const ui::KeyboardDevice kSampleKeyboardUsb(15,
                                            ui::INPUT_DEVICE_USB,
                                            "kSampleKeyboardUsb",
                                            "",
                                            base::FilePath("path15"),
                                            0x1111,
                                            0x2222,
                                            0);
const ui::KeyboardDevice kSampleKeyboardUsb2(20,
                                             ui::INPUT_DEVICE_USB,
                                             "kSampleKeyboardUsb2",
                                             "",
                                             base::FilePath("path20"),
                                             0x1111,
                                             0x3333,
                                             0);
const ui::TouchpadDevice kSampleTouchpadInternal(1,
                                                 ui::INPUT_DEVICE_INTERNAL,
                                                 "kSampleTouchpadInternal",
                                                 "",
                                                 base::FilePath(),
                                                 0x1111,
                                                 0x4444,
                                                 0);
const ui::TouchpadDevice kSampleHapticTouchpadInternal(
    25,
    ui::INPUT_DEVICE_INTERNAL,
    "kSampleHapticTouchpadInternal",
    "",
    base::FilePath(),
    0x1111,
    0x4444,
    0,
    true);
const ui::TouchpadDevice kSampleTouchpadExternal(26,
                                                 ui::INPUT_DEVICE_USB,
                                                 "kSampleTouchpadExternal",
                                                 "",
                                                 base::FilePath(),
                                                 0x1111,
                                                 0x5555,
                                                 0);
const ui::InputDevice kSamplePointingStickInternal(
    2,
    ui::INPUT_DEVICE_INTERNAL,
    "kSamplePointingStickInternal");
const ui::InputDevice kSamplePointingStickExternal(
    3,
    ui::INPUT_DEVICE_USB,
    "kSamplePointingStickExternal");
const ui::InputDevice kSampleMouseUsb(3,
                                      ui::INPUT_DEVICE_USB,
                                      "kSampleMouseUsb",
                                      /*phys=*/"",
                                      /*sys_path=*/base::FilePath(),
                                      /*vendor=*/0x0001,
                                      /*product=*/0x0002,
                                      /*version=*/0x0003);
const ui::InputDevice kSampleGraphicsTablet(4,
                                            ui::INPUT_DEVICE_USB,
                                            "kSampleGraphicsTablet",
                                            /*phys=*/"",
                                            /*sys_path=*/base::FilePath(),
                                            /*vendor=*/0x0004,
                                            /*product=*/0x0005,
                                            /*version=*/0x0006);
const ui::InputDevice kSampleUncustomizableMouse(5,
                                                 ui::INPUT_DEVICE_USB,
                                                 "kSampleUncustomizableMouse",
                                                 /*phys=*/"",
                                                 /*sys_path=*/base::FilePath(),
                                                 /*vendor=*/0xffff,
                                                 /*product=*/0xffff,
                                                 /*version=*/0x0009);
const ui::InputDevice kSampleCustomizableMouse(6,
                                               ui::INPUT_DEVICE_USB,
                                               "kSampleCustomizableMouse",
                                               /*phys=*/"",
                                               /*sys_path=*/base::FilePath(),
                                               /*vendor=*/0xffff,
                                               /*product=*/0xfffe,
                                               /*version=*/0x0009);
const ui::InputDevice kSampleKeyboardMouseCombo(7,
                                                ui::INPUT_DEVICE_USB,
                                                "kSampleKeyboardMouseCombo",
                                                /*phys=*/"",
                                                /*sys_path=*/base::FilePath(),
                                                /*vendor=*/0x046d,
                                                /*product=*/0xc548,
                                                /*version=*/0x0009);
const ui::InputDevice kSampleUnCustomizableGraphicsTablet(
    27,
    ui::INPUT_DEVICE_USB,
    "kSampleGraphicsTablet",
    /*phys=*/"",
    /*sys_path=*/base::FilePath(),
    /*vendor=*/0xeeee,
    /*product=*/0xeeee,
    /*version=*/0x0006);
const ui::InputDevice kSamplekWacomOnePenTabletS(28,
                                                 ui::INPUT_DEVICE_USB,
                                                 "kSamplekWacomOnePenTabletS",
                                                 /*phys=*/"",
                                                 /*sys_path=*/base::FilePath(),
                                                 /*vendor=*/0x0531,
                                                 /*product=*/0x0100,
                                                 /*version=*/0x0006);
const ui::KeyboardDevice kSampleKeychronKeyboard(29,
                                                 ui::INPUT_DEVICE_USB,
                                                 "kSampleKeychronKeyboard",
                                                 /*phys=*/"",
                                                 /*sys_path=*/base::FilePath(),
                                                 /*vendor=*/0x3434,
                                                 /*product=*/0x0311,
                                                 /*version=*/0);

const ui::KeyboardDevice kSampleSplitModifierKeyboard(
    21,
    ui::INPUT_DEVICE_INTERNAL,
    "kSampleSplitModifierKeyboard",
    /*has_assistant_key=*/true,
    /*has_function_key=*/true);

constexpr char kUserEmail1[] = "example1@abc.com";
constexpr char kUserEmail2[] = "joy@abc.com";
constexpr char kUserEmail3[] = "joy1@abc.com";
const AccountId kAccountId1 =
    AccountId::FromUserEmailGaiaId(kUserEmail1, kUserEmail1);
const AccountId kAccountId2 =
    AccountId::FromUserEmailGaiaId(kUserEmail2, kUserEmail2);
const AccountId kAccountId3 =
    AccountId::FromUserEmailGaiaId(kUserEmail3, kUserEmail3);

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kBluetoothDeviceName[] = "Bluetooth Device";
constexpr char kBluetoothDevicePublicAddress[] = "01:23:45:67:89:AB";

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeKeyboard(const ui::KeyboardDevice& fake_keyboard,
                       const std::string& layout) {
    fake_keyboard_devices_.push_back(fake_keyboard);

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = layout;
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/std::nullopt,
                             /*devtype=*/std::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));

    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
  }

  // Add a fake graphics tablet to DeviceDataManagerTestApi and provide layout
  // info to fake udev.
  void AddFakeGraphicsTablet(const ui::InputDevice& fake_graphics_tablet) {
    fake_graphics_tablet_devices_.push_back(fake_graphics_tablet);
    ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
        fake_graphics_tablet_devices_);
  }

  // Add a fake mouse to DeviceDataManagerTestApi and provide layout
  // info to fake udev.
  void AddFakeMouse(const ui::InputDevice& fake_mouse) {
    fake_mouse_devices_.push_back(fake_mouse);
    ui::DeviceDataManagerTestApi().SetMouseDevices(fake_mouse_devices_);
  }

  void RemoveAllDevices() {
    fake_udev_.Reset();
    fake_keyboard_devices_.clear();
    fake_graphics_tablet_devices_.clear();
    fake_mouse_devices_.clear();
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
        fake_graphics_tablet_devices_);
    ui::DeviceDataManagerTestApi().SetMouseDevices(fake_mouse_devices_);
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
  std::vector<ui::InputDevice> fake_graphics_tablet_devices_;
  std::vector<ui::InputDevice> fake_mouse_devices_;
};

mojom::KeyboardSettingsPtr CreateNewKeyboardSettings() {
  mojom::KeyboardSettingsPtr settings = mojom::KeyboardSettings::New();
  settings->six_pack_key_remappings = mojom::SixPackKeyInfo::New();
  return settings;
}

std::string GetPackageIdForTesting(const std::string& device_key) {
  return "web:https://example.com/" + device_key;
}

}  // namespace

class TestPeripheralsAppDelegate : public PeripheralsAppDelegate {
 public:
  void set_should_fail(bool should_fail) { should_fail_ = should_fail; }

  void GetCompanionAppInfo(
      const std::string& device_key,
      base::OnceCallback<void(const std::optional<mojom::CompanionAppInfo>&)>
          callback) override {
    if (should_fail_) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    auto info = mojom::CompanionAppInfo();
    info.package_id = GetPackageIdForTesting(device_key);
    std::move(callback).Run(std::move(info));
  }

 private:
  bool should_fail_ = false;
};

class FakeKeyboardPrefHandler : public KeyboardPrefHandler {
 public:
  void InitializeKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override {
    keyboard->settings = CreateNewKeyboardSettings();
    num_keyboard_settings_initialized_++;
  }

  void InitializeLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override {
    num_login_screen_keyboard_settings_initialized_++;
  }

  void UpdateKeyboardSettings(PrefService* pref_service,
                              const mojom::KeyboardPolicies& keyboard_policies,
                              const mojom::Keyboard& keyboard) override {
    num_keyboard_settings_updated_++;
  }

  void UpdateLoginScreenKeyboardSettings(
      PrefService* local_state,
      const AccountId& account_id,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override {
    num_login_screen_keyboard_settings_updated_++;
  }

  void InitializeWithDefaultKeyboardSettings(
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override {
    keyboard->settings = CreateNewKeyboardSettings();
    num_initialize_default_keyboard_settings_calls_++;
  }

  void UpdateDefaultChromeOSKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override {}

  void UpdateDefaultNonChromeOSKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override {}

  void UpdateDefaultSplitModifierKeyboardSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      const mojom::Keyboard& keyboard) override {}

  void ForceInitializeWithDefaultSettings(
      PrefService* pref_service,
      const mojom::KeyboardPolicies& keyboard_policies,
      mojom::Keyboard* keyboard) override {
    ++num_force_initialize_with_default_settings_calls_;
  }

  uint32_t num_keyboard_settings_initialized() {
    return num_keyboard_settings_initialized_;
  }

  uint32_t num_keyboard_settings_updated() {
    return num_keyboard_settings_updated_;
  }

  uint32_t num_login_screen_keyboard_settings_initialized() {
    return num_login_screen_keyboard_settings_initialized_;
  }

  uint32_t num_login_screen_keyboard_settings_updated() {
    return num_login_screen_keyboard_settings_updated_;
  }

  uint32_t num_initialize_default_keyboard_settings_calls() {
    return num_initialize_default_keyboard_settings_calls_;
  }

  uint32_t num_force_initialize_with_default_settings_calls() {
    return num_force_initialize_with_default_settings_calls_;
  }

  void reset_num_keyboard_settings_initialized() {
    num_keyboard_settings_initialized_ = 0;
  }

 private:
  uint32_t num_keyboard_settings_initialized_ = 0;
  uint32_t num_keyboard_settings_updated_ = 0;
  uint32_t num_force_initialize_with_default_settings_calls_ = 0;
  uint32_t num_login_screen_keyboard_settings_initialized_ = 0;
  uint32_t num_login_screen_keyboard_settings_updated_ = 0;
  uint32_t num_initialize_default_keyboard_settings_calls_ = 0;
};

class FakeInputDeviceSettingsControllerObserver
    : public InputDeviceSettingsController::Observer {
 public:
  void OnKeyboardConnected(const mojom::Keyboard& keyboard) override {
    num_keyboards_connected_++;
  }

  void OnKeyboardDisconnected(const mojom::Keyboard& keyboard) override {
    num_keyboards_connected_--;
  }

  void OnKeyboardSettingsUpdated(const mojom::Keyboard& keyboard) override {
    num_keyboards_settings_updated_++;
  }

  void OnTouchpadSettingsUpdated(const mojom::Touchpad& touchpad) override {
    num_touchpad_settings_updated_++;
  }

  void OnPointingStickSettingsUpdated(
      const mojom::PointingStick& pointing_stick) override {
    num_pointing_stick_settings_updated_++;
  }

  void OnGraphicsTabletConnected(
      const mojom::GraphicsTablet& graphics_tablet) override {
    num_graphics_tablets_connected_++;
  }

  void OnGraphicsTabletDisconnected(
      const mojom::GraphicsTablet& graphics_tablet) override {
    num_graphics_tablets_connected_--;
  }

  void OnGraphicsTabletSettingsUpdated(
      const mojom::GraphicsTablet& graphics_tablet) override {
    num_graphics_tablets_settings_updated_++;
  }

  void OnMouseSettingsUpdated(const mojom::Mouse& mouse) override {
    num_mouse_settings_updated_++;
  }

  void OnCustomizableMouseButtonPressed(const mojom::Mouse& mouse,
                                        const mojom::Button& button) override {
    num_mouse_buttons_pressed_++;
  }
  void OnCustomizableTabletButtonPressed(const mojom::GraphicsTablet& mouse,
                                         const mojom::Button& button) override {
    num_tablet_buttons_pressed_++;
  }
  void OnCustomizablePenButtonPressed(const mojom::GraphicsTablet& mouse,
                                      const mojom::Button& button) override {
    num_pen_buttons_pressed_++;
  }

  void OnCustomizableMouseObservingStarted(const mojom::Mouse& mouse) override {
    num_mouse_observing_started_++;
  }

  void OnCustomizableMouseObservingStopped() override {
    num_mouse_observing_stopped_++;
  }

  void OnKeyboardBatteryInfoChanged(const mojom::Keyboard& keyboard) override {
    num_keyboard_battery_info_updated_++;
  }
  void OnGraphicsTabletBatteryInfoChanged(
      const mojom::GraphicsTablet& graphics_tablet) override {
    num_graphics_tablets_battery_info_updated_++;
  }
  void OnMouseBatteryInfoChanged(const mojom::Mouse& mouse) override {
    num_mouse_battery_info_updated_++;
  }
  void OnTouchpadBatteryInfoChanged(const mojom::Touchpad& touchpad) override {
    num_touchpad_battery_info_updated_++;
  }

  void OnMouseCompanionAppInfoChanged(const mojom::Mouse& mouse) override {
    num_mouse_companion_app_info_updated_++;
  }
  void OnKeyboardCompanionAppInfoChanged(
      const mojom::Keyboard& keyboard) override {
    num_keyboard_companion_app_info_updated_++;
  }
  void OnTouchpadCompanionAppInfoChanged(
      const mojom::Touchpad& touchpad) override {
    num_keyboard_companion_app_info_updated_++;
  }
  void OnGraphicsTabletCompanionAppInfoChanged(
      const mojom::GraphicsTablet& graphics_tablet) override {
    num_keyboard_companion_app_info_updated_++;
  }

  uint32_t num_keyboards_connected() { return num_keyboards_connected_; }
  uint32_t num_graphics_tablets_connected() {
    return num_graphics_tablets_connected_;
  }
  uint32_t num_keyboards_settings_updated() {
    return num_keyboards_settings_updated_;
  }
  uint32_t num_touchpad_settings_updated() {
    return num_touchpad_settings_updated_;
  }
  uint32_t num_pointing_stick_settings_updated() {
    return num_pointing_stick_settings_updated_;
  }
  uint32_t num_graphics_tablets_settings_updated() {
    return num_graphics_tablets_settings_updated_;
  }
  uint32_t num_mouse_settings_updated() { return num_mouse_settings_updated_; }
  uint32_t num_mouse_buttons_pressed() { return num_mouse_buttons_pressed_; }
  uint32_t num_pen_buttons_pressed() { return num_pen_buttons_pressed_; }
  uint32_t num_tablet_buttons_pressed() { return num_tablet_buttons_pressed_; }
  uint32_t num_mouse_observing_started() {
    return num_mouse_observing_started_;
  }
  uint32_t num_mouse_observing_stopped() {
    return num_mouse_observing_stopped_;
  }
  uint32_t num_keyboard_battery_info_updated() {
    return num_keyboard_battery_info_updated_;
  }
  uint32_t num_graphics_tablets_battery_info_updated() {
    return num_graphics_tablets_battery_info_updated_;
  }
  uint32_t num_mouse_battery_info_updated() {
    return num_mouse_battery_info_updated_;
  }
  uint32_t num_touchpad_battery_info_updated() {
    return num_touchpad_battery_info_updated_;
  }

  uint32_t num_mouse_companion_app_info_updated() {
    return num_mouse_companion_app_info_updated_;
  }

  uint32_t num_keyboard_companion_app_info_updated() {
    return num_keyboard_companion_app_info_updated_;
  }

  uint32_t num_touchpad_companion_app_info_updated() {
    return num_touchpad_companion_app_info_updated_;
  }

  uint32_t num_graphics_tablet_companion_app_info_updated() {
    return num_graphics_tablet_companion_app_info_updated_;
  }

 private:
  uint32_t num_keyboards_connected_ = 0;
  uint32_t num_graphics_tablets_connected_ = 0;
  uint32_t num_keyboards_settings_updated_ = 0;
  uint32_t num_touchpad_settings_updated_ = 0;
  uint32_t num_pointing_stick_settings_updated_;
  uint32_t num_graphics_tablets_settings_updated_ = 0;
  uint32_t num_mouse_settings_updated_ = 0;
  uint32_t num_mouse_buttons_pressed_ = 0;
  uint32_t num_tablet_buttons_pressed_ = 0;
  uint32_t num_pen_buttons_pressed_ = 0;
  uint32_t num_mouse_observing_started_ = 0;
  uint32_t num_mouse_observing_stopped_ = 0;
  uint32_t num_keyboard_battery_info_updated_ = 0;
  uint32_t num_graphics_tablets_battery_info_updated_ = 0;
  uint32_t num_mouse_battery_info_updated_ = 0;
  uint32_t num_touchpad_battery_info_updated_ = 0;
  uint32_t num_mouse_companion_app_info_updated_ = 0;
  uint32_t num_keyboard_companion_app_info_updated_ = 0;
  uint32_t num_touchpad_companion_app_info_updated_ = 0;
  uint32_t num_graphics_tablet_companion_app_info_updated_ = 0;
};

class InputDeviceSettingsControllerTest : public NoSessionAshTestBase {
 public:
  InputDeviceSettingsControllerTest() = default;
  InputDeviceSettingsControllerTest(const InputDeviceSettingsControllerTest&) =
      delete;
  InputDeviceSettingsControllerTest& operator=(
      const InputDeviceSettingsControllerTest&) = delete;
  ~InputDeviceSettingsControllerTest() override = default;

  // testing::Test:
  void SetUp() override {
    mock_adapter_ =
        base::MakeRefCounted<testing::NiceMock<device::MockBluetoothAdapter>>();
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
    ON_CALL(*mock_adapter_, IsPowered).WillByDefault(testing::Return(true));
    ON_CALL(*mock_adapter_, IsPresent).WillByDefault(testing::Return(true));
    task_runner_ = base::MakeRefCounted<base::TestSimpleTaskRunner>();
    image_downloader_ = std::make_unique<TestImageDownloader>();
    scoped_feature_list_.InitWithFeatures(
        {features::kPeripheralCustomization,
         features::kInputDeviceSettingsSplit,
         features::kAltClickAndSixPackCustomization,
         features::kPeripheralNotification, features::kWelcomeExperience,
         ::features::kSupportF11AndF12KeyShortcuts, features::kModifierSplit,
         features::kModifierSplitDogfood},
        {});
    NoSessionAshTestBase::SetUp();
    Shell::Get()->event_rewriter_controller()->Initialize(nullptr, nullptr);
    fake_device_manager_ = std::make_unique<FakeDeviceManager>();

    // Resetter must be created before the controller is initialized.
    scoped_resetter_ = std::make_unique<
        InputDeviceSettingsController::ScopedResetterForTest>();
    observer_ = std::make_unique<FakeInputDeviceSettingsControllerObserver>();
    std::unique_ptr<FakeKeyboardPrefHandler> keyboard_pref_handler =
        std::make_unique<FakeKeyboardPrefHandler>();
    keyboard_pref_handler_ = keyboard_pref_handler.get();
    delegate_ = std::make_unique<TestPeripheralsAppDelegate>();
    controller_ = std::make_unique<InputDeviceSettingsControllerImpl>(
        local_state(), std::move(keyboard_pref_handler),
        std::make_unique<TouchpadPrefHandlerImpl>(),
        std::make_unique<MousePrefHandlerImpl>(),
        std::make_unique<PointingStickPrefHandlerImpl>(),
        std::make_unique<GraphicsTabletPrefHandlerImpl>(), task_runner_);
    controller_->SetPeripheralsAppDelegate(delegate_.get());
    controller_->AddObserver(observer_.get());
    sample_keyboards_ = {kSampleKeyboardUsb, kSampleKeyboardInternal,
                         kSampleKeyboardBluetooth};

    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->Reset();

    auto user_1_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_1_prefs->registry(), /*country=*/"",
                             /*for_test=*/true);
    auto user_2_prefs = std::make_unique<TestingPrefServiceSimple>();
    RegisterUserProfilePrefs(user_2_prefs->registry(), /*country=*/"",
                             /*for_test=*/true);

    if (should_sign_in_) {
      session_controller->AddUserSession(kUserEmail1,
                                         user_manager::UserType::kRegular,
                                         /*provide_pref_service=*/false);
      session_controller->SetUserPrefService(kAccountId1,
                                             std::move(user_1_prefs));
      session_controller->AddUserSession(kUserEmail2,
                                         user_manager::UserType::kRegular,
                                         /*provide_pref_service=*/false);
      session_controller->SetUserPrefService(kAccountId2,
                                             std::move(user_2_prefs));
      session_controller->AddUserSession(kUserEmail3,
                                         user_manager::UserType::kRegular,
                                         /*provide_pref_service=*/false);

      session_controller->SwitchActiveUser(kAccountId1);
      session_controller->SetSessionState(
          session_manager::SessionState::ACTIVE);
    }

    // Reset the `num_keyboard_settings_initialized_` to account for the
    // `InitializeKeyboardSettings` call made after test setup where we
    // simualate
    //  a user logging in.
    keyboard_pref_handler_->reset_num_keyboard_settings_initialized();
  }

  void TearDown() override {
    observer_.reset();
    controller_.reset();
    keyboard_pref_handler_ = nullptr;

    // Scoped Resetter must be deleted before the test base is teared down.
    scoped_resetter_.reset();
    NoSessionAshTestBase::TearDown();
    image_downloader_.reset();
    task_runner_.reset();
  }

  void SetActiveUser(const AccountId& account_id) {
    TestSessionControllerClient* session_controller =
        GetSessionControllerClient();
    session_controller->SwitchActiveUser(account_id);
    session_controller->SetSessionState(session_manager::SessionState::ACTIVE);
  }

  std::unique_ptr<device::MockBluetoothDevice> SetupMockBluetoothDevice(
      uint32_t vendor_id,
      uint32_t product_id,
      const char* device_name,
      const char* device_address) {
    std::unique_ptr<device::MockBluetoothDevice> mock_device =
        std::make_unique<testing::NiceMock<device::MockBluetoothDevice>>(
            mock_adapter_.get(), /*bluetooth_class=*/0, device_name,
            device_address,
            /*initially_paired=*/true, /*connected=*/true);
    mock_device->SetBatteryInfo(device::BluetoothDevice::BatteryInfo(
        device::BluetoothDevice::BatteryType::kDefault, 66));
    ON_CALL(*mock_device, GetDeviceType)
        .WillByDefault(testing::Return(device::BluetoothDeviceType::KEYBOARD));
    ON_CALL(*mock_device, GetVendorID)
        .WillByDefault(testing::Return(vendor_id));
    ON_CALL(*mock_device, GetProductID)
        .WillByDefault(testing::Return(product_id));
    return mock_device;
  }

  void SendAppUpdate(const std::string& package_id, apps::Readiness readiness) {
    auto test_app = std::make_unique<apps::App>(apps::AppType::kWeb, "app_id");
    test_app->installer_package_id = apps::PackageId::FromString(package_id);
    test_app->readiness = readiness;
    apps::AppUpdate test_update(nullptr, /*delta=*/test_app.get(), AccountId());
    controller_->OnAppUpdate(test_update);
  }

 protected:
  std::unique_ptr<InputDeviceSettingsControllerImpl> controller_;
  std::unique_ptr<TestPeripheralsAppDelegate> delegate_;
  std::unique_ptr<FakeDeviceManager> fake_device_manager_;
  std::vector<ui::InputDevice> sample_keyboards_;
  std::unique_ptr<FakeInputDeviceSettingsControllerObserver> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  std::unique_ptr<InputDeviceSettingsController::ScopedResetterForTest>
      scoped_resetter_;
  raw_ptr<FakeKeyboardPrefHandler, DanglingUntriaged> keyboard_pref_handler_ =
      nullptr;

  // Used by other instances of the InputDeviceSettingsControllerTest to control
  // whether or not to sign in within the SetUp() function. Configured to sign
  // in by default.
  bool should_sign_in_ = true;
  scoped_refptr<testing::NiceMock<device::MockBluetoothAdapter>> mock_adapter_;
  base::AutoReset<bool> modifier_split_reset_ =
      ash::switches::SetIgnoreModifierSplitSecretKeyForTest();
  std::unique_ptr<TestImageDownloader> image_downloader_;
};

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingOne) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingMultiple) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth});
  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingOne) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingThenRemovingMultiple) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, kSampleKeyboardInternal, kSampleKeyboardBluetooth});
  EXPECT_EQ(observer_->num_keyboards_connected(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  EXPECT_EQ(observer_->num_keyboards_connected(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardAddingAndRemoving) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
}

TEST_F(InputDeviceSettingsControllerTest,
       DeletesPrefsWhenInputDeviceSettingsSplitFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);

  std::unique_ptr<TestingPrefServiceSimple> pref_service =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service->registry(), /*country=*/"",
                                /*for_test=*/true);

  base::Value::Dict test_pref_value;
  test_pref_value.Set("Fake Key", base::Value::Dict());
  pref_service->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kMouseDeviceSettingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kPointingStickDeviceSettingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kTouchpadDeviceSettingsDictPref,
                        test_pref_value.Clone());
  GetSessionControllerClient()->SetUserPrefService(kAccountId3,
                                                   std::move(pref_service));

  SetActiveUser(kAccountId3);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(base::Value::Dict(), active_pref_service->GetDict(
                                     prefs::kKeyboardDeviceSettingsDictPref));
  EXPECT_EQ(base::Value::Dict(),
            active_pref_service->GetDict(prefs::kMouseDeviceSettingsDictPref));
  EXPECT_EQ(base::Value::Dict(),
            active_pref_service->GetDict(
                prefs::kPointingStickDeviceSettingsDictPref));
  EXPECT_EQ(base::Value::Dict(), active_pref_service->GetDict(
                                     prefs::kTouchpadDeviceSettingsDictPref));
}

TEST_F(InputDeviceSettingsControllerTest,
       DeletesPrefsWhenPeripheralCustomizationFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPeripheralCustomization);

  std::unique_ptr<TestingPrefServiceSimple> pref_service =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service->registry(), /*country=*/"",
                                /*for_test=*/true);

  base::Value::Dict test_pref_value;
  test_pref_value.Set("Fake Key", base::Value::Dict());
  pref_service->SetDict(prefs::kGraphicsTabletPenButtonRemappingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kGraphicsTabletTabletButtonRemappingsDictPref,
                        test_pref_value.Clone());
  pref_service->SetDict(prefs::kMouseButtonRemappingsDictPref,
                        test_pref_value.Clone());
  GetSessionControllerClient()->SetUserPrefService(kAccountId3,
                                                   std::move(pref_service));

  SetActiveUser(kAccountId3);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  EXPECT_EQ(base::Value::Dict(),
            active_pref_service->GetDict(
                prefs::kGraphicsTabletPenButtonRemappingsDictPref));
  EXPECT_EQ(base::Value::Dict(),
            active_pref_service->GetDict(
                prefs::kGraphicsTabletTabletButtonRemappingsDictPref));
  EXPECT_EQ(base::Value::Dict(), active_pref_service->GetDict(
                                     prefs::kMouseButtonRemappingsDictPref));
}

TEST_F(InputDeviceSettingsControllerTest,
       DeletesSimulateRightClickPrefsWhenAltFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(user_prefs->registry(), /*country=*/"",
                           /*for_test=*/true);

  base::Value::Dict test_pref_value;
  base::Value::Dict six_pack_remappings_dict;
  six_pack_remappings_dict.Set(
      prefs::kTouchpadSettingSimulateRightClick,
      static_cast<int>(ui::mojom::SimulateRightClickModifier::kAlt));
  test_pref_value.Set("key", std::move(six_pack_remappings_dict));
  user_prefs->SetDict(prefs::kTouchpadDeviceSettingsDictPref,
                      test_pref_value.Clone());
  GetSessionControllerClient()->SetUserPrefService(kAccountId3,
                                                   std::move(user_prefs));

  SetActiveUser(kAccountId3);
  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict devices_dict =
      active_pref_service->GetDict(prefs::kTouchpadDeviceSettingsDictPref)
          .Clone();
  base::Value::Dict* existing_settings_dict = devices_dict.FindDict("key");
  EXPECT_EQ(base::Value::Dict(), *existing_settings_dict);
}

TEST_F(InputDeviceSettingsControllerTest,
       DeletesSixPackKeyPrefsWhenAltFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  auto user_prefs = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(user_prefs->registry(), /*country=*/"",
                           /*for_test=*/true);

  base::Value::Dict test_pref_value;
  base::Value::Dict six_pack_remappings_dict;
  base::Value::Dict settings_dict;

  six_pack_remappings_dict.Set(
      prefs::kSixPackKeyPageUp,
      static_cast<int>(ui::mojom::SimulateRightClickModifier::kAlt));

  settings_dict.Set(prefs::kKeyboardSettingSixPackKeyRemappings,
                    std::move(six_pack_remappings_dict));

  test_pref_value.Set("key", std::move(settings_dict));
  user_prefs->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                      test_pref_value.Clone());
  GetSessionControllerClient()->SetUserPrefService(kAccountId3,
                                                   std::move(user_prefs));

  SetActiveUser(kAccountId3);
  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict devices_dict =
      active_pref_service->GetDict(prefs::kKeyboardDeviceSettingsDictPref)
          .Clone();
  base::Value::Dict* existing_settings_dict = devices_dict.FindDict("key");
  EXPECT_EQ(base::Value::Dict(), *existing_settings_dict);
}

TEST_F(InputDeviceSettingsControllerTest,
       InitializeSettingsWhenUserSessionChanges) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  SimulateUserLogin(kAccountId2);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
  SimulateUserLogin(kAccountId1);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsUpdated) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  auto* kb = controller_->GetKeyboard(kSampleKeyboardUsb.id);
  controller_->AddWelcomeNotificationDeviceKeyForTesting(kb->device_key);
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);

  auto settings = CreateNewKeyboardSettings();
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   std::move(settings));

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.WelcomeExperienceNotificationEvent",
      InputDeviceSettingsMetricsManager::
          WelcomeExperienceNotificationEventType::kSettingChanged,
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest, PrefsInitializedBasedOnLoginState) {
  ClearLogin();
  // Reset the `active_pref_service_` to account for the
  // `InitializeKeyboardSettings` call made after test setup where we simualate
  //  a user logging in.
  controller_->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetPrimaryUserPrefService());
  controller_->OnLoginScreenFocusedPodChanged(kAccountId1);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_initialized(),
      1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 0u);
  controller_->OnLoginScreenFocusedPodChanged(kAccountId2);
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_initialized(),
      2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 0u);
  SimulateUserLogin(kAccountId1);
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_initialized(),
      2u);
}

TEST_F(InputDeviceSettingsControllerTest, UpdateLoginScreenSettings) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);

  controller_->OnLoginScreenFocusedPodChanged(kAccountId1);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   CreateNewKeyboardSettings());
  task_runner_->RunUntilIdle();
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
  // Expect multiple calls to persist settings to the login screen prefs due to:
  // updates to the following: active pref service, keyboard settings,
  // list of keyboards.
  EXPECT_EQ(
      keyboard_pref_handler_->num_login_screen_keyboard_settings_updated(), 3u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsAreValid) {
  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardInternal,
                                        kKbdTopRowLayout1Tag);
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  const mojom::KeyboardSettingsPtr settings = CreateNewKeyboardSettings();
  settings->suppress_meta_fkey_rewrites = !kDefaultSuppressMetaFKeyRewrites;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);

  // The keyboard does not have an assistant key so therefore, it cannot be
  // remapped.
  settings->suppress_meta_fkey_rewrites = kDefaultSuppressMetaFKeyRewrites;
  settings->modifier_remappings[ui::mojom::ModifierKey::kAssistant] =
      ui::mojom::ModifierKey::kAlt;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);

  const mojom::KeyboardSettingsPtr new_settings = CreateNewKeyboardSettings();
  // Function key is not a modifier key so alt key can't be remapped to function
  // key.
  new_settings->modifier_remappings[ui::mojom::ModifierKey::kAlt] =
      ui::mojom::ModifierKey::kFunction;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   new_settings.Clone());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);
}

TEST_F(InputDeviceSettingsControllerTest, FkeySettingsAreValid) {
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard;
  Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
      kSampleKeyboardUsb, std::move(keyboard_info));
  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardUsb,
                                        kKbdTopRowLayout1Tag);

  const mojom::KeyboardSettingsPtr usb_kb_settings =
      mojom::KeyboardSettings::New();
  usb_kb_settings->f11 = ui::mojom::ExtendedFkeysModifier::kAlt;
  usb_kb_settings->f12 = ui::mojom::ExtendedFkeysModifier::kAlt;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   usb_kb_settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);
}

TEST_F(InputDeviceSettingsControllerTest,
       UpdateSplitModifierKeyboardDefaultSettings) {
  fake_device_manager_->AddFakeKeyboard(kSampleSplitModifierKeyboard,
                                        kKbdTopRowLayout1Tag);

  const mojom::KeyboardSettingsPtr usb_kb_settings =
      mojom::KeyboardSettings::New();
  usb_kb_settings->modifier_remappings[ui::mojom::ModifierKey::kFunction] =
      ui::mojom::ModifierKey::kAlt;
  controller_->SetKeyboardSettings((DeviceId)kSampleSplitModifierKeyboard.id,
                                   usb_kb_settings.Clone());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
  fake_device_manager_->RemoveAllDevices();
}

TEST_F(InputDeviceSettingsControllerTest, GraphicsTabletSettingsAreValid) {
  fake_device_manager_->AddFakeGraphicsTablet(kSampleGraphicsTablet);
  EXPECT_EQ(observer_->num_graphics_tablets_connected(), 1u);

  mojom::ButtonPtr button =
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kBack);
  controller_->OnGraphicsTabletButtonPressed(kSampleGraphicsTablet.id, *button);
  EXPECT_EQ(observer_->num_graphics_tablets_settings_updated(), 1u);

  // Create a valid settings.
  mojom::ButtonRemapping button_remapping1(
      /*name=*/"test",
      /*button=*/
      button->Clone(),
      /*remapping_action=*/
      mojom::RemappingAction::NewAcceleratorAction(
          ash::AcceleratorAction::kBrightnessDown));
  std::vector<mojom::ButtonRemappingPtr> tablet_button_remappings;
  std::vector<mojom::ButtonRemappingPtr> pen_button_remappings;
  pen_button_remappings.push_back(button_remapping1.Clone());
  mojom::GraphicsTabletSettingsPtr settings =
      mojom::GraphicsTabletSettings::New(mojo::Clone(tablet_button_remappings),
                                         mojo::Clone(pen_button_remappings));

  controller_->SetGraphicsTabletSettings(kSampleGraphicsTablet.id,
                                         settings.Clone());
  EXPECT_EQ(observer_->num_graphics_tablets_settings_updated(), 2u);

  // Invalid empty settings.
  controller_->SetGraphicsTabletSettings(kSampleGraphicsTablet.id,
                                         mojom::GraphicsTabletSettings::New());
  EXPECT_EQ(observer_->num_graphics_tablets_settings_updated(), 2u);

  // Invalid settings with a button remapping name more than 32 characters.
  settings->pen_button_remappings[0]->name =
      "This is a really long name to test the name max length";
  controller_->SetGraphicsTabletSettings(kSampleGraphicsTablet.id,
                                         settings.Clone());
  EXPECT_EQ(observer_->num_graphics_tablets_settings_updated(), 2u);

  // Valid settings with a button remapping name fewer than 32 characters.
  settings->pen_button_remappings[0]->name = "Short valid name";
  controller_->SetGraphicsTabletSettings(kSampleGraphicsTablet.id,
                                         settings.Clone());
  EXPECT_EQ(observer_->num_graphics_tablets_settings_updated(), 3u);

  // Valid settings with a button remapping name with exactly 32 characters.
  settings->pen_button_remappings[0]->name = std::string(32, 'a');
  controller_->SetGraphicsTabletSettings(kSampleGraphicsTablet.id,
                                         settings.Clone());
  EXPECT_EQ(observer_->num_graphics_tablets_settings_updated(), 4u);

  // Valid settings with a button remapping name with exactly 33 characters.
  settings->pen_button_remappings[0]->name = std::string(33, 'a');
  controller_->SetGraphicsTabletSettings(kSampleGraphicsTablet.id,
                                         settings.Clone());
  EXPECT_EQ(observer_->num_graphics_tablets_settings_updated(), 4u);
}

TEST_F(InputDeviceSettingsControllerTest,
       RecordSetKeyboardSettingsValidMetric) {
  base::HistogramTester histogram_tester;
  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardInternal,
                                        kKbdTopRowLayout1Tag);
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   CreateNewKeyboardSettings());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set keyboard with invalid settings.
  const mojom::KeyboardSettingsPtr settings = CreateNewKeyboardSettings();
  settings->suppress_meta_fkey_rewrites = !kDefaultSuppressMetaFKeyRewrites;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Keyboard.SetSettingsSucceeded", false,
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest,
       RecordSetTouchpadSettingsValidMetric) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({kSampleTouchpadInternal});

  // Set non-haptic touchpad with valid settings.
  auto settings = mojom::TouchpadSettings::New();
  settings->haptic_enabled = kDefaultHapticFeedbackEnabled;
  settings->haptic_sensitivity = kDefaultHapticSensitivity;
  controller_->SetTouchpadSettings((DeviceId)kSampleTouchpadInternal.id,
                                   settings->Clone());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Touchpad.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set non-haptic touchpad with invalid id.
  controller_->SetTouchpadSettings(/*id=*/4, mojom::TouchpadSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Touchpad.SetSettingsSucceeded", false,
      /*expected_count=*/1u);

  // Set non-haptic touchpad with invalid settings.
  controller_->SetTouchpadSettings((DeviceId)kSampleTouchpadInternal.id,
                                   mojom::TouchpadSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Touchpad.SetSettingsSucceeded", false,
      /*expected_count=*/2u);

  // Set haptic touchpad with valid settings.
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {kSampleHapticTouchpadInternal});
  controller_->SetTouchpadSettings((DeviceId)kSampleHapticTouchpadInternal.id,
                                   mojom::TouchpadSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Touchpad.SetSettingsSucceeded", true,
      /*expected_count=*/2u);
}

TEST_F(InputDeviceSettingsControllerTest,
       RecordSetPointingStickSettingsValidMetric) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {kSamplePointingStickInternal});
  controller_->SetPointingStickSettings(
      (DeviceId)kSamplePointingStickInternal.id,
      mojom::PointingStickSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.PointingStick.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set pointing stick with invalid id.
  controller_->SetPointingStickSettings(/*id=*/4,
                                        mojom::PointingStickSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.PointingStick.SetSettingsSucceeded", false,
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest, RecordSetMouseSettingsValidMetric) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetMouseDevices({kSampleMouseUsb});
  controller_->SetMouseSettings((DeviceId)kSampleMouseUsb.id,
                                mojom::MouseSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.SetSettingsSucceeded", true,
      /*expected_count=*/1u);

  // Set mouse with invalid id.
  controller_->SetMouseSettings(/*id=*/4, mojom::MouseSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.SetSettingsSucceeded", false,
      /*expected_count=*/1u);

  // Set mouse with valid id and valid settings.
  controller_->SetMouseSettings(/*id=*/3, mojom::MouseSettings::New());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.SetSettingsSucceeded", true,
      /*expected_count=*/2u);

  // Create invalid settings.
  mojom::ButtonRemapping button_remapping(
      /*name=*/"test",
      /*button=*/
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kBack),
      /*remapping_action=*/
      mojom::RemappingAction::NewAcceleratorAction(
          ash::AcceleratorAction::kBrightnessDown));
  std::vector<mojom::ButtonRemappingPtr> button_remappings;
  button_remappings.push_back(button_remapping.Clone());
  mojom::MouseSettingsPtr settings = mojom::MouseSettings::New(
      /*swap_right=*/kDefaultSwapRight, /*sensitivity=*/kDefaultSensitivity,
      /*reverse_scrolling=*/kDefaultReverseScrolling,
      /*acceleration_enabled=*/kDefaultAccelerationEnabled,
      /*scroll_sensitivity=*/kDefaultSensitivity,
      /*scroll_acceleration=*/kDefaultScrollAccelerationEnabled,
      /*button_remappings=*/mojo::Clone(button_remappings));

  // Set mouse with valid id and invalid settings.
  controller_->SetMouseSettings(
      /*id=*/3, settings->Clone());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Settings.Device.Mouse.SetSettingsSucceeded", false,
      /*expected_count=*/2u);
}

// Tests that given an invalid id, keyboard settings are not updated and
// observers are not notified.
TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsUpdatedInvalidId) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardUsb});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id + 1,
                                   CreateNewKeyboardSettings());

  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 0u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 0u);
}

TEST_F(InputDeviceSettingsControllerTest, KeyboardSettingsUpdateMultiple) {
  // The SetKeyboardSettings call should update both keyboards since they have
  // the same |device_key|.
  ui::KeyboardDevice sample_usb_keyboard_copy = kSampleKeyboardUsb;
  sample_usb_keyboard_copy.id = kSampleKeyboardUsb2.id;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardUsb, sample_usb_keyboard_copy});

  EXPECT_EQ(observer_->num_keyboards_connected(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 2u);
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   CreateNewKeyboardSettings());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);
}

TEST_F(InputDeviceSettingsControllerTest, RecordsMetricsSettings) {
  base::HistogramTester histogram_tester;
  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardUsb,
                                        kKbdTopRowLayout1Tag);
  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardUsb2,
                                        kKbdTopRowLayout1Tag);

  // Initially expect no user preferences recorded.
  // Two input device settings controllers publish this metric at the same time,
  // so we expect 2 times the number of devices for the initial count.
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/4u);
  SimulateUserLogin(kAccountId2);
  task_runner_->RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Initial",
      /*expected_count=*/6u);

  // Test Metrics Updates when setKeyboardSettings is called.
  auto updated_settings = CreateNewKeyboardSettings();
  updated_settings.get()->top_row_are_fkeys = true;
  controller_->SetKeyboardSettings(kSampleKeyboardUsb.id,
                                   std::move(updated_settings));
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS.TopRowAreFKeys."
      "Changed",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Keyboard.ExternalChromeOS."
      "BlockMetaFKeyRewrites.Changed",
      /*expected_count=*/0);

  // Test Metrics Updates when setTouchpadSettings is called.
  controller_->OnTouchpadListUpdated({kSampleTouchpadInternal}, {});
  auto settings = mojom::TouchpadSettings::New();
  settings->haptic_enabled = kDefaultHapticFeedbackEnabled;
  settings->haptic_sensitivity = kDefaultHapticSensitivity;
  controller_->SetTouchpadSettings(kSampleTouchpadInternal.id,
                                   settings.Clone());
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Touchpad.Internal.AccelerationEnabled.Changed",
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest, RecordsMetadataMetrics) {
  base::HistogramTester histogram_tester;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardInternal, kSampleKeychronKeyboard});
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {kSampleGraphicsTablet, kSampleUnCustomizableGraphicsTablet,
       kSamplekWacomOnePenTabletS});
  ui::DeviceDataManagerTestApi().SetMouseDevices(
      {kSampleMouseUsb, kSampleCustomizableMouse, kSampleUncustomizableMouse});
  // Two input device settings controllers publish this metric at the same time,
  // so we expect 2 times the number of metadata tiers for all devices.
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.GraphicsTablet.MetadataTier", MetadataTier::kNoMetadata,
      /*expected_count=*/2u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.GraphicsTablet.MetadataTier", MetadataTier::kNoMetadata,
      /*expected_count=*/2u);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.Inputs.GraphicsTablet.MetadataTier",
      MetadataTier::kHasButtonConfig,
      /*expected_count=*/2u);
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.Mouse.MetadataTier",
                                     MetadataTier::kNoMetadata,
                                     /*expected_count=*/2u);
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.Mouse.MetadataTier",
                                     MetadataTier::kNoMetadata,
                                     /*expected_count=*/2u);
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.Mouse.MetadataTier",
                                     MetadataTier::kHasButtonConfig,
                                     /*expected_count=*/2u);
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.Keyboard.MetadataTier",
                                     MetadataTier::kNoMetadata,
                                     /*expected_count=*/2u);
  histogram_tester.ExpectBucketCount("ChromeOS.Inputs.Keyboard.MetadataTier",
                                     MetadataTier::kNoMetadata,
                                     /*expected_count=*/2u);
}

TEST_F(InputDeviceSettingsControllerTest, GetGeneralizedKeyboard) {
  // If there are no keyboards, return nullptr.
  EXPECT_EQ(nullptr, controller_->GetGeneralizedKeyboard());

  // If there is only internal keyboard, return it.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  EXPECT_EQ((DeviceId)kSampleKeyboardInternal.id,
            controller_->GetGeneralizedKeyboard()->id);

  // If there are multiple external keyboards, return the external keyboard
  // which has the largest device id.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardInternal, kSampleKeyboardUsb, kSampleKeyboardUsb2});
  EXPECT_EQ((DeviceId)kSampleKeyboardUsb2.id,
            controller_->GetGeneralizedKeyboard()->id);
}

TEST_F(InputDeviceSettingsControllerTest, GetGeneralizedTopRowAreFKeys) {
  // If there no keyboards, return false.
  EXPECT_EQ(false, controller_->GetGeneralizedTopRowAreFKeys());

  // If there is only internal keyboard, return its top_row_are_fkeys value.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});

  auto internal_keyboard_settings = CreateNewKeyboardSettings();

  internal_keyboard_settings->top_row_are_fkeys = true;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   internal_keyboard_settings.Clone());
  EXPECT_EQ(true, controller_->GetGeneralizedTopRowAreFKeys());

  // If there are multiple external keyboards, return the top_row_are_fkeys
  // value of the external keyboard which has the largest device id.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {kSampleKeyboardInternal, kSampleKeyboardUsb, kSampleKeyboardUsb2});

  auto settings = CreateNewKeyboardSettings();
  ;
  settings->top_row_are_fkeys = true;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardUsb.id,
                                   settings.Clone());
  EXPECT_EQ(false, controller_->GetGeneralizedTopRowAreFKeys());
}

TEST_F(InputDeviceSettingsControllerTest,
       KeyboardSettingsAreValidWithEnterprisePolicy) {
  // Test when top_row_are_fkeys_policy doesn't exist.
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  const mojom::KeyboardSettingsPtr settings = CreateNewKeyboardSettings();
  settings->top_row_are_fkeys = !kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 1u);

  // Test when policy status is kRecommended.
  std::unique_ptr<TestingPrefServiceSimple> pref_service =
      std::make_unique<TestingPrefServiceSimple>();
  ash::RegisterUserProfilePrefs(pref_service->registry(), /*country=*/"",
                                /*for_test=*/true);
  controller_->OnActiveUserPrefServiceChanged(pref_service.get());
  pref_service->SetRecommendedPref(prefs::kSendFunctionKeys,
                                   base::Value(kDefaultTopRowAreFKeys));
  settings->top_row_are_fkeys = !kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 2u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 2u);

  // Test when policy status is kManaged and the settings are valid.
  pref_service->SetManagedPref(prefs::kSendFunctionKeys,
                               base::Value(kDefaultTopRowAreFKeys));
  settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 3u);

  // Test when policy status is kManaged and the settings are invalid.
  pref_service->SetManagedPref(prefs::kSendFunctionKeys,
                               base::Value(!kDefaultTopRowAreFKeys));
  settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings.Clone());
  EXPECT_EQ(observer_->num_keyboards_settings_updated(), 3u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_updated(), 3u);
  controller_->OnActiveUserPrefServiceChanged(
      Shell::Get()->session_controller()->GetActivePrefService());
}

TEST_F(InputDeviceSettingsControllerTest, RestoreDefaultKeyboardRemappings) {
  base::HistogramTester histogram_tester;

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});
  const mojom::KeyboardSettingsPtr settings = CreateNewKeyboardSettings();
  settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  settings->modifier_remappings[ui::mojom::ModifierKey::kMeta] =
      ui::mojom::ModifierKey::kAlt;
  controller_->SetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id,
                                   settings->Clone());

  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  EXPECT_EQ(keyboard_pref_handler_->num_keyboard_settings_initialized(), 1u);
  EXPECT_EQ(
      controller_->GetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id)
          ->modifier_remappings.size(),
      1u);
  controller_->RestoreDefaultKeyboardRemappings(
      (DeviceId)kSampleKeyboardInternal.id);
  EXPECT_EQ(
      controller_->GetKeyboardSettings((DeviceId)kSampleKeyboardInternal.id)
          ->modifier_remappings.size(),
      0u);
  histogram_tester.ExpectUniqueSample(
      "ChromeOS.Settings.Device.Keyboard.Internal.Modifiers.NumberOfKeysReset",
      /*sample=*/1u, /*expected_bucket_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest, MouseButtonPressed) {
  ui::DeviceDataManagerTestApi().SetMouseDevices({kSampleMouseUsb});

  base::HistogramTester histogram_tester;
  controller_->OnMouseButtonPressed(kSampleMouseUsb.id,
                                    *mojom::Button::NewCustomizableButton(
                                        mojom::CustomizableButton::kMiddle));
  EXPECT_EQ(1u, observer_->num_mouse_settings_updated());
  EXPECT_EQ(1u, observer_->num_mouse_buttons_pressed());
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.Registered."
      "CustomizableButton",
      /*expected_count=*/1u);

  controller_->OnMouseButtonPressed(
      kSampleMouseUsb.id,
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kSide));
  EXPECT_EQ(2u, observer_->num_mouse_settings_updated());
  EXPECT_EQ(2u, observer_->num_mouse_buttons_pressed());

  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.Registered."
      "CustomizableButton",
      /*expected_count=*/2u);
  controller_->OnMouseButtonPressed(
      kSampleMouseUsb.id,
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kExtra));
  EXPECT_EQ(3u, observer_->num_mouse_settings_updated());
  EXPECT_EQ(3u, observer_->num_mouse_buttons_pressed());
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.Registered."
      "CustomizableButton",
      /*expected_count=*/3u);

  auto* settings = controller_->GetMouseSettings(kSampleMouseUsb.id);
  ASSERT_TRUE(settings);
  ASSERT_EQ(3u, settings->button_remappings.size());
  EXPECT_EQ(
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
      *settings->button_remappings[0]->button);
  EXPECT_EQ("Middle Button", settings->button_remappings[0]->name);
  EXPECT_EQ(nullptr, settings->button_remappings[0]->remapping_action.get());

  EXPECT_EQ(
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kSide),
      *settings->button_remappings[1]->button);
  EXPECT_EQ("Other Button 1", settings->button_remappings[1]->name);
  EXPECT_EQ(nullptr, settings->button_remappings[1]->remapping_action.get());

  EXPECT_EQ(
      *mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kExtra),
      *settings->button_remappings[2]->button);
  EXPECT_EQ("Other Button 2", settings->button_remappings[2]->name);
  EXPECT_EQ(nullptr, settings->button_remappings[2]->remapping_action.get());

  // Press a same button again, the button register metrics should not emit.
  controller_->OnMouseButtonPressed(kSampleMouseUsb.id,
                                    *mojom::Button::NewCustomizableButton(
                                        mojom::CustomizableButton::kMiddle));
  EXPECT_EQ(3u, observer_->num_mouse_settings_updated());
  EXPECT_EQ(4u, observer_->num_mouse_buttons_pressed());
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.Mouse.ButtonRemapping.Registered."
      "CustomizableButton",
      /*expected_count=*/3u);
}

TEST_F(InputDeviceSettingsControllerTest, GraphicsTabletButtonPressed) {
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {kSampleGraphicsTablet});
  base::HistogramTester histogram_tester;

  mojom::ButtonPtr pen_button =
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kForward);
  mojom::ButtonPtr pen_middle_button =
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle);
  mojom::ButtonPtr tablet_button = mojom::Button::NewVkey(ui::VKEY_A);
  controller_->OnGraphicsTabletButtonPressed(kSampleGraphicsTablet.id,
                                             *pen_button);
  controller_->OnGraphicsTabletButtonPressed(kSampleGraphicsTablet.id,
                                             *pen_middle_button);
  controller_->OnGraphicsTabletButtonPressed(kSampleGraphicsTablet.id,
                                             *tablet_button);
  EXPECT_EQ(3u, observer_->num_graphics_tablets_settings_updated());
  EXPECT_EQ(1u, observer_->num_tablet_buttons_pressed());
  EXPECT_EQ(2u, observer_->num_pen_buttons_pressed());
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.GraphicsTablet.ButtonRemapping.Registered."
      "Vkey",
      /*expected_count=*/1u);
  histogram_tester.ExpectTotalCount(
      "ChromeOS.Settings.Device.GraphicsTabletPen.ButtonRemapping.Registered."
      "CustomizableButton",
      /*expected_count=*/2u);

  auto* settings =
      controller_->GetGraphicsTabletSettings(kSampleGraphicsTablet.id);
  ASSERT_TRUE(settings);
  ASSERT_EQ(2u, settings->pen_button_remappings.size());
  EXPECT_EQ(*pen_button, *settings->pen_button_remappings[0]->button);
  EXPECT_EQ("Other Button 1", settings->pen_button_remappings[0]->name);
  EXPECT_EQ(nullptr,
            settings->pen_button_remappings[0]->remapping_action.get());
  EXPECT_EQ(*pen_middle_button, *settings->pen_button_remappings[1]->button);
  EXPECT_EQ("Other Button 2", settings->pen_button_remappings[1]->name);
  EXPECT_EQ(nullptr,
            settings->pen_button_remappings[1]->remapping_action.get());

  ASSERT_EQ(1u, settings->tablet_button_remappings.size());
  EXPECT_EQ(*tablet_button, *settings->tablet_button_remappings[0]->button);
  EXPECT_EQ("Other Button 1", settings->tablet_button_remappings[0]->name);
  EXPECT_EQ(nullptr,
            settings->tablet_button_remappings[0]->remapping_action.get());
}

TEST_F(InputDeviceSettingsControllerTest, ObservingButtons) {
  ui::DeviceDataManagerTestApi().SetMouseDevices({kSampleMouseUsb});
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {kSampleGraphicsTablet});

  auto* rewriter = Shell::Get()
                       ->event_rewriter_controller()
                       ->peripheral_customization_event_rewriter();

  controller_->StartObservingButtons(kSampleMouseUsb.id);
  ASSERT_EQ(1u, rewriter->mice_to_observe().size());
  EXPECT_TRUE(rewriter->mice_to_observe().contains(kSampleMouseUsb.id));
  EXPECT_EQ(1u, observer_->num_mouse_observing_started());

  controller_->StopObservingButtons();
  ASSERT_EQ(0u, rewriter->mice_to_observe().size());
  ASSERT_EQ(0u, rewriter->graphics_tablets_to_observe().size());
  EXPECT_EQ(1u, observer_->num_mouse_observing_stopped());

  controller_->StartObservingButtons(kSampleGraphicsTablet.id);
  ASSERT_EQ(1u, rewriter->graphics_tablets_to_observe().size());
  EXPECT_TRUE(rewriter->graphics_tablets_to_observe().contains(
      kSampleGraphicsTablet.id));

  controller_->StopObservingButtons();
  ASSERT_EQ(0u, rewriter->mice_to_observe().size());
  ASSERT_EQ(0u, rewriter->graphics_tablets_to_observe().size());

  controller_->StartObservingButtons(kSampleTouchpadInternal.id);
  ASSERT_EQ(0u, rewriter->mice_to_observe().size());
  ASSERT_EQ(0u, rewriter->graphics_tablets_to_observe().size());
}

TEST_F(InputDeviceSettingsControllerTest, ObservingMouseButtons) {
  ui::DeviceDataManagerTestApi().SetMouseDevices({kSampleUncustomizableMouse,
                                                  kSampleCustomizableMouse,
                                                  kSampleKeyboardMouseCombo});

  auto* rewriter = Shell::Get()
                       ->event_rewriter_controller()
                       ->peripheral_customization_event_rewriter();

  controller_->StartObservingButtons(kSampleUncustomizableMouse.id);
  ASSERT_EQ(0u, rewriter->mice_to_observe().size());
  EXPECT_FALSE(
      rewriter->mice_to_observe().contains(kSampleUncustomizableMouse.id));

  controller_->StartObservingButtons(kSampleCustomizableMouse.id);
  ASSERT_EQ(1u, rewriter->mice_to_observe().size());
  EXPECT_TRUE(
      rewriter->mice_to_observe().contains(kSampleCustomizableMouse.id));
  EXPECT_EQ(
      rewriter->mice_to_observe().find(kSampleCustomizableMouse.id)->second,
      mojom::CustomizationRestriction::kAllowCustomizations);

  controller_->StartObservingButtons(kSampleKeyboardMouseCombo.id);
  ASSERT_EQ(2u, rewriter->mice_to_observe().size());
  EXPECT_TRUE(
      rewriter->mice_to_observe().contains(kSampleCustomizableMouse.id));
  EXPECT_EQ(
      rewriter->mice_to_observe().find(kSampleKeyboardMouseCombo.id)->second,
      mojom::CustomizationRestriction::kDisableKeyEventRewrites);
}

TEST_F(InputDeviceSettingsControllerTest, ObservingButtonsDuplicateIds) {
  auto mouse1 = kSampleMouseUsb;
  auto mouse2 = kSampleMouseUsb;
  mouse2.id = mouse2.id + 100;

  auto graphics_tablet1 = kSampleGraphicsTablet;
  auto graphics_tablet2 = kSampleGraphicsTablet;
  graphics_tablet2.id = graphics_tablet2.id + 100;

  ui::DeviceDataManagerTestApi().SetMouseDevices({mouse1});
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices({graphics_tablet1});
  ui::DeviceDataManagerTestApi().SetUncategorizedDevices(
      {mouse2, graphics_tablet2});

  auto* rewriter = Shell::Get()
                       ->event_rewriter_controller()
                       ->peripheral_customization_event_rewriter();

  controller_->StartObservingButtons(mouse1.id);
  ASSERT_EQ(2u, rewriter->mice_to_observe().size());
  EXPECT_TRUE(rewriter->mice_to_observe().contains(mouse1.id));
  EXPECT_TRUE(rewriter->mice_to_observe().contains(mouse2.id));

  controller_->StopObservingButtons();
  ASSERT_EQ(0u, rewriter->mice_to_observe().size());
  ASSERT_EQ(0u, rewriter->graphics_tablets_to_observe().size());

  controller_->StartObservingButtons(kSampleGraphicsTablet.id);
  ASSERT_EQ(2u, rewriter->graphics_tablets_to_observe().size());
  EXPECT_TRUE(
      rewriter->graphics_tablets_to_observe().contains(graphics_tablet1.id));
  EXPECT_TRUE(
      rewriter->graphics_tablets_to_observe().contains(graphics_tablet2.id));

  controller_->StopObservingButtons();
  ASSERT_EQ(0u, rewriter->mice_to_observe().size());
  ASSERT_EQ(0u, rewriter->graphics_tablets_to_observe().size());

  controller_->StartObservingButtons(kSampleTouchpadInternal.id);
  ASSERT_EQ(0u, rewriter->mice_to_observe().size());
  ASSERT_EQ(0u, rewriter->graphics_tablets_to_observe().size());
}

TEST_F(InputDeviceSettingsControllerTest, GetSettingsDuplicateIds) {
  auto mouse1 = kSampleMouseUsb;
  auto mouse2 = kSampleMouseUsb;
  mouse2.id = mouse2.id + 100;

  auto graphics_tablet1 = kSampleGraphicsTablet;
  auto graphics_tablet2 = kSampleGraphicsTablet;
  graphics_tablet2.id = graphics_tablet2.id + 100;

  ui::DeviceDataManagerTestApi().SetMouseDevices({mouse1});
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices({graphics_tablet1});
  ui::DeviceDataManagerTestApi().SetUncategorizedDevices(
      {mouse2, graphics_tablet2});

  EXPECT_EQ(controller_->GetMouseSettings(mouse1.id),
            controller_->GetMouseSettings(mouse2.id));
  EXPECT_EQ(controller_->GetGraphicsTabletSettings(graphics_tablet1.id),
            controller_->GetGraphicsTabletSettings(graphics_tablet2.id));
}
TEST_F(InputDeviceSettingsControllerTest,
       KeyboardReceivesCorrectTopRowActionKeys) {
  // `kKbdTopRowLayout1Tag` maps to the original Chrome OS Layout:
  // Browser Back, Browser Forward, Refresh, Full Screen, Overview,
  // Brightness Down, Brightness Up, Mute, Volume Down, Volume Up.
  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardInternal,
                                        kKbdTopRowLayout1Tag);
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  auto keyboards = controller_->GetConnectedKeyboards();
  EXPECT_EQ(keyboards.size(), 1u);
  auto keyboard = mojo::Clone(keyboards[0]);
  EXPECT_EQ(keyboard->top_row_action_keys.size(), 10u);
  EXPECT_EQ(keyboard->top_row_action_keys[0], mojom::TopRowActionKey::kBack);
  EXPECT_EQ(keyboard->top_row_action_keys[1], mojom::TopRowActionKey::kForward);
  EXPECT_EQ(keyboard->top_row_action_keys[2], mojom::TopRowActionKey::kRefresh);
  EXPECT_EQ(keyboard->top_row_action_keys[3],
            mojom::TopRowActionKey::kFullscreen);
  EXPECT_EQ(keyboard->top_row_action_keys[4],
            mojom::TopRowActionKey::kOverview);
  EXPECT_EQ(keyboard->top_row_action_keys[5],
            mojom::TopRowActionKey::kScreenBrightnessDown);
  EXPECT_EQ(keyboard->top_row_action_keys[6],
            mojom::TopRowActionKey::kScreenBrightnessUp);
  EXPECT_EQ(keyboard->top_row_action_keys[7],
            mojom::TopRowActionKey::kVolumeMute);
  EXPECT_EQ(keyboard->top_row_action_keys[8],
            mojom::TopRowActionKey::kVolumeDown);
  EXPECT_EQ(keyboard->top_row_action_keys[9],
            mojom::TopRowActionKey::kVolumeUp);

  fake_device_manager_->RemoveAllDevices();

  // `kKbdTopRowLayout2Tag` represents the 2017 keyboard layout:
  // Browser Forward is gone and Play/Pause key is added between
  // Brightness Up and Mute.
  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardInternal2,
                                        kKbdTopRowLayout2Tag);
  EXPECT_EQ(observer_->num_keyboards_connected(), 1u);
  keyboards = controller_->GetConnectedKeyboards();
  EXPECT_EQ(keyboards.size(), 1u);
  keyboard = mojo::Clone(keyboards[0]);
  EXPECT_EQ(keyboard->top_row_action_keys.size(), 10u);

  EXPECT_EQ(keyboard->top_row_action_keys[0], mojom::TopRowActionKey::kBack);
  EXPECT_EQ(keyboard->top_row_action_keys[1], mojom::TopRowActionKey::kRefresh);
  EXPECT_EQ(keyboard->top_row_action_keys[2],
            mojom::TopRowActionKey::kFullscreen);
  EXPECT_EQ(keyboard->top_row_action_keys[3],
            mojom::TopRowActionKey::kOverview);
  EXPECT_EQ(keyboard->top_row_action_keys[4],
            mojom::TopRowActionKey::kScreenBrightnessDown);
  EXPECT_EQ(keyboard->top_row_action_keys[5],
            mojom::TopRowActionKey::kScreenBrightnessUp);
  EXPECT_EQ(keyboard->top_row_action_keys[6],
            mojom::TopRowActionKey::kPlayPause);
  EXPECT_EQ(keyboard->top_row_action_keys[7],
            mojom::TopRowActionKey::kVolumeMute);
  EXPECT_EQ(keyboard->top_row_action_keys[8],
            mojom::TopRowActionKey::kVolumeDown);
  EXPECT_EQ(keyboard->top_row_action_keys[9],
            mojom::TopRowActionKey::kVolumeUp);
}

TEST_F(InputDeviceSettingsControllerTest, InternalTouchpadUpdatedWithPrefs) {
  ui::DeviceDataManagerTestApi().SetTouchpadDevices(
      {kSampleTouchpadInternal, kSampleTouchpadExternal});

  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_dict;
  updated_dict.Set("test_key", 1);
  pref_service->SetDict(prefs::kTouchpadInternalSettings, updated_dict.Clone());
  EXPECT_EQ(2u, observer_->num_touchpad_settings_updated());

  // If there is no internal touchpad, expect no settings updated call.
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({kSampleTouchpadExternal});
  updated_dict.Set("test_key", 2);
  pref_service->SetDict(prefs::kTouchpadInternalSettings, updated_dict.Clone());
  EXPECT_EQ(2u, observer_->num_touchpad_settings_updated());

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({kSampleTouchpadInternal});
  updated_dict.Set("test_key", 3);
  pref_service->SetDict(prefs::kTouchpadInternalSettings, updated_dict.Clone());
  EXPECT_EQ(4u, observer_->num_touchpad_settings_updated());
}

TEST_F(InputDeviceSettingsControllerTest,
       InternalPointingStickUpdatedWithPrefs) {
  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {kSamplePointingStickInternal, kSamplePointingStickExternal});

  PrefService* pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_dict;
  updated_dict.Set("test_key", 1);
  pref_service->SetDict(prefs::kPointingStickInternalSettings,
                        updated_dict.Clone());
  EXPECT_EQ(1u, observer_->num_pointing_stick_settings_updated());

  // If there is no internal touchpad, expect no settings updated call.
  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {kSamplePointingStickExternal});
  updated_dict.Set("test_key", 2);
  pref_service->SetDict(prefs::kPointingStickInternalSettings,
                        updated_dict.Clone());
  EXPECT_EQ(1u, observer_->num_pointing_stick_settings_updated());

  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {kSamplePointingStickInternal});
  updated_dict.Set("test_key", 3);
  pref_service->SetDict(prefs::kPointingStickInternalSettings,
                        updated_dict.Clone());
  EXPECT_EQ(2u, observer_->num_pointing_stick_settings_updated());
}

class InputDeviceSettingsControllerNoSignInTest
    : public InputDeviceSettingsControllerTest {
 public:
  InputDeviceSettingsControllerNoSignInTest() { should_sign_in_ = false; }
};

TEST_F(InputDeviceSettingsControllerNoSignInTest,
       NoCrashOnPodSelectionRaceConditionMouse) {
  ui::DeviceDataManagerTestApi().SetMouseDevices({kSampleMouseUsb});

  auto* mouse = controller_->GetMouse(kSampleMouseUsb.id);
  ASSERT_TRUE(mouse);
  ASSERT_TRUE(mouse->settings);
}

TEST_F(InputDeviceSettingsControllerNoSignInTest,
       NoCrashOnPodSelectionRaceConditionKeyboard) {
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({kSampleKeyboardInternal});

  auto* keyboard = controller_->GetKeyboard(kSampleKeyboardInternal.id);
  ASSERT_TRUE(keyboard);
  ASSERT_TRUE(keyboard->settings);
}

TEST_F(InputDeviceSettingsControllerNoSignInTest,
       NoCrashOnPodSelectionRaceConditionTouchpad) {
  ui::DeviceDataManagerTestApi().SetTouchpadDevices({kSampleTouchpadInternal});

  auto* touchpad = controller_->GetTouchpad(kSampleTouchpadInternal.id);
  ASSERT_TRUE(touchpad);
  ASSERT_TRUE(touchpad->settings);
}

TEST_F(InputDeviceSettingsControllerNoSignInTest,
       NoCrashOnPodSelectionRaceConditionPointingStick) {
  ui::DeviceDataManagerTestApi().SetPointingStickDevices(
      {kSamplePointingStickExternal});

  auto* pointing_stick =
      controller_->GetPointingStick(kSamplePointingStickExternal.id);
  ASSERT_TRUE(pointing_stick);
  ASSERT_TRUE(pointing_stick->settings);
}

TEST_F(InputDeviceSettingsControllerNoSignInTest,
       NoCrashOnPodSelectionRaceConditionGraphicsTablet) {
  ui::DeviceDataManagerTestApi().SetGraphicsTabletDevices(
      {kSampleGraphicsTablet});

  auto* graphics_tablet =
      controller_->GetGraphicsTablet(kSampleGraphicsTablet.id);
  ASSERT_TRUE(graphics_tablet);
  ASSERT_TRUE(graphics_tablet->settings);
}

// TODO(crbug.com/349179793): Disabled due to segfaults across platforms.
TEST_F(InputDeviceSettingsControllerTest,
       DISABLED_BatteryInfoAddedForBluetoothDevices) {
  uint32_t test_vendor_id = 0x1111;
  uint32_t test_product_id = 0x1112;
  auto mock_device = SetupMockBluetoothDevice(test_vendor_id, test_product_id,
                                              kBluetoothDeviceName,
                                              kBluetoothDevicePublicAddress);
  mock_device->SetBatteryInfo(device::BluetoothDevice::BatteryInfo(
      device::BluetoothDevice::BatteryType::kDefault, 66,
      device::BluetoothDevice::BatteryInfo::ChargeState::kDischarging));
  std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
      devices;
  devices.push_back(mock_device.get());
  ON_CALL(*mock_adapter_, GetDevices).WillByDefault(testing::Return(devices));
  ui::KeyboardDevice bluetooth_keyboard = kSampleKeyboardBluetooth;
  bluetooth_keyboard.product_id = test_product_id;
  bluetooth_keyboard.vendor_id = test_vendor_id;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({bluetooth_keyboard});
  auto* keyboard = controller_->GetKeyboard(bluetooth_keyboard.id);
  ASSERT_TRUE(keyboard);
  ASSERT_TRUE(keyboard->battery_info);
  ASSERT_EQ(mojom::ChargeState::kDischarging,
            keyboard->battery_info->charge_state);
  ASSERT_EQ(66, keyboard->battery_info->battery_percentage);
}

TEST_F(InputDeviceSettingsControllerTest, BatteryInfoUpdates) {
  // Create mock BT device for testing.
  uint32_t test_vendor_id = 0x1111;
  uint32_t test_product_id = 0x1112;
  auto mock_device = SetupMockBluetoothDevice(test_vendor_id, test_product_id,
                                              kBluetoothDeviceName,
                                              kBluetoothDevicePublicAddress);
  std::vector<raw_ptr<const device::BluetoothDevice, VectorExperimental>>
      devices;
  devices.push_back(mock_device.get());
  ON_CALL(*mock_adapter_, GetDevices).WillByDefault(testing::Return(devices));

  // Set initial battery info for BT device.
  mock_device->SetBatteryInfo(device::BluetoothDevice::BatteryInfo(
      device::BluetoothDevice::BatteryType::kDefault, 66));
  ui::KeyboardDevice bluetooth_keyboard = kSampleKeyboardBluetooth;
  bluetooth_keyboard.product_id = test_product_id;
  bluetooth_keyboard.vendor_id = test_vendor_id;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({bluetooth_keyboard});

  // Keyboard populated with initial battery info.
  auto* keyboard = controller_->GetKeyboard(bluetooth_keyboard.id);
  ASSERT_EQ(66, keyboard->battery_info->battery_percentage);

  // Battery percentage change should trigger a call to `DeviceBatteryChanged`.
  mock_device->SetBatteryInfo(device::BluetoothDevice::BatteryInfo(
      device::BluetoothDevice::BatteryType::kDefault, 65));
  keyboard = controller_->GetKeyboard(bluetooth_keyboard.id);
  ASSERT_EQ(65, keyboard->battery_info->battery_percentage);
  // Ensure pending tasks are cleared.
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDeviceSettingsControllerTest,
       KeyboardInternalDefaultsUpdatedDuringOobe) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardInternal,
                                        kKbdTopRowLayout2Tag);
  const auto* keyboard = controller_->GetKeyboard(kSampleKeyboardInternal.id);
  ASSERT_TRUE(keyboard);
  ASSERT_EQ(kDefaultTopRowAreFKeys, keyboard->settings->top_row_are_fkeys);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_defaults;
  updated_defaults.Set(prefs::kKeyboardSettingTopRowAreFKeys,
                       !kDefaultTopRowAreFKeys);
  active_pref_service->SetDict(prefs::kKeyboardDefaultChromeOSSettings,
                               std::move(updated_defaults));

  ASSERT_EQ(1u, keyboard_pref_handler_
                    ->num_force_initialize_with_default_settings_calls());
}

TEST_F(InputDeviceSettingsControllerTest,
       KeyboardExternalDefaultsUpdatedDuringOobe) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  fake_device_manager_->AddFakeKeyboard(kSampleKeyboardUsb, "");

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_defaults;
  updated_defaults.Set(prefs::kKeyboardSettingTopRowAreFKeys,
                       !kDefaultTopRowAreFKeys);
  active_pref_service->SetDict(prefs::kKeyboardDefaultNonChromeOSSettings,
                               std::move(updated_defaults));

  ASSERT_EQ(1u, keyboard_pref_handler_
                    ->num_force_initialize_with_default_settings_calls());
}

TEST_F(InputDeviceSettingsControllerTest,
       KeyboardSplitModifierTopRowAreKeysUpdated) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  fake_device_manager_->AddFakeKeyboard(kSampleSplitModifierKeyboard,
                                        kKbdTopRowLayout1Tag);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_defaults;
  updated_defaults.Set(prefs::kKeyboardSettingTopRowAreFKeys,
                       !kDefaultTopRowAreFKeys);
  active_pref_service->SetDict(prefs::kKeyboardDefaultChromeOSSettings,
                               std::move(updated_defaults));

  ASSERT_EQ(controller_->GetKeyboard(kSampleSplitModifierKeyboard.id)
                ->settings->top_row_are_fkeys,
            !kDefaultTopRowAreFKeys);

  // Change session from oobe to login primary will update the
  // kKeyboardHasSplitModifierKeyboard pref value.
  ASSERT_EQ(
      active_pref_service->GetBoolean(prefs::kKeyboardHasSplitModifierKeyboard),
      false);
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOGIN_PRIMARY);
  ASSERT_EQ(
      active_pref_service->GetBoolean(prefs::kKeyboardHasSplitModifierKeyboard),
      true);
}

TEST_F(InputDeviceSettingsControllerTest,
       KeyboardSplitModifierDefaultsUpdatedDuringOobe) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  fake_device_manager_->AddFakeKeyboard(kSampleSplitModifierKeyboard,
                                        kKbdTopRowLayout1Tag);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_defaults;
  updated_defaults.Set(prefs::kKeyboardSettingTopRowAreFKeys,
                       !kDefaultTopRowAreFKeys);
  active_pref_service->SetDict(prefs::kKeyboardDefaultSplitModifierSettings,
                               std::move(updated_defaults));

  ASSERT_EQ(1u, keyboard_pref_handler_
                    ->num_force_initialize_with_default_settings_calls());
}

TEST_F(InputDeviceSettingsControllerTest, MouseDefaultsUpdatedDuringOobe) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  ui::DeviceDataManagerTestApi().SetMouseDevices({kSampleMouseUsb});
  const auto* mouse = controller_->GetMouse(kSampleMouseUsb.id);
  ASSERT_TRUE(mouse);
  ASSERT_EQ(kDefaultReverseScrolling, mouse->settings->reverse_scrolling);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_defaults;
  updated_defaults.Set(prefs::kMouseSettingReverseScrolling,
                       !kDefaultReverseScrolling);
  active_pref_service->SetDict(prefs::kMouseDefaultSettings,
                               std::move(updated_defaults));

  ASSERT_EQ(!kDefaultReverseScrolling, mouse->settings->reverse_scrolling);
}

TEST_F(InputDeviceSettingsControllerTest, TouchpadDefaultsUpdatedDuringOobe) {
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::OOBE);

  ui::DeviceDataManagerTestApi().SetTouchpadDevices({kSampleTouchpadExternal});
  const auto* touchpad = controller_->GetTouchpad(kSampleTouchpadExternal.id);
  ASSERT_TRUE(touchpad);
  ASSERT_EQ(kDefaultReverseScrolling, touchpad->settings->reverse_scrolling);

  PrefService* active_pref_service =
      Shell::Get()->session_controller()->GetActivePrefService();
  base::Value::Dict updated_defaults;
  updated_defaults.Set(prefs::kTouchpadSettingReverseScrolling,
                       !kDefaultReverseScrolling);
  active_pref_service->SetDict(prefs::kTouchpadDefaultSettings,
                               std::move(updated_defaults));
  ASSERT_EQ(!kDefaultReverseScrolling, touchpad->settings->reverse_scrolling);
}

TEST_F(InputDeviceSettingsControllerNoSignInTest, ModifierKeyRefresh) {
  ui::KeyboardDevice test_keyboard = kSampleKeyboardInternal;
  test_keyboard.id = kSampleSplitModifierKeyboard.id;

  fake_device_manager_->AddFakeKeyboard(test_keyboard, kKbdTopRowLayout1Tag);
  {
    const auto* keyboard = controller_->GetKeyboard(test_keyboard.id);
    ASSERT_TRUE(keyboard);
    ASSERT_EQ(
        (std::vector<ui::mojom::ModifierKey>{
            ui::mojom::ModifierKey::kBackspace,
            ui::mojom::ModifierKey::kControl, ui::mojom::ModifierKey::kMeta,
            ui::mojom::ModifierKey::kEscape, ui::mojom::ModifierKey::kAlt}),
        keyboard->modifier_keys);
  }

  // Update keyboard to now be a split modifier keyboard (this usually happens
  // in reality due to flags being updated, but hard to simulate in tests).
  test_keyboard = kSampleSplitModifierKeyboard;
  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {test_keyboard, kSampleKeyboardInternal});

  SimulateUserLogin(kAccountId1);
  task_runner_->RunUntilIdle();
  {
    const auto* keyboard = controller_->GetKeyboard(test_keyboard.id);
    ASSERT_TRUE(keyboard);
    ASSERT_EQ(
        (std::vector<ui::mojom::ModifierKey>{
            ui::mojom::ModifierKey::kBackspace,
            ui::mojom::ModifierKey::kControl, ui::mojom::ModifierKey::kMeta,
            ui::mojom::ModifierKey::kEscape, ui::mojom::ModifierKey::kAlt,
            ui::mojom::ModifierKey::kFunction,
            ui::mojom::ModifierKey::kRightAlt}),
        keyboard->modifier_keys);
  }
}

TEST_F(InputDeviceSettingsControllerTest, GetCompanionAppInfo) {
  base::HistogramTester histogram_tester;
  fake_device_manager_->AddFakeMouse(kSampleMouseUsb);
  auto* mouse = controller_->GetMouse(kSampleMouseUsb.id);
  ASSERT_FALSE(mouse->app_info.is_null());
  histogram_tester.ExpectBucketCount(
      "ChromeOS.WelcomeExperienceCompanionAppState",
      InputDeviceSettingsMetricsManager::CompanionAppState::kAvailable,
      /*expected_count=*/1u);
  fake_device_manager_->RemoveAllDevices();
  delegate_->set_should_fail(/*should_fail=*/true);
  fake_device_manager_->AddFakeMouse(kSampleMouseUsb);
  mouse = controller_->GetMouse(kSampleMouseUsb.id);
  ASSERT_TRUE(mouse->app_info.is_null());
}

TEST_F(InputDeviceSettingsControllerTest, CompanionAppStateUpdated) {
  base::HistogramTester histogram_tester;
  fake_device_manager_->AddFakeMouse(kSampleMouseUsb);
  auto* mouse = controller_->GetMouse(kSampleMouseUsb.id);
  auto expected_package_id = GetPackageIdForTesting(mouse->device_key);
  ASSERT_FALSE(mouse->app_info.is_null());
  ASSERT_EQ(mojom::CompanionAppState::kAvailable, mouse->app_info->state);
  // Simulate installing the companion app.
  SendAppUpdate(expected_package_id, apps::Readiness::kReady);
  ASSERT_EQ(mojom::CompanionAppState::kInstalled, mouse->app_info->state);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.WelcomeExperienceCompanionAppState",
      InputDeviceSettingsMetricsManager::CompanionAppState::kInstalled,
      /*expected_count=*/1u);
}

TEST_F(InputDeviceSettingsControllerTest, MarketingNameOverridesGeneric) {
  const ui::InputDevice kMouseWithMarketingName(44, ui::INPUT_DEVICE_USB,
                                                "kMouseWithMarketingName",
                                                /*phys=*/"",
                                                /*sys_path=*/base::FilePath(),
                                                /*vendor=*/0x046d,
                                                /*product=*/0x405e,
                                                /*version=*/0x0003);
  // Add two mice, one with a known marketing name and one without.
  fake_device_manager_->AddFakeMouse(kSampleMouseUsb);
  fake_device_manager_->AddFakeMouse(kMouseWithMarketingName);

  auto* sample_mouse = controller_->GetMouse(kSampleMouseUsb.id);
  auto* mouse_with_marketing_name =
      controller_->GetMouse(kMouseWithMarketingName.id);

  ASSERT_EQ(kSampleMouseUsb.name, sample_mouse->name);
  ASSERT_EQ("M720 Triathlon", mouse_with_marketing_name->name);
}

}  // namespace ash
