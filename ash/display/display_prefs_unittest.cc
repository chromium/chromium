// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_prefs.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/display_configuration_observer.h"
#include "ash/display/display_util.h"
#include "ash/display/resolution_notification_controller.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/math_constants.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/user_manager/user_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/json_converter.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/test/touch_device_manager_test_api.h"
#include "ui/display/manager/util/display_manager_test_util.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/gfx/geometry/vector3d_f.h"

namespace ash {

using testing::DoubleEq;
using testing::ElementsAre;
using testing::Optional;

namespace {
const char kPrimaryIdKey[] = "primary-id";

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
 public:
  DisplayPrefsTest(const DisplayPrefsTest&) = delete;
  DisplayPrefsTest& operator=(const DisplayPrefsTest&) = delete;

 protected:
  DisplayPrefsTest() = default;
  ~DisplayPrefsTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    observer_ = std::make_unique<DisplayConfigurationObserver>();
    observer_->OnDisplaysInitialized();
  }

  void TearDown() override {
    observer_.reset();
    AshTestBase::TearDown();
  }

  void LoggedInAsUser() { SimulateUserLogin("user1@test.com"); }

  void LoggedInAsGuest() { SimulateGuestLogin(); }

  void LoggedInAsPublicAccount() {
    SimulateUserLogin("pa@test.com", user_manager::UserType::kPublicAccount);
  }

  void LoadDisplayPreferences() { display_prefs()->LoadDisplayPreferences(); }

  display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                                const gfx::Rect& bounds) {
    display::ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);
    // Each display should have at least one native mode.
    display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                     /*is_interlaced=*/true,
                                     /*native=*/true);
    info.SetManagedDisplayModes({mode});
    return info;
  }

  // Do not use the implementation of display_prefs.cc directly to avoid
  // notifying the update to the system.
  void StoreDisplayLayoutPrefForList(
      const display::DisplayIdList& list,
      display::DisplayPlacement::Position position,
      int offset,
      int64_t primary_id) {
    std::string name = display::DisplayIdListToString(list);
    ScopedDictPrefUpdate update(local_state(), prefs::kSecondaryDisplays);
    display::DisplayLayout display_layout;
    display_layout.placement_list.emplace_back(position, offset);
    display_layout.primary_id = primary_id;

    DCHECK(!name.empty());

    base::Value::Dict* layout_dict = update->EnsureDict(name);
    display::DisplayLayoutToJson(display_layout, *layout_dict);
  }

  void StoreDisplayPropertyForList(const display::DisplayIdList& list,
                                   const std::string& key,
                                   base::Value value) {
    std::string name = display::DisplayIdListToString(list);

    ScopedDictPrefUpdate update(local_state(), prefs::kSecondaryDisplays);
    if (base::Value::Dict* existing_layout_value = update->FindDict(name)) {
      existing_layout_value->Set(key, std::move(value));
    } else {
      base::Value::Dict layout_dict;
      layout_dict.Set(key, true);
      update->SetByDottedPath(name, std::move(layout_dict));
    }
  }

  void StoreDisplayBoolPropertyForList(const display::DisplayIdList& list,
                                       const std::string& key,
                                       bool value) {
    StoreDisplayPropertyForList(list, key, base::Value(value));
  }

  void StoreDisplayLayoutPrefForList(const display::DisplayIdList& list,
                                     display::DisplayPlacement::Position layout,
                                     int offset) {
    StoreDisplayLayoutPrefForList(list, layout, offset, list[0]);
  }

  void StoreDisplayOverscan(int64_t id, const gfx::Insets& insets) {
    ScopedDictPrefUpdate update(local_state(), prefs::kDisplayProperties);
    const std::string name = base::NumberToString(id);

    base::Value::Dict insets_value;
    insets_value.Set("insets_top", insets.top());
    insets_value.Set("insets_left", insets.left());
    insets_value.Set("insets_bottom", insets.bottom());
    insets_value.Set("insets_right", insets.right());
    update->Set(name, std::move(insets_value));
  }

  display::Display::Rotation GetRotation() {
    return Shell::Get()
        ->display_manager()
        ->GetDisplayInfo(display::Display::InternalDisplayId())
        .GetRotation(display::Display::RotationSource::ACCELEROMETER);
  }

  void StoreExternalDisplayMirrorInfo(
      const std::set<int64_t>& external_display_mirror_info) {
    ScopedListPrefUpdate update(local_state(),
                                prefs::kExternalDisplayMirrorInfo);
    base::Value::List& pref_data = update.Get();
    pref_data.clear();
    for (const auto& id : external_display_mirror_info)
      pref_data.Append(base::NumberToString(id));
  }

  std::string GetRegisteredDisplayPlacementStr(
      const display::DisplayIdList& list) {
    return Shell::Get()
        ->display_manager()
        ->layout_store()
        ->GetRegisteredDisplayLayout(list)
        .placement_list[0]
        .ToString();
  }

  const base::Value::Dict* ReadPropertiesForDisplay(int64_t display_id) {
    const base::Value::Dict& properties =
        local_state()->GetDict(prefs::kDisplayProperties);
    const base::Value::Dict* property =
        properties.FindDict(base::NumberToString(display_id));
    EXPECT_TRUE(property);
    return property;
  }

  void ExpectMixedMirrorModeParamsPrefs(
      int64_t source_id,
      const display::DisplayIdList& dest_ids) {
    std::vector<std::string> expected_dest_id_strs;
    for (const int64_t id : dest_ids) {
      expected_dest_id_strs.push_back(base::NumberToString(id));
    }
    SCOPED_TRACE(testing::Message()
                 << "Expected to read kDisplayMixedMirrorModeParams with "
                    "mirroring_source_id="
                 << source_id << " and mirroring_destination_ids="
                 << base::JoinString(expected_dest_id_strs, ","));
    const base::Value::Dict& prefs =
        local_state()->GetDict(prefs::kDisplayMixedMirrorModeParams);
    EXPECT_THAT(prefs.FindString("mirroring_source_id"),
                testing::Pointee(base::NumberToString(source_id)));
    const auto* mirror_ids = prefs.FindList("mirroring_destination_ids");
    ASSERT_TRUE(mirror_ids);
    display::DisplayIdList pref_dest_ids;
    for (const auto& value : *mirror_ids) {
      int64_t id;
      EXPECT_TRUE(base::StringToInt64(value.GetString(), &id));
      pref_dest_ids.push_back(id);
    }
    EXPECT_EQ(pref_dest_ids, dest_ids);
  }

  void ExpectExternalDisplayMirrorPrefs(const std::set<int64_t>& display_ids) {
    std::vector<std::string> expected_display_id_strs;
    for (const int64_t id : display_ids) {
      expected_display_id_strs.push_back(base::NumberToString(id));
    }
    SCOPED_TRACE(
        testing::Message()
        << "Expected to read kExternalDisplayMirrorInfo with list values="
        << base::JoinString(expected_display_id_strs, ","));
    const base::Value::List& prefs =
        local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
    std::set<int64_t> read_ids;
    for (const auto& value : prefs) {
      int64_t id;
      EXPECT_TRUE(base::StringToInt64(value.GetString(), &id));
      read_ids.insert(id);
    }
    EXPECT_EQ(read_ids, display_ids);
  }

  display::DisplayConfigurator* display_configurator() {
    return Shell::Get()->display_configurator();
  }

  DisplayPrefs* display_prefs() { return Shell::Get()->display_prefs(); }

 private:
  std::unique_ptr<display::DisplayManagerObserver> observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DisplayPrefsTestGuest : public DisplayPrefsTest {
 public:
  DisplayPrefsTestGuest() { set_start_session(false); }

  DisplayPrefsTestGuest(const DisplayPrefsTestGuest&) = delete;
  DisplayPrefsTestGuest& operator=(const DisplayPrefsTestGuest&) = delete;
};

TEST_F(DisplayPrefsTest, ListedLayoutOverrides) {
  UpdateDisplay("200x100,300x200");

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  display::DisplayIdList dummy_list = display::test::CreateDisplayIdList2(
      list[0], display::SynthesizeDisplayIdFromSeed(list[1]));
  ASSERT_NE(list[0], dummy_list[1]);

  StoreDisplayLayoutPrefForList(list, display::DisplayPlacement::TOP, 20);
  StoreDisplayLayoutPrefForList(dummy_list, display::DisplayPlacement::LEFT,
                                30);
  display_prefs()->StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFirstExecAfterBoot);
  LoadDisplayPreferences();

  // requested_power_state_ should be chromeos::DISPLAY_POWER_ALL_ON at boot
  const std::optional<chromeos::DisplayPowerState> requested_power_state =
      display_configurator()->GetRequestedPowerStateForTest();
  ASSERT_NE(std::nullopt, requested_power_state);
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON, *requested_power_state);
  // DisplayPowerState should be ignored at boot.
  EXPECT_EQ(chromeos::DISPLAY_POWER_ALL_ON,
            display_configurator()->GetRequestedPowerState());

  Shell::Get()->display_manager()->UpdateDisplays();
  // Check if the layout settings are notified to the system properly.
  // The new layout overrides old layout.
  // Inverted one of for specified pair (id1, id2).  Not used for the list
  // (id1, dummy_id) since dummy_id is not connected right now.
  EXPECT_EQ("id=2200000257, parent=2200000000, top, 20",
            Shell::Get()
                ->display_manager()
                ->GetCurrentDisplayLayout()
                .placement_list[0]
                .ToString());
  EXPECT_EQ("id=2200000257, parent=2200000000, top, 20",
            GetRegisteredDisplayPlacementStr(list));
  EXPECT_EQ("id=2200000258, parent=2200000000, left, 30",
            GetRegisteredDisplayPlacementStr(dummy_list));
}

TEST_F(DisplayPrefsTest, BasicStores) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  // For each configuration change, we store mirror info only for external
  // displays. So set internal display first before adding display.
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         id1);
  UpdateDisplay("300x200*2, 400x300#500x400|300x200*1.25");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();
  int64_t dummy_id = display::SynthesizeDisplayIdFromSeed(id2);
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

  window_tree_host_manager->SetOverscanInsets(
      id1, gfx::Insets::TLBR(10, 11, 12, 13));
  display_manager()->SetDisplayRotation(id1, display::Display::ROTATE_90,
                                        display::Display::RotationSource::USER);

  constexpr float zoom_factor_1 = 1.f / display::kDsf_2_252;
  constexpr float zoom_factor_2 = 1.60f;

  display_manager()->UpdateZoomFactor(id1, zoom_factor_1);
  display_manager()->UpdateZoomFactor(id2, zoom_factor_2);

  // Set touch calibration data for display |id2|.
  ui::TouchscreenDevice touchdevice(11, ui::InputDeviceType::INPUT_DEVICE_USB,
                                    std::string("test touch device"),
                                    gfx::Size(123, 456), 1);
  touchdevice.phys = "5678";
  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad_1 = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size touch_size_1(200, 150);

  ui::TouchscreenDevice touchdevice_2(12, ui::InputDeviceType::INPUT_DEVICE_USB,
                                      std::string("test touch device 2"),
                                      gfx::Size(132, 465), 1);
  touchdevice_2.phys = "3456";

  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad_2 = {
      {std::make_pair(gfx::Point(10, 10), gfx::Point(11, 12)),
       std::make_pair(gfx::Point(190, 10), gfx::Point(195, 8)),
       std::make_pair(gfx::Point(10, 90), gfx::Point(12, 94)),
       std::make_pair(gfx::Point(190, 90), gfx::Point(189, 88))}};
  gfx::Size touch_size_2(150, 150);

  // Create a 3rd touch device which has the same primary ID as the 2nd touch
  // device but is connected to a different port.
  ui::TouchscreenDevice touchdevice_3(15, ui::InputDeviceType::INPUT_DEVICE_USB,
                                      std::string("test touch device 3"),
                                      gfx::Size(231, 416), 1);
  touchdevice_3.phys = "1357";

  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_1, touch_size_1, touchdevice,
      /*apply_spatial_calibration=*/true);
  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_2, touch_size_2, touchdevice_2,
      /*apply_spatial_calibration=*/true);
  display_manager()->SetTouchCalibrationData(
      id2, point_pair_quad_2, touch_size_1, touchdevice_3,
      /*apply_spatial_calibration=*/true);

  const base::Value::Dict& displays =
      local_state()->GetDict(prefs::kSecondaryDisplays);
  std::string key = base::NumberToString(id1) + "," + base::NumberToString(id2);
  std::string dummy_key =
      base::NumberToString(id1) + "," + base::NumberToString(dummy_id);
  const base::Value::Dict* layout_value = displays.FindDict(dummy_key);
  EXPECT_TRUE(layout_value);

  display::DisplayLayout stored_layout;
  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  ASSERT_EQ(1u, stored_layout.placement_list.size());

  EXPECT_EQ(dummy_layout->placement_list[0].position,
            stored_layout.placement_list[0].position);
  EXPECT_EQ(dummy_layout->placement_list[0].offset,
            stored_layout.placement_list[0].offset);

  const base::Value::List* external_display_mirror_info =
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(0U, external_display_mirror_info->size());

  const base::Value::Dict& properties =
      local_state()->GetDict(prefs::kDisplayProperties);
  const base::Value::Dict* property =
      properties.FindDict(base::NumberToString(id1));
  EXPECT_TRUE(property);
  EXPECT_EQ(1, property->FindInt("rotation"));

  std::optional<double> display_zoom_1 =
      property->FindDouble("display_zoom_factor");
  ASSERT_TRUE(display_zoom_1);
  EXPECT_NEAR(*display_zoom_1, zoom_factor_1, 0.0001);

  const base::Value::Dict* display_zoom_dict_1 =
      property->FindDict("display_zoom_factor_map");
  std::optional<double> display_zoom_from_map_1 =
      display_zoom_dict_1->FindDouble("300x200");
  ASSERT_TRUE(display_zoom_from_map_1);
  EXPECT_NEAR(*display_zoom_from_map_1, zoom_factor_1, 0.0001);

  // Internal display never registered the resolution.
  EXPECT_FALSE(property->FindInt("width"));
  EXPECT_FALSE(property->FindInt("height"));

  EXPECT_EQ(10, property->FindInt("insets_top"));
  EXPECT_EQ(11, property->FindInt("insets_left"));
  EXPECT_EQ(12, property->FindInt("insets_bottom"));
  EXPECT_EQ(13, property->FindInt("insets_right"));

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

  property = properties.FindDict(base::NumberToString(id2));
  ASSERT_TRUE(property);
  EXPECT_EQ(0, property->FindInt("rotation"));

  std::optional<double> display_zoom_2 =
      property->FindDouble("display_zoom_factor");
  ASSERT_TRUE(display_zoom_2);
  EXPECT_NEAR(*display_zoom_2, zoom_factor_2, 0.0001);

  const base::Value::Dict* display_zoom_dict_2 =
      property->FindDict("display_zoom_factor_map");
  std::optional<double> display_zoom_from_map_2 =
      display_zoom_dict_2->FindDouble("500x400");
  ASSERT_TRUE(display_zoom_from_map_2);
  EXPECT_NEAR(*display_zoom_from_map_2, zoom_factor_2, 0.0001);

  EXPECT_FALSE(property->FindInt("insets_top"));
  EXPECT_FALSE(property->FindInt("insets_left"));
  EXPECT_FALSE(property->FindInt("insets_bottom"));
  EXPECT_FALSE(property->FindInt("insets_right"));

  // Resolution is saved only when the resolution is set
  // by DisplayManager::SetDisplayMode
  EXPECT_FALSE(property->FindInt("width"));
  EXPECT_FALSE(property->FindInt("height"));

  display::ManagedDisplayMode mode(gfx::Size(300, 200), 60.0f, false, false,
                                   1.25f /* device_scale_factor */);
  display_manager()->SetDisplayMode(id2, mode);

  window_tree_host_manager->SetPrimaryDisplayId(id2);

  EXPECT_EQ(id2, display::Screen::GetScreen()->GetPrimaryDisplay().id());

  property = properties.FindDict(base::NumberToString(id1));
  ASSERT_TRUE(property);
  // Internal display shouldn't store its resolution.
  EXPECT_FALSE(property->FindInt("width"));
  EXPECT_FALSE(property->FindInt("height"));

  // External display's resolution must be stored this time because
  // it's not best.
  property = properties.FindDict(base::NumberToString(id2));
  ASSERT_TRUE(property);
  EXPECT_EQ(property->FindInt("width"), 300);
  EXPECT_EQ(property->FindInt("height"), 200);
  EXPECT_EQ(property->FindInt("device-scale-factor"), 1250);

  // The layout is swapped.
  layout_value = displays.FindDict(key);
  ASSERT_TRUE(layout_value);

  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  ASSERT_EQ(1u, stored_layout.placement_list.size());
  const display::DisplayPlacement& stored_placement =
      stored_layout.placement_list[0];
  EXPECT_EQ(display::DisplayPlacement::BOTTOM, stored_placement.position);
  EXPECT_EQ(-10, stored_placement.offset);
  EXPECT_EQ(id1, stored_placement.display_id);
  EXPECT_EQ(id2, stored_placement.parent_display_id);
  EXPECT_EQ(id2, stored_layout.primary_id);

  const std::string* primary_id_str = layout_value->FindString(kPrimaryIdKey);
  ASSERT_TRUE(primary_id_str);
  EXPECT_EQ(base::NumberToString(id2), *primary_id_str);

  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(Shell::Get()->display_manager(),
                                         display::DisplayPlacement::BOTTOM,
                                         20));
  // Test Hardware Mirroring scenario.
  UpdateDisplay("1+0-300x200*2,1+0-300x200");
  EXPECT_FALSE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_TRUE(display_manager()->IsInHardwareMirrorMode());

  layout_value = displays.FindDict(key);
  ASSERT_TRUE(layout_value);
  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  EXPECT_EQ(display::DisplayPlacement::BOTTOM, stored_placement.position);
  EXPECT_EQ(20, stored_placement.offset);
  EXPECT_EQ(id1, stored_placement.display_id);
  EXPECT_EQ(id2, stored_placement.parent_display_id);

  property = properties.FindDict(base::NumberToString(id1));
  ASSERT_TRUE(property);
  EXPECT_FALSE(property->FindInt("width"));
  EXPECT_FALSE(property->FindInt("height"));

  external_display_mirror_info =
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, external_display_mirror_info->size());
  // ExternalDisplayInfo stores ID without output index.
  EXPECT_EQ(base::NumberToString(display::GetDisplayIdWithoutOutputIndex(id2)),
            (*external_display_mirror_info)[0].GetString());

  // External display's selected resolution must not change
  // by mirroring.
  property = properties.FindDict(base::NumberToString(id2));
  ASSERT_TRUE(property);
  EXPECT_EQ(300, property->FindInt("width"));
  EXPECT_EQ(200, property->FindInt("height"));

  // Set new display's selected resolution.
  display_manager()->RegisterDisplayProperty(
      display::SynthesizeDisplayIdFromSeed(id2), display::Display::ROTATE_0,
      /*overscan_insets=*/nullptr, /*resolution_in_pixels=*/gfx::Size(500, 400),
      /*device_scale_factor=*/1.0f, /*display_zoom_factor=*/1.0f,
      /*display_zoom_factor_map=*/{}, /*refresh_rate=*/60.f,
      /*is_interlaced=*/false,
      /*variable_refresh_rate_state=*/
      display::VariableRefreshRateState::kVrrNotCapable,
      /*vsync_rate_min=*/std::nullopt);

  UpdateDisplay("300x200*2, 600x500#600x500|500x400");
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Update key as the 2nd display gets new id.
  id2 = display_manager_test.GetSecondaryDisplay().id();
  key = base::NumberToString(id1) + "," + base::NumberToString(id2);

  layout_value = displays.FindDict(key);
  ASSERT_TRUE(layout_value);
  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  EXPECT_EQ(display::DisplayPlacement::RIGHT, stored_placement.position);
  EXPECT_EQ(0, stored_placement.offset);
  EXPECT_EQ(id1, stored_placement.parent_display_id);
  EXPECT_EQ(id2, stored_placement.display_id);
  primary_id_str = layout_value->FindString(kPrimaryIdKey);
  ASSERT_TRUE(primary_id_str);
  EXPECT_EQ(base::NumberToString(id1), *primary_id_str);

  // Best resolution should not be saved.
  property = properties.FindDict(base::NumberToString(id2));
  ASSERT_TRUE(property);
  EXPECT_FALSE(property->FindInt("width"));
  EXPECT_FALSE(property->FindInt("height"));

  // Set yet another new display's selected resolution.
  display_manager()->RegisterDisplayProperty(
      display::SynthesizeDisplayIdFromSeed(id2), display::Display::ROTATE_0,
      /*overscan_insets=*/nullptr, /*resolution_in_pixels=*/gfx::Size(500, 400),
      /*device_scale_factor=*/1.0f, /*display_zoom_factor=*/1.0f,
      /*display_zoom_factor_map=*/{}, /*refresh_rate=*/60.f,
      /*is_interlaced=*/false,
      /*variable_refresh_rate_state=*/
      display::VariableRefreshRateState::kVrrNotCapable,
      /*vsync_rate_min=*/std::nullopt);
  // Disconnect 2nd display first to generate new id for external display.
  UpdateDisplay("300x200*2");
  UpdateDisplay("300x200*2, 500x400#600x500|500x400%60.0f");

  // Update key as the 2nd display gets new id.
  id2 = display_manager_test.GetSecondaryDisplay().id();
  key = base::NumberToString(id1) + "," + base::NumberToString(id2);
  layout_value = displays.FindDict(key);
  ASSERT_TRUE(layout_value);
  EXPECT_TRUE(display::JsonToDisplayLayout(*layout_value, &stored_layout));
  EXPECT_EQ(display::DisplayPlacement::RIGHT, stored_placement.position);
  EXPECT_EQ(0, stored_placement.offset);
  primary_id_str = layout_value->FindString(kPrimaryIdKey);
  ASSERT_TRUE(primary_id_str);
  EXPECT_EQ(base::NumberToString(id1), *primary_id_str);

  // External display's selected resolution must be updated.
  property = properties.FindDict(base::NumberToString(id2));
  ASSERT_TRUE(property);
  EXPECT_EQ(500, property->FindInt("width"));
  EXPECT_EQ(400, property->FindInt("height"));
}

TEST_F(DisplayPrefsTest, PreventStore) {
  LoggedInAsUser();
  UpdateDisplay("400x300#500x400|400x300|300x200");
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  // Set display's resolution in single display. It creates the notification and
  // display preferences should not stored meanwhile.
  Shell* shell = Shell::Get();

  display::ManagedDisplayMode old_mode(gfx::Size(400, 300), 60.0f, false,
                                       false);
  display::ManagedDisplayMode new_mode(gfx::Size(500, 400), 60.0f, false, true);
  EXPECT_TRUE(shell->resolution_notification_controller()
                  ->PrepareNotificationAndSetDisplayMode(
                      id, old_mode, new_mode,
                      crosapi::mojom::DisplayConfigSource::kUser,
                      base::OnceClosure()));
  UpdateDisplay("500x400#500x400|400x300|300x200");

  const base::Value::Dict& properties =
      local_state()->GetDict(prefs::kDisplayProperties);
  const base::Value::Dict* property =
      properties.FindDict(base::NumberToString(id));
  EXPECT_TRUE(property);
  EXPECT_FALSE(property->FindInt("width"));
  EXPECT_FALSE(property->FindInt("height"));

  // Revert the change.
  shell->resolution_notification_controller()->RevertResolutionChange(false);
  base::RunLoop().RunUntilIdle();

  // The specified resolution will be stored by SetDisplayMode.
  Shell::Get()->display_manager()->SetDisplayMode(
      id, display::ManagedDisplayMode(gfx::Size(300, 200), 60.0f, false, true));
  UpdateDisplay("300x200#500x400|400x300|300x200");

  property = properties.FindDict(base::NumberToString(id));
  ASSERT_TRUE(property);
  EXPECT_EQ(300, property->FindInt("width"));
  EXPECT_EQ(200, property->FindInt("height"));
}

TEST_F(DisplayPrefsTest, StoreForSwappedDisplay) {
  UpdateDisplay("200x100,300x200");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  int64_t id2 = display_manager_test.GetSecondaryDisplay().id();

  LoggedInAsUser();

  SwapPrimaryDisplay();
  ASSERT_EQ(id1, display_manager_test.GetSecondaryDisplay().id());

  std::string key = base::NumberToString(id1) + "," + base::NumberToString(id2);
  const base::Value::Dict& displays =
      local_state()->GetDict(prefs::kSecondaryDisplays);
  // Initial saved value is swapped.
  {
    const base::Value::Dict* new_value = displays.FindDict(key);
    EXPECT_TRUE(new_value);
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
    const base::Value::Dict* new_value = displays.FindDict(key);
    ASSERT_TRUE(new_value);
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
    const base::Value::Dict* new_value = displays.FindDict(key);
    ASSERT_TRUE(new_value);
    display::DisplayLayout stored_layout;
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
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  UpdateDisplay("300x200*2,300x200");

  LoggedInAsGuest();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(), id1);
  int64_t id2 = display::test::DisplayManagerTestApi(display_manager())
                    .GetSecondaryDisplay()
                    .id();
  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 10));
  const float scale = 1.25f;
  display_manager()->UpdateZoomFactor(id1, 1.f / scale);
  window_tree_host_manager->SetPrimaryDisplayId(id2);
  int64_t new_primary = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  window_tree_host_manager->SetOverscanInsets(
      new_primary, gfx::Insets::TLBR(10, 11, 12, 13));
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
  EXPECT_EQ(gfx::Size(178, 276), primary_display.bounds().size());
  EXPECT_EQ(display::Display::ROTATE_90, primary_display.rotation());

  const display::ManagedDisplayInfo& info1 =
      display_manager()->GetDisplayInfo(id1);
  EXPECT_FLOAT_EQ(1.f / scale, info1.zoom_factor());

  const display::ManagedDisplayInfo& info_primary =
      display_manager()->GetDisplayInfo(new_primary);
  EXPECT_EQ(display::Display::ROTATE_90, info_primary.GetActiveRotation());
  EXPECT_EQ(1.0f, info_primary.zoom_factor());
}

// Test case which accepts the boolean value of the
// AllowMGSToStoreDisplayProperties policy. When set to True, this policy allows
// managed guest session to store display configuration permanently in the local
// state. When set to False or unset, the display configuration is not stored in
// the local state.
class DisplayPrefsPublicAccountTest : public DisplayPrefsTestGuest,
                                      public testing::WithParamInterface<bool> {
 public:
  bool IsMGSAllowedToStoreDisplayProperties() const { return GetParam(); }

  void SetUp() override {
    DisplayPrefsTestGuest::SetUp();

    UpdateDisplay("300x200*2,300x200");
    local_state()->SetBoolean(prefs::kAllowMGSToStoreDisplayProperties,
                              IsMGSAllowedToStoreDisplayProperties());
  }
};

TEST_P(DisplayPrefsPublicAccountTest, StoreDisplayPrefsForPublicAccount) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();
  LoggedInAsPublicAccount();

  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(), id1);
  int64_t id2 = display::test::DisplayManagerTestApi(display_manager())
                    .GetSecondaryDisplay()
                    .id();
  display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::TOP, 10));
  const float scale = 1.25f;
  display_manager()->UpdateZoomFactor(id1, 1.f / scale);
  window_tree_host_manager->SetPrimaryDisplayId(id2);
  const int64_t new_primary =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  window_tree_host_manager->SetOverscanInsets(
      new_primary, gfx::Insets::TLBR(10, 11, 12, 13));
  display_manager()->SetDisplayRotation(new_primary,
                                        display::Display::ROTATE_90,
                                        display::Display::RotationSource::USER);

  // Preferences should only be stored if the AllowMGSToStoreDisplayProperties
  // policy was set to true.
  EXPECT_EQ(IsMGSAllowedToStoreDisplayProperties(),
            local_state()
                ->FindPreference(prefs::kSecondaryDisplays)
                ->HasUserSetting());
  EXPECT_EQ(IsMGSAllowedToStoreDisplayProperties(),
            local_state()
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
  EXPECT_EQ(gfx::Size(178, 276), primary_display.bounds().size());
  EXPECT_EQ(display::Display::ROTATE_90, primary_display.rotation());

  const display::ManagedDisplayInfo& info1 =
      display_manager()->GetDisplayInfo(id1);
  EXPECT_FLOAT_EQ(1.f / scale, info1.zoom_factor());

  const display::ManagedDisplayInfo& info_primary =
      display_manager()->GetDisplayInfo(new_primary);
  EXPECT_EQ(display::Display::ROTATE_90, info_primary.GetActiveRotation());
  EXPECT_EQ(1.0f, info_primary.zoom_factor());
}

INSTANTIATE_TEST_SUITE_P(All, DisplayPrefsPublicAccountTest, testing::Bool());

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
  EXPECT_EQ("internal_off_external_on",
            local_state()->GetString(prefs::kDisplayPowerState));
  display_configurator()->reset_requested_power_state_for_test();
  LoadDisplayPreferences();
  EXPECT_EQ(chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON,
            display_configurator()->GetRequestedPowerState());
}

TEST_F(DisplayPrefsTest, DontSaveAndRestoreAllOff) {
  display_prefs()->StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_INTERNAL_OFF_EXTERNAL_ON);
  EXPECT_EQ("internal_off_external_on",
            local_state()->GetString(prefs::kDisplayPowerState));

  // Don't save ALL_OFF.
  display_prefs()->StoreDisplayPowerStateForTest(
      chromeos::DISPLAY_POWER_ALL_OFF);
  EXPECT_EQ("internal_off_external_on",
            local_state()->GetString(prefs::kDisplayPowerState));

  // Don't restore ALL_OFF.
  local_state()->SetString(prefs::kDisplayPowerState, "all_off");
  display_configurator()->reset_requested_power_state_for_test();
  LoadDisplayPreferences();
  EXPECT_EQ(std::nullopt,
            display_configurator()->GetRequestedPowerStateForTest());
}

// Tests that display configuration changes caused by TabletModeController
// are not saved.
TEST_F(DisplayPrefsTest, DontSaveTabletModeControllerRotations) {
  Shell* shell = Shell::Get();
  display::SetInternalDisplayIds(
      {display::Screen::GetScreen()->GetPrimaryDisplay().id()});
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
  AccelerometerUpdate update;
  update.Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, 0.0f, 0.0f,
             -base::kMeanGravityFloat);
  update.Set(ACCELEROMETER_SOURCE_SCREEN, 0.0f, base::kMeanGravityFloat, 0.0f);
  TabletModeController* controller = Shell::Get()->tablet_mode_controller();
  controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Trigger 90 degree rotation
  update.Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, base::kMeanGravityFloat,
             0.0f, 0.0f);
  update.Set(ACCELEROMETER_SOURCE_SCREEN, base::kMeanGravityFloat, 0.0f, 0.0f);
  controller->OnAccelerometerUpdated(update);
  shell->screen_orientation_controller()->OnAccelerometerUpdated(update);
  EXPECT_EQ(display::Display::ROTATE_90, GetCurrentInternalDisplayRotation());

  const base::Value::Dict* properties =
      &local_state()->GetDict(prefs::kDisplayProperties);
  const base::Value::Dict* property = properties->FindDict(
      base::NumberToString(display::Display::InternalDisplayId()));
  EXPECT_TRUE(property);
  EXPECT_EQ(display::Display::ROTATE_0, property->FindInt("rotation"));

  // Trigger a save, the acceleration rotation should not be saved as the user
  // rotation.
  display_prefs()->MaybeStoreDisplayPrefs();
  properties = &local_state()->GetDict(prefs::kDisplayProperties);
  property = properties->FindDict(
      base::NumberToString(display::Display::InternalDisplayId()));
  EXPECT_TRUE(property);
  EXPECT_EQ(display::Display::ROTATE_0, property->FindInt("rotation"));
}

// Tests that the rotation state is saved without a user being logged in.
TEST_F(DisplayPrefsTest, StoreRotationStateNoLogin) {
  display::SetInternalDisplayIds(
      {display::Screen::GetScreen()->GetPrimaryDisplay().id()});
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  bool current_rotation_lock = IsRotationLocked();
  display_prefs()->StoreDisplayRotationPrefsForTest(GetRotation(),
                                                    current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::Value::Dict& properties =
      local_state()->GetDict(prefs::kDisplayRotationLock);
  std::optional<bool> rotation_lock = properties.FindBool("lock");
  ASSERT_TRUE(rotation_lock.has_value());
  EXPECT_EQ(current_rotation_lock, rotation_lock.value());

  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_EQ(current_rotation, properties.FindInt("orientation"));
}

// Tests that the rotation state is saved when a guest is logged in.
TEST_F(DisplayPrefsTest, StoreRotationStateGuest) {
  display::SetInternalDisplayIds(
      {display::Screen::GetScreen()->GetPrimaryDisplay().id()});
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  display_prefs()->StoreDisplayRotationPrefsForTest(GetRotation(),
                                                    current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::Value::Dict& properties =
      local_state()->GetDict(prefs::kDisplayRotationLock);
  std::optional<bool> rotation_lock = properties.FindBool("lock");
  ASSERT_TRUE(rotation_lock.has_value());
  EXPECT_EQ(current_rotation_lock, rotation_lock.value());

  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_EQ(current_rotation, properties.FindInt("orientation"));
}

// Tests that the rotation state is saved when a normal user is logged in.
TEST_F(DisplayPrefsTest, StoreRotationStateNormalUser) {
  display::SetInternalDisplayIds(
      {display::Screen::GetScreen()->GetPrimaryDisplay().id()});
  EXPECT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));
  LoggedInAsGuest();

  bool current_rotation_lock = IsRotationLocked();
  display_prefs()->StoreDisplayRotationPrefsForTest(GetRotation(),
                                                    current_rotation_lock);
  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::Value::Dict& properties =
      local_state()->GetDict(prefs::kDisplayRotationLock);
  std::optional<bool> rotation_lock = properties.FindBool("lock");
  ASSERT_TRUE(rotation_lock.has_value());
  EXPECT_EQ(current_rotation_lock, rotation_lock.value());

  display::Display::Rotation current_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_EQ(current_rotation, properties.FindInt("orientation"));
}

// Tests that rotation state is loaded without a user being logged in, and that
// entering tablet mode applies the state.
TEST_F(DisplayPrefsTest, LoadRotationNoLogin) {
  display::SetInternalDisplayIds(
      {display::Screen::GetScreen()->GetPrimaryDisplay().id()});
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
  AccelerometerUpdate update;
  update.Set(ACCELEROMETER_SOURCE_ATTACHED_KEYBOARD, 0.0f, 0.0f,
             -base::kMeanGravityFloat);
  update.Set(ACCELEROMETER_SOURCE_SCREEN, 0.0f, base::kMeanGravityFloat, 0.0f);
  TabletModeController* tablet_mode_controller =
      Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->OnAccelerometerUpdated(update);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  bool screen_orientation_rotation_lock = IsRotationLocked();
  display::Display::Rotation tablet_mode_rotation =
      GetCurrentInternalDisplayRotation();
  EXPECT_TRUE(screen_orientation_rotation_lock);
  EXPECT_EQ(display::Display::ROTATE_90, tablet_mode_rotation);
}

// Tests that rotation lock being set causes the rotation state to be saved.
TEST_F(DisplayPrefsTest, RotationLockTriggersStore) {
  display::SetInternalDisplayIds(
      {display::Screen::GetScreen()->GetPrimaryDisplay().id()});
  ASSERT_FALSE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  Shell::Get()->screen_orientation_controller()->ToggleUserRotationLock();

  EXPECT_TRUE(local_state()->HasPrefPath(prefs::kDisplayRotationLock));

  const base::Value::Dict& properties =
      local_state()->GetDict(prefs::kDisplayRotationLock);
  std::optional<bool> rotation_lock = properties.FindBool("lock");
  EXPECT_TRUE(rotation_lock.has_value());
}

TEST_F(DisplayPrefsTest, SaveUnifiedMode) {
  LoggedInAsUser();
  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("300x200,200x100");
  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  EXPECT_EQ(gfx::Size(700, 200),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  const base::Value::Dict& secondary_displays =
      local_state()->GetDict(prefs::kSecondaryDisplays);
  const base::Value::Dict* new_value =
      secondary_displays.FindDict(display::DisplayIdListToString(list));
  EXPECT_TRUE(new_value);

  display::DisplayLayout stored_layout;
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);

  const base::Value::Dict& displays =
      local_state()->GetDict(prefs::kDisplayProperties);
  int64_t unified_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  new_value = displays.FindDict(base::NumberToString(unified_id));
  EXPECT_FALSE(new_value);

  display::test::SetDisplayResolution(display_manager(), unified_id,
                                      gfx::Size(350, 100));
  EXPECT_EQ(gfx::Size(350, 100),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());
  EXPECT_FALSE(displays.FindDict(base::NumberToString(unified_id)));

  // Mirror mode should remember if the default mode was unified.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  new_value = secondary_displays.FindDict(display::DisplayIdListToString(list));
  ASSERT_TRUE(new_value);
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);

  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  new_value = secondary_displays.FindDict(display::DisplayIdListToString(list));
  ASSERT_TRUE(new_value);
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_TRUE(stored_layout.default_unified);

  // Exit unified mode.
  display_manager()->SetDefaultMultiDisplayModeForCurrentDisplays(
      display::DisplayManager::EXTENDED);
  new_value = secondary_displays.FindDict(display::DisplayIdListToString(list));
  ASSERT_TRUE(new_value);
  EXPECT_TRUE(display::JsonToDisplayLayout(*new_value, &stored_layout));
  EXPECT_FALSE(stored_layout.default_unified);
}

TEST_F(DisplayPrefsTest, RestoreUnifiedMode) {
  const int64_t first_display_id = 210000001;
  const int64_t second_display_id = 220000002;
  display::ManagedDisplayInfo first_display_info =
      CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 600, 500));
  display::ManagedDisplayInfo second_display_info =
      CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 600, 500));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.emplace_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      first_display_id);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  display::DisplayIdList list =
      display::test::CreateDisplayIdList2(first_display_id, second_display_id);
  StoreDisplayBoolPropertyForList(list, "default_unified", true);
  StoreDisplayPropertyForList(
      list, "primary-id", base::Value(base::NumberToString(first_display_id)));
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

  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
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
  UpdateDisplay("300x200,300x200,400x300");

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());

  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0],
                              display::DisplayPlacement::RIGHT, 0);
  builder.AddDisplayPlacement(list[2], list[0],
                              display::DisplayPlacement::BOTTOM, 100);
  display_manager()->SetLayoutForCurrentDisplays(builder.Build());

  const base::Value::Dict& secondary_displays =
      local_state()->GetDict(prefs::kSecondaryDisplays);
  const base::Value::Dict* new_value =
      secondary_displays.FindDict(display::DisplayIdListToString(list));
  EXPECT_TRUE(new_value);
}

TEST_F(DisplayPrefsTest, RestoreThreeDisplays) {
  LoggedInAsUser();
  int64_t id1 = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list = display::test::CreateDisplayIdListN(id1, 3);

  display::DisplayLayoutBuilder builder(list[0]);
  builder.AddDisplayPlacement(list[1], list[0], display::DisplayPlacement::LEFT,
                              0);
  builder.AddDisplayPlacement(list[2], list[1],
                              display::DisplayPlacement::BOTTOM, 100);
  display_prefs()->StoreDisplayLayoutPrefForTest(list, *builder.Build());
  LoadDisplayPreferences();

  UpdateDisplay("300x200,300x200,400x300");
  display::DisplayIdList new_list =
      display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());
  ASSERT_EQ(list[0], new_list[0]);
  ASSERT_EQ(list[1], new_list[1]);
  ASSERT_EQ(list[2], new_list[2]);

  EXPECT_EQ(gfx::Rect(0, 0, 300, 200),
            display_manager()->GetDisplayForId(list[0]).bounds());
  EXPECT_EQ(gfx::Rect(-300, 0, 300, 200),
            display_manager()->GetDisplayForId(list[1]).bounds());
  EXPECT_EQ(gfx::Rect(-200, 200, 400, 300),
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

  int64_t id_2 = display::test::DisplayManagerTestApi(display_manager())
                     .GetSecondaryDisplay()
                     .id();
  gfx::Size touch_size_2(300, 300);
  display::TouchCalibrationData data_2(point_pair_quad, touch_size_2);

  const ui::TouchscreenDevice touchdevice_4(
      19, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test touch device 4"), gfx::Size(231, 416), 1);
  display::TouchDeviceIdentifier identifier =
      display::TouchDeviceIdentifier::FromDevice(touchdevice_4);
  display_manager()->SetTouchCalibrationData(
      id_2, point_pair_quad, touch_size_2, touchdevice_4,
      /*apply_spatial_calibration=*/true);

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
      switches::kFirstExecAfterBoot);

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
      CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 600, 500));
  display::ManagedDisplayInfo second_display_info =
      CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 600, 500));
  std::vector<display::ManagedDisplayInfo> display_info_list;

  // There's no external display now.
  display_info_list.push_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 200, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Add first display id to the external display mirror info.
  std::set<int64_t> external_display_mirror_info;
  external_display_mirror_info.emplace(first_display_masked_id);
  StoreExternalDisplayMirrorInfo(external_display_mirror_info);
  LoadDisplayPreferences();
  const base::Value::List* pref_external_display_mirror_info =
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->size());
  EXPECT_EQ(base::NumberToString(first_display_masked_id),
            (*pref_external_display_mirror_info)[0].GetString());

  // Add first display, mirror mode restores and the external display mirror
  // info does not change.
  display_info_list.emplace_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->size());
  EXPECT_EQ(base::NumberToString(first_display_masked_id),
            (*pref_external_display_mirror_info)[0].GetString());

  // Add second display, mirror mode persists and the second display id is added
  // to the external display mirror info.
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(2U, pref_external_display_mirror_info->size());
  EXPECT_EQ(base::NumberToString(first_display_masked_id),
            (*pref_external_display_mirror_info)[0].GetString());
  EXPECT_EQ(base::NumberToString(second_display_masked_id),
            (*pref_external_display_mirror_info)[1].GetString());

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
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->size());
  EXPECT_EQ(base::NumberToString(second_display_masked_id),
            (*pref_external_display_mirror_info)[0].GetString());

  // Add first display, mirror mode is off and the external display mirror info
  // does not change.
  display_info_list.emplace_back(first_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(1U, pref_external_display_mirror_info->size());
  EXPECT_EQ(base::NumberToString(second_display_masked_id),
            (*pref_external_display_mirror_info)[0].GetString());

  // Add second display, mirror mode remains off and the second display id is
  // removed from the external display mirror info.
  display_info_list.emplace_back(second_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  pref_external_display_mirror_info =
      &local_state()->GetList(prefs::kExternalDisplayMirrorInfo);
  EXPECT_EQ(0U, pref_external_display_mirror_info->size());
}

TEST_F(DisplayPrefsTest, ExternalDisplayConnectedBeforeLoadingPrefs) {
  LoggedInAsUser();

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kFirstExecAfterBoot);

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t external_display_id = 210000001;
  display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(external_display_id, gfx::Rect(1, 1, 600, 500));

  // Both internal and external displays connect before the prefs are loaded.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 200, 100)));
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
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);

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
  display_info_list.push_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 200, 100)));
  display_info_list.push_back(
      CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 600, 500)));
  display_info_list.push_back(
      CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 600, 500)));

  // Store mixed mirror mode parameters which specify mirroring from the
  // internal display to the first external display.
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(first_display_id);
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, internal_display_id, dst_ids);
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
  const base::Value::Dict* pref_data =
      &local_state()->GetDict(prefs::kDisplayMixedMirrorModeParams);
  EXPECT_EQ(base::NumberToString(internal_display_id),
            *pref_data->FindString("mirroring_source_id"));
  const base::Value::List* destination_ids_list =
      pref_data->FindList("mirroring_destination_ids");
  EXPECT_EQ(1U, destination_ids_list->size());
  EXPECT_EQ(base::NumberToString(first_display_id),
            (*destination_ids_list)[0].GetString());

  // Overwrite current mixed mirror mode with a new configuration. (Mirror from
  // the first external display to the second external display)
  dst_ids.clear();
  dst_ids.emplace_back(second_display_id);
  std::optional<display::MixedMirrorModeParams> new_mixed_params(
      std::in_place, first_display_id, dst_ids);
  display_manager()->SetMirrorMode(display::MirrorMode::kMixed,
                                   new_mixed_params);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(first_display_id, display_manager()->mirroring_source_id());
  destination_ids = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(second_display_id, destination_ids[0]);

  // Check the preferences.
  pref_data = &local_state()->GetDict(prefs::kDisplayMixedMirrorModeParams);
  EXPECT_EQ(base::NumberToString(first_display_id),
            *pref_data->FindString("mirroring_source_id"));
  destination_ids_list = pref_data->FindList("mirroring_destination_ids");
  EXPECT_EQ(1U, destination_ids_list->size());
  EXPECT_EQ(base::NumberToString(second_display_id),
            (*destination_ids_list)[0].GetString());

  // Turn off mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Check the preferences.
  pref_data = &local_state()->GetDict(prefs::kDisplayMixedMirrorModeParams);
  EXPECT_TRUE(pref_data->empty());
}

TEST_F(DisplayPrefsTest, SaveTabletModeWithSingleDisplay) {
  UpdateDisplay("480x320/r@1.25");

  const int64_t id0 = display::test::DisplayManagerTestApi(display_manager())
                          .SetFirstDisplayAsInternalDisplay();

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  const base::Value::Dict* properties = ReadPropertiesForDisplay(id0);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.25f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_90)));

  LoggedInAsUser();

  // Turn on tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Change display settings.
  display_manager()->UpdateZoomFactor(id0, 1.5);
  display_manager()->SetDisplayRotation(id0, display::Display::ROTATE_270,
                                        display::Display::RotationSource::USER);
  // Verify the settings have been changed.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(id0).zoom_factor(), 1.5);
  EXPECT_EQ(display_manager()->GetDisplayInfo(id0).GetActiveRotation(),
            display::Display::ROTATE_270);

  properties = ReadPropertiesForDisplay(id0);
  // Zoom pref should store the new value.
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.5f)));
  // Rotation pref should remain at the original value.
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_90)));

  // Turn off tablet mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  // Zoom should stay at the new value.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(id0).zoom_factor(), 1.5);
  // Rotation should restore to the original value.
  EXPECT_EQ(display_manager()->GetDisplayInfo(id0).GetActiveRotation(),
            display::Display::ROTATE_90);

  properties = ReadPropertiesForDisplay(id0);
  // Zoom pref should keep the new value.
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.5f)));
  // Rotation pref should remain at the original value.
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_90)));
}

TEST_F(DisplayPrefsTest, SaveTabletModeWithMixedExternalDisplays) {
  UpdateDisplay("480x320/r@1.25,640x480/l@1.3,320x240@1.2");

  display::DisplayIdList ids = display_manager()->GetConnectedDisplayIdList();
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Set up mixed mirror mode. (Mirror from the first display to the second
  // display, and extend to third display)
  const display::MixedMirrorModeParams mixed_params(ids[0], {ids[1]});
  display_manager()->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(display_manager()->mirroring_source_id(), ids[0]);
  EXPECT_THAT(display_manager()->GetMirroringDestinationDisplayIdList(),
              ElementsAre(ids[1]));
  // Mixed mirror mode params are properly set.
  const auto& initial_mixed_params =
      display_manager()->mixed_mirror_mode_params();
  ASSERT_TRUE(initial_mixed_params);
  EXPECT_EQ(initial_mixed_params->source_id, mixed_params.source_id);
  EXPECT_EQ(initial_mixed_params->destination_ids,
            mixed_params.destination_ids);
  const std::set<int64_t> old_ext_mirror_info =
      display_manager()->external_display_mirror_info();
  EXPECT_FALSE(old_ext_mirror_info.empty());
  // Mixed mirror mode params prefs are properly saved.
  ExpectMixedMirrorModeParamsPrefs(ids[0], {ids[1]});
  ExpectExternalDisplayMirrorPrefs(old_ext_mirror_info);

  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Verify initial stored display prefs.
  const base::Value::Dict* properties = ReadPropertiesForDisplay(ids[0]);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.25f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_90)));

  properties = ReadPropertiesForDisplay(ids[1]);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.3f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_270)));

  properties = ReadPropertiesForDisplay(ids[2]);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.2f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_0)));

  LoggedInAsUser();

  // Turn on tablet mode and make display changes.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(display_manager()->mirroring_source_id(), ids[0]);
  EXPECT_THAT(display_manager()->GetMirroringDestinationDisplayIdList(),
              ElementsAre(ids[1], ids[2]));
  // Tablet mode forces normal mirror mode, so mixed params is empty.
  EXPECT_EQ(display_manager()->mixed_mirror_mode_params(), std::nullopt);
  EXPECT_EQ(display_manager()->external_display_mirror_info(),
            old_ext_mirror_info);
  // Mixed mirror mode params pref should remain at the original value.
  ExpectMixedMirrorModeParamsPrefs(ids[0], {ids[1]});
  ExpectExternalDisplayMirrorPrefs(old_ext_mirror_info);

  // Make changes to the display and verify.
  display_manager()->UpdateZoomFactor(ids[0], 1.5);
  display_manager()->SetDisplayRotation(ids[0], display::Display::ROTATE_180,
                                        display::Display::RotationSource::USER);
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(ids[0]).zoom_factor(), 1.5);
  EXPECT_EQ(display_manager()->GetDisplayInfo(ids[0]).GetActiveRotation(),
            display::Display::ROTATE_180);

  // Simulate a reboot.
  LoadDisplayPreferences();
  display_manager()->UpdateDisplays();

  // Things should stay in tablet mode.
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(display_manager()->mirroring_source_id(), ids[0]);
  // Currently, restarting in tablet mode reverts back to the original mixed
  // mirror mode, which may be surprising to users. This should be revisited by
  // crbug.com/733092.
  EXPECT_THAT(display_manager()->GetMirroringDestinationDisplayIdList(),
              ElementsAre(ids[1]));
  const auto& loaded_mixed_params =
      display_manager()->mixed_mirror_mode_params();
  ASSERT_TRUE(loaded_mixed_params);
  EXPECT_EQ(loaded_mixed_params->source_id, mixed_params.source_id);
  EXPECT_EQ(loaded_mixed_params->destination_ids, mixed_params.destination_ids);
  EXPECT_EQ(display_manager()->external_display_mirror_info(),
            old_ext_mirror_info);
  // Mixed mirror mode params pref should remain at the original value.
  ExpectMixedMirrorModeParamsPrefs(ids[0], {ids[1]});
  ExpectExternalDisplayMirrorPrefs(old_ext_mirror_info);

  // Check stored and loaded display prefs.
  properties = ReadPropertiesForDisplay(ids[0]);
  // Zoom factor should persist the new value.
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.5f)));
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(ids[0]).zoom_factor(), 1.5);
  // Rotation should remain at the original value.
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_90)));
  EXPECT_EQ(display_manager()->GetDisplayInfo(ids[0]).GetActiveRotation(),
            display::Display::ROTATE_90);

  // Properties for the second display shouldn't change.
  properties = ReadPropertiesForDisplay(ids[1]);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.3f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_270)));

  // Properties for the third display shouldn't change.
  properties = ReadPropertiesForDisplay(ids[2]);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.2f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_0)));

  // Turn off tablet mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(display_manager()->mirroring_source_id(), ids[0]);
  EXPECT_THAT(display_manager()->GetMirroringDestinationDisplayIdList(),
              ElementsAre(ids[1]));
  // Original mixed mirror mode params are preserved.
  const auto& restored_mixed_params =
      display_manager()->mixed_mirror_mode_params();
  ASSERT_TRUE(restored_mixed_params);
  EXPECT_EQ(restored_mixed_params->source_id, mixed_params.source_id);
  EXPECT_EQ(restored_mixed_params->destination_ids,
            mixed_params.destination_ids);
  EXPECT_EQ(display_manager()->external_display_mirror_info(),
            old_ext_mirror_info);
  // Mixed mirror mode params pref should remain at the original value.
  ExpectMixedMirrorModeParamsPrefs(ids[0], {ids[1]});
  ExpectExternalDisplayMirrorPrefs(old_ext_mirror_info);

  // Check restored display prefs.
  properties = ReadPropertiesForDisplay(ids[0]);
  // Zoom factor should remain at the new value.
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.5f)));
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(ids[0]).zoom_factor(), 1.5);
  // Rotation should be restored to the original value.
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_90)));
  EXPECT_EQ(display_manager()->GetDisplayInfo(ids[0]).GetActiveRotation(),
            display::Display::ROTATE_90);

  // Properties for the second display shouldn't change.
  properties = ReadPropertiesForDisplay(ids[1]);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.3f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_270)));

  // Properties for the third display shouldn't change.
  properties = ReadPropertiesForDisplay(ids[2]);
  EXPECT_THAT(properties->FindDouble("display_zoom_factor"),
              Optional(DoubleEq(1.2f)));
  EXPECT_THAT(properties->FindInt("rotation"),
              Optional(static_cast<int>(display::Display::ROTATE_0)));
}

TEST_F(DisplayPrefsTest, IsDisplayAvailableInPref) {
  int64_t id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  // Display is not available in prefs before adding the display.
  EXPECT_FALSE(display_prefs()->IsDisplayAvailableInPref(id));

  // Display is available in prefs after adding the display.
  UpdateDisplay("300x200");
  EXPECT_TRUE(display_prefs()->IsDisplayAvailableInPref(id));
}

}  // namespace ash
