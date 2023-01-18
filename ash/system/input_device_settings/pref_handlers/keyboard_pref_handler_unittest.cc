// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/keyboard_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
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

const mojom::KeyboardSettings kKeyboardSettings1(
    {},
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

}  // namespace ash
