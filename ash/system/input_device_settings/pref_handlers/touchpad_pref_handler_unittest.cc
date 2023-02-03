// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

namespace {
const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kTouchpadKey1 = "device_key1";
const std::string kTouchpadKey2 = "device_key2";

const mojom::TouchpadSettings kTouchpadSettingsDefault(
    /*sensitivity=*/kDefaultSensitivity,
    /*reverse_scrolling=*/kDefaultReverseScrolling,
    /*acceleration_enabled=*/kDefaultAccelerationEnabled,
    /*tap_to_click_enabled=*/kDefaultTapToClickEnabled,
    /*three_finger_click_enabled=*/kDefaultThreeFingerClickEnabled,
    /*tap_dragging_enabled=*/kDefaultTapDraggingEnabled,
    /*scroll_sensitivity=*/kDefaultSensitivity,
    /*scroll_acceleration=*/kDefaultScrollAcceleration,
    /*haptic_sensitivity=*/kDefaultHapticSensitivity,
    /*haptic_enabled=*/kDefaultHapticFeedbackEnabled);

const mojom::TouchpadSettings kTouchpadSettings1(
    /*sensitivity=*/1,
    /*reverse_scrolling=*/false,
    /*acceleration_enabled=*/false,
    /*tap_to_click_enabled=*/false,
    /*three_finger_click_enabled=*/false,
    /*tap_dragging_enabled=*/false,
    /*scroll_sensitivity=*/1,
    /*scroll_acceleration=*/false,
    /*haptic_sensitivity=*/1,
    /*haptic_enabled=*/false);

const mojom::TouchpadSettings kTouchpadSettings2(
    /*sensitivity=*/3,
    /*reverse_scrolling=*/false,
    /*acceleration_enabled=*/false,
    /*tap_to_click_enabled=*/false,
    /*three_finger_click_enabled=*/false,
    /*tap_dragging_enabled=*/false,
    /*scroll_sensitivity=*/3,
    /*scroll_acceleration=*/false,
    /*haptic_sensitivity=*/3,
    /*haptic_enabled=*/false);
}  // namespace

class TouchpadPrefHandlerTest : public AshTestBase {
 public:
  TouchpadPrefHandlerTest() = default;
  TouchpadPrefHandlerTest(const TouchpadPrefHandlerTest&) = delete;
  TouchpadPrefHandlerTest& operator=(const TouchpadPrefHandlerTest&) = delete;
  ~TouchpadPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    InitializePrefService();
    pref_handler_ = std::make_unique<TouchpadPrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

  void InitializePrefService() {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kTouchpadDeviceSettingsDictPref);
  }

  void CheckTouchpadSettingsAndDictAreEqual(
      const mojom::TouchpadSettings& settings,
      const base::Value::Dict& settings_dict) {
    const auto sensitivity =
        settings_dict.FindInt(prefs::kTouchpadSettingSensitivity);
    ASSERT_TRUE(sensitivity.has_value());
    EXPECT_EQ(settings.sensitivity, sensitivity);

    const auto reverse_scrolling =
        settings_dict.FindBool(prefs::kTouchpadSettingReverseScrolling);
    ASSERT_TRUE(reverse_scrolling.has_value());
    EXPECT_EQ(settings.reverse_scrolling, reverse_scrolling);

    const auto acceleration_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingAccelerationEnabled);
    ASSERT_TRUE(acceleration_enabled.has_value());
    EXPECT_EQ(settings.acceleration_enabled, acceleration_enabled);

    const auto scroll_sensitivity =
        settings_dict.FindInt(prefs::kTouchpadSettingScrollSensitivity);
    ASSERT_TRUE(scroll_sensitivity.has_value());
    EXPECT_EQ(settings.scroll_sensitivity, scroll_sensitivity);

    const auto scroll_acceleration =
        settings_dict.FindBool(prefs::kTouchpadSettingScrollAcceleration);
    ASSERT_TRUE(scroll_acceleration.has_value());
    EXPECT_EQ(settings.scroll_acceleration, scroll_acceleration);

    const auto tap_to_click_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingTapToClickEnabled);
    ASSERT_TRUE(tap_to_click_enabled.has_value());
    EXPECT_EQ(settings.tap_to_click_enabled, tap_to_click_enabled);

    const auto three_finger_click_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingThreeFingerClickEnabled);
    ASSERT_TRUE(three_finger_click_enabled.has_value());
    EXPECT_EQ(settings.three_finger_click_enabled, three_finger_click_enabled);

    const auto tap_dragging_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingTapDraggingEnabled);
    ASSERT_TRUE(tap_dragging_enabled.has_value());
    EXPECT_EQ(settings.tap_dragging_enabled, tap_dragging_enabled);

    const auto haptic_sensitivity =
        settings_dict.FindInt(prefs::kTouchpadSettingHapticSensitivity);
    ASSERT_TRUE(haptic_sensitivity.has_value());
    EXPECT_EQ(settings.haptic_sensitivity, haptic_sensitivity);

    const auto haptic_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingHapticEnabled);
    ASSERT_TRUE(haptic_enabled.has_value());
    EXPECT_EQ(settings.haptic_enabled, haptic_enabled);
  }

  void CallUpdateTouchpadSettings(const std::string& device_key,
                                  const mojom::TouchpadSettings& settings) {
    mojom::TouchpadPtr touchpad = mojom::Touchpad::New();
    touchpad->settings = settings.Clone();
    touchpad->device_key = device_key;

    pref_handler_->UpdateTouchpadSettings(pref_service_.get(), *touchpad);
  }

  mojom::TouchpadSettingsPtr CallInitializeTouchpadSettings(
      const std::string& device_key) {
    mojom::TouchpadPtr touchpad = mojom::Touchpad::New();
    touchpad->device_key = device_key;

    pref_handler_->InitializeTouchpadSettings(pref_service_.get(),
                                              touchpad.get());
    return std::move(touchpad->settings);
  }

 protected:
  std::unique_ptr<TouchpadPrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(TouchpadPrefHandlerTest, MultipleDevices) {
  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettings1);
  CallUpdateTouchpadSettings(kTouchpadKey2, kTouchpadSettings2);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
  ASSERT_EQ(2u, devices_dict.size());

  auto* settings_dict = devices_dict.FindDict(kTouchpadKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettings1, *settings_dict);

  settings_dict = devices_dict.FindDict(kTouchpadKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettings2, *settings_dict);
}

TEST_F(TouchpadPrefHandlerTest, PreservesOldSettings) {
  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettings1);

  auto devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kTouchpadKey1);
  ASSERT_NE(nullptr, settings_dict);

  // Set a fake key to simulate a setting being removed from 1 milestone to the
  // next.
  settings_dict->Set(kDictFakeKey, kDictFakeValue);
  pref_service_->SetDict(prefs::kTouchpadDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Update the settings again and verify the fake key and value still exist.
  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettings1);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kTouchpadKey1);

  const std::string* value = updated_settings_dict->FindString(kDictFakeKey);
  ASSERT_NE(nullptr, value);
  EXPECT_EQ(kDictFakeValue, *value);
}

TEST_F(TouchpadPrefHandlerTest, UpdateSettings) {
  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettings1);
  CallUpdateTouchpadSettings(kTouchpadKey2, kTouchpadSettings2);

  auto devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kTouchpadKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettings1, *settings_dict);

  settings_dict = devices_dict.FindDict(kTouchpadKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettings2, *settings_dict);

  mojom::TouchpadSettings updated_settings = kTouchpadSettings1;
  updated_settings.reverse_scrolling = !updated_settings.reverse_scrolling;

  // Update the settings again and verify the settings are updated in place.
  CallUpdateTouchpadSettings(kTouchpadKey1, updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kTouchpadKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(updated_settings,
                                       *updated_settings_dict);

  // Verify other device remains unmodified.
  const auto* unchanged_settings_dict =
      updated_devices_dict.FindDict(kTouchpadKey2);
  ASSERT_NE(nullptr, unchanged_settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettings2,
                                       *unchanged_settings_dict);
}

TEST_F(TouchpadPrefHandlerTest, NewSettingAddedRoundTrip) {
  mojom::TouchpadSettings test_settings = kTouchpadSettings1;
  test_settings.reverse_scrolling = !kDefaultReverseScrolling;

  CallUpdateTouchpadSettings(kTouchpadKey1, test_settings);
  auto devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kTouchpadKey1);

  // Remove key from the dict to mock adding a new setting in the future.
  settings_dict->Remove(prefs::kTouchpadSettingReverseScrolling);
  pref_service_->SetDict(prefs::kTouchpadDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Initialize keyboard settings for the device and check that
  // "new settings" match their default values.
  mojom::TouchpadSettingsPtr settings =
      CallInitializeTouchpadSettings(kTouchpadKey1);
  EXPECT_EQ(kDefaultReverseScrolling, settings->reverse_scrolling);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->reverse_scrolling = !kDefaultReverseScrolling;
  EXPECT_EQ(test_settings, *settings);
}

TEST_F(TouchpadPrefHandlerTest, NewTouchpadDefaultSettings) {
  mojom::TouchpadSettingsPtr settings =
      CallInitializeTouchpadSettings(kTouchpadKey1);
  EXPECT_EQ(*settings, kTouchpadSettingsDefault);
  settings = CallInitializeTouchpadSettings(kTouchpadKey2);
  EXPECT_EQ(*settings, kTouchpadSettingsDefault);

  auto devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
  ASSERT_EQ(2u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(kTouchpadKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettingsDefault,
                                       *settings_dict);

  settings_dict = devices_dict.FindDict(kTouchpadKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettingsDefault,
                                       *settings_dict);
}

class TouchpadSettingsPrefConversionTest
    : public TouchpadPrefHandlerTest,
      public testing::WithParamInterface<
          std::tuple<std::string, mojom::TouchpadSettings>> {
 public:
  TouchpadSettingsPrefConversionTest() = default;
  TouchpadSettingsPrefConversionTest(
      const TouchpadSettingsPrefConversionTest&) = delete;
  TouchpadSettingsPrefConversionTest& operator=(
      const TouchpadSettingsPrefConversionTest&) = delete;
  ~TouchpadSettingsPrefConversionTest() override = default;

  // testing::Test:
  void SetUp() override {
    TouchpadPrefHandlerTest::SetUp();
    std::tie(device_key_, settings_) = GetParam();
  }

 protected:
  std::string device_key_;
  mojom::TouchpadSettings settings_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    TouchpadSettingsPrefConversionTest,
    testing::Combine(testing::Values(kTouchpadKey1, kTouchpadKey2),
                     testing::Values(kTouchpadSettings1, kTouchpadSettings2)));

TEST_P(TouchpadSettingsPrefConversionTest, CheckConversion) {
  CallUpdateTouchpadSettings(device_key_, settings_);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
  ASSERT_EQ(1u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(device_key_);
  ASSERT_NE(nullptr, settings_dict);

  CheckTouchpadSettingsAndDictAreEqual(settings_, *settings_dict);
}

}  // namespace ash