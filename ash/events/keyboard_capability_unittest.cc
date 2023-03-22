// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "components/prefs/pref_service.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/chromeos/events/mojom/modifier_key.mojom-shared.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kKbdTopRowLayoutWilcoTag[] = "3";
constexpr char kKbdTopRowLayoutDrallionTag[] = "4";

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

// NOTE: This only creates a simple ui::InputDevice based on a device
// capabilities report; it is not suitable for subclasses of ui::InputDevice.
ui::InputDevice InputDeviceFromCapabilities(
    int device_id,
    const ui::DeviceCapabilities& capabilities) {
  ui::EventDeviceInfo device_info = {};
  ui::CapabilitiesToDeviceInfo(capabilities, &device_info);
  return ui::InputDevice(
      device_id, device_info.device_type(), device_info.name(),
      device_info.phys(), base::FilePath(capabilities.path),
      device_info.vendor_id(), device_info.product_id(), device_info.version());
}

class FakeDeviceManager {
 public:
  FakeDeviceManager() = default;
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() = default;

  // Add a fake keyboard to DeviceDataManagerTestApi and provide layout info to
  // fake udev.
  void AddFakeKeyboard(const ui::InputDevice& fake_keyboard,
                       const std::string& layout) {
    fake_keyboard_devices_.push_back(fake_keyboard);

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = layout;
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
  std::vector<ui::InputDevice> fake_keyboard_devices_;
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

class KeyboardCapabilityTest : public AshTestBase {
 public:
  KeyboardCapabilityTest() = default;
  ~KeyboardCapabilityTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    keyboard_capability_ = Shell::Get()->keyboard_capability();
    test_observer_ = std::make_unique<TestObserver>();
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
    keyboard_capability_->AddObserver(test_observer_.get());
  }

  void TearDown() override {
    keyboard_capability_->RemoveObserver(test_observer_.get());
    AshTestBase::TearDown();
  }

  ui::InputDevice AddFakeKeyboardInfoToKeyboardCapability(
      int device_id,
      ui::DeviceCapabilities capabilities,
      ui::KeyboardCapability::DeviceType device_type) {
    ui::KeyboardCapability::KeyboardInfo keyboard_info;
    keyboard_info.device_type = device_type;
    keyboard_info.event_device_info = std::make_unique<ui::EventDeviceInfo>();

    ui::InputDevice fake_keyboard =
        InputDeviceFromCapabilities(device_id, capabilities);
    ui::CapabilitiesToDeviceInfo(capabilities,
                                 keyboard_info.event_device_info.get());

    keyboard_capability_->SetKeyboardInfoForTesting(fake_keyboard,
                                                    std::move(keyboard_info));

    return fake_keyboard;
  }

 protected:
  ui::KeyboardCapability* keyboard_capability_;
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
  ui::InputDevice fake_keyboard(
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
  ui::InputDevice fake_keyboard1(
      /*id=*/kDeviceId1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);

  // Provide specific keyboard. Launcher button depends on if the keyboard is
  // layout2 type.
  EXPECT_FALSE(keyboard_capability_->HasLauncherButton(fake_keyboard1));
  // Do not provide specific keyboard. Launcher button depends on if any one
  // of the keyboards is layout2 type.
  EXPECT_FALSE(keyboard_capability_->HasLauncherButton());

  // Add a layout2 keyboard.
  ui::InputDevice fake_keyboard2(
      /*id=*/kDeviceId2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout2Tag);

  EXPECT_FALSE(keyboard_capability_->HasLauncherButton(fake_keyboard1));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButton(fake_keyboard2));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButton());
}

TEST_F(KeyboardCapabilityTest, TestHasSixPackKey) {
  // Add an internal keyboard.
  ui::InputDevice fake_keyboard1(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard1");
  fake_keyboard1.sys_path = base::FilePath("path1");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard1, kKbdTopRowLayout1Tag);

  // Internal keyboard doesn't have six pack key.
  EXPECT_FALSE(keyboard_capability_->HasSixPackKey(fake_keyboard1));
  EXPECT_FALSE(keyboard_capability_->HasSixPackOnAnyKeyboard());

  // Add an external keyboard.
  ui::InputDevice fake_keyboard2(
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_BLUETOOTH,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout1Tag);

  // External keyboard has six pack key.
  EXPECT_TRUE(keyboard_capability_->HasSixPackKey(fake_keyboard2));
  EXPECT_TRUE(keyboard_capability_->HasSixPackOnAnyKeyboard());
}

TEST_F(KeyboardCapabilityTest, TestRemoveDevicesFromList) {
  const ui::InputDevice input_device1 = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId1, ui::kEveKeyboard,
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard);
  const ui::InputDevice input_device2 = AddFakeKeyboardInfoToKeyboardCapability(
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

  const ui::InputDevice test_keyboard = AddFakeKeyboardInfoToKeyboardCapability(
      kDeviceId1, capabilities, device_type);
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
    ui::InputDevice fake_keyboard(
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

}  // namespace ash
