// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/drag_window_from_shelf_controller.h"

#include <tuple>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/overview_test_api.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/root_window_controller.h"
#include "ash/shelf/drag_window_from_shelf_controller_test_api.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/window_scale_animation.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

gfx::Rect GetShelfBounds() {
  return Shelf::ForWindow(Shell::GetPrimaryRootWindow())->GetIdealBounds();
}

// Gets the drag controller owned by the shelf.
DragWindowFromShelfController* GetDragWindowFromShelfController() {
  return AshTestBase::GetPrimaryShelf()
      ->shelf_layout_manager()
      ->window_drag_controller_for_testing();
}

}  // namespace

// This definition is needed because this constant is odr-used.
// https://en.cppreference.com/w/cpp/language/static#Constant_static_members
const float kVelocityToRestoreBoundsThreshold =
    DragWindowFromShelfController::kVelocityToRestoreBoundsThreshold;

class DragWindowFromShelfControllerTest : public AshTestBase {
 public:
  DragWindowFromShelfControllerTest() = default;

  DragWindowFromShelfControllerTest(const DragWindowFromShelfControllerTest&) =
      delete;
  DragWindowFromShelfControllerTest& operator=(
      const DragWindowFromShelfControllerTest&) = delete;

  ~DragWindowFromShelfControllerTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    // Destroy |window_drag_controller_| so that its scheduled task won't get
    // run after the test environment is gone.
    window_drag_controller_.reset();
    AshTestBase::TearDown();
  }

  void StartDrag(aura::Window* window, const gfx::Point& location_in_screen) {
    window_drag_controller_ = std::make_unique<DragWindowFromShelfController>(
        window, gfx::PointF(location_in_screen));
  }
  void Drag(const gfx::Point& location_in_screen,
            float scroll_x,
            float scroll_y) {
    window_drag_controller_->Drag(gfx::PointF(location_in_screen), scroll_x,
                                  scroll_y);
  }
  void EndDrag(const gfx::Point& location_in_screen,
               std::optional<float> velocity_y) {
    window_drag_controller_->EndDrag(gfx::PointF(location_in_screen),
                                     velocity_y);
    window_drag_controller_->FinalizeDraggedWindow();
  }
  void CancelDrag() { window_drag_controller_->CancelDrag(); }
  void WaitForHomeLauncherAnimationToFinish() {
    // Wait until home launcher animation finishes.
    ui::Layer* const layer =
        GetAppListTestHelper()->GetAppListView()->GetWidget()->GetLayer();
    ui::Compositor* const compositor = layer->GetCompositor();

    ui::LayerAnimationStoppedWaiter animation_waiter;
    animation_waiter.Wait(layer);

    // Force frames and wait for all throughput trackers to be gone to allow
    // animation throughput data to be passed from cc to ui.
    while (compositor->has_throughput_trackers_for_testing()) {
      compositor->ScheduleFullRedraw();
      std::ignore = ui::WaitForNextFrameToBePresented(compositor,
                                                      base::Milliseconds(500));
    }
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  DragWindowFromShelfController* window_drag_controller() {
    return window_drag_controller_.get();
  }

  std::unique_ptr<aura::Window> CreateTransientModalChildWindow(
      aura::Window* transient_parent,
      const gfx::Rect& bounds) {
    auto child = std::make_unique<aura::Window>(
        nullptr, aura::client::WINDOW_TYPE_POPUP);
    child->Init(ui::LAYER_NOT_DRAWN);
    child->SetBounds(bounds);
    wm::AddTransientChild(transient_parent, child.get());
    aura::client::ParentWindowWithContext(child.get(),
                                          transient_parent->GetRootWindow(),
                                          bounds, display::kInvalidDisplayId);
    child->Show();

    child->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kWindow);
    wm::SetModalParent(child.get(), transient_parent);
    return child;
  }

 private:
  std::unique_ptr<DragWindowFromShelfController> window_drag_controller_;
};

// Tests that we may hide different sets of windows with a special flag
// kHideDuringWindowDragging.
TEST_F(DragWindowFromShelfControllerTest,
       HideWindowDuringWindowDraggingWithFlag) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();

  auto window3 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  EXPECT_FALSE(window1->GetProperty(kHideDuringWindowDragging));
  EXPECT_FALSE(window2->GetProperty(kHideDuringWindowDragging));
  EXPECT_FALSE(window3->GetProperty(kHideDuringWindowDragging));

  StartDrag(window1.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EXPECT_FALSE(window1->GetProperty(kHideDuringWindowDragging));
  EXPECT_TRUE(window2->GetProperty(kHideDuringWindowDragging));
  EXPECT_TRUE(window3->GetProperty(kHideDuringWindowDragging));
  EndDrag(shelf_bounds.CenterPoint(), /*velocity_y=*/std::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  EXPECT_FALSE(window1->GetProperty(kHideDuringWindowDragging));
  EXPECT_FALSE(window2->GetProperty(kHideDuringWindowDragging));
  EXPECT_FALSE(window3->GetProperty(kHideDuringWindowDragging));
}

// Tests that we may hide different sets of windows in splitview and restores
// windows correctly after dragging.
TEST_F(DragWindowFromShelfControllerTest,
       HideWindowDuringWindowDraggingInSplitView) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();

  auto window3 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  // In splitview mode, the snapped windows will stay visible during dragging.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);

  // Try to drag a left snapped window
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(0, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EndDrag(shelf_bounds.bottom_left(), /*velocity_y=*/std::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  // Ensure that all windows are restored correctly without triggering auto
  // snapping.
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window1.get()),
            SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window2.get()),
            SnapPosition::kSecondary);
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window3.get()));

  // Try to drag a right snapped window
  StartDrag(window2.get(), shelf_bounds.right_center());
  Drag(gfx::Point(400, 200), 1.f, 1.f);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());
  EndDrag(shelf_bounds.bottom_right(), /*velocity_y=*/std::nullopt);
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
  // Ensure that all windows are restored correctly without triggering auto
  // snapping.
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window1.get()),
            SnapPosition::kPrimary);
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window2.get()),
            SnapPosition::kSecondary);
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window3.get()));
}

// Test home launcher is hidden during dragging.
TEST_F(DragWindowFromShelfControllerTest, HideHomeLauncherDuringDraggingTest) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(0, 200), 0.f, 1.f);
  aura::Window* home_screen_window =
      Shell::Get()->app_list_controller()->GetHomeScreenWindow();
  EXPECT_TRUE(home_screen_window);
  EXPECT_FALSE(home_screen_window->IsVisible());

  EndDrag(shelf_bounds.CenterPoint(),
          /*velocity_y=*/std::nullopt);
  EXPECT_TRUE(home_screen_window->IsVisible());
}

// Test that the "No recent items" label is not visible (not created) while
// dragging from shelf. Regression test for http://b/326091611.
TEST_F(DragWindowFromShelfControllerTest, NoWindowsWidget) {
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(0, 200), 0.f, 1.f);

  OverviewSession* overview_session =
      OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);
  EXPECT_FALSE(overview_session->grid_list()[0]->no_windows_widget());

  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), std::nullopt);
  EXPECT_FALSE(overview_session->grid_list()[0]->no_windows_widget());
}

// Test the windows that were hidden before drag started may or may not reshow,
// depending on different scenarios.
TEST_F(DragWindowFromShelfControllerTest, MayOrMayNotReShowHiddenWindows) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();
  EXPECT_FALSE(window1->GetProperty(kHideDuringWindowDragging));
  EXPECT_FALSE(window2->GetProperty(kHideDuringWindowDragging));

  // If the dragged window restores to its original position, reshow the hidden
  // windows.
  StartDrag(window1.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(window2->GetProperty(kHideDuringWindowDragging));
  EndDrag(shelf_bounds.CenterPoint(), std::nullopt);
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_FALSE(window2->GetProperty(kHideDuringWindowDragging));

  // If fling to homescreen, do not reshow the hidden windows.
  StartDrag(window1.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_TRUE(window2->GetProperty(kHideDuringWindowDragging));
  EXPECT_FALSE(window2->IsVisible());
  EndDrag(gfx::Point(200, 200),
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold);
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window2->GetProperty(kHideDuringWindowDragging));

  // If the dragged window is added to overview, do not reshow the hidden
  // windows.
  window2->Show();
  window1->Show();
  StartDrag(window1.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(window2->GetProperty(kHideDuringWindowDragging));
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window2->GetProperty(kHideDuringWindowDragging));
  ExitOverview();

  // If the dragged window is snapped in splitview, while the other windows are
  // showing in overview, do not reshow the hidden windows.
  window2->Show();
  window1->Show();
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(0, 200), 0.f, 1.f);
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(window2->GetProperty(kHideDuringWindowDragging));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(0, 200), std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window2->GetProperty(kHideDuringWindowDragging));
}

// Test during window dragging, if overview is open, the minimized windows can
// show correctly in overview.
TEST_F(DragWindowFromShelfControllerTest, MinimizedWindowsShowInOverview) {
  UpdateDisplay("500x400");
  auto window3 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();

  StartDrag(window1.get(), GetShelfBounds().CenterPoint());
  // Drag it far enough so overview should be open behind the dragged window.
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsMinimized());
  EXPECT_FALSE(window3->IsVisible());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsMinimized());
  EXPECT_FALSE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window2.get()));
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window3.get()));
  // Release the drag, the window should be added to overview.
  EndDrag(gfx::Point(200, 200), std::nullopt);
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
}

// Test when swiping up from the shelf, we only open overview when the y scroll
// delta (velocity) decrease to kOpenOverviewThreshold or less.
TEST_F(DragWindowFromShelfControllerTest, OpenOverviewWhenHold) {
  UpdateDisplay("500x400");
  auto window = CreateTestWindow();

  StartDrag(window.get(), GetShelfBounds().CenterPoint());
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kOpenOverviewThreshold + 1);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kOpenOverviewThreshold);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(200, 200), std::nullopt);
}

// Test if the dragged window is not dragged far enough than
// |GetReturnToMaximizedThreshold| (the top of the hotseat), it will restore
// back to its original position.
TEST_F(DragWindowFromShelfControllerTest, RestoreWindowToOriginalBounds) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();
  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(window.get())
                                       .bounds();

  // Drag it for a small distance and then release.
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kShowOverviewThreshold + 1);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(200, 400), std::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // Drag it for a large distance and then drag back to release.
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(
      gfx::Point(
          200,
          display_bounds.bottom() -
              DragWindowFromShelfController::GetReturnToMaximizedThreshold() +
              1),
      std::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // The same thing should happen if splitview mode is active.
  auto window2 = CreateTestWindow();
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  StartDrag(window.get(), shelf_bounds.left_center());
  Drag(gfx::Point(0, 200), 0.f, 1.f);
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(0, 400), std::nullopt);
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->primary_window(), window.get());
}

// Test that sequence in b/368630347 doesn't crash.
TEST_F(DragWindowFromShelfControllerTest,
       RestoreWindowToOriginalBoundsInterruptedByHomeScreen) {
  OverviewController* const overview_controller = OverviewController::Get();
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();
  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(window.get())
                                       .bounds();

  // Drag window enough distance to start overview, but not so much that it
  // goes to its target location in the overview grid.
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  // End drag. Window should be restored to its original bounds via a
  // `WindowScaleAnimation`.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EndDrag(
      gfx::Point(
          200,
          display_bounds.bottom() -
              DragWindowFromShelfController::GetReturnToMaximizedThreshold() +
              1),
      std::nullopt);

  // `WindowScaleAnimation` should be running, and while that's happening,
  // user provides some input to go back to the home screen. This should
  // immediately end the ongoing overview session.
  ASSERT_TRUE(Shell::Get()->app_list_controller()->GoHome(
      display::Screen::GetScreen()->GetPrimaryDisplay().id()));
  ASSERT_FALSE(overview_controller->InOverviewSession());

  // User then enters overview through some other method. This should destroy
  // the ongoing `WindowScaleAnimation`.
  ASSERT_TRUE(EnterOverview());
  ASSERT_TRUE(overview_controller->InOverviewSession());
}

// Test if overview is active and splitview is not active, fling in overview may
// or may not head to the home screen.
TEST_F(DragWindowFromShelfControllerTest, FlingInOverview) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();

  // If downward fling velocity is equal or larger than
  // kVelocityToRestoreBoundsThreshold.
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(200, 200), kVelocityToRestoreBoundsThreshold);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // If upward fling velocity is smaller than kVelocityToHomeScreenThreshold,
  // decide where the window should go based on the release position.
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(
      gfx::Point(0, 350),
      std::make_optional(
          -DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));
  // The window should restore back to its original position.
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  // If upward fling velocity is equal or larger than
  // kVelocityToHomeScreenThreshold.
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(0, 350),
          std::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Verify that metrics of home launcher animation are recorded correctly when
// swiping up from shelf with sufficient velocity.
TEST_F(DragWindowFromShelfControllerTest, VerifyHomeLauncherAnimationMetrics) {
  // Set non-zero animation duration to report animation metrics.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  UpdateDisplay("500x400");
  auto window = CreateTestWindow();

  base::HistogramTester histogram_tester;

  // Ensure that fling velocity is sufficient to show homelauncher without
  // triggering overview mode.
  StartDrag(window.get(), GetShelfBounds().CenterPoint());
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kOpenOverviewThreshold + 1);
  EndDrag(gfx::Point(0, 350),
          std::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  WaitForHomeLauncherAnimationToFinish();

  // Verify that animation to show the home launcher is recorded.
  histogram_tester.ExpectTotalCount(
      "Apps.HomeLauncherTransition.AnimationSmoothness.FadeOutOverview", 1);
}

// Test if splitview is active when fling happens, the window will be put in
// overview.
TEST_F(DragWindowFromShelfControllerTest, DragOrFlingInSplitView) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();

  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  OverviewController* overview_controller = OverviewController::Get();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // If the window is only dragged for a small distance:
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(100, 350), std::nullopt);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));

  // If the window is dragged for a long distance:
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(100, 200), std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  ExitOverview();

  // If the window is flung with a small velocity:
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(
      gfx::Point(100, 350),
      std::make_optional(
          -DragWindowFromShelfController::kVelocityToOverviewThreshold + 10));
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));

  // If the window is flung with a large velocity:
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(100, 200), 0.f, 1.f);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EndDrag(gfx::Point(100, 150),
          std::make_optional(
              -DragWindowFromShelfController::kVelocityToOverviewThreshold));
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window2.get()));
  ExitOverview();
}

// Test overview is hidden during dragging and shown when drag slows down or
// stops.
TEST_F(DragWindowFromShelfControllerTest, HideOverviewDuringDragging) {
  UpdateDisplay("500x400");
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();

  StartDrag(window1.get(), GetShelfBounds().CenterPoint());
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // We test the visibility of overview by testing the drop target widget's
  // visibility in the overview.
  OverviewGrid* current_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          window1->GetRootWindow());
  auto* drop_target = current_grid->drop_target();
  EXPECT_TRUE(drop_target);
  EXPECT_EQ(drop_target->item_widget()->GetLayer()->GetTargetOpacity(), 1.f);

  Drag(gfx::Point(200, 200), 0.5f,
       DragWindowFromShelfController::kShowOverviewThreshold + 1);
  // Test that the overview drop target is invisible.
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(drop_target->item_widget()->GetLayer()->GetTargetOpacity(), 0.f);

  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200),
          /*velocity_y=*/std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // |window1| should have added to overview. Test its visibility.
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_EQ(window1->layer()->GetTargetOpacity(), 1.f);
}

// Check the split view drag indicators window dragging states.
TEST_F(DragWindowFromShelfControllerTest,
       SplitViewDragIndicatorsWindowDraggingStates) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();

  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_EQ(1u, overview_session->grid_list().size());
  const SplitViewDragIndicators* drag_indicators =
      overview_session->grid_list()[0]->split_view_drag_indicators();
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromShelf,
            drag_indicators->current_window_dragging_state());

  Drag(gfx::Point(0, 200), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            drag_indicators->current_window_dragging_state());

  Drag(gfx::Point(0, 350), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromShelf,
            drag_indicators->current_window_dragging_state());
  Drag(gfx::Point(0, 200), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            drag_indicators->current_window_dragging_state());
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromShelf,
            drag_indicators->current_window_dragging_state());

  EndDrag(shelf_bounds.CenterPoint(), /*velocity_y=*/std::nullopt);
}

// Tests no crash on dragging from shelf from the split view drag indicators.
// Regression test for http://b/339071708.
TEST_F(DragWindowFromShelfControllerTest, NoCrashOnSplitViewDragIndicators) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();

  // Drag just enough to show the shelf.
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  Drag(gfx::Point(200, 200), 0.f,
       DragWindowFromShelfController::kShowOverviewThreshold + 1);
  ASSERT_FALSE(OverviewController::Get()->InOverviewSession());

  // Enable chromevox to destroy the drag indicators.
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  // Continue dragging to the top.
  Drag(gfx::Point(10, 399), 0.5f, 0.5f);
}

// Test there is no black backdrop behind the dragged window if we're doing the
// scale down animation for the dragged window.
TEST_F(DragWindowFromShelfControllerTest, NoBackdropDuringWindowScaleDown) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  auto window = CreateTestWindow();
  EXPECT_TRUE(window->layer()->GetTargetTransform().IsIdentity());
  WindowBackdrop* window_backdrop = WindowBackdrop::Get(window.get());
  EXPECT_NE(window_backdrop->mode(), WindowBackdrop::BackdropMode::kDisabled);

  StartDrag(window.get(), GetShelfBounds().left_center());
  Drag(gfx::Point(0, 200), 0.f, 10.f);
  EndDrag(gfx::Point(0, 200),
          std::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_FALSE(window->layer()->GetTargetTransform().IsIdentity());
  EXPECT_NE(window_backdrop->mode(), WindowBackdrop::BackdropMode::kDisabled);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
}

// Test that if drag is cancelled, overview should be dismissed and other
// hidden windows should restore to its previous visibility state.
TEST_F(DragWindowFromShelfControllerTest, CancelDragDismissOverview) {
  auto window3 = CreateTestWindow();
  auto window2 = CreateTestWindow();
  auto window1 = CreateTestWindow();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());

  StartDrag(window1.get(), GetShelfBounds().CenterPoint());
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());
  EXPECT_FALSE(window3->IsVisible());

  CancelDrag();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
  EXPECT_TRUE(window3->IsVisible());
}

TEST_F(DragWindowFromShelfControllerTest, CancelDragIfWindowDestroyed) {
  auto window = CreateTestWindow();
  StartDrag(window.get(), GetShelfBounds().CenterPoint());
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EXPECT_EQ(window_drag_controller()->dragged_window(), window.get());
  EXPECT_TRUE(window_drag_controller()->drag_started());
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kDragCanceled, 0);

  window.reset();
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kDragCanceled, 1);

  EXPECT_EQ(window_drag_controller()->dragged_window(), nullptr);
  EXPECT_FALSE(window_drag_controller()->drag_started());
  // No crash should happen if Drag() call still comes in.
  Drag(gfx::Point(200, 200), 0.5f, 0.5f);
  CancelDrag();
}

TEST_F(DragWindowFromShelfControllerTest, FlingWithHiddenHotseat) {
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(
      kHandleDragWindowFromShelfHistogramName,
      ShelfWindowDragResult::kRestoreToOriginalBounds, 0);

  auto window = CreateTestWindow();
  gfx::Point start = GetShelfBounds().CenterPoint();
  StartDrag(window.get(), start);
  // Only drag for a small distance and then fling.
  Drag(gfx::Point(start.x(), start.y() - 10), 0.5f, 0.5f);
  EndDrag(gfx::Point(start.x(), start.y() - 10),
          std::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  // The window should restore back to its original position.
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());

  histogram_tester.ExpectBucketCount(
      kHandleDragWindowFromShelfHistogramName,
      ShelfWindowDragResult::kRestoreToOriginalBounds, 1);

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToHomeScreen, 0);

  // Now a bigger distance to fling.
  StartDrag(window.get(), start);
  Drag(gfx::Point(start.x(), start.y() - 200), 0.5f, 0.5f);
  EndDrag(gfx::Point(start.x(), start.y() - 200),
          std::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  // The window should be minimized.
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToHomeScreen, 1);
}

TEST_F(DragWindowFromShelfControllerTest, DragToSnapMinDistance) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();

  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();

  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(window1.get())
                                       .bounds();
  const int snap_edge_inset =
      DragWindowFromShelfController::kScreenEdgeInsetForSnap;

  base::HistogramTester histogram_tester;
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     0);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     0);

  // If the drag starts outside of the snap region and then into snap region,
  // but the drag distance is not long enough.
  gfx::Point start = gfx::Point(display_bounds.x() + snap_edge_inset + 50,
                                shelf_bounds.CenterPoint().y());
  StartDrag(window1.get(), start);
  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  // Drag into the snap region and release.
  gfx::Point end = gfx::Point(
      start.x() - DragWindowFromShelfController::kMinDragDistance + 10, 200);
  EndDrag(end, std::nullopt);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     1);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     0);

  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // If the drag starts outside of the snap region and then into snap region
  // (kScreenEdgeInsetForSnap), and the drag distance is long enough.
  StartDrag(window1.get(), start);

  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  // Drag into the snap region and release.
  end.set_x(start.x() - 10 - DragWindowFromShelfController::kMinDragDistance);
  EndDrag(end, std::nullopt);

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     1);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     1);

  WindowState::Get(window1.get())->Maximize();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // If the drag starts inside of the snap region (kScreenEdgeInsetForSnap), but
  // the drag distance is not long enough.
  start = gfx::Point(display_bounds.x() + snap_edge_inset - 5,
                     shelf_bounds.CenterPoint().y());
  StartDrag(window1.get(), start);
  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  // Drag for a small distance and release.
  end.set_x(start.x() - 10);
  EndDrag(end, std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     2);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     1);

  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // If the drag starts near the screen edge (kDistanceFromEdge), the window
  // should snap directly.
  start = gfx::Point(
      display_bounds.x() + DragWindowFromShelfController::kDistanceFromEdge - 5,
      shelf_bounds.CenterPoint().y());
  StartDrag(window1.get(), start);
  Drag(start + gfx::Vector2d(0, 100), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  end.set_x(start.x() - 5);
  EndDrag(end, std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));

  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToOverviewMode,
                                     2);
  histogram_tester.ExpectBucketCount(kHandleDragWindowFromShelfHistogramName,
                                     ShelfWindowDragResult::kGoToSplitviewMode,
                                     2);
}

// Test that if overview is invisible when drag ends, the window will either be
// restored or taken to the home screen.
TEST_F(DragWindowFromShelfControllerTest, TestOverviewInvisible) {
  UpdateDisplay("500x400");

  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();

  StartDrag(window.get(), shelf_bounds.left_center());
  Drag(gfx::Point(200, 200), 0.f, 10.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  // End drag without any fling, the window should be added to overview.
  EndDrag(gfx::Point(200, 200), std::nullopt);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window.get()));

  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.left_center());
  Drag(gfx::Point(200, 200), 0.f, 10.f);
  // At this moment overview should be invisible. End the drag without any
  // fling, the window should be taken to home screen.
  EndDrag(gfx::Point(200, 200), std::nullopt);
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());

  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.left_center());
  Drag(gfx::Point(200, 200), 0.f, 10.f);
  // At this moment overview should be invisible. End the drag with upward
  // velocity, the window should be taken to home screen.
  EndDrag(gfx::Point(200, 200), -1.f);
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());

  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.left_center());
  Drag(gfx::Point(200, 200), 0.f, 10.f);
  // At this moment overview should be invisible. End the drag with downward
  // velocity, the window should be restored.
  EndDrag(gfx::Point(200, 200), 1.f);
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

// Test that if overview is invisible when drag ends, the window will be taken
// to the home screen, even if drag satisfied min snap distance.
TEST_F(DragWindowFromShelfControllerTest,
       TestOverviewInvisibleWithMinSnapDistance) {
  UpdateDisplay("500x400");

  auto window = CreateTestWindow();
  const gfx::Rect display_bounds = display::Screen::GetScreen()
                                       ->GetDisplayNearestWindow(window.get())
                                       .bounds();
  int snap_edge_inset =
      display_bounds.width() * kHighlightScreenPrimaryAxisRatio +
      kHighlightScreenEdgePaddingDp;

  // Start the drag outside snap region.
  gfx::Point start = gfx::Point(display_bounds.x() + snap_edge_inset + 70,
                                GetShelfBounds().CenterPoint().y());
  StartDrag(window.get(), start);
  // Drag into the snap region and release without a fling.
  // At this moment overview should be invisible, so the window should be taken
  // to the home screen.
  gfx::Point end =
      start -
      gfx::Vector2d(10 + DragWindowFromShelfController::kMinDragDistance, 200);
  EndDrag(end, std::nullopt);

  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
}

// Test that the original backdrop is restored in the drag window after drag
// ends, no matter where the window ends.
TEST_F(DragWindowFromShelfControllerTest, RestoreBackdropAfterDragEnds) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();
  auto window = CreateTestWindow();
  WindowBackdrop* window_backdrop = WindowBackdrop::Get(window.get());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that ends in overview:
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), std::nullopt);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_controller->overview_session()->IsWindowInOverview(
      window.get()));
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_FALSE(window_backdrop->temporarily_disabled());

  // For window that ends in homescreen:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200),
          std::make_optional(
              -DragWindowFromShelfController::kVelocityToHomeScreenThreshold));
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that restores to its original bounds:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  EndDrag(shelf_bounds.CenterPoint(), std::nullopt);
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that ends in homescreen because overview did not start during
  // the gesture:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EndDrag(gfx::Point(0, 200), std::nullopt);
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);

  // For window that ends in splitscreen:
  wm::ActivateWindow(window.get());
  StartDrag(window.get(), shelf_bounds.CenterPoint());
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_TRUE(window_backdrop->temporarily_disabled());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(0, 200), std::nullopt);
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window.get()));
  EXPECT_EQ(window_backdrop->mode(), WindowBackdrop::BackdropMode::kAuto);
  EXPECT_FALSE(window_backdrop->temporarily_disabled());
}

TEST_F(DragWindowFromShelfControllerTest,
       DoNotChangeActiveWindowDuringDragging) {
  UpdateDisplay("500x400");
  auto window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  StartDrag(window.get(), GetShelfBounds().CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  // During dragging, the active window should not change.
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  EndDrag(gfx::Point(200, 200), std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  EXPECT_TRUE(overview_session->IsWindowInOverview(window.get()));
  // After window is added to overview, the active window should change to the
  // overview focus widget.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());
}

// Test that if the window are dropped in overview before the overview start
// animation is completed, there is no crash.
TEST_F(DragWindowFromShelfControllerTest,
       NoCrashIfDropWindowInOverviewBeforeStartAnimationComplete) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->set_delayed_animation_task_delay_for_test(
      base::Milliseconds(100));

  UpdateDisplay("500x400");
  auto window = CreateTestWindow();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());

  StartDrag(window.get(), GetShelfBounds().CenterPoint());
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  EXPECT_TRUE(overview_controller->InOverviewSession());
  // During dragging, the active window should not change.
  EXPECT_EQ(window.get(), window_util::GetActiveWindow());
  OverviewSession* overview_session = overview_controller->overview_session();
  EndDrag(gfx::Point(200, 200), std::nullopt);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window.get()));
  // After window is added to overview, the active window should change to the
  // overview focus widget.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());

  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  // After start animation is done, active window should remain the same.
  EXPECT_EQ(overview_session->GetOverviewFocusWindow(),
            window_util::GetActiveWindow());
}

// Test that when the dragged window is dropped into overview, it is positioned
// and stacked correctly.
TEST_F(DragWindowFromShelfControllerTest, DropsIntoOverviewAtCorrectPosition) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  ToggleOverview();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window1.get())->target_bounds().CenterPoint()));
  generator->DragMouseTo(0, 400);
  generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window2.get())->target_bounds().CenterPoint()));
  generator->DragMouseTo(799, 400);
  EXPECT_EQ(window1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(window2.get(), split_view_controller()->secondary_window());
  ToggleOverview();
  StartDrag(window1.get(), GetShelfBounds().left_center());
  Drag(gfx::Point(200, 200), 1.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), std::nullopt);

  // Verify the grid arrangement.
  OverviewController* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  const std::vector<raw_ptr<aura::Window, VectorExperimental>>
      expected_mru_list = {window2.get(), window1.get(), window3.get()};
  const std::vector<aura::Window*> expected_overview_list = {
      window2.get(), window1.get(), window3.get()};
  EXPECT_EQ(
      expected_mru_list,
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_EQ(expected_overview_list, GetWindowsListInOverviewGrids());

  // Verify the stacking order.
  aura::Window* parent = window1->parent();
  ASSERT_EQ(parent, window2->parent());
  ASSERT_EQ(parent, window3->parent());
  EXPECT_TRUE(window_util::IsStackedBelow(
      GetOverviewItemForWindow(window1.get())->item_widget()->GetNativeWindow(),
      GetOverviewItemForWindow(window2.get())
          ->item_widget()
          ->GetNativeWindow()));
  EXPECT_TRUE(window_util::IsStackedBelow(
      GetOverviewItemForWindow(window3.get())->item_widget()->GetNativeWindow(),
      GetOverviewItemForWindow(window1.get())
          ->item_widget()
          ->GetNativeWindow()));
}

// Test that when the dragged window is returned to maximized state, the
// overview grid does not animate as it can be jarring and use up unneeded
// resources. Regression test for http://crbug.com/1049206.
TEST_F(DragWindowFromShelfControllerTest, NoAnimationWhenReturnToMaximize) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Drag |window1| so that overview is shown.
  const gfx::Point shelf_centerpoint = GetShelfBounds().CenterPoint();
  StartDrag(window1.get(), shelf_centerpoint);
  Drag(gfx::Point(200, 200), 1.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());

  // Get the bounds and transform of the item associated with |item2|.
  OverviewController* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* item = GetOverviewItemForWindow(window2.get());
  ASSERT_TRUE(item);
  aura::Window* item_window = item->item_widget()->GetNativeWindow();
  const gfx::Rect pre_exit_bounds = item_window->bounds();
  const gfx::Transform pre_exit_transform = item_window->transform();

  // Drag back to the shelf, |window2|'s overview item should not move.
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  EndDrag(shelf_centerpoint, std::nullopt);
  EXPECT_EQ(pre_exit_bounds, item_window->bounds());
  EXPECT_EQ(pre_exit_transform, item_window->layer()->GetTargetTransform());

  // Tests that the end drag actually exited and remaximized |window1|.
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
}

// Tests that when dragging a snapped window is cancelled, the window
// still keep at the original snap position.
TEST_F(DragWindowFromShelfControllerTest,
       KeepSplitWindowSnappedAfterRestoreToOriginalBounds) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();

  auto window1 = CreateTestWindow();
  auto window2 = CreateTestWindow();

  // In splitview mode, the snapped windows will stay visible during dragging.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);

  // Try to drag a left snapped window from shelf, but finally restore to
  // original bounds.
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(0, 200), 1.f, 1.f);
  EndDrag(shelf_bounds.bottom_left(), /*velocity_y=*/std::nullopt);
  // Ensure that the window still keep its initial snap position.
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window1.get()),
            SnapPosition::kPrimary);
  // Try to drag a right snapped window from shelf, and finally drop to
  // overview.
  StartDrag(window2.get(), shelf_bounds.right_center());
  Drag(gfx::Point(400, 200), 1.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  OverviewController* overview_controller = OverviewController::Get();
  OverviewSession* overview_session = overview_controller->overview_session();
  EndDrag(gfx::Point(200, 200), /*velocity_y=*/std::nullopt);
  // Ensure that the window is not in splitview but in overview.
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window2.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));

  // Try to drag the left window again within the restore distance.
  StartDrag(window1.get(), shelf_bounds.left_center());
  Drag(gfx::Point(0, 200), 1.f, 1.f);
  EndDrag(shelf_bounds.bottom_left(), /*velocity_y=*/std::nullopt);
  // Ensure that the left window still keep snapped.
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(window1.get()),
            SnapPosition::kPrimary);
  // Ensure that the right window is still in the overview, and doesn't get
  // minimized.
  EXPECT_FALSE(split_view_controller()->IsWindowInSplitView(window2.get()));
  EXPECT_FALSE(WindowState::Get(window2.get())->IsMinimized());
  EXPECT_TRUE(overview_session->IsWindowInOverview(window2.get()));
}

// Tests that even if the animation for transient child is completed first, the
// transient child will become visible after returning back to the window from
// overview mode. Regression test for https://crbug.com/1240843.
TEST_F(DragWindowFromShelfControllerTest,
       TransientChildWindowIsVisibleAfterMinimizingOnFastAnimation) {
  // Use the fast animation for transient child window to ensure its animation
  // could be done faster than its parent.
  auto enable_fast_animation_for_transient_child =
      WindowScaleAnimation::EnableScopedFastAnimationForTransientChildForTest();

  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("1366x768");

  auto window = CreateTestWindow();
  auto window_transient = CreateTransientModalChildWindow(
      window.get(), gfx::Rect(0, 20, 1366, 728));
  wm::TransientWindowManager::GetOrCreate(window_transient.get())
      ->set_parent_controls_visibility(true);

  StartDrag(window_transient.get(), GetShelfBounds().right_center());
  Drag(gfx::Point(745, 616), 47, -47);
  EndDrag(gfx::Point(1366, 0), -1815.28);
  WaitForHomeLauncherAnimationToFinish();

  aura::Window* home_screen_window =
      Shell::Get()->app_list_controller()->GetHomeScreenWindow();
  EXPECT_TRUE(home_screen_window);
  EXPECT_TRUE(home_screen_window->IsVisible());

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(window_transient->IsVisible());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kExitHomeLauncher);

  ASSERT_TRUE(overview_controller->InOverviewSession());
  auto* overview_grid =
      overview_controller->overview_session()->GetGridWithRootWindow(
          Shell::GetPrimaryRootWindow());
  EXPECT_TRUE(overview_grid);
  const auto& overview_items = overview_grid->item_list();
  ASSERT_EQ(1u, overview_items.size());

  // Press on `overview_item` to exit overview mode and show windows.
  auto* overview_item = overview_items[0].get();
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->GetTransformedBounds().CenterPoint()));
  event_generator->PressTouch();
  event_generator->ReleaseTouch();
  ASSERT_FALSE(overview_controller->InOverviewSession());

  // Both transient child and parent windows should become visible.
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(window_transient->IsVisible());
}

// Tests that dragging a window which has multiple transient child windows from
// shelf should work properly.  Regression test for crash in
// `WindowScaleAnimation::DestroyWindowAnimationObserver`.
// https://crbug.com/1263183
TEST_F(DragWindowFromShelfControllerTest,
       DragWindowWithMultipleTransientChildWindows) {
  // Specify using `ZERO_DURATION` here to make sure the drag will still work
  // even all the animations are no-ops.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);

  auto window = CreateTestWindow();
  auto transient_child_win1 = CreateTransientModalChildWindow(
      window.get(), gfx::Rect(0, 20, 1366, 728));
  auto transient_child_win2 = CreateTransientModalChildWindow(
      window.get(), gfx::Rect(100, 100, 1000, 800));
  wm::TransientWindowManager::GetOrCreate(transient_child_win1.get())
      ->set_parent_controls_visibility(true);
  wm::TransientWindowManager::GetOrCreate(transient_child_win2.get())
      ->set_parent_controls_visibility(true);

  StartDrag(window.get(), GetShelfBounds().right_center());
  Drag(gfx::Point(745, 616), 47, -47);
  EndDrag(gfx::Point(1366, 0), -1815.28);
  WaitForHomeLauncherAnimationToFinish();

  aura::Window* home_screen_window =
      Shell::Get()->app_list_controller()->GetHomeScreenWindow();
  EXPECT_TRUE(home_screen_window);
  EXPECT_TRUE(home_screen_window->IsVisible());

  EXPECT_FALSE(window->IsVisible());
  EXPECT_FALSE(transient_child_win1->IsVisible());
  EXPECT_FALSE(transient_child_win2->IsVisible());
}

// Tests that destroying a trasient child that is being dragged from the shelf
// does not result in a crash. Regression test for https://crbug.com/1200596.
TEST_F(DragWindowFromShelfControllerTest, DestroyTransientWhileAnimating) {
  const gfx::Rect shelf_bounds = GetShelfBounds();

  // The crash occurred while destroying an animating window.
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  // The transient child needs to also be an app window.
  auto window = CreateAppWindow();
  auto child = CreateAppWindow();
  wm::AddTransientChild(window.get(), child.get());

  // Drag the child barely above the shelf so that it returns to its original
  // position on release. The drag can go anywhere as long as the window moves
  // and the release is close to the top of the shelf.
  StartDrag(child.get(), shelf_bounds.right_center());
  Drag(gfx::Point(100, 100), 1.f, 1.f);
  EndDrag(gfx::Point(shelf_bounds.width() / 2, shelf_bounds.y() - 10),
          /*velocity_y=*/std::nullopt);
  ASSERT_TRUE(window->layer()->GetAnimator()->is_animating());
  ASSERT_TRUE(child->layer()->GetAnimator()->is_animating());

  // Destroy the transient child during animation. There should be no crash.
  child.reset();
}

// Tests that destroying a dragged window in split view will not cause crash.
TEST_F(DragWindowFromShelfControllerTest,
       DestroyWindowDuringDraggingInSplitView) {
  UpdateDisplay("500x400");
  const gfx::Rect shelf_bounds = GetShelfBounds();

  // Create a window and snapped to the left in split screen.
  auto window = CreateTestWindow();
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);

  // Try to drag the window from shelf.
  StartDrag(window.get(), shelf_bounds.left_center());
  Drag(gfx::Point(0, 200), 1.f, 1.f);

  // Destroy the window while dragging. Expect no crash.
  window.reset();

  EndDrag(shelf_bounds.CenterPoint(), /*velocity_y=*/std::nullopt);
}

// Tests that there should be no crash if we exit overview by switching desks
// during window dragging. See details in http://b/326074747.
TEST_F(DragWindowFromShelfControllerTest, NoCrashDuringDraggingIfExitOverview) {
  UpdateDisplay("500x400");
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Create a new Desk.
  auto* desk_controller = DesksController::Get();
  desk_controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  const Desk* new_desk = desk_controller->GetDeskAtIndex(1);

  auto window1 = CreateAppWindow();

  StartDrag(window1.get(), GetShelfBounds().CenterPoint());
  // Drag it far enough so overview should be open behind the dragged window.
  Drag(gfx::Point(200, 200), 0.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Switch desks which will end overview.
  desk_controller->ActivateDesk(new_desk,
                                DesksSwitchSource::kDeskButtonMiniViewButton);
  WaitForOverviewExitAnimation();

  // Before desk switch animation is done, continue dragging the window. There
  // should be no crash.
  Drag(gfx::Point(200, 100), 0.f, 1.f);

  EndDrag(GetShelfBounds().CenterPoint(), /*velocity_y=*/std::nullopt);
}

class FloatDragWindowFromShelfControllerTest
    : public DragWindowFromShelfControllerTest {
 public:
  FloatDragWindowFromShelfControllerTest() = default;
  FloatDragWindowFromShelfControllerTest(
      const FloatDragWindowFromShelfControllerTest&) = delete;
  FloatDragWindowFromShelfControllerTest& operator=(
      const FloatDragWindowFromShelfControllerTest&) = delete;
  ~FloatDragWindowFromShelfControllerTest() override = default;

  ui::Layer* GetOtherWindowCopyLayer() {
    return DragWindowFromShelfControllerTestApi().GetOtherWindowCopyLayer(
        window_drag_controller());
  }

  // Creates a floated application window.
  std::unique_ptr<aura::Window> CreateFloatedWindow() {
    std::unique_ptr<aura::Window> floated_window = CreateAppWindow();
    PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
    DCHECK(WindowState::Get(floated_window.get())->IsFloated());
    return floated_window;
  }
};

TEST_F(FloatDragWindowFromShelfControllerTest, DragFloatedWindow) {
  const gfx::Rect shelf_bounds = GetShelfBounds();

  // Create one maximized and one floated window.
  auto maximized_window = CreateTestWindow();
  auto floated_window = CreateFloatedWindow();
  wm::ActivateWindow(floated_window.get());

  const gfx::Point start_drag_point(
      floated_window->GetBoundsInScreen().CenterPoint().x(),
      shelf_bounds.CenterPoint().y());
  // Try to drag the window from shelf.
  StartDrag(floated_window.get(), start_drag_point);
  ui::Layer* other_window_copy_layer = GetOtherWindowCopyLayer();
  ASSERT_TRUE(other_window_copy_layer);

  // To check if the copy is of the maximized window, we check the parent and
  // bounds.
  EXPECT_EQ(maximized_window->layer()->parent(),
            other_window_copy_layer->parent());
  EXPECT_EQ(maximized_window->layer()->bounds(),
            other_window_copy_layer->bounds());

  Drag(gfx::Point(0, 200), 1.f, 1.f);
  EndDrag(shelf_bounds.CenterPoint(), /*velocity_y=*/std::nullopt);
  EXPECT_FALSE(GetOtherWindowCopyLayer());
}

TEST_F(FloatDragWindowFromShelfControllerTest, DragMaximizedWindow) {
  const gfx::Rect shelf_bounds = GetShelfBounds();

  // Create one maximized and one floated window.
  auto maximized_window = CreateTestWindow();
  auto floated_window = CreateFloatedWindow();
  wm::ActivateWindow(maximized_window.get());

  const gfx::Point start_drag_point(
      maximized_window->GetBoundsInScreen().CenterPoint().x(),
      shelf_bounds.CenterPoint().y());
  // Try to drag the window from shelf.
  StartDrag(maximized_window.get(), start_drag_point);
  ui::Layer* other_window_copy_layer = GetOtherWindowCopyLayer();
  ASSERT_TRUE(other_window_copy_layer);

  // To check if the copy is of the floated window, we check the bounds. The
  // float container gets stacked under the desk containers during overview, so
  // the copy should be on a different parent.
  EXPECT_EQ(floated_window->layer()->bounds(),
            other_window_copy_layer->bounds());
  EXPECT_NE(floated_window->layer()->parent(),
            other_window_copy_layer->parent());

  Drag(gfx::Point(0, 200), 1.f, 1.f);
  EndDrag(shelf_bounds.CenterPoint(), /*velocity_y=*/std::nullopt);
  EXPECT_FALSE(GetOtherWindowCopyLayer());
}

// Tests that when dragging from shelf with a floated window into overview, the
// window state does not change on overview exit.
TEST_F(FloatDragWindowFromShelfControllerTest, WindowStatePreserved) {
  // Create one maximized and one floated window.
  auto maximized_window = CreateTestWindow();
  auto floated_window = CreateFloatedWindow();
  wm::ActivateWindow(maximized_window.get());

  // Perform a drag such that we end up in overview.
  const gfx::Point start_drag_point(
      floated_window->GetBoundsInScreen().CenterPoint().x(),
      GetShelfBounds().CenterPoint().y());
  StartDrag(floated_window.get(), start_drag_point);
  Drag(gfx::Point(200, 200), 1.f, 1.f);
  DragWindowFromShelfControllerTestApi().WaitUntilOverviewIsShown(
      window_drag_controller());
  EndDrag(gfx::Point(200, 200), /*velocity_y=*/std::nullopt);

  // Verify that on exiting overview, the original window state is preserved
  // (neither window is minimized).
  OverviewController* overview_controller = OverviewController::Get();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  ExitOverview();
  EXPECT_TRUE(WindowState::Get(maximized_window.get())->IsMaximized());
  EXPECT_TRUE(WindowState::Get(floated_window.get())->IsFloated());
}

// Tests that the correct window (if any) gets chosen by the shelf layout
// manager when there is a floated window.
TEST_F(FloatDragWindowFromShelfControllerTest, DraggingFloatedWindow) {
  UpdateDisplay("800x600");

  auto floated_window = CreateFloatedWindow();

  // Start dragging on the shelf, but not under the floated window. Verify that
  // nothing gets dragged.
  const gfx::Rect shelf_bounds = GetShelfBounds();
  const gfx::Point drag_point_not_under_float(100,
                                              shelf_bounds.CenterPoint().y());
  GetEventGenerator()->PressTouch(drag_point_not_under_float);
  GetEventGenerator()->MoveTouchBy(0, -200);
  auto* drag_controller = GetDragWindowFromShelfController();
  ASSERT_FALSE(drag_controller);
  GetEventGenerator()->ReleaseTouch();

  // Drag under the floated window. Verify that the float window gets dragged.
  const gfx::Rect float_bounds = floated_window->GetBoundsInScreen();
  const gfx::Point drag_point_under_float(
      floated_window->GetBoundsInScreen().CenterPoint().x(),
      shelf_bounds.CenterPoint().y());
  GetEventGenerator()->PressTouch(drag_point_under_float);
  GetEventGenerator()->MoveTouchBy(0, -200);
  drag_controller = GetDragWindowFromShelfController();
  ASSERT_TRUE(drag_controller);
  EXPECT_EQ(floated_window.get(), drag_controller->dragged_window());

  // Move back towards the shelf to ensure we do not enter overview.
  GetEventGenerator()->MoveTouchBy(0, 200);
  GetEventGenerator()->ReleaseTouch();
  ASSERT_FALSE(GetDragWindowFromShelfController()->drag_started());

  // We need to force a layout to start dragging. Drag the window so that it is
  // magnetized to the top edge.
  views::test::RunScheduledLayout(
      NonClientFrameViewAsh::Get(floated_window.get()));
  GetEventGenerator()->PressTouch(float_bounds.top_center() +
                                  gfx::Vector2d(0, 10));
  GetEventGenerator()->MoveTouchBy(0, -100);
  GetEventGenerator()->ReleaseTouch();
  EXPECT_NE(float_bounds, floated_window->GetBoundsInScreen());

  // Since the floated window is magnetized to the top, dragging on the shelf
  // does nothing.
  GetEventGenerator()->PressTouch(drag_point_under_float);
  GetEventGenerator()->MoveTouchBy(0, -200);
  GetEventGenerator()->ReleaseTouch();
  EXPECT_FALSE(GetDragWindowFromShelfController()->drag_started());
}

// Tests that the correct window gets chosen by the shelf layout manager when
// there is a floated and maximized window.
TEST_F(FloatDragWindowFromShelfControllerTest,
       DraggingFloatedAndMaximizedWindow) {
  UpdateDisplay("800x600");

  // Create one maximized and one floated window.
  auto maximized_window = CreateTestWindow();
  auto floated_window = CreateFloatedWindow();
  wm::ActivateWindow(maximized_window.get());

  // Drag under the floated window. Even though the maximized window is active,
  // the floated window is the one that is dragged.
  const gfx::Rect shelf_bounds = GetShelfBounds();
  const gfx::Point drag_point_under_float(
      floated_window->GetBoundsInScreen().CenterPoint().x(),
      shelf_bounds.CenterPoint().y());

  GetEventGenerator()->PressTouch(drag_point_under_float);
  GetEventGenerator()->MoveTouchBy(0, -200);
  auto* drag_controller = GetDragWindowFromShelfController();
  ASSERT_TRUE(drag_controller);
  EXPECT_EQ(floated_window.get(), drag_controller->dragged_window());

  // Move back towards the shelf to ensure we do not enter overview.
  GetEventGenerator()->MoveTouchBy(0, 200);
  GetEventGenerator()->ReleaseTouch();

  // Drag under the maximized window. Even though the floated window is active,
  // the maximized window is the one that is dragged.
  wm::ActivateWindow(floated_window.get());
  const gfx::Point drag_point_under_maximize(100,
                                             shelf_bounds.CenterPoint().y());
  GetEventGenerator()->PressTouch(drag_point_under_maximize);
  GetEventGenerator()->MoveTouchBy(0, -200);
  drag_controller = GetDragWindowFromShelfController();
  ASSERT_TRUE(drag_controller);
  EXPECT_EQ(maximized_window.get(), drag_controller->dragged_window());
}

// Tests that the correct window gets chosen by the shelf layout manager when
// there are floated and snapped windows.
TEST_F(FloatDragWindowFromShelfControllerTest,
       DraggingFloatedAndSnappedWindow) {
  UpdateDisplay("800x600");

  // Create two snapped and one floated window.
  auto left_window = CreateTestWindow();
  auto right_window = CreateTestWindow();
  auto floated_window = CreateFloatedWindow();
  split_view_controller()->SnapWindow(left_window.get(),
                                      SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(right_window.get(),
                                      SnapPosition::kSecondary);

  // Ensure we are in a both snapped state with a floated window.
  wm::ActivateWindow(floated_window.get());
  ASSERT_TRUE(WindowState::Get(left_window.get())->IsSnapped());
  ASSERT_TRUE(WindowState::Get(right_window.get())->IsSnapped());
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Verify that the floated window by default is magnetized to the bottom right
  // corner.
  const gfx::Rect work_area =
      WorkAreaInsets::ForWindow(floated_window.get())->user_work_area_bounds();
  ASSERT_EQ(
      gfx::Point(work_area.right() - chromeos::wm::kFloatedWindowPaddingDp,
                 work_area.bottom() - chromeos::wm::kFloatedWindowPaddingDp),
      floated_window->GetBoundsInScreen().bottom_right());

  // Drag under the floated window. It should be the dragged window.
  const gfx::Rect shelf_bounds = GetShelfBounds();
  const gfx::Point drag_point_under_float(
      floated_window->GetBoundsInScreen().CenterPoint().x(),
      shelf_bounds.CenterPoint().y());
  GetEventGenerator()->PressTouch(drag_point_under_float);
  GetEventGenerator()->MoveTouchBy(0, -200);
  auto* drag_controller = GetDragWindowFromShelfController();
  ASSERT_TRUE(drag_controller);
  EXPECT_EQ(floated_window.get(), drag_controller->dragged_window());
  EXPECT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Move back towards the shelf to ensure we do not enter overview.
  GetEventGenerator()->MoveTouchBy(0, 200);
  GetEventGenerator()->ReleaseTouch();
  EXPECT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Drag under the right snapped window. It should be the dragged window.
  const gfx::Point drag_point_under_right(
      right_window->GetBoundsInScreen().x() + 10,
      shelf_bounds.CenterPoint().y());
  GetEventGenerator()->PressTouch(drag_point_under_right);
  GetEventGenerator()->MoveTouchBy(0, -200);
  drag_controller = GetDragWindowFromShelfController();
  ASSERT_TRUE(drag_controller);
  EXPECT_EQ(right_window.get(), drag_controller->dragged_window());

  // Verify that all the window states remain the same.
  EXPECT_TRUE(WindowState::Get(left_window.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(right_window.get())->IsSnapped());
  EXPECT_TRUE(WindowState::Get(floated_window.get())->IsFloated());
}

}  // namespace ash
