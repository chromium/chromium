// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

namespace {
const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kMouseKey1 = "device_key1";
const std::string kMouseKey2 = "device_key2";

const bool kTestSwapRight = false;
const int kTestSensitivity = 2;
const bool kTestReverseScrolling = false;
const bool kTestAccelerationEnabled = false;
const int kTestScrollSensitivity = 3;
const bool kTestScrollAcceleration = false;

const mojom::MouseSettings kMouseSettingsDefault(
    /*swap_right=*/kDefaultSwapRight,
    /*sensitivity=*/kDefaultSensitivity,
    /*reverse_scrolling=*/kDefaultReverseScrolling,
    /*acceleration_enabled=*/kDefaultAccelerationEnabled,
    /*scroll_sensitivity=*/kDefaultSensitivity,
    /*scroll_acceleration=*/kDefaultScrollAcceleration);

const mojom::MouseSettings kMouseSettings1(
    /*swap_right=*/false,
    /*sensitivity=*/1,
    /*reverse_scrolling=*/false,
    /*acceleration_enabled=*/false,
    /*scroll_sensitivity=*/1,
    /*scroll_acceleration=*/false);

const mojom::MouseSettings kMouseSettings2(
    /*swap_right=*/true,
    /*sensitivity=*/10,
    /*reverse_scrolling=*/true,
    /*acceleration_enabled=*/true,
    /*scroll_sensitivity=*/24,
    /*scroll_acceleration=*/true);
}  // namespace

class MousePrefHandlerTest : public AshTestBase {
 public:
  MousePrefHandlerTest() = default;
  MousePrefHandlerTest(const MousePrefHandlerTest&) = delete;
  MousePrefHandlerTest& operator=(const MousePrefHandlerTest&) = delete;
  ~MousePrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    InitializePrefService();
    pref_handler_ = std::make_unique<MousePrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

  void InitializePrefService() {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kMouseDeviceSettingsDictPref);
    // We are using these test constants as a a way to differentiate values
    // retrieved from prefs or default mouse settings.
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kPrimaryMouseButtonRight, kTestSwapRight);
    pref_service_->registry()->RegisterIntegerPref(prefs::kMouseSensitivity,
                                                   kTestSensitivity);
    pref_service_->registry()->RegisterBooleanPref(prefs::kMouseReverseScroll,
                                                   kTestReverseScrolling);
    pref_service_->registry()->RegisterBooleanPref(prefs::kMouseAcceleration,
                                                   kTestAccelerationEnabled);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kMouseScrollSensitivity, kTestScrollSensitivity);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kMouseScrollAcceleration, kTestScrollAcceleration);
  }

  void CheckMouseSettingsAndDictAreEqual(
      const mojom::MouseSettings& settings,
      const base::Value::Dict& settings_dict) {
    const auto swap_right =
        settings_dict.FindBool(prefs::kMouseSettingSwapRight);
    ASSERT_TRUE(swap_right.has_value());
    EXPECT_EQ(settings.swap_right, swap_right);

    const auto sensitivity =
        settings_dict.FindInt(prefs::kMouseSettingSensitivity);
    ASSERT_TRUE(sensitivity.has_value());
    EXPECT_EQ(settings.sensitivity, sensitivity);

    const auto reverse_scrolling =
        settings_dict.FindBool(prefs::kMouseSettingReverseScrolling);
    ASSERT_TRUE(reverse_scrolling.has_value());
    EXPECT_EQ(settings.reverse_scrolling, reverse_scrolling);

    const auto acceleration_enabled =
        settings_dict.FindBool(prefs::kMouseSettingAccelerationEnabled);
    ASSERT_TRUE(acceleration_enabled.has_value());
    EXPECT_EQ(settings.acceleration_enabled, acceleration_enabled);

    const auto scroll_sensitivity =
        settings_dict.FindInt(prefs::kMouseSettingScrollSensitivity);
    ASSERT_TRUE(scroll_sensitivity.has_value());
    EXPECT_EQ(settings.scroll_sensitivity, scroll_sensitivity);

    const auto scroll_acceleration =
        settings_dict.FindBool(prefs::kMouseSettingScrollAcceleration);
    ASSERT_TRUE(scroll_acceleration.has_value());
    EXPECT_EQ(settings.scroll_acceleration, scroll_acceleration);
  }

  void CallUpdateMouseSettings(const std::string& device_key,
                               const mojom::MouseSettings& settings) {
    mojom::MousePtr mouse = mojom::Mouse::New();
    mouse->settings = settings.Clone();
    mouse->device_key = device_key;

    pref_handler_->UpdateMouseSettings(pref_service_.get(), *mouse);
  }

  mojom::MouseSettingsPtr CallInitializeMouseSettings(
      const std::string& device_key) {
    mojom::MousePtr mouse = mojom::Mouse::New();
    mouse->device_key = device_key;

    pref_handler_->InitializeMouseSettings(pref_service_.get(), mouse.get());
    return std::move(mouse->settings);
  }

 protected:
  std::unique_ptr<MousePrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(MousePrefHandlerTest, MultipleDevices) {
  CallUpdateMouseSettings(kMouseKey1, kMouseSettings1);
  CallUpdateMouseSettings(kMouseKey2, kMouseSettings2);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref);
  ASSERT_EQ(2u, devices_dict.size());

  auto* settings_dict = devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettings1, *settings_dict);

  settings_dict = devices_dict.FindDict(kMouseKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettings2, *settings_dict);
}

TEST_F(MousePrefHandlerTest, PreservesOldSettings) {
  CallUpdateMouseSettings(kMouseKey1, kMouseSettings1);

  auto devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, settings_dict);

  // Set a fake key to simulate a setting being removed from 1 milestone to the
  // next.
  settings_dict->Set(kDictFakeKey, kDictFakeValue);
  pref_service_->SetDict(prefs::kMouseDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Update the settings again and verify the fake key and value still exist.
  CallUpdateMouseSettings(kMouseKey1, kMouseSettings1);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* updated_settings_dict = updated_devices_dict.FindDict(kMouseKey1);

  const std::string* value = updated_settings_dict->FindString(kDictFakeKey);
  ASSERT_NE(nullptr, value);
  EXPECT_EQ(kDictFakeValue, *value);
}

TEST_F(MousePrefHandlerTest, UpdateSettings) {
  CallUpdateMouseSettings(kMouseKey1, kMouseSettings1);
  CallUpdateMouseSettings(kMouseKey2, kMouseSettings2);

  auto devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettings1, *settings_dict);

  settings_dict = devices_dict.FindDict(kMouseKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettings2, *settings_dict);

  mojom::MouseSettings updated_settings = kMouseSettings1;
  updated_settings.swap_right = !updated_settings.swap_right;

  // Update the settings again and verify the settings are updated in place.
  CallUpdateMouseSettings(kMouseKey1, updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* updated_settings_dict = updated_devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  CheckMouseSettingsAndDictAreEqual(updated_settings, *updated_settings_dict);

  // Verify other device remains unmodified.
  const auto* unchanged_settings_dict =
      updated_devices_dict.FindDict(kMouseKey2);
  ASSERT_NE(nullptr, unchanged_settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettings2, *unchanged_settings_dict);
}

TEST_F(MousePrefHandlerTest, NewSettingAddedRoundTrip) {
  mojom::MouseSettings test_settings = kMouseSettings1;
  test_settings.swap_right = !kDefaultSwapRight;

  CallUpdateMouseSettings(kMouseKey1, test_settings);
  auto devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kMouseKey1);

  // Remove key from the dict to mock adding a new setting in the future.
  settings_dict->Remove(prefs::kMouseSettingSwapRight);
  pref_service_->SetDict(prefs::kMouseDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Initialize keyboard settings for the device and check that
  // "new settings" matches "test_settings".
  mojom::MouseSettingsPtr settings = CallInitializeMouseSettings(kMouseKey1);
  EXPECT_EQ(kDefaultSwapRight, settings->swap_right);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->swap_right = !kDefaultSwapRight;
  EXPECT_EQ(test_settings, *settings);
}

TEST_F(MousePrefHandlerTest, NewMouseDefaultSettings) {
  mojom::MouseSettingsPtr settings = CallInitializeMouseSettings(kMouseKey1);
  EXPECT_EQ(*settings, kMouseSettingsDefault);
  settings = CallInitializeMouseSettings(kMouseKey2);
  EXPECT_EQ(*settings, kMouseSettingsDefault);

  auto devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  ASSERT_EQ(2u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettingsDefault, *settings_dict);

  settings_dict = devices_dict.FindDict(kMouseKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettingsDefault, *settings_dict);
}

TEST_F(MousePrefHandlerTest, MouseObserveredInTransitionPeriod) {
  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;
  Shell::Get()->input_device_tracker()->OnMouseConnected(mouse);
  // Initialize mouse settings for the device and check that the Test
  // prefs were used as defaults.
  mojom::MouseSettingsPtr settings =
      CallInitializeMouseSettings(mouse.device_key);
  ASSERT_EQ(settings->swap_right, kTestSwapRight);
  ASSERT_EQ(settings->sensitivity, kTestSensitivity);
  ASSERT_EQ(settings->reverse_scrolling, kTestReverseScrolling);
  ASSERT_EQ(settings->acceleration_enabled, kTestAccelerationEnabled);
  ASSERT_EQ(settings->scroll_sensitivity, kTestScrollSensitivity);
  ASSERT_EQ(settings->scroll_acceleration, kTestScrollAcceleration);
}

class MouseSettingsPrefConversionTest
    : public MousePrefHandlerTest,
      public testing::WithParamInterface<
          std::tuple<std::string, mojom::MouseSettings>> {
 public:
  MouseSettingsPrefConversionTest() = default;
  MouseSettingsPrefConversionTest(const MouseSettingsPrefConversionTest&) =
      delete;
  MouseSettingsPrefConversionTest& operator=(
      const MouseSettingsPrefConversionTest&) = delete;
  ~MouseSettingsPrefConversionTest() override = default;

  // testing::Test:
  void SetUp() override {
    MousePrefHandlerTest::SetUp();
    std::tie(device_key_, settings_) = GetParam();
  }

 protected:
  std::string device_key_;
  mojom::MouseSettings settings_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    MouseSettingsPrefConversionTest,
    testing::Combine(testing::Values(kMouseKey1, kMouseKey2),
                     testing::Values(kMouseSettings1, kMouseSettings2)));

TEST_P(MouseSettingsPrefConversionTest, CheckConversion) {
  CallUpdateMouseSettings(device_key_, settings_);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref);
  ASSERT_EQ(1u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(device_key_);
  ASSERT_NE(nullptr, settings_dict);

  CheckMouseSettingsAndDictAreEqual(settings_, *settings_dict);
}

}  // namespace ash
