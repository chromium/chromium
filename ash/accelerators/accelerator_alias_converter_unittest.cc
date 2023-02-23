// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_alias_converter.h"

#include <vector>

#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/chromeos/events/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"

namespace ash {

namespace {

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayout1Tag[] = "1";
constexpr char kKbdTopRowLayout2Tag[] = "2";
constexpr char kKbdTopRowLayoutWilcoTag[] = "3";
constexpr char kKbdTopRowLayoutDrallionTag[] = "4";

struct AcceleratorAliasConverterTestData {
  ui::Accelerator accelerator_;
  absl::optional<ui::Accelerator> expected_accelerator_;
};

struct TopRowAcceleratorAliasConverterTestData {
  // All currently connected keyboards' layout types.
  std::vector<std::string> keyboard_layout_types_;
  ui::Accelerator accelerator_;
  std::vector<ui::Accelerator> expected_accelerator_;
};

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

}  // namespace

using AcceleratorAliasConverterTest = AshTestBase;

TEST_F(AcceleratorAliasConverterTest, CheckTopRowAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  // Top row keys not fKeys prevents remapping.
  Shell::Get()->keyboard_capability()->SetTopRowKeysAsFKeysEnabledForTesting(
      false);
  EXPECT_FALSE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  const ui::Accelerator accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN};
  std::vector<ui::Accelerator> accelerator_aliases =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator);

  EXPECT_EQ(1u, accelerator_aliases.size());
  EXPECT_EQ(accelerator, accelerator_aliases[0]);
}

class TopRowAliasTest : public AcceleratorAliasConverterTest,
                        public testing::WithParamInterface<
                            TopRowAcceleratorAliasConverterTestData> {
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    // Enable top row keys as fKeys.
    Shell::Get()->keyboard_capability()->SetTopRowKeysAsFKeysEnabledForTesting(
        true);
    TopRowAcceleratorAliasConverterTestData test_data = GetParam();
    keyboard_layout_types_ = test_data.keyboard_layout_types_;
    accelerator_ = test_data.accelerator_;
    expected_accelerator_ = test_data.expected_accelerator_;
    fake_keyboard_manager_ = std::make_unique<FakeDeviceManager>();
  }

 protected:
  std::vector<std::string> keyboard_layout_types_;
  ui::Accelerator accelerator_;
  std::vector<ui::Accelerator> expected_accelerator_;
  std::unique_ptr<FakeDeviceManager> fake_keyboard_manager_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    TopRowAliasTest,
    testing::ValuesIn(std::vector<TopRowAcceleratorAliasConverterTestData>{
        // [Search] as original modifier prevents remapping.
        {{kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN},
         {}},

        // key_code not as a top row key prevents remapping.
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN},
         {}},

        // Below are testing each layout type.

        // For TopRowLayout1: [Alt] + [Back] -> [Alt] + [Search] + [F1].
        // TopRowKeysAreFKeys() remains true. This statement applies to all
        // tests in TopRowAliasTest class.
        {{kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F1, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1: [Alt] + [Forward] -> [Alt] + [Search] + [F2].
        {{kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F2, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1: [Alt] + [Zoom] -> [Alt] + [Search] + [F4].
        {{kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F4, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout2: [Alt] + [Shift] + [Back] -> [Alt] + [Shift] +
        // [Search] + [F1].
        {{kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_BROWSER_BACK,
                         ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN},
         {ui::Accelerator{ui::VKEY_F1, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN |
                                           ui::EF_SHIFT_DOWN}}},

        // For TopRowLayout2: [Alt] + [Zoom] -> [Alt] + [Search] + [F3].
        {{kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout2: [Alt] + [Pause] -> [Alt] + [Search] + [F7].
        {{kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_MEDIA_PLAY_PAUSE, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F7, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayoutWilco: [Alt] + [Zoom] -> [Alt] + [Search] + [F3].
        {{kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayoutWilco: [Alt] + [VolumeUp] -> [Alt] + [Search] + [F9].
        {{kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_VOLUME_UP, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F9, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For kKbdTopRowLayoutDrallionTag: [Alt] + [Mute] -> [Alt] + [Search] +
        // [F7].
        {{kKbdTopRowLayoutDrallionTag},
         ui::Accelerator{ui::VKEY_VOLUME_MUTE, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F7, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // Below are testing multiple connected keyboards.

        // Two keyboards with the same layout type: [Alt] + [Forward] -> [Alt] +
        // [Search] + [F2]. No duplicated alias exists.
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout1Tag},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F2, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1 + TopRowLayout2: [Alt] + [Forward] -> [Alt] +
        // [Search] + [F2]. Only layout1 has [Forward] key.
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_BROWSER_FORWARD, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F2, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1 + TopRowLayout2: [Alt] + [Refresh] -> [Alt] +
        // [Search] + [F2] AND [Alt] + [Search] + [F3]. Layout1's [Refresh] key
        // maps to [F3], while layout2 maps to [F2].
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_BROWSER_REFRESH, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F2, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
          ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1 + TopRowLayout2: [Alt] + [VolumeUp] -> [Alt] +
        // [Search] + [F10]. Both layout1 and layout2' [VolumeUp] key maps to
        // [F10]. No duplicated alias exists.
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag},
         ui::Accelerator{ui::VKEY_VOLUME_UP, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F10,
                          ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1 + TopRowLayout2 + TopRowLayoutWilco: [Alt] + [Back]
        // -> [Alt] + [Search] + [F1]. All layouts' [Back] key maps to [F1]. No
        // duplicated alias exists.
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag, kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_BROWSER_BACK, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F1, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1 + TopRowLayout2 + TopRowLayoutWilco: [Alt] + [Zoom]
        // -> [Alt] + [Search] + [F3] AND [Alt] + [Search] + [F4].
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag, kKbdTopRowLayoutWilcoTag},
         ui::Accelerator{ui::VKEY_ZOOM, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F3, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
          ui::Accelerator{ui::VKEY_F4, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},

        // For TopRowLayout1 + TopRowLayout2 + TopRowLayoutWilco +
        // TopRowLayoutDrallion: [Alt] + [Launch] -> [Alt] + [Search] + [F4] AND
        // [Alt] + [Search] + [F5].
        {{kKbdTopRowLayout1Tag, kKbdTopRowLayout2Tag, kKbdTopRowLayoutWilcoTag,
          kKbdTopRowLayoutDrallionTag},
         ui::Accelerator{ui::VKEY_MEDIA_LAUNCH_APP1, ui::EF_ALT_DOWN},
         {ui::Accelerator{ui::VKEY_F4, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
          ui::Accelerator{ui::VKEY_F5, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}}},
    }));

TEST_P(TopRowAliasTest, CheckTopRowAlias) {
  // Add fake keyboards based on layout type.
  fake_keyboard_manager_->RemoveAllDevices();
  for (int i = 0; const std::string& layout : keyboard_layout_types_) {
    ui::InputDevice fake_keyboard(
        /*id=*/i++, /*type=*/ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
        /*name=*/layout);
    fake_keyboard.sys_path = base::FilePath("path" + layout);
    fake_keyboard_manager_->AddFakeKeyboard(fake_keyboard, layout);
  }

  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  EXPECT_TRUE(Shell::Get()->keyboard_capability()->TopRowKeysAreFKeys());
  if (expected_accelerator_.size() > 0) {
    EXPECT_EQ(expected_accelerator_.size(), accelerator_alias.size());
    for (size_t i = 0; i < expected_accelerator_.size(); i++) {
      EXPECT_EQ(expected_accelerator_[i], accelerator_alias[i]);
    }
  } else {
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

class SixPackAliasTest
    : public AcceleratorAliasConverterTest,
      public testing::WithParamInterface<AcceleratorAliasConverterTestData> {
  void SetUp() override {
    AcceleratorAliasConverterTest::SetUp();
    AcceleratorAliasConverterTestData test_data = GetParam();
    accelerator_ = test_data.accelerator_;
    expected_accelerator_ = test_data.expected_accelerator_;
  }

 protected:
  ui::Accelerator accelerator_;
  absl::optional<ui::Accelerator> expected_accelerator_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    SixPackAliasTest,
    testing::ValuesIn(std::vector<AcceleratorAliasConverterTestData>{
        // [Search] as original modifier prevents remapping.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}, absl::nullopt},
        // key_code not as six pack key prevents remapping.
        {ui::Accelerator{ui::VKEY_TAB, ui::EF_ALT_DOWN}, absl::nullopt},
        // [Shift] + [Delete] should not be remapped.
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_SHIFT_DOWN}, absl::nullopt},
        // [Shift] + [Insert] should not be remapped.
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_SHIFT_DOWN}, absl::nullopt},
        // For Insert: [modifiers] -> [Search] + [Shift] + [original_modifiers].
        {ui::Accelerator{ui::VKEY_INSERT, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN |
                                            ui::EF_SHIFT_DOWN |
                                            ui::EF_ALT_DOWN}},
        // For other six-pack-keys: [modifiers] -> [Search] +
        // [original_modifiers].
        {ui::Accelerator{ui::VKEY_DELETE, ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN}},

        // Below are tests for reversed six pack alias.
        // [Search] not in modifiers prevents remapping.
        {ui::Accelerator{ui::VKEY_LEFT, ui::EF_ALT_DOWN}, absl::nullopt},
        // key_code not as reversed six pack key prevent remapping.
        {ui::Accelerator{ui::VKEY_ZOOM, ui::EF_COMMAND_DOWN}, absl::nullopt},
        // [Back] + [Search] -> [Delete]
        {ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN},
         ui::Accelerator{ui::VKEY_DELETE, ui::EF_NONE}},
        // [Back] + [Shift] + [Search] -> [Insert].
        {ui::Accelerator{ui::VKEY_BACK,
                         ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN},
         ui::Accelerator{ui::VKEY_INSERT, ui::EF_NONE}},
        // [Back] + [Shift] + [Search] + [Alt] -> [Insert] + [Alt].
        {ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN |
                                            ui::EF_SHIFT_DOWN |
                                            ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_INSERT, ui::EF_ALT_DOWN}},
        // [Back] + [Search] + [Alt] -> [Delete] + [Alt].
        {ui::Accelerator{ui::VKEY_BACK, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_DELETE, ui::EF_ALT_DOWN}},
        // [Left] + [Search] + [Alt] -> [Home] + [Alt].
        {ui::Accelerator{ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN},
         ui::Accelerator{ui::VKEY_HOME, ui::EF_ALT_DOWN}},
        // [Left] + [Search] + [Shift] + [Alt] -> [Home] + [Shift] + [Alt].
        {ui::Accelerator{ui::VKEY_LEFT, ui::EF_COMMAND_DOWN | ui::EF_ALT_DOWN |
                                            ui::EF_SHIFT_DOWN},
         ui::Accelerator{ui::VKEY_HOME,
                         ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN}}}));

TEST_P(SixPackAliasTest, CheckSixPackAlias) {
  AcceleratorAliasConverter accelerator_alias_converter_;

  std::vector<ui::Accelerator> accelerator_alias =
      accelerator_alias_converter_.CreateAcceleratorAlias(accelerator_);

  if (expected_accelerator_.has_value()) {
    // Accelerator has valid a remapping.
    EXPECT_EQ(2u, accelerator_alias.size());
    EXPECT_EQ(expected_accelerator_, accelerator_alias[0]);
    EXPECT_EQ(accelerator_, accelerator_alias[1]);
  } else {
    // Accelerator doesn't have a valid remapping.
    EXPECT_EQ(1u, accelerator_alias.size());
    EXPECT_EQ(accelerator_, accelerator_alias[0]);
  }
}

}  // namespace ash
