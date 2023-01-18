// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/mojom/input_device_settings.mojom-shared.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "base/containers/contains.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

namespace {
const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kKeyboardKey1 = "device_key1";
const std::string kKeyboardKey2 = "device_key2";
const std::string kKeyboardKey3 = "device_key3";

const mojom::KeyboardSettings kKeyboardSettingsDefault(
    /*modifier_remappings=*/{},
    /*top_row_are_fkeys=*/kDefaultTopRowAreFKeys,
    /*suppress_meta_fkey_rewrites=*/kDefaultSuppressMetaFKeyRewrites,
    /*auto_repeat_enabled=*/kDefaultAutoRepeatEnabled,
    /*auto_repeat_delay=*/kDefaultAutoRepeatDelay,
    /*auto_repeat_interval=*/kDefaultAutoRepeatInterval);

const mojom::KeyboardSettings kKeyboardSettings1(
    /*modifier_remappings=*/{},
    /*top_row_are_fkeys=*/false,
    /*suppress_meta_fkey_rewrites=*/false,
    /*auto_repeat_enabled=*/false,
    /*auto_repeat_delay=*/base::Milliseconds(2000),
    /*auto_repeat_interval=*/base::Milliseconds(500));

const mojom::KeyboardSettings kKeyboardSettings2(
    /*modifier_remappings=*/{{mojom::ModifierKey::kControl,
                              mojom::ModifierKey::kAlt},
                             {mojom::ModifierKey::kAssistant,
                              mojom::ModifierKey::kVoid}},
    /*top_row_are_fkeys=*/true,
    /*suppress_meta_fkey_rewrites=*/true,
    /*auto_repeat_enabled=*/true,
    /*auto_repeat_delay=*/base::Milliseconds(100),
    /*auto_repeat_interval=*/base::Milliseconds(100));

const mojom::KeyboardSettings kKeyboardSettings3(
    /*modifier_remappings=*/{{mojom::ModifierKey::kAlt,
                              mojom::ModifierKey::kCapsLock},
                             {mojom::ModifierKey::kAssistant,
                              mojom::ModifierKey::kVoid},
                             {mojom::ModifierKey::kBackspace,
                              mojom::ModifierKey::kEscape},
                             {mojom::ModifierKey::kControl,
                              mojom::ModifierKey::kAssistant}},
    /*top_row_are_fkeys=*/true,
    /*suppress_meta_fkey_rewrites=*/false,
    /*auto_repeat_enabled=*/true,
    /*auto_repeat_delay=*/base::Milliseconds(5000),
    /*auto_repeat_interval=*/base::Milliseconds(3000));
}  // namespace

class KeyboardPrefHandlerTest : public AshTestBase {
 public:
  KeyboardPrefHandlerTest() = default;
  KeyboardPrefHandlerTest(const KeyboardPrefHandlerTest&) = delete;
  KeyboardPrefHandlerTest& operator=(const KeyboardPrefHandlerTest&) = delete;
  ~KeyboardPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
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
  }

  void CheckKeyboardSettingsAndDictAreEqual(
      const mojom::KeyboardSettings& settings,
      const base::Value::Dict& settings_dict) {
    auto auto_repeat_delay =
        settings_dict.FindInt(prefs::kKeyboardSettingAutoRepeatDelay);
    ASSERT_TRUE(auto_repeat_delay.has_value());
    EXPECT_EQ(static_cast<int>(settings.auto_repeat_delay.InMilliseconds()),
              auto_repeat_delay);

    auto auto_repeat_interval =
        settings_dict.FindInt(prefs::kKeyboardSettingAutoRepeatInterval);
    ASSERT_TRUE(auto_repeat_interval.has_value());
    EXPECT_EQ(static_cast<int>(settings.auto_repeat_interval.InMilliseconds()),
              auto_repeat_interval);

    auto auto_repeat_enabled =
        settings_dict.FindBool(prefs::kKeyboardSettingAutoRepeatEnabled);
    ASSERT_TRUE(auto_repeat_enabled.has_value());
    EXPECT_EQ(settings.auto_repeat_enabled, auto_repeat_enabled);

    auto suppress_meta_fkey_rewrites =
        settings_dict.FindBool(prefs::kKeyboardSettingSuppressMetaFKeyRewrites);
    ASSERT_TRUE(suppress_meta_fkey_rewrites.has_value());
    EXPECT_EQ(settings.suppress_meta_fkey_rewrites,
              suppress_meta_fkey_rewrites);

    auto top_row_are_fkeys =
        settings_dict.FindBool(prefs::kKeyboardSettingTopRowAreFKeys);
    ASSERT_TRUE(top_row_are_fkeys.has_value());
    EXPECT_EQ(settings.top_row_are_fkeys, top_row_are_fkeys);

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

  void CallUpdateKeyboardSettings(const std::string& device_key,
                                  const mojom::KeyboardSettings& settings) {
    mojom::KeyboardPtr keyboard = mojom::Keyboard::New();
    keyboard->settings = settings.Clone();
    keyboard->device_key = device_key;

    pref_handler_->UpdateKeyboardSettings(pref_service_.get(), *keyboard);
  }

  mojom::KeyboardSettingsPtr CallInitializeKeyboardSettings(
      const std::string& device_key) {
    mojom::KeyboardPtr keyboard = mojom::Keyboard::New();
    keyboard->device_key = device_key;

    pref_handler_->InitializeKeyboardSettings(pref_service_.get(),
                                              keyboard.get());
    return std::move(keyboard->settings);
  }

 protected:
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

  mojom::KeyboardSettings updated_settings = kKeyboardSettings1;
  updated_settings.modifier_remappings = {
      {mojom::ModifierKey::kAlt, mojom::ModifierKey::kControl}};
  updated_settings.auto_repeat_enabled = !updated_settings.auto_repeat_enabled;
  updated_settings.suppress_meta_fkey_rewrites =
      !updated_settings.suppress_meta_fkey_rewrites;
  updated_settings.top_row_are_fkeys = !updated_settings.top_row_are_fkeys;

  // Update the settings again and verify the settings are updated in place.
  CallUpdateKeyboardSettings(kKeyboardKey1, updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kKeyboardKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(updated_settings,
                                       *updated_settings_dict);

  // Verify other device remains unmodified.
  const auto* unchanged_settings_dict =
      updated_devices_dict.FindDict(kKeyboardKey2);
  ASSERT_NE(nullptr, unchanged_settings_dict);
  CheckKeyboardSettingsAndDictAreEqual(kKeyboardSettings2,
                                       *unchanged_settings_dict);
}

TEST_F(KeyboardPrefHandlerTest, NewSettingAddedRoundTrip) {
  mojom::KeyboardSettings test_settings = kKeyboardSettings1;
  test_settings.auto_repeat_enabled = !kDefaultAutoRepeatEnabled;
  test_settings.suppress_meta_fkey_rewrites = !kDefaultSuppressMetaFKeyRewrites;

  CallUpdateKeyboardSettings(kKeyboardKey1, test_settings);
  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);

  // Remove keys from the dict to mock adding a new setting in the future.
  settings_dict->Remove(prefs::kKeyboardSettingAutoRepeatEnabled);
  settings_dict->Remove(prefs::kKeyboardSettingSuppressMetaFKeyRewrites);
  pref_service_->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Initialize keyboard settings for the device and check that
  // "new settings" match their default values.
  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(kKeyboardKey1);
  EXPECT_EQ(kDefaultAutoRepeatEnabled, settings->auto_repeat_enabled);
  EXPECT_EQ(kDefaultSuppressMetaFKeyRewrites,
            settings->suppress_meta_fkey_rewrites);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->auto_repeat_enabled = !kDefaultAutoRepeatEnabled;
  settings->suppress_meta_fkey_rewrites = !kDefaultSuppressMetaFKeyRewrites;
  EXPECT_EQ(test_settings, *settings);
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

TEST_F(KeyboardPrefHandlerTest, InvalidModifierRemappings) {
  CallUpdateKeyboardSettings(kKeyboardKey1, kKeyboardSettings1);
  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kKeyboardKey1);

  base::Value::Dict invalid_modifier_remappings;
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(mojom::ModifierKey::kMaxValue) + 1),
      static_cast<int>(mojom::ModifierKey::kControl));
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(mojom::ModifierKey::kMinValue) - 1),
      static_cast<int>(mojom::ModifierKey::kControl));
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(mojom::ModifierKey::kControl)),
      static_cast<int>(mojom::ModifierKey::kMaxValue) + 1);
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(mojom::ModifierKey::kAlt)),
      static_cast<int>(mojom::ModifierKey::kMinValue) - 1);

  // Set 1 valid modifier remapping to check that it skips invalid remappings,
  // and keeps the valid.
  invalid_modifier_remappings.Set(
      base::NumberToString(static_cast<int>(mojom::ModifierKey::kAlt)),
      static_cast<int>(mojom::ModifierKey::kControl));

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
  ASSERT_TRUE(
      base::Contains(settings->modifier_remappings, mojom::ModifierKey::kAlt));
  EXPECT_EQ(mojom::ModifierKey::kControl,
            settings->modifier_remappings[mojom::ModifierKey::kAlt]);

  // Saved prefs should not be modified and should be left as they were in their
  // invalid state.
  devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  settings_dict = devices_dict.FindDict(kKeyboardKey1);
  auto* modifier_remappings_dict =
      settings_dict->FindDict(prefs::kKeyboardSettingModifierRemappings);
  ASSERT_NE(nullptr, modifier_remappings_dict);
  EXPECT_EQ(invalid_modifier_remappings, *modifier_remappings_dict);
}

class KeyboardSettingsPrefConversionTest
    : public KeyboardPrefHandlerTest,
      public testing::WithParamInterface<
          std::tuple<std::string, mojom::KeyboardSettings>> {
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
  mojom::KeyboardSettings settings_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    KeyboardSettingsPrefConversionTest,
    testing::Combine(
        testing::Values(kKeyboardKey1, kKeyboardKey2, kKeyboardKey3),
        testing::Values(kKeyboardSettings1,
                        kKeyboardSettings2,
                        kKeyboardSettings3)));

TEST_P(KeyboardSettingsPrefConversionTest, CheckConversion) {
  CallUpdateKeyboardSettings(device_key_, settings_);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  ASSERT_EQ(1u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(device_key_);
  ASSERT_NE(nullptr, settings_dict);

  CheckKeyboardSettingsAndDictAreEqual(settings_, *settings_dict);
}

TEST_P(KeyboardSettingsPrefConversionTest, CheckRoundtripConversion) {
  CallUpdateKeyboardSettings(device_key_, settings_);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref);
  ASSERT_EQ(1u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(device_key_);
  ASSERT_NE(nullptr, settings_dict);

  CheckKeyboardSettingsAndDictAreEqual(settings_, *settings_dict);

  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(device_key_);
  EXPECT_EQ(settings_, *settings);
}

TEST_P(KeyboardSettingsPrefConversionTest,
       CheckRoundtripConversionWithExtraKey) {
  CallUpdateKeyboardSettings(device_key_, settings_);

  auto devices_dict =
      pref_service_->GetDict(prefs::kKeyboardDeviceSettingsDictPref).Clone();
  ASSERT_EQ(1u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(device_key_);
  ASSERT_NE(nullptr, settings_dict);

  // Set a fake key to simulate a setting being removed from 1 milestone to the
  // next.
  settings_dict->Set(kDictFakeKey, kDictFakeValue);
  pref_service_->SetDict(prefs::kKeyboardDeviceSettingsDictPref,
                         std::move(devices_dict));

  CheckKeyboardSettingsAndDictAreEqual(settings_, *settings_dict);

  mojom::KeyboardSettingsPtr settings =
      CallInitializeKeyboardSettings(device_key_);
  EXPECT_EQ(settings_, *settings);
}

}  // namespace ash
