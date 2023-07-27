// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include <cstdint>

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/events/devices/input_device.h"

namespace ash {
namespace {

constexpr char kTestPrefKey[] = "test_key";
const AccountId account_id = AccountId::FromUserEmail("example@email.com");

const mojom::ButtonRemapping button_remapping1(
    /*name=*/"test1",
    /*button=*/
    mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kBack),
    /*remapping_action=*/
    mojom::RemappingAction::NewAction(ash::AcceleratorAction::kBrightnessDown));
const mojom::ButtonRemapping button_remapping2(
    /*name=*/"test2",
    /*button=*/
    mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kLeft),
    /*remapping_action=*/
    mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(::ui::KeyboardCode::VKEY_0, 1, 2, 3)));
const mojom::ButtonRemapping button_remapping3(
    /*name=*/"test3",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_1),
    /*remapping_action=*/
    mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(::ui::KeyboardCode::VKEY_2, 4, 5, 6)));
const mojom::ButtonRemapping button_remapping4(
    /*name=*/"test4",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_3),
    /*remapping_action=*/nullptr);
}  // namespace

class DeviceKeyTest : public testing::TestWithParam<
                          std::tuple<uint16_t, uint16_t, std::string>> {};

INSTANTIATE_TEST_SUITE_P(
    ,
    DeviceKeyTest,
    testing::ValuesIn(std::vector<std::tuple<uint16_t, uint16_t, std::string>>{
        {0x1234, 0x4321, "1234:4321"},
        {0xaaaa, 0xbbbb, "aaaa:bbbb"},
        {0xaa54, 0xffa1, "aa54:ffa1"},
        {0x1a2b, 0x3c4d, "1a2b:3c4d"},
        {0x5e6f, 0x7890, "5e6f:7890"},
        {0x0001, 0x0001, "0001:0001"},
        {0x1000, 0x1000, "1000:1000"}}));

TEST_P(DeviceKeyTest, BuildDeviceKey) {
  std::string expected_key;
  ui::InputDevice device;
  std::tie(device.vendor_id, device.product_id, expected_key) = GetParam();

  auto key = BuildDeviceKey(device);
  EXPECT_EQ(expected_key, key);
}

TEST(GetLoginScreenSettingsDictTest, RetrieveSettingsDict) {
  auto local_state = std::make_unique<TestingPrefServiceSimple>();
  user_manager::KnownUser::RegisterPrefs(local_state->registry());
  user_manager::KnownUser known_user(local_state.get());
  const base::Value::Dict* settings =
      GetLoginScreenSettingsDict(local_state.get(), account_id, kTestPrefKey);
  EXPECT_EQ(nullptr, settings);
  known_user.SetPath(account_id, kTestPrefKey,
                     absl::make_optional<base::Value>(base::Value::Dict()));
  const base::Value::Dict* valid_settings =
      GetLoginScreenSettingsDict(local_state.get(), account_id, kTestPrefKey);
  EXPECT_NE(nullptr, valid_settings);
}

TEST(ConvertButtonRemappingToDict, ConvertButtonRemappingToDict) {
  const base::Value::Dict dict1 =
      ConvertButtonRemappingToDict(button_remapping1);
  EXPECT_EQ(button_remapping1.name,
            *dict1.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(
      static_cast<int>(button_remapping1.button->get_customizable_button()),
      *dict1.FindInt(prefs::kButtonRemappingCustomizableButton));
  EXPECT_EQ(static_cast<int>(button_remapping1.remapping_action->get_action()),
            *dict1.FindInt(prefs::kButtonRemappingAction));

  const base::Value::Dict dict2 =
      ConvertButtonRemappingToDict(button_remapping2);
  EXPECT_EQ(button_remapping2.name,
            *dict2.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(
      static_cast<int>(button_remapping2.button->get_customizable_button()),
      *dict2.FindInt(prefs::kButtonRemappingCustomizableButton));
  EXPECT_NE(nullptr, dict2.FindDict(prefs::kButtonRemappingKeyEvent));
  EXPECT_EQ(static_cast<int>(
                button_remapping2.remapping_action->get_key_event()->dom_code),
            *dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingDomCode));
  EXPECT_EQ(static_cast<int>(
                button_remapping2.remapping_action->get_key_event()->dom_key),
            *dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingDomKey));
  EXPECT_EQ(static_cast<int>(
                button_remapping2.remapping_action->get_key_event()->modifiers),
            *dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingModifiers));
  EXPECT_EQ(static_cast<int>(
                button_remapping2.remapping_action->get_key_event()->vkey),
            *dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingKeyboardCode));

  const base::Value::Dict dict3 =
      ConvertButtonRemappingToDict(button_remapping3);
  EXPECT_EQ(button_remapping3.name,
            *dict3.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(static_cast<int>(button_remapping3.button->get_vkey()),
            *dict3.FindInt(prefs::kButtonRemappingKeyboardCode));
  EXPECT_NE(nullptr, dict3.FindDict(prefs::kButtonRemappingKeyEvent));
  EXPECT_EQ(static_cast<int>(
                button_remapping3.remapping_action->get_key_event()->dom_code),
            *dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingDomCode));
  EXPECT_EQ(static_cast<int>(
                button_remapping3.remapping_action->get_key_event()->dom_key),
            *dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingDomKey));
  EXPECT_EQ(static_cast<int>(
                button_remapping3.remapping_action->get_key_event()->modifiers),
            *dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingModifiers));
  EXPECT_EQ(static_cast<int>(
                button_remapping3.remapping_action->get_key_event()->vkey),
            *dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                 ->FindInt(prefs::kButtonRemappingKeyboardCode));

  const base::Value::Dict dict4 =
      ConvertButtonRemappingToDict(button_remapping4);
  EXPECT_EQ(button_remapping4.name,
            *dict4.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(static_cast<int>(button_remapping4.button->get_vkey()),
            *dict4.FindInt(prefs::kButtonRemappingKeyboardCode));
  EXPECT_EQ(nullptr, dict4.FindDict(prefs::kButtonRemappingKeyEvent));
  EXPECT_EQ(absl::nullopt, dict4.FindInt(prefs::kButtonRemappingAction));
}

}  // namespace ash
