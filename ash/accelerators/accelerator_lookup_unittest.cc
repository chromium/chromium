// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_lookup.h"

#include <memory>
#include <vector>

#include "ash/accelerators/ash_accelerator_configuration.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"
#include "ui/events/keycodes/dom/dom_codes_array.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";

constexpr char kKbdTopRowLayout1Tag[] = "1";

using AcceleratorDetails = AcceleratorLookup::AcceleratorDetails;

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

    ui::DeviceDataManagerTestApi().SetKeyboardDevices({});
    ui::DeviceDataManagerTestApi().SetKeyboardDevices(fake_keyboard_devices_);
    ui::DeviceDataManagerTestApi().OnDeviceListsComplete();

    std::map<std::string, std::string> sysfs_properties;
    std::map<std::string, std::string> sysfs_attributes;
    sysfs_properties[kKbdTopRowPropertyName] = layout;
    fake_udev_.AddFakeDevice(fake_keyboard.name, fake_keyboard.sys_path.value(),
                             /*subsystem=*/"input", /*devnode=*/std::nullopt,
                             /*devtype=*/std::nullopt,
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

bool CompareAccelerators(const std::vector<AcceleratorDetails>& expected,
                         const std::vector<AcceleratorDetails>& actual) {
  if (expected.size() != actual.size()) {
    return false;
  }

  for (size_t i = 0; i < expected.size(); ++i) {
    const bool accelerators_equal =
        expected[i].accelerator == actual[i].accelerator;
    const bool key_display_equal =
        expected[i].key_display == actual[i].key_display;
    if (!accelerators_equal || !key_display_equal) {
      return false;
    }
  }

  return true;
}

}  // namespace

class AcceleratorLookupTest : public AshTestBase {
 public:
  AcceleratorLookupTest() = default;
  ~AcceleratorLookupTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        ::features::kShortcutCustomization);
    AshTestBase::SetUp();
    config_ = Shell::Get()->ash_accelerator_configuration();
    accelerator_lookup_ = Shell::Get()->accelerator_lookup();
  }

  void TearDown() override {
    config_ = nullptr;
    accelerator_lookup_ = nullptr;
    AshTestBase::TearDown();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  raw_ptr<AshAcceleratorConfiguration> config_;
  raw_ptr<AcceleratorLookup> accelerator_lookup_;
};

TEST_F(AcceleratorLookupTest, NoAccelerators) {
  config_->Initialize({});

  std::vector<AcceleratorDetails> accelerators =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kSwitchToLastUsedIme);

  EXPECT_TRUE(accelerators.empty());
}

TEST_F(AcceleratorLookupTest, LoadAndFetchAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_A, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  const std::vector<ui::Accelerator> expected_accelerators = {
      {ui::VKEY_A, ui::EF_CONTROL_DOWN},
  };

  std::vector<AcceleratorDetails> actual =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kSwitchToLastUsedIme);

  std::vector<AcceleratorDetails> expected = {
      {{ui::VKEY_A, ui::EF_CONTROL_DOWN}, std::u16string(u"a")},
  };

  EXPECT_TRUE(CompareAccelerators(expected, actual));
}

TEST_F(AcceleratorLookupTest, ModifiedAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);

  std::vector<AcceleratorDetails> expected = {
      {{ui::VKEY_SPACE, ui::EF_CONTROL_DOWN}, std::u16string(u"space")},
  };

  std::vector<AcceleratorDetails> actual =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kSwitchToLastUsedIme);

  EXPECT_TRUE(CompareAccelerators(expected, actual));

  config_->AddUserAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                              {ui::VKEY_A, ui::EF_COMMAND_DOWN});

  expected = {
      {{ui::VKEY_SPACE, ui::EF_CONTROL_DOWN}, std::u16string(u"space")},
      {{ui::VKEY_A, ui::EF_COMMAND_DOWN}, std::u16string(u"a")},
  };

  actual = accelerator_lookup_->GetAcceleratorsForAction(
      AcceleratorAction::kSwitchToLastUsedIme);
  EXPECT_TRUE(CompareAccelerators(expected, actual));
}

TEST_F(AcceleratorLookupTest, RemovedAccelerator) {
  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_SPACE, ui::EF_CONTROL_DOWN,
       AcceleratorAction::kSwitchToLastUsedIme},
  };

  config_->Initialize(test_data);
  config_->RemoveAccelerator(AcceleratorAction::kSwitchToLastUsedIme,
                             {ui::VKEY_SPACE, ui::EF_CONTROL_DOWN});

  std::vector<AcceleratorDetails> accelerators =
      accelerator_lookup_->GetAcceleratorsForAction(
          AcceleratorAction::kBrightnessDown);

  EXPECT_TRUE(accelerators.empty());
}

TEST_F(AcceleratorLookupTest, FilteredAccelerators) {
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_ =
      std::make_unique<FakeDeviceManager>();
  ui::KeyboardDevice fake_keyboard(
      /*id=*/1, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
      /*name=*/kKbdTopRowLayout1Tag);
  fake_keyboard.sys_path = base::FilePath("path");
  fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, kKbdTopRowLayout1Tag);

  const AcceleratorData test_data[] = {
      {/*trigger_on_press=*/true, ui::VKEY_HELP, ui::EF_NONE,
       AcceleratorAction::kShowShortcutViewer},
      {/*trigger_on_press=*/true, ui::VKEY_S,
       ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN,
       AcceleratorAction::kShowShortcutViewer},
  };

  config_->Initialize(test_data);

  std::vector<AcceleratorDetails> actual =
      accelerator_lookup_->GetAvailableAcceleratorsForAction(
          AcceleratorAction::kShowShortcutViewer);

  std::vector<AcceleratorDetails> expected = {
      {{ui::VKEY_S, ui::EF_COMMAND_DOWN | ui::EF_CONTROL_DOWN},
       std::u16string(u"s")},
  };

  // Expect that the HELP key shortcut is filtered since this is an internal
  // keyboard.
  EXPECT_EQ(1u, actual.size());
  EXPECT_TRUE(CompareAccelerators(expected, actual));
}

class AcceleratorDetailsTextTest
    : public AcceleratorLookupTest,
      public testing::WithParamInterface<
          std::tuple<AcceleratorDetails, std::u16string>> {
 public:
  void SetUp() override {
    AcceleratorLookupTest::SetUp();
    std::tie(details_, expected_) = GetParam();
  }

 protected:
  AcceleratorDetails details_;
  std::u16string expected_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    AcceleratorDetailsTextTest,
    testing::ValuesIn(std::vector<
                      std::tuple<AcceleratorDetails, std::u16string>>{
        {{ui::Accelerator(ui::VKEY_A, ui::EF_NONE), u"a"}, u"a"},
        {{ui::Accelerator(ui::VKEY_BROWSER_REFRESH, ui::EF_NONE),
          u"browserRefresh"},
         u"browserRefresh"},
        {{ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN), u"a"}, u"ctrl+a"},
        {{ui::Accelerator(ui::VKEY_A, ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN),
          u"a"},
         u"ctrl+alt+a"},
        {{ui::Accelerator(ui::VKEY_A,
                          ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN |
                              ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN),
          u"a"},
         u"search+ctrl+alt+shift+a"},
        {{ui::Accelerator(ui::VKEY_A, ui::EF_COMMAND_DOWN), u"a"}, u"search+a"},
    }));

TEST_P(AcceleratorDetailsTextTest, ExpectedText) {
  const std::u16string actual =
      AcceleratorLookup::GetAcceleratorDetailsText(details_);
  EXPECT_EQ(expected_, actual);
}

}  // namespace ash
