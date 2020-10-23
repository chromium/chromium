// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_close_button.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/capture_mode/capture_mode_source_view.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/capture_mode/capture_mode_type_view.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "ash/capture_mode/capture_mode_util.h"
#include "ash/capture_mode/stop_recording_button_tray.h"
#include "ash/display/cursor_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/magnifier/magnifier_glass.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/system/status_area_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/window_state.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/view.h"

namespace ash {

namespace {

// Returns true if the software-composited cursor is enabled.
bool IsCursorCompositingEnabled() {
  return Shell::Get()
      ->window_tree_host_manager()
      ->cursor_window_controller()
      ->ShouldEnableCursorCompositing();
}

void ClickOnView(const views::View* view,
                 ui::test::EventGenerator* event_generator) {
  DCHECK(view);
  DCHECK(event_generator);

  const gfx::Point view_center = view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(view_center);
  event_generator->ClickLeftButton();
}

void SendKey(ui::KeyboardCode key_code,
             ui::test::EventGenerator* event_generator) {
  event_generator->PressKey(key_code, /*flags=*/0);
  event_generator->ReleaseKey(key_code, /*flags=*/0);
}

// Moves the mouse and updates the cursor's display manually to imitate what a
// real mouse move event does in shell.
void MoveMouseToAndUpdateCursorDisplay(
    const gfx::Point& point,
    ui::test::EventGenerator* event_generator) {
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestPoint(point));
  event_generator->MoveMouseTo(point);
}

}  // namespace

// Wrapper for CaptureModeSession that exposes internal state to test functions.
class CaptureModeSessionTestApi {
 public:
  explicit CaptureModeSessionTestApi(CaptureModeSession* session)
      : session_(session) {}
  CaptureModeSessionTestApi(const CaptureModeSessionTestApi&) = delete;
  CaptureModeSessionTestApi& operator=(const CaptureModeSessionTestApi&) =
      delete;
  ~CaptureModeSessionTestApi() = default;

  CaptureModeBarView* capture_mode_bar_view() const {
    return session_->capture_mode_bar_view_;
  }

  views::Widget* capture_label_widget() const {
    return session_->capture_label_widget_.get();
  }

  views::Widget* dimensions_label_widget() const {
    return session_->dimensions_label_widget_.get();
  }

  const MagnifierGlass& magnifier_glass() const {
    return session_->magnifier_glass_;
  }

 private:
  const CaptureModeSession* const session_;
};

class CaptureModeTest : public AshTestBase {
 public:
  CaptureModeTest() = default;
  CaptureModeTest(const CaptureModeTest&) = delete;
  CaptureModeTest& operator=(const CaptureModeTest&) = delete;
  ~CaptureModeTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(features::kCaptureMode);
    AshTestBase::SetUp();
  }

  CaptureModeBarView* GetCaptureModeBarView() const {
    auto* session = CaptureModeController::Get()->capture_mode_session();
    DCHECK(session);
    return CaptureModeSessionTestApi(session).capture_mode_bar_view();
  }

  CaptureModeToggleButton* GetImageToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->capture_type_view()->image_toggle_button();
  }

  CaptureModeToggleButton* GetVideoToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->capture_type_view()->video_toggle_button();
  }

  CaptureModeToggleButton* GetFullscreenToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->fullscreen_toggle_button();
  }

  CaptureModeToggleButton* GetRegionToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->region_toggle_button();
  }

  CaptureModeToggleButton* GetWindowToggleButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()
        ->capture_source_view()
        ->window_toggle_button();
  }

  CaptureModeCloseButton* GetCloseButton() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    return GetCaptureModeBarView()->close_button();
  }

  aura::Window* GetDimensionsLabelWindow() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto* widget = CaptureModeSessionTestApi(controller->capture_mode_session())
                       .dimensions_label_widget();
    return widget ? widget->GetNativeWindow() : nullptr;
  }

  base::Optional<gfx::Point> GetMagnifierGlassCenterPoint() const {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    auto& magnifier =
        CaptureModeSessionTestApi(controller->capture_mode_session())
            .magnifier_glass();
    if (magnifier.host_widget_for_testing()) {
      return magnifier.host_widget_for_testing()
          ->GetWindowBoundsInScreen()
          .CenterPoint();
    }
    return base::nullopt;
  }

  CaptureModeController* StartCaptureSession(CaptureModeSource source,
                                             CaptureModeType type) {
    auto* controller = CaptureModeController::Get();
    controller->SetSource(source);
    controller->SetType(type);
    controller->Start();
    DCHECK(controller->IsActive());
    return controller;
  }

  // Start Capture Mode with source region and type image.
  CaptureModeController* StartImageRegionCapture() {
    return StartCaptureSession(CaptureModeSource::kRegion,
                               CaptureModeType::kImage);
  }

  // Select a region by pressing and dragging the mouse.
  void SelectRegion(const gfx::Rect& region, bool release_mouse = true) {
    auto* controller = CaptureModeController::Get();
    ASSERT_TRUE(controller->IsActive());
    ASSERT_EQ(CaptureModeSource::kRegion, controller->source());
    auto* event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(region.origin());
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(region.bottom_right());
    if (release_mouse)
      event_generator->ReleaseLeftButton();
    EXPECT_EQ(region, controller->user_capture_region());
  }

  void WaitForCountDownToFinish() {
    auto* controller = CaptureModeController::Get();
    DCHECK(controller->IsActive());
    DCHECK_EQ(controller->type(), CaptureModeType::kVideo);
    while (!controller->is_recording_in_progress()) {
      base::RunLoop run_loop;
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE, run_loop.QuitClosure(),
          base::TimeDelta::FromMilliseconds(100));
      run_loop.Run();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(CaptureModeTest, StartStop) {
  auto* controller = CaptureModeController::Get();
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  // Calling start again is a no-op.
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, StartWithMostRecentTypeAndSource) {
  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start();
  EXPECT_TRUE(controller->IsActive());

  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());

  ClickOnView(GetCloseButton(), GetEventGenerator());
  EXPECT_FALSE(controller->IsActive());
}

TEST_F(CaptureModeTest, ChangeTypeAndSourceFromUI) {
  auto* controller = CaptureModeController::Get();
  controller->Start();
  EXPECT_TRUE(controller->IsActive());

  EXPECT_TRUE(GetImageToggleButton()->GetToggled());
  EXPECT_FALSE(GetVideoToggleButton()->GetToggled());
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetVideoToggleButton(), event_generator);
  EXPECT_FALSE(GetImageToggleButton()->GetToggled());
  EXPECT_TRUE(GetVideoToggleButton()->GetToggled());
  EXPECT_EQ(controller->type(), CaptureModeType::kVideo);

  ClickOnView(GetWindowToggleButton(), event_generator);
  EXPECT_FALSE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_TRUE(GetWindowToggleButton()->GetToggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kWindow);

  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_TRUE(GetFullscreenToggleButton()->GetToggled());
  EXPECT_FALSE(GetRegionToggleButton()->GetToggled());
  EXPECT_FALSE(GetWindowToggleButton()->GetToggled());
  EXPECT_EQ(controller->source(), CaptureModeSource::kFullscreen);
}

// TODO(https://crbug.com/1141927): test is flakey.
TEST_F(CaptureModeTest, DISABLED_VideoRecordingUiBehavior) {
  // We need a non-zero duration to avoid infinite loop on countdown.
  ui::ScopedAnimationDurationScaleMode animatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto* controller = CaptureModeController::Get();
  // Start Capture Mode in a fullscreen video recording mode.
  controller->SetSource(CaptureModeSource::kFullscreen);
  controller->SetType(CaptureModeType::kVideo);
  controller->Start();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());

  // Hit Enter to begin recording.
  auto* event_generator = GetEventGenerator();
  SendKey(ui::VKEY_RETURN, event_generator);
  WaitForCountDownToFinish();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_TRUE(controller->is_recording_in_progress());

  // The composited cursor should be enabled, and the stop-recording button
  // should show up in the status area widget.
  EXPECT_TRUE(IsCursorCompositingEnabled());
  auto* stop_recording_button = Shell::GetPrimaryRootWindowController()
                                    ->GetStatusAreaWidget()
                                    ->stop_recording_button_tray();
  EXPECT_TRUE(stop_recording_button->visible_preferred());

  // End recording via the stop-recording button. Expect that it's now hidden,
  // and the cursor compositing is now disabled.
  ClickOnView(stop_recording_button, event_generator);
  EXPECT_FALSE(stop_recording_button->visible_preferred());
  EXPECT_FALSE(controller->is_recording_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());
}

// Tests the behavior of repositioning a region with capture mode.
TEST_F(CaptureModeTest, CaptureRegionRepositionBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x800");

  auto* controller = StartImageRegionCapture();

  // The first time selecting a region, the region is a default rect.
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());

  // Press down and drag to select a region.
  SelectRegion(gfx::Rect(100, 100, 600, 600));

  // Click somewhere in the center on the region and drag. The whole region
  // should move. Note that the point cannot be in the capture button bounds,
  // which is located in the center of the region.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(200, 200));
  event_generator->DragMouseBy(-50, -50);
  EXPECT_EQ(gfx::Rect(50, 50, 600, 600), controller->user_capture_region());

  // Try to drag the region offscreen. The region should be bound by the display
  // size.
  event_generator->set_current_screen_location(gfx::Point(100, 100));
  event_generator->DragMouseBy(-150, -150);
  EXPECT_EQ(gfx::Rect(600, 600), controller->user_capture_region());
}

// Tests the behavior of resizing a region with capture mode using the corner
// drag affordances.
TEST_F(CaptureModeTest, CaptureRegionCornerResizeBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x800");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 400, 400));
  SelectRegion(target_region);

  // For each corner point try dragging to several points and verify that the
  // capture region is as expected.
  struct {
    std::string trace;
    gfx::Point drag_point;
    // The point that stays the same while dragging. It is the opposite vertex
    // to |drag_point| on |target_region|.
    gfx::Point anchor_point;
  } kDragCornerCases[] = {
      {"origin", target_region.origin(), target_region.bottom_right()},
      {"top_right", target_region.top_right(), target_region.bottom_left()},
      {"bottom_right", target_region.bottom_right(), target_region.origin()},
      {"bottom_left", target_region.bottom_left(), target_region.top_right()},
  };

  // The test corner points are one in each corner outside |target_region| and
  // one point inside |target_region|.
  auto drag_test_points = {gfx::Point(100, 100), gfx::Point(700, 100),
                           gfx::Point(700, 700), gfx::Point(100, 700),
                           gfx::Point(400, 400)};
  auto* event_generator = GetEventGenerator();
  for (auto test_case : kDragCornerCases) {
    SCOPED_TRACE(test_case.trace);
    event_generator->set_current_screen_location(test_case.drag_point);
    event_generator->PressLeftButton();

    // At each drag test point, the region rect should be the rect created by
    // the given |corner_point| and the drag test point. That is, the width
    // should match the x distance between the two points, the height should
    // match the y distance between the two points and that both points are
    // contained in the region.
    for (auto drag_test_point : drag_test_points) {
      event_generator->MoveMouseTo(drag_test_point);
      gfx::Rect region = controller->user_capture_region();
      const gfx::Vector2d distance = test_case.anchor_point - drag_test_point;
      EXPECT_EQ(std::abs(distance.x()), region.width());
      EXPECT_EQ(std::abs(distance.y()), region.height());

      // gfx::Rect::Contains returns the point (x+width, y+height) as false, so
      // make the region one unit bigger to account for this.
      region.Inset(gfx::Insets(-1));
      EXPECT_TRUE(region.Contains(drag_test_point));
      EXPECT_TRUE(region.Contains(test_case.anchor_point));
    }

    // Make sure the region is reset for the next iteration.
    event_generator->MoveMouseTo(test_case.drag_point);
    event_generator->ReleaseLeftButton();
    ASSERT_EQ(target_region, controller->user_capture_region());
  }
}

// Tests the behavior of resizing a region with capture mode using the edge drag
// affordances.
TEST_F(CaptureModeTest, CaptureRegionEdgeResizeBehavior) {
  // Use a set display size as we will be choosing points in this test.
  UpdateDisplay("800x800");

  auto* controller = StartImageRegionCapture();
  // Create the initial region.
  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));
  SelectRegion(target_region);

  // For each edge point try dragging to several points and verify that the
  // capture region is as expected.
  struct {
    std::string trace;
    gfx::Point drag_point;
    // True if horizontal direction (left, right). Height stays the same while
    // dragging if true, width stays the same while dragging if false.
    bool horizontal;
    // The edge that stays the same while dragging. It is the opposite edge to
    // |drag_point|. For example, if |drag_point| is the left center of
    // |target_region|, then |anchor_edge| is the right edge.
    int anchor_edge;
  } kDragEdgeCases[] = {
      {"left", target_region.left_center(), true, target_region.right()},
      {"top", target_region.top_center(), false, target_region.bottom()},
      {"right", target_region.right_center(), true, target_region.x()},
      {"bottom", target_region.bottom_center(), false, target_region.y()},
  };

  // Drag to a couple of points that change both x and y. In all these cases,
  // only the width or height should change.
  auto drag_test_points = {gfx::Point(150, 150), gfx::Point(350, 350),
                           gfx::Point(450, 450)};
  auto* event_generator = GetEventGenerator();
  for (auto test_case : kDragEdgeCases) {
    SCOPED_TRACE(test_case.trace);
    event_generator->set_current_screen_location(test_case.drag_point);
    event_generator->PressLeftButton();

    for (auto drag_test_point : drag_test_points) {
      event_generator->MoveMouseTo(drag_test_point);
      const gfx::Rect region = controller->user_capture_region();

      // One of width/height will always be the same as |target_region|'s
      // initial width/height, depending on the edge affordance. The other
      // dimension will be the distance from |drag_test_point| to the anchor
      // edge.
      const int variable_length = std::abs(
          (test_case.horizontal ? drag_test_point.x() : drag_test_point.y()) -
          test_case.anchor_edge);
      const int expected_width =
          test_case.horizontal ? variable_length : target_region.width();
      const int expected_height =
          test_case.horizontal ? target_region.height() : variable_length;

      EXPECT_EQ(expected_width, region.width());
      EXPECT_EQ(expected_height, region.height());
    }

    // Make sure the region is reset for the next iteration.
    event_generator->MoveMouseTo(test_case.drag_point);
    event_generator->ReleaseLeftButton();
    ASSERT_EQ(target_region, controller->user_capture_region());
  }
}

// Tests that the capture region persists after exiting and reentering capture
// mode.
TEST_F(CaptureModeTest, CaptureRegionPersistsAfterExit) {
  auto* controller = StartImageRegionCapture();
  const gfx::Rect region(100, 100, 200, 200);
  SelectRegion(region);

  controller->Stop();
  controller->Start();
  EXPECT_EQ(region, controller->user_capture_region());
}

// Tests that the capture region resets when clicking outside the current
// capture regions bounds.
TEST_F(CaptureModeTest, CaptureRegionResetsOnClickOutside) {
  auto* controller = StartImageRegionCapture();
  SelectRegion(gfx::Rect(100, 100, 200, 200));

  // Click on an area outside of the current capture region. The capture region
  // should reset to default rect.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(gfx::Point(400, 400));
  event_generator->ClickLeftButton();
  EXPECT_EQ(gfx::Rect(), controller->user_capture_region());
}

// Tests that buttons on the capture mode bar still work when a region is
// "covering" them.
TEST_F(CaptureModeTest, CaptureRegionCoversCaptureModeBar) {
  UpdateDisplay("800x800");

  auto* controller = StartImageRegionCapture();

  // Select a region such that the capture mode bar is covered.
  SelectRegion(gfx::Rect(5, 5, 795, 795));
  EXPECT_TRUE(controller->user_capture_region().Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  // Click on the fullscreen toggle button to verify that we enter fullscreen
  // capture mode. Then click on the region toggle button to verify that we
  // reenter region capture mode and that the region is still covering the
  // capture mode bar.
  auto* event_generator = GetEventGenerator();
  ClickOnView(GetFullscreenToggleButton(), event_generator);
  EXPECT_EQ(CaptureModeSource::kFullscreen, controller->source());
  ClickOnView(GetRegionToggleButton(), GetEventGenerator());
  ASSERT_EQ(CaptureModeSource::kRegion, controller->source());
  ASSERT_TRUE(controller->user_capture_region().Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  ClickOnView(GetCloseButton(), event_generator);
  EXPECT_FALSE(controller->IsActive());
}

// Tests that the magnifying glass appears while fine tuning the capture region,
// and that the cursor is hidden if the magnifying glass is present.
TEST_F(CaptureModeTest, CaptureRegionMagnifierWhenFineTuning) {
  const gfx::Vector2d kDragDelta(50, 50);
  UpdateDisplay("800x800");

  // Start Capture Mode in a region in image mode.
  StartImageRegionCapture();

  // Press down and drag to select a region. The magnifier should not be
  // visible yet.
  gfx::Rect capture_region{200, 200, 400, 400};
  SelectRegion(capture_region);
  EXPECT_EQ(base::nullopt, GetMagnifierGlassCenterPoint());

  auto check_magnifier_shows_properly = [this](const gfx::Point& origin,
                                               const gfx::Point& destination,
                                               bool should_show_magnifier) {
    // If |should_show_magnifier|, check that the magnifying glass is centered
    // on the mouse after press and during drag, and that the cursor is hidden.
    // If not |should_show_magnifier|, check that the magnifying glass never
    // shows. Should always be not visible when mouse button is released.
    auto* event_generator = GetEventGenerator();
    base::Optional<gfx::Point> expected_origin =
        should_show_magnifier ? base::make_optional(origin) : base::nullopt;
    base::Optional<gfx::Point> expected_destination =
        should_show_magnifier ? base::make_optional(destination)
                              : base::nullopt;

    auto* cursor_manager = Shell::Get()->cursor_manager();
    EXPECT_TRUE(cursor_manager->IsCursorVisible());

    // Move cursor to |origin| and click.
    event_generator->set_current_screen_location(origin);
    event_generator->PressLeftButton();
    EXPECT_EQ(expected_origin, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Drag to |destination| while holding left button.
    event_generator->MoveMouseTo(destination);
    EXPECT_EQ(expected_destination, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Drag back to |origin| while still holding left button.
    event_generator->MoveMouseTo(origin);
    EXPECT_EQ(expected_origin, GetMagnifierGlassCenterPoint());
    EXPECT_NE(should_show_magnifier, cursor_manager->IsCursorVisible());

    // Release left button.
    event_generator->ReleaseLeftButton();
    EXPECT_EQ(base::nullopt, GetMagnifierGlassCenterPoint());
    EXPECT_TRUE(cursor_manager->IsCursorVisible());
  };

  // Drag the capture region from within the existing selected region. The
  // magnifier should not be visible at any point.
  check_magnifier_shows_properly(gfx::Point(400, 250), gfx::Point(500, 350),
                                 /*should_show_magnifier=*/false);

  // Check that each corner fine tune position shows the magnifier when
  // dragging.
  struct {
    std::string trace;
    FineTunePosition position;
  } kFineTunePositions[] = {{"top_left", FineTunePosition::kTopLeft},
                            {"top_right", FineTunePosition::kTopRight},
                            {"bottom_right", FineTunePosition::kBottomRight},
                            {"bottom_left", FineTunePosition::kBottomLeft}};
  for (const auto& fine_tune_position : kFineTunePositions) {
    SCOPED_TRACE(fine_tune_position.trace);
    const gfx::Point drag_affordance_location =
        capture_mode_util::GetLocationForFineTunePosition(
            capture_region, fine_tune_position.position);
    check_magnifier_shows_properly(drag_affordance_location,
                                   drag_affordance_location + kDragDelta,
                                   /*should_show_magnifier=*/true);
  }
}

// Tests that the dimensions label properly renders for capture regions.
TEST_F(CaptureModeTest, CaptureRegionDimensionsLabelLocation) {
  UpdateDisplay("800x800");

  // Start Capture Mode in a region in image mode.
  StartImageRegionCapture();

  // Press down and don't move the mouse. Label shouldn't display for empty
  // capture regions.
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(gfx::Point(0, 0));
  generator->PressLeftButton();
  auto* controller = CaptureModeController::Get();
  EXPECT_TRUE(controller->IsActive());
  EXPECT_TRUE(controller->user_capture_region().IsEmpty());
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());
  generator->ReleaseLeftButton();

  // Press down and drag to select a large region. Verify that the dimensions
  // label is centered and that the label is below the capture region.
  gfx::Rect capture_region{100, 100, 600, 200};
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(capture_region.CenterPoint().x(),
            GetDimensionsLabelWindow()->bounds().CenterPoint().x());
  EXPECT_EQ(capture_region.bottom() +
                CaptureModeSession::kSizeLabelYDistanceFromRegionDp,
            GetDimensionsLabelWindow()->bounds().y());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the left side of the screen such that
  // if the label was centered it would extend out of the screen.
  // The x value of the label should be the left edge of the screen (0).
  capture_region.SetRect(2, 100, 2, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(0, GetDimensionsLabelWindow()->bounds().x());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the right side of the screen such that
  // if the label was centered it would extend out of the screen.
  // The right (x + width) of the label should be the right edge of the screen
  // (800).
  capture_region.SetRect(796, 100, 2, 100);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(800, GetDimensionsLabelWindow()->bounds().right());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());

  // Create a new capture region close to the bottom side of the screen.
  // The label should now appear inside the capture region, just above the
  // bottom edge. It should be above the bottom of the screen as well.
  capture_region.SetRect(100, 700, 600, 790);
  SelectRegion(capture_region, /*release_mouse=*/false);
  EXPECT_EQ(800 - CaptureModeSession::kSizeLabelYDistanceFromRegionDp,
            GetDimensionsLabelWindow()->bounds().bottom());
  generator->ReleaseLeftButton();
  EXPECT_EQ(nullptr, GetDimensionsLabelWindow());
}

TEST_F(CaptureModeTest, CaptureRegionCaptureButtonLocation) {
  UpdateDisplay("800x800");

  auto* controller = StartImageRegionCapture();

  // Select a large region. Verify that the capture button widget is centered.
  SelectRegion(gfx::Rect(100, 100, 600, 600));

  views::Widget* capture_button_widget =
      CaptureModeSessionTestApi(controller->capture_mode_session())
          .capture_label_widget();
  ASSERT_TRUE(capture_button_widget);
  aura::Window* capture_button_window =
      capture_button_widget->GetNativeWindow();
  EXPECT_EQ(gfx::Point(400, 400),
            capture_button_window->bounds().CenterPoint());

  // Drag the bottom corner so that the region is too small to fit the capture
  // button. Verify that the button is aligned horizontally and placed below the
  // region.
  auto* event_generator = GetEventGenerator();
  event_generator->DragMouseTo(gfx::Point(120, 120));
  EXPECT_EQ(gfx::Rect(100, 100, 20, 20), controller->user_capture_region());
  EXPECT_EQ(110, capture_button_window->bounds().CenterPoint().x());
  const int distance_from_region =
      CaptureModeSession::kCaptureButtonDistanceFromRegionDp;
  EXPECT_EQ(120 + distance_from_region, capture_button_window->bounds().y());

  // Click inside the region to drag the entire region to the bottom of the
  // screen. Verify that the button is aligned horizontally and placed above the
  // region.
  event_generator->set_current_screen_location(gfx::Point(110, 110));
  event_generator->DragMouseTo(gfx::Point(110, 790));
  EXPECT_EQ(gfx::Rect(100, 780, 20, 20), controller->user_capture_region());
  EXPECT_EQ(110, capture_button_window->bounds().CenterPoint().x());
  EXPECT_EQ(780 - distance_from_region,
            capture_button_window->bounds().bottom());
}

TEST_F(CaptureModeTest, WindowCapture) {
  // Create 2 windows that overlap with each other.
  const gfx::Rect bounds1(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds1));
  const gfx::Rect bounds2(150, 150, 200, 200);
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds2));

  auto* controller = CaptureModeController::Get();
  controller->SetSource(CaptureModeSource::kWindow);
  controller->SetType(CaptureModeType::kImage);
  controller->Start();
  EXPECT_TRUE(controller->IsActive());

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseToCenterOf(window1.get());
  auto* capture_mode_session = controller->capture_mode_session();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  event_generator->MoveMouseToCenterOf(window2.get());
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window2.get());

  // Now move the mouse to the overlapped area.
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window2.get());
  // Close the current selected window should automatically focus to next one.
  window2.reset();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());
  // Open another one on top also change the selected window.
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds2));
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window3.get());
  // Minimize the window should also automatically change the selected window.
  WindowState::Get(window3.get())->Minimize();
  EXPECT_EQ(capture_mode_session->GetSelectedWindow(), window1.get());

  // Stop the capture session to avoid CaptureModeSession from receiving more
  // events during test tearing down.
  controller->Stop();
}

// Tests that the capture bar is located on the root with the cursor when
// starting capture mode.
TEST_F(CaptureModeTest, MultiDisplayCaptureBarInitialLocation) {
  UpdateDisplay("800x800,801+0-800x800");

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);

  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  controller->Stop();

  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);
  StartImageRegionCapture();
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
}

// Tests behavior of a capture mode session if the active display is removed.
TEST_F(CaptureModeTest, DisplayRemoval) {
  UpdateDisplay("800x800,801+0-800x800");

  // Start capture mode on the secondary display.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), GetEventGenerator());
  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
  ASSERT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

  // Remove secondary display.
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);

  // Spin the run loop so that we get a signal that the associated root window
  // of the removed display is destroyed.
  base::RunLoop().RunUntilIdle();

  // Tests that the capture mode bar is now on the primary display.
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));
  ASSERT_EQ(Shell::GetAllRootWindows()[0], session->current_root());
}

// Tests that using fullscreen or window source, moving the mouse across
// displays will change the root window of the capture session.
TEST_F(CaptureModeTest, MultiDisplayFullscreenOrWindowSourceRootWindow) {
  UpdateDisplay("800x800,801+0-800x800");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  for (auto source :
       {CaptureModeSource::kFullscreen, CaptureModeSource::kWindow}) {
    SCOPED_TRACE(source == CaptureModeSource::kFullscreen ? "Fullscreen source"
                                                          : "Window source");

    auto* controller = StartCaptureSession(CaptureModeSource::kFullscreen,
                                           CaptureModeType::kImage);
    auto* session = controller->capture_mode_session();
    EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

    MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
    EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());

    MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);
    EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

    controller->Stop();
  }
}

// Tests that in region mode, moving the mouse across displays will not change
// the root window of the capture session, but clicking on a new display will.
TEST_F(CaptureModeTest, MultiDisplayRegionSourceRootWindow) {
  UpdateDisplay("800x800,801+0-800x800");
  ASSERT_EQ(2u, Shell::GetAllRootWindows().size());

  auto* event_generator = GetEventGenerator();
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(100, 500), event_generator);

  auto* controller = StartImageRegionCapture();
  auto* session = controller->capture_mode_session();
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that moving the mouse to the secondary display does not change the
  // root.
  MoveMouseToAndUpdateCursorDisplay(gfx::Point(1000, 500), event_generator);
  EXPECT_EQ(Shell::GetAllRootWindows()[0], session->current_root());

  // Tests that pressing the mouse changes the root. The capture bar stays on
  // the primary display until the mouse is released.
  event_generator->PressLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_TRUE(gfx::Rect(800, 800).Contains(
      GetCaptureModeBarView()->GetBoundsInScreen()));

  event_generator->ReleaseLeftButton();
  EXPECT_EQ(Shell::GetAllRootWindows()[1], session->current_root());
  EXPECT_TRUE(gfx::Rect(801, 0, 800, 800)
                  .Contains(GetCaptureModeBarView()->GetBoundsInScreen()));
}

TEST_F(CaptureModeTest, RegionCursorStates) {
  using ui::mojom::CursorType;

  auto* cursor_manager = Shell::Get()->cursor_manager();
  CursorType original_cursor_type = cursor_manager->GetCursor().type();
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, original_cursor_type);

  auto* event_generator = GetEventGenerator();
  auto* controller = StartImageRegionCapture();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  event_generator->MoveMouseTo(gfx::Point(175, 175));
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));
  SelectRegion(target_region);

  // Makes sure that the cursor is updated when the user releases the region
  // select and is still hovering in the same location.
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());

  // Verify that all of the |FineTunePosition| locations have the correct cursor
  // when hovered over.
  event_generator->MoveMouseTo(target_region.origin());
  EXPECT_EQ(CursorType::kNorthWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.top_center());
  EXPECT_EQ(CursorType::kNorthSouthResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.top_right());
  EXPECT_EQ(CursorType::kNorthEastResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.right_center());
  EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_right());
  EXPECT_EQ(CursorType::kSouthEastResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_center());
  EXPECT_EQ(CursorType::kNorthSouthResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.bottom_left());
  EXPECT_EQ(CursorType::kSouthWestResize, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.left_center());
  EXPECT_EQ(CursorType::kEastWestResize, cursor_manager->GetCursor().type());

  // Tests that within the bounds of the selected region, the cursor is a hand
  // when hovering over the capture button, otherwise it is a multi-directional
  // move cursor.
  event_generator->MoveMouseTo(gfx::Point(250, 250));
  EXPECT_EQ(CursorType::kMove, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(target_region.CenterPoint());
  EXPECT_EQ(CursorType::kHand, cursor_manager->GetCursor().type());

  // Tests that the cursor changes to a cell type when hovering over the
  // unselected region.
  event_generator->MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  // Check that cursor is unlocked when changing sources, and that the cursor
  // changes to a pointer when hovering over the capture mode bar.
  event_generator->MoveMouseTo(
      GetRegionToggleButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(
      GetWindowToggleButton()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->ClickLeftButton();
  ASSERT_EQ(CaptureModeSource::kWindow, controller->source());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());

  // Tests that on changing back to region capture mode, the cursor becomes
  // locked, and is still a pointer type over the bar, whilst a cell cursor
  // otherwise (not over the selected region).
  event_generator->MoveMouseTo(
      GetRegionToggleButton()->GetBoundsInScreen().CenterPoint());
  original_cursor_type = cursor_manager->GetCursor().type();
  event_generator->ClickLeftButton();
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kPointer, cursor_manager->GetCursor().type());
  event_generator->MoveMouseTo(gfx::Point(50, 50));
  EXPECT_EQ(CursorType::kCell, cursor_manager->GetCursor().type());

  // Tests that when exiting capture mode that the cursor is restored to its
  // original state.
  controller->Stop();
  EXPECT_FALSE(controller->IsActive());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(original_cursor_type, cursor_manager->GetCursor().type());
}

// Tests that in Region mode, cursor compositing is used instead of the system
// cursor when the cursor is being dragged.
TEST_F(CaptureModeTest, RegionDragCursorCompositing) {
  auto* event_generator = GetEventGenerator();
  auto* session = StartImageRegionCapture()->capture_mode_session();
  auto* cursor_manager = Shell::Get()->cursor_manager();

  // Initially cursor should be visible and cursor compositing is not enabled.
  EXPECT_FALSE(session->is_drag_in_progress());
  EXPECT_FALSE(IsCursorCompositingEnabled());
  EXPECT_TRUE(cursor_manager->IsCursorVisible());

  const gfx::Rect target_region(gfx::Rect(200, 200, 200, 200));

  // For each start and end point try dragging and verify that cursor
  // compositing is functioning as expected.
  struct {
    std::string trace;
    gfx::Point start_point;
    gfx::Point end_point;
  } kDragCases[] = {
      {"initial_region", target_region.origin(), target_region.bottom_right()},
      {"edge_resize", target_region.right_center(),
       gfx::Point(target_region.right_center() + gfx::Vector2d(50, 0))},
      {"corner_resize", target_region.origin(), gfx::Point(175, 175)},
      {"move", gfx::Point(250, 250), gfx::Point(300, 300)},
  };

  for (auto test_case : kDragCases) {
    SCOPED_TRACE(test_case.trace);

    event_generator->MoveMouseTo(test_case.start_point);
    event_generator->PressLeftButton();
    EXPECT_TRUE(session->is_drag_in_progress());
    EXPECT_TRUE(IsCursorCompositingEnabled());

    event_generator->MoveMouseTo(test_case.end_point);
    EXPECT_TRUE(session->is_drag_in_progress());
    EXPECT_TRUE(IsCursorCompositingEnabled());

    event_generator->ReleaseLeftButton();
    EXPECT_FALSE(session->is_drag_in_progress());
    EXPECT_FALSE(IsCursorCompositingEnabled());
  }
}

}  // namespace ash
