// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ash/keyboard_capability.h"

#include <linux/input-event-codes.h>
#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/events/keyboard_capability_delegate_impl.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/scoped_file.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/prefs/pref_service.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayoutAttributeName[] = "function_row_physmap";

constexpr char kKbdTopRowLayoutUnspecified[] = "";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kKbdTopRowLayoutWilcoTag[] = "3";
constexpr char kKbdTopRowLayoutDrallionTag[] = "4";

// A tag that should fail parsing for the top row layout.
constexpr char kKbdTopRowLayoutInvalidTag[] = "X";

// A default example of the layout string read from the function_row_physmap
// sysfs attribute. The values represent the scan codes for each position
// in the top row, which maps to F-Keys.
constexpr char kKbdDefaultCustomTopRowLayout[] =
    "01 02 03 04 05 06 07 08 09 0a 0b 0c 0d 0e 0f";

// A tag that should fail parsing for the top row custom scan code string.
constexpr char kKbdInvalidCustomTopRowLayout[] = "X X X";

// This set of scan code mappings come from x86 internal vivaldi keyboards.
enum CustomTopRowScanCode : uint32_t {
  kPreviousTrack = 0x90,
  kFullscreen = 0x91,
  kOverview = 0x92,
  kScreenshot = 0x93,
  kScreenBrightnessDown = 0x94,
  kScreenBrightnessUp = 0x95,
  kPrivacyScreenToggle = 0x96,
  kKeyboardBacklightDown = 0x97,
  kKeyboardBacklightUp = 0x98,
  kNextTrack = 0x99,
  kPlayPause = 0x9A,
  kMicrophoneMute = 0x9B,
  kKeyboardBacklightToggle = 0x9E,
  kVolumeMute = 0xA0,
  kVolumeDown = 0xAE,
  kVolumeUp = 0xB0,
  kForward = 0xE9,
  kBack = 0xEA,
  kRefresh = 0xE7,
};

constexpr int kDeviceId1 = 5;
constexpr int kDeviceId2 = 10;

ui::InputDeviceType INTERNAL = ui::InputDeviceType::INPUT_DEVICE_INTERNAL;
ui::InputDeviceType EXTERNAL_USB = ui::InputDeviceType::INPUT_DEVICE_USB;
ui::InputDeviceType EXTERNAL_BLUETOOTH =
    ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH;
// For INPUT_DEVICE_UNKNOWN type, we treat it as external keyboard.
ui::InputDeviceType EXTERNAL_UNKNOWN =
    ui::InputDeviceType::INPUT_DEVICE_UNKNOWN;

struct KeyEventTestData {
  // All currently connected keyboards' connection type, e.g.
  // INPUT_DEVICE_INTERNAL.
  std::vector<ui::InputDeviceType> keyboard_connection_types;
  // All currently connected keyboards' layout types.
  std::vector<std::string> keyboard_layout_types;
  ui::KeyboardCode key_code;
  // Expected result of whether this key event exists on each keyboard.
  std::vector<bool> expected_has_key_event;
  // Expected result of whether this key event exists on all connected.
  bool expected_has_key_event_on_any_keyboard;
};

// NOTE: This only creates a simple ui::KeyboardDevice based on a device
// capabilities report; it is not suitable for subclasses of ui::KeyboardDevice.
ui::KeyboardDevice KeyboardDeviceFromCapabilities(
    int device_id,
    const ui::DeviceCapabilities& capabilities) {
  ui::EventDeviceInfo device_info = {};
  ui::CapabilitiesToDeviceInfo(capabilities, &device_info);
  return ui::KeyboardDevice{
      ui::KeyboardDevice(device_id, device_info.device_type(),
                         device_info.name(), device_info.phys(),
                         base::FilePath(capabilities.path),
                         device_info.vendor_id(), device_info.product_id(),
                         device_info.version()),
      device_info.HasKeyEvent(KEY_ASSISTANT)};
}

absl::optional<uint32_t> GetEvdevKeyCodeForScanCode(const base::ScopedFD& fd,
                                                    uint32_t scancode) {
  switch (scancode) {
    case CustomTopRowScanCode::kPreviousTrack:
      return KEY_PREVIOUSSONG;
    case CustomTopRowScanCode::kFullscreen:
      return KEY_ZOOM;
    case CustomTopRowScanCode::kOverview:
      return KEY_SCALE;
    case CustomTopRowScanCode::kScreenshot:
      return KEY_SYSRQ;
    case CustomTopRowScanCode::kScreenBrightnessDown:
      return KEY_BRIGHTNESSDOWN;
    case CustomTopRowScanCode::kScreenBrightnessUp:
      return KEY_BRIGHTNESSUP;
    case CustomTopRowScanCode::kPrivacyScreenToggle:
      return KEY_PRIVACY_SCREEN_TOGGLE;
    case CustomTopRowScanCode::kKeyboardBacklightDown:
      return KEY_KBDILLUMDOWN;
    case CustomTopRowScanCode::kKeyboardBacklightUp:
      return KEY_KBDILLUMUP;
    case CustomTopRowScanCode::kNextTrack:
      return KEY_NEXTSONG;
    case CustomTopRowScanCode::kPlayPause:
      return KEY_PLAYPAUSE;
    case CustomTopRowScanCode::kMicrophoneMute:
      return KEY_MICMUTE;
    case CustomTopRowScanCode::kKeyboardBacklightToggle:
      return KEY_KBDILLUMTOGGLE;
    case CustomTopRowScanCode::kVolumeMute:
      return KEY_MUTE;
    case CustomTopRowScanCode::kVolumeDown:
      return KEY_VOLUMEDOWN;
    case CustomTopRowScanCode::kVolumeUp:
      return KEY_VOLUMEUP;
    case CustomTopRowScanCode::kForward:
      return KEY_FORWARD;
    case CustomTopRowScanCode::kBack:
      return KEY_BACK;
    case CustomTopRowScanCode::kRefresh:
      return KEY_REFRESH;
  }

  return absl::nullopt;
}

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeKeyboard(const ui::KeyboardDevice& fake_keyboard,
                       const std::string& layout,
                       bool has_custom_top_row = false) {
    fake_keyboard_devices_.push_back(fake_keyboard);

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    if (has_custom_top_row) {
      sysfs_attributes[kKbdTopRowLayoutAttributeName] = layout;
    } else {
      sysfs_properties[kKbdTopRowPropertyName] = layout;
    }
    fake_udev_.Reset();
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/absl::nullopt,
                             /*devtype=*/absl::nullopt,
                             std::move(sysfs_attributes),
                             std::move(sysfs_properties));
  }

  void RemoveAllDevices() {
    fake_udev_.Reset();
    fake_keyboard_devices_.clear();
  }

 private:
  testing::FakeUdevLoader fake_udev_;
  std::vector<ui::KeyboardDevice> fake_keyboard_devices_;
};

class TestObserver : public ui::KeyboardCapability::Observer {
 public:
  TestObserver() = default;
  TestObserver(const TestObserver&) = delete;
  TestObserver& operator=(const TestObserver&) = delete;
  ~TestObserver() override = default;

  void OnTopRowKeysAreFKeysChanged() override {
    ++top_row_keys_are_f_keys_changed_count_;
  }

  int top_row_keys_are_f_keys_changed_count() {
    return top_row_keys_are_f_keys_changed_count_;
  }

 private:
  int top_row_keys_are_f_keys_changed_count_ = 0;
};

}  // namespace

class KeyboardCapabilityTest : public NoSessionAshTestBase {
 public:
  KeyboardCapabilityTest() = default;
  ~KeyboardCapabilityTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    keyboard_capability_ = std::make_unique<ui::KeyboardCapability>(
        base::BindRepeating(&GetEvdevKeyCodeForScanCode),
        std::make_unique<KeyboardCapabilityDelegateImpl>());
    SimulateUserLogin(/*user_email=*/"email@google.com");
    test_observer_ = std::make_unique<TestObserver>();
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
    keyboard_capability_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    keyboard_capability_->RemoveObserver(test_observer_.get());
    keyboard_capability_.reset();
    AshTestBase::TearDown();
  }

  ui::KeyboardDevice AddFakeKeyboardInfoToKeyboardCapability(
      int device_id,
      ui::DeviceCapabilities capabilities,
      ui::KeyboardCapability::DeviceType device_type) {
    ui::KeyboardCapability::KeyboardInfo keyboard_info;
    keyboard_info.device_type = device_type;
    ui::KeyboardDevice fake_keyboard =
        KeyboardDeviceFromCapabilities(device_id, capabilities);

    keyboard_capability_->SetKeyboardInfoForTesting(fake_keyboard,
                                                    std::move(keyboard_info));

    return fake_keyboard;
  }

 protected:
  std::unique_ptr<ui::KeyboardCapability> keyboard_capability_;
  std::unique_ptr<TestObserver> test_observer_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

TEST_F(KeyboardCapabilityTest, TestObserver) {
  EXPECT_EQ(0, test_observer_->top_row_keys_are_f_keys_changed_count());
  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());
  PrefService* prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  prefs->SetBoolean(prefs::kSendFunctionKeys, true);
  prefs->CommitPendingWrite();

  EXPECT_TRUE(keyboard_capability_->TopRowKeysAreFKeys());
  EXPECT_EQ(1, test_observer_->top_row_keys_are_f_keys_changed_count());

  prefs->SetBoolean(prefs::kSendFunctionKeys, false);
  prefs->CommitPendingWrite();

  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());
  EXPECT_EQ(2, test_observer_->top_row_keys_are_f_keys_changed_count());
}

TEST_F(KeyboardCapabilityTest, TestTopRowKeysAreFKeys) {
  // Top row keys are F-Keys pref is false in default.
  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());

  keyboard_capability_->SetTopRowKeysAsFKeysEnabledForTesting(true);
  EXPECT_TRUE(keyboard_capability_->TopRowKeysAreFKeys());

  keyboard_capability_->SetTopRowKeysAsFKeysEnabledForTesting(false);
  EXPECT_FALSE(keyboard_capability_->TopRowKeysAreFKeys());
}

TEST_F(KeyboardCapabilityTest, TestIsSixPackKey) {
  for (const auto& [key_code, _] : ui::kSixPackKeyToSystemKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsSixPackKey(key_code));
  }

  // A key not in the kSixPackKeyToSystemKeyMap is not a six pack key.
  EXPECT_FALSE(keyboard_capability_->IsSixPackKey(ui::KeyboardCode::VKEY_A));
}

TEST_F(KeyboardCapabilityTest, TestIsReversedSixPackKey) {
  for (const auto& [key_code, _] : ui::kReversedSixPackKeyToSystemKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsReversedSixPackKey(key_code));
  }
  EXPECT_TRUE(
      keyboard_capability_->IsReversedSixPackKey(ui::KeyboardCode::VKEY_BACK));

  // A key not in the kReversedSixPackKeyToSystemKeyMap or as [Back] is not a
  // reversed six pack key.
  EXPECT_FALSE(
      keyboard_capability_->IsReversedSixPackKey(ui::KeyboardCode::VKEY_A));
}

TEST_F(KeyboardCapabilityTest, TestGetMappedFKeyIfExists) {
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"fake_Keyboard");
  fake_keyboard.sys_path = base::FilePath("path1");

  // Add a fake layout1 keyboard.
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);
  for (const auto& [key_code, f_key] : ui::kLayout1TopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_MEDIA_PLAY_PAUSE key is not a top row key for layout1.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(
                       ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE, fake_keyboard)
                   .has_value());

  // Add a fake layout2 keyboard.
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout2Tag);
  for (const auto& [key_code, f_key] : ui::kLayout2TopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_BROWSER_FORWARD key is not a top row key for layout2.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(
                       ui::KeyboardCode::VKEY_BROWSER_FORWARD, fake_keyboard)
                   .has_value());

  // Add a fake wilco keyboard.
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard,
                                          kKbdTopRowLayoutWilcoTag);
  for (const auto& [key_code, f_key] :
       ui::kLayoutWilcoDrallionTopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_MEDIA_PLAY_PAUSE key is not a top row key for wilco layout.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(
                       ui::KeyboardCode::VKEY_MEDIA_PLAY_PAUSE, fake_keyboard)
                   .has_value());

  // Add a fake drallion keyboard.
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard,
                                          kKbdTopRowLayoutDrallionTag);
  for (const auto& [key_code, f_key] :
       ui::kLayoutWilcoDrallionTopRowKeyToFKeyMap) {
    EXPECT_EQ(f_key, keyboard_capability_
                         ->GetMappedFKeyIfExists(key_code, fake_keyboard)
                         .value());
  }
  // VKEY_BROWSER_FORWARD key is not a top row key for drallion layout.
  EXPECT_FALSE(keyboard_capability_
                   ->GetMappedFKeyIfExists(
                       ui::KeyboardCode::VKEY_BROWSER_FORWARD, fake_keyboard)
                   .has_value());
}

TEST_F(KeyboardCapabilityTest, TestHasLauncherButton) {
  // Add a non-layout2 keyboard.
  ui::KeyboardDevice fake_keyboard1(
      /*id=*/kDeviceId1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);

  // Provide specific keyboard. Launcher button depends on if the keyboard is
  // layout2 type.
  EXPECT_FALSE(keyboard_capability_->HasLauncherButton(fake_keyboard1));
  // Do not provide specific keyboard. Launcher button depends on if any one
  // of the keyboards is layout2 type.
  EXPECT_FALSE(keyboard_capability_->HasLauncherButtonOnAnyKeyboard());

  // Add a layout2 keyboard.
  ui::KeyboardDevice fake_keyboard2(
      /*id=*/kDeviceId2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout2Tag);

  EXPECT_FALSE(keyboard_capability_->HasLauncherButton(fake_keyboard1));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButton(fake_keyboard2));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButtonOnAnyKeyboard());
}

TEST_F(KeyboardCapabilityTest, TestHasSixPackKey) {
  // Add an internal keyboard.
  ui::KeyboardDevice fake_keyboard1(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);

  // Internal keyboard doesn't have six pack key.
  EXPECT_FALSE(keyboard_capability_->HasSixPackKey(fake_keyboard1));
  EXPECT_FALSE(keyboard_capability_->HasSixPackOnAnyKeyboard());

  // Add an external keyboard.
  ui::KeyboardDevice fake_keyboard2(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout1Tag);

  // External keyboard has six pack key.
  EXPECT_TRUE(keyboard_capability_->HasSixPackKey(fake_keyboard2));
  EXPECT_TRUE(keyboard_capability_->HasSixPackOnAnyKeyboard());
}

TEST_F(KeyboardCapabilityTest, TestRemoveDevicesFromList) {
  const ui::KeyboardDevice input_device1 =
      AddFakeKeyboardInfoToKeyboardCapability(
          kDeviceId1, ui::kEveKeyboard,
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard);
  const ui::KeyboardDevice input_device2 =
      AddFakeKeyboardInfoToKeyboardCapability(
          kDeviceId2, ui::kHpUsbKeyboard,
          ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard);

  ui::DeviceDataManagerTestApi().SetKeyboardDevices(
      {input_device1, input_device2});
  ASSERT_EQ(2u, keyboard_capability_->keyboard_info_map().size());

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({input_device1});
  ASSERT_EQ(1u, keyboard_capability_->keyboard_info_map().size());
  EXPECT_TRUE(keyboard_capability_->keyboard_info_map().contains(kDeviceId1));

  ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
  ASSERT_EQ(0u, keyboard_capability_->keyboard_info_map().size());
}

TEST_F(KeyboardCapabilityTest, TestIdentifyRevenKeyboard) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kRevenBranding);

  ui::KeyboardDevice internal_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"internal keyboard");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayoutUnspecified);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceInternalRevenKeyboard,
            keyboard_capability_->GetDeviceType(internal_keyboard));
}

TEST_F(KeyboardCapabilityTest, TestIsTopRowKey) {
  for (const auto& [key_code, _] : ui::kLayout1TopRowKeyToFKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }
  for (const auto& [key_code, _] : ui::kLayout2TopRowKeyToFKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }
  for (const auto& [key_code, _] : ui::kLayoutWilcoDrallionTopRowKeyToFKeyMap) {
    EXPECT_TRUE(keyboard_capability_->IsTopRowKey(key_code));
  }

  // A key not in any of the above maps is not a top row key.
  EXPECT_FALSE(keyboard_capability_->IsTopRowKey(ui::KeyboardCode::VKEY_A));
}

TEST_F(KeyboardCapabilityTest, TestHasGlobeKey) {
  ui::KeyboardDevice external_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard1");
  external_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_FALSE(keyboard_capability_->HasGlobeKey(external_keyboard));

  ui::KeyboardDevice internal_keyboard_layout1(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  internal_keyboard_layout1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_layout1,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasGlobeKey(internal_keyboard_layout1));

  ui::KeyboardDevice internal_keyboard_layout2(
      /*id=*/3, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard3");
  internal_keyboard_layout2.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_layout2,
                                          kKbdTopRowLayout2Tag);
  EXPECT_FALSE(keyboard_capability_->HasGlobeKey(internal_keyboard_layout2));

  ui::KeyboardDevice internal_keyboard_layout_custom(
      /*id=*/4, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard4");
  internal_keyboard_layout_custom.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_layout_custom,
                                          kKbdDefaultCustomTopRowLayout,
                                          /*has_custom_top_row=*/true);
  EXPECT_FALSE(
      keyboard_capability_->HasGlobeKey(internal_keyboard_layout_custom));

  ui::KeyboardDevice internal_keyboard_wilco(
      /*id=*/5, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard5");
  internal_keyboard_wilco.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_wilco,
                                          kKbdTopRowLayoutWilcoTag);
  EXPECT_TRUE(keyboard_capability_->HasGlobeKey(internal_keyboard_wilco));

  ui::KeyboardDevice internal_keyboard_drallion(
      /*id=*/6, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard6");
  internal_keyboard_drallion.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_drallion,
                                          kKbdTopRowLayoutDrallionTag);
  EXPECT_TRUE(keyboard_capability_->HasGlobeKey(internal_keyboard_drallion));
}

TEST_F(KeyboardCapabilityTest, TestHasCalculatorKey) {
  ui::KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasCalculatorKey(internal_keyboard));

  ui::KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasCalculatorKey(external_keyboard));
}

TEST_F(KeyboardCapabilityTest, TestHasBrowserSearchKey) {
  ui::KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasBrowserSearchKey(internal_keyboard));

  ui::KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasBrowserSearchKey(external_keyboard));
}

TEST_F(KeyboardCapabilityTest, TestHasMediaKeys) {
  ui::KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasMediaKeys(internal_keyboard));

  ui::KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasMediaKeys(external_keyboard));
}

TEST_F(KeyboardCapabilityTest, TestHasHelpKey) {
  ui::KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasHelpKey(internal_keyboard));

  ui::KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasHelpKey(external_keyboard));
}

TEST_F(KeyboardCapabilityTest, TestHasSettingsKey) {
  ui::KeyboardDevice internal_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(keyboard_capability_->HasSettingsKey(internal_keyboard));

  ui::KeyboardDevice external_keyboard(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  external_keyboard.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(external_keyboard,
                                          kKbdTopRowLayoutUnspecified);
  EXPECT_TRUE(keyboard_capability_->HasSettingsKey(external_keyboard));
}

TEST_F(KeyboardCapabilityTest, TestHasPrivacyScreenKey) {
  ui::KeyboardDevice internal_keyboard_layout1(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  internal_keyboard_layout1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_layout1,
                                          kKbdTopRowLayout1Tag);
  EXPECT_FALSE(
      keyboard_capability_->HasPrivacyScreenKey(internal_keyboard_layout1));

  ui::KeyboardDevice internal_keyboard_drallion(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  internal_keyboard_drallion.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(internal_keyboard_drallion,
                                          kKbdTopRowLayoutDrallionTag);
  EXPECT_FALSE(
      keyboard_capability_->HasPrivacyScreenKey(internal_keyboard_drallion));

  keyboard_capability_->SetPrivacyScreenSupportedForTesting(
      /*is_supported=*/true);
  EXPECT_FALSE(
      keyboard_capability_->HasPrivacyScreenKey(internal_keyboard_layout1));
  EXPECT_TRUE(
      keyboard_capability_->HasPrivacyScreenKey(internal_keyboard_drallion));
}

class ModifierKeyTest : public KeyboardCapabilityTest,
                        public testing::WithParamInterface<
                            std::tuple<ui::DeviceCapabilities,
                                       ui::KeyboardCapability::DeviceType,
                                       std::vector<ui::mojom::ModifierKey>>> {};

// Tests that the given `ui::DeviceCapabilities` and
// `ui::KeyboardCapability::DeviceType` combo generates the given set of
// modifier keys.
INSTANTIATE_TEST_SUITE_P(
    All,
    ModifierKeyTest,
    testing::ValuesIn(std::vector<
                      std::tuple<ui::DeviceCapabilities,
                                 ui::KeyboardCapability::DeviceType,
                                 std::vector<ui::mojom::ModifierKey>>>{
        {ui::kEveKeyboard,
         ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
         {ui::mojom::ModifierKey::kBackspace, ui::mojom::ModifierKey::kControl,
          ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kEscape,
          ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kAssistant}},
        {ui::kDrobitKeyboard,
         ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
         {ui::mojom::ModifierKey::kBackspace, ui::mojom::ModifierKey::kControl,
          ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kEscape,
          ui::mojom::ModifierKey::kAlt}},
        {ui::kLogitechKeyboardK120,
         ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard,
         {ui::mojom::ModifierKey::kBackspace, ui::mojom::ModifierKey::kControl,
          ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kEscape,
          ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kCapsLock}},
        {ui::kHpUsbKeyboard,
         ui::KeyboardCapability::DeviceType::kDeviceExternalGenericKeyboard,
         {ui::mojom::ModifierKey::kBackspace, ui::mojom::ModifierKey::kControl,
          ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kEscape,
          ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kCapsLock}},
        // Tests that an external chromeos keyboard correctly omits capslock.
        {ui::kHpUsbKeyboard,
         ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard,
         {ui::mojom::ModifierKey::kBackspace, ui::mojom::ModifierKey::kControl,
          ui::mojom::ModifierKey::kMeta, ui::mojom::ModifierKey::kEscape,
          ui::mojom::ModifierKey::kAlt}}}));

TEST_P(ModifierKeyTest, TestGetModifierKeys) {
  auto [capabilities, device_type, expected_modifier_keys] = GetParam();

  const ui::KeyboardDevice test_keyboard =
      AddFakeKeyboardInfoToKeyboardCapability(kDeviceId1, capabilities,
                                              device_type);
  auto modifier_keys = keyboard_capability_->GetModifierKeys(test_keyboard);

  base::ranges::sort(expected_modifier_keys);
  base::ranges::sort(modifier_keys);
  EXPECT_EQ(expected_modifier_keys, modifier_keys);
}

class KeyEventTest : public KeyboardCapabilityTest,
                     public testing::WithParamInterface<KeyEventTestData> {};

// Tests that given the keyboard connection type and layout type, check if this
// keyboard has a specific key event.
INSTANTIATE_TEST_SUITE_P(
    All,
    KeyEventTest,
    testing::ValuesIn(std::vector<KeyEventTestData>{
        // Testing top row keys.
        {{INTERNAL},
         {kKbdTopRowLayout1Tag},
         ui::VKEY_BROWSER_FORWARD,
         {true},
         true},
        {{EXTERNAL_BLUETOOTH},
         {kKbdTopRowLayout1Tag},
         ui::VKEY_ZOOM,
         {true},
         true},
        {{EXTERNAL_USB},
         {kKbdTopRowLayout1Tag},
         ui::VKEY_MEDIA_PLAY_PAUSE,
         {false},
         false},
        {{INTERNAL},
         {kKbdTopRowLayout2Tag},
         ui::VKEY_BROWSER_FORWARD,
         {false},
         false},
        {{EXTERNAL_UNKNOWN},
         {kKbdTopRowLayout2Tag},
         ui::VKEY_MEDIA_PLAY_PAUSE,
         {true},
         true},
        {{INTERNAL}, {kKbdTopRowLayoutWilcoTag}, ui::VKEY_ZOOM, {true}, true},
        {{EXTERNAL_BLUETOOTH},
         {kKbdTopRowLayoutDrallionTag},
         ui::VKEY_BRIGHTNESS_UP,
         {true},
         true},
        {{INTERNAL, EXTERNAL_BLUETOOTH},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag},
         ui::VKEY_BROWSER_FORWARD,
         {true, false},
         true},
        {{INTERNAL, EXTERNAL_BLUETOOTH},
         {kKbdTopRowLayout2Tag, kKbdTopRowLayout2Tag},
         ui::VKEY_BROWSER_FORWARD,
         {false, false},
         false},
        {{INTERNAL, EXTERNAL_USB, EXTERNAL_BLUETOOTH},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag, kKbdTopRowLayoutWilcoTag},
         ui::VKEY_VOLUME_UP,
         {true, true, true},
         true},

        // Testing six pack keys.
        {{INTERNAL}, {kKbdTopRowLayout1Tag}, ui::VKEY_INSERT, {false}, false},
        {{EXTERNAL_USB}, {kKbdTopRowLayout1Tag}, ui::VKEY_INSERT, {true}, true},
        {{INTERNAL, EXTERNAL_BLUETOOTH},
         {kKbdTopRowLayout1Tag, kKbdTopRowLayoutWilcoTag},
         ui::VKEY_HOME,
         {false, true},
         true},

        // Testing other keys.
        {{INTERNAL}, {kKbdTopRowLayout1Tag}, ui::VKEY_LEFT, {true}, true},
        {{EXTERNAL_BLUETOOTH},
         {kKbdTopRowLayout2Tag},
         ui::VKEY_ESCAPE,
         {true},
         true},
        {{EXTERNAL_UNKNOWN},
         {kKbdTopRowLayoutWilcoTag},
         ui::VKEY_A,
         {true},
         true},
        {{INTERNAL}, {kKbdTopRowLayoutDrallionTag}, ui::VKEY_2, {true}, true},
    }));

TEST_P(KeyEventTest, TestHasKeyEvent) {
  auto [keyboard_connection_types, keyboard_layout_types, key_code,
        expected_has_key_event, expected_has_key_event_on_any_keyboard] =
      GetParam();

  fake_keyboard_manager_->RemoveAllDevices();
  for (size_t i = 0; i < keyboard_layout_types.size(); i++) {
    std::string layout = keyboard_layout_types[i];
    ui::KeyboardDevice fake_keyboard(
        /*id=*/i, /*type=*/keyboard_connection_types[i],
        /*name=*/layout);
    fake_keyboard.sys_path = base::FilePath("path" + layout);
    fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, layout);

    if (expected_has_key_event[i]) {
      EXPECT_TRUE(keyboard_capability_->HasKeyEvent(key_code, fake_keyboard));
    } else {
      EXPECT_FALSE(keyboard_capability_->HasKeyEvent(key_code, fake_keyboard));
    }
  }

  if (expected_has_key_event_on_any_keyboard) {
    EXPECT_TRUE(keyboard_capability_->HasKeyEventOnAnyKeyboard(key_code));
  } else {
    EXPECT_FALSE(keyboard_capability_->HasKeyEventOnAnyKeyboard(key_code));
  }
}

TEST_F(KeyboardCapabilityTest, TestHasAssistantKey) {
  // Add a fake kEveKeyboard keyboard, which has the assistant key.
  const ui::KeyboardDevice test_keyboard_1 =
      AddFakeKeyboardInfoToKeyboardCapability(
          kDeviceId1, ui::kEveKeyboard,
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard);

  EXPECT_TRUE(keyboard_capability_->HasAssistantKey(test_keyboard_1));

  // Add a fake kDrallionKeyboard keyboard, which does not have the assistant
  // key.
  const ui::KeyboardDevice test_keyboard_2 =
      AddFakeKeyboardInfoToKeyboardCapability(
          kDeviceId1, ui::kDrallionKeyboard,
          ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard);

  EXPECT_FALSE(keyboard_capability_->HasAssistantKey(test_keyboard_2));
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardUnspecified) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutUnspecified);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
      keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardInvalidLayoutTag) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutInvalidTag);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceUnknown,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
      keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_TRUE(!keyboard_capability_->GetTopRowScanCodes(input_device) ||
              keyboard_capability_->GetTopRowScanCodes(input_device)->empty());
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardInvalidCustomLayout) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(
      input_device, kKbdInvalidCustomTopRowLayout, /*has_custom_top_row=*/true);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault,
      keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardLayout1External) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_UNKNOWN,
                                  "External Chrome Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout1Tag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout1,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardLayout2External) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_UNKNOWN,
                                  "External Chrome Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout2Tag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceExternalChromeOsKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardCustomLayout) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Custom Layout Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdDefaultCustomTopRowLayout,
                                          /*has_custom_top_row=*/true);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutCustom,
      keyboard_capability_->GetTopRowLayout(input_device));

  const auto* top_row_scan_codes_ptr =
      keyboard_capability_->GetTopRowScanCodes(input_device);
  ASSERT_TRUE(top_row_scan_codes_ptr);
  const auto& top_row_scan_codes = *top_row_scan_codes_ptr;

  // Basic inspection to match kKbdDefaultCustomTopRowLayout
  EXPECT_EQ(15u, top_row_scan_codes.size());

  for (size_t i = 0; i < top_row_scan_codes.size(); i++) {
    EXPECT_EQ(i + 1, top_row_scan_codes[i]);
  }
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardWilcoTopRowLayout) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutWilcoTag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutWilco,
            keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_F(KeyboardCapabilityTest, IdentifyKeyboardDrallionTopRowLayout) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device,
                                          kKbdTopRowLayoutDrallionTag,
                                          /*has_custom_top_row=*/false);

  EXPECT_EQ(ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard,
            keyboard_capability_->GetDeviceType(input_device));
  EXPECT_EQ(
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDrallion,
      keyboard_capability_->GetTopRowLayout(input_device));
  EXPECT_EQ(0u, keyboard_capability_->GetTopRowScanCodes(input_device)->size());
}

TEST_F(KeyboardCapabilityTest, TopRowLayout1) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout1Tag,
                                          /*has_custom_top_row=*/false);

  for (ui::TopRowActionKey action_key = ui::TopRowActionKey::kMinValue;
       action_key <= ui::TopRowActionKey::kMaxValue;
       action_key =
           static_cast<ui::TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(
        ui::kLayout1TopRowActionKeys.contains(action_key),
        keyboard_capability_->HasTopRowActionKey(input_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  ui::KeyboardCode expected_fkey = ui::VKEY_F1;
  for (const auto action_key : ui::kLayout1TopRowActionKeys) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 input_device, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  input_device, expected_fkey));
    expected_fkey =
        static_cast<ui::KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

TEST_F(KeyboardCapabilityTest, TopRowLayout2) {
  ui::KeyboardDevice input_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(input_device, kKbdTopRowLayout2Tag,
                                          /*has_custom_top_row=*/false);

  for (ui::TopRowActionKey action_key = ui::TopRowActionKey::kMinValue;
       action_key <= ui::TopRowActionKey::kMaxValue;
       action_key =
           static_cast<ui::TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(
        ui::kLayout2TopRowActionKeys.contains(action_key),
        keyboard_capability_->HasTopRowActionKey(input_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  ui::KeyboardCode expected_fkey = ui::VKEY_F1;
  for (const auto action_key : ui::kLayout2TopRowActionKeys) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 input_device, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  input_device, expected_fkey));
    expected_fkey =
        static_cast<ui::KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

TEST_F(KeyboardCapabilityTest, TopRowLayoutWilco) {
  ui::KeyboardDevice wilco_device(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                                  "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(wilco_device,
                                          kKbdTopRowLayoutWilcoTag,
                                          /*has_custom_top_row=*/false);
  ui::KeyboardDevice drallion_device(kDeviceId2, ui::INPUT_DEVICE_INTERNAL,
                                     "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(drallion_device,
                                          kKbdTopRowLayoutDrallionTag,
                                          /*has_custom_top_row=*/false);

  for (ui::TopRowActionKey action_key = ui::TopRowActionKey::kMinValue;
       action_key <= ui::TopRowActionKey::kMaxValue;
       action_key =
           static_cast<ui::TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(
        ui::kLayoutWilcoDrallionTopRowActionKeys.contains(action_key),
        keyboard_capability_->HasTopRowActionKey(wilco_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
    EXPECT_EQ(
        ui::kLayoutWilcoDrallionTopRowActionKeys.contains(action_key),
        keyboard_capability_->HasTopRowActionKey(drallion_device, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  ui::KeyboardCode expected_fkey = ui::VKEY_F1;
  for (const auto action_key : ui::kLayoutWilcoDrallionTopRowActionKeys) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 wilco_device, action_key));
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 drallion_device, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  wilco_device, expected_fkey));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  drallion_device, expected_fkey));
    expected_fkey =
        static_cast<ui::KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

class TopRowLayoutCustomTest
    : public KeyboardCapabilityTest,
      public testing::WithParamInterface<std::vector<ui::TopRowActionKey>> {
 public:
  void SetUp() override {
    KeyboardCapabilityTest::SetUp();
    top_row_action_keys_ = GetParam();
    custom_layout_string_.clear();

    std::vector<std::string> custom_scan_codes;
    custom_scan_codes.reserve(top_row_action_keys_.size());
    for (const auto& action_key : top_row_action_keys_) {
      const uint32_t scan_code = ConvertTopRowActionKeyToScanCode(action_key);
      custom_scan_codes.push_back(
          base::ToLowerASCII(base::HexEncode(&scan_code, 1)));
    }

    custom_layout_string_ = base::JoinString(custom_scan_codes, " ");
  }

  uint32_t ConvertTopRowActionKeyToScanCode(ui::TopRowActionKey action_key) {
    switch (action_key) {
      case ui::TopRowActionKey::kBack:
        return CustomTopRowScanCode::kBack;
      case ui::TopRowActionKey::kForward:
        return CustomTopRowScanCode::kForward;
      case ui::TopRowActionKey::kRefresh:
        return CustomTopRowScanCode::kRefresh;
      case ui::TopRowActionKey::kFullscreen:
        return CustomTopRowScanCode::kFullscreen;
      case ui::TopRowActionKey::kOverview:
        return CustomTopRowScanCode::kOverview;
      case ui::TopRowActionKey::kScreenshot:
        return CustomTopRowScanCode::kScreenshot;
      case ui::TopRowActionKey::kScreenBrightnessDown:
        return CustomTopRowScanCode::kScreenBrightnessDown;
      case ui::TopRowActionKey::kScreenBrightnessUp:
        return CustomTopRowScanCode::kScreenBrightnessUp;
      case ui::TopRowActionKey::kMicrophoneMute:
        return CustomTopRowScanCode::kMicrophoneMute;
      case ui::TopRowActionKey::kVolumeMute:
        return CustomTopRowScanCode::kVolumeMute;
      case ui::TopRowActionKey::kVolumeDown:
        return CustomTopRowScanCode::kVolumeDown;
      case ui::TopRowActionKey::kVolumeUp:
        return CustomTopRowScanCode::kVolumeUp;
      case ui::TopRowActionKey::kKeyboardBacklightToggle:
        return CustomTopRowScanCode::kKeyboardBacklightToggle;
      case ui::TopRowActionKey::kKeyboardBacklightDown:
        return CustomTopRowScanCode::kKeyboardBacklightDown;
      case ui::TopRowActionKey::kKeyboardBacklightUp:
        return CustomTopRowScanCode::kKeyboardBacklightUp;
      case ui::TopRowActionKey::kNextTrack:
        return CustomTopRowScanCode::kNextTrack;
      case ui::TopRowActionKey::kPreviousTrack:
        return CustomTopRowScanCode::kPreviousTrack;
      case ui::TopRowActionKey::kPlayPause:
        return CustomTopRowScanCode::kPlayPause;
      case ui::TopRowActionKey::kPrivacyScreenToggle:
        return CustomTopRowScanCode::kPrivacyScreenToggle;
      case ui::TopRowActionKey::kAllApplications:
      case ui::TopRowActionKey::kEmojiPicker:
      case ui::TopRowActionKey::kDictation:
      case ui::TopRowActionKey::kUnknown:
      case ui::TopRowActionKey::kNone:
        return 0;
    }
  }

 protected:
  std::vector<ui::TopRowActionKey> top_row_action_keys_;
  std::string custom_layout_string_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    TopRowLayoutCustomTest,
    testing::ValuesIn(std::vector<std::vector<ui::TopRowActionKey>>{
        // Test with full 15 key set.
        {
            ui::TopRowActionKey::kBack,
            ui::TopRowActionKey::kForward,
            ui::TopRowActionKey::kRefresh,
            ui::TopRowActionKey::kFullscreen,
            ui::TopRowActionKey::kOverview,
            ui::TopRowActionKey::kScreenshot,
            ui::TopRowActionKey::kScreenBrightnessDown,
            ui::TopRowActionKey::kScreenBrightnessUp,
            ui::TopRowActionKey::kMicrophoneMute,
            ui::TopRowActionKey::kVolumeMute,
            ui::TopRowActionKey::kVolumeDown,
            ui::TopRowActionKey::kVolumeUp,
            ui::TopRowActionKey::kKeyboardBacklightToggle,
            ui::TopRowActionKey::kKeyboardBacklightDown,
            ui::TopRowActionKey::kKeyboardBacklightUp,
        },
        // Test the remaining untested set of keys.
        {
            ui::TopRowActionKey::kOverview,
            ui::TopRowActionKey::kScreenshot,
            ui::TopRowActionKey::kScreenBrightnessDown,
            ui::TopRowActionKey::kScreenBrightnessUp,
            ui::TopRowActionKey::kMicrophoneMute,
            ui::TopRowActionKey::kVolumeMute,
            ui::TopRowActionKey::kVolumeDown,
            ui::TopRowActionKey::kVolumeUp,
            ui::TopRowActionKey::kKeyboardBacklightToggle,
            ui::TopRowActionKey::kKeyboardBacklightDown,
            ui::TopRowActionKey::kKeyboardBacklightUp,
            ui::TopRowActionKey::kNextTrack,
            ui::TopRowActionKey::kPreviousTrack,
            ui::TopRowActionKey::kPlayPause,
        },
        // Tests with a small subset of the possible keys.
        {
            ui::TopRowActionKey::kBack,
            ui::TopRowActionKey::kForward,
            ui::TopRowActionKey::kRefresh,
            ui::TopRowActionKey::kPrivacyScreenToggle,
        },
        {
            ui::TopRowActionKey::kMicrophoneMute,
            ui::TopRowActionKey::kVolumeMute,
            ui::TopRowActionKey::kVolumeDown,
            ui::TopRowActionKey::kVolumeUp,
            ui::TopRowActionKey::kKeyboardBacklightToggle,
        }}));

TEST_P(TopRowLayoutCustomTest, TopRowLayout) {
  ui::KeyboardDevice keyboard(kDeviceId1, ui::INPUT_DEVICE_INTERNAL,
                              "Internal Keyboard");
  fake_keyboard_manager_->AddFakeKeyboard(keyboard, custom_layout_string_,
                                          /*has_custom_top_row=*/true);
  for (ui::TopRowActionKey action_key = ui::TopRowActionKey::kMinValue;
       action_key <= ui::TopRowActionKey::kMaxValue;
       action_key =
           static_cast<ui::TopRowActionKey>(static_cast<int>(action_key) + 1)) {
    EXPECT_EQ(base::Contains(top_row_action_keys_, action_key),
              keyboard_capability_->HasTopRowActionKey(keyboard, action_key))
        << "Action Key: " << static_cast<int>(action_key);
  }

  ui::KeyboardCode expected_fkey = ui::VKEY_F1;
  for (const auto action_key : top_row_action_keys_) {
    EXPECT_EQ(expected_fkey, keyboard_capability_->GetCorrespondingFunctionKey(
                                 keyboard, action_key));
    EXPECT_EQ(action_key,
              keyboard_capability_->GetCorrespondingActionKeyForFKey(
                  keyboard, expected_fkey));
    expected_fkey =
        static_cast<ui::KeyboardCode>(static_cast<int>(expected_fkey) + 1);
  }
}

}  // namespace ash
