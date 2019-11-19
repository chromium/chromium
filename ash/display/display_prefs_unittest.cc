// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_prefs.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/display/display_configuration_observer.h"
#include "ash/display/display_util.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/math_constants.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/display_manager_utilities.h"
#include "ui/display/manager/json_converter.h"
#include "ui/display/manager/test/touch_device_manager_test_api.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {

namespace {
const char kPrimaryIdKey[] = "primary-id";
const char kPositionKey[] = "position";
const char kOffsetKey[] = "offset";
const char kPlacementDisplayIdKey[] = "placement.display_id";
const char kPlacementParentDisplayIdKey[] = "placement.parent_display_id";

bool IsRotationLocked() {
  return ash::Shell::Get()->screen_orientation_controller()->rotation_locked();
}

bool CompareTouchAssociations(
    const display::TouchDeviceManager::TouchAssociationMap& map_1,
    const display::TouchDeviceManager::TouchAssociationMap& map_2) {
  if (map_1.size() != map_2.size())
    return false;
  // Each iterator instance |entry| is a pair of type
  // std::pair<display::TouchDeviceIdentifier,
  //           display::TouchDeviceManager::AssociationInfoMap>
  for (const auto& entry : map_1) {
    if (!map_2.count(entry.first))
      return false;

    const auto& association_info_map_1 = entry.second;
    const auto& association_info_map_2 = map_2.at(entry.first);
    if (association_info_map_1.size() != association_info_map_2.size())
      return false;

    // Each iterator instance is a pair of type:
    // std::pair<int64_t, display::TouchDeviceManager::TouchAssociationInfo>
    for (const auto& info_1 : association_info_map_1) {
      if (!association_info_map_2.count(info_1.first))
        return false;

      const auto& info_2 = association_info_map_2.at(info_1.first);
      if (!(info_1.second.timestamp == info_2.timestamp &&
            info_1.second.calibration_data == info_2.calibration_data)) {
        return false;
      }
    }
  }
  return true;
}

bool ComparePortAssociations(
    const display::TouchDeviceManager::PortAssociationMap& map_1,
    const display::TouchDeviceManager::PortAssociationMap& map_2) {
  if (map_1.size() != map_2.size())
    return false;
  auto it_1 = map_1.begin();
  auto it_2 = map_2.begin();
  while (it_1 != map_1.end()) {
    if (it_1->first != it_2->first)
      return false;
    if (it_1->second != it_2->second)
      return false;
    it_1++;
    it_2++;
  }
  return true;
}

}  // namespace

class DisplayPrefsTest : public AshTestBase {
 protected:
  DisplayPrefsTest() {}

  ~DisplayPrefsTest() override {}

  void SetUp() override {
    disable_provide_local_state();
    AshTestBase::SetUp();
    DisplayPrefs::RegisterLocalStatePrefs(local_state_.registry());
    display_prefs()->SetPrefServiceForTest(&local_state_);
    observer_ = std::make_unique<DisplayConfigurationObserver>();
    observer_->OnDisplaysInitialized();
  }

  void TearDown() override {
    observer_.reset();
    AshTestBase::TearDown();
  }

  void LoggedInAsUser() { SimulateUserLogin("user1@test.com"); }

  void LoggedInAsGuest() { SimulateGuestLogin(); }

  void LoadDisplayPreferences() { display_prefs()->LoadDisplayPreferences(); }

  // Do not use the implementation of display_prefs.cc directly to avoid
  // notifying the update to the system.
  void StoreDisplayLayoutPrefForList(
      const display::DisplayIdList& list,
      display::DisplayPlacement::Position position,
      int offset,
      int64_t primary_id) {
    std::string name = display::DisplayIdListToString(list);
    DictionaryPrefUpdate update(local_state(), prefs::kSecondaryDisplays);
    display::DisplayLayout display_layout;
    display_layout.placement_list.emplace_back(position, offset);
    display_layout.primary_id = primary_id;

    DCHECK(!name.empty());

    base::DictionaryValue* pref_data = update.Get();
    std::unique_ptr<base::Value> layout_value(new base::DictionaryValue());
    base::Value* value = nullptr;
    if (pref_data->Get(name, &value) && value != nullptr)
      layout_value.reset(value->DeepCopy());
    if (display::DisplayLayoutToJson(display_layout, layout_value.get()))
      pref_data->Set(name, std::move(layout_value));
  }

  void StoreDisplayPropertyForList(const display::DisplayIdList& list,
                                   const std::string& key,
                                   std::unique_ptr<base::Value> value) {
    std::string name = display::DisplayIdListToString(list);

    DictionaryPrefUpdate update(local_state(), prefs::kSecondaryDisplays);
    base::DictionaryValue* pref_data = update.Get();

    base::Value* layout_value = pref_data->FindKey(name);
    if (layout_value) {
      static_cast<base::DictionaryValue*>(layout_value)
          ->Set(key, std::move(value));
    } else {
      std::unique_ptr<base::DictionaryValue> layout_value(
          new base::DictionaryValue());
      layout_value->SetBoolean(key, value != nullptr);
      pref_data->Set(name, std::move(layout_value));
    }
  }

  void StoreDisplayBoolPropertyForList(const display::DisplayIdList& list,
                                       const std::string& key,
                                       bool value) {
    StoreDisplayPropertyForList(list, key,
                                std::make_unique<base::Value>(value));
  }

  void StoreDisplayLayoutPrefForList(const display::DisplayIdList& list,
                                     display::DisplayPlacement::Position layout,
                                     int offset) {
    StoreDisplayLayoutPrefForList(list, layout, offset, list[0]);
  }

  void StoreDisplayOverscan(int64_t id, const gfx::Insets& insets) {
    DictionaryPrefUpdate update(local_state(), prefs::kDisplayProperties);
    const std::string name = base::NumberToString(id);

    base::DictionaryValue* pref_data = update.Get();
    auto insets_value = std::make_unique<base::DictionaryValue>();
    insets_value->SetInteger("insets_top", insets.top());
    insets_value->SetInteger("insets_left", insets.left());
    insets_value->SetInteger("insets_bottom", insets.bottom());
    insets_value->SetInteger("insets_right", insets.right());
    pref_data->Set(name, std::move(insets_value));
  }

  display::Display::Rotation GetRotation() {
    return ash::Shell::Get()
        ->display_manager()
        ->GetDisplayInfo(display::Display::InternalDisplayId())
        .GetRotation(display::Display::RotationSource::ACCELEROMETER);
  }

  void StoreExternalDisplayMirrorInfo(
      const std::set<int64_t>& external_display_mirror_info) {
    ListPrefUpdate update(local_state(), prefs::kExternalDisplayMirrorInfo);
    base::ListValue* pref_data = update.Get();
    pref_data->Clear();
    for (const auto& id : external_display_mirror_info)
      pref_data->Append(base::Value(base::NumberToString(id)));
  }

  std::string GetRegisteredDisplayPlacementStr(
      const display::DisplayIdList& list) {
    return ash::Shell::Get()
        ->display_manager()
        ->layout_store()
        ->GetRegisteredDisplayLayout(list)
        .placement_list[0]
        .ToString();
  }

  chromeos::DisplayPowerState GetRequestedPowerState() const {
    return ash::Shell::Get()->display_configurator()->GetRequestedPowerState();
  }
  PrefService* local_state() { return &local_state_; }
  DisplayPrefs* display_prefs() { return ash::Shell::Get()->display_prefs(); }

 private:
  std::unique_ptr<WindowTreeHostManager::Observer> observer_;
  TestingPrefServiceSimple local_state_;
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(DisplayPrefsTest);
};

class DisplayPrefsTestGuest : public DisplayPrefsTest {
 public:
  DisplayPrefsTestGuest() { set_start_session(false); }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisplayPrefsTestGuest);
};

TEST_F(DisplayPrefsTest, ListedLayoutOverrides) {
  UpdateDisplay("100x100,200x200");

  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  display::DisplayIdList dummy_list =
      display::test::CreateDisplayIdList2(list[0], list[1] + 1);
  ASSERT_NE(list[0], dummy_list[1]);

  StoreDisplayLayoutPrefForList(list, display::DisplayPlacement::TOP, 20);
  StoreDisplayLayoutPrefForList(dummy_list, display::DisplayPlacement::LEFT,
                                30);
  display_prefs()->StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kFirstExecAfterBoot);
  LoadDisplayPreferences();

  // requested_power_state_ should be chromeos::DISPLAY_POWER_ALL_ON at boot
  const base::Optional<chromeos::DisplayPowerState> requested_power_state =
      ash::Shell::Get()
          ->display_configurator()
          ->GetRequestedPowerStateForTest();
  ASSERT_NE(base::nullopt, requested_power_state);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, *requested_power_state);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, GetRequestedPowerState());

  Shell::Get()->display_manager()->UpdateDisplays();
  // Check if the layout settings are notified to the system properly.
  // The new layout overrides old layout.
  // Inverted one of for specified pair (id1, id2).  Not used for the list
  // (id1, dummy_id) since dummy_id is not connected right now.
  EXPECT_EQ("id=2200000001, parent=2200000000, top, 20",
            Shell::Get()
                ->display_manager()
                ->GetCurrentDisplayLayout()
                .placement_list[0]
                .ToString());
  EXPECT_EQ("id=2200000001, parent=2200000000, top, 20",
            GetRegisteredDisplayPlacementStr(list));
  EXPECT_EQ("id=2200000002, parent=2200000000, left, 30",
            GetRegisteredDisplayPlacementStr(dummy_list));
}

TEST_F(DisplayPrefsTest, BasicStores) {
  ash::WindowTreeHostManager* window_tree_host_manager =
      ash::Shell::Get()->window_tree_host_manager();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  // For each configuration change, we store mirror info only for external
  // displays. So set internal display first before adding display.
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         id1);
  UpdateDisplay("200x200*2, 400x300#400x400|300x200*1.25");
  int64_t id2 = display_manager()->GetSecondaryDisplay().id();
  int64_t dummy_id = id2 + 1;
  ASSERT_NE(id1, dummy_id);

  LoggedInAsUser();

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 10));
  const display::DisplayLayout& layout =
      display_manager()->GetCurrentDisplayLayout();
  EXPECT_EQ(display::DisplayPlacement::TOP, layout.placement_list[0].position);
  EXPECT_EQ(10, layout.placement_list[0].offset);

  display::DisplayLayoutBuilder dummy_layout_builder(id1);
  dummy_layout_builder.SetSecondaryPlacement(
      dummy_id, display::DisplayPlacement::LEFT, 20);
  std::unique_ptr<display::DisplayLayout> dummy_layout(
      dummy_layout_builder.Build());
  display::DisplayIdList list =
      display::test::CreateDisplayIdList2(id1, dummy_id);
  display_prefs()->StoreDisplayLayoutPrefForTest(list, *dummy_layout);

  // Can't switch to a display that does not exist.
  window_tree_host_manager->SetPrimaryDisplayId(dummy_id);
  EXPECT_NE(dummy_id, display::Screen::GetScreen()->GetPrimaryDisplay().id());

  window_tree_host_manager->SetOverscanInsets(id1, gfx::Insets(10, 11, 12, 13));
  display_manager()->SetDisplayRotation(id1, display::Display::ROTATE_90,
                                        display::Display::RotationSource::USER);

  constexpr float zoom_factor_1 = 1.f / 2.25f;
  constexpr float zoom_factor_2 = 1.60f;

  display_manager()->UpdateZoomFactor(id1, zoom_factor_1);
  display_manager()->UpdateZoomFactor(id2, zoom_factor_2);

  // Set touch calibration data for display |id2|.
  uint32_t id_1 = 1234;
  uint32_t port_1 = 5678;
  const display::TouchDeviceIdentifier touch_device_identifier_1(id_1, port_1);
  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad_1 = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size touch_size_1(200, 150);

  uint32_t id_2 = 2345;
  uint32_t port_2 = 3456;
  const display::TouchDeviceIdentifier touch_device_identifier_2(id_2, port_2);
  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad_2 = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size touch_size_2(150, 150);

  // Create a 3rd touch device which has the same primary ID as the 2nd touch
  // device but is connected to a different port.
  uint32_t port_3 = 1357;
  const display::TouchDeviceIdentifier touch_device_identifier_3(id_2, port_3);

  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_1, touch_size_1, touch_device_identifier_1);
  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_2, touch_size_2, touch_device_identifier_2);
  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_2, touch_size_1, touch_device_identifier_3);

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* layout_value = nullptr;
  std::string key = base::NumberToString(id1) + "," + base::NumberToString(id2);
  std::string dummy_key =
      base::NumberToString(id1) + "," + base::NumberToString(dummy_id);
  EXPECT_TRUE(displays->GetDictionary(dummy_key, &layout_value));

  display::DisplayLayout stored_layout;
  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  ASSERT_EQ(1u, stored_layout.placement_list.size());

  EXPECT_EQ(dummy_layout->placement_list[0].position,
            stored_layout.placement_list[0].position);
  EXPECT_EQ(dummy_layout->placement_list[0].offset,
            stored_layout.placement_list[0].offset);

  const base::ListValue* external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(0U, external_display_mirror_info->GetSize());

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id1), &property));
  int rotation = 0;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(1, rotation);

  double display_zoom_1;
  EXPECT_TRUE(property->GetDouble("display_zoom_factor", &display_zoom_1));
  EXPECT_NEAR(display_zoom_1, zoom_factor_1, 0.0001);

  // Internal display never registered the resolution.
  int width = 0, height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  int top = 0, left = 0, bottom = 0, right = 0;
  EXPECT_TRUE(property->GetInteger("insets_top", &top));
  EXPECT_TRUE(property->GetInteger("insets_left", &left));
  EXPECT_TRUE(property->GetInteger("insets_bottom", &bottom));
  EXPECT_TRUE(property->GetInteger("insets_right", &right));
  EXPECT_EQ(10, top);
  EXPECT_EQ(11, left);
  EXPECT_EQ(12, bottom);
  EXPECT_EQ(13, right);

  display::TouchDeviceManager* tdm = display_manager()->touch_device_manager();
  display::test::TouchDeviceManagerTestApi tdm_test_api(tdm);
  display::TouchDeviceManager::TouchAssociationMap
      expected_touch_associations_map = tdm->touch_associations();
  display::TouchDeviceManager::PortAssociationMap
      expected_port_associations_map = tdm->port_associations();
  tdm_test_api.ResetTouchDeviceManager();

  EXPECT_FALSE(CompareTouchAssociations(expected_touch_associations_map,
                                        tdm->touch_associations()));
  EXPECT_FALSE(ComparePortAssociations(expected_port_associations_map,
                                       tdm->port_associations()));

  display_prefs()->LoadTouchAssociationPreferenceForTest();

  display::TouchDeviceManager::TouchAssociationMap
      actual_touch_associations_map = tdm->touch_associations();

  EXPECT_TRUE(CompareTouchAssociations(actual_touch_associations_map,
                                       expected_touch_associations_map));
  EXPECT_TRUE(ComparePortAssociations(expected_port_associations_map,
                                      tdm->port_associations()));

  std::string touch_str;

  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(0, rotation);

  double display_zoom_2;
  EXPECT_TRUE(property->GetDouble("display_zoom_factor", &display_zoom_2));
  EXPECT_NEAR(display_zoom_2, zoom_factor_2, 0.0001);

  EXPECT_FALSE(property->GetInteger("insets_top", &top));
  EXPECT_FALSE(property->GetInteger("insets_left", &left));
  EXPECT_FALSE(property->GetInteger("insets_bottom", &bottom));
  EXPECT_FALSE(property->GetInteger("insets_right", &right));

  // Resolution is saved only when the resolution is set
  // by DisplayManager::SetDisplayMode
  width = 0;
  height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  display::ManagedDisplayMode mode(gfx::Size(300, 200), 60.0f, false, true,
                                   1.25f /* device_scale_factor */);
  display_manager()->SetDisplayMode(id2, mode);

  window_tree_host_manager->SetPrimaryDisplayId(id2);

  EXPECT_EQ(id2, display::Screen::GetScreen()->GetPrimaryDisplay().id());

  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id1), &property));
  width = 0;
  height = 0;
  // Internal display shouldn't store its resolution.
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // External display's resolution must be stored this time because
  // it's not best.
  int device_scale_factor = 0;
  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_TRUE(
      property->GetInteger("device-scale-factor", &device_scale_factor));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);
  EXPECT_EQ(1250, device_scale_factor);

  // The layout is swapped.
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));

  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  ASSERT_EQ(1u, stored_layout.placement_list.size());
  const display::DisplayPlacement& stored_placement =
      stored_layout.placement_list[0];
  EXPECT_EQ(display::DisplayPlacement::BOTTOM, stored_placement.position);
  EXPECT_EQ(-10, stored_placement.offset);
  EXPECT_EQ(id1, stored_placement.display_id);
  EXPECT_EQ(id2, stored_placement.parent_display_id);
  EXPECT_EQ(id2, stored_layout.primary_id);

  if (true)
    return;

  std::string primary_id_str;
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::NumberToString(id2), primary_id_str);

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(ash::Shell::Get()->display_manager(),
                                         display::DisplayPlacement::BOTTOM,
                                         20));

  UpdateDisplay("1+0-200x200*2,1+0-200x200");
  // Mirrored.
  int offset = 0;
  std::string position;
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("bottom", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(20, offset);
  std::string id;
  EXPECT_TRUE(layout_value->GetString(kPlacementDisplayIdKey, &id));
  EXPECT_EQ(base::NumberToString(id1), id);
  EXPECT_TRUE(layout_value->GetString(kPlacementParentDisplayIdKey, &id));
  EXPECT_EQ(base::NumberToString(id2), id);

  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::NumberToString(id2), primary_id_str);

  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id1), &property));
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, external_display_mirror_info->GetSize());
  EXPECT_EQ(base::NumberToString(id2),
            external_display_mirror_info->GetList()[0].GetString());

  // External display's selected resolution must not change
  // by mirroring.
  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);

  // Set new display's selected resolution.
  display_manager()->RegisterDisplayProperty(
      id2 + 1, display::Display::ROTATE_0, nullptr, gfx::Size(500, 400), 1.0f,
      1.0f, 60.f, false);

  UpdateDisplay("200x200*2, 600x500#600x500|500x400");

  // Update key as the 2nd display gets new id.
  id2 = display_manager()->GetSecondaryDisplay().id();
  key = base::NumberToString(id1) + "," + base::NumberToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("right", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(0, offset);
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::NumberToString(id1), primary_id_str);

  // Best resolution should not be saved.
  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id2), &property));
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // Set yet another new display's selected resolution.
  display_manager()->RegisterDisplayProperty(
      id2 + 1, display::Display::ROTATE_0, nullptr, gfx::Size(500, 400), 1.0f,
      1.0f, 60.f, false);
  // Disconnect 2nd display first to generate new id for external display.
  UpdateDisplay("200x200*2");
  UpdateDisplay("200x200*2, 500x400#600x500|500x400%60.0f");
  // Update key as the 2nd display gets new id.
  id2 = display_manager()->GetSecondaryDisplay().id();
  key = base::NumberToString(id1) + "," + base::NumberToString(id2);
  EXPECT_TRUE(displays->GetDictionary(key, &layout_value));
  EXPECT_TRUE(layout_value->GetString(kPositionKey, &position));
  EXPECT_EQ("right", position);
  EXPECT_TRUE(layout_value->GetInteger(kOffsetKey, &offset));
  EXPECT_EQ(0, offset);
  EXPECT_TRUE(layout_value->GetString(kPrimaryIdKey, &primary_id_str));
  EXPECT_EQ(base::NumberToString(id1), primary_id_str);

  // External display's selected resolution must be updated.
  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id2), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(500, width);
  EXPECT_EQ(400, height);
}

TEST_F(DisplayPrefsTest, PreventStore) {
  ResolutionNotificationController::SuppressTimerForTest();
  LoggedInAsUser();
  UpdateDisplay("400x300#500x400|400x300|300x200");
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  // Set display's resolution in single display. It creates the notification and
  // display preferences should not stored meanwhile.
  ash::Shell* shell = ash::Shell::Get();

  display::ManagedDisplayMode old_mode(gfx::Size(400, 300));
  display::ManagedDisplayMode new_mode(gfx::Size(500, 400));
  EXPECT_TRUE(shell->resolution_notification_controller()
                  ->PrepareNotificationAndSetDisplayMode(
                      id, old_mode, new_mode,
                      ash::mojom::DisplayConfigSource::kUser,
                      base::OnceClosure()));
  UpdateDisplay("500x400#500x400|400x300|300x200");

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id), &property));
  int width = 0, height = 0;
  EXPECT_FALSE(property->GetInteger("width", &width));
  EXPECT_FALSE(property->GetInteger("height", &height));

  // Revert the change.
  shell->resolution_notification_controller()->RevertResolutionChange(false);
  base::RunLoop().RunUntilIdle();

  // The specified resolution will be stored by SetDisplayMode.
  ash::Shell::Get()->display_manager()->SetDisplayMode(
      id, display::ManagedDisplayMode(gfx::Size(300, 200), 60.0f, false, true));
  UpdateDisplay("300x200#500x400|400x300|300x200");

  property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(base::NumberToString(id), &property));
  EXPECT_TRUE(property->GetInteger("width", &width));
  EXPECT_TRUE(property->GetInteger("height", &height));
  EXPECT_EQ(300, width);
  EXPECT_EQ(200, height);
}

TEST_F(DisplayPrefsTest, StoreForSwappedDisplay) {
  UpdateDisplay("100x100,200x200");
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  int64_t id2 = display_manager()->GetSecondaryDisplay().id();

  LoggedInAsUser();

  SwapPrimaryDisplay();
  ASSERT_EQ(id1, display_manager()->GetSecondaryDisplay().id());

  std::string key = base::NumberToString(id1) + "," + base::NumberToString(id2);
  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  // Initial saved value is swapped.
  {
    const base::DictionaryValue* new_value = nullptr;
    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    display::DisplayLayout stored_layout;
    EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
    ASSERT_EQ(1u, stored_layout.placement_list.size());
    const display::DisplayPlacement& stored_placement =
        stored_layout.placement_list[0];
    EXPECT_EQ(display::DisplayPlacement::LEFT, stored_placement.position);
    EXPECT_EQ(0, stored_placement.offset);
    EXPECT_EQ(id1, stored_placement.display_id);
    EXPECT_EQ(id2, stored_placement.parent_display_id);
    EXPECT_EQ(id2, stored_layout.primary_id);
  }

  // Updating layout with primary swapped should save the correct value.
  {
    display_manager()->SetLayoutForCurrentDisplays(
        display::test::CreateDisplayLayout(display_manager(),
                                           display::DisplayPlacement::TOP, 10));
    const base::DictionaryValue* new_value = nullptr;
    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    display::DisplayLayout stored_layout;
    EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
    ASSERT_EQ(1u, stored_layout.placement_list.size());
    const display::DisplayPlacement& stored_placement =
        stored_layout.placement_list[0];
    EXPECT_EQ(display::DisplayPlacement::TOP, stored_placement.position);
    EXPECT_EQ(10, stored_placement.offset);
    EXPECT_EQ(id1, stored_placement.display_id);
    EXPECT_EQ(id2, stored_placement.parent_display_id);
    EXPECT_EQ(id2, stored_layout.primary_id);
  }

  // Swapping primary will save the swapped value.
  {
    SwapPrimaryDisplay();
    const base::DictionaryValue* new_value = nullptr;
    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    display::DisplayLayout stored_layout;

    EXPECT_TRUE(displays->GetDictionary(key, &new_value));
    EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
    ASSERT_EQ(1u, stored_layout.placement_list.size());
    const display::DisplayPlacement& stored_placement =
        stored_layout.placement_list[0];
    EXPECT_EQ(display::DisplayPlacement::BOTTOM, stored_placement.position);
    EXPECT_EQ(-10, stored_placement.offset);
    EXPECT_EQ(id2, stored_placement.display_id);
    EXPECT_EQ(id1, stored_placement.parent_display_id);
    EXPECT_EQ(id1, stored_layout.primary_id);
  }
}

TEST_F(DisplayPrefsTestGuest, DisplayPrefsTestGuest) {
  ash::WindowTreeHostManager* window_tree_host_manager =
      ash::Shell::Get()->window_tree_host_manager();

  UpdateDisplay("200x200*2,200x200");

  LoggedInAsGuest();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(
      ash::Shell::Get()->display_manager(), id1);
  int64_t id2 = display_manager()->GetSecondaryDisplay().id();
  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 10));
  const float scale = 1.25f;
  display_manager()->UpdateZoomFactor(id1, 1.f / scale);
  window_tree_host_manager->SetPrimaryDisplayId(id2);
  int64_t new_primary = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  window_tree_host_manager->SetOverscanInsets(new_primary,
                                              gfx::Insets(10, 11, 12, 13));
  display_manager()->SetDisplayRotation(new_primary,
                                        display::Display::ROTATE_90,
                                        display::Display::RotationSource::USER);

  // Does not store the preferences locally.
  EXPECT_FALSE(local_state()
                   ->FindPreference(prefs::kSecondaryDisplays)
                   ->HasUserSetting());
  EXPECT_FALSE(local_state()
                   ->FindPreference(prefs::kDisplayProperties)
                   ->HasUserSetting());

  // Settings are still notified to the system.
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(id2, screen->GetPrimaryDisplay().id());
  const display::DisplayPlacement& placement =
      display_manager()->GetCurrentDisplayLayout().placement_list[0];
  EXPECT_EQ(display::DisplayPlacement::BOTTOM, placement.position);
  EXPECT_EQ(-10, placement.offset);
  const display::Display& primary_display = screen->GetPrimaryDisplay();
  EXPECT_EQ("178x176", primary_display.bounds().size().ToString());
  EXPECT_EQ(display::Display::ROTATE_90, primary_display.rotation());

  const display::ManagedDisplayInfo& info1 =
      display_manager()->GetDisplayInfo(id1);
  EXPECT_FLOAT_EQ(1.f / scale, info1.zoom_factor());

  const display::ManagedDisplayInfo& info_primary =
      display_manager()->GetDisplayInfo(new_primary);
  EXPECT_EQ(display::Display::ROTATE_90, info_primary.GetActiveRotation());
  EXPECT_EQ(1.0f, info_primary.zoom_factor());
}

TEST_F(DisplayPrefsTest, StorePowerStateNoLogin) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  // Stores display prefs without login, which still stores the power state.
  display_prefs()->MaybeStoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPrefsTest, StorePowerStateGuest) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  LoggedInAsGuest();
  display_prefs()->MaybeStoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPrefsTest, StorePowerStateNormalUser) {
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayPowerState));

  LoggedInAsUser();
  display_prefs()->MaybeStoreDisplayPrefs();
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayPowerState));
}

TEST_F(DisplayPrefsTest, DisplayPowerStateAfterRestart) {
  display_prefs()->StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences();
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            GetRequestedPowerState());
}

TEST_F(DisplayPrefsTest, DontSaveAndRestoreAllOff) {
  display_prefs()->StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  LoadDisplayPreferences();
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            GetRequestedPowerState());

  display_prefs()->StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            GetRequestedPowerState());
  EXPECT_EQ("internal_off_external_on",
            local_state()->GetString(prefs::kDisplayPowerState));

  // Don't try to load
  local_state()->SetString(prefs::kDisplayPowerState, "all_off");
  LoadDisplayPreferences();
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            GetRequestedPowerState());
}

// Tests that display configuration changes caused by TabletModeController
// are not saved.
TEST_F(DisplayPrefsTest, DontSaveTabletModeControllerRotations) {
  ash::Shell* shell = ash::Shell::Get();
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  LoggedInAsUser();
  // Populate the properties.
  display_manager()->SetDisplayRotation(display::Display::InternalDisplayId(),
                                        display::Display::ROTATE_180,
                                        display::Display::RotationSource::USER);
  // Reset property to avoid rotation lock
  display_manager()->SetDisplayRotation(display::Display::InternalDisplayId(),
                                        display::Display::ROTATE_0,
                                        display::Display::RotationSource::USER);

  // Open up 270 degrees to trigger tablet mode
  scoped_refptr<AccelerometerUpdate> update(new AccelerometerUpdate());
  update->Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, false, 0.0f, 0.0f,
              -base::kMeanGravityFloat);
  update->Set(ACCELEROMETER_SOURCE_SCREEN, false, 0.0f, base::kMeanGravityFloat,
              0.0f);
  ash::TabletModeController* controller =
      ash::Shell::Get()->tablet_mode_controller();
  controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(controller->InTabletMode());

  // Trigger 90 degree rotation
  update->Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, false,
              base::kMeanGravityFloat, 0.0f, 0.0f);
  update->Set(ACCELEROMETER_SOURCE_SCREEN, false, base::kMeanGravityFloat, 0.0f,
              0.0f);
  controller->OnAccelerometerUpdated(update);
  shell->screen_orientation_controller()->OnAccelerometerUpdated(update);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  const base::DictionaryValue* property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(
      base::NumberToString(display::Display::InternalDisplayId()), &property));
  int rotation = -1;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(display::Display::ROTATE_0, rotation);

  // Trigger a save, the acceleration rotation should not be saved as the user
  // rotation.
  display_prefs()->MaybeStoreDisplayPrefs();
  properties = local_state()->GetDictionary(prefs::kDisplayProperties);
  property = nullptr;
  EXPECT_TRUE(properties->GetDictionary(
      base::NumberToString(display::Display::InternalDisplayId()), &property));
  rotation = -1;
  EXPECT_TRUE(property->GetInteger("rotation", &rotation));
  EXPECT_EQ(display::Display::ROTATE_0, rotation);
}

// Tests that the rotation state is saved without a user being logged in.
TEST_F(DisplayPrefsTest, StoreRotationStateNoLogin) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  bool current_rotation_lock = IsRotationLocked();
  display_prefs()->StoreDisplayRotationPrefsForTest(GetRotation(),
                                                    current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that the rotation state is saved when a guest is logged in.
TEST_F(DisplayPrefsTest, StoreRotationStateGuest) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  display_prefs()->StoreDisplayRotationPrefsForTest(GetRotation(),
                                                    current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that the rotation state is saved when a normal user is logged in.
TEST_F(DisplayPrefsTest, StoreRotationStateNormalUser) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  display_prefs()->StoreDisplayRotationPrefsForTest(GetRotation(),
                                                    current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
  EXPECT_EQ(current_rotation_lock, rotation_lock);

  int orientation;
  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(properties->GetInteger("orientation", &orientation));
  EXPECT_EQ(current_rotation, orientation);
}

// Tests that rotation state is loaded without a user being logged in, and that
// entering tablet mode applies the state.
TEST_F(DisplayPrefsTest, LoadRotationNoLogin) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  bool initial_rotation_lock = IsRotationLocked();
  ASSERT_FALSE(initial_rotation_lock);
  display::Display::Rotation initial_rotation =
      GetCurrentInternalDisplayRotation();
  ASSERT_EQ(display::Display::ROTATE_0, initial_rotation);

  display_prefs()->StoreDisplayRotationPrefsForTest(GetRotation(),
                                                    initial_rotation_lock);
  ASSERT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  display_prefs()->StoreDisplayRotationPrefsForTest(display::Display::ROTATE_90,
                                                    true);
  LoadDisplayPreferences();

  bool display_rotation_lock =
      display_manager()->registered_internal_display_rotation_lock();
  bool display_rotation =
      display_manager()->registered_internal_display_rotation();
  EXPECT_TRUE(display_rotation_lock);
  EXPECT_EQ(display::Display::ROTATE_90, display_rotation);

  bool rotation_lock = IsRotationLocked();
  display::Display::Rotation before_tablet_mode_rotation =
      GetCurrentInternalDisplayRotation();

  // Settings should not be applied until tablet mode activates
  EXPECT_FALSE(rotation_lock);
  EXPECT_EQ(display::Display::ROTATE_0, before_tablet_mode_rotation);

  // Open up 270 degrees to trigger tablet mode
  scoped_refptr<AccelerometerUpdate> update(new AccelerometerUpdate());
  update->Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, false, 0.0f, 0.0f,
              -base::kMeanGravityFloat);
  update->Set(ACCELEROMETER_SOURCE_SCREEN, false, 0.0f, base::kMeanGravityFloat,
              0.0f);
  ash::TabletModeController* tablet_mode_controller =
      ash::Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(tablet_mode_controller->InTabletMode());
  bool screen_orientation_rotation_lock = IsRotationLocked();
  display::Display::Rotation tablet_mode_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(screen_orientation_rotation_lock);
  EXPECT_EQ(display::Display::ROTATE_90, tablet_mode_rotation);
}

// Tests that rotation lock being set causes the rotation state to be saved.
TEST_F(DisplayPrefsTest, RotationLockTriggersStore) {
  display::Display::SetInternalDisplayId(
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  ash::Shell::Get()->screen_orientation_controller()->ToggleUserRotationLock();

  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::DictionaryValue* properties =
      local_state()->GetDictionary(prefs::kDisplayRotationLock);
  bool rotation_lock;
  EXPECT_TRUE(properties->GetBoolean("lock", &rotation_lock));
}

TEST_F(DisplayPrefsTest, SaveUnifiedMode) {
  LoggedInAsUser();
  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("200x200,100x100");
  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  EXPECT_EQ(
      "400x200",
      display::Screen::GetScreen()->GetPrimaryDisplay().size().ToString());

  const base::DictionaryValue* secondary_displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = nullptr;
  EXPECT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));

  display::DisplayLayout stored_layout;
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);

  const base::DictionaryValue* displays =
      local_state()->GetDictionary(prefs::kDisplayProperties);
  int64_t unified_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  EXPECT_FALSE(
      displays->GetDictionary(base::NumberToString(unified_id), &new_value));

  display::test::SetDisplayResolution(display_manager(), unified_id,
                                      gfx::Size(200, 100));
  EXPECT_EQ(
      "200x100",
      display::Screen::GetScreen()->GetPrimaryDisplay().size().ToString());
  EXPECT_FALSE(
      displays->GetDictionary(base::NumberToString(unified_id), &new_value));

  // Mirror mode should remember if the default mode was unified.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
  ASSERT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);

  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  ASSERT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);

  // Exit unified mode.
  display_manager()->SetDefaultMultiDisplayModeForCurrentDisplays(
      display::DisplayManager::EXTENDED);
  ASSERT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_FALSE(stored_layout.default_unified);
}

TEST_F(DisplayPrefsTest, RestoreUnifiedMode) {
  const int64_t first_display_id = 210000001;
  const int64_t second_display_id = 220000002;
  display::ManagedDisplayInfo first_display_info =
      display::CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 500));
  display::ManagedDisplayInfo second_display_info =
      display::CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 500));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.emplace_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  ash::Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      first_display_id);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  display::DisplayIdList list =
      display::test::CreateDisplayIdList2(first_display_id, second_display_id);
  StoreDisplayBoolPropertyForList(list, "default_unified", true);
  StoreDisplayPropertyForList(
      list, "primary-id",
      std::make_unique<base::Value>(base::NumberToString(first_display_id)));
  LoadDisplayPreferences();

  // Should not restore to unified unless unified desktop is enabled.
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Restored to unified.
  display_manager()->SetUnifiedDesktopEnabled(true);
  StoreDisplayBoolPropertyForList(list, "default_unified", true);
  LoadDisplayPreferences();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Remove the second display.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Restored to mirror, then unified.
  std::set<int64_t> external_display_mirror_info;
  external_display_mirror_info.emplace(
      display::GetDisplayIdWithoutOutputIndex(second_display_id));
  StoreExternalDisplayMirrorInfo(external_display_mirror_info);
  StoreDisplayBoolPropertyForList(list, "default_unified", true);
  LoadDisplayPreferences();
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Remove the second display.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Sanity check. Restore to extended.
  external_display_mirror_info.clear();
  StoreExternalDisplayMirrorInfo(external_display_mirror_info);
  StoreDisplayBoolPropertyForList(list, "default_unified", false);
  LoadDisplayPreferences();
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
}

TEST_F(DisplayPrefsTest, SaveThreeDisplays) {
  LoggedInAsUser();
  UpdateDisplay("200x200,200x200,300x300");

  display::DisplayIdList list = display_manager()->GetCurrentDisplayIdList();
  ASSERT_EQ(3u, list.size());

  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0],
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[0],
                              display::DisplayPlacement::BOTTOM, 100);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  const base::DictionaryValue* secondary_displays =
      local_state()->GetDictionary(prefs::kSecondaryDisplays);
  const base::DictionaryValue* new_value = nullptr;
  EXPECT_TRUE(secondary_displays->GetDictionary(
      display::DisplayIdListToString(list), &new_value));
}

TEST_F(DisplayPrefsTest, RestoreThreeDisplays) {
  LoggedInAsUser();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(3, id1, id1 + 1, id1 + 2);

  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0], display::DisplayPlacement::LEFT,
                              0);
  builder.AddDisplayPlacement(list[2], list[1],
                              display::DisplayPlacement::BOTTOM, 100);
  display_prefs()->StoreDisplayLayoutPrefForTest(list, *builder.Build());
  LoadDisplayPreferences();

  UpdateDisplay("200x200,200x200,300x300");
  display::DisplayIdList new_list =
      display_manager()->GetCurrentDisplayIdList();
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ(list[0], new_list[0]);
  ASSERT_EQ(list[1], new_list[1]);
  ASSERT_EQ(list[2], new_list[2]);

  EXPECT_EQ(gfx::Rect(0, 0, 200, 200),
            display_manager()->GetDisplayForId(list[0]).bounds());
  EXPECT_EQ(gfx::Rect(-200, 0, 200, 200),
            display_manager()->GetDisplayForId(list[1]).bounds());
  EXPECT_EQ(gfx::Rect(-100, 200, 300, 300),
            display_manager()->GetDisplayForId(list[2]).bounds());
}

TEST_F(DisplayPrefsTest, LegacyTouchCalibrationDataSupport) {
  UpdateDisplay("800x600,1200x800");
  LoggedInAsUser();
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size touch_size(200, 150);
  display::TouchCalibrationData data(point_pair_quad, touch_size);

  display_prefs()->StoreLegacyTouchDataForTest(id, data);

  display::TouchDeviceManager* tdm = display_manager()->touch_device_manager();
  display::test::TouchDeviceManagerTestApi tdm_test_api(tdm);
  tdm_test_api.ResetTouchDeviceManager();

  display_prefs()->LoadTouchAssociationPreferenceForTest();

  const display::TouchDeviceManager::TouchAssociationMap& association_map =
      tdm->touch_associations();

  const display::TouchDeviceIdentifier& fallback_identifier =
      display::TouchDeviceIdentifier::GetFallbackTouchDeviceIdentifier();

  EXPECT_TRUE(association_map.count(fallback_identifier));
  EXPECT_TRUE(association_map.at(fallback_identifier).count(id));
  EXPECT_EQ(association_map.at(fallback_identifier).at(id).calibration_data,
            data);

  int64_t id_2 = display_manager()->GetSecondaryDisplay().id();
  gfx::Size touch_size_2(300, 300);
  display::TouchCalibrationData data_2(point_pair_quad, touch_size_2);

  display::TouchDeviceIdentifier identifier(12345);
  display_manager()->SetTouchCalibrationData(id_2, point_pair_quad,
                                             touch_size_2, identifier);

  EXPECT_TRUE(tdm->touch_associations().count(identifier));
  EXPECT_TRUE(tdm->touch_associations().at(identifier).count(id_2));
  EXPECT_EQ(tdm->touch_associations().at(identifier).at(id_2).calibration_data,
            data_2);

  tdm_test_api.ResetTouchDeviceManager();
  EXPECT_TRUE(tdm->touch_associations().empty());

  display_prefs()->LoadTouchAssociationPreferenceForTest();

  EXPECT_TRUE(tdm->touch_associations().count(fallback_identifier));
  EXPECT_TRUE(tdm->touch_associations().at(fallback_identifier).count(id));
  EXPECT_EQ(
      tdm->touch_associations().at(fallback_identifier).at(id).calibration_data,
      data);
  EXPECT_TRUE(tdm->touch_associations().count(identifier));
  EXPECT_TRUE(tdm->touch_associations().at(identifier).count(id_2));
  EXPECT_EQ(tdm->touch_associations().at(identifier).at(id_2).calibration_data,
            data_2);
}

TEST_F(DisplayPrefsTest, ExternalDisplayMirrorInfo) {
  LoggedInAsUser();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kFirstExecAfterBoot);

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t first_display_id = 210000001;
  constexpr int64_t second_display_id = 220000002;
  const int64_t first_display_masked_id =
      display::GetDisplayIdWithoutOutputIndex(first_display_id);
  const int64_t second_display_masked_id =
      display::GetDisplayIdWithoutOutputIndex(second_display_id);
  display::ManagedDisplayInfo first_display_info =
      display::CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 500));
  display::ManagedDisplayInfo second_display_info =
      display::CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 500));
  std::vector<display::ManagedDisplayInfo> display_info_list;

  // There's no external display now.
  display_info_list.push_back(display::CreateDisplayInfo(
      internal_display_id, gfx::Rect(0, 0, 100, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Add first display id to the external display mirror info.
  std::set<int64_t> external_display_mirror_info;
  external_display_mirror_info.emplace(first_display_masked_id);
  StoreExternalDisplayMirrorInfo(external_display_mirror_info);
  LoadDisplayPreferences();
  const base::ListValue* pref_external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->GetSize());
  EXPECT_EQ(base::NumberToString(first_display_masked_id),
            pref_external_display_mirror_info->GetList()[0].GetString());

  // Add first display, mirror mode restores and the external display mirror
  // info does not change.
  display_info_list.emplace_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->GetSize());
  EXPECT_EQ(base::NumberToString(first_display_masked_id),
            pref_external_display_mirror_info->GetList()[0].GetString());

  // Add second display, mirror mode persists and the second display id is added
  // to the external display mirror info.
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(2U, pref_external_display_mirror_info->GetSize());
  EXPECT_EQ(base::NumberToString(first_display_masked_id),
            pref_external_display_mirror_info->GetList()[0].GetString());
  EXPECT_EQ(base::NumberToString(second_display_masked_id),
            pref_external_display_mirror_info->GetList()[1].GetString());

  // Disconnect all external displays.
  display_info_list.erase(display_info_list.begin() + 1,
                          display_info_list.end());
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Clear external display mirror info and only add second display id to it.
  external_display_mirror_info.clear();
  external_display_mirror_info.emplace(second_display_masked_id);
  StoreExternalDisplayMirrorInfo(external_display_mirror_info);
  LoadDisplayPreferences();
  pref_external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->GetSize());
  EXPECT_EQ(base::NumberToString(second_display_masked_id),
            pref_external_display_mirror_info->GetList()[0].GetString());

  // Add first display, mirror mode is off and the external display mirror info
  // does not change.
  display_info_list.emplace_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->GetSize());
  EXPECT_EQ(base::NumberToString(second_display_masked_id),
            pref_external_display_mirror_info->GetList()[0].GetString());

  // Add second display, mirror mode remains off and the second display id is
  // removed from the external display mirror info.
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(0U, pref_external_display_mirror_info->GetSize());
}

TEST_F(DisplayPrefsTest, ExternalDisplayConnectedBeforeLoadingPrefs) {
  LoggedInAsUser();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      chromeos::switches::kFirstExecAfterBoot);

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t external_display_id = 210000001;
  display::ManagedDisplayInfo external_display_info =
      display::CreateDisplayInfo(external_display_id,
                                 gfx::Rect(1, 1, 500, 500));

  // Both internal and external displays connect before the prefs are loaded.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(display::CreateDisplayInfo(
      internal_display_id, gfx::Rect(0, 0, 100, 100)));
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(2u, display_manager()->num_connected_displays());

  // Add external display id to the external display mirror info.
  std::set<int64_t> external_display_mirror_info;
  const int64_t external_display_masked_id =
      display::GetDisplayIdWithoutOutputIndex(external_display_id);
  external_display_mirror_info.emplace(external_display_masked_id);
  StoreExternalDisplayMirrorInfo(external_display_mirror_info);

  // Load the preferences and simulate a native display reconfiguration. Expect
  // that we are mirroring now.
  LoadDisplayPreferences();

  // Simulate a change in display configuration between loading the prefs, and
  // reconfiguring after the prefs have been loaded. Make sure that the external
  // display mirror configs are not overwritten, and the loaded prefs will be
  // applied.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);

  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
}

TEST_F(DisplayPrefsTest, DisplayMixedMirrorMode) {
  LoggedInAsUser();

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t first_display_id = 210000001;
  constexpr int64_t second_display_id = 220000002;
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(display::CreateDisplayInfo(
      internal_display_id, gfx::Rect(0, 0, 100, 100)));
  display_info_list.push_back(
      display::CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 500)));
  display_info_list.push_back(
      display::CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 500)));

  // Store mixed mirror mode parameters which specify mirroring from the
  // internal display to the first external display.
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(first_display_id);
  base::Optional<display::MixedMirrorModeParams> mixed_params(
      base::in_place, internal_display_id, dst_ids);
  display_prefs()->StoreDisplayMixedMirrorModeParamsForTest(mixed_params);
  LoadDisplayPreferences();

  // Connect both first and second external display. Mixed mirror mode is
  // restored.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(internal_display_id, display_manager()->mirroring_source_id());
  display::DisplayIdList destination_ids =
      display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(first_display_id, destination_ids[0]);

  // Check the preferences.
  const base::DictionaryValue* pref_data =
      local_state()->GetDictionary(prefs::kDisplayMixedMirrorModeParams);
  EXPECT_EQ(base::NumberToString(internal_display_id),
            pref_data->FindKey("mirroring_source_id")->GetString());
  const base::Value* destination_ids_value =
      pref_data->FindKey("mirroring_destination_ids");
  EXPECT_EQ(1U, destination_ids_value->GetList().size());
  EXPECT_EQ(base::NumberToString(first_display_id),
            destination_ids_value->GetList()[0].GetString());

  // Overwrite current mixed mirror mode with a new configuration. (Mirror from
  // the first external display to the second external display)
  dst_ids.clear();
  dst_ids.emplace_back(second_display_id);
  base::Optional<display::MixedMirrorModeParams> new_mixed_params(
      base::in_place, first_display_id, dst_ids);
  display_manager()->SetMirrorMode(display::MirrorMode::kMixed,
                                   new_mixed_params);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(first_display_id, display_manager()->mirroring_source_id());
  destination_ids = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(second_display_id, destination_ids[0]);

  // Check the preferences.
  pref_data =
      local_state()->GetDictionary(prefs::kDisplayMixedMirrorModeParams);
  EXPECT_EQ(base::NumberToString(first_display_id),
            pref_data->FindKey("mirroring_source_id")->GetString());
  destination_ids_value = pref_data->FindKey("mirroring_destination_ids");
  EXPECT_EQ(1U, destination_ids_value->GetList().size());
  EXPECT_EQ(base::NumberToString(second_display_id),
            destination_ids_value->GetList()[0].GetString());

  // Turn off mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Check the preferences.
  pref_data =
      local_state()->GetDictionary(prefs::kDisplayMixedMirrorModeParams);
  EXPECT_TRUE(pref_data->empty());
}

}  // namespace ash
