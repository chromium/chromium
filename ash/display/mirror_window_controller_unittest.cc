// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/mirror_window_controller.h"

#include "ash/display/mirror_window_test_api.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/cursor_manager_test_api.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/display/display_switches.h"
#include "ui/display/display_transform.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/display/types/display_constants.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/overlay_transform.h"

namespace ash {

namespace {

display::ManagedDisplayInfo CreateDisplayInfo(int64_t id,
                                              const gfx::Rect& bounds,
                                              float scale = 1.f) {
  display::ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);
  // Each display should have at least one native mode.
  display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                   /*is_interlaced=*/true,
                                   /*native=*/true);
  info.SetManagedDisplayModes({mode});
  info.set_device_scale_factor(scale);
  return info;
}

class MirrorOnBootTest : public AshTestBase {
 public:
  MirrorOnBootTest() = default;

  MirrorOnBootTest(const MirrorOnBootTest&) = delete;
  MirrorOnBootTest& operator=(const MirrorOnBootTest&) = delete;

  ~MirrorOnBootTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "1+1-400x300,1+301-400x300");
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        ::switches::kEnableSoftwareMirroring);
    AshTestBase::SetUp();
  }
};

}  // namespace

using MirrorWindowControllerTest = AshTestBase;

// Make sure that the compositor based mirroring can switch
// from/to dock mode.
TEST_F(MirrorWindowControllerTest, DockMode) {
  const int64_t internal_id = 1;
  const int64_t external_id = 2;

  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(internal_id, gfx::Rect(0, 0, 400, 500));
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

  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());
  EXPECT_EQ(external_id,
            display_manager()->GetMirroringDestinationDisplayIdList()[0]);

  // dock mode.
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_FALSE(display_manager()->IsInMirrorMode());

  // back to software mirroring.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());
  EXPECT_TRUE(display_manager()->IsInMirrorMode());
  EXPECT_EQ(external_id,
            display_manager()->GetMirroringDestinationDisplayIdList()[0]);
}

TEST_F(MirrorOnBootTest, MirrorOnBoot) {
  EXPECT_TRUE(display_manager()->IsInMirrorMode());

  base::RunLoop().RunUntilIdle();
  MirrorWindowTestApi test_api;
  EXPECT_EQ(1U, test_api.GetHosts().size());
}

class MirrorWindowControllerRotationAndPanelOrientationTest
    : public MirrorWindowControllerTest,
      public testing::WithParamInterface<
          std::tuple<display::Display::Rotation, display::PanelOrientation>> {};

// Test that the mirror display matches the size and rotation of the source.
TEST_P(MirrorWindowControllerRotationAndPanelOrientationTest, MirrorSize) {
  const int64_t primary_id = 1;
  const int64_t mirror_id = 2;

  const display::Display::Rotation active_rotation = std::get<0>(GetParam());
  const display::PanelOrientation panel_orientation = std::get<1>(GetParam());

  // Run the test with and without display scaling.
  int scale_factors[] = {1, 2};
  for (int scale : scale_factors) {
    display::ManagedDisplayInfo primary_display_info =
        CreateDisplayInfo(primary_id, gfx::Rect(0, 0, 400, 300), scale);
    primary_display_info.set_panel_orientation(panel_orientation);
    primary_display_info.SetRotation(active_rotation,
                                     display::Display::RotationSource::ACTIVE);

    const display::ManagedDisplayInfo mirror_display_info =
        CreateDisplayInfo(mirror_id, gfx::Rect(400, 0, 600, 500), scale);
    std::vector<display::ManagedDisplayInfo> display_info_list = {
        primary_display_info, mirror_display_info};

    // Start software mirroring.
    display_manager()->OnNativeDisplaysChanged(display_info_list);
    display_manager()->SetMirrorMode(display::MirrorMode::kNormal,
                                     std::nullopt);
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1U, display_manager()->GetNumDisplays());
    EXPECT_TRUE(display_manager()->IsInSoftwareMirrorMode());

    // Check the size of the mirror window.
    const display::Display& primary_display =
        display_manager()->GetDisplayForId(primary_id);
    aura::Window* root_window = Shell::GetRootWindowForDisplayId(mirror_id);
    aura::Window* mirror_window = root_window->children()[0];
    EXPECT_EQ(primary_display.GetSizeInPixel(), root_window->bounds().size());
    EXPECT_EQ(primary_display.GetSizeInPixel(), mirror_window->bounds().size());

    // Mirror should have a display transform hint that matches the active
    // rotation (excluding the panel orientation) of the source.
    EXPECT_EQ(display::DisplayRotationToOverlayTransform(active_rotation),
              root_window->GetHost()->compositor()->display_transform_hint());
  }
}

INSTANTIATE_TEST_SUITE_P(
    All,
    MirrorWindowControllerRotationAndPanelOrientationTest,
    testing::Combine(testing::Values(display::Display::ROTATE_0,
                                     display::Display::ROTATE_90,
                                     display::Display::ROTATE_180,
                                     display::Display::ROTATE_270),
                     testing::Values(display::PanelOrientation::kNormal,
                                     display::PanelOrientation::kBottomUp,
                                     display::PanelOrientation::kLeftUp,
                                     display::PanelOrientation::kRightUp)));

}  // namespace ash
