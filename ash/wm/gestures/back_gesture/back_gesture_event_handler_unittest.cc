// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/model/virtual_keyboard_model.h"
#include "ash/test/ash_test_base.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace_controller.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/display/test/display_manager_test_api.h"

namespace ash {

namespace {

void StartKioskSession() {
  SessionInfo info;
  info.is_running_in_app_mode = true;
  info.state = session_manager::SessionState::ACTIVE;
  Shell::Get()->session_controller()->SetSessionInfo(info);
}

}  // namespace

class BackGestureEventHandlerTest : public AshTestBase {
 public:
  // Distance that swiping from left edge to let the affordance achieve
  // activated state.
  static constexpr int kSwipingDistanceForGoingBack = 80;

  explicit BackGestureEventHandlerTest(bool can_go_back = true)
      : can_go_back_(can_go_back) {}
  BackGestureEventHandlerTest(const BackGestureEventHandlerTest&) = delete;
  BackGestureEventHandlerTest& operator=(const BackGestureEventHandlerTest&) =
      delete;
  ~BackGestureEventHandlerTest() override = default;

  void SetUp() override {
    std::unique_ptr<TestShellDelegate> delegate;
    if (!can_go_back_) {
      delegate = std::make_unique<TestShellDelegate>();
      delegate->SetCanGoBack(false);
    }
    AshTestBase::SetUp(std::move(delegate));

    RecreateTopWindow(chromeos::AppType::BROWSER);
    TabletModeControllerTestApi().EnterTabletMode();
  }

  void TearDown() override {
    top_window_.reset();
    AshTestBase::TearDown();
  }

  void RegisterBackPressAndRelease(ui::TestAcceleratorTarget* back_press,
                                   ui::TestAcceleratorTarget* back_release) {
    AcceleratorControllerImpl* controller =
        Shell::Get()->accelerator_controller();

    // Register an accelerator that looks for back presses.
    ui::Accelerator accelerator_back_press(ui::VKEY_BROWSER_BACK, ui::EF_NONE);
    accelerator_back_press.set_key_state(ui::Accelerator::KeyState::PRESSED);
    controller->Register({accelerator_back_press}, back_press);

    // Register an accelerator that looks for back releases.
    ui::Accelerator accelerator_back_release(ui::VKEY_BROWSER_BACK,
                                             ui::EF_NONE);
    accelerator_back_release.set_key_state(ui::Accelerator::KeyState::RELEASED);
    controller->Register({accelerator_back_release}, back_release);
  }

  // Send touch event with |type| to the toplevel window event handler.
  void SendTouchEvent(const gfx::Point& position, ui::EventType type) {
    ui::TouchEvent event =
        ui::TouchEvent(type, position, base::TimeTicks::Now(),
                       ui::PointerDetails(ui::EventPointerType::kTouch,
                                          /*pointer_id=*/5, /*radius_x=*/5.0f,
                                          /*radius_y=*/5.0, /*force=*/1.0f));
    ui::Event::DispatcherApi(&event).set_target(top_window_.get());
    Shell::Get()->back_gesture_event_handler()->OnTouchEvent(&event);
  }

  void RecreateTopWindow(chromeos::AppType app_type) {
    top_window_ = CreateAppWindow(gfx::Rect(), app_type);
  }

  void ResetTopWindow() { top_window_.reset(); }

  // Generates a scroll sequence that will create a back gesture.
  void GenerateBackSequence() {
    GetEventGenerator()->GestureScrollSequence(
        gfx::Point(0, 100), gfx::Point(kSwipingDistanceForGoingBack + 10, 100),
        base::Milliseconds(100), 3);
  }

  TestShellDelegate* GetShellDelegate() {
    return static_cast<TestShellDelegate*>(Shell::Get()->shell_delegate());
  }

  void SendFullscreenEvent(WindowState* window_state) {
    const WMEvent fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
    window_state->OnWMEvent(&fullscreen_event);
  }

  aura::Window* top_window() { return top_window_.get(); }

 private:
  bool can_go_back_;
  std::unique_ptr<aura::Window> top_window_;
};

class BackGestureEventHandlerTestCantGoBack
    : public BackGestureEventHandlerTest {
 public:
  BackGestureEventHandlerTestCantGoBack()
      : BackGestureEventHandlerTest(false) {}
};

TEST_F(BackGestureEventHandlerTest, SwipingFromLeftEdgeToGoBack) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  // Tests that swiping from the left less than |kSwipingDistanceForGoingBack|
  // should not go to previous page.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Point start(0, 100);
  generator->GestureScrollSequence(
      start, gfx::Point(kSwipingDistanceForGoingBack - 10, 100),
      base::Milliseconds(100), 3);
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  // Tests that swiping from the left more than |kSwipingDistanceForGoingBack|
  // should go to previous page.
  generator->GestureScrollSequence(
      start, gfx::Point(kSwipingDistanceForGoingBack + 10, 100),
      base::Milliseconds(100), 3);
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
}

TEST_F(BackGestureEventHandlerTest, FlingFromLeftEdgeToGoBack) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  // Tests that fling from the left with velocity smaller than
  // |kFlingVelocityForGoingBack| should not go to previous page.
  // Drag further than |touch_slop| in GestureDetector to trigger scroll
  // sequence. Note, |touch_slop| equals to 15.05, which is the value of
  // |max_touch_move_in_pixels_for_click_| + |kSlopEpsilon|. Generate the scroll
  // sequence with short duration and only one step for FLING scroll gestures.
  // X-velocity here will be 800 dips/seconds.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->GestureScrollSequence(gfx::Point(0, 0), gfx::Point(16, 0),
                                   base::Milliseconds(20),
                                   /*steps=*/1);
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  // Tests that fling from the left with velocity larger than
  // |kFlingVelocityForGoingBack| should go to previous page. X-velocity here
  // will be 1600 dips/seconds.
  generator->GestureScrollSequence(gfx::Point(0, 0), gfx::Point(16, 0),
                                   base::Milliseconds(1),
                                   /*steps=*/1);
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());

  // Tests that fling from the left with velocity smaller than
  // |kFlingVelocityForGoingBack| but dragged further enough to trigger
  // activated affordance should still go back to previous page. X-velocity here
  // will be 800 dips/seconds and drag distance is 160, which is larger than
  // |kSwipingDistanceForGoingBack|.
  generator->GestureScrollSequence(gfx::Point(0, 0), gfx::Point(160, 0),
                                   base::Milliseconds(200),
                                   /*steps=*/1);
  EXPECT_EQ(2, target_back_press.accelerator_count());
  EXPECT_EQ(2, target_back_release.accelerator_count());
}

TEST_F(BackGestureEventHandlerTestCantGoBack, GoBackInOverviewMode) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  ASSERT_FALSE(WindowState::Get(top_window())->IsMinimized());
  ASSERT_TRUE(window_util::ShouldMinimizeTopWindowOnBack());
  GenerateBackSequence();
  // Should trigger window minimize instead of go back.
  EXPECT_EQ(0, target_back_release.accelerator_count());
  EXPECT_TRUE(WindowState::Get(top_window())->IsMinimized());

  WindowState::Get(top_window())->Unminimize();
  ASSERT_FALSE(WindowState::Get(top_window())->IsMinimized());
  auto* shell = Shell::Get();
  EnterOverview();
  ASSERT_TRUE(shell->overview_controller()->InOverviewSession());
  GenerateBackSequence();
  // Should trigger go back instead of minimize the window since it is in
  // overview mode.
  EXPECT_EQ(1, target_back_release.accelerator_count());

  // Swipe back at overview mode without opened window should still trigger
  // going back.
  ExitOverview();
  ResetTopWindow();
  EnterOverview();
  GenerateBackSequence();
  EXPECT_EQ(2, target_back_release.accelerator_count());
  EXPECT_TRUE(shell->app_list_controller()->IsHomeScreenVisible());
}

TEST_F(BackGestureEventHandlerTest, GoBackInHomeScreenPage) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  auto* shell = Shell::Get();

  // Should not go back if it is not in ACTIVE session.
  ASSERT_FALSE(shell->overview_controller()->InOverviewSession());
  ASSERT_FALSE(shell->app_list_controller()->IsHomeScreenVisible());
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::LOCKED);
  GenerateBackSequence();
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  // Reset the top window to make sure the back behavior in home screen is not
  // because of sending back event to the top window.
  ResetTopWindow();
  GetSessionControllerClient()->SetSessionState(
      session_manager::SessionState::ACTIVE);
  shell->app_list_controller()->GoHome(GetPrimaryDisplay().id());
  ASSERT_TRUE(shell->app_list_controller()->IsHomeScreenVisible());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  GenerateBackSequence();
  // Stay in home screen and none back event will be triggered.
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);

  GetEventGenerator()->GestureTapAt(GetAppListTestHelper()
                                        ->GetAppListView()
                                        ->search_box_view()
                                        ->GetBoundsInScreen()
                                        .CenterPoint());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenSearch);
  GenerateBackSequence();
  // Exit home screen search page and back to |kFullscreenAllApps| state. But
  // this is not triggered by sending back event.
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
  GetAppListTestHelper()->CheckState(AppListViewState::kFullscreenAllApps);
}

TEST_F(BackGestureEventHandlerTest, CancelOnScreenRotation) {
  UpdateDisplay("807x407");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  gfx::Point start(0, 100);
  gfx::Point update_and_end(200, 100);
  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  // Rotate the screen by 270 degree during drag.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Left edge swipe back should be cancelled due to screen rotation, so the
  // fling event with velocity larger than |kFlingVelocityForGoingBack| above
  // will not trigger actual going back.
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
}

// Tests that there is no crash when destroying the window during drag the
// back gesture affordance from the left edge.
TEST_F(BackGestureEventHandlerTest, DestroyWindowDuringDrag) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  gfx::Point start(0, 100);
  gfx::Point update_and_end(200, 100);
  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  ResetTopWindow();
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
}

// Tests back gesture while in split view mode.
TEST_F(BackGestureEventHandlerTest, DragFromSplitViewDivider) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
  ASSERT_TRUE(split_view_controller->InSplitViewMode());
  ASSERT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());

  gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          /*is_dragging=*/false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  // Drag from the splitview divider's non-resizable area with larger than
  // |kSwipingDistanceForGoingBack| distance should trigger back gesture. The
  // snapped window should go to previous page and divider's position will not
  // be changed.
  gfx::Point start(divider_bounds.x(), 10);
  gfx::Point end(start.x() + kSwipingDistanceForGoingBack + 10, 10);
  EXPECT_GT(split_view_controller->GetDividerPosition(),
            0.33f * display_bounds.width());
  EXPECT_LE(split_view_controller->GetDividerPosition(),
            0.5f * display_bounds.width());
  generator->GestureScrollSequence(start, end, base::Milliseconds(100), 3);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
  EXPECT_GT(split_view_controller->GetDividerPosition(),
            0.33f * display_bounds.width());
  EXPECT_LE(split_view_controller->GetDividerPosition(),
            0.5f * display_bounds.width());

  // Drag from the divider's resizable area should trigger splitview resizing.
  // Divider's position will be changed and back gesture should not be
  // triggered.
  start = divider_bounds.CenterPoint();
  end = gfx::Point(0.67f * display_bounds.width(), start.y());
  generator->GestureScrollSequence(start, end, base::Milliseconds(100), 3);
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
  EXPECT_GT(split_view_controller->GetDividerPosition(),
            0.5f * display_bounds.width());
  EXPECT_LE(split_view_controller->GetDividerPosition(),
            0.67f * display_bounds.width());
  split_view_controller->EndSplitView();
}

// Tests that back gesture should always activate the snapped window in split
// view that is underneath the finger in different screen orientations. And that
// the snapped window that is underneath should go back to the previous page.
TEST_F(BackGestureEventHandlerTest, BackGestureInSplitViewMode) {
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  std::unique_ptr<aura::Window> left_window = CreateTestWindow();
  std::unique_ptr<aura::Window> right_window = CreateTestWindow();

  // Start overview first and then snap window in splitview to make sure
  // window activation order remains the same.
  EnterOverview();
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(left_window.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(right_window.get(),
                                    SnapPosition::kSecondary);

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  ASSERT_EQ(right_window.get(), window_util::GetActiveWindow());
  gfx::Point start(0, 10);
  gfx::Point update_and_end(kSwipingDistanceForGoingBack + 10, 10);
  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the left of the display in LandscapePrimary further than
  // |kSwipingDistanceForGoingBack| should activate the physically left snapped
  // window, which is |left_window| and it should go back to the previous page.
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());

  gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          /*is_dragging=*/false);
  start = gfx::Point(divider_bounds.x(), 10);
  update_and_end =
      gfx::Point(divider_bounds.x() + kSwipingDistanceForGoingBack + 10, 10);
  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the split view divider in LandscapePrimary further than
  // |kSwipingDistanceForGoingBack| should activate the physically right snapped
  // window, which is |right_window| and it should go back to the previous page.
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(2, target_back_press.accelerator_count());
  EXPECT_EQ(2, target_back_release.accelerator_count());

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);

  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the split view divider in LandscapeSecondary further than
  // |kSwipingDistanceForGoingBack| should activate the physically right snapped
  // window, which is |left_window| and it should go back to the previous page.
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(3, target_back_press.accelerator_count());
  EXPECT_EQ(3, target_back_release.accelerator_count());

  start = gfx::Point(0, 10);
  update_and_end = gfx::Point(kSwipingDistanceForGoingBack + 10, 10);
  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the left of the display in LandscapeSecondary further than
  // |kSwipingDistanceForGoingBack| should activate the physically left snapped
  // window, which is |right_window| and it should go back to the previous page.
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(4, target_back_press.accelerator_count());
  EXPECT_EQ(4, target_back_release.accelerator_count());

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);

  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the left of the top half of the display in PortraitPrimary
  // further than |kSwipingDistanceForGoingBack| should activate the physically
  // top snapped window, which is |right_window|, and it should go back to the
  // previous page.
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(5, target_back_press.accelerator_count());
  EXPECT_EQ(5, target_back_release.accelerator_count());

  divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          false);
  start = gfx::Point(0, divider_bounds.bottom() + 10);
  update_and_end = gfx::Point(kSwipingDistanceForGoingBack + 10, start.y());
  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the left of the bottom half of the display in PortraitPrimary
  // further than |kSwipingDistanceForGoingBack| should activate the physically
  // bottom snapped window, which is |right_window|, and it should go back to
  // the previous page.
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(6, target_back_press.accelerator_count());
  EXPECT_EQ(6, target_back_release.accelerator_count());

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);

  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the left of the bottom half of the display in
  // PortraitSecondary further than |kSwipingDistanceForGoingBack| should
  // activate the physically bottom snapped window, which is |left_window|, and
  // it should go back to the previous page.
  EXPECT_EQ(left_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(7, target_back_press.accelerator_count());
  EXPECT_EQ(7, target_back_release.accelerator_count());

  start = gfx::Point(0, 10);
  update_and_end = gfx::Point(kSwipingDistanceForGoingBack + 10, 10);
  SendTouchEvent(start, ui::EventType::kTouchPressed);
  SendTouchEvent(update_and_end, ui::EventType::kTouchMoved);
  SendTouchEvent(update_and_end, ui::EventType::kTouchReleased);
  // Swiping from the left of the top half of the display in PortraitSecondary
  // further than |kSwipingDistanceForGoingBack| should activate the physically
  // top snapped window, which is |right_window| and it should go back to the
  // previous page.
  EXPECT_EQ(right_window.get(), window_util::GetActiveWindow());
  EXPECT_EQ(8, target_back_press.accelerator_count());
  EXPECT_EQ(8, target_back_release.accelerator_count());
}

// Tests the back gesture behavior on a non-ARC fullscreened window.
TEST_F(BackGestureEventHandlerTest, FullscreenedWindow) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  WindowState* window_state = WindowState::Get(top_window());
  SendFullscreenEvent(window_state);
  EXPECT_TRUE(window_state->IsFullscreen());

  GenerateBackSequence();
  // First back gesture should let the window exit fullscreen mode instead of
  // triggering go back.
  EXPECT_FALSE(window_state->IsFullscreen());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  GenerateBackSequence();
  // Second back gesture should trigger go back.
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
}

// Tests the back gesture behavior in the Kiosk session.
TEST_F(BackGestureEventHandlerTest, KioskSession) {
  StartKioskSession();

  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  // Make the test window fullscreen to emulate a real Kiosk session, since in
  // the Kiosk session an app window is always fullscreen.
  WindowState* window_state = WindowState::Get(top_window());
  SendFullscreenEvent(window_state);
  EXPECT_TRUE(window_state->IsFullscreen());

  GenerateBackSequence();
  // First back gesture should not let the window exit fullscreen mode, as we do
  // it with a fullscreen window oppened in a user session.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  GenerateBackSequence();
  // Second back gesture should not minimize the window, as we do it with a
  // fullscreen window oppened in a user session.
  EXPECT_FALSE(window_util::ShouldMinimizeTopWindowOnBack());
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
}

// Tests the back gesture behavior on a ARC fullscreened window.
TEST_F(BackGestureEventHandlerTest, ARCFullscreenedWindow) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  RecreateTopWindow(chromeos::AppType::ARC_APP);

  WindowState* window_state = WindowState::Get(top_window());
  SendFullscreenEvent(window_state);
  ASSERT_TRUE(window_state->IsFullscreen());

  auto shelf_visible_hotseat_extended = [this]() -> bool {
    auto* shelf = Shelf::ForWindow(top_window());
    const bool shelf_visible = shelf->GetVisibilityState() == SHELF_VISIBLE;
    const bool hotseat_extended =
        shelf->hotseat_widget()->state() == HotseatState::kExtended;
    return shelf_visible && hotseat_extended;
  };

  GenerateBackSequence();
  // First back gesture should show the shelf instead of triggering go back. The
  // app should remain fullscreened.
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
  EXPECT_TRUE(shelf_visible_hotseat_extended());

  // Tapping on a point on the screen should hide the shelf and hotseat.
  GetEventGenerator()->GestureTapAt(gfx::Point(100, 100));
  EXPECT_FALSE(shelf_visible_hotseat_extended());

  // Send another back gesture to bring up the shelf and hotseat.
  GenerateBackSequence();
  EXPECT_TRUE(window_state->IsFullscreen());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
  EXPECT_TRUE(shelf_visible_hotseat_extended());

  GenerateBackSequence();
  // Second back gesture in a row should trigger go back. Fullscreen will be
  // dependent on how the app choses to handle the back event.
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
}

// Tests the back gesture behavior when a Chrome OS IME is visible.
TEST_F(BackGestureEventHandlerTest, BackGestureWithCrosKeyboardTest) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  KeyboardController* keyboard_controller = KeyboardController::Get();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kExtensionEnabled);
  // The keyboard needs to be in a loaded state before being shown.
  ASSERT_TRUE(keyboard::test::WaitUntilLoaded());
  keyboard_controller->ShowKeyboard();
  EXPECT_TRUE(keyboard_controller->IsKeyboardVisible());

  GenerateBackSequence();
  // First back gesture should hide the virtual keyboard.
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  GenerateBackSequence();
  // Second back gesture should trigger go back.
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
}

// Tests that the back gesture works properly on the split view divider bar both
// inside and outside of cros virtual keyboard.
TEST_F(BackGestureEventHandlerTest,
       BackGestureWithCrosKeyboardInSplitViewTest) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  std::unique_ptr<aura::Window> left_window = CreateTestWindow();
  std::unique_ptr<aura::Window> right_window = CreateTestWindow();
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(left_window.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(right_window.get(),
                                    SnapPosition::kSecondary);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());

  KeyboardController* keyboard_controller = KeyboardController::Get();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kExtensionEnabled);
  // The keyboard needs to be in a loaded state before being shown.
  ASSERT_TRUE(keyboard::test::WaitUntilLoaded());
  keyboard_controller->ShowKeyboard();
  EXPECT_TRUE(keyboard_controller->IsKeyboardVisible());

  // Get the keyboard bounds:
  keyboard::KeyboardUIController* keyboard_ui_controller =
      keyboard::KeyboardUIController::Get();
  EXPECT_TRUE(keyboard_ui_controller->IsKeyboardVisible());
  gfx::Rect keyboard_bounds = keyboard_ui_controller->GetVisualBoundsInScreen();

  // Start dragging from a position that is right outside the divider bar bounds
  // and outside the VK bounds.
  gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          false);
  gfx::Point start = gfx::Point(divider_bounds.CenterPoint().x(), 10);
  EXPECT_FALSE(keyboard_bounds.Contains(start));
  gfx::Point end =
      gfx::Point(start.x() + kSwipingDistanceForGoingBack + 10, start.y());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100), 3);
  // Virtual keyboard should be closed.
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());
  EXPECT_FALSE(keyboard_controller->IsKeyboardVisible());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());

  // Start dragging from the split view divider bar position that is inside the
  // VK bounds.
  keyboard_controller->ShowKeyboard();
  EXPECT_TRUE(keyboard_controller->IsKeyboardVisible());
  start = gfx::Point(divider_bounds.CenterPoint().x(),
                     keyboard_bounds.CenterPoint().y());
  EXPECT_TRUE(keyboard_bounds.Contains(start));
  end = gfx::Point(start.x() + kSwipingDistanceForGoingBack + 10, start.y());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100), 3);
  // Nothing should happen.
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());
  EXPECT_TRUE(keyboard_controller->IsKeyboardVisible());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
}

// Tests the back gesture behavior when an Android IME is visible. Due to the
// way the Android IME is implemented, a lot of this test is fake behavior, but
// it will help catch regressions.
TEST_F(BackGestureEventHandlerTest, BackGestureWithAndroidKeyboardTest) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  WindowState* window_state = WindowState::Get(top_window());
  ASSERT_FALSE(window_state->IsMinimized());

  VirtualKeyboardModel* keyboard =
      Shell::Get()->system_tray_model()->virtual_keyboard();
  ASSERT_TRUE(keyboard);
  // Fakes showing the keyboard.
  keyboard->OnArcInputMethodBoundsChanged(gfx::Rect(400, 400));
  EXPECT_TRUE(keyboard->arc_keyboard_visible());

  // Unfortunately we cannot hook this all the wall up to see if the Android IME
  // is hidden, but we can check that back key events are generated and the top
  // window is not minimized.
  GenerateBackSequence();
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
  EXPECT_FALSE(window_state->IsMinimized());
}

// Tests that the back gesture works properly on the split view divider bar both
// inside and outside of Android virtual keyboard.
TEST_F(BackGestureEventHandlerTest,
       BackGestureWithAndroidKeyboardInSplitViewTest) {
  UpdateDisplay("800x600");
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  std::unique_ptr<aura::Window> left_window = CreateTestWindow();
  std::unique_ptr<aura::Window> right_window = CreateTestWindow();
  auto* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(left_window.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(right_window.get(),
                                    SnapPosition::kSecondary);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());

  VirtualKeyboardModel* keyboard =
      Shell::Get()->system_tray_model()->virtual_keyboard();
  ASSERT_TRUE(keyboard);
  // Fakes showing the keyboard.
  gfx::Rect keyboard_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          left_window.get());
  keyboard_bounds.set_y(keyboard_bounds.bottom() - 200);
  keyboard_bounds.set_height(200);
  keyboard->OnArcInputMethodBoundsChanged(keyboard_bounds);
  EXPECT_TRUE(keyboard->arc_keyboard_visible());

  // Start dragging from the split view divider bar position that is outside the
  // VK bounds.
  gfx::Rect divider_bounds =
      split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
          false);
  gfx::Point start = gfx::Point(divider_bounds.CenterPoint().x(), 10);
  EXPECT_FALSE(keyboard_bounds.Contains(start));
  gfx::Point end =
      gfx::Point(start.x() + kSwipingDistanceForGoingBack + 10, start.y());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100), 3);
  // Virtual keyboard should be closed. But Unfortunately we cannot hook
  // this all the wall up to see if the Android IME is hidden, but we can check
  // that back key events are generated and we're still in both snapped split
  // view state.
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());

  // Start dragging from the split view divider bar position that is inside the
  // VK bounds.
  target_back_press.ResetCounts();
  target_back_release.ResetCounts();
  keyboard->OnArcInputMethodBoundsChanged(keyboard_bounds);
  EXPECT_TRUE(keyboard->arc_keyboard_visible());
  start = gfx::Point(divider_bounds.CenterPoint().x(),
                     keyboard_bounds.CenterPoint().y());
  EXPECT_TRUE(keyboard_bounds.Contains(start));
  end = gfx::Point(start.x() + kSwipingDistanceForGoingBack + 10, start.y());
  GetEventGenerator()->GestureScrollSequence(start, end,
                                             base::Milliseconds(100), 3);
  // Nothing should happen.
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller->state());
  EXPECT_EQ(0, target_back_press.accelerator_count());
  EXPECT_EQ(0, target_back_release.accelerator_count());
}

TEST_F(BackGestureEventHandlerTest, IgnoreSecondFinger) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  const gfx::Point start_point(0, 100);
  const gfx::Point end_point(200, 100);

  // Scenario 1:
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressTouchId(0, std::make_optional(start_point));
  generator->MoveTouch(end_point);
  // Without releasing the first finger, now press and release the second
  // finger.
  generator->PressTouchId(1);
  generator->ReleaseTouchId(1);
  // Then release the first finger. Back should be able to be performed.
  generator->ReleaseTouchId(0);
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());

  // Scenario 2:
  wm::ActivateWindow(top_window());
  generator->PressTouchId(0, std::make_optional(start_point));
  generator->MoveTouch(end_point);
  // Without releasing the first finger, now press the second finger.
  generator->PressTouchId(1);
  // Release the first finger and then the second finger.
  generator->ReleaseTouchId(0);
  generator->ReleaseTouchId(1);
  // Test that back should still be able to be performed.
  EXPECT_EQ(2, target_back_press.accelerator_count());
  EXPECT_EQ(2, target_back_release.accelerator_count());

  // Scenario 3:
  wm::ActivateWindow(top_window());
  GetShellDelegate()->SetShouldWaitForTouchAck(
      /*should_wait_for_touch_ack=*/true);
  generator->PressTouchId(0, std::make_optional(start_point));
  generator->MoveTouch(end_point);
  // Without releasing the first finger, now press and release the second
  // finger.
  generator->PressTouchId(1);
  generator->ReleaseTouchId(1);
  // Then release the first finger. Back should be able to be performed.
  generator->ReleaseTouchId(0);
  EXPECT_EQ(3, target_back_press.accelerator_count());
  EXPECT_EQ(3, target_back_release.accelerator_count());

  // Scenario 4:
  wm::ActivateWindow(top_window());
  generator->PressTouchId(0, std::make_optional(start_point));
  generator->MoveTouch(end_point);
  // Without releasing the first finger, now press the second finger.
  generator->PressTouchId(1);
  // Release the first finger and then the second finger.
  generator->ReleaseTouchId(0);
  generator->ReleaseTouchId(1);
  // Test that back should still be able to be performed.
  EXPECT_EQ(4, target_back_press.accelerator_count());
  EXPECT_EQ(4, target_back_release.accelerator_count());
}

TEST_F(BackGestureEventHandlerTest, CancelledEventOnSecondFinger) {
  ui::TestAcceleratorTarget target_back_press, target_back_release;
  RegisterBackPressAndRelease(&target_back_press, &target_back_release);

  const gfx::Point start_point(0, 100);
  const gfx::Point end_point(200, 100);

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressTouchId(0, std::make_optional(start_point));
  generator->MoveTouch(end_point);
  // Without releasing the first finger, now press the second finger.
  generator->PressTouchId(1);
  // Then release the first finger. Back should be able to be performed.
  generator->ReleaseTouchId(0);
  EXPECT_EQ(1, target_back_press.accelerator_count());
  EXPECT_EQ(1, target_back_release.accelerator_count());
  generator->ReleaseTouchId(1);
  // Manually dispatch a ui::EventType::kTouchCancelled event to the second
  // finger to simulate what's happending in real world.
  ui::TouchEvent event = ui::TouchEvent(
      ui::EventType::kTouchCancelled, start_point, base::TimeTicks::Now(),
      ui::PointerDetails(ui::EventPointerType::kTouch,
                         /*pointer_id=*/1, /*radius_x=*/5.0f,
                         /*radius_y=*/5.0, /*force=*/1.0f));
  ui::Event::DispatcherApi(&event).set_target(top_window());
  Shell::Get()->back_gesture_event_handler()->OnTouchEvent(&event);

  wm::ActivateWindow(top_window());
  generator->PressTouchId(0, std::make_optional(start_point));
  generator->MoveTouch(end_point);
  generator->ReleaseTouchId(0);
  // Test that back should still be able to be performed.
  EXPECT_EQ(2, target_back_press.accelerator_count());
  EXPECT_EQ(2, target_back_release.accelerator_count());
}

// Tests that swiping on the backdrop to minimize a non-resizable app will not
// cause a crash. Regression test for http://crbug.com/1064618.
TEST_F(BackGestureEventHandlerTestCantGoBack, NonResizableApp) {
  // Make the top window non-resizable and set its bounds so that the backdrop
  // will take the gesture events.
  top_window()->SetProperty(aura::client::kResizeBehaviorKey,
                            aura::client::kResizeBehaviorCanMinimize);

  WindowState* window_state = WindowState::Get(top_window());
  window_state->Restore();
  SetBoundsWMEvent bounds_event(gfx::Rect(200, 100, 300, 300));
  window_state->OnWMEvent(&bounds_event);
  ASSERT_FALSE(window_state->IsMinimized());

  // Check that the backdrop is visible.
  WorkspaceController* workspace_controller =
      GetWorkspaceControllerForContext(top_window());
  WorkspaceLayoutManager* layout_manager =
      workspace_controller->layout_manager();
  BackdropController* backdrop_controller =
      layout_manager->backdrop_controller();
  aura::Window* backdrop_window = backdrop_controller->backdrop_window();
  ASSERT_TRUE(backdrop_window);
  ASSERT_TRUE(backdrop_window->IsVisible());

  // Generate a back seqeuence. There should be no crash.
  GenerateBackSequence();
  EXPECT_TRUE(window_state->IsMinimized());
}

TEST_F(BackGestureEventHandlerTestCantGoBack, NonAppAndSystemApps) {
  RecreateTopWindow(chromeos::AppType::NON_APP);
  GenerateBackSequence();
  EXPECT_TRUE(WindowState::Get(top_window())->IsMinimized());

  RecreateTopWindow(chromeos::AppType::SYSTEM_APP);
  GenerateBackSequence();
  EXPECT_TRUE(WindowState::Get(top_window())->IsMinimized());
}

// Tests that the back gesture will force minimize even non minimizeable apps.
TEST_F(BackGestureEventHandlerTestCantGoBack, NonMinimizeableApp) {
  // Make the top window non minimizeable.
  top_window()->SetProperty(aura::client::kResizeBehaviorKey,
                            aura::client::kResizeBehaviorNone);
  GenerateBackSequence();
  EXPECT_TRUE(WindowState::Get(top_window())->IsMinimized());
}

}  // namespace ash
