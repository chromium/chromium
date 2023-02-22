// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/events/keyboard_capability.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_service.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kKbdTopRowLayoutWilcoTag[] = "3";
constexpr char kKbdTopRowLayoutDrallionTag[] = "4";

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
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
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
      /*id=*/2, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/"Keyboard2");
  fake_keyboard1.sys_path = base::FilePath("path2");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard2, kKbdTopRowLayout2Tag);

  EXPECT_FALSE(keyboard_capability_->HasLauncherButton(fake_keyboard1));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButton(fake_keyboard2));
  EXPECT_TRUE(keyboard_capability_->HasLauncherButton());
}

}  // namespace ash
