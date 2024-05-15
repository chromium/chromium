// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/public/cpp/accelerators.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/style/close_button.h"
#include "ash/style/system_toast_style.h"
#include "ash/system/toast/toast_manager_impl.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test_shell_delegate.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_focus_cycler_old.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_group_item.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/snap_group/snap_group_constants.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/splitview/faster_split_view.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_divider_view.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_list.h"
#include "ash/wm/window_cycle/window_cycle_view.h"
#include "ash/wm/window_mini_view.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_modality_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/window_move_client.h"

namespace ash {

namespace {

using chromeos::WindowStateType;
using testing::ElementsAre;
using ui::mojom::CursorType;
using WindowCyclingDirection = WindowCycleController::WindowCyclingDirection;

SplitViewController* split_view_controller() {
  return SplitViewController::Get(Shell::GetPrimaryRootWindow());
}

SplitViewDivider* split_view_divider() {
  return split_view_controller()->split_view_divider();
}

SplitViewDivider* snap_group_divider() {
  auto* top_snap_group = SnapGroupController::Get()->GetTopmostSnapGroup();
  return top_snap_group ? top_snap_group->snap_group_divider() : nullptr;
}

gfx::Rect split_view_divider_bounds_in_screen() {
  return split_view_divider()->GetDividerBoundsInScreen(
      /*is_dragging=*/false);
}

gfx::Rect snap_group_divider_bounds_in_screen() {
  return snap_group_divider()->GetDividerBoundsInScreen(
      /*is_dragging=*/false);
}

const gfx::Rect work_area_bounds() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
}

const gfx::Rect GetWorkAreaBoundsForWindow(aura::Window* window) {
  return display::Screen::GetScreen()
      ->GetDisplayNearestWindow(window)
      .work_area();
}

void SwitchToTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
}

void ExitTabletMode() {
  TabletModeControllerTestApi().LeaveTabletMode();
}

gfx::Rect GetOverviewGridBounds(aura::Window* root_window) {
  OverviewSession* overview_session = GetOverviewSession();
  return overview_session ? OverviewGridTestApi(root_window).bounds()
                          : gfx::Rect();
}

void SnapOneTestWindow(
    aura::Window* window,
    WindowStateType state_type,
    float snap_ratio,
    WindowSnapActionSource snap_action_source = WindowSnapActionSource::kTest) {
  WindowState* window_state = WindowState::Get(window);
  const WindowSnapWMEvent snap_event(
      state_type == WindowStateType::kPrimarySnapped ? WM_EVENT_SNAP_PRIMARY
                                                     : WM_EVENT_SNAP_SECONDARY,
      snap_ratio, snap_action_source);
  window_state->OnWMEvent(&snap_event);
  EXPECT_EQ(state_type, window_state->GetStateType());
}

// Verifies that `window` is in split view overview, where `window` is
// excluded from overview, and overview occupies the work area opposite of
// `window`. Returns the corresponding `SplitViewOverviewSession` if exits and
// nullptr otherwise. `faster_split_screen_setup` specifies whether the
// `SplitViewOverviewSession` is initiated by faster split screen set up or not,
// where behaviors differ such as overview widget.
SplitViewOverviewSession* VerifySplitViewOverviewSession(
    aura::Window* window,
    bool faster_split_screen_setup = true) {
  auto* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(
      overview_controller->overview_session()->IsWindowInOverview(window));

  SplitViewOverviewSession* split_view_overview_session =
      RootWindowController::ForWindow(window)->split_view_overview_session();
  EXPECT_TRUE(split_view_overview_session);
  gfx::Rect expected_grid_bounds = GetWorkAreaBoundsForWindow(window);
  expected_grid_bounds.Subtract(window->GetBoundsInScreen());

  if (split_view_divider() && split_view_divider()->divider_widget()) {
    expected_grid_bounds.Subtract(split_view_divider_bounds_in_screen());
  }

  // Clamp the length on the side that can be shrunk by resizing to avoid going
  // below the threshold i.e. 1/3 of the corresponding work area length.
  const bool is_horizontal = IsLayoutHorizontal(Shell::GetPrimaryRootWindow());
  const int min_length = (is_horizontal ? work_area_bounds().width()
                                        : work_area_bounds().height()) /
                         3;
  if (is_horizontal) {
    expected_grid_bounds.set_width(
        std::max(expected_grid_bounds.width(), min_length));
  } else {
    expected_grid_bounds.set_height(
        std::max(expected_grid_bounds.height(), min_length));
  }

  auto* root_window = window->GetRootWindow();
  if (!Shell::Get()->IsInTabletMode()) {
    EXPECT_EQ(expected_grid_bounds, GetOverviewGridBounds(root_window));
  }

  EXPECT_TRUE(
      expected_grid_bounds.Contains(GetOverviewGridBounds(root_window)));

  if (!Shell::Get()->IsInTabletMode() && faster_split_screen_setup) {
    auto* overview_grid = GetOverviewGridForRoot(window->GetRootWindow());
    EXPECT_TRUE(overview_grid->faster_splitview_widget());
    EXPECT_FALSE(overview_grid->no_windows_widget());
    EXPECT_FALSE(overview_grid->GetSaveDeskButtonContainer());
    EXPECT_FALSE(overview_grid->desks_bar_view());
  }

  return split_view_overview_session;
}

// Maximize the snapped window which will exit the split view session. This is
// used in preparation for the next round of testing.
void MaximizeToClearTheSession(aura::Window* window) {
  WindowState* window_state = WindowState::Get(window);
  window_state->Maximize();
  SplitViewOverviewSession* split_view_overview_session =
      RootWindowController::ForWindow(window)->split_view_overview_session();
  EXPECT_FALSE(split_view_overview_session);
}

// Selects the overview item for `window`.
void ClickOverviewItem(ui::test::EventGenerator* event_generator,
                       aura::Window* window) {
  event_generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window)->GetTransformedBounds().CenterPoint()));
  event_generator->ClickLeftButton();
}

// Drag the given group `item` to the `screen_location`. This is added before
// the event handling of the middle seam is done.
void DragGroupItemToPoint(OverviewItemBase* item,
                          const gfx::Point& screen_location,
                          ui::test::EventGenerator* event_generator,
                          bool by_touch_gestures,
                          bool drop) {
  DCHECK(item);

  gfx::Point location =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
  // TODO(michelefan): Use the center point of the `overview_item` after
  // implementing or defining the event handling for the middle seam area.
  location.Offset(/*delta_x=*/5, /*delta_y=*/5);
  event_generator->set_current_screen_location(location);
  if (by_touch_gestures) {
    event_generator->PressTouch();
    event_generator->MoveTouchBy(50, 0);
    event_generator->MoveTouch(screen_location);
    if (drop) {
      event_generator->ReleaseTouch();
    }
  } else {
    event_generator->PressLeftButton();
    Shell::Get()->cursor_manager()->SetDisplay(
        display::Screen::GetScreen()->GetDisplayNearestPoint(screen_location));
    event_generator->MoveMouseTo(screen_location);
    if (drop) {
      event_generator->ReleaseLeftButton();
    }
  }
}

// Gets a point to drag `window` by the header.
gfx::Point GetDragPoint(aura::Window* window) {
  auto* frame = NonClientFrameViewAsh::Get(window);
  views::test::RunScheduledLayout(frame);
  return frame->GetHeaderView()->GetBoundsInScreen().CenterPoint();
}

// Verifies that the union bounds of `w1`, `w2` and the divider are equal to
// the bounds of the work area with no overlap.
void UnionBoundsEqualToWorkAreaBounds(aura::Window* w1,
                                      aura::Window* w2,
                                      SplitViewDivider* divider) {
  gfx::Rect w1_bounds(w1->GetTargetBounds());
  wm::ConvertRectToScreen(w1->GetRootWindow(), &w1_bounds);
  gfx::Rect w2_bounds(w2->GetTargetBounds());
  wm::ConvertRectToScreen(w2->GetRootWindow(), &w2_bounds);

  const auto divider_bounds =
      divider->GetDividerBoundsInScreen(/*is_dragging=*/false);
  EXPECT_FALSE(w1_bounds.IsEmpty());
  EXPECT_FALSE(w2_bounds.IsEmpty());
  EXPECT_FALSE(divider_bounds.IsEmpty());

  gfx::Rect union_bounds;
  union_bounds.Union(w1_bounds);
  union_bounds.Union(w2_bounds);
  EXPECT_FALSE(w1_bounds.Contains(divider_bounds));
  EXPECT_FALSE(w2_bounds.Contains(divider_bounds));
  if (IsLayoutHorizontal(w1)) {
    EXPECT_EQ(w1_bounds.right(), divider_bounds.x());
    EXPECT_EQ(w2_bounds.x(), divider_bounds.right());
  } else {
    EXPECT_EQ(w1_bounds.bottom(), divider_bounds.y());
    EXPECT_EQ(w2_bounds.y(), divider_bounds.bottom());
  }

  union_bounds.Union(divider_bounds);
  EXPECT_EQ(
      display::Screen::GetScreen()->GetDisplayNearestWindow(w1).work_area(),
      union_bounds);
}

// Verifies the windows and divider of `snap_group` are on `display_id`.
void VerifySnapGroupOnDisplay(SnapGroup* snap_group, const int64_t display_id) {
  aura::Window* root = snap_group->window1()->GetRootWindow();
  EXPECT_EQ(root, snap_group->window2()->GetRootWindow());
  EXPECT_EQ(root, snap_group->snap_group_divider()->GetRootWindow());
  EXPECT_EQ(display_id,
            display::Screen::GetScreen()->GetDisplayNearestWindow(root).id());
}

}  // namespace

// -----------------------------------------------------------------------------
// FasterSplitScreenTest:

// Test fixture to verify faster split screen feature.

class FasterSplitScreenTest : public OverviewTestBase {
 public:
  FasterSplitScreenTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kFasterSplitScreenSetup,
                              features::kOsSettingsRevampWayfinding},
        /*disabled_features=*/{});
  }
  FasterSplitScreenTest(const FasterSplitScreenTest&) = delete;
  FasterSplitScreenTest& operator=(const FasterSplitScreenTest&) = delete;
  ~FasterSplitScreenTest() override = default;

  // AshTestBase:
  void SetUp() override {
    OverviewTestBase::SetUp();
    WindowCycleList::SetDisableInitialDelayForTesting(true);
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that if the user disables the pref for snap window suggestions, we
// don't start partial overview.
TEST_F(FasterSplitScreenTest, DisableSnapWindowSuggestionsPref) {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();

  pref->SetBoolean(prefs::kSnapWindowSuggestions, false);
  ASSERT_FALSE(pref->GetBoolean(prefs::kSnapWindowSuggestions));

  // Snap a window. Test we don't start overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(FasterSplitScreenTest, Basic) {
  // Create two test windows, snap `w1`. Test `w1` is snapped and excluded from
  // overview while `w2` is in overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  auto* overview_controller = Shell::Get()->overview_controller();
  EXPECT_TRUE(
      overview_controller->overview_session()->IsWindowInOverview(w2.get()));

  // Select `w2` from overview. Test `w2` auto snaps.
  ClickOverviewItem(GetEventGenerator(), w2.get());
  WaitForOverviewExitAnimation();
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Create a new `w3` and snap it to the left. Test it doesn't start overview.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Create a new `w4` and snap it to the right. Test it doesn't start overview.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w4.get())->GetStateType());

  // Test all the other window states remain the same.
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w3.get())->GetStateType());

  // Enter overview normally. Test that no windows widget will not show.
  ToggleOverview();
  auto* overview_grid = GetOverviewGridForRoot(w1->GetRootWindow());
  EXPECT_FALSE(overview_grid->no_windows_widget());
  EXPECT_FALSE(overview_grid->faster_splitview_widget());
}

// Tests that on one window snapped, `SnapGroupController` starts
// `SplitViewOverviewSession` (snap group creation session).
TEST_F(FasterSplitScreenTest, CloseSnappedWindowEndsSplitViewOverviewSession) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  // Snap `w1` to the left. Test that we are in split view overview, excluding
  // `w1` and taking half the screen.
  SnapOneTestWindow(w1.get(),
                    /*state_type=*/WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // Close `w1`. Test that we end overview.
  w1.reset();
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Tests that faster split screen can only start with certain snap action
// sources.
TEST_F(FasterSplitScreenTest, SnapActionSourceLimitations) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  struct {
    WindowSnapActionSource snap_action_source;
    bool should_show_partial_overview;
  } kTestCases[]{
      {WindowSnapActionSource::kSnapByWindowLayoutMenu,
       /*should_show_partial_overview=*/true},
      {WindowSnapActionSource::kDragWindowToEdgeToSnap,
       /*should_show_partial_overview=*/true},
      {WindowSnapActionSource::kLongPressCaptionButtonToSnap,
       /*should_show_partial_overview=*/true},
      {WindowSnapActionSource::kLacrosSnapButtonOrWindowLayoutMenu,
       /*should_show_partial_overview=*/true},
      {WindowSnapActionSource::kKeyboardShortcutToSnap,
       /*should_show_partial_overview=*/false},
      {WindowSnapActionSource::kSnapByWindowStateRestore,
       /*should_show_partial_overview=*/false},
      {WindowSnapActionSource::kSnapByFullRestoreOrDeskTemplateOrSavedDesk,
       /*should_show_partial_overview=*/false},
  };

  for (const auto test_case : kTestCases) {
    SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                      chromeos::kDefaultSnapRatio,
                      test_case.snap_action_source);
    EXPECT_EQ(test_case.should_show_partial_overview, IsInOverviewSession());
    MaximizeToClearTheSession(w1.get());
  }
}

TEST_F(FasterSplitScreenTest, CycleSnap) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  auto* window_state = WindowState::Get(w1.get());

  // Cycle snap to the left.
  const WindowSnapWMEvent cycle_snap_primary(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state->OnWMEvent(&cycle_snap_primary);
  auto* overview_controller = Shell::Get()->overview_controller();
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Cycle snap to the right.
  const WindowSnapWMEvent cycle_snap_secondary(WM_EVENT_CYCLE_SNAP_SECONDARY);
  window_state->OnWMEvent(&cycle_snap_secondary);
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

TEST_F(FasterSplitScreenTest, EndSplitViewOverviewSession) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // Drag `w1` out of split view. Test it ends overview.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseBy(10, 10);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Snap then minimize the window. Test it ends overview.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(w1.get())->OnWMEvent(&minimize_event);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Snap then close the window. Test it ends overview.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  w1.reset();
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

TEST_F(FasterSplitScreenTest, ResizeSplitViewOverviewAndWindow) {
  UpdateDisplay("900x600");
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  const gfx::Rect initial_bounds(w1->GetBoundsInScreen());

  // Drag the right edge of the window to resize the window and overview at the
  // same time. Test that the bounds are updated.
  const gfx::Point start_point(w1->GetBoundsInScreen().right_center());
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(start_point);

  // Resize to less than 1/3. Test we don't end overview.
  const auto drag_point1 =
      gfx::Point(work_area_bounds().width() * chromeos::kOneThirdSnapRatio - 10,
                 start_point.y());
  generator->DragMouseTo(drag_point1);
  gfx::Rect expected_window_bounds(initial_bounds);
  expected_window_bounds.set_width(drag_point1.x());
  EXPECT_EQ(expected_window_bounds, w1->GetBoundsInScreen());
  VerifySplitViewOverviewSession(w1.get());

  // Resize to greater than 2/3. Test we don't end overview.
  const auto drag_point2 =
      gfx::Point(work_area_bounds().width() * chromeos::kTwoThirdSnapRatio + 10,
                 start_point.y());
  generator->DragMouseTo(drag_point2);
  expected_window_bounds.set_width(drag_point2.x());
  EXPECT_EQ(expected_window_bounds, w1->GetBoundsInScreen());
  VerifySplitViewOverviewSession(w1.get());
}

// Tests that drag to snap window -> resize window -> snap window again restores
// to the default snap ratio. Regression test for b/315039407.
TEST_F(FasterSplitScreenTest, ResizeThenDragToSnap) {
  // Create `w2` first, as `w1` will be created on top and we want to drag it.
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());

  // Drag to snap `w1` to 1/2.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(0, 100);
  WindowState* window_state = WindowState::Get(w1.get());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  const gfx::Rect work_area(work_area_bounds());
  const gfx::Rect snapped_bounds(0, 0, work_area.width() / 2,
                                 work_area.height());
  EXPECT_EQ(snapped_bounds, w1->GetBoundsInScreen());

  // Resize `w1` to an arbitrary size not 1/2.
  event_generator->set_current_screen_location(snapped_bounds.right_center());
  event_generator->DragMouseBy(100, 10);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_NE(snapped_bounds, w1->GetBoundsInScreen());

  // Drag `w1` to unsnap and skip overview pairing.
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseBy(10, 10);
  EXPECT_FALSE(IsInOverviewSession());
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
  EXPECT_NE(snapped_bounds, w1->GetBoundsInScreen());

  // Drag to snap `w1` again. Test it snaps to 1/2.
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(0, 100);
  EXPECT_EQ(snapped_bounds, w1->GetBoundsInScreen());

  // Resize `w1` to an arbitrary size not 1/2 again.
  event_generator->set_current_screen_location(snapped_bounds.right_center());
  event_generator->DragMouseBy(-100, 10);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_NE(snapped_bounds, w1->GetBoundsInScreen());

  // Drag to snap `w2`. Test it snaps to 1/2.
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(0, 100);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_EQ(snapped_bounds, w2->GetBoundsInScreen());
}

TEST_F(FasterSplitScreenTest, ResizeAndAutoSnap) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  const gfx::Rect initial_bounds(w1->GetBoundsInScreen());
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(
      w1->GetBoundsInScreen().right_center());
  const int drag_x = 100;
  generator->DragMouseBy(drag_x, 0);
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  gfx::Rect expected_window_bounds(initial_bounds);
  expected_window_bounds.set_width(initial_bounds.width() + drag_x);
  EXPECT_EQ(expected_window_bounds, w1->GetBoundsInScreen());

  gfx::Rect expected_grid_bounds(work_area_bounds());
  expected_grid_bounds.Subtract(w1->GetBoundsInScreen());
  EXPECT_EQ(expected_grid_bounds, GetOverviewGridBounds(w1->GetRootWindow()));

  // Create a window and test that it auto snaps.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w3.get())->GetStateType());
  EXPECT_EQ(expected_grid_bounds, w3->GetBoundsInScreen());
}

// Verify the window focus behavior both when activing a window or skipping
// pairing in partial overview.
// 1. When activating a window in partial overview, the chosen window will be
// the activated one upon exit;
// 2. When skipping pairing in partial overview, the snapped window will still
// be the activated one if it was activated before entering
// `SplitViewOverviewSession`.
TEST_F(FasterSplitScreenTest, SnappedWindowFocusTest) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(200, 100)));
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(100, 100)));
  ASSERT_TRUE(wm::IsActiveWindow(w1.get()));

  auto* event_generator = GetEventGenerator();
  for (const bool skip_pairing : {true, false}) {
    SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kSecondarySnapped,
                      chromeos::kDefaultSnapRatio);
    VerifySplitViewOverviewSession(w1.get());

    auto* w2_overview_item = GetOverviewItemForWindow(w2.get());
    EXPECT_TRUE(w2_overview_item);

    const auto w2_overview_item_bounds = w2_overview_item->target_bounds();
    const gfx::Point click_point =
        skip_pairing
            ? gfx::ToRoundedPoint(w2_overview_item_bounds.bottom_right()) +
                  gfx::Vector2d(20, 20)
            : gfx::ToRoundedPoint(w2_overview_item_bounds.CenterPoint());

    event_generator->MoveMouseTo(click_point);
    event_generator->ClickLeftButton();

    EXPECT_EQ(wm::IsActiveWindow(w1.get()), skip_pairing);
    EXPECT_FALSE(IsInOverviewSession());
    MaximizeToClearTheSession(w1.get());
  }
}

TEST_F(FasterSplitScreenTest, DragToPartialOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  ToggleOverview();
  OverviewSession* overview_session =
      OverviewController::Get()->overview_session();
  ASSERT_TRUE(overview_session);
  EXPECT_TRUE(overview_session->IsWindowInOverview(w1.get()));
  EXPECT_TRUE(overview_session->IsWindowInOverview(w2.get()));

  // Drag `w1` to enter partial overview.
  auto* event_generator = GetEventGenerator();
  DragGroupItemToPoint(GetOverviewItemForWindow(w1.get()), gfx::Point(0, 0),
                       event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  VerifySplitViewOverviewSession(w1.get(), /*faster_split_screen_setup=*/false);
  EXPECT_TRUE(overview_session->IsWindowInOverview(w2.get()));

  // Select `w2`. Test it snaps and we end overview.
  ClickOverviewItem(event_generator, w2.get());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Tests that when clicking or tapping on the empty area during faster split
// screen setup session, overview will end.
TEST_F(FasterSplitScreenTest, SkipPairingInOverviewWhenActivatingTheEmptyArea) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());

  auto* w2_overview_item = GetOverviewItemForWindow(w2.get());
  EXPECT_TRUE(w2_overview_item);
  const gfx::Point outside_point =
      gfx::ToRoundedPoint(
          w2_overview_item->GetTransformedBounds().bottom_right()) +
      gfx::Vector2d(20, 20);

  // Verify that clicking on an empty area in overview will exit the paring.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(outside_point);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(IsInOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);

  // Verify that tapping on an empty area in overview will exit the paring.
  MaximizeToClearTheSession(w1.get());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  event_generator->MoveTouch(outside_point);
  event_generator->PressTouch();
  event_generator->ReleaseTouch();
  EXPECT_FALSE(IsInOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
}

// Tests that when clicking or tapping on the snapped window on the `HTCLIENT`
// or `HTCAPTION` area during faster split screen setup session, overview will
// end.
TEST_F(FasterSplitScreenTest, SkipPairingWhenActivatingTheSnappedWindow) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  aura::test::TestWindowDelegate delegate;

  auto* event_generator = GetEventGenerator();

  // Snap `w1`. Test that moving the mouse around won't end overview
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(IsInOverviewSession());
  MaximizeToClearTheSession(w1.get());

  // Build test cases to verify that overview will end when clicking or tapping
  // on the window caption or client area.
  struct {
    int window_component;
    bool is_click_event;
  } kTestCases[]{
      {/*window_component*/ HTCLIENT, /*is_click_event=*/true},
      {/*window_component*/ HTCAPTION, /*is_click_event=*/true},
      {/*window_component*/ HTCLIENT, /*is_click_event=*/false},
      {/*window_component*/ HTCAPTION, /*is_click_event=*/false},
  };

  for (const auto& test_case : kTestCases) {
    SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                      chromeos::kDefaultSnapRatio);
    VerifySplitViewOverviewSession(w1.get());
    delegate.set_window_component(test_case.window_component);
    if (test_case.is_click_event) {
      event_generator->ClickLeftButton();
    } else {
      event_generator->PressTouch();
      event_generator->ReleaseTouch();
    }
    EXPECT_FALSE(IsInOverviewSession());
    MaximizeToClearTheSession(w1.get());
  }
}

TEST_F(FasterSplitScreenTest, SkipPairingOnKeyEvent) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());

  // Test that Esc key exits overview.
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);

  // Test that Alt + Tab exits overview.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  PressAndReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
  EXPECT_TRUE(Shell::Get()->window_cycle_controller()->IsCycling());
}

TEST_F(FasterSplitScreenTest, SkipPairingToast) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  auto* overview_grid = GetOverviewGridForRoot(w1->GetRootWindow());
  ASSERT_TRUE(overview_grid);
  auto* faster_split_view = overview_grid->GetFasterSplitView();
  ASSERT_TRUE(faster_split_view);
  LeftClickOn(faster_split_view->GetDismissButton());

  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

TEST_F(FasterSplitScreenTest, DontStartPartialOverviewAfterSkippingPairing) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // Press Esc key to skip pairing.
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Snap `w2`. Since `w1` is snapped to primary, it doesn't start partial
  // overview. wm::ActivateWindow(w2.get());
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
  EXPECT_EQ(WindowState::Get(w2.get())->GetStateType(),
            WindowStateType::kSecondarySnapped);
}

TEST_F(FasterSplitScreenTest, DontStartPartialOverviewAfterClosingWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // Select `w2` to auto-snap it.
  ClickOverviewItem(GetEventGenerator(), w2.get());

  // Close `w2`, then open and snap a new `w3`. Test we don't start partial
  // overview.
  w2.reset();
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

TEST_F(FasterSplitScreenTest, StartPartialOverviewForMinimizedWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // Now minimize `w1`, so visually there is no primary snapped window.
  WindowState::Get(w1.get())->Minimize();

  // Now snap `w2` to secondary. Since `w1` is minimized, it starts partial
  // overview.
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w2.get());
}

// Tests that when activating an already snapped window, cannot snap toast will
// not show by mistake. See b/323391799 for details.
TEST_F(FasterSplitScreenTest,
       DoNotShowCannotSnapToastWhenActivatingTheSnappedWindow) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);
  ASSERT_TRUE(WindowState::Get(w1.get())->IsSnapped());

  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  EXPECT_FALSE(IsInOverviewSession());

  wm::ActivateWindow(w1.get());
  EXPECT_FALSE(ToastManager::Get()->IsToastShown(kAppCannotSnapToastId));
}

TEST_F(FasterSplitScreenTest, DontStartPartialOverviewForFloatedWindow) {
  // Snap 2 test windows in place.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // To simulate the CUJ when a user selects a window from overview, activate
  // and snap `w2`.
  wm::ActivateWindow(w2.get());
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());

  // Create a 3rd floated window on top of `w2`.
  std::unique_ptr<aura::Window> floated_window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(WindowState::Get(floated_window.get())->IsFloated());
  EXPECT_TRUE(
      w2->GetBoundsInScreen().Contains(floated_window->GetBoundsInScreen()));

  // Open a 4th window and snap it on top of `w1`. Test we don't start partial
  // overview.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
}

// Tests that partial overview will not be triggered if the window to be snapped
// is only one window for the active desk and on the current display.
TEST_F(FasterSplitScreenTest, DontStartPartiOverviewIfThereIsOnlyOneWindow) {
  UpdateDisplay("900x600, 901+0-900x600");
  ASSERT_EQ(Shell::GetAllRootWindows().size(), 2u);

  DesksController* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  Desk* desk1 = desks_controller->GetDeskAtIndex(1);

  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(10, 20, 200, 100)));

  // Create the 2nd window and move it to another desk.
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(100, 20, 200, 100)));
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w2.get()), desk0);
  desks_controller->MoveWindowFromActiveDeskTo(
      w2.get(), desk1, w2->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kShortcut);
  ASSERT_EQ(desks_util::GetDeskForContext(w2.get()), desk1);

  // Create the 3rd window on the 2nd display.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(1000, 20, 200, 100)));

  // Verify that snapping `w1` won't trigger partial overview.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_FALSE(IsInOverviewSession());
}

// Tests that only when there is a non-occluded window snapped on the opposite
// side should we skip showing partial overview on window snapped. This test
// focuses on the window layout setup **with** intersections.
TEST_F(FasterSplitScreenTest,
       OppositeSnappedWindowOcclusionWithIntersectionsTest) {
  UpdateDisplay("800x600");
  // Window Layout before snapping `w1` to the primary snapped position:
  // `w2` is snapped on the secondary snapped position;
  // `w3` is stacked above `w2` with intersections.
  //
  //                  +-----------+
  //          +-------|-+         |
  //          |       | |         |
  //          |   w3  | |   w2    |
  //          |       | |         |
  //          +-------|-+         |
  //                  +-----------+
  //
  // For the window layout setup above, we should show partial overview
  // when snapping `w1` by the desired snap action source.

  // Snap `w2` to the secondary snapped location without triggering faster split
  // screen to get window layout setup ready.
  std::unique_ptr<aura::Window> w2 = CreateAppWindow();
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);
  ASSERT_TRUE(w2->IsVisible());

  // Create `w3` with bounds that intersect with `w2`.
  std::unique_ptr<aura::Window> w3 =
      CreateAppWindow(gfx::Rect(350, 200, 150, 200));
  ASSERT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w3->GetBoundsInScreen().Intersects(w2->GetBoundsInScreen()));

  // Create and snap `w1` to the primary snapped position and expect to trigger
  // the faster split screen setup.
  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(w1->IsVisible());
  VerifySplitViewOverviewSession(w1.get());

  // Activate `w2` to bring it to the front and snap it to the primary
  // snapped location without triggering faster split screen in preparation for
  // the next round of testing. `w2` is fully visible now.
  wm::ActivateWindow(w2.get());
  SnapOneTestWindow(w2.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);
  EXPECT_FALSE(IsInOverviewSession());

  // Snap `w1` to secondary snapped position with desired snap action source to
  // trigger faster split screen setup, with `w1` occupying the primary snapped
  // position, partial overview shouldn't start.
  SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_FALSE(IsInOverviewSession());
}

// Tests that only when there is a non-occluded window snapped on the opposite
// side should we skip showing partial overview on window snapped. This test
// focuses on the window layout setup **without** intersections.
TEST_F(FasterSplitScreenTest,
       OppositeSnappedWindowOcclusionWithoutIntersectionsTest) {
  UpdateDisplay("800x600");
  // Window Layout before snapping `w1` to the primary snapped position:
  // `w2` is snapped on the secondary snapped position;
  // `w3` is stacked above `w2` without intersections.
  //
  //              +-----------+
  //              |    +---+  |
  //              |    | w3|  |
  //              |    +---+  |
  //              |    w2     |
  //              |           |
  //              +-----------+
  //
  // For the window layout setup above, we should show partial overview
  // when snapping `w1` by the desired snap action source.

  // Snap `w2` to the secondary snapped location without triggering faster split
  // screen to get window layout setup ready.
  std::unique_ptr<aura::Window> w2 = CreateAppWindow();
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);
  ASSERT_TRUE(w2->IsVisible());

  // Create `w3` with bounds confined by the bounds `w2`.
  std::unique_ptr<aura::Window> w3 =
      CreateAppWindow(gfx::Rect(550, 45, 50, 50));
  ASSERT_TRUE(w3->IsVisible());
  EXPECT_TRUE(w2->GetBoundsInScreen().Contains(w3->GetBoundsInScreen()));

  // Create and snap `w1` to the primary snapped position and expect to trigger
  // the faster split screen setup.
  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(w1->IsVisible());
  VerifySplitViewOverviewSession(w1.get());

  // Activate `w2` to bring it to the front and snap it to the primary
  // snapped location without triggering faster split screen in preparation for
  // the next round of testing. `w2` is fully visible now.
  wm::ActivateWindow(w2.get());
  SnapOneTestWindow(w2.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);
  EXPECT_FALSE(IsInOverviewSession());

  // Snap `w1` to secondary snapped position with desired snap action source to
  // trigger faster split screen setup, with `w1` occupying the primary snapped
  // position, partial overview shouldn't start.
  SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_FALSE(IsInOverviewSession());
}

TEST_F(FasterSplitScreenTest, NoCrashOnDisplayChange) {
  UpdateDisplay("800x600,1000x600");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());

  // Snap `window` on the second display. Test its bounds are updated.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(900, 0, 100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(1000, 0, 100, 100)));
  SnapOneTestWindow(window1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  ASSERT_EQ(display_manager_test.GetSecondaryDisplay().id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestWindow(window1.get())
                .id());
  const gfx::Rect work_area(
      display_manager_test.GetSecondaryDisplay().work_area());
  EXPECT_EQ(gfx::Rect(800, 0, work_area.width() / 2, work_area.height()),
            window1->GetBoundsInScreen());
  VerifySplitViewOverviewSession(window1.get());

  // Disconnect the second display. Test no crash.
  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
}

// Tests that autosnapping a window with minimum size doesn't crash. Regression
// test for http://b/324483718.
TEST_F(FasterSplitScreenTest, SnapWindowWithMinimumSize) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> w1(CreateAppWindow());

  // 1 - Test min size > 1/3 scenario.
  // Set `w2` min size to be > 1/3 of the display width.
  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate, /*id=*/-1, gfx::Rect(800, 600)));
  int min_width = 396;
  delegate.set_minimum_size(gfx::Size(min_width, 0));

  // Snap `w1` to primary 2/3.
  WindowState* window_state = WindowState::Get(w1.get());
  const WindowSnapWMEvent snap_type(
      WM_EVENT_SNAP_PRIMARY, chromeos::kTwoThirdSnapRatio,
      /*snap_action_source=*/WindowSnapActionSource::kTest);
  window_state->OnWMEvent(&snap_type);
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Select `w2` from overview.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(GetOverviewItemForWindow(w2.get())
                              ->GetTransformedBounds()
                              .CenterPoint()));
  event_generator->ClickLeftButton();

  // Test it gets snapped at its minimum size.
  EXPECT_EQ(min_width, w2->GetBoundsInScreen().width());

  MaximizeToClearTheSession(w2.get());

  // 2 - Test min size > 1/2 scenario.
  // Set `w2` min size to be > 1/2 of the display width.
  min_width = 450;
  delegate.set_minimum_size(gfx::Size(min_width, 0));

  // Snap `w1` to primary 1/2.
  const WindowSnapWMEvent snap_default(
      WM_EVENT_SNAP_PRIMARY, chromeos::kDefaultSnapRatio,
      /*snap_action_source=*/WindowSnapActionSource::kTest);
  window_state->OnWMEvent(&snap_default);
  ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

  // Select `w2` from overview.
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(GetOverviewItemForWindow(w2.get())
                              ->GetTransformedBounds()
                              .CenterPoint()));
  event_generator->ClickLeftButton();

  // Test it gets snapped at its minimum size.
  EXPECT_EQ(min_width, w2->GetBoundsInScreen().width());
}

// Tests we start partial overview if there's an opposite snapped window on
// another display.
TEST_F(FasterSplitScreenTest, OppositeSnappedWindowOnOtherDisplay) {
  UpdateDisplay("800x600,801+0-800x600");

  // Create 3 test windows, with `w3` on display 2.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(900, 0, 100, 100)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(1000, 0, 100, 100)));

  // Snap `w1` to primary on display 1.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  display::Screen* screen = display::Screen::GetScreen();
  auto display_list = screen->GetAllDisplays();
  ASSERT_EQ(display_list[0], screen->GetDisplayNearestWindow(w1.get()));

  // Test we start partial overview.
  EXPECT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(
      RootWindowController::ForWindow(w1.get())->split_view_overview_session());

  // Select `w2` to snap on the first display.
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_EQ(display_list[0], screen->GetDisplayNearestWindow(w2.get()));

  // Snap `w3` to secondary on display 2.
  SnapOneTestWindow(w3.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_EQ(display_list[1], screen->GetDisplayNearestWindow(w3.get()));

  // Test we start partial overview since no window is snapped on display 2.
  EXPECT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(
      RootWindowController::ForWindow(w3.get())->split_view_overview_session());
}

// Tests that the snapped window bounds will be refreshed on display changes to
// preserve the snap ratio.
TEST_F(FasterSplitScreenTest, WindowBoundsRefreshedOnDisplayChanges) {
  UpdateDisplay("900x600");
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  SnapOneTestWindow(window1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(window1.get());
  ASSERT_EQ(WindowState::Get(window1.get())->snap_ratio(),
            chromeos::kTwoThirdSnapRatio);
  const auto work_area_bounds_1 = work_area_bounds();
  ASSERT_EQ(
      window1->GetBoundsInScreen(),
      gfx::Rect(0, 0, work_area_bounds_1.width() * chromeos::kTwoThirdSnapRatio,
                work_area_bounds_1.height()));

  UpdateDisplay("1200x600");
  VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(WindowState::Get(window1.get())->snap_ratio(),
            chromeos::kTwoThirdSnapRatio);
  const auto work_area_bounds_2 = work_area_bounds();
  EXPECT_EQ(
      window1->GetBoundsInScreen(),
      gfx::Rect(0, 0, work_area_bounds_2.width() * chromeos::kTwoThirdSnapRatio,
                work_area_bounds_2.height()));
}

// Tests that the grid and faster splitview widget is updated on keyboard
// and work area bounds changes.
TEST_F(FasterSplitScreenTest, KeyboardAndWorkAreaBoundsChanges) {
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  SnapOneTestWindow(window1.get(), chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(window1.get());

  // Show the virtual keyboard. Test we refresh the grid and widget bounds.
  SetVirtualKeyboardEnabled(true);
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  VerifySplitViewOverviewSession(window1.get());
  auto* root_window = window1->GetRootWindow();
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window1.get())->GetStateType());
  auto* overview_grid = GetOverviewGridForRoot(root_window);
  EXPECT_TRUE(
      GetOverviewGridBounds(root_window)
          .Contains(overview_grid->GetFasterSplitView()->GetBoundsInScreen()));

  // Hide the virtual keyboard. Test we refresh the grid and widget bounds.
  keyboard_controller->HideKeyboardByUser();
  VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window1.get())->GetStateType());
  EXPECT_TRUE(
      GetOverviewGridBounds(root_window)
          .Contains(overview_grid->GetFasterSplitView()->GetBoundsInScreen()));

  // Show the docked magnifier, which ends overview.
  auto* docked_magnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_magnifier_controller->SetEnabled(/*enabled=*/true);
  EXPECT_FALSE(IsInOverviewSession());
  // TODO(sophiewen): Consider testing no faster splitview widget.
}

// Test to verify that there will be no crash when dragging the snapped window
// out without resizing the window see crash in b/321111182.
TEST_F(FasterSplitScreenTest, NoCrashWhenDraggingTheSnappedWindow) {
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  SnapOneTestWindow(window1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(window1.get());

  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window1.get(), gfx::PointF(), HTCAPTION, wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::PointF(500, 100), /*event_flags=*/0);
  WindowState* window_state = WindowState::Get(window1.get());
  EXPECT_TRUE(window_state->is_dragged());
  resizer->CompleteDrag();
  EXPECT_FALSE(window_state->IsSnapped());
}

// Tests that after a minimized window gets auto-snapped, dragging the window
// won't lead to crash. See crash at http://b/324483508.
TEST_F(FasterSplitScreenTest,
       NoCrashWhenDraggingTheAutoSnappedWindowThatWasPreviouslyMinimized) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(100, 100, 100, 100)));
  WindowState* w2_window_state = WindowState::Get(w2.get());
  w2_window_state->Minimize();
  ASSERT_TRUE(w2_window_state->IsMinimized());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(w1.get());

  auto* w2_overview_item = GetOverviewItemForWindow(w2.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      gfx::ToRoundedPoint(w2_overview_item->target_bounds().CenterPoint()));
  event_generator->ClickLeftButton();
  EXPECT_EQ(w2_window_state->GetStateType(),
            WindowStateType::kSecondarySnapped);

  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      w2.get(), gfx::PointF(), HTCAPTION, wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::PointF(500, 100), /*event_flags=*/0);
  EXPECT_TRUE(w2_window_state->is_dragged());
  resizer->CompleteDrag();
  EXPECT_FALSE(w2_window_state->IsSnapped());
}

// Verifies the issue to snap a window in overview is working properly. see
// b/322893408.
TEST_F(FasterSplitScreenTest, EnterOverviewSnappingWindow) {
  std::unique_ptr<aura::Window> window1(
      CreateAppWindow(gfx::Rect(20, 20, 200, 100)));
  std::unique_ptr<aura::Window> windo2(
      CreateAppWindow(gfx::Rect(10, 10, 200, 100)));

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(IsInOverviewSession());

  auto* overview_item = GetOverviewItemForWindow(window1.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();
  event_generator->DragMouseTo(0, 0);
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(IsInOverviewSession());
}

// Verifies that there will be no crash when transitioning the
// `SplitViewOverviewSession` between clamshell and tablet mode.
TEST_F(FasterSplitScreenTest, ClamshellTabletTransitionOneSnappedWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  SwitchToTabletMode();
  EXPECT_TRUE(split_view_divider()->divider_widget());
  auto observed_windows = split_view_divider()->observed_windows();
  EXPECT_EQ(1u, observed_windows.size());
  EXPECT_EQ(w1.get(), observed_windows.front());

  TabletModeControllerTestApi().LeaveTabletMode();
}

TEST_F(FasterSplitScreenTest, ClamshellTabletTransitionTwoSnappedWindows) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  // Select the second window from overview to snap it.
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_FALSE(split_view_divider()->divider_widget());

  SwitchToTabletMode();
  EXPECT_TRUE(split_view_divider()->divider_widget());
  auto observed_windows = split_view_divider()->observed_windows();
  EXPECT_EQ(2u, observed_windows.size());
  // TODO(b/312229933): Determine whether the order of `observed_windows_`
  // matters.
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), split_view_divider());

  TabletModeControllerTestApi().LeaveTabletMode();
}

// Tests that there will be no overlap between two windows on window layout
// setup complete. It used to happen because the minimum size of the window was
// never taken into account. See http://b/324631432 for more details.
TEST_F(FasterSplitScreenTest,
       NoOverlapAfterSnapRatioVariesToAccommodateForMinimumSize) {
  UpdateDisplay("900x600");

  std::unique_ptr<aura::Window> window1(CreateAppWindow());

  // Create `window2` with window minimum size above 1/3 of the work area.
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithDelegate(
      &delegate2, /*id=*/-1, gfx::Rect(600, 300)));
  delegate2.set_minimum_size(gfx::Size(400, 200));

  SnapOneTestWindow(window2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio);
  VerifySplitViewOverviewSession(window2.get());

  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint()));
  event_generator->ClickLeftButton();
  WaitForOverviewExitAnimation();
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());

  // Verify that the snap ratio of `window2` will be adjusted to accommodate for
  // the window minimum size.
  WindowState* window2_state = WindowState::Get(window2.get());
  ASSERT_TRUE(window2_state->snap_ratio());
  EXPECT_EQ(window2_state->GetStateType(), WindowStateType::kSecondarySnapped);
  EXPECT_GT(window2_state->snap_ratio().value(), chromeos::kOneThirdSnapRatio);

  // Verify that the auto snap ratio of `window1` will be adjusted as well.
  WindowState* window1_state = WindowState::Get(window1.get());
  ASSERT_TRUE(window1_state->snap_ratio());
  EXPECT_EQ(window1_state->GetStateType(), WindowStateType::kPrimarySnapped);
  EXPECT_LT(window1_state->snap_ratio().value(), chromeos::kTwoThirdSnapRatio);

  // Both windows will fit within the work are with no overlap
  EXPECT_EQ(window1->GetBoundsInScreen().width() +
                window2->GetBoundsInScreen().width(),
            work_area_bounds().width());
}

// Tests that double tap to swap windows doesn't crash after transition to
// tablet mode (b/308216746).
TEST_F(FasterSplitScreenTest, NoCrashWhenDoubleTapAfterTransition) {
  // Use non-zero to start an animation, which will notify
  // `SplitViewOverviewSession::OnWindowBoundsChanged()`.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  SwitchToTabletMode();
  EXPECT_TRUE(split_view_divider()->divider_widget());

  // Double tap on the divider. This will start a drag and notify
  // SplitViewOverviewSession.
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  GetEventGenerator()->GestureTapAt(divider_center);
  GetEventGenerator()->GestureTapAt(divider_center);
}

TEST_F(FasterSplitScreenTest, BasicTabKeyNavigation) {
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  std::unique_ptr<aura::Window> window1(CreateAppWindow());

  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY,
                                     WindowSnapActionSource::kTest);
  WindowState::Get(window1.get())->OnWMEvent(&snap_event);
  ASSERT_TRUE(IsInOverviewSession());

  // Tab until we get to the first overview item.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview_windows =
      GetOverviewItemsForRoot(0);
  EXPECT_EQ(overview_windows[0]->GetWindow(), GetOverviewFocusedWindow());

  OverviewFocusCyclerOld* focus_cycler_old =
      GetOverviewSession()->focus_cycler_old();
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();

  // Tab to the toast dismiss button.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(grid->GetFasterSplitView()->GetDismissButton(),
            focus_cycler_old->focused_view()->GetView());

  // Tab to the settings button.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(grid->GetFasterSplitView()->settings_button(),
            focus_cycler_old->focused_view());

  // Note we use `PressKeyAndModifierKeys()` to send modifier and key separately
  // to simulate real user input.

  // Shift + Tab reverse tabs to the dismiss button.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKeyAndModifierKeys(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(grid->GetFasterSplitView()->GetDismissButton(),
            focus_cycler_old->focused_view()->GetView());

  // Shift + Tab reverse tabs to the overview item.
  event_generator->PressKeyAndModifierKeys(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(overview_windows[0]->GetWindow(), GetOverviewFocusedWindow());
}

// Tests no crash when the faster splitview toast is destroyed. Regression test
// for http://b/336289329.
TEST_F(FasterSplitScreenTest, NoCrashOnToastDestroying) {
  auto w1 = CreateAppWindow(gfx::Rect(100, 100));
  auto w2 = CreateAppWindow(gfx::Rect(100, 100));

  // Snap `w1` to start faster splitview.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_TRUE(IsInOverviewSession());
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  auto* faster_splitview_widget = grid->faster_splitview_widget();
  ASSERT_TRUE(faster_splitview_widget);

  // Tab to the dismiss button.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  OverviewFocusCyclerOld* focus_cycler_old =
      GetOverviewSession()->focus_cycler_old();
  EXPECT_EQ(grid->GetFasterSplitView()->GetDismissButton(),
            focus_cycler_old->focused_view()->GetView());

  // Enter tablet mode to destroy the toast.
  SwitchToTabletMode();

  // Exit tablet mode, then tab.
  ExitTabletMode();
  PressAndReleaseKey(ui::VKEY_TAB);
}

// Tests that the chromevox keys work as expected.
TEST_F(FasterSplitScreenTest, TabbingChromevox) {
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);

  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  std::unique_ptr<aura::Window> window1(CreateAppWindow());

  const WindowSnapWMEvent snap_event(WM_EVENT_SNAP_PRIMARY,
                                     WindowSnapActionSource::kTest);
  auto* event_generator = GetEventGenerator();

  enum class TestCase { kDismissButton, kSettingsButton };
  const auto kTestCases = {TestCase::kDismissButton, TestCase::kSettingsButton};

  for (auto test_case : kTestCases) {
    WindowState::Get(window1.get())->OnWMEvent(&snap_event);
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

    // Note we use `PressKeyAndModifierKeys()` to send modifier and key
    // separately to simulate real user input.

    // Search + Right moves to the first overview item.
    event_generator->PressKeyAndModifierKeys(ui::VKEY_RIGHT,
                                             ui::EF_COMMAND_DOWN);
    const std::vector<std::unique_ptr<OverviewItemBase>>& overview_windows =
        GetOverviewItemsForRoot(0);
    EXPECT_EQ(overview_windows[0]->GetWindow(), GetOverviewFocusedWindow());

    // Search + Right moves to the dismiss button.
    event_generator->PressKeyAndModifierKeys(ui::VKEY_RIGHT,
                                             ui::EF_COMMAND_DOWN);
    OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
    OverviewFocusCyclerOld* focus_cycler_old =
        GetOverviewSession()->focus_cycler_old();
    EXPECT_EQ(grid->GetFasterSplitView()->GetDismissButton(),
              focus_cycler_old->focused_view()->GetView());

    // Search + Right moves to the settings button.
    event_generator->PressKeyAndModifierKeys(ui::VKEY_RIGHT,
                                             ui::EF_COMMAND_DOWN);
    EXPECT_EQ(grid->GetFasterSplitView()->settings_button(),
              focus_cycler_old->focused_view());

    if (test_case == TestCase::kSettingsButton) {
      // Search + Space activates the settings button.
      event_generator->PressKeyAndModifierKeys(ui::VKEY_SPACE,
                                               ui::EF_COMMAND_DOWN);
      EXPECT_FALSE(IsInOverviewSession());
    } else {
      // Search + Left moves back to the dismiss button.
      event_generator->PressKeyAndModifierKeys(ui::VKEY_LEFT,
                                               ui::EF_COMMAND_DOWN);
      EXPECT_EQ(grid->GetFasterSplitView()->GetDismissButton(),
                focus_cycler_old->focused_view()->GetView());

      // Search + Space activates the dismiss button.
      event_generator->PressKeyAndModifierKeys(ui::VKEY_SPACE,
                                               ui::EF_COMMAND_DOWN);
      EXPECT_FALSE(IsInOverviewSession());
    }
  }
}

TEST_F(FasterSplitScreenTest, AccessibilityFocusAnnotator) {
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));
  auto window0 = CreateAppWindow(gfx::Rect(100, 100));

  // Snap `window0`, so it is excluded from the overview list.
  SnapOneTestWindow(window0.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  ASSERT_TRUE(focus_widget);
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  ASSERT_FALSE(grid->desks_widget());
  ASSERT_FALSE(grid->GetSaveDeskForLaterButton());
  auto* faster_splitview_widget = grid->faster_splitview_widget();
  ASSERT_TRUE(faster_splitview_widget);

  // Overview items are in MRU order, so the expected order in the grid list is
  // the reverse creation order.
  auto* item_widget1 = GetOverviewItemForWindow(window1.get())->item_widget();

  // Order should be [focus_widget, item_widget1, faster_splitview_widget].
  CheckA11yOverrides("focus", focus_widget,
                     /*expected_previous=*/faster_splitview_widget,
                     /*expected_next=*/item_widget1);
  CheckA11yOverrides("item1", item_widget1, /*expected_previous=*/focus_widget,
                     /*expected_next=*/faster_splitview_widget);
  CheckA11yOverrides("splitview", faster_splitview_widget,
                     /*expected_previous=*/item_widget1,
                     /*expected_next=*/focus_widget);
}

// Tests the histograms for the split view overview session exit points are
// recorded correctly in clamshell.
TEST_F(FasterSplitScreenTest,
       SplitViewOverviewSessionExitPointClamshellHistograms) {
  const auto kWindowLayoutCompleteOnSessionExit =
      BuildWindowLayoutCompleteOnSessionExitHistogram();
  const auto kSplitViewOverviewSessionExitPoint =
      BuildSplitViewOverviewExitPointHistogramName(
          WindowSnapActionSource::kDragWindowToEdgeToSnap);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  // Verify the initial count for the histogram.
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/true,
                                      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/false,
                                      /*expected_count=*/0);

  // Set up the splitview overview session and select a window in the partial
  // overview to complete the window layout.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(w1.get());
  auto* event_generator = GetEventGenerator();
  ClickOverviewItem(event_generator, w2.get());
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/true,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/false,
                                      /*expected_count=*/0);
  histogram_tester_.ExpectBucketCount(
      kSplitViewOverviewSessionExitPoint,
      SplitViewOverviewSessionExitPoint::kCompleteByActivating,
      /*expected_count=*/1);
  MaximizeToClearTheSession(w1.get());
  MaximizeToClearTheSession(w2.get());

  // Set up the splitview overview session and click an empty area to skip the
  // pairing.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(w1.get());
  auto* item2 = GetOverviewItemForWindow(w2.get());
  gfx::Point outside_point =
      gfx::ToRoundedPoint(item2->target_bounds().bottom_right());
  outside_point.Offset(/*delta_x=*/5, /*delta_y=*/5);
  event_generator->MoveMouseTo(outside_point);
  event_generator->ClickLeftButton();
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/true,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/false,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kSplitViewOverviewSessionExitPoint,
                                      SplitViewOverviewSessionExitPoint::kSkip,
                                      /*expected_count=*/1);
  MaximizeToClearTheSession(w1.get());
  MaximizeToClearTheSession(w2.get());

  // Set up the splitview overview session, create a 3rd window to be
  // auto-snapped and complete the window layout.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(w1.get());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/true,
                                      /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/false,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(
      kSplitViewOverviewSessionExitPoint,
      SplitViewOverviewSessionExitPoint::kCompleteByActivating,
      /*expected_count=*/2);
  MaximizeToClearTheSession(w1.get());
  MaximizeToClearTheSession(w3.get());

  // Set up the splitview overview session and press escape key to skip pairing.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(w1.get());
  event_generator->PressAndReleaseKey(ui::VKEY_ESCAPE);
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/true,
                                      /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/false,
                                      /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(kSplitViewOverviewSessionExitPoint,
                                      SplitViewOverviewSessionExitPoint::kSkip,
                                      /*expected_count=*/2);
  MaximizeToClearTheSession(w1.get());
  MaximizeToClearTheSession(w2.get());

  // Set up the splitview overview session and close the snapped window to exit
  // the session.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(w1.get());
  w1.reset();
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/true,
                                      /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(kWindowLayoutCompleteOnSessionExit,
                                      /*sample=*/false,
                                      /*expected_count=*/2);
  histogram_tester_.ExpectBucketCount(
      kSplitViewOverviewSessionExitPoint,
      SplitViewOverviewSessionExitPoint::kWindowDestroy,
      /*expected_count=*/1);
}

// Integration test of the `SplitViewOverviewSession` exit point with drag to
// snap action source. Verify that the end-to-end metric is recorded correctly.
TEST_F(FasterSplitScreenTest, KeyMetricsIntegrationTest_DragToSnap) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  const auto kSplitViewOverviewSessionExitPoint =
      BuildSplitViewOverviewExitPointHistogramName(
          WindowSnapActionSource::kDragWindowToEdgeToSnap);
  histogram_tester_.ExpectBucketCount(
      kSplitViewOverviewSessionExitPoint,
      SplitViewOverviewSessionExitPoint::kCompleteByActivating,
      /*expected_count=*/0);

  // Drag a window to snap on the primary snapped position and verify the
  // metrics.
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      w1.get(), gfx::PointF(), HTCAPTION, wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::PointF(0, 400), /*event_flags=*/0);
  resizer->CompleteDrag();
  resizer.reset();
  SplitViewOverviewSession* split_view_overview_session =
      VerifySplitViewOverviewSession(w1.get());
  EXPECT_EQ(split_view_overview_session->snap_action_source_for_testing(),
            WindowSnapActionSource::kDragWindowToEdgeToSnap);
  auto* event_generator = GetEventGenerator();
  ClickOverviewItem(event_generator, w2.get());
  histogram_tester_.ExpectBucketCount(
      kSplitViewOverviewSessionExitPoint,
      SplitViewOverviewSessionExitPoint::kCompleteByActivating,
      /*expected_count=*/1);

  MaximizeToClearTheSession(w1.get());
  MaximizeToClearTheSession(w2.get());

  // Drag a window to snap on the secondary snapped position and verify the
  // metrics.
  resizer = CreateWindowResizer(w1.get(), gfx::PointF(), HTCAPTION,
                                wm::WINDOW_MOVE_SOURCE_MOUSE);
  resizer->Drag(gfx::PointF(800, 0), /*event_flags=*/0);
  resizer->CompleteDrag();
  resizer.reset();
  split_view_overview_session = VerifySplitViewOverviewSession(w1.get());
  EXPECT_EQ(split_view_overview_session->snap_action_source_for_testing(),
            WindowSnapActionSource::kDragWindowToEdgeToSnap);

  auto* item2 = GetOverviewItemForWindow(w2.get());
  gfx::Point outside_point =
      gfx::ToRoundedPoint(item2->target_bounds().bottom_right());
  outside_point.Offset(/*delta_x=*/5, /*delta_y=*/5);
  event_generator->MoveMouseTo(outside_point);
  event_generator->ClickLeftButton();
  histogram_tester_.ExpectBucketCount(kSplitViewOverviewSessionExitPoint,
                                      SplitViewOverviewSessionExitPoint::kSkip,
                                      /*expected_count=*/1);
  MaximizeToClearTheSession(w1.get());
}

// Integration test of the `SplitViewOverviewSession` exit point with window
// size button as the snap action source. Verify that the end-to-end metric is
// recorded correctly.
TEST_F(FasterSplitScreenTest, KeyMetricsIntegrationTest_WindowSizeButton) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  struct SnapRequestWithActionSource {
    chromeos::SnapController::SnapRequestSource request_source;
    WindowSnapActionSource snap_action_source;
  } kTestCases[]{
      {chromeos::SnapController::SnapRequestSource::kWindowLayoutMenu,
       WindowSnapActionSource::kSnapByWindowLayoutMenu},
      {chromeos::SnapController::SnapRequestSource::kSnapButton,
       WindowSnapActionSource::kLongPressCaptionButtonToSnap},
  };

  for (const auto test_case : kTestCases) {
    const auto kSplitViewOverviewSessionExitPoint =
        BuildSplitViewOverviewExitPointHistogramName(
            test_case.snap_action_source);
    histogram_tester_.ExpectBucketCount(
        kSplitViewOverviewSessionExitPoint,
        SplitViewOverviewSessionExitPoint::kCompleteByActivating,
        /*expected_count=*/0);

    auto commit_snap = [&]() {
      chromeos::SnapController::Get()->CommitSnap(
          w1.get(), chromeos::SnapDirection::kSecondary,
          chromeos::kDefaultSnapRatio, test_case.request_source);
      SplitViewOverviewSession* split_view_overview_session =
          VerifySplitViewOverviewSession(w1.get());
      EXPECT_TRUE(split_view_overview_session);
      EXPECT_EQ(split_view_overview_session->snap_action_source_for_testing(),
                test_case.snap_action_source);
    };

    commit_snap();
    auto* event_generator = GetEventGenerator();
    ClickOverviewItem(GetEventGenerator(), w2.get());
    histogram_tester_.ExpectBucketCount(
        kSplitViewOverviewSessionExitPoint,
        SplitViewOverviewSessionExitPoint::kCompleteByActivating,
        /*expected_count=*/1);
    MaximizeToClearTheSession(w1.get());
    MaximizeToClearTheSession(w2.get());

    commit_snap();
    auto* item2 = GetOverviewItemForWindow(w2.get());
    gfx::Point outside_point =
        gfx::ToRoundedPoint(item2->target_bounds().bottom_right());
    outside_point.Offset(/*delta_x=*/5, /*delta_y=*/5);
    event_generator->MoveMouseTo(outside_point);
    event_generator->ClickLeftButton();

    histogram_tester_.ExpectBucketCount(
        kSplitViewOverviewSessionExitPoint,
        SplitViewOverviewSessionExitPoint::kSkip,
        /*expected_count=*/1);
    MaximizeToClearTheSession(w1.get());
  }
}

// Tests that the `OverviewStartAction` will be recorded correctly in uma for
// the faster split screen setup.
TEST_F(FasterSplitScreenTest, OverviewStartActionHistogramTest) {
  constexpr char kOverviewStartActionHistogram[] = "Ash.Overview.StartAction";
  // Verify the initial count for the histogram.
  histogram_tester_.ExpectBucketCount(
      kOverviewStartActionHistogram,
      OverviewStartAction::kFasterSplitScreenSetup,
      /*expected_count=*/0);
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  SnapOneTestWindow(window1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(window1.get());
  histogram_tester_.ExpectBucketCount(
      kOverviewStartActionHistogram,
      OverviewStartAction::kFasterSplitScreenSetup,
      /*expected_count=*/1);
}

// Tests that a11y alert will be announced upon entering the faster split screen
// setup session.
TEST_F(FasterSplitScreenTest, A11yAlertOnEnteringFaterSplitScreenSetup) {
  TestAccessibilityControllerClient client;
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  EXPECT_NE(AccessibilityAlert::FASTER_SPLIT_SCREEN_SETUP,
            client.last_a11y_alert());
  SnapOneTestWindow(window1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_EQ(AccessibilityAlert::FASTER_SPLIT_SCREEN_SETUP,
            client.last_a11y_alert());
}

// Tests that there will be no crash when dragging a snapped window in overview
// toward the edge. In this case, the overview components will become too small
// to meet the minimum requirement of the fundamental UI layer such as shadow.
// See the regression behavior in http://b/324478757.
TEST_F(FasterSplitScreenTest, NoCrashWhenDraggingSnappedWindowToEdge) {
  std::unique_ptr<aura::Window> window1(
      CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateAppWindow(gfx::Rect(100, 100, 200, 100)));
  SnapOneTestWindow(window1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  WaitForOverviewEntered();
  VerifySplitViewOverviewSession(window1.get());

  // Drag the snapped window towards the edge of the work area and verify that
  // there is no crash.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      window1.get()->GetBoundsInScreen().right_center());
  gfx::Point drag_end_point = work_area_bounds().right_center();
  drag_end_point.Offset(/*delta_x=*/-10, 0);
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(drag_end_point);

  // Verify that shadow exists for overview item.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  const auto shadow_content_bounds =
      overview_item2->get_shadow_content_bounds_for_testing();
  EXPECT_FALSE(shadow_content_bounds.IsEmpty());

  VerifySplitViewOverviewSession(window1.get());
  EXPECT_TRUE(WindowState::Get(window1.get())->is_dragged());
}

TEST_F(FasterSplitScreenTest, RecordWindowIndexAndCount) {
  // Start partial overview with 1 window in overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  // Select `w2` which is the only window.
  ClickOverviewItem(GetEventGenerator(), w2.get());
  histogram_tester_.ExpectBucketCount(kPartialOverviewSelectedWindowIndex,
                                      /*sample=*/0,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kPartialOverviewWindowListSize,
                                      /*sample=*/1, /*expected_count=*/1);
  MaximizeToClearTheSession(w2.get());

  // Start partial overview with 2 windows in overview.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  // Select `w2` which is the 2nd mru window.
  ClickOverviewItem(GetEventGenerator(), w2.get());
  histogram_tester_.ExpectBucketCount(kPartialOverviewSelectedWindowIndex,
                                      /*sample=*/1,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kPartialOverviewWindowListSize,
                                      /*sample=*/2, /*expected_count=*/1);
  MaximizeToClearTheSession(w2.get());

  // Start partial overview with 3 windows in overview.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  // Select `w3` which is the 3rd mru window.
  ClickOverviewItem(GetEventGenerator(), w3.get());
  histogram_tester_.ExpectBucketCount(kPartialOverviewSelectedWindowIndex,
                                      /*sample=*/2,
                                      /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(kPartialOverviewWindowListSize,
                                      /*sample=*/3, /*expected_count=*/1);
}

// -----------------------------------------------------------------------------
// SnapGroupTest:

// A test fixture to test the snap group feature.
class SnapGroupTest : public AshTestBase {
 public:
  SnapGroupTest() {
    scoped_feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {features::kSnapGroup,
                           features::kOsSettingsRevampWayfinding,
                           features::kSameAppWindowCycle},
                          /*disabled_features=*/{});
  }
  explicit SnapGroupTest(base::test::TaskEnvironment::TimeSource time)
      : AshTestBase(time) {
    scoped_feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {features::kSnapGroup,
                           features::kOsSettingsRevampWayfinding,
                           features::kSameAppWindowCycle},
                          /*disabled_features=*/{});
  }
  SnapGroupTest(const SnapGroupTest&) = delete;
  SnapGroupTest& operator=(const SnapGroupTest&) = delete;
  ~SnapGroupTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    WindowCycleList::SetDisableInitialDelayForTesting(true);
  }

  void SnapTwoTestWindows(aura::Window* window1,
                          aura::Window* window2,
                          bool horizontal = true) {
    CHECK_NE(window1, window2);
    // Snap `window1` to trigger the overview session shown on the other side of
    // the screen.
    SnapOneTestWindow(window1,
                      /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                      chromeos::kDefaultSnapRatio);
    WaitForOverviewEntered();
    VerifySplitViewOverviewSession(window1);

    // Snapping the first window makes it fill half the screen, either
    // vertically or horizontally (based on orientation).
    gfx::Rect work_area(GetWorkAreaBoundsForWindow(window1));
    gfx::Rect primary_bounds, secondary_bounds;
    if (horizontal) {
      work_area.SplitVertically(primary_bounds, secondary_bounds);
    } else {
      work_area.SplitHorizontally(primary_bounds, secondary_bounds);
    }

    EXPECT_EQ(primary_bounds, window1->GetBoundsInScreen());

    // The `window2` gets selected in the overview will be snapped to the
    // non-occupied snap position and the overview session will end.
    ClickOverviewItem(GetEventGenerator(), window2);
    WaitForOverviewExitAnimation();
    EXPECT_EQ(WindowState::Get(window2)->GetStateType(),
              chromeos::WindowStateType::kSecondarySnapped);
    EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
    EXPECT_FALSE(RootWindowController::ForWindow(window1)
                     ->split_view_overview_session());

    auto* snap_group_controller = SnapGroupController::Get();
    ASSERT_TRUE(snap_group_controller);
    EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1, window2));

    // The snap group divider will show on two windows snapped.
    EXPECT_TRUE(snap_group_divider()->divider_widget());
    EXPECT_EQ(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window1)->snap_ratio());
    EXPECT_EQ(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window2)->snap_ratio());

    gfx::Rect divider_bounds(snap_group_divider_bounds_in_screen());
    EXPECT_EQ(work_area.CenterPoint().x(), divider_bounds.CenterPoint().x());
    UnionBoundsEqualToWorkAreaBounds(window1, window2, snap_group_divider());

    if (horizontal) {
      primary_bounds.set_width(primary_bounds.width() -
                               divider_bounds.width() / 2);
      secondary_bounds.set_x(secondary_bounds.x() + divider_bounds.width() / 2);
      secondary_bounds.set_width(secondary_bounds.width() -
                                 divider_bounds.width() / 2);
      EXPECT_EQ(primary_bounds.width(), window1->GetBoundsInScreen().width());
      EXPECT_EQ(secondary_bounds.width(), window2->GetBoundsInScreen().width());
      EXPECT_EQ(primary_bounds.width() + secondary_bounds.width() +
                    divider_bounds.width(),
                work_area.width());
    } else {
      primary_bounds.set_height(primary_bounds.height() -
                                divider_bounds.height() / 2);
      secondary_bounds.set_y(secondary_bounds.y() +
                             divider_bounds.height() / 2);
      secondary_bounds.set_height(secondary_bounds.height() -
                                  divider_bounds.height() / 2);
      EXPECT_EQ(primary_bounds.height(), window1->GetBoundsInScreen().height());
      EXPECT_EQ(secondary_bounds.height(),
                window2->GetBoundsInScreen().height());
      EXPECT_EQ(primary_bounds.height() + secondary_bounds.height() +
                    divider_bounds.height(),
                work_area.height());
    }
  }

  void CompleteWindowCycling() {
    WindowCycleController* window_cycle_controller =
        Shell::Get()->window_cycle_controller();
    window_cycle_controller->CompleteCycling();
    EXPECT_FALSE(window_cycle_controller->IsCycling());
  }

  void CycleWindow(WindowCyclingDirection direction, int steps) {
    WindowCycleController* window_cycle_controller =
        Shell::Get()->window_cycle_controller();
    for (int i = 0; i < steps; i++) {
      window_cycle_controller->HandleCycleWindow(direction);
      EXPECT_TRUE(window_cycle_controller->IsCycling());
    }
  }

  // TODO(michelefan): Consider put this test util in a base class or test file.
  std::unique_ptr<aura::Window> CreateTestWindowWithAppID(
      std::string app_id_key) {
    std::unique_ptr<aura::Window> window = CreateAppWindow();
    window->SetProperty(kAppIDKey, std::move(app_id_key));
    return window;
  }

  std::unique_ptr<aura::Window> CreateTransientChildWindow(
      aura::Window* transient_parent,
      gfx::Rect child_window_bounds) {
    auto child = CreateAppWindow(child_window_bounds);
    wm::AddTransientChild(transient_parent, child.get());
    return child;
  }

  std::unique_ptr<aura::Window> CreateAppWindowWithMinSize(gfx::Size min_size) {
    std::unique_ptr<aura::Window> window =
        CreateAppWindow(gfx::Rect(800, 600), chromeos::AppType::SYSTEM_APP,
                        kShellWindowId_Invalid, new TestWidgetDelegateAsh);
    auto* custom_frame = static_cast<TestNonClientFrameViewAsh*>(
        NonClientFrameViewAsh::Get(window.get()));
    custom_frame->SetMinimumSize(min_size);
    return window;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that the creation and removal of snap group.
TEST_F(SnapGroupTest, AddAndRemoveSnapGroupTest) {
  auto* snap_group_controller = SnapGroupController::Get();
  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_EQ(snap_groups.size(), 0u);
  EXPECT_EQ(window_to_snap_group_map.size(), 0u);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());

  SnapTwoTestWindows(w1.get(), w2.get());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_FALSE(snap_group_controller->AddSnapGroup(
      w1.get(), w3.get(), /*replace=*/false,
      /*carry_over_creation_Time=*/std::nullopt));

  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);
  const auto iter1 = window_to_snap_group_map.find(w1.get());
  ASSERT_TRUE(iter1 != window_to_snap_group_map.end());
  const auto iter2 = window_to_snap_group_map.find(w2.get());
  ASSERT_TRUE(iter2 != window_to_snap_group_map.end());
  auto* snap_group = snap_groups.back().get();
  EXPECT_EQ(iter1->second, snap_group);
  EXPECT_EQ(iter2->second, snap_group);

  ASSERT_TRUE(snap_group_controller->RemoveSnapGroup(snap_group));
  ASSERT_TRUE(snap_groups.empty());
  ASSERT_TRUE(window_to_snap_group_map.empty());
}

// Verify that when there is no gap between the edges of the windows and the
// divider in a Snap Group in landscape. See the regression at
// http://b/333618907.
TEST_F(SnapGroupTest, NoGapAfterSnapGroupCreationInLandscape) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  UpdateDisplay("1366x768");

  // The minimum size of the window is set to a value between 1/3 and 1/2 of the
  // work area's width to simulate the browser window CUJ.
  const gfx::Size window_minimum_size = gfx::Size(500, 0);

  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, /*id=*/-1, gfx::Rect(800, 600)));
  delegate1.set_minimum_size(window_minimum_size);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, /*id=*/-1, gfx::Rect(500, 0, 800, 600)));
  delegate2.set_minimum_size(window_minimum_size);

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  WaitForOverviewEntered();
  VerifySplitViewOverviewSession(w1.get());
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  WaitForOverviewExitAnimation();

  EXPECT_TRUE(snap_group_divider());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
}

// Verify that when there is no gap between the edges of the windows and the
// divider in a Snap Group in portrait. See the regression at
// http://b/335323278.
TEST_F(SnapGroupTest, NoGapAfterSnapGroupCreationInPortrait) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  UpdateDisplay("768x1366");

  // The minimum size of the window is set to a value between 1/3 and 1/2 of the
  // work area's height to simulate the browser window CUJ.
  const gfx::Size window_minimum_size = gfx::Size(0, 500);

  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
      &delegate1, /*id=*/-1, gfx::Rect(800, 600)));
  delegate1.set_minimum_size(window_minimum_size);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, /*id=*/-1, gfx::Rect(500, 0, 800, 600)));
  delegate2.set_minimum_size(window_minimum_size);

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  WaitForOverviewEntered();
  VerifySplitViewOverviewSession(w1.get());
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  WaitForOverviewExitAnimation();

  EXPECT_TRUE(snap_group_divider());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
}

// Tests that the shelf's corners are rounded by default. Upon Snap Group
// creation,  the shelf's corners become sharp. When the Snap Group breaks, the
// shelf returns to its default state with rounded corners.
TEST_F(SnapGroupTest, ShelfBehavior) {
  ShelfLayoutManager* shelf_layout_manager =
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
  ASSERT_EQ(ShelfBackgroundType::kDefaultBg,
            shelf_layout_manager->shelf_background_type());

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Test that Shelf will be updated to have sharp rounded corners.
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            shelf_layout_manager->shelf_background_type());

  // Drag `w1` out to break the group.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().top_center());
  aura::test::TestWindowDelegate().set_window_component(HTCAPTION);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  event_generator->ReleaseLeftButton();

  // Verify that Shelf restores its default background type with rounded
  // corners.
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            shelf_layout_manager->shelf_background_type());
}

// Test that dragging a snapped window's caption hides the divider and that the
// snap group will be removed on drag complete.
TEST_F(SnapGroupTest, DragSnappedWindowExitPointTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  aura::test::TestWindowDelegate test_window_delegate;

  // Test dragging a snapped window out by mouse to exit the group.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().top_center());
  test_window_delegate.set_window_component(HTCAPTION);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  EXPECT_FALSE(snap_group_divider());

  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  MaximizeToClearTheSession(w2.get());
  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Test dragging a snapped window out by touch to exit the group.
  event_generator->MoveTouch(w1->GetBoundsInScreen().top_center());
  test_window_delegate.set_window_component(HTCAPTION);
  event_generator->PressTouch();
  event_generator->MoveTouchBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  EXPECT_FALSE(snap_group_divider());

  event_generator->ReleaseTouch();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that dragging a window within a Snap Group to the same snap position
// will break the existing Snap Group. See regression at http://b/335311879.
TEST_F(SnapGroupTest, DragSnappedWindowToSnapWithDifferentSnapRatio) {
  UpdateDisplay("1200x900");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  ASSERT_TRUE(IsInOverviewSession());
  ClickOverviewItem(GetEventGenerator(), w2.get());

  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  aura::test::TestWindowDelegate test_window_delegate;

  // Drag a snapped window out by mouse to exit the group.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(w2->GetBoundsInScreen().top_center());
  test_window_delegate.set_window_component(HTCAPTION);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w2.get())->is_dragged());
  // The existing Snap Group will break.
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Drag to re-snap `w2` to the same snap position but with
  // `chromeos::kDefaultSnapRatio`.
  event_generator->MoveMouseTo(gfx::Point(1250, 0));
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());

  // Two windows remain not in a Snap Group.
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that when snapping the snapped window to the opposite side, partial
// overview will be triggered and that the snap group will be removed.
TEST_F(SnapGroupTest, SnapToTheOppositeSideToExit) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Snap the current primary window as the secondary window, partial overview
  // will be triggered.
  SnapOneTestWindow(w1.get(),
                    /*state_type=*/WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Select the other window in overview to re-form a snap group.
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests to verify that dragging a window out of a snap group breaks the group
// and removes the divider.
TEST_F(SnapGroupTest, DragWindowOutToBreakSnapGroup) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(snap_group_divider());

  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(work_area_bounds().CenterPoint());
  EXPECT_FALSE(snap_group_divider());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Test that maximizing a snapped window breaks the snap group.
TEST_F(SnapGroupTest, MaximizeSnappedWindowExitPointTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  WindowState::Get(w2.get())->Maximize();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that the corresponding snap group will be removed when one of the
// windows in the snap group gets destroyed.
TEST_F(SnapGroupTest, WindowDestroyTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);

  // Destroy one window in the snap group and the entire snap group will be
  // removed.
  w1.reset();
  EXPECT_TRUE(snap_groups.empty());
  EXPECT_TRUE(window_to_snap_group_map.empty());
}

// Tests that if one window in the snap group is actiaved, the stacking order of
// the other window in the snap group will be updated to be right below the
// activated window i.e. the two windows in the snap group will be placed on
// top.
TEST_F(SnapGroupTest, WindowStackingOrderTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());

  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));

  wm::ActivateWindow(w3.get());

  // Actiave one of the windows in the snap group.
  wm::ActivateWindow(w1.get());

  MruWindowTracker::WindowList window_list =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  EXPECT_EQ(window_list, aura::WindowTracker::WindowList({
                             w1.get(),
                             w3.get(),
                             w2.get(),
                         }));

  // `w3` is stacked below `w2` even though the activation order of `w3` is
  // before `w2`.
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w2.get()));
}

// Tests that when there is one snapped window and overview open, creating a new
// window, i.e. by clicking the shelf icon, will auto-snap it.
TEST_F(SnapGroupTest, AutoSnapNewWindow) {
  // Snap `w1` to start split view overview session.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(),
                    /*state_type=*/WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // Create a new `w3`. Test it auto-snaps and forms a snap group with `w1`.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w3.get())->GetStateType());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w3.get()));
}

TEST_F(SnapGroupTest, DontAutoSnapNewWindowOutsideSplitViewOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_FALSE(
      RootWindowController::ForWindow(w1.get())->split_view_overview_session());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());

  // Open a third window. Test it does *not* snap.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_FALSE(WindowState::Get(w3.get())->IsSnapped());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(snap_group_divider()->divider_widget());
}

// Tests the snap ratio is updated correctly when resizing the windows in a snap
// group with the split view divider.
TEST_F(SnapGroupTest, SnapRatioTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  const gfx::Point hover_location =
      snap_group_divider_bounds_in_screen().CenterPoint();
  snap_group_divider()->StartResizeWithDivider(hover_location);
  const auto end_point =
      hover_location + gfx::Vector2d(-work_area_bounds().width() / 6, 0);
  snap_group_divider()->ResizeWithDivider(end_point);
  snap_group_divider()->EndResizeWithDivider(end_point);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio,
              WindowState::Get(w1.get())->snap_ratio().value(),
              /*abs_error=*/0.1);
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio,
              WindowState::Get(w2.get())->snap_ratio().value(),
              /*abs_error=*/0.1);
}

// Tests that the windows in a snap group can be resized to an arbitrary
// location with the split view divider if neither of the windows has the
// minimum size constraints.
TEST_F(SnapGroupTest, ResizeWithSplitViewDividerToArbitraryLocations) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  for (const int distance_delta : {-10, 6, -15}) {
    const auto w1_cached_bounds = w1.get()->GetBoundsInScreen();
    const auto w2_cached_bounds = w2.get()->GetBoundsInScreen();

    const gfx::Point hover_location =
        snap_group_divider_bounds_in_screen().CenterPoint();
    snap_group_divider()->StartResizeWithDivider(hover_location);
    snap_group_divider()->ResizeWithDivider(hover_location +
                                            gfx::Vector2d(distance_delta, 0));
    EXPECT_FALSE(split_view_controller()->InSplitViewMode());

    // TODO(michelefan): Consolidate the bounds update / calculation with the
    // existence of divider between clamshell and tablet mode. Change
    // `EXPECT_NEAR` back to `EXPECT_EQ`.
    const int abs_error = kSplitviewDividerShortSideLength / 2;
    EXPECT_NEAR(w1_cached_bounds.width() + distance_delta,
                w1.get()->GetBoundsInScreen().width(), abs_error);
    EXPECT_NEAR(w2_cached_bounds.width() - distance_delta,
                w2.get()->GetBoundsInScreen().width(), abs_error);
    EXPECT_NEAR(w1.get()->GetBoundsInScreen().width() +
                    w2.get()->GetBoundsInScreen().width() +
                    kSplitviewDividerShortSideLength,
                work_area_bounds().width(), abs_error);
  }
}

// Tests that the divider resizing respects the window's minimum size
// constraints.
TEST_F(SnapGroupTest, RespectWindowMinimumSizeWhileResizingWithDivider) {
  UpdateDisplay("1200x900");

  aura::test::TestWindowDelegate delegate1;
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithDelegate(
      &delegate1, /*id=*/-1, gfx::Rect(600, 500)));
  delegate1.set_minimum_size(gfx::Size(300, 600));

  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  SnapTwoTestWindows(window1.get(), window2.get());

  // The divider position updates while dragging, if it doesn't go below the
  // window's minimum size.
  snap_group_divider()->StartResizeWithDivider(
      snap_group_divider_bounds_in_screen().CenterPoint());
  snap_group_divider()->ResizeWithDivider(gfx::Point(400, 200));
  EXPECT_GT(snap_group_divider()->divider_position(), 300);
  snap_group_divider()->EndResizeWithDivider(gfx::Point(400, 200));
  EXPECT_GT(snap_group_divider()->divider_position(), 300);

  // Attempt to drag the divider below the window's minimum size. Verify it
  // stops at the minimum.
  snap_group_divider()->StartResizeWithDivider(
      snap_group_divider_bounds_in_screen().CenterPoint());
  snap_group_divider()->ResizeWithDivider(gfx::Point(200, 200));
  EXPECT_EQ(snap_group_divider()->divider_position(), 300);
  snap_group_divider()->EndResizeWithDivider(gfx::Point(200, 200));
  EXPECT_EQ(snap_group_divider()->divider_position(), 300);
}

// Tests that a snap group and the split view divider will be will be
// automatically created on two windows snapped in the clamshell mode. The snap
// group will be removed together with the split view divider on destroying of
// one window in the snap group.
TEST_F(SnapGroupTest, AutomaticallyCreateGroupOnTwoWindowsSnappedInClamshell) {
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller);
  const auto& snap_groups = snap_group_controller->snap_groups_for_testing();
  const auto& window_to_snap_group_map =
      snap_group_controller->window_to_snap_group_map_for_testing();
  EXPECT_TRUE(snap_groups.empty());
  EXPECT_TRUE(window_to_snap_group_map.empty());

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);

  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w1.get()));

  w1.reset();
  EXPECT_FALSE(snap_group_divider());
  EXPECT_TRUE(snap_groups.empty());
  EXPECT_TRUE(window_to_snap_group_map.empty());
}

// Tests we correctly end split view if partial overview is skipped and another
// window is snapped. Regression test for http://b/333600706.
TEST_F(SnapGroupTest, EndSplitView) {
  // Snap `w1` to start partial overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->primary_window());
  // Skip partial overview.
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  // Test we cleared the observed window.
  EXPECT_FALSE(split_view_controller()->primary_window());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Drag to snap `w2` to the opposite side.
  wm::ActivateWindow(w2.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(work_area_bounds().right_center());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  // Test we cleared the observed window.
  EXPECT_FALSE(split_view_controller()->secondary_window());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Activate `w1`. Test we don't create a snap group.
  wm::ActivateWindow(w1.get());
}

// Tests that when the auto-snapped window has minimum size that doesn't fit the
// work area, we adjust the divider and window bounds.
TEST_F(SnapGroupTest, AutoSnapWindowWithMinimumSize) {
  // Test with both the bottom and side shelf.
  for (const auto& shelf_alignment :
       {ShelfAlignment::kBottom, ShelfAlignment::kLeft}) {
    SCOPED_TRACE(base::StringPrintf("Shelf alignment = %d",
                                    static_cast<int>(shelf_alignment)));
    GetPrimaryShelf()->SetAlignment(shelf_alignment);
    // Create `w2` so it doesn't fit on the other side of `w1`.
    std::unique_ptr<aura::Window> w1(CreateAppWindow());
    aura::test::TestWindowDelegate delegate2;
    std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
        &delegate2, /*id=*/0, gfx::Rect(800, 600)));
    const gfx::Rect work_area(work_area_bounds());
    delegate2.set_minimum_size(
        gfx::Size(work_area.width() * 0.4f, work_area.height()));

    // Snap `w1` to start partial overview.
    SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                      chromeos::kTwoThirdSnapRatio);
    EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

    // Select `w2` to be auto-snapped.
    ClickOverviewItem(GetEventGenerator(), w2.get());
    EXPECT_TRUE(
        SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
    auto* snap_group =
        SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());

    // Expect `w2` is snapped at its minimum width and `w1` and the divider are
    // adjusted to fit.
    EXPECT_NEAR(work_area.width() * 0.4f, w2->GetBoundsInScreen().width(), 1);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     snap_group->snap_group_divider());
  }
}

// Tests that when both windows have minimum sizes, we don't create a snap
// group.
TEST_F(SnapGroupTest, AutoSnapBothWindowsWithMinimumSizes) {
  // Test with both the bottom and side shelf.
  for (const auto& shelf_alignment :
       {ShelfAlignment::kBottom, ShelfAlignment::kLeft}) {
    SCOPED_TRACE(base::StringPrintf("Shelf alignment = %d",
                                    static_cast<int>(shelf_alignment)));
    GetPrimaryShelf()->SetAlignment(shelf_alignment);
    // Create `w1` and `w2` both with minimum size 0.6f.
    aura::test::TestWindowDelegate delegate1;
    aura::test::TestWindowDelegate delegate2;
    std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
        &delegate1, /*id=*/-1, gfx::Rect(800, 600)));
    std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
        &delegate2, /*id=*/0, gfx::Rect(800, 600)));
    const gfx::Rect work_area(work_area_bounds());
    const int min_width = work_area.width() * 0.6f;
    delegate1.set_minimum_size(gfx::Size(min_width, work_area.height()));
    delegate2.set_minimum_size(gfx::Size(min_width, work_area.height()));

    // Snap `w1` to 2/3.
    SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                      chromeos::kTwoThirdSnapRatio);
    const gfx::Rect w1_bounds = w1->GetBoundsInScreen();
    EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

    // Select `w2` to be auto-snapped. Since it tries to snap to 0.6f, but `w1`
    // can't fit in the other side, we don't create a group.
    ClickOverviewItem(GetEventGenerator(), w2.get());
    EXPECT_NEAR(min_width, w2->GetBoundsInScreen().width(), 1);
    EXPECT_EQ(w1_bounds.width(), w1->GetBoundsInScreen().width());
    EXPECT_FALSE(
        SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
  }
}

// -----------------------------------------------------------------------------
// SnapGroupDividerTest:

using SnapGroupDividerTest = SnapGroupTest;

// Tests that the divider starts with a thin default width
// (`kSplitviewDividerShortSideLength`) in landscape mode, expands to
// `kSplitviewDividerEnlargedShortSideLength` on mouse hover or drag, and
// returns to its default thin width on mouse exit.
TEST_F(SnapGroupDividerTest, HoverToEnlargeDivider) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  SplitViewDivider* divider = snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  auto* divider_view = divider->divider_view_for_testing();
  ASSERT_TRUE(divider_view);
  auto* handler_view = divider_view->handler_view_for_testing();
  ASSERT_TRUE(handler_view);

  const auto divider_bounds_before_hover =
      divider_widget->GetWindowBoundsInScreen();
  EXPECT_EQ(kSplitviewDividerShortSideLength,
            divider_bounds_before_hover.width());

  const auto handler_view_bounds_before_hover =
      divider_view->GetHandlerViewBoundsInScreenForTesting();
  EXPECT_EQ(kDividerHandlerShortSideLength,
            handler_view_bounds_before_hover.width());
  EXPECT_EQ(kDividerHandlerLongSideLength,
            handler_view_bounds_before_hover.height());

  auto* event_generator = GetEventGenerator();

  // Shift the hover point so that it is not right on the divider handler view
  // to trigger hover to enlarge.
  event_generator->MoveMouseTo(
      divider_bounds_before_hover.CenterPoint() +
      gfx::Vector2d(0, kDividerHandlerEnlargedLongSideLength / 2 + 1));
  EXPECT_EQ(kSplitviewDividerEnlargedShortSideLength, divider_view->width());
  const auto handler_view_bounds_on_hover =
      divider_view->GetHandlerViewBoundsInScreenForTesting();
  EXPECT_EQ(kDividerHandlerEnlargedShortSideLength,
            handler_view_bounds_on_hover.width());
  EXPECT_EQ(kDividerHandlerEnlargedLongSideLength,
            handler_view_bounds_on_hover.height());

  event_generator->MoveMouseBy(10, 0);
  EXPECT_EQ(kSplitviewDividerEnlargedShortSideLength, divider_view->width());
  const auto handler_view_bounds_on_drag =
      divider_view->GetHandlerViewBoundsInScreenForTesting();
  EXPECT_EQ(kDividerHandlerEnlargedShortSideLength,
            handler_view_bounds_on_drag.width());
  EXPECT_EQ(kDividerHandlerEnlargedLongSideLength,
            handler_view_bounds_on_drag.height());

  event_generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_EQ(kSplitviewDividerShortSideLength, divider_view->width());
  const auto handler_view_bounds_after_hover =
      divider_view->GetHandlerViewBoundsInScreenForTesting();
  EXPECT_EQ(kDividerHandlerShortSideLength,
            handler_view_bounds_after_hover.width());
  EXPECT_EQ(kDividerHandlerLongSideLength,
            handler_view_bounds_after_hover.height());
}

// Tests that the split view divider will be stacked on top of both windows in
// the snap group and that on a third window activated the split view divider
// will be stacked below the newly activated window.
TEST_F(SnapGroupDividerTest, DividerStackingOrderTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  wm::ActivateWindow(w1.get());

  SplitViewDivider* divider = snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  aura::Window* divider_window = divider_widget->GetNativeWindow();
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), w1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(100, 200, 300, 400)));
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w3.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), w1.get()));

  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));
}

// Tests that divider will be closely tied to the windows in a snap group, which
// will also apply on transient window added.
TEST_F(SnapGroupDividerTest, DividerStackingOrderWithTransientWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  wm::ActivateWindow(w1.get());

  SplitViewDivider* divider = snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  aura::Window* divider_window = divider_widget->GetNativeWindow();
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), w1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  auto w1_transient =
      CreateTransientChildWindow(w1.get(), gfx::Rect(100, 200, 200, 200));
  w1_transient->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  wm::SetModalParent(w1_transient.get(), w1.get());
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w1_transient.get()));
}

// Tests the overall stacking order with two transient windows each of which
// belongs to a window in snap group is expected. The tests is to verify the
// transient windows issue showed in http://b/297448600#comment2.
TEST_F(SnapGroupDividerTest, DividerStackingOrderWithTwoTransientWindows) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  SplitViewDivider* divider = snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  aura::Window* divider_window = divider_widget->GetNativeWindow();
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  ASSERT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  // By default `w1_transient` is `MODAL_TYPE_NONE`, meaning that the associated
  // `w1` interactable.
  std::unique_ptr<aura::Window> w1_transient(
      CreateTransientChildWindow(w1.get(), gfx::Rect(10, 20, 20, 30)));

  // Add transient window for `w2` and making it not interactable by setting it
  // with the type of `ui::MODAL_TYPE_WINDOW`.
  std::unique_ptr<aura::Window> w2_transient(
      CreateTransientChildWindow(w2.get(), gfx::Rect(200, 20, 20, 30)));
  w2_transient->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  wm::SetModalParent(w2_transient.get(), w2.get());

  // The expected stacking order is as follows:
  //                    TOP
  // `w2_transient`      |
  //      |              |
  //   divider           |
  //      |              |
  //     `w2`            |
  //      |              |
  // `w1_transient`      |
  //      |              |
  //     `w1`            |
  //                   BOTTOM
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w2_transient.get()));
  EXPECT_TRUE(
      window_util::IsStackedBelow(w1_transient.get(), w2_transient.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1_transient.get(), divider_window));
}

// Tests that the union bounds of the primary window, secondary window in a snap
// group and the snap group divider will be equal to the work area bounds both
// in horizontal and vertical split view mode.
TEST_F(SnapGroupDividerTest, SnapGroupDividerBoundsTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  for (const auto is_horizontal : {true, false}) {
    if (is_horizontal) {
      UpdateDisplay("900x600");
    } else {
      UpdateDisplay("600x900");
    }

    ASSERT_EQ(IsLayoutHorizontal(w1.get()), is_horizontal);

    SnapTwoTestWindows(w1.get(), w2.get(), is_horizontal);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());

    MaximizeToClearTheSession(w1.get());
    MaximizeToClearTheSession(w2.get());
    ASSERT_FALSE(
        SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get()));
  }
}

// Tests that window and divider boundaries adjust correctly with shelf
// auto-hide behavior change.
TEST_F(SnapGroupDividerTest,
       SnapGroupDividerBoundsWithShelfAutoHideBehaviorChange) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  SplitViewDivider* divider = snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);

  Shelf* shelf = GetPrimaryShelf();
  ASSERT_EQ(shelf->auto_hide_behavior(), ShelfAutoHideBehavior::kNever);

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(divider_widget->GetWindowBoundsInScreen().height(),
            work_area_bounds().height());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), divider);
}

// Tests that snapped windows and divider bounds adjust correctly when shelf
// alignment changes.
TEST_F(SnapGroupDividerTest, SnapGroupDividerBoundsWithShelfAlignmentChange) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  SplitViewDivider* divider = snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);

  Shelf* shelf = GetPrimaryShelf();
  ASSERT_EQ(shelf->alignment(), ShelfAlignment::kBottom);
  for (auto alignment : {ShelfAlignment::kLeft, ShelfAlignment::kRight,
                         ShelfAlignment::kBottom}) {
    shelf->SetAlignment(alignment);
    const gfx::Rect divider_bounds = divider_widget->GetWindowBoundsInScreen();
    EXPECT_EQ(divider_bounds.x(), w1->GetBoundsInScreen().right());
    EXPECT_EQ(divider_bounds.right(), w2->GetBoundsInScreen().x());
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), divider);
  }
}

TEST_F(SnapGroupDividerTest, FeedbackButtonTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);

  SplitViewDividerView* divider_view =
      snap_group_divider()->divider_view_for_testing();
  auto* feedback_button = divider_view->feedback_button_for_testing();
  EXPECT_TRUE(feedback_button);

  // Verify that the feedback button is insivible by default.
  EXPECT_FALSE(feedback_button->GetVisible());

  // Test that the feedback button becomes visible upon hover on the divider.
  const gfx::Point hover_location =
      snap_group_divider_bounds_in_screen().CenterPoint() +
      gfx::Vector2d(0, kDividerHandlerEnlargedLongSideLength / 2 + 1);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(hover_location);
  EXPECT_TRUE(feedback_button->GetVisible());

  // Test that the feedback button will be invisible when drag starts.
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(10, 0);
  EXPECT_FALSE(feedback_button->GetVisible());

  // Test that the feedback button will be visible again when drag ends.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(feedback_button->GetVisible());

  // Test that open feedback dialog callback will be triggered.
  event_generator->MoveMouseTo(
      feedback_button->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
  EXPECT_EQ(1, static_cast<TestShellDelegate*>(Shell::Get()->shell_delegate())
                   ->open_feedback_dialog_call_count());
}

// Tests that the cursor type gets updated to be resize cursor on mouse hovering
// on the split view divider excluding the feedback button.
TEST_F(SnapGroupDividerTest, CursorUpdateTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* divider = snap_group_divider();
  ASSERT_TRUE(divider->divider_widget());

  auto divider_bounds = snap_group_divider_bounds_in_screen();
  auto outside_point = divider_bounds.CenterPoint();
  outside_point.Offset(-kSplitviewDividerShortSideLength * 5, 0);
  EXPECT_FALSE(divider_bounds.Contains(outside_point));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(CursorType::kPointer);

  // Test that the default cursor type when mouse is not hovered over the split
  // view divider.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(outside_point);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kNull, cursor_manager->GetCursor().type());

  // Test that the cursor changed to resize cursor while hovering over the split
  // view divider.
  const auto delta_vector = gfx::Vector2d(0, -10);
  const gfx::Point cached_hover_point =
      divider_bounds.CenterPoint() + delta_vector;
  event_generator->MoveMouseTo(cached_hover_point);
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());

  // Test that after resizing, the cursor type is still the resize cursor.
  event_generator->PressLeftButton();
  const auto move_vector = gfx::Vector2d(20, 0);
  event_generator->MoveMouseTo(cached_hover_point + move_vector);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());
  EXPECT_EQ(snap_group_divider_bounds_in_screen().CenterPoint() + delta_vector,
            cached_hover_point + move_vector);

  // Test that when hovering over the feedback button, the cursor type changed
  // back to the default type.
  SplitViewDividerView* divider_view =
      snap_group_divider()->divider_view_for_testing();
  auto* feedback_button = divider_view->feedback_button_for_testing();
  EXPECT_TRUE(feedback_button);
  event_generator->MoveMouseTo(divider_view->feedback_button_for_testing()
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  EXPECT_EQ(CursorType::kNull, cursor_manager->GetCursor().type());
}

//  Tests that the cursor updates correctly after snap to replace. See
//  regression at http://b/331240308
TEST_F(SnapGroupDividerTest, CursorUpdateAfterSnapToReplace) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Snapping `w3` on top of the snap group and expect the successful
  // snap-to-replace.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  ASSERT_TRUE(snap_group_divider()->divider_widget());

  auto divider_bounds = snap_group_divider_bounds_in_screen();
  auto outside_point = snap_group_divider_bounds_in_screen().CenterPoint();
  outside_point.Offset(-kSplitviewDividerShortSideLength * 5, 0);
  EXPECT_FALSE(divider_bounds.Contains(outside_point));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(CursorType::kPointer);

  // Test that the default cursor type when mouse is not hovered over the split
  // view divider.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(outside_point);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
  EXPECT_EQ(CursorType::kNull, cursor_manager->GetCursor().type());

  // Test that the cursor changed to resize cursor while hovering over the split
  // view divider.
  const auto delta_vector = gfx::Vector2d(0, -10);
  const gfx::Point cached_hover_point =
      divider_bounds.CenterPoint() + delta_vector;
  event_generator->MoveMouseTo(cached_hover_point);
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());

  // Test that after resizing, the cursor type is still the resize cursor.
  event_generator->PressLeftButton();
  const auto move_vector = gfx::Vector2d(20, 0);
  event_generator->MoveMouseTo(cached_hover_point + move_vector);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());
  EXPECT_EQ(snap_group_divider_bounds_in_screen().CenterPoint() + delta_vector,
            cached_hover_point + move_vector);

  // Test that when hovering over the feedback button, the cursor type changed
  // back to the default type.
  SplitViewDividerView* divider_view =
      snap_group_divider()->divider_view_for_testing();
  auto* feedback_button = divider_view->feedback_button_for_testing();
  EXPECT_TRUE(feedback_button);
  event_generator->MoveMouseTo(divider_view->feedback_button_for_testing()
                                   ->GetBoundsInScreen()
                                   .CenterPoint());
  EXPECT_EQ(CursorType::kNull, cursor_manager->GetCursor().type());
}

// Verify that the cursor changes to `kColumnResize` when hovering over the
// divider handler view in landscape.
TEST_F(SnapGroupDividerTest, CursorUpdateOnHandlerViewInLandscape) {
  UpdateDisplay("900x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* divider = snap_group_divider();
  ASSERT_TRUE(divider->divider_widget());

  auto divider_bounds = snap_group_divider_bounds_in_screen();

  auto* cursor_manager = Shell::Get()->cursor_manager();
  const auto center_point = divider_bounds.CenterPoint();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(center_point);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());

  event_generator->MoveMouseTo(center_point + gfx::Vector2d(0, 20));
  EXPECT_EQ(CursorType::kColumnResize, cursor_manager->GetCursor().type());
}

// Verify that the cursor changes to `kRowResize` when hovering over the divider
// handler view in portrait.
TEST_F(SnapGroupDividerTest, CursorUpdateOnHandlerViewInPortrait) {
  UpdateDisplay("600x900");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/false);
  auto* divider = snap_group_divider();
  ASSERT_TRUE(divider->divider_widget());

  auto divider_bounds = snap_group_divider_bounds_in_screen();

  auto* cursor_manager = Shell::Get()->cursor_manager();
  const auto center_point = divider_bounds.CenterPoint();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(center_point);
  EXPECT_TRUE(cursor_manager->IsCursorVisible());
  EXPECT_EQ(CursorType::kRowResize, cursor_manager->GetCursor().type());

  event_generator->MoveMouseTo(center_point + gfx::Vector2d(20, 0));
  EXPECT_EQ(CursorType::kRowResize, cursor_manager->GetCursor().type());
}

// Tests that the hit area of the snap group divider can be outside of its
// bounds with the extra insets whose value is `kSplitViewDividerExtraInset`.
TEST_F(SnapGroupDividerTest, SnapGroupDividerEnlargedHitArea) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);

  const gfx::Point cached_divider_center_point =
      snap_group_divider_bounds_in_screen().CenterPoint();
  auto* event_generator = GetEventGenerator();
  gfx::Point hover_location =
      cached_divider_center_point -
      gfx::Vector2d(kSplitviewDividerShortSideLength / 2 +
                        kSplitViewDividerExtraInset / 2,
                    0);
  event_generator->MoveMouseTo(hover_location);
  event_generator->PressLeftButton();
  const auto move_vector = -gfx::Vector2d(50, 0);
  event_generator->MoveMouseTo(hover_location + move_vector);
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(hover_location + move_vector,
            snap_group_divider_bounds_in_screen().CenterPoint());
}

// TODO(b/326481241): Currently it's not possible to swap windows since
// `SplitViewController` still manages the windows and updates the bounds in a
// `SnapGroup`. This will just check that double tap still works after
// conversion.
TEST_F(SnapGroupTest, DoubleTapDividerInTablet) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group = SnapGroupController::Get()->GetTopmostSnapGroup();
  EXPECT_TRUE(snap_group);
  auto* new_primary_window = snap_group->window1();
  auto* new_secondary_window = snap_group->window2();

  // Switch to tablet mode. Test that double tap on the divider swaps the
  // windows.
  SwitchToTabletMode();
  EXPECT_EQ(new_primary_window, split_view_controller()->primary_window());
  EXPECT_EQ(new_secondary_window, split_view_controller()->secondary_window());
  EXPECT_TRUE(split_view_divider()->divider_widget());
  const gfx::Point divider_center =
      split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  GetEventGenerator()->GestureTapAt(divider_center);
  GetEventGenerator()->GestureTapAt(divider_center);
  EXPECT_EQ(new_secondary_window, split_view_controller()->primary_window());
  EXPECT_EQ(new_primary_window, split_view_controller()->secondary_window());
}

// Tests to verify that when a window is dragged out of a snap group and onto
// another display, it snaps correctly with accurate bounds on the destination
// display. See regression at http://b/331663949.
TEST_F(SnapGroupTest, DragWindowOutOfSnapGroupToAnotherDisplay) {
  UpdateDisplay("800x700,801+0-800x700,1602+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(3U, displays.size());

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);

  const gfx::PointF point_in_display2(802, 0);
  EXPECT_FALSE(
      displays[0].bounds().Contains(gfx::ToRoundedPoint(point_in_display2)));
  EXPECT_TRUE(
      displays[1].bounds().Contains(gfx::ToRoundedPoint(point_in_display2)));

  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(gfx::ToRoundedPoint(point_in_display2));

  ASSERT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));

  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w2.get()).id());
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(w2.get())->GetStateType());

  gfx::Rect display1_left_half, display1_right_half;
  displays[1].work_area().SplitVertically(display1_left_half,
                                          display1_right_half);

  EXPECT_EQ(display1_left_half, w2->GetBoundsInScreen());
}

// Tests that when the cursor is moved significantly past the window sizes
// then moved the other direction, we don't update bounds.
TEST_F(SnapGroupDividerTest, ResizeCursor) {
  const int min_width = 300;
  std::unique_ptr<aura::Window> w1(
      CreateAppWindowWithMinSize(gfx::Size(min_width, min_width)));
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_divider = SnapGroupController::Get()
                                 ->GetSnapGroupForGivenWindow(w1.get())
                                 ->snap_group_divider();
  for (const auto& display_specs : {"800x600", "600x800"}) {
    UpdateDisplay(display_specs);
    const auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
    // Press and move the mouse left without releasing, past `w1`'s min width.
    // Test we don't update bounds beyond `w1`'s min width.
    auto* event_generator = GetEventGenerator();
    const gfx::Point divider_point(
        snap_group_divider->GetDividerBoundsInScreen(/*is_dragging=*/false)
            .CenterPoint());
    event_generator->set_current_screen_location(divider_point);
    event_generator->PressLeftButton();
    const bool horizontal = IsLayoutHorizontal(display);
    const gfx::Point resize_point1 = horizontal
                                         ? gfx::Point(10, divider_point.y())
                                         : gfx::Point(divider_point.x(), 10);
    event_generator->MoveMouseTo(resize_point1, /*count=*/2);
    ASSERT_TRUE(snap_group_divider->is_resizing_with_divider());
    EXPECT_EQ(min_width, GetWindowLength(w1.get(), horizontal));
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

    // Move the mouse right but still beyond `w1`'s min width. Test we don't
    // update bounds yet.
    const gfx::Point resize_point2 = horizontal
                                         ? gfx::Point(150, divider_point.y())
                                         : gfx::Point(divider_point.x(), 150);
    event_generator->MoveMouseTo(resize_point2, /*count=*/2);
    EXPECT_EQ(min_width, GetWindowLength(w1.get(), horizontal));
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

    // Move the mouse over `w1`'s min width. Test we update bounds now.
    const gfx::Point resize_point3 =
        horizontal ? gfx::Point(min_width, divider_point.y())
                   : gfx::Point(divider_point.x(), min_width);
    event_generator->MoveMouseTo(resize_point3,
                                 /*count=*/2);
    EXPECT_EQ(min_width, GetWindowLength(w1.get(), horizontal));
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

    // Move the mouse past `w1`'s min width. Test we update bounds now.
    const gfx::Point resize_point4 = horizontal
                                         ? gfx::Point(600, divider_point.y())
                                         : gfx::Point(divider_point.x(), 600);
    event_generator->MoveMouseTo(resize_point4,
                                 /*count=*/2);
    EXPECT_EQ(600,
              horizontal
                  ? snap_group_divider_bounds_in_screen().CenterPoint().x()
                  : snap_group_divider_bounds_in_screen().CenterPoint().y());
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
    event_generator->ReleaseLeftButton();
  }
}

// -----------------------------------------------------------------------------
// SnapGroupOverviewTest:
using SnapGroupOverviewTest = SnapGroupTest;

TEST_F(SnapGroupOverviewTest, OverviewEnterExitBasic) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);

  // Verify that full overview session is expected when starting overview from
  // accelerator and that split view divider will not be available.
  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(overview_controller->overview_session());
  EXPECT_EQ(GetOverviewGridBounds(w1->GetRootWindow()), work_area_bounds());
  EXPECT_FALSE(snap_group_divider()->divider_widget()->IsVisible());
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());

  // Verify that the snap group is restored with two windows snapped and that
  // the snap group divider becomes available on overview exit.
  ToggleOverview();
  EXPECT_FALSE(overview_controller->overview_session());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_TRUE(snap_group_divider()->divider_widget());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
}

// Tests that partial overview is shown on the other side of the screen on one
// window snapped.
TEST_F(SnapGroupOverviewTest, PartialOverview) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  auto* root_window = w1->GetRootWindow();
  for (const auto& snap_state :
       {WindowStateType::kPrimarySnapped, WindowStateType::kSecondarySnapped}) {
    SnapOneTestWindow(w1.get(), snap_state, chromeos::kDefaultSnapRatio);
    WaitForOverviewEnterAnimation();
    EXPECT_TRUE(OverviewController::Get()->overview_session());
    EXPECT_NE(GetOverviewGridBounds(root_window), work_area_bounds());
    EXPECT_NEAR(GetOverviewGridBounds(root_window).width(),
                work_area_bounds().width() / 2.f,
                kSplitviewDividerShortSideLength / 2.f);
  }
}

// Tests that the group item will be created properly and that the snap group
// will be represented as one group item in overview.
TEST_F(SnapGroupOverviewTest, OverviewGroupItemCreationBasic) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 2u);
}

// Verifies that the divider doesn't appear precipitously before the exit
// animation of the two windows in overview mode is complete, guaranteeing a
// seamless transition. See regression at http://b/333465871.
TEST_F(SnapGroupOverviewTest, DividerExitOverviewAnimation) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SplitViewDivider* divider = snap_group_divider();
  ASSERT_TRUE(divider);
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  ASSERT_TRUE(divider_widget->IsVisible());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  WaitForOverviewEntered();
  EXPECT_TRUE(divider_widget);
  EXPECT_FALSE(divider_widget->IsVisible());

  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  SendKey(ui::VKEY_RETURN, GetEventGenerator(), 0);

  // Verify that `divider_widget` remains invisible until overview exit
  // animation is complete.
  EXPECT_TRUE(divider_widget);
  EXPECT_FALSE(divider_widget->IsVisible());
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(divider_widget);
  EXPECT_TRUE(divider_widget->IsVisible());
}

// Tests that if one of the windows in a snap group gets destroyed in overview,
// the overview group item will only host the other window. If both of the
// windows get destroyed, the corresponding overview group item will be removed
// from the overview grid.
TEST_F(SnapGroupOverviewTest, WindowDestructionInOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  ASSERT_EQ(overview_grid->window_list().size(), 2u);

  // On one window in snap group destroying, the group item will host the other
  // window.
  w2.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 2u);

  // On the only remaining window in snap group destroying, the group item will
  // be removed from the overview grid.
  w1.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 1u);
}

// Tests that the rounded corners of the remaining item in the snap group on
// window destruction will be refreshed so that the exposed corners will be
// rounded corners.
TEST_F(SnapGroupOverviewTest, RefreshVisualsOnWindowDestructionInOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& overview_items = overview_grid->window_list();
  ASSERT_EQ(overview_items.size(), 2u);

  w2.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 2u);

  for (const auto& overview_item : overview_items) {
    const gfx::RoundedCornersF rounded_corners =
        overview_item->GetRoundedCorners();
    EXPECT_NEAR(rounded_corners.upper_left(), kWindowMiniViewCornerRadius,
                /*abs_error=*/0.01);
    EXPECT_NEAR(rounded_corners.upper_right(), kWindowMiniViewCornerRadius,
                /*abs_error=*/0.01);
    EXPECT_NEAR(rounded_corners.lower_right(), kWindowMiniViewCornerRadius,
                /*abs_error=*/0.01);
    EXPECT_NEAR(rounded_corners.lower_left(), kWindowMiniViewCornerRadius,
                /*abs_error=*/0.01);
  }
}

// Tests that when one of the window in snap group gets destroyed in overview,
// the other window will restore its bounds properly when been activated to exit
// overview.
TEST_F(SnapGroupOverviewTest,
       RemainingWindowBoundsRestoreAfterDestructionInOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_TRUE(snap_group_divider()->divider_widget());
  // Note here `w1` would have been shrunk for the divider width.
  const gfx::Size w1_size_before_overview = w1->GetBoundsInScreen().size();

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(w1->transform().IsIdentity());
  EXPECT_FALSE(w2->transform().IsIdentity());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  ASSERT_EQ(overview_grid->window_list().size(), 2u);

  // On one window in snap group destroying, the group item will host the other
  // window.
  w2.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 2u);

  ClickOverviewItem(GetEventGenerator(), w1.get());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get()));
  const gfx::Size w1_size_after_overview = w1->GetBoundsInScreen().size();

  // Verify that w1 is restored to its pre-overview bounds and any
  // divider-related margin adjustments have been reverted.
  EXPECT_EQ(
      w1_size_before_overview.width() + kSplitviewDividerShortSideLength / 2.f,
      w1_size_after_overview.width());

  // Verify that the transform is identity.
  EXPECT_TRUE(w1->transform().IsIdentity());
}

// Tests that the individual items within the same group will be hosted by the
// same overview group item.
TEST_F(SnapGroupOverviewTest, OverviewItemTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);

  EXPECT_EQ(overview_session->GetOverviewItemForWindow(w1.get()),
            overview_session->GetOverviewItemForWindow(w2.get()));
}

// Tests that the size of the `OverviewItem`s hosted by the `OverviewGroupItem`
// will correspond to the actual window layout.
TEST_F(SnapGroupOverviewTest, ReflectSnapRatioInOverviewGroupItem) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_TRUE(snap_group_divider()->divider_widget());
  const gfx::Point hover_location =
      snap_group_divider_bounds_in_screen().CenterPoint();
  snap_group_divider()->StartResizeWithDivider(hover_location);
  const gfx::Vector2d drag_delta(-work_area_bounds().width() / 6, 0);
  const auto end_point = hover_location + drag_delta;
  snap_group_divider()->ResizeWithDivider(end_point);
  snap_group_divider()->EndResizeWithDivider(end_point);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio,
              WindowState::Get(w1.get())->snap_ratio().value(),
              /*abs_error=*/0.01);
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio,
              WindowState::Get(w2.get())->snap_ratio().value(),
              /*abs_error=*/0.01);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);

  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w1.get()));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(overview_items.size(), 2u);

  // Since `w1` is roughly half the width of `w2`, verify that `item1_bounds` is
  // also half the width of `item2_bounds`.
  const gfx::RectF item1_bounds = overview_items[0]->target_bounds();
  const gfx::RectF item2_bounds = overview_items[1]->target_bounds();
  const float size_ratio =
      static_cast<float>(item1_bounds.width()) / item2_bounds.width();
  EXPECT_NEAR(size_ratio, 0.5, /*abs_error=*/0.05);
}

// Tests that snap group restores to its original snap ratio after on Overview
// exit.
TEST_F(SnapGroupOverviewTest, RestoreSnapRatioOnOverviewExit) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_TRUE(snap_group_divider()->divider_widget());

  // Drag the divider between the snapped windows to get the 1/3 and 2/3 split
  // screen.
  const gfx::Point hover_location =
      snap_group_divider_bounds_in_screen().CenterPoint();
  snap_group_divider()->StartResizeWithDivider(hover_location);
  const gfx::Vector2d drag_delta(-work_area_bounds().width() / 6, 0);
  const auto end_point = hover_location + drag_delta;
  snap_group_divider()->ResizeWithDivider(end_point);
  snap_group_divider()->EndResizeWithDivider(end_point);

  WindowState* w1_window_state = WindowState::Get(w1.get());
  WindowState* w2_window_state = WindowState::Get(w2.get());

  auto w1_snap_ratio_before = w1_window_state->snap_ratio();
  ASSERT_TRUE(w1_snap_ratio_before.has_value());
  auto w2_snap_ratio_before = w2_window_state->snap_ratio();
  ASSERT_TRUE(w2_snap_ratio_before.has_value());
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio, *w1_snap_ratio_before,
              /*abs_error=*/0.01);
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio, *w2_snap_ratio_before,
              /*abs_error=*/0.01);

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());

  // Both of the windows restored to their original snap ratio on Overview exit.
  auto w1_snap_ratio_after = w1_window_state->snap_ratio();
  ASSERT_TRUE(w1_snap_ratio_after.has_value());
  auto w2_snap_ratio_after = w2_window_state->snap_ratio();
  ASSERT_TRUE(w2_snap_ratio_after.has_value());
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio, *w1_snap_ratio_after,
              /*abs_error=*/0.01);
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio, *w2_snap_ratio_after,
              /*abs_error=*/0.01);
}

// Tests the individual close functionality of the `OverviewGroupItem` by
// clicking on the close button of each overview item.
TEST_F(SnapGroupOverviewTest, CloseIndividualWindowByCloseButton) {
  ScopedOverviewTransformWindow::SetImmediateCloseForTests(/*immediate=*/true);
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w0.get()));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(overview_items.size(), 2u);

  // Since the window will be deleted in overview, release the ownership to
  // avoid double deletion.
  w0.release();

  auto* event_generator = GetEventGenerator();
  const CloseButton* w0_close_button =
      overview_items[0]->overview_item_view()->close_button();
  event_generator->MoveMouseTo(
      w0_close_button->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();

  // Use the run loop so that to wait until the window is closed.
  base::RunLoop().RunUntilIdle();

  // Verify that only one item remains to be hosted by the group item.
  ASSERT_EQ(overview_items.size(), 1u);

  // Verify that the visuals of the remaining item will be refreshed with four
  // rounded corners applied.
  const gfx::RoundedCornersF rounded_corners =
      GetOverviewItemForWindow(w1.get())->GetRoundedCorners();
  EXPECT_NEAR(rounded_corners.upper_left(), kWindowMiniViewCornerRadius,
              /*abs_error=*/1);
  EXPECT_NEAR(rounded_corners.upper_right(), kWindowMiniViewCornerRadius,
              /*abs_error=*/1);
  EXPECT_NEAR(rounded_corners.lower_right(), kWindowMiniViewCornerRadius,
              /*abs_error=*/1);
  EXPECT_NEAR(rounded_corners.lower_left(), kWindowMiniViewCornerRadius,
              /*abs_error=*/1);
}

// Tests that the overview group item will be closed when focused in overview
// with `Ctrl + W`.
// TODO(michelefan@): Re-purpose this test. Currently disabled due to product
// decision change.
TEST_F(SnapGroupOverviewTest, DISABLED_CtrlPlusWToCloseFocusedGroupInOverview) {
  // Explicitly enable immediate close so that we can directly close the
  // window(s) without waiting the delayed task to be completed in
  // `ScopedOverviewTransformWindow::Close()`.
  ScopedOverviewTransformWindow::SetImmediateCloseForTests(/*immediate=*/true);
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(GetOverviewItemForWindow(w0.get()));

  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  EXPECT_TRUE(overview_session->focus_cycler_old()->GetFocusedItem());

  // Since the window will be deleted in overview, release the ownership to
  // avoid double deletion.
  w0.release();
  w1.release();
  SendKey(ui::VKEY_TAB, GetEventGenerator(), ui::EF_CONTROL_DOWN);

  // Verify that both windows in the snap group will be deleted.
  EXPECT_FALSE(w0.get());
  EXPECT_FALSE(w1.get());
}

// Tests that the minimized windows in a snap group will be shown as a single
// group item in overview.
// Disabled due to product decision change.
TEST_F(SnapGroupOverviewTest, DISABLED_MinimizedSnapGroupInOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  SnapGroupController::Get()->MinimizeTopMostSnapGroup();

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 1u);
}

// Tests that the bounds on the overview group item as well as the individual
// overview item hosted by the group item will be set correctly.
TEST_F(SnapGroupOverviewTest, OverviewItemBoundsTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_TRUE(wm::IsActiveWindow(w2.get()));

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);

  // The cumulative sum of the bounds while iterating through the individual
  // items hosted by the overview item should always be inside the group item
  // widget target bounds.
  auto* overview_group_item =
      overview_session->GetOverviewItemForWindow(w1.get());
  const gfx::RectF& group_item_bounds = overview_group_item->target_bounds();
  gfx::RectF cumulative_bounds;
  for (aura::Window* window : overview_group_item->GetWindows()) {
    auto* overview_item = overview_session->GetOverviewItemForWindow(window);
    cumulative_bounds.Union(overview_item->target_bounds());
    EXPECT_GT(cumulative_bounds.width(), 0u);
    EXPECT_TRUE(group_item_bounds.Contains(cumulative_bounds));
  }
}

// Tests the rounded corners will be applied to the exposed corners of the
// overview group item in horizontal wndow layout.
TEST_F(SnapGroupOverviewTest, OverviewGroupItemRoundedCornersInHorizontal) {
  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(100, 100));
  SnapTwoTestWindows(window0.get(), window1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 2u);
  for (const auto& overview_item : window_list) {
    EXPECT_EQ(overview_item->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }
}

// Tests the rounded corners will be applied to the exposed corners of the
// overview group item in vertical wndow layout.
TEST_F(SnapGroupOverviewTest, OverviewGroupItemRoundedCornersInVertical) {
  UpdateDisplay("600x900");
  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(100, 100));
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/false);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 2u);
  for (const auto& overview_item : window_list) {
    EXPECT_EQ(overview_item->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }
}

// Tests the rounded corners will be applied to the exposed corners of the
// overview group item if the corresponding snap group is minimized.
// Disabled due to product decision change.
TEST_F(SnapGroupOverviewTest,
       DISABLED_MinimizedSnapGroupRoundedCornersInOverview) {
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(100, 100)));
  SnapTwoTestWindows(w0.get(), w1.get());

  SnapGroupController::Get()->MinimizeTopMostSnapGroup();

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 2u);
  for (const auto& overview_item : window_list) {
    EXPECT_EQ(overview_item->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }
}

// Tests that the shadow for the group item in overview will be applied on the
// group-level.
TEST_F(SnapGroupOverviewTest, OverviewGroupItemShadow) {
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(100, 100)));
  SnapTwoTestWindows(w0.get(), w1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->overview_session());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 2u);

  // Wait until the post task to `UpdateRoundedCornersAndShadow()` triggered in
  // `OverviewController::DelayedUpdateRoundedCornersAndShadow()` is finished.
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  base::RunLoop().RunUntilIdle();
  for (const auto& overview_item : window_list) {
    const auto shadow_content_bounds =
        overview_item->get_shadow_content_bounds_for_testing();
    ASSERT_FALSE(shadow_content_bounds.IsEmpty());
    EXPECT_EQ(shadow_content_bounds.size(),
              gfx::ToRoundedSize(overview_item->target_bounds().size()));
  }
}

// Tests that when one of the windows in the snap group gets destroyed in
// overview the shadow contents bounds on the remaining item get updated
// correctly.
TEST_F(SnapGroupOverviewTest, CorrectShadowBoundsOnRemainingItemInOverview) {
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());

  // Create more windows to ensure the position of the `OverviewGroupItem` needs
  // to be updated during the Overview grid re-layout since the Overview grid
  // layout is left-aligned.
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(100, 100, 200, 100)));
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 200, 100, 200)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(100, 200, 200, 300)));
  std::unique_ptr<aura::Window> w5(
      CreateAppWindow(gfx::Rect(200, 100, 300, 200)));

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->overview_session());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 5u);

  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(window_list[4].get());
  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(overview_items.size(), 2u);

  w0.reset();
  EXPECT_EQ(window_list.size(), 5u);
  EXPECT_EQ(overview_items.size(), 1u);

  // Verify that the group-level shadow will be reset and the window-level
  // shadow bounds of the remaining item is refreshed to fit with the remaining
  // item.
  auto* group_shadow = overview_group_item->shadow_for_testing();
  EXPECT_FALSE(group_shadow);

  auto* window1_shadow = overview_items[0]->shadow_for_testing();
  ASSERT_TRUE(window1_shadow);
  EXPECT_EQ(gfx::ToRoundedSize(overview_group_item->target_bounds().size()),
            window1_shadow->GetContentBounds().size());
}

// Tests the basic functionality of activating a group item in overview with
// mouse or touch. Overview will exit upon mouse/touch release and the overview
// item that directly handles the event will be activated.
TEST_F(SnapGroupOverviewTest, GroupItemActivation) {
  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  SnapTwoTestWindows(window0.get(), window1.get());
  // Pre-check that `window1` is the active window between the windows in the
  // snap group.
  ASSERT_TRUE(wm::IsActiveWindow(window1.get()));
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(100, 100));
  ASSERT_TRUE(wm::IsActiveWindow(window2.get()));

  struct {
    bool use_touch;
    gfx::Vector2d offset;
    raw_ptr<aura::Window> expected_activated_window;
  } kTestCases[]{
      {false, gfx::Vector2d(-10, 0), window0.get()},
      {true, gfx::Vector2d(-10, 0), window0.get()},
      {false, gfx::Vector2d(10, 0), window1.get()},
      {true, gfx::Vector2d(10, 0), window1.get()},
  };

  OverviewController* overview_controller = OverviewController::Get();
  auto* event_generator = GetEventGenerator();

  for (const auto& test : kTestCases) {
    overview_controller->StartOverview(OverviewStartAction::kTests,
                                       OverviewEnterExitType::kImmediateEnter);
    ASSERT_TRUE(overview_controller->InOverviewSession());

    const auto* overview_grid =
        GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(overview_grid);
    const auto& window_list = overview_grid->window_list();
    ASSERT_EQ(window_list.size(), 2u);

    OverviewSession* overview_session = overview_controller->overview_session();
    auto* overview_item =
        overview_session->GetOverviewItemForWindow(window0.get());
    const auto hover_point =
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()) +
        test.offset;
    event_generator->set_current_screen_location(hover_point);
    if (test.use_touch) {
      event_generator->PressTouch();
      event_generator->ReleaseTouch();
    } else {
      event_generator->ClickLeftButton();
    }

    EXPECT_FALSE(overview_controller->InOverviewSession());

    // Verify that upon mouse/touch release, the snap group will be brought to
    // the front with the expected activated.
    EXPECT_TRUE(wm::IsActiveWindow(test.expected_activated_window));
  }
}

// Tests the basic drag and drop functionality for overview group item with both
// mouse and touch events. The group item will be dropped to its original
// position before drag started.
TEST_F(SnapGroupOverviewTest, DragAndDropBasic) {
  // Explicitly create another desk so that the virtual desk bar won't expand
  // from zero-state to expanded-state when dragging starts.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  SnapTwoTestWindows(window0.get(), window1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 1u);

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window0.get());
  auto* event_generator = GetEventGenerator();
  const auto target_bounds_before_dragging = overview_item->target_bounds();

  for (const bool by_touch : {false, true}) {
    DragGroupItemToPoint(
        overview_item,
        Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
        event_generator, by_touch, /*drop=*/false);
    EXPECT_NE(overview_item->target_bounds(), target_bounds_before_dragging);

    if (by_touch) {
      event_generator->ReleaseTouch();
    } else {
      event_generator->ReleaseLeftButton();
    }

    EXPECT_TRUE(overview_controller->InOverviewSession());

    // Verify that `overview_item` is dropped to its old position before
    // dragging.
    EXPECT_EQ(overview_item->target_bounds(), target_bounds_before_dragging);
  }
}

// Tests that the bounds of the drop target for `OverviewGroupItem` will match
// that of the corresponding item which the drop target is a placeholder for.
TEST_F(SnapGroupOverviewTest, DropTargetBoundsForGroupItem) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  SnapTwoTestWindows(window0.get(), window1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  aura::Window* primary_root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid = GetOverviewGridForRoot(primary_root_window);
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 1u);

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window0.get());
  auto* event_generator = GetEventGenerator();
  const gfx::RectF target_bounds_before_dragging =
      overview_item->target_bounds();

  for (const bool by_touch : {true}) {
    DragGroupItemToPoint(
        overview_item,
        Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
        event_generator, by_touch, /*drop=*/false);
    EXPECT_TRUE(overview_controller->InOverviewSession());

    auto* drop_target = overview_grid->drop_target();
    ASSERT_TRUE(drop_target);

    // Verify that the bounds of the `drop_target` will be the same as the
    // `target_bounds_before_dragging`.
    EXPECT_EQ(gfx::RectF(drop_target->target_bounds()),
              target_bounds_before_dragging);
    if (by_touch) {
      event_generator->ReleaseTouch();
    } else {
      event_generator->ReleaseLeftButton();
    }
  }
}

// Tests the stacking order of the overview group item should be above other
// overview items while being dragged.
TEST_F(SnapGroupOverviewTest, StackingOrderWhileDraggingInOverview) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> w0 = CreateAppWindow();
  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  std::unique_ptr<aura::Window> w2 = CreateAppWindow(gfx::Rect(100, 100));
  SnapTwoTestWindows(w0.get(), w1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 2u);

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* group_item = overview_session->GetOverviewItemForWindow(w0.get());
  auto* group_item_widget = group_item->item_widget();
  auto* w2_item_pre_drag = GetOverviewItemForWindow(w2.get());
  EXPECT_TRUE(window_util::IsStackedBelow(
      w2_item_pre_drag->item_widget()->GetNativeWindow(),
      group_item_widget->GetNativeWindow()));

  // Initiate the first drag.
  auto* event_generator = GetEventGenerator();
  DragGroupItemToPoint(
      group_item,
      Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
      event_generator, /*by_touch_gestures=*/false, /*drop=*/false);
  EXPECT_TRUE(overview_controller->InOverviewSession());

  auto* w2_item_during_drag = GetOverviewItemForWindow(w2.get());
  auto* w2_item_window_during_drag =
      w2_item_during_drag->item_widget()->GetNativeWindow();

  // Verify that the two windows together with the group item widget will be
  // stacked above the other overview item.
  EXPECT_TRUE(window_util::IsStackedBelow(
      w2_item_window_during_drag, group_item_widget->GetNativeWindow()));
  EXPECT_TRUE(
      window_util::IsStackedBelow(w2_item_window_during_drag, w0.get()));
  EXPECT_TRUE(
      window_util::IsStackedBelow(w2_item_window_during_drag, w1.get()));
  event_generator->ReleaseLeftButton();

  // Verify that the group item can be dragged again after completing the first
  // drag.
  DragGroupItemToPoint(
      group_item,
      Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
      event_generator, /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

// Verify that when dragging the group item, the close buttons of the individual
// items within the group are disabled with opacity set to 0, and their opacity
// is restored once the drag ends.
TEST_F(SnapGroupOverviewTest, HideCloseButtonsOnDragStart) {
  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  auto* window_widget0 = views::Widget::GetWidgetForNativeView(window0.get());
  views::test::TestWidgetObserver observer0(window_widget0);
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  SnapTwoTestWindows(window0.get(), window1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(window0.get()));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());

  auto* event_generator = GetEventGenerator();

  // On drag starts, the close buttons of the individual items are disabled and
  // their opacity is set to 0.
  DragGroupItemToPoint(
      overview_group_item,
      Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
      event_generator, /*by_touch_gestures=*/false, /*drop=*/false);
  for (const auto& item : overview_items) {
    auto* close_button = item->overview_item_view()->close_button();
    ASSERT_TRUE(item->overview_item_view()->close_button());
    EXPECT_EQ(close_button->layer()->GetTargetOpacity(), 0.f);
    EXPECT_FALSE(close_button->GetEnabled());
  }

  // On the drag and drop completes, the close buttons of the individual items
  // restore to their default state.
  event_generator->ReleaseLeftButton();
  for (const auto& item : overview_items) {
    auto* close_button = item->overview_item_view()->close_button();
    ASSERT_TRUE(item->overview_item_view()->close_button());
    EXPECT_EQ(close_button->layer()->GetTargetOpacity(), 1.f);
    EXPECT_TRUE(close_button->GetEnabled());
  }
}

// Tests that `OverviewGroupItem` is not snappable in overview when there are
// two windows hosted by it however when one of the windows gets destroyed in
// overview, the remaining item becomes snappable.
TEST_F(SnapGroupOverviewTest, GroupItemSnapBehaviorInOverview) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  SnapTwoTestWindows(window0.get(), window1.get());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 1u);

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window0.get());
  auto* event_generator = GetEventGenerator();
  const auto target_bounds_before_dragging = overview_item->target_bounds();
  const auto drag_point =
      Shell::GetPrimaryRootWindow()->GetBoundsInScreen().left_center();
  DragGroupItemToPoint(overview_item, drag_point, event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);

  DragGroupItemToPoint(overview_item, drag_point, event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_FALSE(overview_item->get_cannot_snap_widget_for_testing());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Verify that `overview_item` is dropped to its old position before
  // dragging.
  EXPECT_EQ(overview_item->target_bounds(), target_bounds_before_dragging);

  // Reset `window0` and verify that the remaining item becomes snappable.
  window0.reset();

  DragGroupItemToPoint(
      overview_session->GetOverviewItemForWindow(window1.get()), drag_point,
      event_generator, /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_EQ(WindowState::Get(window1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
}

// Tests that in non-primary display orientations, the visuals of the Snap Group
// item in Overview accurately represent the actual layout of the windows in the
// group, and that stepping through the windows in the Overview also follows the
// correct layout order. See http://b/339023083 for more details about the
// issue.
TEST_F(SnapGroupOverviewTest, OverviewGroupItemForNonPrimaryScreenOrientation) {
  // Update display to be in non-primary portrait mode.
  UpdateDisplay("1200x900/r");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(1U, displays.size());
  ASSERT_EQ(chromeos::OrientationType::kPortraitSecondary,
            chromeos::GetDisplayCurrentOrientation(displays[0]));

  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(200, 200));
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(gfx::Rect(100, 100));
  std::unique_ptr<aura::Window> window0 = CreateAppWindow(gfx::Rect(10, 10));

  // Drag `window0` to the **top** of the screen to snap it into the
  // **secondary** position, as the display is currently oriented in secondary
  // portrait mode.
  SnapOneTestWindow(window0.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_TRUE(IsInOverviewSession());

  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_RETURN, /*flags=*/0);
  ASSERT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window0.get(),
                                                                window1.get()));

  // With non-primary layout, this is the layout of `window0` and `window1`.
  //              +-----------+
  //              |           |
  //              |    w0     |
  //              |           |
  //              |-----------|
  //              |           |
  //              |    w1     |
  //              |           |
  //              +-----------+

  // Verify the windows bounds i.e. `window0` is on top (secondary snapped) and
  // `window1` in on bottom (primary snapped).
  gfx::Rect work_area = GetWorkAreaBoundsForWindow(window0.get());
  const gfx::Rect divider_bounds =
      snap_group_divider()->divider_widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(gfx::Rect(work_area.x(), work_area.y(), work_area.width(),
                      divider_bounds.y()),
            window0->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(work_area.x(), divider_bounds.bottom(), work_area.width(),
                      work_area.height() - divider_bounds.bottom()),
            window1->GetBoundsInScreen());

  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  std::unique_ptr<aura::Window> window3 =
      CreateAppWindow(gfx::Rect(300, 300), chromeos::AppType::CHROME_APP);
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  ToggleOverview();

  // Overview item list:
  // window3, [window0, window1], window2
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  event_generator->PressKey(ui::VKEY_TAB, /*flags=*/0);
  event_generator->PressKey(ui::VKEY_RETURN, /*flags=*/0);
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));
}

TEST_F(SnapGroupOverviewTest, SkipPairingInOverviewWhenClickingEmptyArea) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  WaitForOverviewEnterAnimation();
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());

  auto* w2_overview_item = GetOverviewItemForWindow(w2.get());
  EXPECT_TRUE(w2_overview_item);
  const gfx::Point outside_point =
      gfx::ToRoundedPoint(
          w2_overview_item->GetTransformedBounds().bottom_right()) +
      gfx::Vector2d(20, 20);

  // Verify that clicking on an empty area in overview will exit the paring.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(outside_point);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
  EXPECT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

TEST_F(SnapGroupOverviewTest, SkipPairingInOverviewWithEscapeKey) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(GetOverviewSession()->IsWindowInOverview(w2.get()));
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());

  GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(WindowState::Get(w1.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
  EXPECT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// -----------------------------------------------------------------------------
// SnapGroupDesksTest:
using SnapGroupDesksTest = SnapGroupTest;

// Tests that the two windows contained in the overview group item will be moved
// from the original desk to another desk on drag complete and that the two
// windows will still be in a snap group. The divider will show up in the
// destination desk on target desk activated.
TEST_F(SnapGroupDesksTest, DragOverviewGroupItemToAnotherDesk) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  SnapTwoTestWindows(window0.get(), window1.get());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 1u);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(mini_views.size(), 2u);

  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);

  // Verify the initial conditions before dragging the item to another desk.
  ASSERT_EQ(desks_util::GetDeskForContext(window0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(window1.get()), desk0);

  // Test that both windows contained in the overview group item will be moved
  // to the another desk.
  DragGroupItemToPoint(
      overview_controller->overview_session()->GetOverviewItemForWindow(
          window0.get()),
      mini_views[1]->GetBoundsInScreen().CenterPoint(), GetEventGenerator(),
      /*by_touch_gestures=*/false,
      /*drop=*/true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(desks_util::GetDeskForContext(window0.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(window1.get()), desk1);
  EXPECT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window0.get(),
                                                                window1.get()));
  ActivateDesk(desk1);
  EXPECT_TRUE(snap_group_divider()->divider_widget());
  EXPECT_EQ(desks_util::GetDeskForContext(
                snap_group_divider()->divider_widget()->GetNativeWindow()),
            desk1);
}

// Verify that there will be no crash when dragging the group item with the
// existence of bubble widget to another desk in overview. See the crash at
// http://b/311255082.
TEST_F(SnapGroupDesksTest,
       NoCrashWhenDraggingOverviewGroupItemWithBubbleToAnotherDesk) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> w0(CreateAppWindow(gfx::Rect(0, 0, 300, 300)));
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(500, 20, 200, 200)));
  SnapTwoTestWindows(w0.get(), w1.get());

  // Create a dummy view for the bubble, adding it to the `w0`.
  views::Widget* w0_widget = views::Widget::GetWidgetForNativeWindow(w0.get());
  auto* child_view =
      w0_widget->GetRootView()->AddChildView(std::make_unique<views::View>());
  child_view->SetBounds(100, 10, 20, 20);

  // Create a bubble widget that's anchored to `w0`.
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegateView>(
      child_view, views::BubbleBorder::TOP_RIGHT);

  // The line below is essential to make sure that the bubble doesn't get closed
  // when entering overview.
  bubble_delegate->set_close_on_deactivate(false);
  views::Widget* bubble_widget(views::BubbleDialogDelegateView::CreateBubble(
      std::move(bubble_delegate)));
  aura::Window* bubble_window = bubble_widget->GetNativeWindow();
  wm::AddTransientChild(w0.get(), bubble_window);

  bubble_widget->Show();
  EXPECT_TRUE(wm::HasTransientAncestor(bubble_window, w0.get()));

  // Verify that the bubble is created inside its anchor widget.
  EXPECT_TRUE(
      w0->GetBoundsInScreen().Contains(bubble_window->GetBoundsInScreen()));

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 1u);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(mini_views.size(), 2u);

  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);

  // Verify the initial conditions before dragging the item to another desk.
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  // Test that both windows contained in the overview group item are contained
  // in `desk1` after the drag.
  DragGroupItemToPoint(
      overview_controller->overview_session()->GetOverviewItemForWindow(
          w0.get()),
      mini_views[1]->GetBoundsInScreen().CenterPoint(), GetEventGenerator(),
      /*by_touch_gestures=*/false,
      /*drop=*/true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk1);
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w0.get(), w1.get()));
}

// Test: Dragging an `OverviewGroupItem` between desk containers (both
// containing `OverviewGroupItem`)
//  - Verify that an `OverviewGroupItem` can be dragged from one desk container
//  to another when both containers already have `OverviewGroupItem` present.
//  - Ensure no crashes occur during the process.
//  - Confirm that the OverviewGroupItem is reparented to the new desk
//  container.
// See http://b/333613078 for more details about the crash.
TEST_F(SnapGroupDesksTest, DragOverviewGroupItemToAnotherDeskWithSnapGroup) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);

  std::unique_ptr<aura::Window> w0(CreateAppWindow(gfx::Rect(0, 0, 300, 300)));
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(500, 20, 200, 200)));
  SnapTwoTestWindows(w0.get(), w1.get());
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  ActivateDesk(desk1);
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 20, 100, 200)));
  SnapTwoTestWindows(w2.get(), w3.get());
  ASSERT_EQ(desks_util::GetDeskForContext(w2.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(w3.get()), desk1);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(mini_views.size(), 2u);

  // Test that both windows contained in the overview group item will be moved
  // to the `desk0` and no crash on activating `desk0`.
  DragGroupItemToPoint(
      overview_controller->overview_session()->GetOverviewItemForWindow(
          w3.get()),
      mini_views[0]->GetBoundsInScreen().CenterPoint(), GetEventGenerator(),
      /*by_touch_gestures=*/false,
      /*drop=*/true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(desks_util::GetDeskForContext(w2.get()), desk0);
  EXPECT_EQ(desks_util::GetDeskForContext(w3.get()), desk0);
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w0.get(), w1.get()));
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w2.get(), w3.get()));
  ActivateDesk(desk0);
}

// Tests that if one of the windows parent container change in snap group, the
// other window will follow and get moved to the same target desk container. To
// simulate the CUJ, `Search + Shift + [ or ]` is used in the test to trigger
// moving active window to the left / right desk
TEST_F(SnapGroupDesksTest, WindowDeskContainerChange) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  // Use `Search + Shift + ]` to move `w0` and `w1` to `desk1`.
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desks_util::GetDeskForContext(w0.get()), desk1);
  EXPECT_EQ(desks_util::GetDeskForContext(w1.get()), desk1);

  ActivateDesk(desk1);

  // Use `Search + Shift + [` to move `w0` and `w1` to `desk0`
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  EXPECT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);
}

// Tests that switching desks in Overview mode with a Snap Group using a
// keyboard shortcut does not end the Overview session. Also verify that
// returning to the original desk where the Snap Group belongs does not re-snap
// the windows. See regression at http://b/334221711.
TEST_F(SnapGroupDesksTest, DeskSwitchingInOverview) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(IsInOverviewSession());

  // Use `Search + ]` to switch to `desk1`.
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(desk1->is_active());

  // Use `Search + [` to switch to `desk0`.
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(desk0->is_active());

  auto* overview_group_item = GetOverviewItemForWindow(w0.get());
  ASSERT_TRUE(overview_group_item);

  // Activate the group item and verify the union bounds.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_RETURN);
  UnionBoundsEqualToWorkAreaBounds(w0.get(), w1.get(), snap_group_divider());
}

TEST_F(SnapGroupDesksTest, DeskSwitchingWithKeyboardShortcut) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  // Use `Search + ]` to switch to `desk1`, `w0` and `w1` will remain on `desk0`
  // with `TargetVisibility()` equals to true.
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  EXPECT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);
  EXPECT_TRUE(w0->TargetVisibility());
  EXPECT_TRUE(w1->TargetVisibility());

  // Use `Search + [` to switch back to `desk0`, `w0` and `w1` will remain on
  // `desk0` with `TargetVisibility()` equals to true.
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN);
  EXPECT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  EXPECT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);
  EXPECT_TRUE(w0->TargetVisibility());
  EXPECT_TRUE(w1->TargetVisibility());
}

// Tests that after a Snap Group is resized and then moved to a different desk,
// the relative positions of the snapped windows within the group remain
// unchanged. See the regression details at http://b/335303673.
TEST_F(SnapGroupDesksTest, ResizeThenMoveGroupToAnotherDesk) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  auto* divider = snap_group_divider();
  ASSERT_TRUE(divider);
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      divider_widget->GetWindowBoundsInScreen().CenterPoint());

  event_generator->DragMouseBy(100, 0);
  const gfx::Rect cached_w0_bounds(w0->GetBoundsInScreen());
  const gfx::Rect cached_w1_bounds(w1->GetBoundsInScreen());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(w0->GetRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& window_list = overview_grid->window_list();
  ASSERT_EQ(window_list.size(), 1u);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(mini_views.size(), 2u);

  DragGroupItemToPoint(
      overview_controller->overview_session()->GetOverviewItemForWindow(
          w0.get()),
      mini_views[1]->GetBoundsInScreen().CenterPoint(), event_generator,
      /*by_touch_gestures=*/false,
      /*drop=*/true);

  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk1);

  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w0.get(), w1.get()));
  ActivateDesk(desk1);
  ASSERT_TRUE(divider_widget);
  EXPECT_EQ(desks_util::GetDeskForContext(divider_widget->GetNativeWindow()),
            desk1);

  EXPECT_EQ(cached_w0_bounds, w0->GetBoundsInScreen());
  EXPECT_EQ(cached_w1_bounds, w1->GetBoundsInScreen());
  UnionBoundsEqualToWorkAreaBounds(w0.get(), w1.get(), divider);
}

// Tests that pressing the 'Close All' button closes both windows in a Snap
// Group.
TEST_F(SnapGroupDesksTest, CloseAll) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  ASSERT_TRUE(desk0->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  auto* window_widget0 = views::Widget::GetWidgetForNativeView(w0.get());
  views::test::TestWidgetObserver observer0(window_widget0);
  auto* window_widget1 = views::Widget::GetWidgetForNativeView(w1.get());
  views::test::TestWidgetObserver observer1(window_widget1);

  // Pre-release ownership of `w0` and `w1` using `release()`. This is crucial
  // to avoid double-freeing memory. When unique_ptr goes out of scope, its
  // destructor will attempt to deallocate the owned memory. Since CloseAll will
  // already handle the window destruction, leaving the unique_ptrs to manage
  // the memory would lead to a second deallocation attempt on the same address,
  // resulting in crash.
  w0.release();
  w1.release();

  RemoveDesk(desk0, DeskCloseType::kCloseAllWindows);
  EXPECT_EQ(1u, desks_controller->desks().size());

  // Widget closure is asynchronous and may not finish immediately. For
  // guaranteed completion, run the current thread's RunLoop until idle (See
  // `NativeWidgetAura::Close()` for details).
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer0.widget_closed());
  EXPECT_TRUE(observer1.widget_closed());
}

// Verifies Snap Group behavior during desk removal and undo: windows become
// invisible during removal, and clicking 'Undo' restores them to their original
// desk.
TEST_F(SnapGroupDesksTest, DeskRemovalAndUndo) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  RemoveDesk(desk0, DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_TRUE(desk0->is_desk_being_removed());
  // `w0` and `w1` will remain invisible while the desk is being removed.
  EXPECT_FALSE(w0->IsVisible());
  EXPECT_FALSE(w1->IsVisible());

  // Restoring desk0 will also restore the visibility of `w0` and `w1`.
  views::LabelButton* dismiss_button =
      DesksTestApi::GetCloseAllUndoToastDismissButton();
  ASSERT_TRUE(dismiss_button);
  LeftClickOn(dismiss_button);
  EXPECT_TRUE(w0->IsVisible());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  EXPECT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));
}

// Tests that when an active desk is removed by keyboard shortcut `Search +
// Shift + -`, the Snap Group will be moved to the next available desk. The
// divider widget is visible when not in overview mode, and hidden when in
// overview mode. See the bug at http://b/335300918.
TEST_F(SnapGroupDesksTest, DeskRemovalAndEnterOverview) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  // Use `Search + Shift + -` to remove `desk0`.
  DeskSwitchAnimationWaiter waiter;
  PressAndReleaseKey(ui::VKEY_OEM_MINUS,
                     ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);
  waiter.Wait();

  // Verify that both `w0` and `w1` will be moved to `desk1`.
  EXPECT_TRUE(desk1->is_active());
  EXPECT_EQ(1u, desks_controller->desks().size());
  EXPECT_EQ(desk1, desks_util::GetDeskForContext(w0.get()));
  EXPECT_EQ(desk1, desks_util::GetDeskForContext(w1.get()));
  EXPECT_TRUE(snap_group);
  auto* divider = snap_group->snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  EXPECT_TRUE(divider_widget);
  EXPECT_TRUE(divider_widget->IsVisible());
  UnionBoundsEqualToWorkAreaBounds(w0.get(), w1.get(), divider);

  // Verify that the divider is invisible in Overview.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(divider_widget->IsVisible());

  // Verify that the divider is visible again on Overview exit.
  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());
  EXPECT_TRUE(divider_widget->IsVisible());
  UnionBoundsEqualToWorkAreaBounds(w0.get(), w1.get(), divider);
}

// Tests that the desk removal with only one active desk by keyboard shortcut
// `Search + Shift + -` will not be successful, the Snap Group will remain on
// the active desk.
TEST_F(SnapGroupDesksTest, DeskRemovalWithOneAciveDesk) {
  auto* desks_controller = DesksController::Get();
  ASSERT_EQ(1u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  // Use `Search + Shift + -` to attempt to remove `desk0`.
  PressAndReleaseKey(ui::VKEY_OEM_MINUS,
                     ui::EF_COMMAND_DOWN | ui::EF_SHIFT_DOWN);

  // Verify that both `w0` and `w1` will still be on `desk0`.
  EXPECT_EQ(1u, desks_controller->desks().size());
  EXPECT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  EXPECT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));
  EXPECT_TRUE(snap_group);
  auto* divider = snap_group->snap_group_divider();
  auto* divider_widget = divider->divider_widget();
  EXPECT_TRUE(divider_widget);
  EXPECT_TRUE(divider_widget->IsVisible());
  UnionBoundsEqualToWorkAreaBounds(w0.get(), w1.get(), divider);
}

// Test that merging a desk with a Snap Group into another desk doesn't cause
// crash and correctly moves all windows to the destination desk.
TEST_F(SnapGroupDesksTest, DesksMerge) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ActivateDesk(desk1);
  ASSERT_TRUE(desk1->is_active());
  ASSERT_FALSE(desk0->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w1.get()));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  // Merge `desk1` into `desk0`.
  RemoveDesk(desk1, DeskCloseType::kCombineDesks);

  // Verify that windows in the Snap Group are properly moved to the new desk.
  EXPECT_EQ(1u, desks_controller->desks().size());
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));
}

// Tests that create one Snap Group per desk and then switching between desks,
// the windows in the Snap Group remain visible. See regression at
// http://b/335477702.
TEST_F(SnapGroupDesksTest, OneSnapGroupOnEachDesk) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  // Create `snap_group0` on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group0 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group0);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  ActivateDesk(desk1);
  ASSERT_TRUE(desk1->is_active());
  ASSERT_FALSE(desk0->is_active());

  // Create `snap_group1` on `desk1`.
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w2.get(), w3.get());
  SnapGroup* snap_group1 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group1);
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w2.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w3.get()));

  // Activate `desk0` and verify that both windows in `snap_group0` are visible.
  ActivateDesk(desk0);
  EXPECT_TRUE(w0->IsVisible());
  EXPECT_TRUE(w1->IsVisible());

  // Activate `desk1` and verify that both windows in `snap_group1` are visible.
  ActivateDesk(desk1);
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w3->IsVisible());
}

// Verify that existing snap groups are hidden in partial overview mode only if
// they are located on the currently active desktop.
TEST_F(SnapGroupDesksTest, OnlyHideSnapGroupOnActiveDesk) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  // Create `snap_group0` on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get());
  SnapGroup* snap_group0 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group0);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  // Activate `desk1` and start partial Overview on `desk1`.
  ActivateDesk(desk1);
  ASSERT_TRUE(desk1->is_active());
  ASSERT_FALSE(desk0->is_active());

  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w2.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  VerifySplitViewOverviewSession(w2.get());

  // Verify that the target visibility of `w0` and `w1` are not affected.
  EXPECT_TRUE(w0->TargetVisibility());
  EXPECT_TRUE(w1->TargetVisibility());
}

// Tests that accessing the saved desks library after creating a Snap Group does
// not result in a crash, and the Snap Group is successfully restored upon
// exiting overview mode. See regression at http://b/335301800.
TEST_F(SnapGroupDesksTest, SaveDeskForSnapGroupWithAnotherSavedDesk) {
  OverviewController* overview_controller = OverviewController::Get();

  // Explicitly disable `disable_app_id_check_for_saved_desks_` otherwise "Save
  // desk for later" button will be disabled.
  overview_controller->set_disable_app_id_check_for_saved_desks_for_test(true);

  // Create `w0` and save `w0` in a saved desk by activing "Save desk for later"
  // button in Overview.
  std::unique_ptr<aura::Window> w0(
      CreateAppWindow(gfx::Rect(10, 10, 500, 300)));

  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);

  auto* root_window = Shell::GetPrimaryRootWindow();
  OverviewGrid* overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  ASSERT_EQ(1u, overview_grid->window_list().size());

  auto* save_for_later_button = overview_grid->GetSaveDeskForLaterButton();
  ASSERT_TRUE(save_for_later_button);
  base::RunLoop().RunUntilIdle();
  LeftClickOn(save_for_later_button);

  auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  auto* library_button = desks_bar_view->library_button();
  ASSERT_TRUE(library_button);

  overview_controller->EndOverview(OverviewEndAction::kOverviewButton);

  // Create a Snap Group and enter Overview again, click on the library button
  // on the virtual desks bar and verify that there is no crash.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(overview_session);

  overview_grid = GetOverviewGridForRoot(root_window);
  desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  library_button = desks_bar_view->library_button();
  ASSERT_TRUE(library_button);

  auto* overview_group_item = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(overview_group_item);
  ASSERT_FALSE(snap_group_divider()->divider_widget()->IsVisible());

  const auto cached_group_item_bounds = overview_group_item->target_bounds();

  LeftClickOn(library_button);

  // Click the point outside of `cached_group_item_bounds` will exit Overview
  // and bring back the Snap Group.
  auto* event_generator = GetEventGenerator();
  const gfx::Point click_point = gfx::ToRoundedPoint(
      cached_group_item_bounds.bottom_right() + gfx::Vector2d(20, 0));
  event_generator->MoveMouseTo(click_point);

  event_generator->ClickLeftButton();
  EXPECT_FALSE(IsInOverviewSession());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());

  overview_controller->set_disable_app_id_check_for_saved_desks_for_test(false);
}

// -----------------------------------------------------------------------------
// SnapGroupWindowCycleTest:

using SnapGroupWindowCycleTest = SnapGroupTest;

// Tests that the window list is reordered when there is snap group. The two
// windows will be adjacent with each other with physically left/top snapped
// window put before physically right/bottom snapped window.
TEST_F(SnapGroupWindowCycleTest, WindowReorderInAltTabInPrimaryOrientation) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindows(window0.get(), window1.get());

  wm::ActivateWindow(window2.get());
  // Initial window activation order: window2, [window1, window0].
  ASSERT_TRUE(wm::IsActiveWindow(window2.get()));

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);

  const auto& windows =
      window_cycle_controller->window_cycle_list()->windows_for_testing();

  // Test that the two windows in a snap group are reordered to be adjacent
  // with each other to reflect the window layout with the revised order as :
  // window2, [window0, window1].
  ASSERT_EQ(windows.size(), 3u);
  EXPECT_EQ(windows.at(0), window2.get());
  EXPECT_EQ(windows.at(1), window0.get());
  EXPECT_EQ(windows.at(2), window1.get());
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // With the activation of `window1`, `window0` will be inserted right before
  // `window1`.
  // The new window cycle list order as: [window0, window1], window2. Cycle
  // twice to focus on `window2`.
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/2);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Tests that the number of views to be cycled through inside the mirror
// container view of window cycle view will be the number of free-form windows
// plus snap groups.
TEST_F(SnapGroupWindowCycleTest, WindowCycleViewTest) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindows(window0.get(), window1.get());

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  const auto& windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(windows.size(), 3u);

  const WindowCycleView* cycle_view = window_cycle_list->cycle_view();
  ASSERT_TRUE(cycle_view);
  EXPECT_EQ(cycle_view->mirror_container_for_testing()->children().size(), 2u);
  CompleteWindowCycling();
}

// Tests that on window that belongs to a snap group destroying while cycling
// the window list with Alt + Tab, there will be no crash. The corresponding
// child mini view hosted by the group container view will be destroyed, the
// group container view will host the other child mini view.
TEST_F(SnapGroupWindowCycleTest, WindowInSnapGroupDestructionInAltTab) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindows(window0.get(), window1.get());

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  const auto& windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(windows.size(), 3u);

  const WindowCycleView* cycle_view = window_cycle_list->cycle_view();
  ASSERT_TRUE(cycle_view);
  // Verify that the number of child views hosted by mirror container is two at
  // the beginning.
  EXPECT_EQ(cycle_view->mirror_container_for_testing()->children().size(), 2u);

  // Destroy `window0` which belongs to a snap group.
  window0.reset();
  // Verify that we should still be cycling.
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const auto* updated_window_cycle_list =
      window_cycle_controller->window_cycle_list();
  const auto& updated_windows =
      updated_window_cycle_list->windows_for_testing();
  // Verify that the updated windows list size decreased.
  EXPECT_EQ(updated_windows.size(), 2u);

  // Verify that the number of child views hosted by mirror container will still
  // be two.
  EXPECT_EQ(cycle_view->mirror_container_for_testing()->children().size(), 2u);
}

// Tests and verifies the steps it takes to focus on a window cycle item by
// tabbing and reverse tabbing. The focused item will be activated upon
// completion of window cycling.
TEST_F(SnapGroupWindowCycleTest, SteppingInWindowCycleView) {
  std::unique_ptr<aura::Window> window3 =
      CreateAppWindow(gfx::Rect(300, 300), chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(200, 200), chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window1 =
      CreateAppWindow(gfx::Rect(100, 100), chromeos::AppType::BROWSER);
  std::unique_ptr<aura::Window> window0 =
      CreateAppWindow(gfx::Rect(10, 10), chromeos::AppType::BROWSER);

  SnapTwoTestWindows(window0.get(), window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  WindowState::Get(window3.get())->Activate();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  // Window cycle list:
  // window3, [window0, window1], window2
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/2);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Window cycle list:
  // [window0, window1], window3, window2
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Window cycle list:
  // [window0, window1], window3, window2
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Window cycle list:
  // window2, [window0, window1], window3
  CycleWindow(WindowCyclingDirection::kBackward, /*steps=*/1);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));
}

// Tests that the exposed rounded corners of the cycling items are rounded
// corners. The visuals will be refreshed on window destruction that belongs to
// a snap group.
TEST_F(SnapGroupWindowCycleTest, WindowCycleItemRoundedCorners) {
  std::unique_ptr<aura::Window> window0 =
      CreateAppWindow(gfx::Rect(100, 200), chromeos::AppType::BROWSER);
  std::unique_ptr<aura::Window> window1 =
      CreateAppWindow(gfx::Rect(200, 300), chromeos::AppType::BROWSER);
  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(300, 400), chromeos::AppType::BROWSER);
  SnapTwoTestWindows(window0.get(), window1.get());

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  const auto* cycle_view = window_cycle_list->cycle_view();
  auto& cycle_item_views = cycle_view->cycle_views_for_testing();
  ASSERT_EQ(cycle_item_views.size(), 2u);
  for (ash::WindowMiniViewBase* cycle_item_view : cycle_item_views) {
    EXPECT_EQ(cycle_item_view->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }

  // Destroy `window0` which belongs to a snap group while cycling.
  window0.reset();
  auto& new_cycle_item_views = cycle_view->cycle_views_for_testing();
  EXPECT_EQ(new_cycle_item_views.size(), 2u);

  // Verify that the visuals of the cycling items will be refreshed so that the
  // exposed corners will be rounded corners.
  for (ash::WindowMiniViewBase* cycle_item_view : new_cycle_item_views) {
    EXPECT_EQ(cycle_item_view->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }
  CompleteWindowCycling();
}

TEST_F(SnapGroupWindowCycleTest, WindowCycleItemRoundedCornersInPortait) {
  UpdateDisplay("600x900");

  std::unique_ptr<aura::Window> window0 =
      CreateAppWindow(gfx::Rect(100, 200), chromeos::AppType::BROWSER);
  std::unique_ptr<aura::Window> window1 =
      CreateAppWindow(gfx::Rect(200, 300), chromeos::AppType::BROWSER);
  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(300, 400), chromeos::AppType::BROWSER);
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/false);

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  const auto* cycle_view = window_cycle_list->cycle_view();
  auto& cycle_item_views = cycle_view->cycle_views_for_testing();
  ASSERT_EQ(cycle_item_views.size(), 2u);
  for (ash::WindowMiniViewBase* cycle_item_view : cycle_item_views) {
    EXPECT_EQ(cycle_item_view->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }
}

// Tests that in non-primary display orientations, the visuals of the Snap Group
// item within the `Alt + Tab` cycle view accurately represent the actual layout
// of the windows in the group, and that stepping through the windows in the
// Alt + Tab also follows the correct layout order. See http://b/339023083 for
// more details about the issue.
TEST_F(SnapGroupWindowCycleTest,
       WindowCycleItemForNonPrimaryScreenOrientation) {
  // Update display to be in non-primary portrait mode.
  UpdateDisplay("1200x900/r");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(1U, displays.size());
  ASSERT_EQ(chromeos::OrientationType::kPortraitSecondary,
            chromeos::GetDisplayCurrentOrientation(displays[0]));

  std::unique_ptr<aura::Window> window2 =
      CreateAppWindow(gfx::Rect(200, 200), chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window1 =
      CreateAppWindow(gfx::Rect(100, 100), chromeos::AppType::BROWSER);
  std::unique_ptr<aura::Window> window0 =
      CreateAppWindow(gfx::Rect(10, 10), chromeos::AppType::BROWSER);

  // Drag `window0` to the **top** of the screen to snap it into the
  // **secondary** position, as the display is currently oriented in secondary
  // portrait mode.
  SnapOneTestWindow(window0.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_TRUE(IsInOverviewSession());

  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
  GetEventGenerator()->PressKey(ui::VKEY_RETURN, /*flags=*/0);
  ASSERT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window0.get(),
                                                                window1.get()));

  // With non-primary layout, this is the layout of `window0` and `window1`.
  //              +-----------+
  //              |           |
  //              |    w0     |
  //              |           |
  //              |-----------|
  //              |           |
  //              |    w1     |
  //              |           |
  //              +-----------+

  // Verify the windows bounds i.e. `window0` is on top (secondary snapped) and
  // `window1` in on bottom (primary snapped).
  gfx::Rect work_area = GetWorkAreaBoundsForWindow(window0.get());
  const gfx::Rect divider_bounds =
      snap_group_divider()->divider_widget()->GetWindowBoundsInScreen();
  EXPECT_EQ(gfx::Rect(work_area.x(), work_area.y(), work_area.width(),
                      divider_bounds.y()),
            window0->GetBoundsInScreen());
  EXPECT_EQ(gfx::Rect(work_area.x(), divider_bounds.bottom(), work_area.width(),
                      work_area.height() - divider_bounds.bottom()),
            window1->GetBoundsInScreen());

  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Create `window3` and start testing the stepping.
  std::unique_ptr<aura::Window> window3 =
      CreateAppWindow(gfx::Rect(300, 300), chromeos::AppType::CHROME_APP);
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));

  // Window cycle list:
  // window3, [window0, window1], window2
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/2);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Window cycle list:
  // [window0, window1], window3, window2
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window0.get()));

  // Window cycle list:
  // [window0, window1], window3, window2
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/3);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  // Window cycle list:
  // window2, [window0, window1], window3
  CycleWindow(WindowCyclingDirection::kBackward, /*steps=*/1);
  CompleteWindowCycling();
  EXPECT_TRUE(wm::IsActiveWindow(window3.get()));
}

// Tests that two windows in a snap group is allowed to be shown as group item
// view only if both of them belong to the same app as the mru window. If only
// one window belongs to the app, the representation of the window will be shown
// as the individual window cycle item view.
TEST_F(SnapGroupWindowCycleTest, SameAppWindowCycle) {
  struct app_id_pair {
    const char* trace_message;
    const std::string app_id_2;
    const std::string app_id_3;
    const size_t windows_size;
    const size_t cycle_views_count;
  } kTestCases[]{
      {/*trace_message=*/"Windows in snap group with same app id",
       /*app_id_2=*/"A", /*app_id_3=*/"A", /*windows_size=*/4u,
       /*cycle_views_count=*/3u},
      {/*trace_message=*/"Windows in snap group with different app ids",
       /*app_id_2=*/"A", /*app_id_3=*/"B", /*windows_size=*/3u,
       /*cycle_views_count=*/3u},
  };

  std::unique_ptr<aura::Window> w0(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w1(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w2(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w3(CreateTestWindowWithAppID(std::string("A")));
  SnapTwoTestWindows(w2.get(), w3.get());
  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  for (const auto& test_case : kTestCases) {
    w2->SetProperty(kAppIDKey, std::move(test_case.app_id_2));
    w3->SetProperty(kAppIDKey, std::move(test_case.app_id_3));

    wm::ActivateWindow(w2.get());
    ASSERT_TRUE(wm::IsActiveWindow(w2.get()));

    // Simulate pressing Alt + Backtick to trigger the same app cycling.
    auto* event_generator = GetEventGenerator();
    event_generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
    event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_ALT_DOWN);

    const auto* window_cycle_list =
        window_cycle_controller->window_cycle_list();
    ASSERT_TRUE(window_cycle_list->same_app_only());

    // Verify the number of windows for the cycling.
    const auto& windows = window_cycle_list->windows_for_testing();
    EXPECT_EQ(windows.size(), test_case.windows_size);
    EXPECT_TRUE(window_cycle_controller->IsCycling());
    const auto* cycle_view = window_cycle_list->cycle_view();
    ASSERT_TRUE(cycle_view);

    // Verify the number of cycle views.
    auto& cycle_item_views = cycle_view->cycle_views_for_testing();
    EXPECT_EQ(cycle_item_views.size(), test_case.cycle_views_count);
    event_generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  }
}

// Tests and verifies that if one of the window in a snap group gets destroyed
// while doing same app window cycling the corresponding window cycle item view
// will be properly removed and re-configured with no crash.
TEST_F(SnapGroupWindowCycleTest, WindowDestructionDuringSameAppWindowCycle) {
  std::unique_ptr<aura::Window> w0(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w1(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w2(CreateTestWindowWithAppID(std::string("A")));
  SnapTwoTestWindows(w0.get(), w1.get());

  // Simulate pressing Alt + Backtick to trigger the same app cycling.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_ALT_DOWN);

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  ASSERT_TRUE(window_cycle_list->same_app_only());
  const auto* cycle_view = window_cycle_list->cycle_view();
  ASSERT_TRUE(cycle_view);
  const auto& windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(windows.size(), 3u);
  w0.reset();

  // After the window destruction, the window cycle view is still available.
  ASSERT_TRUE(cycle_view);
  const auto& updated_windows = window_cycle_list->windows_for_testing();
  EXPECT_EQ(updated_windows.size(), 2u);
  CompleteWindowCycling();
}

// Tests that if a snap group is at the beginning of a window cycling list, the
// mru window will depend on the mru window between the two windows in the snap
// group, since the windows are reordered so that it reflects the actual window
// layout.
TEST_F(SnapGroupWindowCycleTest, MruWindowForSameApp) {
  // Generate 5 windows with 3 of them from app A and 2 of them from app B.
  std::unique_ptr<aura::Window> w0(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w1(CreateTestWindowWithAppID(std::string("B")));
  std::unique_ptr<aura::Window> w2(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w3(CreateTestWindowWithAppID(std::string("A")));
  std::unique_ptr<aura::Window> w4(CreateTestWindowWithAppID(std::string("B")));
  SnapTwoTestWindows(w0.get(), w1.get());

  // Specifically activate the secondary snapped window with app type B.
  wm::ActivateWindow(w1.get());

  // Simulate pressing Alt + Backtick to trigger the same app cycling.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_MENU, ui::EF_NONE);
  event_generator->PressAndReleaseKey(ui::VKEY_OEM_3, ui::EF_ALT_DOWN);

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();
  const auto* window_cycle_list = window_cycle_controller->window_cycle_list();
  ASSERT_TRUE(window_cycle_list->same_app_only());
  const auto& windows = window_cycle_list->windows_for_testing();

  // Verify that the windows in the list that are been cycled all belong to app
  // B.
  EXPECT_EQ(windows.size(), 2u);
  CompleteWindowCycling();
}

// -----------------------------------------------------------------------------
// SnapGroupTabletConversionTest:

using SnapGroupTabletConversionTest = SnapGroupTest;

// Tests that after creating a snap group in clamshell, transition to tablet
// mode won't crash (b/288179725).
TEST_F(SnapGroupTabletConversionTest, NoCrashWhenRemovingGroupInTabletMode) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);

  SwitchToTabletMode();

  // Close w2. Test that the group is destroyed but we are still in split view.
  w2.reset();
  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w2.get()));
  EXPECT_EQ(split_view_controller()->primary_window(), w1.get());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
}

// Tests that one snap group in clamshell will be converted to windows in tablet
// split view. When converted back to clamshell, the snap group will be
// restored.
TEST_F(SnapGroupTabletConversionTest,
       ClamshellTabletTransitionWithOneSnapGroup) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  SnapTwoTestWindows(window1.get(), window2.get(), /*horizontal=*/true);
  EXPECT_TRUE(snap_group_divider()->divider_widget());
  UnionBoundsEqualToWorkAreaBounds(window1.get(), window2.get(),
                                   snap_group_divider());

  SwitchToTabletMode();
  EXPECT_FALSE(snap_group_divider());
  EXPECT_TRUE(split_view_divider()->divider_widget());
  // The snap group is removed in tablet mode.
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(
      snap_group_controller->GetSnapGroupForGivenWindow(window1.get()));
  EXPECT_EQ(window1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(window2.get(), split_view_controller()->secondary_window());
  UnionBoundsEqualToWorkAreaBounds(window1.get(), window2.get(),
                                   split_view_divider());
  EXPECT_NEAR(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window1.get())->snap_ratio(), 0.05);
  EXPECT_NEAR(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window2.get())->snap_ratio(), 0.05);

  ExitTabletMode();
  EXPECT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window1.get(),
                                                                window2.get()));
  EXPECT_NEAR(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window1.get())->snap_ratio(), 0.05);
  EXPECT_NEAR(chromeos::kDefaultSnapRatio,
              *WindowState::Get(window2.get())->snap_ratio(), 0.05);
  UnionBoundsEqualToWorkAreaBounds(window1.get(), window2.get(),
                                   snap_group_divider());
  EXPECT_TRUE(snap_group_divider()->divider_widget());
}

// Tests that when converting to tablet mode with split view divider at an
// arbitrary location, the bounds of the two windows and the divider will be
// updated such that the snap ratio of the layout is one of the fixed snap
// ratios.
TEST_F(SnapGroupTabletConversionTest,
       ClamshellTabletTransitionGetClosestFixedRatio) {
  UpdateDisplay("900x600");
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  SnapTwoTestWindows(window1.get(), window2.get(), /*horizontal=*/true);
  ASSERT_TRUE(snap_group_divider()->divider_widget());
  EXPECT_EQ(*WindowState::Get(window1.get())->snap_ratio(),
            chromeos::kDefaultSnapRatio);

  // Build test cases to be used for divider dragging, with expected fixed ratio
  // and corresponding pixels shown in the ASCII diagram below:
  //   ┌────────────────┬────────────────┐
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   │                │                │
  //   └─────────┬──────┼───────┬────────┘
  //             │      │       │
  // ratio:     1/3    1/2     2/3
  // pixel:     300    450     600      900

  struct {
    int distance_delta;
    float expected_snap_ratio;
  } kTestCases[]{{/*distance_delta=*/-200, chromeos::kOneThirdSnapRatio},
                 {/*distance_delta=*/400, chromeos::kTwoThirdSnapRatio},
                 {/*distance_delta=*/-180, chromeos::kDefaultSnapRatio}};

  auto* event_generator = GetEventGenerator();
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          split_view_controller()->root_window()->GetChildById(
              desks_util::GetActiveDeskContainerId()));
  for (const auto test_case : kTestCases) {
    event_generator->set_current_screen_location(
        snap_group_divider_bounds_in_screen().CenterPoint());
    event_generator->DragMouseBy(test_case.distance_delta, 0);
    snap_group_divider()->EndResizeWithDivider(
        event_generator->current_screen_location());
    SwitchToTabletMode();
    EXPECT_TRUE(split_view_divider() && !snap_group_divider());
    const auto current_divider_position =
        split_view_divider_bounds_in_screen().x();

    // We need to take into consideration of the variation introduced by the
    // divider shorter side length when calculating using snap ratio, i.e.
    // `kSplitviewDividerShortSideLength / 2`.
    const auto expected_divider_position = std::round(
        work_area_bounds_in_screen.width() * test_case.expected_snap_ratio -
        kSplitviewDividerShortSideLength / 2);

    // Verifies that the bounds of the windows and divider are updated correctly
    // such that snap ratio in the new window layout is expected.
    EXPECT_NEAR(current_divider_position, expected_divider_position,
                /*abs_error=*/1);
    EXPECT_NEAR(float(window1->GetBoundsInScreen().width()) /
                    work_area_bounds_in_screen.width(),
                test_case.expected_snap_ratio, /*abs_error=*/1);
    ExitTabletMode();
  }
}

// -----------------------------------------------------------------------------
// SnapGroupMultipleSnapGroupsTest:

using SnapGroupMultipleSnapGroupsTest = SnapGroupTest;

// Tests the basic functionalities of multiple snap groups.
TEST_F(SnapGroupMultipleSnapGroupsTest, MultipleSnapGroups) {
  // Create the 1st snap group.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  auto* snap_group1 =
      snap_group_controller->GetSnapGroupForGivenWindow(w2.get());
  auto* snap_group_divider1 = snap_group1->snap_group_divider();

  // Create a new window (w3) and maximize it. This will temporarily clear any
  // visible snapped windows, allowing the second snap group to be initialized.
  std::unique_ptr<aura::Window> w3(CreateAppWindow(gfx::Rect(0, 0, 800, 600)));

  // Create a 2nd group using a different snap ratio from `group1`.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  std::unique_ptr<aura::Window> w5(CreateAppWindow());
  ash::SnapOneTestWindow(w4.get(), WindowStateType::kPrimarySnapped,
                         chromeos::kTwoThirdSnapRatio);
  ClickOverviewItem(GetEventGenerator(), w5.get());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w4.get(), w5.get()));
  auto* snap_group2 =
      snap_group_controller->GetSnapGroupForGivenWindow(w4.get());
  auto* snap_group_divider2 = snap_group2->snap_group_divider();
  EXPECT_EQ(2u, snap_group_controller->snap_groups_for_testing().size());
  EXPECT_NE(snap_group_divider1, snap_group_divider2);
  aura::Window* divider1_window =
      snap_group_divider1->divider_widget()->GetNativeWindow();
  aura::Window* divider2_window =
      snap_group_divider2->divider_widget()->GetNativeWindow();

  // Spin the run loop to wait for the divider widgets to be closed and re-shown
  // during the 2nd snap group creation session. See
  // `SnapGroupController::OnOverviewModeStarting|EndingAnimationComplete()`.
  base::RunLoop().RunUntilIdle();

  // Ensure each snap group divider is directly attached to its associated
  // windows. Verify the stacking order is correct inside each group and across
  // different groups.
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());
  EXPECT_THAT(desk_container->children(),
              ElementsAre(/*group_1*/ w1.get(), w2.get(), divider1_window,
                          /*maximized_window*/ w3.get(), /*group_2*/ w4.get(),
                          w5.get(), divider2_window));
}

// Tests that the snap group can be recalled with its original bounds with
// multiple snap groups.
TEST_F(SnapGroupMultipleSnapGroupsTest, MultipleSnapGroupsRecall) {
  UpdateDisplay("900x600");
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  // Create the 1st snap group with 2/3 and 1/3 snap ratio.
  SnapOneTestWindow(w1.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  ASSERT_TRUE(IsInOverviewSession());
  ClickOverviewItem(GetEventGenerator(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  SnapGroup* snap_group1 =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  SplitViewDivider* snap_group_divider1 = snap_group1->snap_group_divider();
  const int divider_position1 = snap_group_divider1->divider_position();

  // Create a new window (w0) and maximize it. This will temporarily clear any
  // visible snapped windows, allowing the second snap group to be initialized.
  std::unique_ptr<aura::Window> w3(CreateAppWindow(gfx::Rect(0, 0, 800, 600)));

  // Create the 2nd group. Test the 2nd group's divider is at 1/2.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  std::unique_ptr<aura::Window> w5(CreateAppWindow());
  SnapTwoTestWindows(w4.get(), w5.get(), /*horizontal=*/true);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w4.get(), w5.get()));
  auto* snap_group2 =
      snap_group_controller->GetSnapGroupForGivenWindow(w4.get());
  auto* snap_group_divider2 = snap_group2->snap_group_divider();
  EXPECT_EQ(work_area_bounds().width() * chromeos::kDefaultSnapRatio -
                kSplitviewDividerShortSideLength / 2,
            snap_group_divider2->divider_position());

  // Activate `w2` to simulate selecting the window from the shelf. Test we
  // restore `w1`'s group.
  wm::ActivateWindow(w2.get());
  EXPECT_EQ(2u, snap_group_controller->snap_groups_for_testing().size());
  EXPECT_EQ(snap_group1, snap_group_controller->GetTopmostSnapGroup());

  // Spin the run loop to wait for the divider widgets to be closed and re-shown
  // during the 2nd snap group creation session. See
  // `SnapGroupController::OnOverviewModeStarting|EndingAnimationComplete()`.
  base::RunLoop().RunUntilIdle();

  // Verify the stacking order from bottom to top. The order for each group is:
  // {2nd_mru_window, mru_window, divider}, with `w0` on the bottom.
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());
  aura::Window* divider1 =
      snap_group_divider1->divider_widget()->GetNativeWindow();
  aura::Window* divider2 =
      snap_group_divider2->divider_widget()->GetNativeWindow();
  EXPECT_THAT(desk_container->children(),
              ElementsAre(/*maximized_window*/ w3.get(), /*group_2*/ w4.get(),
                          w5.get(), divider2,
                          /*group_1*/ w1.get(), w2.get(), divider1));

  // Verify the bounds of the 1st group are restored.
  EXPECT_EQ(divider_position1, snap_group_divider1->divider_position());
  EXPECT_EQ(divider_position1, w1->GetBoundsInScreen().width());
  EXPECT_EQ(divider_position1 + kSplitviewDividerShortSideLength,
            w2->GetBoundsInScreen().x());
}

// Tests that the snap groups will be hidden with two snapped window invisible
// in partial Overview. The visibility of the two snapped windows in snap group
// will be restored on overview ended. It only applies to partial Overview, the
// snap group will still be visible in full Overview.
TEST_F(SnapGroupMultipleSnapGroupsTest, DoNotShowSnapGroupsInPartialOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 200, 200, 200)));

  // Snap `w4` to start the partial Overview and verify that the snap group will
  // not show.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio);
  ASSERT_TRUE(IsInOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 1u);
  EXPECT_FALSE(w1->IsVisible());
  EXPECT_FALSE(w2->IsVisible());

  // The visibility of the snapped windows in a snap group will be restored on
  // Overview exit.
  OverviewController::Get()->EndOverview(OverviewEndAction::kKeyEscapeOrBack);
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Start full Overview and verify that snap group will show.
  OverviewController::Get()->StartOverview(
      OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(IsInOverviewSession());
  overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(overview_grid->window_list().size(), 3u);
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
}

// -----------------------------------------------------------------------------
// SnapGroupSnapToReplaceTest:

using SnapGroupSnapToReplaceTest = SnapGroupTest;

// Tests that when dragging a window to 'snap replace' a visible window in a
// snap group, the original window is replaced and a new snap group is created.
TEST_F(SnapGroupSnapToReplaceTest, SnapToReplaceBasic) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Verify that split view remains inactive to avoid split view specific
  // behaviors such as auto-snap or showing cannot snap toast.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests if the window being snapped to replace has a min size which is smaller
// than size calculated from the target snap ratio, it will be snapped at its
// min size instead.
TEST_F(SnapGroupSnapToReplaceTest, WindowWithMinimumSize) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Create a new `w3` with min size > 0.5f.
  const gfx::Size min_size(work_area_bounds().width() * 0.6f, 0);
  std::unique_ptr<aura::Window> w3 = CreateAppWindowWithMinSize(min_size);

  // Snap to replace `w3` on top of the group.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w3.get()));
  event_generator->DragMouseTo(0, 100);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w3.get())->GetStateType());

  // Test `w3` snaps at its min size and the group bounds are updated.
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(min_size.width(), w3->GetBoundsInScreen().width());
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w2.get(), snap_group_divider());
}

// Test that if both the window being snapped to replace and the opposite window
// in the group to replace have min sizes that can't fit, we don't allow snap to
// replace.
TEST_F(SnapGroupSnapToReplaceTest, BothWindowsMinimumSizes) {
  // Create a snap group where `w2` has min size 0.45f.
  const int work_area_length = work_area_bounds().width();
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(
      CreateAppWindowWithMinSize(gfx::Size(work_area_length * 0.45f, 0)));
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Create `w3` with min size 0.6f.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindowWithMinSize(gfx::Size(work_area_length * 0.6f, 0)));

  // Snap to replace `w3` over `w1`. Since `w3` and `w2` would not fit, test we
  // don't replace `w1` in the group.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w3.get()));
  event_generator->DragMouseTo(0, 100);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w3.get())->GetStateType());
  EXPECT_TRUE(w2->GetBoundsInScreen().Intersects(w3->GetBoundsInScreen()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w2.get(), w3.get()));
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that if a third window is snapped via any method except the window
// layout menu, it should allow 'snap to replace' regardless of snap ratio
// difference, the previous snap group's layout will be preserved.
TEST_F(SnapGroupSnapToReplaceTest,
       SnapToReplaceWithNonWindowLayoutSnapActionSource) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  const gfx::Rect w1_bounds(w1->GetBoundsInScreen());
  const gfx::Rect w2_bounds(w2->GetBoundsInScreen());

  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  const float w3_snap_ratio = 0.2;
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped, w3_snap_ratio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  EXPECT_GT(std::abs(w3_snap_ratio - chromeos::kDefaultSnapRatio),
            kSnapToReplaceRatioDiffThreshold);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(w3->GetBoundsInScreen(), w1_bounds);
  EXPECT_EQ(w2->GetBoundsInScreen(), w2_bounds);
}

// Test that the snap ratio difference is calculated before snap-to-replace when
// snapping from window layout menu. If it's below the threshold, the
// snap-to-replace action will occur. If not, we'll start a new faster
// split-screen session. The previous snap ratio will be preserved after window
// replacement within a snap group.
TEST_F(SnapGroupSnapToReplaceTest, SnapToReplaceWithRatioMargin) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  const float w1_snap_ratio = *WindowState::Get(w1.get())->snap_ratio();
  EXPECT_EQ(w1_snap_ratio, chromeos::kDefaultSnapRatio);
  EXPECT_EQ(*WindowState::Get(w1.get())->snap_ratio(),
            chromeos::kDefaultSnapRatio);

  // Snap the new window `w3` with `chromeos::kDefaultSnapRatio` snap ratio from
  // window layout menu. Since the difference between
  // `chromeos::kDefaultSnapRatio` and `w1_snap_ratio` is less than
  // `kSnapToReplaceRatioDiffThreshold`, replace w1 with w3. Maintain the
  // previous snap ratio in the snap group formed by `w1` and `w2`.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_LT(std::abs(w1_snap_ratio - chromeos::kDefaultSnapRatio),
            kSnapToReplaceRatioDiffThreshold);

  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(*WindowState::Get(w3.get())->snap_ratio(),
            chromeos::kDefaultSnapRatio);
  EXPECT_EQ(*WindowState::Get(w2.get())->snap_ratio(),
            chromeos::kDefaultSnapRatio);

  // Snap the new window `w4` with `chromeos::kOneThirdSnapRatio` ratio. Since
  // the difference between `w4_snap_event_snap_ratio` and snap ratio of `w3` is
  // greater than `kSnapToReplaceRatioDiffThreshold`, we will start a new faster
  // split-screen session.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kOneThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_GT(std::abs(*WindowState::Get(w3.get())->snap_ratio() -
                     chromeos::kOneThirdSnapRatio),
            kSnapToReplaceRatioDiffThreshold);
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w4.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w4.get(), w3.get()));
}

// Tests that when dragging another window to snap in Overview with the
// existence of snap group. The to-be-snapped window will not replace the window
// in the snap group. See http://b/333603509 for more details.
TEST_F(SnapGroupSnapToReplaceTest, DoNotSnapToReplaceSnapGroupInOverview) {
  std::unique_ptr<aura::Window> w0(
      CreateAppWindow(gfx::Rect(10, 10, 200, 100)));

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  auto* overview_item0 = GetOverviewItemForWindow(w0.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item0->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(0, 0));
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(WindowState::Get(w0.get())->GetStateType(),
            chromeos::WindowStateType::kPrimarySnapped);

  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w0.get(), w1.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w0.get(), w2.get()));
}

// -----------------------------------------------------------------------------
// SnapGroupDisplayMetricsTest:

using SnapGroupDisplayMetricsTest = SnapGroupTest;

// Tests that snapped window and divider widget bounds scale dynamically with
// display changes, preserving their relative snap ratio.
TEST_F(SnapGroupDisplayMetricsTest, DisplayScaleChange) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  const float w1_snap_ratio = *WindowState::Get(w1.get())->snap_ratio();
  const float w2_snap_ratio = *WindowState::Get(w2.get())->snap_ratio();

  SplitViewDivider* divider = snap_group_divider();
  ASSERT_TRUE(divider->divider_widget());

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto display_id = WindowTreeHostManager::GetPrimaryDisplayId();

  for (const bool zoom_in : {true, true, true, true, false, false, false}) {
    display_manager->ZoomDisplay(display_id, zoom_in);
    EXPECT_NEAR(w1_snap_ratio, *WindowState::Get(w1.get())->snap_ratio(), 0.01);
    EXPECT_NEAR(w2_snap_ratio, *WindowState::Get(w2.get())->snap_ratio(), 0.01);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), divider);
  }
}

// Tests that when rotating display, the bounds of the snapped windows and
// divider will be adjusted properly.
TEST_F(SnapGroupDisplayMetricsTest, DisplayRotation) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SplitViewDivider* divider = snap_group_divider();
  ASSERT_TRUE(divider->divider_widget());

  auto* display_manager = Shell::Get()->display_manager();
  for (auto rotation :
       {display::Display::ROTATE_270, display::Display::ROTATE_0}) {
    SCOPED_TRACE(
        base::StringPrintf("Screen rotation = %d", static_cast<int>(rotation)));
    display_manager->SetDisplayRotation(
        WindowTreeHostManager::GetPrimaryDisplayId(), rotation,
        display::Display::RotationSource::USER);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), divider);
  }
}

// Tests no crash when scaling up the work area. Regression test for
// http://b/331991853.
TEST_F(SnapGroupDisplayMetricsTest, ScaleUpWorkArea) {
  UpdateDisplay("800x600");

  // Set the min width so that the windows fit in `zoom_factor_1` but not
  // `zoom_factor_2`.
  const gfx::Size min_size(370, 0);
  std::unique_ptr<aura::Window> w1(CreateAppWindowWithMinSize(min_size));
  std::unique_ptr<aura::Window> w2(CreateAppWindowWithMinSize(min_size));

  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());

  // Zoom in once. Test we update the group bounds.
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const int64_t primary_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const float zoom_factor_1 = 1.05f;
  ASSERT_EQ(zoom_factor_1,
            display_manager()->GetDisplayInfo(primary_id).zoom_factor());
  ASSERT_FALSE(w1->GetBoundsInScreen().Intersects(w2->GetBoundsInScreen()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());

  // Zoom in again. Since the windows no longer fit, test we break the group.
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const float zoom_factor_2 = 1.1f;
  ASSERT_EQ(zoom_factor_2,
            display_manager()->GetDisplayInfo(primary_id).zoom_factor());
  ASSERT_TRUE(w1->GetBoundsInScreen().Intersects(w2->GetBoundsInScreen()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that when scaling up work area in Overview to make snapped windows no
// longer fit within the work area, the Snap Group will be broken upon Overview
// exit. See http://b/339719019 for more details.
TEST_F(SnapGroupDisplayMetricsTest, ScaleUpWorkAreaInOverview) {
  UpdateDisplay("800x600");

  // Set the min width so that the windows don't fit after zooming in using
  // keyboard shortcut.
  const gfx::Size min_size(395, 0);
  std::unique_ptr<aura::Window> w1(CreateAppWindowWithMinSize(min_size));
  std::unique_ptr<aura::Window> w2(CreateAppWindowWithMinSize(min_size));

  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  OverviewGroupItem* group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w1.get()));
  const auto& items = group_item->overview_items_for_testing();
  CHECK_EQ(2U, items.size());
  const auto& overview_item1 = items[0];
  const auto& overview_item2 = items[1];
  ASSERT_FALSE(overview_item1->target_bounds().Intersects(
      overview_item2->target_bounds()));
  ASSERT_FALSE(GetUnionScreenBoundsForWindow(w1.get()).Intersects(
      GetUnionScreenBoundsForWindow(w2.get())));

  // Zoom in to make the windows no longer fit, the Snap Group should be broken.
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const int64_t primary_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  ASSERT_EQ(1.05f, display_manager()->GetDisplayInfo(primary_id).zoom_factor());
  ASSERT_TRUE(overview_item1->target_bounds().Intersects(
      overview_item2->target_bounds()));
  ASSERT_TRUE(GetUnionScreenBoundsForWindow(w1.get()).Intersects(
      GetUnionScreenBoundsForWindow(w2.get())));

  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_FALSE(snap_group_divider());
}

// Tests that there is no crash when work area changed after snapping two
// windows. Docked mananifier is used as an example to trigger the work area
// change.
TEST_F(SnapGroupDisplayMetricsTest, DockedMagnifier) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* docked_mangnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_mangnifier_controller->SetEnabled(/*enabled=*/true);
}

// Tests verifying virtual keyboard activation/deactivation which triggers work
// area change works properly with Snap Group.
TEST_F(SnapGroupDisplayMetricsTest, VirtualKeyboard) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  SetVirtualKeyboardEnabled(/*enabled=*/true);
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());

  keyboard_controller->HideKeyboardByUser();
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
}

// Tests verifying ChromeVox activation/deactivation which triggers work area
// change works properly with Snap Group.
TEST_F(SnapGroupDisplayMetricsTest, ChromeVox) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());

  auto* a11y_controller = Shell::Get()->accessibility_controller();
  PressAndReleaseKey(ui::VKEY_Z, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(a11y_controller->spoken_feedback().enabled());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());

  PressAndReleaseKey(ui::VKEY_Z, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);

  EXPECT_FALSE(a11y_controller->spoken_feedback().enabled());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
}

// -----------------------------------------------------------------------------
// SnapGroupMultiDisplayTest:

using SnapGroupMultiDisplayTest = SnapGroupTest;

// Tests that snapping two windows on an external display now works as expected,
// with both windows and the divider fully visible on the external display. This
// addresses the previous issue where the snapped window would be off-screen.
TEST_F(SnapGroupMultiDisplayTest, SnapGroupCreationOnExternalDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());

  // Create Snap Group on display #2.
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(900, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(1000, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);

  // Verify that both windows and divider are visible on display #2.
  VerifySnapGroupOnDisplay(snap_group, displays[1].id());

  // Start resizing to the left.
  auto* event_generator = GetEventGenerator();
  auto* snap_group_divider = snap_group->snap_group_divider();
  const gfx::Point divider_point(
      snap_group_divider_bounds_in_screen().CenterPoint());
  event_generator->set_current_screen_location(divider_point);
  event_generator->PressLeftButton();

  // Resize to a point between `w1` and `w2`'s minimum sizes.
  const gfx::Point resize_point1 = gfx::Point(950, divider_point.y());
  const bool horizontal = IsLayoutHorizontal(displays[1]);
  // The windows have a default min width of 104.
  const int min_length = GetMinimumWindowLength(w1.get(), horizontal);
  ASSERT_EQ(min_length, GetMinimumWindowLength(w2.get(), horizontal));
  ASSERT_EQ(104, min_length);
  event_generator->MoveMouseTo(resize_point1, /*count=*/2);
  EXPECT_EQ(resize_point1.x(),
            snap_group_divider_bounds_in_screen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

  // Resize to the left to a point less than `w1`'s minimum width.
  const gfx::Point resize_point2 = gfx::Point(810, divider_point.y());
  event_generator->MoveMouseTo(resize_point2, /*count=*/2);
  EXPECT_EQ(min_length, w1->GetBoundsInScreen().width());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

  // Resize to the right to a point less than `w2`'s minimum width.
  const gfx::Point resize_point3 = gfx::Point(1500, divider_point.y());
  event_generator->MoveMouseTo(resize_point3, /*count=*/2);
  EXPECT_EQ(min_length, w2->GetBoundsInScreen().width());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
}

TEST_F(SnapGroupMultiDisplayTest, NoGapAfterSnapGroupCreation) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);

  UpdateDisplay("1366x768,1367+0-1366x768");
  const gfx::Size window_minimum_size = gfx::Size(500, 0);

  for (const int window_x_origin : {0, 1367}) {
    SCOPED_TRACE(base::StringPrintf("window origin = %d", window_x_origin));

    aura::test::TestWindowDelegate delegate1;
    std::unique_ptr<aura::Window> w1(CreateTestWindowInShellWithDelegate(
        &delegate1, /*id=*/-1, gfx::Rect(window_x_origin, 0, 800, 600)));
    delegate1.set_minimum_size(window_minimum_size);
    aura::test::TestWindowDelegate delegate2;
    std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
        &delegate2, /*id=*/-1, gfx::Rect(window_x_origin + 500, 0, 800, 600)));
    delegate2.set_minimum_size(window_minimum_size);

    SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                      chromeos::kTwoThirdSnapRatio);
    WaitForOverviewEntered();

    ClickOverviewItem(GetEventGenerator(), w2.get());
    EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
              WindowState::Get(w2.get())->GetStateType());
    WaitForOverviewExitAnimation();

    EXPECT_TRUE(snap_group_divider());
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
  }
}

// Tests that removing a display during split view overview session doesn't
// crash.
TEST_F(SnapGroupMultiDisplayTest, RemoveDisplay) {
  UpdateDisplay("800x600,801+0-800x600");
  display::test::DisplayManagerTestApi display_manager_test(display_manager());

  // Snap `window` on the second display to start split view overview session.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindowInShellWithBounds(gfx::Rect(900, 0, 100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindowInShellWithBounds(gfx::Rect(1000, 0, 100, 100)));
  WindowState* window_state = WindowState::Get(window1.get());
  const WindowSnapWMEvent snap_type(
      WM_EVENT_SNAP_PRIMARY,
      /*snap_action_source=*/WindowSnapActionSource::kTest);
  window_state->OnWMEvent(&snap_type);
  ASSERT_EQ(display_manager_test.GetSecondaryDisplay().id(),
            display::Screen::GetScreen()
                ->GetDisplayNearestWindow(window1.get())
                .id());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_TRUE(RootWindowController::ForWindow(window1.get())
                  ->split_view_overview_session());

  // Disconnect the second display. Test no crash.
  UpdateDisplay("800x600");
  base::RunLoop().RunUntilIdle();
}

// Tests to verify that when a window is dragged out of a snap group and onto
// another display, it snaps correctly with accurate bounds on the destination
// display. See regression at http://b/331663949.
TEST_F(SnapGroupMultiDisplayTest, DragWindowOutOfSnapGroupToAnotherDisplay) {
  UpdateDisplay("800x700,801+0-800x700,1602+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(3U, displays.size());

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);

  const gfx::Point point_in_display2(802, 0);
  ASSERT_FALSE(displays[0].bounds().Contains(point_in_display2));
  ASSERT_TRUE(displays[1].bounds().Contains(point_in_display2));

  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(point_in_display2);

  ASSERT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));

  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w2.get()).id());
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(w2.get())->GetStateType());

  gfx::Rect display1_left_half, display1_right_half;
  displays[1].work_area().SplitVertically(display1_left_half,
                                          display1_right_half);

  EXPECT_EQ(display1_left_half, w2->GetBoundsInScreen());
}

// Test that Search+Alt+M moves the snap group between displays.
TEST_F(SnapGroupMultiDisplayTest, MoveSnapGroupBetweenDisplays) {
  UpdateDisplay("800x600,1000x600");

  // Snap `w1` and `w2` on display 1.
  std::unique_ptr<aura::Window> w1(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_divider = SnapGroupController::Get()
                                 ->GetSnapGroupForGivenWindow(w1.get())
                                 ->snap_group_divider();
  const int64_t primary_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(primary_id, screen->GetDisplayNearestWindow(w1.get()).id());
  ASSERT_EQ(primary_id, screen->GetDisplayNearestWindow(w2.get()).id());

  // Activate `w1`, then press Search+Alt+M to move it to display 2.
  wm::ActivateWindow(w1.get());
  PressAndReleaseKey(ui::VKEY_M, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();
  ASSERT_EQ(secondary_id, screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(w2.get()).id());
  aura::Window* divider_window =
      snap_group_divider->divider_widget()->GetNativeWindow();
  EXPECT_EQ(secondary_id, screen->GetDisplayNearestWindow(divider_window).id());

  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetRootWindowForDisplayId(secondary_id));

  MruWindowTracker* mru_window_tracker = Shell::Get()->mru_window_tracker();
  aura::Window* mru_window = window_util::GetTopMostWindow(
      mru_window_tracker->BuildMruWindowList(DesksMruType::kActiveDesk));

  // `w1` will be the mru window. With the window stacking fixed by
  // `window_util::FixWindowStackingAccordingToGlobalMru()`, the `w2` that gets
  // moved after will be stacked above `w1`.
  EXPECT_EQ(mru_window, w1.get());
  EXPECT_THAT(desk_container->children(),
              ElementsAre(w2.get(), w1.get(), divider_window));

  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
}

// Tests that moving an `OverviewGroupItem` between displays correctly
// relocates the group item and its windows without crashing, while maintaining
// divider widget invisibility during the overview session.
TEST_F(SnapGroupMultiDisplayTest, MoveSnapGroupBetweenDisplaysInOverview) {
  UpdateDisplay("800x700,801+0-800x700,1602+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(3U, displays.size());

  const gfx::Point point_in_display2(900, 100);
  EXPECT_FALSE(displays[0].bounds().Contains(point_in_display2));
  EXPECT_TRUE(displays[1].bounds().Contains(point_in_display2));
  EXPECT_FALSE(displays[2].bounds().Contains(point_in_display2));

  const gfx::Point point_in_display3(1700, 200);
  EXPECT_FALSE(displays[0].bounds().Contains(point_in_display3));
  EXPECT_FALSE(displays[1].bounds().Contains(point_in_display3));
  EXPECT_TRUE(displays[2].bounds().Contains(point_in_display3));

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* divider = snap_group_divider();
  ASSERT_TRUE(divider);
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  ASSERT_TRUE(divider_widget->IsVisible());

  struct {
    gfx::Point drop_location;
    int display_index;
  } kTestCases[]{
      {point_in_display2, 1}, {point_in_display3, 2}, {gfx::Point(0, 0), 0}};

  OverviewController* overview_controller = OverviewController::Get();
  auto* event_generator = GetEventGenerator();
  for (const auto test_case : kTestCases) {
    SCOPED_TRACE("\nDrop location: " + test_case.drop_location.ToString() +
                 ";\n" + "Destination display index: " +
                 base::NumberToString(test_case.display_index) + ".");
    overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
    EXPECT_FALSE(divider_widget->IsVisible());

    auto* overview_group_item = GetOverviewItemForWindow(w1.get());
    DragGroupItemToPoint(overview_group_item, test_case.drop_location,
                         event_generator,
                         /*by_touch_gestures=*/false, /*drop=*/true);
    EXPECT_FALSE(divider_widget->IsVisible());

    display::Screen* screen = display::Screen::GetScreen();
    EXPECT_EQ(displays[test_case.display_index].id(),
              screen->GetDisplayNearestWindow(w1.get()).id());
    EXPECT_EQ(displays[test_case.display_index].id(),
              screen->GetDisplayNearestWindow(w2.get()).id());

    SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB);
    event_generator->PressKey(ui::VKEY_RETURN, /*flags=*/0);

    EXPECT_TRUE(divider_widget->IsVisible());
  }
}

// Verifies that when an `OverviewGroupItem` is dragged between displays in
// Overview mode, both the item's widget and the windows are mirrored properly.
// See http://b/335463631 for more details.
TEST_F(SnapGroupMultiDisplayTest,
       MirrorSnapGroupWhenMovingAcrossDisplaysInOverview) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());

  const gfx::Point point_in_display2(1000, 100);
  EXPECT_FALSE(displays[0].bounds().Contains(point_in_display2));
  EXPECT_TRUE(displays[1].bounds().Contains(point_in_display2));

  // Create Snap Group on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  // Move Snap Group to display #2.
  auto* event_generator = GetEventGenerator();
  OverviewGroupItem* group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w1.get()));
  DragGroupItemToPoint(group_item, point_in_display2, event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/false);

  // Verify that the item widget and window are mirrored for the individual
  // items.
  for (const auto& item : group_item->overview_items_for_testing()) {
    EXPECT_TRUE(item->item_mirror_for_dragging_for_testing());
    EXPECT_TRUE(item->window_mirror_for_dragging_for_testing());
  }

  event_generator->ReleaseLeftButton();

  // Verify that the windows are moved to the destination display properly.
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w2.get()).id());
}

// Tests that when moving snap group to another display with snap group, the
// windows will be moved to the destination display properly.
TEST_F(SnapGroupMultiDisplayTest,
       MoveSnapGroupToAnotherDisplayWithSnapGroupInOverview) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());

  const gfx::Point point_in_display1(100, 10);
  EXPECT_TRUE(displays[0].bounds().Contains(point_in_display1));
  EXPECT_FALSE(displays[1].bounds().Contains(point_in_display1));

  const gfx::Point point_in_display2(1000, 100);
  EXPECT_FALSE(displays[0].bounds().Contains(point_in_display2));
  EXPECT_TRUE(displays[1].bounds().Contains(point_in_display2));

  // Create Snap Group #1 on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  // Create Snap Group #2 on display #2.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(900, 0, 200, 100)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(1000, 50, 100, 200)));
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true);
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w3.get()),
      displays[1].id());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Move Snap Group #2 to display #1 and move Snap Group #1 to display #2.
  auto* event_generator = GetEventGenerator();
  DragGroupItemToPoint(GetOverviewItemForWindow(w3.get()), point_in_display1,
                       event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);
  DragGroupItemToPoint(GetOverviewItemForWindow(w1.get()), point_in_display2,
                       event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);

  // Verify that the windows are moved to the destination display properly.
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[0].id(), screen->GetDisplayNearestWindow(w3.get()).id());
  EXPECT_EQ(displays[0].id(), screen->GetDisplayNearestWindow(w4.get()).id());
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w2.get()).id());
}

// Tests that dragging an `OverviewGroupItem` to a different desk in another
// display will properly move the windows to the destination desk and display.
TEST_F(SnapGroupMultiDisplayTest,
       MoveSnapGroupToADifferentDeskInAnotherDisplayInOverview) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());
  auto* root2 = root_windows[1].get();

  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  // Create Snap Group on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(root2);
  ASSERT_TRUE(overview_grid);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& desks_mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(desks_mini_views.size(), 2u);

  // Drag `group_item` to the middle of the `desks_mini_views[1]` and drop.
  auto* event_generator = GetEventGenerator();
  const auto drop_point =
      desks_mini_views[1]->GetBoundsInScreen().CenterPoint();
  DragGroupItemToPoint(GetOverviewItemForWindow(w1.get()), drop_point,
                       event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);

  // Activate `desks_mini_views[1]` to switch to a different desk.
  event_generator->MoveMouseTo(drop_point);
  DeskSwitchAnimationWaiter waiter;
  event_generator->ClickLeftButton();
  waiter.Wait();
  ASSERT_FALSE(IsInOverviewSession());

  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w2.get()).id());
  EXPECT_TRUE(desks_util::IsActiveDeskContainer(w1->parent()));
  EXPECT_TRUE(desks_util::IsActiveDeskContainer(w2->parent()));
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[1].id());
}

// Tests that mirrored mode works correctly.
TEST_F(SnapGroupMultiDisplayTest, MirroredMode) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());
  const int64_t primary_id = displays[0].id();
  const int64_t secondary_id = displays[1].id();

  // Create Snap Group #1 on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_controller = SnapGroupController::Get();
  auto* group1 = snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  VerifySnapGroupOnDisplay(group1, primary_id);

  // Create Snap Group #2 on display #2.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(900, 0, 200, 100)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(1000, 50, 100, 200)));
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true);
  auto* group2 = snap_group_controller->GetSnapGroupForGivenWindow(w3.get());
  VerifySnapGroupOnDisplay(group2, secondary_id);

  // Enter mirrored mode.
  display_manager->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  ASSERT_EQ(1U, displays.size());
  VerifySnapGroupOnDisplay(group1, primary_id);
  VerifySnapGroupOnDisplay(group2, primary_id);

  // Exit mirrored mode.
  display_manager->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  ASSERT_EQ(2U, displays.size());

  // TODO(b/325331792): Verify the group is restored to its display. Currently
  // we just verify the group bounds are visible and on-screen.
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   group1->snap_group_divider());
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w4.get(),
                                   group2->snap_group_divider());
}

// Tests drag to snap across a landscape and portrait display.
TEST_F(SnapGroupMultiDisplayTest, LandscapeAndPortrait) {
  UpdateDisplay("800x600,600x800");
  // Set `w1` on the bottom half of the display since we need to drag a vertical
  // movement > kSnapTriggerVerticalMoveThreshold in order to snap to top. See
  // `WorkspaceWindowResizer::Drag()`.
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(0, 400, 200, 200)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(800, 0, 200, 200)));

  // Drag to snap `w1` to primary on display 2.
  wm::ActivateWindow(w1.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  const display::Display display2 =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay();
  ASSERT_FALSE(IsLayoutHorizontal(display2));
  const gfx::Rect work_area2 = display2.work_area();
  event_generator->DragMouseTo(work_area2.top_center());
  gfx::Rect top_half, bottom_half;
  work_area2.SplitHorizontally(top_half, bottom_half);
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(top_half, w1->GetBoundsInScreen());

  // Select `w2` to be auto-snapped.
  ClickOverviewItem(event_generator, w2.get());
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Expect the divider is horizontal.
  const gfx::Rect divider_bounds(
      work_area2.x(),
      work_area2.CenterPoint().y() - kSplitviewDividerShortSideLength / 2,
      work_area2.width(), kSplitviewDividerShortSideLength);
  EXPECT_EQ(divider_bounds, snap_group_divider_bounds_in_screen());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
}

// Tests that the snap group bounds are updated for removing and adding the
// primary display. Regression test for http://b/335313098.
TEST_F(SnapGroupMultiDisplayTest, AddRemovePrimaryDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  const int64_t primary_id = WindowTreeHostManager::GetPrimaryDisplayId();
  const int64_t secondary_id =
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay()
          .id();

  // Create Snap Group #1 on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_controller = SnapGroupController::Get();
  auto* group1 = snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  VerifySnapGroupOnDisplay(group1, primary_id);

  // Create Snap Group #2 on display #2.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(801, 0, 200, 100)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(810, 50, 100, 200)));
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true);
  auto* group2 = snap_group_controller->GetSnapGroupForGivenWindow(w3.get());
  VerifySnapGroupOnDisplay(group2, secondary_id);

  // Disconnect primary display.
  display::ManagedDisplayInfo primary_info =
      display_manager()->GetDisplayInfo(primary_id);
  display::ManagedDisplayInfo secondary_info =
      display_manager()->GetDisplayInfo(secondary_id);
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(secondary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  const auto& displays = display_manager()->active_display_list();
  ASSERT_EQ(1U, displays.size());
  ASSERT_EQ(WindowTreeHostManager::GetPrimaryDisplayId(), secondary_id);
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   group1->snap_group_divider());
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w4.get(),
                                   group2->snap_group_divider());

  // Reconnect primary display.
  display_info_list.push_back(primary_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  ASSERT_EQ(2U, displays.size());
  ASSERT_EQ(WindowTreeHostManager::GetPrimaryDisplayId(), primary_id);
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   group1->snap_group_divider());
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w4.get(),
                                   group2->snap_group_divider());
}

// Tests that resizing via the cursor between displays works correctly.
TEST_F(SnapGroupDividerTest, ResizeCursorBetweenDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  const int min_width = 300;
  std::unique_ptr<aura::Window> w1(
      CreateAppWindowWithMinSize(gfx::Size(min_width, 0)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindowWithMinSize(gfx::Size(min_width, 0)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());
  auto* snap_group_divider = snap_group->snap_group_divider();

  // Press and move the mouse right, within `w1` and `w2` min widths. Test we
  // update bounds.
  auto* event_generator = GetEventGenerator();
  const gfx::Point divider_point(
      snap_group_divider->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint());
  event_generator->set_current_screen_location(divider_point);
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(350, divider_point.y()), /*count=*/2);
  ASSERT_TRUE(snap_group_divider->is_resizing_with_divider());
  EXPECT_EQ(350, snap_group_divider_bounds_in_screen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

  // Move the mouse right, past `w2`'s min width and onto display #2. Test we
  // don't move beyond `w2`'s min width.
  event_generator->MoveMouseTo(gfx::Point(810, divider_point.y()), /*count=*/2);
  EXPECT_EQ(min_width, w2->GetBoundsInScreen().width());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

  // Move the mouse left, back onto display #1 but still beyond `w2`'s min
  // width. Test we don't move beyond `w2`'s min width.
  event_generator->MoveMouseTo(gfx::Point(799, divider_point.y()),
                               /*count=*/2);
  EXPECT_EQ(min_width, w2->GetBoundsInScreen().width());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

  // Move the mouse left, back within range. Test we update bounds now.
  event_generator->MoveMouseTo(gfx::Point(350, divider_point.y()), /*count=*/2);
  EXPECT_EQ(350, snap_group_divider_bounds_in_screen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
  event_generator->ReleaseLeftButton();
}

// Verify that an Overview group item remains interactive after being dragged to
// a different display and back without releasing the mouse. See
// http://b/339088510 for more details.
TEST_F(SnapGroupMultiDisplayTest, GroupItemCrossDisplayDragInteractivity) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2U, root_windows.size());

  const gfx::Point point_in_display1(100, 10);
  EXPECT_TRUE(displays[0].bounds().Contains(point_in_display1));
  EXPECT_FALSE(displays[1].bounds().Contains(point_in_display1));

  const gfx::Point point_in_display2(1000, 100);
  EXPECT_FALSE(displays[0].bounds().Contains(point_in_display2));
  EXPECT_TRUE(displays[1].bounds().Contains(point_in_display2));

  // Create Snap Group on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  auto* overview_group_item = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(overview_group_item);

  // Drag `overview_group_item` to display #2 w/o releasing mouse and drag back
  // then drop.
  auto* event_generator = GetEventGenerator();
  DragGroupItemToPoint(overview_group_item, point_in_display2, event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/false);
  DragGroupItemToPoint(overview_group_item, point_in_display1, event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);

  // Verify that Overview exits on mouse click, and both windows remaining on
  // the display they were originally on.
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(overview_group_item->target_bounds().CenterPoint()) +
      gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();
  EXPECT_FALSE(IsInOverviewSession());

  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[0].id(), screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(displays[0].id(), screen->GetDisplayNearestWindow(w2.get()).id());
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider());
}

// -----------------------------------------------------------------------------
// SnapGroupHistogramTest:

class SnapGroupHistogramTest : public SnapGroupTest {
 public:
  SnapGroupHistogramTest()
      : SnapGroupTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SnapGroupHistogramTest() override = default;

  void AdvanceClock(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
    task_environment()->RunUntilIdle();
  }

 protected:
  base::HistogramTester histogram_tester_;
};

// Tests that the pipeline to get snap action source info all the way to be
// stored in the `SplitViewOverviewSession` is working. This test focuses on the
// snap action source with top-usage in clamshell.
TEST_F(SnapGroupHistogramTest, SnapActionSourcePipeline) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(200, 100)));

  // Drag a window to snap and verify the snap action source info.
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window1.get(), gfx::PointF(), HTCAPTION, wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::PointF(0, 400), /*event_flags=*/0);
  resizer->CompleteDrag();
  resizer.reset();
  SplitViewOverviewSession* split_view_overview_session =
      VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(split_view_overview_session->snap_action_source_for_testing(),
            WindowSnapActionSource::kDragWindowToEdgeToSnap);
  MaximizeToClearTheSession(window1.get());

  // Mock snap from window layout menu and verify the snap action source info.
  chromeos::SnapController::Get()->CommitSnap(
      window1.get(), chromeos::SnapDirection::kSecondary,
      chromeos::kDefaultSnapRatio,
      chromeos::SnapController::SnapRequestSource::kWindowLayoutMenu);
  split_view_overview_session = VerifySplitViewOverviewSession(window1.get());
  EXPECT_TRUE(split_view_overview_session);
  EXPECT_EQ(split_view_overview_session->snap_action_source_for_testing(),
            WindowSnapActionSource::kSnapByWindowLayoutMenu);
  MaximizeToClearTheSession(window1.get());

  // Mock snap from window snap button and verify the snap action source info.
  chromeos::SnapController::Get()->CommitSnap(
      window1.get(), chromeos::SnapDirection::kPrimary,
      chromeos::kDefaultSnapRatio,
      chromeos::SnapController::SnapRequestSource::kSnapButton);
  split_view_overview_session = VerifySplitViewOverviewSession(window1.get());
  EXPECT_TRUE(split_view_overview_session);
  EXPECT_EQ(split_view_overview_session->snap_action_source_for_testing(),
            WindowSnapActionSource::kLongPressCaptionButtonToSnap);
  MaximizeToClearTheSession(window1.get());
}

// Verifies that the recorded duration of a snap group accurately reflects both
// its persistence and actual duration.
TEST_F(SnapGroupHistogramTest, SnapGroupDuration) {
  const std::string persistence_duration_histogram_name =
      BuildHistogramName(kSnapGroupPersistenceDurationRootWord);
  histogram_tester_.ExpectTotalCount(persistence_duration_histogram_name, 0);

  const std::string actual_duration_histogram_name =
      BuildHistogramName(kSnapGroupActualDurationRootWord);
  histogram_tester_.ExpectTotalCount(actual_duration_histogram_name, 0);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  SnapGroup* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  AdvanceClock(base::Seconds(10));

  // Snap `w3` to perform "snap to replace".
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // `persistence_duration_histogram_name` remains as 0 as the lifespan of the
  // Snap Group persists.
  histogram_tester_.ExpectTotalCount(persistence_duration_histogram_name, 0);

  // Whereas for `actual_duration_histogram_name`, it recorded the actual Snap
  // Group duration.
  histogram_tester_.ExpectBucketCount(actual_duration_histogram_name,
                                      /*sample=*/10, /*expected_count=*/1);

  AdvanceClock(base::Seconds(10));

  snap_group_controller->RemoveSnapGroup(
      snap_group_controller->GetSnapGroupForGivenWindow(w2.get()));
  histogram_tester_.ExpectBucketCount(persistence_duration_histogram_name,
                                      /*sample=*/20, /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(actual_duration_histogram_name,
                                      /*sample=*/10, /*expected_count=*/2);
}

// Verify `SnapGroupExitPoint` is correctly recorded across various Snap Group
// exit points (drag, window state change, destruction, tablet transition).
TEST_F(SnapGroupHistogramTest, SnapGroupExitPoint) {
  const std::string snap_group_exit_point =
      BuildHistogramName(kSnapGroupExitPointRootWord);
  histogram_tester_.ExpectTotalCount(snap_group_exit_point, 0);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  SCOPED_TRACE("Test case 1: drag window out to exit");
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().top_center());
  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTCAPTION);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w2.get()));
  histogram_tester_.ExpectBucketCount(snap_group_exit_point,
                                      SnapGroupExitPoint::kDragWindowOut, 1);
  MaximizeToClearTheSession(w2.get());

  SCOPED_TRACE("Test case 2: maximize window to exit");
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w2.get(), w3.get());
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w3.get()));
  WindowState* w2_state = WindowState::Get(w2.get());
  w2_state->Maximize();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w2.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w3.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowStateChange, 1);
  MaximizeToClearTheSession(w2.get());

  SCOPED_TRACE("Test case 3: minimize window to exit");
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapTwoTestWindows(w3.get(), w4.get());
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w4.get()));
  WindowState* w3_state = WindowState::Get(w3.get());
  w3_state->Minimize();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w3.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w4.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowStateChange, 2);
  MaximizeToClearTheSession(w4.get());

  SCOPED_TRACE("Test case 4: float window to exit");
  std::unique_ptr<aura::Window> w5(CreateAppWindow());
  SnapTwoTestWindows(w4.get(), w5.get());
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w4.get(), w5.get()));
  WindowState* w4_state = WindowState::Get(w4.get());
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  w4_state->OnWMEvent(&float_event);
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w4.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w5.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowStateChange, 3);
  MaximizeToClearTheSession(w5.get());

  SCOPED_TRACE("Test case 5: window destruction to exit");
  std::unique_ptr<aura::Window> w6(CreateAppWindow());
  SnapTwoTestWindows(w5.get(), w6.get());
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w5.get(), w6.get()));
  w5.reset();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w6.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowDestruction, 1);

  SCOPED_TRACE("Test case 6: switch to tablet mode to exit");
  std::unique_ptr<aura::Window> w7(CreateAppWindow());
  SnapTwoTestWindows(w6.get(), w7.get());
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w6.get(), w7.get()));
  SwitchToTabletMode();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w6.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w7.get()));
  histogram_tester_.ExpectBucketCount(snap_group_exit_point,
                                      SnapGroupExitPoint::kTabletTransition, 1);
}

TEST_F(SnapGroupHistogramTest, SnapGroupsCount) {
  UpdateDisplay("800x600");

  const std::string snap_groups_count_histogram =
      BuildHistogramName(kSnapGroupsCountRootWord);

  histogram_tester_.ExpectTotalCount(snap_groups_count_histogram, 0);

  // Create and test we record 1 group.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_EQ(1u, snap_group_controller->snap_groups_for_testing().size());
  histogram_tester_.ExpectBucketCount(snap_groups_count_histogram,
                                      /*sample=*/1,
                                      /*expected_count=*/1);

  // Create a maximized window to occlude the snapped windows so we can start
  // partial overview and create a 2nd snap group.
  std::unique_ptr<aura::Window> w0(CreateAppWindow(gfx::Rect(0, 0, 800, 600)));

  // Create and test we record 2 groups.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapTwoTestWindows(w3.get(), w4.get());
  ASSERT_EQ(2u, snap_group_controller->snap_groups_for_testing().size());
  histogram_tester_.ExpectBucketCount(snap_groups_count_histogram,
                                      /*sample=*/2,
                                      /*expected_count=*/1);

  // Close `w3` so we remove the 2nd snap group. Test we record 1 group.
  w3.reset();
  ASSERT_EQ(1u, snap_group_controller->snap_groups_for_testing().size());
  histogram_tester_.ExpectBucketCount(snap_groups_count_histogram,
                                      /*sample=*/1,
                                      /*expected_count=*/2);

  // Snap to replace `w5` in the 1st snap group. Test we don't record.
  std::unique_ptr<aura::Window> w5(CreateAppWindow());
  SnapOneTestWindow(w5.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_EQ(1u, snap_group_controller->snap_groups_for_testing().size());
  histogram_tester_.ExpectBucketCount(snap_groups_count_histogram,
                                      /*sample=*/1,
                                      /*expected_count=*/2);

  histogram_tester_.ExpectTotalCount(snap_groups_count_histogram, 3);
}

TEST_F(SnapGroupHistogramTest, SnapGroupUserActions) {
  UpdateDisplay("800x600");
  base::UserActionTester user_action_tester;

  // Add a snap group, which will incidentally start partial overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(
      user_action_tester.GetActionCount("SnapGroups_StartPartialOverview"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_AddSnapGroup"), 1);

  // Snap to replace.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w3.get()));
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_SnapToReplace"), 1);

  // Snap via the window layout menu with a ratio >
  // kSnapToReplaceRatioDiffThreshold to directly snap on top.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w4.get()));
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_SnapDirect"), 1);

  // Remove the snap group.
  w3.reset();
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w3.get()));
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_RemoveSnapGroup"), 1);
}

TEST_F(SnapGroupHistogramTest, RecallSnapGroupUserAction) {
  UpdateDisplay("800x600");
  base::UserActionTester user_action_tester;

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  ASSERT_TRUE(wm::IsActiveWindow(w2.get()));

  // Create a maximized window to occlude the snap group, then activate `w1` to
  // recall the group.
  std::unique_ptr<aura::Window> w3(CreateAppWindow(work_area_bounds()));
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_RecallSnapGroup"), 1);

  // Enter overview, then click the overview group item to recall the group.
  ToggleOverview();
  ASSERT_TRUE(
      wm::IsActiveWindow(GetOverviewSession()->GetOverviewFocusWindow()));
  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w1.get()));
  ASSERT_TRUE(overview_group_item);
  GetOverviewSession()->SelectWindow(overview_group_item);
  ASSERT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_RecallSnapGroup"), 2);

  // Activate `w3`, then window cycle to `w1` to recall the group.
  wm::ActivateWindow(w3.get());
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);
  CompleteWindowCycling();
  ASSERT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_RecallSnapGroup"), 3);

  // Window cycle to `w2`. Test we don't record since the other `w1` was
  // previously active.
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);
  CompleteWindowCycling();
  ASSERT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_EQ(user_action_tester.GetActionCount("SnapGroups_RecallSnapGroup"), 3);
}

TEST_F(SnapGroupHistogramTest, SkipPartialOverviewAndSnapGroup) {
  UpdateDisplay("800x600");
  base::UserActionTester user_action_tester;

  std::unique_ptr<aura::Window> w1(CreateAppWindow());

  // Snap using the keyboard shortcut won't record.
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_ALT_DOWN);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "SnapGroups_SkipPartialOverviewAndSnapGroup"),
            0);

  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Drag out `w1` from the group, which will break the group, then re-snap it.
  // This will skip both partial overview and snap group creation since `w2` is
  // an opposite snapped window.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(work_area_bounds().CenterPoint());
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  ASSERT_FALSE(IsInOverviewSession());
  EXPECT_EQ(user_action_tester.GetActionCount(
                "SnapGroups_SkipPartialOverviewAndSnapGroup"),
            1);
}

}  // namespace ash
