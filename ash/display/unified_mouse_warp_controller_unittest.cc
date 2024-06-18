// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/unified_mouse_warp_controller.h"

#include <sstream>

#include "ash/display/display_util.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/mouse_cursor_event_filter.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/display_finder.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/events/test/event_generator.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

struct WarpGroup {
  // Native point at a warp edge before warping.
  gfx::Point native_point_at_edge;

  // Expected DIP point after warping.
  gfx::Point expected_point_after_warp;

  // Expected display ID after warping.
  int64_t expected_target_display_id;
};

}  // namespace

class UnifiedMouseWarpControllerTest : public AshTestBase {
 public:
  UnifiedMouseWarpControllerTest() = default;

  UnifiedMouseWarpControllerTest(const UnifiedMouseWarpControllerTest&) =
      delete;
  UnifiedMouseWarpControllerTest& operator=(
      const UnifiedMouseWarpControllerTest&) = delete;

  ~UnifiedMouseWarpControllerTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    display_manager()->SetUnifiedDesktopEnabled(true);
  }

 protected:
  bool MoveMouseToNativePoint(const gfx::Point& point_in_native,
                              int64_t* out_original_mirroring_display_id) {
    for (auto display : display_manager()->software_mirroring_display_list()) {
      display::ManagedDisplayInfo info =
          display_manager()->GetDisplayInfo(display.id());
      if (info.bounds_in_native().Contains(point_in_native)) {
        *out_original_mirroring_display_id = info.id();
        gfx::Point point_in_mirroring_host = point_in_native;
        const gfx::Point& origin = info.bounds_in_native().origin();
        // Convert to mirroring host.
        point_in_mirroring_host.Offset(-origin.x(), -origin.y());

        // Move the mouse inside the host.
        AshWindowTreeHost* ash_host =
            Shell::Get()
                ->window_tree_host_manager()
                ->mirror_window_controller()
                ->GetAshWindowTreeHostForDisplayId(info.id());
        ui::test::EventGenerator gen(ash_host->AsWindowTreeHost()->window());
        gen.MoveMouseToWithNative(point_in_mirroring_host,
                                  point_in_mirroring_host);
        return true;
      }
    }
    return false;
  }

  bool TestIfMouseWarpsAt(const gfx::Point& point_in_native) {
    static_cast<UnifiedMouseWarpController*>(
        Shell::Get()->mouse_cursor_filter()->mouse_warp_controller_for_test())
        ->update_location_for_test();
    int64_t orig_mirroring_display_id;
    if (!MoveMouseToNativePoint(point_in_native, &orig_mirroring_display_id))
      return false;

    aura::Window* root = Shell::GetPrimaryRootWindow();
    gfx::Point new_location_in_unified_host =
        aura::Env::GetInstance()->last_mouse_location();
    // Convert screen to the host.
    root->GetHost()->ConvertDIPToPixels(&new_location_in_unified_host);

    auto iter = display::FindDisplayContainingPoint(
        display_manager()->software_mirroring_display_list(),
        new_location_in_unified_host);
    if (iter == display_manager()->software_mirroring_display_list().end())
      return false;
    return orig_mirroring_display_id != iter->id();
  }

  MouseCursorEventFilter* event_filter() {
    return Shell::Get()->mouse_cursor_filter();
  }

  UnifiedMouseWarpController* mouse_warp_controller() {
    return static_cast<UnifiedMouseWarpController*>(
        event_filter()->mouse_warp_controller_for_test());
  }

  // |expected_edges| should have a row for each display which contains the
  // expected native bounds of the shared edges with that display in the order
  // "top", "left", "right", "bottom".
  // If |matrix| is empty, default unified layout will be used.
  void BoundaryTestBody(
      const std::string& displays_specs,
      const display::UnifiedDesktopLayoutMatrix& matrix,
      const std::vector<std::vector<std::string>>& expected_edges) {
    UpdateDisplay(displays_specs);
    display_manager()->SetUnifiedDesktopMatrix(matrix);

    // Let the UnifiedMouseWarpController compute the bounds by
    // generating a mouse move event.
    GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));
    const display::Displays& mirroring_displays =
        display_manager()->software_mirroring_display_list();

    ASSERT_EQ(expected_edges.size(), mirroring_displays.size());
    int index = 0;
    for (const auto& display : mirroring_displays) {
      const int64_t id = display.id();
      std::stringstream scoped_trace_message;
      scoped_trace_message << "Edges of display with ID: " << id
                           << " at index: " << index;
      SCOPED_TRACE(scoped_trace_message.str());
      const auto& display_expected_edges = expected_edges[index++];
      const auto& display_actual_edges =
          mouse_warp_controller()->displays_edges_map_.at(id);
      ASSERT_EQ(display_expected_edges.size(), display_actual_edges.size());
      for (size_t i = 0; i < display_expected_edges.size(); ++i) {
        EXPECT_EQ(display_expected_edges[i],
                  display_actual_edges[i]
                      .edge_native_bounds_in_source_display.ToString());
      }
    }
  }

  void WarpTestBody(const std::vector<WarpGroup>& warp_groups) {
    for (const auto& group : warp_groups) {
      EXPECT_TRUE(TestIfMouseWarpsAt(group.native_point_at_edge));

      gfx::Point new_location = aura::Env::GetInstance()->last_mouse_location();
      EXPECT_EQ(group.expected_point_after_warp, new_location);

      // Convert screen to the host.
      aura::Window* root = Shell::GetPrimaryRootWindow();
      root->GetHost()->ConvertDIPToPixels(&new_location);

      auto iter = display::FindDisplayContainingPoint(
          display_manager()->software_mirroring_display_list(), new_location);
      EXPECT_FALSE(iter ==
                   display_manager()->software_mirroring_display_list().end());
      EXPECT_EQ(group.expected_target_display_id, iter->id());
    }
  }

  void NoWarpTestBody() {
    // Touch the left edge of the first display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(0, 10)));
    // Touch the top edge of the first display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 0)));
    // Touch the bottom edge of the first display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 499)));

    // Touch the right edge of the second display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(1099, 10)));
    // Touch the top edge of the second display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(610, 0)));
    // Touch the bottom edge of the second display.
    EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(610, 499)));
  }
};

// Verifies if MouseCursorEventFilter's bounds calculation works correctly.
TEST_F(UnifiedMouseWarpControllerTest, BoundaryTest) {
  {
    SCOPED_TRACE("1x1");
    BoundaryTestBody("500x400,0+450-700x400",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x400"}});
    BoundaryTestBody("500x400,0+450-700x600",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x600"}});
  }
  {
    SCOPED_TRACE("2x1");
    BoundaryTestBody("500x400*2,0+450-700x400",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x400"}});
    BoundaryTestBody("500x400*2,0+450-700x600",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x600"}});
  }
  {
    SCOPED_TRACE("1x2");
    BoundaryTestBody("500x400,0+450-700x400*2",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x400"}});
    BoundaryTestBody("500x400,0+450-700x600*2",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x600"}});
  }
  {
    SCOPED_TRACE("2x2");
    BoundaryTestBody("500x400*2,0+450-700x400*2",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x400"}});
    BoundaryTestBody("500x400*2,0+450-700x600*2",
                     {},  // Empty matrix (use horizontal layout).
                     {{"499,0 1x400"}, {"0,450 1x600"}});
  }
}

TEST_F(UnifiedMouseWarpControllerTest, BoundaryAndWarpSimpleTest) {
  const std::vector<std::vector<std::string>> expected_edges = {
      // Display 0 edges.
      {
          "1919,0 1x1080",  // Right with display 1.
      },
      // Display 1 edges.
      {
          "1930,0 1x1200",  // Left with display 0.
      },
  };

  BoundaryTestBody("0+0-1920x1080,1930+0-1920x1200",
                   {} /* empty matrix = default */, expected_edges);

  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(2u, list.size());

  // Assert mouse warps in all bounds to the correct display.
  const std::vector<WarpGroup> warp_groups = {
      {{1919, 500}, {1920, 499}, list[1]},  // Display 0 --> 1.
      {{1930, 600}, {1918, 540}, list[0]},  // Display 1 --> 0.
  };
  WarpTestBody(warp_groups);
}

TEST_F(UnifiedMouseWarpControllerTest, BoundaryTestGrid) {
  // Update displays here first so we get the correct display IDs list. The
  // below are the native bounds.
  const std::string display_specs =
      "0+0-500x300,510+0-400x500,920+0-300x600,"
      "0+600-200x300,210+600-700x200,920+600-350x480,"
      "0+1080-300x500,310+1080-600x599,920+1080-400x450";
  UpdateDisplay(display_specs);
  display_manager()->SetUnifiedDesktopEnabled(true);
  display::DisplayIdList list = display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(9u, list.size());

  // Test a very general case of a 3 x 3 matrix.
  // 0:[500 x 300] 1:[400 x 500] 2:[300 x 600]
  // 3:[200 x 300] 4:[700 x 200] 5:[350 x 480]
  // 6:[300 x 500] 7:[600 x 599] 8:[400 x 450]
  display::UnifiedDesktopLayoutMatrix matrix;
  matrix.resize(3u);
  matrix[0].emplace_back(list[0]);
  matrix[0].emplace_back(list[1]);
  matrix[0].emplace_back(list[2]);
  matrix[1].emplace_back(list[3]);
  matrix[1].emplace_back(list[4]);
  matrix[1].emplace_back(list[5]);
  matrix[2].emplace_back(list[6]);
  matrix[2].emplace_back(list[7]);
  matrix[2].emplace_back(list[8]);

  const std::vector<std::vector<std::string>> expected_edges = {
      // Display 0 edges.
      {
          "499,0 1x300",    // Right with display 1.
          "0,299 121x1",    // Bottom with display 3.
          "121,299 379x1",  // Bottom with display 4.
      },
      // Display 1 edges.
      {
          "510,0 1x500",    // Left with display 0.
          "909,0 1x500",    // Right with display 2.
          "510,499 400x1",  // Bottom with display 4.
      },
      // Display 2 edges.
      {
          "920,0 1x600",    // Left with display 1.
          "920,599 34x1",   // Bottom with display 4.
          "954,599 266x1",  // Bottom with display 5.
      },
      // Display 3 edges.
      {
          "0,600 199x1",    // Top with display 0.
          "199,600 1x300",  // Right with display 4.
          "0,899 199x1",    // Bottom with display 6.
      },
      // Display 4 edges.
      {
          "210,600 416x1",  // Top with display 0.
          "626,600 264x1",  // Top with display 1.
          "890,600 18x1",   // Top with display 2.
          "210,600 1x200",  // Left with display 3.
          "909,600 1x200",  // Right with display 5.
          "210,799 102x1",  // Bottom with display 6.
          "312,799 393x1",  // Bottom with display 7.
          "705,799 203x1",  // Bottom with display 8.
      },
      // Display 5 edges.
      {
          "920,600 350x1",   // Top with display 2.
          "920,600 1x480",   // Left with display 4.
          "920,1079 350x1",  // Bottom with display 8.
      },
      // Display 6 edges.
      {
          "0,1080 169x1",    // Top with display 3.
          "169,1080 130x1",  // Top with display 4.
          "299,1080 1x500",  // Right with display 7.
      },
      // Display 7 edges.
      {
          "310,1080 600x1",  // Top with display 4.
          "310,1080 1x599",  // Left with display 6.
          "909,1080 1x599",  // Right with display 8.
      },
      // Display 8 edges.
      {
          "920,1080 233x1",   // Top with display 4.
          "1153,1080 167x1",  // Top with display 5.
          "920,1080 1x450",   // Left with display 7.
      },
  };

  BoundaryTestBody(display_specs, matrix, expected_edges);

  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  // Assert mouse warps in all bounds to the correct display.
  const std::vector<WarpGroup> warp_groups = {
      {{499, 10}, {500, 9}, list[1]},     // Display 0 --> 1.
      {{10, 299}, {9, 300}, list[3]},     // Display 0 --> 3.
      {{130, 299}, {129, 300}, list[4]},  // Display 0 --> 4.

      {{510, 10}, {498, 6}, list[0]},     // Display 1 --> 0.
      {{909, 50}, {740, 30}, list[2]},    // Display 1 --> 2.
      {{600, 499}, {553, 300}, list[4]},  // Display 1 --> 4.

      {{920, 50}, {738, 24}, list[1]},    // Display 2 --> 1.
      {{930, 599}, {744, 300}, list[4]},  // Display 2 --> 4.
      {{970, 599}, {764, 300}, list[5]},  // Display 2 --> 5.

      {{10, 600}, {6, 298}, list[0]},     // Display 3 --> 0.
      {{199, 700}, {121, 359}, list[4]},  // Display 3 --> 4.
      {{100, 899}, {59, 482}, list[6]},   // Display 3 --> 6.

      {{250, 600}, {157, 298}, list[0]},  // Display 4 --> 0.
      {{700, 600}, {566, 298}, list[1]},  // Display 4 --> 1.
      {{900, 600}, {748, 299}, list[2]},  // Display 4 --> 2.
      {{210, 700}, {120, 391}, list[3]},  // Display 4 --> 3.
      {{909, 650}, {757, 344}, list[5]},  // Display 4 --> 5.
      {{250, 799}, {156, 482}, list[6]},  // Display 4 --> 6.
      {{500, 799}, {383, 482}, list[7]},  // Display 4 --> 7.
      {{800, 799}, {656, 482}, list[8]},  // Display 4 --> 8.

      {{950, 600}, {768, 299}, list[2]},    // Display 5 --> 2.
      {{920, 750}, {756, 355}, list[4]},    // Display 5 --> 4.
      {{1000, 1079}, {786, 482}, list[8]},  // Display 5 --> 8.

      {{100, 1080}, {70, 480}, list[3]},   // Display 6 --> 3.
      {{200, 1080}, {141, 480}, list[4]},  // Display 6 --> 4.
      {{299, 1200}, {214, 566}, list[7]},  // Display 6 --> 7.

      {{500, 1080}, {326, 480}, list[4]},  // Display 7 --> 4.
      {{310, 1500}, {212, 731}, list[6]},  // Display 7 --> 6.
      {{909, 1500}, {572, 731}, list[8]},  // Display 7 --> 8.

      {{1000, 1080}, {634, 480}, list[4]},  // Display 8 --> 4.
      {{1200, 1080}, {793, 481}, list[5]},  // Display 8 --> 5.
      {{920, 1500}, {570, 814}, list[7]},   // Display 8 --> 7.
  };
  WarpTestBody(warp_groups);
}

// Verifies if the mouse pointer correctly moves to another display in
// unified desktop mode.
TEST_F(UnifiedMouseWarpControllerTest, WarpMouse) {
  UpdateDisplay("600x500,700+0-600x500");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(599, 10)));
  EXPECT_EQ("601,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(700, 10)));
  EXPECT_EQ("598,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
  {
    SCOPED_TRACE("1x1 NO WARP");
    NoWarpTestBody();
  }

  // With 2X and 1X displays
  UpdateDisplay("600x500*2,700+0-600x500");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(599, 10)));
  EXPECT_EQ("300,5",  // moved to 601 by 2px, divided by 2 (dsf).
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(700, 10)));
  EXPECT_EQ("299,5",  // moved to 598 by 2px, divided by 2 (dsf).
            aura::Env::GetInstance()->last_mouse_location().ToString());

  {
    SCOPED_TRACE("2x1 NO WARP");
    NoWarpTestBody();
  }

  // With 1X and 2X displays
  UpdateDisplay("600x500,700+0-600x500*2");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(599, 10)));
  EXPECT_EQ("601,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(700, 10)));
  EXPECT_EQ("598,10",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());
  {
    SCOPED_TRACE("1x2 NO WARP");
    NoWarpTestBody();
  }

  // With two 2X displays
  UpdateDisplay("600x500*2,700+0-600x500*2");
  ASSERT_EQ(1, display::Screen::GetScreen()->GetNumDisplays());

  EXPECT_FALSE(TestIfMouseWarpsAt(gfx::Point(10, 10)));
  // Touch the right edge of the first display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(599, 10)));
  EXPECT_EQ("300,5",  // by 2px.
            aura::Env::GetInstance()->last_mouse_location().ToString());

  // Touch the left edge of the second display. Pointer should warp.
  EXPECT_TRUE(TestIfMouseWarpsAt(gfx::Point(700, 10)));
  EXPECT_EQ("299,5",  // moved to 598 by 2px, divided by 2 (dsf).
            aura::Env::GetInstance()->last_mouse_location().ToString());
  {
    SCOPED_TRACE("1x2 NO WARP");
    NoWarpTestBody();
  }
}

}  // namespace aura
