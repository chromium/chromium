// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_drag_indicators.h"

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/work_area_insets.h"
#include "base/command_line.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/display/display_switches.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class SplitViewDragIndicatorsTest : public AshTestBase {
 public:
  SplitViewDragIndicatorsTest() = default;
  SplitViewDragIndicatorsTest(const SplitViewDragIndicatorsTest&) = delete;
  SplitViewDragIndicatorsTest& operator=(const SplitViewDragIndicatorsTest&) =
      delete;
  ~SplitViewDragIndicatorsTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFirstDisplayAsInternal);
    AshTestBase::SetUp();

    // Ensure calls to SetEnabledForTest complete.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    base::RunLoop().RunUntilIdle();
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        true);
  }
  void TearDown() override {
    ui::PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    AshTestBase::TearDown();
  }

  void ToggleOverview() {
    auto* overview_controller = Shell::Get()->overview_controller();
    if (overview_controller->InOverviewSession())
      ExitOverview();
    else
      EnterOverview();

    if (!overview_controller->InOverviewSession()) {
      overview_session_ = nullptr;
      return;
    }

    overview_session_ = Shell::Get()->overview_controller()->overview_session();
    ASSERT_TRUE(overview_session_);
  }

  const SplitViewDragIndicators* split_view_drag_indicators() const {
    return overview_session_->grid_list()[0]->split_view_drag_indicators();
  }

  SplitViewDragIndicators::WindowDraggingState window_dragging_state() const {
    CHECK(split_view_drag_indicators());
    return split_view_drag_indicators()->current_window_dragging_state();
  }

  bool IsPreviewAreaShowing() {
    return SplitViewDragIndicators::GetSnapPosition(window_dragging_state()) !=
           SnapPosition::kNone;
  }

  float GetEdgeInset(int screen_width) const {
    return screen_width * kHighlightScreenPrimaryAxisRatio +
           kHighlightScreenEdgePaddingDp;
  }

  // Creates a window which cannot be snapped by splitview.
  std::unique_ptr<aura::Window> CreateUnsnappableWindow() {
    std::unique_ptr<aura::Window> window(CreateTestWindow());
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        aura::client::kResizeBehaviorNone);
    return window;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_{
      chromeos::features::kOverviewSessionInitOptimizations};
  raw_ptr<OverviewSession, DanglingUntriaged> overview_session_ = nullptr;
};

TEST_F(SplitViewDragIndicatorsTest, DraggingBasic) {
  UpdateDisplay("800x600");

  base::HistogramTester histogram_tester;
  aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);

  std::unique_ptr<aura::Window> window = CreateAppWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();

  ToggleOverview();
  auto* overview_item = GetOverviewItemForWindow(window.get());

  // This is the amount the event needs to move before a drag starts.
  const int drag_offset = 5;

  // Verify if the drag is not started in either snap region, the drag still
  // must move by `drag_offset` before split view acknowledges the drag (ie.
  // starts moving the overview item).
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  const gfx::RectF original_bounds = overview_item->target_bounds();
  generator->MoveMouseBy(drag_offset - 1, 0);
  EXPECT_EQ(original_bounds, overview_item->target_bounds());
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 0);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 0);

  generator->MoveMouseBy(1, 0);
  EXPECT_NE(original_bounds, overview_item->target_bounds());
  EXPECT_FALSE(IsPreviewAreaShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 0);

  const int left_drag_area_x =
      800 * kHighlightScreenPrimaryAxisRatio + kHighlightScreenEdgePaddingDp;
  generator->MoveMouseTo(left_drag_area_x, 300);
  EXPECT_TRUE(IsPreviewAreaShowing());
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 2);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 0);

  generator->ReleaseLeftButton();
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 2);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 1);
}

TEST_F(SplitViewDragIndicatorsTest, DraggingStartingInPreviewArea) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();
  ui::test::EventGenerator* generator = GetEventGenerator();

  ToggleOverview();
  auto* left_item = GetOverviewItemForWindow(window2.get());
  auto* right_item = GetOverviewItemForWindow(window1.get());

  // This is the amount the event needs to move before a drag starts.
  const int drag_offset = 5;

  // The indicator drag areas.
  const int left_drag_area_x = GetEdgeInset(800);
  const int right_drag_area_x = 800 - left_drag_area_x;

  ASSERT_LT(left_item->target_bounds().x(), left_drag_area_x);
  ASSERT_GT(right_item->target_bounds().right(), right_drag_area_x);

  // Start dragging the left item. Even though we are in the left drag region,
  // the preview won't be shown since we started the drag there.
  generator->set_current_screen_location(gfx::ToRoundedPoint(
      left_item->target_bounds().left_center() + gfx::Vector2d(1, 0)));
  generator->PressLeftButton();
  generator->MoveMouseBy(-drag_offset - 2, 0);
  EXPECT_FALSE(IsPreviewAreaShowing());

  // The preview will still show up if we are super close to the left edge.
  generator->MoveMouseTo(2, 300);
  EXPECT_TRUE(IsPreviewAreaShowing());

  // Move back to the center point so we do not trigger splitview on mouse
  // release.
  generator->MoveMouseTo(
      gfx::ToRoundedPoint(left_item->target_bounds().CenterPoint()));
  generator->ReleaseLeftButton();
  ASSERT_TRUE(overview_session_);

  // Start dragging the right item. Even though we are in the right drag region,
  // the preview won't be shown since we started the drag there.
  generator->set_current_screen_location(gfx::ToRoundedPoint(
      right_item->target_bounds().right_center() - gfx::Vector2d(1, 0)));
  generator->PressLeftButton();
  generator->MoveMouseBy(drag_offset + 2, 0);
  EXPECT_FALSE(IsPreviewAreaShowing());

  // The preview will still show up if we are super close to the right edge.
  generator->MoveMouseTo(798, 300);
  EXPECT_TRUE(IsPreviewAreaShowing());
}

// Verify the split view preview area becomes visible when expected.
TEST_F(SplitViewDragIndicatorsTest, PreviewAreaVisibility) {
  UpdateDisplay("800x600");
  const int screen_width = 800;
  const float edge_inset = GetEdgeInset(screen_width);
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  ToggleOverview();

  // Verify the preview area is visible when |item|'s x is in the
  // range [0, edge_inset] or [screen_width - edge_inset - 1, screen_width].
  auto* item = GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(item);
  const gfx::PointF start_location(item->target_bounds().CenterPoint());
  // Drag horizontally to avoid activating drag to close.
  const float y = start_location.y();
  overview_session_->InitiateDrag(item, start_location,
                                  /*is_touch_dragging=*/false,
                                  /*event_source_item=*/item);
  EXPECT_FALSE(IsPreviewAreaShowing());
  overview_session_->Drag(item, gfx::PointF(edge_inset + 1, y));
  EXPECT_FALSE(IsPreviewAreaShowing());
  overview_session_->Drag(item, gfx::PointF(edge_inset, y));
  EXPECT_TRUE(IsPreviewAreaShowing());

  overview_session_->Drag(item, gfx::PointF(screen_width - edge_inset - 2, y));
  EXPECT_FALSE(IsPreviewAreaShowing());
  overview_session_->Drag(item, gfx::PointF(screen_width - edge_inset - 1, y));
  EXPECT_TRUE(IsPreviewAreaShowing());

  // Drag back to |start_location| before compeleting the drag, otherwise
  // |selector_time| will snap to the right and the system will enter splitview,
  // making |window_drag_controller()| nullptr.
  overview_session_->Drag(item, start_location);
  overview_session_->CompleteDrag(item, start_location);
  EXPECT_FALSE(IsPreviewAreaShowing());
}

// Verify that the preview area never shows up when dragging a unsnappable
// window.
TEST_F(SplitViewDragIndicatorsTest, PreviewAreaVisibilityUnsnappableWindow) {
  UpdateDisplay("800x600");
  const int screen_width = 800;
  std::unique_ptr<aura::Window> window(CreateUnsnappableWindow());
  ToggleOverview();

  auto* item = GetOverviewItemForWindow(window.get());
  const gfx::PointF start_location(item->target_bounds().CenterPoint());
  overview_session_->InitiateDrag(item, start_location,
                                  /*is_touch_dragging=*/false,
                                  /*event_source_item=*/item);
  EXPECT_FALSE(IsPreviewAreaShowing());
  overview_session_->Drag(item, gfx::PointF(0.f, 1.f));
  EXPECT_FALSE(IsPreviewAreaShowing());
  overview_session_->Drag(item, gfx::PointF(screen_width, 1.f));
  EXPECT_FALSE(IsPreviewAreaShowing());

  overview_session_->CompleteDrag(item, start_location);
  EXPECT_FALSE(IsPreviewAreaShowing());
}

// Check |SplitViewDragIndicators::current_window_dragging_state_| in common
// workflows (see the comments in the definition of
// |SplitViewDragIndicators::WindowDraggingState|).
TEST_F(SplitViewDragIndicatorsTest,
       SplitViewDragIndicatorsWindowDraggingState) {
  UpdateDisplay("800x600");
  const int screen_width = 800;
  const float edge_inset = GetEdgeInset(screen_width);
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  ToggleOverview();

  // Start dragging from overview.
  auto* item = GetOverviewItemForWindow(window1.get());
  gfx::PointF start_location(item->target_bounds().CenterPoint());
  overview_session_->InitiateDrag(item, start_location,
                                  /*is_touch_dragging=*/false,
                                  /*event_source_item=*/item);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            window_dragging_state());
  overview_session_->StartNormalDragMode(start_location);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            window_dragging_state());

  // Reset the gesture so we stay in overview mode.
  overview_session_->ResetDraggedWindowGesture();

  // Verify the width of a snap area.
  const float y_position = start_location.y();
  overview_session_->InitiateDrag(item, start_location,
                                  /*is_touch_dragging=*/false,
                                  /*event_source_item=*/item);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            window_dragging_state());
  overview_session_->Drag(item, gfx::PointF(edge_inset + 1, y_position));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            window_dragging_state());
  overview_session_->Drag(item, gfx::PointF(edge_inset, y_position));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            window_dragging_state());

  // Snap window to the left.
  overview_session_->CompleteDrag(item, gfx::PointF(edge_inset, y_position));
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(split_view_controller->InSplitViewMode());
  ASSERT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller->state());

  // Drag from overview and snap to the right.
  item = GetOverviewItemForWindow(window2.get());
  start_location = item->target_bounds().CenterPoint();
  overview_session_->InitiateDrag(item, start_location,
                                  /*is_touch_dragging=*/false,
                                  /*event_source_item=*/item);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            window_dragging_state());
  overview_session_->Drag(item, gfx::PointF(screen_width - 1, y_position));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary,
            window_dragging_state());
  overview_session_->CompleteDrag(item, start_location);
}

// Test dragging an unsnappable window.
TEST_F(SplitViewDragIndicatorsTest,
       SplitViewDragIndicatorVisibilityUnsnappableWindow) {
  std::unique_ptr<aura::Window> unsnappable_window(CreateUnsnappableWindow());
  ToggleOverview();

  auto* item = GetOverviewItemForWindow(unsnappable_window.get());
  gfx::PointF start_location(item->target_bounds().CenterPoint());
  overview_session_->InitiateDrag(item, start_location,
                                  /*is_touch_dragging=*/false,
                                  /*event_source_item=*/item);
  overview_session_->StartNormalDragMode(start_location);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            window_dragging_state());
  const gfx::PointF end_location1(0.f, 0.f);
  overview_session_->Drag(item, end_location1);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            window_dragging_state());
  overview_session_->CompleteDrag(item, end_location1);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            window_dragging_state());
}

// Verify when the window dragging state changes, the expected indicators will
// become visible or invisible.
TEST_F(SplitViewDragIndicatorsTest, SplitViewDragIndicatorsVisibility) {
  std::unique_ptr<aura::Window> dragged_window(CreateTestWindow());
  auto indicator = std::make_unique<SplitViewDragIndicators>(
      dragged_window->GetRootWindow());
  indicator->SetDraggedWindow(dragged_window.get());

  auto to_int = [](IndicatorType type) { return static_cast<int>(type); };

  // Helper function to which checks that all indicator types passed in |mask|
  // are visible, and those that are not are not visible.
  auto check_helper = [](SplitViewDragIndicators* svdi, int mask) {
    const std::vector<IndicatorType> types = {
        IndicatorType::kLeftHighlight, IndicatorType::kLeftText,
        IndicatorType::kRightHighlight, IndicatorType::kRightText};
    for (auto type : types) {
      if ((static_cast<int>(type) & mask) > 0)
        EXPECT_TRUE(svdi->GetIndicatorTypeVisibilityForTesting(type));
      else
        EXPECT_FALSE(svdi->GetIndicatorTypeVisibilityForTesting(type));
    }
  };

  // Check each state has the correct views displayed. Verify that nothing is
  // shown in state |SplitViewDragIndicators::WindowDraggingState::kNoDrag|.
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kNoDrag);
  check_helper(indicator.get(), 0);

  const int all = to_int(IndicatorType::kLeftHighlight) |
                  to_int(IndicatorType::kLeftText) |
                  to_int(IndicatorType::kRightHighlight) |
                  to_int(IndicatorType::kRightText);
  // Verify that everything is visible in state
  // |SplitViewDragIndicators::WindowDraggingState::kFromOverview|.
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromOverview);
  check_helper(indicator.get(), all);

  // Verify that only one highlight shows up for the snap states.
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary);
  check_helper(indicator.get(), to_int(IndicatorType::kLeftHighlight));
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary);
  check_helper(indicator.get(), to_int(IndicatorType::kRightHighlight));

  // Verify that only snap previews are shown for window dragging from shelf.
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kNoDrag);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromShelf);
  check_helper(indicator.get(), 0);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary);
  check_helper(indicator.get(), to_int(IndicatorType::kLeftHighlight));
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromShelf);
  check_helper(indicator.get(), 0);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary);
  check_helper(indicator.get(), to_int(IndicatorType::kRightHighlight));
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromShelf);
  check_helper(indicator.get(), 0);

  ScreenOrientationControllerTestApi orientation_api(
      Shell::Get()->screen_orientation_controller());
  // Verify that only snap preview in state
  // |SplitViewDragIndicators::WindowDraggingState::kFromTop| in landscape
  // orientation.
  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            orientation_api.GetCurrentOrientation());
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kNoDrag);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromTop);
  check_helper(indicator.get(), 0);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary);
  check_helper(indicator.get(), to_int(IndicatorType::kRightHighlight));
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromTop);
  check_helper(indicator.get(), 0);

  // Verify that no drag-to-snap indicators are shown in state
  // |SplitViewDragIndicators::WindowDraggingState::kFromTop| in portrait
  // orientation.
  orientation_api.SetDisplayRotation(display::Display::ROTATE_270,
                                     display::Display::RotationSource::ACTIVE);
  ASSERT_EQ(chromeos::OrientationType::kPortraitPrimary,
            orientation_api.GetCurrentOrientation());
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kNoDrag);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromTop);
  check_helper(indicator.get(), 0);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary);
  indicator->SetWindowDraggingState(
      SplitViewDragIndicators::WindowDraggingState::kFromTop);
  check_helper(indicator.get(), 0);
}

// Verify that the preview area visibility is determined with default snap
// ratio.
TEST_F(SplitViewDragIndicatorsTest, PreviewAreaVisibilityDefaultSnapRatio) {
  UpdateDisplay("900x600");
  constexpr int screen_width = 900;
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, -1, gfx::Rect(100, 100, 500, 500)));

  // Snap `window` to 1/3 to set its snap ratio to 1/3.
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY,
                                    chromeos::kOneThirdSnapRatio);
  WindowState::Get(window.get())->OnWMEvent(&snap_left);
  const WMEvent restore(WM_EVENT_RESTORE);
  WindowState::Get(window.get())->OnWMEvent(&restore);

  // Set minimum size to make `window` snappable in 1/2 ratio but not in 1/3
  // ratio.
  delegate.set_minimum_size(gfx::Size(400, 0));

  ToggleOverview();

  // The preview area should be visible as `window` is still snappable in 1/2
  // ratio.
  auto* const item = GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(item);

  const gfx::PointF start_location(item->target_bounds().CenterPoint());
  overview_session_->InitiateDrag(item, start_location,
                                  /*is_touch_dragging=*/false,
                                  /*event_source_item=*/item);
  EXPECT_FALSE(IsPreviewAreaShowing());
  overview_session_->Drag(item, gfx::PointF(0.f, 1.f));
  EXPECT_TRUE(IsPreviewAreaShowing());
  overview_session_->Drag(item, gfx::PointF(screen_width, 1.f));
  EXPECT_TRUE(IsPreviewAreaShowing());
  overview_session_->CompleteDrag(item, start_location);
  EXPECT_FALSE(IsPreviewAreaShowing());
}

// Defines a test fixture to test behavior of SplitViewDragIndicators on
// multi-display in clamshell mode.
class ClamshellMultiDisplaySplitViewDragIndicatorsTest
    : public SplitViewDragIndicatorsTest {
 public:
  ClamshellMultiDisplaySplitViewDragIndicatorsTest() = default;
  ~ClamshellMultiDisplaySplitViewDragIndicatorsTest() override = default;

  // SplitViewDragIndicatorsTest:
  void SetUp() override {
    SplitViewDragIndicatorsTest::SetUp();
    // Disable tablet mode that is enabled in
    // `SplitViewDragIndicatorsTest::SetUp()` to test clamshell mode.
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
    base::RunLoop().RunUntilIdle();
  }
};

// Tests that dragging a window to external portrait display will layout
// split view drag indicators vertically instead of horizontally.
TEST_F(ClamshellMultiDisplaySplitViewDragIndicatorsTest,
       IndicatorsLayoutWhileDraggingWindowToPortraitDisplay) {
  UpdateDisplay("800x600,600x800");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  const display::Display landscape_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]);
  const display::Display portrait_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);
  ToggleOverview();
  // Overview starts with no split view drag indicator.
  auto* indicators = overview_session_->GetGridWithRootWindow(root_windows[0])
                         ->split_view_drag_indicators();
  EXPECT_FALSE(indicators->GetIndicatorTypeVisibilityForTesting(
      IndicatorType::kLeftText));
  EXPECT_FALSE(indicators->GetIndicatorTypeVisibilityForTesting(
      IndicatorType::kRightText));

  // Start dragging from overview in the landscape display.
  auto* item = GetOverviewItemForWindow(window1.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            window_dragging_state());

  event_generator->MoveMouseTo(gfx::Point(400, 300));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            window_dragging_state());

  // The split view indicator should show up with left indicator on the left
  // and its height span over height of the display work area.
  EXPECT_TRUE(indicators->GetIndicatorTypeVisibilityForTesting(
      IndicatorType::kLeftText));
  EXPECT_TRUE(indicators->GetIndicatorTypeVisibilityForTesting(
      IndicatorType::kRightText));
  gfx::Rect left_indicator_bounds = indicators->GetLeftHighlightViewBounds();
  EXPECT_EQ(left_indicator_bounds.height(),
            landscape_display.work_area().height() -
                2 * kHighlightScreenEdgePaddingDp);

  // Stop dragging and verify we are still in overview.
  event_generator->ReleaseLeftButton();
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Drag a window to the portrait display.
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(1100, 400));
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            window_dragging_state());
  indicators = overview_session_->GetGridWithRootWindow(root_windows[1])
                   ->split_view_drag_indicators();
  EXPECT_TRUE(indicators->GetIndicatorTypeVisibilityForTesting(
      IndicatorType::kLeftText));
  EXPECT_TRUE(indicators->GetIndicatorTypeVisibilityForTesting(
      IndicatorType::kRightText));

  // The left indicator should be on the top of the display and its width span
  // the work area width. Otherwise, the left indicator should be on the left
  // and its height span the work area height.
  left_indicator_bounds = indicators->GetLeftHighlightViewBounds();
  EXPECT_EQ(
      left_indicator_bounds.width(),
      portrait_display.work_area().width() - 2 * kHighlightScreenEdgePaddingDp);
}

// Test that when we drag in overview clamshell to another display, the
// splitview indicators are the expected size. Regression test for
// http://b/316043785.
TEST_F(ClamshellMultiDisplaySplitViewDragIndicatorsTest, IndicatorSize) {
  // The displays need to be different widths for the bug to repro.
  UpdateDisplay("800x600,800+0-1000x600");

  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(300, 300));

  ToggleOverview();
  auto* item = GetOverviewItemForWindow(window.get());

  // Start dragging the overview item on the primary display. Drag the overview
  // item to the secondary display right edge, so the right side preview
  // indicator shows up.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(1780, 500));

  // Verify the size of the right side indicator. It should be roughly half of
  // the secondary display.
  aura::Window* secondary_root = Shell::GetAllRootWindows()[1];
  auto* secondary_display_indicators =
      overview_session_->GetGridWithRootWindow(secondary_root)
          ->split_view_drag_indicators();
  gfx::Rect expected_bounds_in_parent(500, 0, 500,
                                      WorkAreaInsets::ForWindow(secondary_root)
                                          ->user_work_area_bounds()
                                          .height());
  expected_bounds_in_parent.Inset(kHighlightScreenEdgePaddingDp);
  EXPECT_EQ(
      expected_bounds_in_parent,
      secondary_display_indicators->GetRightHighlightViewBoundsForTesting());
}

}  // namespace ash
