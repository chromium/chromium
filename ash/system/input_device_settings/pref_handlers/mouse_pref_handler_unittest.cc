// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/mouse_pref_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/shell.h"
#include "ash/system/input_device_settings/input_device_settings_defaults.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/system/input_device_settings/input_device_tracker.h"
#include "ash/system/input_device_settings/settings_updated_metrics_info.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/strcat.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"
#include "mojo/public/cpp/bindings/clone_traits.h"

namespace ash {

namespace {
const std::string kDictFakeKey = "fake_key";
const std::string kDictFakeValue = "fake_value";

const std::string kMouseKey1 = "device_key1";
const std::string kMouseKey2 = "device_key2";
const std::string kMouseKey3 = "device_key3";

constexpr char kUserEmail[] = "example@email.com";
constexpr char kUserEmail2[] = "example2@email.com";
const AccountId account_id_1 = AccountId::FromUserEmail(kUserEmail);
const AccountId account_id_2 = AccountId::FromUserEmail(kUserEmail2);

const bool kTestSwapRight = false;
const int kTestSensitivity = 2;
const bool kTestReverseScrolling = false;
const bool kTestAccelerationEnabled = false;
const int kTestScrollSensitivity = 3;
const bool kTestScrollAcceleration = false;

const mojom::ButtonRemapping button_remapping1(
    /*name=*/"test1",
    /*button=*/
    mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kMiddle),
    /*remapping_action=*/
    mojom::RemappingAction::NewAcceleratorAction(
        ash::AcceleratorAction::kBrightnessDown));

const mojom::ButtonRemapping button_remapping2(
    /*name=*/"test2",
    /*button=*/mojom::Button::NewVkey(::ui::KeyboardCode::VKEY_1),
    /*remapping_action=*/
    mojom::RemappingAction::NewStaticShortcutAction(
        mojom::StaticShortcutAction::kCopy));

const mojom::MouseSettings kMouseSettingsDefault(
    /*swap_right=*/kDefaultSwapRight,
    /*sensitivity=*/kDefaultSensitivity,
    /*reverse_scrolling=*/kDefaultReverseScrolling,
    /*acceleration_enabled=*/kDefaultAccelerationEnabled,
    /*scroll_sensitivity=*/kDefaultScrollSensitivity,
    /*scroll_acceleration=*/kDefaultScrollAccelerationEnabled,
    /*button_remappings=*/std::vector<mojom::ButtonRemappingPtr>());

const mojom::MouseSettings kMouseSettingsNotDefault(
    /*swap_right=*/!kDefaultSwapRight,
    /*sensitivity=*/1,
    /*reverse_scrolling=*/!kDefaultReverseScrolling,
    /*acceleration_enabled=*/!kDefaultAccelerationEnabled,
    /*scroll_sensitivity=*/1,
    /*scroll_acceleration=*/!kDefaultScrollAccelerationEnabled,
    /*button_remappings=*/std::vector<mojom::ButtonRemappingPtr>());

const mojom::MouseSettings kMouseSettings1(
    /*swap_right=*/false,
    /*sensitivity=*/1,
    /*reverse_scrolling=*/false,
    /*acceleration_enabled=*/false,
    /*scroll_sensitivity=*/1,
    /*scroll_acceleration=*/false,
    /*button_remappings=*/std::vector<mojom::ButtonRemappingPtr>());

const mojom::MouseSettings kMouseSettings2(
    /*swap_right=*/true,
    /*sensitivity=*/10,
    /*reverse_scrolling=*/true,
    /*acceleration_enabled=*/true,
    /*scroll_sensitivity=*/24,
    /*scroll_acceleration=*/true,
    /*button_remappings=*/std::vector<mojom::ButtonRemappingPtr>());
}  // namespace

class MousePrefHandlerTest : public AshTestBase {
 public:
  MousePrefHandlerTest() = default;
  MousePrefHandlerTest(const MousePrefHandlerTest&) = delete;
  MousePrefHandlerTest& operator=(const MousePrefHandlerTest&) = delete;
  ~MousePrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kPeripheralCustomization,
                                           features::kInputDeviceSettingsSplit},
                                          {});
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
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kMouseButtonRemappingsDictPref);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kMouseDefaultSettings);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kMouseUpdateSettingsMetricInfo);

    // We are using these test constants as a a way to differentiate values
    // retrieved from prefs or default mouse settings.
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kPrimaryMouseButtonRight, kDefaultSwapRight);
    pref_service_->registry()->RegisterIntegerPref(prefs::kMouseSensitivity,
                                                   kDefaultSensitivity);
    pref_service_->registry()->RegisterBooleanPref(prefs::kMouseReverseScroll,
                                                   kDefaultReverseScrolling);
    pref_service_->registry()->RegisterBooleanPref(prefs::kMouseAcceleration,
                                                   kDefaultAccelerationEnabled);
    pref_service_->registry()->RegisterIntegerPref(
        prefs::kMouseScrollSensitivity, kDefaultScrollSensitivity);
    pref_service_->registry()->RegisterBooleanPref(
        prefs::kMouseScrollAcceleration, kDefaultScrollAccelerationEnabled);

    pref_service_->SetUserPref(prefs::kPrimaryMouseButtonRight,
                               base::Value(kTestSwapRight));
    pref_service_->SetUserPref(prefs::kMouseSensitivity,
                               base::Value(kTestSensitivity));
    pref_service_->SetUserPref(prefs::kMouseReverseScroll,
                               base::Value(kTestReverseScrolling));
    pref_service_->SetUserPref(prefs::kMouseAcceleration,
                               base::Value(kTestAccelerationEnabled));
    pref_service_->SetUserPref(prefs::kMouseScrollSensitivity,
                               base::Value(kTestScrollSensitivity));
    pref_service_->SetUserPref(prefs::kMouseScrollAcceleration,
                               base::Value(kTestScrollAcceleration));
  }

  void CheckMouseSettingsAndDictAreEqual(
      const mojom::MouseSettings& settings,
      const base::Value::Dict& settings_dict) {
    const auto swap_right =
        settings_dict.FindBool(prefs::kMouseSettingSwapRight);
    if (swap_right.has_value()) {
      EXPECT_EQ(settings.swap_right, swap_right);
    } else {
      EXPECT_EQ(settings.swap_right, kDefaultSwapRight);
    }

    const auto sensitivity =
        settings_dict.FindInt(prefs::kMouseSettingSensitivity);
    if (sensitivity.has_value()) {
      EXPECT_EQ(settings.sensitivity, sensitivity);
    } else {
      EXPECT_EQ(settings.sensitivity, kDefaultSensitivity);
    }

    const auto reverse_scrolling =
        settings_dict.FindBool(prefs::kMouseSettingReverseScrolling);
    if (reverse_scrolling.has_value()) {
      EXPECT_EQ(settings.reverse_scrolling, reverse_scrolling);
    } else {
      EXPECT_EQ(settings.reverse_scrolling, kDefaultReverseScrolling);
    }

    const auto acceleration_enabled =
        settings_dict.FindBool(prefs::kMouseSettingAccelerationEnabled);
    if (acceleration_enabled.has_value()) {
      EXPECT_EQ(settings.acceleration_enabled, acceleration_enabled);
    } else {
      EXPECT_EQ(settings.acceleration_enabled, kDefaultAccelerationEnabled);
    }

    const auto scroll_sensitivity =
        settings_dict.FindInt(prefs::kMouseSettingScrollSensitivity);
    if (scroll_sensitivity.has_value()) {
      EXPECT_EQ(settings.scroll_sensitivity, scroll_sensitivity);
    } else {
      EXPECT_EQ(settings.scroll_sensitivity, kDefaultScrollSensitivity);
    }

    const auto scroll_acceleration =
        settings_dict.FindBool(prefs::kMouseSettingScrollAcceleration);
    if (scroll_acceleration.has_value()) {
      EXPECT_EQ(settings.scroll_acceleration, scroll_acceleration);
    } else {
      EXPECT_EQ(settings.scroll_acceleration,
                kDefaultScrollAccelerationEnabled);
    }
  }

  void CheckMouseSettingsAreSetToDefaultValues(
      const mojom::MouseSettings& settings) {
    EXPECT_EQ(kMouseSettingsDefault.swap_right, settings.swap_right);
    EXPECT_EQ(kMouseSettingsDefault.sensitivity, settings.sensitivity);
    EXPECT_EQ(kMouseSettingsDefault.reverse_scrolling,
              settings.reverse_scrolling);
    EXPECT_EQ(kMouseSettingsDefault.acceleration_enabled,
              settings.acceleration_enabled);
    EXPECT_EQ(kMouseSettingsDefault.scroll_sensitivity,
              settings.scroll_sensitivity);
    EXPECT_EQ(kMouseSettingsDefault.scroll_acceleration,
              settings.scroll_acceleration);
  }

  void CallUpdateMouseSettings(
      const std::string& device_key,
      const mojom::MouseSettings& settings,
      mojom::CustomizationRestriction customization_restriction =
          mojom::CustomizationRestriction::kAllowCustomizations) {
    mojom::MousePtr mouse = mojom::Mouse::New();
    mouse->settings = settings.Clone();
    mouse->device_key = device_key;
    mouse->customization_restriction = customization_restriction;

    pref_handler_->UpdateMouseSettings(pref_service_.get(),
                                       /*mouse_policies=*/{}, *mouse);
  }

  void CallUpdateLoginScreenMouseSettings(
      const AccountId& account_id,
      const std::string& device_key,
      const mojom::MouseSettings& settings,
      mojom::CustomizationRestriction customization_restriction) {
    mojom::MousePtr mouse = mojom::Mouse::New();
    mouse->settings = settings.Clone();
    mouse->customization_restriction = customization_restriction;
    pref_handler_->UpdateLoginScreenMouseSettings(
        local_state(), account_id, /*mouse_policies=*/{}, *mouse);
  }

  void CallUpdateDefaultMouseSettings(const std::string& device_key,
                                      const mojom::MouseSettings& settings) {
    mojom::MousePtr mouse = mojom::Mouse::New();
    mouse->settings = settings.Clone();
    mouse->device_key = device_key;

    pref_handler_->UpdateDefaultMouseSettings(pref_service_.get(),
                                              /*mouse_policies=*/{}, *mouse);
  }

  mojom::MouseSettingsPtr CallInitializeMouseSettings(
      const std::string& device_key,
      mojom::CustomizationRestriction customization_restriction =
          mojom::CustomizationRestriction::kAllowCustomizations,
      mojom::MouseButtonConfig mouse_button_config =
          mojom::MouseButtonConfig::kNoConfig) {
    mojom::MousePtr mouse = mojom::Mouse::New();
    mouse->device_key = device_key;
    mouse->customization_restriction = customization_restriction;
    mouse->mouse_button_config = mouse_button_config;

    pref_handler_->InitializeMouseSettings(pref_service_.get(),
                                           /*mouse_policies=*/{}, mouse.get());
    return std::move(mouse->settings);
  }

  mojom::MouseSettingsPtr CallInitializeLoginScreenMouseSettings(
      const AccountId& account_id,
      const mojom::Mouse& mouse) {
    const auto mouse_ptr = mouse.Clone();

    pref_handler_->InitializeLoginScreenMouseSettings(
        local_state(), account_id, /*mouse_policies=*/{}, mouse_ptr.get());
    return std::move(mouse_ptr->settings);
  }

  const base::Value::Dict* GetSettingsDict(const std::string& device_key) {
    const auto& devices_dict =
        pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref);
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
        account_id, prefs::kMouseLoginScreenInternalSettingsPref);
    return dict && dict->is_dict();
  }

  bool HasExternalLoginScreenSettingsDict(AccountId account_id) {
    const auto* dict = known_user().FindPath(
        account_id, prefs::kMouseLoginScreenExternalSettingsPref);
    return dict && dict->is_dict();
  }

  bool HasLoginScreenMouseButtonRemappingList(AccountId account_id) {
    const auto* button_remapping_list = known_user().FindPath(
        account_id, prefs::kMouseLoginScreenButtonRemappingListPref);
    return button_remapping_list && button_remapping_list->is_list();
  }

  base::Value::Dict GetInternalLoginScreenSettingsDict(AccountId account_id) {
    return known_user()
        .FindPath(account_id, prefs::kMouseLoginScreenInternalSettingsPref)
        ->GetDict()
        .Clone();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<MousePrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(MousePrefHandlerTest, UpdateButtonRemappingsWithCompleteList) {
  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;
  mouse.is_external = false;

  // Update the button remappings pref dict to mock adding a new
  // button remapping in the future.
  std::vector<mojom::ButtonRemappingPtr> button_remappings;
  button_remappings.push_back(button_remapping1.Clone());
  base::Value::Dict updated_button_remappings_dict;
  updated_button_remappings_dict.Set(
      kMouseKey1, ConvertButtonRemappingArrayToList(
                      button_remappings,
                      mojom::CustomizationRestriction::kAllowCustomizations));

  pref_service_->SetDict(prefs::kMouseButtonRemappingsDictPref,
                         updated_button_remappings_dict.Clone());

  mojom::MouseSettingsPtr updated_settings = CallInitializeMouseSettings(
      kMouseKey1, mojom::CustomizationRestriction::kAllowCustomizations,
      mojom::MouseButtonConfig::kFiveKey);
  EXPECT_NE(button_remappings, updated_settings->button_remappings);
  EXPECT_EQ(3u, updated_settings->button_remappings.size());
}

TEST_F(MousePrefHandlerTest, InitializeLoginScreenMouseSettings) {
  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;
  mouse.is_external = false;
  mouse.customization_restriction =
      mojom::CustomizationRestriction::kAllowCustomizations;
  mojom::Mouse mouse2;
  mouse2.device_key = kMouseKey2;
  mouse2.is_external = false;
  mouse2.customization_restriction =
      mojom::CustomizationRestriction::kDisallowCustomizations;
  mojom::Mouse mouse3;
  mouse3.device_key = kMouseKey3;
  mouse3.is_external = false;
  mouse3.customization_restriction =
      mojom::CustomizationRestriction::kDisableKeyEventRewrites;

  mojom::MouseSettingsPtr settings =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse);
  mojom::MouseSettingsPtr settings2 =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse2);
  mojom::MouseSettingsPtr settings3 =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse3);

  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
  CheckMouseSettingsAreSetToDefaultValues(*settings);

  EXPECT_FALSE(HasLoginScreenMouseButtonRemappingList(account_id_1));
  EXPECT_EQ(std::vector<mojom::ButtonRemappingPtr>(),
            settings->button_remappings);

  // Update the button remappings pref list to mock adding a new
  // button remapping in the future.
  std::vector<mojom::ButtonRemappingPtr> button_remappings;
  button_remappings.push_back(button_remapping2.Clone());
  known_user().SetPath(
      account_id_1, prefs::kMouseLoginScreenButtonRemappingListPref,
      std::optional<base::Value>(ConvertButtonRemappingArrayToList(
          button_remappings,
          mojom::CustomizationRestriction::kAllowCustomizations)));

  // updated_settings have updated button remappings since mouse
  // has kAllowCustomizations customization restriction.
  mojom::MouseSettingsPtr updated_settings =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse);
  EXPECT_EQ(button_remappings, updated_settings->button_remappings);

  // updated_settings2 have no button remappings since mouse2
  // has kDisallowCustomizations customization restriction.
  mojom::MouseSettingsPtr updated_settings2 =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse2);
  EXPECT_EQ(std::vector<mojom::ButtonRemappingPtr>(),
            updated_settings2->button_remappings);

  // updated_settings3 have no button remappings since mouse3
  // has kDisableKeyEventRewrites customization restriction and the
  // button is a VKey.
  mojom::MouseSettingsPtr updated_settings3 =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse3);
  EXPECT_EQ(std::vector<mojom::ButtonRemappingPtr>(),
            updated_settings3->button_remappings);
}

TEST_F(MousePrefHandlerTest, UpdateLoginScreenButtonRemappingList) {
  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;
  mouse.is_external = false;
  mouse.customization_restriction =
      mojom::CustomizationRestriction::kAllowCustomizations;
  mojom::Mouse mouse2;
  mouse2.device_key = kMouseKey2;
  mouse2.is_external = false;
  mouse2.customization_restriction =
      mojom::CustomizationRestriction::kDisallowCustomizations;
  mojom::Mouse mouse3;
  mouse3.device_key = kMouseKey3;
  mouse3.is_external = false;
  mouse3.customization_restriction =
      mojom::CustomizationRestriction::kDisableKeyEventRewrites;

  mojom::MouseSettingsPtr settings =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse);
  mojom::MouseSettingsPtr settings2 =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse2);
  mojom::MouseSettingsPtr settings3 =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse3);

  // Update button_remappings in mouse settings.
  mojom::MouseSettingsPtr updated_settings = settings->Clone();
  std::vector<mojom::ButtonRemappingPtr> button_remapping_list;
  button_remapping_list.push_back(button_remapping1.Clone());
  updated_settings->button_remappings = mojo::Clone(button_remapping_list);
  CallUpdateLoginScreenMouseSettings(account_id_1, kMouseKey1,
                                     *updated_settings,
                                     mouse.customization_restriction);
  EXPECT_TRUE(HasLoginScreenMouseButtonRemappingList(account_id_1));

  // Verify the updated button remapping list. It should have updated prefs
  // since mouse has kAllowCustomizations customization restriction.
  const auto* updated_button_remapping_list = GetLoginScreenButtonRemappingList(
      local_state(), account_id_1,
      prefs::kMouseLoginScreenButtonRemappingListPref);
  ASSERT_NE(nullptr, updated_button_remapping_list);
  ASSERT_EQ(1u, updated_button_remapping_list->size());
  const auto& button_remapping = (*updated_button_remapping_list)[0].GetDict();
  EXPECT_EQ("REDACTED",
            *button_remapping.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(
      static_cast<int>(button_remapping1.button->get_customizable_button()),
      *button_remapping.FindInt(prefs::kButtonRemappingCustomizableButton));
  EXPECT_EQ(
      static_cast<int>(
          button_remapping1.remapping_action->get_accelerator_action()),
      *button_remapping.FindInt(prefs::kButtonRemappingAcceleratorAction));

  // Update button_remappings in mouse2 settings.
  mojom::MouseSettingsPtr updated_settings2 = settings2->Clone();
  std::vector<mojom::ButtonRemappingPtr> button_remapping_list2;
  button_remapping_list2.push_back(button_remapping2.Clone());
  updated_settings2->button_remappings = mojo::Clone(button_remapping_list2);
  CallUpdateLoginScreenMouseSettings(account_id_1, kMouseKey1,
                                     *updated_settings2,
                                     mouse2.customization_restriction);
  EXPECT_TRUE(HasLoginScreenMouseButtonRemappingList(account_id_1));

  // Verify the updated button remapping list2. It should be empty
  // since mouse2 has kDisallowCustomizations customization restriction.
  const auto* updated_button_remapping_list2 =
      GetLoginScreenButtonRemappingList(
          local_state(), account_id_1,
          prefs::kMouseLoginScreenButtonRemappingListPref);
  ASSERT_NE(nullptr, updated_button_remapping_list2);
  ASSERT_EQ(0u, updated_button_remapping_list2->size());

  // Update button_remappings in mouse3 settings.
  mojom::MouseSettingsPtr updated_settings3 = settings3->Clone();
  std::vector<mojom::ButtonRemappingPtr> button_remapping_list3;
  button_remapping_list3.push_back(button_remapping2.Clone());
  updated_settings3->button_remappings = mojo::Clone(button_remapping_list3);
  CallUpdateLoginScreenMouseSettings(account_id_1, kMouseKey1,
                                     *updated_settings3,
                                     mouse3.customization_restriction);
  EXPECT_TRUE(HasLoginScreenMouseButtonRemappingList(account_id_1));

  // Verify the updated button remapping list3. It should be empty
  // since mouse3 has has kDisableKeyEventRewrites customization restriction and
  // the button is a VKey.
  const auto* updated_button_remapping_list3 =
      GetLoginScreenButtonRemappingList(
          local_state(), account_id_1,
          prefs::kMouseLoginScreenButtonRemappingListPref);
  ASSERT_NE(nullptr, updated_button_remapping_list3);
  ASSERT_EQ(0u, updated_button_remapping_list3->size());
}

TEST_F(MousePrefHandlerTest, UpdateLoginScreenMouseSettings) {
  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;
  mouse.is_external = false;
  mouse.customization_restriction =
      mojom::CustomizationRestriction::kAllowCustomizations;
  mojom::MouseSettingsPtr settings =
      CallInitializeLoginScreenMouseSettings(account_id_1, mouse);
  mojom::MouseSettingsPtr updated_settings = settings->Clone();
  updated_settings->reverse_scrolling = !updated_settings->reverse_scrolling;
  updated_settings->acceleration_enabled =
      !updated_settings->acceleration_enabled;
  CallUpdateLoginScreenMouseSettings(account_id_1, kMouseKey1,
                                     *updated_settings,
                                     mouse.customization_restriction);
  const auto& updated_settings_dict =
      GetInternalLoginScreenSettingsDict(account_id_1);
  CheckMouseSettingsAndDictAreEqual(*updated_settings, updated_settings_dict);
  EXPECT_TRUE(HasInternalLoginScreenSettingsDict(account_id_1));
}

TEST_F(MousePrefHandlerTest, LoginScreenPrefsNotPersistedWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::Mouse mouse1;
  mouse1.device_key = kMouseKey1;
  mouse1.is_external = false;
  mouse1.customization_restriction =
      mojom::CustomizationRestriction::kAllowCustomizations;
  mojom::Mouse mouse2;
  mouse2.device_key = kMouseKey2;
  mouse2.is_external = true;
  mouse2.customization_restriction =
      mojom::CustomizationRestriction::kAllowCustomizations;
  CallInitializeLoginScreenMouseSettings(account_id_1, mouse1);
  CallInitializeLoginScreenMouseSettings(account_id_1, mouse2);
  EXPECT_FALSE(HasInternalLoginScreenSettingsDict(account_id_1));
  EXPECT_FALSE(HasExternalLoginScreenSettingsDict(account_id_1));
}

TEST_F(MousePrefHandlerTest,
       LoginScreenButtonRemappingListNotPersistedWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPeripheralCustomization);
  mojom::Mouse mouse1;
  mouse1.device_key = kMouseKey1;
  mouse1.is_external = false;
  mouse1.customization_restriction =
      mojom::CustomizationRestriction::kAllowCustomizations;

  CallInitializeLoginScreenMouseSettings(account_id_1, mouse1);
  EXPECT_FALSE(HasLoginScreenMouseButtonRemappingList(account_id_1));
}

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

TEST_F(MousePrefHandlerTest, LastUpdated) {
  CallUpdateMouseSettings(kMouseKey1, kMouseSettings1);
  auto devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, settings_dict);
  auto* time_stamp1 = settings_dict->Find(prefs::kLastUpdatedKey);
  ASSERT_NE(nullptr, time_stamp1);
  auto button_remapping_time_stamp_path =
      base::StrCat({prefs::kLastUpdatedKey, ".", kMouseKey1});
  auto* button_remapping_time_stamp =
      pref_service_->GetDict(prefs::kMouseButtonRemappingsDictPref)
          .FindByDottedPath(button_remapping_time_stamp_path);
  ASSERT_NE(nullptr, button_remapping_time_stamp);

  mojom::MouseSettingsPtr updated_settings = kMouseSettings1.Clone();
  updated_settings->swap_right = !updated_settings->swap_right;
  CallUpdateMouseSettings(kMouseKey1, *updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* updated_settings_dict = updated_devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  auto* updated_time_stamp1 =
      updated_settings_dict->Find(prefs::kLastUpdatedKey);
  ASSERT_NE(nullptr, updated_time_stamp1);
  ASSERT_NE(time_stamp1, updated_time_stamp1);

  std::vector<mojom::ButtonRemappingPtr> button_remappings;
  button_remappings.push_back(button_remapping2.Clone());
  mojom::MouseSettings kUpdatedMouseSettingsWithButtonRemapping(
      /*swap_right=*/kDefaultSwapRight,
      /*sensitivity=*/kDefaultSensitivity,
      /*reverse_scrolling=*/kDefaultReverseScrolling,
      /*acceleration_enabled=*/kDefaultAccelerationEnabled,
      /*scroll_sensitivity=*/kDefaultScrollSensitivity,
      /*scroll_acceleration=*/kDefaultScrollAccelerationEnabled,
      /*button_remappings=*/mojo::Clone(button_remappings));

  CallUpdateMouseSettings(kMouseKey1, kUpdatedMouseSettingsWithButtonRemapping);
  auto* updated_button_remapping_time_stamp =
      pref_service_->GetDict(prefs::kMouseButtonRemappingsDictPref)
          .FindByDottedPath(button_remapping_time_stamp_path);
  ASSERT_NE(nullptr, updated_button_remapping_time_stamp);
  ASSERT_NE(button_remapping_time_stamp, updated_button_remapping_time_stamp);
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

  mojom::MouseSettingsPtr updated_settings = kMouseSettings1.Clone();
  updated_settings->swap_right = !updated_settings->swap_right;

  // Update the settings again and verify the settings are updated in place.
  CallUpdateMouseSettings(kMouseKey1, *updated_settings);

  const auto& updated_devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref);
  const auto* updated_settings_dict = updated_devices_dict.FindDict(kMouseKey1);
  ASSERT_NE(nullptr, updated_settings_dict);
  CheckMouseSettingsAndDictAreEqual(*updated_settings, *updated_settings_dict);

  // Verify other device remains unmodified.
  const auto* unchanged_settings_dict =
      updated_devices_dict.FindDict(kMouseKey2);
  ASSERT_NE(nullptr, unchanged_settings_dict);
  CheckMouseSettingsAndDictAreEqual(kMouseSettings2, *unchanged_settings_dict);
}

TEST_F(MousePrefHandlerTest, NewSettingAddedRoundTrip) {
  mojom::MouseSettingsPtr test_settings = kMouseSettings1.Clone();
  test_settings->swap_right = !kDefaultSwapRight;

  CallUpdateMouseSettings(kMouseKey1, *test_settings);
  auto devices_dict =
      pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
  auto* settings_dict = devices_dict.FindDict(kMouseKey1);

  // Remove key from the dict to mock adding a new setting in the future.
  settings_dict->Remove(prefs::kMouseSettingSwapRight);
  pref_service_->SetDict(prefs::kMouseDeviceSettingsDictPref,
                         std::move(devices_dict));

  // Initialize mouse settings for the device and check that
  // "new settings" matches "test_settings".
  mojom::MouseSettingsPtr settings = CallInitializeMouseSettings(kMouseKey1);
  EXPECT_EQ(kDefaultSwapRight, settings->swap_right);

  // Reset "new settings" to the values that match `test_settings` and check
  // that the rest of the fields are equal.
  settings->swap_right = !kDefaultSwapRight;
  EXPECT_EQ(*test_settings, *settings);
}

TEST_F(MousePrefHandlerTest, DefaultSettingsWhenPrefServiceNull) {
  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;
  pref_handler_->InitializeMouseSettings(nullptr, /*mouse_policies=*/{},
                                         &mouse);
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);
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
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
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

TEST_F(MousePrefHandlerTest, TransitionPeriodSettingsPersistedWhenUserChosen) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;
  Shell::Get()->input_device_tracker()->OnMouseConnected(mouse);

  pref_service_->SetUserPref(prefs::kPrimaryMouseButtonRight,
                             base::Value(kDefaultSwapRight));
  pref_service_->SetUserPref(prefs::kMouseSensitivity,
                             base::Value(kDefaultSensitivity));
  pref_service_->SetUserPref(prefs::kMouseReverseScroll,
                             base::Value(kDefaultReverseScrolling));
  pref_service_->SetUserPref(prefs::kMouseAcceleration,
                             base::Value(kDefaultAccelerationEnabled));
  pref_service_->SetUserPref(prefs::kMouseScrollSensitivity,
                             base::Value(kDefaultScrollSensitivity));
  pref_service_->SetUserPref(prefs::kMouseScrollAcceleration,
                             base::Value(kDefaultScrollAccelerationEnabled));
  mojom::MouseSettingsPtr settings =
      CallInitializeMouseSettings(mouse.device_key);
  EXPECT_EQ(kMouseSettingsDefault, *settings);

  const auto* settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingSwapRight));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingReverseScrolling));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingAccelerationEnabled));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingScrollSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingScrollAcceleration));
  CheckMouseSettingsAndDictAreEqual(kMouseSettingsDefault, *settings_dict);
}

TEST_F(MousePrefHandlerTest, DefaultNotPersistedUntilUpdated) {
  CallUpdateMouseSettings(kMouseKey1, kMouseSettingsDefault);

  const auto* settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingSwapRight));
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingSensitivity));
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingReverseScrolling));
  EXPECT_FALSE(
      settings_dict->contains(prefs::kMouseSettingAccelerationEnabled));
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingScrollSensitivity));
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingScrollAcceleration));
  CheckMouseSettingsAndDictAreEqual(kMouseSettingsDefault, *settings_dict);

  CallUpdateMouseSettings(kMouseKey1, kMouseSettingsNotDefault);
  settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingSwapRight));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingReverseScrolling));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingAccelerationEnabled));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingScrollSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingScrollAcceleration));
  CheckMouseSettingsAndDictAreEqual(kMouseSettingsNotDefault, *settings_dict);

  CallUpdateMouseSettings(kMouseKey1, kMouseSettingsDefault);
  settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingSwapRight));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingReverseScrolling));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingAccelerationEnabled));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingScrollSensitivity));
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingScrollAcceleration));
  CheckMouseSettingsAndDictAreEqual(kMouseSettingsDefault, *settings_dict);
}

TEST_F(MousePrefHandlerTest, NewMouse_ManagedEnterprisePolicy_GetsDefaults) {
  mojom::MousePolicies policies;
  policies.swap_right_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kManaged, !kDefaultSwapRight);

  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;

  pref_handler_->InitializeMouseSettings(pref_service_.get(), policies, &mouse);

  EXPECT_EQ(!kDefaultSwapRight, mouse.settings->swap_right);
  mouse.settings->swap_right = kDefaultSwapRight;
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);

  const auto* settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingSwapRight));
}

TEST_F(MousePrefHandlerTest, LoginScreen_ManagedEnterprisePolicy) {
  mojom::MousePolicies policies;
  policies.swap_right_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kManaged, !kDefaultSwapRight);

  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;

  pref_handler_->InitializeLoginScreenMouseSettings(local_state(), account_id_1,
                                                    policies, &mouse);

  EXPECT_EQ(!kDefaultSwapRight, mouse.settings->swap_right);
  mouse.settings->swap_right = kDefaultSwapRight;
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);
}

TEST_F(MousePrefHandlerTest,
       NewMouse_RecommendedEnterprisePolicy_GetsDefaults) {
  mojom::MousePolicies policies;
  policies.swap_right_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kRecommended, !kDefaultSwapRight);

  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;

  pref_handler_->InitializeMouseSettings(pref_service_.get(), policies, &mouse);

  EXPECT_EQ(!kDefaultSwapRight, mouse.settings->swap_right);
  mouse.settings->swap_right = kDefaultSwapRight;
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);

  const auto* settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingSwapRight));
}

TEST_F(MousePrefHandlerTest,
       ExistingMouse_RecommendedEnterprisePolicy_GetsNewPolicy) {
  mojom::MousePolicies policies;
  policies.swap_right_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kRecommended, !kDefaultSwapRight);

  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;

  pref_handler_->InitializeMouseSettings(pref_service_.get(),
                                         /*mouse_policies=*/{}, &mouse);
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);

  pref_handler_->InitializeMouseSettings(pref_service_.get(), policies, &mouse);
  EXPECT_EQ(!kDefaultSwapRight, mouse.settings->swap_right);
  mouse.settings->swap_right = kDefaultSwapRight;
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);

  const auto* settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_FALSE(settings_dict->contains(prefs::kMouseSettingSwapRight));
}

TEST_F(MousePrefHandlerTest,
       ExistingMouse_ManagedEnterprisePolicy_GetsNewPolicy) {
  mojom::MousePolicies policies;
  policies.swap_right_policy = mojom::InputDeviceSettingsPolicy::New(
      mojom::PolicyStatus::kManaged, !kDefaultSwapRight);

  mojom::Mouse mouse;
  mouse.device_key = kMouseKey1;

  pref_handler_->InitializeMouseSettings(pref_service_.get(),
                                         /*mouse_policies=*/{}, &mouse);
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);

  mouse.settings->swap_right = !kDefaultSwapRight;
  CallUpdateMouseSettings(kMouseKey1, *mouse.settings);

  pref_handler_->InitializeMouseSettings(pref_service_.get(), policies, &mouse);
  EXPECT_EQ(!kDefaultSwapRight, mouse.settings->swap_right);
  mouse.settings->swap_right = kDefaultSwapRight;
  EXPECT_EQ(kMouseSettingsDefault, *mouse.settings);

  const auto* settings_dict = GetSettingsDict(kMouseKey1);
  EXPECT_TRUE(settings_dict->contains(prefs::kMouseSettingSwapRight));
  EXPECT_EQ(!kDefaultSwapRight,
            settings_dict->FindBool(prefs::kMouseSettingSwapRight).value());
}

TEST_F(MousePrefHandlerTest, UpdateButtonRemapping) {
  CallUpdateMouseSettings(kMouseKey1, kMouseSettingsDefault);
  const auto* button_remappings_list =
      pref_service_->GetDict(prefs::kMouseButtonRemappingsDictPref)
          .FindList(kMouseKey1);
  ASSERT_EQ(0u, button_remappings_list->size());

  std::vector<mojom::ButtonRemappingPtr> button_remappings;
  button_remappings.push_back(button_remapping2.Clone());
  mojom::MouseSettings kUpdatedMouseSettings(
      /*swap_right=*/kDefaultSwapRight,
      /*sensitivity=*/kDefaultSensitivity,
      /*reverse_scrolling=*/kDefaultReverseScrolling,
      /*acceleration_enabled=*/kDefaultAccelerationEnabled,
      /*scroll_sensitivity=*/kDefaultScrollSensitivity,
      /*scroll_acceleration=*/kDefaultScrollAccelerationEnabled,
      /*button_remappings=*/mojo::Clone(button_remappings));

  CallUpdateMouseSettings(kMouseKey1, kUpdatedMouseSettings);
  auto* updated_button_remappings_list =
      pref_service_->GetDict(prefs::kMouseButtonRemappingsDictPref)
          .FindList(kMouseKey1);
  ASSERT_NE(nullptr, updated_button_remappings_list);
  ASSERT_EQ(1u, updated_button_remappings_list->size());
  const auto& updated_button_remapping_dict =
      (*updated_button_remappings_list)[0].GetDict();
  EXPECT_EQ(
      button_remappings[0]->name,
      *updated_button_remapping_dict.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(static_cast<int>(button_remappings[0]->button->get_vkey()),
            *updated_button_remapping_dict.FindInt(
                prefs::kButtonRemappingKeyboardCode));
  EXPECT_EQ(
      static_cast<int>(
          button_remappings[0]->remapping_action->get_static_shortcut_action()),
      *updated_button_remapping_dict.FindInt(
          prefs::kButtonRemappingStaticShortcutAction));

  CallUpdateMouseSettings(
      kMouseKey1, kUpdatedMouseSettings,
      mojom::CustomizationRestriction::kDisallowCustomizations);
  auto* updated_button_remappings_list2 =
      pref_service_->GetDict(prefs::kMouseButtonRemappingsDictPref)
          .FindList(kMouseKey1);
  ASSERT_NE(nullptr, updated_button_remappings_list2);
  ASSERT_EQ(0u, updated_button_remappings_list2->size());

  CallUpdateMouseSettings(
      kMouseKey1, kUpdatedMouseSettings,
      mojom::CustomizationRestriction::kDisableKeyEventRewrites);
  auto* updated_button_remappings_list3 =
      pref_service_->GetDict(prefs::kMouseButtonRemappingsDictPref)
          .FindList(kMouseKey1);
  ASSERT_NE(nullptr, updated_button_remappings_list3);
  ASSERT_EQ(0u, updated_button_remappings_list3->size());
}

TEST_F(MousePrefHandlerTest, InitializeButtonRemappings) {
  mojom::MouseSettingsPtr settings = CallInitializeMouseSettings(kMouseKey1);
  ASSERT_NE(nullptr, settings.get());
  EXPECT_EQ(0u, settings->button_remappings.size());

  const auto& button_remappings_dict =
      pref_service_->GetDict(prefs::kMouseButtonRemappingsDictPref);
  auto* button_remappings_list = button_remappings_dict.FindList(kMouseKey1);
  ASSERT_NE(nullptr, button_remappings_list);
  ASSERT_EQ(0u, button_remappings_list->size());

  // Update the button remappings pref dict to mock adding a new
  // button remapping in the future.
  std::vector<mojom::ButtonRemappingPtr> button_remappings;
  button_remappings.push_back(button_remapping2.Clone());
  base::Value::Dict updated_button_remappings_dict;
  updated_button_remappings_dict.Set(
      kMouseKey1, ConvertButtonRemappingArrayToList(
                      button_remappings,
                      mojom::CustomizationRestriction::kAllowCustomizations));

  pref_service_->SetDict(prefs::kMouseButtonRemappingsDictPref,
                         updated_button_remappings_dict.Clone());

  // updated_settings have updated button remappings since mouse
  // has kAllowCustomizations customization restriction.
  mojom::MouseSettingsPtr updated_settings =
      CallInitializeMouseSettings(kMouseKey1);
  EXPECT_EQ(button_remappings, updated_settings->button_remappings);

  // updated_settings2 have no button remappings since mouse2
  // has kDisallowCustomizations customization restriction.
  mojom::MouseSettingsPtr updated_settings2 = CallInitializeMouseSettings(
      kMouseKey1, mojom::CustomizationRestriction::kDisallowCustomizations);
  EXPECT_EQ(std::vector<mojom::ButtonRemappingPtr>(),
            updated_settings2->button_remappings);

  // updated_settings3 have no button remappings since mouse3
  // has kDisableKeyEventRewrites customization restriction and the
  // button is a VKey.
  mojom::MouseSettingsPtr updated_settings3 = CallInitializeMouseSettings(
      kMouseKey1, mojom::CustomizationRestriction::kDisableKeyEventRewrites);
  EXPECT_EQ(std::vector<mojom::ButtonRemappingPtr>(),
            updated_settings3->button_remappings);
}

TEST_F(MousePrefHandlerTest, RememberDefaultsFromLastUpdatedSettings) {
  mojom::MouseSettingsPtr settings = CallInitializeMouseSettings(kMouseKey1);
  settings->swap_right = !kDefaultSwapRight;
  settings->sensitivity = 1;
  CallUpdateMouseSettings(kMouseKey1, *settings);
  CallUpdateDefaultMouseSettings(kMouseKey1, *settings);

  mojom::MouseSettingsPtr settings2 = CallInitializeMouseSettings(kMouseKey2);
  EXPECT_EQ(*settings2, *settings);

  settings2->sensitivity = 5;
  CallUpdateDefaultMouseSettings(kMouseKey2, *settings2);

  mojom::MouseSettingsPtr settings_duplicate =
      CallInitializeMouseSettings(kMouseKey1);
  EXPECT_EQ(*settings, *settings_duplicate);
}

TEST_F(MousePrefHandlerTest, SettingsUpdateMetricTest) {
  const auto settings1 = CallInitializeMouseSettings(kMouseKey1);

  // When its the first device of the type the category should be kFirstEver.
  {
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kMouseUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kMouseKey1));

    auto metrics_info =
        SettingsUpdatedMetricsInfo::FromDict(*metric_dict.FindDict(kMouseKey1));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kFirstEver,
              metrics_info->category());
  }

  // When its taken off the the defaults, the category should be kDefault.
  {
    CallUpdateDefaultMouseSettings(kMouseKey1, *settings1);
    CallInitializeMouseSettings(kMouseKey2);
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kMouseUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kMouseKey2));

    auto metrics_info =
        SettingsUpdatedMetricsInfo::FromDict(*metric_dict.FindDict(kMouseKey2));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kDefault,
              metrics_info->category());
  }

  // When its taken from synced prefs on a different device, category should
  // match.
  {
    auto devices_dict =
        pref_service_->GetDict(prefs::kMouseDeviceSettingsDictPref).Clone();
    devices_dict.Set(kMouseKey3, base::Value::Dict());
    pref_service_->SetDict(prefs::kMouseDeviceSettingsDictPref,
                           std::move(devices_dict));

    CallInitializeMouseSettings(kMouseKey3);
    const auto& metric_dict =
        pref_service_->GetDict(prefs::kMouseUpdateSettingsMetricInfo);
    ASSERT_TRUE(metric_dict.contains(kMouseKey3));

    auto metrics_info =
        SettingsUpdatedMetricsInfo::FromDict(*metric_dict.FindDict(kMouseKey3));
    ASSERT_TRUE(metrics_info);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kSynced,
              metrics_info->category());
  }
}

class MouseSettingsPrefConversionTest
    : public MousePrefHandlerTest,
      public testing::WithParamInterface<
          std::tuple<std::string, const mojom::MouseSettings*>> {
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
  raw_ptr<const mojom::MouseSettings> settings_;
};

INSTANTIATE_TEST_SUITE_P(
    // Empty to simplify gtest output
    ,
    MouseSettingsPrefConversionTest,
    testing::Combine(testing::Values(kMouseKey1, kMouseKey2),
                     testing::Values(&kMouseSettings1, &kMouseSettings2)));

TEST_P(MouseSettingsPrefConversionTest, CheckConversion) {
  CallUpdateMouseSettings(device_key_, *settings_);

  const auto* settings_dict = GetSettingsDict(device_key_);
  CheckMouseSettingsAndDictAreEqual(*settings_, *settings_dict);
}

}  // namespace ash
