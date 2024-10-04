// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/snap_group/snap_group.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
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
#include "ash/session/session_controller_impl.h"
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
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/desks/templates/saved_desk_test_util.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_focus_cycler.h"
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
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/snap_group/snap_group_constants.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_metrics.h"
#include "ash/wm/snap_group/snap_group_observer.h"
#include "ash/wm/snap_group/snap_group_test_util.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_divider_view.h"
#include "ash/wm/splitview/split_view_overview_session.h"
#include "ash/wm/splitview/split_view_setup_view.h"
#include "ash/wm/splitview/split_view_test_util.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/test/test_non_client_frame_view_ash.h"
#include "ash/wm/toplevel_window_event_handler.h"
#include "ash/wm/window_cycle/window_cycle_controller.h"
#include "ash/wm/window_cycle/window_cycle_item_view.h"
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
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/workspace_event_handler.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ash/wm/workspace/workspace_window_resizer_test_api.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/check_op.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/test/test_widget_observer.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
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

void SwitchToTabletMode() {
  TabletModeControllerTestApi test_api;
  test_api.DetachAllMice();
  test_api.EnterTabletMode();
}

void ExitTabletMode() {
  TabletModeControllerTestApi().LeaveTabletMode();
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

// Activates the given 'window' hosted by an `OverviewGroupItem`.
void ActivateWindowInOverviewGroupItem(
    aura::Window* window,
    ui::test::EventGenerator* event_generator,
    bool by_touch_gestures) {
  ASSERT_TRUE(IsInOverviewSession());

  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(window));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  const std::unique_ptr<OverviewItem>& selected_item =
      overview_items[0]->GetWindow() == window ? overview_items[0]
                                               : overview_items[1];
  ASSERT_EQ(window, selected_item->GetWindow());

  event_generator->set_current_screen_location(
      gfx::ToRoundedPoint(selected_item->target_bounds().CenterPoint()));
  if (by_touch_gestures) {
    event_generator->PressTouch();
    event_generator->ReleaseTouch();
  } else {
    event_generator->ClickLeftButton();
  }
}

// Gets a point to drag `window` by the header.
gfx::Point GetDragPoint(aura::Window* window) {
  auto* frame = NonClientFrameViewAsh::Get(window);
  views::test::RunScheduledLayout(frame);
  return frame->GetHeaderView()->GetBoundsInScreen().CenterPoint();
}

// Verifies the windows and divider of `snap_group` are on `display_id`.
void VerifySnapGroupOnDisplay(SnapGroup* snap_group, const int64_t display_id) {
  aura::Window* root = snap_group->window1()->GetRootWindow();
  EXPECT_EQ(root, snap_group->window2()->GetRootWindow());
  EXPECT_EQ(root, snap_group->snap_group_divider()->GetRootWindow());
  EXPECT_EQ(display_id,
            display::Screen::GetScreen()->GetDisplayNearestWindow(root).id());
}

void ResizeDividerTo(ui::test::EventGenerator* event_generator,
                     gfx::Point resize_point) {
  const gfx::Point divider_center(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  event_generator->MoveMouseTo(divider_center);
  event_generator->PressLeftButton();
  // Resize with at least 2 steps to simulate the real CUJ of dragging the
  // mouse. The default test EventGenerator sends only the start and end points
  // which is an abrupt jump between points.
  event_generator->MoveMouseTo(resize_point, /*count=*/2);
  event_generator->ReleaseLeftButton();
}

void LongTapAt(ui::test::EventGenerator* event_generator,
               const gfx::Point& point) {
  ui::GestureEvent long_press(
      point.x(), point.y(), 0, base::TimeTicks::Now(),
      ui::GestureEventDetails(ui::EventType::kGestureLongPress));
  event_generator->Dispatch(&long_press);
}

// Simulates dragging `window` to `point`, verifying that it gets dragged.
void DragWindowTo(ui::test::EventGenerator* event_generator,
                  aura::Window* window,
                  gfx::Point point,
                  bool release) {
  wm::ActivateWindow(window);
  event_generator->MoveMouseTo(GetDragPoint(window));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(point);
  ASSERT_TRUE(WindowState::Get(window)->is_dragged());
  if (release) {
    event_generator->ReleaseLeftButton();
  }
}

// -----------------------------------------------------------------------------
// SnapGroupTestBase:

class SnapGroupTestBase : public OverviewTestBase {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit SnapGroupTestBase(TaskEnvironmentTraits&&... traits)
      : OverviewTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {}
  SnapGroupTestBase(const SnapGroupTestBase&) = delete;
  SnapGroupTestBase& operator=(const SnapGroupTestBase&) = delete;
  ~SnapGroupTestBase() override = default;

  std::unique_ptr<aura::Window> CreateAppWindowWithMinSize(gfx::Size min_size) {
    std::unique_ptr<aura::Window> window =
        CreateAppWindow(gfx::Rect(800, 600), chromeos::AppType::SYSTEM_APP,
                        kShellWindowId_Invalid, new TestWidgetDelegateAsh);
    auto* custom_frame = static_cast<TestNonClientFrameViewAsh*>(
        NonClientFrameViewAsh::Get(window.get()));
    custom_frame->SetMinimumSize(min_size);
    return window;
  }
};

}  // namespace

// -----------------------------------------------------------------------------
// FasterSplitScreenTest:

// Test fixture to verify faster split screen feature.

class FasterSplitScreenTest : public SnapGroupTestBase {
 public:
  FasterSplitScreenTest() = default;
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
};

// Tests the behavior in existing partial overview, i.e. overview -> drag to
// snap.
TEST_F(FasterSplitScreenTest, OldPartialOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  // Enter overview, then drag to snap. Test we start partial overview.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  // Note `DragGroupItemToPoint()` just drags the overview item, not necessarily
  // the group item.
  // TODO(b/344877340): Consider renaming this to `DragOverviewItemToPoint()`.
  auto* event_generator = GetEventGenerator();
  DragGroupItemToPoint(GetOverviewItemForWindow(w1.get()), gfx::Point(0, 0),
                       event_generator, /*by_touch_gestures=*/false,
                       /*drop=*/true);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  // Verify that split view overview session is active and the grid bounds are
  // updated, but the faster splitscreen setup UI is *not* shown.
  VerifySplitViewOverviewSession(w1.get());
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());
  // Select the other window in overview, test we end overview and split view.
  ClickOverviewItem(event_generator, w2.get());
  VerifyNotSplitViewOrOverviewSession(w1.get());
  MaximizeToClearTheSession(w1.get());

  // Enter overview, then drag `w1` to snap, then drag the other `w2` to snap.
  // Test we start and end overview and split view correctly.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  DragGroupItemToPoint(GetOverviewItemForWindow(w1.get()), gfx::Point(0, 0),
                       event_generator, /*by_touch_gestures=*/false,
                       /*drop=*/true);
  VerifySplitViewOverviewSession(w1.get());
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());
  DragGroupItemToPoint(GetOverviewItemForWindow(w2.get()),
                       GetWorkAreaBounds().top_right(), event_generator,
                       /*by_touch_gestures=*/false,
                       /*drop=*/true);
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  VerifyNotSplitViewOrOverviewSession(w1.get());
  MaximizeToClearTheSession(w1.get());
  // We need to maximize both windows so that overview isn't started upon
  // conversion to tablet mode.
  // TODO(b/327269057): Investigate tablet mode conversion.
  MaximizeToClearTheSession(w2.get());

  // Test that tablet mode works as normal.
  SwitchToTabletMode();
  EXPECT_FALSE(IsInOverviewSession());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());
}

// Tests that if the user disables the pref for snap window suggestions, we
// don't start faster splitview.
TEST_F(FasterSplitScreenTest, DisableSnapWindowSuggestionsPref) {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();

  pref->SetBoolean(prefs::kSnapWindowSuggestions, false);
  ASSERT_FALSE(pref->GetBoolean(prefs::kSnapWindowSuggestions));

  // Snap a window. Test we don't start overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifyNotSplitViewOrOverviewSession(w1.get());

  // Turn on the pref. Test we start overview.
  pref->SetBoolean(prefs::kSnapWindowSuggestions, true);
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(w1.get());
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
  EXPECT_FALSE(
      RootWindowController::ForWindow(w1.get())->split_view_overview_session());

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
  EXPECT_FALSE(overview_grid->split_view_setup_widget());
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
  const auto drag_point1 = gfx::Point(
      GetWorkAreaBounds().width() * chromeos::kOneThirdSnapRatio - 10,
      start_point.y());
  generator->DragMouseTo(drag_point1);
  gfx::Rect expected_window_bounds(initial_bounds);
  expected_window_bounds.set_width(drag_point1.x());
  EXPECT_EQ(expected_window_bounds, w1->GetBoundsInScreen());
  VerifySplitViewOverviewSession(w1.get());

  // Resize to greater than 2/3. Test we don't end overview.
  const auto drag_point2 = gfx::Point(
      GetWorkAreaBounds().width() * chromeos::kTwoThirdSnapRatio + 10,
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
  const gfx::Rect work_area(GetWorkAreaBounds());
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

  gfx::Rect expected_autosnap_bounds(GetWorkAreaBounds());
  expected_autosnap_bounds.Subtract(w1->GetBoundsInScreen());
  EXPECT_EQ(expected_autosnap_bounds,
            GetOverviewGridBounds(w1->GetRootWindow()));

  // Create a window and test that it auto snaps.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w3.get())->GetStateType());
  const int divider_delta = IsSnapGroupEnabledInClamshellMode()
                                ? kSplitviewDividerShortSideLength / 2
                                : 0;
  expected_autosnap_bounds.Subtract(
      gfx::Rect(expected_window_bounds.top_right(),
                gfx::Size(divider_delta, GetWorkAreaBounds().height())));
  EXPECT_EQ(expected_autosnap_bounds, w3->GetBoundsInScreen());
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
  VerifySplitViewOverviewSession(w1.get());
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

  auto* split_view_setup_view = overview_grid->GetSplitViewSetupView();
  ASSERT_TRUE(split_view_setup_view);
  LeftClickOn(split_view_setup_view->GetViewByID(
      SplitViewSetupView::kDismissButtonIDForTest));
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
  const auto work_area_bounds_1 = GetWorkAreaBounds();
  ASSERT_EQ(
      window1->GetBoundsInScreen(),
      gfx::Rect(0, 0, work_area_bounds_1.width() * chromeos::kTwoThirdSnapRatio,
                work_area_bounds_1.height()));

  UpdateDisplay("1200x600");
  VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(WindowState::Get(window1.get())->snap_ratio(),
            chromeos::kTwoThirdSnapRatio);
  const auto work_area_bounds_2 = GetWorkAreaBounds();
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
          .Contains(
              overview_grid->GetSplitViewSetupView()->GetBoundsInScreen()));

  // Hide the virtual keyboard. Test we refresh the grid and widget bounds.
  keyboard_controller->HideKeyboardByUser();
  VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            WindowState::Get(window1.get())->GetStateType());
  EXPECT_TRUE(
      GetOverviewGridBounds(root_window)
          .Contains(
              overview_grid->GetSplitViewSetupView()->GetBoundsInScreen()));

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
  EXPECT_TRUE(GetSplitViewDivider()->divider_widget());
  auto observed_windows = GetSplitViewDivider()->observed_windows();
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
  EXPECT_FALSE(GetSplitViewDivider()->divider_widget());

  SwitchToTabletMode();
  EXPECT_TRUE(GetSplitViewDivider()->divider_widget());
  auto observed_windows = GetSplitViewDivider()->observed_windows();
  EXPECT_EQ(2u, observed_windows.size());
  // TODO(b/312229933): Determine whether the order of `observed_windows_`
  // matters.
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), GetSplitViewDivider());

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
  std::unique_ptr<aura::Window> window2(
      CreateAppWindowWithMinSize(gfx::Size(400, 200)));

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

  const int divider_delta = IsSnapGroupEnabledInClamshellMode()
                                ? kSplitviewDividerShortSideLength
                                : 0;
  // Both windows will fit within the work are with no overlap
  if (auto* snap_group_controller = SnapGroupController::Get()) {
    EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window1.get(),
                                                             window2.get()));
    UnionBoundsEqualToWorkAreaBounds(window1.get(), window2.get(),
                                     GetTopmostSnapGroupDivider());
  } else {
    EXPECT_EQ(window1->GetBoundsInScreen().width() +
                  window2->GetBoundsInScreen().width() + divider_delta,
              GetWorkAreaBounds().width());
  }
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
  EXPECT_TRUE(GetSplitViewDivider()->divider_widget());

  // Double tap on the divider. This will start a drag and notify
  // SplitViewOverviewSession.
  const gfx::Point divider_center =
      GetSplitViewDivider()
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

  OverviewFocusCycler* focus_cycler = GetOverviewSession()->focus_cycler();

  // Tab until we get to the first overview item.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, GetEventGenerator());
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview_windows =
      GetOverviewItemsForRoot(0);
  EXPECT_EQ(overview_windows[0]->item_widget(),
            focus_cycler->GetOverviewFocusedView()->GetWidget());

  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();

  // Tab to the toast dismiss button.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                SplitViewSetupView::kDismissButtonIDForTest),
            focus_cycler->GetOverviewFocusedView());

  // Tab to the settings button.
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                SplitViewSetupView::kSettingsButtonIDForTest),
            focus_cycler->GetOverviewFocusedView());

  // Note we use `PressKeyAndModifierKeys()` to send modifier and key separately
  // to simulate real user input.

  // Shift + Tab reverse tabs to the dismiss button.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKeyAndModifierKeys(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                SplitViewSetupView::kDismissButtonIDForTest),
            focus_cycler->GetOverviewFocusedView());

  // Shift + Tab reverse tabs to the overview item.
  event_generator->PressKeyAndModifierKeys(ui::VKEY_TAB, ui::EF_SHIFT_DOWN);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_EQ(overview_windows[0]->item_widget(),
            focus_cycler->GetOverviewFocusedView()->GetWidget());
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
  auto* split_view_setup_widget = grid->split_view_setup_widget();
  ASSERT_TRUE(split_view_setup_widget);

  // Tab to the dismiss button.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, GetEventGenerator());
  PressAndReleaseKey(ui::VKEY_TAB);
  OverviewFocusCycler* focus_cycler = GetOverviewSession()->focus_cycler();
  EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                SplitViewSetupView::kDismissButtonIDForTest),
            focus_cycler->GetOverviewFocusedView());

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

  enum class TestCase { kDismissButton, kSettingsButton };
  for (auto test_case : {TestCase::kDismissButton}) {
    WindowState::Get(window1.get())->OnWMEvent(&snap_event);
    ASSERT_TRUE(OverviewController::Get()->InOverviewSession());

    // Search + Right moves to the first overview item.
    PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
    const std::vector<std::unique_ptr<OverviewItemBase>>& overview_windows =
        GetOverviewItemsForRoot(0);
    OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
    OverviewFocusCycler* focus_cycler = GetOverviewSession()->focus_cycler();
    EXPECT_EQ(overview_windows[0]->item_widget(),
              focus_cycler->GetOverviewFocusedView()->GetWidget());

    // Search + Right moves to the dismiss button.
    PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
    EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                  SplitViewSetupView::kDismissButtonIDForTest),
              focus_cycler->GetOverviewFocusedView());

    // Search + Right moves to the settings button.
    PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_COMMAND_DOWN);
    EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                  SplitViewSetupView::kSettingsButtonIDForTest),
              focus_cycler->GetOverviewFocusedView());

    switch (test_case) {
      case TestCase::kDismissButton: {
        // Search + Left moves back to the dismiss button.
        PressAndReleaseKey(ui::VKEY_LEFT, ui::EF_COMMAND_DOWN);
        EXPECT_EQ(grid->GetSplitViewSetupView()->GetViewByID(
                      SplitViewSetupView::kDismissButtonIDForTest),
                  focus_cycler->GetOverviewFocusedView());

        // Search + Space activates the dismiss button.
        PressAndReleaseKey(ui::VKEY_SPACE, ui::EF_COMMAND_DOWN);
        EXPECT_FALSE(IsInOverviewSession());
        break;
      }
      case TestCase::kSettingsButton: {
        // Search + Space activates the settings button.
        PressAndReleaseKey(ui::VKEY_SPACE, ui::EF_COMMAND_DOWN);
        EXPECT_FALSE(IsInOverviewSession());
        break;
      }
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
  auto* split_view_setup_widget = grid->split_view_setup_widget();
  ASSERT_TRUE(split_view_setup_widget);

  // Overview items are in MRU order, so the expected order in the grid list is
  // the reverse creation order.
  auto* item_widget1 = GetOverviewItemForWindow(window1.get())->item_widget();

  // Order should be [focus_widget, item_widget1, split_view_setup_widget].
  CheckA11yOverrides("focus", focus_widget,
                     /*expected_previous=*/split_view_setup_widget,
                     /*expected_next=*/item_widget1);
  CheckA11yOverrides("item1", item_widget1, /*expected_previous=*/focus_widget,
                     /*expected_next=*/split_view_setup_widget);
  CheckA11yOverrides("splitview", split_view_setup_widget,
                     /*expected_previous=*/item_widget1,
                     /*expected_next=*/focus_widget);
}

// Tests if only the `kResizeBehaviorKey` is set, snapping the window does not
// start partial overview.
TEST_F(FasterSplitScreenTest, SnapUnresizableWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  w1->SetProperty(aura::client::kResizeBehaviorKey,
                  aura::client::kResizeBehaviorNone);

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifyNotSplitViewOrOverviewSession(w1.get());
}

// Tests if both the `kResizeBehaviorKey` and `kUnresizableSnappedSizeKey` are
// set, snapping the window starts partial overview.
TEST_F(FasterSplitScreenTest, SnapUnresizableCanSnapWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  w1->SetProperty(aura::client::kResizeBehaviorKey,
                  aura::client::kResizeBehaviorNone);
  w1->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(300, 0));

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(w1.get());
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
  VerifySplitViewOverviewSession(w1.get());
  EXPECT_EQ(
      GetSplitViewOverviewSession(w1.get())->snap_action_source_for_testing(),
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
  VerifySplitViewOverviewSession(w1.get());
  EXPECT_EQ(
      GetSplitViewOverviewSession(w1.get())->snap_action_source_for_testing(),
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
          VerifySplitViewOverviewSession(w1.get());
          EXPECT_EQ(GetSplitViewOverviewSession(w1.get())
                        ->snap_action_source_for_testing(),
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
  gfx::Point drag_end_point = GetWorkAreaBounds().right_center();
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
class SnapGroupTest : public SnapGroupTestBase {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit SnapGroupTest(TaskEnvironmentTraits&&... traits)
      : SnapGroupTestBase(std::forward<TaskEnvironmentTraits>(traits)...) {
    scoped_feature_list_
        .InitWithFeatures(/*enabled_features=*/
                          {features::kSameAppWindowCycle,
                           chromeos::features::
                               kOverviewSessionInitOptimizations},
                          /*disabled_features=*/{});
  }
  SnapGroupTest(const SnapGroupTest&) = delete;
  SnapGroupTest& operator=(const SnapGroupTest&) = delete;
  ~SnapGroupTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFirstDisplayAsInternal);
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

  std::unique_ptr<aura::Window> CreateAlwaysOnTopWindow() {
    std::unique_ptr<aura::Window> always_on_top_window(CreateAppWindow());
    always_on_top_window->SetProperty(aura::client::kZOrderingKey,
                                      ui::ZOrderLevel::kFloatingWindow);
    EXPECT_EQ(kShellWindowId_AlwaysOnTopContainer,
              always_on_top_window->parent()->GetId());
    return always_on_top_window;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that if the user disables the pref for snap window suggestions, we
// still add snap group.
TEST_F(SnapGroupTest, DisableSnapWindowSuggestionsPref) {
  PrefService* pref =
      Shell::Get()->session_controller()->GetActivePrefService();

  pref->SetBoolean(prefs::kSnapWindowSuggestions, false);
  ASSERT_FALSE(pref->GetBoolean(prefs::kSnapWindowSuggestions));

  // Snap a window. Test we don't start overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifyNotSplitViewOrOverviewSession(w1.get());

  // Turn on the pref. Test we start overview.
  pref->SetBoolean(prefs::kSnapWindowSuggestions, true);
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(w1.get());

  // Select `w2` from overview. Test we form a group.
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

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

  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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

  ASSERT_TRUE(snap_group_controller->RemoveSnapGroup(
      snap_group, SnapGroupExitPoint::kDragWindowOut));
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
  w1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, /*id=*/-1, gfx::Rect(500, 0, 800, 600)));
  delegate2.set_minimum_size(window_minimum_size);
  w2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  WaitForOverviewEntered();
  VerifySplitViewOverviewSession(w1.get());
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  WaitForOverviewExitAnimation();

  EXPECT_TRUE(GetTopmostSnapGroupDivider());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
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
  w1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  delegate1.set_minimum_size(window_minimum_size);
  aura::test::TestWindowDelegate delegate2;
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &delegate2, /*id=*/-1, gfx::Rect(500, 0, 800, 600)));
  delegate2.set_minimum_size(window_minimum_size);
  w2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  WaitForOverviewEntered();
  VerifySplitViewOverviewSession(w1.get());
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  WaitForOverviewExitAnimation();

  EXPECT_TRUE(GetTopmostSnapGroupDivider());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Verify snap group will not be formed when attempting to include a window from
// the always-on-top container.
TEST_F(SnapGroupTest, DisallowFormSnapGroupWithAlwaysOnTopWindow) {
  std::unique_ptr<aura::Window> normal_window(CreateAppWindow());
  std::unique_ptr<aura::Window> always_on_top_window(CreateAlwaysOnTopWindow());

  SnapOneTestWindow(normal_window.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(normal_window.get());
  OverviewItemBase* always_on_top_overview_item =
      GetOverviewItemForWindow(always_on_top_window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::ToRoundedPoint(
      always_on_top_overview_item->target_bounds().CenterPoint()));
  event_generator->ClickLeftButton();

  // Verify that Snap Group will not be formed after activating
  // `always_on_top_window` in partial Overview.
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(snap_group_controller->AreWindowsInSnapGroup(
      normal_window.get(), always_on_top_window.get()));
  EXPECT_FALSE(
      snap_group_controller->GetSnapGroupForGivenWindow(normal_window.get()));
  EXPECT_FALSE(
      snap_group_controller->GetSnapGroupForGivenWindow(normal_window.get()));
}

// Verify that a window configured to be "visible on all workspaces" cannot be
// part of any Snap Group.
TEST_F(SnapGroupTest, DisallowVisibleOnAllWorkspacesWindowToFormGroup) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  auto* window_widget0 = views::Widget::GetWidgetForNativeView(w0.get());
  // Configure the property for `w0` to be visible on all workspaces.
  window_widget0->SetVisibleOnAllWorkspaces(true);
  std::unique_ptr<aura::Window> w1(CreateAppWindow());

  SnapOneTestWindow(w0.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w0.get());
  OverviewItemBase* overview_item1 = GetOverviewItemForWindow(w1.get());
  const gfx::Point overview_item1_center =
      gfx::ToRoundedPoint(overview_item1->target_bounds().CenterPoint());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(overview_item1_center);
  event_generator->ClickLeftButton();

  // Verify that Snap Group will not be formed after activating `w1` in partial
  // Overview.
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w0.get(), w1.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w0.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
}

// Tests that the shelf's corners are rounded by default. Upon Snap Group
// creation, the shelf's corners become sharp. When the Snap Group breaks, the
// shelf returns to its default state with rounded corners.
TEST_F(SnapGroupTest, ShelfRoundedCornersInFasterSplitScreenEntryPoint) {
  ShelfLayoutManager* shelf_layout_manager =
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
  ASSERT_EQ(ShelfBackgroundType::kDefaultBg,
            shelf_layout_manager->shelf_background_type());

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Test that Shelf will be updated to have sharp rounded corners.
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            shelf_layout_manager->shelf_background_type());

  // Drag `w1` out to break the group.
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().top_center());
  aura::test::TestWindowDelegate().set_window_component(HTCAPTION);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  event_generator->ReleaseLeftButton();
  ASSERT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  aura::test::TestWindowDelegate test_window_delegate;

  // Test dragging a snapped window out by mouse to exit the group.
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().top_center());
  test_window_delegate.set_window_component(HTCAPTION);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  EXPECT_FALSE(GetTopmostSnapGroupDivider());

  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  MaximizeToClearTheSession(w2.get());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Test dragging a snapped window out by touch to exit the group.
  event_generator->MoveTouch(w1->GetBoundsInScreen().top_center());
  test_window_delegate.set_window_component(HTCAPTION);
  event_generator->PressTouch();
  event_generator->MoveTouchBy(50, 200);
  EXPECT_TRUE(WindowState::Get(w1.get())->is_dragged());
  EXPECT_FALSE(GetTopmostSnapGroupDivider());

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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
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
  ClickOverviewItem(event_generator, w2.get());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests to verify that dragging a window out of a snap group breaks the group
// and removes the divider.
TEST_F(SnapGroupTest, DragWindowOutToBreakSnapGroup) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());

  // Create `w2` with `HTCAPTION`. This ensures that dragging behavior is
  // initiated from the caption region. `SnapGroup::OnLocatedEvent()` will
  // process of this event.
  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTCAPTION);
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &test_window_delegate, aura::client::WINDOW_TYPE_NORMAL,
      gfx::Rect(400, 5, 100, 50)));
  w2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);

  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());

  event_generator->set_current_screen_location(
      w2->GetBoundsInScreen().top_center());
  event_generator->DragMouseTo(GetWorkAreaBounds().CenterPoint());
  EXPECT_FALSE(GetTopmostSnapGroupDivider());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// This class simulates a crash scenario that can occur within the
// `ToplevelWindowEventHandler`. Specifically, when a window belonging to a Snap
// Group is dragged out to break the group, the `window_resizer_` can be reset,
// leading to a crash.
class ToplevelWindowEventHandlerCrashSimulator : public SnapGroupObserver {
 public:
  ToplevelWindowEventHandlerCrashSimulator() {
    SnapGroupController::Get()->AddObserver(this);
  }
  ToplevelWindowEventHandlerCrashSimulator(
      const ToplevelWindowEventHandlerCrashSimulator&) = delete;
  ToplevelWindowEventHandlerCrashSimulator& operator=(
      const ToplevelWindowEventHandlerCrashSimulator&) = delete;
  ~ToplevelWindowEventHandlerCrashSimulator() override {
    SnapGroupController::Get()->RemoveObserver(this);
  }

  // SnapGroupObserver:
  void OnSnapGroupRemoving(SnapGroup* snap_group,
                           SnapGroupExitPoint exit_pint) override {
    if (exit_pint != SnapGroupExitPoint::kDragWindowOut) {
      return;
    }

    // Explicitly reset the `window_resizier_` on window drag out from a Snap
    // Group to simulate the scenario that may possibly lead to crash, reported
    // in http://b/348673912.
    ToplevelWindowEventHandler* toplevel_window_event_handler =
        Shell::Get()->toplevel_window_event_handler();
    toplevel_window_event_handler->ResetWindowResizerForTesting();
  }
};

// Tests that dragging a window out of a Snap Group does not cause a crash,
// even if the window_resizer_ is reset during the process. Regression test for
// http://b/348673912.
TEST_F(SnapGroupTest, ToplevelWindowEventHandlerDragCrashFix) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());

  aura::test::TestWindowDelegate test_window_delegate;
  test_window_delegate.set_window_component(HTCAPTION);
  std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
      &test_window_delegate, aura::client::WINDOW_TYPE_NORMAL,
      gfx::Rect(400, 5, 100, 50)));
  w2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);

  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());

  ToplevelWindowEventHandlerCrashSimulator toplevel_window_drag_simulator;

  event_generator->set_current_screen_location(
      w2->GetBoundsInScreen().top_center());
  event_generator->DragMouseTo(GetWorkAreaBounds().CenterPoint());

  EXPECT_FALSE(GetTopmostSnapGroupDivider());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// This class simulates a crash scenario that can occur in
// `SplitViewOverviewSession::OnWindowBoundsChanged()`.
class OverviewCrashSimulator : public OverviewObserver {
 public:
  OverviewCrashSimulator() { OverviewController::Get()->AddObserver(this); }
  OverviewCrashSimulator(const OverviewCrashSimulator&) = delete;
  OverviewCrashSimulator& operator=(const OverviewCrashSimulator&) = delete;
  ~OverviewCrashSimulator() override {
    OverviewController::Get()->RemoveObserver(this);
  }

  // OverviewObserver:
  void OnOverviewModeEnding(OverviewSession* overview_session) override {
    auto* split_view_overview_session =
        GetSplitViewOverviewSession(Shell::GetPrimaryRootWindow());
    if (!split_view_overview_session) {
      return;
    }
    // The crash can occur in any scenario where ending overview would cause a
    // window bounds animation and notify
    // `SplitViewOverviewSession::OnWindowBoundsChanged()`. Send a bounds events
    // to `window` with animation.
    aura::Window* window = split_view_overview_session->window();
    const SetBoundsWMEvent event(gfx::Rect(100, 100), /*animate=*/true);
    WindowState::Get(window)->OnWMEvent(&event);
  }
};

// Tests no crash when ending overview causes a window animation. Regression
// test for http://b/352383998.
TEST_F(SnapGroupTest, NoCrashOnOverviewModeEnding) {
  OverviewCrashSimulator overview_crash_simulator;

  // Start partial overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // End overview.
  OverviewController::Get()->EndOverview(OverviewEndAction::kTests);
}

// Test that maximizing a snapped window breaks the snap group.
TEST_F(SnapGroupTest, MaximizeSnappedWindowExitPointTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  WindowState::Get(w2.get())->Maximize();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that the corresponding snap group will be removed when one of the
// windows in the snap group gets destroyed.
TEST_F(SnapGroupTest, WindowDestroyToBreakSnapGroup) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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

  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  EXPECT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
}

// Tests the snap ratio is updated correctly when resizing the windows in a snap
// group with the split view divider.
TEST_F(SnapGroupTest, SnapRatioTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  const gfx::Point hover_location =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  GetTopmostSnapGroupDivider()->StartResizeWithDivider(hover_location);
  const auto end_point =
      hover_location + gfx::Vector2d(-GetWorkAreaBounds().width() / 6, 0);
  GetTopmostSnapGroupDivider()->ResizeWithDivider(end_point);
  GetTopmostSnapGroupDivider()->EndResizeWithDivider(end_point);

  // Verify that split view remains inactive to avoid split view specific
  // behaviors such as auto-snap or showing cannot snap toast.
  EXPECT_FALSE(GetSplitViewController()->InSplitViewMode());
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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  for (const auto& shelf_alignment :
       {ShelfAlignment::kBottom, ShelfAlignment::kLeft}) {
    std::stringstream ss;
    ss << shelf_alignment;
    SCOPED_TRACE("Shelf alignment = " + ss.str());
    GetPrimaryShelf()->SetAlignment(shelf_alignment);

    auto* event_generator = GetEventGenerator();
    const gfx::Point divider_center(
        GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
    event_generator->MoveMouseTo(divider_center);
    event_generator->PressLeftButton();

    for (const int resize_delta : {-10, 6, -15}) {
      const gfx::Point resize_point(divider_center +
                                    gfx::Vector2d(resize_delta, 0));
      event_generator->MoveMouseTo(resize_point, /*count=*/2);
      EXPECT_EQ(resize_point,
                GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
      UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                       GetTopmostSnapGroupDivider());
    }
    event_generator->ReleaseLeftButton();
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());
  }
}

// Tests that the divider resizing respects the window's minimum size
// constraints.
TEST_F(SnapGroupTest, RespectWindowMinimumSizeWhileResizingWithDivider) {
  UpdateDisplay("1200x900");

  std::unique_ptr<aura::Window> window1(
      CreateAppWindowWithMinSize(gfx::Size(300, 600)));

  std::unique_ptr<aura::Window> window2(CreateAppWindow());
  SnapTwoTestWindows(window1.get(), window2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // The divider position updates while dragging, if it doesn't go below the
  // window's minimum size.
  GetTopmostSnapGroupDivider()->StartResizeWithDivider(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  GetTopmostSnapGroupDivider()->ResizeWithDivider(gfx::Point(400, 200));
  EXPECT_GT(GetTopmostSnapGroupDivider()->divider_position(), 300);
  GetTopmostSnapGroupDivider()->EndResizeWithDivider(gfx::Point(400, 200));
  EXPECT_GT(GetTopmostSnapGroupDivider()->divider_position(), 300);

  // Attempt to drag the divider below the window's minimum size. Verify it
  // stops at the minimum.
  GetTopmostSnapGroupDivider()->StartResizeWithDivider(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  GetTopmostSnapGroupDivider()->ResizeWithDivider(gfx::Point(200, 200));
  EXPECT_EQ(GetTopmostSnapGroupDivider()->divider_position(), 300);
  GetTopmostSnapGroupDivider()->EndResizeWithDivider(gfx::Point(200, 200));
  EXPECT_EQ(GetTopmostSnapGroupDivider()->divider_position(), 300);
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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  EXPECT_EQ(snap_groups.size(), 1u);
  EXPECT_EQ(window_to_snap_group_map.size(), 2u);

  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  wm::ActivateWindow(w2.get());
  EXPECT_TRUE(window_util::IsStackedBelow(w3.get(), w1.get()));

  w1.reset();
  EXPECT_FALSE(GetTopmostSnapGroupDivider());
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
  EXPECT_TRUE(GetSplitViewController()->primary_window());
  // Skip partial overview.
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  // Test we cleared the observed window.
  EXPECT_FALSE(GetSplitViewController()->primary_window());
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Drag to snap `w2` to the opposite side.
  wm::ActivateWindow(w2.get());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().right_center());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  // Test we cleared the observed window.
  EXPECT_FALSE(GetSplitViewController()->secondary_window());
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
    std::stringstream ss;
    ss << shelf_alignment;
    SCOPED_TRACE("Shelf alignment = " + ss.str());
    GetPrimaryShelf()->SetAlignment(shelf_alignment);
    // Create `w2` so it doesn't fit on the other side of `w1`.
    std::unique_ptr<aura::Window> w1(CreateAppWindow());
    const gfx::Rect work_area(GetWorkAreaBounds());
    const int min_width = work_area.width() * 0.4f;
    std::unique_ptr<aura::Window> w2(
        CreateAppWindowWithMinSize(gfx::Size(min_width, work_area.height())));

    // Snap `w1` to start partial overview.
    SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                      chromeos::kTwoThirdSnapRatio);
    EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

    // Select `w2` to be auto-snapped.
    ClickOverviewItem(GetEventGenerator(), w2.get());
    auto* snap_group_controller = SnapGroupController::Get();
    EXPECT_TRUE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

    // Expect `w2` is snapped at its minimum width and `w1` and the divider are
    // adjusted to fit.
    EXPECT_GE(min_width, w2->GetBoundsInScreen().width());
    EXPECT_NEAR(min_width, w2->GetBoundsInScreen().width(), /*abs_error=*/1);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());

    // Re-snap `w1` to 2/3. Test we keep the group and adjust the bounds.
    SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                      chromeos::kTwoThirdSnapRatio);
    EXPECT_TRUE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
    EXPECT_GE(min_width, w2->GetBoundsInScreen().width());
    EXPECT_NEAR(min_width, w2->GetBoundsInScreen().width(), /*abs_error=*/1);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());

    // Re-snap `w2` to 1/3. Test we keep the group and adjust the bounds.
    SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                      chromeos::kOneThirdSnapRatio);
    EXPECT_TRUE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
    EXPECT_GE(min_width, w2->GetBoundsInScreen().width());
    EXPECT_NEAR(min_width, w2->GetBoundsInScreen().width(), /*abs_error=*/1);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());
  }
}

// Tests that when both windows have minimum sizes, we don't create a snap
// group.
TEST_F(SnapGroupTest, AutoSnapBothWindowsWithMinimumSizes) {
  // Test with both the bottom and side shelf.
  for (const auto& shelf_alignment :
       {ShelfAlignment::kBottom, ShelfAlignment::kLeft}) {
    std::stringstream ss;
    ss << shelf_alignment;
    SCOPED_TRACE("Shelf alignment = " + ss.str());
    GetPrimaryShelf()->SetAlignment(shelf_alignment);
    // Create `w1` and `w2` both with minimum size 0.6f.
    const gfx::Rect work_area(GetWorkAreaBounds());
    const int min_width = work_area.width() * 0.6f;
    std::unique_ptr<aura::Window> w1(
        CreateAppWindowWithMinSize(gfx::Size(min_width, work_area.height())));
    std::unique_ptr<aura::Window> w2(
        CreateAppWindowWithMinSize(gfx::Size(min_width, work_area.height())));

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

// Test that dragging a window to snap in Overview to activate Partial Overview
// works properly with the existence of Snap Group. Regression test for
// http://b/340931820.
TEST_F(SnapGroupTest, DragToSnapInOverviewWithSnapGroup) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  std::unique_ptr<aura::Window> w3(CreateAppWindow(gfx::Rect()));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  // Drag `w3` to snap and verify the partial Overview Grid bounds.
  DragItemToPoint(GetOverviewItemForWindow(w3.get()), gfx::Point(0, 400),
                  GetEventGenerator(), /*by_touch_gestures=*/false,
                  /*drop=*/true);
  EXPECT_TRUE(IsInOverviewSession());
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w3.get())->GetStateType());
  gfx::Rect expected_overview_grid_bounds = GetWorkAreaBounds();
  expected_overview_grid_bounds.Subtract(w3->GetBoundsInScreen());
  EXPECT_EQ(expected_overview_grid_bounds,
            GetOverviewGridBounds(Shell::GetPrimaryRootWindow()));
}

// Tests the behavior in existing partial overview, i.e. overview -> drag to
// snap.
TEST_F(SnapGroupTest, OldPartialOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  // Enter overview, then drag to snap. Test we start partial overview.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  auto* event_generator = GetEventGenerator();
  DragGroupItemToPoint(GetOverviewItemForWindow(w1.get()), gfx::Point(0, 0),
                       event_generator, /*by_touch_gestures=*/false,
                       /*drop=*/true);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  VerifySplitViewOverviewSession(w1.get());
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());

  // Select the other window in overview, test we end overview and split view
  // but create a snap group.
  ClickOverviewItem(event_generator, w2.get());
  VerifyNotSplitViewOrOverviewSession(w1.get());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
  MaximizeToClearTheSession(w1.get());

  // Enter overview, then drag `w1` to snap, then drag the other `w2` to snap.
  // Test we start and end overview and split view correctly.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  DragGroupItemToPoint(GetOverviewItemForWindow(w1.get()), gfx::Point(0, 0),
                       event_generator, /*by_touch_gestures=*/false,
                       /*drop=*/true);
  VerifySplitViewOverviewSession(w1.get());
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());
  DragGroupItemToPoint(GetOverviewItemForWindow(w2.get()),
                       GetWorkAreaBounds().top_right(), event_generator,
                       /*by_touch_gestures=*/false,
                       /*drop=*/true);
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  VerifyNotSplitViewOrOverviewSession(w1.get());
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
  MaximizeToClearTheSession(w1.get());
  // We need to maximize both windows so that overview isn't started upon
  // conversion to tablet mode.
  // TODO(b/327269057): Investigate tablet mode conversion.
  MaximizeToClearTheSession(w2.get());

  // Test that tablet mode works as normal.
  SwitchToTabletMode();
  EXPECT_FALSE(IsInOverviewSession());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());
}

// Verifies that with a 3rd window, visually stacked below two snapped
// windows, but placed before the opposite snapped window within the Snap Group
// in MRU order, snapping a 4th window in this setup does not initiate partial
// overview. See http://b/339709601 for details.
TEST_F(SnapGroupTest, RecallSnapGroupWontStartPartialOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group_controller = SnapGroupController::Get();
  auto* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);

  // Open a 3rd window on top to occlude the snap group.
  std::unique_ptr<aura::Window> w3(CreateAppWindow(GetWorkAreaBounds()));

  // Recall the snap group.
  wm::ActivateWindow(w1.get());
  auto* desk_container = desks_util::GetActiveDeskContainerForRoot(
      Shell::Get()->GetPrimaryRootWindow());

  // Verify the stacking order from bottom to top.
  EXPECT_THAT(desk_container->children(),
              ElementsAre(w3.get(), w2.get(), w1.get(),
                          snap_group->snap_group_divider()
                              ->divider_widget()
                              ->GetNativeWindow()));

  // Open a 4th window and snap it on top. Test we don't start partial overview.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifyNotSplitViewOrOverviewSession(w4.get());

  // Test the window gets snapped to replace.
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w4.get()));
}

// Verify that 'Search + Shift + G' creates a Snap Group from two snapped
// windows.
TEST_F(SnapGroupTest, UseShortcutToGroupSnappedWindows) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);

  // Press 'Search + Shift + G' to to group `w1` and `w2`.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);

  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Press the shortcut again and the windows will still be grouped.
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());
}

// Tests the behavior for an unresizable window that cannot snap.
TEST_F(SnapGroupTest, UnresizableWindowWontFormSnapGroup) {
  std::unique_ptr<aura::Window> normal(CreateAppWindow());
  std::unique_ptr<aura::Window> unresizable(CreateAppWindow());
  unresizable->SetProperty(aura::client::kResizeBehaviorKey,
                           aura::client::kResizeBehaviorNone);

  // 1 - Snap the normal window to start partial overview.
  SnapOneTestWindow(normal.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(normal.get());

  // Selecting the unresizable window from partial overview won't snap and won't
  // add to snap group.
  ClickOverviewItem(GetEventGenerator(), unresizable.get());
  EXPECT_TRUE(ToastManager::Get()->IsToastShown(kAppCannotSnapToastId));
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(snap_group_controller->AreWindowsInSnapGroup(normal.get(),
                                                            unresizable.get()));

  // 2 - Snap the unresizable window won't start partial overview and won't
  // form a group even when the layout is complete.
  SnapOneTestWindow(unresizable.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifyNotSplitViewOrOverviewSession(unresizable.get());
  SnapOneTestWindow(normal.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  EXPECT_FALSE(snap_group_controller->AreWindowsInSnapGroup(normal.get(),
                                                            unresizable.get()));
}

// Tests the behavior for an unresizable window that can snap.
TEST_F(SnapGroupTest, UnresizableCanSnapWindowWontFormSnapGroup) {
  std::unique_ptr<aura::Window> normal(CreateAppWindow());
  std::unique_ptr<aura::Window> unresizable(CreateAppWindow());
  unresizable->SetProperty(aura::client::kResizeBehaviorKey,
                           aura::client::kResizeBehaviorNone);
  unresizable->SetProperty(kUnresizableSnappedSizeKey, new gfx::Size(300, 0));

  // 1 - Snap the normal window to start partial overview.
  SnapOneTestWindow(normal.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(normal.get());

  // Select the `kUnresizableSnappedSizeKey` window from partial overview. Test
  // it snaps but does not form a group.
  auto* event_generator = GetEventGenerator();
  ClickOverviewItem(event_generator, unresizable.get());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(unresizable.get())->GetStateType());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(snap_group_controller->AreWindowsInSnapGroup(normal.get(),
                                                            unresizable.get()));

  // 2 - Snap the `kUnresizableSnappedSizeKey` window to start partial overview.
  SnapOneTestWindow(unresizable.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  VerifySplitViewOverviewSession(unresizable.get());

  // Select the normal window from partial overview. Test it snaps but does not
  // form a group.
  ClickOverviewItem(event_generator, normal.get());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(normal.get())->GetStateType());
  EXPECT_FALSE(snap_group_controller->AreWindowsInSnapGroup(normal.get(),
                                                            unresizable.get()));
}

// Tests that re-snapping to the opposite side with a different snap ratio
// updates the bounds correctly. Regression test for http://b/349951979.
TEST_F(SnapGroupTest, ReSnapToOppositeSnapRatio) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // Re-snap `w1` to secondary 1/3.
  SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_EQ(
      std::round(GetWorkAreaBounds().width() * chromeos::kOneThirdSnapRatio),
      w1->GetBoundsInScreen().width());

  // Re-snap `w1` to primary 2/3.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_EQ(
      std::round(GetWorkAreaBounds().width() * chromeos::kTwoThirdSnapRatio),
      w1->GetBoundsInScreen().width());

  // Re-snap `w1` to secondary 1/2.
  SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_EQ(
      std::round(GetWorkAreaBounds().width() * chromeos::kDefaultSnapRatio),
      w1->GetBoundsInScreen().width());
}

// Tests no dump without crash when one of the windows is minimized. Regression
// test for http://b/352159258.
TEST_F(SnapGroupTest, NoDumpWithoutCrashOnMinimize) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* snap_group_controller = SnapGroupController::Get();
  // Test with both primary and secondary display orientation.
  for (const bool is_layout_primary : {true, false}) {
    if (is_layout_primary) {
      UpdateDisplay("800x600");
    } else {
      UpdateDisplay("800x600/u");
    }
    ASSERT_EQ(is_layout_primary, IsLayoutPrimary(w1.get()));
    SnapOneTestWindow(w1.get(),
                      is_layout_primary ? WindowStateType::kPrimarySnapped
                                        : WindowStateType::kSecondarySnapped,
                      chromeos::kDefaultSnapRatio);
    SnapOneTestWindow(w2.get(),
                      is_layout_primary ? WindowStateType::kSecondarySnapped
                                        : WindowStateType::kPrimarySnapped,
                      chromeos::kDefaultSnapRatio);
    ASSERT_TRUE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
    const gfx::Rect w1_bounds(w1->GetBoundsInScreen());
    const gfx::Rect w2_bounds(w2->GetBoundsInScreen());

    // Minimize `w1`.
    auto* window_state1 = WindowState::Get(w1.get());
    window_state1->Minimize();
    ASSERT_FALSE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

    // Verify `w2` is back at 1/2.
    const gfx::Rect work_area(GetWorkAreaBounds());
    gfx::Rect left_half, right_half;
    work_area.SplitVertically(left_half, right_half);
    // Verify `w2` is at the same position, aka approximately the same
    // bounds it was at before.
    EXPECT_TRUE(w2_bounds.ApproximatelyEqual(
        w2->GetBoundsInScreen(),
        /*tolerance=*/kSplitviewDividerShortSideLength / 2));
    EXPECT_EQ(right_half, w2->GetBoundsInScreen());
    auto* window_state2 = WindowState::Get(w2.get());
    EXPECT_EQ(chromeos::kDefaultSnapRatio, window_state2->snap_ratio());

    // Unminimize `w1`. Test the windows are still at 1/2 with no divider.
    window_state1->Unminimize();
    ASSERT_FALSE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
    // Verify `w1` is at the same position, aka approximately the same
    // bounds it was at before.
    EXPECT_TRUE(w1_bounds.ApproximatelyEqual(
        w1->GetBoundsInScreen(),
        /*tolerance=*/kSplitviewDividerShortSideLength / 2));
    EXPECT_EQ(left_half, w1->GetBoundsInScreen());
    EXPECT_EQ(chromeos::kDefaultSnapRatio, window_state1->snap_ratio());
    EXPECT_TRUE(w2_bounds.ApproximatelyEqual(
        w2->GetBoundsInScreen(),
        /*tolerance=*/kSplitviewDividerShortSideLength / 2));
    EXPECT_EQ(right_half, w2->GetBoundsInScreen());
    EXPECT_EQ(chromeos::kDefaultSnapRatio, window_state2->snap_ratio());
    MaximizeToClearTheSession(w1.get());
    MaximizeToClearTheSession(w2.get());
  }
}

// Verifies no crashes occur when re-snapping a secondary window (with transient
// child) to the primary position within a Snap Group, triggering a partial
// Overview and subsequent selection of the previous primary window. And the
// transient child window remains visible in partial Overview. Regression test
// for http://b/353574797.
TEST_F(SnapGroupTest, NoCrashWhenReSnappingSecondaryToPrimaryWithTransient) {
  std::unique_ptr<aura::Window> w0(CreateAppWindow(gfx::Rect(0, 0, 300, 300)));
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(500, 0, 300, 300)));
  // Create a bubble widget that's anchored to `w1`.
  auto bubble_delegate1 = std::make_unique<views::BubbleDialogDelegateView>(
      NonClientFrameViewAsh::Get(w1.get()), views::BubbleBorder::TOP_RIGHT);
  // The line below is essential to make sure that the bubble doesn't get closed
  // when entering overview.
  bubble_delegate1->set_close_on_deactivate(false);
  bubble_delegate1->set_parent_window(w1.get());
  views::Widget* bubble_widget1(views::BubbleDialogDelegateView::CreateBubble(
      std::move(bubble_delegate1)));
  bubble_widget1->Show();
  aura::Window* bubble_window1 = bubble_widget1->GetNativeWindow();
  ASSERT_TRUE(bubble_window1->IsVisible());
  ASSERT_TRUE(window_util::AsBubbleDialogDelegate(bubble_window1));

  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
  EXPECT_TRUE(bubble_window1->IsVisible());
  EXPECT_FALSE(bubble_window1->GetProperty(chromeos::kIsShowingInOverviewKey));
  EXPECT_FALSE(bubble_window1->GetProperty(kHideInOverviewKey));

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // Induce visibility changes on `bubble_widget1`.
  bubble_widget1->Hide();
  bubble_widget1->Show();
  EXPECT_TRUE(bubble_window1->IsVisible());

  // Verify that there is no crash when selecting OverviewItem for `w0` in
  // partial Overview.
  event_generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(w0.get())->target_bounds().CenterPoint()));
  event_generator->ClickLeftButton();
  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w0.get()));
}

// -----------------------------------------------------------------------------
// SnapGroupPhantomBoundsTest:

using SnapGroupPhantomBoundsTest = SnapGroupTest;

// Tests that the snap phantom bounds are updated when a window is dragged over
// a snap group.
TEST_F(SnapGroupPhantomBoundsTest, SnapGroupPhantomBounds) {
  UpdateDisplay("800x600");

  // Create a snap group.
  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  std::unique_ptr<aura::Window> w2 = CreateAppWindow();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  auto* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  // Resize the snap group to a ratio < kSnapToReplaceRatioDiffThreshold from
  // the default ratio.
  SplitViewDivider* snap_group_divider = snap_group->snap_group_divider();
  const gfx::Point resize_point(
      snap_group_divider->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint());
  snap_group_divider->StartResizeWithDivider(resize_point);
  snap_group_divider->ResizeWithDivider(resize_point - gfx::Vector2d(50, 0));
  snap_group_divider->EndResizeWithDivider(resize_point - gfx::Vector2d(50, 0));
  ASSERT_LE(WindowState::Get(w1.get())->snap_ratio().value() -
                chromeos::kDefaultSnapRatio,
            kSnapToReplaceRatioDiffThreshold);

  // Drag to snap `w3` over `w1`. Test we update the phantom bounds.
  std::unique_ptr<aura::Window> w3 = CreateAppWindow();
  auto* event_generator = GetEventGenerator();
  DragWindowTo(event_generator, w3.get(), gfx::Point(0, 100),
               /*release=*/false);
  // The phantom bounds will not account for the divider width.
  EXPECT_TRUE(w1->GetBoundsInScreen().ApproximatelyEqual(
      WorkspaceWindowResizerTestApi()
          .GetSnapPhantomWindowController()
          ->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Drag to snap `w3` over `w2`. Test we update the phantom bounds.
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(w3.get());
  DragWindowTo(event_generator, w3.get(),
               gfx::Point(work_area.right(), work_area.y() + 100),
               /*release=*/false);
  EXPECT_TRUE(w2->GetBoundsInScreen().ApproximatelyEqual(
      WorkspaceWindowResizerTestApi()
          .GetSnapPhantomWindowController()
          ->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Create a new window on top of the snap group that fully occludes the snap
  // group. See http://b/347768613 for why phantom bounds for a snap group when
  // there is a partially occluding windows isn't defined.
  std::unique_ptr<aura::Window> w4 = CreateAppWindow(work_area);

  // Now drag to snap `w3`. Since the snap group is not fully visible, the
  // phantom bounds are back to default.
  DragWindowTo(event_generator, w3.get(), gfx::Point(0, 100),
               /*release=*/false);
  EXPECT_EQ(gfx::Rect(0, 0, work_area.width() / 2, work_area.height()),
            WorkspaceWindowResizerTestApi()
                .GetSnapPhantomWindowController()
                ->GetTargetWindowBounds());
}

// Tests that the snap phantom bounds reflect the opposite snapped window.
TEST_F(SnapGroupPhantomBoundsTest, ReflectOppositeSnappedWindow) {
  UpdateDisplay("800x600");

  // Create an app window so it can be recognized by
  // `GetOppositeVisibleSnappedWindow()`, then snap `w1` and resize `w1` to an
  // arbitrary size.
  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  auto* event_generator = GetEventGenerator();
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(w1.get());
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().right_center());
  event_generator->DragMouseTo(250, work_area.CenterPoint().y());
  ASSERT_EQ(250, w1->GetBoundsInScreen().width());

  // Now drag to snap `w2` to the opposite side of `w1`. Test we update the
  // phantom bounds to reflect `w1`.
  std::unique_ptr<aura::Window> w2 = CreateAppWindow();
  gfx::Rect expected_bounds(work_area);
  expected_bounds.Subtract(w1->GetBoundsInScreen());
  DragWindowTo(event_generator, w2.get(), work_area.right_center(),
               /*release=*/false);
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      WorkspaceWindowResizerTestApi()
          .GetSnapPhantomWindowController()
          ->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Test that the window snaps at the phantom bounds.
  event_generator->ReleaseLeftButton();
  ASSERT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w2.get(), w1.get()));
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      w2->GetBoundsInScreen(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));
}

// Tests that the snap phantom bounds work on multi-displays.
TEST_F(SnapGroupPhantomBoundsTest, SnapPhantomBoundsMultiDisplay) {
  UpdateDisplay("800x600,1200x900");

  // Create an app window so it can be recognized by
  // `GetOppositeVisibleSnappedWindow()`, then snap `w1` to 1/3 on display 2.
  std::unique_ptr<aura::Window> w1 =
      CreateAppWindow(gfx::Rect(1200, 0, 400, 400));
  SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio);
  const display::Display display2 = display_manager()->active_display_list()[1];
  ASSERT_EQ(display2,
            display::Screen::GetScreen()->GetDisplayNearestWindow(w1.get()));

  // Drag to snap `w2` to display 2.
  std::unique_ptr<aura::Window> w2 = CreateAppWindow();
  const gfx::Rect work_area2 = display2.work_area();
  auto* event_generator = GetEventGenerator();
  DragWindowTo(event_generator, w2.get(), work_area2.left_center(),
               /*release=*/false);

  // Test the phantom bounds are updated on display 2.
  gfx::Rect expected_bounds(work_area2);
  expected_bounds.Subtract(w1->GetBoundsInScreen());
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      WorkspaceWindowResizerTestApi()
          .GetSnapPhantomWindowController()
          ->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Test that the window snaps at the phantom bounds.
  event_generator->ReleaseLeftButton();
  ASSERT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w2.get(), w1.get()));
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      w2->GetBoundsInScreen(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));
}

// Tests that the phantom bounds work correctly in portrait mode.
TEST_F(SnapGroupPhantomBoundsTest, SnapPhantomBoundsPortraitMode) {
  UpdateDisplay("600x900");

  // Create an app window so it can be recognized by
  // `GetOppositeVisibleSnappedWindow()`, then snap to 2/3 top.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(w1.get());
  ASSERT_EQ(gfx::Rect(0, 0, work_area.width(),
                      work_area.height() * chromeos::kTwoThirdSnapRatio),
            w1->GetBoundsInScreen());

  // Drag to snap `w2` to the bottom.
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  DragWindowTo(event_generator, w2.get(), work_area.bottom_center(),
               /*release=*/false);

  // Test the phantom bounds are updated to reflect `w1`.
  gfx::Rect expected_bounds(work_area);
  expected_bounds.Subtract(w1->GetBoundsInScreen());
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      WorkspaceWindowResizerTestApi()
          .GetSnapPhantomWindowController()
          ->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Test that the window snaps at the phantom bounds.
  event_generator->ReleaseLeftButton();
  ASSERT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w2.get(), w1.get()));
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      w2->GetBoundsInScreen(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));
}

// Tests that when the snap phantom bounds window has a minimum size, the
// minimum size is respected.
TEST_F(SnapGroupPhantomBoundsTest, SnapPhantomBoundsMinimumSize) {
  UpdateDisplay("800x600");

  // Create an app window so it can be recognized by
  // `GetOppositeVisibleSnappedWindow()`, then snap to 2/3 left.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(w1.get());
  ASSERT_EQ(gfx::Rect(0, 0, work_area.width() * chromeos::kTwoThirdSnapRatio,
                      work_area.height()),
            w1->GetBoundsInScreen());

  // Set `w2` minimum size to be > 1/3, then drag to snap `window_`.
  const gfx::Size min_size(work_area.width() * 0.4f, work_area.height());
  std::unique_ptr<aura::Window> w2(CreateAppWindowWithMinSize(min_size));
  auto* event_generator = GetEventGenerator();
  DragWindowTo(event_generator, w2.get(), work_area.right_center(),
               /*release=*/false);

  // Test the phantom bounds are at `w2` minimum size.
  gfx::Rect expected_bounds(work_area);
  expected_bounds.set_x(work_area.right() - min_size.width());
  expected_bounds.set_width(min_size.width());
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      WorkspaceWindowResizerTestApi()
          .GetSnapPhantomWindowController()
          ->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Test that the window snaps at the phantom bounds.
  event_generator->ReleaseLeftButton();
  ASSERT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w2.get(), w1.get()));
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      w2->GetBoundsInScreen(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));
}

// Tests that when the snap ratio gap exceeds the threshold, we do not update
// the phantom bounds.
TEST_F(SnapGroupPhantomBoundsTest, SnapRatioGapThreshold) {
  UpdateDisplay("800x600");

  // Snap and resize `w1` so that the snap ratio gap between `w1` and the
  // default snap ratio exceeds the threshold.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  const WindowSnapWMEvent snap_primary(
      WM_EVENT_SNAP_PRIMARY, chromeos::kDefaultSnapRatio,
      WindowSnapActionSource::kDragWindowToEdgeToSnap);
  WindowState::Get(w1.get())->OnWMEvent(&snap_primary);
  auto* event_generator = GetEventGenerator();
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(w1.get());
  event_generator->MoveMouseTo(w1->GetBoundsInScreen().right_center());
  event_generator->DragMouseTo(105, work_area.CenterPoint().y());
  const gfx::Rect w1_bounds(w1->GetBoundsInScreen());
  ASSERT_EQ(105, w1_bounds.width());
  ASSERT_GT(std::abs(1.f - *WindowState::Get(w1.get())->snap_ratio() -
                     chromeos::kDefaultSnapRatio),
            kSnapToReplaceRatioDiffThreshold);

  // Drag to snap `w2` on the opposite side. Since we won't auto group, we
  // also don't update the phantom bounds.
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  DragWindowTo(event_generator, w2.get(), work_area.right_center(),
               /*release=*/false);
  gfx::Rect expected_bounds(work_area);
  expected_bounds.set_width(work_area.width() / 2);
  expected_bounds.set_x(work_area.width() - expected_bounds.width());
  EXPECT_EQ(expected_bounds, WorkspaceWindowResizerTestApi()
                                 .GetSnapPhantomWindowController()
                                 ->GetTargetWindowBounds());

  // Test the window snaps at the default bounds without forming a group.
  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w2.get(), w1.get()));
  EXPECT_EQ(expected_bounds, w2->GetBoundsInScreen());
  EXPECT_EQ(w1_bounds, w1->GetBoundsInScreen());
}

// Tests that the snap phantom bounds are updated correctly after snap to
// replace. Regression test for http://b/349892846.
TEST_F(SnapGroupPhantomBoundsTest, SnapPhantomBoundsAfterSnapToReplace) {
  // Create app windows with big enough bounds, since if the bounds are too
  // small the drag point might conflict with the size button and not start a
  // drag on the caption bar.
  const gfx::Rect work_area(GetWorkAreaBounds());
  std::unique_ptr<aura::Window> w1(CreateAppWindow(work_area));
  std::unique_ptr<aura::Window> w2(CreateAppWindow(work_area));
  std::unique_ptr<aura::Window> w3(CreateAppWindow(work_area));

  // Create a snap group with non-default snap ratio.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  ClickOverviewItem(GetEventGenerator(), w2.get());
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  ASSERT_NEAR(chromeos::kTwoThirdSnapRatio,
              *WindowState::Get(w1.get())->snap_ratio(), 0.01);
  ASSERT_NEAR(chromeos::kOneThirdSnapRatio,
              *WindowState::Get(w2.get())->snap_ratio(), 0.01);

  // Create a 3rd window and drag to snap over `w2` to show the phantom bounds.
  auto* event_generator = GetEventGenerator();
  wm::ActivateWindow(w3.get());
  event_generator->MoveMouseTo(GetDragPoint(w3.get()));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(work_area.right_center());
  ASSERT_TRUE(WindowState::Get(w3.get())->is_dragged());
  auto* snap_phantom_window_controller =
      WorkspaceWindowResizerTestApi().GetSnapPhantomWindowController();
  ASSERT_TRUE(snap_phantom_window_controller);
  EXPECT_TRUE(w2->GetBoundsInScreen().ApproximatelyEqual(
      snap_phantom_window_controller->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Release the drag to snap to replace `w2` in the group.
  event_generator->ReleaseLeftButton();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w3.get()));
  EXPECT_TRUE(w3->GetBoundsInScreen().ApproximatelyEqual(
      w2->GetBoundsInScreen(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio,
              *WindowState::Get(w3.get())->snap_ratio(), 0.01);

  // Drag to break `w3` from the group.
  event_generator->DragMouseTo(work_area.CenterPoint());
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w3.get()));

  // Activate `w2` to stack it on top of `w3` so it can auto-group with `w1` to
  // simulate the bug.
  wm::ActivateWindow(w2.get());
  ASSERT_TRUE(window_util::IsStackedBelow(w3.get(), w2.get()));

  // Drag to snap `w1` to the opposite side of `w2`.
  wm::ActivateWindow(w1.get());
  event_generator->MoveMouseTo(GetDragPoint(w1.get()));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(work_area.origin());
  ASSERT_TRUE(WindowState::Get(w1.get())->is_dragged());
  snap_phantom_window_controller =
      WorkspaceWindowResizerTestApi().GetSnapPhantomWindowController();
  ASSERT_TRUE(snap_phantom_window_controller);
  gfx::Rect expected_bounds(work_area);
  expected_bounds.Subtract(w2->GetBoundsInScreen());
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      snap_phantom_window_controller->GetTargetWindowBounds(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));

  // Release the drag. Test `w1` snaps at `expected_bounds` and auto-groups with
  // `w2`.
  event_generator->ReleaseLeftButton();
  EXPECT_TRUE(expected_bounds.ApproximatelyEqual(
      w1->GetBoundsInScreen(),
      /*tolerance=*/kSplitviewDividerShortSideLength / 2));
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio,
              *WindowState::Get(w1.get())->snap_ratio(), 0.01);
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio,
              *WindowState::Get(w2.get())->snap_ratio(), 0.01);
}

// -----------------------------------------------------------------------------
// SnapGroupFloatTest:

using SnapGroupFloatTest = SnapGroupTest;

// Tests that we can create a Snap Group with a floated window.
TEST_F(SnapGroupFloatTest, SnapGroupCreationWithFloatedWindow) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> normal_window(CreateAppWindow());
  std::unique_ptr<aura::Window> floated_window(CreateAppWindow());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  SnapOneTestWindow(normal_window.get(),
                    /*state_type=*/chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  WaitForOverviewEntered();
  VerifySplitViewOverviewSession(normal_window.get());
  OverviewItemBase* floated_window_overview_item =
      GetOverviewItemForWindow(floated_window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::ToRoundedPoint(
      floated_window_overview_item->target_bounds().CenterPoint()));
  event_generator->ClickLeftButton();
  EXPECT_TRUE(floated_window->layer()->GetAnimator()->is_animating());
  EXPECT_NE(floated_window->layer()->transform(),
            floated_window->layer()->GetTargetTransform());
  WaitForOverviewExitAnimation();

  // Verify that Snap Group will be formed after activating `floated_window` in
  // partial Overview.
  EXPECT_FALSE(GetSplitViewController()->InSplitViewMode());
  EXPECT_FALSE(WindowState::Get(floated_window.get())->IsFloated());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(
      normal_window.get(), floated_window.get()));
  UnionBoundsEqualToWorkAreaBounds(normal_window.get(), floated_window.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests that creating a snap group, then floating a window in the group, then
// re-snapping snaps to the correct bounds. See http://b/349177630 for context.
TEST_F(SnapGroupFloatTest, ReSnapFloatedWindow) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  // 1 - Snap `w1` to 2/3 and `w2` to 1/3.
  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  WaitForOverviewEntered();
  auto* event_generator = GetEventGenerator();
  ClickOverviewItem(event_generator, w2.get());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  WaitForOverviewExitAnimation();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Float `w2`.
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(w2.get())->IsFloated());
  ASSERT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Drag to snap `w2` to secondary. Test it snaps at 1/3 from the right.
  event_generator->MoveMouseTo(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().right_center());
  EXPECT_TRUE(w2->layer()->GetAnimator()->is_animating());
  EXPECT_NE(w2->layer()->transform(), w2->layer()->GetTargetTransform());
  ASSERT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_EQ(
      std::round(GetWorkAreaBounds().width() * chromeos::kTwoThirdSnapRatio),
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio,
              *WindowState::Get(w1.get())->snap_ratio(), 0.01);
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio,
              *WindowState::Get(w2.get())->snap_ratio(), 0.01);
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Float `w1`.
  wm::ActivateWindow(w1.get());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(w1.get())->IsFloated());
  ASSERT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Drag to snap `w1` to primary. Test it snaps at 2/3 from the left.
  event_generator->MoveMouseTo(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().left_center());
  ASSERT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(
      std::round(GetWorkAreaBounds().width() * chromeos::kTwoThirdSnapRatio),
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  EXPECT_NEAR(chromeos::kTwoThirdSnapRatio,
              *WindowState::Get(w1.get())->snap_ratio(), 0.01);
  EXPECT_NEAR(chromeos::kOneThirdSnapRatio,
              *WindowState::Get(w2.get())->snap_ratio(), 0.01);
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  auto* divider_view = divider->divider_view_for_testing();
  ASSERT_TRUE(divider_view);
  auto* focus_ring = views::FocusRing::Get(divider_view);
  ASSERT_TRUE(focus_ring);
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
  EXPECT_FALSE(focus_ring->GetVisible());

  event_generator->MoveMouseBy(10, 0);
  EXPECT_EQ(kSplitviewDividerEnlargedShortSideLength, divider_view->width());
  const auto handler_view_bounds_on_drag =
      divider_view->GetHandlerViewBoundsInScreenForTesting();
  EXPECT_EQ(kDividerHandlerEnlargedShortSideLength,
            handler_view_bounds_on_drag.width());
  EXPECT_EQ(kDividerHandlerEnlargedLongSideLength,
            handler_view_bounds_on_drag.height());
  EXPECT_FALSE(focus_ring->GetVisible());

  event_generator->MoveMouseTo(gfx::Point(0, 0));
  EXPECT_EQ(kSplitviewDividerShortSideLength, divider_view->width());
  const auto handler_view_bounds_after_hover =
      divider_view->GetHandlerViewBoundsInScreenForTesting();
  EXPECT_EQ(kDividerHandlerShortSideLength,
            handler_view_bounds_after_hover.width());
  EXPECT_EQ(kDividerHandlerLongSideLength,
            handler_view_bounds_after_hover.height());
  EXPECT_FALSE(focus_ring->GetVisible());
}

// Tests that the split view divider will be stacked on top of both windows in
// the snap group and that on a third window activated the split view divider
// will be stacked below the newly activated window.
TEST_F(SnapGroupDividerTest, DividerStackingOrderTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  wm::ActivateWindow(w1.get());

  aura::Window* divider_window =
      GetTopmostSnapGroupDivider()->GetDividerWindow();
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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  wm::ActivateWindow(w1.get());

  aura::Window* divider_window =
      GetTopmostSnapGroupDivider()->GetDividerWindow();
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), w1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  auto w1_transient =
      CreateTransientChildWindow(w1.get(), gfx::Rect(100, 200, 200, 200));
  w1_transient->SetProperty(aura::client::kModalKey,
                            ui::mojom::ModalType::kWindow);
  wm::SetModalParent(w1_transient.get(), w1.get());
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w1_transient.get()));
}

// Tests the overall stacking order with two transient windows each of which
// belongs to a window in snap group is expected. The tests is to verify the
// transient windows issue showed in http://b/297448600#comment2.
TEST_F(SnapGroupDividerTest, DividerStackingOrderWithTwoTransientWindows) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  aura::Window* divider_window =
      GetTopmostSnapGroupDivider()->GetDividerWindow();
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  ASSERT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  // By default `w1_transient` is `ModalType::kNone`, meaning that the
  // associated `w1` is interactable.
  std::unique_ptr<aura::Window> w1_transient(
      CreateTransientChildWindow(w1.get(), gfx::Rect(10, 20, 20, 30)));

  // Add transient window for `w2` and making it not interactable by setting it
  // with the type of `ui::mojom::ModalType::kWindow`.
  std::unique_ptr<aura::Window> w2_transient(
      CreateTransientChildWindow(w2.get(), gfx::Rect(200, 20, 20, 30)));
  w2_transient->SetProperty(aura::client::kModalKey,
                            ui::mojom::ModalType::kWindow);
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

// Verifies that if the stacking order of the divider window is altered by
// another dialog transient window, `SplitViewDivider` is able to correct it,
// placing the divider below the dialog transient window. See http://b/341332379
// for more details.
TEST_F(SnapGroupDividerTest,
       DividerStackingOrderWithDialogTransientUndoStacking) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));
  aura::Window* top_window = w2.get();
  aura::Window* top_window_parent = top_window->parent();

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  ASSERT_TRUE(divider);
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  aura::Window* divider_window = divider_widget->GetNativeWindow();
  ASSERT_TRUE(wm::HasTransientAncestor(divider_window, w2.get()));
  ASSERT_EQ(top_window_parent, divider_window->parent());

  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  ASSERT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  // Create a dialog widget that's associated with `w2`.
  views::DialogDelegateView* delegate = new views::DialogDelegateView();
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      delegate, GetContext(), /*parent=*/w2.get());
  aura::Window* w2_transient = widget->GetNativeWindow();
  ASSERT_TRUE(wm::HasTransientAncestor(w2_transient, w2.get()));
  ASSERT_EQ(top_window_parent, w2_transient->parent());

  // When stacking a child window relative to a target, only the child is
  // notified of stacking order changes (see `Window::StackChildRelativeTo()`).
  // Since `SplitViewDivider` doesn't observe the divider window, we use
  // `StackChildBelow` to ensure the transient window (`w2_transient`) receives
  // this notification, correcting the stacking order so that the divider stays
  // below it.
  top_window_parent->StackChildBelow(w2_transient, divider_window);
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w2_transient));
}

// Tests divider stacking behavior below transient dialog transient of Snap
// Group windows:
//  - During resizing
//  - After clicking post-resize
// Regression test for http://b/349894878.
TEST_F(SnapGroupDividerTest, DividerStackingWhenResizingWithDialogTransient) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  ASSERT_TRUE(divider);
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  aura::Window* divider_window = divider_widget->GetNativeWindow();
  ASSERT_TRUE(wm::HasTransientAncestor(divider_window, w2.get()));

  aura::Window* top_window = w2.get();
  aura::Window* top_window_parent = top_window->parent();
  ASSERT_EQ(top_window_parent, divider_window->parent());

  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), w2.get()));
  ASSERT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  ASSERT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));

  // Create a dialog widget that's associated with `w2`.
  views::DialogDelegateView* delegate = new views::DialogDelegateView();
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      delegate, GetContext(), /*parent=*/w2.get());
  aura::Window* w2_transient = widget->GetNativeWindow();
  ASSERT_TRUE(wm::HasTransientAncestor(w2_transient, w2.get()));
  ASSERT_EQ(top_window_parent, w2_transient->parent());

  // Verify that divider remains stacked below the `w2_transient` on resize
  // ended.
  ResizeDividerTo(event_generator,
                  gfx::Point(w2_transient->GetBoundsInScreen().CenterPoint()));
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w2_transient));

  // Click on the divider and it is still stacked below the `w2_transient`.
  event_generator->MoveMouseTo(
      divider_widget->GetWindowBoundsInScreen().top_center());
  event_generator->ClickLeftButton();
  EXPECT_TRUE(window_util::IsStackedBelow(divider_window, w2_transient));
  EXPECT_TRUE(window_util::IsStackedBelow(w1.get(), divider_window));
  EXPECT_TRUE(window_util::IsStackedBelow(w2.get(), divider_window));
}

// Tests that the union bounds of the primary window, secondary window in a snap
// group and the snap group divider will be equal to the work area bounds both
// in horizontal and vertical split view mode.
TEST_F(SnapGroupDividerTest, SnapGroupDividerBoundsTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  for (const auto is_horizontal : {true, false}) {
    if (is_horizontal) {
      UpdateDisplay("900x600");
    } else {
      UpdateDisplay("600x900");
    }

    ASSERT_EQ(IsLayoutHorizontal(w1.get()), is_horizontal);

    SnapTwoTestWindows(w1.get(), w2.get(), is_horizontal, event_generator);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());

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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);

  Shelf* shelf = GetPrimaryShelf();
  ASSERT_EQ(shelf->auto_hide_behavior(), ShelfAutoHideBehavior::kNever);

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(divider_widget->GetWindowBoundsInScreen().height(),
            GetWorkAreaBounds().height());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), divider);
}

// Tests that snapped windows and divider bounds adjust correctly when shelf
// alignment changes.
TEST_F(SnapGroupDividerTest, SnapGroupDividerBoundsWithShelfAlignmentChange) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
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

// Tests that the cursor type gets updated to be resize cursor on mouse hovering
// on the split view divider excluding the feedback button.
TEST_F(SnapGroupDividerTest, CursorUpdateTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* divider = GetTopmostSnapGroupDivider();
  ASSERT_TRUE(divider->divider_widget());

  auto divider_bounds = GetTopmostSnapGroupDividerBoundsInScreen();
  auto outside_point = divider_bounds.CenterPoint();
  outside_point.Offset(-kSplitviewDividerShortSideLength * 5, 0);
  EXPECT_FALSE(divider_bounds.Contains(outside_point));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(CursorType::kPointer);

  // Test that the default cursor type when mouse is not hovered over the split
  // view divider.
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
  EXPECT_EQ(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint() + delta_vector,
      cached_hover_point + move_vector);
}

//  Tests that the cursor updates correctly after snap to replace. See
//  regression at http://b/331240308
TEST_F(SnapGroupDividerTest, CursorUpdateAfterSnapToReplace) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
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

  ASSERT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());

  auto divider_bounds = GetTopmostSnapGroupDividerBoundsInScreen();
  auto outside_point = GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  outside_point.Offset(-kSplitviewDividerShortSideLength * 5, 0);
  EXPECT_FALSE(divider_bounds.Contains(outside_point));

  auto* cursor_manager = Shell::Get()->cursor_manager();
  cursor_manager->SetCursor(CursorType::kPointer);

  // Test that the default cursor type when mouse is not hovered over the split
  // view divider.
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
  EXPECT_EQ(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint() + delta_vector,
      cached_hover_point + move_vector);
}

// Verify that the cursor changes to `kColumnResize` when hovering over the
// divider handler view in landscape.
TEST_F(SnapGroupDividerTest, CursorUpdateOnHandlerViewInLandscape) {
  UpdateDisplay("900x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* divider = GetTopmostSnapGroupDivider();
  ASSERT_TRUE(divider->divider_widget());

  auto divider_bounds = GetTopmostSnapGroupDividerBoundsInScreen();

  auto* cursor_manager = Shell::Get()->cursor_manager();
  const auto center_point = divider_bounds.CenterPoint();

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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/false, event_generator);
  auto* divider = GetTopmostSnapGroupDivider();
  ASSERT_TRUE(divider->divider_widget());

  auto divider_bounds = GetTopmostSnapGroupDividerBoundsInScreen();

  auto* cursor_manager = Shell::Get()->cursor_manager();
  const auto center_point = divider_bounds.CenterPoint();

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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  const gfx::Point cached_divider_center_point =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
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
  // Verify that split view remains inactive to avoid split view specific
  // behaviors such as auto-snap or showing cannot snap toast.
  EXPECT_FALSE(GetSplitViewController()->InSplitViewMode());
  EXPECT_EQ(hover_location + move_vector,
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
}

// Tests that a double-tap gesture on the divider handler within a Snap Group
// successfully swaps the two snapped windows.
TEST_F(SnapGroupDividerTest, DoubleTapDividerBasic) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  auto* divider_view = divider->divider_view_for_testing();
  ASSERT_TRUE(divider_view);
  auto* handler_view = divider_view->handler_view_for_testing();
  ASSERT_TRUE(handler_view);

  // A double-tap on the divider handler within a Snap Group swaps the window
  // positions.
  const auto handler_view_center =
      divider_view->GetHandlerViewBoundsInScreenForTesting().CenterPoint();
  event_generator->set_current_screen_location(handler_view_center);
  event_generator->GestureTapAt(handler_view_center);
  event_generator->GestureTapAt(handler_view_center);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  SnapGroup* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  EXPECT_EQ(w1.get(), snap_group->window2());
  EXPECT_EQ(w2.get(), snap_group->window1());
  UnionBoundsEqualToWorkAreaBounds(snap_group);
}

// Tests that double-tap on the Snap Group divider handler swaps the windows
// and their bounds, and that the divider position will adjust correspondingly.
TEST_F(SnapGroupDividerTest, DoubleTapDividerToSwapWindowsBounds) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  auto* divider_view = divider->divider_view_for_testing();
  ASSERT_TRUE(divider_view);
  auto* handler_view = divider_view->handler_view_for_testing();
  ASSERT_TRUE(handler_view);

  auto handler_view_center =
      divider_view->GetHandlerViewBoundsInScreenForTesting().CenterPoint();
  event_generator->set_current_screen_location(handler_view_center);
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(100, handler_view_center.y()),
                               /*count=*/2);
  event_generator->ReleaseLeftButton();

  const auto w1_snap_ratio_after_drag =
      WindowState::Get(w1.get())->snap_ratio();
  ASSERT_TRUE(w1_snap_ratio_after_drag);
  EXPECT_NE(chromeos::kDefaultSnapRatio, *w1_snap_ratio_after_drag);

  const auto w2_snap_ratio_after_drag =
      WindowState::Get(w2.get())->snap_ratio();
  ASSERT_TRUE(w2_snap_ratio_after_drag);
  EXPECT_NE(chromeos::kDefaultSnapRatio, *w2_snap_ratio_after_drag);

  const gfx::Rect w1_bounds_before_swap = w1->GetBoundsInScreen();
  const gfx::Rect w2_bounds_before_swap = w2->GetBoundsInScreen();
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), divider);

  // Perform double tap on the divider handler and verify that the snapped
  // windows bounds will swap.
  handler_view_center =
      divider_view->GetHandlerViewBoundsInScreenForTesting().CenterPoint();
  event_generator->GestureTapAt(handler_view_center);
  event_generator->GestureTapAt(handler_view_center);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  SnapGroup* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  EXPECT_EQ(w1.get(), snap_group->window2());
  EXPECT_EQ(w2.get(), snap_group->window1());

  EXPECT_EQ(w1_bounds_before_swap.width(),
            snap_group->window2()->GetBoundsInScreen().width());
  EXPECT_EQ(w2_bounds_before_swap.width(),
            snap_group->window1()->GetBoundsInScreen().width());
  UnionBoundsEqualToWorkAreaBounds(snap_group);
}

// Tests that a double-tap gesture on the divider handler within a Snap Group
// successfully swaps the two snapped windows, each of which has a transient
// window attached.
TEST_F(SnapGroupDividerTest, DoubleTapDividerWithTransient) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);

  // By default transient is `ModalType::kNone`, meaning that the associated
  // window is interactable.
  std::unique_ptr<aura::Window> w1_transient(
      CreateTransientChildWindow(w1.get(), gfx::Rect(10, 20, 20, 30)));
  std::unique_ptr<aura::Window> w2_transient(
      CreateTransientChildWindow(w2.get(), gfx::Rect(510, 30, 50, 30)));

  // A double-tap on the divider handler within a Snap Group swaps the window
  // positions.
  const auto divider_center_point =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  event_generator->set_current_screen_location(divider_center_point);
  event_generator->GestureTapAt(divider_center_point);
  event_generator->GestureTapAt(divider_center_point);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  SnapGroup* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  EXPECT_EQ(w1.get(), snap_group->window2());
  EXPECT_TRUE(
      wm::HasTransientAncestor(w1_transient.get(), snap_group->window2()));
  EXPECT_EQ(w2.get(), snap_group->window1());
  EXPECT_TRUE(
      wm::HasTransientAncestor(w2_transient.get(), snap_group->window1()));
  UnionBoundsEqualToWorkAreaBounds(snap_group);
}

// Tests that performing a double-tap gesture during a divider drag operation
// does not cause crash.
TEST_F(SnapGroupDividerTest, DoubleTapWhileDraggingDivider) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);

  const auto divider_center_point_0 =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  event_generator->PressTouchId(
      /*touch_id=*/0, divider_center_point_0);

  // Use the initial tap to drag the divider.
  event_generator->MoveTouchId(gfx::Point(100, divider_center_point_0.y()), 0);
  ASSERT_TRUE(divider->is_resizing_with_divider());

  // Trigger double tap, using a different `touch_id`.
  const auto divider_center_point_1 =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  event_generator->PressTouchId(
      /*touch_id=*/1, divider_center_point_1);
  event_generator->PressTouchId(
      /*touch_id=*/1, divider_center_point_1);

  base::RunLoop().RunUntilIdle();
}

TEST_F(SnapGroupDividerTest, DoubleTapDividerInTablet) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group = SnapGroupController::Get()->GetTopmostSnapGroup();
  EXPECT_TRUE(snap_group);
  auto* new_primary_window = snap_group->window1();
  auto* new_secondary_window = snap_group->window2();

  // Switch to tablet mode. Test that double tap on the divider swaps the
  // windows.
  SwitchToTabletMode();
  EXPECT_EQ(new_primary_window, GetSplitViewController()->primary_window());
  EXPECT_EQ(new_secondary_window, GetSplitViewController()->secondary_window());
  EXPECT_TRUE(GetSplitViewDivider()->divider_widget());
  const gfx::Point divider_center =
      GetSplitViewDivider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  event_generator->GestureTapAt(divider_center);
  event_generator->GestureTapAt(divider_center);
  EXPECT_EQ(new_secondary_window, GetSplitViewController()->primary_window());
  EXPECT_EQ(new_primary_window, GetSplitViewController()->secondary_window());
}

// Tests that when the cursor is moved significantly past the window sizes
// then moved the other direction, we don't update bounds.
TEST_F(SnapGroupDividerTest, ResizeCursor) {
  const int min_width = 300;
  std::unique_ptr<aura::Window> w1(
      CreateAppWindowWithMinSize(gfx::Size(min_width, min_width)));
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_divider = SnapGroupController::Get()
                                 ->GetSnapGroupForGivenWindow(w1.get())
                                 ->snap_group_divider();
  for (const auto& display_specs : {"800x600", "600x800"}) {
    UpdateDisplay(display_specs);
    const auto display = display::Screen::GetScreen()->GetPrimaryDisplay();
    // Press and move the mouse left without releasing, past `w1`'s min width.
    // Test we don't update bounds beyond `w1`'s min width.
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
    EXPECT_EQ(
        600,
        horizontal
            ? GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x()
            : GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().y());
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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // Verify that full overview session is expected when starting overview from
  // accelerator and that split view divider will not be available.
  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(overview_controller->overview_session());
  EXPECT_EQ(GetOverviewGridBounds(w1->GetRootWindow()), GetWorkAreaBounds());
  EXPECT_FALSE(GetTopmostSnapGroupDivider()->divider_widget()->IsVisible());
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
  EXPECT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
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
    EXPECT_NE(GetOverviewGridBounds(root_window), GetWorkAreaBounds());
    EXPECT_NEAR(GetOverviewGridBounds(root_window).width(),
                GetWorkAreaBounds().width() / 2.f,
                kSplitviewDividerShortSideLength / 2.f);
  }
}

// Tests that the group item will be created properly and that the snap group
// will be represented as one group item in overview.
TEST_F(SnapGroupOverviewTest, OverviewGroupItemCreationBasic) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(),
                     /*horizontal=*/true, GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());
}

// Verifies that the divider doesn't appear precipitously before the exit
// animation of the two windows in overview mode is complete, guaranteeing a
// seamless transition. See regression at http://b/333465871.
TEST_F(SnapGroupOverviewTest, DividerExitOverviewAnimation) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  ASSERT_TRUE(divider);
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  ASSERT_TRUE(divider_widget->IsVisible());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  WaitForOverviewEntered();
  EXPECT_TRUE(divider_widget);
  EXPECT_FALSE(divider_widget->IsVisible());

  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, GetEventGenerator());
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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  ASSERT_EQ(2u, overview_grid->item_list().size());

  // On one window in snap group destroying, the group item will host the other
  // window.
  w2.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());

  // On the only remaining window in snap group destroying, the group item will
  // be removed from the overview grid.
  w1.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(1u, overview_grid->item_list().size());
}

// Tests that the rounded corners of the remaining item in the snap group on
// window destruction will be refreshed so that the exposed corners will be
// rounded corners.
TEST_F(SnapGroupOverviewTest, RefreshVisualsOnWindowDestructionInOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests);
  ASSERT_TRUE(overview_controller->overview_session());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& overview_items = overview_grid->item_list();
  ASSERT_EQ(2u, overview_items.size());

  w2.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());

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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  ASSERT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
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
  ASSERT_EQ(2u, overview_grid->item_list().size());

  // On one window in snap group destroying, the group item will host the other
  // window.
  w2.reset();
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());

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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  ASSERT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
  const gfx::Point hover_location =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  GetTopmostSnapGroupDivider()->StartResizeWithDivider(hover_location);
  const gfx::Vector2d drag_delta(-GetWorkAreaBounds().width() / 6, 0);
  const auto end_point = hover_location + drag_delta;
  GetTopmostSnapGroupDivider()->ResizeWithDivider(end_point);
  GetTopmostSnapGroupDivider()->EndResizeWithDivider(end_point);
  // Verify that split view remains inactive to avoid split view specific
  // behaviors such as auto-snap or showing cannot snap toast.
  EXPECT_FALSE(GetSplitViewController()->InSplitViewMode());
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
  ASSERT_EQ(2u, overview_items.size());

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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  ASSERT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());

  // Drag the divider between the snapped windows to get the 1/3 and 2/3 split
  // screen.
  const gfx::Point hover_location =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  GetTopmostSnapGroupDivider()->StartResizeWithDivider(hover_location);
  const gfx::Vector2d drag_delta(-GetWorkAreaBounds().width() / 6, 0);
  const auto end_point = hover_location + drag_delta;
  GetTopmostSnapGroupDivider()->ResizeWithDivider(end_point);
  GetTopmostSnapGroupDivider()->EndResizeWithDivider(end_point);

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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w0.get()));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());

  // Since the window will be deleted in overview, release the ownership to
  // avoid double deletion.
  w0.release();

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

// Test some basic keyboard traversal on a snap group in overview.
TEST_F(SnapGroupOverviewTest, TabbingBasic) {
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w0.get()));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());

  OverviewFocusCycler* focus_cycler =
      overview_controller->overview_session()->focus_cycler();
  // Tab through and verify each individual snap group item.
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(overview_items[0]->overview_item_view(),
            focus_cycler->GetOverviewFocusedView());

  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_EQ(overview_items[1]->overview_item_view(),
            focus_cycler->GetOverviewFocusedView());

  // Press return which will activate the snap group and exit overview.
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Within an overview group, verify that "Ctrl + W" closes only the focused
// item's window, and that tabbing afterwards activates remaining items without
// causing a crash. See http://b/344216297 for more details.
TEST_F(SnapGroupOverviewTest, CtrlPlusWToCloseFocusedItemInGroupInOverview) {
  // Explicitly enable immediate close so that we can directly close the
  // window(s) without waiting the delayed task to be completed in
  // `ScopedOverviewTransformWindow::Close()`.
  ScopedOverviewTransformWindow::SetImmediateCloseForTests(/*immediate=*/true);

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(GetOverviewItemForWindow(w0.get()));

  auto* event_generator = GetEventGenerator();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_TRUE(overview_session->focus_cycler()->GetOverviewFocusedView());

  // Since the window will be deleted in overview, release the ownership to
  // avoid double deletion.
  w0.release();

  // Press `Ctrl + w` to close `w0`.
  SendKey(ui::VKEY_W, event_generator, ui::EF_CONTROL_DOWN);

  // Verify that `w0` in the snap group will be deleted.
  EXPECT_FALSE(w0.get());
  EXPECT_TRUE(w1.get());

  // Native widget close is not immediate. Run `base::RunLoop().RunUntilIdle()`
  // to ensure close task to finish before proceeding.
  base::RunLoop().RunUntilIdle();

  // Tab again to focus on the remaining window `w1`.
  SendKey(ui::VKEY_TAB, event_generator);

  // Press enter key to active `w1`, verify that there will be no crash.
  SendKey(ui::VKEY_RETURN, event_generator);
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

// Tests that the bounds on the overview group item as well as the individual
// overview item hosted by the group item will be set correctly.
TEST_F(SnapGroupOverviewTest, OverviewItemBoundsTest) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(2u, item_list.size());
  for (const auto& overview_item : item_list) {
    EXPECT_EQ(overview_item->GetRoundedCorners(),
              gfx::RoundedCornersF(kWindowMiniViewCornerRadius));
  }
}

// Tests that if two windows are both snapped before entering overview but not
// in a Snap Group, re-snapping the windows on both sides of the screen will not
// result in crash and two windows will be grouped. See http://b/346617565 for
// more details
TEST_F(SnapGroupOverviewTest, ReSnapSnappedWindowInOverview) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  std::unique_ptr<aura::Window> w2 = CreateAppWindow();
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(w2.get())->OnWMEvent(&minimize_event);

  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  OverviewItemBase* overview_item1 = GetOverviewItemForWindow(w1.get());
  OverviewItemBase* overview_item2 = GetOverviewItemForWindow(w2.get());

  DragItemToPoint(overview_item1, gfx::Point(0, 200), event_generator);
  DragItemToPoint(overview_item2, gfx::Point(800, 200), event_generator);
  VerifyNotSplitViewOrOverviewSession(w1.get());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests the rounded corners will be applied to the exposed corners of the
// overview group item in vertical wndow layout.
TEST_F(SnapGroupOverviewTest, OverviewGroupItemRoundedCornersInVertical) {
  UpdateDisplay("600x900");
  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(100, 100));
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/false,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(2u, item_list.size());
  for (const auto& overview_item : item_list) {
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->overview_session());
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(2u, item_list.size());

  // Wait until the post task to `UpdateRoundedCornersAndShadow()` triggered in
  // `OverviewController::DelayedUpdateRoundedCornersAndShadow()` is finished.
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  base::RunLoop().RunUntilIdle();
  for (const auto& overview_item : item_list) {
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

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
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(5u, item_list.size());

  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(item_list[4].get());
  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());

  w0.reset();
  EXPECT_EQ(item_list.size(), 5u);
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     event_generator);
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

  for (const auto& test : kTestCases) {
    overview_controller->StartOverview(OverviewStartAction::kTests,
                                       OverviewEnterExitType::kImmediateEnter);
    ASSERT_TRUE(overview_controller->InOverviewSession());

    const auto* overview_grid =
        GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
    ASSERT_TRUE(overview_grid);
    const auto& item_list = overview_grid->item_list();
    ASSERT_EQ(2u, item_list.size());

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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     event_generator);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window0.get());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     event_generator);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  aura::Window* primary_root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid = GetOverviewGridForRoot(primary_root_window);
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window0.get());
  const gfx::RectF target_bounds_before_dragging =
      overview_item->target_bounds();

  for (const bool by_touch : {false, true}) {
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(2u, item_list.size());

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* group_item = overview_session->GetOverviewItemForWindow(w0.get());
  auto* group_item_widget = group_item->item_widget();
  auto* w2_item_pre_drag = GetOverviewItemForWindow(w2.get());
  EXPECT_TRUE(window_util::IsStackedBelow(
      w2_item_pre_drag->item_widget()->GetNativeWindow(),
      group_item_widget->GetNativeWindow()));

  // Initiate the first drag.
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     event_generator);

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

// Tests that if one of `OverviewItem`s hosted by the `OverviewGroupItem` has a
// focus ring before being dragged, the focus ring is cleared when dragging
// begins.
TEST_F(SnapGroupOverviewTest, ClearFocusOnDragStart) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> w0 = CreateAppWindow();
  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());

  auto* group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w0.get()));
  const auto& overview_items = group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());

  OverviewFocusCycler* focus_cycler =
      overview_controller->overview_session()->focus_cycler();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  EXPECT_EQ(overview_items[0]->overview_item_view(),
            focus_cycler->GetOverviewFocusedView());

  // Verify that the focus ring gets cleared on drag starts.
  DragGroupItemToPoint(
      group_item,
      Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
      event_generator, /*by_touch_gestures=*/false, /*drop=*/false);

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(focus_cycler->GetOverviewFocusedView());
  event_generator->ReleaseLeftButton();
}

// Tests that converting to tablet mode while dragging an `OverviewGroupItem`
// doesn't result in crash. Regression test for http://b/359942514.
TEST_F(SnapGroupOverviewTest, ConvertToTabletModeWhileDragging) {
  std::unique_ptr<aura::Window> w0 = CreateAppWindow();
  std::unique_ptr<aura::Window> w1 = CreateAppWindow();
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());

  auto* group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w0.get()));
  const auto& overview_items = group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());

  DragGroupItemToPoint(
      group_item,
      Shell::GetPrimaryRootWindow()->GetBoundsInScreen().CenterPoint(),
      event_generator, /*by_touch_gestures=*/false, /*drop=*/false);

  EXPECT_TRUE(overview_controller->InOverviewSession());

  SwitchToTabletMode();
  base::RunLoop().RunUntilIdle();
}

// Tests that fling-to-close gestures on `OverviewGroupItem` closes the windows
// in the Snap Group.
TEST_F(SnapGroupOverviewTest, FlingToCloseGroupItem) {
  // Explicitly enable immediate close so that we can directly close the
  // window(s) without waiting the delayed task to be completed in
  // `ScopedOverviewTransformWindow::Close()`.
  ScopedOverviewTransformWindow::SetImmediateCloseForTests(true);

  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  std::unique_ptr<aura::Window> window0 = CreateAppWindow();
  auto* window_widget0 = views::Widget::GetWidgetForNativeView(window0.get());
  views::test::TestWidgetObserver observer0(window_widget0);
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  auto* window_widget1 = views::Widget::GetWidgetForNativeView(window1.get());
  views::test::TestWidgetObserver observer1(window_widget1);
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     GetEventGenerator());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton,
                                     OverviewEnterExitType::kImmediateEnter);
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);

  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(window0.get()));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());

  // Pre-release ownership of `window0` and `window1` using `release()`. This is
  // crucial to avoid double-freeing memory. When unique_ptr goes out of scope,
  // its destructor will attempt to deallocate the owned memory. Since CloseAll
  // will already handle the window destruction, leaving the unique_ptrs to
  // manage the memory would lead to a second deallocation attempt on the same
  // address, resulting in crash.
  window0.release();
  window1.release();

  gfx::PointF location = overview_group_item->target_bounds().CenterPoint();
  location.Offset(/*delta_x=*/-10, /*delta_y=*/0);

  overview_session->InitiateDrag(overview_group_item, location,
                                 /*is_touch_dragging=*/true,
                                 /*event_source_item=*/overview_items[0].get());

  // Perform fling-to-close with vertical velocity greater than
  // `kFlingToCloseVelocityThreshold` to trigger fling-to-close.
  overview_session->Fling(overview_group_item, location, /*velocity_x=*/0,
                          /*velocity_y=*/2500);

  // Widget closure is asynchronous and may not finish immediately. For
  // guaranteed completion, run the current thread's RunLoop until idle (See
  // `NativeWidgetAura::Close()` for details).
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(observer0.widget_closed());
  EXPECT_TRUE(observer1.widget_closed());

  // Verify that Overview exits with no Overview items exist.
  EXPECT_FALSE(IsInOverviewSession());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     event_generator);

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());

  OverviewSession* overview_session = overview_controller->overview_session();
  auto* overview_item =
      overview_session->GetOverviewItemForWindow(window0.get());
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

  auto* event_generator = GetEventGenerator();
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
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
      GetTopmostSnapGroupDivider()->divider_widget()->GetWindowBoundsInScreen();
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
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
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

// This test validates `OverviewItemFillMode` restrictions on
// `OverviewGroupItem`:
//   - Windows within an `OverviewGroupItem` are prohibited from using special
//   `OverviewItemFillMode`s (`kPillarBoxed`, `kLetterBoxed`).
//   - This restriction is in place to avoid visual glitches and header
//   misalignment problems that can occur when one window in the group is
//   resized to a very narrow or wide aspect ratio.
// See http://b/341750824 for more details.
TEST_F(SnapGroupOverviewTest, OverviewItemFillMode) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  const gfx::Point divider_center(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  event_generator->MoveMouseTo(divider_center);
  event_generator->PressLeftButton();

  // Resize the divider to the extreme left. This makes the left snapped window
  // so narrow that it will be the `OverviewItemFillMode::kPillarBoxed` if it is
  // an independent window.
  event_generator->MoveMouseTo(gfx::Point(10, 200), /*count=*/2);
  event_generator->ReleaseLeftButton();

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w1.get()));
  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  for (const auto& overview_item : overview_items) {
    // As we disallow setting special `OverviewItemFillMode` for windows in a
    // Snap Group, the left snapped window will maintain the default
    // `OverviewItemFillMode::kNormal` fill mode.
    EXPECT_EQ(OverviewItemFillMode::kNormal,
              overview_item->GetOverviewItemFillMode());
  }
}

// Verifies bubble transient windows hide in Overview, reappear on Overview
// exit, while other transient windows (unless `kHideInOverviewKey` is set to
// true) remain visible.
TEST_F(SnapGroupOverviewTest, HideBubbleTransientInOverview) {
  std::unique_ptr<aura::Window> w0(CreateAppWindow(gfx::Rect(0, 0, 300, 300)));
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(500, 20, 200, 200)));
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // Create a bubble widget that's anchored to `w0`.
  auto bubble_delegate0 = std::make_unique<views::BubbleDialogDelegateView>(
      NonClientFrameViewAsh::Get(w0.get()), views::BubbleBorder::TOP_RIGHT);

  // The line below is essential to make sure that the bubble doesn't get closed
  // when entering overview.
  bubble_delegate0->set_close_on_deactivate(false);
  bubble_delegate0->set_parent_window(w0.get());
  views::Widget* bubble_widget0(views::BubbleDialogDelegateView::CreateBubble(
      std::move(bubble_delegate0)));
  aura::Window* bubble_window0 = bubble_widget0->GetNativeWindow();
  ASSERT_TRUE(window_util::AsBubbleDialogDelegate(bubble_window0));

  bubble_widget0->Show();
  EXPECT_TRUE(wm::HasTransientAncestor(bubble_window0, w0.get()));

  // Verify that the bubble is created inside its anchor widget.
  EXPECT_TRUE(
      w0->GetBoundsInScreen().Contains(bubble_window0->GetBoundsInScreen()));

  // By default `w1_transient` is `ModalType::kNone`.
  std::unique_ptr<aura::Window> w1_transient(
      CreateTransientChildWindow(w1.get(), gfx::Rect(510, 30, 50, 30)));
  wm::AddTransientChild(w1.get(), w1_transient.get());

  // Verify that bubble transient windows are hidden on entering Overview mode.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(bubble_window0->IsVisible());
  EXPECT_TRUE(w1_transient->IsVisible());

  // Verify that bubble transient windows reappear on exiting Overview mode.
  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());

  EXPECT_TRUE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w0.get(), w1.get()));
  EXPECT_TRUE(bubble_window0->IsVisible());
  EXPECT_TRUE(w1_transient->IsVisible());
}

// Test that duplicate group items are not created when one of the windows in
// Snap Group has an activatable transient window. Regression test for
// http://b/349482229.
TEST_F(SnapGroupOverviewTest, NoDuplicateGroupItemsWithActivatableTransient) {
  UpdateDisplay("900x600");

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // By default `w1_transient` is `ModalType::kNone`, meaning that the
  // associated `w1` is interactable.
  auto w1_transient =
      CreateTransientChildWindow(w1.get(), gfx::Rect(600, 200, 200, 200));
  w1_transient->SetProperty(aura::client::kModalKey,
                            ui::mojom::ModalType::kWindow);
  wm::SetModalParent(w1_transient.get(), w1.get());

  wm::ActivateWindow(w0.get());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);

  // Verify that there will be only one Overview item in the list.
  EXPECT_EQ(1u, overview_grid->item_list().size());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     event_generator);

  ASSERT_TRUE(EnterOverview(OverviewEnterExitType::kImmediateEnter));

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(2u, mini_views.size());

  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);

  // Verify the initial conditions before dragging the item to another desk.
  ASSERT_EQ(desks_util::GetDeskForContext(window0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(window1.get()), desk0);

  // Test that both windows contained in the overview group item will be moved
  // to the another desk.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  DragGroupItemToPoint(
      overview_controller->overview_session()->GetOverviewItemForWindow(
          window0.get()),
      mini_views[1]->GetBoundsInScreen().CenterPoint(), event_generator,
      /*by_touch_gestures=*/false,
      /*drop=*/true);
  EXPECT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(desks_util::GetDeskForContext(window0.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(window1.get()), desk1);
  EXPECT_TRUE(SnapGroupController::Get()->AreWindowsInSnapGroup(window0.get(),
                                                                window1.get()));
  ActivateDesk(desk1);
  EXPECT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
  EXPECT_EQ(desks_util::GetDeskForContext(
                GetTopmostSnapGroupDivider()->GetDividerWindow()),
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);

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

  ASSERT_TRUE(EnterOverview(OverviewEnterExitType::kImmediateEnter));

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(2u, mini_views.size());

  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);

  // Verify the initial conditions before dragging the item to another desk.
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  // Test that both windows contained in the overview group item are contained
  // in `desk1` after the drag.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  ActivateDesk(desk1);
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 20, 100, 200)));
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true, event_generator);
  ASSERT_EQ(desks_util::GetDeskForContext(w2.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(w3.get()), desk1);

  ASSERT_TRUE(EnterOverview());

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(mini_views.size(), 2u);

  // Test that both windows contained in the overview group item will be moved
  // to the `desk0` and no crash on activating `desk0`.
  OverviewController* overview_controller = Shell::Get()->overview_controller();
  DragGroupItemToPoint(
      overview_controller->overview_session()->GetOverviewItemForWindow(
          w3.get()),
      mini_views[0]->GetBoundsInScreen().CenterPoint(), event_generator,
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
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
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  PressAndReleaseKey(ui::VKEY_RETURN);
  UnionBoundsEqualToWorkAreaBounds(w0.get(), w1.get(),
                                   GetTopmostSnapGroupDivider());
}

// Ensures no crashes occur when switching and merging desks with an active Snap
// Group in Overview. Regression test for http://b/348067578.
TEST_F(SnapGroupDesksTest, DesksSwitchingThenMergingInOverview) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());

  // Create `snap_group0` on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  ToggleOverview();
  WaitForOverviewEntered();
  ASSERT_TRUE(IsInOverviewSession());

  // Use `Search + ]` to switch to `desk1`.
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();
  ASSERT_TRUE(IsInOverviewSession());

  // Merge `desk0` into `desk1`.
  RemoveDesk(desk0, DeskCloseType::kCombineDesks);

  ASSERT_TRUE(IsInOverviewSession());
  auto* overview_group_item = GetOverviewItemForWindow(w0.get());
  ASSERT_TRUE(overview_group_item);

  // Verify that windows in `snap_group0` have been moved to `desk1`.
  EXPECT_EQ(1u, desks_controller->desks().size());
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w1.get()));

  // Activate the group item and verify the union bounds.
  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, event_generator);
  PressAndReleaseKey(ui::VKEY_RETURN);
  UnionBoundsEqualToWorkAreaBounds(w0.get(), w1.get(),
                                   GetTopmostSnapGroupDivider());
}

// Ensures switching and merging desks with one Snap Group on each desk works
// properly in Overview.
TEST_F(SnapGroupDesksTest,
       DesksSwitchingThenMergingWithOneSnapGroupPerDeskInOverview) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());

  // Create `snap_group0` on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
  SnapGroup* snap_group0 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group0);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  // Create `snap_group1` on `desk1`.
  ActivateDesk(desk1);
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 20, 100, 200)));
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true, event_generator);
  SnapGroup* snap_group1 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w2.get());
  ASSERT_TRUE(snap_group1);
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w2.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w3.get()));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  // Use `Search + [` to switch to `desk0`.
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();
  ASSERT_TRUE(IsInOverviewSession());

  // Merge `desk0` into `desk1`.
  RemoveDesk(desk0, DeskCloseType::kCombineDesks);

  ASSERT_TRUE(IsInOverviewSession());
  auto* overview_group_item = GetOverviewItemForWindow(w0.get());
  ASSERT_TRUE(overview_group_item);

  // Verify that windows in the Snap Group have been moved to `desk1`.
  EXPECT_EQ(1u, desks_controller->desks().size());
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w1.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w2.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w3.get()));
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  auto* divider = GetTopmostSnapGroupDivider();
  ASSERT_TRUE(divider);
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);

  event_generator->MoveMouseTo(
      divider_widget->GetWindowBoundsInScreen().CenterPoint());

  event_generator->DragMouseBy(100, 0);
  const gfx::Rect cached_w0_bounds(w0->GetBoundsInScreen());
  const gfx::Rect cached_w1_bounds(w1->GetBoundsInScreen());

  ASSERT_TRUE(EnterOverview(OverviewEnterExitType::kImmediateEnter));

  auto* overview_grid = GetOverviewGridForRoot(w0->GetRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(1u, item_list.size());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  const auto& mini_views = desks_bar_view->mini_views();
  ASSERT_EQ(2u, mini_views.size());

  OverviewController* overview_controller = Shell::Get()->overview_controller();
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
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
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true, event_generator);
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
TEST_F(SnapGroupDesksTest, SaveDeskForSnapGroupWithAnotherSavedDeskOld) {
  base::test::ScopedFeatureList disable;
  disable.InitAndDisableFeature(features::kSavedDeskUiRevamp);

  OverviewController* overview_controller = OverviewController::Get();

  // Explicitly disable `disable_app_id_check_for_saved_desks_` otherwise "Save
  // desk for later" button will be disabled.
  base::AutoReset<bool> disable_app_id_check =
      overview_controller->SetDisableAppIdCheckForTests();

  // Create `w0` and save `w0` in a saved desk by activing "Save desk for later"
  // button in Overview.
  aura::WindowTracker window_tracker;
  aura::Window* w0 = CreateAppWindow(gfx::Rect(10, 10, 500, 300)).release();
  window_tracker.Add(w0);

  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  OverviewSession* overview_session = overview_controller->overview_session();
  ASSERT_TRUE(overview_session);

  auto* root_window = Shell::GetPrimaryRootWindow();
  OverviewGrid* overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  ASSERT_EQ(1u, overview_grid->item_list().size());

  auto* save_for_later_button = overview_grid->GetSaveDeskForLaterButton();
  ASSERT_TRUE(save_for_later_button);
  base::RunLoop().RunUntilIdle();
  LeftClickOn(save_for_later_button);

  auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  ASSERT_TRUE(WaitForLibraryButtonVisible());

  overview_controller->EndOverview(OverviewEndAction::kOverviewButton);
  // `w0` should have been destroyed automatically when the
  // `save_for_later_button` was clicked.
  ASSERT_FALSE(window_tracker.Contains(w0));

  // Create a Snap Group and enter Overview again, click on the library button
  // on the virtual desks bar and verify that there is no crash.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(overview_session);

  overview_grid = GetOverviewGridForRoot(root_window);
  desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  auto* overview_group_item = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(overview_group_item);
  ASSERT_FALSE(GetTopmostSnapGroupDivider()->divider_widget()->IsVisible());

  const auto cached_group_item_bounds = overview_group_item->target_bounds();

  ASSERT_TRUE(WaitForLibraryButtonVisible());
  LeftClickOn(desks_bar_view->library_button());

  // Click the point outside of `cached_group_item_bounds` will exit Overview
  // and bring back the Snap Group.
  const gfx::Point click_point = gfx::ToRoundedPoint(
      cached_group_item_bounds.bottom_right() + gfx::Vector2d(20, 0));
  event_generator->MoveMouseTo(click_point);

  event_generator->ClickLeftButton();
  EXPECT_FALSE(IsInOverviewSession());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests that accessing the saved desks library after creating a Snap Group does
// not result in a crash, and the Snap Group is successfully restored upon
// exiting overview mode. See regression at http://b/335301800.
TEST_F(SnapGroupDesksTest, SaveDeskForSnapGroupWithAnotherSavedDesk) {
  saved_desk_test_helper()->WaitForDeskModels();
  base::test::ScopedFeatureList enable{features::kSavedDeskUiRevamp};

  OverviewController* overview_controller = OverviewController::Get();
  // Explicitly disable `disable_app_id_check_for_saved_desks_` otherwise "Save
  // desk for later" context menu item will be disabled.
  base::AutoReset<bool> disable_app_id_check =
      overview_controller->SetDisableAppIdCheckForTests();

  // Create a window and save it in a saved desk by clicking the "Save desk for
  // later" menu item in Overview. Release ownership as it will be destroyed
  // when saving it.
  CreateAppWindow(gfx::Rect(500, 300)).release();

  // Open Overview and then click the "Save desk for later" menu item. Verify
  // that it has saved a desk by checking for the library button.
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  views::MenuItemView* menu_item =
      DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
          Shell::GetPrimaryRootWindow(), DeskBarViewBase::Type::kOverview,
          /*index=*/0u, DeskActionContextMenu::CommandId::kSaveForLater);
  LeftClickOn(menu_item);
  // We have to wait one extra time for the closing windows. See
  // `SavedDeskTest::OpenOverviewAndSaveDeskForLater()`.
  WaitForSavedDeskUI();
  WaitForSavedDeskUI();
  ASSERT_TRUE(GetLibraryButton());
  overview_controller->EndOverview(OverviewEndAction::kOverviewButton);

  // Create a Snap Group and enter Overview again, click on the library button
  // on the virtual desks bar and verify that there is no crash.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  auto* group_item = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(group_item);
  ASSERT_FALSE(GetTopmostSnapGroupDivider()->divider_widget()->IsVisible());
  const gfx::RectF cached_group_item_bounds = group_item->target_bounds();
  auto* library_button = GetLibraryButton();
  LeftClickOn(library_button);

  // Click the point outside of `cached_group_item_bounds` will exit Overview
  // and bring back the Snap Group.
  const gfx::Point click_point = gfx::ToRoundedPoint(
      cached_group_item_bounds.bottom_right() + gfx::Vector2d(20, 0));
  event_generator->MoveMouseTo(click_point);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(IsInOverviewSession());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Verify that Snap Group will be broken when setting a window that belongs to a
// Snap Group to be visible on all workspaces.
TEST_F(SnapGroupDesksTest, MoveToAllDesksToBreakTheGroup) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  // Create `snap_group` on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  auto* window_widget0 = views::Widget::GetWidgetForNativeView(w0.get());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SnapGroup* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  // Move `w0` to all desks.
  window_widget0->SetVisibleOnAllWorkspaces(true);

  // Verify that `snap_group` will be broken.
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w0.get(), w1.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w0.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
}

// Test to verify that moving a Snap Group to all desks and removing it from its
// original desk doesn't cause a crash with one Snap Group per desk. See
// http://b/344982754 for more details.
TEST_F(SnapGroupDesksTest, DeskRemovalAfterMovingSnapGroupToAllDesks) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());
  ASSERT_FALSE(desk1->is_active());

  // Create `snap_group0` on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  auto* window_widget0 = views::Widget::GetWidgetForNativeView(w0.get());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);

  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  SnapGroup* snap_group0 =
      snap_group_controller->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group0);
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w0.get()));
  ASSERT_EQ(desk0, desks_util::GetDeskForContext(w1.get()));

  ActivateDesk(desk1);
  ASSERT_TRUE(desk1->is_active());
  ASSERT_FALSE(desk0->is_active());

  // Create `snap_group1` on `desk1`.
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true, event_generator);
  SnapGroup* snap_group1 =
      snap_group_controller->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group1);
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w2.get()));
  ASSERT_EQ(desk1, desks_util::GetDeskForContext(w3.get()));

  ActivateDesk(desk0);

  // Move `snap_group0` to all desks will break the Snap Group.
  window_widget0->SetVisibleOnAllWorkspaces(true);
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w0.get(), w1.get()));

  // Pre-release ownership of `w0` and `w1` using `release()`. This is crucial
  // to avoid double-freeing memory. When unique_ptr goes out of scope, its
  // destructor will attempt to deallocate the owned memory. Since CloseAll will
  // already handle the window destruction, leaving the unique_ptrs to manage
  // the memory would lead to a second deallocation attempt on the same address,
  // resulting in crash.
  w0.release();
  w1.release();

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  // Remove `desk0` and verify that there will be no CHECK crash.
  RemoveDesk(desk0, DeskCloseType::kCloseAllWindowsAndWait);
  ASSERT_TRUE(desk0->is_desk_being_removed());

  base::RunLoop().RunUntilIdle();
}

// -----------------------------------------------------------------------------
// SnapGroupWindowCycleTest:

class SnapGroupWindowCycleTest : public SnapGroupTest {
 public:
  SnapGroupWindowCycleTest() {
    WindowCycleList::SetDisableInitialDelayForTesting(true);
  }

  ~SnapGroupWindowCycleTest() override = default;

  void AltTabNTimes(int n) {
    WindowCycleController* window_cycle_controller =
        Shell::Get()->window_cycle_controller();

    auto* event_generator = GetEventGenerator();

    for (int i = 0; i < n; i++) {
      event_generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
      event_generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
      EXPECT_TRUE(window_cycle_controller->IsCycling());
    }

    event_generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
    EXPECT_FALSE(window_cycle_controller->IsCycling());
  }
};

// Tests that the window list is reordered when there is snap group. The two
// windows will be adjacent with each other with physically left/top snapped
// window put before physically right/bottom snapped window.
TEST_F(SnapGroupWindowCycleTest, WindowReorderInAltTabInPrimaryOrientation) {
  std::unique_ptr<aura::Window> window0(CreateTestWindowInShellWithId(0));
  window0->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  window1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     GetEventGenerator());

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
  window0->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  window1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     GetEventGenerator());

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
  window0->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  window1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     GetEventGenerator());

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

  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     GetEventGenerator());
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

// Tests that using 'Alt + Tab' for quick switch correctly cycles focus between
// two snapped windows within a Snap Group, without showing the window cycling
// UI.
TEST_F(SnapGroupWindowCycleTest, QuickSwitch) {
  WindowCycleList::SetDisableInitialDelayForTesting(false);

  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));

  WindowCycleController* window_cycle_controller =
      Shell::Get()->window_cycle_controller();

  // Press 'Alt + Tab' keyboard shortcut to trigger window cycling, verify that
  // the focus is switched to `w0` in the Snap Group.
  auto* event_generator = GetEventGenerator();
  event_generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  event_generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const auto* window_cycle_list0 = window_cycle_controller->window_cycle_list();
  ASSERT_TRUE(window_cycle_list0);
  EXPECT_FALSE(window_cycle_list0->cycle_view());
  event_generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));

  // Press 'Alt + Tab' keyboard shortcut again to trigger window cycling ,
  // verify that the focus is switched back to `w1` in the Snap Group.
  event_generator->PressKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  event_generator->ReleaseKey(ui::VKEY_TAB, ui::EF_ALT_DOWN);
  EXPECT_TRUE(window_cycle_controller->IsCycling());
  const auto* window_cycle_list1 = window_cycle_controller->window_cycle_list();
  ASSERT_TRUE(window_cycle_list1);
  EXPECT_FALSE(window_cycle_list1->cycle_view());
  event_generator->ReleaseKey(ui::VKEY_MENU, ui::EF_NONE);
  EXPECT_TRUE(wm::IsActiveWindow(w1.get()));
}

// Tests that window cycling correctly navigates across multiple Snap Groups on
// different virtual desks. Upon completing a window cycle, the active desk is
// switched to the one containing the activated window.
TEST_F(SnapGroupWindowCycleTest, AllDesksWindowCycling) {
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());

  // Create 1st Snap Group on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SnapGroup* snap_group1 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group1);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  // Create 2nd Snap Group on `desk1`.
  ActivateDesk(desk1);
  ASSERT_TRUE(desk1->is_active());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SnapGroup* snap_group2 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group2);
  ASSERT_EQ(desks_util::GetDeskForContext(w2.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(w3.get()), desk1);

  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));

  // Initial window cycle UI for "All desks" is as follows:
  //
  //        desk1           |        desk0
  //                        |
  //     +------+------+    |    +------+------+
  //     |      |      |    |    |      |      |
  //     |  w2  | [w3] |    |    |  w0  |  w1  |
  //     |      |      |    |    |      |      |
  //     +------+------+    |    +------+------+

  // Press 'Alt + Tab' 3 times to activate `w0`(cycling sequence:
  // `w2`->`w3`->`w0`). Verify that the active desk is switched to `desk0`
  // correspondingly.
  AltTabNTimes(3);

  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
  DeskSwitchAnimationWaiter().Wait();
  EXPECT_TRUE(desk0->is_active());

  // Updated window cycle UI for "All desks" is as follows:
  //
  //        desk0           |        desk1
  //                        |
  //     +------+------+    |    +------+------+
  //     |      |      |    |    |      |      |
  //     | [w0] |  w1  |    |    |  w2  |  w3  |
  //     |      |      |    |    |      |      |
  //     +------+------+    |    +------+------+

  // Press 'Alt + Tab' 2 times to activate `w2`(cycling sequence: `w1`->`w2`).
  // Verify that the active desk is switched to `desk1` correspondingly.
  AltTabNTimes(2);

  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  DeskSwitchAnimationWaiter().Wait();
  EXPECT_TRUE(desk1->is_active());
}

// Verifies that window cycling through Snap Groups on multiple virtual desks
// only activates windows present on the currently active desk. Windows on
// inactive desktops will not be cycled through or brought into focus.
TEST_F(SnapGroupWindowCycleTest, PerDeskWindowCycling) {
  PrefService* active_user_prefs =
      Shell::Get()->session_controller()->GetActivePrefService();
  DCHECK(active_user_prefs);
  active_user_prefs->SetBoolean(prefs::kAltTabPerDesk, true);

  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());

  // Create 1st Snap Group on `desk0`.
  std::unique_ptr<aura::Window> w0(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true, event_generator);
  SnapGroup* snap_group1 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group1);
  ASSERT_EQ(desks_util::GetDeskForContext(w0.get()), desk0);
  ASSERT_EQ(desks_util::GetDeskForContext(w1.get()), desk0);

  // Create 2nd Snap Group on `desk1`.
  ActivateDesk(desk1);
  ASSERT_TRUE(desk1->is_active());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true, event_generator);
  SnapGroup* snap_group2 =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w0.get());
  ASSERT_TRUE(snap_group2);
  ASSERT_EQ(desks_util::GetDeskForContext(w2.get()), desk1);
  ASSERT_EQ(desks_util::GetDeskForContext(w3.get()), desk1);

  EXPECT_TRUE(wm::IsActiveWindow(w3.get()));

  // Tests per-desk focus cycling on `desk1`.
  // Press 'Alt + Tab' 3 times to activate `w2`(cycling sequence:
  // `w2`->`w3`->`w2`). Verify that the active desk will not be modified.
  AltTabNTimes(3);

  EXPECT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_FALSE(desks_controller->AreDesksBeingModified());

  // Tests per-desk focus cycling on `desk0`.
  ActivateDesk(desk0);
  ASSERT_TRUE(desk0->is_active());

  // Press 'Alt + Tab' 5 times to activate `w2`(cycling sequence:
  // `w0`->`w1`->`w0`->`w1`->`w0`). Verify that the active desk will not be
  // modified.
  AltTabNTimes(5);

  EXPECT_TRUE(wm::IsActiveWindow(w0.get()));
  EXPECT_FALSE(desks_controller->AreDesksBeingModified());
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
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/true,
                     GetEventGenerator());

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
  SnapTwoTestWindows(window0.get(), window1.get(), /*horizontal=*/false,
                     GetEventGenerator());

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

  SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, GetEventGenerator());
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
      GetTopmostSnapGroupDivider()->divider_widget()->GetWindowBoundsInScreen();
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
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

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
  SnapTwoTestWindows(w0.get(), w1.get(), /*horizontal=*/true,
                     GetEventGenerator());

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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  SwitchToTabletMode();

  // Close w2. Test that the group is destroyed but we are still in split view.
  w2.reset();
  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w2.get()));
  EXPECT_EQ(GetSplitViewController()->primary_window(), w1.get());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
}

// Tests that one snap group in clamshell will be converted to windows in tablet
// split view. When converted back to clamshell, the snap group will be
// restored.
TEST_F(SnapGroupTabletConversionTest,
       ClamshellTabletTransitionWithOneSnapGroup) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  SnapTwoTestWindows(window1.get(), window2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  EXPECT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
  UnionBoundsEqualToWorkAreaBounds(window1.get(), window2.get(),
                                   GetTopmostSnapGroupDivider());

  SwitchToTabletMode();
  EXPECT_FALSE(GetTopmostSnapGroupDivider());
  EXPECT_TRUE(GetSplitViewDivider()->divider_widget());
  // The snap group is removed in tablet mode.
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_FALSE(
      snap_group_controller->GetSnapGroupForGivenWindow(window1.get()));
  EXPECT_EQ(window1.get(), GetSplitViewController()->primary_window());
  EXPECT_EQ(window2.get(), GetSplitViewController()->secondary_window());
  UnionBoundsEqualToWorkAreaBounds(window1.get(), window2.get(),
                                   GetSplitViewDivider());
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
                                   GetTopmostSnapGroupDivider());
  EXPECT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
}

// Tests that when converting to tablet mode with split view divider at an
// arbitrary location, the bounds of the two windows and the divider will be
// updated such that the snap ratio of the layout is one of the fixed snap
// ratios.
TEST_F(SnapGroupTabletConversionTest,
       ClamshellTabletTransitionGetClosestFixedRatio) {
  UpdateDisplay("900x600");
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(0));
  window1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(1));
  window2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(window1.get(), window2.get(), /*horizontal=*/true,
                     event_generator);
  ASSERT_TRUE(GetTopmostSnapGroupDivider()->divider_widget());
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

  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          GetSplitViewController()->root_window()->GetChildById(
              desks_util::GetActiveDeskContainerId()));
  for (const auto test_case : kTestCases) {
    event_generator->set_current_screen_location(
        GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
    event_generator->DragMouseBy(test_case.distance_delta, 0);
    GetTopmostSnapGroupDivider()->EndResizeWithDivider(
        event_generator->current_screen_location());
    SwitchToTabletMode();
    EXPECT_TRUE(GetSplitViewDivider() && !GetTopmostSnapGroupDivider());
    const auto current_divider_position =
        GetSplitViewDividerBoundsInScreen().x();

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

// Verify that entering tablet mode with Snap Group in Overview results in the
// Snap Group being represented by two separate items in Overview. This change
// should be maintained when switching back to clamshell mode. See
// http://b/343803517 for more details.
TEST_F(SnapGroupTabletConversionTest, TransitionToTabletInOverview) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  // Verify that there is one `OverviewGroupItem` initially in clamshell mode.
  EXPECT_EQ(1u, overview_grid->item_list().size());

  // Upon switching to tablet mode, the `OverviewGroupItem` is removed and
  // replaced with two separate items, there is no overlap in their bounds.
  SwitchToTabletMode();
  EXPECT_EQ(2u, overview_grid->item_list().size());

  OverviewItemBase* overview_item1 = GetOverviewItemForWindow(w1.get());
  OverviewItemBase* overview_item2 = GetOverviewItemForWindow(w2.get());
  EXPECT_NE(overview_item1, overview_item2);
  const gfx::RectF overview_item1_bounds_tablet =
      overview_item1->target_bounds();
  const gfx::RectF overview_item2_bounds_tablet =
      overview_item2->target_bounds();
  EXPECT_FALSE(
      overview_item1_bounds_tablet.Intersects(overview_item2_bounds_tablet));
  EXPECT_FALSE(
      overview_item1_bounds_tablet.Contains(overview_item2_bounds_tablet));
  EXPECT_FALSE(
      overview_item2_bounds_tablet.Contains(overview_item1_bounds_tablet));

  // After returning to clamshell mode, the two individual items should remain
  // distinctly separate within the Overview grid, with no intersection of their
  // bounds.
  ExitTabletMode();
  EXPECT_EQ(2u, overview_grid->item_list().size());

  overview_item1 = GetOverviewItemForWindow(w1.get());
  overview_item2 = GetOverviewItemForWindow(w2.get());
  EXPECT_NE(overview_item1, overview_item2);
  const gfx::RectF overview_item1_bounds_clamshell =
      overview_item1->target_bounds();
  const gfx::RectF overview_item2_bounds_clamshell =
      overview_item2->target_bounds();
  EXPECT_FALSE(overview_item1_bounds_clamshell.Intersects(
      overview_item2_bounds_clamshell));
  EXPECT_FALSE(overview_item1_bounds_clamshell.Contains(
      overview_item2_bounds_clamshell));
  EXPECT_FALSE(overview_item2_bounds_clamshell.Contains(
      overview_item1_bounds_clamshell));
}

// Tests that transitioning to tablet mode while an `OverviewGroupItem` is in
// partial Overview mode does not result in a crash. Regression test for crash
// stack trace reported in http://b/346617565#comment20.
TEST_F(SnapGroupTabletConversionTest, TransitionToTabletInPartialOverview) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // Create `w3` to partially occlude primary snapped `w1`.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 200, 200, 200)));
  std::unique_ptr<aura::Window> w4(CreateAppWindow());

  // Snap `w4` to secondary to start the partial Overview.
  //                                |
  //                                |-------+
  //     +----+    +------+------+  |       |
  //     | w3 |    |  w1  |  w2  |  |   w4  |
  //     |    |    |      |      |  |       |
  //     +----+    +------+------+  |       |
  //                                |-------+
  //                                |

  SnapOneTestWindow(w4.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio);
  VerifySplitViewOverviewSession(w4.get());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Verify that the the `OverviewGroupItem` is removed and replaced with two
  // separate items after converting to tablet mode.
  SwitchToTabletMode();
  EXPECT_EQ(3u, overview_grid->item_list().size());
}

// -----------------------------------------------------------------------------
// SnapGroupMultipleSnapGroupsTest:

using SnapGroupMultipleSnapGroupsTest = SnapGroupTest;

// Tests the basic functionalities of multiple snap groups.
TEST_F(SnapGroupMultipleSnapGroupsTest, MultipleSnapGroups) {
  // Create the 1st snap group.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  SnapOneTestWindow(w4.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio);
  ClickOverviewItem(GetEventGenerator(), w5.get());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w4.get(), w5.get()));
  auto* snap_group2 =
      snap_group_controller->GetSnapGroupForGivenWindow(w4.get());
  auto* snap_group_divider2 = snap_group2->snap_group_divider();
  EXPECT_EQ(2u, snap_group_controller->snap_groups_for_testing().size());
  EXPECT_NE(snap_group_divider1, snap_group_divider2);
  aura::Window* divider1_window = snap_group_divider1->GetDividerWindow();
  aura::Window* divider2_window = snap_group_divider2->GetDividerWindow();

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
  SnapTwoTestWindows(w4.get(), w5.get(), /*horizontal=*/true,
                     GetEventGenerator());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w4.get(), w5.get()));
  auto* snap_group2 =
      snap_group_controller->GetSnapGroupForGivenWindow(w4.get());
  auto* snap_group_divider2 = snap_group2->snap_group_divider();
  EXPECT_EQ(GetWorkAreaBounds().width() * chromeos::kDefaultSnapRatio -
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
  aura::Window* divider1 = snap_group_divider1->GetDividerWindow();
  aura::Window* divider2 = snap_group_divider2->GetDividerWindow();
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

// In the faster split screen setup, selecting an individual window within an
// existing Snap Group forms a **new** Snap Group combining the already snapped
// window (from the partial overview) and the newly selected window.
TEST_F(SnapGroupMultipleSnapGroupsTest,
       SelectWindowInSnapGroupInFasterPartialOverview) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  // Create `w3` to partially occlude primary snapped `w1`.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 200, 200, 200)));
  std::unique_ptr<aura::Window> w4(CreateAppWindow());

  // Round #1: Test that selecting a snapped window doesn't change its snap
  // position (snap state) to fill the opposite snapped area case.
  // Snap `w4` to secondary to start the partial Overview and verify that the
  // snap group is visible;
  //                                |
  //                                |-------+
  //     +----+    +------+------+  |       |
  //     | w3 |    |  w1  |  w2  |  |   w4  |
  //     |    |    |      |      |  |       |
  //     +----+    +------+------+  |       |
  //                                |-------+
  //                                |

  SnapOneTestWindow(w4.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio);
  ASSERT_TRUE(IsInOverviewSession());
  VerifySplitViewOverviewSession(w4.get());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Select `w1` in the Snap Group, which remains as primary snapped.
  ActivateWindowInOverviewGroupItem(w1.get(), event_generator,
                                    /*by_touch_gestures=*/false);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  ASSERT_FALSE(IsInOverviewSession());
  VerifyNotSplitViewOrOverviewSession(w1.get());

  SnapGroupController* snap_group_controller = SnapGroupController::Get();

  // A new Snap Group comprising windows `w1` and `w4` will be created.
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w4.get()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w4.get(),
                                   GetTopmostSnapGroupDivider());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Round #2: Test that selecting a snapped window that changes its snap
  // position (snap state) to fill the opposite snapped area case.
  wm::ActivateWindow(w3.get());
  // Set bounds on `w3` to partially occlude secondary snapped window `w2`.
  w3->SetBounds(gfx::Rect(500, 200, 100, 200));
  wm::ActivateWindow(w2.get());

  // Snap `w2` to primary to start the partial Overview and verify that the
  // snap group is visible;
  //
  //               |
  //               |
  //     +---------+
  //     |         | +----+    +------+------+
  //     |   w2    | | w3 |    |  w1  |  w4  |
  //     |         | |    |    |      |      |
  //     |         | +----+    +------+------+
  //     +---------+
  //               |
  //               |
  SnapOneTestWindow(w2.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  ASSERT_TRUE(IsInOverviewSession());
  VerifySplitViewOverviewSession(w2.get());

  overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());
  EXPECT_TRUE(w2->IsVisible());
  EXPECT_TRUE(w4->IsVisible());

  // Select `w1` in the Snap Group, which changes its snapped state from primary
  // snapped to secondary snapped.
  ActivateWindowInOverviewGroupItem(w1.get(), event_generator,
                                    /*by_touch_gestures=*/false);
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  ASSERT_FALSE(IsInOverviewSession());
  VerifyNotSplitViewOrOverviewSession(w2.get());

  // A new Snap Group comprising windows `w2` and `w1` will be created.
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w1.get()));
  UnionBoundsEqualToWorkAreaBounds(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
}

//  In the Overview, after manually setting up a partial overview by dragging to
//  snap a window, selecting a window from an existing Snap Group creates a
//  **new** Snap Group. This new group combines the selected window in the
//  previous Snap Group with the window already snapped in the partial overview.
TEST_F(SnapGroupMultipleSnapGroupsTest,
       SelectWindowInSnapGroupInManualPartialOverview) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 200, 200, 200)));

  // Start full Overview and verify that snap group will show.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Round #1: Test that selecting a snapped window that doesn't change its snap
  // position (snap state) to fill the opposite snapped area case.
  // Drag to snap `w3` to primary position to start the partial Overview.
  //
  //               |
  //               |
  //     +---------+
  //     |         |    +------+------+
  //     |   w3    |    |  w1  |  w2  |
  //     |         |    |      |      |
  //     |         |    +------+------+
  //     +---------+
  //               |
  //               |

  DragItemToPoint(GetOverviewItemForWindow(w3.get()), gfx::Point(0, 300),
                  event_generator, /*by_touch_gestures=*/false, /*drop=*/true);
  ASSERT_TRUE(IsInOverviewSession());
  VerifySplitViewOverviewSession(w3.get());

  overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(1u, overview_grid->item_list().size());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Select `w2` in the Snap Group, which remains as secondary snapped.
  ActivateWindowInOverviewGroupItem(w2.get(), event_generator,
                                    /*by_touch_gestures=*/false);
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  ASSERT_FALSE(IsInOverviewSession());
  VerifyNotSplitViewOrOverviewSession(w3.get());

  // A new Snap Group comprising windows `w3` and `w2` will be created.
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Round #2: Test that selecting a snapped window that changes its snap
  // position (snap state) to fill the opposite snapped area case.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Drag to snap `w1` to secondary position to start the partial Overview.
  //                        |
  //                        |---------+
  //     +------+------+    |         |
  //     |  w3  |  w2  |    |   w1    |
  //     |      |      |    |         |
  //     +------+------+    |         |
  //                        |---------+
  //                        |

  DragItemToPoint(GetOverviewItemForWindow(w1.get()), gfx::Point(800, 300),
                  event_generator, /*by_touch_gestures=*/false, /*drop=*/true);
  ASSERT_TRUE(IsInOverviewSession());
  VerifySplitViewOverviewSession(w1.get());

  overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(1u, overview_grid->item_list().size());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  // Select `w2` in the Snap Group, which changes its snapped state from
  // secondary snapped to primary snapped.
  ActivateWindowInOverviewGroupItem(w2.get(), event_generator,
                                    /*by_touch_gestures=*/false);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  ASSERT_FALSE(IsInOverviewSession());
  VerifyNotSplitViewOrOverviewSession(w1.get());

  // A new Snap Group comprising windows `w2` and `w1` will be created.
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w1.get()));
  UnionBoundsEqualToWorkAreaBounds(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
}

TEST_F(SnapGroupMultipleSnapGroupsTest,
       NoCrashWhenLongTappingOnGroupItemInPartialOverview) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  // Create `w3` to partially occlude primary snapped `w1`.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(200, 200, 200, 200)));
  std::unique_ptr<aura::Window> w4(CreateAppWindow());

  //                                |
  //                                |-------+
  //     +----+    +------+------+  |       |
  //     | w3 |    |  w1  |  w2  |  |   w4  |
  //     |    |    |      |      |  |       |
  //     +----+    +------+------+  |       |
  //                                |-------+
  //                                |

  SnapOneTestWindow(w4.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio);
  OverviewSession* overview_session =
      OverviewController::Get()->overview_session();
  CHECK(overview_session);
  VerifySplitViewOverviewSession(w4.get());

  auto* root_window = Shell::GetPrimaryRootWindow();
  const auto* overview_grid = GetOverviewGridForRoot(root_window);
  ASSERT_TRUE(overview_grid);
  EXPECT_EQ(2u, overview_grid->item_list().size());
  EXPECT_TRUE(w1->IsVisible());
  EXPECT_TRUE(w2->IsVisible());

  OverviewItemBase* group_item = GetOverviewItemForWindow(w1.get());
  gfx::Point location =
      gfx::ToRoundedPoint(group_item->target_bounds().CenterPoint());
  // Offset the location from the center a bit since the event is not be handled
  // in the center gap of the `OverviewGroupItem` yet.
  location.Offset(/*delta_x=*/10, /*delta_y=*/0);
  event_generator->set_current_screen_location(location);

  overview_session->InitiateDrag(group_item, gfx::PointF(location),
                                 /*is_touch_dragging=*/true, group_item);
  LongTapAt(event_generator, location);

  base::RunLoop().RunUntilIdle();
}

// -----------------------------------------------------------------------------
// SnapGroupSnapToReplaceTest:

using SnapGroupSnapToReplaceTest = SnapGroupTest;

// Tests that when dragging a window to 'snap replace' a visible window in a
// snap group, the original window is replaced and a new snap group is created.
TEST_F(SnapGroupSnapToReplaceTest, SnapToReplaceBasic) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  ASSERT_FALSE(GetSplitViewController()->InSplitViewMode());
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
  EXPECT_FALSE(GetSplitViewController()->InSplitViewMode());
}

// Tests if the window being snapped to replace has a min size which is smaller
// than size calculated from the target snap ratio, it will be snapped at its
// min size instead.
TEST_F(SnapGroupSnapToReplaceTest, WindowWithMinimumSize) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Create a new `w3` with min size > 0.5f.
  const gfx::Size min_size(GetWorkAreaBounds().width() * 0.6f, 0);
  std::unique_ptr<aura::Window> w3 = CreateAppWindowWithMinSize(min_size);

  // Snap to replace `w3` on top of the group.
  event_generator->set_current_screen_location(GetDragPoint(w3.get()));
  event_generator->DragMouseTo(0, 100);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w3.get())->GetStateType());

  // Test `w3` snaps at its min size and the group bounds are updated.
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(min_size.width(), w3->GetBoundsInScreen().width());
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Test that if both the window being snapped to replace and the opposite window
// in the group to replace have min sizes that can't fit, we don't allow snap to
// replace.
TEST_F(SnapGroupSnapToReplaceTest, BothWindowsMinimumSizes) {
  // Create a snap group where `w2` has min size 0.45f.
  const int work_area_length = GetWorkAreaBounds().width();
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(
      CreateAppWindowWithMinSize(gfx::Size(work_area_length * 0.45f, 0)));
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Create `w3` with min size 0.6f.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindowWithMinSize(gfx::Size(work_area_length * 0.6f, 0)));

  // Snap to replace `w3` over `w1`. Since `w3` and `w2` would not fit, test we
  // don't replace `w1` in the group.
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
// layout menu, it also checks the snap ratio threshold, the previous snap
// group's layout will be preserved.
TEST_F(SnapGroupSnapToReplaceTest,
       SnapToReplaceWithNonWindowLayoutSnapActionSource) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  const gfx::Rect w1_bounds(w1->GetBoundsInScreen());
  const gfx::Rect w2_bounds(w2->GetBoundsInScreen());

  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  const float w3_snap_ratio = 0.15f;
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped, w3_snap_ratio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);

  // Since the gap between `w3` and the opposite snapped `w2` exceeds the
  // threshold, we do not snap to replace.
  ASSERT_GT(std::abs(1.f - *WindowState::Get(w3.get())->snap_ratio() -
                     *WindowState::Get(w2.get())->snap_ratio()),
            kSnapToReplaceRatioDiffThreshold);
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(w1_bounds, w1->GetBoundsInScreen());
  EXPECT_EQ(w2_bounds, w2->GetBoundsInScreen());
}

// Test that the snap ratio difference is calculated before snap-to-replace when
// snapping from window layout menu. If it's below the threshold, the
// snap-to-replace action will occur. If not, we'll directly snap on top.
TEST_F(SnapGroupSnapToReplaceTest, SnapToReplaceWithRatioMargin) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
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

  // Resize `w2` and `w3` so the snap ratio gap will exceed the threshold.
  ResizeDividerTo(event_generator, /*resize_point=*/gfx::Point(
                      GetWorkAreaBounds().width() * 0.8f,
                      GetWorkAreaBounds().CenterPoint().y()));

  // Snap the new window `w4` with `chromeos::kOneThirdSnapRatio` ratio. Since
  // the snap ratio gap between `w4` and the opposite snapped `w2` is
  // greater than `kSnapToReplaceRatioDiffThreshold`, we directly snap on top.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kOneThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_GT(std::abs(1.f - *WindowState::Get(w2.get())->snap_ratio() -
                     *WindowState::Get(w4.get())->snap_ratio()),
            kSnapToReplaceRatioDiffThreshold);
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w4.get()));
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w3.get()));
}

// Tests that when dragging another window to snap in Overview with the
// existence of snap group. The to-be-snapped window will not replace the window
// in the snap group. See http://b/333603509 for more details.
TEST_F(SnapGroupSnapToReplaceTest, DoNotSnapToReplaceSnapGroupInOverview) {
  std::unique_ptr<aura::Window> w0(
      CreateAppWindow(gfx::Rect(10, 10, 200, 100)));

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  ASSERT_FALSE(GetSplitViewController()->InSplitViewMode());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  auto* overview_item0 = GetOverviewItemForWindow(w0.get());
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

// Verify 'Search + Shift + G' replaces the window on the same side when a
// snapped window is stacked on a Snap Group.
TEST_F(SnapGroupSnapToReplaceTest, UseShortcutToGroupPerformSnapToReplace) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  // Create a snapped `w3` stacked above the Snap Group.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);

  // Press 'Search + Shift + G' to perform snap-to-replace i.e. replacing `w1`
  // in the Snap Group with `w3`.
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);

  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w2.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);

  // Press 'Search + Shift + G' to perform snap-to-replace again i.e. replacing
  // `w2` in the Snap Group with `w4`.
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w4.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());
  UnionBoundsEqualToWorkAreaBounds(w3.get(), w4.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests that we can perform snap-to-replace with a floated window.
TEST_F(SnapGroupSnapToReplaceTest, SnapToReplaceWithFloatedWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  ASSERT_FALSE(GetSplitViewController()->InSplitViewMode());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  std::unique_ptr<aura::Window> floated_window(CreateAppWindow());
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  SnapOneTestWindow(floated_window.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  ASSERT_FALSE(WindowState::Get(floated_window.get())->IsFloated());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(floated_window.get(),
                                                           w2.get()));
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Verify snap-to-replace is disallowed when attempting to snap an always-on-top
// window. See http://b/347356195 for more details.
TEST_F(SnapGroupSnapToReplaceTest, DisallowSnapToReplaceWithAlwaysOnTopWindow) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  ASSERT_FALSE(GetSplitViewController()->InSplitViewMode());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  std::unique_ptr<aura::Window> always_on_top_window(CreateAlwaysOnTopWindow());
  EXPECT_NE(w1->parent(), always_on_top_window->parent());
  SnapOneTestWindow(always_on_top_window.get(),
                    WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);

  EXPECT_FALSE(snap_group_controller->AreWindowsInSnapGroup(
      w1.get(), always_on_top_window.get()));
  EXPECT_FALSE(snap_group_controller->AreWindowsInSnapGroup(
      w2.get(), always_on_top_window.get()));
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// -----------------------------------------------------------------------------
// SnapGroupAutoSnapGroupTest:

using SnapGroupAutoSnapGroupTest = SnapGroupTest;

// Tests to verify that resnapping a window within a Snap Group to the same
// position but with a different snap ratio will update the existing group. See
// http://b/342230763 for more details.
TEST_F(SnapGroupAutoSnapGroupTest, ReSnapWindowWithDifferentSnapRatio) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  ASSERT_TRUE(GetTopmostSnapGroupDivider());

  // Re-snap `w2` to 1/3. Test we re-form the group with the divider at 2/3.
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  const int work_area_width(GetWorkAreaBounds().width());
  EXPECT_EQ(std::round(work_area_width * chromeos::kTwoThirdSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests that drag to snap will respect the opposite snapped window's snap
// ratio.
TEST_F(SnapGroupAutoSnapGroupTest, DragToSnap) {
  // Create a snap group at 2/3 and 1/3.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  auto* event_generator = GetEventGenerator();
  ClickOverviewItem(event_generator, w2.get());
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  const int work_area_width(GetWorkAreaBounds().width());
  EXPECT_EQ(std::round(work_area_width * chromeos::kTwoThirdSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Drag out to unsnap `w1`, then re-snap. Test we re-form a group with the
  // divider at 2/3 to keep the opposite snapped `w2` at 1/3.
  event_generator->MoveMouseTo(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().CenterPoint());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().left_center());
  ASSERT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(std::round(work_area_width * chromeos::kTwoThirdSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Drag out to unsnap `w2`, then re-snap. Test we re-form a group with the
  // divider at 2/3 to keep the opposite snapped `w2` at 1/3.
  event_generator->MoveMouseTo(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().CenterPoint());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().right_center());
  ASSERT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(std::round(work_area_width * chromeos::kTwoThirdSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests that drag out to unsnap, then drag back to snap without releasing the
// mouse will keep the group snap ratio. See bug in http://b/346624805.
TEST_F(SnapGroupAutoSnapGroupTest, DragToSnapWithoutReleasingMouse) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  const int work_area_width(GetWorkAreaBounds().width());
  EXPECT_EQ(std::round(work_area_width * chromeos::kDefaultSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Drag out to unsnap `w1` without releasing the left button.
  event_generator->MoveMouseTo(GetDragPoint(w1.get()));
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(GetWorkAreaBounds().CenterPoint());
  // At this point `w1` will look visually unsnapped but still have snapped
  // state.
  ASSERT_EQ(gfx::Rect(250, 252, 300, 300), w1->GetTargetBounds());
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  // Now snap `w1` back to primary then release. Test it snaps at 1/2 and
  // re-forms a group.
  event_generator->MoveMouseTo(GetWorkAreaBounds().left_center());
  event_generator->ReleaseLeftButton();
  ASSERT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(GetWorkAreaBounds().CenterPoint(),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests that resizing the snap group, then dragging to re-snap works correctly.
TEST_F(SnapGroupAutoSnapGroupTest, ResizeThenDragToSnap) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Resize to arbitrary locations, then drag to re-snap.
  for (const int resize_delta : {-30, 0, 15}) {
    SCOPED_TRACE(base::StringPrintf("Resize delta: %d", resize_delta));
    const gfx::Point divider_center(
        GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
    event_generator->MoveMouseTo(divider_center);
    event_generator->PressLeftButton();
    const gfx::Point resize_point(divider_center +
                                  gfx::Vector2d(resize_delta, 0));
    event_generator->MoveMouseTo(resize_point, /*count=*/2);
    EXPECT_EQ(resize_point,
              GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());
    event_generator->ReleaseLeftButton();

    // Drag out to unsnap, then re-snap. Test it snaps at approximately the same
    // ratio.
    event_generator->MoveMouseTo(GetDragPoint(w1.get()));
    event_generator->DragMouseTo(GetWorkAreaBounds().CenterPoint());
    ASSERT_FALSE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
    event_generator->DragMouseTo(GetWorkAreaBounds().left_center());
    ASSERT_TRUE(
        snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
    EXPECT_NEAR(resize_point.x(),
                GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x(),
                1);
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());
  }
}

// Tests re-snapping to different ratios via the window layout menu.
TEST_F(SnapGroupAutoSnapGroupTest, WindowLayoutMenu) {
  // Create a snap group at 2/3 and 1/3.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  auto* event_generator = GetEventGenerator();
  ClickOverviewItem(event_generator, w2.get());
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  const int work_area_width(GetWorkAreaBounds().width());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Re-snap `w1` to 1/2.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(work_area_width * chromeos::kDefaultSnapRatio,
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Re-snap `w1` to 2/3.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(std::round(work_area_width * chromeos::kTwoThirdSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Re-snap `w2` to 1/2.
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(std::round(work_area_width * chromeos::kDefaultSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Re-snap `w2` to 1/3.
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(std::round(work_area_width * chromeos::kTwoThirdSnapRatio),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests that even if we skip partial overview, a snap group is formed on window
// layout complete.
TEST_F(SnapGroupAutoSnapGroupTest, SkipPartialAndFormSnapGroup) {
  // Snap `w1` to 2/3, then skip partial overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(w1.get());
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  VerifyNotSplitViewOrOverviewSession(w1.get());

  // Drag to snap `w2` to the opposite side. Test we form a group.
  wm::ActivateWindow(w2.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(GetDragPoint(w2.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().right_center());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            WindowState::Get(w2.get())->GetStateType());
  auto* snap_group_controller = Shell::Get()->snap_group_controller();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Drag out to unsnap `w1`, then re-snap with the shortcut.
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().CenterPoint());
  EXPECT_EQ(WindowStateType::kNormal,
            WindowState::Get(w1.get())->GetStateType());
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_ALT_DOWN);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that when the gap between the snapped window and opposite snapped
// window exceeds the threshold, we do not auto group.
TEST_F(SnapGroupAutoSnapGroupTest, SnapRatioGapThreshold) {
  UpdateDisplay("1000x800");

  // Snap `w1`, then resize it to be < 1/3.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  auto* event_generator = GetEventGenerator();
  const gfx::Point resize_point1(w1->GetBoundsInScreen().right_center());
  event_generator->MoveMouseTo(resize_point1);
  event_generator->DragMouseTo(250, resize_point1.y());
  ASSERT_LT(*WindowState::Get(w1.get())->snap_ratio(),
            chromeos::kOneThirdSnapRatio);

  // Now snap `w2` to 1/3 secondary.
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  EXPECT_LT(*WindowState::Get(w1.get())->snap_ratio(),
            chromeos::kOneThirdSnapRatio);

  // Since the gap between `w1` and `w2` exceeds the threshold, we do not group.
  ASSERT_GT(GetSnapRatioGap(w1.get(), w2.get()),
            kSnapToReplaceRatioDiffThreshold);
  EXPECT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Tests that when the overlap between the snapped window and opposite snapped
// window exceeds the threshold, we do not auto group.
TEST_F(SnapGroupAutoSnapGroupTest, SnapRatioOverlapThreshold) {
  UpdateDisplay("1000x800");

  // Snap `w1` to secondary, then resize it to be > 2/3.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  const gfx::Point resize_point(w1->GetBoundsInScreen().left_center());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(resize_point);
  event_generator->DragMouseTo(100, resize_point.y());
  ASSERT_GT(*WindowState::Get(w1.get())->snap_ratio(),
            chromeos::kTwoThirdSnapRatio);

  // Now snap `w2` to 2/3 primary.
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w2.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);

  // Since the overlap of `w1` and `w2` exceeds the threshold, we do not group.
  ASSERT_GT(GetSnapRatioGap(w1.get(), w2.get()),
            kSnapToReplaceRatioDiffThreshold);
  EXPECT_FALSE(
      SnapGroupController::Get()->AreWindowsInSnapGroup(w1.get(), w2.get()));
}

// Verifies shelf rounded corners behavior in relation to Snap Group state.
// - By default (no Snap Group): Shelf corners are rounded.
// - Upon Snap Group creation (auto-group entry point): Corners become sharp.
// - Upon Snap Group break: Corners revert to default rounded state.
TEST_F(SnapGroupAutoSnapGroupTest, ShelfRoundedCornersInAutoGroupEntryPoint) {
  ShelfLayoutManager* shelf_layout_manager =
      AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
  ASSERT_EQ(ShelfBackgroundType::kDefaultBg,
            shelf_layout_manager->shelf_background_type());

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);

  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kOneThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);

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
  ASSERT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Verify that Shelf restores its default background type with rounded
  // corners.
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            shelf_layout_manager->shelf_background_type());
}

// -----------------------------------------------------------------------------
// SnapGroupDisplayMetricsTest:

using SnapGroupDisplayMetricsTest = SnapGroupTest;

// Tests that snapped window and divider widget bounds scale dynamically with
// display changes, preserving their relative snap ratio.
TEST_F(SnapGroupDisplayMetricsTest, DisplayScaleChange) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  const float w1_snap_ratio = *WindowState::Get(w1.get())->snap_ratio();
  const float w2_snap_ratio = *WindowState::Get(w2.get())->snap_ratio();

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
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
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
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

  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Zoom in once. Test we update the group bounds.
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const int64_t primary_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const float zoom_factor_1 = 1.05f;
  ASSERT_EQ(zoom_factor_1,
            display_manager()->GetDisplayInfo(primary_id).zoom_factor());
  ASSERT_FALSE(w1->GetBoundsInScreen().Intersects(w2->GetBoundsInScreen()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

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
// exit. See http://b/339719019 and http://b/343803517 for more details.
TEST_F(SnapGroupDisplayMetricsTest, ScaleUpWorkAreaInOverview) {
  UpdateDisplay("800x600");

  // Set the min width so that the windows don't fit after zooming in using
  // keyboard shortcut.
  const gfx::Size min_size(395, 0);
  std::unique_ptr<aura::Window> w1(CreateAppWindowWithMinSize(min_size));
  std::unique_ptr<aura::Window> w2(CreateAppWindowWithMinSize(min_size));

  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group_controller = SnapGroupController::Get();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);

  // Zoom in to make the windows no longer fit, the Snap Group should be broken.
  PressAndReleaseKey(ui::VKEY_OEM_PLUS,
                     ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  const int64_t primary_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  ASSERT_EQ(1.05f, display_manager()->GetDisplayInfo(primary_id).zoom_factor());
  ASSERT_TRUE(GetUnionScreenBoundsForWindow(w1.get()).Intersects(
      GetUnionScreenBoundsForWindow(w2.get())));

  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_FALSE(GetTopmostSnapGroupDivider());
}

// Tests that there is no crash when work area changed after snapping two
// windows. Docked mananifier is used as an example to trigger the work area
// change.
TEST_F(SnapGroupDisplayMetricsTest, DockedMagnifier) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* docked_mangnifier_controller =
      Shell::Get()->docked_magnifier_controller();
  docked_mangnifier_controller->SetEnabled(/*enabled=*/true);
}

// Tests verifying virtual keyboard activation/deactivation which triggers work
// area change works properly with Snap Group.
TEST_F(SnapGroupDisplayMetricsTest, VirtualKeyboard) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  SetVirtualKeyboardEnabled(/*enabled=*/true);
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  keyboard_controller->HideKeyboardByUser();
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Tests verifying ChromeVox activation/deactivation which triggers work area
// change works properly with Snap Group.
TEST_F(SnapGroupDisplayMetricsTest, ChromeVox) {
  const gfx::Rect work_area_without_cvox(GetWorkAreaBounds());

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());

  // Enable ChromeVox panel.
  const int kAccessibilityPanelHeight = 45;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       nullptr, kShellWindowId_AccessibilityPanelContainer);
  SetAccessibilityPanelHeight(kAccessibilityPanelHeight);
  auto* a11y_controller = Shell::Get()->accessibility_controller();
  a11y_controller->spoken_feedback().SetEnabled(true);
  const gfx::Rect work_area_with_cvox(GetWorkAreaBounds());
  ASSERT_NE(work_area_without_cvox, work_area_with_cvox);
  EXPECT_TRUE(a11y_controller->spoken_feedback().enabled());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Disable ChromeVox panel.
  SetAccessibilityPanelHeight(0);
  a11y_controller->spoken_feedback().SetEnabled(false);
  ASSERT_EQ(work_area_without_cvox, GetWorkAreaBounds());
  EXPECT_FALSE(a11y_controller->spoken_feedback().enabled());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);

  // Verify that both windows and divider are visible on display #2.
  VerifySnapGroupOnDisplay(snap_group, displays[1].id());

  // Start resizing to the left.
  auto* snap_group_divider = snap_group->snap_group_divider();
  const gfx::Point divider_point(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
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
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
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
    w1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
    delegate1.set_minimum_size(window_minimum_size);
    aura::test::TestWindowDelegate delegate2;
    std::unique_ptr<aura::Window> w2(CreateTestWindowInShellWithDelegate(
        &delegate2, /*id=*/-1, gfx::Rect(window_x_origin + 500, 0, 800, 600)));
    w2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
    delegate2.set_minimum_size(window_minimum_size);

    SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                      chromeos::kTwoThirdSnapRatio);
    WaitForOverviewEntered();

    ClickOverviewItem(GetEventGenerator(), w2.get());
    EXPECT_EQ(chromeos::WindowStateType::kSecondarySnapped,
              WindowState::Get(w2.get())->GetStateType());
    WaitForOverviewExitAnimation();

    EXPECT_TRUE(GetTopmostSnapGroupDivider());
    UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                     GetTopmostSnapGroupDivider());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  const gfx::Point point_in_display2(802, 0);
  ASSERT_FALSE(displays[0].bounds().Contains(point_in_display2));
  ASSERT_TRUE(displays[1].bounds().Contains(point_in_display2));

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
  w1->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  std::unique_ptr<aura::Window> w2(
      CreateTestWindowInShellWithBounds(gfx::Rect(0, 0, 100, 100)));
  w2->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::CHROME_APP);
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
  aura::Window* divider_window = snap_group_divider->GetDividerWindow();
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* divider = GetTopmostSnapGroupDivider();
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

    SendKeyUntilOverviewItemIsFocused(ui::VKEY_TAB, GetEventGenerator());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  // Move Snap Group to display #2.
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  // Create Snap Group #2 on display #2.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(900, 0, 200, 100)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(1000, 50, 100, 200)));
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true, event_generator);
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w3.get()),
      displays[1].id());

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kOverviewButton);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  // Move Snap Group #2 to display #1 and move Snap Group #1 to display #2.
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
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
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[1].id());
}

// Tests if a `SnapGroup` is created on the external display, desk change with
// will not move the `SnapGroup` to the internal display.
TEST_F(SnapGroupMultiDisplayTest, DeskChangeWithMultiDisplay) {
  UpdateDisplay("800x700,801+0-800x700");

  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());

  // Create Snap Group on display #2.
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(900, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(1000, 50, 100, 200)));
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[1].id());
  display::Screen* screen = display::Screen::GetScreen();
  ASSERT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w1.get()).id());
  ASSERT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w2.get()).id());

  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  const Desk* desk0 = desks_controller->GetDeskAtIndex(0);
  const Desk* desk1 = desks_controller->GetDeskAtIndex(1);
  ASSERT_TRUE(desk0->is_active());

  // Use `Search + ]` to switch to `desk1`.
  PressAndReleaseKey(ui::VKEY_OEM_6, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();
  ASSERT_TRUE(desk1->is_active());

  // Use `Search + [` to switch back to `desk0`.
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_COMMAND_DOWN);
  DeskSwitchAnimationWaiter().Wait();
  ASSERT_TRUE(desk0->is_active());

  // The snap group remains on display #2 after desk switches.
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(displays[1].id(), screen->GetDisplayNearestWindow(w2.get()).id());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  auto* group1 = snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  VerifySnapGroupOnDisplay(group1, primary_id);

  // Create Snap Group #2 on display #2.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(900, 0, 200, 100)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(1000, 50, 100, 200)));
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true, event_generator);
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

// Tests that toggling mirror mode with a Snap Group on external display doesn't
// result in crash. Regression test for http://b/358539486.
TEST_F(SnapGroupMultiDisplayTest, ToggleMirrorMode) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());

  const gfx::Point point_in_display2(1000, 100);
  EXPECT_FALSE(displays[0].bounds().Contains(point_in_display2));
  EXPECT_TRUE(displays[1].bounds().Contains(point_in_display2));

  // Create Snap Group on display #2.
  std::unique_ptr<aura::Window> w1(
      CreateAppWindow(gfx::Rect(1000, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(1050, 50, 100, 200)));
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  SnapGroup* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  VerifySnapGroupOnDisplay(snap_group, displays[1].id());

  // Enable mirror mode and there should be no crash.
  display_manager->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
  ASSERT_EQ(1U, displays.size());
  VerifySnapGroupOnDisplay(snap_group, displays[0].id());
  base::RunLoop().RunUntilIdle();

  // Disable mirror mode and there should be no crash.
  display_manager->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  ASSERT_EQ(2U, displays.size());
  VerifySnapGroupOnDisplay(snap_group, displays[1].id());
  base::RunLoop().RunUntilIdle();
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
  EXPECT_EQ(divider_bounds, GetTopmostSnapGroupDividerBoundsInScreen());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  auto* group1 = snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  VerifySnapGroupOnDisplay(group1, primary_id);

  // Create Snap Group #2 on display #2.
  std::unique_ptr<aura::Window> w3(
      CreateAppWindow(gfx::Rect(801, 0, 200, 100)));
  std::unique_ptr<aura::Window> w4(
      CreateAppWindow(gfx::Rect(810, 50, 100, 200)));
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true, event_generator);
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

// Tests no overlap in the divider and window bounds after disconnecting and
// reconnecting the primary display.
TEST_F(SnapGroupMultiDisplayTest, AddRemovePrimaryDisplayAfterResize) {
  UpdateDisplay("1200x900,0+901-1200x900/u");
  ASSERT_EQ(2U, display_manager()->active_display_list().size());

  // Create Snap Group #1 on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  auto* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);

  // Resize via the divider to an arbitrary point.
  auto* snap_group_divider = snap_group->snap_group_divider();
  const gfx::Point divider_point(
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  event_generator->set_current_screen_location(divider_point);
  event_generator->PressLeftButton();
  const gfx::Point resize_point(350, divider_point.y());
  event_generator->MoveMouseTo(resize_point, /*count=*/22);
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(resize_point.x(),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);

  // Disconnect the primary display.
  UpdateDisplay("1200x900/u");
  ASSERT_EQ(1U, display_manager()->active_display_list().size());
  UnionBoundsEqualToWorkAreaBounds(w2.get(), w1.get(), snap_group_divider);

  // Reconnect the primary display.
  UpdateDisplay("1200x900,0+901-1200x900/u");
  ASSERT_EQ(2U, display_manager()->active_display_list().size());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
}

// Tests that resizing via the cursor between displays works correctly.
TEST_F(SnapGroupMultiDisplayTest, ResizeCursorBetweenDisplays) {
  UpdateDisplay("800x700,801+0-800x700");
  const int min_width = 300;
  std::unique_ptr<aura::Window> w1(
      CreateAppWindowWithMinSize(gfx::Size(min_width, 0)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindowWithMinSize(gfx::Size(min_width, 0)));
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());
  auto* snap_group_divider = snap_group->snap_group_divider();

  // Press and move the mouse right, within `w1` and `w2` min widths. Test we
  // update bounds.
  const gfx::Point divider_point(
      snap_group_divider->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint());
  event_generator->set_current_screen_location(divider_point);
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(gfx::Point(350, divider_point.y()), /*count=*/2);
  ASSERT_TRUE(snap_group_divider->is_resizing_with_divider());
  EXPECT_EQ(350, GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
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
  EXPECT_EQ(350, GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint().x());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
  event_generator->ReleaseLeftButton();
}

// Verify that an Overview group item remains interactive after being dragged to
// a different display and back without releasing the mouse. Also verify the
// group item's widget consistently stays beneath the item widget of the
// individual windows it contains. See http://b/339088510 for more details.
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
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w1.get()));
  ASSERT_TRUE(overview_group_item);

  const auto& overview_items =
      overview_group_item->overview_items_for_testing();
  ASSERT_EQ(2u, overview_items.size());
  auto* group_item_widget = overview_group_item->item_widget();
  ASSERT_TRUE(group_item_widget);
  auto* group_item_widget_window = group_item_widget->GetNativeWindow();

  // Stacking order verification before drag.
  for (const auto& overview_item : overview_items) {
    EXPECT_TRUE(window_util::IsStackedBelow(
        group_item_widget_window,
        overview_item->item_widget()->GetNativeWindow()));
  }

  // Drag `overview_group_item` to display #2 w/o releasing mouse and drag back
  // then drop.
  DragGroupItemToPoint(overview_group_item, point_in_display2, event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/false);
  DragGroupItemToPoint(overview_group_item, point_in_display1, event_generator,
                       /*by_touch_gestures=*/false, /*drop=*/true);

  // Stacking order verification after drag.
  for (const auto& overview_item : overview_items) {
    EXPECT_TRUE(window_util::IsStackedBelow(
        group_item_widget_window,
        overview_item->item_widget()->GetNativeWindow()));
  }

  // Verify that Overview exits on mouse click (shift the click position by
  // `gfx::Vector2d(10, 0)` as there is a gap between the two individual items
  // and it is not handling event currently), and both windows remaining on the
  // display they were originally on.
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(overview_group_item->target_bounds().CenterPoint()) +
      gfx::Vector2d(10, 0));
  event_generator->ClickLeftButton();
  VerifyNotSplitViewOrOverviewSession(w1.get());

  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[0].id(), screen->GetDisplayNearestWindow(w1.get()).id());
  EXPECT_EQ(displays[0].id(), screen->GetDisplayNearestWindow(w2.get()).id());
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());
}

// Verify the following behavior when dragging an `OverviewGroupItem` to the new
// desk button on a different display:
// 1. The new desk button on the target display changes to
// `DeskIconButton::State::kActive`.
// 2. New desk buttons on other displays remain in
// `DeskIconButton::State::kExpanded`.
// 3. Upon dropping the OverviewItem, all new desk buttons (including the target
// display) are restored to `DeskIconButton::State::kExpanded` state.
TEST_F(SnapGroupMultiDisplayTest, NewDeskButtonStateUpdateOnMultiDisplay) {
  auto skip_scale_up_new_desk_button_duration = OverviewWindowDragController::
      SkipNewDeskButtonScaleUpDurationForTesting();

  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());

  const gfx::Point point_in_display1(502, 300);
  ASSERT_TRUE(displays[0].bounds().Contains(point_in_display1));
  ASSERT_FALSE(displays[1].bounds().Contains(point_in_display1));

  // Create Snap Group on display #1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> w2(
      CreateAppWindow(gfx::Rect(50, 50, 100, 200)));
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  VerifySnapGroupOnDisplay(
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get()),
      displays[0].id());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  ASSERT_TRUE(IsWindowInItsCorrespondingOverviewGrid(w1.get()));

  // Verify that the new desk buttons on both displays have
  // `DeskIconButton::State::kZero` state initially.
  const auto& grids = GetOverviewSession()->grid_list();
  ASSERT_EQ(2u, grids.size());
  auto* grid0 = grids[0].get();
  ASSERT_TRUE(grid0);
  auto* desks_bar_view0 = grid0->desks_bar_view();
  const DeskIconButton* new_desk_button0 = desks_bar_view0->new_desk_button();
  ASSERT_TRUE(new_desk_button0);
  ASSERT_TRUE(new_desk_button0->GetVisible());
  ASSERT_EQ(DeskIconButton::State::kZero, new_desk_button0->state());

  auto* grid1 = grids[1].get();
  ASSERT_TRUE(grid1);
  auto* desks_bar_view1 = grid1->desks_bar_view();
  const DeskIconButton* new_desk_button1 = desks_bar_view1->new_desk_button();
  ASSERT_TRUE(new_desk_button1);
  ASSERT_TRUE(new_desk_button1->GetVisible());
  ASSERT_EQ(DeskIconButton::State::kZero, new_desk_button1->state());

  OverviewItemBase* overview_group_item = GetOverviewItemForWindow(w1.get());
  ASSERT_TRUE(overview_group_item);

  // Drag the `overview_item` to new desk button on display #2 w/o releasing the
  // mouse. Verify that the new desk button on display #2 turns into
  // `DeskIconButton::State::kActive` state.
  DragGroupItemToPoint(
      overview_group_item, new_desk_button1->GetBoundsInScreen().CenterPoint(),
      event_generator, /*by_touch_gestures=*/false, /*drop=*/false);
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button0->state());
  EXPECT_EQ(DeskIconButton::State::kActive, new_desk_button1->state());

  // Drag the `overview_group_item` back to display #1 w/o and drop. Verify that
  // the new desk buttons on all displays are restored to
  // `DeskIconButton::State::kExpanded` state.
  DragItemToPoint(overview_group_item, point_in_display1, event_generator,
                  /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button0->state());
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button1->state());
}

// -----------------------------------------------------------------------------
// SnapGroupA11yTest:

using SnapGroupA11yTest = SnapGroupTest;

// Tests that the divider receives system pane focus.
TEST_F(SnapGroupA11yTest, DividerPaneFocus) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);

  auto* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);
  auto* snap_group_divider = snap_group->snap_group_divider();

  // Test initially the divider is not active and focus ring is hidden.
  auto* divider_widget = snap_group_divider->divider_widget();
  EXPECT_FALSE(divider_widget->IsActive());

  auto* focus_ring =
      views::FocusRing::Get(snap_group_divider->divider_view_for_testing());
  ASSERT_TRUE(focus_ring);
  EXPECT_FALSE(focus_ring->GetVisible());

  // Cycle Backward. Test the divider is active and focus ring is shown.
  event_generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(divider_widget->IsActive());
  EXPECT_TRUE(focus_ring->GetVisible());
  constexpr int kFocusRingPaddingDp = 8;
  EXPECT_TRUE(focus_ring->GetBoundsInScreen().ApproximatelyEqual(
      GetTopmostSnapGroupDividerBoundsInScreen(),
      /*tolerance=*/kFocusRingPaddingDp));

  // Cycle Backward. Test the divider loses active and focus ring is hidden.
  event_generator->PressKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(divider_widget->IsActive());
  EXPECT_FALSE(focus_ring->GetVisible());

  // Cycle Forward. Test the divider is active and focus ring is shown.
  event_generator->PressKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(divider_widget->IsActive());
  EXPECT_TRUE(focus_ring->GetVisible());
  EXPECT_TRUE(focus_ring->GetBoundsInScreen().ApproximatelyEqual(
      GetTopmostSnapGroupDividerBoundsInScreen(),
      /*tolerance=*/kFocusRingPaddingDp));
}

// Tests that the divider can be resized via the keyboard.
TEST_F(SnapGroupA11yTest, DividerResize) {
  TestAccessibilityControllerClient client;
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);
  auto* snap_group_divider = snap_group->snap_group_divider();

  auto* divider_widget = snap_group_divider->divider_widget();

  // Cycle focus to the divider.
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  ASSERT_TRUE(divider_widget->IsActive());
  gfx::Point divider_center =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();

  // Resize left.
  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_EQ(divider_center + gfx::Vector2d(-kSplitViewDividerResizeDistance, 0),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_LEFT,
            client.last_a11y_alert());

  // Resize right.
  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_EQ(divider_center,
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_RIGHT,
            client.last_a11y_alert());

  // 2. Test with horizontal secondary display.
  UpdateDisplay("800x600/u");
  divider_center = GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();

  // Resize left.
  PressAndReleaseKey(ui::VKEY_LEFT);
  EXPECT_EQ(divider_center + gfx::Vector2d(-kSplitViewDividerResizeDistance, 0),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w2.get(), w1.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_LEFT,
            client.last_a11y_alert());

  // Resize right.
  PressAndReleaseKey(ui::VKEY_RIGHT);
  EXPECT_EQ(divider_center,
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w2.get(), w1.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_RIGHT,
            client.last_a11y_alert());

  // 3. Test with vertical display.
  UpdateDisplay("600x800");
  divider_center = GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();

  // Resize up.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(divider_center + gfx::Vector2d(0, -kSplitViewDividerResizeDistance),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_UP, client.last_a11y_alert());

  // Resize down.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(divider_center,
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_DOWN,
            client.last_a11y_alert());

  // 4. Test with vertical secondary display.
  UpdateDisplay("600x800/u");
  divider_center = GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();

  // Resize up.
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(divider_center + gfx::Vector2d(0, -kSplitViewDividerResizeDistance),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w2.get(), w1.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_UP, client.last_a11y_alert());

  // Resize down.
  PressAndReleaseKey(ui::VKEY_DOWN);
  EXPECT_EQ(divider_center,
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w2.get(), w1.get(), snap_group_divider);
  EXPECT_EQ(AccessibilityAlert::SNAP_GROUP_RESIZE_DOWN,
            client.last_a11y_alert());
}

// Tests that resize vertically works with ChromeVox on.
TEST_F(SnapGroupA11yTest, ResizeVertical) {
  UpdateDisplay("600x800");
  const gfx::Rect work_area_without_cvox(GetWorkAreaBounds());

  // Simulate enabling ChromeVox.
  const int kAccessibilityPanelHeight = 45;
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       nullptr, kShellWindowId_AccessibilityPanelContainer);
  SetAccessibilityPanelHeight(kAccessibilityPanelHeight);
  Shell::Get()->accessibility_controller()->spoken_feedback().SetEnabled(true);
  const gfx::Rect work_area_with_cvox(GetWorkAreaBounds());
  ASSERT_NE(work_area_without_cvox, work_area_with_cvox);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/false,
                     GetEventGenerator());
  auto* snap_group =
      SnapGroupController::Get()->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);
  auto* snap_group_divider = snap_group->snap_group_divider();
  const gfx::Point divider_center =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();

  // Note that the divider widget bounds are in screen, but `divider_position`
  // is relative to the work area.
  ASSERT_EQ(work_area_with_cvox.CenterPoint(), divider_center);
  ASSERT_EQ(
      GetTopmostSnapGroupDividerBoundsInScreen().y() - work_area_with_cvox.y(),
      snap_group_divider->divider_position());

  // Cycle focus to the divider.
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK, ui::EF_CONTROL_DOWN);
  ASSERT_TRUE(snap_group_divider->divider_widget()->IsActive());

  // Resize up.
  ASSERT_EQ(GetWorkAreaBounds().CenterPoint(), divider_center);
  PressAndReleaseKey(ui::VKEY_UP);
  EXPECT_EQ(divider_center + gfx::Vector2d(0, -kSplitViewDividerResizeDistance),
            GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(), snap_group_divider);
}

// -----------------------------------------------------------------------------
// SnapGroupMetricsTest:

class SnapGroupMetricsTest : public SnapGroupTest {
 public:
  SnapGroupMetricsTest()
      : SnapGroupTest(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~SnapGroupMetricsTest() override = default;

  void AdvanceClock(base::TimeDelta delta) {
    task_environment()->AdvanceClock(delta);
    task_environment()->RunUntilIdle();
  }

 protected:
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

// Tests that the pipeline to get snap action source info all the way to be
// stored in the `SplitViewOverviewSession` is working. This test focuses on the
// snap action source with top-usage in clamshell.
TEST_F(SnapGroupMetricsTest, SnapActionSourcePipeline) {
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(200, 100)));

  // Drag a window to snap and verify the snap action source info.
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window1.get(), gfx::PointF(), HTCAPTION, wm::WINDOW_MOVE_SOURCE_MOUSE));
  resizer->Drag(gfx::PointF(0, 400), /*event_flags=*/0);
  resizer->CompleteDrag();
  resizer.reset();
  VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(GetSplitViewOverviewSession(window1.get())
                ->snap_action_source_for_testing(),
            WindowSnapActionSource::kDragWindowToEdgeToSnap);
  MaximizeToClearTheSession(window1.get());

  // Mock snap from window layout menu and verify the snap action source info.
  chromeos::SnapController::Get()->CommitSnap(
      window1.get(), chromeos::SnapDirection::kSecondary,
      chromeos::kDefaultSnapRatio,
      chromeos::SnapController::SnapRequestSource::kWindowLayoutMenu);
  VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(GetSplitViewOverviewSession(window1.get())
                ->snap_action_source_for_testing(),
            WindowSnapActionSource::kSnapByWindowLayoutMenu);
  MaximizeToClearTheSession(window1.get());

  // Mock snap from window snap button and verify the snap action source info.
  chromeos::SnapController::Get()->CommitSnap(
      window1.get(), chromeos::SnapDirection::kPrimary,
      chromeos::kDefaultSnapRatio,
      chromeos::SnapController::SnapRequestSource::kSnapButton);
  VerifySplitViewOverviewSession(window1.get());
  EXPECT_EQ(GetSplitViewOverviewSession(window1.get())
                ->snap_action_source_for_testing(),
            WindowSnapActionSource::kLongPressCaptionButtonToSnap);
  MaximizeToClearTheSession(window1.get());
}

// Verifies that the recorded duration of a snap group accurately reflects both
// its persistence and actual duration.
TEST_F(SnapGroupMetricsTest, SnapGroupDuration) {
  const std::string persistence_duration_histogram_name =
      BuildHistogramName(kSnapGroupPersistenceDurationRootWord);
  histogram_tester_.ExpectTotalCount(persistence_duration_histogram_name, 0);

  const std::string actual_duration_histogram_name =
      BuildHistogramName(kSnapGroupActualDurationRootWord);
  histogram_tester_.ExpectTotalCount(actual_duration_histogram_name, 0);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
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
      snap_group_controller->GetSnapGroupForGivenWindow(w2.get()),
      SnapGroupExitPoint::kDragWindowOut);
  histogram_tester_.ExpectBucketCount(persistence_duration_histogram_name,
                                      /*sample=*/20, /*expected_count=*/1);
  histogram_tester_.ExpectBucketCount(actual_duration_histogram_name,
                                      /*sample=*/10, /*expected_count=*/2);
}

// Verify `SnapGroupExitPoint` is correctly recorded across various Snap Group
// exit points (drag, window state change, destruction, tablet transition).
TEST_F(SnapGroupMetricsTest, SnapGroupExitPoint) {
  const std::string snap_group_exit_point =
      BuildHistogramName(kSnapGroupExitPointRootWord);
  histogram_tester_.ExpectTotalCount(snap_group_exit_point, 0);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  SCOPED_TRACE("Test case 1: drag window out to exit");
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
  SnapTwoTestWindows(w2.get(), w3.get(), /*horizontal=*/true, event_generator);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w3.get()));
  WindowState* w2_state = WindowState::Get(w2.get());
  w2_state->Maximize();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w2.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w3.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowStateChangedMaximized,
      1);
  MaximizeToClearTheSession(w2.get());

  SCOPED_TRACE("Test case 3: minimize window to exit");
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true, event_generator);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w3.get(), w4.get()));
  WindowState* w3_state = WindowState::Get(w3.get());
  w3_state->Minimize();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w3.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w4.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowStateChangedMinimized,
      1);
  MaximizeToClearTheSession(w4.get());

  SCOPED_TRACE("Test case 4: float window to exit");
  std::unique_ptr<aura::Window> w5(CreateAppWindow());
  SnapTwoTestWindows(w4.get(), w5.get(), /*horizontal=*/true, event_generator);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w4.get(), w5.get()));
  WindowState* w4_state = WindowState::Get(w4.get());
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  w4_state->OnWMEvent(&float_event);
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w4.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w5.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowStateChangedFloated, 1);
  MaximizeToClearTheSession(w5.get());

  SCOPED_TRACE("Test case 5: window destruction to exit");
  std::unique_ptr<aura::Window> w6(CreateAppWindow());
  SnapTwoTestWindows(w5.get(), w6.get(), /*horizontal=*/true, event_generator);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w5.get(), w6.get()));
  w5.reset();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w6.get()));
  histogram_tester_.ExpectBucketCount(
      snap_group_exit_point, SnapGroupExitPoint::kWindowDestruction, 1);

  SCOPED_TRACE("Test case 6: switch to tablet mode to exit");
  std::unique_ptr<aura::Window> w7(CreateAppWindow());
  SnapTwoTestWindows(w6.get(), w7.get(), /*horizontal=*/true, event_generator);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w6.get(), w7.get()));
  SwitchToTabletMode();
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w6.get()));
  EXPECT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w7.get()));
  histogram_tester_.ExpectBucketCount(snap_group_exit_point,
                                      SnapGroupExitPoint::kTabletTransition, 1);
}

TEST_F(SnapGroupMetricsTest, SnapGroupsCount) {
  UpdateDisplay("800x600");

  const std::string snap_groups_count_histogram =
      BuildHistogramName(kSnapGroupsCountRootWord);

  histogram_tester_.ExpectTotalCount(snap_groups_count_histogram, 0);

  // Create and test we record 1 group.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
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
  SnapTwoTestWindows(w3.get(), w4.get(), /*horizontal=*/true, event_generator);
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

  // At this point `w4` is active but a single snapped window. Recall the group
  // for `w1` and `w2` so we can start snap to replace.
  wm::ActivateWindow(w1.get());
  ASSERT_TRUE(
      snap_group_controller->GetTopmostVisibleSnapGroup(w1->GetRootWindow()));

  // Snap to replace `w5` in the 1st snap group. Test we don't record.
  std::unique_ptr<aura::Window> w5(CreateAppWindow());
  ASSERT_EQ(1u, snap_group_controller->snap_groups_for_testing().size());
  SnapOneTestWindow(w5.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_EQ(1u, snap_group_controller->snap_groups_for_testing().size());
  histogram_tester_.ExpectBucketCount(snap_groups_count_histogram,
                                      /*sample=*/1,
                                      /*expected_count=*/2);

  histogram_tester_.ExpectTotalCount(snap_groups_count_histogram, 3);
}

// Validate the accurate recording for 'Search + Shift + G' shortcut histogram.
TEST_F(SnapGroupMetricsTest, KeyboardshortcutToCreateSnapGroupHistogram) {
  const std::string histogram_name = "Ash.Accelerators.Actions.CreateSnapGroup";

  // Initially histogram is recorded as 0.
  histogram_tester_.ExpectTotalCount(histogram_name, 0);

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());

  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);

  // Press 'Search + Shift + G' to to group `w1` and `w2`.
  auto* event_generator = GetEventGenerator();
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);

  SnapGroupController* snap_group_controller =
      Shell::Get()->snap_group_controller();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w2.get(),
                                   GetTopmostSnapGroupDivider());

  // Verify that the histogram is recorded correctly.
  histogram_tester_.ExpectTotalCount(histogram_name, 1);

  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kKeyboardShortcutToSnap);

  // Press 'Search + Shift + G' to to perform snap-to-replace.
  event_generator->PressAndReleaseKey(ui::VKEY_G,
                                      ui::EF_SHIFT_DOWN | ui::EF_COMMAND_DOWN);
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w3.get()));
  EXPECT_TRUE(GetTopmostSnapGroupDivider());
  UnionBoundsEqualToWorkAreaBounds(w1.get(), w3.get(),
                                   GetTopmostSnapGroupDivider());

  // Validate histogram counter increments.
  histogram_tester_.ExpectTotalCount(histogram_name, 2);
}

TEST_F(SnapGroupMetricsTest, SnapGroupUserActions) {
  UpdateDisplay("800x600");

  // Add a snap group, which will incidentally start partial overview.
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(
      user_action_tester_.GetActionCount("SnapGroups_StartPartialOverview"), 1);
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_AddSnapGroup"), 1);

  // Snap to replace.
  std::unique_ptr<aura::Window> w3(CreateAppWindow());
  SnapOneTestWindow(w3.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w2.get(), w3.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_SnapToReplace"), 1);

  // Resize `w3` to be < 1/3 so the snap ratio gap exceeds the threshold.
  const gfx::Point resize_point(w3->GetBoundsInScreen().right_center());
  event_generator->MoveMouseTo(resize_point);
  event_generator->DragMouseTo(150, resize_point.y());

  // Snap via the window layout menu with a ratio >
  // `kSnapToReplaceRatioDiffThreshold` to directly snap on top.
  std::unique_ptr<aura::Window> w4(CreateAppWindow());
  SnapOneTestWindow(w4.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_GT(GetSnapRatioGap(w4.get(), w2.get()),
            kSnapToReplaceRatioDiffThreshold);
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w4.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_SnapDirect"), 1);

  // Remove the snap group.
  w3.reset();
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w3.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_RemoveSnapGroup"),
            1);
}

TEST_F(SnapGroupMetricsTest, RecallSnapGroupUserAction) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  ASSERT_TRUE(wm::IsActiveWindow(w2.get()));

  // Create a maximized window to occlude the snap group, then activate `w1` to
  // recall the group.
  std::unique_ptr<aura::Window> w3(CreateAppWindow(GetWorkAreaBounds()));
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_RecallSnapGroup"),
            1);

  // Enter overview, then click the overview group item to recall the group.
  ToggleOverview();
  ASSERT_TRUE(
      wm::IsActiveWindow(GetOverviewSession()->GetOverviewFocusWindow()));
  OverviewGroupItem* overview_group_item =
      static_cast<OverviewGroupItem*>(GetOverviewItemForWindow(w1.get()));
  ASSERT_TRUE(overview_group_item);
  GetOverviewSession()->SelectWindow(overview_group_item);
  ASSERT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_RecallSnapGroup"),
            2);

  // Activate `w3`, then window cycle to `w1` to recall the group.
  wm::ActivateWindow(w3.get());
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);
  CompleteWindowCycling();
  ASSERT_TRUE(wm::IsActiveWindow(w1.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_RecallSnapGroup"),
            3);

  // Window cycle to `w2`. Test we don't record since the other `w1` was
  // previously active.
  CycleWindow(WindowCyclingDirection::kForward, /*steps=*/1);
  CompleteWindowCycling();
  ASSERT_TRUE(wm::IsActiveWindow(w2.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_RecallSnapGroup"),
            3);
}

TEST_F(SnapGroupMetricsTest, SkipFormSnapGroupAfterSnapping) {
  UpdateDisplay("800x600");

  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  std::unique_ptr<aura::Window> w1(CreateAppWindow());

  // Snap using the keyboard shortcut won't record.
  PressAndReleaseKey(ui::VKEY_OEM_4, ui::EF_ALT_DOWN);
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_SkipFormSnapGroupAfterSnapping"),
            0);

  // Snap using an invalid snap action source won't record.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowStateRestore);
  VerifyNotSplitViewOrOverviewSession(w1.get());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_SkipFormSnapGroupAfterSnapping"),
            0);

  // Test that just skipping partial overview normally won't record.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  PressAndReleaseKey(ui::VKEY_ESCAPE, ui::EF_NONE);
  VerifyNotSplitViewOrOverviewSession(w1.get());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_SkipFormSnapGroupAfterSnapping"),
            0);

  // Selecting the 2nd window in partial overview won't record and will create a
  // snap group.
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());
  ClickOverviewItem(GetEventGenerator(), w2.get());
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_SkipFormSnapGroupAfterSnapping"),
            0);
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_AddSnapGroup"), 1);

  // Drag out `w1` from the group, which will break the group, then re-snap it.
  // This will create a new snap group.
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(GetDragPoint(w1.get()));
  event_generator->DragMouseTo(GetWorkAreaBounds().CenterPoint());
  ASSERT_FALSE(snap_group_controller->GetSnapGroupForGivenWindow(w1.get()));
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kDragWindowToEdgeToSnap);
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_EQ(user_action_tester_.GetActionCount("SnapGroups_AddSnapGroup"), 2);
}

// Verifies that the "double tap to swap windows" user action metrics are
// recorded accurately.
TEST_F(SnapGroupMetricsTest, DoubleTapDividerUserAction) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  auto* event_generator = GetEventGenerator();
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true, event_generator);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  SplitViewDivider* divider = GetTopmostSnapGroupDivider();
  auto* divider_widget = divider->divider_widget();
  ASSERT_TRUE(divider_widget);
  auto* divider_view = divider->divider_view_for_testing();
  ASSERT_TRUE(divider_view);
  auto* handler_view = divider_view->handler_view_for_testing();
  ASSERT_TRUE(handler_view);

  const auto divider_center_point =
      GetTopmostSnapGroupDividerBoundsInScreen().CenterPoint();
  event_generator->set_current_screen_location(divider_center_point);
  event_generator->DoubleClickLeftButton();
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Verify that the correct user action metrics are recorded after a
  // successful window swap triggered by double-click on the divider.
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_DoubleTapWindowSwapAttempts"),
            1);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_DoubleTapWindowSwapSuccess"),
            1);

  // Verify that after a successful window swap initiated by a double-tap on the
  // divider, the corresponding user action metrics are incremented.
  event_generator->GestureTapAt(divider_center_point);
  event_generator->GestureTapAt(divider_center_point);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_DoubleTapWindowSwapAttempts"),
            2);
  EXPECT_EQ(user_action_tester_.GetActionCount(
                "SnapGroups_DoubleTapWindowSwapSuccess"),
            2);
}

TEST_F(SnapGroupMetricsTest, GroupContainerCycleViewAccessibleProperties) {
  std::unique_ptr<aura::Window> w1(CreateAppWindow());
  std::unique_ptr<aura::Window> w2(CreateAppWindow());
  SnapTwoTestWindows(w1.get(), w2.get(), /*horizontal=*/true,
                     GetEventGenerator());
  auto* snap_group_controller = SnapGroupController::Get();
  auto* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(w1.get());
  ASSERT_TRUE(snap_group);
  std::unique_ptr<GroupContainerCycleView> cycle_view =
      std::make_unique<GroupContainerCycleView>(snap_group);
  ui::AXNodeData data;

  cycle_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGroup);
}

}  // namespace ash
