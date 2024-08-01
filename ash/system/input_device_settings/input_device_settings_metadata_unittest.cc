// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_metadata.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/test/ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"

namespace ash {

class MetadataTest : public AshTestBase {};

bool ValidateDeviceLists() {
  for (auto keyboard_metadata : GetKeyboardMetadataList()) {
    if (GetKeyboardMouseComboMetadataList().contains(keyboard_metadata.first)) {
      return false;
    }
  }
  for (auto keyboard_mouse_combo_metadata :
       GetKeyboardMouseComboMetadataList()) {
    if (GetMouseMetadataList().contains(keyboard_mouse_combo_metadata.first)) {
      return false;
    }
  }
  for (const auto& mouse_metadata : GetMouseMetadataList()) {
    if (GetKeyboardMetadataList().contains(mouse_metadata.first)) {
      return false;
    }
  }
  return true;
}

bool ValidateVidPidAliasList() {
  for (auto vid_pid_alias_pair : GetVidPidAliasList()) {
    if (!GetMouseMetadataList().contains(vid_pid_alias_pair.second) &&
        !GetGraphicsTabletMetadataList().contains(vid_pid_alias_pair.second) &&
        !GetKeyboardMouseComboMetadataList().contains(
            vid_pid_alias_pair.second) &&
        !GetKeyboardMetadataList().contains(vid_pid_alias_pair.second)) {
      return false;
    }
  }
  return true;
}

TEST_F(MetadataTest, GetMetadata) {
  ASSERT_TRUE(ValidateDeviceLists());
  const ui::InputDevice kSampleMouse1(0, ui::INPUT_DEVICE_USB, "kSampleMouse1",
                                      /*phys=*/"",
                                      /*sys_path=*/base::FilePath(),
                                      /*vendor=*/0x0000,
                                      /*product=*/0x0000,
                                      /*version=*/0x0001);
  const ui::InputDevice kSampleMouse2(1, ui::INPUT_DEVICE_USB, "kSampleMouse2",
                                      /*phys=*/"",
                                      /*sys_path=*/base::FilePath(),
                                      /*vendor=*/0xffff,
                                      /*product=*/0xffff,
                                      /*version=*/0x0002);
  const ui::InputDevice kSampleGraphicsTablet1(1, ui::INPUT_DEVICE_USB,
                                               "kSampleGraphicsTablet1",
                                               /*phys=*/"",
                                               /*sys_path=*/base::FilePath(),
                                               /*vendor=*/0x000e,
                                               /*product=*/0x000e,
                                               /*version=*/0x0001);
  const ui::InputDevice kSampleGraphicsTablet2(1, ui::INPUT_DEVICE_USB,
                                               "kSampleGraphicsTablet2",
                                               /*phys=*/"",
                                               /*sys_path=*/base::FilePath(),
                                               /*vendor=*/0xeeee,
                                               /*product=*/0xeeee,
                                               /*version=*/0x0002);

  const auto* mouse_metadata1 = GetMouseMetadata(kSampleMouse1);
  EXPECT_EQ(mouse_metadata1, nullptr);

  const auto* mouse_metadata2 = GetMouseMetadata(kSampleMouse2);
  MouseMetadata expected_mouse_metadata;
  expected_mouse_metadata.customization_restriction =
      mojom::CustomizationRestriction::kDisallowCustomizations;
  expected_mouse_metadata.mouse_button_config =
      mojom::MouseButtonConfig::kNoConfig;
  ASSERT_TRUE(mouse_metadata2);
  EXPECT_EQ(*mouse_metadata2, expected_mouse_metadata);

  const auto* graphics_tablet_metadata1 =
      GetGraphicsTabletMetadata(kSampleGraphicsTablet1);
  EXPECT_EQ(graphics_tablet_metadata1, nullptr);

  const auto* graphics_tablet_metadata2 =
      GetGraphicsTabletMetadata(kSampleGraphicsTablet2);
  GraphicsTabletMetadata graphics_tablet_expected_metadata;
  graphics_tablet_expected_metadata.customization_restriction =
      mojom::CustomizationRestriction::kAllowCustomizations;
  ASSERT_TRUE(graphics_tablet_metadata2);
  EXPECT_EQ(*graphics_tablet_metadata2, graphics_tablet_expected_metadata);
}

TEST_F(MetadataTest, GetDeviceType) {
  ASSERT_TRUE(ValidateDeviceLists());
  const ui::InputDevice kSampleMouse(0, ui::INPUT_DEVICE_USB,
                                     "Razer Naga Pro (USB Dongle)",
                                     /*phys=*/"",
                                     /*sys_path=*/base::FilePath(),
                                     /*vendor=*/0x1532,
                                     /*product=*/0x0090,
                                     /*version=*/0x0001);
  const ui::InputDevice kSampleKeyboard(0, ui::INPUT_DEVICE_USB,
                                        "HP OMEN Sequencer",
                                        /*phys=*/"",
                                        /*sys_path=*/base::FilePath(),
                                        /*vendor=*/0x03f0,
                                        /*product=*/0x1f41,
                                        /*version=*/0x0001);
  const ui::InputDevice kSampleKeyboardMouseCombo(0, ui::INPUT_DEVICE_USB,
                                                  "Logitech K400",
                                                  /*phys=*/"",
                                                  /*sys_path=*/base::FilePath(),
                                                  /*vendor=*/0x046d,
                                                  /*product=*/0x4024,
                                                  /*version=*/0x0001);
  const ui::InputDevice kSampleUnknown(0, ui::INPUT_DEVICE_USB,
                                       "kSampleUnknown",
                                       /*phys=*/"",
                                       /*sys_path=*/base::FilePath(),
                                       /*vendor=*/0x0000,
                                       /*product=*/0x0000,
                                       /*version=*/0x0001);
  ASSERT_EQ(GetDeviceType(kSampleMouse), DeviceType::kMouse);
  ASSERT_EQ(GetDeviceType(kSampleKeyboardMouseCombo),
            DeviceType::kKeyboardMouseCombo);
  ASSERT_EQ(GetDeviceType(kSampleKeyboard), DeviceType::kKeyboard);
  ASSERT_EQ(GetDeviceType(kSampleUnknown), DeviceType::kUnknown);
}

TEST_F(MetadataTest, GetButtonRemappingListForConfig) {
  const ui::InputDevice kDefaultMouse(0, ui::INPUT_DEVICE_USB, "kDefaultMouse",
                                      /*phys=*/"",
                                      /*sys_path=*/base::FilePath(),
                                      /*vendor=*/0xffff,
                                      /*product=*/0xffff,
                                      /*version=*/0x0001);
  const ui::InputDevice kFiveKeyMouse(1, ui::INPUT_DEVICE_USB, "kFiveKeyMouse",
                                      /*phys=*/"",
                                      /*sys_path=*/base::FilePath(),
                                      /*vendor=*/0x3f0,
                                      /*product=*/0x804a,
                                      /*version=*/0x0002);
  const ui::InputDevice kLogitechSixKeyMouse(2, ui::INPUT_DEVICE_USB,
                                             "kLogitechSixKeyMouse",
                                             /*phys=*/"",
                                             /*sys_path=*/base::FilePath(),
                                             /*vendor=*/0xffff,
                                             /*product=*/0xfffe,
                                             /*version=*/0x0003);
  EXPECT_EQ(0u, GetButtonRemappingListForConfig(
                    GetMouseMetadata(kDefaultMouse)->mouse_button_config)
                    .size());
  EXPECT_EQ(3u, GetButtonRemappingListForConfig(
                    GetMouseMetadata(kFiveKeyMouse)->mouse_button_config)
                    .size());
  EXPECT_EQ(4u, GetButtonRemappingListForConfig(
                    GetMouseMetadata(kLogitechSixKeyMouse)->mouse_button_config)
                    .size());
}

TEST_F(MetadataTest, GetVidPidAliasList) {
  ASSERT_TRUE(ValidateDeviceLists());
  ASSERT_TRUE(ValidateVidPidAliasList());
  const ui::InputDevice kSampleBluetoothMouse(0, ui::INPUT_DEVICE_BLUETOOTH,
                                              "Razer Naga Pro (Bluetooth)",
                                              /*phys=*/"",
                                              /*sys_path=*/base::FilePath(),
                                              /*vendor=*/0x1532,
                                              /*product=*/0x0092,
                                              /*version=*/0x0001);
  const ui::InputDevice kSampleUSBMouse(1, ui::INPUT_DEVICE_USB,
                                        "Razer Naga Pro (USB Doggle)",
                                        /*phys=*/"",
                                        /*sys_path=*/base::FilePath(),
                                        /*vendor=*/0x1532,
                                        /*product=*/0x0090,
                                        /*version=*/0x0001);

  ASSERT_EQ(GetMouseMetadata(kSampleUSBMouse),
            GetMouseMetadata(kSampleBluetoothMouse));
}

}  // namespace ash
