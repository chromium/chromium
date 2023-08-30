// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/pref_handlers/graphics_tablet_pref_handler_impl.h"

#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/accelerator_actions.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "ash/system/input_device_settings/input_device_settings_pref_names.h"
#include "ash/system/input_device_settings/input_device_settings_utils.h"
#include "ash/test/ash_test_base.h"
#include "base/test/scoped_feature_list.h"
#include "components/account_id/account_id.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/known_user.h"

namespace ash {

namespace {
const std::string kGraphicsTabletKey1 = "device_key1";
const std::string kGraphicsTabletKey2 = "device_key2";

constexpr char kUserEmail[] = "example@email.com";
const AccountId account_id_1 = AccountId::FromUserEmail(kUserEmail);

const mojom::ButtonRemapping button_remapping1(
    /*name=*/"test1",
    /*button=*/
    mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kBack),
    /*remapping_action=*/
    mojom::RemappingAction::NewAction(ash::AcceleratorAction::kBrightnessDown));
const mojom::ButtonRemapping button_remapping2(
    /*name=*/"test2",
    /*button=*/
    mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kLeft),
    /*remapping_action=*/
    mojom::RemappingAction::NewKeyEvent(
        mojom::KeyEvent::New(::ui::KeyboardCode::VKEY_0, 1, 2, 3)));
}  // namespace

class GraphicsTabletPrefHandlerTest : public AshTestBase {
 public:
  GraphicsTabletPrefHandlerTest() = default;
  GraphicsTabletPrefHandlerTest(const GraphicsTabletPrefHandlerTest&) = delete;
  GraphicsTabletPrefHandlerTest& operator=(
      const GraphicsTabletPrefHandlerTest&) = delete;
  ~GraphicsTabletPrefHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({features::kPeripheralCustomization,
                                           features::kInputDeviceSettingsSplit},
                                          {});
    AshTestBase::SetUp();
    InitializePrefService();
    pref_handler_ = std::make_unique<GraphicsTabletPrefHandlerImpl>();
  }

  void TearDown() override {
    pref_handler_.reset();
    AshTestBase::TearDown();
  }

  void InitializePrefService() {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();

    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kGraphicsTabletTabletButtonRemappingsDictPref);
    pref_service_->registry()->RegisterDictionaryPref(
        prefs::kGraphicsTabletPenButtonRemappingsDictPref);
  }

  void CallUpdateGraphicsTabletSettings(
      const std::string& device_key,
      const mojom::GraphicsTabletSettings& settings) {
    mojom::GraphicsTabletPtr graphics_tablet = mojom::GraphicsTablet::New();
    graphics_tablet->settings = settings.Clone();
    graphics_tablet->device_key = device_key;

    pref_handler_->UpdateGraphicsTabletSettings(pref_service_.get(),
                                                *graphics_tablet);
  }

  mojom::GraphicsTabletSettingsPtr
  CallInitializeLoginScreenGraphicsTabletSettings(
      const AccountId& account_id,
      const mojom::GraphicsTablet& graphics_tablet) {
    const auto graphics_tablet_ptr = graphics_tablet.Clone();

    pref_handler_->InitializeLoginScreenGraphicsTabletSettings(
        local_state(), account_id, graphics_tablet_ptr.get());
    return std::move(graphics_tablet_ptr->settings);
  }

  void CallUpdateLoginScreenGraphicsTabletSettings(
      const AccountId& account_id,
      const std::string& device_key,
      const mojom::GraphicsTabletSettings& settings) {
    mojom::GraphicsTabletPtr graphics_tablet = mojom::GraphicsTablet::New();
    graphics_tablet->settings = settings.Clone();
    pref_handler_->UpdateLoginScreenGraphicsTabletSettings(
        local_state(), account_id, *graphics_tablet);
  }

  mojom::GraphicsTabletSettingsPtr CallInitializeGraphicsTabletSettings(
      const std::string& device_key) {
    mojom::GraphicsTabletPtr graphics_tablet = mojom::GraphicsTablet::New();
    graphics_tablet->device_key = device_key;

    pref_handler_->InitializeGraphicsTabletSettings(pref_service_.get(),
                                                    graphics_tablet.get());
    return std::move(graphics_tablet->settings);
  }

  user_manager::KnownUser known_user() {
    return user_manager::KnownUser(local_state());
  }

  bool HasLoginScreenGraphicsTabletButtonRemappingList(AccountId account_id) {
    const auto* tablet_button_remapping_list = known_user().FindPath(
        account_id,
        prefs::kGraphicsTabletLoginScreenTabletButtonRemappingListPref);
    const auto* pen_button_remapping_list = known_user().FindPath(
        account_id,
        prefs::kGraphicsTabletLoginScreenPenButtonRemappingListPref);
    return tablet_button_remapping_list &&
           tablet_button_remapping_list->is_list() &&
           pen_button_remapping_list && pen_button_remapping_list->is_list();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<GraphicsTabletPrefHandlerImpl> pref_handler_;
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
};

TEST_F(GraphicsTabletPrefHandlerTest,
       InitializeLoginScreenGraphicsTabletSettings) {
  mojom::GraphicsTablet graphics_tablet;
  graphics_tablet.device_key = kGraphicsTabletKey1;
  mojom::GraphicsTabletSettingsPtr settings =
      CallInitializeLoginScreenGraphicsTabletSettings(account_id_1,
                                                      graphics_tablet);

  EXPECT_FALSE(HasLoginScreenGraphicsTabletButtonRemappingList(account_id_1));
  EXPECT_EQ(mojom::GraphicsTabletSettings::New(), settings);
}

TEST_F(GraphicsTabletPrefHandlerTest,
       LoginScreenPrefsNotPersistedWhenFlagIsDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kInputDeviceSettingsSplit);
  mojom::GraphicsTablet graphics_tablet;
  graphics_tablet.device_key = kGraphicsTabletKey1;
  CallInitializeLoginScreenGraphicsTabletSettings(account_id_1,
                                                  graphics_tablet);
  EXPECT_FALSE(HasLoginScreenGraphicsTabletButtonRemappingList(account_id_1));
}

TEST_F(GraphicsTabletPrefHandlerTest, UpdateLoginScreenGraphicsTabletSettings) {
  mojom::GraphicsTablet graphics_tablet;
  graphics_tablet.device_key = kGraphicsTabletKey1;

  mojom::GraphicsTabletSettingsPtr settings =
      CallInitializeLoginScreenGraphicsTabletSettings(account_id_1,
                                                      graphics_tablet);
  EXPECT_FALSE(HasLoginScreenGraphicsTabletButtonRemappingList(account_id_1));

  // Create new graphics tablet settings.
  std::vector<mojom::ButtonRemappingPtr> tablet_button_remappings1;
  std::vector<mojom::ButtonRemappingPtr> pen_button_remappings1;
  tablet_button_remappings1.push_back(button_remapping1.Clone());
  const mojom::GraphicsTabletSettings kGraphicsTabletSettings1(
      /*tablet_button_remappings=*/mojo::Clone(tablet_button_remappings1),
      /*pen_button_remappings=*/mojo::Clone(pen_button_remappings1));

  CallUpdateLoginScreenGraphicsTabletSettings(account_id_1, kGraphicsTabletKey1,
                                              kGraphicsTabletSettings1);
  EXPECT_TRUE(HasLoginScreenGraphicsTabletButtonRemappingList(account_id_1));

  // Verify the updated tablet and pen button remapping lists.
  const auto* updated_tablet_button_remapping_list =
      GetLoginScreenButtonRemappingList(
          local_state(), account_id_1,
          prefs::kGraphicsTabletLoginScreenTabletButtonRemappingListPref);
  const auto* updated_pen_button_remapping_list =
      GetLoginScreenButtonRemappingList(
          local_state(), account_id_1,
          prefs::kGraphicsTabletLoginScreenPenButtonRemappingListPref);
  ASSERT_NE(nullptr, updated_tablet_button_remapping_list);
  ASSERT_EQ(1u, updated_tablet_button_remapping_list->size());
  const auto& tablet_button_remapping =
      (*updated_tablet_button_remapping_list)[0].GetDict();
  EXPECT_EQ(button_remapping1.name,
            *tablet_button_remapping.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(
      static_cast<int>(button_remapping1.button->get_customizable_button()),
      *tablet_button_remapping.FindInt(
          prefs::kButtonRemappingCustomizableButton));
  EXPECT_EQ(static_cast<int>(button_remapping1.remapping_action->get_action()),
            *tablet_button_remapping.FindInt(prefs::kButtonRemappingAction));
  ASSERT_NE(nullptr, updated_pen_button_remapping_list);
  ASSERT_EQ(0u, updated_pen_button_remapping_list->size());
}

TEST_F(GraphicsTabletPrefHandlerTest, UpdateSettings) {
  std::vector<mojom::ButtonRemappingPtr> tablet_button_remappings1;
  std::vector<mojom::ButtonRemappingPtr> pen_button_remappings1;
  tablet_button_remappings1.push_back(button_remapping1.Clone());
  tablet_button_remappings1.push_back(button_remapping2.Clone());
  pen_button_remappings1.push_back(button_remapping2.Clone());

  const mojom::GraphicsTabletSettings kGraphicsTabletSettings1(
      /*tablet_button_remappings=*/mojo::Clone(tablet_button_remappings1),
      /*pen_button_remappings=*/mojo::Clone(pen_button_remappings1));

  std::vector<mojom::ButtonRemappingPtr> tablet_button_remappings2;
  std::vector<mojom::ButtonRemappingPtr> pen_button_remappings2;
  tablet_button_remappings2.push_back(button_remapping2.Clone());
  pen_button_remappings2.push_back(button_remapping2.Clone());

  const mojom::GraphicsTabletSettings kGraphicsTabletSettings2(
      /*tablet_button_remappings=*/mojo::Clone(tablet_button_remappings2),
      /*pen_button_remappings=*/mojo::Clone(pen_button_remappings2));

  CallUpdateGraphicsTabletSettings(kGraphicsTabletKey1,
                                   kGraphicsTabletSettings1);
  CallUpdateGraphicsTabletSettings(kGraphicsTabletKey2,
                                   kGraphicsTabletSettings2);

  // Verify tablet button remapping pref dicts.
  const auto& tablet_button_remappings_dict = pref_service_->GetDict(
      prefs::kGraphicsTabletTabletButtonRemappingsDictPref);
  ASSERT_EQ(2u, tablet_button_remappings_dict.size());
  auto* graphics_tablet1_tablet_button_remappings =
      tablet_button_remappings_dict.FindList(kGraphicsTabletKey1);
  ASSERT_NE(nullptr, graphics_tablet1_tablet_button_remappings);
  ASSERT_EQ(2u, graphics_tablet1_tablet_button_remappings->size());
  auto* graphics_tablet2_tablet_button_remappings =
      tablet_button_remappings_dict.FindList(kGraphicsTabletKey2);
  ASSERT_NE(nullptr, graphics_tablet2_tablet_button_remappings);
  ASSERT_EQ(1u, graphics_tablet2_tablet_button_remappings->size());

  // Verify pen button remapping pref dicts.
  const auto& pen_button_remappings_dict =
      pref_service_->GetDict(prefs::kGraphicsTabletPenButtonRemappingsDictPref);
  ASSERT_EQ(2u, pen_button_remappings_dict.size());
  auto* graphics_tablet1_pen_button_remappings =
      pen_button_remappings_dict.FindList(kGraphicsTabletKey1);
  ASSERT_NE(nullptr, graphics_tablet1_pen_button_remappings);
  ASSERT_EQ(1u, graphics_tablet1_pen_button_remappings->size());
  auto* graphics_tablet2_pen_button_remappings =
      pen_button_remappings_dict.FindList(kGraphicsTabletKey2);
  ASSERT_NE(nullptr, graphics_tablet2_pen_button_remappings);
  ASSERT_EQ(1u, graphics_tablet2_pen_button_remappings->size());

  // Update button remapping1 and graphics tablet settings1.
  auto updated_button_remapping1 = button_remapping1.Clone();
  updated_button_remapping1->name = "new test name";
  updated_button_remapping1->button =
      mojom::Button::NewCustomizableButton(mojom::CustomizableButton::kExtra);
  updated_button_remapping1->remapping_action =
      mojom::RemappingAction::NewAction(
          ash::AcceleratorAction::kCycleBackwardMru);
  std::vector<mojom::ButtonRemappingPtr> updated_tablet_button_remappings1;
  std::vector<mojom::ButtonRemappingPtr> updated_pen_button_remappings1;
  updated_tablet_button_remappings1.push_back(
      updated_button_remapping1.Clone());
  const mojom::GraphicsTabletSettings kUpdatedGraphicsTabletSettings1(
      /*tablet_button_remappings=*/mojo::Clone(
          updated_tablet_button_remappings1),
      /*pen_button_remappings=*/mojo::Clone(updated_pen_button_remappings1));

  // Update graphics tablet1 settings1.
  CallUpdateGraphicsTabletSettings(kGraphicsTabletKey1,
                                   kUpdatedGraphicsTabletSettings1);

  // Verify if the graphics tablet1 tablet button remappings are updated.
  auto* updated_graphics_tablet1_tablet_button_remappings =
      pref_service_
          ->GetDict(prefs::kGraphicsTabletTabletButtonRemappingsDictPref)
          .FindList(kGraphicsTabletKey1);
  ASSERT_NE(nullptr, updated_graphics_tablet1_tablet_button_remappings);
  ASSERT_EQ(1u, updated_graphics_tablet1_tablet_button_remappings->size());
  ASSERT_TRUE(
      (*updated_graphics_tablet1_tablet_button_remappings)[0].is_dict());
  const auto& updated_dict =
      (*updated_graphics_tablet1_tablet_button_remappings)[0].GetDict();
  EXPECT_EQ(updated_button_remapping1->name,
            *updated_dict.FindString(prefs::kButtonRemappingName));
  EXPECT_EQ(static_cast<int>(
                updated_button_remapping1->button->get_customizable_button()),
            *updated_dict.FindInt(prefs::kButtonRemappingCustomizableButton));
  EXPECT_EQ(static_cast<int>(
                updated_button_remapping1->remapping_action->get_action()),
            *updated_dict.FindInt(prefs::kButtonRemappingAction));

  // Verify if the graphics tablet1 pen button remappings are updated.
  auto* updated_graphics_tablet1_pen_button_remappings =
      pref_service_->GetDict(prefs::kGraphicsTabletPenButtonRemappingsDictPref)
          .FindList(kGraphicsTabletKey1);
  ASSERT_NE(nullptr, updated_graphics_tablet1_pen_button_remappings);
  ASSERT_EQ(0u, updated_graphics_tablet1_pen_button_remappings->size());
}

TEST_F(GraphicsTabletPrefHandlerTest, InitializeSettings) {
  // There should be no button remappings in the settings for
  // a new graphics tablet by default. Verify if both the tablet and pen
  // button remappings pref lists are empty by default.
  mojom::GraphicsTabletSettingsPtr settings =
      CallInitializeGraphicsTabletSettings(kGraphicsTabletKey1);
  ASSERT_NE(nullptr, settings.get());
  EXPECT_EQ(0u, settings->tablet_button_remappings.size());
  EXPECT_EQ(0u, settings->pen_button_remappings.size());

  auto tablet_button_remappings_dict =
      pref_service_
          ->GetDict(prefs::kGraphicsTabletTabletButtonRemappingsDictPref)
          .Clone();
  auto* graphics_tablet1_tablet_button_remappings =
      tablet_button_remappings_dict.FindList(kGraphicsTabletKey1);
  ASSERT_NE(nullptr, graphics_tablet1_tablet_button_remappings);
  ASSERT_EQ(0u, graphics_tablet1_tablet_button_remappings->size());
  const auto& pen_button_remappings_dict =
      pref_service_->GetDict(prefs::kGraphicsTabletPenButtonRemappingsDictPref);
  auto* graphics_tablet1_pen_button_remappings =
      pen_button_remappings_dict.FindList(kGraphicsTabletKey1);
  ASSERT_NE(nullptr, graphics_tablet1_pen_button_remappings);
  ASSERT_EQ(0u, graphics_tablet1_pen_button_remappings->size());

  // Update the button remappings pref dicts to mock adding new tablet
  // and pen button remappings in the future.
  std::vector<mojom::ButtonRemappingPtr> tablet_button_remappings;
  std::vector<mojom::ButtonRemappingPtr> pen_button_remappings;
  tablet_button_remappings.push_back(button_remapping1.Clone());
  tablet_button_remappings.push_back(button_remapping2.Clone());
  pen_button_remappings.push_back(button_remapping1.Clone());
  base::Value::Dict updated_graphics_tablet1_tablet_button_remappings_dict;
  updated_graphics_tablet1_tablet_button_remappings_dict.Set(
      kGraphicsTabletKey1,
      ConvertButtonRemappingArrayToList(tablet_button_remappings));
  base::Value::Dict updated_graphics_tablet1_pen_button_remappings_dict;
  updated_graphics_tablet1_pen_button_remappings_dict.Set(
      kGraphicsTabletKey1,
      ConvertButtonRemappingArrayToList(pen_button_remappings));

  pref_service_->SetDict(
      prefs::kGraphicsTabletTabletButtonRemappingsDictPref,
      updated_graphics_tablet1_tablet_button_remappings_dict.Clone());
  pref_service_->SetDict(
      prefs::kGraphicsTabletPenButtonRemappingsDictPref,
      updated_graphics_tablet1_pen_button_remappings_dict.Clone());

  mojom::GraphicsTabletSettingsPtr updated_settings =
      CallInitializeGraphicsTabletSettings(kGraphicsTabletKey1);
  EXPECT_EQ(tablet_button_remappings,
            updated_settings->tablet_button_remappings);
  EXPECT_EQ(pen_button_remappings, updated_settings->pen_button_remappings);
}

}  // namespace ash
