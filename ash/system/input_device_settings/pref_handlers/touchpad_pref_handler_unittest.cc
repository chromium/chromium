// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/touchpad_pref_handler_impl.h"

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
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom-shared.h"
#include "ui/events/ash/mojom/simulate_right_click_modifier.mojom.h"

namespace ash {

namespace {
const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kTouchpadKey1 = "device_key1";
const std::string kTouchpadKey2 = "device_key2";
const std::string kTouchpadKey3 = "device_key3";

constexpr char kUserEmail[] = "example@email.com";
constexpr char kUserEmail2[] = "example2@email.com";
const AccountId account_id_1 = AccountId::FromUserEmail(kUserEmail);
const AccountId account_id_2 = AccountId::FromUserEmail(kUserEmail2);

const int kTestSensitivity = 2;
const bool kTestReverseScrolling = false;
const bool kTestAccelerationEnabled = false;
const bool kTestTapToClickEnabled = false;
const bool kTestThreeFingerClickEnabled = false;
const bool kTestTapDraggingEnabled = false;
const bool kTestScrollAcceleration = false;
const int kTestHapticSensitivity = 2;
const bool kTestHapticFeedbackEnabled = false;

const mojom::TouchpadSettings kTouchpadSettingsDefault(
    /*sensitivity=*/kDefaultSensitivity,
    /*reverse_scrolling=*/kDefaultReverseScrolling,
    /*acceleration_enabled=*/kDefaultAccelerationEnabled,
    /*tap_to_click_enabled=*/kDefaultTapToClickEnabled,
    /*three_finger_click_enabled=*/kDefaultThreeFingerClickEnabled,
    /*tap_dragging_enabled=*/kDefaultTapDraggingEnabled,
    /*scroll_sensitivity=*/kDefaultScrollSensitivity,
    /*scroll_acceleration=*/kDefaultScrollAccelerationEnabled,
    /*haptic_sensitivity=*/kDefaultHapticSensitivity,
    /*haptic_enabled=*/kDefaultHapticFeedbackEnabled,
    /*simulate_right_click=*/ui::mojom::SimulateRightClickModifier::kNone);

const mojom::TouchpadSettings kTouchpadSettingsNotDefault(
    /*sensitivity=*/1,
    /*reverse_scrolling=*/!kDefaultReverseScrolling,
    /*acceleration_enabled=*/!kDefaultAccelerationEnabled,
    /*tap_to_click_enabled=*/!kDefaultTapToClickEnabled,
    /*three_finger_click_enabled=*/!kDefaultThreeFingerClickEnabled,
    /*tap_dragging_enabled=*/!kDefaultTapDraggingEnabled,
    /*scroll_sensitivity=*/1,
    /*scroll_acceleration=*/!kDefaultScrollAccelerationEnabled,
    /*haptic_sensitivity=*/1,
    /*haptic_enabled=*/!kDefaultHapticFeedbackEnabled,
    /*simulate_right_click=*/ui::mojom::SimulateRightClickModifier::kNone);

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
    /*haptic_enabled=*/false,
    /*simulate_right_click=*/ui::mojom::SimulateRightClickModifier::kNone);

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
    /*haptic_enabled=*/false,
    /*simulate_right_click=*/ui::mojom::SimulateRightClickModifier::kNone);
}  // namespace

class TouchpadPrefHandlerTest : public AshTestBase {
 public:
  TouchpadPrefHandlerTest() = default;
  TouchpadPrefHandlerTest(const TouchpadPrefHandlerTest&) = delete;
  TouchpadPrefHandlerTest& operator=(const TouchpadPrefHandlerTest&) = delete;
  ~TouchpadPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kInputDeviceSettingsSplit,
         features::kAltClickAndSixPackCustomization},
        {});
    AshTestBase::SetUp();
    InitializePrefService();
    pref_handler_ = std::make_unique<TouchpadPrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

  void InitializePrefService() {
    local_state()->registry()->RegisterBooleanPref(
        prefs::kOwnerTapToClickEnabled, /*default_value=*/false);
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kTouchpadDeviceSettingsDictPref);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kTouchpadInternalSettings);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kTouchpadDefaultSettings);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kTouchpadUpdateSettingsMetricInfo);

    // We are using these test constants as a a way to differentiate values
    // retrieved from prefs or default touchpad settings.
    pref_service_->registry()->RegisterIntegerPref(prefs::kTouchpadSensitivity,
                                                   kDefaultSensitivity);
    pref_service_->registry()->RegisterBooleanPref(prefs::kNaturalScroll,
                                                   kDefaultReverseScrolling);
    pref_service_->registry()->RegisterBooleanPref(prefs::kTouchpadAcceleration,
                                                   kDefaultAccelerationEnabled);
    pref_service_->registry()->RegisterBooleanPref(prefs::kTapToClickEnabled,
                                                   kDefaultTapToClickEnabled);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kEnableTouchpadThreeFingerClick,
        kDefaultThreeFingerClickEnabled);
    pref_service_->registry()->RegisterBooleanPref(prefs::kTapDraggingEnabled,
                                                   kDefaultTapDraggingEnabled);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kTouchpadScrollSensitivity, kDefaultScrollSensitivity);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kTouchpadScrollAcceleration, kDefaultScrollAccelerationEnabled);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kTouchpadHapticClickSensitivity, kDefaultHapticSensitivity);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kTouchpadHapticFeedback, kDefaultHapticFeedbackEnabled);

    pref_service_->registry()->RegisterIntegerPref(
        prefs::kAltEventRemappedToRightClick, 0);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kSearchEventRemappedToRightClick, 0);

    pref_service_->SetUserPref(prefs::kTouchpadSensitivity,
                               base::Value(kTestSensitivity));
    pref_service_->SetUserPref(prefs::kNaturalScroll,
                               base::Value(kTestReverseScrolling));
    pref_service_->SetUserPref(prefs::kTouchpadAcceleration,
                               base::Value(kTestAccelerationEnabled));
    pref_service_->SetUserPref(prefs::kTapToClickEnabled,
                               base::Value(kTestTapToClickEnabled));
    pref_service_->SetUserPref(prefs::kEnableTouchpadThreeFingerClick,
                               base::Value(kTestThreeFingerClickEnabled));
    pref_service_->SetUserPref(prefs::kTapDraggingEnabled,
                               base::Value(kTestTapDraggingEnabled));
    pref_service_->SetUserPref(prefs::kTouchpadScrollSensitivity,
                               base::Value(kTestSensitivity));
    pref_service_->SetUserPref(prefs::kTouchpadScrollAcceleration,
                               base::Value(kTestScrollAcceleration));
    pref_service_->SetUserPref(prefs::kTouchpadHapticClickSensitivity,
                               base::Value(kTestHapticSensitivity));
    pref_service_->SetUserPref(prefs::kTouchpadHapticFeedback,
                               base::Value(kTestHapticFeedbackEnabled));
  }

  void CheckTouchpadSettingsAndDictAreEqual(
      const mojom::TouchpadSettings& settings,
      const base::Value::Dict& settings_dict) {
    const auto sensitivity =
        settings_dict.FindInt(prefs::kTouchpadSettingSensitivity);
    if (sensitivity.has_value()) {
      EXPECT_EQ(settings.sensitivity, sensitivity);
    } else {
      EXPECT_EQ(settings.sensitivity, kDefaultSensitivity);
    }

    const auto reverse_scrolling =
        settings_dict.FindBool(prefs::kTouchpadSettingReverseScrolling);
    if (reverse_scrolling.has_value()) {
      EXPECT_EQ(settings.reverse_scrolling, reverse_scrolling);
    } else {
      EXPECT_EQ(settings.reverse_scrolling, kDefaultReverseScrolling);
    }

    const auto acceleration_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingAccelerationEnabled);
    if (acceleration_enabled.has_value()) {
      EXPECT_EQ(settings.acceleration_enabled, acceleration_enabled);
    } else {
      EXPECT_EQ(settings.acceleration_enabled, kDefaultAccelerationEnabled);
    }

    const auto scroll_sensitivity =
        settings_dict.FindInt(prefs::kTouchpadSettingScrollSensitivity);
    if (scroll_sensitivity.has_value()) {
      EXPECT_EQ(settings.scroll_sensitivity, scroll_sensitivity);
    } else {
      EXPECT_EQ(settings.scroll_sensitivity, kDefaultScrollSensitivity);
    }

    const auto scroll_acceleration =
        settings_dict.FindBool(prefs::kTouchpadSettingScrollAcceleration);
    if (scroll_acceleration.has_value()) {
      EXPECT_EQ(settings.scroll_acceleration, scroll_acceleration);
    } else {
      EXPECT_EQ(settings.scroll_acceleration,
                kDefaultScrollAccelerationEnabled);
    }

    const auto tap_to_click_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingTapToClickEnabled);
    if (tap_to_click_enabled.has_value()) {
      EXPECT_EQ(settings.tap_to_click_enabled, tap_to_click_enabled);
    } else {
      EXPECT_EQ(settings.tap_to_click_enabled, kDefaultTapToClickEnabled);
    }

    const auto three_finger_click_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingThreeFingerClickEnabled);
    if (three_finger_click_enabled.has_value()) {
      EXPECT_EQ(settings.three_finger_click_enabled,
                three_finger_click_enabled);
    } else {
      EXPECT_EQ(settings.three_finger_click_enabled,
                kDefaultThreeFingerClickEnabled);
    }

    const auto tap_dragging_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingTapDraggingEnabled);
    if (tap_dragging_enabled.has_value()) {
      EXPECT_EQ(settings.tap_dragging_enabled, tap_dragging_enabled);
    } else {
      EXPECT_EQ(settings.tap_dragging_enabled, kDefaultTapDraggingEnabled);
    }

    const auto haptic_sensitivity =
        settings_dict.FindInt(prefs::kTouchpadSettingHapticSensitivity);
    if (haptic_sensitivity.has_value()) {
      EXPECT_EQ(settings.haptic_sensitivity, haptic_sensitivity);
    } else {
      EXPECT_EQ(settings.haptic_sensitivity, kDefaultHapticSensitivity);
    }

    const auto haptic_enabled =
        settings_dict.FindBool(prefs::kTouchpadSettingHapticEnabled);
    if (haptic_enabled.has_value()) {
      EXPECT_EQ(settings.haptic_enabled, haptic_enabled);
    } else {
      EXPECT_EQ(settings.haptic_enabled, kDefaultHapticFeedbackEnabled);
    }
  }

  void CheckTouchpadSettingsAreSetToDefaultValues(
      const mojom::TouchpadSettings& settings) {
    EXPECT_EQ(kTouchpadSettingsDefault.sensitivity, settings.sensitivity);
    EXPECT_EQ(kTouchpadSettingsDefault.reverse_scrolling,
              settings.reverse_scrolling);
    EXPECT_EQ(kTouchpadSettingsDefault.acceleration_enabled,
              settings.acceleration_enabled);
    EXPECT_EQ(kTouchpadSettingsDefault.tap_to_click_enabled,
              settings.tap_to_click_enabled);
    EXPECT_EQ(kTouchpadSettingsDefault.three_finger_click_enabled,
              settings.three_finger_click_enabled);
    EXPECT_EQ(kTouchpadSettingsDefault.tap_dragging_enabled,
              settings.tap_dragging_enabled);
    EXPECT_EQ(kTouchpadSettingsDefault.scroll_sensitivity,
              settings.scroll_sensitivity);
    EXPECT_EQ(kTouchpadSettingsDefault.scroll_acceleration,
              settings.scroll_acceleration);
    EXPECT_EQ(kTouchpadSettingsDefault.haptic_sensitivity,
              settings.haptic_sensitivity);
    EXPECT_EQ(kTouchpadSettingsDefault.haptic_enabled, settings.haptic_enabled);
    if (features::IsAltClickAndSixPackCustomizationEnabled()) {
      EXPECT_EQ(kTouchpadSettingsDefault.simulate_right_click,
                settings.simulate_right_click);
    }
  }

  void CallUpdateTouchpadSettings(const std::string& device_key,
                                  const mojom::TouchpadSettings& settings,
                                  bool is_external = true) {
    mojom::TouchpadPtr touchpad = mojom::Touchpad::New();
    touchpad->settings = settings.Clone();
    touchpad->device_key = device_key;
    touchpad->is_external = is_external;
    touchpad->is_haptic = true;

    pref_handler_->UpdateTouchpadSettings(pref_service_.get(), *touchpad);
  }

  void CallUpdateLoginScreenTouchpadSettings(
      const AccountId& account_id,
      const std::string& device_key,
      const mojom::TouchpadSettings& settings) {
    mojom::TouchpadPtr touchpad = mojom::Touchpad::New();
    touchpad->settings = settings.Clone();
    pref_handler_->UpdateLoginScreenTouchpadSettings(local_state(), account_id,
                                                     *touchpad);
  }

  void CallUpdateDefaultTouchpadSettings(
      const std::string& device_key,
      const mojom::TouchpadSettings& settings) {
    mojom::TouchpadPtr touchpad = mojom::Touchpad::New();
    touchpad->settings = settings.Clone();
    touchpad->device_key = device_key;

    pref_handler_->UpdateDefaultTouchpadSettings(pref_service_.get(),
                                                 *touchpad);
  }

  mojom::TouchpadSettingsPtr CallInitializeTouchpadSettings(
      const std::string& device_key,
      bool is_external = true) {
    mojom::TouchpadPtr touchpad = mojom::Touchpad::New();
    touchpad->device_key = device_key;
    touchpad->is_external = is_external;
    touchpad->is_haptic = true;

    pref_handler_->InitializeTouchpadSettings(pref_service_.get(),
                                              touchpad.get());
    return std::move(touchpad->settings);
  }

  mojom::TouchpadSettingsPtr CallInitializeLoginScreenTouchpadSettings(
      const AccountId& account_id,
      const mojom::Touchpad& touchpad) {
    const auto touchpad_ptr = touchpad.Clone();

    pref_handler_->InitializeLoginScreenTouchpadSettings(
        local_state(), account_id, touchpad_ptr.get());
    return std::move(touchpad_ptr->settings);
  }

  const base::Value::Dict* GetSettingsDict(const std::string& device_key,
                                           bool is_external = true) {
    if (!is_external) {
      return &pref_service_->GetDict(prefs::kTouchpadInternalSettings);
    }

    const auto& devices_dict =
        pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
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
        account_id, prefs::kTouchpadLoginScreenInternalSettingsPref);
    return dict && dict->is_dict();
  }

  bool HasExternalLoginScreenSettingsDict(AccountId account_id) {
    const auto* dict = known_user().FindPath(
        account_id, prefs::kTouchpadLoginScreenExternalSettingsPref);
    return dict && dict->is_dict();
  }

  base::Value::Dict GetInternalLoginScreenSettingsDict(AccountId account_id) {
    return known_user()
        .FindPath(account_id, prefs::kTouchpadLoginScreenInternalSettingsPref)
        ->GetDict()
        .Clone();
  }

  void SetSimulateRightClickPrefs(int alt_count, int search_count) {
    pref_service_->SetUserPref(prefs::kAltEventRemappedToRightClick,
                               base::Value(alt_count));
    pref_service_->SetUserPref(prefs::kSearchEventRemappedToRightClick,
                               base::Value(search_count));
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TouchpadPrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(TouchpadPrefHandlerTest, InitializeLoginScreenTouchpadSettings) {
  mojom::Touchpad touchpad;
  touchpad.device_key = kTouchpadKey1;
  touchpad.is_external = false;
  mojom::TouchpadSettingsPtr settings =
      CallInitializeLoginScreenTouchpadSettings(account_id_1, touchpad);

  CheckTouchpadSettingsAreSetToDefaultValues(*settings);
  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
}

TEST_F(TouchpadPrefHandlerTest, UpdateLoginScreenTouchpadSettings) {
  mojom::Touchpad touchpad;
  touchpad.device_key = kTouchpadKey1;
  touchpad.is_external = false;
  mojom::TouchpadSettingsPtr settings =
      CallInitializeLoginScreenTouchpadSettings(account_id_1, touchpad);
  mojom::TouchpadSettings updated_settings = *settings;
  updated_settings.reverse_scrolling = !updated_settings.reverse_scrolling;
  updated_settings.acceleration_enabled =
      !updated_settings.acceleration_enabled;
  CallUpdateLoginScreenTouchpadSettings(account_id_1, kTouchpadKey1,
                                        updated_settings);
  const auto& updated_settings_dict =
      GetInternalLoginScreenSettingsDict(account_id_1);
  CheckTouchpadSettingsAndDictAreEqual(updated_settings, updated_settings_dict);
}

TEST_F(TouchpadPrefHandlerTest,
       LoginScreenPrefsNotPersistedWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::Touchpad touchpad1;
  touchpad1.device_key = kTouchpadKey1;
  touchpad1.is_external = false;
  mojom::Touchpad touchpad2;
  touchpad2.device_key = kTouchpadKey2;
  touchpad2.is_external = true;
  CallInitializeLoginScreenTouchpadSettings(account_id_1, touchpad1);
  CallInitializeLoginScreenTouchpadSettings(account_id_1, touchpad2);
  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
  EXPECT_FALSE(HasExternalLoginScreenSettingsDict(account_id_1));
}

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

TEST_F(TouchpadPrefHandlerTest, LastUpdated) {
  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettings1,
                             /*is_external=*/true);
  auto devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kTouchpadKey1);
  ASSERT_NE(nullptr, settings_dict);
  auto* time_stamp1 = settings_dict->Find(prefs::kLastUpdatedKey);
  ASSERT_NE(nullptr, time_stamp1);

  mojom::TouchpadSettingsPtr updated_settings = kTouchpadSettings1.Clone();
  updated_settings->reverse_scrolling = !updated_settings->reverse_scrolling;
  CallUpdateTouchpadSettings(kTouchpadKey1, *updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref);
  const auto* updated_settings_dict =
      updated_devices_dict.FindDict(kTouchpadKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  auto* updated_time_stamp1 =
      updated_settings_dict->Find(prefs::kLastUpdatedKey);
  ASSERT_NE(nullptr, updated_time_stamp1);
  ASSERT_NE(time_stamp1, updated_time_stamp1);
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

TEST_F(TouchpadPrefHandlerTest, UpdateSettingsInternal) {
  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettings1,
                             /*is_external=*/false);

  const auto& settings_dict =
      pref_service_->GetDict(prefs::kTouchpadInternalSettings);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettings1, settings_dict);

  mojom::TouchpadSettings updated_settings = kTouchpadSettings1;
  updated_settings.reverse_scrolling = !updated_settings.reverse_scrolling;

  // Update the settings again and verify the settings are updated in place.
  CallUpdateTouchpadSettings(kTouchpadKey1, updated_settings,
                             /*is_external=*/false);

  const auto& updated_settings_dict =
      pref_service_->GetDict(prefs::kTouchpadInternalSettings);
  CheckTouchpadSettingsAndDictAreEqual(updated_settings, updated_settings_dict);
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

  // Initialize touchpad settings for the device and check that
  // "new settings" match their default values.
  mojom::TouchpadSettingsPtr settings =
      CallInitializeTouchpadSettings(kTouchpadKey1);
  EXPECT_EQ(kDefaultReverseScrolling, settings->reverse_scrolling);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->reverse_scrolling = !kDefaultReverseScrolling;
  EXPECT_EQ(test_settings, *settings);
}

TEST_F(TouchpadPrefHandlerTest, NewSettingAddedRoundTripInternal) {
  mojom::TouchpadSettings test_settings = kTouchpadSettings1;
  test_settings.reverse_scrolling = !kDefaultReverseScrolling;

  CallUpdateTouchpadSettings(kTouchpadKey1, test_settings,
                             /*is_external=*/false);
  auto settings_dict =
      pref_service_->GetDict(prefs::kTouchpadInternalSettings).Clone();

  // Remove key from the dict to mock adding a new setting in the future.
  settings_dict.Remove(prefs::kTouchpadSettingReverseScrolling);
  pref_service_->SetDict(prefs::kTouchpadInternalSettings,
                         std::move(settings_dict));

  // Initialize touchpad settings for the device and check that
  // "new settings" match their default values.
  mojom::TouchpadSettingsPtr settings =
      CallInitializeTouchpadSettings(kTouchpadKey1, /*is_external=*/false);
  EXPECT_EQ(kDefaultReverseScrolling, settings->reverse_scrolling);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->reverse_scrolling = !kDefaultReverseScrolling;
  EXPECT_EQ(test_settings, *settings);
}

TEST_F(TouchpadPrefHandlerTest, DefaultSettingsWhenPrefServiceNull) {
  mojom::Touchpad touchpad;
  touchpad.device_key = kTouchpadKey1;
  pref_handler_->InitializeTouchpadSettings(nullptr, &touchpad);
  EXPECT_EQ(kTouchpadSettingsDefault, *touchpad.settings);
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

TEST_F(TouchpadPrefHandlerTest, NewTouchpadDefaultSettingsInternal) {
  mojom::TouchpadSettingsPtr settings =
      CallInitializeTouchpadSettings(kTouchpadKey1, /*is_external=*/false);
  EXPECT_EQ(*settings, kTouchpadSettingsDefault);

  auto& settings_dict =
      pref_service_->GetDict(prefs::kTouchpadInternalSettings);
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettingsDefault, settings_dict);
}

TEST_F(TouchpadPrefHandlerTest,
       TransitionPeriodSettingsPersistedWhenUserChosen) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::Touchpad touchpad;
  touchpad.device_key = kTouchpadKey1;
  Shell::Get()->input_device_tracker()->OnTouchpadConnected(touchpad);

  pref_service_->SetUserPref(prefs::kTouchpadSensitivity,
                             base::Value(kDefaultSensitivity));
  pref_service_->SetUserPref(prefs::kNaturalScroll,
                             base::Value(kDefaultReverseScrolling));
  pref_service_->SetUserPref(prefs::kTouchpadAcceleration,
                             base::Value(kDefaultAccelerationEnabled));
  pref_service_->SetUserPref(prefs::kTapToClickEnabled,
                             base::Value(kDefaultTapToClickEnabled));
  pref_service_->SetUserPref(prefs::kTapDraggingEnabled,
                             base::Value(kDefaultTapDraggingEnabled));
  pref_service_->SetUserPref(prefs::kTouchpadScrollSensitivity,
                             base::Value(kDefaultScrollSensitivity));
  pref_service_->SetUserPref(prefs::kTouchpadScrollAcceleration,
                             base::Value(kDefaultScrollAccelerationEnabled));
  pref_service_->SetUserPref(prefs::kTouchpadHapticClickSensitivity,
                             base::Value(kDefaultHapticSensitivity));
  pref_service_->SetUserPref(prefs::kTouchpadHapticFeedback,
                             base::Value(kDefaultHapticFeedbackEnabled));

  auto settings = CallInitializeTouchpadSettings(kTouchpadKey1);
  EXPECT_EQ(kTouchpadSettingsDefault, *settings);

  const auto* settings_dict = GetSettingsDict(kTouchpadKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingReverseScrolling));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingAccelerationEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingTapToClickEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingTapDraggingEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingScrollSensitivity));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingScrollAcceleration));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingHapticSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingHapticEnabled));
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettingsDefault,
                                       *settings_dict);
}

TEST_F(TouchpadPrefHandlerTest, TouchpadObserveredInTransitionPeriod) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::Touchpad touchpad;
  touchpad.device_key = kTouchpadKey1;
  Shell::Get()->input_device_tracker()->OnTouchpadConnected(touchpad);
  // Initialize Touchpad settings for the device and check that the global
  // prefs were used as defaults.
  mojom::TouchpadSettingsPtr settings =
      CallInitializeTouchpadSettings(touchpad.device_key);
  ASSERT_EQ(settings->sensitivity, kTestSensitivity);
  ASSERT_EQ(settings->reverse_scrolling, kTestReverseScrolling);
  ASSERT_EQ(settings->acceleration_enabled, kTestAccelerationEnabled);
  ASSERT_EQ(settings->tap_to_click_enabled, kTestTapToClickEnabled);
  ASSERT_EQ(settings->three_finger_click_enabled, kTestThreeFingerClickEnabled);
  ASSERT_EQ(settings->tap_dragging_enabled, kTestTapDraggingEnabled);
  ASSERT_EQ(settings->scroll_sensitivity, kTestSensitivity);
  ASSERT_EQ(settings->scroll_acceleration, kTestScrollAcceleration);
  ASSERT_EQ(settings->haptic_sensitivity, kTestHapticSensitivity);
  ASSERT_EQ(settings->haptic_enabled, kTestHapticFeedbackEnabled);
}

TEST_F(TouchpadPrefHandlerTest, DefaultNotPersistedUntilUpdated) {
  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettingsDefault);

  const auto* settings_dict = GetSettingsDict(kTouchpadKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kTouchpadSettingSensitivity));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingReverseScrolling));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingAccelerationEnabled));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingTapToClickEnabled));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingTapDraggingEnabled));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingScrollSensitivity));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingScrollAcceleration));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingHapticSensitivity));
  EXPECT_FALSE(settings_dict->contains(prefs::kTouchpadSettingHapticEnabled));
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettingsDefault,
                                       *settings_dict);

  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettingsNotDefault);
  settings_dict = GetSettingsDict(kTouchpadKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingReverseScrolling));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingAccelerationEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingTapToClickEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingTapDraggingEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingScrollSensitivity));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingScrollAcceleration));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingHapticSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingHapticEnabled));
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettingsNotDefault,
                                       *settings_dict);

  CallUpdateTouchpadSettings(kTouchpadKey1, kTouchpadSettingsDefault);
  settings_dict = GetSettingsDict(kTouchpadKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingReverseScrolling));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingAccelerationEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingTapToClickEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingTapDraggingEnabled));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingScrollSensitivity));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingScrollAcceleration));
  EXPECT_TRUE(
      settings_dict->contains(prefs::kTouchpadSettingHapticSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kTouchpadSettingHapticEnabled));
  CheckTouchpadSettingsAndDictAreEqual(kTouchpadSettingsDefault,
                                       *settings_dict);
}

TEST_F(TouchpadPrefHandlerTest, SimulateRightClickSettingFlagDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAltClickAndSixPackCustomization);
  CallInitializeTouchpadSettings(kTouchpadKey1);
  const auto* settings_dict = GetSettingsDict(kTouchpadKey1);
  EXPECT_FALSE(
      settings_dict->contains(prefs::kTouchpadSettingSimulateRightClick));
}

TEST_F(TouchpadPrefHandlerTest, RememberDefaultsFromLastUpdatedSettings) {
  mojom::TouchpadSettingsPtr settings =
      CallInitializeTouchpadSettings(kTouchpadKey1);
  settings->reverse_scrolling = !kDefaultReverseScrolling;
  settings->sensitivity = 1;
  CallUpdateTouchpadSettings(kTouchpadKey1, *settings);
  CallUpdateDefaultTouchpadSettings(kTouchpadKey1, *settings);

  mojom::TouchpadSettingsPtr settings2 =
      CallInitializeTouchpadSettings(kTouchpadKey2);
  EXPECT_EQ(*settings2, *settings);

  settings2->sensitivity = 5;
  CallUpdateDefaultTouchpadSettings(kTouchpadKey2, *settings2);

  mojom::TouchpadSettingsPtr settings_duplicate =
      CallInitializeTouchpadSettings(kTouchpadKey1);
  EXPECT_EQ(*settings, *settings_duplicate);
}

TEST_F(TouchpadPrefHandlerTest, SettingsUpdateMetricTest) {
  const auto settings1 = CallInitializeTouchpadSettings(kTouchpadKey1);

  // When its the first device of the type the category should be kFirstEver.
  {
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kTouchpadUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kTouchpadKey1));

    auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(
        *metric_dict.FindDict(kTouchpadKey1));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kFirstEver,
              metrics_info->category());
  }

  // When its taken off the the defaults, the category should be kDefault.
  {
    CallUpdateDefaultTouchpadSettings(kTouchpadKey1, *settings1);
    CallInitializeTouchpadSettings(kTouchpadKey2);
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kTouchpadUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kTouchpadKey2));

    auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(
        *metric_dict.FindDict(kTouchpadKey2));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kDefault,
              metrics_info->category());
  }

  // When its taken from synced prefs on a different device, category should
  // match.
  {
    auto devices_dict =
        pref_service_->GetDict(prefs::kTouchpadDeviceSettingsDictPref).Clone();
    devices_dict.Set(kTouchpadKey3, base::Value::Dict());
    pref_service_->SetDict(prefs::kTouchpadDeviceSettingsDictPref,
                           std::move(devices_dict));

    CallInitializeTouchpadSettings(kTouchpadKey3);
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kTouchpadUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kTouchpadKey3));

    auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(
        *metric_dict.FindDict(kTouchpadKey3));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kSynced,
              metrics_info->category());
  }
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

  const auto* settings_dict = GetSettingsDict(device_key_);
  CheckTouchpadSettingsAndDictAreEqual(settings_, *settings_dict);
}

TEST_P(TouchpadSettingsPrefConversionTest, CheckConversionInternal) {
  CallUpdateTouchpadSettings(device_key_, settings_, /*is_external=*/false);

  const auto* settings_dict =
      GetSettingsDict(device_key_, /*is_external=*/false);
  CheckTouchpadSettingsAndDictAreEqual(settings_, *settings_dict);
}

class TouchpadSettingsSimulateRightClickTest
    : public TouchpadPrefHandlerTest,
      public testing::WithParamInterface<
          std::tuple<int, int, ui::mojom::SimulateRightClickModifier>> {
 public:
  TouchpadSettingsSimulateRightClickTest() = default;
  TouchpadSettingsSimulateRightClickTest(
      const TouchpadSettingsSimulateRightClickTest&) = delete;
  TouchpadSettingsSimulateRightClickTest& operator=(
      const TouchpadSettingsSimulateRightClickTest&) = delete;
  ~TouchpadSettingsSimulateRightClickTest() override = default;

  // testing::Test:
  void SetUp() override {
    TouchpadPrefHandlerTest::SetUp();
    std::tie(alt_count, search_count, expected) = GetParam();
  }

 protected:
  int alt_count;
  int search_count;
  ui::mojom::SimulateRightClickModifier expected;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    TouchpadSettingsSimulateRightClickTest,
    testing::ValuesIn(
        std::vector<
            std::tuple<int, int, ui::mojom::SimulateRightClickModifier>>{
            {0, 0, ui::mojom::SimulateRightClickModifier::kNone},
            {5, 0, ui::mojom::SimulateRightClickModifier::kAlt},
            {0, 5, ui::mojom::SimulateRightClickModifier::kSearch}}));

TEST_P(TouchpadSettingsSimulateRightClickTest, SimulateRightClickSetting) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAltClickAndSixPackCustomization);
    CallInitializeTouchpadSettings(kTouchpadKey1);
  }

  SetSimulateRightClickPrefs(alt_count, search_count);
  const auto settings = CallInitializeTouchpadSettings(kTouchpadKey1);
  EXPECT_EQ(expected, settings->simulate_right_click);
}

TEST_P(TouchpadSettingsSimulateRightClickTest,
       SimulateRightClickSettingInternal) {
  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndDisableFeature(
        features::kAltClickAndSixPackCustomization);
    CallInitializeTouchpadSettings(kTouchpadKey1, /*is_external=*/false);
  }

  SetSimulateRightClickPrefs(alt_count, search_count);
  const auto settings =
      CallInitializeTouchpadSettings(kTouchpadKey1, /*is_external=*/false);
  EXPECT_EQ(expected, settings->simulate_right_click);
}

}  // namespace ash
