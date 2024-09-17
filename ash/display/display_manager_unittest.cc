// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/display_manager.h"

#include "ash/accelerators/accelerator_commands.h"
#include "ash/accelerometer/accelerometer_reader.h"
#include "ash/accelerometer/accelerometer_types.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/display_configuration_controller.h"
#include "ash/display/display_util.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/mirror_window_test_api.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/root_window_controller.h"
#include "ash/rounded_display/rounded_display_provider.h"
#include "ash/rounded_display/rounded_display_provider_test_api.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/format_macros.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/math_constants.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/display_features.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/display_observer.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_change_observer.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager_observer.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/manager/test/fake_display_snapshot.h"
#include "ui/display/manager/test/touch_device_manager_test_api.h"
#include "ui/display/manager/util/display_manager_test_util.h"
#include "ui/display/manager/util/display_manager_util.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/test/virtual_display_util.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/util/display_util.h"
#include "ui/events/devices/touchscreen_device.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/font_render_params.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/wm/core/compound_event_filter.h"

namespace ash {

using std::string;
using std::vector;

using base::StringPrintf;

namespace {

std::string ToDisplayName(int64_t id) {
  return base::StringPrintf("Display-%d", static_cast<int>(id));
}

// Asserts that metrics propagated by DisplayManager and DisplayManagerObserver
// are consistent.
class DisplayManagerObserverValidator : public display::DisplayObserver,
                                        public display::DisplayManagerObserver {
 public:
  DisplayManagerObserverValidator() {
    display_observer_.emplace(this);
    display_manager_observation_.Observe(Shell::Get()->display_manager());
  }

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    if (!base::Contains(added_displays_, new_display)) {
      EXPECT_TRUE(base::Contains(active_display_list(), new_display));
      added_displays_.push_back(new_display);
    }
  }
  void OnWillRemoveDisplays(const Displays& removed_displays) override {
    for (const auto& display : removed_displays) {
      EXPECT_TRUE(base::Contains(active_display_list(), display));
    }
  }
  void OnDisplaysRemoved(const display::Displays& removed_displays) override {
    for (const auto& display : removed_displays) {
      EXPECT_FALSE(base::Contains(active_display_list(), display));
      if (!base::Contains(added_displays_, display)) {
        removed_displays_.push_back(display);
      }
    }
  }
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    EXPECT_TRUE(base::Contains(active_display_list(), display));
    if (!base::Contains(changed_displays_, display)) {
      changed_displays_.push_back(display);
    }
    if (!changed_metrics_.try_emplace(display.id(), changed_metrics).second) {
      changed_metrics_[display.id()] |= changed_metrics;
    }
  }

  // display::DisplayManager::Observer:
  void OnWillProcessDisplayChanges() override {
    // There should not be multiple OnWillProcessDisplayChanges() calls before
    // the subsequent call to OnDidProcessDisplayChanges().
    EXPECT_FALSE(processing_display_changes_);
    processing_display_changes_ = true;
  }
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override {
    EXPECT_TRUE(processing_display_changes_);

    EXPECT_TRUE(base::ranges::is_permutation(
        added_displays_, configuration_change.added_displays));
    EXPECT_TRUE(base::ranges::is_permutation(
        removed_displays_, configuration_change.removed_displays));

    EXPECT_EQ(changed_metrics_.size(),
              configuration_change.display_metrics_changes.size());
    for (const auto& change : configuration_change.display_metrics_changes) {
      EXPECT_TRUE(base::Contains(changed_metrics_, change.display->id()));
      EXPECT_EQ(changed_metrics_[change.display->id()], change.changed_metrics);
    }

    processing_display_changes_ = false;
    added_displays_.clear();
    removed_displays_.clear();
    changed_displays_.clear();
    changed_metrics_.clear();
  }

  const display::Displays& active_display_list() {
    return Shell::Get()->display_manager()->active_display_list();
  }

 private:
  bool processing_display_changes_ = false;
  vector<display::Display> added_displays_;
  vector<display::Display> removed_displays_;
  vector<display::Display> changed_displays_;
  base::flat_map<int64_t, uint32_t> changed_metrics_;

  std::optional<display::ScopedDisplayObserver> display_observer_;
  base::ScopedObservation<display::DisplayManager,
                          display::DisplayManagerObserver>
      display_manager_observation_{this};
};

}  // namespace

class DisplayManagerTest : public AshTestBase,
                           public display::DisplayObserver,
                           public aura::WindowObserver,
                           public display::DisplayManagerObserver {
 public:
  DisplayManagerTest() = default;

  DisplayManagerTest(const DisplayManagerTest&) = delete;
  DisplayManagerTest& operator=(const DisplayManagerTest&) = delete;

  ~DisplayManagerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    display_observer_.emplace(this);
    display_manager_observation_.Observe(Shell::Get()->display_manager());
    Shell::GetPrimaryRootWindow()->AddObserver(this);
    display_manager_observer_validator_.emplace();
  }
  void TearDown() override {
    display_manager_observer_validator_.reset();
    Shell::GetPrimaryRootWindow()->RemoveObserver(this);
    display_manager_observation_.Reset();
    display_observer_.reset();
    AshTestBase::TearDown();
  }

  const vector<display::Display>& changed() const { return changed_; }
  const vector<display::Display>& added() const { return added_; }
  int32_t changed_metrics() const {
    int32_t changed_metrics = 0;
    for (const auto& display_metrics : changed_metrics_) {
      changed_metrics |= display_metrics.second;
    }
    return changed_metrics;
  }
  int32_t changed_metrics(int64_t display_id) const {
    return changed_metrics_.at(display_id);
  }

  string GetCountSummary() const {
    return StringPrintf("c%" PRIuS " a%" PRIuS " r%" PRIuS " w%" PRIuS
                        " d%" PRIuS,
                        changed_.size(), added_.size(), removed_count_,
                        will_process_count_, did_process_count_);
  }

  void reset() {
    changed_.clear();
    added_.clear();
    removed_count_ = will_process_count_ = did_process_count_ = 0U;
    changed_metrics_.clear();
    root_window_destroyed_ = false;
  }

  bool root_window_destroyed() const { return root_window_destroyed_; }

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

  const display::ManagedDisplayInfo& GetDisplayInfo(
      const display::Display& display) {
    return display_manager()->GetDisplayInfo(display.id());
  }

  const display::ManagedDisplayInfo& GetDisplayInfoAt(int index) {
    return GetDisplayInfo(display_manager()->GetDisplayAt(index));
  }

  const display::Display& GetDisplayForId(int64_t id) {
    return display_manager()->GetDisplayForId(id);
  }

  const display::ManagedDisplayInfo& GetDisplayInfoForId(int64_t id) {
    return GetDisplayInfo(display_manager()->GetDisplayForId(id));
  }

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    changed_.push_back(display);
    if (!changed_metrics_.try_emplace(display.id(), changed_metrics).second)
      changed_metrics_[display.id()] |= changed_metrics;
  }
  void OnDisplayAdded(const display::Display& new_display) override {
    added_.push_back(new_display);
  }
  void OnDisplaysRemoved(const display::Displays& removed_displays) override {
    removed_count_ += removed_displays.size();
  }

  // display::DisplayManager::Observer:
  void OnWillProcessDisplayChanges() override { ++will_process_count_; }
  void OnDidProcessDisplayChanges(
      const DisplayConfigurationChange& configuration_change) override {
    ++did_process_count_;
  }

  // aura::WindowObserver overrides:
  void OnWindowDestroying(aura::Window* window) override {
    if (check_root_window_on_destruction_)
      ASSERT_EQ(Shell::GetPrimaryRootWindow(), window);
    root_window_destroyed_ = true;
  }

  // Returns true if there exists any overlapping mirroring displays.
  bool OverlappingMirroringDisplaysExist() {
    const auto& mirroring_displays =
        display_manager()->software_mirroring_display_list();
    for (size_t i = 0; i < mirroring_displays.size() - 1; ++i) {
      for (size_t j = i + 1; j < mirroring_displays.size(); ++j) {
        const gfx::Rect& bounds_1 = mirroring_displays[i].bounds();
        const gfx::Rect& bounds_2 = mirroring_displays[j].bounds();
        if (bounds_1.Intersects(bounds_2))
          return true;
      }
    }

    return false;
  }

  void SetSoftwareMirrorMode(bool active) {
    display_manager()->SetMirrorMode(
        active ? display::MirrorMode::kNormal : display::MirrorMode::kOff,
        std::nullopt);
    base::RunLoop().RunUntilIdle();
  }

  void disable_check_root_window_on_destruction() {
    check_root_window_on_destruction_ = false;
  }

  base::test::ScopedFeatureList& scoped_feature_list() {
    return scoped_features_;
  }

 private:
  vector<display::Display> changed_;
  vector<display::Display> added_;
  size_t removed_count_ = 0u;
  size_t will_process_count_ = 0u;
  size_t did_process_count_ = 0u;
  bool root_window_destroyed_ = false;
  base::flat_map<int64_t, uint32_t> changed_metrics_;
  bool check_root_window_on_destruction_ = true;

  std::optional<DisplayManagerObserverValidator>
      display_manager_observer_validator_;

  std::optional<display::ScopedDisplayObserver> display_observer_;
  base::ScopedObservation<display::DisplayManager,
                          display::DisplayManagerObserver>
      display_manager_observation_{this};

  // Currently `display::features::kRoundedDisplay` feature is used during the
  // `ash::Shell` shutdown as we call `AshTestBase::TearDown()`, therefore
  // `scoped_features_` needs to outlive the call.
  base::test::ScopedFeatureList scoped_features_;
};

TEST_F(DisplayManagerTest,
       RoundedDisplayProviderIsOnlyCreatedForEachRoundedDisplay) {
  scoped_feature_list().InitAndEnableFeature(
      display::features::kRoundedDisplay);

  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  // Have 4 displays out of which 2 displays have rounded panels. Value after
  // '~' specifies radii of the display's panel.
  UpdateDisplay("500x400,400x300~15,400x300~16,500x400");
  ASSERT_EQ(4U, display_manager()->GetNumDisplays());

  for (auto& display : display_manager()->active_display_list()) {
    const display::ManagedDisplayInfo& display_info =
        display_manager()->GetDisplayInfo(display.id());
    const RoundedDisplayProvider* rounded_display_provider =
        window_tree_host_manager->GetRoundedDisplayProvider(display.id());
    EXPECT_EQ(!!rounded_display_provider,
              !display_info.panel_corners_radii().IsEmpty());
  }
}

TEST_F(DisplayManagerTest, RoundedDisplayProviderIsRemovedForRemovedDisplay) {
  scoped_feature_list().InitAndEnableFeature(
      display::features::kRoundedDisplay);

  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  // Have 4 displays out of which 2 displays have rounded panels. Value after
  // '~' specifies radii of the display's panel.
  UpdateDisplay("500x400,400x300~15,400x300~16,500x400");
  ASSERT_EQ(4U, display_manager()->GetNumDisplays());

  auto to_be_removed_display_id = display_manager()->GetDisplayAt(2).id();

  const RoundedDisplayProvider* rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(
          to_be_removed_display_id);
  EXPECT_TRUE(rounded_display_provider);

  // Remove one display that had rounded corners.
  UpdateDisplay("500x400,400x300~15");

  rounded_display_provider =
      window_tree_host_manager->GetRoundedDisplayProvider(
          to_be_removed_display_id);
  EXPECT_FALSE(rounded_display_provider);
}

TEST_F(DisplayManagerTest, UpdateDisplayTest) {
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Update primary and add secondary.
  UpdateDisplay("100+0-500x400,0+501-400x300");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            display_manager()->GetDisplayAt(0).bounds());

  EXPECT_EQ("c1 a1 r0 w1 d1", GetCountSummary());
  // Metrics change immediately when new displays set shelf work area insets.
  // After that, DisplayManager::OnNativeDisplaysChanged trigger changes of the
  // primary display's metrics. So the observed order of changes is [1, 0].
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), changed()[0].bounds());
  // Secondary display is on right.
  EXPECT_EQ(gfx::Rect(500, 0, 400, 300), added()[0].bounds());
  EXPECT_EQ(gfx::Rect(0, 501, 400, 300),
            GetDisplayInfo(added()[0]).bounds_in_native());

  reset();
  // Delete secondary.
  UpdateDisplay("100+0-500x400");
  EXPECT_EQ("c0 a0 r1 w1 d1", GetCountSummary());
  reset();
  // Change primary.
  UpdateDisplay("1+1-1000x600");
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 600), changed()[0].bounds());
  reset();
  // Add secondary.
  UpdateDisplay("1+1-1000x600,1002+0-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("c0 a1 r0 w1 d1", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  // Secondary display is on right.
  EXPECT_EQ(gfx::Rect(1000, 0, 600, 400), added()[0].bounds());
  EXPECT_EQ(gfx::Rect(1002, 0, 600, 400),
            GetDisplayInfo(added()[0]).bounds_in_native());
  reset();
  // Secondary removed, primary changed.
  UpdateDisplay("1+1-800x300");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("c1 a0 r1 w1 d1", GetCountSummary());
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 300), changed()[0].bounds());
  reset();
  // # of display can go to zero when screen is off.
  const vector<display::ManagedDisplayInfo> empty;
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  // Going to 0 displays doesn't actually change the active display list but the
  // detected bit for the previously connected displays is propagated as false.
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  // Display configuration stays the same
  EXPECT_EQ(gfx::Rect(0, 0, 800, 300),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_FALSE(display_manager()->GetDisplayAt(0).detected());
  EXPECT_EQ(changed_metrics(),
            display::DisplayObserver::DISPLAY_METRIC_DETECTED);
  reset();
  // Connect to display again.
  UpdateDisplay("1+1-800x300");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  EXPECT_EQ(gfx::Rect(800, 300), changed()[0].bounds());
  EXPECT_EQ(gfx::Rect(1, 1, 800, 300),
            GetDisplayInfo(changed()[0]).bounds_in_native());
  EXPECT_TRUE(display_manager()->GetDisplayAt(0).detected());
  EXPECT_EQ(changed_metrics(),
            display::DisplayObserver::DISPLAY_METRIC_DETECTED);

  // Resumed with different resolution.
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  reset();
  UpdateDisplay("100+100-500x400");
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());
  EXPECT_FALSE(root_window_destroyed());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), changed()[0].bounds());
  EXPECT_EQ(gfx::Rect(100, 100, 500, 400),
            GetDisplayInfo(changed()[0]).bounds_in_native());
  EXPECT_TRUE(display_manager()->GetDisplayAt(0).detected());
  EXPECT_EQ(changed_metrics(),
            (display::DisplayObserver::DISPLAY_METRIC_DETECTED |
             display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
             display::DisplayObserver::DISPLAY_METRIC_WORK_AREA));

  reset();
  // Go back to zero and wake up with multiple displays.
  display_manager()->OnNativeDisplaysChanged(empty);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(root_window_destroyed());
  reset();

  // Add secondary.
  UpdateDisplay("0+0-1000x600,1000+1000-600x400");
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 600),
            display_manager()->GetDisplayAt(0).bounds());
  // Secondary display is on right.
  EXPECT_EQ(gfx::Rect(1000, 0, 600, 400),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(1000, 1000, 600, 400),
            GetDisplayInfoAt(1).bounds_in_native());
  reset();

  // Changing primary will update secondary as well.
  UpdateDisplay("0+0-800x600,1000+1000-600x400");
  EXPECT_EQ("c2 a0 r0 w1 d1", GetCountSummary());
  reset();
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(800, 0, 600, 400),
            display_manager()->GetDisplayAt(1).bounds());
}

// Test recommended zoom factor will be applied to external display when
// connected for the first time.
TEST_F(DisplayManagerTest, UpdateDisplayWithUnseenExternalDisplayTest) {
  // Set up internal display and external display.
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const int external_id_1 = 10;
  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 1920, 1200));
  display::ManagedDisplayInfo external_display_info_1 =
      CreateDisplayInfo(external_id_1, gfx::Rect(1, 1, 3840, 2160));
  const float external_display_dpi_1 = 192.f;
  external_display_info_1.set_device_dpi(external_display_dpi_1);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info_1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());

  // The recommended zoom factor should be applied since this external display
  // is connected for the first time.

  // The available zoom factors for 3840X2160 are
  // {1.f, 1.10f, 1.20f, 1.40f, 1.60f, 1.80f, 2.00f, 2.20f, 2.40f}. The expected
  // zoom factor = external_display_dpi_1 /
  // kRecommendedDefaultExternalDisplayDpi = 192 / 96 = 2, which is available.
  const float expect_zoom_factor_1 = 2.f;
  EXPECT_EQ(expect_zoom_factor_1,
            GetDisplayInfoForId(external_id_1).zoom_factor());

  // Update the external display again.
  external_display_info_1.set_device_dpi(300.f);
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info_1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The recommended zoom factor should not be applied since this external
  // display is connected before.
  EXPECT_EQ(1.f, GetDisplayInfoForId(external_id_1).zoom_factor());

  // Test with a new external display with different display dpi.
  const int external_id_2 = 20;
  display::ManagedDisplayInfo external_display_info_2 =
      CreateDisplayInfo(external_id_2, gfx::Rect(1, 1, 3840, 2160));
  const float external_display_dpi_2 = 140.f;
  external_display_info_2.set_device_dpi(external_display_dpi_2);

  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info_2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The available zoom factors for 3840X2160 are
  // {1.f, 1.10f, 1.20f, 1.40f, 1.60f, 1.80f, 2.00f, 2.20f, 2.40f}. The expected
  // zoom factor = external_display_dpi_2 /
  // kRecommendedDefaultExternalDisplayDpi = 140 / 96 = 1.46, the closest
  // available zoom factor is 1.4.
  const float expect_zoom_factor_2 = 1.4f;
  EXPECT_EQ(expect_zoom_factor_2,
            GetDisplayInfoForId(external_id_2).zoom_factor());

  // Test with a new external display with a large display dpi.
  const int external_id_3 = 30;
  display::ManagedDisplayInfo external_display_info_3 =
      CreateDisplayInfo(external_id_3, gfx::Rect(1, 1, 3840, 2160));
  const float external_display_dpi_3 = 300.f;
  external_display_info_3.set_device_dpi(external_display_dpi_3);

  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info_3);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The available zoom factors for 3840X2160 are
  // {1.f, 1.10f, 1.20f, 1.40f, 1.60f, 1.80f, 2.00f, 2.20f, 2.40f}. The expected
  // zoom factor = external_display_dpi_3 /
  // kRecommendedDefaultExternalDisplayDpi = 300 / 96 = 3.125, the closest
  // available zoom factor is 2.4.
  const float expect_zoom_factor_3 = 2.4f;
  EXPECT_EQ(expect_zoom_factor_3,
            GetDisplayInfoForId(external_id_3).zoom_factor());

  // Test with a new external display with a small display dpi.
  const int external_id_4 = 40;
  display::ManagedDisplayInfo external_display_info_4 =
      CreateDisplayInfo(external_id_4, gfx::Rect(1, 1, 3840, 2160));
  const float external_display_dpi_4 = 50.f;
  external_display_info_4.set_device_dpi(external_display_dpi_4);

  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info_4);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // The available zoom factors for 3840X2160 are
  // {1.f, 1.10f, 1.20f, 1.40f, 1.60f, 1.80f, 2.00f, 2.20f, 2.40f}. The expected
  // zoom factor = external_display_dpi_4 /
  // kRecommendedDefaultExternalDisplayDpi = 50 / 96 = 0.52, the closest
  // available zoom factor is 1.
  const float expect_zoom_factor_4 = 1.f;
  EXPECT_EQ(expect_zoom_factor_4,
            GetDisplayInfoForId(external_id_4).zoom_factor());
}

// Test in emulation mode (use_fullscreen_host_window=false)
TEST_F(DisplayManagerTest, EmulatorTest) {
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  display_manager()->AddRemoveDisplay();
  // Add secondary.
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("c0 a1 r0 w1 d1", GetCountSummary());
  reset();

  display_manager()->AddRemoveDisplay();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ("c0 a0 r1 w1 d1", GetCountSummary());
  reset();

  display_manager()->AddRemoveDisplay();
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ("c0 a1 r0 w1 d1", GetCountSummary());
}

// Tests support for 3 displays.
TEST_F(DisplayManagerTest, UpdateThreeDisplaysWithDefaultLayout) {
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Test with three displays. Native origin will not affect ash
  // display layout.
  UpdateDisplay("0+0-640x480,1000+0-320x200,2000+0-400x300");

  EXPECT_EQ(3U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 640, 480),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(640, 0, 320, 200),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(960, 0, 400, 300),
            display_manager()->GetDisplayAt(2).bounds());

  EXPECT_EQ("c1 a2 r0 w1 d1", GetCountSummary());
  // Metrics change immediately when new displays set shelf work area insets.
  // After that, DisplayManager::OnNativeDisplaysChanged trigger changes of the
  // primary display's metrics. So the observed order of changes is [1, 2, 0].
  EXPECT_EQ(display_manager()->GetDisplayAt(0).id(), changed()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(1).id(), added()[0].id());
  EXPECT_EQ(display_manager()->GetDisplayAt(2).id(), added()[1].id());
  EXPECT_EQ(gfx::Rect(0, 0, 640, 480), changed()[0].bounds());
  // Secondary and tertiary displays are on right.
  EXPECT_EQ(gfx::Rect(640, 0, 320, 200), added()[0].bounds());
  EXPECT_EQ(gfx::Rect(1000, 0, 320, 200),
            GetDisplayInfo(added()[0]).bounds_in_native());
  EXPECT_EQ(gfx::Rect(960, 0, 400, 300), added()[1].bounds());
  EXPECT_EQ(gfx::Rect(2000, 0, 400, 300),
            GetDisplayInfo(added()[1]).bounds_in_native());

  // Verify calling ReconfigureDisplays doesn't change anything.
  display_manager()->ReconfigureDisplays();
  EXPECT_EQ(3U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 640, 480),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(640, 0, 320, 200),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(960, 0, 400, 300),
            display_manager()->GetDisplayAt(2).bounds());

  display::DisplayPlacement default_placement(display::DisplayPlacement::BOTTOM,
                                              10);
  display_manager()->layout_store()->SetDefaultDisplayPlacement(
      default_placement);

  // Test with new displays.
  UpdateDisplay("640x480");
  UpdateDisplay("640x480,320x200,400x300");

  EXPECT_EQ(gfx::Rect(0, 0, 640, 480),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(10, 480, 320, 200),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(20, 680, 400, 300),
            display_manager()->GetDisplayAt(2).bounds());
}

TEST_F(DisplayManagerTest, LayoutMoreThanThreeDisplaysTest) {
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 3);
  {
    // Layout: [2]
    //         [1][P]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::LEFT, 10);
    builder.AddDisplayPlacement(list[2], list[1],
                                display::DisplayPlacement::TOP, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300");

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());

    EXPECT_EQ(gfx::Rect(0, 0, 640, 480),
              display_manager()->GetDisplayAt(0).bounds());
    EXPECT_EQ(gfx::Rect(-320, 10, 320, 200),
              display_manager()->GetDisplayAt(1).bounds());

    // The above layout causes an overlap between [P] and [2], making [2]'s
    // bounds be "-310,-290 400x300" if the overlap is not fixed. The overlap
    // must be detected and fixed and [2] is shifted up to remove the overlap.
    EXPECT_EQ(gfx::Rect(-310, -300, 400, 300),
              display_manager()->GetDisplayAt(2).bounds());
  }
  {
    // Layout: [1]
    //         [P][2]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::TOP, 10);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::RIGHT, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300");

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());

    EXPECT_EQ(gfx::Rect(0, 0, 640, 480),
              display_manager()->GetDisplayAt(0).bounds());
    EXPECT_EQ(gfx::Rect(10, -200, 320, 200),
              display_manager()->GetDisplayAt(1).bounds());
    EXPECT_EQ(gfx::Rect(640, 10, 400, 300),
              display_manager()->GetDisplayAt(2).bounds());
  }
  {
    // Layout: [P]
    //         [2]
    //         [1]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], list[2],
                                display::DisplayPlacement::BOTTOM, 10);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::BOTTOM, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300");

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());

    EXPECT_EQ(gfx::Rect(0, 0, 640, 480),
              display_manager()->GetDisplayAt(0).bounds());
    EXPECT_EQ(gfx::Rect(20, 780, 320, 200),
              display_manager()->GetDisplayAt(1).bounds());
    EXPECT_EQ(gfx::Rect(10, 480, 400, 300),
              display_manager()->GetDisplayAt(2).bounds());
  }

  {
    list = display::test::CreateDisplayIdListN(primary_id, 5);
    // Layout: [P][2]
    //      [3][4]
    //      [1]
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::RIGHT, 10);
    builder.AddDisplayPlacement(list[1], list[3],
                                display::DisplayPlacement::BOTTOM, 10);
    builder.AddDisplayPlacement(list[3], list[4],
                                display::DisplayPlacement::LEFT, 10);
    builder.AddDisplayPlacement(list[4], primary_id,
                                display::DisplayPlacement::BOTTOM, 10);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay("640x480,320x200,400x300,300x200,200x100");

    EXPECT_EQ(5U, display_manager()->GetNumDisplays());

    EXPECT_EQ(gfx::Rect(0, 0, 640, 480),
              display_manager()->GetDisplayAt(0).bounds());
    // 2nd is right of the primary.
    EXPECT_EQ(gfx::Rect(640, 10, 400, 300),
              display_manager()->GetDisplayAt(2).bounds());
    // 4th is bottom of the primary.
    EXPECT_EQ(gfx::Rect(10, 480, 200, 100),
              display_manager()->GetDisplayAt(4).bounds());
    // 3rd is the left of 4th.
    EXPECT_EQ(gfx::Rect(-290, 480, 300, 200),
              display_manager()->GetDisplayAt(3).bounds());
    // 1st is the bottom of 3rd.
    EXPECT_EQ(gfx::Rect(-280, 680, 320, 200),
              display_manager()->GetDisplayAt(1).bounds());
  }
}

// Makes sure that layouts with overlapped displays are detected and fixed when
// applied.
TEST_F(DisplayManagerTest, NoOverlappedDisplays) {
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  {
    // Layout with multiple overlaps and special cases:
    //
    //            +-----+
    //       +----+-+6  |
    //       |  5 | |   |
    //  +----+----+ |   |
    //  | 7  |    | |   |
    //  +----+----+-+---+---+-+---------+
    //       |      |   P   | |   2     |
    //       +------+       | |         +----------+
    //              |       | |         |     3    |
    //              |       | |         |          |
    //              +--+----+-+-+-+-----+--+       |
    //                 |    1   | |   4 |  |       |
    //                 |        | |     +--+-------+
    //                 |        | |        |
    //                 +--------+ +--------+

    display::DisplayIdList list =
        display::test::CreateDisplayIdListN(primary_id, 8);
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::BOTTOM, 50);
    builder.AddDisplayPlacement(list[2], list[1],
                                display::DisplayPlacement::TOP, 300);
    builder.AddDisplayPlacement(list[3], list[2],
                                display::DisplayPlacement::RIGHT, 30);
    builder.AddDisplayPlacement(list[4], list[2],
                                display::DisplayPlacement::BOTTOM, 400);
    builder.AddDisplayPlacement(list[5], primary_id,
                                display::DisplayPlacement::LEFT, -300);
    builder.AddDisplayPlacement(list[6], primary_id,
                                display::DisplayPlacement::TOP, -250);
    builder.AddDisplayPlacement(list[7], list[6],
                                display::DisplayPlacement::LEFT, 250);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());

    UpdateDisplay(
        "480x400,480x400,480x400,480x400,480x400,480x400,480x400,530x150");

    // The resulting layout after overlaps had been removed:
    //
    //
    //  +---------+
    //  | 7       +-----+
    //  +-+-------+  6  |
    //    |   5   |     |
    //    |       |     |
    //    |       |     |
    //    |       |-+---+----+---------+
    //    |       | |   P    |   2     |
    //    +-------+ |        |         +----------+
    //              |        |         |     3    |
    //              |        |         |          |
    //              +--+-----+-+-------+          |
    //                 |   1   |       |          |
    //                 |       |  +----+---+------+
    //                 |       |  |   4    |
    //                 +-------+  |        |
    //                            |        |
    //                            +--------+

    EXPECT_EQ(8U, display_manager()->GetNumDisplays());

    EXPECT_EQ(gfx::Rect(0, 0, 480, 400),
              display_manager()->GetDisplayAt(0).bounds());
    EXPECT_EQ(gfx::Rect(50, 400, 480, 400),
              display_manager()->GetDisplayAt(1).bounds());
    EXPECT_EQ(gfx::Rect(480, 0, 480, 400),
              display_manager()->GetDisplayAt(2).bounds());
    EXPECT_EQ(gfx::Rect(960, 30, 480, 400),
              display_manager()->GetDisplayAt(3).bounds());
    EXPECT_EQ(gfx::Rect(730, 430, 480, 400),
              display_manager()->GetDisplayAt(4).bounds());
    EXPECT_EQ(gfx::Rect(-730, -300, 480, 400),
              display_manager()->GetDisplayAt(5).bounds());
    EXPECT_EQ(gfx::Rect(-250, -400, 480, 400),
              display_manager()->GetDisplayAt(6).bounds());
    EXPECT_EQ(gfx::Rect(-780, -450, 530, 150),
              display_manager()->GetDisplayAt(7).bounds());

    // Expect that the displays have been reparented correctly, such that a
    // child is always touching its parent.
    display::DisplayLayoutBuilder expected_layout_builder(primary_id);
    expected_layout_builder.AddDisplayPlacement(
        list[1], primary_id, display::DisplayPlacement::BOTTOM, 50);
    expected_layout_builder.AddDisplayPlacement(
        list[2], list[1], display::DisplayPlacement::TOP, 430);
    expected_layout_builder.AddDisplayPlacement(
        list[3], list[2], display::DisplayPlacement::RIGHT, 30);
    // [4] became a child of [3] instead of [2] as they no longer touch.
    expected_layout_builder.AddDisplayPlacement(
        list[4], list[3], display::DisplayPlacement::BOTTOM, -230);
    // [5] became a child of [6] instead of [P] as they no longer touch.
    expected_layout_builder.AddDisplayPlacement(
        list[5], list[6], display::DisplayPlacement::LEFT, 100);
    expected_layout_builder.AddDisplayPlacement(
        list[6], primary_id, display::DisplayPlacement::TOP, -250);
    expected_layout_builder.AddDisplayPlacement(
        list[7], list[6], display::DisplayPlacement::LEFT, -50);

    const display::DisplayLayout& layout =
        display_manager()->GetCurrentResolvedDisplayLayout();

    EXPECT_TRUE(
        layout.HasSamePlacementList(*(expected_layout_builder.Build())));
  }

  {
    // The following is a special case where a child display is closer to the
    // origin than its parent. Test that we can handle it successfully without
    // introducing a circular dependency.
    //
    // +---------+
    // |    P    |    +---------+
    // |         |    |    3    |
    // |         |    |         |
    // |         |    |         |
    // +------+--+----+         |
    // |    1 |  | 2  +---------+
    // |      |  |    |
    // |      |  |    |
    // |      |  |    |
    // +------+--+----+
    //

    display::DisplayIdList list =
        display::test::CreateDisplayIdListN(primary_id, 4);
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::BOTTOM, 0);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::BOTTOM, 464);
    builder.AddDisplayPlacement(list[3], list[2],
                                display::DisplayPlacement::RIGHT, -700);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());
    UpdateDisplay("696x800,696x800,300x800,696x800");

    // The expected layout should be:
    //
    // +---------+
    // |    P    |       +---------+
    // |         |       |    3    |
    // |         |       |         |
    // |         |       |         |
    // +---------+-------+         |
    // |    1    |   2   +---------+
    // |         |       |
    // |         |       |
    // |         |       |
    // +---------+-------+
    //
    //

    EXPECT_EQ(4U, display_manager()->GetNumDisplays());
    EXPECT_EQ(gfx::Rect(0, 0, 696, 800),
              display_manager()->GetDisplayAt(0).bounds());
    EXPECT_EQ(gfx::Rect(0, 800, 696, 800),
              display_manager()->GetDisplayAt(1).bounds());
    EXPECT_EQ(gfx::Rect(696, 800, 300, 800),
              display_manager()->GetDisplayAt(2).bounds());
    EXPECT_EQ(gfx::Rect(996, 100, 696, 800),
              display_manager()->GetDisplayAt(3).bounds());

    // This case if not handled correctly might lead to a cyclic dependency.
    // Make sure this doesn't happen.
    display::DisplayLayoutBuilder expected_layout_builder(primary_id);
    expected_layout_builder.AddDisplayPlacement(
        list[1], primary_id, display::DisplayPlacement::BOTTOM, 0);
    expected_layout_builder.AddDisplayPlacement(
        list[2], primary_id, display::DisplayPlacement::BOTTOM, 696);
    expected_layout_builder.AddDisplayPlacement(
        list[3], list[2], display::DisplayPlacement::RIGHT, -700);

    const display::DisplayLayout& layout =
        display_manager()->GetCurrentResolvedDisplayLayout();
    EXPECT_TRUE(
        layout.HasSamePlacementList(*(expected_layout_builder.Build())));
  }

  {
    // The following is a layout with an overlap to the left of the primary
    // display.
    //
    // +---------+---------+
    // |    1    |    P    |
    // |         |         |
    // +---------+         |
    // |         |         |
    // +---------+---------+
    // |    2    |
    // |         |
    // +---------+

    display::DisplayIdList list =
        display::test::CreateDisplayIdListN(primary_id, 3);
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::LEFT, 0);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::LEFT, 250);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());
    UpdateDisplay("696x500,696x500,696x500");

    // The expected layout should be:
    //
    // +---------+---------+
    // |    1    |    P    |
    // |         |         |
    // |         |         |
    // |         |         |
    // +---------+---------+
    // |    2    |
    // |         |
    // |         |
    // |         |
    // +---------+

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());
    EXPECT_EQ(gfx::Rect(0, 0, 696, 500),
              display_manager()->GetDisplayAt(0).bounds());
    EXPECT_EQ(gfx::Rect(-696, 0, 696, 500),
              display_manager()->GetDisplayAt(1).bounds());
    EXPECT_EQ(gfx::Rect(-696, 500, 696, 500),
              display_manager()->GetDisplayAt(2).bounds());
  }

  {
    // The following is a layout with an overlap occurring above the primary
    // display.
    //
    //    +------+--+------+
    //    |  2   |  | 1    |
    //    |      |  |      |
    //    |      |  |      |
    //    |      |  |      |
    //    +------+--+------+
    //           |    P    |
    //           |         |
    //           |         |
    //           |         |
    //           +---------+
    //

    display::DisplayIdList list =
        display::test::CreateDisplayIdListN(primary_id, 3);
    display::DisplayLayoutBuilder builder(primary_id);
    builder.AddDisplayPlacement(list[1], primary_id,
                                display::DisplayPlacement::TOP, 0);
    builder.AddDisplayPlacement(list[2], primary_id,
                                display::DisplayPlacement::TOP, -348);
    display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
        list, builder.Build());
    UpdateDisplay("696x500,696x500,696x500");

    // The expected layout should be:
    //
    // +---------+---------+
    // |    2    |    1    |
    // |         |         |
    // |         |         |
    // |         |         |
    // +---------+---------+
    //           |    P    |
    //           |         |
    //           |         |
    //           |         |
    //           +---------+
    //

    EXPECT_EQ(3U, display_manager()->GetNumDisplays());
    EXPECT_EQ(gfx::Rect(0, 0, 696, 500),
              display_manager()->GetDisplayAt(0).bounds());
    EXPECT_EQ(gfx::Rect(0, -500, 696, 500),
              display_manager()->GetDisplayAt(1).bounds());
    EXPECT_EQ(gfx::Rect(-696, -500, 696, 500),
              display_manager()->GetDisplayAt(2).bounds());
  }
}

TEST_F(DisplayManagerTest, NoOverlappedDisplaysNotFitBetweenTwo) {
  //    +------+--+----+--+------+
  //    |  1   |  |  2 |  |  3   |
  //    |      |  |    |  |      |
  //    |      |  |    |  |      |
  //    |      |  |    |  |      |
  //    +-+----+--+----+--+---+--+
  //      |         P         |
  //      |                   |
  //      |                   |
  //      |                   |
  //      +-------------------+
  //

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 4);
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::TOP, -110);
  builder.AddDisplayPlacement(list[2], primary_id,
                              display::DisplayPlacement::TOP, 300);
  builder.AddDisplayPlacement(list[3], primary_id,
                              display::DisplayPlacement::TOP, 600);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("1200x500,600x500,600x500,600x500");

  // The expected layout should be:
  //
  //    +---------+---------+---------+
  //    |    1    |    2    |    3    |
  //    |         |         |         |
  //    |         |         |         |
  //    |         |         |         |
  //    +-+-------+---------+-+-------+
  //      |         P         |
  //      |                   |
  //      |                   |
  //      |                   |
  //      +-------------------+
  //

  EXPECT_EQ(4U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 1200, 500),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(-110, -500, 600, 500),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(490, -500, 600, 500),
            display_manager()->GetDisplayAt(2).bounds());
  EXPECT_EQ(gfx::Rect(1090, -500, 600, 500),
            display_manager()->GetDisplayAt(3).bounds());
}

TEST_F(DisplayManagerTest, NoOverlappedDisplaysAfterResolutionChange) {
  // Starting with a good layout with no overlaps, test that if the resolution
  // of one of the displays is changed, it won't result in any overlaps.
  //
  //         +-------------------+
  //         |         4         |
  //         |                   |
  //         |                   |
  //         |                   |
  //    +----+----+---------+----+----+
  //    |    1    |    2    |    3    |
  //    |         |         |         |
  //    |         |         |         |
  //    |         |         |         |
  //    +----+----+---------+----+----+
  //         |         p         |
  //         |                   |
  //         |                   |
  //         |                   |
  //         +-------------------+
  //

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 5);
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::TOP, -250);
  builder.AddDisplayPlacement(list[2], primary_id,
                              display::DisplayPlacement::TOP, 250);
  builder.AddDisplayPlacement(list[3], primary_id,
                              display::DisplayPlacement::TOP, 750);
  builder.AddDisplayPlacement(list[4], list[1], display::DisplayPlacement::TOP,
                              250);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("1000x500,600x500,600x500,600x500,1000x500");

  // There should be no overlap at all.
  EXPECT_EQ(5U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 500),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(-250, -500, 600, 500),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(350, -500, 600, 500),
            display_manager()->GetDisplayAt(2).bounds());
  EXPECT_EQ(gfx::Rect(950, -500, 600, 500),
            display_manager()->GetDisplayAt(3).bounds());
  EXPECT_EQ(gfx::Rect(0, -1000, 1000, 500),
            display_manager()->GetDisplayAt(4).bounds());

  // Change the resolution of display (2) and expect the following layout.
  //
  //         +-------------------+
  //         |         4         |
  //         |                   |
  //         |                   |
  //         |                   |
  //         +----+-------------++
  //              |      2      |
  //    +---------+             +---------+
  //    |    1    |             |    3    |
  //    |         |             |         |
  //    |         |             |         |
  //    |         |             |         |
  //    +----+----+-------------++--------+
  //         |         p         |
  //         |                   |
  //         |                   |
  //         |                   |
  //         +-------------------+
  //

  UpdateDisplay("1000x500,600x500,600x700,600x500,1000x500");

  EXPECT_EQ(5U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 500),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(-250, -500, 600, 500),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(350, -700, 600, 700),
            display_manager()->GetDisplayAt(2).bounds());
  EXPECT_EQ(gfx::Rect(950, -500, 600, 500),
            display_manager()->GetDisplayAt(3).bounds());
  EXPECT_EQ(gfx::Rect(0, -1200, 1000, 500),
            display_manager()->GetDisplayAt(4).bounds());
}

TEST_F(DisplayManagerTest, NoOverlappedDisplaysWithDetachedDisplays) {
  // Detached displays that intersect other non-detached displays.
  //
  //    +---------+---------+---------+
  //    |    1    |    2    |    3    |
  //    |         |         |         |
  //    |         |         |         |
  //    |         |         |         |
  //    +----+----+-----+---+----+----+
  //         |  4, 5    | P      |
  //         | detached |        |
  //         |          |        |
  //         +----------+        |
  //         +-------------------+
  //

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 6);
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::TOP, -250);
  builder.AddDisplayPlacement(list[2], primary_id,
                              display::DisplayPlacement::TOP, 250);
  builder.AddDisplayPlacement(list[3], primary_id,
                              display::DisplayPlacement::TOP, 750);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  UpdateDisplay("1000x500,600x500,600x500,600x500,500x400,500x400");

  // Detached displays will be de-intersected and reparented appropriately.
  //
  //    +---------+---------+---------+
  //    |    1    |    2    |    3    |
  //    |         |         |         |
  //    |         |         |         |
  //    |         |         |         |
  //    +----+----+---------+----+----+
  //         |         P         |
  //         |                   |
  //         |                   |
  //         |                   |
  //         +----------+--------+
  //         |     4    |
  //         |          |
  //         |          |
  //         +----------+
  //         |     5    |
  //         |          |
  //         |          |
  //         +----------+
  //

  EXPECT_EQ(6U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 1000, 500),
            display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(gfx::Rect(-250, -500, 600, 500),
            display_manager()->GetDisplayAt(1).bounds());
  EXPECT_EQ(gfx::Rect(350, -500, 600, 500),
            display_manager()->GetDisplayAt(2).bounds());
  EXPECT_EQ(gfx::Rect(950, -500, 600, 500),
            display_manager()->GetDisplayAt(3).bounds());
  EXPECT_EQ(gfx::Rect(0, 500, 500, 400),
            display_manager()->GetDisplayAt(4).bounds());
  EXPECT_EQ(gfx::Rect(0, 900, 500, 400),
            display_manager()->GetDisplayAt(5).bounds());

  // This case if not handled correctly might lead to a cyclic dependency.
  // Make sure this doesn't happen.
  display::DisplayLayoutBuilder expected_layout_builder(primary_id);
  expected_layout_builder.AddDisplayPlacement(
      list[1], primary_id, display::DisplayPlacement::TOP, -250);
  expected_layout_builder.AddDisplayPlacement(
      list[2], primary_id, display::DisplayPlacement::TOP, 350);
  expected_layout_builder.AddDisplayPlacement(
      list[3], primary_id, display::DisplayPlacement::TOP, 950);
  expected_layout_builder.AddDisplayPlacement(
      list[4], primary_id, display::DisplayPlacement::BOTTOM, 0);
  expected_layout_builder.AddDisplayPlacement(
      list[5], list[4], display::DisplayPlacement::BOTTOM, 0);

  const display::DisplayLayout& layout =
      display_manager()->GetCurrentResolvedDisplayLayout();
  EXPECT_TRUE(layout.HasSamePlacementList(*(expected_layout_builder.Build())));
}

TEST_F(DisplayManagerTest, OverscanInsetsTest) {
  UpdateDisplay("0+0-500x400,0+501-400x300");
  reset();
  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  const display::ManagedDisplayInfo display_info1 = GetDisplayInfoAt(0);
  const display::ManagedDisplayInfo display_info2 = GetDisplayInfoAt(1);

  display_manager()->SetOverscanInsets(display_info2.id(),
                                       gfx::Insets::TLBR(13, 12, 11, 10));

  std::vector<display::Display> changed_displays = changed();
  ASSERT_EQ(1u, changed_displays.size());
  EXPECT_EQ(display_info2.id(), changed_displays[0].id());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetDisplayInfoAt(0).bounds_in_native());
  display::ManagedDisplayInfo updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ(gfx::Rect(0, 501, 400, 300),
            updated_display_info2.bounds_in_native());
  EXPECT_EQ(gfx::Size(378, 276), updated_display_info2.size_in_pixel());
  EXPECT_EQ(gfx::Insets::TLBR(13, 12, 11, 10),
            updated_display_info2.overscan_insets_in_dip());
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  EXPECT_EQ(gfx::Rect(500, 0, 378, 276),
            display_manager_test.GetSecondaryDisplay().bounds());

  // Make sure that SetOverscanInsets() is idempotent.
  display_manager()->SetOverscanInsets(display_info1.id(), gfx::Insets());
  display_manager()->SetOverscanInsets(display_info2.id(),
                                       gfx::Insets::TLBR(13, 12, 11, 10));
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetDisplayInfoAt(0).bounds_in_native());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ(gfx::Rect(0, 501, 400, 300),
            updated_display_info2.bounds_in_native());
  EXPECT_EQ(gfx::Size(378, 276), updated_display_info2.size_in_pixel());
  EXPECT_EQ(gfx::Insets::TLBR(13, 12, 11, 10),
            updated_display_info2.overscan_insets_in_dip());

  display_manager()->SetOverscanInsets(display_info2.id(),
                                       gfx::Insets::TLBR(10, 11, 12, 13));
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetDisplayInfoAt(0).bounds_in_native());
  EXPECT_EQ(gfx::Size(376, 278), GetDisplayInfoAt(1).size_in_pixel());
  EXPECT_EQ(gfx::Insets::TLBR(10, 11, 12, 13),
            GetDisplayInfoAt(1).overscan_insets_in_dip());

  // Recreate a new 2nd display. It won't apply the overscan inset because the
  // new display has a different ID.
  UpdateDisplay("0+0-500x400");
  UpdateDisplay("0+0-500x400,0+501-400x300");
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetDisplayInfoAt(0).bounds_in_native());
  EXPECT_EQ(gfx::Rect(0, 501, 400, 300),
            GetDisplayInfoAt(1).bounds_in_native());

  // Recreate the displays with the same ID.  It should apply the overscan
  // inset.
  UpdateDisplay("0+0-500x400");

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(display_info1);
  display_info_list.push_back(display_info2);

  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetDisplayInfoAt(0).bounds_in_native());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ(gfx::Size(376, 278), updated_display_info2.size_in_pixel());
  EXPECT_EQ(gfx::Insets::TLBR(10, 11, 12, 13),
            updated_display_info2.overscan_insets_in_dip());

  // HiDPI but overscan display. The specified insets size should be doubled.
  UpdateDisplay("0+0-500x400,0+501-400x300*2");
  display_manager()->SetOverscanInsets(display_manager()->GetDisplayAt(1).id(),
                                       gfx::Insets::TLBR(4, 5, 6, 7));
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400), GetDisplayInfoAt(0).bounds_in_native());
  updated_display_info2 = GetDisplayInfoAt(1);
  EXPECT_EQ(gfx::Rect(0, 501, 400, 300),
            updated_display_info2.bounds_in_native());
  EXPECT_EQ(gfx::Size(376, 280), updated_display_info2.size_in_pixel());
  EXPECT_EQ(gfx::Insets::TLBR(4, 5, 6, 7),
            updated_display_info2.overscan_insets_in_dip());
  EXPECT_EQ(gfx::Insets::TLBR(8, 10, 12, 14),
            updated_display_info2.GetOverscanInsetsInPixel());

  // Make sure switching primary display applies the overscan offset only once.
  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      display_manager_test.GetSecondaryDisplay().id());
  EXPECT_EQ(gfx::Rect(-500, 0, 500, 400),
            display_manager_test.GetSecondaryDisplay().bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayInfo(display_manager_test.GetSecondaryDisplay())
                .bounds_in_native());
  EXPECT_EQ(gfx::Rect(0, 501, 400, 300),
            GetDisplayInfo(display::Screen::GetScreen()->GetPrimaryDisplay())
                .bounds_in_native());
  EXPECT_EQ(gfx::Rect(0, 0, 188, 140),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());

  // Make sure just moving the overscan area should property notify observers.
  UpdateDisplay("0+0-500x400");
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display_manager()->SetOverscanInsets(primary_id,
                                       gfx::Insets::TLBR(0, 0, 20, 20));
  EXPECT_EQ(gfx::Rect(0, 0, 480, 380),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  reset();
  display_manager()->SetOverscanInsets(primary_id, gfx::Insets(10));
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_EQ(gfx::Rect(0, 0, 480, 380),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  reset();
  display_manager()->SetOverscanInsets(primary_id, gfx::Insets());
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
}

TEST_F(DisplayManagerTest, ZeroOverscanInsets) {
  // Make sure the display change events is emitted for overscan inset changes.
  UpdateDisplay("0+0-500x400,0+501-400x300");
  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  int64_t display2_id = display_manager()->GetDisplayAt(1).id();

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets());
  EXPECT_EQ(0u, changed().size());

  reset();
  display_manager()->SetOverscanInsets(display2_id,
                                       gfx::Insets::TLBR(1, 0, 0, 0));
  ASSERT_EQ(1u, changed().size());
  EXPECT_EQ(display2_id, changed()[0].id());

  reset();
  display_manager()->SetOverscanInsets(display2_id, gfx::Insets());
  ASSERT_EQ(1u, changed().size());
  EXPECT_EQ(display2_id, changed()[0].id());
}

TEST_F(DisplayManagerTest, TouchCalibrationTest) {
  UpdateDisplay("0+0-500x400,0+501-1024x600");
  reset();
  display::TouchDeviceManager* touch_device_manager =
      display_manager()->touch_device_manager();
  display::test::TouchDeviceManagerTestApi tdm_test_api(touch_device_manager);

  const ui::TouchscreenDevice touchdevice(
      11, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test touch device"), gfx::Size(123, 456), 1);

  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  const display::ManagedDisplayInfo display_info1 = GetDisplayInfoAt(0);
  const display::ManagedDisplayInfo display_info2 = GetDisplayInfoAt(1);

  EXPECT_FALSE(tdm_test_api.AreAssociated(display_info2, touchdevice));

  const display::TouchCalibrationData::CalibrationPointPairQuad
      point_pair_quad = {
          {std::make_pair(gfx::Point(50, 50), gfx::Point(43, 51)),
           std::make_pair(gfx::Point(950, 50), gfx::Point(975, 45)),
           std::make_pair(gfx::Point(50, 550), gfx::Point(48, 534)),
           std::make_pair(gfx::Point(950, 550), gfx::Point(967, 574))}};
  const gfx::Size bounds_at_calibration(display_info2.size_in_pixel());
  const display::TouchCalibrationData touch_data(point_pair_quad,
                                                 bounds_at_calibration);

  // Set the touch calibration data for the secondary display.
  display_manager()->SetTouchCalibrationData(
      display_info2.id(), point_pair_quad, bounds_at_calibration, touchdevice,
      /*apply_spatial_calibration=*/true);

  EXPECT_TRUE(tdm_test_api.AreAssociated(display_info2, touchdevice));
  EXPECT_EQ(touch_data, touch_device_manager->GetCalibrationData(
                            touchdevice, display_info2.id()));

  // Clearing touch calibration data from the secondary display.
  touch_device_manager->ClearTouchCalibrationData(touchdevice,
                                                  GetDisplayInfoAt(1).id());

  EXPECT_TRUE(touch_device_manager
                  ->GetCalibrationData(touchdevice, GetDisplayInfoAt(1).id())
                  .IsEmpty());

  // Make sure that SetTouchCalibrationData() is idempotent.
  display::TouchCalibrationData::CalibrationPointPairQuad point_pair_quad_2 =
      point_pair_quad;
  point_pair_quad_2[1] =
      std::make_pair(gfx::Point(950, 50), gfx::Point(975, 53));
  display::TouchCalibrationData touch_data_2(point_pair_quad_2,
                                             bounds_at_calibration);
  display_manager()->SetTouchCalibrationData(
      display_info2.id(), point_pair_quad_2, bounds_at_calibration, touchdevice,
      /*apply_spatial_calibration=*/true);

  EXPECT_EQ(touch_data_2, touch_device_manager->GetCalibrationData(
                              touchdevice, GetDisplayInfoAt(1).id()));

  // Recreate a new 2nd display. It won't apply the touch calibration data
  // because the new display has a different ID.
  UpdateDisplay("0+0-500x400");
  UpdateDisplay("0+0-500x400,0+501-400x300");
  tdm_test_api.ResetTouchDeviceManager();

  // Recreate the displays with the same ID.  It should apply the touch
  // calibration associated data.
  UpdateDisplay("0+0-500x400");
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(display_info1);
  display_info_list.push_back(display_info2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Make sure multiple touch devices works.
  display_manager()->SetTouchCalibrationData(
      display_info2.id(), point_pair_quad, bounds_at_calibration, touchdevice,
      /*apply_spatial_calibration=*/true);

  EXPECT_EQ(touch_data, touch_device_manager->GetCalibrationData(
                            touchdevice, GetDisplayInfoAt(1).id()));

  const ui::TouchscreenDevice touchdevice_2(
      12, ui::InputDeviceType::INPUT_DEVICE_USB,
      std::string("test touch device 2"), gfx::Size(234, 567), 1);

  display_manager()->SetTouchCalibrationData(
      display_info2.id(), point_pair_quad_2, bounds_at_calibration,
      touchdevice_2, /*apply_spatial_calibration=*/true);
  EXPECT_EQ(touch_data_2, touch_device_manager->GetCalibrationData(
                              touchdevice_2, GetDisplayInfoAt(1).id()));
  EXPECT_EQ(touch_data, touch_device_manager->GetCalibrationData(
                            touchdevice, GetDisplayInfoAt(1).id()));
}

TEST_F(DisplayManagerTest, UpdateDisplayZoomTest) {
  // Initialize a display pair.
  UpdateDisplay("1920x1080#1280x720|640x480%60, 600x400*2#600x400");
  reset();

  // The second display has a device scale factor of 2 set.
  constexpr float display_2_dsf = 2.0f;

  ASSERT_EQ(2u, display_manager()->GetNumDisplays());
  const display::ManagedDisplayInfo& info_1 = GetDisplayInfoAt(0);

  // The display should have 2 display modes based on the initialization spec.
  ASSERT_EQ(2u, info_1.display_modes().size());

  const display::ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info_1.display_modes();

  // Set the display mode.
  display::test::SetDisplayResolution(display_manager(), info_1.id(),
                                      modes[0].size());
  display_manager()->UpdateDisplays();

  // Since no zoom factor or device scale factor has been set on the display,
  // the total/effective device scale factor on the display is 1.
  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_1.id()).device_scale_factor(),
      1.f);

  float zoom_factor_1 = 2.0f;
  display_manager()->UpdateZoomFactor(info_1.id(), zoom_factor_1);
  EXPECT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
            zoom_factor_1);

  // With the zoom factor set for the display. The effective zoom factor
  // returned should have the display zoom taken into consideration.
  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_1.id()).device_scale_factor(),
      zoom_factor_1);

  // Update the zoom factor for a different display mode.
  float zoom_factor_2 = 1.5f;
  display_manager()->UpdateZoomFactor(info_1.id(), zoom_factor_2);

  EXPECT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
            zoom_factor_2);

  // Change the display mode of the device.
  display::test::SetDisplayResolution(display_manager(), info_1.id(),
                                      modes[1].size());
  display_manager()->UpdateDisplays();

  // Since the display mode was changed, the zoom factor for the display will
  // be retrieved from cache. If not available in cache, default to 1.
  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_1.id()).device_scale_factor(),
      1.f);

  // When setting the display mode back to the old one, the final effective
  // device scale factor should be using the correct zoom factor.
  display::test::SetDisplayResolution(display_manager(), info_1.id(),
                                      modes[0].size());
  display_manager()->UpdateDisplays();

  // Set the zoom factor back to |zoom_factor_2| for first display.
  display_manager()->UpdateZoomFactor(info_1.id(), zoom_factor_2);
  EXPECT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
            zoom_factor_2);
  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_1.id()).device_scale_factor(),
      zoom_factor_2);

  // Update the zoom factor for the second display.
  float zoom_factor_3 = 1.25f;
  const display::ManagedDisplayInfo& info_2 = GetDisplayInfoAt(1);
  display_manager()->UpdateZoomFactor(info_2.id(), zoom_factor_3);
  EXPECT_EQ(display_manager()->GetDisplayInfo(info_2.id()).zoom_factor(),
            zoom_factor_3);
  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_2.id()).device_scale_factor(),
      zoom_factor_3 * display_2_dsf);

  // Modifying zoom factor for a display should not effect zoom factors of
  // other displays.
  EXPECT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
            zoom_factor_2);

  // Update the zoom factor for display to see if it gets reflected.
  display_manager()->UpdateZoomFactor(info_1.id(), zoom_factor_3);

  EXPECT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
            zoom_factor_3);
  display::test::SetDisplayResolution(display_manager(), info_1.id(),
                                      modes[0].size());
  display_manager()->UpdateDisplays();

  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_1.id()).device_scale_factor(),
      zoom_factor_3);
  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_2.id()).device_scale_factor(),
      zoom_factor_3 * display_2_dsf);
}

TEST_F(DisplayManagerTest, ZoomDisplay) {
  // Initialize a display pair.
  UpdateDisplay("1920x1080#1920x1080|1280x720%60, 2560x1440*2#2560x1440");
  reset();

  ASSERT_EQ(2u, display_manager()->GetNumDisplays());

  const display::ManagedDisplayInfo& info_1 = GetDisplayInfoAt(0);
  const display::ManagedDisplayInfo::ManagedDisplayModeList& modes_1 =
      info_1.display_modes();

  const display::ManagedDisplayInfo& info_2 = GetDisplayInfoAt(1);
  const display::ManagedDisplayInfo::ManagedDisplayModeList& modes_2 =
      info_2.display_modes();

  // Set the display mode for each display.
  display::test::SetDisplayResolution(display_manager(), info_1.id(),
                                      modes_1[0].size());
  display::test::SetDisplayResolution(display_manager(), info_2.id(),
                                      modes_2[0].size());
  display_manager()->UpdateDisplays();

  // Enumerate the zoom factors for display.
  const std::vector<float> zoom_factors_1 =
      display::GetDisplayZoomFactors(modes_1[0]);

  // Set the zoom factor to one of the enumerated zoom factors for the said
  // display.
  const std::size_t zoom_factor_idx_1 = 0;
  display_manager()->UpdateZoomFactor(info_1.id(),
                                      zoom_factors_1[zoom_factor_idx_1]);

  // Make sure the change was successful.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
                  zoom_factors_1[zoom_factor_idx_1]);

  // Zoom out the display. This should have no effect, since the display is
  // already at the minimum zoom level.
  display_manager()->ZoomDisplay(info_1.id(), true /* up */);
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
                  zoom_factors_1[zoom_factor_idx_1]);

  // Ensure that this call did not modify the zoom value for the other display.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_2.id()).zoom_factor(),
                  1.f);

  // Zoom in the display.
  display_manager()->ZoomDisplay(info_1.id(), false /* up */);

  // The zoom factor for the display should be set to the next zoom factor in
  // list.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
                  zoom_factors_1[zoom_factor_idx_1 + 1]);

  // Zoom out the display.
  display_manager()->ZoomDisplay(info_1.id(), true /* up */);

  // The zoom level should decrease from the previous level.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
                  zoom_factors_1[zoom_factor_idx_1]);

  // Enumerate the zoom factors for display.
  const std::vector<float> zoom_factors_2 =
      display::GetDisplayZoomFactors(modes_2[0]);

  // Set the zoom factor to one of the enumerated zoom factors for the said
  // display.
  const std::size_t zoom_factor_idx_2 = zoom_factors_2.size() - 1;
  display_manager()->UpdateZoomFactor(info_2.id(),
                                      zoom_factors_2[zoom_factor_idx_2]);

  // Make sure the change was successful.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_2.id()).zoom_factor(),
                  zoom_factors_2[zoom_factor_idx_2]);

  // Zoom in the display. This should have no effect since we are already at
  // maximum zoom.
  display_manager()->ZoomDisplay(info_2.id(), false /* up */);
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_2.id()).zoom_factor(),
                  zoom_factors_2[zoom_factor_idx_2]);

  // Zoom out the display
  display_manager()->ZoomDisplay(info_2.id(), true /* up */);
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_2.id()).zoom_factor(),
                  zoom_factors_2[zoom_factor_idx_2 - 1]);

  // Ensure that this call did not modify the zoom value for the other display.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
                  zoom_factors_1[zoom_factor_idx_1]);

  // Reset the zoom value for displays.
  display_manager()->ResetDisplayZoom(info_1.id());
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
                  1.f);
  // Resetting the zoom level of one display should not effect the other display
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_2.id()).zoom_factor(),
                  zoom_factors_2[zoom_factor_idx_2 - 1]);

  // Now reset the zoom value for other display.
  display_manager()->ResetDisplayZoom(info_2.id());
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_2.id()).zoom_factor(),
                  1.f);
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info_1.id()).zoom_factor(),
                  1.f);
}

TEST_F(DisplayManagerTest, ZoomFactorMapTest) {
  // Initialize a display pair.
  UpdateDisplay("2560x1440#1920x1080|1280x720");
  reset();

  ASSERT_EQ(1u, display_manager()->GetNumDisplays());

  const display::ManagedDisplayInfo& info = GetDisplayInfoAt(0);
  const display::ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info.display_modes();

  // Set the display mode.
  display::test::SetDisplayResolution(display_manager(), info.id(),
                                      modes[0].size());
  display_manager()->UpdateDisplays();

  // Set the zoom factor.
  float zoom_factor_1 = 1.2f;
  display_manager()->UpdateZoomFactor(info.id(), zoom_factor_1);

  // Make sure the change was successful.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info.id()).zoom_factor(),
                  zoom_factor_1);

  // Make sure zoom factor is stored in cache.
  display::DisplaySizeToZoomFactorMap zoom_factor_map =
      display_manager()->GetDisplayInfo(info.id()).zoom_factor_map();
  const auto iter1 = zoom_factor_map.find(modes[0].size().ToString());
  EXPECT_NE(iter1, zoom_factor_map.end());
  EXPECT_EQ(iter1->second, zoom_factor_1);

  // Set to a different display mode.
  display::test::SetDisplayResolution(display_manager(), info.id(),
                                      modes[1].size());
  display_manager()->UpdateDisplays();

  // If not available in cache, default zoom factor is 1.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info.id()).zoom_factor(),
                  1.f);

  // Set the zoom factor.
  float zoom_factor_2 = 1.5f;
  display_manager()->UpdateZoomFactor(info.id(), zoom_factor_2);

  // Make sure the change was successful.
  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info.id()).zoom_factor(),
                  zoom_factor_2);

  // Make sure zoom factor is stored in cache.
  zoom_factor_map =
      display_manager()->GetDisplayInfo(info.id()).zoom_factor_map();
  const auto iter2 = zoom_factor_map.find(modes[1].size().ToString());
  EXPECT_NE(iter2, zoom_factor_map.end());
  EXPECT_EQ(iter2->second, zoom_factor_2);

  // Set back to previous display mode.
  display::test::SetDisplayResolution(display_manager(), info.id(),
                                      modes[0].size());
  display_manager()->UpdateDisplays();

  // Make sure zoom factor is retrieved from cache.
  zoom_factor_map =
      display_manager()->GetDisplayInfo(info.id()).zoom_factor_map();
  const auto iter3 = zoom_factor_map.find(modes[0].size().ToString());
  EXPECT_NE(iter3, zoom_factor_map.end());

  EXPECT_FLOAT_EQ(display_manager()->GetDisplayInfo(info.id()).zoom_factor(),
                  iter3->second);
}

TEST_F(DisplayManagerTest, TestDeviceScaleOnlyChange) {
  UpdateDisplay("1000x600");
  aura::WindowTreeHost* host = Shell::GetPrimaryRootWindow()->GetHost();
  EXPECT_EQ(1, host->compositor()->device_scale_factor());
  EXPECT_EQ(gfx::Size(1000, 600),
            Shell::GetPrimaryRootWindow()->bounds().size());
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());

  UpdateDisplay("1000x600*2");
  EXPECT_EQ(2, host->compositor()->device_scale_factor());
  EXPECT_EQ("c2 a0 r0 w2 d2", GetCountSummary());
  EXPECT_EQ(gfx::Size(500, 300),
            Shell::GetPrimaryRootWindow()->bounds().size());
}

TEST_F(DisplayManagerTest, TestNativeDisplaysChanged) {
  // Disable restoring mirror mode to prevent interference from previous
  // display configuration.
  display_manager()->set_disable_restoring_mirror_mode_for_test(true);

  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const int external_id = 10;
  const int mirror_id = 11;
  const int64_t invalid_id = display::kInvalidDisplayId;
  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 400));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(external_id, gfx::Rect(1, 1, 200, 100));
  const display::ManagedDisplayInfo mirroring_display_info =
      CreateDisplayInfo(mirror_id, gfx::Rect(0, 0, 500, 400));

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  gfx::Rect default_bounds = display_manager()->GetDisplayAt(0).bounds();

  std::vector<display::ManagedDisplayInfo> display_info_list;
  // Primary disconnected.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(default_bounds, display_manager()->GetDisplayAt(0).bounds());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // External connected while primary was disconnected.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  EXPECT_EQ(invalid_id, GetDisplayForId(internal_display_id).id());
  EXPECT_EQ(gfx::Rect(1, 1, 200, 100),
            GetDisplayInfoForId(external_id).bounds_in_native());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(external_id,
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  EXPECT_EQ(internal_display_id, display::Display::InternalDisplayId());

  // Primary connected, with different bounds.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(internal_display_id,
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  // This combination is new, so internal display becomes primary.
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(gfx::Rect(1, 1, 200, 100),
            GetDisplayInfoForId(10).bounds_in_native());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));

  // Emulate suspend.
  display_info_list.clear();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(gfx::Rect(1, 1, 200, 100),
            GetDisplayInfoForId(10).bounds_in_native());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));

  // External display has disconnected then resumed.
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // External display was changed during suspend.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // suspend...
  display_info_list.clear();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // and resume with different external display.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(CreateDisplayInfo(12, gfx::Rect(1, 1, 200, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // mirrored...
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(mirroring_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(11U, display_manager()->GetMirroringDestinationDisplayIdList()[0]);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // Test display name.
  EXPECT_EQ(ToDisplayName(internal_display_id),
            display_manager()->GetDisplayNameForId(internal_display_id));
  EXPECT_EQ("Display-10", display_manager()->GetDisplayNameForId(10));
  EXPECT_EQ("Display-11", display_manager()->GetDisplayNameForId(11));
  EXPECT_EQ("Display-12", display_manager()->GetDisplayNameForId(12));
  // Default name for the id that doesn't exist.
  EXPECT_EQ("Display 100", display_manager()->GetDisplayNameForId(100));

  // and exit mirroring.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(gfx::Rect(500, 0, 200, 100), GetDisplayForId(10).bounds());

  // Turn off internal
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(invalid_id, GetDisplayForId(internal_display_id).id());
  EXPECT_EQ(gfx::Rect(1, 1, 200, 100),
            GetDisplayInfoForId(external_id).bounds_in_native());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Switched to another display
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayInfoForId(internal_display_id).bounds_in_native());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  display_manager()->set_disable_restoring_mirror_mode_for_test(false);
}

// Make sure crash does not happen if add and remove happens at the same time.
// See: crbug.com/414394
TEST_F(DisplayManagerTest, DisplayAddRemoveAtTheSameTime) {
  UpdateDisplay("100+0-500x400,0+501-400x300");

  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id = display_manager_test.GetSecondaryDisplay().id();

  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);

  // An id which is different from primary and secondary.
  const int64_t third_id = display::SynthesizeDisplayIdFromSeed(secondary_id);

  display::ManagedDisplayInfo third_info =
      CreateDisplayInfo(third_id, gfx::Rect(0, 0, 600, 500));

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(third_info);
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Secondary secondary_id becomes the primary as it has smaller output index.
  EXPECT_EQ(secondary_id, WindowTreeHostManager::GetPrimaryDisplayId());
  EXPECT_EQ(third_id, display_manager_test.GetSecondaryDisplay().id());
  EXPECT_EQ(gfx::Size(600, 500), GetDisplayForId(third_id).size());
}

TEST_F(DisplayManagerTest, TestNativeDisplaysChangedNoInternal) {
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Don't change the display info if all displays are disconnected.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Connect another display which will become primary.
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(10, gfx::Rect(1, 1, 200, 100));
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(1, 1, 200, 100),
            GetDisplayInfoForId(10).bounds_in_native());
  EXPECT_EQ(
      gfx::Size(200, 100),
      Shell::GetPrimaryRootWindow()->GetHost()->GetBoundsInPixels().size());
}

// TODO(crbug.com/40902297): Fix the test flakiness on MSan.
#if defined(MEMORY_SANITIZER)
#define MAYBE_NativeDisplaysChangedAfterPrimaryChange \
  DISABLED_NativeDisplaysChangedAfterPrimaryChange
#else
#define MAYBE_NativeDisplaysChangedAfterPrimaryChange \
  NativeDisplaysChangedAfterPrimaryChange
#endif
TEST_F(DisplayManagerTest, MAYBE_NativeDisplaysChangedAfterPrimaryChange) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  const display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 400));
  const display::ManagedDisplayInfo secondary_display_info =
      CreateDisplayInfo(10, gfx::Rect(1, 1, 200, 100));

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_info_list.push_back(secondary_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(gfx::Rect(500, 0, 200, 100), GetDisplayForId(10).bounds());

  Shell::Get()->window_tree_host_manager()->SetPrimaryDisplayId(
      secondary_display_info.id());
  EXPECT_EQ(gfx::Rect(-500, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 100), GetDisplayForId(10).bounds());

  // OnNativeDisplaysChanged may change the display bounds.  Here makes sure
  // nothing changed if the exactly same displays are specified.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(gfx::Rect(-500, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());
  EXPECT_EQ(gfx::Rect(0, 0, 200, 100), GetDisplayForId(10).bounds());
}

TEST_F(DisplayManagerTest, ActiveModeWhenNativeResolutionNotSupported) {
  int display_id = 1000;
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 800, 300));
  native_display_info.set_is_interlaced(false);
  native_display_info.set_native(false);
  native_display_info.set_refresh_rate(59.0f);

  display::ManagedDisplayInfo::ManagedDisplayModeList display_modes;
  display_modes.emplace_back(gfx::Size(1000, 500), 58.0f,
                             /*is_interlaced=*/false, /*native=*/true);
  display_modes.emplace_back(gfx::Size(800, 300), 59.0f,
                             /*is_interlaced=*/false, /*native=*/false);
  display_modes.emplace_back(gfx::Size(400, 500), 60.0f,
                             /*is_interlaced=*/false, /*native=*/false);
  native_display_info.SetManagedDisplayModes(display_modes);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  display::ManagedDisplayMode expected_mode(gfx::Size(800, 300), 59.0f,
                                            /*is_interlaced=*/false,
                                            /*native=*/false);

  // Make sure there is no selected mode.
  display::ManagedDisplayMode mode;
  EXPECT_FALSE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));

  // Check display info for the active mode to handle the case when native mode
  // is not supported.
  display::ManagedDisplayMode active_mode;
  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(display_id, &active_mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(active_mode));
}

TEST_F(DisplayManagerTest, DontRememberBestResolution) {
  int display_id = 1000;
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
  display::ManagedDisplayInfo::ManagedDisplayModeList display_modes;
  display_modes.emplace_back(gfx::Size(1000, 500), 58.0f,
                             /*is_interlaced=*/false, /*native=*/true);
  display_modes.emplace_back(gfx::Size(800, 300), 59.0f,
                             /*is_interlaced=*/false, /*native=*/false);
  display_modes.emplace_back(gfx::Size(400, 500), 60.0f,
                             /*is_interlaced=*/false, /*native=*/false);

  native_display_info.SetManagedDisplayModes(display_modes);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  display::ManagedDisplayMode expected_mode(gfx::Size(1000, 500), 58.0f,
                                            /*is_interlaced=*/false,
                                            /*native=*/true);

  display::ManagedDisplayMode mode;
  EXPECT_FALSE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  display::ManagedDisplayMode active_mode;
  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(display_id, &active_mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(active_mode));

  // Unsupported resolution.
  display::test::SetDisplayResolution(display_manager(), display_id,
                                      gfx::Size(800, 4000));
  EXPECT_FALSE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(display_id, &active_mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(active_mode));

  // Supported resolution.
  display::test::SetDisplayResolution(display_manager(), display_id,
                                      gfx::Size(800, 300));
  EXPECT_TRUE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  EXPECT_EQ(gfx::Size(800, 300), mode.size());
  EXPECT_EQ(59.0f, mode.refresh_rate());
  EXPECT_FALSE(mode.native());

  expected_mode = display::ManagedDisplayMode(
      gfx::Size(800, 300), 59.0f, /*is_interlaced=*/false, /*native=*/false);

  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(display_id, &active_mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(active_mode));

  // Best resolution.
  display::test::SetDisplayResolution(display_manager(), display_id,
                                      gfx::Size(1000, 500));
  EXPECT_TRUE(
      display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
  EXPECT_EQ(gfx::Size(1000, 500), mode.size());
  EXPECT_EQ(58.0f, mode.refresh_rate());
  EXPECT_TRUE(mode.native());

  expected_mode = display::ManagedDisplayMode(
      gfx::Size(1000, 500), 58.0f, /*is_interlaced=*/false, /*native=*/true);

  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(display_id, &active_mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(active_mode));
}

TEST_F(DisplayManagerTest, ResolutionFallback) {
  int display_id = 1000;
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
  display::ManagedDisplayInfo::ManagedDisplayModeList display_modes;
  display_modes.emplace_back(gfx::Size(1000, 500), 60.0f,
                             /*is_interlaced=*/false, /*native=*/true);
  display_modes.emplace_back(gfx::Size(800, 300), 59.0f,
                             /*is_interlaced=*/false, /*native=*/false);
  display_modes.emplace_back(gfx::Size(400, 500), 60.0f,
                             /*is_interlaced=*/false, /*native=*/false);

  native_display_info.SetManagedDisplayModes(display_modes);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  {
    display::test::SetDisplayResolution(display_manager(), display_id,
                                        gfx::Size(800, 300));
    display::ManagedDisplayInfo new_native_display_info =
        CreateDisplayInfo(display_id, gfx::Rect(0, 0, 400, 500));
    new_native_display_info.SetManagedDisplayModes(display_modes);
    std::vector<display::ManagedDisplayInfo> new_display_info_list;
    new_display_info_list.push_back(new_native_display_info);
    display_manager()->OnNativeDisplaysChanged(new_display_info_list);

    display::ManagedDisplayMode mode;
    EXPECT_TRUE(
        display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
    EXPECT_EQ(gfx::Size(400, 500), mode.size());
    EXPECT_EQ(60.0f, mode.refresh_rate());
    EXPECT_FALSE(mode.native());
  }
  {
    // Best resolution should find itself on the resolutions list.
    display::test::SetDisplayResolution(display_manager(), display_id,
                                        gfx::Size(800, 300));
    display::ManagedDisplayInfo new_native_display_info =
        CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1000, 500));
    new_native_display_info.set_native(true);
    new_native_display_info.SetManagedDisplayModes(display_modes);
    std::vector<display::ManagedDisplayInfo> new_display_info_list;
    new_display_info_list.push_back(new_native_display_info);
    display_manager()->OnNativeDisplaysChanged(new_display_info_list);

    display::ManagedDisplayMode mode;
    EXPECT_TRUE(
        display_manager()->GetSelectedModeForDisplayId(display_id, &mode));
    EXPECT_EQ(gfx::Size(1000, 500), mode.size());
    EXPECT_EQ(60.0f, mode.refresh_rate());
    EXPECT_TRUE(mode.native());
  }
}

TEST_F(DisplayManagerTest, DisplayRemovedOnlyOnceWhenEnteringDockedMode) {
  // Create two displays, one internal, and one external, such that the full ID
  // of the internal display is *greater* than the full ID of the external
  // display, but the port-index part (least significant 8-bit) of the ID of the
  // internal display is *less* than the port-index part of the external
  // display.
  constexpr int64_t kInternalDisplayId = 0x4D10DBEBF24802LL;
  constexpr int64_t kExternalDisplayId = 0x4CABEF61B95735LL;
  const auto internal_info = display::ManagedDisplayInfo::CreateFromSpecWithID(
      "0+0-400x300", kInternalDisplayId);
  const auto external_info = display::ManagedDisplayInfo::CreateFromSpecWithID(
      "401+0-600x500", kExternalDisplayId);
  vector<display::ManagedDisplayInfo> display_info_list{internal_info,
                                                        external_info};
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Switching to docked mode in this configuration should result in only a
  // single display removal, and no new display additions.
  // https://crbug.com/921275.
  reset();
  display_info_list.clear();
  display_info_list.emplace_back(external_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // There should only be 1 display change, 0 adds, and 1 removal.
  EXPECT_EQ("c1 a0 r1 w1 d1", GetCountSummary());
  const int expected_changed_metrics =
      display::DisplayObserver::DISPLAY_METRIC_BOUNDS |
      display::DisplayObserver::DISPLAY_METRIC_WORK_AREA |
      display::DisplayObserver::DISPLAY_METRIC_PRIMARY;
  EXPECT_EQ(expected_changed_metrics, changed_metrics());

  // Exit docked mode by re-adding the internal display again.
  reset();
  display_info_list.clear();
  display_info_list.emplace_back(internal_info);
  display_info_list.emplace_back(external_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Expect that we get a "primary" change notification.
  EXPECT_EQ("c4 a1 r0 w1 d1", GetCountSummary());
  EXPECT_EQ(expected_changed_metrics, changed_metrics());
}

TEST_F(DisplayManagerTest, Rotate) {
  UpdateDisplay("100x200/r,300x400/l");
  EXPECT_EQ(gfx::Rect(1, 1, 100, 200), GetDisplayInfoAt(0).bounds_in_native());
  EXPECT_EQ(gfx::Size(200, 100), GetDisplayInfoAt(0).size_in_pixel());

  EXPECT_EQ(gfx::Rect(1, 201, 300, 400),
            GetDisplayInfoAt(1).bounds_in_native());
  EXPECT_EQ(gfx::Size(400, 300), GetDisplayInfoAt(1).size_in_pixel());
  reset();
  UpdateDisplay("100x200/b,300x400");
  EXPECT_EQ("c2 a0 r0 w1 d1", GetCountSummary());
  reset();

  EXPECT_EQ(gfx::Rect(1, 1, 100, 200), GetDisplayInfoAt(0).bounds_in_native());
  EXPECT_EQ(gfx::Size(100, 200), GetDisplayInfoAt(0).size_in_pixel());

  EXPECT_EQ(gfx::Rect(1, 201, 300, 400),
            GetDisplayInfoAt(1).bounds_in_native());
  EXPECT_EQ(gfx::Size(300, 400), GetDisplayInfoAt(1).size_in_pixel());

  // Just Rotating display will change the bounds on both display.
  UpdateDisplay("100x200/l,300x400");
  EXPECT_EQ("c2 a0 r0 w1 d1", GetCountSummary());
  reset();

  // Updating to the same configuration should report no changes. A will/did
  // change is still sent.
  UpdateDisplay("100x200/l,300x400");
  EXPECT_EQ("c0 a0 r0 w1 d1", GetCountSummary());
  reset();

  // Rotating 180 degrees should report one change.
  UpdateDisplay("100x200/r,300x400");
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());
  reset();

  UpdateDisplay("300x200");
  EXPECT_EQ("c1 a0 r1 w1 d1", GetCountSummary());
  reset();

  // Rotating 180 degrees should report one change.
  UpdateDisplay("300x200/u");
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());
  reset();

  UpdateDisplay("300x200/l");
  EXPECT_EQ("c1 a0 r0 w1 d1", GetCountSummary());

  // Having the internal display deactivated should restore user rotation. Newly
  // set rotations should be applied.
  UpdateDisplay("300x200, 300x200");
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  display_manager()->SetDisplayRotation(internal_display_id,
                                        display::Display::ROTATE_90,
                                        display::Display::RotationSource::USER);
  display_manager()->SetDisplayRotation(
      internal_display_id, display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);

  const display::ManagedDisplayInfo info =
      GetDisplayInfoForId(internal_display_id);
  EXPECT_EQ(display::Display::ROTATE_0, info.GetActiveRotation());

  // Deactivate internal display to simulate Docked Mode.
  vector<display::ManagedDisplayInfo> secondary_only;
  secondary_only.push_back(GetDisplayInfoAt(1));
  display_manager()->OnNativeDisplaysChanged(secondary_only);

  const display::ManagedDisplayInfo& post_removal_info =
      display::test::DisplayManagerTestApi(display_manager())
          .GetInternalManagedDisplayInfo(internal_display_id);
  EXPECT_NE(info.GetActiveRotation(), post_removal_info.GetActiveRotation());
  EXPECT_EQ(display::Display::ROTATE_90, post_removal_info.GetActiveRotation());

  display_manager()->SetDisplayRotation(
      internal_display_id, display::Display::ROTATE_180,
      display::Display::RotationSource::ACTIVE);
  const display::ManagedDisplayInfo& post_rotation_info =
      display::test::DisplayManagerTestApi(display_manager())
          .GetInternalManagedDisplayInfo(internal_display_id);
  EXPECT_NE(info.GetActiveRotation(), post_rotation_info.GetActiveRotation());
  EXPECT_EQ(display::Display::ROTATE_180,
            post_rotation_info.GetActiveRotation());
}

namespace {

class CloseDisplayHandler : public ui::EventHandler {
 public:
  CloseDisplayHandler(AshTestBase* test_base, aura::Window* root)
      : test_base_(test_base), root_(root) {}
  CloseDisplayHandler(const CloseDisplayHandler&) = delete;
  CloseDisplayHandler& operator=(const CloseDisplayHandler&) = delete;
  ~CloseDisplayHandler() override = default;

  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    test_base_->UpdateDisplay("300x200");
    root_->RemovePreTargetHandler(this);
  }

 private:
  raw_ptr<AshTestBase> test_base_;
  raw_ptr<aura::Window> root_;
};

}  // namespace

// Make sure that we can emulate disconnecting an external display using
// Key/MouseEvent while it is in the unified desktop mode. This doesn't happen
// in real device as the disconnect is triggered by ozone, not by UI events, but
// this is still useful in the testing environment.
TEST_F(DisplayManagerTest, CloseDisplayByEvent) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("300x200, 600x400");
  EXPECT_EQ(2u, display_manager()->software_mirroring_display_list().size());

  auto* desktop_root = Shell::GetPrimaryRootWindow();
  CloseDisplayHandler handler(this, desktop_root);
  desktop_root->AddPreTargetHandler(&handler);

  auto* mirror_window_controller =
      Shell::Get()->window_tree_host_manager()->mirror_window_controller();
  auto* host_root = mirror_window_controller->GetAllRootWindows()[1].get();
  ui::test::EventGenerator generator(host_root);
  generator.PressAndReleaseKey(ui::VKEY_A);

  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
  EXPECT_EQ(0u, display_manager()->software_mirroring_display_list().size());
  // the root window the handler was added has already been destroyed.
  EXPECT_NE(desktop_root, Shell::GetPrimaryRootWindow());
}

TEST_F(DisplayManagerTest, ResolutionChangeInUnifiedMode) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("300x200, 600x400");

  int64_t unified_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::ManagedDisplayInfo info =
      display_manager()->GetDisplayInfo(unified_id);
  ASSERT_EQ(2u, info.display_modes().size());
  EXPECT_EQ(gfx::Size(600, 200), info.display_modes()[0].size());
  EXPECT_TRUE(info.display_modes()[0].native());
  EXPECT_EQ(gfx::Size(1200, 400), info.display_modes()[1].size());
  EXPECT_FALSE(info.display_modes()[1].native());
  EXPECT_EQ(gfx::Size(600, 200),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());
  display::ManagedDisplayMode active_mode;
  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(unified_id, &active_mode));
  EXPECT_EQ(gfx::Size(600, 200), active_mode.size());

  EXPECT_TRUE(display::test::SetDisplayResolution(display_manager(), unified_id,
                                                  gfx::Size(1200, 400)));
  EXPECT_EQ(gfx::Size(1200, 400),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(unified_id, &active_mode));
  EXPECT_EQ(gfx::Size(1200, 400), active_mode.size());

  // resolution change will not persist in unified desktop mode.
  UpdateDisplay("600x400, 300x200");
  EXPECT_EQ(gfx::Size(1200, 400),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());
  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(unified_id, &active_mode));
  EXPECT_TRUE(active_mode.native());
  EXPECT_EQ(gfx::Size(1200, 400), active_mode.size());
}

TEST_F(DisplayManagerTest, RotateExternalDisplayWithNonNativeMode) {
  const int64_t internal_display_id = 5;
  const int64_t external_id = 11;
  const display::ManagedDisplayInfo internal_display_info =
      display::ManagedDisplayInfo::CreateFromSpecWithID(
          "1920x1080#1280x720|640x480%60", internal_display_id);
  // Create an external display with a different origin to avoid triggering HW
  // mirroring.
  display::ManagedDisplayInfo external_display_info =
      display::ManagedDisplayInfo::CreateFromSpecWithID(
          "1+1-1280x720#1280x720|640x480%60", external_id);

  std::vector<display::ManagedDisplayInfo> display_info_list;

  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(internal_display_id,
            display::Screen::GetScreen()->GetPrimaryDisplay().id());

  display::ManagedDisplayMode active_mode;
  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(external_id, &active_mode));
  EXPECT_TRUE(active_mode.native());

  const auto& modes = external_display_info.display_modes();
  EXPECT_TRUE(display::test::SetDisplayResolution(
      display_manager(), external_id, modes[0].size()));
  display_manager()->UpdateDisplays();

  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(external_id, &active_mode));
  EXPECT_FALSE(active_mode.native());

  // Rotate the display.
  display_manager()->SetDisplayRotation(
      external_id, display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);

  // Refresh |external_display_info| since we have rotated the display.
  external_display_info = display_manager()->GetDisplayInfo(external_id);

  // Disconnect the external display.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(1U, display_manager()->num_connected_displays());

  // Reconnect the external display.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  EXPECT_EQ(2U, display_manager()->num_connected_displays());

  // Verify the display maintains the rotation.
  auto external_info = display_manager()->GetDisplayInfo(external_id);
  EXPECT_EQ(display::Display::ROTATE_90, external_info.GetActiveRotation());
}

TEST_F(DisplayManagerTest, UpdateMouseCursorAfterRotateZoom) {
  // Make sure just rotating will not change native location.
  UpdateDisplay("300x200,200x150");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  aura::Env* env = aura::Env::GetInstance();

  ui::test::EventGenerator generator1(root_windows[0]);
  ui::test::EventGenerator generator2(root_windows[1]);

  // Test on 1st display.
  generator1.MoveMouseToInHost(150, 50);
  EXPECT_EQ(gfx::Point(150, 50), env->last_mouse_location());
  UpdateDisplay("300x200/r,200x150");
  EXPECT_EQ(gfx::Point(50, 150), env->last_mouse_location());

  // Test on 2nd display.
  generator2.MoveMouseToInHost(50, 100);
  EXPECT_EQ(gfx::Point(250, 100), env->last_mouse_location());
  UpdateDisplay("300x200/r,200x150/l");
  EXPECT_EQ(gfx::Point(250, 50), env->last_mouse_location());

  // The native location is now outside, so move to the center
  // of closest display.
  UpdateDisplay("300x200/r,100x50/l");
  EXPECT_EQ(gfx::Point(225, 50), env->last_mouse_location());

  // Make sure just zooming will not change native location.
  UpdateDisplay("600x400*2,400x300");

  // Test on 1st display.
  generator1.MoveMouseToInHost(200, 300);
  EXPECT_EQ(gfx::Point(100, 150), env->last_mouse_location());
  UpdateDisplay("600x400*2@1.5,400x300");
  EXPECT_EQ(gfx::Point(66, 100), env->last_mouse_location());

  // Test on 2nd display.
  UpdateDisplay("600x400,400x300*2");
  generator2.MoveMouseToInHost(200, 250);
  EXPECT_EQ(gfx::Point(700, 125), env->last_mouse_location());
  UpdateDisplay("600x400,400x300*2@1.5");
  EXPECT_EQ(gfx::Point(666, 84), env->last_mouse_location());

  // The native location is now outside, so move to the
  // center of closest display.
  UpdateDisplay("600x400,400x200*2@1.5");
  EXPECT_EQ(gfx::Point(665, 66), env->last_mouse_location());
}

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() = default;

  TestDisplayObserver(const TestDisplayObserver&) = delete;
  TestDisplayObserver& operator=(const TestDisplayObserver&) = delete;

  ~TestDisplayObserver() override = default;

  // display::DisplayObserver overrides:
  void OnDisplayMetricsChanged(const display::Display&, uint32_t) override {}
  void OnDisplayAdded(const display::Display& new_display) override {
    // Mirror window should already be delete before restoring
    // the external display.
    EXPECT_TRUE(test_api.GetHosts().empty());
    changed_ = true;
  }
  void OnDisplaysRemoved(const display::Displays& removed_displays) override {
    // Mirror window should not be created until the external display
    // is removed.
    EXPECT_TRUE(test_api.GetHosts().empty());
    changed_ = true;
  }

  bool changed_and_reset() {
    bool changed = changed_;
    changed_ = false;
    return changed;
  }

 private:
  MirrorWindowTestApi test_api;
  bool changed_ = false;
};

TEST_F(DisplayManagerTest, SoftwareMirroring) {
  UpdateDisplay("300x400,400x500");

  MirrorWindowTestApi test_api;
  EXPECT_TRUE(test_api.GetHosts().empty());

  TestDisplayObserver display_observer;
  display::Screen::GetScreen()->AddObserver(&display_observer);

  display_manager()->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  display_manager()->UpdateDisplays();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 400),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  std::vector<aura::WindowTreeHost*> hosts = test_api.GetHosts();
  ASSERT_EQ(1U, hosts.size());
  EXPECT_EQ(gfx::Size(400, 500), hosts[0]->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Size(300, 400), hosts[0]->window()->bounds().size());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  SetSoftwareMirrorMode(false);
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_TRUE(test_api.GetHosts().empty());
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Make sure the mirror window has the pixel size of the
  // source display.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_observer.changed_and_reset());

  UpdateDisplay("300x400@0.5,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ(gfx::Size(300, 400),
            test_api.GetHosts()[0]->window()->bounds().size());

  UpdateDisplay("310x410*2,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ(gfx::Size(310, 410),
            test_api.GetHosts()[0]->window()->bounds().size());

  UpdateDisplay("320x420/r,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ(gfx::Size(420, 320),
            test_api.GetHosts()[0]->window()->bounds().size());

  UpdateDisplay("330x440/r,400x500");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ(gfx::Size(440, 330),
            test_api.GetHosts()[0]->window()->bounds().size());

  // Overscan insets are ignored.
  UpdateDisplay("400x600/o,600x800/o");
  EXPECT_FALSE(display_observer.changed_and_reset());
  EXPECT_EQ(gfx::Size(400, 600),
            test_api.GetHosts()[0]->window()->bounds().size());

  display::Screen::GetScreen()->RemoveObserver(&display_observer);
}

TEST_F(DisplayManagerTest, RotateInSoftwareMirroring) {
  UpdateDisplay("600x400,500x300");
  SetSoftwareMirrorMode(true);

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display_manager()->SetDisplayRotation(
      primary_id, display::Display::ROTATE_180,
      display::Display::RotationSource::ACTIVE);
  SetSoftwareMirrorMode(false);
}

TEST_F(DisplayManagerTest, InvertLayout) {
  EXPECT_EQ("left, 0",
            display::DisplayPlacement(display::DisplayPlacement::RIGHT, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("left, -100",
            display::DisplayPlacement(display::DisplayPlacement::RIGHT, 100)
                .Swap()
                .ToString());
  EXPECT_EQ("left, 50",
            display::DisplayPlacement(display::DisplayPlacement::RIGHT, -50)
                .Swap()
                .ToString());

  EXPECT_EQ("right, 0",
            display::DisplayPlacement(display::DisplayPlacement::LEFT, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("right, -90",
            display::DisplayPlacement(display::DisplayPlacement::LEFT, 90)
                .Swap()
                .ToString());
  EXPECT_EQ("right, 60",
            display::DisplayPlacement(display::DisplayPlacement::LEFT, -60)
                .Swap()
                .ToString());

  EXPECT_EQ("bottom, 0",
            display::DisplayPlacement(display::DisplayPlacement::TOP, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("bottom, -80",
            display::DisplayPlacement(display::DisplayPlacement::TOP, 80)
                .Swap()
                .ToString());
  EXPECT_EQ("bottom, 70",
            display::DisplayPlacement(display::DisplayPlacement::TOP, -70)
                .Swap()
                .ToString());

  EXPECT_EQ("top, 0",
            display::DisplayPlacement(display::DisplayPlacement::BOTTOM, 0)
                .Swap()
                .ToString());
  EXPECT_EQ("top, -70",
            display::DisplayPlacement(display::DisplayPlacement::BOTTOM, 70)
                .Swap()
                .ToString());
  EXPECT_EQ("top, 80",
            display::DisplayPlacement(display::DisplayPlacement::BOTTOM, -80)
                .Swap()
                .ToString());
}

TEST_F(DisplayManagerTest, NotifyPrimaryChangeSwapped) {
  UpdateDisplay("500x400,500x400");
  int64_t old_primary_id = GetPrimaryDisplay().id();
  int64_t new_primary_id = GetSecondaryDisplay().id();
  SwapPrimaryDisplay();

  // Old primary display.
  EXPECT_TRUE(changed_metrics(old_primary_id) &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics(old_primary_id) &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_FALSE(changed_metrics(old_primary_id) &
               display::DisplayObserver::DISPLAY_METRIC_PRIMARY);

  // New primary display.
  EXPECT_TRUE(changed_metrics(new_primary_id) &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics(new_primary_id) &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_TRUE(changed_metrics(new_primary_id) &
              display::DisplayObserver::DISPLAY_METRIC_PRIMARY);
}

TEST_F(DisplayManagerTest, NotifyPrimaryChangeDock) {
  UpdateDisplay("500x400,500x400");
  SwapPrimaryDisplay();
  reset();
  UpdateDisplay("500x400");
  EXPECT_FALSE(changed_metrics() &
               display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_FALSE(changed_metrics() &
               display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_PRIMARY);

  UpdateDisplay("500x400,500x400");
  SwapPrimaryDisplay();
  UpdateDisplay("500x400");
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_PRIMARY);
}

TEST_F(DisplayManagerTest, NotifyPrimaryChangeUndock) {
  // Assume the default display is an external display, and
  // emulates undocking by switching to another display.
  display::ManagedDisplayInfo another_display_info =
      CreateDisplayInfo(1, gfx::Rect(0, 0, 1280, 800));
  std::vector<display::ManagedDisplayInfo> info_list;
  info_list.push_back(another_display_info);
  reset();
  display_manager()->OnNativeDisplaysChanged(info_list);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_WORK_AREA);
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_PRIMARY);
}

TEST_F(DisplayManagerTest, UpdateDisplayWithHostOrigin) {
  UpdateDisplay("100x200,300x400");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  aura::WindowTreeHost* host0 = root_windows[0]->GetHost();
  aura::WindowTreeHost* host1 = root_windows[1]->GetHost();

  EXPECT_EQ(gfx::Point(1, 1), host0->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(100, 200), host0->GetBoundsInPixels().size());
  // UpdateDisplay set the origin if it's not set.
  EXPECT_NE(gfx::Point(1, 1), host1->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(300, 400), host1->GetBoundsInPixels().size());

  UpdateDisplay("100x200,200+300-300x400");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(gfx::Point(0, 0), host0->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(100, 200), host0->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Point(200, 300), host1->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(300, 400), host1->GetBoundsInPixels().size());

  UpdateDisplay("400+500-200x300,300x400");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(gfx::Point(400, 500), host0->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(200, 300), host0->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Point(0, 0), host1->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(300, 400), host1->GetBoundsInPixels().size());

  UpdateDisplay("100+200-100x200,300+500-200x300");
  ASSERT_EQ(2, display::Screen::GetScreen()->GetNumDisplays());
  EXPECT_EQ(gfx::Point(100, 200), host0->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(100, 200), host0->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Point(300, 500), host1->GetBoundsInPixels().origin());
  EXPECT_EQ(gfx::Size(200, 300), host1->GetBoundsInPixels().size());
}

TEST_F(DisplayManagerTest, UnifiedDesktopBasic) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");

  // Enable after extended mode.
  display_manager()->SetUnifiedDesktopEnabled(true);

  // Defaults to the unified desktop.
  display::Screen* screen = display::Screen::GetScreen();
  // The 2nd display is scaled so that it has the same height as 1st display.
  // 300 * 500 / 200  + 400 = 1150.
  EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());

  SetSoftwareMirrorMode(true);
  EXPECT_EQ(gfx::Size(400, 500), screen->GetPrimaryDisplay().size());

  SetSoftwareMirrorMode(false);
  EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());

  // Switch to single desktop.
  UpdateDisplay("500x300");
  EXPECT_EQ(gfx::Size(500, 300), screen->GetPrimaryDisplay().size());

  // Switch to unified desktop.
  UpdateDisplay("500x300,400x500");
  // 400 * 300 / 500 + 500 ~= 739.
  EXPECT_EQ(gfx::Size(739, 300), screen->GetPrimaryDisplay().size());

  // The default should fit to the internal display.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(CreateDisplayInfo(10, gfx::Rect(0, 0, 500, 300)));
  display_info_list.push_back(
      CreateDisplayInfo(11, gfx::Rect(500, 0, 400, 500)));
  {
    display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                           11);
    display_manager()->OnNativeDisplaysChanged(display_info_list);
    // 500 * 500 / 300 + 400 ~= 1233.
    EXPECT_EQ(gfx::Size(1233, 500), screen->GetPrimaryDisplay().size());
  }

  // Switch to 3 displays.
  UpdateDisplay("500x300,400x500,500x300");
  EXPECT_EQ(gfx::Size(1239, 300), screen->GetPrimaryDisplay().size());

  // Switch back to extended desktop.
  display_manager()->SetUnifiedDesktopEnabled(false);
  EXPECT_EQ(gfx::Size(500, 300), screen->GetPrimaryDisplay().size());
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  EXPECT_EQ(gfx::Size(400, 500),
            display_manager_test.GetSecondaryDisplay().size());
  EXPECT_EQ(gfx::Size(500, 300),
            display_manager()
                ->GetDisplayForId(display::SynthesizeDisplayIdFromSeed(
                    display_manager_test.GetSecondaryDisplay().id()))
                .size());
}

TEST_F(DisplayManagerTest, UnifiedDesktopWithHardwareMirroring) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  // Enter to hardware mirroring.
  display::ManagedDisplayInfo d1 =
      CreateDisplayInfo(1, gfx::Rect(0, 0, 500, 400));
  display::ManagedDisplayInfo d2 =
      CreateDisplayInfo(2, gfx::Rect(0, 0, 500, 400));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(d1);
  display_info_list.push_back(d2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  ASSERT_TRUE(display_manager()->IsInHardwareMirrorMode());
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(display_manager()->IsInHardwareMirrorMode());

  // The display manager automatically switches to software mirroring if
  // hardware mirroring is no longer available, because previous mirror mode
  // enforces current display mode to be mirror mode.
  display::DisplayIdList list = display::test::CreateDisplayIdList2(1, 2);
  display::DisplayLayoutBuilder builder(
      display_manager()->layout_store()->GetRegisteredDisplayLayout(list));
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  d2.SetBounds(gfx::Rect(0, 500, 500, 400));
  display_info_list.clear();
  display_info_list.push_back(d1);
  display_info_list.push_back(d2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Exit software mirroring and enter unified desktop mode after mirror mode is
  // turned off.
  SetSoftwareMirrorMode(false);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());
}

TEST_F(DisplayManagerTest, UnifiedDesktopEnabledWithExtended) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");
  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  display::DisplayLayoutBuilder builder(
      display_manager()->layout_store()->GetRegisteredDisplayLayout(list));
  builder.SetDefaultUnified(false);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
}

TEST_F(DisplayManagerTest, UnifiedDesktopWith2xDSF) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  display_manager()->SetUnifiedDesktopEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  // 2nd display is 2x.
  UpdateDisplay("400x500,1000x800*2");
  display::ManagedDisplayInfo info =
      display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  ASSERT_EQ(2u, info.display_modes().size());
  EXPECT_EQ(gfx::Size(1640, 800), info.display_modes()[0].size());
  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(gfx::Size(1025, 500), info.display_modes()[1].size());
  EXPECT_EQ(1.0f, info.display_modes()[1].device_scale_factor());

  // For 1x, 400 + 500 / 800 * 100 = 1025.
  EXPECT_EQ(gfx::Size(1025, 500), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(1025, 500),
            Shell::GetPrimaryRootWindow()->bounds().size());
  accelerators::ZoomDisplay(false);
  // (800 / 500 * 400 + 500) /2 = 820
  EXPECT_EQ(gfx::Size(820, 400), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(820, 400),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // 1st display is 2x.
  UpdateDisplay("1200x800*2,1100x1000");
  info = display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  ASSERT_EQ(2u, info.display_modes().size());
  EXPECT_EQ(gfx::Size(2080, 800), info.display_modes()[0].size());
  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(gfx::Size(2600, 1000), info.display_modes()[1].size());
  EXPECT_EQ(1.0f, info.display_modes()[1].device_scale_factor());

  // For 2x, (800 / 1000 * 1100 + 1200) / 2 = 1040
  EXPECT_EQ(gfx::Size(1040, 400), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(1040, 400),
            Shell::GetPrimaryRootWindow()->bounds().size());
  accelerators::ZoomDisplay(true);
  // 1000 / 800 * 1200 + 1100 = 2600
  EXPECT_EQ(gfx::Size(2600, 1000), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(2600, 1000),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // Both displays are 2x.
  // 1st display is 2x.
  UpdateDisplay("1200x800*2,1100x1000*2");
  info = display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  ASSERT_EQ(2u, info.display_modes().size());
  EXPECT_EQ(gfx::Size(2080, 800), info.display_modes()[0].size());
  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(gfx::Size(2600, 1000), info.display_modes()[1].size());
  EXPECT_EQ(2.0f, info.display_modes()[1].device_scale_factor());

  EXPECT_EQ(gfx::Size(1040, 400), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(1040, 400),
            Shell::GetPrimaryRootWindow()->bounds().size());
  accelerators::ZoomDisplay(true);
  EXPECT_EQ(gfx::Size(1300, 500), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(1300, 500),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // Both displays have the same physical height, with the first display
  // being 2x.
  UpdateDisplay("1000x800*2,300x800");
  info = display_manager()->GetDisplayInfo(screen->GetPrimaryDisplay().id());
  ASSERT_EQ(2u, info.display_modes().size());
  EXPECT_EQ(gfx::Size(1300, 800), info.display_modes()[0].size());
  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(gfx::Size(1300, 800), info.display_modes()[1].size());
  EXPECT_EQ(1.0f, info.display_modes()[1].device_scale_factor());

  EXPECT_EQ(gfx::Size(650, 400), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(650, 400),
            Shell::GetPrimaryRootWindow()->bounds().size());
  accelerators::ZoomDisplay(true);
  EXPECT_EQ(gfx::Size(1300, 800), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(1300, 800),
            Shell::GetPrimaryRootWindow()->bounds().size());

  // Both displays have the same physical height, with the second display
  // being 2x.
  UpdateDisplay("1000x800,300x800*2");
  ASSERT_EQ(2u, info.display_modes().size());
  EXPECT_EQ(gfx::Size(1300, 800), info.display_modes()[0].size());
  EXPECT_EQ(2.0f, info.display_modes()[0].device_scale_factor());
  EXPECT_EQ(gfx::Size(1300, 800), info.display_modes()[1].size());
  EXPECT_EQ(1.0f, info.display_modes()[1].device_scale_factor());

  EXPECT_EQ(gfx::Size(1300, 800), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(1300, 800),
            Shell::GetPrimaryRootWindow()->bounds().size());
  accelerators::ZoomDisplay(false);
  EXPECT_EQ(gfx::Size(650, 400), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(gfx::Size(650, 400),
            Shell::GetPrimaryRootWindow()->bounds().size());
}

// Updating displays again in unified desktop mode should not crash.
// crbug.com/491094.
TEST_F(DisplayManagerTest, ConfigureUnifiedTwice) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("300x200,400x500");
  // Mirror windows are created in a posted task.
  base::RunLoop().RunUntilIdle();

  UpdateDisplay("300x250,400x550");
  base::RunLoop().RunUntilIdle();
}

TEST_F(DisplayManagerTest, NoRotateUnifiedDesktop) {
  display_manager()->SetUnifiedDesktopEnabled(true);

  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  EXPECT_EQ(gfx::Size(1150, 500), display.size());
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(display::Display::ROTATE_0,
            screen->GetPrimaryDisplay().panel_rotation());
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_0,
      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());
  EXPECT_EQ(display::Display::ROTATE_0,
            screen->GetPrimaryDisplay().panel_rotation());

  UpdateDisplay("400x500");
  EXPECT_EQ(gfx::Size(400, 500), screen->GetPrimaryDisplay().size());
}

// Validate that setting an invalid matrix will fall back to the default
// horizontal unified desktop layout.
TEST_F(DisplayManagerTest, UnifiedDesktopInvalidMatrices) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");
  display_manager()->SetUnifiedDesktopEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(2u, list.size());
  {
    // Create an empty matrix.
    display::UnifiedDesktopLayoutMatrix matrix;
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // The result is still a valid default horizontal layout.
    EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());

    // 2 x 1 empty matrix.
    matrix.resize(2u);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // The result is still a valid default horizontal layout.
    EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());
  }

  {
    // 2 x 1 vertical matrix with invalid IDs.
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(2u);
    matrix[0].emplace_back(list[0]);
    matrix[1].emplace_back(-100);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // The result is still a valid default horizontal layout.
    EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());
  }

  {
    // Matrix with a missing ID.
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(2u);
    matrix[0].emplace_back(list[0]);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // The result is still a valid default horizontal layout.
    EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());
  }

  // Switch to 3 displays.
  UpdateDisplay("500x300,400x500,500x300");
  list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());
  {
    // Create a matrix with unequal rows
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(3u);
    matrix[0].emplace_back(list[0]);
    matrix[1].emplace_back(list[1]);
    matrix[1].emplace_back(list[2]);  // Typo; meant to say matrix[2].
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // The result is still a valid default horizontal layout.
    EXPECT_EQ(gfx::Size(1239, 300), screen->GetPrimaryDisplay().size());
  }

  {
    // Create a matrix with repeated IDs.
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(3u);
    matrix[0].emplace_back(list[0]);
    matrix[1].emplace_back(list[1]);
    matrix[2].emplace_back(list[1]);  // Typo; meant to say list[2].
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // The result is still a valid default horizontal layout.
    EXPECT_EQ(gfx::Size(1239, 300), screen->GetPrimaryDisplay().size());
  }
}

TEST_F(DisplayManagerTest, UnifiedDesktopVerticalLayout2x1) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x500,300x200");
  display_manager()->SetUnifiedDesktopEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();
  // This is still a horizontal layout.
  EXPECT_EQ(gfx::Size(1150, 500), screen->GetPrimaryDisplay().size());

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(2u, list.size());
  {
    // Create a 2 x 1 vertical layout matrix and set it.
    // [400 x 500]
    // [300 x 200]
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(2u);
    matrix[0].emplace_back(list[0]);
    matrix[1].emplace_back(list[1]);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // 500 + 400 * 200 / 300 ~= 766.
    EXPECT_EQ(gfx::Size(400, 766), screen->GetPrimaryDisplay().size());
    // Default shelf alignment is bottom. Display in bottom-left cell is
    // considered the primary mirroring display.
    EXPECT_EQ(list[1], Shell::Get()
                           ->display_configuration_controller()
                           ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                           .id());

    // Validate display rows and max heights.
    EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[0]));
    EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[1]));
    EXPECT_EQ(500, display_manager()->GetUnifiedDesktopRowMaxHeight(0));
    EXPECT_EQ(400 * 200 / 300,
              display_manager()->GetUnifiedDesktopRowMaxHeight(1));
    EXPECT_FALSE(OverlappingMirroringDisplaysExist());
  }

  {
    // Change the order of the displays such that the [300 x 200] is on top,
    // which should make its bounds used for the default mode.
    // [300 x 200]
    // [400 x 500]
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(2u);
    matrix[0].emplace_back(list[1]);
    matrix[1].emplace_back(list[0]);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    // 200 + 300 * 500 / 400 ~= 574 (Note that we actually scale the max unified
    // bounds).
    EXPECT_EQ(gfx::Size(300, 574), screen->GetPrimaryDisplay().size());
    // Display in bottom-left cell is considered primary.
    EXPECT_EQ(list[0], Shell::Get()
                           ->display_configuration_controller()
                           ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                           .id());

    // Validate display rows and max heights.
    EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[0]));
    EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[1]));
    EXPECT_EQ(199, display_manager()->GetUnifiedDesktopRowMaxHeight(0));
    // 300 * 500 / 400.
    EXPECT_EQ(375, display_manager()->GetUnifiedDesktopRowMaxHeight(1));
    EXPECT_FALSE(OverlappingMirroringDisplaysExist());
  }

  {
    // Revert to the first matrix, but mark the [300 x 200] display as internal.
    // [400 x 500]
    // [300 x 200] : Internal
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(2u);
    matrix[0].emplace_back(list[0]);
    matrix[1].emplace_back(list[1]);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    std::vector<display::ManagedDisplayInfo> display_info_list;
    display_info_list.emplace_back(
        CreateDisplayInfo(list[0], gfx::Rect(0, 0, 400, 500)));
    display_info_list.emplace_back(
        CreateDisplayInfo(list[1], gfx::Rect(400, 0, 300, 200)));
    display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                           list[1]);
    display_manager()->OnNativeDisplaysChanged(display_info_list);
    // Run loop to create mirroring displays.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(gfx::Size(300, 574), screen->GetPrimaryDisplay().size());
    // Display in bottom-left cell is considered primary.
    EXPECT_EQ(list[1], Shell::Get()
                           ->display_configuration_controller()
                           ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                           .id());

    // Validate display rows and max heights.
    EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[0]));
    EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[1]));
    // 300 * 500 / 400.
    EXPECT_EQ(375, display_manager()->GetUnifiedDesktopRowMaxHeight(0));
    EXPECT_EQ(199, display_manager()->GetUnifiedDesktopRowMaxHeight(1));
    EXPECT_FALSE(OverlappingMirroringDisplaysExist());
  }
}

TEST_F(DisplayManagerTest, UnifiedDesktopVerticalLayout3x1) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("500x300,400x500,500x300");
  display_manager()->SetUnifiedDesktopEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(3u, list.size());
  {
    // Create a 3 x 1 vertical layout matrix and set it.
    // [500 x 300]
    // [400 x 500]
    // [500 x 300]
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(3u);
    matrix[0].emplace_back(list[0]);
    matrix[1].emplace_back(list[1]);
    matrix[2].emplace_back(list[2]);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    EXPECT_EQ(gfx::Size(500, 1225), screen->GetPrimaryDisplay().size());
    // Display in bottom-left cell is considered primary.
    EXPECT_EQ(list[2], Shell::Get()
                           ->display_configuration_controller()
                           ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                           .id());

    // Validate display rows and max heights.
    EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[0]));
    EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[1]));
    EXPECT_EQ(2, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[2]));
    EXPECT_EQ(300, display_manager()->GetUnifiedDesktopRowMaxHeight(0));
    // 500 * 500 / 400 = 625.
    EXPECT_EQ(625, display_manager()->GetUnifiedDesktopRowMaxHeight(1));
    EXPECT_EQ(300, display_manager()->GetUnifiedDesktopRowMaxHeight(2));
    EXPECT_FALSE(OverlappingMirroringDisplaysExist());
  }

  {
    // We can change the order however we want.
    // [400 x 500]
    // [500 x 300]
    // [500 x 300]
    display::UnifiedDesktopLayoutMatrix matrix;
    matrix.resize(3u);
    matrix[0].emplace_back(list[1]);
    matrix[1].emplace_back(list[0]);
    matrix[2].emplace_back(list[2]);
    display_manager()->SetUnifiedDesktopMatrix(matrix);
    EXPECT_EQ(gfx::Size(400, 980), screen->GetPrimaryDisplay().size());
    // Display in bottom-left cell is considered primary.
    EXPECT_EQ(list[2], Shell::Get()
                           ->display_configuration_controller()
                           ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                           .id());

    // Validate display rows and max heights.
    EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[0]));
    EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[1]));
    EXPECT_EQ(2, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                     list[2]));
    EXPECT_EQ(500, display_manager()->GetUnifiedDesktopRowMaxHeight(0));
    // 400 * 300 / 500 = 240.
    EXPECT_EQ(240, display_manager()->GetUnifiedDesktopRowMaxHeight(1));
    EXPECT_EQ(240, display_manager()->GetUnifiedDesktopRowMaxHeight(2));
    EXPECT_FALSE(OverlappingMirroringDisplaysExist());
  }
}

TEST_F(DisplayManagerTest, UnifiedDesktopGridLayout2x2) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("500x300,400x500,300x600,200x300");
  display_manager()->SetUnifiedDesktopEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(4u, list.size());
  // Create a 2 x 2 vertical layout matrix and set it.
  // [500 x 300] [400 x 500]
  // [300 x 600] [200 x 300]
  display::UnifiedDesktopLayoutMatrix matrix;
  matrix.resize(2u);
  matrix[0].emplace_back(list[0]);
  matrix[0].emplace_back(list[1]);
  matrix[1].emplace_back(list[2]);
  matrix[1].emplace_back(list[3]);
  display_manager()->SetUnifiedDesktopMatrix(matrix);
  EXPECT_EQ(gfx::Size(739, 933), screen->GetPrimaryDisplay().size());

  // Default shelf alignment is bottom.
  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  EXPECT_EQ(shelf->alignment(), ShelfAlignment::kBottom);

  // Display in bottom-left cell is considered the primary mirroring display.
  EXPECT_EQ(list[2], Shell::Get()
                         ->display_configuration_controller()
                         ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                         .id());

  // Validate display rows and max heights.
  EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[0]));
  EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[1]));
  EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[2]));
  EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[3]));
  EXPECT_EQ(300, display_manager()->GetUnifiedDesktopRowMaxHeight(0));
  EXPECT_EQ(633, display_manager()->GetUnifiedDesktopRowMaxHeight(1));
  EXPECT_FALSE(OverlappingMirroringDisplaysExist());

  // Change the shelf alignment to left, and expect that the primary mirroring
  // display in the top-left display in the matrix.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(list[0], Shell::Get()
                         ->display_configuration_controller()
                         ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                         .id());

  // Change the shelf alignment to right, and expect that the primary mirroring
  // display in the top-right display in the matrix.
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(list[1], Shell::Get()
                         ->display_configuration_controller()
                         ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                         .id());
}

TEST_F(DisplayManagerTest, UnifiedDesktopGridLayout3x2) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("500x300,400x500,300x600,200x300,700x200,350x480");
  display_manager()->SetUnifiedDesktopEnabled(true);
  display::Screen* screen = display::Screen::GetScreen();

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(6u, list.size());
  // Create a 3 x 2 vertical layout matrix and set it.
  // [500 x 300] [400 x 500]
  // [300 x 600] [200 x 300]
  // [700 x 200] [350 x 480]
  display::UnifiedDesktopLayoutMatrix matrix;
  matrix.resize(3u);
  matrix[0].emplace_back(list[0]);
  matrix[0].emplace_back(list[1]);
  matrix[1].emplace_back(list[2]);
  matrix[1].emplace_back(list[3]);
  matrix[2].emplace_back(list[4]);
  matrix[2].emplace_back(list[5]);
  display_manager()->SetUnifiedDesktopMatrix(matrix);
  EXPECT_EQ(gfx::Size(739, 1108), screen->GetPrimaryDisplay().size());

  // Default shelf alignment is bottom.
  Shelf* shelf = Shell::GetPrimaryRootWindowController()->shelf();
  EXPECT_EQ(shelf->alignment(), ShelfAlignment::kBottom);

  // Display in bottom-left cell is considered the primary mirroring display.
  EXPECT_EQ(list[4], Shell::Get()
                         ->display_configuration_controller()
                         ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                         .id());

  // Validate display rows and max heights.
  EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[0]));
  EXPECT_EQ(0, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[1]));
  EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[2]));
  EXPECT_EQ(1, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[3]));
  EXPECT_EQ(2, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[4]));
  EXPECT_EQ(2, display_manager()->GetMirroringDisplayRowIndexInUnifiedMatrix(
                   list[5]));
  EXPECT_EQ(300, display_manager()->GetUnifiedDesktopRowMaxHeight(0));
  EXPECT_EQ(633, display_manager()->GetUnifiedDesktopRowMaxHeight(1));
  EXPECT_EQ(175, display_manager()->GetUnifiedDesktopRowMaxHeight(2));
  EXPECT_FALSE(OverlappingMirroringDisplaysExist());

  // Change the shelf alignment to left, and expect that the primary mirroring
  // display in the top-left display in the matrix.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_EQ(list[0], Shell::Get()
                         ->display_configuration_controller()
                         ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                         .id());

  // Change the shelf alignment to right, and expect that the primary mirroring
  // display in the top-right display in the matrix.
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_EQ(list[1], Shell::Get()
                         ->display_configuration_controller()
                         ->GetPrimaryMirroringDisplayForUnifiedDesktop()
                         .id());
}

TEST_F(DisplayManagerTest, UnifiedDesktopTabletMode) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("400x300,800x700");
  base::RunLoop().RunUntilIdle();

  // Set the first display as internal display so that the tablet mode can be
  // enabled.
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  display_manager()->SetUnifiedDesktopEnabled(true);
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Turn on tablet mode, expect that we switch to mirror mode without any
  // crashes.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());

  // The Home Launcher should be created and shown, not dismissed as a result of
  // the destruction of the Unified host when we switched to mirror mode
  // asynchronously.
  auto* app_list_controller = Shell::Get()->app_list_controller();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_TRUE(
      app_list_controller->IsVisible(display_manager()->first_display_id()));

  // Exiting tablet mode should exit mirror mode and return back to Unified
  // mode.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Home Launcher should be dismissed.
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_FALSE(
      app_list_controller->IsVisible(display_manager()->first_display_id()));
}

TEST_F(DisplayManagerTest, UnifiedDesktopPrimarySizeWithRotatedDisplays) {
  MirrorWindowTestApi test_api;

  // RootWidow for primary changes during unified desktop transition.
  disable_check_root_window_on_destruction();

  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  display_manager()->SetUnifiedDesktopEnabled(true);

  UpdateDisplay("1000x700/r");
  EXPECT_EQ(gfx::Size(700, 1000),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  UpdateDisplay("1000x700/r,1000x700/r");
  EXPECT_EQ(gfx::Size(1400, 1000),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  std::vector<aura::WindowTreeHost*> host_list = test_api.GetHosts();
  std::vector<display::Display> display_list =
      display_manager()->software_mirroring_display_list();
  EXPECT_EQ(gfx::Size(700, 1000), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
            host_list[0]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_90, display_list[0].panel_rotation());
  EXPECT_EQ(gfx::Size(700, 1000), host_list[1]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
            host_list[1]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_90, display_list[1].panel_rotation());

  // Use custom display offset to ensure rotation is properly updated.
  UpdateDisplay("1000x700/r,1200+100-1000x700/l");
  EXPECT_EQ(gfx::Size(1400, 1000),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  host_list = test_api.GetHosts();
  display_list = display_manager()->software_mirroring_display_list();
  EXPECT_EQ(gfx::Size(700, 1000), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
            host_list[0]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_90, display_list[0].panel_rotation());
  EXPECT_EQ(gfx::Size(700, 1000), host_list[1]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
            host_list[1]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_270, display_list[1].panel_rotation());

  UpdateDisplay("1000x700/r,1000x700");
  // width = 1000 / 700 * 1000 + 700 ~= 2128
  EXPECT_EQ(gfx::Size(2128, 1000),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  host_list = test_api.GetHosts();
  display_list = display_manager()->software_mirroring_display_list();
  EXPECT_EQ(gfx::Size(700, 1000), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
            host_list[0]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_90, display_list[0].panel_rotation());
  EXPECT_EQ(gfx::Size(1000, 700), host_list[1]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_NONE,
            host_list[1]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_0, display_list[1].panel_rotation());

  // Three displays
  UpdateDisplay("1000x700/l,1000x700/r,1000x700/l");
  EXPECT_EQ(gfx::Size(2100, 1000),
            display::Screen::GetScreen()->GetPrimaryDisplay().size());

  host_list = test_api.GetHosts();
  display_list = display_manager()->software_mirroring_display_list();
  EXPECT_EQ(gfx::Size(700, 1000), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
            host_list[0]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_270, display_list[0].panel_rotation());
  EXPECT_EQ(gfx::Size(700, 1000), host_list[1]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_90,
            host_list[1]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_90, display_list[1].panel_rotation());
  EXPECT_EQ(gfx::Size(700, 1000), host_list[2]->window()->bounds().size());
  EXPECT_EQ(gfx::OVERLAY_TRANSFORM_ROTATE_CLOCKWISE_270,
            host_list[2]->compositor()->display_transform_hint());
  EXPECT_EQ(display::Display::ROTATE_270, display_list[2].panel_rotation());
}

TEST_F(DisplayManagerTest, DisplayPrefsAndForcedMirrorMode) {
  UpdateDisplay("400x300,800x700");
  base::RunLoop().RunUntilIdle();

  // Set the first display as internal display so that the tablet mode can be
  // enabled.
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Initially we can save display prefs ...
  EXPECT_TRUE(Shell::Get()->ShouldSaveDisplaySettings());
  // ... and there are no external displays that are candidates for mirror
  // restore.
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());

  // Turn on tablet mode, and expect that it can persist certain
  // display prefs while forced mirror mode is active.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_TRUE(
      display_manager()->layout_store()->forced_mirror_mode_for_tablet());
  EXPECT_TRUE(Shell::Get()->ShouldSaveDisplaySettings());
  // Forced mirror mode does not add external displays as candidates for mirror
  // restore.
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());

  // Exit tablet mode and expect everything is back to normal.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_FALSE(
      display_manager()->layout_store()->forced_mirror_mode_for_tablet());
  EXPECT_TRUE(Shell::Get()->ShouldSaveDisplaySettings());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());
}

TEST_F(DisplayManagerTest, ForcedMirrorModeExited) {
  UpdateDisplay("400x300,800x700");
  base::RunLoop().RunUntilIdle();

  // Set the first display as internal display so that the tablet mode can be
  // enabled.
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Initially we can save display prefs ...
  EXPECT_TRUE(Shell::Get()->ShouldSaveDisplaySettings());
  // ... and there are no external displays that are candidates for mirror
  // restore.
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());

  // Turn on tablet mode, and expect that it can persist certain
  // display prefs while forced mirror mode is active.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_TRUE(
      display_manager()->layout_store()->forced_mirror_mode_for_tablet());
  EXPECT_TRUE(Shell::Get()->ShouldSaveDisplaySettings());
  // Forced mirror mode does not add external displays as candidates for mirror
  // restore.
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());

  // Exit mirror mode, and expect that `forced_mirror_mode_for_tablet` is now
  // false.
  SetSoftwareMirrorMode(false);
  EXPECT_FALSE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_FALSE(
      display_manager()->layout_store()->forced_mirror_mode_for_tablet());

  // Randomly change the external monitor's resolution/refresh rate, and
  // expect that the setting is retained.
  const display::ManagedDisplayInfo& info_1 = GetDisplayInfoAt(0);
  const display::ManagedDisplayInfo::ManagedDisplayModeList& modes =
      info_1.display_modes();
  display::test::SetDisplayResolution(display_manager(), info_1.id(),
                                      modes[0].size());
  display_manager()->UpdateDisplays();
  EXPECT_EQ(
      display_manager()->GetDisplayForId(info_1.id()).device_scale_factor(),
      1.f);
  EXPECT_FALSE(display_manager()->IsInSoftwareMirrorMode());
}

TEST_F(DisplayManagerTest, DisplayPrefsAndKioskMode) {
  // Login in as kiosk app.
  UserSession session;
  session.session_id = 1u;
  session.user_info.type = user_manager::UserType::kKioskApp;
  session.user_info.account_id = AccountId::FromUserEmail("user1@test.com");
  session.user_info.display_name = "User 1";
  session.user_info.display_email = "user1@test.com";
  Shell::Get()->session_controller()->UpdateUserSession(std::move(session));
  EXPECT_EQ(LoginStatus::KIOSK_APP,
            Shell::Get()->session_controller()->login_status());
  UpdateDisplay("400x300,800x700");
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(Shell::Get()->ShouldSaveDisplaySettings());
}

TEST_F(DisplayManagerTest, DockMode) {
  const int64_t internal_id = 1;
  const int64_t external_id = 2;

  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_id, gfx::Rect(0, 0, 500, 400));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(external_id, gfx::Rect(1, 1, 200, 100));
  std::vector<display::ManagedDisplayInfo> display_info_list;

  // software mirroring.
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  EXPECT_EQ(internal_id, internal_display_id);

  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->active_display_list().size());

  EXPECT_TRUE(display_manager()->IsActiveDisplayId(external_id));
  EXPECT_FALSE(display_manager()->IsActiveDisplayId(internal_id));
}

// Make sure that bad layout information is ignored and does not crash.
TEST_F(DisplayManagerTest, DontRegisterBadConfig) {
  display::DisplayIdList list = display::test::CreateDisplayIdList2(1, 2);
  display::DisplayLayoutBuilder builder(1);
  builder.AddDisplayPlacement(2, 1, display::DisplayPlacement::LEFT, 0);
  builder.AddDisplayPlacement(3, 1, display::DisplayPlacement::BOTTOM, 0);

  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());
}

class ScreenShutdownTest : public AshTestBase {
 public:
  ScreenShutdownTest() = default;

  ScreenShutdownTest(const ScreenShutdownTest&) = delete;
  ScreenShutdownTest& operator=(const ScreenShutdownTest&) = delete;

  ~ScreenShutdownTest() override = default;

  void TearDown() override {
    display::Screen* orig_screen = display::Screen::GetScreen();
    AshTestBase::TearDown();
    display::Screen* screen = display::Screen::GetScreen();
    EXPECT_NE(orig_screen, screen);
    EXPECT_EQ(2, screen->GetNumDisplays());
    EXPECT_EQ(gfx::Size(500, 300), screen->GetPrimaryDisplay().size());
    std::vector<display::Display> all = screen->GetAllDisplays();
    EXPECT_EQ(gfx::Size(500, 300), all[0].size());
    EXPECT_EQ(gfx::Size(800, 400), all[1].size());
  }
};

TEST_F(ScreenShutdownTest, ScreenAfterShutdown) {
  UpdateDisplay("500x300,800x400");
}

namespace {

// A helper class that sets the display configuration and starts ash.
// This is to make sure the font configuration happens during ash
// initialization process.
class FontTestHelper : public AshTestBase {
 public:
  enum DisplayType { INTERNAL, EXTERNAL };

  FontTestHelper(float scale, DisplayType display_type) {
    gfx::ClearFontRenderParamsCacheForTest();
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (display_type == INTERNAL)
      command_line->AppendSwitch(::switches::kUseFirstDisplayAsInternal);
    command_line->AppendSwitchASCII(::switches::kHostWindowBounds,
                                    StringPrintf("1000x800*%f", scale));
    SetUp();
  }

  FontTestHelper(const FontTestHelper&) = delete;
  FontTestHelper& operator=(const FontTestHelper&) = delete;

  ~FontTestHelper() override { TearDown(); }

  // AshTestBase:
  void TestBody() override { NOTREACHED(); }
};

bool IsTextSubpixelPositioningEnabled() {
  gfx::FontRenderParams params =
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr);
  return params.subpixel_positioning;
}

gfx::FontRenderParams::Hinting GetFontHintingParams() {
  gfx::FontRenderParams params =
      gfx::GetFontRenderParams(gfx::FontRenderParamsQuery(), nullptr);
  return params.hinting;
}

}  // namespace

using DisplayManagerFontTest = testing::Test;

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf100Internal) {
  FontTestHelper helper(1.0f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf200Internal) {
  FontTestHelper helper(2.0f, FontTestHelper::INTERNAL);
  ASSERT_DOUBLE_EQ(
      2.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());

  helper.display_manager()->UpdateZoomFactor(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(), 0.5f);

  ASSERT_DOUBLE_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf100External) {
  FontTestHelper helper(1.0f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      1.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf125External) {
  FontTestHelper helper(1.25f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      1.25f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
  EXPECT_EQ(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest, TextSubpixelPositioningWithDsf200External) {
  FontTestHelper helper(2.0f, FontTestHelper::EXTERNAL);
  ASSERT_DOUBLE_EQ(
      2.0f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerFontTest,
       TextSubpixelPositioningWithDsf125InternalWithScaling) {
  FontTestHelper helper(1.25f, FontTestHelper::INTERNAL);

  ASSERT_DOUBLE_EQ(
      1.25f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_TRUE(IsTextSubpixelPositioningEnabled());
  EXPECT_EQ(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());

  helper.display_manager()->UpdateZoomFactor(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(), 0.8f);

  ASSERT_DOUBLE_EQ(
      1.f,
      display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor());
  EXPECT_FALSE(IsTextSubpixelPositioningEnabled());
  EXPECT_NE(gfx::FontRenderParams::HINTING_NONE, GetFontHintingParams());
}

TEST_F(DisplayManagerTest, CheckInitializationOfRotationProperty) {
  int64_t id = display_manager()->GetDisplayAt(0).id();
  display_manager()->RegisterDisplayProperty(
      id, display::Display::ROTATE_90, /*overscan_insets=*/nullptr,
      /*resolution_in_pixels=*/gfx::Size(),
      /*device_scale_factor=*/1.0f, /*display_zoom_factor=*/1.0f,
      /*display_zoom_factor_map=*/{}, /*refresh_rate=*/60.f,
      /*is_interlaced=*/false,
      /*variable_refresh_rate_state=*/
      display::VariableRefreshRateState::kVrrNotCapable,
      /*vsync_rate_min=*/std::nullopt);

  const display::ManagedDisplayInfo& info =
      display_manager()->GetDisplayInfo(id);

  EXPECT_EQ(display::Display::ROTATE_90,
            info.GetRotation(display::Display::RotationSource::USER));
  EXPECT_EQ(display::Display::ROTATE_90,
            info.GetRotation(display::Display::RotationSource::ACTIVE));
}

TEST_F(DisplayManagerTest, RejectInvalidLayoutData) {
  display::DisplayLayoutStore* layout_store = display_manager()->layout_store();
  int64_t id1 = 10001;
  int64_t id2 = 10002;
  ASSERT_TRUE(display::CompareDisplayIds(id1, id2));
  display::DisplayLayoutBuilder good_builder(id1);
  good_builder.SetSecondaryPlacement(id2, display::DisplayPlacement::LEFT, 0);
  std::unique_ptr<display::DisplayLayout> good(good_builder.Build());

  display::DisplayIdList good_list =
      display::test::CreateDisplayIdList2(id1, id2);
  layout_store->RegisterLayoutForDisplayIdList(good_list, good->Copy());

  display::DisplayLayoutBuilder bad(id1);
  bad.SetSecondaryPlacement(id2, display::DisplayPlacement::BOTTOM, 0);

  display::DisplayIdList bad_list(2);
  bad_list[0] = id2;
  bad_list[1] = id1;
  layout_store->RegisterLayoutForDisplayIdList(bad_list, bad.Build());

  EXPECT_EQ(good->ToString(),
            layout_store->GetRegisteredDisplayLayout(good_list).ToString());
}

TEST_F(DisplayManagerTest, GuessDisplayIdFieldsInDisplayLayout) {
  int64_t id1 = 10001;
  int64_t id2 = 10002;

  std::unique_ptr<display::DisplayLayout> old_layout(
      new display::DisplayLayout);
  old_layout->placement_list.emplace_back(display::DisplayPlacement::BOTTOM, 0);
  old_layout->primary_id = id1;

  display::DisplayLayoutStore* layout_store = display_manager()->layout_store();
  display::DisplayIdList list = display::test::CreateDisplayIdList2(id1, id2);
  layout_store->RegisterLayoutForDisplayIdList(list, std::move(old_layout));
  const display::DisplayLayout& stored =
      layout_store->GetRegisteredDisplayLayout(list);

  EXPECT_EQ(id1, stored.placement_list[0].parent_display_id);
  EXPECT_EQ(id2, stored.placement_list[0].display_id);
}

TEST_F(DisplayManagerTest, AccelerometerSupport) {
  display::test::DisplayManagerTestApi display_manager_test(display_manager());
  display_manager_test.SetFirstDisplayAsInternalDisplay();
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(display::Display::AccelerometerSupport::UNAVAILABLE,
            screen->GetPrimaryDisplay().accelerometer_support());

  display_manager()->set_internal_display_has_accelerometer(true);
  display_manager()->UpdateDisplays();
  EXPECT_EQ(display::Display::AccelerometerSupport::AVAILABLE,
            screen->GetPrimaryDisplay().accelerometer_support());

  UpdateDisplay("1000x9000,800x700");
  EXPECT_EQ(display::Display::AccelerometerSupport::AVAILABLE,
            screen->GetPrimaryDisplay().accelerometer_support());
  EXPECT_EQ(display::Display::AccelerometerSupport::UNAVAILABLE,
            display_manager_test.GetSecondaryDisplay().accelerometer_support());

  // Secondary is now primary and should not have accelerometer support.
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(
      CreateDisplayInfo(display_manager_test.GetSecondaryDisplay().id(),
                        gfx::Rect(1, 1, 200, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(display::Display::AccelerometerSupport::UNAVAILABLE,
            screen->GetPrimaryDisplay().accelerometer_support());

  // Re-enable internal display.
  display_info_list.clear();
  display_info_list.push_back(CreateDisplayInfo(
      display::Display::InternalDisplayId(), gfx::Rect(1, 1, 200, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(display::Display::AccelerometerSupport::AVAILABLE,
            screen->GetPrimaryDisplay().accelerometer_support());
}

namespace {

std::unique_ptr<display::DisplayMode> MakeDisplayMode() {
  return std::make_unique<display::DisplayMode>(gfx::Size(1366, 768), false,
                                                60);
}

}  // namespace

TEST_F(DisplayManagerTest, DisconnectedInternalDisplayShouldUpdateDisplayInfo) {
  constexpr int64_t external_id = 123;
  const int64_t internal_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  display::Screen* screen = display::Screen::GetScreen();
  DCHECK(screen);
  Shell* shell = Shell::Get();
  display::DisplayChangeObserver observer(shell->display_manager());
  display::DisplayConfigurator::DisplayStateList outputs;
  std::unique_ptr<display::DisplaySnapshot> internal_snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(internal_id)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetDPI(210)  // 1.6f
          .SetNativeMode(MakeDisplayMode())
          .Build();
  EXPECT_FALSE(internal_snapshot->current_mode());

  outputs.push_back(internal_snapshot.get());
  std::unique_ptr<display::DisplaySnapshot> external_snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(external_id)
          .SetNativeMode(MakeDisplayMode())
          .AddMode(MakeDisplayMode())
          .SetOrigin({0, 1000})
          .Build();
  // "Connected display" has the current mode.
  external_snapshot->set_current_mode(external_snapshot->native_mode());

  outputs.push_back(external_snapshot.get());

  // Update the display manager through DisplayChangeObserver.
  observer.GetStateForDisplayIds(outputs);
  observer.OnDisplayConfigurationChanged(outputs);

  EXPECT_EQ(1u, display_manager()->GetNumDisplays());
  EXPECT_TRUE(display_manager()->IsActiveDisplayId(external_id));
  EXPECT_FALSE(display_manager()->IsActiveDisplayId(internal_id));

  const display::ManagedDisplayInfo& display_info =
      display_manager()->GetDisplayInfo(internal_id);
  EXPECT_EQ(1.6f, display_info.device_scale_factor());
  ASSERT_EQ(1u, display_info.display_modes().size());
  EXPECT_EQ(1.6f, display_info.display_modes()[0].device_scale_factor());
}

TEST_F(DisplayManagerTest, UpdateInternalDisplayNativeBounds) {
  constexpr int64_t external_id = 123;
  const int64_t internal_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  display::Screen* screen = display::Screen::GetScreen();
  DCHECK(screen);
  display::DisplayChangeObserver observer(display_manager());
  display::DisplayConfigurator::DisplayStateList outputs;
  std::unique_ptr<display::DisplaySnapshot> internal_snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(internal_id)
          .SetType(display::DISPLAY_CONNECTION_TYPE_INTERNAL)
          .SetDPI(210)  // 1.6f
          .SetNativeMode(MakeDisplayMode())
          .Build();
  internal_snapshot->set_current_mode(internal_snapshot->native_mode());
  outputs.push_back(internal_snapshot.get());

  observer.GetStateForDisplayIds(outputs);
  observer.OnDisplayConfigurationChanged(outputs);
  EXPECT_EQ(1u, display_manager()->GetNumDisplays());

  internal_snapshot->set_origin({0, 1000});

  std::unique_ptr<display::DisplaySnapshot> external_snapshot =
      display::FakeDisplaySnapshot::Builder()
          .SetId(external_id)
          .SetNativeMode(MakeDisplayMode())
          .AddMode(MakeDisplayMode())
          .Build();
  // "Connected display" has the current mode.
  external_snapshot->set_current_mode(external_snapshot->native_mode());
  outputs.push_back(external_snapshot.get());

  reset();
  observer.GetStateForDisplayIds(outputs);
  observer.OnDisplayConfigurationChanged(outputs);

  EXPECT_EQ(2u, display_manager()->GetNumDisplays());
  EXPECT_TRUE(changed_metrics() &
              display::DisplayObserver::DISPLAY_METRIC_BOUNDS);
}

TEST_F(DisplayManagerTest, InternalDisplayWithMultipleModesAndOneNative) {
  int display_id = 1000;
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 800, 300));
  display::ManagedDisplayInfo::ManagedDisplayModeList display_modes;
  display_modes.emplace_back(gfx::Size(1000, 500), 58.0f, false, false);
  display_modes.emplace_back(gfx::Size(800, 300), 59.0f, false, true);
  display_modes.emplace_back(gfx::Size(400, 500), 60.0f, false, false);

  native_display_info.SetManagedDisplayModes(display_modes);
  display::SetInternalDisplayIds({display_id});

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  display::ManagedDisplayMode expected_mode(gfx::Size(800, 300), 59.0f, false,
                                            true);

  display::ManagedDisplayMode active_mode;
  EXPECT_TRUE(
      display_manager()->GetActiveModeForDisplayId(display_id, &active_mode));
  EXPECT_TRUE(expected_mode.IsEquivalent(active_mode));
}

// It's difficult to test with full stack due to crbug.com/771178.
// Improve the coverage once it is fixed.
TEST_F(DisplayManagerTest, ForcedMirrorMode) {
  // Disable restoring mirror mode to prevent interference from previous
  // display configuration.
  display_manager()->set_disable_restoring_mirror_mode_for_test(true);

  constexpr int64_t id1 = 1;
  constexpr int64_t id2 = 2;
  display::Screen* screen = display::Screen::GetScreen();
  DCHECK(screen);
  display::DisplayChangeObserver observer(display_manager());
  display::DisplayConfigurator::DisplayStateList outputs;
  std::unique_ptr<display::DisplaySnapshot> snapshot1 =
      display::FakeDisplaySnapshot::Builder()
          .SetId(id1)
          .SetNativeMode(MakeDisplayMode())
          .Build();
  std::unique_ptr<display::DisplaySnapshot> snapshot2 =
      display::FakeDisplaySnapshot::Builder()
          .SetId(id2)
          .SetNativeMode(MakeDisplayMode())
          .SetOrigin({0, 1000})
          .Build();
  snapshot1->set_current_mode(snapshot1->native_mode());
  snapshot2->set_current_mode(snapshot2->native_mode());

  outputs.push_back(snapshot1.get());
  outputs.push_back(snapshot2.get());

  EXPECT_EQ(display::MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            observer.GetStateForDisplayIds(outputs));

  display_manager()->layout_store()->set_forced_mirror_mode_for_tablet(true);

  observer.OnDisplayConfigurationChanged(outputs);

  const display::DisplayIdList current_list =
      display_manager()->GetConnectedDisplayIdList();
  display_manager()->layout_store()->UpdateDefaultUnified(current_list,
                                                          false /* unified */);
  EXPECT_EQ(display::MULTIPLE_DISPLAY_STATE_MULTI_MIRROR,
            observer.GetStateForDisplayIds(outputs));

  display_manager()->layout_store()->set_forced_mirror_mode_for_tablet(false);

  EXPECT_EQ(display::MULTIPLE_DISPLAY_STATE_MULTI_EXTENDED,
            observer.GetStateForDisplayIds(outputs));

  display_manager()->set_disable_restoring_mirror_mode_for_test(false);
}

namespace {

class DisplayManagerOrientationTest : public DisplayManagerTest {
 public:
  DisplayManagerOrientationTest() = default;

  DisplayManagerOrientationTest(const DisplayManagerOrientationTest&) = delete;
  DisplayManagerOrientationTest& operator=(
      const DisplayManagerOrientationTest&) = delete;

  ~DisplayManagerOrientationTest() override = default;

  void SetUp() override {
    DisplayManagerTest::SetUp();
    portrait_primary.Set(ACCELEROMETER_SOURCE_SCREEN, -base::kMeanGravityFloat,
                         0.f, 0.f);
    portrait_secondary.Set(ACCELEROMETER_SOURCE_SCREEN, base::kMeanGravityFloat,
                           0.f, 0.f);
    landscape_primary.Set(ACCELEROMETER_SOURCE_SCREEN, 0,
                          -base::kMeanGravityFloat, 0.f);
  }

 protected:
  AccelerometerUpdate portrait_primary;
  AccelerometerUpdate portrait_secondary;
  AccelerometerUpdate landscape_primary;
};

class TestObserver : public ScreenOrientationController::Observer {
 public:
  TestObserver() = default;
  ~TestObserver() override = default;

  void OnUserRotationLockChanged() override { count_++; }

  int countAndReset() {
    int tmp = count_;
    count_ = 0;
    return tmp;
  }

 private:
  int count_ = 0;
};

}  // namespace

TEST_F(DisplayManagerOrientationTest, SaveRestoreUserRotationLock) {
  Shell* shell = Shell::Get();
  display::DisplayManager* display_manager = shell->display_manager();
  display::test::DisplayManagerTestApi(display_manager)
      .SetFirstDisplayAsInternalDisplay();
  ScreenOrientationController* orientation_controller =
      shell->screen_orientation_controller();
  ScreenOrientationControllerTestApi test_api(orientation_controller);
  TestObserver test_observer;
  orientation_controller->AddObserver(&test_observer);

  // Set up windows with portrait, landscape, and any.
  aura::Window* window_a = CreateTestWindowInShellWithId(0);
  {
    window_a->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
    orientation_controller->LockOrientationForWindow(
        window_a, chromeos::OrientationType::kAny);
  }
  aura::Window* window_p = CreateTestWindowInShellWithId(0);
  {
    window_p->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
    orientation_controller->LockOrientationForWindow(
        window_p, chromeos::OrientationType::kPortrait);
  }
  aura::Window* window_l = CreateTestWindowInShellWithId(0);
  {
    window_l->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
    orientation_controller->LockOrientationForWindow(
        window_l, chromeos::OrientationType::kLandscape);
  }

  DisplayConfigurationController* configuration_controller =
      shell->display_configuration_controller();
  display::Screen* screen = display::Screen::GetScreen();

  // Rotate to portrait in clamshell.
  configuration_controller->SetDisplayRotation(
      screen->GetPrimaryDisplay().id(), display::Display::ROTATE_270,
      display::Display::RotationSource::USER);
  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());
  EXPECT_FALSE(display_manager->registered_internal_display_rotation_lock());

  EXPECT_EQ(0, test_observer.countAndReset());
  // Just enabling will not save the lock.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(1, test_observer.countAndReset());

  EXPECT_EQ(display::Display::ROTATE_0, screen->GetPrimaryDisplay().rotation());
  EXPECT_FALSE(display_manager->registered_internal_display_rotation_lock());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            test_api.GetCurrentOrientation());

  // Enable lock at 0.
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(1, test_observer.countAndReset());

  EXPECT_TRUE(display_manager->registered_internal_display_rotation_lock());
  EXPECT_EQ(display::Display::ROTATE_0,
            display_manager->registered_internal_display_rotation());

  // Application can overwrite the locked orientation.
  wm::ActivateWindow(window_p);
  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());
  EXPECT_EQ(display::Display::ROTATE_0,
            display_manager->registered_internal_display_rotation());
  EXPECT_EQ(0, test_observer.countAndReset());
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());

  // Any will rotate to the locked rotation.
  wm::ActivateWindow(window_a);
  EXPECT_EQ(display::Display::ROTATE_0, screen->GetPrimaryDisplay().rotation());
  EXPECT_TRUE(display_manager->registered_internal_display_rotation_lock());
  EXPECT_EQ(display::Display::ROTATE_0,
            display_manager->registered_internal_display_rotation());
  EXPECT_EQ(0, test_observer.countAndReset());

  wm::ActivateWindow(window_l);
  EXPECT_EQ(display::Display::ROTATE_0, screen->GetPrimaryDisplay().rotation());
  EXPECT_TRUE(display_manager->registered_internal_display_rotation_lock());
  EXPECT_EQ(display::Display::ROTATE_0,
            display_manager->registered_internal_display_rotation());
  EXPECT_EQ(0, test_observer.countAndReset());

  // Exit tablet mode reset to clamshell's rotation, which is 90.
  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(1, test_observer.countAndReset());
  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());
  // Activate Any.
  wm::ActivateWindow(window_a);
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(1, test_observer.countAndReset());
  // Entering with active ANY will lock again to landscape.
  EXPECT_EQ(display::Display::ROTATE_0, screen->GetPrimaryDisplay().rotation());

  wm::ActivateWindow(window_p);
  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());
  EXPECT_EQ(0, test_observer.countAndReset());
  orientation_controller->ToggleUserRotationLock();
  orientation_controller->ToggleUserRotationLock();
  EXPECT_EQ(2, test_observer.countAndReset());

  EXPECT_TRUE(display_manager->registered_internal_display_rotation_lock());
  EXPECT_EQ(display::Display::ROTATE_270,
            display_manager->registered_internal_display_rotation());

  wm::ActivateWindow(window_l);
  EXPECT_EQ(display::Display::ROTATE_0, screen->GetPrimaryDisplay().rotation());
  EXPECT_EQ(display::Display::ROTATE_270,
            display_manager->registered_internal_display_rotation());

  // ANY will rotate to locked rotation.
  wm::ActivateWindow(window_a);
  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());

  orientation_controller->RemoveObserver(&test_observer);
}

TEST_F(DisplayManagerOrientationTest, UserRotationLockReverse) {
  Shell* shell = Shell::Get();
  display::DisplayManager* display_manager = shell->display_manager();
  display::test::DisplayManagerTestApi test_api(display_manager);
  test_api.SetFirstDisplayAsInternalDisplay();
  ScreenOrientationController* orientation_controller =
      shell->screen_orientation_controller();

  // Set up windows with portrait, landscape, and any.
  aura::Window* window = CreateTestWindowInShellWithId(0);
  window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  display::Screen* screen = display::Screen::GetScreen();

  // Just enabling will not save the lock.
  ash::TabletModeControllerTestApi().EnterTabletMode();

  orientation_controller->LockOrientationForWindow(
      window, chromeos::OrientationType::kPortrait);
  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());

  orientation_controller->OnAccelerometerUpdated(portrait_secondary);

  EXPECT_EQ(display::Display::ROTATE_90,
            screen->GetPrimaryDisplay().rotation());

  orientation_controller->OnAccelerometerUpdated(portrait_primary);
  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());

  // Enable lock at 270.
  orientation_controller->ToggleUserRotationLock();
  EXPECT_TRUE(display_manager->registered_internal_display_rotation_lock());
  EXPECT_EQ(display::Display::ROTATE_270,
            display_manager->registered_internal_display_rotation());

  orientation_controller->OnAccelerometerUpdated(portrait_secondary);

  EXPECT_EQ(display::Display::ROTATE_270,
            screen->GetPrimaryDisplay().rotation());
}

TEST_F(DisplayManagerOrientationTest, LockToSpecificOrientation) {
  Shell* shell = Shell::Get();
  display::DisplayManager* display_manager = shell->display_manager();
  display::test::DisplayManagerTestApi(display_manager)
      .SetFirstDisplayAsInternalDisplay();
  ScreenOrientationController* orientation_controller =
      shell->screen_orientation_controller();
  ScreenOrientationControllerTestApi test_api(orientation_controller);

  aura::Window* window_a = CreateTestWindowInShellWithId(0);
  {
    window_a->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
    orientation_controller->LockOrientationForWindow(
        window_a, chromeos::OrientationType::kAny);
  }
  wm::ActivateWindow(window_a);
  ash::TabletModeControllerTestApi().EnterTabletMode();

  orientation_controller->OnAccelerometerUpdated(portrait_primary);

  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());

  orientation_controller->OnAccelerometerUpdated(portrait_secondary);

  aura::Window* window_lsc = CreateTestWindowInShellWithId(1);
  window_lsc->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);

  aura::Window* window_psc = CreateTestWindowInShellWithId(1);
  window_psc->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);

  orientation_controller->LockOrientationForWindow(
      window_psc, chromeos::OrientationType::kPortraitSecondary);
  orientation_controller->LockOrientationForWindow(
      window_psc, chromeos::OrientationType::kCurrent);
  wm::ActivateWindow(window_psc);

  orientation_controller->LockOrientationForWindow(
      window_lsc, chromeos::OrientationType::kLandscapeSecondary);
  orientation_controller->LockOrientationForWindow(
      window_lsc, chromeos::OrientationType::kCurrent);

  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            test_api.GetCurrentOrientation());

  // The orientation should stay portrait secondary.
  orientation_controller->OnAccelerometerUpdated(portrait_primary);
  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
  wm::ActivateWindow(window_lsc);

  EXPECT_EQ(chromeos::OrientationType::kLandscapeSecondary,
            test_api.GetCurrentOrientation());

  // The orientation should stay landscape secondary.
  orientation_controller->OnAccelerometerUpdated(landscape_primary);
  EXPECT_EQ(chromeos::OrientationType::kLandscapeSecondary,
            test_api.GetCurrentOrientation());

  wm::ActivateWindow(window_a);
  orientation_controller->OnAccelerometerUpdated(portrait_primary);

  // Switching to |window_a| enables rotation.
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            test_api.GetCurrentOrientation());

  // The orientation has already been locked to secondary once, so
  // it should switch back to the portrait secondary.
  wm::ActivateWindow(window_psc);
  EXPECT_EQ(chromeos::OrientationType::kPortraitSecondary,
            test_api.GetCurrentOrientation());
}

// crbug.com/734107
TEST_F(DisplayManagerOrientationTest, DisplayChangeShouldNotSaveUserRotation) {
  Shell* shell = Shell::Get();
  display::DisplayManager* display_manager = shell->display_manager();
  display::test::DisplayManagerTestApi test_api(display_manager);
  test_api.SetFirstDisplayAsInternalDisplay();
  display::Screen* screen = display::Screen::GetScreen();

  ash::TabletModeControllerTestApi().EnterTabletMode();
  // Emulate that Animator is calling this async when animation is completed.
  display_manager->SetDisplayRotation(
      screen->GetPrimaryDisplay().id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACCELEROMETER);
  EXPECT_EQ(display::Display::ROTATE_90,
            screen->GetPrimaryDisplay().rotation());

  ash::TabletModeControllerTestApi().LeaveTabletMode();
  EXPECT_EQ(display::Display::ROTATE_0, screen->GetPrimaryDisplay().rotation());
}

TEST_F(DisplayManagerTest, HardwareMirrorMode) {
  // Create three displays with the same origin in frame buffer.
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t first_mirror_id = 11;
  constexpr int64_t second_mirror_id = 12;
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 500, 400)));
  display_info_list.push_back(
      CreateDisplayInfo(first_mirror_id, gfx::Rect(0, 0, 500, 400)));
  display_info_list.push_back(
      CreateDisplayInfo(second_mirror_id, gfx::Rect(0, 0, 500, 400)));

  // mirrored across 3 displays...
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(3U, display_manager()->num_connected_displays());

  EXPECT_EQ(internal_display_id, display_manager()->mirroring_source_id());
  EXPECT_EQ(gfx::Rect(0, 0, 500, 400),
            GetDisplayForId(internal_display_id).bounds());

  const display::DisplayIdList id_list =
      display_manager()->GetMirroringDestinationDisplayIdList();
  ASSERT_EQ(2U, id_list.size());
  EXPECT_EQ(11U, id_list[0]);
  EXPECT_EQ(12U, id_list[1]);

  EXPECT_FALSE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_TRUE(display_manager()->IsInHardwareMirrorMode());
}

TEST_F(DisplayManagerTest, SoftwareMirrorModeBasics) {
  UpdateDisplay("300x400,400x500,500x600");

  // There's not mirror window by default.
  MirrorWindowTestApi test_api;
  EXPECT_TRUE(test_api.GetHosts().empty());

  TestDisplayObserver display_observer;
  display::Screen::GetScreen()->AddObserver(&display_observer);

  // Turn on mirror mode.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 400),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());

  std::vector<aura::WindowTreeHost*> host_list = test_api.GetHosts();
  ASSERT_EQ(2U, host_list.size());
  EXPECT_EQ(gfx::Size(400, 500), host_list[0]->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Size(300, 400), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::Size(500, 600), host_list[1]->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Size(300, 400), host_list[1]->window()->bounds().size());

  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_FALSE(display_manager()->IsInHardwareMirrorMode());

  // Turn off mirror mode.
  SetSoftwareMirrorMode(false);
  EXPECT_TRUE(display_observer.changed_and_reset());
  EXPECT_EQ(3U, display_manager()->GetNumDisplays());

  host_list = test_api.GetHosts();
  EXPECT_TRUE(host_list.empty());

  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Make sure the mirror window has the pixel size of the
  // source display.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_observer.changed_and_reset());

  UpdateDisplay("300x400@0.5,400x500,500x600");
  EXPECT_FALSE(display_observer.changed_and_reset());
  host_list = test_api.GetHosts();
  EXPECT_EQ(gfx::Size(300, 400), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::Size(300, 400), host_list[1]->window()->bounds().size());

  UpdateDisplay("310x410*2,400x500,500x600");
  EXPECT_FALSE(display_observer.changed_and_reset());
  host_list = test_api.GetHosts();
  EXPECT_EQ(gfx::Size(310, 410), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::Size(310, 410), host_list[1]->window()->bounds().size());

  UpdateDisplay("320x420/r,400x500,500x600");
  EXPECT_FALSE(display_observer.changed_and_reset());
  host_list = test_api.GetHosts();
  EXPECT_EQ(gfx::Size(420, 320), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::Size(420, 320), host_list[1]->window()->bounds().size());

  UpdateDisplay("330x440/r,400x500,500x600");
  EXPECT_FALSE(display_observer.changed_and_reset());
  host_list = test_api.GetHosts();
  EXPECT_EQ(gfx::Size(440, 330), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::Size(440, 330), host_list[1]->window()->bounds().size());

  // Overscan insets are ignored.
  UpdateDisplay("400x600/o,600x800/o,500x600/o");
  EXPECT_FALSE(display_observer.changed_and_reset());
  host_list = test_api.GetHosts();
  EXPECT_EQ(gfx::Size(400, 600), host_list[0]->window()->bounds().size());
  EXPECT_EQ(gfx::Size(400, 600), host_list[1]->window()->bounds().size());

  display::Screen::GetScreen()->RemoveObserver(&display_observer);
}

TEST_F(DisplayManagerTest, SwitchToAndFromSoftwareMirrorMode) {
  // Don't check root window destruction in unified mode.
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  UpdateDisplay("300x400,400x500,500x600");

  // Switch from extended to mirroring.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Switch from mirroring to extended.
  SetSoftwareMirrorMode(false);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Switch from mirroring to unified, but it fails.
  SetSoftwareMirrorMode(true);
  display_manager()->SetUnifiedDesktopEnabled(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());

  // Turn off mirroring, it switches to unified.
  SetSoftwareMirrorMode(false);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->IsInUnifiedMode());

  // Switch from unified to mirroring.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_FALSE(display_manager()->IsInUnifiedMode());
}

TEST_F(DisplayManagerTest, SourceAndDestinationInSoftwareMirrorMode) {
  constexpr int64_t first_display_id = 10;
  constexpr int64_t second_display_id = 11;
  constexpr int64_t third_display_id = 12;
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.emplace_back(
      CreateDisplayInfo(first_display_id, gfx::Rect(0, 0, 200, 100)));
  display_info_list.emplace_back(
      CreateDisplayInfo(second_display_id, gfx::Rect(1, 1, 500, 400)));
  display_info_list.emplace_back(
      CreateDisplayInfo(third_display_id, gfx::Rect(2, 2, 500, 400)));

  // Connect all displays.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(display::kInvalidDisplayId,
            display_manager()->mirroring_source_id());
  EXPECT_TRUE(
      display_manager()->GetMirroringDestinationDisplayIdList().empty());

  // Activate software mirror mode.
  SetSoftwareMirrorMode(true);
  EXPECT_EQ(first_display_id, display_manager()->mirroring_source_id());
  display::DisplayIdList id_list =
      display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(2U, id_list.size());
  EXPECT_EQ(second_display_id, id_list[0]);
  EXPECT_EQ(third_display_id, id_list[1]);

  // Set the second display as internal display.
  SetSoftwareMirrorMode(false);
  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         second_display_id);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(second_display_id, display_manager()->mirroring_source_id());
  id_list = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(2U, id_list.size());
  EXPECT_EQ(first_display_id, id_list[0]);
  EXPECT_EQ(third_display_id, id_list[1]);
}

TEST_F(DisplayManagerTest, CompositingCursorInMultiSoftwareMirroring) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t first_mirror_id = 11;
  constexpr int64_t second_mirror_id = 12;
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 200, 100)));
  display_info_list.push_back(
      CreateDisplayInfo(first_mirror_id, gfx::Rect(1, 1, 500, 400)));
  display_info_list.push_back(
      CreateDisplayInfo(second_mirror_id, gfx::Rect(2, 2, 500, 400)));

  // Connect all displays, cursor compositing is disabled by default.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  base::RunLoop().RunUntilIdle();
  CursorWindowController* cursor_window_controller =
      Shell::Get()->window_tree_host_manager()->cursor_window_controller();
  EXPECT_FALSE(cursor_window_controller->is_cursor_compositing_enabled());
  MirrorWindowTestApi test_api;
  EXPECT_EQ(nullptr, test_api.GetCursorHostWindow());

  // Turn on mirror mode, cursor compositing is enabled and cursor window is
  // composited in internal display's root window.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(cursor_window_controller->is_cursor_compositing_enabled());
  EXPECT_TRUE(Shell::GetRootWindowForDisplayId(internal_display_id)
                  ->Contains(test_api.GetCursorHostWindow()));

  // Turn off mirror mode, cursor compositing is disabled and cursor window does
  // not exist.
  SetSoftwareMirrorMode(false);
  EXPECT_FALSE(cursor_window_controller->is_cursor_compositing_enabled());
  EXPECT_EQ(nullptr, test_api.GetCursorHostWindow());
}

TEST_F(DisplayManagerTest, MirrorModeRestore) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t first_display_id = 210000001;
  constexpr int64_t second_display_id = 220000002;
  const int64_t first_display_masked_id =
      display::GetDisplayIdWithoutOutputIndex(first_display_id);
  const int64_t second_display_masked_id =
      display::GetDisplayIdWithoutOutputIndex(second_display_id);
  display::ManagedDisplayInfo first_mirror_info =
      CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 400));
  display::ManagedDisplayInfo second_mirror_info =
      CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 400));
  std::vector<display::ManagedDisplayInfo> display_info_list;

  // There's no external display now.
  display_info_list.push_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 200, 100)));
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());

  // Connect the first external display.
  display_info_list.push_back(first_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());

  // Turn on mirror mode.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));

  // Remove the first external display.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));

  // Reconnect the first external display.
  display_info_list.push_back(first_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));

  // Remove the first external display.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));

  // Connect the second external display.
  display_info_list.push_back(second_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));

  // Remove the second external display.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));

  // Add the first and then add the second external display (not mirrored
  // before).
  display_info_list.push_back(first_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  display_info_list.push_back(second_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(2U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      second_display_masked_id));

  // Remove the second display.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(2U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      second_display_masked_id));

  // Remove the first display and then add the second display.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  display_info_list.push_back(second_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(2U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      second_display_masked_id));

  // Turn off mirror mode.
  SetSoftwareMirrorMode(false);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(1U, display_manager()->external_display_mirror_info().size());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().count(
      first_display_masked_id));

  // Add the first display (mirrored before).
  display_info_list.push_back(first_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_TRUE(display_manager()->external_display_mirror_info().empty());
}

TEST_F(DisplayManagerTest, MixedMirrorModeBasics) {
  UpdateDisplay("300x400,400x500,500x600");
  display::DisplayIdList id_list =
      display_manager()->GetConnectedDisplayIdList();

  // Turn on mixed mirror mode. (Mirror from the first display to the second
  // display)
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(id_list[1]);
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, id_list[0], dst_ids);
  display_manager()->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
  display::DisplayIdList destination_ids =
      display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(id_list[1], destination_ids[0]);
  EXPECT_TRUE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(gfx::Point(300, 0),
            display_manager()->GetDisplayForId(id_list[2]).bounds().origin());

  // Turn off mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());
  EXPECT_FALSE(display_manager()->mixed_mirror_mode_params());
  EXPECT_EQ(gfx::Point(300, 0),
            display_manager()->GetDisplayForId(id_list[1]).bounds().origin());
  EXPECT_EQ(gfx::Point(700, 0),
            display_manager()->GetDisplayForId(id_list[2]).bounds().origin());
}

TEST_F(DisplayManagerTest, MixedMirrorModeToMirrorMode) {
  UpdateDisplay("300x400,400x500,500x600");
  display::DisplayIdList id_list =
      display_manager()->GetConnectedDisplayIdList();

  // Turn on mixed mirror mode. (Mirror from the first display to the second
  // display)
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(id_list[1]);
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, id_list[0], dst_ids);
  display_manager()->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
  display::DisplayIdList destination_ids =
      display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(id_list[1], destination_ids[0]);
  EXPECT_TRUE(display_manager()->mixed_mirror_mode_params());

  // Overwrite mixed mirror mode with default mirror mode (Mirror all
  // displays).
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
  destination_ids = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(2U, destination_ids.size());
  EXPECT_EQ(id_list[1], destination_ids[0]);
  EXPECT_EQ(id_list[2], destination_ids[1]);
  EXPECT_FALSE(display_manager()->mixed_mirror_mode_params());
}

TEST_F(DisplayManagerTest, MirrorModeToMixedMirrorMode) {
  UpdateDisplay("300x400,400x500,500x600");
  display::DisplayIdList id_list =
      display_manager()->GetConnectedDisplayIdList();

  // Turn on mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
  display::DisplayIdList destination_ids =
      display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(2U, destination_ids.size());
  EXPECT_EQ(id_list[1], destination_ids[0]);
  EXPECT_EQ(id_list[2], destination_ids[1]);
  EXPECT_FALSE(display_manager()->mixed_mirror_mode_params());

  // Overwrite default mirror mode with mixed mirror mode. (Mirror from the
  // first display to the second display)
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(id_list[1]);
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, id_list[0], dst_ids);
  display_manager()->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(id_list[0], display_manager()->mirroring_source_id());
  destination_ids = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(id_list[1], destination_ids[0]);
  EXPECT_TRUE(display_manager()->mixed_mirror_mode_params());
}

TEST_F(DisplayManagerTest, MixedMirrorModeRestore) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t first_display_id = 210000001;
  constexpr int64_t second_display_id = 220000002;
  display::ManagedDisplayInfo first_mirror_info =
      CreateDisplayInfo(first_display_id, gfx::Rect(1, 1, 500, 400));
  display::ManagedDisplayInfo second_mirror_info =
      CreateDisplayInfo(second_display_id, gfx::Rect(2, 2, 500, 400));
  std::vector<display::ManagedDisplayInfo> display_info_list;

  // Connect the first and second displays.
  display_info_list.push_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 200, 100)));
  display_info_list.push_back(first_mirror_info);
  display_info_list.push_back(second_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Turn on mixed mirror mode. (Mirror from the internal display to the
  // first display)
  display::DisplayIdList dst_ids;
  dst_ids.emplace_back(first_display_id);
  std::optional<display::MixedMirrorModeParams> mixed_params(
      std::in_place, internal_display_id, dst_ids);
  display_manager()->SetMirrorMode(display::MirrorMode::kMixed, mixed_params);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(internal_display_id, display_manager()->mirroring_source_id());
  display::DisplayIdList destination_ids =
      display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(first_display_id, destination_ids[0]);

  // Remove the second display. Mirroring is not changed.
  display_info_list.erase(display_info_list.end() - 1);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(internal_display_id, display_manager()->mirroring_source_id());
  destination_ids = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(first_display_id, destination_ids[0]);

  // Add the second display. Mirroring is not changed.
  display_info_list.push_back(second_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(internal_display_id, display_manager()->mirroring_source_id());
  destination_ids = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(first_display_id, destination_ids[0]);

  // Remove the first display. Mirroring ends.
  display_info_list.erase(display_info_list.end() - 2);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // Add the first display. Mirroring is restored.
  display_info_list.push_back(first_mirror_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(internal_display_id, display_manager()->mirroring_source_id());
  destination_ids = display_manager()->GetMirroringDestinationDisplayIdList();
  EXPECT_EQ(1U, destination_ids.size());
  EXPECT_EQ(first_display_id, destination_ids[0]);
}

TEST_F(DisplayManagerTest, MirrorModeRestoreAfterResume) {
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();
  constexpr int64_t external_display_id = 210000001;
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.emplace_back(
      CreateDisplayInfo(internal_display_id, gfx::Rect(0, 0, 200, 100)));
  display_info_list.emplace_back(
      CreateDisplayInfo(external_display_id, gfx::Rect(1, 1, 500, 400)));

  // Turn on mirror mode.
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // Suspend.
  display_manager()->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  display_manager()->OnNativeDisplaysChanged(
      std::vector<display::ManagedDisplayInfo>());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  // Resume.
  display_manager()->SetMultiDisplayMode(display::DisplayManager::MIRRORING);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
}

TEST_F(DisplayManagerTest, SoftwareMirrorRotationForTablet) {
  enum Scenario {
    // Auto mirror mode set when entering tablet mode.
    kForcedMirror,
    // Manual mirror mode with device is in physical tablet mode.
    kPhysicalTablet,
  };

  // Set the first display as internal display so that the tablet mode can be
  // enabled.
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();

  auto tablet_mode_test_api = std::make_unique<TabletModeControllerTestApi>();

  for (auto sc : {kForcedMirror, kPhysicalTablet}) {
    SCOPED_TRACE(testing::Message() << "Scenario: " << sc);

    UpdateDisplay("400x300,800x700");
    switch (sc) {
      case kForcedMirror: {
        // Simulate turning on mirror mode triggered by tablet mode on.
        tablet_mode_test_api->EnterTabletMode();
        base::RunLoop().RunUntilIdle();
        break;
      }
      case kPhysicalTablet: {
        // Simulate physical tablet mode with clamshell ui.
        tablet_mode_test_api->EnterTabletMode();
        tablet_mode_test_api->AttachExternalMouse();
        base::RunLoop().RunUntilIdle();

        // Manual mirror mode.
        SetSoftwareMirrorMode(true);

        ASSERT_TRUE(Shell::Get()
                        ->tablet_mode_controller()
                        ->is_in_tablet_physical_state());
        ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());
        break;
      }
    }

    ASSERT_TRUE(display_manager()->IsInSoftwareMirrorMode());
    EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
              display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
    MirrorWindowTestApi test_api;
    std::vector<aura::WindowTreeHost*> host_list = test_api.GetHosts();
    ASSERT_EQ(1U, host_list.size());
    EXPECT_EQ(gfx::Size(800, 700), host_list[0]->GetBoundsInPixels().size());
    EXPECT_EQ(gfx::Size(400, 300), host_list[0]->window()->bounds().size());

    // Test the target display's bounds after the transforms are applied.
    gfx::RectF transformed_rect1 =
        Shell::Get()->GetPrimaryRootWindow()->transform().MapRect(gfx::RectF(
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds()));
    transformed_rect1 =
        host_list[0]->window()->transform().MapRect(transformed_rect1);
    EXPECT_EQ(gfx::RectF(0.0f, 50.0f, 800.0f, 600.0f), transformed_rect1);

    // Rotate the source display by 90 degrees.
    UpdateDisplay("400x300/r,800x700");
    EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
    EXPECT_EQ(gfx::Rect(0, 0, 300, 400),
              display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
    host_list = test_api.GetHosts();
    ASSERT_EQ(1U, host_list.size());
    EXPECT_EQ(gfx::Size(800, 700), host_list[0]->GetBoundsInPixels().size());
    EXPECT_EQ(gfx::Size(300, 400), host_list[0]->window()->bounds().size());

    // Test the target display's bounds after the transforms are applied.
    gfx::RectF transformed_rect2 =
        Shell::Get()->GetPrimaryRootWindow()->transform().MapRect(gfx::RectF(
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds()));
    transformed_rect2 =
        host_list[0]->window()->transform().MapRect(transformed_rect2);
    // Use gfx::ToEnclosingRect because `transformed_rect2` has rounding errors:
    //   137.000000,0.000000 524.999939x699.999939
    EXPECT_EQ(gfx::Rect(137.0f, 0.0f, 525.0f, 700.0f),
              gfx::ToEnclosingRect(transformed_rect2));

    // Change the bounds of the source display and rotate the source display by
    // 90 degrees.
    UpdateDisplay("300x400/r,800x700");
    EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
    EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
              display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
    host_list = test_api.GetHosts();
    ASSERT_EQ(1U, host_list.size());
    EXPECT_EQ(gfx::Size(800, 700), host_list[0]->GetBoundsInPixels().size());
    EXPECT_EQ(gfx::Size(400, 300), host_list[0]->window()->bounds().size());

    // Test the target display's bounds after the transforms are applied.
    gfx::RectF transformed_rect3 =
        Shell::Get()->GetPrimaryRootWindow()->transform().MapRect(gfx::RectF(
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds()));
    transformed_rect3 =
        host_list[0]->window()->transform().MapRect(transformed_rect3);
    EXPECT_EQ(gfx::RectF(0.0f, 50.0f, 800.0f, 600.0f), transformed_rect3);
  }
}

TEST_F(DisplayManagerTest, SoftwareMirrorRotationForNonTablet) {
  MirrorWindowTestApi test_api;
  UpdateDisplay("400x300,800x700");

  // Simulate turning on mirror mode not triggered by tablet mode.
  SetSoftwareMirrorMode(true);
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  std::vector<aura::WindowTreeHost*> host_list = test_api.GetHosts();
  ASSERT_EQ(1U, host_list.size());
  EXPECT_EQ(gfx::Size(800, 700), host_list[0]->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Size(400, 300), host_list[0]->window()->bounds().size());

  // Test the target display's bounds after the transforms are applied.
  gfx::RectF transformed_rect1 =
      Shell::Get()->GetPrimaryRootWindow()->transform().MapRect(gfx::RectF(
          display::Screen::GetScreen()->GetPrimaryDisplay().bounds()));
  transformed_rect1 =
      host_list[0]->window()->transform().MapRect(transformed_rect1);
  EXPECT_EQ(gfx::RectF(0.0f, 50.0f, 800.0f, 600.0f), transformed_rect1);

  // Rotate the source display by 90 degrees.
  UpdateDisplay("400x300/r,800x700");
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 400),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  host_list = test_api.GetHosts();
  ASSERT_EQ(1U, host_list.size());
  EXPECT_EQ(gfx::Size(800, 700), host_list[0]->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Size(300, 400), host_list[0]->window()->bounds().size());

  // Test the target display's bounds after the transforms are applied.
  gfx::RectF transformed_rect2 =
      Shell::Get()->GetPrimaryRootWindow()->transform().MapRect(gfx::RectF(
          display::Screen::GetScreen()->GetPrimaryDisplay().bounds()));
  transformed_rect2 =
      host_list[0]->window()->transform().MapRect(transformed_rect2);
  EXPECT_EQ(gfx::RectF(50.0f, 0.0f, 600.0f, 800.0f), transformed_rect2);

  // Change the bounds of the source display and rotate the source display by 90
  // degrees.
  UpdateDisplay("300x400/r,800x700");
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 300),
            display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  host_list = test_api.GetHosts();
  ASSERT_EQ(1U, host_list.size());
  EXPECT_EQ(gfx::Size(800, 700), host_list[0]->GetBoundsInPixels().size());
  EXPECT_EQ(gfx::Size(400, 300), host_list[0]->window()->bounds().size());

  // Test the target display's bounds after the transforms are applied.
  gfx::RectF transformed_rect3 =
      Shell::Get()->GetPrimaryRootWindow()->transform().MapRect(gfx::RectF(
          display::Screen::GetScreen()->GetPrimaryDisplay().bounds()));
  transformed_rect3 =
      host_list[0]->window()->transform().MapRect(transformed_rect3);
  // Use gfx::ToEnclosingRect because `transformed_rect3` has rounding errors.
  EXPECT_EQ(gfx::Rect(0.0f, 137.0f, 700.0f, 525.0f),
            gfx::ToEnclosingRect(transformed_rect3));
}

TEST_F(DisplayManagerTest, DPSizeTest) {
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay(base::StringPrintf("3840x2160*%s", display::kDsfStr_2_666));
  {
    gfx::Size expected(1440, 810);
    EXPECT_EQ(expected,
              display::Screen::GetScreen()->GetPrimaryDisplay().size());
    EXPECT_EQ(expected, Shell::GetPrimaryRootWindow()->bounds().size());
  }

  UpdateDisplay(base::StringPrintf("1920x1200*%s", display::kDsfStr_1_777));
  {
    gfx::Size expected(1080, 675);
    EXPECT_EQ(expected,
              display::Screen::GetScreen()->GetPrimaryDisplay().size());
    EXPECT_EQ(expected, Shell::GetPrimaryRootWindow()->bounds().size());
  }
  UpdateDisplay(base::StringPrintf("3000x2000*%s", display::kDsfStr_2_252));
  {
    gfx::Size expected(1332, 888);
    EXPECT_EQ(expected,
              display::Screen::GetScreen()->GetPrimaryDisplay().size());
    EXPECT_EQ(expected, Shell::GetPrimaryRootWindow()->bounds().size());
  }
  UpdateDisplay(base::StringPrintf("2160x1440*%s", display::kDsfStr_1_8));
  {
    gfx::Size expected(1200, 800);
    EXPECT_EQ(expected,
              display::Screen::GetScreen()->GetPrimaryDisplay().size());
    EXPECT_EQ(expected, Shell::GetPrimaryRootWindow()->bounds().size());
  }
}

TEST_F(DisplayManagerTest, PanelOrientation) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();

  display::test::ScopedSetInternalDisplayId set_internal(display_manager(),
                                                         display_id);

  // The panel is portrait but its orientation is landscape.
  display::ManagedDisplayInfo native_display_info =
      CreateDisplayInfo(display_id, gfx::Rect(0, 0, 1920, 1080));
  native_display_info.set_panel_orientation(
      display::PanelOrientation::kRightUp);
  const display::ManagedDisplayMode base_mode(gfx::Size(1920, 1080), 60.0f,
                                              false, false);
  display::ManagedDisplayInfo::ManagedDisplayModeList mode_list =
      CreateInternalManagedDisplayModeList(base_mode);
  native_display_info.SetManagedDisplayModes(mode_list);

  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(native_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  // Check display is landscape at ROTATE_0.
  EXPECT_EQ(gfx::Size(1080, 1920),
            display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel());
  EXPECT_EQ(display::Display::ROTATE_0,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());

  // Check the orientation controller reports correct orientation.
  auto* screen_orientation_controller =
      Shell::Get()->screen_orientation_controller();
  EXPECT_EQ(chromeos::OrientationType::kLandscape,
            screen_orientation_controller->natural_orientation());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());

  // Test if changing rotation works as if it's landscape panel.
  DisplayConfigurationController::DisableAnimatorForTest();
  ScreenOrientationControllerTestApi(screen_orientation_controller)
      .SetDisplayRotation(display::Display::ROTATE_270,
                          display::Display::RotationSource::USER);

  EXPECT_EQ(gfx::Size(1920, 1080),
            display::Screen::GetScreen()->GetPrimaryDisplay().GetSizeInPixel());
  EXPECT_EQ(display::Display::ROTATE_270,
            display::Screen::GetScreen()->GetPrimaryDisplay().rotation());
  EXPECT_EQ(chromeos::OrientationType::kPortraitPrimary,
            screen_orientation_controller->GetCurrentOrientation());
}

TEST_F(DisplayManagerTest, UpdateRootWindowForNewWindows) {
  Shell::GetPrimaryRootWindow()->RemoveObserver(this);

  // Sets display configuration with three displays, sets the "root window for
  // new windows" to the one at index before, removes the one at index 2, and
  // checks that the "root window for new windows" is the one at index after.
  const auto test_removing_secondary = [this](size_t before, size_t after) {
    UpdateDisplay("800x600,800x600,800x600");
    aura::Window::Windows root_windows = Shell::GetAllRootWindows();
    Shell::SetRootWindowForNewWindows(root_windows[before]);
    UpdateDisplay("800x600,800x600");
    EXPECT_EQ(root_windows[after], Shell::GetRootWindowForNewWindows());
  };
  test_removing_secondary(0u, 0u);
  test_removing_secondary(1u, 1u);
  test_removing_secondary(2u, 0u);

  // Each iteration sets display configuration with three displays, sets the
  // "root window for new windows" to the one at index before, enters unified
  // desktop mode, and checks that the "root window for new windows" is the
  // primary one.
  for (size_t before = 0u; before < 3u; ++before) {
    UpdateDisplay("800x600,800x600,800x600");
    Shell::SetRootWindowForNewWindows(Shell::GetAllRootWindows()[before]);
    display_manager()->SetUnifiedDesktopEnabled(true);
    EXPECT_EQ(Shell::GetPrimaryRootWindow(),
              Shell::GetRootWindowForNewWindows());
    display_manager()->SetUnifiedDesktopEnabled(false);
  }
}

// Test that exiting mirror mode in tablet mode with a fullscreen window
// does not cause a crash (crbug.com/1021662).
TEST_F(DisplayManagerTest, ExitMirrorModeInTabletMode) {
  // Simulate a tablet with and external display connected.
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  UpdateDisplay("800x600,800x600");
  ash::TabletModeControllerTestApi().EnterTabletMode();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());

  // Create a window to force in-app shelf.
  std::unique_ptr<aura::Window> window = CreateTestWindow();

  // Exit mirror mode.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(display_manager()->IsInSoftwareMirrorMode());
}

TEST_F(DisplayManagerTest, DifferentDisplayConnectedToSameOutput) {
  // Emulate connect a display to the same output. This can happen when
  // a display is swapped while the device is in sleep mode.
  auto display_manager_test =
      display::test::DisplayManagerTestApi(display_manager());
  display_manager_test.SetFirstDisplayAsInternalDisplay();
  const auto internal_display_info = display_manager()->GetDisplayInfo(
      *display::GetInternalDisplayIds().begin());

  constexpr int64_t kExternalId_1 = 210000010;
  constexpr int64_t kExternalId_2 = 220000010;

  const auto external_info_1 =
      display::ManagedDisplayInfo::CreateFromSpecWithID("401+0-400x300",
                                                        kExternalId_1);
  const auto external_info_2 =
      display::ManagedDisplayInfo::CreateFromSpecWithID("401+0-500x400",
                                                        kExternalId_2);

  const auto second_external_info =
      display::ManagedDisplayInfo::CreateFromSpecWithID("0+601-800x600",
                                                        230000011);

  display_manager()->OnNativeDisplaysChanged(
      vector<display::ManagedDisplayInfo>{
          internal_display_info, external_info_1, second_external_info});

  auto* screen = display::Screen::GetScreen();
  EXPECT_EQ(3u, screen->GetAllDisplays().size());
  EXPECT_EQ(kExternalId_1, screen->GetAllDisplays()[1].id());

  reset();

  display_manager()->OnNativeDisplaysChanged(
      vector<display::ManagedDisplayInfo>{
          internal_display_info, external_info_2, second_external_info});
  // There should be 1 display change, 1 removal, and 1 add.
  EXPECT_EQ("c1 a1 r1 w1 d1", GetCountSummary());

  EXPECT_EQ(3u, screen->GetAllDisplays().size());
  EXPECT_EQ(kExternalId_2, screen->GetAllDisplays()[1].id());
}

// Regression test for crbug.com/327282654. This asserts that DisplayManager
// changes that occur during change notification propagation are sequenced
// correctly to all observers.
TEST_F(DisplayManagerTest, DisplayManagerObserverNestedChangesOrdering) {
  // Assert that observers receive display configuration notifications in order.
  class ChangeOrderingObserver : public display::DisplayManagerObserver {
   public:
    explicit ChangeOrderingObserver(
        base::OnceClosure on_processed_cb = base::NullCallback())
        : on_processed_cb_(std::move(on_processed_cb)) {
      auto* display_manager = Shell::Get()->display_manager();
      EXPECT_EQ(1u, display_manager->GetNumDisplays());
      tracked_display_ids_.insert(display_manager->GetDisplayAt(0).id());
    }

    // display::DisplayManagerObserver:
    void OnWillProcessDisplayChanges() override {
      EXPECT_EQ(0u, will_process_count_);
      will_process_count_++;
    }
    void OnDidProcessDisplayChanges(
        const DisplayConfigurationChange& configuration_change) override {
      EXPECT_EQ(1u, will_process_count_);
      will_process_count_ = 0;

      // Update the set of tracked display ids communicated through did
      // process changes.
      base::ranges::transform(
          configuration_change.added_displays,
          std::inserter(tracked_display_ids_, tracked_display_ids_.begin()),
          [](const display::Display& display) { return display.id(); });

      // If correctly ordered observers should be notified of added displays
      // before any changes to the metrics for these displays.
      base::ranges::for_each(configuration_change.display_metrics_changes,
                             [this](const auto& change) {
                               EXPECT_TRUE(base::Contains(
                                   tracked_display_ids_, change.display->id()));
                             });

      if (on_processed_cb_) {
        std::move(on_processed_cb_).Run();
      }
    }

   private:
    size_t will_process_count_ = 0;
    base::OnceClosure on_processed_cb_;
    std::set<int64_t> tracked_display_ids_;
  };

  // Add a second display and configure the first observer to update the insets
  // of the second display when a did process event for the display addition
  // is received. The events should be received by the observers in the expected
  // order.
  ChangeOrderingObserver observer_1(base::BindOnce([]() {
    auto* display_manager = Shell::Get()->display_manager();
    EXPECT_EQ(2u, display_manager->GetNumDisplays());
    const int64_t second_display_id = display_manager->GetDisplayAt(1).id();
    display_manager->SetOverscanInsets(second_display_id,
                                       gfx::Insets::TLBR(13, 12, 11, 10));
  }));
  ChangeOrderingObserver observer_2;
  display_manager()->AddDisplayManagerObserver(&observer_1);
  display_manager()->AddDisplayManagerObserver(&observer_2);
  UpdateDisplay("800x600,800x600");
}

// Tests adding and removing displays via the VirtualDisplayUtil interface.
// This test should roughly match other platforms, i.e.
// TODO(crbug.com/40271794): Consolidate testing of VirtualDisplayUtil.
TEST_F(DisplayManagerTest, VirtualDisplayUtilAddRemove) {
  const display::Screen* screen = display::Screen::GetScreen();
  std::unique_ptr<display::test::VirtualDisplayUtil> virtual_display_util =
      std::make_unique<display::test::DisplayManagerTestApi>(display_manager());
  int64_t display_id[3];
  int initial_display_count = screen->GetNumDisplays();
  display_id[0] = virtual_display_util->AddDisplay(
      display::test::VirtualDisplayUtil::k1920x1080);
  EXPECT_NE(display_id[0], display::kInvalidDisplayId);
  EXPECT_EQ(screen->GetNumDisplays(), initial_display_count + 1);
  display::Display d;
  EXPECT_TRUE(screen->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  display_id[1] = virtual_display_util->AddDisplay(
      display::test::VirtualDisplayUtil::k1024x768);
  EXPECT_NE(display_id[1], display::kInvalidDisplayId);
  EXPECT_EQ(screen->GetNumDisplays(), initial_display_count + 2);
  EXPECT_TRUE(screen->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_EQ(d.size(), gfx::Size(1024, 768));

  display_id[2] = virtual_display_util->AddDisplay(
      display::test::VirtualDisplayUtil::k1920x1080);
  EXPECT_NE(display_id[2], display::kInvalidDisplayId);
  EXPECT_EQ(screen->GetNumDisplays(), initial_display_count + 3);
  EXPECT_TRUE(screen->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  virtual_display_util->RemoveDisplay(display_id[1]);
  EXPECT_EQ(screen->GetNumDisplays(), initial_display_count + 2);
  // Only virtual display 2 should no longer exist.
  EXPECT_TRUE(screen->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));
  EXPECT_FALSE(screen->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_TRUE(screen->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(d.size(), gfx::Size(1920, 1080));

  virtual_display_util->ResetDisplays();
  EXPECT_FALSE(screen->GetDisplayWithDisplayId(display_id[0], &d));
  EXPECT_FALSE(screen->GetDisplayWithDisplayId(display_id[1], &d));
  EXPECT_FALSE(screen->GetDisplayWithDisplayId(display_id[2], &d));
  EXPECT_EQ(screen->GetNumDisplays(), initial_display_count);
}

}  // namespace ash
