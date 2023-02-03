// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"

#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/test/ash_test_base.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"

namespace ash {

namespace {
const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kPointingStickKey1 = "device_key1";
const std::string kPointingStickKey2 = "device_key2";

const mojom::PointingStickSettings kPointingStickSettingsDefault(
    /*swap_right=*/kDefaultSwapRight,
    /*sensitivity=*/kDefaultSensitivity,
    /*acceleration_enabled=*/kDefaultAccelerationEnabled);

const mojom::PointingStickSettings kPointingStickSettings1(
    /*swap_right=*/true,
    /*sensitivity=*/1,
    /*acceleration_enabled=*/false);

const mojom::PointingStickSettings kPointingStickSettings2(
    /*swap_right=*/false,
    /*sensitivity=*/3,
    /*acceleration_enabled=*/true);

}  // namespace

class PointingStickPrefHandlerTest : public AshTestBase {
 public:
  PointingStickPrefHandlerTest() = default;
  PointingStickPrefHandlerTest(const PointingStickPrefHandlerTest&) = delete;
  PointingStickPrefHandlerTest& operator=(const PointingStickPrefHandlerTest&) =
      delete;
  ~PointingStickPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    AshTestBase::SetUp();
    InitializePrefService();
    pref_handler_ = std::make_unique<PointingStickPrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

  void InitializePrefService() {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kPointingStickDeviceSettingsDictPref);
  }

  void CheckPointingStickSettingsAndDictAreEqual(
      const mojom::PointingStickSettings& settings,
      const base::Value::Dict& settings_dict) {
    const auto sensitivity =
        settings_dict.FindInt(prefs::kPointingStickSettingSensitivity);
    ASSERT_TRUE(sensitivity.has_value());
    EXPECT_EQ(settings.sensitivity, sensitivity);

    const auto swap_right =
        settings_dict.FindBool(prefs::kPointingStickSettingSwapRight);
    ASSERT_TRUE(swap_right.has_value());
    EXPECT_EQ(settings.swap_right, swap_right);

    const auto acceleration_enabled =
        settings_dict.FindBool(prefs::kPointingStickSettingAcceleration);
    ASSERT_TRUE(acceleration_enabled.has_value());
    EXPECT_EQ(settings.acceleration_enabled, acceleration_enabled);
  }

  void CallUpdatePointingStickSettings(
      const std::string& device_key,
      const mojom::PointingStickSettings& settings) {
    mojom::PointingStickPtr pointing_stick = mojom::PointingStick::New();
    pointing_stick->settings = settings.Clone();
    pointing_stick->device_key = device_key;

    pref_handler_->UpdatePointingStickSettings(pref_service_.get(),
                                               *pointing_stick);
  }

  mojom::PointingStickSettingsPtr CallInitializePointingStickSettings(
      const std::string& device_key) {
    mojom::PointingStickPtr pointing_stick = mojom::PointingStick::New();
    pointing_stick->device_key = device_key;

    pref_handler_->InitializePointingStickSettings(pref_service_.get(),
                                                   pointing_stick.get());
    return std::move(pointing_stick->settings);
  }

 protected:
  std::unique_ptr<PointingStickPrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(PointingStickPrefHandlerTest, MultipleDevices) {
  CallUpdatePointingStickSettings(kPointingStickKey1, kPointingStickSettings1);
  CallUpdatePointingStickSettings(kPointingStickKey2, kPointingStickSettings2);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
  ASSERT_EQ(2u, devices_dict.size());

  auto* settings_dict = devices_dict.FindDict(kPointingStickKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettings1,
                                            *settings_dict);

  settings_dict = devices_dict.FindDict(kPointingStickKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettings2,
                                            *settings_dict);
}

TEST_F(PointingStickPrefHandlerTest, PreservesOldSettings) {
  CallUpdatePointingStickSettings(kPointingStickKey1, kPointingStickSettings1);

  auto devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();
  auto* settings_dict = devices_dict.FindDict(kPointingStickKey1);
  ASSERT_NE(nullptr, settings_dict);

  // Set a fake key to simulate a setting being removed from 1 milestone to the
  // next.
  settings_dict->Set(kDictFakeKey, kDictFakeValue);
  pref_service_->SetDict(prefs::kPointingStickDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Update the settings again and verify the fake key and value still exist.
  CallUpdatePointingStickSettings(kPointingStickKey1, kPointingStickSettings1);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kPointingStickKey1);

  const std::string* value = updated_settings_dict->FindString(kDictFakeKey);
  ASSERT_NE(nullptr, value);
  EXPECT_EQ(kDictFakeValue, *value);
}

TEST_F(PointingStickPrefHandlerTest, UpdateSettings) {
  CallUpdatePointingStickSettings(kPointingStickKey1, kPointingStickSettings1);
  CallUpdatePointingStickSettings(kPointingStickKey2, kPointingStickSettings2);

  auto devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();
  auto* settings_dict = devices_dict.FindDict(kPointingStickKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettings1,
                                            *settings_dict);

  settings_dict = devices_dict.FindDict(kPointingStickKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettings2,
                                            *settings_dict);

  mojom::PointingStickSettings updated_settings = kPointingStickSettings1;
  updated_settings.swap_right = !updated_settings.swap_right;

  // Update the settings again and verify the settings are updated in place.
  CallUpdatePointingStickSettings(kPointingStickKey1, updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kPointingStickKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(updated_settings,
                                            *updated_settings_dict);

  // Verify other device remains unmodified.
  const auto* unchanged_settings_dict =
      updated_devices_dict.FindDict(kPointingStickKey2);
  ASSERT_NE(nullptr, unchanged_settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettings2,
                                            *unchanged_settings_dict);
}

TEST_F(PointingStickPrefHandlerTest, NewSettingAddedRoundTrip) {
  mojom::PointingStickSettings test_settings = kPointingStickSettings1;
  test_settings.swap_right = !kDefaultSwapRight;

  CallUpdatePointingStickSettings(kPointingStickKey1, test_settings);
  auto devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();
  auto* settings_dict = devices_dict.FindDict(kPointingStickKey1);

  // Remove key from the dict to mock adding a new setting in the future.
  settings_dict->Remove(prefs::kPointingStickSettingSwapRight);
  pref_service_->SetDict(prefs::kPointingStickDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Initialize keyboard settings for the device and check that
  // "new settings" match their default values.
  mojom::PointingStickSettingsPtr settings =
      CallInitializePointingStickSettings(kPointingStickKey1);
  EXPECT_EQ(kDefaultSwapRight, settings->swap_right);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->swap_right = !kDefaultSwapRight;
  EXPECT_EQ(test_settings, *settings);
}

TEST_F(PointingStickPrefHandlerTest, NewPointingStickDefaultSettings) {
  mojom::PointingStickSettingsPtr settings =
      CallInitializePointingStickSettings(kPointingStickKey1);
  EXPECT_EQ(*settings, kPointingStickSettingsDefault);
  settings = CallInitializePointingStickSettings(kPointingStickKey2);
  EXPECT_EQ(*settings, kPointingStickSettingsDefault);

  auto devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();
  ASSERT_EQ(2u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(kPointingStickKey1);
  ASSERT_NE(nullptr, settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettingsDefault,
                                            *settings_dict);

  settings_dict = devices_dict.FindDict(kPointingStickKey2);
  ASSERT_NE(nullptr, settings_dict);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettingsDefault,
                                            *settings_dict);
}

class PointingStickSettingsPrefConversionTest
    : public PointingStickPrefHandlerTest,
      public testing::WithParamInterface<
          std::tuple<std::string, mojom::PointingStickSettings>> {
 public:
  PointingStickSettingsPrefConversionTest() = default;
  PointingStickSettingsPrefConversionTest(
      const PointingStickSettingsPrefConversionTest&) = delete;
  PointingStickSettingsPrefConversionTest& operator=(
      const PointingStickSettingsPrefConversionTest&) = delete;
  ~PointingStickSettingsPrefConversionTest() override = default;

  // testing::Test:
  void SetUp() override {
    PointingStickPrefHandlerTest::SetUp();
    std::tie(device_key_, settings_) = GetParam();
  }

 protected:
  std::string device_key_;
  mojom::PointingStickSettings settings_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    PointingStickSettingsPrefConversionTest,
    testing::Combine(testing::Values(kPointingStickKey1, kPointingStickKey2),
                     testing::Values(kPointingStickSettings1,
                                     kPointingStickSettings2)));

TEST_P(PointingStickSettingsPrefConversionTest, CheckConversion) {
  CallUpdatePointingStickSettings(device_key_, settings_);

  const auto& devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
  ASSERT_EQ(1u, devices_dict.size());
  auto* settings_dict = devices_dict.FindDict(device_key_);
  ASSERT_NE(nullptr, settings_dict);

  CheckPointingStickSettingsAndDictAreEqual(settings_, *settings_dict);
}

}  // namespace ash
