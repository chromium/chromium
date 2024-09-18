// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/picker/picker_shortcuts.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/picker/picker_search_result.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ash/keyboard_capability.h"
#include "ui/events/devices/device_data_manager_test_api.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/devices/keyboard_device.h"

namespace ash {
namespace {

using PickerShortcutsTest = AshTestBase;

TEST_F(PickerShortcutsTest, GetsCapsLockShortcutWithSearchKey) {
  ui::KeyboardDevice keyboard(/*id=*/1, ui::INPUT_DEVICE_INTERNAL,
                              /*name=*/"Keyboard1");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  keyboard_info.top_row_layout =
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
  Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
      keyboard, std::move(keyboard_info));

  EXPECT_EQ(GetPickerShortcutForCapsLock(),
            PickerCapsLockResult::Shortcut::kAltSearch);
}

TEST_F(PickerShortcutsTest, GetsCapsLockShortcutWithLauncherKey) {
  ui::KeyboardDevice keyboard(/*id=*/1, ui::INPUT_DEVICE_INTERNAL,
                              /*name=*/"Keyboard1");
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  keyboard_info.top_row_layout =
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2;
  Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
      keyboard, std::move(keyboard_info));

  EXPECT_EQ(GetPickerShortcutForCapsLock(),
            PickerCapsLockResult::Shortcut::kAltLauncher);
}

TEST_F(PickerShortcutsTest, GetsCapsLockShortcutWithFnKey) {
  base::test::ScopedFeatureList scoped_feature_list(features::kModifierSplit);
  base::AutoReset<bool> ignore_modifier_split_key =
      switches::SetIgnoreModifierSplitSecretKeyForTest();
  Shell::Get()
      ->keyboard_capability()
      ->ResetModifierSplitDogfoodControllerForTesting();
  ui::KeyboardDevice keyboard(/*id=*/1, ui::INPUT_DEVICE_INTERNAL,
                              /*name=*/"Keyboard1", /*has_assistant_key=*/true,
                              /*has_function_key=*/true);
  ui::DeviceDataManagerTestApi().SetKeyboardDevices({keyboard});
  ui::KeyboardCapability::KeyboardInfo keyboard_info;
  keyboard_info.device_type =
      ui::KeyboardCapability::DeviceType::kDeviceInternalKeyboard;
  keyboard_info.top_row_layout =
      ui::KeyboardCapability::KeyboardTopRowLayout::kKbdTopRowLayout2;
  Shell::Get()->keyboard_capability()->SetKeyboardInfoForTesting(
      keyboard, std::move(keyboard_info));

  EXPECT_EQ(GetPickerShortcutForCapsLock(),
            PickerCapsLockResult::Shortcut::kFnRightAlt);
}

}  // namespace
}  // namespace ash
