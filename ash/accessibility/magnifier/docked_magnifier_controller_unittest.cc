// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"

#include <memory>
#include <vector>

#include "ash/accessibility/magnifier/magnifier_test_utils.h"
#include "ash/capture_mode/capture_mode_controller.h"
#include "ash/capture_mode/capture_mode_metrics.h"
#include "ash/capture_mode/capture_mode_session.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/display/display_util.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

constexpr char kUser1Email[] = "user1@dockedmagnifier";
constexpr char kUser2Email[] = "user2@dockedmagnifier";

// Returns the magnifier area height given the display height.
int GetMagnifierHeight(int display_height) {
  return (display_height /
          DockedMagnifierController::kDefaultScreenHeightDivisor) +
         DockedMagnifierController::kSeparatorHeight;
}

class DockedMagnifierTest : public NoSessionAshTestBase {
 public:
  DockedMagnifierTest() = default;
  ~DockedMagnifierTest() override = default;

  DockedMagnifierController* controller() const {
    return Shell::Get()->docked_magnifier_controller();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  PrefService* user1_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser1Email));
  }

  PrefService* user2_pref_service() {
    return Shell::Get()->session_controller()->GetUserPrefServiceForUser(
        AccountId::FromUserEmail(kUser2Email));
  }

  // AshTestBase:
  void SetUp() override {
    // Explicitly enable --ash-constrain-pointer-to-root to be able to test
    // mouse cursor confinement outside the magnifier viewport.
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshConstrainPointerToRoot);

    NoSessionAshTestBase::SetUp();

    // Create user 1 session and simulate its login.
    SimulateUserLogin(kUser1Email);

    // Create user 2 session.
    GetSessionControllerClient()->AddUserSession(kUser2Email);

    // Place the cursor in the first display.
    GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));
  }

  void SwitchActiveUser(const std::string& email) {
    GetSessionControllerClient()->SwitchActiveUser(
        AccountId::FromUserEmail(email));
  }

  // Tests that when the magnifier layer's transform is applied on the point in
  // the |root_window| coordinates that corresponds to the
  // |point_of_interest_in_screen|, the resulting point is at the center of the
  // magnifier viewport widget.
  void TestMagnifierLayerTransform(
      const gfx::Point& point_of_interest_in_screen,
      const aura::Window* root_window) {
    // Convert to root coordinates.
    gfx::Point point_of_interest_in_root = point_of_interest_in_screen;
    ::wm::ConvertPointFromScreen(root_window, &point_of_interest_in_root);
    // Account for point of interest being outside the minimum height threshold.
    // Do this in gfx::PointF to avoid rounding errors.
    gfx::PointF point_of_interest_in_root_f(point_of_interest_in_root);
    const float min_pov_height =
        controller()->GetMinimumPointOfInterestHeightForTesting();
    if (point_of_interest_in_root_f.y() < min_pov_height)
      point_of_interest_in_root_f.set_y(min_pov_height);

    const ui::Layer* magnifier_layer =
        controller()->GetViewportMagnifierLayerForTesting();
    // The magnifier layer's transform, when applied to the point of interest
    // (in root coordinates), should take it to the point at the center of the
    // viewport widget (also in root coordinates).
    point_of_interest_in_root_f =
        magnifier_layer->transform().MapPoint(point_of_interest_in_root_f);
    const views::Widget* viewport_widget =
        controller()->GetViewportWidgetForTesting();
    const gfx::Point viewport_center_in_root =
        viewport_widget->GetNativeWindow()
            ->GetBoundsInRootWindow()
            .CenterPoint();
    EXPECT_EQ(viewport_center_in_root,
              gfx::ToFlooredPoint(point_of_interest_in_root_f));
  }

  void TouchPoint(const gfx::Point& touch_point_in_screen) {
    // TODO(oshima): Currently touch event doesn't update the
    // event dispatcher in the event generator. Fix it and use
    // touch event insteead.
    auto* generator = GetEventGenerator();
    generator->GestureTapAt(touch_point_in_screen);
  }

  std::unique_ptr<views::Widget> CreateLockSystemModalWindow(
      const gfx::Rect& bounds) {
    auto* widget_delegate_view = new views::WidgetDelegateView();
    widget_delegate_view->SetModalType(ui::mojom::ModalType::kSystem);
    return CreateTestWidget(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
        widget_delegate_view, kShellWindowId_LockSystemModalContainer, bounds);
  }

  // Test that display work area and a modal window is adjusted correctly
  // after enabling and disabling a docked magnifier.
  void TestDisplayWorkAreaAndLockSystemModalBoundsUpdated() {
    // Start with the docked magnifier disabled.
    EXPECT_FALSE(controller()->GetEnabled());

    // Create a lock system modal window.
    auto lock_system_modal_widget =
        CreateLockSystemModalWindow(gfx::Rect(800, 600));

    // Enable the docked magnifier.
    controller()->SetEnabled(true);
    EXPECT_TRUE(controller()->GetEnabled());

    // Expect that the modal window fits inside the shrunk valid area.
    const gfx::Rect modal_bounds =
        lock_system_modal_widget->GetWindowBoundsInScreen();
    const gfx::Rect valid_area =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow())
            .work_area();
    const gfx::Rect docked_magnifier_bounds =
        controller()->GetViewportWidgetForTesting()->GetWindowBoundsInScreen();
    // Check that display work area does not overlap with a docked magnifier.
    EXPECT_FALSE(docked_magnifier_bounds.Intersects(valid_area));
    // Check that modal window fits inside the display work area, |valid_area|,
    // to make sure the |modal_bounds| size does not overflow.
    EXPECT_TRUE(valid_area.Contains(modal_bounds));

    // Disable the docked magnifier.
    controller()->SetEnabled(false);
    EXPECT_FALSE(controller()->GetEnabled());

    const gfx::Rect modal_bounds_no_magnifier =
        lock_system_modal_widget->GetWindowBoundsInScreen();

    // Expect that the window stays inside the valid area.
    const gfx::Rect valid_area_no_magnifier =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow())
            .work_area();
    EXPECT_TRUE(valid_area_no_magnifier.Contains(modal_bounds_no_magnifier));
    // With larger work area, |modal_bounds_no_magnifier| size must not shrink.
    // Even stricter, the size must remain the same as |modal_bounds| size
    // because a centered system modal only shrinks but never expands according
    // to SystemModalContainerLayoutManager::GetCenteredAndOrFittedBounds()
    // ClampToCenteredSize.
    EXPECT_EQ(modal_bounds.size(), modal_bounds_no_magnifier.size());
    // Expect y offset of the modal window to be centered correctly.
    EXPECT_EQ((valid_area_no_magnifier.height() -
               modal_bounds_no_magnifier.height()) /
                  2,
              modal_bounds_no_magnifier.y());
  }

  // Test that bounds of a modal window are the same when initially created and
  // updated. views::NativeWidgetAura::CenterWindow() sets the initial modal
  // bounds, while
  // SystemModalContainerLayoutManager::GetCenteredAndOrFittedBounds() updates
  // the modal window bounds. Thus, this test makes sure the bounds are the same
  // in both cases.
  void TestLockSystemModalBoundUpdateAndCreationConsistency() {
    // Start with the docked magnifier disabled.
    EXPECT_FALSE(controller()->GetEnabled());

    // Create a lock system modal window for an update case.
    auto lock_system_modal_widget_update_case =
        CreateLockSystemModalWindow(gfx::Rect(800, 600));

    // Enable the docked magnifier.
    controller()->SetEnabled(true);
    EXPECT_TRUE(controller()->GetEnabled());

    // Create a lock system modal window for a creation case.
    auto lock_system_modal_widget =
        CreateLockSystemModalWindow(gfx::Rect(800, 600));

    // Expect that both modal windows fit inside the shrunk area
    // and have the exact same bounds.
    const gfx::Rect modal_bounds =
        lock_system_modal_widget->GetWindowBoundsInScreen();
    const gfx::Rect modal_bounds_update_case =
        lock_system_modal_widget_update_case->GetWindowBoundsInScreen();
    const gfx::Rect valid_area =
        display::Screen::GetScreen()
            ->GetDisplayNearestWindow(Shell::GetPrimaryRootWindow())
            .work_area();
    const gfx::Rect docked_magnifier_bounds =
        controller()->GetViewportWidgetForTesting()->GetWindowBoundsInScreen();
    EXPECT_FALSE(docked_magnifier_bounds.Intersects(valid_area));
    EXPECT_TRUE(valid_area.Contains(modal_bounds));
    EXPECT_EQ(modal_bounds, modal_bounds_update_case);

    // Disable the docked magnifier.
    controller()->SetEnabled(false);
    EXPECT_FALSE(controller()->GetEnabled());
  }
};

// If not signed in, test that display work area and window bounds
// are updated correctly after enabling and disabling a docked magnifier.
TEST_F(DockedMagnifierTest, WindowBoundsChangeInNonActiveState) {
  UpdateDisplay("800x600");

  struct {
    std::string trace;
    session_manager::SessionState state;
  } kNonActiveStatesTestCases[] = {
      {"oobe", session_manager::SessionState::OOBE},
      {"login_primary", session_manager::SessionState::LOGIN_PRIMARY},
      {"locked", session_manager::SessionState::LOCKED},
      {"login_secondary", session_manager::SessionState::LOGIN_SECONDARY},
  };

  // For each of the states which is not ACTIVE, LOGGED_IN_NOT_ACTIVE, and
  // UNKNOWN, set the session state and make sure that work area and window
  // bounds are as expected after enabling and disabling the docked magnifier.
  // In LOGGED_IN_NOT_ACTIVE state, no window can be added to
  // LockSystemModalContainer.
  for (auto test_case : kNonActiveStatesTestCases) {
    SCOPED_TRACE(test_case.trace);
    GetSessionControllerClient()->SetSessionState(test_case.state);
    // Test that display work area and the modal window position is dynamically
    // adjusted regarding the existence of a docked magnifier.
    TestDisplayWorkAreaAndLockSystemModalBoundsUpdated();
    // Test that bounds of a lock system modal window are
    // the same during creation and update.
    TestLockSystemModalBoundUpdateAndCreationConsistency();
  }
}

// Tests that the Fullscreen and Docked Magnifiers are mutually exclusive.
// TODO(afakhry): Update this test to use ash::MagnificationController once
// refactored. https://crbug.com/817157.
TEST_F(DockedMagnifierTest, MutuallyExclusiveMagnifiers) {
  // Start with both magnifiers disabled.
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());

  // Enabling one disables the other.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());

  controller()->SetFullscreenMagnifierEnabled(true);
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_TRUE(controller()->GetFullscreenMagnifierEnabled());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());

  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_FALSE(controller()->GetFullscreenMagnifierEnabled());
}

// Tests the changes in the magnifier's status, user switches.
TEST_F(DockedMagnifierTest, TestEnableAndDisable) {
  // Enable for user 1, and switch to user 2. User 2 should have it disabled.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  SwitchActiveUser(kUser2Email);
  EXPECT_FALSE(controller()->GetEnabled());

  // Switch back to user 1, expect it to be enabled.
  SwitchActiveUser(kUser1Email);
  EXPECT_TRUE(controller()->GetEnabled());
}

// Tests the magnifier's scale changes.
TEST_F(DockedMagnifierTest, TestScale) {
  // Scale changes are persisted even when the Docked Magnifier is disabled.
  EXPECT_FALSE(controller()->GetEnabled());
  controller()->SetScale(5.0f);
  EXPECT_FLOAT_EQ(5.0f, controller()->GetScale());

  // Scale values are clamped to a minimum of 1.0f (which means no scale).
  controller()->SetScale(0.0f);
  EXPECT_FLOAT_EQ(1.0f, controller()->GetScale());

  // Switch to user 2, change the scale, then switch back to user 1. User 1's
  // scale should not change.
  SwitchActiveUser(kUser2Email);
  controller()->SetScale(6.5f);
  EXPECT_FLOAT_EQ(6.5f, controller()->GetScale());
  SwitchActiveUser(kUser1Email);
  EXPECT_FLOAT_EQ(1.0f, controller()->GetScale());
}

// Tests that updates of the Docked Magnifier user prefs from outside the
// DockedMagnifierController (such as Settings UI) are observed and applied.
TEST_F(DockedMagnifierTest, TestOutsidePrefsUpdates) {
  EXPECT_FALSE(controller()->GetEnabled());
  user1_pref_service()->SetBoolean(prefs::kDockedMagnifierEnabled, true);
  EXPECT_TRUE(controller()->GetEnabled());
  user1_pref_service()->SetDouble(prefs::kDockedMagnifierScale, 7.3f);
  EXPECT_FLOAT_EQ(7.3f, controller()->GetScale());
  user1_pref_service()->SetBoolean(prefs::kDockedMagnifierEnabled, false);
  EXPECT_FALSE(controller()->GetEnabled());
}

// Tests that the workareas of displays are adjusted properly when the Docked
// Magnifier's viewport moves from one display to the next.
TEST_F(DockedMagnifierTest, DisplaysWorkAreas) {
  UpdateDisplay("800x600,800+0-400x300");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  // Place the cursor in the first display.
  GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));

  // Before the magnifier is enabled, the work areas of both displays are their
  // full size minus the shelf height.
  const display::Display& display_1 = display_manager()->GetDisplayAt(0);
  const gfx::Rect disp_1_bounds(0, 0, 800, 600);
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  gfx::Rect disp_1_workarea_no_magnifier = disp_1_bounds;
  disp_1_workarea_no_magnifier.Inset(
      gfx::Insets::TLBR(0, 0, ShelfConfig::Get()->shelf_size(), 0));
  EXPECT_EQ(disp_1_workarea_no_magnifier, display_1.work_area());
  // At this point, normal mouse cursor confinement should be used.
  AshWindowTreeHost* host1 =
      Shell::Get()
          ->window_tree_host_manager()
          ->GetAshWindowTreeHostForDisplayId(display_1.id());
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_1_bounds.size()));

  const display::Display& display_2 = display_manager()->GetDisplayAt(1);
  const gfx::Rect disp_2_bounds(800, 0, 400, 300);
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  gfx::Rect disp_2_workarea_no_magnifier = disp_2_bounds;
  disp_2_workarea_no_magnifier.Inset(
      gfx::Insets::TLBR(0, 0, ShelfConfig::Get()->shelf_size(), 0));
  EXPECT_EQ(disp_2_workarea_no_magnifier, display_2.work_area());
  AshWindowTreeHost* host2 =
      Shell::Get()
          ->window_tree_host_manager()
          ->GetAshWindowTreeHostForDisplayId(display_2.id());
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_2_bounds.size()));

  // Enable the magnifier and the check the workareas.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  const views::Widget* viewport_1_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_1_widget);
  EXPECT_EQ(root_windows[0],
            viewport_1_widget->GetNativeView()->GetRootWindow());
  // Since the cursor is in the first display, the height of its workarea will
  // be further shrunk from the top by 1/4th of its full height + the height of
  // the separator layer.
  gfx::Rect disp_1_workspace_with_magnifier = disp_1_workarea_no_magnifier;
  const int disp_1_magnifier_height =
      GetMagnifierHeight(disp_1_bounds.height());
  disp_1_workspace_with_magnifier.Inset(
      gfx::Insets::TLBR(disp_1_magnifier_height, 0, 0, 0));
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  EXPECT_EQ(disp_1_workspace_with_magnifier, display_1.work_area());
  // The first display should confine the mouse movement outside of the
  // viewport.
  gfx::Rect disp_1_confine_bounds(
      0, disp_1_magnifier_height, disp_1_bounds.width(),
      disp_1_bounds.height() - disp_1_magnifier_height);
  disp_1_confine_bounds.Inset(
      gfx::Insets().set_top(-DockedMagnifierController::kSeparatorHeight));
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(), disp_1_confine_bounds);

  // The second display should remain unaffected.
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  EXPECT_EQ(disp_2_workarea_no_magnifier, display_2.work_area());
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_2_bounds.size()));

  // Now, move mouse cursor to display 2, and expect that the workarea of
  // display 1 is restored to its original value, while that of display 2 is
  // shrunk to fit the Docked Magnifier's viewport.
  GetEventGenerator()->MoveMouseTo(gfx::Point(800, 0));
  const views::Widget* viewport_2_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_2_widget);
  EXPECT_NE(viewport_1_widget, viewport_2_widget);  // It's a different widget.
  EXPECT_EQ(root_windows[1],
            viewport_2_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  EXPECT_EQ(disp_1_workarea_no_magnifier, display_1.work_area());
  // Display 1 goes back to the normal mouse confinement.
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_1_bounds.size()));
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  gfx::Rect disp_2_workspace_with_magnifier = disp_2_workarea_no_magnifier;
  const int disp_2_magnifier_height =
      GetMagnifierHeight(disp_2_bounds.height());
  disp_2_workspace_with_magnifier.Inset(
      gfx::Insets().set_top(disp_2_magnifier_height));
  EXPECT_EQ(disp_2_workspace_with_magnifier, display_2.work_area());
  // Display 2's mouse is confined outside the viewport.
  gfx::Rect disp_2_confine_bounds(
      0, disp_2_magnifier_height, disp_2_bounds.width(),
      disp_2_bounds.height() - disp_2_magnifier_height);
  disp_2_confine_bounds.Inset(
      gfx::Insets().set_top(-DockedMagnifierController::kSeparatorHeight));
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(), disp_2_confine_bounds);

  // Now, disable the magnifier, and expect both displays to return back to
  // their original state.
  controller()->SetEnabled(false);
  EXPECT_FALSE(controller()->GetEnabled());
  EXPECT_EQ(disp_1_bounds, display_1.bounds());
  EXPECT_EQ(disp_1_workarea_no_magnifier, display_1.work_area());
  EXPECT_EQ(disp_2_bounds, display_2.bounds());
  EXPECT_EQ(disp_2_workarea_no_magnifier, display_2.work_area());
  // Normal mouse confinement for both displays.
  EXPECT_EQ(host1->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_1_bounds.size()));
  EXPECT_EQ(host2->GetLastCursorConfineBoundsInPixels(),
            gfx::Rect(gfx::Point(0, 0), disp_2_bounds.size()));
}

// Test that we exit overview mode when enabling the docked magnifier.
TEST_F(DockedMagnifierTest, DisplaysWorkAreasOverviewMode) {
  std::unique_ptr<aura::Window> window =
      TestWindowBuilder()
          .SetBounds(gfx::Rect(0, 0, 200, 200))
          .AllowAllWindowStates()
          .Build();
  WindowState::Get(window.get())->Maximize();

  // Enable overview mode followed by the magnifier.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());

  // Expect that overview mode is exited, the display's work area is updated,
  // and the window's bounds are updated to be equal to the new display's work
  // area bounds.
  EXPECT_FALSE(overview_controller->InOverviewSession());
  const display::Display& display = display_manager()->GetDisplayAt(0);
  gfx::Rect workarea = display.bounds();
  const int magnifier_height = GetMagnifierHeight(display.bounds().height());
  workarea.Inset(gfx::Insets::TLBR(magnifier_height, 0,
                                   ShelfConfig::Get()->shelf_size(), 0));
  EXPECT_EQ(workarea, display.work_area());
  EXPECT_EQ(workarea, window->bounds());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

// Test that we exist split view and over view modes when a single window is
// snapped and the other snap region is hosting overview mode.
TEST_F(DockedMagnifierTest, DisplaysWorkAreasSingleSplitView) {
  // Verify that we're in tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  std::unique_ptr<aura::Window> window =
      TestWindowBuilder()
          .SetBounds(gfx::Rect(0, 0, 200, 200))
          .AllowAllWindowStates()
          .Build();
  WindowState::Get(window.get())->Maximize();

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);

  // Simulate going into split view, by enabling overview mode, and snapping
  // a window to the left.
  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window.get());
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Enable the docked magnifier and expect that both overview and split view
  // modes are exited, and the window remains maximized, and its bounds are
  // updated to match the new display's work area.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kNoSnap);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  const display::Display& display = display_manager()->GetDisplayAt(0);
  const int magnifier_height = GetMagnifierHeight(display.bounds().height());
  gfx::Rect work_area = display.bounds();
  work_area.Inset(gfx::Insets::TLBR(magnifier_height, 0,
                                    ShelfConfig::Get()->shelf_size(), 0));
  EXPECT_EQ(work_area, display.work_area());
  EXPECT_EQ(work_area, window->bounds());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMaximized());
}

// Test that we don't exit split view with two windows snapped on both sides
// when we enable the docked magnifier, but rather their bounds are updated.
TEST_F(DockedMagnifierTest, DisplaysWorkAreasDoubleSplitView) {
  // Verify that we're in tablet mode.
  ash::TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());

  std::unique_ptr<aura::Window> window1 =
      TestWindowBuilder()
          .SetBounds(gfx::Rect(0, 0, 200, 200))
          .AllowAllWindowStates()
          .Build();
  std::unique_ptr<aura::Window> window2 =
      TestWindowBuilder()
          .SetBounds(gfx::Rect(0, 0, 200, 200))
          .AllowAllWindowStates()
          .Build();

  auto* overview_controller = Shell::Get()->overview_controller();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  EXPECT_EQ(split_view_controller()->InSplitViewMode(), false);
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);

  // Snapping both windows should exit overview mode.
  EXPECT_FALSE(overview_controller->InOverviewSession());

  // Enable the docked magnifier, and expect that split view does not exit, and
  // the two windows heights are updated to be equal to the height of the
  // updated display's work area.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_EQ(split_view_controller()->InSplitViewMode(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  const display::Display& display = display_manager()->GetDisplayAt(0);
  const int magnifier_height = GetMagnifierHeight(display.bounds().height());
  gfx::Rect work_area = display.bounds();
  work_area.Inset(gfx::Insets::TLBR(magnifier_height, 0,
                                    ShelfConfig::Get()->shelf_size(), 0));
  EXPECT_EQ(work_area, display.work_area());
  EXPECT_EQ(work_area.height(), window1->bounds().height());
  EXPECT_EQ(work_area.height(), window2->bounds().height());
}

// Tests that the Docked Magnifier follows touch events.
TEST_F(DockedMagnifierTest, TouchEvents) {
  UpdateDisplay("800x600,800+0-400x300");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  controller()->SetScale(4.0f);

  // Generate some touch events in both displays and expect the magnifier
  // viewport moves accordingly.
  gfx::Point touch_point(200, 350);
  TouchPoint(touch_point);
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  TestMagnifierLayerTransform(touch_point, root_windows[0]);

  // Touch a new point in the other display.
  touch_point = gfx::Point(900, 200);
  TouchPoint(touch_point);

  // New viewport widget is created in the second display.
  ASSERT_NE(viewport_widget, controller()->GetViewportWidgetForTesting());
  viewport_widget = controller()->GetViewportWidgetForTesting();
  EXPECT_EQ(root_windows[1], viewport_widget->GetNativeView()->GetRootWindow());
  TestMagnifierLayerTransform(touch_point, root_windows[1]);
}

// Tests the behavior of the magnifier when displays are added or removed.
TEST_F(DockedMagnifierTest, AddRemoveDisplays) {
  // Start with a single display.
  const auto disp_1_info = display::ManagedDisplayInfo::CreateFromSpecWithID(
      "0+0-600x800", 101 /* id */);
  std::vector<display::ManagedDisplayInfo> info_list;
  info_list.push_back(disp_1_info);
  display_manager()->OnNativeDisplaysChanged(info_list);
  auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  // Enable the magnifier, and validate the state of the viewport widget.
  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_1_height =
      800 / DockedMagnifierController::kDefaultScreenHeightDivisor;
  EXPECT_EQ(gfx::Rect(0, 0, 600, viewport_1_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Adding a new display should not affect where the viewport currently is.
  const auto disp_2_info = display::ManagedDisplayInfo::CreateFromSpecWithID(
      "600+0-400x600", 102 /* id */);
  info_list.push_back(disp_2_info);
  display_manager()->OnNativeDisplaysChanged(info_list);
  root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  // Same viewport widget in same root window.
  EXPECT_EQ(viewport_widget, controller()->GetViewportWidgetForTesting());
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(gfx::Rect(0, 0, 600, viewport_1_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Move the cursor to the second display, expect the viewport widget to get
  // updated accordingly.
  GetEventGenerator()->MoveMouseTo(gfx::Point(800, 0));
  // New viewport widget is created.
  ASSERT_NE(viewport_widget, controller()->GetViewportWidgetForTesting());
  viewport_widget = controller()->GetViewportWidgetForTesting();
  EXPECT_EQ(root_windows[1], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_2_height =
      600 / DockedMagnifierController::kDefaultScreenHeightDivisor;
  EXPECT_EQ(gfx::Rect(600, 0, 400, viewport_2_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Now, remove display 2 ** while ** the magnifier viewport is there. This
  // should cause no crashes, the viewport widget should be recreated in
  // display 1.
  info_list.clear();
  info_list.push_back(disp_1_info);
  display_manager()->OnNativeDisplaysChanged(info_list);
  // We need to spin this run loop to wait for a new mouse event to be
  // dispatched so that the viewport widget is re-created.
  base::RunLoop().RunUntilIdle();
  root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());
  viewport_widget = controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  EXPECT_EQ(gfx::Rect(0, 0, 600, viewport_1_height),
            viewport_widget->GetWindowBoundsInScreen());
}

// Tests various magnifier layer transform in the simple cases (i.e. no device
// scale factors or screen rotations).
TEST_F(DockedMagnifierTest, TransformSimple) {
  UpdateDisplay("800x700");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  controller()->SetEnabled(true);
  const float scale1 = 2.0f;
  controller()->SetScale(scale1);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FLOAT_EQ(scale1, controller()->GetScale());
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_height =
      root_windows[0]->bounds().height() /
      DockedMagnifierController::kDefaultScreenHeightDivisor;
  EXPECT_EQ(gfx::Rect(0, 0, 800, viewport_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Move the cursor to the center of the screen.
  gfx::Point point_of_interest(400, 400);
  GetEventGenerator()->MoveMouseTo(point_of_interest);
  TestMagnifierLayerTransform(point_of_interest, root_windows[0]);

  // Move the cursor to the bottom right corner.
  point_of_interest = gfx::Point(799, 799);
  GetEventGenerator()->MoveMouseTo(point_of_interest);
  TestMagnifierLayerTransform(point_of_interest, root_windows[0]);

  // Tricky: Move the cursor to the top right corner, such that the cursor is
  // over the magnifier viewport. The transform should be such that the viewport
  // doesn't show itself.
  point_of_interest = gfx::Point(799, 0);
  GetEventGenerator()->MoveMouseTo(point_of_interest);
  TestMagnifierLayerTransform(point_of_interest, root_windows[0]);
  // In this case, our point of interest is changed to be at the bottom of the
  // separator, and it should go to the center of the top *edge* of the viewport
  // widget.
  point_of_interest.set_y(viewport_height +
                          DockedMagnifierController::kSeparatorHeight);
  const gfx::Point viewport_center =
      viewport_widget->GetNativeWindow()->GetBoundsInRootWindow().CenterPoint();
  gfx::Point viewport_top_edge_center = viewport_center;
  viewport_top_edge_center.set_y(0);
  const ui::Layer* magnifier_layer =
      controller()->GetViewportMagnifierLayerForTesting();
  EXPECT_EQ(viewport_top_edge_center,
            magnifier_layer->transform().MapPoint(point_of_interest));
  // The minimum height for the point of interest is the bottom of the viewport
  // + the height of the separator + half the height of the viewport when scaled
  // back to the non-magnified space.
  EXPECT_FLOAT_EQ(viewport_height +
                      DockedMagnifierController::kSeparatorHeight +
                      (viewport_center.y() / scale1),
                  controller()->GetMinimumPointOfInterestHeightForTesting());

  // Leave the mouse cursor where it is, and only change the magnifier's scale.
  const float scale2 = 5.3f;
  controller()->SetScale(scale2);
  EXPECT_FLOAT_EQ(scale2, controller()->GetScale());
  // The transform behaves exactly as above even with a different scale.
  point_of_interest = gfx::Point(799, 0);
  TestMagnifierLayerTransform(point_of_interest, root_windows[0]);
  point_of_interest.set_y(viewport_height +
                          DockedMagnifierController::kSeparatorHeight);
  EXPECT_EQ(viewport_top_edge_center,
            magnifier_layer->transform().MapPoint(point_of_interest));

  EXPECT_FLOAT_EQ(viewport_height +
                      DockedMagnifierController::kSeparatorHeight +
                      (viewport_center.y() / scale2),
                  controller()->GetMinimumPointOfInterestHeightForTesting());
}

// Tests resizing docked magnifier by dragging the separator.
TEST_F(DockedMagnifierTest, ResizeDockedMagnifier) {
  UpdateDisplay("800x600");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_height =
      root_windows[0]->bounds().height() /
      DockedMagnifierController::kDefaultScreenHeightDivisor;
  EXPECT_EQ(gfx::Rect(0, 0, 800, viewport_height),
            viewport_widget->GetWindowBoundsInScreen());

  ::wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  EXPECT_NE(ui::mojom::CursorType::kNorthSouthResize,
            cursor_manager->GetCursor().type());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());

  // Move cursor over docked magnifier separator (to drag for resizing).
  gfx::Point mouse_location(400, viewport_height);
  GetEventGenerator()->MoveMouseTo(mouse_location);
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthResize,
            cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Drag separator 100 pixels down.
  mouse_location = gfx::Point(400, viewport_height + 100);
  GetEventGenerator()->DragMouseTo(mouse_location);
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthResize,
            cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());

  // Assert docked magnifier viewport is now taller.
  EXPECT_EQ(gfx::Rect(0, 0, 800, viewport_height + 100),
            viewport_widget->GetWindowBoundsInScreen());

  // Move off of the separator. The cursor should reset.
  GetEventGenerator()->MoveMouseTo(400, viewport_height + 200);
  EXPECT_NE(ui::mojom::CursorType::kNorthSouthResize,
            cursor_manager->GetCursor().type());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());

  // Drag again, but turn off docked magnifier during drag (simulating keyboard
  // shortcut). The cursor should reset.
  GetEventGenerator()->MoveMouseTo(400, viewport_height + 100);
  GetEventGenerator()->DragMouseTo(gfx::Point(400, viewport_height));
  EXPECT_EQ(ui::mojom::CursorType::kNorthSouthResize,
            cursor_manager->GetCursor().type());
  EXPECT_TRUE(cursor_manager->IsCursorLocked());
  controller()->SetEnabled(false);
  EXPECT_NE(ui::mojom::CursorType::kNorthSouthResize,
            cursor_manager->GetCursor().type());
  EXPECT_FALSE(cursor_manager->IsCursorLocked());
}

// Tests to verify dragging above separator does not resize docked magnifier.
TEST_F(DockedMagnifierTest, DragAboveSeparatorDoesNotResizeDockedMagnifier) {
  UpdateDisplay("800x600");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_height =
      root_windows[0]->bounds().height() /
      DockedMagnifierController::kDefaultScreenHeightDivisor;
  EXPECT_EQ(gfx::Rect(0, 0, 800, viewport_height),
            viewport_widget->GetWindowBoundsInScreen());

  // Move cursor 2px above the docked magnifier separator, in the viewport area,
  // where dragging should not work.
  gfx::Point mouse_location(400, viewport_height - 2);
  GetEventGenerator()->MoveMouseTo(mouse_location);

  // Drag 100 pixels down.
  mouse_location = gfx::Point(400, viewport_height + 100);
  GetEventGenerator()->DragMouseTo(mouse_location);

  // Assert docked magnifier viewport size remains at old height.
  EXPECT_EQ(gfx::Rect(0, 0, 800, viewport_height),
            viewport_widget->GetWindowBoundsInScreen());
}

// Tests to verify hovering and resizing the docked magnifier moves the cursor
// in front of the viewport.
TEST_F(DockedMagnifierTest, HoverAndResizeDockedMagnifierMovesCursorInFront) {
  UpdateDisplay("800x600");
  const auto root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(1u, root_windows.size());

  controller()->SetEnabled(true);
  EXPECT_TRUE(controller()->GetEnabled());
  const views::Widget* viewport_widget =
      controller()->GetViewportWidgetForTesting();
  ASSERT_NE(nullptr, viewport_widget);
  EXPECT_EQ(root_windows[0], viewport_widget->GetNativeView()->GetRootWindow());
  const int viewport_height =
      root_windows[0]->bounds().height() /
      DockedMagnifierController::kDefaultScreenHeightDivisor;
  EXPECT_EQ(gfx::Rect(0, 0, 800, viewport_height),
            viewport_widget->GetWindowBoundsInScreen());

  CursorWindowController* cursor_window_controller =
      Shell::Get()->window_tree_host_manager()->cursor_window_controller();

  // Verify mouse is in layer behind separator.
  EXPECT_EQ(cursor_window_controller->GetContainerForTest()->GetId(),
            ash::kShellWindowId_MouseCursorContainer);

  // Move cursor over the docked magnifier separator.
  gfx::Point mouse_location(400, viewport_height);
  GetEventGenerator()->MoveMouseTo(mouse_location);

  // Verify mouse is in layer on top of separator
  EXPECT_EQ(cursor_window_controller->GetContainerForTest()->GetId(),
            ash::kShellWindowId_DockedMagnifierContainer);

  // Drag mouse 100 pixels down.
  mouse_location = gfx::Point(400, viewport_height + 100);
  GetEventGenerator()->DragMouseTo(mouse_location);

  // Assert mouse is still in layer on top of separator.
  EXPECT_EQ(cursor_window_controller->GetContainerForTest()->GetId(),
            ash::kShellWindowId_DockedMagnifierContainer);

  // Move mouse 50 pixels down.
  mouse_location = gfx::Point(400, viewport_height + 50);
  GetEventGenerator()->MoveMouseTo(mouse_location);

  // Assert mouse is back in layer behind separator.
  EXPECT_EQ(cursor_window_controller->GetContainerForTest()->GetId(),
            ash::kShellWindowId_MouseCursorContainer);
}

// Tests that there are no crashes observed when the docked magnifier switches
// displays, moving away from a display with a maximized window that has a
// focused text input field. Changing the old display's work area bounds should
// not cause recursive caret bounds change notifications into the docked
// magnifier. https://crbug.com/1000903.
TEST_F(DockedMagnifierTest, NoCrashDueToRecursion) {
  UpdateDisplay("600x900,800x600");
  const auto roots = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, roots.size());

  MagnifierTextInputTestHelper text_input_helper;
  text_input_helper.CreateAndShowTextInputViewInRoot(gfx::Rect(0, 0, 600, 900),
                                                     roots[0]);
  text_input_helper.MaximizeWidget();

  // Enable the docked magnifier.
  controller()->SetEnabled(true);
  const float scale1 = 2.0f;
  controller()->SetScale(scale1);
  EXPECT_TRUE(controller()->GetEnabled());
  EXPECT_FLOAT_EQ(scale1, controller()->GetScale());

  // Move the mouse to the second display and expect no crashes.
  GetEventGenerator()->MoveMouseTo(1000, 300);
}

TEST_F(DockedMagnifierTest, CaptureMode) {
  UpdateDisplay("600x900");

  controller()->SetEnabled(true);
  controller()->SetScale(2.f);

  auto* capture_mode_controller = CaptureModeController::Get();
  capture_mode_controller->Start(CaptureModeEntryType::kQuickSettings);

  // Test that the magnifier viewport follows the cursor when it moves to
  // various points even though capture mode consumes mouse events.
  auto* event_generator = GetEventGenerator();
  gfx::Point point_of_interest{10, 20};
  event_generator->MoveMouseTo(point_of_interest);
  auto* root = Shell::GetPrimaryRootWindow();
  TestMagnifierLayerTransform(point_of_interest, root);
  point_of_interest = gfx::Point{510, 820};
  event_generator->MoveMouseTo(point_of_interest);
  TestMagnifierLayerTransform(point_of_interest, root);

  // And the magnifier viewport follows the cursor when it's above the capture
  // mode bar.
  const auto* bar_widget = capture_mode_controller->capture_mode_session()
                               ->GetCaptureModeBarWidget();
  point_of_interest = bar_widget->GetWindowBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(point_of_interest);
  TestMagnifierLayerTransform(point_of_interest, root);
}

// TODO(afakhry): Expand tests:
// - Test magnifier viewport's layer transforms with screen rotation,
//   multi display, and unified mode.
// - Test adjust scale using scroll events.

}  // namespace

}  // namespace ash
