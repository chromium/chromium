// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_utils.h"

#include <cstdint>
#include <optional>

#include "ash/constants/ash_features.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"

namespace ash {
namespace {

constexpr char kTestPrefKey[] = "test_key";
const AccountId account_id = AccountId::FromUserEmail("example@email.com");

const mojom::ButtonRemapping button_remapping1(
    /*name=*/"test1",
    /*button=*/
    mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kBack),
    /*remapping_action=*/
    mojom::RemappingAction::NewAcceleratorAction(
        ash::AcceleratorAction::kBrightnessDown));
const mojom::ButtonRemapping button_remapping2(
    /*name=*/"test2",
    /*button=*/
    mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kLeft),
    /*remapping_action=*/
    mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(::ui::KeyboardCode::VKEY_0,
                             1,
                             2,
                             3,
                             /*key_display=*/"0")));
const mojom::ButtonRemapping button_remapping3(
    /*name=*/"test3",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_1),
    /*remapping_action=*/
    mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(::ui::KeyboardCode::VKEY_2,
                             4,
                             5,
                             6,
                             /*key_display=*/"2")));
const mojom::ButtonRemapping button_remapping4(
    /*name=*/"test4",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_3),
    /*remapping_action=*/nullptr);
const mojom::ButtonRemapping button_remapping5(
    /*name=*/"test5",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_3),
    /*remapping_action=*/
    mojom::RemappingAction::NewStaticShortcutAction(
        mojom::StaticShortcutAction::kCopy));
const mojom::ButtonRemapping button_remapping6(
    /*name=*/"test6",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_A),
    /*remapping_action=*/
    mojom::RemappingAction::NewStaticShortcutAction(
        mojom::StaticShortcutAction::kCopy));
const mojom::ButtonRemapping button_remapping7(
    /*name=*/"test7",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_LEFT),
    /*remapping_action=*/
    mojom::RemappingAction::NewStaticShortcutAction(
        mojom::StaticShortcutAction::kCopy));
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
                     std::make_optional<base::Value>(base::Value::Dict()));
  const base::Value::Dict* valid_settings =
      GetLoginScreenSettingsDict(local_state.get(), account_id, kTestPrefKey);
  EXPECT_NE(nullptr, valid_settings);
}

TEST(GetLoginScreenButtonRemappingListTest, RetrieveButtonRemappingList) {
  auto local_state = std::make_unique<TestingPrefServiceSimple>();
  user_manager::KnownUser::RegisterPrefs(local_state->registry());
  user_manager::KnownUser known_user(local_state.get());
  const base::Value::List* button_remapping_list =
      GetLoginScreenButtonRemappingList(local_state.get(), account_id,
                                        kTestPrefKey);
  EXPECT_EQ(nullptr, button_remapping_list);
  known_user.SetPath(account_id, kTestPrefKey,
                     std::make_optional<base::Value>(base::Value::List()));
  const base::Value::List* valid_button_remapping_list =
      GetLoginScreenButtonRemappingList(local_state.get(), account_id,
                                        kTestPrefKey);
  EXPECT_NE(nullptr, valid_button_remapping_list);
}

class ButtonRemappingConversionTest : public AshTestBase {};

TEST_F(ButtonRemappingConversionTest, ConvertButtonRemappingToDict) {
  const base::Value::Dict dict1 = ConvertButtonRemappingToDict(
      button_remapping1, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(button_remapping1.name,
            *dict1.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(
      static_cast<int>(button_remapping1.button->get_customizable_button()),
      *dict1.FindInt(prefs::kButtonRemappingCustomizableButton));
  EXPECT_EQ(static_cast<int>(
                button_remapping1.remapping_action->get_accelerator_action()),
            *dict1.FindInt(prefs::kButtonRemappingAcceleratorAction));

  const base::Value::Dict dict2 = ConvertButtonRemappingToDict(
      button_remapping2, mojom::CustomizationRestriction::kAllowCustomizations);
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

  const base::Value::Dict dict3 = ConvertButtonRemappingToDict(
      button_remapping3, mojom::CustomizationRestriction::kAllowCustomizations);
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

  const base::Value::Dict dict4 = ConvertButtonRemappingToDict(
      button_remapping4, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(button_remapping4.name,
            *dict4.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(static_cast<int>(button_remapping4.button->get_vkey()),
            *dict4.FindInt(prefs::kButtonRemappingKeyboardCode));
  EXPECT_EQ(nullptr, dict4.FindDict(prefs::kButtonRemappingKeyEvent));
  EXPECT_EQ(std::nullopt,
            dict4.FindInt(prefs::kButtonRemappingAcceleratorAction));

  const base::Value::Dict dict5 = ConvertButtonRemappingToDict(
      button_remapping5, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(button_remapping5.name,
            *dict5.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(static_cast<int>(button_remapping5.button->get_vkey()),
            *dict5.FindInt(prefs::kButtonRemappingKeyboardCode));
  EXPECT_EQ(nullptr, dict5.FindDict(prefs::kButtonRemappingKeyEvent));
  EXPECT_EQ(std::nullopt,
            dict5.FindInt(prefs::kButtonRemappingAcceleratorAction));
  EXPECT_EQ(
      static_cast<int>(
          button_remapping5.remapping_action->get_static_shortcut_action()),
      dict5.FindInt(prefs::kButtonRemappingStaticShortcutAction));
}

TEST_F(ButtonRemappingConversionTest,
       ButtonToDictMouseCustomizationRestriction) {
  const base::Value::Dict dict1 = ConvertButtonRemappingToDict(
      button_remapping3,
      mojom::CustomizationRestriction::kDisallowCustomizations);
  EXPECT_TRUE(dict1.empty());

  const base::Value::Dict dict2 = ConvertButtonRemappingToDict(
      button_remapping3,
      mojom::CustomizationRestriction::kDisableKeyEventRewrites);
  EXPECT_TRUE(dict2.empty());

  // Rewrite alphabet letter key event.
  const base::Value::Dict dict3 = ConvertButtonRemappingToDict(
      button_remapping6,
      mojom::CustomizationRestriction::kAllowAlphabetKeyEventRewrites);
  EXPECT_FALSE(dict3.empty());

  // Rewrite non alphabet letter key event.
  const base::Value::Dict dict4 = ConvertButtonRemappingToDict(
      button_remapping3,
      mojom::CustomizationRestriction::kAllowAlphabetKeyEventRewrites);
  EXPECT_TRUE(dict4.empty());

  // Rewrite number key event.
  const base::Value::Dict dict5 = ConvertButtonRemappingToDict(
      button_remapping3,
      mojom::CustomizationRestriction::kAllowAlphabetOrNumberKeyEventRewrites);
  EXPECT_FALSE(dict5.empty());

  // Rewrite neither alphabet letter key event or number key event.
  const base::Value::Dict dict6 = ConvertButtonRemappingToDict(
      button_remapping7,
      mojom::CustomizationRestriction::kAllowAlphabetOrNumberKeyEventRewrites);
  EXPECT_TRUE(dict6.empty());
}

TEST_F(ButtonRemappingConversionTest, ConvertDictToButtonRemapping) {
  // Valid dict with name, customizable button and accelerator action fields.
  base::Value::Dict dict1;
  dict1.Set(prefs::kButtonRemappingName, button_remapping1.name);
  dict1.Set(
      prefs::kButtonRemappingCustomizableButton,
      static_cast<int>(button_remapping1.button->get_customizable_button()));
  dict1.Set(prefs::kButtonRemappingAcceleratorAction,
            static_cast<int>(
                button_remapping1.remapping_action->get_accelerator_action()));

  mojom::ButtonRemappingPtr remapping1 = ConvertDictToButtonRemapping(
      dict1, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(*dict1.FindString(prefs::kButtonRemappingName), remapping1->name);
  EXPECT_TRUE(remapping1->button->is_customizable_button());
  EXPECT_EQ(static_cast<mojom::CustomizableButton>(
                *dict1.FindInt(prefs::kButtonRemappingCustomizableButton)),
            remapping1->button->get_customizable_button());
  EXPECT_TRUE(remapping1->remapping_action->is_accelerator_action());
  EXPECT_EQ(static_cast<uint>(
                *dict1.FindInt(prefs::kButtonRemappingAcceleratorAction)),
            remapping1->remapping_action->get_accelerator_action());

  // Valid dict with name, customizable button and key event fields.
  base::Value::Dict dict2;
  dict2.Set(prefs::kButtonRemappingName, button_remapping2.name);
  dict2.Set(
      prefs::kButtonRemappingCustomizableButton,
      static_cast<int>(button_remapping2.button->get_customizable_button()));

  // Construct the key event dict.
  base::Value::Dict dict2_key_event;
  dict2_key_event.Set(
      prefs::kButtonRemappingDomCode,
      static_cast<int>(
          button_remapping2.remapping_action->get_key_event()->dom_code));
  dict2_key_event.Set(
      prefs::kButtonRemappingDomKey,
      static_cast<int>(
          button_remapping2.remapping_action->get_key_event()->dom_key));
  dict2_key_event.Set(
      prefs::kButtonRemappingModifiers,
      static_cast<int>(
          button_remapping2.remapping_action->get_key_event()->modifiers));
  dict2_key_event.Set(
      prefs::kButtonRemappingKeyboardCode,
      static_cast<int>(
          button_remapping2.remapping_action->get_key_event()->vkey));
  dict2.Set(prefs::kButtonRemappingKeyEvent, std::move(dict2_key_event));

  mojom::ButtonRemappingPtr remapping2 = ConvertDictToButtonRemapping(
      dict2, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(*dict2.FindString(prefs::kButtonRemappingName), remapping2->name);
  EXPECT_TRUE(remapping2->button->is_customizable_button());
  EXPECT_EQ(static_cast<mojom::CustomizableButton>(
                *dict2.FindInt(prefs::kButtonRemappingCustomizableButton)),
            remapping2->button->get_customizable_button());
  EXPECT_TRUE(remapping2->remapping_action->is_key_event());
  EXPECT_EQ(static_cast<uint>(*dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingDomCode)),
            remapping2->remapping_action->get_key_event()->dom_code);
  EXPECT_EQ(static_cast<uint>(*dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingDomKey)),
            remapping2->remapping_action->get_key_event()->dom_key);
  EXPECT_EQ(static_cast<uint>(*dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingModifiers)),
            remapping2->remapping_action->get_key_event()->modifiers);
  EXPECT_EQ(
      static_cast<uint>(*dict2.FindDict(prefs::kButtonRemappingKeyEvent)
                             ->FindInt(prefs::kButtonRemappingKeyboardCode)),
      remapping2->remapping_action->get_key_event()->vkey);

  // Valid dict with name, vkey and key event fields.
  base::Value::Dict dict3;
  dict3.Set(prefs::kButtonRemappingName, button_remapping3.name);
  dict3.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping3.button->get_vkey()));

  // Construct the key event dict.
  base::Value::Dict dict3_key_event;
  dict3_key_event.Set(
      prefs::kButtonRemappingDomCode,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->dom_code));
  dict3_key_event.Set(
      prefs::kButtonRemappingDomKey,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->dom_key));
  dict3_key_event.Set(
      prefs::kButtonRemappingModifiers,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->modifiers));
  dict3_key_event.Set(
      prefs::kButtonRemappingKeyboardCode,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->vkey));
  dict3.Set(prefs::kButtonRemappingKeyEvent, std::move(dict3_key_event));

  mojom::ButtonRemappingPtr remapping3 = ConvertDictToButtonRemapping(
      dict3, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(*dict3.FindString(prefs::kButtonRemappingName), remapping3->name);
  EXPECT_TRUE(remapping3->button->is_vkey());
  EXPECT_EQ(static_cast<::ui::KeyboardCode>(
                *dict3.FindInt(prefs::kButtonRemappingKeyboardCode)),
            remapping3->button->get_vkey());
  EXPECT_TRUE(remapping3->remapping_action->is_key_event());
  EXPECT_EQ(static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingDomCode)),
            remapping3->remapping_action->get_key_event()->dom_code);
  EXPECT_EQ(static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingDomKey)),
            remapping3->remapping_action->get_key_event()->dom_key);
  EXPECT_EQ(static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingModifiers)),
            remapping3->remapping_action->get_key_event()->modifiers);
  EXPECT_EQ(
      static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                             ->FindInt(prefs::kButtonRemappingKeyboardCode)),
      remapping3->remapping_action->get_key_event()->vkey);

  // Valid dict with name and vkey fields.
  base::Value::Dict dict4;
  dict4.Set(prefs::kButtonRemappingName, button_remapping3.name);
  dict4.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping3.button->get_vkey()));

  mojom::ButtonRemappingPtr remapping4 = ConvertDictToButtonRemapping(
      dict4, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(*dict4.FindString(prefs::kButtonRemappingName), remapping4->name);
  EXPECT_TRUE(remapping4->button->is_vkey());
  EXPECT_EQ(static_cast<::ui::KeyboardCode>(
                *dict4.FindInt(prefs::kButtonRemappingKeyboardCode)),
            remapping4->button->get_vkey());
  EXPECT_EQ(std::nullopt,
            dict4.FindInt(prefs::kButtonRemappingAcceleratorAction));
  EXPECT_EQ(nullptr, dict4.FindDict(prefs::kButtonRemappingKeyEvent));

  // Invalid dict with customizable button and vkey fields.
  base::Value::Dict dict5;
  dict5.Set(prefs::kButtonRemappingName, button_remapping3.name);
  dict5.Set(
      prefs::kButtonRemappingCustomizableButton,
      static_cast<int>(button_remapping2.button->get_customizable_button()));
  dict5.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping3.button->get_vkey()));
  mojom::ButtonRemappingPtr remapping5 = ConvertDictToButtonRemapping(
      dict5, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_FALSE(remapping5);

  // Invalid dict without name field.
  base::Value::Dict dict6;
  dict6.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping3.button->get_vkey()));
  mojom::ButtonRemappingPtr remapping6 = ConvertDictToButtonRemapping(
      dict6, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_FALSE(remapping6);

  // Invalid dict without customizable button or vkey field.
  base::Value::Dict dict7;
  dict7.Set(prefs::kButtonRemappingName, button_remapping3.name);
  mojom::ButtonRemappingPtr remapping7 = ConvertDictToButtonRemapping(
      dict7, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_FALSE(remapping7);

  // Invalid dict with key event and accelerator action fields.
  base::Value::Dict dict8;
  dict8.Set(prefs::kButtonRemappingName, button_remapping3.name);
  dict8.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping3.button->get_vkey()));
  dict8.Set(prefs::kButtonRemappingKeyEvent, std::move(dict3_key_event));
  dict8.Set(prefs::kButtonRemappingAcceleratorAction,
            static_cast<int>(
                button_remapping1.remapping_action->get_accelerator_action()));
  mojom::ButtonRemappingPtr remapping8 = ConvertDictToButtonRemapping(
      dict8, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_FALSE(remapping8);

  // Valid dict with name, vkey and static shortcut action fields.
  base::Value::Dict dict9;
  dict9.Set(prefs::kButtonRemappingName, button_remapping5.name);
  dict9.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping5.button->get_vkey()));
  dict9.Set(
      prefs::kButtonRemappingStaticShortcutAction,
      static_cast<int>(
          button_remapping5.remapping_action->get_static_shortcut_action()));

  mojom::ButtonRemappingPtr remapping9 = ConvertDictToButtonRemapping(
      dict9, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(*dict9.FindString(prefs::kButtonRemappingName), remapping9->name);
  EXPECT_TRUE(remapping9->button->is_vkey());
  EXPECT_EQ(static_cast<::ui::KeyboardCode>(
                *dict9.FindInt(prefs::kButtonRemappingKeyboardCode)),
            remapping9->button->get_vkey());
  EXPECT_TRUE(remapping9->remapping_action->is_static_shortcut_action());
  EXPECT_EQ(static_cast<mojom::StaticShortcutAction>(
                *dict9.FindInt(prefs::kButtonRemappingStaticShortcutAction)),
            remapping9->remapping_action->get_static_shortcut_action());
}

TEST_F(ButtonRemappingConversionTest,
       DictToButtonMouseCustomizationRestriction) {
  // Valid dict with name, vkey and static shortcut action fields.
  base::Value::Dict dict;
  dict.Set(prefs::kButtonRemappingName, button_remapping5.name);
  dict.Set(prefs::kButtonRemappingKeyboardCode,
           static_cast<int>(button_remapping5.button->get_vkey()));
  dict.Set(
      prefs::kButtonRemappingStaticShortcutAction,
      static_cast<int>(
          button_remapping5.remapping_action->get_static_shortcut_action()));

  // Return nullptr if the customization restriction is kDisallowCustomizations.
  mojom::ButtonRemappingPtr remapping1 = ConvertDictToButtonRemapping(
      dict, mojom::CustomizationRestriction::kDisallowCustomizations);
  EXPECT_FALSE(remapping1);

  // Return nullptr if the customization restriction is
  // kDisableKeyEventRewrites.
  mojom::ButtonRemappingPtr remapping2 = ConvertDictToButtonRemapping(
      dict, mojom::CustomizationRestriction::kDisableKeyEventRewrites);
  EXPECT_FALSE(remapping2);
}

TEST_F(ButtonRemappingConversionTest, DictToButtonUnknownKeyCode) {
  // Valid dict with name, vkey and static shortcut action fields.
  base::Value::Dict dict;
  dict.Set(prefs::kButtonRemappingName, "Button 1");
  dict.Set(prefs::kButtonRemappingKeyboardCode,
           static_cast<int>(ui::VKEY_UNKNOWN));
  dict.Set(
      prefs::kButtonRemappingStaticShortcutAction,
      static_cast<int>(
          button_remapping5.remapping_action->get_static_shortcut_action()));

  // Return nullptr if the KeyCode is set to be an unknown key.
  mojom::ButtonRemappingPtr remapping1 = ConvertDictToButtonRemapping(
      dict, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_FALSE(remapping1);
}

TEST_F(ButtonRemappingConversionTest, ConvertButtonRemappingArrayToList) {
  std::vector<mojom::ButtonRemappingPtr> remappings;
  remappings.push_back(button_remapping1.Clone());
  remappings.push_back(button_remapping2.Clone());
  remappings.push_back(button_remapping4.Clone());
  base::Value::List list1 = ConvertButtonRemappingArrayToList(
      remappings, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(3, static_cast<int>(list1.size()));

  ASSERT_TRUE(list1[0].is_dict());
  const auto& dict1 = list1[0].GetDict();
  EXPECT_EQ(button_remapping1.name,
            *dict1.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(
      static_cast<int>(button_remapping1.button->get_customizable_button()),
      *dict1.FindInt(prefs::kButtonRemappingCustomizableButton));
  EXPECT_EQ(static_cast<int>(
                button_remapping1.remapping_action->get_accelerator_action()),
            *dict1.FindInt(prefs::kButtonRemappingAcceleratorAction));

  ASSERT_TRUE(list1[1].is_dict());
  const auto& dict2 = list1[1].GetDict();
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

  ASSERT_TRUE(list1[2].is_dict());
  const auto& dict3 = list1[2].GetDict();
  EXPECT_EQ(button_remapping4.name,
            *dict3.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(static_cast<int>(button_remapping4.button->get_vkey()),
            *dict3.FindInt(prefs::kButtonRemappingKeyboardCode));
  EXPECT_EQ(nullptr, dict3.FindDict(prefs::kButtonRemappingKeyEvent));
  EXPECT_EQ(std::nullopt,
            dict3.FindInt(prefs::kButtonRemappingAcceleratorAction));

  base::Value::List list2 = ConvertButtonRemappingArrayToList(
      remappings, mojom::CustomizationRestriction::kDisallowCustomizations);
  EXPECT_EQ(0, static_cast<int>(list2.size()));

  base::Value::List list3 = ConvertButtonRemappingArrayToList(
      remappings, mojom::CustomizationRestriction::kDisableKeyEventRewrites);
  EXPECT_EQ(2, static_cast<int>(list3.size()));

  ASSERT_TRUE(list3[0].is_dict());
  const auto& dict5 = list3[0].GetDict();
  EXPECT_EQ(button_remapping1.name,
            *dict5.FindString(prefs::kButtonRemappingName));

  ASSERT_TRUE(list3[1].is_dict());
  const auto& dict6 = list3[1].GetDict();
  EXPECT_EQ(button_remapping2.name,
            *dict6.FindString(prefs::kButtonRemappingName));
}

TEST_F(ButtonRemappingConversionTest, ConvertListToButtonRemappingArray) {
  // Valid dict with name, customizable button and accelerator action fields.
  base::Value::Dict dict1;
  dict1.Set(prefs::kButtonRemappingName, button_remapping1.name);
  dict1.Set(
      prefs::kButtonRemappingCustomizableButton,
      static_cast<int>(button_remapping1.button->get_customizable_button()));
  dict1.Set(prefs::kButtonRemappingAcceleratorAction,
            static_cast<int>(
                button_remapping1.remapping_action->get_accelerator_action()));

  // Invalid dict without name field.
  base::Value::Dict dict2;
  dict2.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping3.button->get_vkey()));

  // Valid dict with name, vkey and key event fields.
  base::Value::Dict dict3;
  dict3.Set(prefs::kButtonRemappingName, button_remapping3.name);
  dict3.Set(prefs::kButtonRemappingKeyboardCode,
            static_cast<int>(button_remapping3.button->get_vkey()));

  // Construct the key event dict.
  base::Value::Dict dict3_key_event;
  dict3_key_event.Set(
      prefs::kButtonRemappingDomCode,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->dom_code));
  dict3_key_event.Set(
      prefs::kButtonRemappingDomKey,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->dom_key));
  dict3_key_event.Set(
      prefs::kButtonRemappingModifiers,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->modifiers));
  dict3_key_event.Set(
      prefs::kButtonRemappingKeyboardCode,
      static_cast<int>(
          button_remapping3.remapping_action->get_key_event()->vkey));
  dict3.Set(prefs::kButtonRemappingKeyEvent, std::move(dict3_key_event));

  base::Value::List list;
  list.Append(dict1.Clone());
  list.Append(dict2.Clone());
  list.Append(dict3.Clone());

  std::vector<mojom::ButtonRemappingPtr> array =
      ConvertListToButtonRemappingArray(
          list, mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ(2, static_cast<int>(array.size()));

  mojom::ButtonRemappingPtr remapping1 = std::move(array[0]);
  EXPECT_EQ(*dict1.FindString(prefs::kButtonRemappingName), remapping1->name);
  EXPECT_TRUE(remapping1->button->is_customizable_button());
  EXPECT_EQ(static_cast<mojom::CustomizableButton>(
                *dict1.FindInt(prefs::kButtonRemappingCustomizableButton)),
            remapping1->button->get_customizable_button());
  EXPECT_TRUE(remapping1->remapping_action->is_accelerator_action());
  EXPECT_EQ(static_cast<uint>(
                *dict1.FindInt(prefs::kButtonRemappingAcceleratorAction)),
            remapping1->remapping_action->get_accelerator_action());

  mojom::ButtonRemappingPtr remapping2 = std::move(array[1]);
  EXPECT_EQ(*dict3.FindString(prefs::kButtonRemappingName), remapping2->name);
  EXPECT_TRUE(remapping2->button->is_vkey());
  EXPECT_EQ(static_cast<::ui::KeyboardCode>(
                *dict3.FindInt(prefs::kButtonRemappingKeyboardCode)),
            remapping2->button->get_vkey());
  EXPECT_TRUE(remapping2->remapping_action->is_key_event());
  EXPECT_EQ(static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingDomCode)),
            remapping2->remapping_action->get_key_event()->dom_code);
  EXPECT_EQ(static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingDomKey)),
            remapping2->remapping_action->get_key_event()->dom_key);
  EXPECT_EQ(static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                                   ->FindInt(prefs::kButtonRemappingModifiers)),
            remapping2->remapping_action->get_key_event()->modifiers);
  EXPECT_EQ(
      static_cast<uint>(*dict3.FindDict(prefs::kButtonRemappingKeyEvent)
                             ->FindInt(prefs::kButtonRemappingKeyboardCode)),
      remapping2->remapping_action->get_key_event()->vkey);

  std::vector<mojom::ButtonRemappingPtr> array2 =
      ConvertListToButtonRemappingArray(
          list, mojom::CustomizationRestriction::kDisallowCustomizations);
  EXPECT_EQ(0, static_cast<int>(array2.size()));

  std::vector<mojom::ButtonRemappingPtr> array3 =
      ConvertListToButtonRemappingArray(
          list, mojom::CustomizationRestriction::kDisableKeyEventRewrites);
  EXPECT_EQ(1, static_cast<int>(array3.size()));
  mojom::ButtonRemappingPtr remapping3 = std::move(array3[0]);
  EXPECT_EQ(*dict1.FindString(prefs::kButtonRemappingName), remapping3->name);
  EXPECT_TRUE(remapping3->button->is_customizable_button());
  EXPECT_EQ(static_cast<mojom::CustomizableButton>(
                *dict1.FindInt(prefs::kButtonRemappingCustomizableButton)),
            remapping3->button->get_customizable_button());
  EXPECT_TRUE(remapping3->remapping_action->is_accelerator_action());
  EXPECT_EQ(static_cast<uint>(
                *dict1.FindInt(prefs::kButtonRemappingAcceleratorAction)),
            remapping3->remapping_action->get_accelerator_action());
}

TEST_F(ButtonRemappingConversionTest, RedactButtonNames) {
  auto button_remapping_dict = ConvertButtonRemappingToDict(
      button_remapping1, mojom::CustomizationRestriction::kAllowCustomizations,
      /*redact_button_names=*/true);
  auto button_remapping = ConvertDictToButtonRemapping(
      button_remapping_dict,
      mojom::CustomizationRestriction::kAllowCustomizations);
  EXPECT_EQ("REDACTED", button_remapping->name);
}

class GetDeviceKeyForMetadataRequestTest : public testing::Test {
 public:
  GetDeviceKeyForMetadataRequestTest() = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(GetDeviceKeyForMetadataRequestTest, DeviceKeyRewrittenWhenFlagEnabled) {
  scoped_feature_list_.InitWithFeatures(
      {features::kPeripheralCustomization, features::kWelcomeExperience,
       features::kWelcomeExperienceTestUnsupportedDevices},
      {});
  auto device_key = GetDeviceKeyForMetadataRequest("040e:0726");
  EXPECT_EQ("0111:185a", device_key);
}

TEST_F(GetDeviceKeyForMetadataRequestTest, DeviceKeyUnchangedWhenFlagDisabled) {
  auto device_key = GetDeviceKeyForMetadataRequest("040e:0726");
  EXPECT_EQ("040e:0726", device_key);
}

}  // namespace ash
