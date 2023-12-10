// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/mojom/input_device_settings.mojom-forward.h"
#include "ash/system/input_device_settings/pref_handlers/pointing_stick_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/settings_updated_metrics_info.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {
const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kPointingStickKey1 = "device_key1";
const std::string kPointingStickKey2 = "device_key2";

constexpr char kUserEmail[] = "example@email.com";
constexpr char kUserEmail2[] = "example2@email.com";
const AccountId account_id_1 = AccountId::FromUserEmail(kUserEmail);
const AccountId account_id_2 = AccountId::FromUserEmail(kUserEmail2);

const int kTestSensitivity = 2;
const bool kTestSwapRight = false;
const bool kTestAccelerationEnabled = false;

const mojom::PointingStickSettings kPointingStickSettingsDefault(
    /*swap_right=*/kDefaultSwapRight,
    /*sensitivity=*/kDefaultSensitivity,
    /*acceleration_enabled=*/kDefaultAccelerationEnabled);

const mojom::PointingStickSettings kPointingStickSettingsNotDefault(
    /*swap_right=*/!kDefaultSwapRight,
    /*sensitivity=*/1,
    /*acceleration_enabled=*/!kDefaultAccelerationEnabled);

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
    scoped_feature_list_.InitAndEnableFeature(
        features::kInputDeviceSettingsSplit);
    AshTestBase::SetUp();
    InitializePrefService();
    pref_handler_ = std::make_unique<PointingStickPrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

  void InitializePrefService() {
    local_state()->registry()->RegisterBooleanPref(
        prefs::kOwnerPrimaryPointingStickButtonRight, /*default_value=*/false);
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kPointingStickDeviceSettingsDictPref);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kPointingStickInternalSettings);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kPointingStickUpdateSettingsMetricInfo);

    pref_service_->registry()->RegisterIntegerPref(
        prefs::kPointingStickSensitivity, kDefaultSensitivity);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kPrimaryPointingStickButtonRight, kDefaultSwapRight);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kPointingStickAcceleration, kDefaultAccelerationEnabled);

    pref_service_->SetUserPref(prefs::kPointingStickSensitivity,
                               base::Value(kTestSensitivity));
    pref_service_->SetUserPref(prefs::kPrimaryPointingStickButtonRight,
                               base::Value(kTestSwapRight));
    pref_service_->SetUserPref(prefs::kPointingStickAcceleration,
                               base::Value(kTestAccelerationEnabled));
  }

  void CheckPointingStickSettingsAndDictAreEqual(
      const mojom::PointingStickSettings& settings,
      const base::Value::Dict& settings_dict) {
    const auto sensitivity =
        settings_dict.FindInt(prefs::kPointingStickSettingSensitivity);
    if (sensitivity.has_value()) {
      EXPECT_EQ(settings.sensitivity, sensitivity);
    } else {
      EXPECT_EQ(settings.sensitivity, kDefaultSensitivity);
    }

    const auto swap_right =
        settings_dict.FindBool(prefs::kPointingStickSettingSwapRight);
    if (swap_right.has_value()) {
      EXPECT_EQ(settings.swap_right, swap_right);
    } else {
      EXPECT_EQ(settings.swap_right, kDefaultSwapRight);
    }

    const auto acceleration_enabled =
        settings_dict.FindBool(prefs::kPointingStickSettingAcceleration);
    if (acceleration_enabled.has_value()) {
      EXPECT_EQ(settings.acceleration_enabled, acceleration_enabled);
    } else {
      EXPECT_EQ(settings.acceleration_enabled, kDefaultAccelerationEnabled);
    }
  }

  void CheckPointingStickSettingsAreSetToDefaultValues(
      const mojom::PointingStickSettings& settings) {
    EXPECT_EQ(kPointingStickSettingsDefault.swap_right, settings.swap_right);
    EXPECT_EQ(kPointingStickSettingsDefault.sensitivity, settings.sensitivity);
    EXPECT_EQ(kPointingStickSettingsDefault.acceleration_enabled,
              settings.acceleration_enabled);
  }

  void CallUpdatePointingStickSettings(
      const std::string& device_key,
      const mojom::PointingStickSettings& settings,
      bool is_external = true) {
    mojom::PointingStickPtr pointing_stick = mojom::PointingStick::New();
    pointing_stick->settings = settings.Clone();
    pointing_stick->device_key = device_key;
    pointing_stick->is_external = is_external;

    pref_handler_->UpdatePointingStickSettings(pref_service_.get(),
                                               *pointing_stick);
  }

  void CallUpdateLoginScreenPointingStickSettings(
      const AccountId& account_id,
      const std::string& device_key,
      const mojom::PointingStickSettings& settings) {
    mojom::PointingStickPtr pointing_stick = mojom::PointingStick::New();
    pointing_stick->settings = settings.Clone();
    pref_handler_->UpdateLoginScreenPointingStickSettings(
        local_state(), account_id, *pointing_stick);
  }

  mojom::PointingStickSettingsPtr CallInitializePointingStickSettings(
      const std::string& device_key,
      bool is_external = true) {
    mojom::PointingStickPtr pointing_stick = mojom::PointingStick::New();
    pointing_stick->device_key = device_key;
    pointing_stick->is_external = is_external;

    pref_handler_->InitializePointingStickSettings(pref_service_.get(),
                                                   pointing_stick.get());
    return std::move(pointing_stick->settings);
  }

  mojom::PointingStickSettingsPtr
  CallInitializeLoginScreenPointingStickSettings(
      const AccountId& account_id,
      const mojom::PointingStick& pointing_stick) {
    const auto pointing_stick_ptr = pointing_stick.Clone();

    pref_handler_->InitializeLoginScreenPointingStickSettings(
        local_state(), account_id, pointing_stick_ptr.get());
    return std::move(pointing_stick_ptr->settings);
  }

  const base::Value::Dict* GetSettingsDict(const std::string& device_key,
                                           bool is_external = true) {
    if (!is_external) {
      return &pref_service_->GetDict(prefs::kPointingStickInternalSettings);
    }

    const auto& devices_dict =
        pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
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
        account_id, prefs::kPointingStickLoginScreenInternalSettingsPref);
    return dict && dict->is_dict();
  }

  bool HasExternalLoginScreenSettingsDict(AccountId account_id) {
    const auto* dict = known_user().FindPath(
        account_id, prefs::kPointingStickLoginScreenExternalSettingsPref);
    return dict && dict->is_dict();
  }

  base::Value::Dict GetInternalLoginScreenSettingsDict(AccountId account_id) {
    return known_user()
        .FindPath(account_id,
                  prefs::kPointingStickLoginScreenInternalSettingsPref)
        ->GetDict()
        .Clone();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<PointingStickPrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(PointingStickPrefHandlerTest,
       InitializeLoginScreenPointingStickSettings) {
  mojom::PointingStick pointing_stick;
  pointing_stick.device_key = kPointingStickKey1;
  pointing_stick.is_external = false;
  mojom::PointingStickSettingsPtr settings =
      CallInitializeLoginScreenPointingStickSettings(account_id_1,
                                                     pointing_stick);

  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
  CheckPointingStickSettingsAreSetToDefaultValues(*settings);
}

TEST_F(PointingStickPrefHandlerTest, UpdateLoginScreenPointingStickSettings) {
  mojom::PointingStick PointingStick;
  PointingStick.device_key = kPointingStickKey1;
  PointingStick.is_external = false;
  mojom::PointingStickSettingsPtr settings =
      CallInitializeLoginScreenPointingStickSettings(account_id_1,
                                                     PointingStick);
  mojom::PointingStickSettings updated_settings = *settings;
  updated_settings.swap_right = !updated_settings.swap_right;
  updated_settings.acceleration_enabled =
      !updated_settings.acceleration_enabled;
  CallUpdateLoginScreenPointingStickSettings(account_id_1, kPointingStickKey1,
                                             updated_settings);
  const auto& updated_settings_dict =
      GetInternalLoginScreenSettingsDict(account_id_1);
  CheckPointingStickSettingsAndDictAreEqual(updated_settings,
                                            updated_settings_dict);
}

TEST_F(PointingStickPrefHandlerTest,
       LoginScreenPrefsNotPersistedWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::PointingStick pointing_stick1;
  pointing_stick1.device_key = kPointingStickKey1;
  pointing_stick1.is_external = false;
  mojom::PointingStick pointing_stick2;
  pointing_stick2.device_key = kPointingStickKey2;
  pointing_stick2.is_external = true;
  CallInitializeLoginScreenPointingStickSettings(account_id_1, pointing_stick1);
  CallInitializeLoginScreenPointingStickSettings(account_id_1, pointing_stick2);
  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
  EXPECT_FALSE(HasExternalLoginScreenSettingsDict(account_id_1));
}

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

TEST_F(PointingStickPrefHandlerTest, LastUpdated) {
  CallUpdatePointingStickSettings(kPointingStickKey1, kPointingStickSettings1,
                                  /*is_external=*/true);
  auto devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
          .Clone();
  auto* settings_dict = devices_dict.FindDict(kPointingStickKey1);
  ASSERT_NE(nullptr, settings_dict);
  auto* time_stamp1 = settings_dict->Find(prefs::kLastUpdatedKey);
  ASSERT_NE(nullptr, time_stamp1);

  mojom::PointingStickSettingsPtr updated_settings =
      kPointingStickSettings1.Clone();
  updated_settings->swap_right = !updated_settings->swap_right;
  CallUpdatePointingStickSettings(kPointingStickKey1, *updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kPointingStickKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  auto* updated_time_stamp1 =
      updated_settings_dict->Find(prefs::kLastUpdatedKey);
  ASSERT_NE(nullptr, updated_time_stamp1);
  ASSERT_NE(time_stamp1, updated_time_stamp1);
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

TEST_F(PointingStickPrefHandlerTest, UpdateSettingsInternal) {
  CallUpdatePointingStickSettings(kPointingStickKey1, kPointingStickSettings1,
                                  /*is_external=*/false);

  const auto& settings_dict =
      pref_service_->GetDict(prefs::kPointingStickInternalSettings);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettings1,
                                            settings_dict);

  mojom::PointingStickSettings updated_settings = kPointingStickSettings1;
  updated_settings.swap_right = !updated_settings.swap_right;

  // Update the settings again and verify the settings are updated in place.
  CallUpdatePointingStickSettings(kPointingStickKey1, updated_settings,
                                  /*is_external=*/false);

  const auto& updated_settings_dict =
      pref_service_->GetDict(prefs::kPointingStickInternalSettings);
  CheckPointingStickSettingsAndDictAreEqual(updated_settings,
                                            updated_settings_dict);
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

TEST_F(PointingStickPrefHandlerTest, NewSettingAddedRoundTripInternal) {
  mojom::PointingStickSettings test_settings = kPointingStickSettings1;
  test_settings.swap_right = !kDefaultSwapRight;

  CallUpdatePointingStickSettings(kPointingStickKey1, test_settings,
                                  /*is_external=*/false);
  auto settings_dict =
      pref_service_->GetDict(prefs::kPointingStickInternalSettings).Clone();

  // Remove key from the dict to mock adding a new setting in the future.
  settings_dict.Remove(prefs::kPointingStickSettingSwapRight);
  pref_service_->SetDict(prefs::kPointingStickInternalSettings,
                         std::move(settings_dict));

  // Initialize PointingStick settings for the device and check that
  // "new settings" match their default values.
  mojom::PointingStickSettingsPtr settings =
      CallInitializePointingStickSettings(kPointingStickKey1,
                                          /*is_external=*/false);
  EXPECT_EQ(kDefaultReverseScrolling, settings->swap_right);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->swap_right = !kDefaultReverseScrolling;
  EXPECT_EQ(test_settings, *settings);
}

TEST_F(PointingStickPrefHandlerTest, DefaultSettingsWhenPrefServiceNull) {
  mojom::PointingStick pointing_stick;
  pointing_stick.device_key = kPointingStickKey1;
  pref_handler_->InitializePointingStickSettings(nullptr, &pointing_stick);
  EXPECT_EQ(kPointingStickSettingsDefault, *pointing_stick.settings);
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

TEST_F(PointingStickPrefHandlerTest, NewPointingStickDefaultSettingsInternal) {
  mojom::PointingStickSettingsPtr settings =
      CallInitializePointingStickSettings(kPointingStickKey1,
                                          /*is_external=*/false);
  EXPECT_EQ(*settings, kPointingStickSettingsDefault);

  auto& settings_dict =
      pref_service_->GetDict(prefs::kPointingStickInternalSettings);
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettingsDefault,
                                            settings_dict);
}

TEST_F(PointingStickPrefHandlerTest,
       PointingStickObserveredInTransitionPeriod) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::PointingStick pointing_stick;
  pointing_stick.device_key = kPointingStickKey1;
  Shell::Get()->input_device_tracker()->OnPointingStickConnected(
      pointing_stick);
  // Initialize PointingStick settings for the device and check that the global
  // prefs were used as defaults.
  mojom::PointingStickSettingsPtr settings =
      CallInitializePointingStickSettings(pointing_stick.device_key);
  ASSERT_EQ(settings->sensitivity, kTestSensitivity);
  ASSERT_EQ(settings->swap_right, kTestSwapRight);
  ASSERT_EQ(settings->acceleration_enabled, kTestAccelerationEnabled);
}

TEST_F(PointingStickPrefHandlerTest,
       TransitionPeriodSettingsPersistedWhenUserChosen) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::PointingStick pointing_stick;
  pointing_stick.device_key = kPointingStickKey1;
  Shell::Get()->input_device_tracker()->OnPointingStickConnected(
      pointing_stick);

  pref_service_->SetUserPref(prefs::kPointingStickSensitivity,
                             base::Value(kDefaultSensitivity));
  pref_service_->SetUserPref(prefs::kPrimaryPointingStickButtonRight,
                             base::Value(kDefaultSwapRight));
  pref_service_->SetUserPref(prefs::kPointingStickAcceleration,
                             base::Value(kDefaultAccelerationEnabled));
  mojom::PointingStickSettingsPtr settings =
      CallInitializePointingStickSettings(pointing_stick.device_key);
  EXPECT_EQ(kPointingStickSettingsDefault, *settings);

  const auto* settings_dict = GetSettingsDict(pointing_stick.device_key);
  EXPECT_TRUE(settings_dict->contains(prefs::kPointingStickSettingSensitivity));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kPointingStickSettingAcceleration));
  EXPECT_TRUE(settings_dict->contains(prefs::kPointingStickSettingSwapRight));
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettingsDefault,
                                            *settings_dict);
}

TEST_F(PointingStickPrefHandlerTest, DefaultNotPersistedUntilUpdated) {
  CallUpdatePointingStickSettings(kPointingStickKey1,
                                  kPointingStickSettingsDefault);

  const auto* settings_dict = GetSettingsDict(kPointingStickKey1);
  EXPECT_FALSE(
      settings_dict->contains(prefs::kPointingStickSettingSensitivity));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kPointingStickSettingAcceleration));
  EXPECT_FALSE(settings_dict->contains(prefs::kPointingStickSettingSwapRight));
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettingsDefault,
                                            *settings_dict);

  CallUpdatePointingStickSettings(kPointingStickKey1,
                                  kPointingStickSettingsNotDefault);
  settings_dict = GetSettingsDict(kPointingStickKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kPointingStickSettingSensitivity));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kPointingStickSettingAcceleration));
  EXPECT_TRUE(settings_dict->contains(prefs::kPointingStickSettingSwapRight));
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettingsNotDefault,
                                            *settings_dict);

  CallUpdatePointingStickSettings(kPointingStickKey1,
                                  kPointingStickSettingsDefault);
  settings_dict = GetSettingsDict(kPointingStickKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kPointingStickSettingSensitivity));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kPointingStickSettingAcceleration));
  EXPECT_TRUE(settings_dict->contains(prefs::kPointingStickSettingSwapRight));
  CheckPointingStickSettingsAndDictAreEqual(kPointingStickSettingsDefault,
                                            *settings_dict);
}

TEST_F(PointingStickPrefHandlerTest, SettingsUpdateMetricTest) {
  const auto settings1 =
      CallInitializePointingStickSettings(kPointingStickKey1);

  // When its the first device of the type the category should be kFirstEver.
  {
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kPointingStickUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kPointingStickKey1));

    auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(
        *metric_dict.FindDict(kPointingStickKey1));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kFirstEver,
              metrics_info->category());
  }

  // When its taken from synced prefs on a different device, category should
  // match.
  {
    auto devices_dict =
        pref_service_->GetDict(prefs::kPointingStickDeviceSettingsDictPref)
            .Clone();
    devices_dict.Set(kPointingStickKey2, base::Value::Dict());
    pref_service_->SetDict(prefs::kPointingStickDeviceSettingsDictPref,
                           std::move(devices_dict));

    CallInitializePointingStickSettings(kPointingStickKey2);
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kPointingStickUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kPointingStickKey2));

    auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(
        *metric_dict.FindDict(kPointingStickKey2));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kSynced,
              metrics_info->category());
  }
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

TEST_P(PointingStickSettingsPrefConversionTest, CheckConversionInternal) {
  CallUpdatePointingStickSettings(device_key_, settings_,
                                  /*is_external=*/false);

  const auto* settings_dict =
      GetSettingsDict(device_key_, /*is_external=*/false);
  CheckPointingStickSettingsAndDictAreEqual(settings_, *settings_dict);
}

}  // namespace ash
