// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "ui/events/ash/mojom/modifier_key.mojom-shared.h"
#include "ui/events/ash/mojom/modifier_key.mojom.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/six_pack_shortcut_modifier.mojom.h"
#include "ui/events/ash/pref_names.h"

namespace ash {

namespace {
constexpr char kUserEmail[] = "example@email.com";
constexpr char kUserEmail2[] = "example2@email.com";
const AccountId account_id_1 = AccountId::FromUserEmail(kUserEmail);
const AccountId account_id_2 = AccountId::FromUserEmail(kUserEmail2);

const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kKeyboardKey1 = "device_key1";
const std::string kKeyboardKey2 = "device_key2";
const std::string kKeyboardKey3 = "device_key3";

const bool kGlobalSendFunctionKeys = false;

const mojom::KeyboardSettings kKeyboardSettingsDefault(
    /*modifier_remappings=*/{},
    /*top_row_are_fkeys=*/kDefaultTopRowAreFKeys,
    /*suppress_meta_fkey_rewrites=*/kDefaultSuppressMetaFKeyRewrites,
    /*six_pack_key_remappings=*/{});

const mojom::KeyboardSettings kKeyboardSettingsNotDefault(
    /*modifier_remappings=*/{},
    /*top_row_are_fkeys=*/!kDefaultTopRowAreFKeys,
    /*suppress_meta_fkey_rewrites=*/!kDefaultSuppressMetaFKeyRewrites,
    /*six_pack_key_remappings=*/{});

const mojom::KeyboardSettings kKeyboardSettings1(
    /*modifier_remappings=*/{},
    /*top_row_are_fkeys=*/false,
    /*suppress_meta_fkey_rewrites=*/false,
    /*six_pack_key_remappings=*/{});

const mojom::KeyboardSettings kKeyboardSettings2(
    /*modifier_remappings=*/{{ui::mojom::ModifierKey::kControl,
                              ui::mojom::ModifierKey::kAlt},
                             {ui::mojom::ModifierKey::kAssistant,
                              ui::mojom::ModifierKey::kVoid}},
    /*top_row_are_fkeys=*/true,
    /*suppress_meta_fkey_rewrites=*/true,
    /*six_pack_key_remappings=*/{});

const mojom::KeyboardSettings kKeyboardSettings3(
    /*modifier_remappings=*/{{ui::mojom::ModifierKey::kAlt,
                              ui::mojom::ModifierKey::kCapsLock},
                             {ui::mojom::ModifierKey::kAssistant,
                              ui::mojom::ModifierKey::kVoid},
                             {ui::mojom::ModifierKey::kBackspace,
                              ui::mojom::ModifierKey::kEscape},
                             {ui::mojom::ModifierKey::kControl,
                              ui::mojom::ModifierKey::kAssistant}},
    /*top_row_are_fkeys=*/true,
    /*suppress_meta_fkey_rewrites=*/false,
    /*six_pack_key_remappings=*/{});
}  // namespace

class KeyboardPrefHandlerTest : public AshTestBase {
 public:
  KeyboardPrefHandlerTest() = default;
  KeyboardPrefHandlerTest(const KeyboardPrefHandlerTest&) = delete;
  KeyboardPrefHandlerTest& operator=(const KeyboardPrefHandlerTest&) = delete;
  ~KeyboardPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kInputDeviceSettingsSplit);
    AshTestBase::SetUp();
    InitializePrefService();
    pref_handler_ = std::make_unique<KeyboardPrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

  void InitializePrefService() {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kKeyboardDeviceSettingsDictPref);
    pref_service_->registry()->RegisterBooleanPref(prefs::kSendFunctionKeys,
                                                   kGlobalSendFunctionKeys);

    // Register Integer prefs which determine how we remap modifiers.
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapAltKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kAlt));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapControlKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kControl));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapEscapeKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kEscape));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapBackspaceKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kBackspace));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapAssistantKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kAssistant));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapCapsLockKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kCapsLock));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapExternalMetaKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kMeta));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapSearchKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kMeta));
    pref_service_->registry()->RegisterIntegerPref(
        ::prefs::kLanguageRemapExternalCommandKeyTo,
        static_cast<int>(ui::mojom::ModifierKey::kMeta));

    pref_service_->registry()->RegisterIntegerPref(
        prefs::kKeyEventRemappedToSixPackDelete, 0);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kKeyEventRemappedToSixPackEnd, 0);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kKeyEventRemappedToSixPackHome, 0);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kKeyEventRemappedToSixPackPageUp, 0);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kKeyEventRemappedToSixPackPageDown, 0);
  }

  void CheckKeyboardSettingsAndDictAreEqual(
      const mojom::KeyboardSettings& settings,
      const base::Value::Dict& settings_dict) {
    auto suppress_meta_fkey_rewrites =
        settings_dict.FindBool(prefs::kKeyboardSettingSuppressMetaFKeyRewrites);
    if (suppress_meta_fkey_rewrites.has_value()) {
      EXPECT_EQ(settings.suppress_meta_fkey_rewrites,
                suppress_meta_fkey_rewrites);
    } else {
      EXPECT_EQ(settings.suppress_meta_fkey_rewrites,
                kDefaultSuppressMetaFKeyRewrites);
    }

    auto top_row_are_fkeys =
        settings_dict.FindBool(prefs::kKeyboardSettingTopRowAreFKeys);
    if (top_row_are_fkeys.has_value()) {
      EXPECT_EQ(settings.top_row_are_fkeys, top_row_are_fkeys);
    } else {
      EXPECT_EQ(settings.top_row_are_fkeys, kDefaultTopRowAreFKeys);
    }

    auto* modifier_remappings_dict =
        settings_dict.FindDict(prefs::kKeyboardSettingModifierRemappings);
    ASSERT_NE(modifier_remappings_dict, nullptr);
    for (const auto& [from_expected, to_expected] :
         settings.modifier_remappings) {
      auto to = modifier_remappings_dict->FindInt(
          base::NumberToString(static_cast<int>(from_expected)));
      ASSERT_TRUE(to.has_value());
      EXPECT_EQ(static_cast<int>(to_expected), to);
    }
  }

  void CheckKeyboardSettingsAreSetToDefaultValues(
      const mojom::KeyboardSettings& settings,
      bool is_external) {
    const bool default_top_row_are_fkeys =
        is_external ? !kKeyboardSettingsDefault.top_row_are_fkeys
                    : kKeyboardSettingsDefault.top_row_are_fkeys;
    EXPECT_EQ(settings.suppress_meta_fkey_rewrites,
              kKeyboardSettingsDefault.suppress_meta_fkey_rewrites);
    EXPECT_EQ(settings.top_row_are_fkeys, default_top_row_are_fkeys);
    EXPECT_EQ(0u, settings.modifier_remappings.size());
  }

  void CallUpdateKeyboardSettings(const std::string& device_key,
                                  const mojom::KeyboardSettings& settings) {
    mojom::KeyboardPtr keyboard = mojom::Keyboard::New();
    keyboard->settings = settings.Clone();
    keyboard->device_key = device_key;

    pref_handler_->UpdateKeyboardSettings(pref_service_.get(),
                                          /*keyboard_policies=*/{}, *keyboard);
  }

  void CallUpdateLoginScreenKeyboardSettings(
      const AccountId& account_id,
      const std::string& device_key,
      const mojom::KeyboardSettings& settings) {
    mojom::KeyboardPtr keyboard = mojom::Keyboard::New();
    keyboard->settings = settings.Clone();
    pref_handler_->UpdateLoginScreenKeyboardSettings(local_state(), account_id,
                                                     /*keyboard_policies=*/{},
                                                     *keyboard);
  }

  mojom::KeyboardSettingsPtr CallInitializeKeyboardSettings(
      const std::string& device_key) {
    mojom::KeyboardPtr keyboard = mojom::Keyboard::New();
    keyboard->device_key = device_key;

    pref_handler_->InitializeKeyboardSettings(
        pref_service_.get(), /*keyboard_policies=*/{}, keyboard.get());
    return std::move(keyboard->settings);
  }

  mojom::KeyboardSettingsPtr CallInitializeKeyboardSettings(
      const mojom::Keyboard& keyboard) {
    const auto keyboard_ptr = keyboard.Clone();
    pref_handler_->InitializeKeyboardSettings(
        pref_service_.get(), /*keyboard_policies=*/{}, keyboard_ptr.get());
    return std::move(keyboard_ptr->settings);
  }

  mojom::KeyboardSettingsPtr CallInitializeLoginScreenKeyboardSettings(
      const AccountId& account_id,
      const mojom::Keyboard& keyboard) {
    const auto keyboard_ptr = keyboard.Clone();

    pref_handler_->InitializeLoginScreenKeyboardSettings(
        local_state(), account_id, /*keyboard_policies=*/{},
        keyboard_ptr.get());
    return std::move(keyboard_ptr->settings);
  }

  const base::Value::Dict* GetSettingsDictForDeviceKey(
      const std::string& device_key) {
    const auto& devices_dict =
        pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
    EXPECT_EQ(1u, devices_dict.size());
    const auto* settings_dict = devices_dict.FindDict(device_key);
    EXPECT_NE(nullptr, settings_dict);

    return settings_dict;
  }

  user_manager::KnownUser known_user() {
    return user_manager::KnownUser(local_state());
  }

  bool HasInternalLoginScreenSettingsDict(AccountId account_id) {
    const auto* dict = known_user().FindPath(
        account_id, prefs::kKeyboardLoginScreenInternalSettingsPref);
    return dict && dict->is_dict();
  }

  bool HasExternalLoginScreenSettingsDict(AccountId account_id) {
    const auto* dict = known_user().FindPath(
        account_id, prefs::kKeyboardLoginScreenExternalSettingsPref);
    return dict && dict->is_dict();
  }

  base::Value::Dict GetInternalLoginScreenSettingsDict(AccountId account_id) {
    return known_user()
        .FindPath(account_id, prefs::kKeyboardLoginScreenInternalSettingsPref)
        ->GetDict()
        .Clone();
  }

  base::Value::Dict GetExternalLoginScreenSettingsDict(AccountId account_id) {
    return known_user()
        .FindPath(account_id, prefs::kKeyboardLoginScreenExternalSettingsPref)
        ->GetDict()
        .Clone();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<KeyboardPrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(KeyboardPrefHandlerTest, MultipleDevices) {
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettings1);
  CallUpdateKeyboardSettings(kKeyboardKey2, kKeyboardSettings2);
  CallUpdateKeyboardSettings(kKeyboardKey3, kKeyboardSettings3);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  ASSERT_EQ(3u, devices_dict.size());

  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettings1, *settings_dict);

  settings_dict = devices_dict.FindDict(kKeyboardKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettings2, *settings_dict);

  settings_dict = devices_dict.FindDict(kKeyboardKey3);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettings3, *settings_dict);
}

TEST_F(KeyboardPrefHandlerTest, InitializeLoginScreenKeyboardSettings) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;
  keyboard.is_external = false;
  mojom::KeyboardSettingsPtr settings =
      CallInitializeLoginScreenKeyboardSettings(account_id_1, keyboard);

  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
  CheckKeyboardSettingsAreSetToDefaultValues(*settings, keyboard.is_external);

  keyboard.is_external = true;
  settings = CallInitializeLoginScreenKeyboardSettings(account_id_2, keyboard);
  EXPECT_FALSE(HasExternalLoginScreenSettingsDict(account_id_2));

  CheckKeyboardSettingsAreSetToDefaultValues(*settings, keyboard.is_external);
}

TEST_F(KeyboardPrefHandlerTest, UpdateLoginScreenKeyboardSettings) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;
  keyboard.is_external = false;
  mojom::KeyboardSettingsPtr settings =
      CallInitializeLoginScreenKeyboardSettings(account_id_1, keyboard);

  CheckKeyboardSettingsAreSetToDefaultValues(*settings, keyboard.is_external);
  mojom::KeyboardSettingsPtr updated_settings = mojo::Clone(settings);
  updated_settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kControl}};
  updated_settings->suppress_meta_fkey_rewrites =
      !updated_settings->suppress_meta_fkey_rewrites;
  updated_settings->top_row_are_fkeys = !updated_settings->top_row_are_fkeys;
  CallUpdateLoginScreenKeyboardSettings(account_id_1, kKeyboardKey1,
                                        *updated_settings);
  const auto& updated_settings_dict =
      GetInternalLoginScreenSettingsDict(account_id_1);
  CheckKeyboardSettingsAndDictAreEqual(*updated_settings,
                                       updated_settings_dict);
}

TEST_F(KeyboardPrefHandlerTest,
       LoginScreenPrefsNotPersistedWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::Keyboard keyboard1;
  keyboard1.device_key = kKeyboardKey1;
  keyboard1.is_external = false;
  mojom::Keyboard keyboard2;
  keyboard2.device_key = kKeyboardKey2;
  keyboard2.is_external = true;
  CallInitializeLoginScreenKeyboardSettings(account_id_1, keyboard1);
  CallInitializeLoginScreenKeyboardSettings(account_id_1, keyboard2);
  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
  EXPECT_FALSE(HasExternalLoginScreenSettingsDict(account_id_1));
}

TEST_F(KeyboardPrefHandlerTest, PreservesOldSettings) {
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettings1);

  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);
  ASSERT_NE(nullptr, settings_dict);

  // Set a fake key to simulate a setting being removed from 1 milestone to the
  // next.
  settings_dict->Set(kDictFakeKey, kDictFakeValue);
  pref_service_->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Update the settings again and verify the fake key and value still exist.
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettings1);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kKeyboardKey1);

  const std::string* value = updated_settings_dict->FindString(kDictFakeKey);
  ASSERT_NE(nullptr, value);
  EXPECT_EQ(kDictFakeValue, *value);
}

TEST_F(KeyboardPrefHandlerTest, UpdateSettings) {
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettings1);
  CallUpdateKeyboardSettings(kKeyboardKey2, kKeyboardSettings2);

  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettings1, *settings_dict);

  settings_dict = devices_dict.FindDict(kKeyboardKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettings2, *settings_dict);

  mojom::KeyboardSettingsPtr updated_settings = kKeyboardSettings1.Clone();
  updated_settings->modifier_remappings = {
      {ui::mojom::ModifierKey::kAlt, ui::mojom::ModifierKey::kControl}};
  updated_settings->suppress_meta_fkey_rewrites =
      !updated_settings->suppress_meta_fkey_rewrites;
  updated_settings->top_row_are_fkeys = !updated_settings->top_row_are_fkeys;

  // Update the settings again and verify the settings are updated in place.
  CallUpdateKeyboardSettings(kKeyboardKey1, *updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kKeyboardKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(*updated_settings,
                                       *updated_settings_dict);

  // Verify other device remains unmodified.
  const auto* unchanged_settings_dict =
      updated_devices_dict.FindDict(kKeyboardKey2);
  ASSERT_NE(nullptr, unchanged_settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettings2,
                                       *unchanged_settings_dict);
}

TEST_F(KeyboardPrefHandlerTest, NewSettingAddedRoundTrip) {
  mojom::KeyboardSettingsPtr test_settings = kKeyboardSettings1.Clone();
  test_settings->suppress_meta_fkey_rewrites =
      !kDefaultSuppressMetaFKeyRewrites;

  CallUpdateKeyboardSettings(kKeyboardKey1, *test_settings);
  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);

  // Remove keys from the dict to mock adding a new setting in the future.
  settings_dict->Remove(prefs::kKeyboardSettingSuppressMetaFKeyRewrites);
  pref_service_->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Initialize keyboard settings for the device and check that
  // "new settings" match their default values.
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(kKeyboardKey1);
  EXPECT_EQ(kDefaultSuppressMetaFKeyRewrites,
            settings->suppress_meta_fkey_rewrites);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->suppress_meta_fkey_rewrites = !kDefaultSuppressMetaFKeyRewrites;
  EXPECT_EQ(*test_settings, *settings);
}

TEST_F(KeyboardPrefHandlerTest, DefaultSettingsWhenPrefServiceNull) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;
  pref_handler_->InitializeKeyboardSettings(nullptr, /*keyboard_policies=*/{},
                                            &keyboard);
  EXPECT_EQ(kKeyboardSettingsDefault, *keyboard.settings);
}

TEST_F(KeyboardPrefHandlerTest, NewKeyboardsDefaultSettings) {
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(kKeyboardKey1);
  EXPECT_EQ(*settings, kKeyboardSettingsDefault);
  settings = CallInitializeKeyboardSettings(kKeyboardKey2);
  EXPECT_EQ(*settings, kKeyboardSettingsDefault);
  settings = CallInitializeKeyboardSettings(kKeyboardKey3);
  EXPECT_EQ(*settings, kKeyboardSettingsDefault);

  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  ASSERT_EQ(3u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettingsDefault,
                                       *settings_dict);

  settings_dict = devices_dict.FindDict(kKeyboardKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettingsDefault,
                                       *settings_dict);

  settings_dict = devices_dict.FindDict(kKeyboardKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettingsDefault,
                                       *settings_dict);

  settings_dict = devices_dict.FindDict(kKeyboardKey3);
  ASSERT_NE(nullptr, settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettingsDefault,
                                       *settings_dict);
}

TEST_F(KeyboardPrefHandlerTest,
       TopRowAreFKeysEnabledByDefaultForExternalKeyboard) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;
  keyboard.is_external = kDefaultTopRowAreFKeysExternal;
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(keyboard);
  EXPECT_TRUE(settings->top_row_are_fkeys);

  mojom::Keyboard keyboard2;
  keyboard.device_key = kKeyboardKey2;
  mojom::KeyboardSettingsPtr settings2 =
      CallInitializeKeyboardSettings(keyboard2);
  // `top_row_are_fkeys` defaults to false for internal keyboards.
  EXPECT_FALSE(settings2->top_row_are_fkeys);
}

TEST_F(KeyboardPrefHandlerTest, InvalidModifierRemappings) {
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettings1);
  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);

  base::Value::Dict invalid_modifier_remappings;
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(ui::mojom::ModifierKey::kMaxValue) +
                           1),
      static_cast<int>(ui::mojom::ModifierKey::kControl));
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(ui::mojom::ModifierKey::kMinValue) -
                           1),
      static_cast<int>(ui::mojom::ModifierKey::kControl));
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(ui::mojom::ModifierKey::kControl)),
      static_cast<int>(ui::mojom::ModifierKey::kMaxValue) + 1);
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(ui::mojom::ModifierKey::kAlt)),
      static_cast<int>(ui::mojom::ModifierKey::kMinValue) - 1);

  // Set 1 valid modifier remapping to check that it skips invalid remappings,
  // and keeps the valid.
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(ui::mojom::ModifierKey::kAlt)),
      static_cast<int>(ui::mojom::ModifierKey::kControl));

  // Set modifier remappings to the invalid set.
  settings_dict->Set(prefs::kKeyboardSettingModifierRemappings,
                     invalid_modifier_remappings.Clone());
  pref_service_->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Initialize keyboard settings for the device and check that the remapped
  // modifiers only include the valid value.
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(kKeyboardKey1);

  ASSERT_EQ(1u, settings->modifier_remappings.size());
  ASSERT_TRUE(base::Contains(settings->modifier_remappings,
                             ui::mojom::ModifierKey::kAlt));
  EXPECT_EQ(ui::mojom::ModifierKey::kControl,
            settings->modifier_remappings[ui::mojom::ModifierKey::kAlt]);
}

TEST_F(KeyboardPrefHandlerTest, KeyboardObserveredInTransitionPeriod) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;
  Shell::Get()->input_device_tracker()->OnKeyboardConnected(keyboard);
  // Initialize keyboard settings for the device and check that the global
  // prefs were used as defaults.
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(keyboard.device_key);
  ASSERT_EQ(settings->top_row_are_fkeys, kGlobalSendFunctionKeys);
  ASSERT_EQ(settings->suppress_meta_fkey_rewrites,
            kDefaultSuppressMetaFKeyRewrites);
}

TEST_F(KeyboardPrefHandlerTest,
       KeyboardSendFunctionKeysTransitionPrefAlwaysConsistent) {
  mojom::Keyboard keyboard;
  keyboard.is_external = true;
  keyboard.meta_key = mojom::MetaKey::kExternalMeta;
  keyboard.device_key = kKeyboardKey1;
  {
    base::test::ScopedFeatureList disable_settings_split_feature_list;
    disable_settings_split_feature_list.InitAndDisableFeature(
        features::kInputDeviceSettingsSplit);
    Shell::Get()->input_device_tracker()->OnKeyboardConnected(keyboard);
  }

  // Initialize keyboard settings for the device and check that the global
  // prefs were used as defaults even though the `kSendFunctionKeys` pref was
  // never changed and it goes against the default since the keyboard is
  // external.
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(keyboard);
  ASSERT_EQ(settings->top_row_are_fkeys, kGlobalSendFunctionKeys);
  ASSERT_EQ(settings->suppress_meta_fkey_rewrites,
            kDefaultSuppressMetaFKeyRewrites);
}

TEST_F(KeyboardPrefHandlerTest, ModifierRemappingsFromGlobalPrefs) {
  // Disable flag in this test since `InputDeviceTracker` only records
  // connected devices when the settings split flag is disabled.
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;
  keyboard.modifier_keys = {
      ui::mojom::ModifierKey::kAlt,       ui::mojom::ModifierKey::kControl,
      ui::mojom::ModifierKey::kAssistant, ui::mojom::ModifierKey::kBackspace,
      ui::mojom::ModifierKey::kMeta,      ui::mojom::ModifierKey::kEscape};
  keyboard.meta_key = mojom::MetaKey::kSearch;
  // Remap Alt + Meta keys.
  pref_service_->SetInteger(
      ::prefs::kLanguageRemapAltKeyTo,
      static_cast<int>(ui::mojom::ModifierKey::kCapsLock));
  pref_service_->SetInteger(::prefs::kLanguageRemapSearchKeyTo,
                            static_cast<int>(ui::mojom::ModifierKey::kEscape));

  Shell::Get()->input_device_tracker()->OnKeyboardConnected(keyboard);
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(keyboard);

  ASSERT_EQ(settings->modifier_remappings.size(), 2u);
  ASSERT_EQ(settings->modifier_remappings.at(ui::mojom::ModifierKey::kAlt),
            ui::mojom::ModifierKey::kCapsLock);
  ASSERT_EQ(settings->modifier_remappings.at(ui::mojom::ModifierKey::kMeta),
            ui::mojom::ModifierKey::kEscape);
}

TEST_F(KeyboardPrefHandlerTest, SwichControlAndCommandForAppleKeyboard) {
  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;
  keyboard.meta_key = mojom::MetaKey::kCommand;
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(keyboard);

  ASSERT_EQ(settings->modifier_remappings.at(ui::mojom::ModifierKey::kControl),
            ui::mojom::ModifierKey::kMeta);
  ASSERT_EQ(settings->modifier_remappings.at(ui::mojom::ModifierKey::kMeta),
            ui::mojom::ModifierKey::kControl);
}

TEST_F(KeyboardPrefHandlerTest, DefaultNotPersistedUntilUpdated) {
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettingsDefault);

  // Default settings are not persisted to storage.
  const auto* settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kKeyboardSettingTopRowAreFKeys));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kKeyboardSettingSuppressMetaFKeyRewrites));
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettingsDefault,
                                       *settings_dict);

  // When the settings are updated, their values should be persisted.
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettingsNotDefault);
  settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kKeyboardSettingTopRowAreFKeys));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kKeyboardSettingSuppressMetaFKeyRewrites));
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettingsNotDefault,
                                       *settings_dict);

  // When the settings are set back to the default, values should be persisted
  // as they are now "user chosen".
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettingsDefault);
  settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kKeyboardSettingTopRowAreFKeys));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kKeyboardSettingSuppressMetaFKeyRewrites));
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettingsDefault,
                                       *settings_dict);
}

TEST_F(KeyboardPrefHandlerTest,
       NewKeyboard_ManagedEnterprisePolicy_GetsDefaults) {
  mojom::KeyboardPolicies policies;
  policies.top_row_are_fkeys_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kManaged, !kDefaultTopRowAreFKeys);

  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;

  pref_handler_->InitializeKeyboardSettings(pref_service_.get(), policies,
                                            &keyboard);

  EXPECT_EQ(!kDefaultTopRowAreFKeys, keyboard.settings->top_row_are_fkeys);
  keyboard.settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  EXPECT_EQ(kKeyboardSettingsDefault, *keyboard.settings);

  const auto* settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kKeyboardSettingTopRowAreFKeys));
}

TEST_F(KeyboardPrefHandlerTest,
       NewKeyboard_RecommendedEnterprisePolicy_GetsDefaults) {
  mojom::KeyboardPolicies policies;
  policies.top_row_are_fkeys_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kRecommended, !kDefaultTopRowAreFKeys);

  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;

  pref_handler_->InitializeKeyboardSettings(pref_service_.get(), policies,
                                            &keyboard);

  EXPECT_EQ(!kDefaultTopRowAreFKeys, keyboard.settings->top_row_are_fkeys);
  keyboard.settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  EXPECT_EQ(kKeyboardSettingsDefault, *keyboard.settings);

  const auto* settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kKeyboardSettingTopRowAreFKeys));
}

TEST_F(KeyboardPrefHandlerTest,
       ExistingKeyboard_RecommendedEnterprisePolicy_GetsNewPolicy) {
  mojom::KeyboardPolicies policies;
  policies.top_row_are_fkeys_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kRecommended, !kDefaultTopRowAreFKeys);

  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;

  pref_handler_->InitializeKeyboardSettings(
      pref_service_.get(), /*keyboard_policies=*/{}, &keyboard);
  EXPECT_EQ(kKeyboardSettingsDefault, *keyboard.settings);

  pref_handler_->InitializeKeyboardSettings(pref_service_.get(), policies,
                                            &keyboard);
  EXPECT_EQ(!kDefaultTopRowAreFKeys, keyboard.settings->top_row_are_fkeys);
  keyboard.settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  EXPECT_EQ(kKeyboardSettingsDefault, *keyboard.settings);

  const auto* settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kKeyboardSettingTopRowAreFKeys));
}

TEST_F(KeyboardPrefHandlerTest,
       ExistingKeyboard_ManagedEnterprisePolicy_GetsNewPolicy) {
  mojom::KeyboardPolicies policies;
  policies.top_row_are_fkeys_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kManaged, !kDefaultTopRowAreFKeys);

  mojom::Keyboard keyboard;
  keyboard.device_key = kKeyboardKey1;

  pref_handler_->InitializeKeyboardSettings(
      pref_service_.get(), /*keyboard_policies=*/{}, &keyboard);
  EXPECT_EQ(kKeyboardSettingsDefault, *keyboard.settings);

  keyboard.settings->top_row_are_fkeys = !kDefaultTopRowAreFKeys;
  CallUpdateKeyboardSettings(kKeyboardKey1, *keyboard.settings);

  pref_handler_->InitializeKeyboardSettings(pref_service_.get(), policies,
                                            &keyboard);
  EXPECT_EQ(!kDefaultTopRowAreFKeys, keyboard.settings->top_row_are_fkeys);
  keyboard.settings->top_row_are_fkeys = kDefaultTopRowAreFKeys;
  EXPECT_EQ(kKeyboardSettingsDefault, *keyboard.settings);

  const auto* settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kKeyboardSettingTopRowAreFKeys));
  EXPECT_EQ(
      !kDefaultTopRowAreFKeys,
      settings_dict->FindBool(prefs::kKeyboardSettingTopRowAreFKeys).value());
}

TEST_F(KeyboardPrefHandlerTest, SixPackKeyRemappingsFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  CallInitializeKeyboardSettings(kKeyboardKey1);
  const auto* settings_dict = GetSettingsDictForDeviceKey(kKeyboardKey1);
  EXPECT_FALSE(
      settings_dict->contains(prefs::kKeyboardSettingSixPackKeyRemappings));
}

TEST_F(KeyboardPrefHandlerTest, SixPackKeyRemappings_Default) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAltClickAndSixPackCustomization);

  auto settings = CallInitializeKeyboardSettings(kKeyboardKey1);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kSearch,
            settings->six_pack_key_remappings->page_up);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kSearch,
            settings->six_pack_key_remappings->page_down);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kSearch,
            settings->six_pack_key_remappings->home);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kSearch,
            settings->six_pack_key_remappings->end);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kSearch,
            settings->six_pack_key_remappings->del);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kSearch,
            settings->six_pack_key_remappings->insert);
}

TEST_F(KeyboardPrefHandlerTest, SixPackKeyRemappings_Group) {
  CallInitializeKeyboardSettings(kKeyboardKey1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAltClickAndSixPackCustomization);

  pref_service_->SetUserPref(prefs::kKeyEventRemappedToSixPackPageUp,
                             base::Value(15));
  pref_service_->SetUserPref(prefs::kKeyEventRemappedToSixPackPageDown,
                             base::Value(0));
  // `kAlt` is the expected default since the `PageUp` pref has a positive
  // value and the PageDown pref rewrite is either unused or usage is split
  // equally between both the Search and Alt rewrites.
  auto settings = CallInitializeKeyboardSettings(kKeyboardKey1);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kAlt,
            settings->six_pack_key_remappings->page_up);
}

TEST_F(KeyboardPrefHandlerTest, SixPackKeyRemappings_RetrieveSettings) {
  CallInitializeKeyboardSettings(kKeyboardKey1);
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kAltClickAndSixPackCustomization);

  pref_service_->SetUserPref(prefs::kKeyEventRemappedToSixPackPageUp,
                             base::Value(15));
  auto settings = CallInitializeKeyboardSettings(kKeyboardKey1);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kAlt,
            settings->six_pack_key_remappings->page_up);
  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);
  EXPECT_TRUE(
      settings_dict->contains(prefs::kKeyboardSettingSixPackKeyRemappings));
  EXPECT_EQ(
      *settings_dict->FindDict(prefs::kKeyboardSettingSixPackKeyRemappings)
           ->FindInt(prefs::kSixPackKeyPageUp),
      static_cast<int>(ui::mojom::SixPackShortcutModifier::kAlt));
  settings = CallInitializeKeyboardSettings(kKeyboardKey1);
  EXPECT_EQ(ui::mojom::SixPackShortcutModifier::kAlt,
            settings->six_pack_key_remappings->page_up);
}

class KeyboardSettingsPrefConversionTest
    : public KeyboardPrefHandlerTest,
      public testing::WithParamInterface<
          std::tuple<std::string, const mojom::KeyboardSettings*>> {
 public:
  KeyboardSettingsPrefConversionTest() = default;
  KeyboardSettingsPrefConversionTest(
      const KeyboardSettingsPrefConversionTest&) = delete;
  KeyboardSettingsPrefConversionTest& operator=(
      const KeyboardSettingsPrefConversionTest&) = delete;
  ~KeyboardSettingsPrefConversionTest() override = default;

  // testing::Test:
  void SetUp() override {
    KeyboardPrefHandlerTest::SetUp();
    std::tie(device_key_, settings_) = GetParam();
  }

 protected:
  std::string device_key_;
  raw_ptr<const mojom::KeyboardSettings, ExperimentalAsh> settings_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    KeyboardSettingsPrefConversionTest,
    testing::Combine(
        testing::Values(kKeyboardKey1, kKeyboardKey2, kKeyboardKey3),
        testing::Values(&kKeyboardSettings1,
                        &kKeyboardSettings2,
                        &kKeyboardSettings3)));

TEST_P(KeyboardSettingsPrefConversionTest, CheckConversion) {
  CallUpdateKeyboardSettings(device_key_, *settings_);

  auto* settings_dict = GetSettingsDictForDeviceKey(device_key_);
  CheckKeyboardSettingsAndDictAreEqual(*settings_, *settings_dict);
}

TEST_P(KeyboardSettingsPrefConversionTest, CheckRoundtripConversion) {
  CallUpdateKeyboardSettings(device_key_, *settings_);

  auto* settings_dict = GetSettingsDictForDeviceKey(device_key_);
  CheckKeyboardSettingsAndDictAreEqual(*settings_, *settings_dict);

  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(device_key_);
  EXPECT_EQ(*settings_, *settings);
}

TEST_P(KeyboardSettingsPrefConversionTest,
       CheckRoundtripConversionWithExtraKey) {
  CallUpdateKeyboardSettings(device_key_, *settings_);

  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  ASSERT_EQ(1u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(device_key_);
  ASSERT_NE(nullptr, settings_dict);

  // Set a fake key to simulate a setting being removed from 1 milestone to
  // the next.
  settings_dict->Set(kDictFakeKey, kDictFakeValue);
  pref_service_->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                         std::move(devices_dict));

  CheckKeyboardSettingsAndDictAreEqual(*settings_, *settings_dict);

  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(device_key_);
  EXPECT_EQ(*settings_, *settings);
}

}  // namespace ash
