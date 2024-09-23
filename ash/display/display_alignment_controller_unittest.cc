// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_alignment_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/display/display_alignment_indicator.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/test/scoped_feature_list.h"
#include "base/timer/mock_timer.h"
#include "ui/display/display_layout_builder.h"
#include "ui/display/manager/display_layout_store.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

enum class EdgeType { kTop, kRight, kBottom, kLeft };

DisplayAlignmentController* display_alignment_controller() {
  return Shell::Get()->display_alignment_controller();
}

void TriggerIndicator(const display::Display& display, EdgeType edge) {
  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();
  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(display.id());

  ui::test::EventGenerator primary_generator(primary_root);

  const gfx::Rect display_bounds = primary_root->GetBoundsInRootWindow();

  gfx::Point point_on_edge;
  if (edge == EdgeType::kTop)
    point_on_edge = display_bounds.top_center();
  else if (edge == EdgeType::kRight)
    point_on_edge = display_bounds.right_center();
  else if (edge == EdgeType::kBottom)
    point_on_edge = display_bounds.bottom_center();
  else
    point_on_edge = display_bounds.left_center();

  primary_generator.MoveMouseToInHost(point_on_edge);
  primary_generator.MoveMouseToInHost(display_bounds.CenterPoint());
  primary_generator.MoveMouseToInHost(point_on_edge);
}

}  // namespace

class DisplayAlignmentControllerTest : public AshTestBase {
 public:
  DisplayAlignmentControllerTest() = default;
  ~DisplayAlignmentControllerTest() override = default;

 protected:
  void LockScreen() { GetSessionControllerClient()->LockScreen(); }
  void UnlockScreen() { GetSessionControllerClient()->UnlockScreen(); }

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kDisplayAlignAssist);

    AshTestBase::SetUp();

    std::unique_ptr<base::MockOneShotTimer> mock_timer =
        std::make_unique<base::MockOneShotTimer>();

    mock_timer_ptr_ = mock_timer.get();

    display_alignment_controller()->SetTimerForTesting(std::move(mock_timer));
  }

  void DragDisplay(int64_t id, int32_t delta_x, int32_t delta_y) {
    display_alignment_controller()->DisplayDragged(id, delta_x, delta_y);
  }

  bool NoIndicatorsExist() {
    return display_alignment_controller()
        ->GetActiveIndicatorsForTesting()
        .empty();
  }

  void CheckIndicatorShown(size_t num_indicators,
                           const display::Display& src_display) {
    const auto& active_indicators =
        display_alignment_controller()->GetActiveIndicatorsForTesting();

    WindowTreeHostManager* window_tree_host_manager =
        Shell::Get()->window_tree_host_manager();
    aura::Window* primary_root =
        window_tree_host_manager->GetRootWindowForDisplayId(src_display.id());

    EXPECT_EQ(num_indicators, active_indicators.size());

    for (const auto& indicator : active_indicators) {
      ASSERT_TRUE(indicator);

      EXPECT_TRUE(indicator->indicator_widget_.IsVisible());

      aura::Window* current_root =
          indicator->indicator_widget_.GetNativeWindow()->GetRootWindow();
      if (current_root == primary_root) {
        ASSERT_TRUE(indicator->pill_widget_);
        EXPECT_TRUE(indicator->pill_widget_->IsVisible());
      } else {
        EXPECT_FALSE(indicator->pill_widget_);
      }
    }
  }

  void CheckPreviewIndicatorShown(int64_t dragged_display_id,
                                  int64_t target_display_id,
                                  bool is_visible) {
    ASSERT_EQ(dragged_display_id,
              display_alignment_controller()->GetDraggedDisplayIdForTesting());

    const auto& active_indicators_ =
        display_alignment_controller()->GetActiveIndicatorsForTesting();

    const auto& iter =
        base::ranges::find(active_indicators_, target_display_id,
                           &DisplayAlignmentIndicator::display_id);

    if (iter == active_indicators_.end()) {
      EXPECT_FALSE(is_visible);
      return;
    }

    DisplayAlignmentIndicator* indicator = iter->get();
    ASSERT_TRUE(indicator);

    const views::Widget& indicator_widget = indicator->indicator_widget_;

    if (!is_visible) {
      EXPECT_FALSE(indicator_widget.IsVisible());
      return;
    }

    EXPECT_EQ(Shell::GetRootWindowForDisplayId(target_display_id),
              indicator_widget.GetNativeWindow()->GetRootWindow());
    EXPECT_TRUE(indicator_widget.IsVisible());
  }

  raw_ptr<base::MockOneShotTimer, DanglingUntriaged> mock_timer_ptr_ = nullptr;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DisplayAlignmentControllerTest, SingleDisplayNoIndicators) {
  UpdateDisplay("1920x1080");

  EXPECT_TRUE(NoIndicatorsExist());
}

TEST_F(DisplayAlignmentControllerTest, TriggerIndicatorPrimary) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& display = display_manager()->GetDisplayAt(0);

  aura::Window* root_window =
      Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
          display.id());

  ui::test::EventGenerator generator(root_window);

  // Move mouse on to an edge.
  generator.MoveMouseToInHost(gfx::Point(0, 0));
  EXPECT_TRUE(NoIndicatorsExist());

  // Move mouse off the edge.
  generator.MoveMouseToInHost(gfx::Point(20, 50));
  EXPECT_TRUE(NoIndicatorsExist());

  // Move mouse on to an edge for the second time.
  generator.MoveMouseToInHost(gfx::Point(0, 0));

  CheckIndicatorShown(2, display);

  // Finish displaying indicators.
  mock_timer_ptr_->Fire();
  EXPECT_TRUE(NoIndicatorsExist());
}

TEST_F(DisplayAlignmentControllerTest, RetriggerIndicatorPrimary) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& display = display_manager()->GetDisplayAt(0);

  EXPECT_TRUE(NoIndicatorsExist());

  TriggerIndicator(display, EdgeType::kTop);

  CheckIndicatorShown(2, display);

  // Finish displaying indicators.
  mock_timer_ptr_->Fire();

  EXPECT_TRUE(NoIndicatorsExist());

  // Re-trigger indicators.
  TriggerIndicator(display, EdgeType::kTop);

  CheckIndicatorShown(2, display);
}

TEST_F(DisplayAlignmentControllerTest, OnlyTriggerNeighboringIndicators) {
  UpdateDisplay("1920x1080,1366x768,800x600");

  const auto& display = display_manager()->GetDisplayAt(0);

  TriggerIndicator(display, EdgeType::kTop);

  CheckIndicatorShown(2, display);
}

TEST_F(DisplayAlignmentControllerTest, TriggerSecondaryDisplay) {
  UpdateDisplay("1920x1080,1366x768,800x600");

  const auto& display = display_manager()->GetDisplayAt(1);

  TriggerIndicator(display, EdgeType::kTop);

  CheckIndicatorShown(4, display);
}

TEST_F(DisplayAlignmentControllerTest, TriggerTwoDisplayOnSameEdge) {
  //
  //    +---------+---------+
  //    |    1    |    2    |
  //    |         |         |
  //    |         |         |
  //    |         |         |
  //    +-+-------+---------+-+
  //      |         P         |
  //      |                   |
  //      |                   |
  //      |                   |
  //      +-------------------+
  //

  int64_t primary_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayIdList list =
      display::test::CreateDisplayIdListN(primary_id, 3);
  display::DisplayLayoutBuilder builder(primary_id);
  builder.AddDisplayPlacement(list[1], primary_id,
                              display::DisplayPlacement::TOP, -110);
  builder.AddDisplayPlacement(list[2], primary_id,
                              display::DisplayPlacement::TOP, 490);
  display_manager()->layout_store()->RegisterLayoutForDisplayIdList(
      list, builder.Build());

  UpdateDisplay("1200x500,600x500,600x500");

  const auto& display = display_manager()->GetDisplayAt(1);

  TriggerIndicator(display, EdgeType::kTop);

  CheckIndicatorShown(4, display);
}

TEST_F(DisplayAlignmentControllerTest, DontTriggerIndicator) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& display = display_manager()->GetDisplayAt(0);

  aura::Window* root_window =
      Shell::Get()->window_tree_host_manager()->GetRootWindowForDisplayId(
          display.id());

  ui::test::EventGenerator generator(root_window);

  // Move the mouse on to the edge once.
  generator.MoveMouseToInHost(gfx::Point(0, 0));
  generator.MoveMouseToInHost(gfx::Point(20, 50));

  EXPECT_TRUE(NoIndicatorsExist());
  mock_timer_ptr_->Fire();
  EXPECT_TRUE(NoIndicatorsExist());
}

TEST_F(DisplayAlignmentControllerTest, DontTriggerIndicatorDifferentDisplays) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);
  const auto& secondary_display = display_manager()->GetDisplayAt(1);

  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display.id());
  aura::Window* secondary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(
          secondary_display.id());

  ui::test::EventGenerator primary_generator(primary_root);
  ui::test::EventGenerator secondary_generator(secondary_root);

  // Simulate hitting the edge of the first display.
  primary_generator.MoveMouseToInHost(gfx::Point(0, 0));
  primary_generator.MoveMouseToInHost(gfx::Point(20, 20));

  // Hitting the edge of the second display should not trigger alignment
  // indicators.
  secondary_generator.MoveMouseToInHost(gfx::Point(1365, 0));

  EXPECT_TRUE(NoIndicatorsExist());
}

TEST_F(DisplayAlignmentControllerTest, RemoveDisplay) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);

  TriggerIndicator(primary_display, EdgeType::kLeft);

  CheckIndicatorShown(2, primary_display);

  UpdateDisplay("1920x1080");

  EXPECT_TRUE(NoIndicatorsExist());

  TriggerIndicator(primary_display, EdgeType::kTop);

  EXPECT_TRUE(NoIndicatorsExist());
}

TEST_F(DisplayAlignmentControllerTest, LockScreen) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);

  TriggerIndicator(primary_display, EdgeType::kBottom);

  CheckIndicatorShown(2, primary_display);

  LockScreen();

  EXPECT_TRUE(NoIndicatorsExist());

  TriggerIndicator(primary_display, EdgeType::kTop);

  EXPECT_TRUE(NoIndicatorsExist());

  UnlockScreen();

  TriggerIndicator(primary_display, EdgeType::kTop);

  CheckIndicatorShown(2, primary_display);
}

TEST_F(DisplayAlignmentControllerTest, ChangeResolution) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);

  TriggerIndicator(primary_display, EdgeType::kTop);

  CheckIndicatorShown(2, primary_display);

  UpdateDisplay("2560x1440,1366x768");

  // Resolution change immediately hides the indicator.
  EXPECT_TRUE(NoIndicatorsExist());

  TriggerIndicator(primary_display, EdgeType::kRight);

  CheckIndicatorShown(2, primary_display);
}

TEST_F(DisplayAlignmentControllerTest, AllowOffByOnes) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);

  WindowTreeHostManager* window_tree_host_manager =
      Shell::Get()->window_tree_host_manager();

  aura::Window* primary_root =
      window_tree_host_manager->GetRootWindowForDisplayId(primary_display.id());

  ui::test::EventGenerator primary_generator(primary_root);

  // (1, 1) is one off of both top and left edge.
  primary_generator.MoveMouseToInHost(gfx::Point(1, 1));
  primary_generator.MoveMouseToInHost(gfx::Point(20, 20));
  primary_generator.MoveMouseToInHost(gfx::Point(1, 1));

  CheckIndicatorShown(2, primary_display);
}

TEST_F(DisplayAlignmentControllerTest, DragDisplayBasic) {
  UpdateDisplay("1920x1080,1366x768");
  const auto& primary_display = display_manager()->GetDisplayAt(0);
  const auto& secondary_display = display_manager()->GetDisplayAt(1);

  DragDisplay(primary_display.id(), 0, 0);

  CheckPreviewIndicatorShown(primary_display.id(), secondary_display.id(),
                             /*is_visible=*/true);

  UpdateDisplay("1920x1080,1366x768");
  EXPECT_TRUE(NoIndicatorsExist());
}

// When drag display starts, all existing indicators should be replaced.
TEST_F(DisplayAlignmentControllerTest, DragDisplayReplaceExistingIndicators) {
  UpdateDisplay("1920x1080,1366x768");

  const auto& primary_display = display_manager()->GetDisplayAt(0);
  const auto& secondary_display = display_manager()->GetDisplayAt(1);

  TriggerIndicator(primary_display, EdgeType::kLeft);

  CheckIndicatorShown(2, primary_display);

  DragDisplay(primary_display.id(), 0, 10);

  CheckPreviewIndicatorShown(primary_display.id(), secondary_display.id(),
                             /*is_visible=*/true);
}

TEST_F(DisplayAlignmentControllerTest, DragDisplayHideOldNeighbors) {
  UpdateDisplay("1920x1080,1366x768");
  const auto& primary_display = display_manager()->GetDisplayAt(0);
  const auto& secondary_display = display_manager()->GetDisplayAt(1);

  DragDisplay(primary_display.id(), 0, 0);

  // Move the primary display so that the two are no longer neighbors
  DragDisplay(primary_display.id(), -2000, -2000);

  CheckPreviewIndicatorShown(primary_display.id(), secondary_display.id(),
                             /*is_visible=*/false);
}

TEST_F(DisplayAlignmentControllerTest, DragDisplayNewNeighbor) {
  UpdateDisplay("1000x900,1000x900,1000x100");
  const auto& display_1 = display_manager()->GetDisplayAt(0);
  const auto& display_2 = display_manager()->GetDisplayAt(1);
  const auto& display_3 = display_manager()->GetDisplayAt(2);

  DragDisplay(display_1.id(), 0, 0);

  // Move the primary display so that the other two are no longer neighbors
  DragDisplay(display_1.id(), 3000, 0);

  CheckPreviewIndicatorShown(display_1.id(), display_2.id(),
                             /*is_visible=*/false);

  CheckPreviewIndicatorShown(display_1.id(), display_3.id(),
                             /*is_visible=*/true);
}
}  // namespace ash
