// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/focus_cycler.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_prefs.h"
#include "ash/public/cpp/keyboard/keyboard_controller.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_item.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/wallpaper/wallpaper_controller_observer.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/drag_window_from_shelf_controller.h"
#include "ash/shelf/drag_window_from_shelf_controller_test_api.h"
#include "ash/shelf/home_button.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_app_button.h"
#include "ash/shelf/shelf_controller.h"
#include "ash/shelf/shelf_focus_cycler.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_test_util.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/test/hotseat_state_watcher.h"
#include "ash/shelf/test/shelf_layout_manager_test_base.h"
#include "ash/shell.h"
#include "ash/system/eche/eche_tray.h"
#include "ash/system/ime_menu/ime_menu_tray.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_ash_web_view_factory.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace_controller.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/icu_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/image/image_skia_rep.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/test/widget_animation_waiter.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::kHideShelfWhenFullscreenKey;

void PressHomeButton() {
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      AppListShowSource::kShelfButton, base::TimeTicks());
}

void StepWidgetLayerAnimatorToEnd(views::Widget* widget) {
  widget->GetNativeView()->layer()->GetAnimator()->Step(base::TimeTicks::Now() +
                                                        base::Seconds(1));
}

ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}

HotseatWidget* GetHotseatWidget() {
  return AshTestBase::GetPrimaryShelf()->hotseat_widget();
}

gfx::Rect GetScreenAvailableBounds() {
  const WorkAreaInsets* const work_area =
      WorkAreaInsets::ForWindow(GetShelfWidget()->GetNativeWindow());
  gfx::Rect available_bounds = screen_util::GetDisplayBoundsWithShelf(
      GetShelfWidget()->GetNativeWindow());
  available_bounds.Inset(work_area->GetAccessibilityInsets());
  return available_bounds;
}

// Returns the distance of the top of a widget from the bottom of the primary
// screen.
int GetWidgetOffsetFromBottom(const views::Widget* widget) {
  const int display_bottom =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().bottom();

  return display_bottom -
         widget->GetClientAreaBoundsInScreen().top_center().y();
}

// Creates and displays a test app in the hotseat.
void AddApp() {
  ShelfController* controller = Shell::Get()->shelf_controller();
  const int next_app_index = controller->model()->item_count();
  ShelfTestUtil::AddAppShortcut(
      "app_id_" + base::NumberToString(next_app_index), TYPE_PINNED_APP);
}

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() = default;

  TestDisplayObserver(const TestDisplayObserver&) = delete;
  TestDisplayObserver& operator=(const TestDisplayObserver&) = delete;

  ~TestDisplayObserver() override = default;

  int metrics_change_count() const { return metrics_change_count_; }

 private:
  // ShelfLayoutManagerObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    metrics_change_count_++;
  }

  display::ScopedDisplayObserver display_observer_{this};
  int metrics_change_count_ = 0;
};

class WallpaperShownWaiter : public WallpaperControllerObserver {
 public:
  WallpaperShownWaiter() {
    Shell::Get()->wallpaper_controller()->AddObserver(this);
  }

  WallpaperShownWaiter(const WallpaperShownWaiter&) = delete;
  WallpaperShownWaiter& operator=(const WallpaperShownWaiter&) = delete;

  ~WallpaperShownWaiter() override {
    Shell::Get()->wallpaper_controller()->RemoveObserver(this);
  }

  // Note this could only be called once because RunLoop would not run after
  // Quit is called. Create a new instance if there's need to wait again.
  void Wait() { run_loop_.Run(); }

 private:
  // WallpaperControllerObserver:
  void OnFirstWallpaperShown() override { run_loop_.Quit(); }

  base::RunLoop run_loop_;
};

// This class detects the auto hide state change in shelf layout manager within
// its lifetime.
class AutoHideStateDetector : public ShelfLayoutManagerObserver {
 public:
  AutoHideStateDetector() {
    Shell::GetPrimaryRootWindowController()
        ->shelf()
        ->shelf_layout_manager()
        ->AddObserver(this);
  }

  AutoHideStateDetector(const AutoHideStateDetector&) = delete;
  AutoHideStateDetector& operator=(const AutoHideStateDetector&) = delete;

  ~AutoHideStateDetector() override {
    Shell::GetPrimaryRootWindowController()
        ->shelf()
        ->shelf_layout_manager()
        ->RemoveObserver(this);
  }

  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override {
    if (new_state == SHELF_AUTO_HIDE_HIDDEN)
      was_shelf_auto_hidden = true;
  }

  bool WasShelfAutoHidden() const { return was_shelf_auto_hidden; }

 private:
  bool was_shelf_auto_hidden = false;
};

}  // namespace

class ShelfLayoutManagerTest : public ShelfLayoutManagerTestBase {
 public:
  ShelfLayoutManagerTest() {
    // TODO(b/293400777): Test currently crashes when Jelly is enabled because
    // of a crash in ShellTestApi. Remove when that is fixed.
    scoped_features_.InitAndDisableFeature(chromeos::features::kJelly);
  }

  void SetUpKioskSession() {
    SessionInfo info;
    info.is_running_in_app_mode = true;
    info.state = session_manager::SessionState::ACTIVE;
    Shell::Get()->session_controller()->SetSessionInfo(info);
  }

 private:
  base::test::ScopedFeatureList scoped_features_;
};

// Makes sure SetVisible updates work area and widget appropriately.
TEST_F(ShelfLayoutManagerTest, SetVisible) {
  ShelfWidget* shelf_widget = GetShelfWidget();
  ShelfLayoutManager* manager = shelf_widget->shelf_layout_manager();
  // Force an initial layout.
  manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());

  gfx::Rect status_bounds(
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen());
  gfx::Rect shelf_bounds(shelf_widget->GetWindowBoundsInScreen());
  int shelf_height = manager->GetIdealBounds().height();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const gfx::Rect stable_work_area = display.work_area();
  ASSERT_NE(-1, display.id());
  // Bottom inset should be the max of widget heights.
  EXPECT_EQ(shelf_height, display.GetWorkAreaInsets().bottom());

  // Hide the shelf.
  SetState(manager, SHELF_HIDDEN);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_HIDDEN, manager->visibility_state());
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());

  // Make sure the bounds of the two widgets changed.
  EXPECT_GE(shelf_widget->GetNativeView()->bounds().y(),
            display.bounds().bottom());
  EXPECT_GE(shelf_widget->status_area_widget()->GetNativeView()->bounds().y(),
            display.bounds().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // And show it again.
  SetState(manager, SHELF_VISIBLE);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(shelf_height, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Make sure the bounds of the two widgets changed.
  shelf_bounds = shelf_widget->GetNativeView()->bounds();
  EXPECT_LT(shelf_bounds.y(), display.bounds().bottom());
  status_bounds = shelf_widget->status_area_widget()->GetNativeView()->bounds();
  EXPECT_LT(status_bounds.y(), display.bounds().bottom());
}

// Makes sure LayoutShelf invoked while animating cleans things up.
TEST_F(ShelfLayoutManagerTest, LayoutShelfWhileAnimating) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  // Force an initial layout.
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Hide the shelf.
  SetState(layout_manager, SHELF_HIDDEN);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());

  // Make sure the bounds of the two widgets changed.
  ShelfWidget* shelf_widget = GetShelfWidget();
  EXPECT_GE(shelf_widget->GetNativeView()->bounds().y(),
            display.bounds().bottom());
  EXPECT_GE(shelf_widget->status_area_widget()->GetNativeView()->bounds().y(),
            display.bounds().bottom());
}

// Test that switching to a different visibility state does not restart the
// shelf show / hide animation if it is already running. (crbug.com/250918)
TEST_F(ShelfLayoutManagerTest, SetStateWhileAnimating) {
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  SetState(layout_manager, SHELF_VISIBLE);
  ShelfWidget* shelf_widget = GetShelfWidget();
  gfx::Rect initial_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  gfx::Rect initial_status_bounds =
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen();

  ui::ScopedAnimationDurationScaleMode normal_animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  SetState(layout_manager, SHELF_HIDDEN);
  SetState(layout_manager, SHELF_VISIBLE);

  gfx::Rect current_shelf_bounds = shelf_widget->GetWindowBoundsInScreen();
  gfx::Rect current_status_bounds =
      shelf_widget->status_area_widget()->GetWindowBoundsInScreen();

  const int small_change = initial_shelf_bounds.height() / 2;
  EXPECT_LE(
      std::abs(initial_shelf_bounds.height() - current_shelf_bounds.height()),
      small_change);
  EXPECT_LE(
      std::abs(initial_status_bounds.height() - current_status_bounds.height()),
      small_change);
}

// Various assertions around auto-hide.
TEST_F(ShelfLayoutManagerTest, AutoHide) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  const gfx::Rect stable_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  layout_manager->LayoutShelf();

  const int display_bottom = display.bounds().bottom();
  EXPECT_EQ(
      display_bottom - ShelfConfig::Get()->hidden_shelf_in_screen_portion(),
      GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(display_bottom, display.work_area().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Move the mouse to the bottom of the screen.
  generator->MoveMouseTo(0, display_bottom - 1);

  // Shelf should be shown again (but it shouldn't have changed the work area).
  SetState(layout_manager, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  layout_manager->LayoutShelf();
  EXPECT_EQ(display_bottom - layout_manager->GetIdealBounds().height(),
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(display_bottom, display.work_area().bottom());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Tap the system tray when shelf is shown should open the system tray menu.
  generator->GestureTapAt(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());

  // Move mouse back up and click to dismiss the opened system tray menu.
  generator->MoveMouseTo(0, 0);
  generator->ClickLeftButton();
  SetState(layout_manager, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  layout_manager->LayoutShelf();
  EXPECT_EQ(
      display_bottom - ShelfConfig::Get()->hidden_shelf_in_screen_portion(),
      GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  // Move the mouse to the bottom again to show the shelf.
  generator->MoveMouseTo(0, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // A tap on the maximized window should hide the shelf, even if the most
  // recent mouse position was over the shelf (crbug.com/963977).
  EXPECT_TRUE(widget->IsMouseEventsEnabled());
  gfx::Rect window_bounds = widget->GetNativeWindow()->GetBoundsInScreen();
  generator->GestureTapAt(window_bounds.origin() + gfx::Vector2d(10, 10));
  EXPECT_FALSE(widget->IsMouseEventsEnabled());
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Return the mouse to the top.
  generator->MoveMouseTo(0, 0);

  // Drag mouse to bottom of screen.
  generator->PressLeftButton();
  generator->MoveMouseTo(0, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  generator->ReleaseLeftButton();
  generator->MoveMouseTo(1, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  generator->PressLeftButton();
  generator->MoveMouseTo(1, display_bottom - 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Switch to tablet mode should hide the AUTO_HIDE_SHOWN shelf even the mouse
  // cursor is inside the shelf area.
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Test the behavior of the shelf when it is auto hidden and it is on the
// boundary between the primary and the secondary display.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfOnScreenBoundary) {
  UpdateDisplay("800x600,800x600");
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::RIGHT, 0));
  // Put the primary monitor's shelf on the display boundary.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kRight);

  // Create a window because the shelf is always shown when no windows are
  // visible.
  CreateTestWidget();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const int right_edge = display.bounds().right() - 1;
  const int y = display.bounds().y();

  // Start off the mouse nowhere near the shelf; the shelf should be hidden.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(right_edge - 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse over the light bar (but not to the edge of the screen)
  // should show the shelf.
  generator->MoveMouseTo(right_edge - 1, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(right_edge - 1,
            display::Screen::GetScreen()->GetCursorScreenPoint().x());

  // Moving the mouse off the light bar should hide the shelf.
  generator->MoveMouseTo(right_edge - 60, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse to the right edge of the screen crossing the light bar
  // should show the shelf despite the mouse cursor getting warped to the
  // secondary display.
  generator->MoveMouseTo(right_edge - 1, y);
  generator->MoveMouseTo(right_edge, y);
  UpdateAutoHideStateNow();
  EXPECT_NE(right_edge - 1,
            display::Screen::GetScreen()->GetCursorScreenPoint().x());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hide the shelf.
  generator->MoveMouseTo(right_edge - 60, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse to the right edge of the screen crossing the light bar and
  // overshooting by a lot should keep the shelf hidden.
  generator->MoveMouseTo(right_edge - 1, y);
  generator->MoveMouseTo(right_edge + 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving the mouse to the right edge of the screen crossing the light bar and
  // overshooting a bit should show the shelf.
  generator->MoveMouseTo(right_edge - 1, y);
  generator->MoveMouseTo(right_edge + 2, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Keeping the mouse close to the left edge of the secondary display after the
  // shelf is shown should keep the shelf shown.
  generator->MoveMouseTo(right_edge + 2, y + 1);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Moving the mouse far from the left edge of the secondary display should
  // hide the shelf.
  generator->MoveMouseTo(right_edge + 50, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Moving to the left edge of the secondary display without first crossing
  // the primary display's right aligned shelf first should not show the shelf.
  generator->MoveMouseTo(right_edge + 2, y);
  UpdateAutoHideStateNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Assertions around the login screen.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLoginScreenShowing) {
  Shelf* shelf = GetPrimaryShelf();
  auto* wallpaper_controller = Shell::Get()->wallpaper_controller();
  WallpaperShownWaiter waiter;

  SessionInfo info;
  info.state = session_manager::SessionState::LOGIN_PRIMARY;
  Shell::Get()->session_controller()->SetSessionInfo(info);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // No wallpaper.
  ASSERT_FALSE(wallpaper_controller->HasShownAnyWallpaper());
  EXPECT_EQ(ShelfBackgroundType::kLogin,
            GetShelfLayoutManager()->shelf_background_type());

  // Showing wallpaper is asynchronous.
  wallpaper_controller->ShowDefaultWallpaperForTesting();
  waiter.Wait();
  ASSERT_TRUE(wallpaper_controller->HasShownAnyWallpaper());

  // Non-blurred wallpaper.
  wallpaper_controller->UpdateWallpaperBlurForLockState(/*blur=*/false);
  EXPECT_EQ(ShelfBackgroundType::kLoginNonBlurredWallpaper,
            GetShelfLayoutManager()->shelf_background_type());

  // Blurred wallpaper.
  wallpaper_controller->UpdateWallpaperBlurForLockState(/*blur=*/true);
  EXPECT_EQ(ShelfBackgroundType::kLogin,
            GetShelfLayoutManager()->shelf_background_type());
}

// Assertions around the lock screen showing.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLockScreenShowing) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  layout_manager->LayoutShelf();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display.bounds().bottom() -
                ShelfConfig::Get()->hidden_shelf_in_screen_portion(),
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  std::unique_ptr<views::Widget> lock_widget(AshTestBase::CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      kShellWindowId_LockScreenContainer, gfx::Rect(200, 200)));
  lock_widget->Maximize();

  // Lock the screen.
  LockScreen();
  // Showing a widget in the lock screen should force the shelf to be visible.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfBackgroundType::kLogin,
            GetShelfLayoutManager()->shelf_background_type());

  UnlockScreen();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());
}

// Verifies that the hidden shelf shows after triggering the
// AcceleratorAction::kFocusShelf accelerator (https://crbug.com/1111426).
TEST_F(ShelfLayoutManagerTest, ShowHiddenShelfByFocusShelfAccelerator) {
  // Open a window so that the shelf will auto-hide.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  window->Show();
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Focus on the shelf by accelerator.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kFocusShelf, {});

  // Shelf should be visible.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

TEST_F(ShelfLayoutManagerTest, ShelfDoesNotAutoHideWithVoxAndTabletMode) {
  TabletModeControllerTestApi().EnterTabletMode();
  // Open a window so that the shelf will auto-hide.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  window->Show();
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Enable Chromevox. The shelf should now show.
  Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
      true, A11Y_NOTIFICATION_NONE);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Disable Chromevox again. The shelf should be able to auto-hide again.
  Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
      false, A11Y_NOTIFICATION_NONE);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests that the shelf should be visible when in overview mode.
TEST_F(ShelfLayoutManagerTest, VisibleInOverview) {
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window(CreateTestWindow());
  window->SetBounds({0, 0, 120, 320});
  window->Show();
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  GetShelfLayoutManager()->LayoutShelf();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display.bounds().bottom() -
                ShelfConfig::Get()->hidden_shelf_in_screen_portion(),
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  // Tests that the shelf is visible when in overview mode.
  EnterOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfBackgroundType::kOverview,
            GetShelfLayoutManager()->shelf_background_type());

  // Test that on exiting overview mode, the shelf returns to auto hide state.
  ExitOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(display.bounds().bottom() -
                ShelfConfig::Get()->hidden_shelf_in_screen_portion(),
            GetShelfWidget()->GetNativeWindow()->GetTargetBounds().y());
}

TEST_F(ShelfLayoutManagerTest,
       HotseatStateAfterTabletModeTransitionWithOverview) {
  // Tests that the shelf is visible when in overview mode.
  EnterOverview();
  ASSERT_EQ(HotseatState::kShownClamshell,
            GetShelfLayoutManager()->hotseat_state());

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
}

// Assertions around SetAutoHideBehavior.
TEST_F(ShelfLayoutManagerTest, SetAutoHideBehavior) {
  Shelf* shelf = GetPrimaryShelf();
  views::Widget* widget = CreateTestWidget();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  widget->Maximize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  ShelfWidget* shelf_widget = GetShelfWidget();
  EXPECT_TRUE(shelf_widget->status_area_widget()->IsVisible());
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());
}

// Verifies the shelf is visible when status/shelf is focused.
TEST_F(ShelfLayoutManagerTest, VisibleWhenStatusOrShelfFocused) {
  Shelf* shelf = GetPrimaryShelf();
  views::Widget* widget = CreateTestWidget();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Focus the shelf. Have to go through the focus cycler as normal focus
  // requests to it do nothing.
  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  widget->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Trying to activate the status should fail, since we only allow activating
  // it when the user is using the keyboard (i.e. through FocusCycler).
  GetShelfWidget()->status_area_widget()->Activate();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  GetShelfWidget()->GetFocusCycler()->RotateFocus(FocusCycler::FORWARD);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Checks that the status area follows along the auto-hidden shelf when the
// user swipes it up or down.
TEST_F(ShelfLayoutManagerTest, StatusAreaMoveWithSwipeOnAutoHiddenShelf) {
  Shelf* shelf = GetPrimaryShelf();
  CreateTestWidget();
  TabletModeControllerTestApi().EnterTabletMode();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  const int hidden_shelf_in_screen_portion =
      ShelfConfig::Get()->hidden_shelf_in_screen_portion();

  // The shelf is hidden. The status area should also be off-screen.
  EXPECT_EQ(hidden_shelf_in_screen_portion,
            GetWidgetOffsetFromBottom(shelf->status_area_widget()));

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point middle(start + gfx::Vector2d(0, -40));
  const gfx::Point end(start + gfx::Vector2d(0, -80));

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(start);
  generator->PressTouch();

  // The drag has just started, but we haven't moved yet.
  EXPECT_EQ(hidden_shelf_in_screen_portion,
            GetWidgetOffsetFromBottom(shelf->status_area_widget()));

  generator->MoveTouch(middle);

  // Now the status area should have entered the screen.
  const int status_area_visible_px_mid_gesture =
      GetWidgetOffsetFromBottom(shelf->status_area_widget());
  EXPECT_LT(hidden_shelf_in_screen_portion, status_area_visible_px_mid_gesture);

  // Finish the gesture, the status area should follow.
  generator->MoveTouch(end);
  generator->ReleaseTouch();

  const int status_area_visible_px_end_gesture =
      GetWidgetOffsetFromBottom(shelf->status_area_widget());
  EXPECT_LT(status_area_visible_px_mid_gesture,
            status_area_visible_px_end_gesture);

  // Now start swiping down. The status area should follow the other way.
  generator->MoveTouch(end);
  generator->PressTouch();
  EXPECT_EQ(status_area_visible_px_end_gesture,
            GetWidgetOffsetFromBottom(shelf->status_area_widget()));

  // And it should be back to off-screen after the gesture ends.
  generator->MoveTouch(start);
  generator->ReleaseTouch();

  EXPECT_EQ(hidden_shelf_in_screen_portion,
            GetWidgetOffsetFromBottom(shelf->status_area_widget()));
}

// Checks that the shelf keeps hidden during the Kiosk mode.
TEST_F(ShelfLayoutManagerTest, HiddenShelfInKioskMode_FullScreen) {
  SetUpKioskSession();

  // Create a window and make it full screen; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  window->SetProperty(kHideShelfWhenFullscreenKey, false);
  window->Show();
  wm::ActivateWindow(window);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());

  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
}

// Checks that the shelf keeps hidden during the Kiosk mode. (Some windows might
// not be fullscreen, e.g., the a11y setting window.)
TEST_F(ShelfLayoutManagerTest, HiddenShelfInKioskMode_Default) {
  SetUpKioskSession();

  // Create a default window; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(kHideShelfWhenFullscreenKey, false);
  window->Show();
  wm::ActivateWindow(window);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());

  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_HIDDEN, GetPrimaryShelf()->GetVisibilityState());
}

TEST_F(ShelfLayoutManagerTest,
       NavigationWidgetDoesNotMoveWithoutAutoHiddenShelf) {
  Shelf* shelf = GetPrimaryShelf();
  CreateTestWidget();
  TabletModeControllerTestApi().EnterTabletMode();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  gfx::Rect nav_widget_bounds =
      shelf->navigation_widget()->GetWindowBoundsInScreen();

  const gfx::Point end(nav_widget_bounds.top_center());
  const gfx::Point middle(end +
                          gfx::Vector2d(0, -nav_widget_bounds.height() / 2));
  const gfx::Point start(end + gfx::Vector2d(0, -nav_widget_bounds.height()));

  // Perform a drag down on the status area widget.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(start);
  generator->PressTouch();
  generator->MoveTouch(middle);
  EXPECT_EQ(nav_widget_bounds,
            shelf->navigation_widget()->GetWindowBoundsInScreen());
  generator->MoveTouch(end);
  EXPECT_EQ(nav_widget_bounds,
            shelf->navigation_widget()->GetWindowBoundsInScreen());
  generator->ReleaseTouch();
  EXPECT_EQ(nav_widget_bounds,
            shelf->navigation_widget()->GetWindowBoundsInScreen());
}

TEST_F(ShelfLayoutManagerTest, StatusWidgetDoesNotMoveWithoutAutoHiddenShelf) {
  Shelf* shelf = GetPrimaryShelf();
  CreateTestWidget();
  TabletModeControllerTestApi().EnterTabletMode();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  gfx::Rect status_widget_bounds =
      shelf->status_area_widget()->GetWindowBoundsInScreen();

  const gfx::Point end(status_widget_bounds.top_center());
  const gfx::Point middle(end +
                          gfx::Vector2d(0, -status_widget_bounds.height() / 2));
  const gfx::Point start(end +
                         gfx::Vector2d(0, -status_widget_bounds.height()));

  // Perform a drag down on the status area widget.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(start);
  generator->PressTouch();
  generator->MoveTouch(middle);
  EXPECT_EQ(status_widget_bounds,
            shelf->status_area_widget()->GetWindowBoundsInScreen());
  generator->MoveTouch(end);
  EXPECT_EQ(status_widget_bounds,
            shelf->status_area_widget()->GetWindowBoundsInScreen());
  generator->ReleaseTouch();
  EXPECT_EQ(status_widget_bounds,
            shelf->status_area_widget()->GetWindowBoundsInScreen());
}

// Checks that the navigation widget follows along the auto-hidden shelf when
// the user swipes it up or down.
TEST_F(ShelfLayoutManagerTest, NavigationWidgetMoveWithSwipeOnAutoHiddenShelf) {
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);

  Shelf* shelf = GetPrimaryShelf();
  CreateTestWidget();
  TabletModeControllerTestApi().EnterTabletMode();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  const int hidden_shelf_in_screen_portion =
      ShelfConfig::Get()->hidden_shelf_in_screen_portion();

  // The shelf is hidden. The navigation widget should also be off-screen.
  EXPECT_EQ(hidden_shelf_in_screen_portion,
            GetWidgetOffsetFromBottom(shelf->navigation_widget()));

  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point middle(start + gfx::Vector2d(0, -40));
  const gfx::Point end(start + gfx::Vector2d(0, -80));

  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveTouch(start);
  generator->PressTouch();

  // The drag has just started, but we haven't moved yet.
  EXPECT_EQ(hidden_shelf_in_screen_portion,
            GetWidgetOffsetFromBottom(shelf->navigation_widget()));

  generator->MoveTouch(middle);

  // Now the navigation widget should have entered the screen.
  const int navigation_visible_px_mid_gesture =
      GetWidgetOffsetFromBottom(shelf->navigation_widget());
  EXPECT_LT(hidden_shelf_in_screen_portion, navigation_visible_px_mid_gesture);

  // Verify that the navigation widget and status area moved the same amount.
  EXPECT_EQ(navigation_visible_px_mid_gesture,
            GetWidgetOffsetFromBottom(shelf->status_area_widget()));

  // Finish the gesture, the navigation widget should follow.
  generator->MoveTouch(end);
  generator->ReleaseTouch();

  const int navigation_visible_px_end_gesture =
      GetWidgetOffsetFromBottom(shelf->navigation_widget());
  EXPECT_LT(navigation_visible_px_mid_gesture,
            navigation_visible_px_end_gesture);

  // Now start swiping down. The navigation widget should follow the other way.
  generator->MoveTouch(end);
  generator->PressTouch();
  EXPECT_EQ(navigation_visible_px_end_gesture,
            GetWidgetOffsetFromBottom(shelf->navigation_widget()));

  // And it should be back to off-screen after the gesture ends.
  generator->MoveTouch(start);
  generator->ReleaseTouch();

  EXPECT_EQ(hidden_shelf_in_screen_portion,
            GetWidgetOffsetFromBottom(shelf->navigation_widget()));
}

// Ensure a SHELF_VISIBLE shelf stays visible when the app list is shown.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfVisibleState) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);

  // Create a normal unmaximized window; the shelf should be visible.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Show the app list and the shelf stays visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Hide the app list and the shelf stays visible.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
}

// Ensure a SHELF_AUTO_HIDE shelf is shown temporarily (SHELF_AUTO_HIDE_SHOWN)
// when the app list is shown, but the visibility state doesn't change.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfAutoHideState) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a normal unmaximized window; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  // The shelf's auto hide state won't be changed until the timer fires, so
  // force it to update now.
  GetShelfLayoutManager()->UpdateVisibilityState(/*force_layout=*/false);
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hide the app list and the shelf should be hidden again.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Makes sure that when we have dual displays, with one or both shelves are set
// to AutoHide, viewing the AppList on one of them doesn't unhide the other
// hidden shelf.
TEST_F(ShelfLayoutManagerTest, DualDisplayOpenAppListWithShelfAutoHideState) {
  // Create two displays.
  UpdateDisplay("0+0-200x300,+200+0-100x200");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows.size(), 2U);

  // Get the shelves in both displays and set them to be 'AutoHide'.
  Shelf* shelf_1 = Shelf::ForWindow(root_windows[0]);
  Shelf* shelf_2 = Shelf::ForWindow(root_windows[1]);
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->GetWindow()->GetRootWindow(),
            shelf_2->GetWindow()->GetRootWindow());
  shelf_1->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  shelf_1->shelf_layout_manager()->LayoutShelf();
  shelf_2->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  shelf_2->shelf_layout_manager()->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 = CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kMaximized);
  window_1->Show();
  aura::Window* window_2 = CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kMaximized);
  window_2->Show();

  EXPECT_EQ(shelf_1->GetWindow()->GetRootWindow(), window_1->GetRootWindow());
  EXPECT_EQ(shelf_2->GetWindow()->GetRootWindow(), window_2->GetRootWindow());

  // Activate one window in one display.
  wm::ActivateWindow(window_1);

  Shelf::UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());

  // Show the app list; only the shelf on the same display should be shown.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  Shelf::UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());

  // Hide the app list, both shelves should be hidden.
  GetAppListTestHelper()->DismissAndRunLoop();
  Shelf::UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());
}

// Ensure a SHELF_HIDDEN shelf (for a fullscreen window) is shown temporarily
// when the app list is shown, and hidden again when the app list is dismissed.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfHiddenState) {
  Shelf* shelf = GetPrimaryShelf();

  // Create a window and make it full screen; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  window->Show();
  wm::ActivateWindow(window);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hide the app list and the shelf should be hidden again.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
}

// Test that in tablet mode with auto hide enabled, opening a tray bubble while
// closing another keeps the hotseat hidden.
TEST_F(ShelfLayoutManagerTest, HotseatExtendingWhileClosingTrayBubble) {
  TabletModeControllerTestApi().EnterTabletMode();
  ui::test::EventGenerator* generator = GetEventGenerator();
  Shelf* shelf = GetPrimaryShelf();
  StatusAreaWidget* status_area = shelf->status_area_widget();
  status_area->ime_menu_tray()->SetVisiblePreferred(true);

  // Set the shelf to be auto hidden.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Create a widget. The shelf and hotseat should become hidden.
  CreateTestWidget()->GetNativeWindow();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Swipe up to show the auto-hide shelf, and extend the hotseat.
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Tap on the ime menu tray button, to show a tray bubble. The shelf
  // should be showing, but the hotseat should be hidden.
  EXPECT_FALSE(status_area->ime_menu_tray()->GetBubbleView());
  generator->GestureTapAt(
      status_area->ime_menu_tray()->GetBoundsInScreen().CenterPoint());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(status_area->ime_menu_tray()->GetBubbleView());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Tap on the unified system tray to open its bubble and close the ime menu
  // bubble. The hotseat should remain in the hidden state.
  EXPECT_FALSE(status_area->IsMessageBubbleShown());
  generator->GestureTapAt(
      status_area->unified_system_tray()->GetBoundsInScreen().CenterPoint());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(status_area->IsMessageBubbleShown());
  EXPECT_FALSE(status_area->ime_menu_tray()->GetBubbleView());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Check that the status bubble and shelf are hidden after tapping on the
  // in-app shelf.
  generator->GestureTapAt(
      GetShelfWidget()->GetVisibleShelfBounds().CenterPoint());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(status_area->IsMessageBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
}

// With a fullscreen window, ensure the hidden shelf is shown temporarily when
// the app list is shown and when tray bubbles are shown. Ensure that the shelf
// is hidden again once tray bubbles are closed.
TEST_F(ShelfLayoutManagerTest, OpenAppListInFullscreenWithShelfHiddenState) {
  Shelf* shelf = GetPrimaryShelf();
  StatusAreaWidget* status_area = shelf->status_area_widget();
  status_area->ime_menu_tray()->SetVisiblePreferred(true);

  // Create a window and make it full screen; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  window->Show();
  wm::ActivateWindow(window);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click on the ime menu tray button, to show a tray bubble. The shelf
  // should still be showing and the app list should hide.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      status_area->ime_menu_tray()->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  // Click away from the shelf and tray bubble to hide the shelf.
  generator->MoveMouseTo(10, 10);
  generator->ClickLeftButton();
  EXPECT_TRUE(RunVisibilityUpdateForTrayCallback());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click on the unified system tray button, opening the tray and hiding the
  // app list.
  EXPECT_FALSE(status_area->IsMessageBubbleShown());
  generator->MoveMouseTo(
      status_area->unified_system_tray()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(status_area->IsMessageBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  // Click away from the shelf and unified system tray to hide the shelf.
  generator->MoveMouseTo(10, 10);
  generator->ClickLeftButton();
  EXPECT_TRUE(RunVisibilityUpdateForTrayCallback());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click on the unified system tray button, closing the app list.
  EXPECT_FALSE(status_area->IsMessageBubbleShown());
  generator->MoveMouseTo(
      status_area->unified_system_tray()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(status_area->IsMessageBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  // Click on the ime menu tray button, to show a tray bubble and close the
  // unified system tray. The shelf should still be showing.
  generator->MoveMouseTo(
      status_area->ime_menu_tray()->GetBoundsInScreen().CenterPoint());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area->IsMessageBubbleShown());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Click away from the shelf and tray bubble to hide the shelf.
  generator->MoveMouseTo(10, 10);
  generator->ClickLeftButton();
  EXPECT_TRUE(RunVisibilityUpdateForTrayCallback());
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have a single display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowSingleDisplay) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  window->Show();
  wm::ActivateWindow(window);

  // Enable system modal dialog, and make sure shelf is still hidden.
  ShellTestApi().SimulateModalWindowOpenForTest(true);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(wm::CanActivateWindow(window));
  Shelf::UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have dual display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowDualDisplay) {
  // Create two displays.
  UpdateDisplay("200x300,100x200");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  // Get the shelves in both displays and set them to be 'AutoHide'.
  Shelf* shelf_1 = Shelf::ForWindow(root_windows[0]);
  Shelf* shelf_2 = Shelf::ForWindow(root_windows[1]);
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->GetWindow()->GetRootWindow(),
            shelf_2->GetWindow()->GetRootWindow());
  shelf_1->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  shelf_1->shelf_layout_manager()->LayoutShelf();
  shelf_2->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  shelf_2->shelf_layout_manager()->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 = CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kMaximized);
  window_1->Show();
  aura::Window* window_2 = CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kMaximized);
  window_2->Show();

  EXPECT_EQ(shelf_1->GetWindow()->GetRootWindow(), window_1->GetRootWindow());
  EXPECT_EQ(shelf_2->GetWindow()->GetRootWindow(), window_2->GetRootWindow());
  EXPECT_TRUE(window_1->IsVisible());
  EXPECT_TRUE(window_2->IsVisible());

  // Enable system modal dialog, and make sure both shelves are still hidden.
  ShellTestApi().SimulateModalWindowOpenForTest(true);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(wm::CanActivateWindow(window_1));
  EXPECT_FALSE(wm::CanActivateWindow(window_2));
  Shelf::UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());
}

TEST_F(ShelfLayoutManagerTest, FullscreenWidgetHidesShelf) {
  Shelf* shelf = GetPrimaryShelf();
  // Create a normal window.
  views::Widget* widget = TestWidgetBuilder()
                              .SetBounds(gfx::Rect(11, 22, 300, 400))
                              .BuildOwnedByNativeWidget();
  ASSERT_FALSE(widget->IsFullscreen());

  // Shelf defaults to visible.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Fullscreen window hides it.
  widget->SetFullscreen(true);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  // Restoring the window restores it.
  widget->Restore();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Clean up.
  widget->Close();
}

// Tests that the shelf is only hidden for a fullscreen window at the front and
// toggles visibility when another window is activated.
TEST_F(ShelfLayoutManagerTest, FullscreenWindowInFrontHidesShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Create a window and make it full screen.
  aura::Window* window1 = CreateTestWindow();
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey,
                       ui::mojom::WindowShowState::kFullscreen);
  window1->Show();
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  aura::Window* window2 = CreateTestWindow();
  window2->SetBounds(gfx::Rect(0, 0, 100, 100));
  window2->Show();

  WindowState::Get(window1)->Activate();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  WindowState::Get(window2)->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  WindowState::Get(window1)->Activate();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
}

// Test the behavior of the shelf when a window on one display is fullscreen
// but the other display has the active window.
TEST_F(ShelfLayoutManagerTest, FullscreenWindowOnSecondDisplay) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Create windows on either display.
  aura::Window* window1 = CreateTestWindow();
  window1->SetBoundsInScreen(gfx::Rect(0, 0, 100, 100),
                             display::Screen::GetScreen()->GetAllDisplays()[0]);
  window1->SetProperty(aura::client::kShowStateKey,
                       ui::mojom::WindowShowState::kFullscreen);
  window1->Show();

  aura::Window* window2 = CreateTestWindow();
  window2->SetBoundsInScreen(gfx::Rect(800, 0, 100, 100),
                             display::Screen::GetScreen()->GetAllDisplays()[1]);
  window2->Show();

  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  WindowState::Get(window2)->Activate();
  EXPECT_EQ(
      SHELF_HIDDEN,
      Shelf::ForWindow(window1)->shelf_layout_manager()->visibility_state());
  EXPECT_EQ(
      SHELF_VISIBLE,
      Shelf::ForWindow(window2)->shelf_layout_manager()->visibility_state());
}

// Test for Pinned mode.
TEST_F(ShelfLayoutManagerTest, PinnedWindowHidesShelf) {
  Shelf* shelf = GetPrimaryShelf();

  aura::Window* window1 = CreateTestWindow();
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->Show();

  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  window_util::PinWindow(window1, /* trusted */ false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  WindowState::Get(window1)->Restore();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
}

// Tests SHELF_ALIGNMENT_(LEFT, RIGHT).
TEST_F(ShelfLayoutManagerTest, SetAlignment) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  // Force an initial layout.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  shelf->SetAlignment(ShelfAlignment::kLeft);
  gfx::Rect shelf_bounds(GetShelfWidget()->GetWindowBoundsInScreen());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_GE(shelf_bounds.width(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(ShelfAlignment::kLeft, GetPrimaryShelf()->alignment());
  EXPECT_EQ(display.work_area(),
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  gfx::Rect status_bounds(status_area_widget->GetWindowBoundsInScreen());
  // TODO(estade): Re-enable this check. See crbug.com/660928.
  //  EXPECT_GE(
  //      status_bounds.width(),
  //      status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(display.bounds().x(), shelf_bounds.x());
  EXPECT_EQ(display.bounds().y(), shelf_bounds.y());
  EXPECT_EQ(display.bounds().height(), shelf_bounds.height());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(0, display.work_area().x());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  shelf->SetAlignment(ShelfAlignment::kRight);
  shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_GE(shelf_bounds.width(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(ShelfAlignment::kRight, GetPrimaryShelf()->alignment());
  EXPECT_EQ(display.work_area(),
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());

  status_bounds = gfx::Rect(status_area_widget->GetWindowBoundsInScreen());
  // TODO(estade): Re-enable this check. See crbug.com/660928.
  //  EXPECT_GE(
  //      status_bounds.width(),
  //      status_area_widget->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.GetWorkAreaInsets().top());
  EXPECT_EQ(0, display.GetWorkAreaInsets().bottom());
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(display.work_area().right(), shelf_bounds.x());
  EXPECT_EQ(display.bounds().y(), shelf_bounds.y());
  EXPECT_EQ(display.bounds().height(), shelf_bounds.height());

  const gfx::Rect stable_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.bounds().right() - display.work_area().right());
  EXPECT_EQ(stable_work_area,
            GetPrimaryWorkAreaInsets()->ComputeStableWorkArea());
}

// Verifies that the shelf looks the way it should after an alignment change.
// See crbug/1051824 .
TEST_F(ShelfLayoutManagerTest, ShelfWidgetLayoutUpdatedAfterAlignmentChange) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  ShelfWidget* shelf_widget = shelf->shelf_widget();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  shelf->SetAlignment(ShelfAlignment::kLeft);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  gfx::Rect opaque_background_bounds =
      shelf->shelf_widget()->GetOpaqueBackground()->bounds();
  ::wm::ConvertRectToScreen(shelf_widget->GetNativeWindow(),
                            &opaque_background_bounds);
  int cross_axis_visible_pixels =
      opaque_background_bounds.right() - display::Screen::GetScreen()
                                             ->GetPrimaryDisplay()
                                             .bounds()
                                             .left_center()
                                             .x();
  EXPECT_EQ(ShelfConfig::Get()->shelf_size(), cross_axis_visible_pixels);

  shelf->SetAlignment(ShelfAlignment::kRight);
  layout_manager->LayoutShelf();
  opaque_background_bounds =
      shelf->shelf_widget()->GetOpaqueBackground()->bounds();
  ::wm::ConvertRectToScreen(shelf_widget->GetNativeWindow(),
                            &opaque_background_bounds);
  cross_axis_visible_pixels =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds().right() -
      opaque_background_bounds.left_center().x();
  EXPECT_EQ(ShelfConfig::Get()->shelf_size(), cross_axis_visible_pixels);
}

// Tests swipe gestures in a various of shelf alignments and shelf auto hide
// configurations.
TEST_F(ShelfLayoutManagerTest, GestureDrag) {
  // Slop is an implementation detail of gesture recognition, and complicates
  // these tests. Ignore it.
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(0);
  Shelf* shelf = GetPrimaryShelf();
  {
    SCOPED_TRACE("BOTTOM");
    shelf->SetAlignment(ShelfAlignment::kBottom);
    gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
    gfx::Point bottom_center = shelf_bounds.bottom_center();
    bottom_center.Offset(0, -1);  // Make sure the point is inside shelf.
    RunGestureDragTests(bottom_center, shelf_bounds.top_center());
    GetAppListTestHelper()->WaitUntilIdle();
  }
  {
    SCOPED_TRACE("LEFT");
    shelf->SetAlignment(ShelfAlignment::kLeft);
    gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
    gfx::Point right_center = shelf_bounds.right_center();
    right_center.Offset(-1, 0);  // Make sure the point is inside shelf.
    RunGestureDragTests(shelf_bounds.left_center(), right_center);
    GetAppListTestHelper()->WaitUntilIdle();
  }
  {
    SCOPED_TRACE("RIGHT");
    shelf->SetAlignment(ShelfAlignment::kRight);
    gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
    gfx::Point right_center = shelf_bounds.right_center();
    right_center.Offset(-1, 0);  // Make sure the point is inside shelf.
    RunGestureDragTests(right_center, shelf_bounds.left_center());
    GetAppListTestHelper()->WaitUntilIdle();
  }
}

// Tests that the shelf does not "overscroll", that is, dragging the shelf in
// does not bring it past its ideal bounds.
TEST_F(ShelfLayoutManagerTest, ShelfDoesNotOverscrollDuringGestureDragIn) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();

  // Note: A window must be visible in order to hide the shelf.
  CreateTestWidget();
  {
    SCOPED_TRACE("BOTTOM");
    shelf->SetAlignment(ShelfAlignment::kBottom);
    gfx::Rect ideal_bounds = layout_manager->GetIdealBounds();
    ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

    // Scroll past the shelf's ideal bounds.
    gfx::Point start = GetPrimaryDisplay().bounds().bottom_center();
    StartScroll(start);
    UpdateScroll(gfx::Vector2d(0, -ideal_bounds.height() - 50));

    // The shelf should not extend past its ideal bounds.
    EXPECT_EQ(ideal_bounds, shelf->GetShelfBoundsInScreen());

    // End the scroll so as not to interfere with future tests.
    EndScroll(/*is_fling=*/false, 0.f);
  }
  {
    SCOPED_TRACE("LEFT");
    shelf->SetAlignment(ShelfAlignment::kLeft);
    gfx::Rect ideal_bounds = layout_manager->GetIdealBounds();
    ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

    // Scroll past the shelf's ideal bounds.
    gfx::Point start = GetPrimaryDisplay().bounds().left_center();
    StartScroll(start);
    UpdateScroll(gfx::Vector2d(ideal_bounds.width() + 50, 0));

    EXPECT_EQ(ideal_bounds, shelf->GetShelfBoundsInScreen());

    // End the scroll so as not to interfere with future tests.
    EndScroll(/*is_fling=*/false, 0.f);
  }
  {
    SCOPED_TRACE("RIGHT");
    shelf->SetAlignment(ShelfAlignment::kRight);
    gfx::Rect ideal_bounds = layout_manager->GetIdealBounds();
    ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

    // Scroll past the shelf's ideal bounds.
    gfx::Point start = GetPrimaryDisplay().bounds().right_center();
    StartScroll(start);
    UpdateScroll(gfx::Vector2d(-ideal_bounds.width() - 50, 0));

    EXPECT_EQ(ideal_bounds, shelf->GetShelfBoundsInScreen());

    // End the scroll so as not to interfere with future tests.
    EndScroll(/*is_fling=*/false, 0.f);
  }
}

// Swiping on shelf when fullscreen app list is opened should have no effect.
TEST_F(ShelfLayoutManagerTest, SwipingOnShelfIfAppListOpened) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->OnAppListVisibilityChanged(true, GetPrimaryDisplayId());
  EXPECT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Note: A window must be visible in order to hide the shelf.
  CreateTestWidget();

  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::Milliseconds(100);
  constexpr int kNumScrollSteps = 4;
  gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();

  // Swiping down on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  gfx::Point end = start + gfx::Vector2d(0, 120);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  // Swiping left on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  end = start + gfx::Vector2d(-120, 0);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  // Swiping right on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  shelf->SetAlignment(ShelfAlignment::kRight);
  end = start + gfx::Vector2d(120, 0);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
}

TEST_F(ShelfLayoutManagerTest, WindowVisibilityDisablesAutoHide) {
  UpdateDisplay("800x600,800x600");
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a visible window so auto-hide behavior is enforced
  views::Widget* dummy = CreateTestWidget();

  // Window visible => auto hide behaves normally.
  layout_manager->UpdateVisibilityState(/*force_layout=*/false);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Window minimized => auto hide disabled.
  dummy->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Window closed => auto hide disabled.
  dummy->CloseNow();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Multiple window test
  views::Widget* window1 = CreateTestWidget();
  views::Widget* window2 = CreateTestWidget();

  // both visible => normal autohide
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // either minimzed => normal autohide
  window2->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  window2->Restore();
  window1->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // both minimized => disable auto hide
  window2->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Test moving windows to/from other display.
  window2->Restore();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  // Move to second display.
  window2->SetBounds(gfx::Rect(850, 50, 50, 50));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  // Move back to primary display.
  window2->SetBounds(gfx::Rect(50, 50, 50, 50));
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests the shelf animates back to its original visible bounds when it is
// dragged down but there are no visible windows.
TEST_F(ShelfLayoutManagerTest,
       ShelfAnimatesWhenGestureCompleteNoVisibleWindow) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  gfx::Rect visible_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  {
    // Enable animations so that we can make sure that they occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    ui::test::EventGenerator* generator = GetEventGenerator();
    gfx::Rect shelf_bounds_in_screen =
        GetShelfWidget()->GetWindowBoundsInScreen();
    gfx::Point start(shelf_bounds_in_screen.CenterPoint());
    gfx::Point end(start.x(), shelf_bounds_in_screen.bottom());
    views::WidgetAnimationWaiter waiter(GetShelfWidget(), visible_bounds);
    generator->GestureScrollSequence(start, end, base::Milliseconds(10), 5);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

    // Wait for the animation to complete and check that it was valid.
    waiter.WaitForAnimation();
    EXPECT_TRUE(waiter.WasValidAnimation());
  }
}

// Tests that the shelf animates to the visible bounds after a swipe up on
// the auto hidden shelf.
TEST_F(ShelfLayoutManagerTest, ShelfAnimatesToVisibleWhenGestureInComplete) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  gfx::Rect visible_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  // Create a visible window, otherwise the shelf will not hide.
  CreateTestWidget();

  // Get the bounds of the shelf when it is hidden.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  {
    // Enable the animations so that we can make sure they do occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    gfx::Point start = display.bounds().bottom_center();
    gfx::Point end(
        start.x(),
        start.y() - (Shell::Get()->shelf_config()->shelf_size() - 1));
    ui::test::EventGenerator* generator = GetEventGenerator();

    views::WidgetAnimationWaiter waiter(GetShelfWidget(), visible_bounds);
    generator->GestureScrollSequence(start, end, base::Milliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
    waiter.WaitForAnimation();
    EXPECT_TRUE(waiter.WasValidAnimation());
  }
}

// Tests that the shelf hide immediately without animation after a swipe down
// on the visible shelf to the bottom of the display bounds.
TEST_F(ShelfLayoutManagerTest,
       ShelfHidesImmediatelyWhenGestureOutCompleteBelowTargetBounds) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Create a visible window, otherwise the shelf will not hide.
  CreateTestWidget();

  gfx::Rect auto_hidden_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  {
    // Enable the animations so that we can make sure they do occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    ui::test::EventGenerator* generator = GetEventGenerator();

    // Show the shelf first.
    SwipeUpOnShelf();
    EXPECT_FALSE(GetShelfWidget()->GetLayer()->GetAnimator()->is_animating());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

    gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().top_center();

    // Set the endpoint below the display bounds of the shelf widget.
    gfx::Point endpoint_below_target(start.x(), start.y() + 100);

    // Now swipe down to the endpoint.
    generator->GestureScrollSequence(start, endpoint_below_target,
                                     base::Milliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

    // Check that the shelf widget is moved back to the auto hidden bounds
    // without animating.
    EXPECT_FALSE(GetShelfWidget()->GetLayer()->GetAnimator()->is_animating());
    EXPECT_EQ(auto_hidden_bounds, GetShelfWidget()->GetWindowBoundsInScreen());
  }
}

// Tests that the shelf animates to the auto hidden bounds after a swipe down
// on the visible shelf to a point above the auto hidden bounds.
TEST_F(ShelfLayoutManagerTest,
       ShelfAnimatesToHiddenWhenGestureOutCompleteAboveTargetBounds) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Create a visible window, otherwise the shelf will not hide.
  CreateTestWidget();

  gfx::Rect auto_hidden_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  {
    // Enable the animations so that we can make sure they do occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    ui::test::EventGenerator* generator = GetEventGenerator();

    // Show the shelf first.
    SwipeUpOnShelf();
    EXPECT_FALSE(GetShelfWidget()->GetLayer()->GetAnimator()->is_animating());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

    gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().top_center();

    // Set the end point to a point above the target bounds which is
    // hidden_shelf_in_screen_portion pixels high.
    const gfx::Rect display_bounds =
        display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
    gfx::Point endpoint_above_target = gfx::Point(
        display_bounds.bottom_center().x(),
        display_bounds.bottom_center().y() -
            ShelfConfig::Get()->hidden_shelf_in_screen_portion() - 1);

    // Now swipe down to the endpoint.
    views::WidgetAnimationWaiter waiter2(GetShelfWidget(), auto_hidden_bounds);
    generator->GestureScrollSequence(start, endpoint_above_target,
                                     base::Milliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
    waiter2.WaitForAnimation();

    // Check if the shelf widget is animated to the auto hidden bounds after the
    // drag.
    EXPECT_TRUE(waiter2.WasValidAnimation());
  }
}

TEST_F(ShelfLayoutManagerTest, AutohideShelfForAutohideWhenActiveWindow) {
  Shelf* shelf = GetPrimaryShelf();

  views::Widget* widget_one = CreateTestWidget();
  views::Widget* widget_two = CreateTestWidget();
  aura::Window* window_two = widget_two->GetNativeWindow();

  // Turn on hide_shelf_when_active behavior for window two - shelf should
  // still be visible when window two is made active since it is not yet
  // maximized.
  widget_one->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  WindowState::Get(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(true);
  widget_two->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Now the flag takes effect once window two is maximized.
  widget_two->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  // The hide_shelf_when_active flag should override the behavior of the
  // hide_shelf_when_fullscreen flag even if the window is currently fullscreen.
  WindowState::Get(window_two)->SetHideShelfWhenFullscreen(false);
  widget_two->SetFullscreen(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  WindowState::Get(window_two)->Restore();

  // With the flag off, shelf no longer auto-hides.
  widget_one->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  WindowState::Get(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(false);
  widget_two->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  WindowState::Get(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(true);
  window_two->SetProperty(aura::client::kZOrderingKey,
                          ui::ZOrderLevel::kFloatingWindow);

  auto* shelf_window = shelf->GetWindow();
  aura::Window* container = shelf_window->GetRootWindow()->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  EXPECT_TRUE(base::Contains(container->children(), window_two));

  widget_two->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());
}

TEST_F(ShelfLayoutManagerTest, ShelfFlickerOnTrayActivation) {
  Shelf* shelf = GetPrimaryShelf();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  // Turn on auto-hide for the shelf.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the status menu. That should make the shelf visible again.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kToggleSystemTrayBubble, {});
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

TEST_F(ShelfLayoutManagerTest, WorkAreaChangeWorkspace) {
  // Make sure the shelf is always visible.
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  layout_manager->LayoutShelf();

  views::Widget* widget_one = CreateTestWidget();
  widget_one->Maximize();

  views::Widget* widget_two = CreateTestWidget();
  widget_two->Maximize();
  widget_two->Activate();

  // Both windows are maximized. They should be of the same size.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  int area_when_shelf_shown =
      widget_one->GetNativeWindow()->bounds().size().GetArea();

  // Now hide the shelf.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Both windows should be resized according to the shelf status.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  // Resized to small.
  EXPECT_LT(area_when_shelf_shown,
            widget_one->GetNativeWindow()->bounds().size().GetArea());

  // Now show the shelf.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);

  // Again both windows should be of the same size.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  EXPECT_EQ(area_when_shelf_shown,
            widget_one->GetNativeWindow()->bounds().size().GetArea());
}

TEST_F(ShelfLayoutManagerTest, BackgroundTypeWhenLockingScreen) {
  // Creates a maximized window to have a background type other than default.
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  window->Show();
  wm::ActivateWindow(window.get());
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            GetShelfLayoutManager()->shelf_background_type());

  Shell::Get()->lock_state_controller()->LockWithoutAnimation();
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());
}

TEST_F(ShelfLayoutManagerTest, WorkspaceMask) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Overlaps with shelf should not cause any specific behavior.
  w1->SetBounds(GetShelfLayoutManager()->GetIdealBounds());
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  w2->Show();
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w2.reset();
  EXPECT_EQ(WorkspaceWindowState::kFullscreen, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(WorkspaceWindowState::kDefault, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());
}

TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColor) {
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());
  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            GetShelfLayoutManager()->shelf_background_type());

  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  // Overlaps with shelf.
  w2->SetBounds(GetShelfLayoutManager()->GetIdealBounds());

  // Still background is 'maximized'.
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            GetShelfLayoutManager()->shelf_background_type());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMinimized);
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());
  w2->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMinimized);
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            GetShelfLayoutManager()->shelf_background_type());

  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  w3->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kWindow);
  ::wm::AddTransientChild(w1.get(), w3.get());
  w3->Show();
  wm::ActivateWindow(w3.get());

  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w3.reset();
  w1.reset();
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());
}

// Tests that the shelf background gets updated when the AppList stays open
// during the tablet mode transition with a visible window.
TEST_F(ShelfLayoutManagerTest, TabletModeTransitionWithAppListVisible) {
  // Home Launcher requires an internal display.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();

  // Show a window, which will later fill the whole screen.
  std::unique_ptr<aura::Window> window(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      aura::client::kResizeBehaviorCanResize |
                          aura::client::kResizeBehaviorCanMaximize);
  wm::ActivateWindow(window.get());

  // Show the AppList over |window|.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Transition to tablet mode.
  TabletModeControllerTestApi().EnterTabletMode();

  // |window| should be maximized, and the shelf background should match the
  // maximized state.
  EXPECT_EQ(WorkspaceWindowState::kMaximized, GetWorkspaceWindowState());
  EXPECT_EQ(ShelfBackgroundType::kInApp,
            GetShelfLayoutManager()->shelf_background_type());

  // Hiding the window should exit app mode.
  window->Hide();
  EXPECT_EQ(ShelfBackgroundType::kHomeLauncher,
            GetShelfLayoutManager()->shelf_background_type());
}

// Verify that the auto-hide shelf has default background by default and still
// has the default background when a window is maximized in clamshell mode.
TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColorAutoHide) {
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());

  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());
}

// Verify that the shelf has a maximized background when a window is in the
// fullscreen state.
TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColorFullscreen) {
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            GetShelfLayoutManager()->shelf_background_type());

  w1->SetProperty(aura::client::kShowStateKey,
                  ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(ShelfBackgroundType::kMaximized,
            GetShelfLayoutManager()->shelf_background_type());
}

// Verify the hit bounds of the status area extend to the edge of the shelf.
TEST_F(ShelfLayoutManagerTest, StatusAreaHitBoxCoversEdge) {
  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  ui::test::EventGenerator* generator = GetEventGenerator();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Rect inset_display_bounds = display.bounds();
  inset_display_bounds.Inset(gfx::Insets::TLBR(0, 0, 1, 1));

  // Test bottom right pixel for bottom alignment.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kBottom);
  generator->MoveMouseTo(inset_display_bounds.bottom_right());
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom right pixel for right alignment.
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kRight);
  generator->MoveMouseTo(inset_display_bounds.bottom_right());
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom left pixel for left alignment.
  generator->MoveMouseTo(inset_display_bounds.bottom_left());
  GetPrimaryShelf()->SetAlignment(ShelfAlignment::kLeft);
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
}

// Tests that when the auto-hide behaviour is changed during an animation the
// target bounds are updated to reflect the new state.
TEST_F(ShelfLayoutManagerTest,
       ShelfAutoHideToggleDuringAnimationUpdatesBounds) {
  aura::Window* status_window =
      GetShelfWidget()->status_area_widget()->GetNativeView();
  gfx::Rect initial_bounds = status_window->bounds();

  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);
  gfx::Rect hide_target_bounds = status_window->GetTargetBounds();
  EXPECT_GT(hide_target_bounds.y(), initial_bounds.y());

  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  gfx::Rect reshow_target_bounds = status_window->GetTargetBounds();
  EXPECT_EQ(initial_bounds, reshow_target_bounds);
}

// Tests that during shutdown, that window activation changes are properly
// handled, and do not crash (crbug.com/458768)
TEST_F(ShelfLayoutManagerTest, ShutdownHandlesWindowActivation) {
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  aura::Window* window1 = CreateTestWindowInShellWithId(0);
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey,
                       ui::mojom::WindowShowState::kMaximized);
  window1->Show();
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(0));
  window2->SetBounds(gfx::Rect(0, 0, 100, 100));
  window2->Show();
  wm::ActivateWindow(window1);

  GetShelfLayoutManager()->PrepareForShutdown();

  // Deleting a focused maximized window will switch focus to |window2|. This
  // would normally cause the ShelfLayoutManager to update its state. However
  // during shutdown we want to handle this without crashing.
  delete window1;
}

TEST_F(ShelfLayoutManagerTest, ShelfLayoutInUnifiedDesktop) {
  Shell::Get()->display_manager()->SetUnifiedDesktopEnabled(true);
  UpdateDisplay("500x400, 500x400");

  // When the unified desktop is enabled, UpdateDisplay() adds a display so the
  // shelf is recreated. Therefore, update the shelf related data members.
  UpdateShelfRelatedMembers();

  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  EXPECT_TRUE(status_area_widget->IsVisible());
  // Shelf should be in the first display's area.
  gfx::Rect status_area_bounds(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_TRUE(gfx::Rect(0, 0, 500, 400).Contains(status_area_bounds));
  EXPECT_EQ(gfx::Point(500, 400), status_area_bounds.bottom_right());
}

// Tests that tapping the home button is successful on the autohidden shelf.
TEST_F(ShelfLayoutManagerTest, PressHomeButtonOnAutoHideShelf) {
  // Enable accessibility feature that forces home button to be shown even with
  // kHideShelfControlsInTabletMode enabled.
  Shell::Get()
      ->accessibility_controller()
      ->SetTabletModeShelfNavigationButtonsEnabled(true);
  TabletModeControllerTestApi().EnterTabletMode();
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  // Create a window to hide the shelf in auto-hide mode.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  ShelfNavigationWidget::TestApi navigation_test_api(
      shelf->navigation_widget());
  ASSERT_TRUE(navigation_test_api.IsHomeButtonVisible());
  // Wait for the back button to finish animating from behind the home button.
  ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting())
      .RunMessageLoopUntilAnimationsDone(
          navigation_test_api.GetBoundsAnimator());

  // Press the home button with touch.
  GetEventGenerator()->GestureTapAt(shelf->shelf_widget()
                                        ->navigation_widget()
                                        ->GetHomeButton()
                                        ->GetBoundsInScreen()
                                        .CenterPoint());

  // The app list should now be visible, and the window we created should hide.
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_FALSE(window->IsVisible());
}

// Tests that the auto-hide shelf has expected behavior when pressing the
// AppList button while the shelf is being dragged by gesture (see
// https://crbug.com/953877).
TEST_F(ShelfLayoutManagerTest, PressHomeBtnWhenAutoHideShelfBeingDragged) {
  // Create a widget to hide the shelf in auto-hide mode.
  CreateTestWidget();
  GetPrimaryShelf()->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(GetPrimaryShelf()->IsVisible());

  // Emulate to drag the shelf to show it.
  gfx::Point gesture_location = display::Screen::GetScreen()
                                    ->GetPrimaryDisplay()
                                    .bounds()
                                    .bottom_center();
  int delta_y = -1;
  base::TimeTicks timestamp = base::TimeTicks::Now();

  ui::GestureEvent start_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, delta_y));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &start_event, GetShelfWidget()->GetNativeView());
  gesture_location.Offset(0, delta_y);

  // Ensure that Shelf is higher than the default height, required by the bug
  // reproduction procedures.
  delta_y = -ShelfConfig::Get()->shelf_size() - 1;

  timestamp += base::Milliseconds(200);
  ui::GestureEvent update_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 0, delta_y));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &update_event, GetShelfWidget()->GetNativeView());

  // Emulate to press the AppList button while dragging the Shelf.
  PressHomeButton();
  EXPECT_TRUE(GetPrimaryShelf()->IsVisible());

  // Release the press.
  delta_y -= 1;
  gesture_location.Offset(0, delta_y);
  timestamp += base::Milliseconds(200);
  ui::GestureEvent scroll_end_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &scroll_end_event, GetShelfWidget()->GetNativeView());
  ui::GestureEvent gesture_end_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::EventType::kGestureEnd));
  GetShelfLayoutManager()->ProcessGestureEventOfAutoHideShelf(
      &gesture_end_event, GetShelfWidget()->GetNativeView());

  // Press the AppList button to hide the AppList and Shelf. Check the following
  // things:
  // (1) Shelf is hidden
  // (2) Shelf has correct bounds in screen coordinates.
  PressHomeButton();
  EXPECT_EQ(
      GetScreenAvailableBounds().bottom_left() +
          gfx::Point(0, -ShelfConfig::Get()->hidden_shelf_in_screen_portion())
              .OffsetFromOrigin(),
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen().origin());
  EXPECT_FALSE(GetPrimaryShelf()->IsVisible());
}

// Tests that the shelf has expected bounds when dragging the shelf by gesture
// and pressing the AppList button by mouse during drag (see
// https://crbug.com/968768).
TEST_F(ShelfLayoutManagerTest, MousePressAppListBtnWhenShelfBeingDragged) {
  // Drag the shelf upward. Notice that in order to drag shelf instead of
  // AppList from shelf, we need to drag the shelf downward a little bit then
  // upward. Because the bug is related with RootView, the event should be sent
  // through the shelf widget.
  gfx::Point gesture_location =
      GetPrimaryShelf()->GetShelfViewForTesting()->bounds().CenterPoint();
  int delta_y = 1;
  base::TimeTicks timestamp = base::TimeTicks::Now();
  ui::GestureEvent start_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, delta_y));
  GetPrimaryShelf()->shelf_widget()->OnGestureEvent(&start_event);
  delta_y = -5;
  timestamp += base::Milliseconds(200);
  ui::GestureEvent update_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, 0, delta_y));
  GetPrimaryShelf()->shelf_widget()->OnGestureEvent(&update_event);

  // Press the AppList button by mouse.
  views::View* home_button =
      GetPrimaryShelf()->navigation_widget()->GetHomeButton();
  GetEventGenerator()->MoveMouseTo(
      home_button->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->ClickLeftButton();

  // End the gesture event.
  delta_y -= 1;
  gesture_location.Offset(0, delta_y);
  timestamp += base::Milliseconds(200);
  ui::GestureEvent scroll_end_event = ui::GestureEvent(
      gesture_location.x(), gesture_location.y(), ui::EF_NONE, timestamp,
      ui::GestureEventDetails(ui::EventType::kGestureScrollEnd));
  GetPrimaryShelf()->shelf_widget()->OnGestureEvent(&scroll_end_event);

  // Verify that the shelf has expected bounds.
  EXPECT_EQ(
      GetScreenAvailableBounds().bottom_left() +
          gfx::Point(0, -ShelfConfig::Get()->shelf_size()).OffsetFromOrigin(),
      GetPrimaryShelf()->shelf_widget()->GetWindowBoundsInScreen().origin());
}

// Tests that tap outside of the AUTO_HIDE_SHOWN shelf should hide it.
TEST_F(ShelfLayoutManagerTest, TapOutsideOfAutoHideShownShelf) {
  views::Widget* widget = CreateTestWidget();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect window_bounds = window->GetBoundsInScreen();
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap outside the window and AUTO_HIDE_SHOWN shelf should hide the shelf.
  gfx::Point tap_location =
      window_bounds.bottom_right() + gfx::Vector2d(10, 10);
  generator->GestureTapAt(tap_location);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  SwipeUpOnShelf();

  // Tap inside the AUTO_HIDE_SHOWN shelf should not hide the shelf.
  gfx::Rect shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  tap_location = gfx::Point(shelf_bounds.CenterPoint());
  generator->GestureTapAt(tap_location);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap inside the window should still hide the shelf.
  tap_location = window_bounds.origin() + gfx::Vector2d(10, 10);
  generator->GestureTapAt(tap_location);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  SwipeUpOnShelf();

  // Tap the system tray that inside the status area should not hide the shelf
  // but open the systrem tray bubble.
  generator->GestureTapAt(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

// Tests that swiping up on the AUTO_HIDE_HIDDEN shelf, with various speeds,
// offsets, and angles, always shows the shelf.
TEST_F(ShelfLayoutManagerTest, SwipeUpAutoHideHiddenShelf) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  Shelf* shelf = GetPrimaryShelf();

  // Create a window so that the shelf will hide.
  const aura::Window* window = CreateTestWidget()->GetNativeWindow();
  const gfx::Point tap_to_hide_shelf_location =
      window->GetBoundsInScreen().CenterPoint();
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  const int time_deltas[] = {10, 50, 100, 500};
  const int num_scroll_steps[] = {2, 5, 10, 50};
  const int x_offsets[] = {10, 20, 50};
  const int y_offsets[] = {70, 100, 300, 500};

  for (int time_delta : time_deltas) {
    for (int scroll_steps : num_scroll_steps) {
      for (int x_offset : x_offsets) {
        for (int y_offset : y_offsets) {
          const gfx::Point start(display_bounds.bottom_center());
          const gfx::Point end(start + gfx::Vector2d(x_offset, -y_offset));
          generator->GestureScrollSequence(
              start, end, base::Milliseconds(time_delta), scroll_steps);
          EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState())
              << "Failure to show shelf after a swipe up in " << time_delta
              << "ms, " << scroll_steps << " steps, " << x_offset
              << " X-offset and " << y_offset << " Y-offset.";
          generator->GestureTapAt(tap_to_hide_shelf_location);
          EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
        }
      }
    }
  }
}

// Tests the auto-hide shelf status when moving the mouse in and out.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfOnMouseMove) {
  // Create one window, or the shelf won't auto-hide.
  CreateTestWidget();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();

  // Set the shelf to auto-hide.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up to show the shelf.
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Move the mouse far away from the shelf, but without having been on the
  // shelf first. This isn't technically a mouse-out event, so the shelf should
  // not hide.
  generator->MoveMouseTo(0, 0);
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Now place the mouse on the shelf, then move away. The shelf should hide.
  generator->MoveMouseTo(1, display.bounds().bottom() - 1);
  generator->MoveMouseTo(0, 0);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Now let's show the shelf again.
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Move the mouse away, but move it back within the shelf immediately. The
  // shelf should remain shown.
  generator->MoveMouseTo(0, 0);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  generator->MoveMouseTo(1, display.bounds().bottom() - 1);
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Verifies that after showing the system tray by shortcut, the shelf item still
// responds to the gesture event. (see https://crbug.com/921182)
TEST_F(ShelfLayoutManagerTest, ShelfItemRespondToGestureEvent) {
  // Prepare for the auto-hide shelf test.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(0, 0);

  ShelfTestUtil::AddAppShortcut("app_id", TYPE_APP);
  ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting())
      .RunMessageLoopUntilAnimationsDone();

  // Turn on the auto-hide mode for shelf. Check the initial states.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the system tray with the shortcut. Expect that the shelf is shown
  // after triggering the accelerator.
  PressAndReleaseKey(ui::VKEY_S, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  GetAppListTestHelper()->WaitUntilIdle();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Tap on the shelf button. Expect that the shelf button responds to gesture
  // events.
  base::UserActionTester user_action_tester;
  user_action_tester.ResetCounts();
  views::View* button = GetPrimaryShelf()
                            ->GetShelfViewForTesting()
                            ->first_visible_button_for_testing();
  gfx::Point shelf_btn_center = button->GetBoundsInScreen().CenterPoint();
  generator->GestureTapAt(shelf_btn_center);
  EXPECT_EQ(1, user_action_tester.GetActionCount("Launcher_ClickOnApp"));
}

// Tests the auto-hide shelf status with mouse events.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfOnMouseEvents) {
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  Shelf* shelf = GetPrimaryShelf();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(0, 0);

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up to show the auto-hide shelf.
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Move the mouse should not hide the AUTO_HIDE_SHOWN shelf immediately.
  generator->MoveMouseTo(5, 5);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Mouse press outside the shelf should hide the AUTO_HIDE_SHOWN shelf.
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Move the mouse to the position which is contained by the bounds of the
  // shelf when it is visible should show the auto-hide shelf.
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const int display_bottom = display.bounds().bottom();
  generator->MoveMouseTo(1, display_bottom - 1);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Mouse press inside the shelf should not hide the AUTO_HIDE_SHOWN shelf.
  generator->MoveMouseTo(
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Mouse press the system tray should open the system tray bubble.
  generator->MoveMouseTo(
      GetPrimaryUnifiedSystemTray()->GetBoundsInScreen().CenterPoint());
  generator->PressLeftButton();
  generator->ReleaseLeftButton();
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

// Tests that tap shelf item in auto-hide shelf should do nothing.
TEST_F(ShelfLayoutManagerTest, TapShelfItemInAutoHideShelf) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  // Create a normal unmaximized window; the shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Tap home button should not open the app list and shelf should keep
  // hidden.
  gfx::Rect home_button_bounds = shelf->shelf_widget()
                                     ->navigation_widget()
                                     ->GetHomeButton()
                                     ->GetBoundsInScreen();
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  home_button_bounds.Intersect(display_bounds);
  GetEventGenerator()->GestureTapAt(home_button_bounds.CenterPoint());
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests the a11y feedback for entering/exiting fullscreen workspace state.
TEST_F(ShelfLayoutManagerTest, A11yAlertOnWorkspaceState) {
  TestAccessibilityControllerClient client;
  std::unique_ptr<aura::Window> window1(
      AshTestBase::CreateToplevelTestWindow());
  std::unique_ptr<aura::Window> window2(
      AshTestBase::CreateToplevelTestWindow());
  EXPECT_NE(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());

  // Toggle the current normal window in workspace to fullscreen should send the
  // ENTERED alert.
  const WMEvent fullscreen(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState* window_state2 = WindowState::Get(window2.get());
  window_state2->OnWMEvent(&fullscreen);
  EXPECT_TRUE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());

  // Toggle the current fullscreen'ed window in workspace to exit fullscreen
  // should send the EXITED alert.
  window_state2->OnWMEvent(&fullscreen);
  EXPECT_FALSE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_EXITED,
            client.last_a11y_alert());

  // Fullscreen the |window2| again to prepare for the following tests.
  window_state2->OnWMEvent(&fullscreen);
  EXPECT_TRUE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());
  // Changes the current window in workspace from a fullscreen window to a
  // normal window should send the EXITD alert.
  window_state2->Minimize();
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_EXITED,
            client.last_a11y_alert());

  // Changes the current window in workspace from a normal window to fullscreen
  // window should send ENTERED alert.
  window_state2->Unminimize();
  EXPECT_TRUE(window_state2->IsFullscreen());
  EXPECT_EQ(AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED,
            client.last_a11y_alert());
}

// Verifies the auto-hide shelf is shown if there is only a single PIP window.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfShownForSinglePipWindow) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Create a PIP window.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  // Set always on top so it is put in the PIP container.
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);
  window->Show();
  const WMEvent pip_event(WM_EVENT_PIP);
  WindowState::Get(window)->OnWMEvent(&pip_event);
  Shelf::UpdateShelfVisibility();

  // Expect the shelf to be hidden.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Verifies that shelf components are placed properly in right-to-left UI.
TEST_F(ShelfLayoutManagerTest, RtlPlacement) {
  // Helper function to check that the given widget is placed symmetrically
  // between LTR and RTL.
  auto check_mirrored_placement = [](views::Widget* widget) {
    base::i18n::SetICUDefaultLocale("en");
    EXPECT_FALSE(base::i18n::IsRTL());
    GetShelfLayoutManager()->LayoutShelf();
    const int ltr_left_position =
        widget->GetNativeWindow()->GetBoundsInScreen().x();

    base::i18n::SetICUDefaultLocale("ar");
    EXPECT_TRUE(base::i18n::IsRTL());
    GetShelfLayoutManager()->LayoutShelf();
    const int rtl_right_position =
        widget->GetNativeWindow()->GetBoundsInScreen().right();

    EXPECT_EQ(
        GetShelfWidget()->GetWindowBoundsInScreen().width() - ltr_left_position,
        rtl_right_position);
  };

  const std::string locale = base::i18n::GetConfiguredLocale();

  ShelfWidget* shelf_widget = GetPrimaryShelf()->shelf_widget();
  check_mirrored_placement(shelf_widget->navigation_widget());
  check_mirrored_placement(shelf_widget->status_area_widget());
  check_mirrored_placement(shelf_widget);

  // Reset the lauguage setting.
  base::i18n::SetICUDefaultLocale(locale);
}

// Tests the auto-hide shelf status when opening and closing a context menu.
TEST_F(ShelfLayoutManagerTest, AutoHideShelfWithContextMenu) {
  // Create one window, or the shelf won't auto-hide.
  CreateTestWidget();

  // Set the shelf to auto-hide.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Create an app that we can use to pull up a context menu.
  EXPECT_EQ(0u,
            ShelfViewTestAPI(shelf->GetShelfViewForTesting()).GetButtonCount());
  AddApp();
  EXPECT_EQ(1u,
            ShelfViewTestAPI(shelf->GetShelfViewForTesting()).GetButtonCount());

  // Swipe up to show the shelf.
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Open hotseat context menu.
  ui::test::EventGenerator* generator = GetEventGenerator();
  ShelfAppButton* clickable_app_button =
      ShelfViewTestAPI(shelf->GetShelfViewForTesting()).GetButton(0);
  EXPECT_TRUE(clickable_app_button);
  EXPECT_FALSE(shelf->shelf_widget()->IsShowingMenu());
  generator->MoveMouseTo(
      clickable_app_button->GetBoundsInScreen().CenterPoint());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  generator->ClickRightButton();
  EXPECT_TRUE(shelf->shelf_widget()->IsShowingMenu());

  // Close the context menu with the mouse over the shelf. The shelf should
  // remain shown.
  generator->ClickRightButton();
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_FALSE(shelf->shelf_widget()->IsShowingMenu());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Reopen hotseat context menu.
  generator->ClickRightButton();
  EXPECT_TRUE(shelf->shelf_widget()->IsShowingMenu());

  // Mouse away from the shelf with the context menu still showing. The shelf
  // should remain shown.
  generator->MoveMouseTo(0, 0);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Close the context menu with the mouse away from the shelf. The shelf
  // should hide.
  generator->ClickRightButton();
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_FALSE(shelf->shelf_widget()->IsShowingMenu());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

namespace {

class DragTestView : public views::View {
 public:
  DragTestView() = default;
  DragTestView(const DragTestView&) = delete;
  DragTestView& operator=(const DragTestView&) = delete;
  ~DragTestView() override = default;

  int DragThreshold() { return views::View::GetVerticalDragThreshold(); }

 private:
  // views::View:
  int GetDragOperations(const gfx::Point& press_pt) override {
    return ui::DragDropTypes::DRAG_COPY;
  }

  void WriteDragData(const gfx::Point& p, OSExchangeData* data) override {
    gfx::ImageSkiaRep image_rep(gfx::Size(1, 1), 1.0f);
    gfx::ImageSkia image_skia(image_rep);
    data->provider().SetDragImage(image_skia, gfx::Vector2d());
  }
};

enum class DragEventType {
  kMouse,
  kGesture,
};

}  // namespace

// Shelf tests parametrized to perform drags using mouse and gesture events.
class ShelfLayoutManagerDragDropTest
    : public ShelfLayoutManagerTestBase,
      public testing::WithParamInterface<DragEventType> {
 public:
  ShelfLayoutManagerDragDropTest() = default;

  void SetUp() override {
    ShelfLayoutManagerTestBase::SetUp();

    auto* drag_drop_controller =
        static_cast<DragDropController*>(aura::client::GetDragDropClient(
            GetPrimaryShelf()->GetWindow()->GetRootWindow()));
    drag_drop_controller->set_should_block_during_drag_drop(false);
    generator_ = GetEventGenerator();
  }

  // Moves to `view` and mouse/gesture presses it. Does not actually start
  // dragging `view` to a different location.
  void StartDrag(DragTestView* view) {
    if (GetParam() == DragEventType::kMouse) {
      generator_->MoveMouseTo(view->GetBoundsInScreen().origin());
      generator_->PressLeftButton();
    } else {
      generator_->PressTouch(view->GetBoundsInScreen().origin());
      ui::GestureEvent long_press(
          generator_->current_screen_location().x(),
          generator_->current_screen_location().y(), ui::EF_NONE,
          ui::EventTimeForNow(),
          ui::GestureEventDetails(ui::EventType::kGestureLongPress));
      generator_->Dispatch(&long_press);
    }
  }

  // Drags a view vertically by `dy` pixels. Assumes the drag has been started.
  void MoveDragBy(int dy) {
    auto move_fn =
        base::BindRepeating(GetParam() == DragEventType::kMouse
                                ? &ui::test::EventGenerator::MoveMouseBy
                                : &ui::test::EventGenerator::MoveTouchBy,
                            base::Unretained(generator_));
    const int step = dy / abs(dy);
    for (int i = 0; i < abs(dy); ++i) {
      move_fn.Run(0, step);
    }
  }

  // Releases the mouse/gesture press that started a drag, possibly triggering
  // a drop.
  void EndDrag() {
    if (GetParam() == DragEventType::kMouse) {
      generator_->ReleaseLeftButton();
    } else {
      generator_->ReleaseTouch();
    }
  }

 private:
  raw_ptr<ui::test::EventGenerator, DanglingUntriaged> generator_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         ShelfLayoutManagerDragDropTest,
                         testing::Values(DragEventType::kMouse,
                                         DragEventType::kGesture));

// Tests the auto-hide shelf status with drag-drop events.
TEST_P(ShelfLayoutManagerDragDropTest, AutoHideShelfOnDragDropEvents) {
  // Create one window, or the shelf won't auto-hide.
  std::unique_ptr<views::Widget> widget = CreateFramelessTestWidget();

  // Set the shelf to auto-hide.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Create draggable view.
  DragTestView* view =
      widget->SetContentsView(std::make_unique<DragTestView>());
  const int drag_distance =
      view->DragThreshold() + layout_manager->GetIdealBounds().height();
  auto bounds = gfx::Rect(
      0, GetShelfWidget()->GetVisibleShelfBounds().y() - drag_distance, 1, 1);

  // `DragDropController` applies a vertical offset when determining the target
  // view for touch-initiated dragging, so we compensate for that here.
  if (GetParam() == DragEventType::kGesture)
    bounds.Offset(0, /*DragDropController::kTouchDragImageVerticalOffset=*/25);

  widget->SetBounds(bounds);

  // Drag a view to make the shelf appear.
  StartDrag(view);
  MoveDragBy(drag_distance);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Drag a view away to make the shelf disappear.
  MoveDragBy(-drag_distance);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Drag a view to make the shelf reappear (make sure all state has reset).
  MoveDragBy(drag_distance);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // End the drag with mouse over the shelf, so the shelf should stay shown.
  EndDrag();
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_EQ(GetParam() == DragEventType::kMouse ? SHELF_AUTO_HIDE_SHOWN
                                                : SHELF_AUTO_HIDE_HIDDEN,
            shelf->GetAutoHideState());

  // Move pointer away to make the shelf disappear.
  StartDrag(view);
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Drag a view to make the shelf reappear (make sure all state has reset).
  MoveDragBy(drag_distance);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Verify that dragging does nothing when the shelf is not in auto-hide mode.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  MoveDragBy(-drag_distance);
  MoveDragBy(drag_distance);
  ASSERT_FALSE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  // If the dragging had been observed, the shelf would be shown.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // TODO(crbug.com/1240332): Test screen exits when behavior is consistent.
}

// Tests that the shelf background does not change when the bubble launcher is
// shown.
TEST_F(ShelfLayoutManagerTest, NoBackgroundChange) {
  const auto shelf_background_type =
      GetShelfLayoutManager()->shelf_background_type();
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  // Show the AppListBubble, test that the shelf background has not changed.
  PressHomeButton();
  ASSERT_TRUE(app_list_controller->IsVisible());
  EXPECT_EQ(shelf_background_type,
            GetShelfLayoutManager()->shelf_background_type());

  // Hide the bubble, test that the shelf background has still not changed.
  PressHomeButton();
  ASSERT_FALSE(app_list_controller->IsVisible());
  EXPECT_EQ(shelf_background_type,
            GetShelfLayoutManager()->shelf_background_type());
}

// Tests that tapping the home button is successful on the autohidden shelf.
//
// TODO(crbug.com/40894666): Test is flaky.
TEST_F(ShelfLayoutManagerTest,
       DISABLED_NoTemporaryAutoHideStateWhileOpeningLauncher) {
  // Enable animations and simulate the zero state search called when showing
  // the launcher.
  ui::ScopedAnimationDurationScaleMode duration(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetTestAppListClient()->set_run_zero_state_callback_immediately(false);

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  const display::Display display =
      display::Screen::GetScreen()->GetPrimaryDisplay();

  // Create a window to hide the shelf in auto-hide mode.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  GetAppListTestHelper()->CheckVisibility(false);

  {
    AutoHideStateDetector detector;

    // Open the launcher by tapping the home button.
    GestureTapOn(GetPrimaryShelf()->navigation_widget()->GetHomeButton());

    // Wait until the zero state callback is called.
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(1, GetTestAppListClient()->zero_state_search_done_count());

    // No `SHELF_AUTO_HIDE_HIDDEN` should be set when launcher is showing.
    EXPECT_FALSE(detector.WasShelfAutoHidden());
  }

  // The app list should now be visible.
  GetAppListTestHelper()->CheckVisibility(true);
}

class ShelfLayoutManagerWindowDraggingTest : public ShelfLayoutManagerTestBase {
 public:
  ShelfLayoutManagerWindowDraggingTest() = default;
  ~ShelfLayoutManagerWindowDraggingTest() override = default;

  // ShelfLayoutManagerTestBase:
  void SetUp() override {
    ShelfLayoutManagerTestBase::SetUp();

    TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();
  }

  bool IsWindowDragInProgress() {
    return GetShelfLayoutManager()->IsWindowDragInProgress();
  }
};

// Test that when swiping up on the shelf, we may or may not drag up the MRU
// window.
TEST_F(ShelfLayoutManagerWindowDraggingTest, DraggedMRUWindow) {
  const int shelf_widget_height =
      GetShelfWidget()->GetWindowBoundsInScreen().height();
  const int shelf_widget_bottom =
      GetShelfWidget()->GetWindowBoundsInScreen().bottom();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();

  const struct TestCase {
    // The shelf widget whose bounds are used as the base for gesture start and
    // end locations.
    raw_ptr<const views::Widget> widget;
    // Whether the widget bounds are completely in the left part of the split
    // view.
    const bool left_in_split_view;
    // Whether the widget bounds are completely in the right part of the split
    // view.
    const bool right_in_split_view;
    const std::string description;
  } test_cases[] = {
      {GetShelfWidget(), false /*left_in_split_view*/,
       false /*right_in_split_view*/, "Shelf widget"},
      {GetPrimaryShelf()->navigation_widget(), true /*left_in_split_view*/,
       false /*right_in_split_view*/, "Navigation widget"},
      {GetShelfWidget()->status_area_widget(), false /*left_in_split_view*/,
       true /*right_in_split_view*/, "Status area widget"}};

  for (const auto& test_case : test_cases) {
    SCOPED_TRACE(test_case.description);
    ASSERT_TRUE(!test_case.left_in_split_view ||
                !test_case.right_in_split_view);

    // Starts the drag from the center of the shelf's bottom.
    const gfx::Rect widget_bounds = test_case.widget->GetWindowBoundsInScreen();
    // NOTE: Navigation widget might have zero size (depending on whether
    // home and back buttons are shown) - use the shelf widget bottom value to
    // ensure the drag starts from the bottom of the shelf.
    gfx::Point start(widget_bounds.CenterPoint().x(), shelf_widget_bottom);
    StartScroll(start);
    UpdateScroll(
        gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
    // We need at least one window to work with.
    EXPECT_FALSE(IsWindowDragInProgress());
    EndScroll(/*is_fling=*/false, 0.f);

    std::unique_ptr<aura::Window> window =
        AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
    wm::ActivateWindow(window.get());

    StartScroll(start);
    UpdateScroll(
        gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
    DragWindowFromShelfController* window_drag_controller =
        GetShelfLayoutManager()->window_drag_controller_for_testing();
    EXPECT_TRUE(IsWindowDragInProgress());
    EXPECT_EQ(window_drag_controller->dragged_window(), window.get());
    UpdateScroll(gfx::Vector2d(0, -shelf_widget_height - hotseat_size));
    EXPECT_FALSE(window->transform().IsIdentityOrTranslation());
    EXPECT_TRUE(window->transform().IsScaleOrTranslation());
    EndScroll(/*is_fling=*/false, 0.f);
    EXPECT_FALSE(IsWindowDragInProgress());
    EXPECT_TRUE(window->transform().IsIdentity());

    // The window needs to be visible to drag up.
    window->Hide();
    StartScroll(start);
    UpdateScroll(
        gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
    EXPECT_FALSE(IsWindowDragInProgress());
    EXPECT_TRUE(window->transform().IsIdentity());
    EndScroll(/*is_fling=*/false, 0.f);

    // In splitview, depends on the drag position, the active dragged window
    // might be different.
    window->Show();
    auto window2 = AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
    SplitViewController* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    split_view_controller->SnapWindow(window.get(), SnapPosition::kPrimary);
    split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
    StartScroll(gfx::Point(widget_bounds.x(), shelf_widget_bottom));
    UpdateScroll(
        gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
    window_drag_controller =
        GetShelfLayoutManager()->window_drag_controller_for_testing();
    EXPECT_TRUE(IsWindowDragInProgress());
    aura::Window* drag_window =
        test_case.right_in_split_view ? window2.get() : window.get();
    EXPECT_EQ(window_drag_controller->dragged_window(), drag_window);
    // No window transform at the point where the window drag starts.
    EXPECT_TRUE(drag_window->transform().IsIdentity());
    // The window is expected to be scaled down as the drag progresses.
    UpdateScroll(gfx::Vector2d(0, -10));
    EXPECT_FALSE(drag_window->transform().IsIdentityOrTranslation());
    EXPECT_TRUE(drag_window->transform().IsScaleOrTranslation());
    EndScroll(/*is_fling=*/false, 0.f);
    EXPECT_FALSE(IsWindowDragInProgress());
    EXPECT_TRUE(drag_window->transform().IsIdentity());

    StartScroll(gfx::Point(widget_bounds.right(), shelf_widget_bottom));
    UpdateScroll(
        gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
    window_drag_controller =
        GetShelfLayoutManager()->window_drag_controller_for_testing();
    EXPECT_TRUE(IsWindowDragInProgress());
    drag_window = test_case.left_in_split_view ? window.get() : window2.get();
    EXPECT_EQ(window_drag_controller->dragged_window(), drag_window);
    EXPECT_FALSE(drag_window->transform().IsIdentityOrTranslation());
    EXPECT_TRUE(drag_window->transform().IsScaleOrTranslation());
    EndScroll(/*is_fling=*/false, 0.f);
    split_view_controller->EndSplitView();
    EXPECT_FALSE(IsWindowDragInProgress());
    EXPECT_TRUE(window->transform().IsIdentity());
    EXPECT_TRUE(window2->transform().IsIdentity());
  }
}

// Tests that downward swipe on shelf does not start window drag.
TEST_F(ShelfLayoutManagerWindowDraggingTest,
       DownwardSwipeDoesnotStartWnidowDrag) {
  // Starts the drag from the center of the shelf's bottom.
  const gfx::Rect shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start = shelf_bounds.top_center();
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  // Tests that downward swipe on shelf does not start window drag, nor change
  // the window transform when hotseat is hidden.
  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  StartScroll(start);
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  UpdateScroll(gfx::Vector2d(0, shelf_bounds.height() / 2));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  UpdateScroll(gfx::Vector2d(0, shelf_bounds.height() / 2));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  EndScroll(/*is_fling=*/false, 0.f);
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  // Tests that downward swipe on shelf does not start window drag, nor change
  // the window transform when hotseat is extended.
  SwipeUpOnShelf();
  ASSERT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  StartScroll(start);
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  UpdateScroll(gfx::Vector2d(0, shelf_bounds.height() / 2));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  UpdateScroll(gfx::Vector2d(0, shelf_bounds.height() / 2));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  EndScroll(/*is_fling=*/false, 0.f);
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
}

// Test that drag from shelf when overview is active is a no-op.
TEST_F(ShelfLayoutManagerWindowDraggingTest, NoOpInOverview) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window1.get());

  // Starts the drag from the center of the shelf's bottom.
  EnterOverview();
  gfx::Point start = shelf_widget_bounds.bottom_center();
  StartScroll(start);
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EXPECT_FALSE(IsWindowDragInProgress());
  EndScroll(/*is_fling=*/false, 0.f);

  // In splitview + overview case, drag from shelf in the overview side of the
  // screen also does nothing.
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EnterOverview();
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  StartScroll(shelf_widget_bounds.bottom_right());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EXPECT_FALSE(IsWindowDragInProgress());
  EndScroll(/*is_fling=*/false, 0.f);
}

// Test that upward fling to exit overview mode does not cause the shelf to
// animate if we are in kShownHomeLauncher.
TEST_F(ShelfLayoutManagerWindowDraggingTest, SwipeToExitOverview) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // Hide |window1| so we remain in kShownHomeLauncher when we enter overview.
  window1->Hide();
  EXPECT_EQ(HotseatState::kShownHomeLauncher, GetHotseatWidget()->state());

  EnterOverview();
  GetHotseatWidget()->SetState(HotseatState::kShownHomeLauncher);
  const gfx::Rect hotseat_bounds = GetHotseatWidget()->GetTargetBounds();

  // Fling up from the center of the shelf's bottom.
  StartScroll(shelf_widget_bounds.bottom_center());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  // Hotseat should move as it is in the |kShownHomeLauncher| state.
  EXPECT_EQ(hotseat_bounds, GetHotseatWidget()->GetTargetBounds());
  EndScroll(
      true /* is_fling */,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  // We should exit overview mode after completing the fling gesture.
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
}

// Test that upward fling in overview transitions from overview to home.
TEST_F(ShelfLayoutManagerWindowDraggingTest, FlingInOverview) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window1.get());

  EnterOverview();

  base::HistogramTester histogram_tester;
  HotseatStateWatcher watcher(GetShelfLayoutManager());

  SwipeUpOnShelf();
  watcher.CheckEqual({HotseatState::kExtended});

  // Fling up from the center of the shelf's bottom.
  StartScroll(shelf_widget_bounds.bottom_center());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EndScroll(
      /*is_fling=*/true,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());

  watcher.WaitUntilStateChanged();
  watcher.CheckEqual(
      {HotseatState::kExtended, HotseatState::kShownHomeLauncher});

  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 1);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Test that upward fling in overview transitions from home shelf overview to
// home.
TEST_F(ShelfLayoutManagerWindowDraggingTest, FlingInOverviewHomeShelf) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  // This will ensure we enter overview in home shelf mode.
  WindowState::Get(window1.get())->Minimize();

  OverviewController* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  base::HistogramTester histogram_tester;

  // Fling up from the center of the shelf's bottom.
  StartScroll(shelf_widget_bounds.bottom_center());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EndScroll(
      true /* is_fling */,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  // Exit overview session.
  EXPECT_FALSE(overview_controller->InOverviewSession());

  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 1);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);
}

// Test that upward fling in split mode on overview side shows hotseat and
// remains in split view is the swipe is short.
TEST_F(ShelfLayoutManagerWindowDraggingTest,
       FlingToShowHotseatInSplitModeWithOverview) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(gfx::Rect(400, 400));
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(400, 400));

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
  OverviewController* overview_controller = OverviewController::Get();

  base::HistogramTester histogram_tester;
  HotseatStateWatcher watcher(GetShelfLayoutManager());

  // Short fling (little longer than the drag required to show the extended
  // hotseat).
  StartScroll(shelf_widget_bounds.bottom_right());
  UpdateScroll(gfx::Vector2d(
      0, -shelf_size - 1.5f * hotseat_size - hotseat_padding_size));
  EndScroll(
      /*is_fling=*/true,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  watcher.CheckEqual({HotseatState::kExtended});

  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);

  // The same fling gesture should transition to overview since the hotseat is
  // in extended state.
  StartScroll(shelf_widget_bounds.bottom_right());
  UpdateScroll(gfx::Vector2d(
      0, -shelf_size - 1.5f * hotseat_size - hotseat_padding_size));
  EndScroll(
      /*is_fling=*/true,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  watcher.CheckEqual({HotseatState::kExtended, HotseatState::kHidden});

  // Swipe up on the shelf to show the hotseat.
  SwipeUpOnShelf();

  // The same fling gesture should transition to home since overview mode
  // is active.
  StartScroll(shelf_widget_bounds.bottom_right());
  UpdateScroll(gfx::Vector2d(
      0, -shelf_size - 1.5f * hotseat_size - hotseat_padding_size));
  EndScroll(
      /*is_fling=*/true,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  watcher.WaitUntilStateChanged();
  watcher.CheckEqual({HotseatState::kExtended, HotseatState::kHidden,
                      HotseatState::kExtended,
                      HotseatState::kShownHomeLauncher});

  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 1);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 2);
}

// Test that upward fling in split view on overview side transitions to home, if
// the swipe length is long enough.
TEST_F(ShelfLayoutManagerWindowDraggingTest, FlingHomeInSplitViewWithOverview) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(gfx::Rect(400, 400));
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(400, 400));

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);
  OverviewController* overview_controller = OverviewController::Get();
  EnterOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  base::HistogramTester histogram_tester;
  HotseatStateWatcher watcher(GetShelfLayoutManager());

  // Longer fling, one that significantly exceeds the distance required to show
  // the hotseat (by 2 hotseat heights).
  StartScroll(shelf_widget_bounds.bottom_right());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - 3 * hotseat_size - hotseat_padding_size));
  EndScroll(
      /*is_fling=*/true,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  EXPECT_FALSE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  watcher.WaitUntilStateChanged();
  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());

  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 1);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 0);
}

// Tests that the hotseat ends up in manually extended state after swiping up
// a window in split screen to overview (the final state is a split screen with
// one side in overview).
TEST_F(ShelfLayoutManagerWindowDraggingTest, FlingInSplitView) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 = CreateAppWindow(gfx::Rect(400, 400));
  std::unique_ptr<aura::Window> window2 = CreateAppWindow(gfx::Rect(400, 400));

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);

  base::HistogramTester histogram_tester;
  HotseatStateWatcher watcher(GetShelfLayoutManager());

  // Longer fling, one that significantly exceeds the distance required to show
  // the hotseat (by 2 hotseat heights).
  StartScroll(shelf_widget_bounds.bottom_left());
  // Ensure swipe goes past the top of the hotseat first to activate the window
  // drag controller
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size - 10));
  UpdateScroll(gfx::Vector2d(0, -2 * hotseat_size));
  EndScroll(
      /*is_fling=*/true,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  watcher.CheckEqual({});

  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that the hotseat ends up in manually extended state after swiping up
// hotseat when window drag from shelf in split view ends up restoring original
// window bounds.
TEST_F(ShelfLayoutManagerWindowDraggingTest, ShortFlingInSplitView) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  split_view_controller->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller->SnapWindow(window2.get(), SnapPosition::kSecondary);

  base::HistogramTester histogram_tester;
  HotseatStateWatcher watcher(GetShelfLayoutManager());

  StartScroll(shelf_widget_bounds.bottom_left());
  UpdateScroll(gfx::Vector2d(
      0, -shelf_size - 1.5f * hotseat_size - hotseat_padding_size));
  EndScroll(
      true /* is_fling */,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  watcher.CheckEqual({HotseatState::kExtended});
  EXPECT_TRUE(GetPrimaryShelf()->hotseat_widget()->is_manually_extended());

  histogram_tester.ExpectBucketCount(
      kHotseatGestureHistogramName,
      InAppShelfGestures::kFlingUpToShowHomeScreen, 0);
  histogram_tester.ExpectBucketCount(kHotseatGestureHistogramName,
                                     InAppShelfGestures::kSwipeUpToShow, 1);
}

// Tests that hotseat transition animation is not delayed (i.e. that it happens
// as soon as shelf opaque background changes) when virtual keyboard is hidden,
// and the user swipes from shelf to home.
TEST_F(ShelfLayoutManagerWindowDraggingTest,
       NoDelayedAnimatingBackgroundForTransitionFromVirtualKeyboardToHome) {
  std::unique_ptr<aura::Window> window1 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window1.get());

  std::unique_ptr<aura::Window> window2 =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window2.get());

  // Show virtual keyboard.
  KeyboardController* const keyboard_controller =
      Shell::Get()->keyboard_controller();
  keyboard_controller->SetEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);
  keyboard_controller->ShowKeyboard();

  // Verify the shelf state.
  EXPECT_TRUE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_TRUE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_FALSE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_FALSE(GetShelfWidget()
                   ->GetAnimatingBackground()
                   ->GetAnimator()
                   ->is_animating());
  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Make animations not end immediately for the rest of the test (so the test
  // can test whether the animating shelf background is animating).
  ui::ScopedAnimationDurationScaleMode non_zero_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();

  // Simulate virtual keyboard closing, and a swipe from shelf to home.
  StartScroll(shelf_widget_bounds.bottom_right());
  keyboard_controller->HideKeyboard(HideReason::kUser);

  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - 3 * hotseat_size - hotseat_padding_size));
  EndScroll(
      true /* is_fling */,
      -(DragWindowFromShelfController::kVelocityToHomeScreenThreshold + 10));

  // Verify that the shelf background start animating immediately.
  EXPECT_FALSE(GetShelfWidget()->GetOpaqueBackground()->visible());
  EXPECT_FALSE(GetShelfWidget()->GetDragHandle()->GetVisible());
  ASSERT_TRUE(GetShelfWidget()->GetAnimatingBackground()->visible());
  ASSERT_TRUE(GetShelfWidget()
                  ->GetAnimatingBackground()
                  ->GetAnimator()
                  ->is_animating());

  keyboard_controller->ClearEnableFlag(
      keyboard::KeyboardEnableFlag::kShelfEnabled);
}

// Test that if shelf if hidden or auto-hide hidden, drag window from shelf is a
// no-op.
// TODO(crbug.com/40107332): This test consistently crashes.
TEST_F(ShelfLayoutManagerWindowDraggingTest, DISABLED_NoOpForHiddenShelf) {
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  Shelf* shelf = GetPrimaryShelf();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();

  // The window can be dragged on a visible shelf.
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  StartScroll(shelf_widget_bounds.bottom_center());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EXPECT_TRUE(IsWindowDragInProgress());
  EXPECT_FALSE(window->transform().IsIdentityOrTranslation());
  EndScroll(/*is_fling=*/false, 0.f);

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // The window can't be dragged on an auto-hidden hidden shelf.
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  StartScroll(display_bounds.bottom_center());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  EndScroll(/*is_fling=*/false, 0.f);

  // The window can be dragged on an auto-hidden shown shelf.
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  StartScroll(display_bounds.bottom_center());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EXPECT_TRUE(IsWindowDragInProgress());
  EXPECT_FALSE(window->transform().IsIdentityOrTranslation());
  EXPECT_TRUE(window->transform().IsScaleOrTranslation());
  EndScroll(/*is_fling=*/false, 0.f);

  // The window can't be dragged on a hidden shelf.
  SetState(GetShelfLayoutManager(), SHELF_HIDDEN);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  StartScroll(display_bounds.bottom_center());
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  EndScroll(/*is_fling=*/false, 0.f);
}

// Tests that dragging below the hotseat after dragging the MRU up results in
// the hotseat not moving from its extended position.
TEST_F(ShelfLayoutManagerWindowDraggingTest,
       DragBelowHotseatDoesNotMoveHotseat) {
  // Go to in-app shelf, then drag the hotseat up until it is extended, this
  // will start a window drag.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start = shelf_widget_bounds.bottom_center();
  StartScroll(start);
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  const int hotseat_y =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen().y();

  // Drag down, the hotseat should not move because it was extended when the
  // window drag began.
  UpdateScroll(gfx::Vector2d(0, 10));

  EXPECT_EQ(hotseat_y,
            GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen().y());
  EndScroll(/*is_fling=*/false, 0.f);
}

// Tests that dragging below the hotseat after dragging the MRU up results in
// the hotseat not moving from its extended position with an autohidden shelf.
TEST_F(ShelfLayoutManagerWindowDraggingTest,
       DragBelowHotseatDoesNotMoveHotseatAutoHiddenShelf) {
  // Extend the hotseat, then start dragging the window.
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  SwipeUpOnShelf();
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const gfx::Point start = shelf_widget_bounds.bottom_center();
  StartScroll(start);
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  UpdateScroll(
      gfx::Vector2d(0, -shelf_size - hotseat_size - hotseat_padding_size));
  const int hotseat_y =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen().y();

  // Drag down, the hotseat should not move because it was extended when the
  // window drag began.
  UpdateScroll(gfx::Vector2d(0, 10));

  EXPECT_EQ(hotseat_y,
            GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen().y());
  EndScroll(/*is_fling=*/false, 0.f);
}

TEST_F(ShelfLayoutManagerWindowDraggingTest, NoOpIfDragStartsAboveShelf) {
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  gfx::Rect hotseat_bounds =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen();
  StartScroll(hotseat_bounds.CenterPoint());
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  EndScroll(/*is_fling=*/false, 0.f);

  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());
}

// Test that gesture that starts within hotseat bounds, goes down to shelf, and
// start moving up does not start window drag (as upward swipe from hotseat does
// not start window drag either).
TEST_F(ShelfLayoutManagerWindowDraggingTest,
       NoOpIfDragSTartsAboveShelfAndMovesToShelf) {
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  gfx::Rect hotseat_bounds =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen();
  StartScroll(hotseat_bounds.CenterPoint());
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  const gfx::Vector2d vector_from_hotseat_to_shelf_center =
      hotseat_bounds.CenterPoint() -
      GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
  UpdateScroll(gfx::Vector2d(0, vector_from_hotseat_to_shelf_center.y()));

  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  UpdateScroll(gfx::Vector2d(0, -vector_from_hotseat_to_shelf_center.y()));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());

  EndScroll(/*is_fling=*/false, 0.f);
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
}

// Tests that the MRU window can only be dragged window after the hotseat is
// fully dragged up if hotseat was hidden before.
TEST_F(ShelfLayoutManagerWindowDraggingTest, StartsDragAfterHotseatIsUp) {
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int hotseat_size = GetHotseatWidget()->GetHotseatSize();
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();

  // Starts the drag from the center of the shelf's bottom.
  gfx::Point start = shelf_widget_bounds.bottom_center();
  StartScroll(start);
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  // Continues the drag until the hotseat should have been fully dragged up.
  UpdateScroll(gfx::Vector2d(0, -shelf_size));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  UpdateScroll(gfx::Vector2d(0, -hotseat_padding_size));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  UpdateScroll(gfx::Vector2d(0, -hotseat_size));
  EXPECT_TRUE(IsWindowDragInProgress());
  // The window should not be transformed at the point where the drag starts.
  EXPECT_TRUE(window->transform().IsIdentity());
  // The window is expected to be scaled down as the drag progresses.
  UpdateScroll(gfx::Vector2d(0, -10));
  EXPECT_FALSE(window->transform().IsIdentityOrTranslation());
  EXPECT_TRUE(window->transform().IsScaleOrTranslation());
  EndScroll(/*is_fling=*/false, 0.f);
}

TEST_F(ShelfLayoutManagerWindowDraggingTest, NoDragForDownwardEvent) {
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  SwipeUpOnShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  // Start drag on the extended hotseat.
  const int hotseat_padding_size = ShelfConfig::Get()->hotseat_bottom_padding();
  gfx::Rect hotseat_bounds =
      GetPrimaryShelf()->hotseat_widget()->GetWindowBoundsInScreen();
  StartScroll(hotseat_bounds.CenterPoint());
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  UpdateScroll(
      gfx::Vector2d(0, hotseat_bounds.height() + hotseat_padding_size));
  EXPECT_FALSE(IsWindowDragInProgress());
  EXPECT_TRUE(window->transform().IsIdentity());
  EndScroll(/*is_fling=*/false, 0.f);
}

// Verifies that there is no crash on shutdown while swipe from home to overview
// is in progress.
TEST_F(ShelfLayoutManagerWindowDraggingTest,
       ShutdownWhileSwipingHomeToOverview) {
  const gfx::Rect shelf_widget_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();
  StartScroll(shelf_widget_bounds.bottom_center());
  UpdateScroll(gfx::Vector2d(0, -20));
  UpdateScroll(gfx::Vector2d(0, -20));
  ASSERT_TRUE(
      GetShelfLayoutManager()->swipe_home_to_overview_controller_for_testing());
}

class ShelfLayoutManagerKeyboardTest : public AshTestBase {
 public:
  ShelfLayoutManagerKeyboardTest() = default;

  ShelfLayoutManagerKeyboardTest(const ShelfLayoutManagerKeyboardTest&) =
      delete;
  ShelfLayoutManagerKeyboardTest& operator=(
      const ShelfLayoutManagerKeyboardTest&) = delete;

  ~ShelfLayoutManagerKeyboardTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("800x600");
    keyboard::SetTouchKeyboardEnabled(true);
    keyboard::SetAccessibilityKeyboardEnabled(true);
  }

  // AshTestBase:
  void TearDown() override {
    keyboard::SetAccessibilityKeyboardEnabled(false);
    keyboard::SetTouchKeyboardEnabled(false);
    AshTestBase::TearDown();
  }

  void InitKeyboardBounds() {
    gfx::Rect work_area(
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
    keyboard_bounds_.SetRect(work_area.x(),
                             work_area.y() + work_area.height() / 2,
                             work_area.width(), work_area.height() / 2);
  }

  void NotifyKeyboardChanging(ShelfLayoutManager* layout_manager,
                              bool is_locked,
                              const gfx::Rect& bounds_in_screen) {
    WorkAreaInsets* work_area_insets = GetPrimaryWorkAreaInsets();
    KeyboardStateDescriptor state;
    state.visual_bounds = bounds_in_screen;
    state.occluded_bounds_in_screen = bounds_in_screen;
    state.displaced_bounds_in_screen =
        is_locked ? bounds_in_screen : gfx::Rect();
    state.is_visible = !bounds_in_screen.IsEmpty();
    work_area_insets->OnKeyboardVisibilityChanged(state.is_visible);
    work_area_insets->OnKeyboardAppearanceChanged(state);
  }

  const gfx::Rect& keyboard_bounds() const { return keyboard_bounds_; }

 private:
  gfx::Rect keyboard_bounds_;
};

TEST_F(ShelfLayoutManagerKeyboardTest, ShelfNotMoveOnKeyboardOpen) {
  gfx::Rect orig_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardUIController::Get();
  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);
  NotifyKeyboardChanging(layout_manager, false, keyboard_bounds());

  // Shelf position should not be changed.
  EXPECT_EQ(orig_bounds, GetShelfWidget()->GetWindowBoundsInScreen());
}

// When kAshUseNewVKWindowBehavior flag enabled, do not change accessibility
// keyboard work area in non-sticky mode.
TEST_F(ShelfLayoutManagerKeyboardTest,
       ShelfIgnoreWorkAreaChangeInNonStickyMode) {
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardUIController::Get();
  gfx::Rect orig_work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);
  NotifyKeyboardChanging(layout_manager, false, keyboard_bounds());

  // Work area should not be changed.
  EXPECT_EQ(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  kb_controller->HideKeyboardExplicitlyBySystem();
  NotifyKeyboardChanging(layout_manager, false, gfx::Rect());
  EXPECT_EQ(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
}

// Change accessibility keyboard work area in sticky mode.
TEST_F(ShelfLayoutManagerKeyboardTest, ShelfShouldChangeWorkAreaInStickyMode) {
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardUIController::Get();
  gfx::Rect orig_work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  // Open keyboard in sticky mode.
  kb_controller->ShowKeyboard(true);
  NotifyKeyboardChanging(layout_manager, true, keyboard_bounds());

  // Work area should be changed.
  EXPECT_NE(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  // Hide the keyboard.
  kb_controller->HideKeyboardByUser();
  NotifyKeyboardChanging(layout_manager, true, gfx::Rect());

  // Work area should be reset to its original value.
  EXPECT_EQ(orig_work_area,
            display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
}

// Make sure we don't update the work area during overview animation
// (crbug.com/947343).
TEST_F(ShelfLayoutManagerTest, NoShelfUpdateDuringOverviewAnimation) {
  // Finish lid detection task.
  base::RunLoop().RunUntilIdle();
  TabletModeControllerTestApi().EnterTabletMode();
  // Run overview animations.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> fullscreen(CreateTestWindow());
  fullscreen->SetProperty(aura::client::kShowStateKey,
                          ui::mojom::WindowShowState::kFullscreen);
  wm::ActivateWindow(fullscreen.get());

  TestDisplayObserver observer;
  EnterOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_EQ(0, observer.metrics_change_count());
  ExitOverview();
  WaitForOverviewAnimation(/*enter=*/false);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  EXPECT_EQ(0, observer.metrics_change_count());
}

// Tests that shelf bounds are updated properly after overview animation.
TEST_F(ShelfLayoutManagerTest, ShelfBoundsUpdateAfterOverviewAnimation) {
  // Run overview animations.
  ui::ScopedAnimationDurationScaleMode regular_animations(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  Shelf* shelf = GetPrimaryShelf();
  ASSERT_EQ(ShelfAlignment::kBottom, shelf->alignment());
  const gfx::Rect bottom_shelf_bounds =
      GetShelfWidget()->GetWindowBoundsInScreen();

  const int shelf_size = bottom_shelf_bounds.height();
  const gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Rect left_shelf_bounds =
      gfx::Rect(display_bounds.x(), display_bounds.y(), shelf_size,
                display_bounds.height());

  // Change alignment during overview enter animation.
  EnterOverview();
  // When setting the shelf alignment, bounds aren't expected to animate.
  shelf->SetAlignment(ShelfAlignment::kLeft);
  // Setting alignment exits overview which we should wait for.
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_EQ(left_shelf_bounds, GetShelfWidget()->GetWindowBoundsInScreen());

  // Change alignment during overview exit animation.
  EnterOverview();
  WaitForOverviewAnimation(/*enter=*/true);
  ExitOverview();
  // When setting the shelf alignment, bounds aren't expected to animate.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  WaitForOverviewAnimation(/*enter=*/false);
  EXPECT_EQ(bottom_shelf_bounds, GetShelfWidget()->GetWindowBoundsInScreen());
}

// Tests that the shelf on a second display is properly centered.
TEST_F(ShelfLayoutManagerTest, ShelfRemainsCenteredOnSecondDisplay) {
  // Create two displays.
  UpdateDisplay("600x400,1000x700");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  Shelf* shelf_1 = Shelf::ForWindow(root_windows[0]);
  Shelf* shelf_2 = Shelf::ForWindow(root_windows[1]);
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->GetWindow()->GetRootWindow(),
            shelf_2->GetWindow()->GetRootWindow());

  ShelfView* shelf_view_1 = shelf_1->GetShelfViewForTesting();
  ShelfView* shelf_view_2 = shelf_2->GetShelfViewForTesting();

  const display::Display display_1 =
      display::Screen::GetScreen()->GetPrimaryDisplay();
  const display::Display display_2 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);
  EXPECT_NE(display_1, display_2);

  ShelfTestUtil::AddAppShortcut("app_id", TYPE_PINNED_APP);
  ShelfViewTestAPI(GetPrimaryShelf()->GetShelfViewForTesting())
      .RunMessageLoopUntilAnimationsDone();
  gfx::Point app_center_1 = shelf_1->GetShelfViewForTesting()
                                ->first_visible_button_for_testing()
                                ->bounds()
                                .CenterPoint();
  views::View::ConvertPointToScreen(shelf_view_1, &app_center_1);

  gfx::Point app_center_2 = shelf_2->GetShelfViewForTesting()
                                ->first_visible_button_for_testing()
                                ->bounds()
                                .CenterPoint();
  views::View::ConvertPointToScreen(shelf_view_2, &app_center_2);

  // The app icon should be at the horizontal center of each display.
  EXPECT_EQ(display_1.bounds().CenterPoint().x(), app_center_1.x());
  EXPECT_EQ(display_2.bounds().CenterPoint().x(), app_center_2.x());
}

// Verifies that showing the system tray view on the secondary display
// should not affect the auto-hide shelf on the primary display
// (https://crbug.com/1079464).
TEST_F(ShelfLayoutManagerTest, VerifyAutoHideBehaviorOnMultipleDisplays) {
  UpdateDisplay("800x600, 800x600");
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  CreateTestWidget();

  // The primary shelf should be hidden.
  ASSERT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  ASSERT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Set focus on the secondary display.
  aura::Window* secondary_root_window =
      Shell::GetRootWindowForDisplayId(GetSecondaryDisplay().id());
  Shell::SetRootWindowForNewWindows(secondary_root_window);

  // Show the system tray on the secondary display.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      AcceleratorAction::kToggleSystemTrayBubble, {});
  Shelf* secondary_shelf =
      RootWindowController::ForWindow(secondary_root_window)->shelf();
  ASSERT_TRUE(secondary_shelf->status_area_widget()->IsMessageBubbleShown());
  ASSERT_FALSE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());

  // Verify that the primary shelf is still hidden.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests that pinned app icons are visible on non-primary displays.
TEST_F(ShelfLayoutManagerTest, ShelfShowsPinnedAppsOnOtherDisplays) {
  // Create three displays. Should use 600+ pixel as the horizontal display
  // size, otherwise there's no enough space to show both the date tray and
  // unified system tray on the screen.
  UpdateDisplay("700x400,1000x700,800x900");
  const unsigned int display_count = 3U;
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(display_count, root_windows.size());

  // Keep this low so that all apps fit at the center of the screen on all
  // displays.
  const size_t max_app_count = 4;
  for (size_t app_count = 1; app_count <= max_app_count; ++app_count) {
    AddApp();

    // Wait for everything to settle down.
    for (unsigned int display_index = 0; display_index < display_count;
         ++display_index) {
      Shelf* shelf = Shelf::ForWindow(root_windows[display_index]);
      ShelfView* shelf_view = shelf->GetShelfViewForTesting();
      ShelfViewTestAPI(shelf_view).RunMessageLoopUntilAnimationsDone();
    }

    // If everything is as expected, the middle app (if applicable) should be
    // exactly at the center of the screen, on all displays. Also, the
    // distance between the first app and the left edge of the display should be
    // the same as the distance between the third app and the right edge of the
    // display.
    for (unsigned int display_index = 0; display_index < display_count;
         ++display_index) {
      const display::Display display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(
              root_windows[display_index]);
      Shelf* shelf = Shelf::ForWindow(root_windows[display_index]);
      ShelfView* shelf_view = shelf->GetShelfViewForTesting();

      EXPECT_EQ(app_count, shelf_view->number_of_visible_apps());

      // Only check the middle app if we have an odd number of apps.
      if (app_count % 2 == 1) {
        const gfx::Point center = ShelfViewTestAPI(shelf_view)
                                      .GetViewAt(app_count / 2)
                                      ->GetBoundsInScreen()
                                      .CenterPoint();
        EXPECT_EQ(display.bounds().CenterPoint().x(), center.x())
            << "App at index " << (app_count / 2) << " should be at "
            << "the center of display " << display_index << " with "
            << app_count << " apps";
      }

      const gfx::Point left = ShelfViewTestAPI(shelf_view)
                                  .GetViewAt(0)
                                  ->GetBoundsInScreen()
                                  .left_center();
      const gfx::Point right = ShelfViewTestAPI(shelf_view)
                                   .GetViewAt(app_count - 1)
                                   ->GetBoundsInScreen()
                                   .right_center();

      EXPECT_EQ(left.x() - display.bounds().x(),
                display.bounds().right() - right.x())
          << "Apps on either end should be at the same distance from the "
          << "screen edge on display " << display_index << " with " << app_count
          << " apps";
    }
  }
}

class QuickActionShowBubbleTest : public ShelfLayoutManagerTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  QuickActionShowBubbleTest() : scoped_locale_(GetParam() ? "ar" : "") {}
  ~QuickActionShowBubbleTest() override = default;

 private:
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
};

const struct {
  ShelfAlignment alignment;
  bool swipe_gesture;
} test_table[]{
    {ShelfAlignment::kBottom, false},
    {ShelfAlignment::kBottom, true},
    {ShelfAlignment::kBottomLocked, false},
    {ShelfAlignment::kBottomLocked, true},
    {ShelfAlignment::kLeft, false},
    {ShelfAlignment::kLeft, true},
    {ShelfAlignment::kRight, false},
    {ShelfAlignment::kRight, true},
};

// Used to test RTL UI orientation.
INSTANTIATE_TEST_SUITE_P(All, QuickActionShowBubbleTest, testing::Bool());

// Tests that the two finger gesture and the swipe gesture when the mouse is
// over the shelf near the edge shows the bubble launcher.
TEST_P(QuickActionShowBubbleTest, ScrollFromShelfToShowAppList) {
  base::HistogramTester histogram_tester;
  const int scroll_offset_threshold =
      ShelfConfig::Get()->mousewheel_scroll_offset_threshold() + 10;
  int bucket_scroll_count = 0;
  int bucket_swipe_count = 0;

  for (auto test : test_table) {
    GetShelfLayoutManager()->LayoutShelf();
    GetPrimaryShelf()->SetAlignment(test.alignment);
    ASSERT_EQ(test.alignment, GetPrimaryShelf()->alignment());

    // Direction of the swipe gesture depends on the shelf alignment and on the
    // event being a swipe or a fling.
    gfx::Vector2d offset = GetPrimaryShelf()->SelectValueForShelfAlignment(
        gfx::Vector2d(0, scroll_offset_threshold),
        gfx::Vector2d(-scroll_offset_threshold, 0),
        gfx::Vector2d(scroll_offset_threshold, 0));

    // Action performed from the navigation_widget should show the bubble
    // launcher.
    const gfx::Point navigation_widget_center = GetPrimaryShelf()
                                                    ->navigation_widget()
                                                    ->GetContentsView()
                                                    ->GetBoundsInScreen()
                                                    .CenterPoint();
    if (test.swipe_gesture) {
      FlingBetweenLocations(navigation_widget_center,
                            navigation_widget_center - offset);
      ++bucket_swipe_count;
    } else {
      DoTwoFingerScrollAtLocation(navigation_widget_center, offset.x(),
                                  offset.y(), false);
      ++bucket_scroll_count;
    }

    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckVisibility(true);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kSwipeFromShelf,
                                       bucket_swipe_count);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kScrollFromShelf,
                                       bucket_scroll_count);

    // The same gesture on the opposite direction should dismiss the app list.
    if (test.swipe_gesture) {
      FlingBetweenLocations(navigation_widget_center,
                            navigation_widget_center + offset);
    } else {
      DoTwoFingerScrollAtLocation(navigation_widget_center, -offset.x(),
                                  -offset.y(), false);
    }
    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckVisibility(false);

    // Action performed from the status area should not show the bubble
    // launcher.
    const gfx::Point status_area_widget_center = GetShelfWidget()
                                                     ->status_area_widget()
                                                     ->GetContentsView()
                                                     ->GetBoundsInScreen()
                                                     .CenterPoint();
    if (test.swipe_gesture) {
      FlingBetweenLocations(status_area_widget_center,
                            status_area_widget_center - offset);
    } else {
      DoTwoFingerScrollAtLocation(status_area_widget_center, offset.x(),
                                  offset.y(), false);
    }

    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckVisibility(false);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kSwipeFromShelf,
                                       bucket_swipe_count);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kScrollFromShelf,
                                       bucket_scroll_count);
  }
}

// Tests that the two finger gesture and the swipe gesture when the mouse is
// over the shelf before the shelf apps, does not show the bubble launcher.
TEST_P(QuickActionShowBubbleTest, ScrollFromShelfToShowAppListOverShelfApps) {
  base::HistogramTester histogram_tester;
  const int scroll_offset_threshold =
      ShelfConfig::Get()->mousewheel_scroll_offset_threshold() + 10;
  int bucket_scroll_count = 0;
  int bucket_swipe_count = 0;

  ShelfView* shelf_view = GetPrimaryShelf()->GetShelfViewForTesting();
  const size_t max_app_count = 4;
  for (size_t app_count = 1; app_count <= max_app_count; ++app_count)
    AddApp();
  EXPECT_EQ(max_app_count, shelf_view->number_of_visible_apps());

  for (auto test : test_table) {
    GetShelfLayoutManager()->LayoutShelf();
    GetPrimaryShelf()->SetAlignment(test.alignment);
    ASSERT_EQ(test.alignment, GetPrimaryShelf()->alignment());

    // Direction of the swipe gesture depends on the shelf alignment and on the
    // event being a swipe or a fling.
    gfx::Vector2d offset = GetPrimaryShelf()->SelectValueForShelfAlignment(
        gfx::Vector2d(0, scroll_offset_threshold),
        gfx::Vector2d(-scroll_offset_threshold, 0),
        gfx::Vector2d(scroll_offset_threshold, 0));
    // Action performed on the edge closer to the home button should show the
    // bubble launcher.
    gfx::Point swipe_point;
    if (GetParam())
      swipe_point = GetHotseatWidget()->GetTargetBounds().right_center();
    else
      swipe_point = GetHotseatWidget()->GetTargetBounds().left_center();

    swipe_point = GetPrimaryShelf()->PrimaryAxisValue(
        swipe_point, GetHotseatWidget()->GetTargetBounds().top_center());

    if (test.swipe_gesture) {
      FlingBetweenLocations(swipe_point, swipe_point - offset);
      ++bucket_swipe_count;
    } else {
      DoTwoFingerScrollAtLocation(swipe_point, offset.x(), offset.y(), false);
      ++bucket_scroll_count;
    }

    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckVisibility(true);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kSwipeFromShelf,
                                       bucket_swipe_count);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kScrollFromShelf,
                                       bucket_scroll_count);

    // The same gesture on the opposite direction should dismiss the app list.
    if (test.swipe_gesture)
      FlingBetweenLocations(swipe_point, swipe_point + offset);
    else
      DoTwoFingerScrollAtLocation(swipe_point, -offset.x(), -offset.y(), false);

    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckVisibility(false);

    // Action performed over the hotseat should not show the bubble launcher
    swipe_point = GetHotseatWidget()->GetTargetBounds().CenterPoint();

    if (test.swipe_gesture) {
      FlingBetweenLocations(swipe_point, swipe_point - offset);
    } else {
      DoTwoFingerScrollAtLocation(swipe_point, offset.x(), offset.y(), false);
    }

    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckVisibility(false);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kSwipeFromShelf,
                                       bucket_swipe_count);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kScrollFromShelf,
                                       bucket_scroll_count);

    // Action performed on the edge farther to the home button should not show
    // the bubble launcher.
    if (GetParam())
      swipe_point = GetHotseatWidget()->GetTargetBounds().left_center();
    else
      swipe_point = GetHotseatWidget()->GetTargetBounds().right_center();

    swipe_point = GetPrimaryShelf()->PrimaryAxisValue(
        swipe_point, GetHotseatWidget()->GetTargetBounds().bottom_center());

    if (test.swipe_gesture) {
      FlingBetweenLocations(swipe_point, swipe_point - offset);
    } else {
      DoTwoFingerScrollAtLocation(swipe_point, offset.x(), offset.y(), false);
    }

    GetAppListTestHelper()->WaitUntilIdle();
    GetAppListTestHelper()->CheckVisibility(false);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kSwipeFromShelf,
                                       bucket_swipe_count);
    histogram_tester.ExpectBucketCount("Apps.AppListBubbleShowSource",
                                       AppListShowSource::kScrollFromShelf,
                                       bucket_scroll_count);
  }
}

// TODO(https://crbug.com/1286875): This behavior is broken in production. An
// auto-hidden shelf will close after a short swipe up that fails to show the
// app list.
TEST_P(QuickActionShowBubbleTest,
       DISABLED_ShortSwipeUpOnAutoHideShelfKeepsShelfOpen) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  auto* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::Milliseconds(100);
  constexpr int kNumScrollSteps = 4;

  // Starts the drag from the center of the shelf's bottom.
  gfx::Rect shelf_bounds = GetVisibleShelfWidgetBoundsInScreen();
  gfx::Point start = shelf_bounds.bottom_center();

  // Create a normal unmaximized window, the auto-hide shelf should be hidden.
  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->Show();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swiping up to show the auto-hide shelf.
  gfx::Point end = shelf_bounds.top_center();
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Swiping up on the auto-hide shelf to drag up the app list. Scroll Velocity
  // is not enough to show the app list but keep the previous shown auto-hide
  // shelf still visible. Starts fling from the navigation_widget.
  start = GetPrimaryShelf()
              ->navigation_widget()
              ->GetContentsView()
              ->GetBoundsInScreen()
              .CenterPoint();
  const int scroll_offset_threshold =
      ShelfConfig::Get()->mousewheel_scroll_offset_threshold() + 10;
  gfx::Vector2d offset(0, scroll_offset_threshold);
  end = start - offset;
  generator->GestureScrollSequence(
      start, end,
      generator->CalculateScrollDurationForFlingVelocity(start, end,
                                                         /*velocity =*/50, 4),
      4);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  // This line fails, see https://crbug.com/1286875.
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
}

// Tests that the shelf background is opaque in both screens after app list is
// dismissed in a secondary display. (See https://crbug.com/1060686)
TEST_F(ShelfLayoutManagerTest, ShelfBackgroundOpaqueAfterAppListUpdate) {
  UpdateDisplay("800x600,800x600");
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  int64_t primary_display_id = display_manager()->GetDisplayAt(0).id();
  int64_t secondary_display_id = display_manager()->GetDisplayAt(1).id();

  app_list_controller->ToggleAppList(
      secondary_display_id, AppListShowSource::kShelfButton, base::TimeTicks());
  EXPECT_FALSE(app_list_controller->IsVisible(primary_display_id));
  EXPECT_TRUE(app_list_controller->IsVisible(secondary_display_id));

  app_list_controller->ToggleAppList(
      secondary_display_id, AppListShowSource::kShelfButton, base::TimeTicks());
  EXPECT_FALSE(app_list_controller->IsVisible(primary_display_id));
  EXPECT_FALSE(app_list_controller->IsVisible(secondary_display_id));

  auto* primary_root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(primary_display_id);
  auto* secondary_root_window_controller =
      Shell::GetRootWindowControllerWithDisplayId(secondary_display_id);

  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            primary_root_window_controller->shelf()
                ->shelf_layout_manager()
                ->shelf_background_type());
  EXPECT_EQ(ShelfBackgroundType::kDefaultBg,
            secondary_root_window_controller->shelf()
                ->shelf_layout_manager()
                ->shelf_background_type());
}

using NoSessionShelfLayoutManagerTest = NoSessionAshTestBase;

// Tests that shelf visibility is updated on login. (See
// https://crbug.com/1097464)
TEST_F(NoSessionShelfLayoutManagerTest, UpdateShelfVisibilityAfterLogin) {
  UpdateDisplay("1000x800");
  constexpr char kUser[] = "user1@test.com";
  const AccountId kUserAccount = AccountId::FromUserEmail(kUser);

  // Setup autohide shelf pref.
  auto pref_service = std::make_unique<TestingPrefServiceSimple>();
  RegisterUserProfilePrefs(pref_service->registry(), /*country=*/"",
                           /*for_test=*/true);
  SetShelfAutoHideBehaviorPref(pref_service.get(),
                               WindowTreeHostManager::GetPrimaryDisplayId(),
                               ShelfAutoHideBehavior::kAlways);
  GetSessionControllerClient()->SetUserPrefService(kUserAccount,
                                                   std::move(pref_service));

  // Create a window that covers the full height of the in-session work area.
  const int kExpectedWindowHeight = 800 - ShelfConfig::Get()->shelf_size();
  auto window = CreateTestWindow(gfx::Rect(400, kExpectedWindowHeight));

  // Simulate login.
  SimulateUserLogin(kUser);

  // The window should be the same height.
  EXPECT_EQ(kExpectedWindowHeight, window->bounds().height());
}

// Test base for unit test related to shelf dimming.
class DimShelfLayoutManagerTestBase : public ShelfLayoutManagerTestBase {
 public:
  DimShelfLayoutManagerTestBase() = default;

  bool AutoDimEventHandlerInitialized() {
    return GetPrimaryShelf()->auto_dim_event_handler_.get();
  }

  bool ShelfDimmed() { return GetShelfLayoutManager()->dimmed_for_inactivity_; }

  void TriggerDimShelf() { GetPrimaryShelf()->DimShelf(); }

  void ResetDimShelf() { GetPrimaryShelf()->UndimShelf(); }

  bool HasDimShelfTimer() {
    return AutoDimEventHandlerInitialized() &&
           GetPrimaryShelf()->HasDimShelfTimer();
  }

  float GetWidgetOpacity(views::Widget* widget) {
    return widget->GetNativeView()->layer()->opacity();
  }

  // Expected opacity for floating shelf.
  const float kExpectedFloatingShelfDimOpacity = 0.74f;

  // Expected opacity for shelf when shelf is in the maximized state.
  const float kExpectedMaximizedShelfDimOpacity = 0.6f;

  // Expected opacity for shelf without dimming.
  const float kExpectedDefaultShelfOpacity = 1.0f;
};

// Paramaterized tests for shelf with and without shelf dimming enabled.
class DimShelfLayoutManagerTest : public DimShelfLayoutManagerTestBase,
                                  public testing::WithParamInterface<bool> {
 public:
  DimShelfLayoutManagerTest() = default;

  // testing::Test:
  void SetUp() override {
    if (GetParam()) {
      base::CommandLine::ForCurrentProcess()->AppendSwitch(
          switches::kEnableDimShelf);
    }
    DimShelfLayoutManagerTestBase::SetUp();
  }
};

// Used to test shelf dimming.
INSTANTIATE_TEST_SUITE_P(All, DimShelfLayoutManagerTest, testing::Bool());

// Tests that the auto dim handler is initialized and shelf is not dim on
// startup.
TEST_P(DimShelfLayoutManagerTest, AutoDimHandlerInitialized) {
  ASSERT_FALSE(ShelfDimmed());
  ASSERT_EQ(GetParam(), AutoDimEventHandlerInitialized());

  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->navigation_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->hotseat_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()->status_area_widget()),
      kExpectedDefaultShelfOpacity);
}

// Tests that the auto dim handler dims the shelf when called.
TEST_P(DimShelfLayoutManagerTest, AutoDimHandlerSetDim) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());
  ASSERT_FALSE(ShelfDimmed());
  if (!dim_shelf_enabled)
    return;
  TriggerDimShelf();
  ASSERT_TRUE(ShelfDimmed());
  ResetDimShelf();
  ASSERT_FALSE(ShelfDimmed());
}

// Tests that the auto dim handler sets the shelf opacity correctly for floating
// shelf.
TEST_P(DimShelfLayoutManagerTest, FloatingShelfDimAlpha) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());

  if (dim_shelf_enabled)
    TriggerDimShelf();

  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->hotseat_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->navigation_widget()),
            dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                              : kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetPrimaryShelf()->hotseat_widget()->GetShelfView()->layer()->opacity(),
      dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                        : kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()->status_area_widget()),
      dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                        : kExpectedDefaultShelfOpacity);
}

// Tests that the auto dim handler sets the shelf opacity correctly in the
// special case of maximized shelf.
TEST_P(DimShelfLayoutManagerTest, MaximizedShelfDimAlpha) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());
  ASSERT_FALSE(ShelfDimmed());

  if (dim_shelf_enabled) {
    views::Widget* widget = CreateTestWidget();
    widget->Maximize();
    TriggerDimShelf();
  }

  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->navigation_widget()),
            dim_shelf_enabled ? kExpectedMaximizedShelfDimOpacity
                              : kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->hotseat_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetPrimaryShelf()->hotseat_widget()->GetShelfView()->layer()->opacity(),
      dim_shelf_enabled ? kExpectedMaximizedShelfDimOpacity
                        : kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()->status_area_widget()),
      dim_shelf_enabled ? kExpectedMaximizedShelfDimOpacity
                        : kExpectedDefaultShelfOpacity);
}

// Tests that navigation and status area widgets are dimmed. Verifies the shelf
// view is not dimmed when the hotseat is in the kExtended state. Verifies that
// the shelf background/hotseat widget are not dimmed.
TEST_P(DimShelfLayoutManagerTest, InAppShelfDimAlpha) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());

  TabletModeControllerTestApi().EnterTabletMode();
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  ASSERT_FALSE(ShelfDimmed());

  EXPECT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());
  SwipeUpOnShelf();
  EXPECT_EQ(HotseatState::kExtended, GetShelfLayoutManager()->hotseat_state());

  if (dim_shelf_enabled)
    TriggerDimShelf();

  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->hotseat_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetPrimaryShelf()->hotseat_widget()->GetShelfView()->layer()->opacity(),
      kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->navigation_widget()),
            dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                              : kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()->status_area_widget()),
      dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                        : kExpectedDefaultShelfOpacity);
}

// Tests that shelf view, navigation widget, and status area widget are
// dimmed but the shelf background and hotseat are not.
TEST_P(DimShelfLayoutManagerTest, TabletModeHomeShelfDimAlpha) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());

  TabletModeControllerTestApi().EnterTabletMode();
  EXPECT_FALSE(ShelfDimmed());

  EXPECT_EQ(HotseatState::kShownHomeLauncher,
            GetShelfLayoutManager()->hotseat_state());

  if (dim_shelf_enabled)
    TriggerDimShelf();

  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->hotseat_widget()),
            kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetPrimaryShelf()->hotseat_widget()->GetShelfView()->layer()->opacity(),
      dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                        : kExpectedDefaultShelfOpacity);
  EXPECT_EQ(GetWidgetOpacity(GetPrimaryShelf()->navigation_widget()),
            dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                              : kExpectedDefaultShelfOpacity);
  EXPECT_EQ(
      GetWidgetOpacity(GetPrimaryShelf()->shelf_widget()->status_area_widget()),
      dim_shelf_enabled ? kExpectedFloatingShelfDimOpacity
                        : kExpectedDefaultShelfOpacity);
}

// Shelf dimming should not trigger when shelf is hidden in tablet mode.
TEST_P(DimShelfLayoutManagerTest, AutoHiddenShelfTabletModeDimAlpha) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());
  TabletModeControllerTestApi().EnterTabletMode();

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(ShelfDimmed());
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // Shelf should not be dimmed when auto hidden.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  if (dim_shelf_enabled)
    TriggerDimShelf();
  EXPECT_FALSE(ShelfDimmed());

  // Minimizing the widget should show the shelf. The shelf can now be dimmed.
  widget->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_FALSE(ShelfDimmed());
  if (dim_shelf_enabled)
    TriggerDimShelf();
  EXPECT_EQ(dim_shelf_enabled, ShelfDimmed());
}

// Shelf dimming should not trigger when shelf is hidden in clamshell mode.
TEST_P(DimShelfLayoutManagerTest, AutoHiddenShelfClamshellModeDimAlpha) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(ShelfDimmed());
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // Shelf should not be dimmed when auto hidden. The dim shelf timer should
  // persist after failing to dim the shelf.
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  if (dim_shelf_enabled)
    TriggerDimShelf();
  EXPECT_FALSE(ShelfDimmed());
  EXPECT_EQ(dim_shelf_enabled, HasDimShelfTimer());

  // Minimizing the widget should show the shelf. The shelf can now be dimmed
  // and the dim shelf timer should no longer be active.
  widget->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  ASSERT_FALSE(ShelfDimmed());
  EXPECT_EQ(dim_shelf_enabled, HasDimShelfTimer());
  if (dim_shelf_enabled)
    TriggerDimShelf();
  EXPECT_EQ(dim_shelf_enabled, ShelfDimmed());
  EXPECT_FALSE(HasDimShelfTimer());
}

// Shelf should be undimmed when transitioning into the visible state and create
// a dim shelf timer.
TEST_P(DimShelfLayoutManagerTest, AutoHiddenShelfUndimOnShow) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(ShelfDimmed());
  if (dim_shelf_enabled)
    TriggerDimShelf();
  EXPECT_EQ(dim_shelf_enabled, ShelfDimmed());
  views::Widget* widget = CreateTestWidget();

  // Maximize and minimize the widget to cycle between shelf auto hidden states.
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  widget->Minimize();
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hiding and showing the auto hidden shelf should set the shelf to the
  // undimmed state but also create a dim shelf timer.
  ASSERT_FALSE(ShelfDimmed());
  EXPECT_EQ(dim_shelf_enabled, HasDimShelfTimer());
  if (dim_shelf_enabled)
    TriggerDimShelf();
  EXPECT_EQ(dim_shelf_enabled, ShelfDimmed());
  EXPECT_FALSE(HasDimShelfTimer());
}

// Shelf should be undimmed when auto hidden shelf is disabled.
TEST_P(DimShelfLayoutManagerTest, AutoHiddenShelfUndimOnDisable) {
  const bool dim_shelf_enabled = GetParam();
  ASSERT_EQ(dim_shelf_enabled, AutoDimEventHandlerInitialized());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  EXPECT_FALSE(ShelfDimmed());
  if (dim_shelf_enabled)
    TriggerDimShelf();
  EXPECT_EQ(dim_shelf_enabled, ShelfDimmed());

  // Create and maximize a widget to cycle force auto hidden shelf.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Disabling auto hidden shelf should undim the shelf.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  ASSERT_FALSE(ShelfDimmed());
}

class NavigationWidgetRTLTest
    : public ShelfLayoutManagerTest,
      public testing::WithParamInterface<
          std::tuple</*in_tablet=*/bool, /*in_rtl=*/bool>> {
 public:
  NavigationWidgetRTLTest()
      : in_tablet_(std::get<0>(GetParam())),
        in_rtl_(std::get<1>(GetParam())),
        scoped_locale_(in_rtl_ ? "ar" : "") {}
  ~NavigationWidgetRTLTest() override = default;

  // Indicates whether the test should run in the tablet mode.
  const bool in_tablet_;

  // Indicates whether the test should run under RTL.
  const bool in_rtl_;

  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
};

INSTANTIATE_TEST_SUITE_P(ALL,
                         NavigationWidgetRTLTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(NavigationWidgetRTLTest, VerifyHomeButtonBounds) {
  if (in_tablet_) {
    Shell::Get()
        ->accessibility_controller()
        ->SetTabletModeShelfNavigationButtonsEnabled(true);
    TabletModeControllerTestApi().EnterTabletMode();
    ASSERT_EQ(HotseatState::kShownHomeLauncher,
              GetShelfLayoutManager()->hotseat_state());
  } else {
    ASSERT_EQ(HotseatState::kShownClamshell,
              GetShelfLayoutManager()->hotseat_state());
  }

  const gfx::Rect display_bounds = GetPrimaryDisplay().bounds();
  Shelf* shelf = GetPrimaryShelf();
  ShelfNavigationWidget* navigation_widget = shelf->navigation_widget();

  auto fetch_home_button_screen_bounds =
      [](const ShelfNavigationWidget* navigation_widget,
         bool in_rtl) -> gfx::Rect {
    gfx::Rect home_button_bounds_in_widget =
        navigation_widget->bounds_animator_for_test()->GetTargetBounds(
            navigation_widget->GetHomeButton());
    if (in_rtl) {
      home_button_bounds_in_widget =
          navigation_widget->GetRootView()->GetMirroredRect(
              home_button_bounds_in_widget);
    }
    gfx::Rect home_button_bounds_in_screen = home_button_bounds_in_widget;
    home_button_bounds_in_screen.Offset(
        navigation_widget->GetWindowBoundsInScreen().OffsetFromOrigin());
    return home_button_bounds_in_screen;
  };

  // Verify home button bounds in home launcher.
  {
    const gfx::Rect home_button_bounds_in_screen =
        fetch_home_button_screen_bounds(navigation_widget, in_rtl_);
    const int horizontal_edge_spacing =
        ShelfConfig::Get()->control_button_edge_spacing(
            /*is_primary_axis_edge=*/true);
    EXPECT_EQ(
        horizontal_edge_spacing,
        in_rtl_ ? display_bounds.right() - home_button_bounds_in_screen.right()
                : home_button_bounds_in_screen.x());
    const int vertical_edge_spacing =
        ShelfConfig::Get()->control_button_edge_spacing(
            /*is_primary_axis_edge=*/false);
    EXPECT_EQ(display_bounds.bottom(),
              home_button_bounds_in_screen.bottom() + vertical_edge_spacing);

    auto* home_button = navigation_widget->GetHomeButton();
    ASSERT_EQ(views::Button::STATE_NORMAL, home_button->GetState());
    GetEventGenerator()->MoveMouseTo(
        home_button_bounds_in_screen.CenterPoint());
    EXPECT_EQ(views::Button::STATE_HOVERED, home_button->GetState());
  }

  if (!in_tablet_)
    return;

  // The test code below is only for the tablet mode.

  // Activate a window and wait for the navigation widget animation to finish.
  views::WidgetAnimationWaiter waiter(shelf->navigation_widget());
  std::unique_ptr<aura::Window> window =
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400));
  wm::ActivateWindow(window.get());
  waiter.WaitForAnimation();

  ASSERT_EQ(HotseatState::kHidden, GetShelfLayoutManager()->hotseat_state());

  // Verify home button bounds in the hidden state.
  {
    const gfx::Rect home_button_bounds_in_screen =
        fetch_home_button_screen_bounds(navigation_widget, in_rtl_);
    EXPECT_EQ(display_bounds.bottom(), home_button_bounds_in_screen.bottom());
  }
}

class ShelfLayoutManagerWithEcheTest : public ShelfLayoutManagerTestBase {
 public:
  template <typename... TaskEnvironmentTraits>
  explicit ShelfLayoutManagerWithEcheTest(TaskEnvironmentTraits&&... traits)
      : ShelfLayoutManagerTestBase(
            std::forward<TaskEnvironmentTraits>(traits)...) {
    scoped_feature_list_.InitAndEnableFeature(features::kEcheSWA);
  }

 protected:
  void SetUp() override { ShelfLayoutManagerTestBase::SetUp(); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  // Calling the factory constructor is enough to set it up.
  TestAshWebViewFactory test_web_view_factory_;
};

TEST_F(ShelfLayoutManagerWithEcheTest, AutoHideShelfWithEcheHidden) {
  // Create and maximize a widget to cycle force auto hidden shelf.
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  // Set the shelf to auto-hide.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  GetShelfLayoutManager()->UpdateVisibilityState(/*force_layout=*/false);

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Create and show Eche Bubble.
  StatusAreaWidget* status_area =
      StatusAreaWidgetTestHelper::GetStatusAreaWidget();
  SkBitmap bitmap;
  bitmap.allocN32Pixels(30, 30);
  gfx::ImageSkia image_skia = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  image_skia.MakeThreadSafe();
  status_area->eche_tray()->LoadBubble(
      GURL("http://google.com"), gfx::Image(image_skia), u"app 1",
      u"your phone",
      eche_app::mojom::ConnectionStatus::kConnectionStatusDisconnected,
      eche_app::mojom::AppStreamLaunchEntryPoint::RECENT_APPS);
  status_area->eche_tray()->ShowBubble();
  UpdateAutoHideStateNow();

  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

  // Hide Eche Bubble.
  status_area->eche_tray()->HideBubble();
  UpdateAutoHideStateNow();

  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

}  // namespace ash
