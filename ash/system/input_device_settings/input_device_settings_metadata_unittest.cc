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

TEST_F(MetadataTest, MouseMetadata) {
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

  const auto* metadata1 = GetMouseMetadata(kSampleMouse1);
  EXPECT_EQ(metadata1, nullptr);

  const auto* metadata2 = GetMouseMetadata(kSampleMouse2);
  MouseMetadata expected_metadata;
  expected_metadata.customization_restriction =
      mojom::CustomizationRestriction::kDisallowCustomizations;
  ASSERT_TRUE(metadata2);
  EXPECT_EQ(*metadata2, expected_metadata);
}

TEST_F(MetadataTest, GetDeviceType) {
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

}  // namespace ash
