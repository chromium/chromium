// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/wm/overview/overview_session.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/exit_warning_handler.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/frame_throttler/frame_throttling_controller.h"
#include "ash/frame_throttler/mock_frame_throttling_observer.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/close_button.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/raster_scale_change_tracker.h"
#include "ash/test/test_window_builder.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_action_button.h"
#include "ash/wm/desks/desk_action_context_menu.h"
#include "ash/wm/desks/desk_action_view.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desk_icon_button.h"
#include "ash/wm/desks/desks_constants.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_api.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/overview_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_save_desk_button.h"
#include "ash/wm/desks/templates/saved_desk_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"
#include "ash/wm/gestures/wm_gesture_handler.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_drop_target.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_grid_test_api.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_base.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_test_base.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_test_util.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/window_mini_view_header_view.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/containers/contains.h"
#include "base/containers/to_vector.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/layer_animation_stopped_waiter.h"
#include "ui/compositor/test/layer_animator_test_controller.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/geometry/transform_util.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::WindowStateType;

constexpr const char kActiveWindowChangedFromOverview[] =
    "WindowSelector_ActiveWindowChanged";

constexpr gfx::Rect kInitWindowBoundsToGrow(80, 80);
constexpr gfx::Rect kInitWindowBoundsToShrink(600, 600);

class TweenTester : public ui::LayerAnimationObserver {
 public:
  explicit TweenTester(aura::Window* window) : window_(window) {
    window->layer()->GetAnimator()->AddObserver(this);
  }

  TweenTester(const TweenTester&) = delete;
  TweenTester& operator=(const TweenTester&) = delete;

  ~TweenTester() override {
    window_->layer()->GetAnimator()->RemoveObserver(this);
    EXPECT_TRUE(will_animate_);
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {}
  void OnAttachedToSequence(ui::LayerAnimationSequence* sequence) override {
    ui::LayerAnimationObserver::OnAttachedToSequence(sequence);
    if (!will_animate_) {
      tween_type_ = sequence->FirstElement()->tween_type();
      will_animate_ = true;
    }
  }

  gfx::Tween::Type tween_type() const { return tween_type_; }

 private:
  gfx::Tween::Type tween_type_ = gfx::Tween::LINEAR;
  raw_ptr<aura::Window> window_;
  bool will_animate_ = false;
};

// Class which tracks if a given widget has been destroyed.
class TestDestroyedWidgetObserver : public views::WidgetObserver {
 public:
  explicit TestDestroyedWidgetObserver(views::Widget* widget) {
    DCHECK(widget);
    observation_.Observe(widget);
  }
  TestDestroyedWidgetObserver(const TestDestroyedWidgetObserver&) = delete;
  TestDestroyedWidgetObserver& operator=(const TestDestroyedWidgetObserver&) =
      delete;
  ~TestDestroyedWidgetObserver() override = default;

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override {
    DCHECK(!widget_destroyed_);
    widget_destroyed_ = true;
    observation_.Reset();
  }

  bool widget_destroyed() const { return widget_destroyed_; }

 private:
  bool widget_destroyed_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver> observation_{
      this};
};

void CombineDesksViaMiniView(const DeskMiniView* desk_mini_view,
                             ui::test::EventGenerator* event_generator) {
  CHECK(desk_mini_view);

  // Move to the center of the mini view so that the combine button shows up.
  const gfx::Point mini_view_center =
      desk_mini_view->GetBoundsInScreen().CenterPoint();
  event_generator->MoveMouseTo(mini_view_center);
  const DeskActionView* desk_action_view = desk_mini_view->desk_action_view();
  auto* combine_button = desk_action_view->combine_desks_button();
  EXPECT_TRUE(combine_button->GetVisible());

  // Move to the center of the combine button and click.
  event_generator->MoveMouseTo(
      combine_button->GetBoundsInScreen().CenterPoint());
  event_generator->ClickLeftButton();
}

std::string OverviewSessionTestParamsToString(
    const testing::TestParamInfo<std::tuple<bool, bool>>& info) {
  const auto& [desk_templates, has_snapshot] = info.param;
  std::string name = desk_templates ? "DesksTemplatesOn" : "DesksTemplatesOff";
  name += has_snapshot ? "_SnapshotOn" : "_SnapshotOff";
  return name;
}

}  // namespace

class OverviewSessionTest
    : public OverviewTestBase,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  OverviewSessionTest() = default;
  OverviewSessionTest(const OverviewSessionTest&) = delete;
  OverviewSessionTest& operator=(const OverviewSessionTest&) = delete;
  ~OverviewSessionTest() override = default;

  // Used for tests regarding the exit warning popup.
  void StubForTest(ExitWarningHandler* ewh) {
    ewh->stub_timer_for_test_ = true;
  }
  bool IsUIShown(ExitWarningHandler* ewh) { return !!ewh->widget_; }

  bool DeskTemplatesOn() const { return std::get<0>(GetParam()); }

  bool SnapshotOn() const { return std::get<1>(GetParam()); }

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatureStates(
        {{features::kDesksTemplates, DeskTemplatesOn()},
         {features::kDeskBarWindowOcclusionOptimization, true}});

    OverviewTestBase::SetUp();
    Shell::Get()->overview_controller()->set_windows_have_snapshot_for_test(
        SnapshotOn());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that close buttons on windows in overview do not work
// when one window is being dragged.
TEST_P(OverviewSessionTest, CloseButtonDisabledOnDrag) {
  std::unique_ptr<views::Widget> widget1(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  std::unique_ptr<views::Widget> widget2(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));

  aura::Window* window1 = widget1->GetNativeWindow();
  aura::Window* window2 = widget2->GetNativeWindow();

  ToggleOverview();

  ASSERT_FALSE(widget1->IsClosed());
  ASSERT_FALSE(widget2->IsClosed());

  auto* item1 = GetOverviewItemForWindow(window1);
  auto* item2 = GetOverviewItemForWindow(window2);

  // Get location of close button on `window1` before drag.
  const gfx::Point item1_close_button_position =
      GetCloseButton(item1)->GetBoundsInScreen().CenterPoint();

  // Drag `window1` in overview to trigger drag animations.
  GetEventGenerator()->PressTouchId(
      /*touch_id=*/0,
      gfx::ToRoundedPoint(item1->GetTransformedBounds().CenterPoint()));
  GetEventGenerator()->MoveTouchIdBy(/*touch_id=*/0, -100, 0);

  // Close button for both items has 0 opacity.
  EXPECT_EQ(1.f, GetTitlebarOpacity(item1));
  EXPECT_EQ(0.f, GetCloseButtonOpacity(item1));
  EXPECT_EQ(1.f, GetTitlebarOpacity(item2));
  EXPECT_EQ(0.f, GetCloseButtonOpacity(item2));

  // Both close buttons should be disabled at this point.
  EXPECT_FALSE(GetCloseButton(item1)->GetEnabled());
  EXPECT_FALSE(GetCloseButton(item2)->GetEnabled());

  // Try to close `window2` and `window1`.
  GetEventGenerator()->GestureTapAt(
      GetCloseButton(item2)->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->GestureTapAt(item1_close_button_position);
  GetEventGenerator()->GestureTapAt(
      GetCloseButton(item1)->GetBoundsInScreen().CenterPoint());

  // Check that both windows are still open.
  ASSERT_FALSE(widget1->IsClosed());
  ASSERT_FALSE(widget2->IsClosed());

  // Release touch 0 to exit drag.
  GetEventGenerator()->ReleaseTouchId(0);

  // We should still be in an overview session.
  ASSERT_TRUE(InOverviewSession());

  // The windows should now be closable.
  GetEventGenerator()->GestureTapAt(
      GetCloseButton(item2)->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->GestureTapAt(
      GetCloseButton(item1)->GetBoundsInScreen().CenterPoint());

  EXPECT_TRUE(widget1->IsClosed());
  EXPECT_TRUE(widget2->IsClosed());
}

// Tests that close buttons on windows in overview are re-enabled
// when one window is snapped to a side of the screen.
TEST_P(OverviewSessionTest, CloseButtonEnabledOnSnap) {
  std::unique_ptr<views::Widget> widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  aura::Window* window2 = widget2->GetNativeWindow();

  ToggleOverview();

  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2);

  ASSERT_FALSE(widget2->IsClosed());

  ASSERT_TRUE(GetSplitViewController()->CanSnapWindow(
      window1.get(), chromeos::kDefaultSnapRatio));

  // Snap `window1` to the left side of the screen while in
  // overview.
  GetEventGenerator()->PressTouchId(
      /*touch_id=*/0,
      gfx::ToRoundedPoint(item1->GetTransformedBounds().CenterPoint()));

  GetEventGenerator()->MoveTouchId(gfx::Point(0, 0), /*touch_id=*/0);

  EXPECT_FALSE(GetCloseButton(item1)->GetEnabled());
  EXPECT_FALSE(GetCloseButton(item2)->GetEnabled());

  GetEventGenerator()->ReleaseTouchId(0);

  ASSERT_TRUE(InOverviewSession());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            GetSplitViewController()->state());

  // The close button for `window2` should be enabled.
  EXPECT_TRUE(GetCloseButton(item2)->GetEnabled());

  // Try to close `window2`.
  GetEventGenerator()->GestureTapAt(
      GetCloseButton(item2)->GetBoundsInScreen().CenterPoint());

  // Check that `window2` is closed.
  EXPECT_TRUE(widget2->IsClosed());
}

// Tests that an a11y alert is sent on entering overview mode.
TEST_P(OverviewSessionTest, A11yAlertOnOverviewMode) {
  TestAccessibilityControllerClient client;
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  EXPECT_NE(AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());
  ToggleOverview();
  EXPECT_EQ(AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());
}

// Tests that there are no crashes when there is not enough screen space
// available to show all of the windows.
TEST_P(OverviewSessionTest, SmallDisplay) {
  UpdateDisplay("3x1");
  gfx::Rect bounds(0, 0, 1, 1);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateTestWindow(bounds));
  window1->SetProperty(aura::client::kTopViewInset, 0);
  window2->SetProperty(aura::client::kTopViewInset, 0);
  window3->SetProperty(aura::client::kTopViewInset, 0);
  window4->SetProperty(aura::client::kTopViewInset, 0);
  ToggleOverview();
}

// Tests entering overview mode with two windows and selecting one by clicking.
TEST_P(OverviewSessionTest, Basic) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Overview disabled by default.
  EXPECT_FALSE(InOverviewSession());

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window2.get(), window_util::GetFocusedWindow());

  // Hide the cursor before entering overview to test that it will be shown.
  aura::client::GetCursorClient(root_window)->HideCursor();

  CheckOverviewEnterExitHistogram("Init", {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0});
  // In overview mode the windows should no longer overlap and the overview
  // focus window should be focused.
  ToggleOverview();

  // Warms up the compositor so that UI changes are picked up in time before
  // throughput tracker is stopped.
  ui::Compositor* const compositor = window1->GetHost()->compositor();
  compositor->ScheduleFullRedraw();
  ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));

  WaitForOverviewEnterAnimation();

  EXPECT_EQ(GetOverviewSession()->GetOverviewFocusWindow(),
            window_util::GetFocusedWindow());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  CheckOverviewEnterExitHistogram("Enter", {1, 0, 0, 0, 0}, {0, 0, 0, 0, 0});

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());

  // Warms up the compositor so that UI changes are picked up in time before
  // throughput tracker is stopped.
  compositor->ScheduleFullRedraw();
  ASSERT_TRUE(ui::WaitForNextFrameToBePresented(compositor));

  WaitForOverviewExitAnimation();

  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window1.get(), window_util::GetFocusedWindow());

  // Cursor should have been unlocked.
  EXPECT_FALSE(aura::client::GetCursorClient(root_window)->IsCursorLocked());

  CheckOverviewEnterExitHistogram("Exit", {1, 0, 0, 0, 0}, {1, 0, 0, 0, 0});
}

// Tests activating minimized window.
TEST_P(OverviewSessionTest, ActivateMinimized) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  WindowState* window_state = WindowState::Get(window.get());
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimize_event);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(WindowStateType::kMinimized,
            WindowState::Get(window.get())->GetStateType());

  ToggleOverview();

  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(WindowStateType::kMinimized, window_state->GetStateType());
  WindowPreviewView* preview_view =
      GetPreviewView(GetOverviewItemForWindow(window.get()));
  EXPECT_TRUE(preview_view);

  const gfx::Point point = preview_view->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->set_current_screen_location(point);
  GetEventGenerator()->ClickLeftButton();

  EXPECT_FALSE(InOverviewSession());

  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
}

// A window can be minimized when losing a focus upon entering overview.
// If such window was active, it will be unminimized when exiting overview.
// b/163551595.
TEST_P(OverviewSessionTest, MinimizeDuringOverview) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  WindowState* window_state = WindowState::Get(window.get());
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimize_event);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(WindowStateType::kMinimized,
            WindowState::Get(window.get())->GetStateType());
  ToggleOverview();
}

// Tests that the ordering of windows is stable across different overview
// sessions even when the windows have the same bounds.
TEST_P(OverviewSessionTest, WindowsOrder) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  std::unique_ptr<aura::Window> window3(CreateTestWindowInShellWithId(3));

  // The order of windows in overview mode is MRU.
  WindowState::Get(window1.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview1 =
      GetOverviewItemsForRoot(0);
  EXPECT_EQ(1, overview1[0]->GetWindow()->GetId());
  EXPECT_EQ(3, overview1[1]->GetWindow()->GetId());
  EXPECT_EQ(2, overview1[2]->GetWindow()->GetId());
  ToggleOverview();

  // Activate the second window.
  WindowState::Get(window2.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItemBase>>& overview2 =
      GetOverviewItemsForRoot(0);

  // The order should be MRU.
  EXPECT_EQ(2, overview2[0]->GetWindow()->GetId());
  EXPECT_EQ(1, overview2[1]->GetWindow()->GetId());
  EXPECT_EQ(3, overview2[2]->GetWindow()->GetId());
  ToggleOverview();
}

// Tests selecting a window by tapping on it.
TEST_P(OverviewSessionTest, BasicGesture) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(window1.get(), window_util::GetFocusedWindow());
  ToggleOverview();
  EXPECT_EQ(GetOverviewSession()->GetOverviewFocusWindow(),
            window_util::GetFocusedWindow());
  GetEventGenerator()->GestureTapAt(
      GetTransformedTargetBounds(window2.get()).CenterPoint());
  EXPECT_EQ(window2.get(), window_util::GetFocusedWindow());
}

// Tests that calling `views::Widget::CloseNow` on a minimized window that is
// currently being dragged does not cause a crash. Regression test for
// b/268413746.
TEST_P(OverviewSessionTest, CloseNowDraggedMinimizedWindow) {
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  WindowState::Get(window.get())->Minimize();

  // Start dragging the window.
  ToggleOverview();
  GetEventGenerator()->set_current_screen_location(
      GetTransformedTargetBounds(window.get()).CenterPoint());
  GetEventGenerator()->PressLeftButton();

  // Call `views::Widget::CloseNow` on the window mid drag and verify no crash.
  // This could happen in production when an exo window shuts down.
  views::Widget::GetWidgetForNativeView(window.release())->CloseNow();
}

// Tests that the user action WindowSelector_ActiveWindowChanged is
// recorded when the mouse/touchscreen/keyboard are used to select a window
// in overview mode which is different from the previously-active window.
TEST_P(OverviewSessionTest, ActiveWindowChangedUserActionRecorded) {
  base::UserActionTester user_action_tester;
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());
  ToggleOverview();

  // Tap on |window2| to activate it and exit overview.
  GetEventGenerator()->GestureTapAt(
      GetTransformedTargetBounds(window2.get()).CenterPoint());
  EXPECT_EQ(
      1, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Click on |window2| to activate it and exit overview.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_EQ(
      2, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Focus `window2` using the arrow keys. Activate it (and exit overview) by
  // pressing the return key.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(static_cast<OverviewItem*>(GetOverviewItemForWindow(window2.get()))
                ->overview_item_view(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(
      3, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that the user action WindowSelector_ActiveWindowChanged is not
// recorded when the mouse/touchscreen/keyboard are used to select the
// already-active window from overview mode. Also verifies that entering and
// exiting overview without selecting a window does not record the action.
TEST_P(OverviewSessionTest, ActiveWindowChangedUserActionNotRecorded) {
  base::UserActionTester user_action_tester;
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());
  ToggleOverview();

  // Tap on |window1| to exit overview.
  GetEventGenerator()->GestureTapAt(
      GetTransformedTargetBounds(window1.get()).CenterPoint());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Click on it to exit overview.
  ASSERT_EQ(window1.get(), window_util::GetFocusedWindow());
  ToggleOverview();
  ClickWindow(window1.get());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Select using the keyboard.
  ASSERT_EQ(window1.get(), window_util::GetFocusedWindow());
  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_TAB);
  ASSERT_EQ(static_cast<OverviewItem*>(GetOverviewItemForWindow(window1.get()))
                ->overview_item_view(),
            GetFocusedView());
  PressAndReleaseKey(ui::VKEY_RETURN);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Entering and exiting overview without user input should not record
  // the action.
  ToggleOverview();
  ToggleOverview();
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that the user action WindowSelector_ActiveWindowChanged is not
// recorded when overview mode exits as a result of closing its only window.
TEST_P(OverviewSessionTest, ActiveWindowChangedUserActionWindowClose) {
  base::UserActionTester user_action_tester;
  std::unique_ptr<views::Widget> widget(CreateTestWidget(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET, nullptr,
      desks_util::GetActiveDeskContainerId(), gfx::Rect(400, 400)));

  ToggleOverview();
  aura::Window* window = widget->GetNativeWindow();
  const gfx::Point point = GetCloseButton(GetOverviewItemForWindow(window))
                               ->GetBoundsInScreen()
                               .CenterPoint();
  ASSERT_FALSE(widget->IsClosed());
  GetEventGenerator()->set_current_screen_location(point);
  GetEventGenerator()->ClickLeftButton();
  ASSERT_TRUE(widget->IsClosed());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that we do not crash and overview mode remains engaged if the desktop
// is tapped while a finger is already down over a window.
TEST_P(OverviewSessionTest, NoCrashWithDesktopTap) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(200, 300, 250, 450)));

  ToggleOverview();

  const gfx::Rect bounds = GetTransformedBoundsInRootWindow(window.get());
  GetEventGenerator()->set_current_screen_location(bounds.CenterPoint());

  // Press down on the window.
  const int kTouchId = 19;
  GetEventGenerator()->PressTouchId(kTouchId);

  // Tap on the desktop, which should not cause a crash. Overview mode should
  // remain engaged.
  GetEventGenerator()->GestureTapAt(GetGridBounds().CenterPoint());
  EXPECT_TRUE(InOverviewSession());

  GetEventGenerator()->ReleaseTouchId(kTouchId);
}

// Tests that we do not crash and a window is selected when appropriate when
// we click on a window during touch.
TEST_P(OverviewSessionTest, ClickOnWindowDuringTouch) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  ToggleOverview();

  gfx::Rect window1_bounds = GetTransformedBoundsInRootWindow(window1.get());
  GetEventGenerator()->set_current_screen_location(
      window1_bounds.CenterPoint());

  // Clicking on |window2| while touching on |window1| should not cause a
  // crash, it should do nothing since overview only handles one click or touch
  // at a time.
  const int kTouchId = 19;
  GetEventGenerator()->PressTouchId(kTouchId);
  GetEventGenerator()->MoveMouseToCenterOf(window2.get());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(InOverviewSession());
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));

  // Clicking on |window1| while touching on |window1| should not cause
  // a crash, overview mode should be disengaged, and |window1| should
  // be active.
  GetEventGenerator()->MoveMouseToCenterOf(window1.get());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  GetEventGenerator()->ReleaseTouchId(kTouchId);
}

// Tests that a window does not receive located events when in overview mode.
TEST_P(OverviewSessionTest, WindowDoesNotReceiveEvents) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(400, 400)));
  const gfx::Point point1 = window->bounds().CenterPoint();
  ui::MouseEvent event1(ui::EventType::kMousePressed, point1, point1,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);

  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  ui::EventTarget* root_target = root_window;
  ui::EventTargeter* targeter =
      root_window->GetHost()->dispatcher()->GetDefaultEventTargeter();

  // The event should target the window because we are still not in overview
  // mode.
  EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &event1));

  ToggleOverview();

  // The bounds have changed, take that into account.
  const gfx::Point point2 =
      GetTransformedBoundsInRootWindow(window.get()).CenterPoint();
  ui::MouseEvent event2(ui::EventType::kMousePressed, point2, point2,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);

  // Now the transparent window should be intercepting this event.
  EXPECT_NE(window.get(), targeter->FindTargetForEvent(root_target, &event2));
}

// Tests that clicking on the close button effectively closes the window.
TEST_P(OverviewSessionTest, CloseButton) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  std::unique_ptr<views::Widget> minimized_widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  minimized_widget->Minimize();

  ToggleOverview();
  aura::Window* window = widget->GetNativeWindow();
  const gfx::Point point = GetCloseButton(GetOverviewItemForWindow(window))
                               ->GetBoundsInScreen()
                               .CenterPoint();
  GetEventGenerator()->set_current_screen_location(point);

  EXPECT_FALSE(widget->IsClosed());
  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());
  ASSERT_TRUE(InOverviewSession());

  aura::Window* minimized_window = minimized_widget->GetNativeWindow();
  WindowPreviewView* preview_view =
      GetPreviewView(GetOverviewItemForWindow(minimized_window));
  EXPECT_TRUE(preview_view);
  const gfx::Point point2 =
      GetCloseButton(GetOverviewItemForWindow(minimized_window))
          ->GetBoundsInScreen()
          .CenterPoint();
  GetEventGenerator()->MoveMouseTo(point2);
  EXPECT_FALSE(minimized_widget->IsClosed());

  GetEventGenerator()->ClickLeftButton();
  EXPECT_TRUE(minimized_widget->IsClosed());

  // All minimized windows are closed, so it should exit overview mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(InOverviewSession());
}

// Tests that the shadow disappears before the close animation starts.
// Regression test for https://crbug.com/981509.
TEST_P(OverviewSessionTest, CloseAnimationShadow) {
  // Give us some time to check if the shadow has disappeared.
  ScopedOverviewTransformWindow::SetImmediateCloseForTests(/*immediate=*/false);
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  ToggleOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  // Click the close button.
  auto* item = GetOverviewItemForWindow(widget->GetNativeWindow());
  const gfx::Point point =
      GetCloseButton(item)->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->set_current_screen_location(point);
  GetEventGenerator()->ClickLeftButton();
  ASSERT_FALSE(widget->IsClosed());
  ASSERT_TRUE(InOverviewSession());

  // The shadow bounds are empty, which means its not visible.
  EXPECT_EQ(gfx::Rect(), GetShadowBounds(item));
}

// Tests minimizing/unminimizing in overview mode.
TEST_P(OverviewSessionTest, MinimizeUnminimize) {
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  aura::Window* window = widget->GetNativeWindow();

  ToggleOverview();
  EXPECT_FALSE(GetPreviewView(GetOverviewItemForWindow(window)));

  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(GetPreviewView(GetOverviewItemForWindow(window)));

  widget->Restore();
  EXPECT_FALSE(widget->IsMinimized());
  EXPECT_FALSE(GetPreviewView(GetOverviewItemForWindow(window)));
  EXPECT_TRUE(InOverviewSession());
}

// Tests that clicking on the close button on a secondary display effectively
// closes the window.
TEST_P(OverviewSessionTest, CloseButtonOnMultipleDisplay) {
  UpdateDisplay("600x400,600x400");

  // We need a widget for the close button to work because windows are closed
  // via the widget. We also use the widget to determine if the window has been
  // closed or not. Parent the window to a window in a non-primary root window.
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(650, 300, 250, 450)));
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  widget->SetBounds(gfx::Rect(650, 0, 400, 400));
  aura::Window* window2 = widget->GetNativeWindow();
  window2->SetProperty(aura::client::kTopViewInset,
                       kWindowMiniViewHeaderHeight);
  views::Widget::ReparentNativeView(window2, window->parent());
  ASSERT_EQ(Shell::GetAllRootWindows()[1], window2->GetRootWindow());

  ToggleOverview();
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window2);
  gfx::Point point(bounds.right() - 15, bounds.y() + 15);
  ui::test::EventGenerator event_generator(window2->GetRootWindow(), point);

  EXPECT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());
}

// Test that we mirror the the correct widgets when dragging across displays.
TEST_P(OverviewSessionTest, DraggingOnMultipleDisplay) {
  UpdateDisplay("600x400,600x400");

  // Create one normal window and one minimzied window.
  auto normal_window = CreateAppWindow();
  auto minimized_window = CreateAppWindow();
  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(minimized_window.get())->OnWMEvent(&minimize_event);

  ToggleOverview();
  auto* generator = GetEventGenerator();
  OverviewItem* normal_item =
      static_cast<OverviewItem*>(GetOverviewItemForWindow(normal_window.get()));
  OverviewItem* minimized_item = static_cast<OverviewItem*>(
      GetOverviewItemForWindow(minimized_window.get()));

  // Start dragging the normal window. We mirror both the overview item widget
  // and the normal window.
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(normal_item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  generator->MoveMouseBy(20, 20);
  EXPECT_TRUE(normal_item->item_mirror_for_dragging_for_testing());
  EXPECT_TRUE(normal_item->window_mirror_for_dragging_for_testing());
  EXPECT_FALSE(minimized_item->item_mirror_for_dragging_for_testing());
  EXPECT_FALSE(minimized_item->window_mirror_for_dragging_for_testing());

  // Start dragging the minimzed window. We don't mirror the original window,
  // since the overview item widget already contains a mirror.
  generator->ReleaseLeftButton();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(minimized_item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  generator->MoveMouseBy(20, 20);
  EXPECT_FALSE(normal_item->item_mirror_for_dragging_for_testing());
  EXPECT_FALSE(normal_item->window_mirror_for_dragging_for_testing());
  EXPECT_TRUE(minimized_item->item_mirror_for_dragging_for_testing());
  EXPECT_FALSE(minimized_item->window_mirror_for_dragging_for_testing());
}

// Tests that dragging an overview item with multiple displays and then exiting
// overview does not result in a u-a-f. Regression test for b/293867778.
TEST_P(OverviewSessionTest, ExitOverviewWhileDraggingOnMultipleDisplay) {
  UpdateDisplay("600x400,600x400");

  auto window = CreateAppWindow();

  ToggleOverview();
  auto* generator = GetEventGenerator();
  auto* item = GetOverviewItemForWindow(window.get());
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  generator->MoveMouseBy(20, 20);

  // Exit overview without completing the drag.
  ToggleOverview();
}

// Tests entering overview mode with two windows and selecting one.
TEST_P(OverviewSessionTest, FullscreenWindow) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window1.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFullscreen());

  // Enter overview and select the fullscreen window.
  ToggleOverview();
  ClickWindow(window1.get());
  ASSERT_FALSE(InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFullscreen());

  // Entering overview and selecting another window, the previous window remains
  // fullscreen.
  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFullscreen());
}

// Tests entering overview mode with maximized window.
TEST_P(OverviewSessionTest, MaximizedWindow) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());

  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());

  // Enter overview and select the maximized window.
  ToggleOverview();
  ClickWindow(window1.get());
  ASSERT_FALSE(InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());

  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
}

// Tests the animation histograms when entering and exiting overview with a
// maximized and fullscreen window.
#if defined(NDEBUG) && !defined(ADDRESS_SANITIZER) &&         \
    !defined(LEAK_SANITIZER) && !defined(THREAD_SANITIZER) && \
    !defined(MEMORY_SANITIZER)
TEST_P(OverviewSessionTest, MaximizedFullscreenHistograms) {
  std::unique_ptr<aura::Window> maximized_window(CreateTestWindow());
  std::unique_ptr<aura::Window> fullscreen_window(CreateTestWindow());

  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(maximized_window.get())->OnWMEvent(&maximize_event);
  ASSERT_TRUE(WindowState::Get(maximized_window.get())->IsMaximized());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(fullscreen_window.get())
      ->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(WindowState::Get(fullscreen_window.get())->IsFullscreen());

  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  // Enter and exit overview with the maximized window activated.
  wm::ActivateWindow(maximized_window.get());
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("MaximizedWindowEnter1", {0, 1, 0, 0, 0},
                                  {0, 0, 0, 0, 0});
  ToggleOverview();
  WaitForOverviewExitAnimation();
  CheckOverviewEnterExitHistogram("MaximizedWindowExit1", {0, 1, 0, 0, 0},
                                  {0, 1, 0, 0, 0});

  // Enter and exit overview with the fullscreen window activated.
  wm::ActivateWindow(fullscreen_window.get());
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("FullscreenWindowEnter1", {0, 2, 0, 0, 0},
                                  {0, 1, 0, 0, 0});
  ToggleOverview();
  WaitForOverviewExitAnimation();
  CheckOverviewEnterExitHistogram("FullscreenWindowExit1", {0, 2, 0, 0, 0},
                                  {0, 2, 0, 0, 0});
}
#endif

// TODO(crbug.com/1493835): Re-enable this test. Disabled because of flakiness.
TEST_P(OverviewSessionTest, DISABLED_TabletModeHistograms) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  EnterTabletMode();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());

  // Enter overview with the window maximized.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("MaximizedWindowTabletEnter", {0, 0, 1, 0, 0},
                                  {0, 0, 0, 0, 0});

  ToggleOverview();
  WaitForOverviewExitAnimation();
  CheckOverviewEnterExitHistogram("MaximizedWindowTabletExit", {0, 0, 1, 0, 0},
                                  {0, 0, 1, 0, 0});

  WindowState::Get(window1.get())->Minimize();
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("MinimizedWindowTabletEnter", {0, 0, 1, 1, 0},
                                  {0, 0, 1, 0, 0});

  ToggleOverview();
  WaitForOverviewExitAnimation();
  CheckOverviewEnterExitHistogram("MinimizedWindowTabletExit", {0, 0, 1, 1, 0},
                                  {0, 0, 1, 1, 0});
}

// Tests that entering overview when a fullscreen window is active in maximized
// mode correctly applies the transformations to the window and correctly
// updates the window bounds on exiting overview mode: http://crbug.com/401664.
// TODO(crbug.com/41496866): Fix flaky test.
TEST_P(OverviewSessionTest, DISABLED_FullscreenWindowTabletMode) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);

  UpdateDisplay("800x600");
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EnterTabletMode();
  gfx::Rect normal_window_bounds(window1->bounds());
  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window1.get())->OnWMEvent(&toggle_fullscreen_event);

  // Finish fullscreen state change animation since it is irrelevant.
  window1->layer()->GetAnimator()->StopAnimating();

  gfx::Rect fullscreen_window_bounds(window1->bounds());
  EXPECT_NE(normal_window_bounds, fullscreen_window_bounds);
  EXPECT_EQ(fullscreen_window_bounds, window2->GetTargetBounds());

  const gfx::Rect fullscreen(800, 600);
  const int shelf_inset = 600 - ShelfConfig::Get()->shelf_size();
  const gfx::Rect normal_work_area(800, shelf_inset);
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(gfx::Rect(800, 600),
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_EQ(fullscreen,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  CheckOverviewEnterExitHistogram("FullscreenWindowTabletEnter1",
                                  {0, 0, 1, 0, 0}, {0, 0, 0, 0, 0});

  // Window 2 would normally resize to normal window bounds on showing the shelf
  // for overview but this is deferred until overview is exited.
  EXPECT_EQ(fullscreen_window_bounds, window2->GetTargetBounds());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  ToggleOverview();
  WaitForOverviewExitAnimation();
  EXPECT_EQ(fullscreen,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  // Since the fullscreen window is still active, window2 will still have the
  // larger bounds.
  EXPECT_EQ(fullscreen_window_bounds, window2->GetTargetBounds());
  CheckOverviewEnterExitHistogram("FullscreenWindowTabletExit1",
                                  {0, 0, 1, 0, 0}, {0, 0, 1, 0, 0});

  // Enter overview again and select window 2. Selecting window 2 should show
  // the shelf bringing window2 back to the normal bounds.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("FullscreenWindowTabletEnter2",
                                  {0, 0, 2, 0, 0}, {0, 0, 1, 0, 0});

  ClickWindow(window2.get());
  WaitForOverviewExitAnimation();
  // Selecting non fullscreen window should set the work area back to normal.
  EXPECT_EQ(normal_work_area,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  EXPECT_EQ(normal_window_bounds, window2->GetTargetBounds());
  CheckOverviewEnterExitHistogram("FullscreenWindowTabletExit2",
                                  {0, 0, 2, 0, 0}, {0, 0, 2, 0, 0});

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("FullscreenWindowTabletEnter3",
                                  {0, 0, 3, 0, 0}, {0, 0, 2, 0, 0});
  EXPECT_EQ(normal_work_area,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  ClickWindow(window1.get());
  WaitForOverviewExitAnimation();
  // Selecting fullscreen. The work area should be updated to fullscreen as
  // well.
  EXPECT_EQ(fullscreen,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  CheckOverviewEnterExitHistogram("FullscreenWindowTabletExit3",
                                  {0, 0, 3, 0, 0}, {0, 0, 3, 0, 0});
}

// Tests that when disabling ChromeVox, desks widget bounds on overview mode
// should be updated. Desks widget will be moved to the top of the screen.
TEST_P(OverviewSessionTest, DesksWidgetBoundsChangeWhenDisableChromeVox) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();

  AccessibilityController* accessibility_controller =
      Shell::Get()->accessibility_controller();

  // Enable ChromeVox.
  const int kAccessibilityPanelHeight = 45;
  // ChromeVox layout manager relies on the widget to validate ChromaVox panel's
  // exist. Check AccessibilityPanelLayoutManager::SetPanelBounds.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                       nullptr, kShellWindowId_AccessibilityPanelContainer);
  SetAccessibilityPanelHeight(kAccessibilityPanelHeight);
  accessibility_controller->SetSpokenFeedbackEnabled(true,
                                                     A11Y_NOTIFICATION_NONE);
  // Enable overview mode.
  ToggleOverview();

  const views::Widget* desks_widget =
      GetOverviewSession()->grid_list()[0].get()->desks_widget();

  const gfx::Rect desks_widget_bounds = desks_widget->GetWindowBoundsInScreen();
  // Desks widget should lay out right below ChromeVox panel.
  EXPECT_EQ(desks_widget_bounds.y(), kAccessibilityPanelHeight);

  // Disable ChromeVox panel.
  accessibility_controller->SetSpokenFeedbackEnabled(false,
                                                     A11Y_NOTIFICATION_NONE);
  SetAccessibilityPanelHeight(0);

  const gfx::Rect desks_widget_bounds_after_disable_chromeVox =
      desks_widget->GetWindowBoundsInScreen();
  // Desks widget should be moved to the top of the screen after
  // disabling ChromeVox panel.
  EXPECT_EQ(desks_widget_bounds_after_disable_chromeVox.y(), 0);

  EXPECT_NE(desks_widget_bounds, desks_widget_bounds_after_disable_chromeVox);
}

TEST_P(OverviewSessionTest, SkipOverviewWindow) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  window2->SetProperty(kHideInOverviewKey, true);

  // Enter overview.
  ToggleOverview();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_FALSE(window2->IsVisible());

  // Exit overview.
  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(window2->IsVisible());
}

// Tests that a minimized window's visibility and layer visibility
// stay invisible (A minimized window is cloned during overview).
TEST_P(OverviewSessionTest, MinimizedWindowState) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  WindowState::Get(window1.get())->Minimize();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());

  ToggleOverview();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());

  ToggleOverview();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
}

// Tests that a bounds change during overview is corrected for.
TEST_P(OverviewSessionTest, BoundsChangeDuringOverview) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect(400, 400)));
  // Use overview headers above the window in this test.
  window->SetProperty(aura::client::kTopViewInset, 0);
  ToggleOverview();
  gfx::Rect overview_bounds = GetTransformedTargetBounds(window.get());
  window->SetBounds(gfx::Rect(200, 0, 200, 200));
  gfx::Rect new_overview_bounds = GetTransformedTargetBounds(window.get());
  EXPECT_EQ(overview_bounds, new_overview_bounds);
  ToggleOverview();
}

// Tests that a change to the |kTopViewInset| window property during overview is
// corrected for.
TEST_P(OverviewSessionTest, TopViewInsetChangeDuringOverview) {
  std::unique_ptr<aura::Window> window = CreateTestWindow(gfx::Rect(400, 400));
  window->SetProperty(aura::client::kTopViewInset, 32);
  ToggleOverview();
  gfx::Rect overview_bounds = GetTransformedTargetBounds(window.get());
  window->SetProperty(aura::client::kTopViewInset, 0);
  gfx::Rect new_overview_bounds = GetTransformedTargetBounds(window.get());
  EXPECT_NE(overview_bounds, new_overview_bounds);
  ToggleOverview();
}

// Tests that a newly created window aborts overview.
TEST_P(OverviewSessionTest, NewWindowCancelsOverview) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  // A window being created should exit overview mode.
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  EXPECT_FALSE(InOverviewSession());
}

// Tests that a window activation exits overview mode.
TEST_P(OverviewSessionTest, ActivationCancelsOverview) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  window2->Focus();
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  // A window being activated should exit overview mode.
  window1->Focus();
  EXPECT_FALSE(InOverviewSession());

  // window1 should be focused after exiting even though window2 was focused on
  // entering overview because we exited due to an activation.
  EXPECT_EQ(window1.get(), window_util::GetFocusedWindow());
}

// Tests that if an overview item is dragged, the activation of the
// corresponding window does not cancel overview.
TEST_P(OverviewSessionTest, ActivateDraggedOverviewWindowNotCancelOverview) {
  UpdateDisplay("800x600");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  ToggleOverview();
  auto* item = GetOverviewItemForWindow(window.get());
  gfx::PointF drag_point = item->target_bounds().CenterPoint();
  GetOverviewSession()->InitiateDrag(item, drag_point,
                                     /*is_touch_dragging=*/false,
                                     /*event_source_item=*/item);
  drag_point.Offset(5.f, 0.f);
  GetOverviewSession()->Drag(item, drag_point);
  wm::ActivateWindow(window.get());
  EXPECT_TRUE(InOverviewSession());
}

// Tests that if an overview item is dragged, the activation of the window
// corresponding to another overview item does not cancel overview.
TEST_P(OverviewSessionTest,
       ActivateAnotherOverviewWindowDuringOverviewDragNotCancelOverview) {
  UpdateDisplay("800x600");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  ToggleOverview();
  OverviewItemBase* item1 = GetOverviewItemForWindow(window1.get());
  gfx::PointF drag_point = item1->target_bounds().CenterPoint();
  GetOverviewSession()->InitiateDrag(item1, drag_point,
                                     /*is_touch_dragging=*/false,
                                     /*event_source_item=*/item1);
  drag_point.Offset(5.f, 0.f);
  GetOverviewSession()->Drag(item1, drag_point);
  wm::ActivateWindow(window2.get());
  EXPECT_TRUE(InOverviewSession());
}

// Tests that if an overview item is dragged, the activation of a window
// excluded from overview does not cancel overview.
TEST_P(OverviewSessionTest,
       ActivateWindowExcludedFromOverviewDuringOverviewDragNotCancelOverview) {
  UpdateDisplay("800x600");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(), aura::client::WINDOW_TYPE_POPUP));
  EXPECT_TRUE(window_util::ShouldExcludeForOverview(window2.get()));
  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  gfx::PointF drag_point = item1->target_bounds().CenterPoint();
  GetOverviewSession()->InitiateDrag(item1, drag_point,
                                     /*is_touch_dragging=*/false,
                                     /*event_source_item=*/item1);
  drag_point.Offset(5.f, 0.f);
  GetOverviewSession()->Drag(item1, drag_point);
  wm::ActivateWindow(window2.get());
  EXPECT_TRUE(InOverviewSession());
}

// Tests that exiting overview mode without selecting a window restores focus
// to the previously focused window.
TEST_P(OverviewSessionTest, CancelRestoresFocus) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), window_util::GetFocusedWindow());

  // In overview mode, the overview focus window should be focused.
  ToggleOverview();
  EXPECT_EQ(GetOverviewSession()->GetOverviewFocusWindow(),
            window_util::GetFocusedWindow());

  // If canceling overview mode, focus should be restored.
  ToggleOverview();
  EXPECT_EQ(window.get(), window_util::GetFocusedWindow());
}

// Tests that overview mode is exited if the last remaining window is destroyed.
TEST_P(OverviewSessionTest, LastWindowDestroyed) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  ToggleOverview();

  window1.reset();
  window2.reset();
  EXPECT_FALSE(InOverviewSession());
}

// Tests that entering overview mode restores a window to its original
// target location.
TEST_P(OverviewSessionTest, QuickReentryRestoresInitialTransform) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(400, 400)));
  gfx::Rect initial_bounds = GetTransformedBounds(window.get());
  ToggleOverview();
  // Quickly exit and reenter overview mode. The window should still be
  // animating when we reenter. We cannot short circuit animations for this but
  // we also don't have to wait for them to complete.
  {
    ui::ScopedAnimationDurationScaleMode test_duration_mode(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
    ToggleOverview();
    ToggleOverview();
  }
  EXPECT_NE(initial_bounds, GetTransformedTargetBounds(window.get()));
  ToggleOverview();
  EXPECT_FALSE(InOverviewSession());
  EXPECT_EQ(initial_bounds, GetTransformedTargetBounds(window.get()));
}

// Tests that windows with modal child windows are transformed with the modal
// child even though not activatable themselves.
TEST_P(OverviewSessionTest, ModalChild) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> child(CreateTestWindow(bounds));
  child->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kWindow);
  ::wm::AddTransientChild(window.get(), child.get());
  EXPECT_EQ(window->parent(), child->parent());
  ToggleOverview();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_TRUE(child->IsVisible());
  EXPECT_EQ(GetTransformedTargetBounds(child.get()),
            GetTransformedTargetBounds(window.get()));
  ToggleOverview();
}

// Tests that clicking a modal window's parent activates the modal window in
// overview.
TEST_P(OverviewSessionTest, ClickModalWindowParent) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(180, 180)));
  std::unique_ptr<aura::Window> child(
      CreateTestWindow(gfx::Rect(200, 0, 180, 180)));
  child->SetProperty(aura::client::kModalKey, ui::mojom::ModalType::kWindow);
  ::wm::AddTransientChild(window.get(), child.get());
  EXPECT_FALSE(WindowsOverlapping(window.get(), child.get()));
  EXPECT_EQ(window->parent(), child->parent());
  ToggleOverview();
  // Given that their relative positions are preserved, the windows should still
  // not overlap.
  EXPECT_FALSE(WindowsOverlapping(window.get(), child.get()));
  ClickWindow(window.get());
  EXPECT_FALSE(InOverviewSession());

  // Clicking on window1 should activate child1.
  EXPECT_TRUE(wm::IsActiveWindow(child.get()));
}

// Verifies bubble transient windows hide in Overview, reappear on Overview
// exit.
TEST_P(OverviewSessionTest, HideBubbleTransient) {
  std::unique_ptr<aura::Window> window(
      CreateAppWindow(gfx::Rect(0, 0, 300, 300)));

  // Create a bubble widget that's anchored to frame.
  auto bubble_delegate = std::make_unique<views::BubbleDialogDelegateView>(
      NonClientFrameViewAsh::Get(window.get()), views::BubbleBorder::TOP_RIGHT);

  // The line below is essential to make sure that the bubble doesn't get closed
  // when entering overview.
  bubble_delegate->set_close_on_deactivate(false);
  bubble_delegate->set_parent_window(window.get());
  views::Widget* bubble_widget(views::BubbleDialogDelegateView::CreateBubble(
      std::move(bubble_delegate)));
  aura::Window* bubble_window = bubble_widget->GetNativeWindow();
  ASSERT_TRUE(window_util::AsBubbleDialogDelegate(bubble_window));

  bubble_widget->Show();
  EXPECT_TRUE(wm::HasTransientAncestor(bubble_window, window.get()));

  // Hides bubble transient windows on entering Overview mode.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(bubble_window->IsVisible());

  // Re-shows bubble transient windows on exiting Overview mode.
  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());
  EXPECT_TRUE(bubble_window->IsVisible());
}

// Tests that windows remain on the display they are currently on in overview
// mode, and that the close buttons are on matching displays.
TEST_P(OverviewSessionTest, MultipleDisplays) {
  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect bounds1(0, 0, 400, 400);
  gfx::Rect bounds2(650, 0, 400, 400);

  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds1));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds2));
  std::unique_ptr<aura::Window> window4(CreateTestWindow(bounds2));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());

  // In overview mode, each window remains in the same root window.
  ToggleOverview();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());
  EXPECT_EQ(root_windows[1], window3->GetRootWindow());
  EXPECT_EQ(root_windows[1], window4->GetRootWindow());

  // Window indices are based on top-down order. The reverse of our creation.
  CheckWindowAndCloseButtonInScreen(window1.get(),
                                    GetOverviewItemForWindow(window1.get()));
  CheckWindowAndCloseButtonInScreen(window2.get(),
                                    GetOverviewItemForWindow(window2.get()));
  CheckWindowAndCloseButtonInScreen(window3.get(),
                                    GetOverviewItemForWindow(window3.get()));
  CheckWindowAndCloseButtonInScreen(window4.get(),
                                    GetOverviewItemForWindow(window4.get()));
}

// Tests shutting down during overview.
TEST_P(OverviewSessionTest, Shutdown) {
  // These windows will be deleted when the test exits and the Shell instance
  // is shut down.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
}

// Tests adding a display during overview.
TEST_P(OverviewSessionTest, AddDisplay) {
  UpdateDisplay("500x400");
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());
  UpdateDisplay("500x400,500x400");
  EXPECT_FALSE(InOverviewSession());
}

// Tests removing a display during overview.
TEST_P(OverviewSessionTest, RemoveDisplay) {
  UpdateDisplay("500x400,500x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(550, 0, 100, 100)));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());
  UpdateDisplay("500x400");
  EXPECT_FALSE(InOverviewSession());
}

// Tests removing a display during overview with NON_ZERO_DURATION animation.
TEST_P(OverviewSessionTest, RemoveDisplayWithAnimation) {
  UpdateDisplay("500x400,500x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(550, 0, 100, 100)));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("500x400");
  EXPECT_FALSE(InOverviewSession());
}

// Tests that tab key does not cause crash if pressed just after overview
// session exits.
TEST_P(OverviewSessionTest, NoCrashOnTabAfterExit) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  wm::ActivateWindow(window.get());

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that tab key does not cause crash if pressed just after overview
// session exits, and a child window was active before session start.
TEST_P(OverviewSessionTest,
       NoCrashOnTabAfterExitWithChildWindowInitiallyFocused) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  std::unique_ptr<aura::Window> child_window =
      ChildTestWindowBuilder(window.get()).Build();

  wm::ActivateWindow(child_window.get());

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that tab key does not cause crash if pressed just after overview
// session exits when no windows existed before starting overview session.
TEST_P(OverviewSessionTest, NoCrashOnTabAfterExitWithNoWindows) {
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that dragging a window from overview creates a drop target on the same
// display.
TEST_P(OverviewSessionTest, DropTargetOnCorrectDisplayForDraggingFromOverview) {
  UpdateDisplay("600x500,600x500");
  EnterTabletMode();
  // DisplayConfigurationObserver enables mirror mode when tablet mode is
  // enabled. Disable mirror mode to test multiple displays.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  base::RunLoop().RunUntilIdle();

  const aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  std::unique_ptr<aura::Window> primary_screen_window =
      CreateTestWindow(gfx::Rect(0, 0, 600, 500));
  ASSERT_EQ(root_windows[0], primary_screen_window->GetRootWindow());
  std::unique_ptr<aura::Window> secondary_screen_window =
      CreateTestWindow(gfx::Rect(600, 0, 600, 500));
  ASSERT_EQ(root_windows[1], secondary_screen_window->GetRootWindow());

  ToggleOverview();
  auto* primary_screen_item =
      GetOverviewItemForWindow(primary_screen_window.get());
  auto* secondary_screen_item =
      GetOverviewItemForWindow(secondary_screen_window.get());

  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  gfx::PointF drag_point = primary_screen_item->target_bounds().CenterPoint();
  GetOverviewSession()->InitiateDrag(primary_screen_item, drag_point,
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/primary_screen_item);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point.Offset(5.f, 0.f);
  GetOverviewSession()->Drag(primary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(1));
  ASSERT_TRUE(GetDropTarget(0));
  EXPECT_EQ(root_windows[0], GetDropTarget(0)->root_window());
  GetOverviewSession()->CompleteDrag(primary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point = secondary_screen_item->target_bounds().CenterPoint();
  GetOverviewSession()->InitiateDrag(
      secondary_screen_item, drag_point,
      /*is_touch_dragging=*/true,
      /*event_source_item=*/secondary_screen_item);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point.Offset(5.f, 0.f);
  GetOverviewSession()->Drag(secondary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  ASSERT_TRUE(GetDropTarget(1));
  EXPECT_EQ(root_windows[1], GetDropTarget(1)->root_window());
  GetOverviewSession()->CompleteDrag(secondary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
}

// Tests that toggling overview on and off does not cancel drag.
TEST_P(OverviewSessionTest, DragDropInProgress) {
  auto* window_delegate =
      aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
  window_delegate->set_window_component(HTCAPTION);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      window_delegate, -1, gfx::Rect(100, 100)));

  GetEventGenerator()->set_current_screen_location(
      window->GetBoundsInScreen().CenterPoint());
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseBy(10, 10);
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), window->bounds());

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  GetEventGenerator()->MoveMouseBy(10, 10);

  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());

  GetEventGenerator()->MoveMouseBy(10, 10);
  GetEventGenerator()->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(gfx::Rect(30, 30, 100, 100), window->bounds());
}

// Tests that toggling overview on removes any resize shadows that may have been
// present.
TEST_P(OverviewSessionTest, DragWindowShadow) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(100, 100)));
  wm::ActivateWindow(window.get());
  Shell::Get()->resize_shadow_controller()->ShowShadow(window.get(), HTTOP);

  ToggleOverview();
  ResizeShadow* shadow =
      Shell::Get()->resize_shadow_controller()->GetShadowForWindowForTest(
          window.get());
  EXPECT_FALSE(shadow);
}

// Test that a label is created under the window on entering overview mode.
TEST_P(OverviewSessionTest, CreateLabelUnderWindow) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(300, 500)));
  const std::u16string window_title = u"My window";
  window->SetTitle(window_title);
  ToggleOverview();
  auto* window_item = GetOverviewItemsForRoot(0).back().get();
  views::Label* label = GetLabelView(window_item);
  ASSERT_TRUE(label);

  // Verify the label matches the window title.
  EXPECT_EQ(window_title, label->GetText());

  // Update the window title and check that the label is updated, too.
  const std::u16string updated_title = u"Updated title";
  window->SetTitle(updated_title);
  EXPECT_EQ(updated_title, label->GetText());

  // Labels are located based on target_bounds, not the actual window item
  // bounds.
  gfx::RectF label_bounds(label->GetWidget()->GetWindowBoundsInScreen());
  EXPECT_EQ(label_bounds, window_item->target_bounds());
}

// Tests that overview updates the window positions if the display orientation
// changes.
TEST_P(OverviewSessionTest, DisplayOrientationChanged) {
  aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
  UpdateDisplay("600x200");
  EXPECT_EQ(gfx::Rect(600, 200), root_window->bounds());
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 3; i++) {
    windows.push_back(
        std::unique_ptr<aura::Window>(CreateTestWindow(gfx::Rect(150, 150))));
  }

  ToggleOverview();
  for (const auto& window : windows) {
    EXPECT_TRUE(root_window->bounds().Contains(
        GetTransformedTargetBounds(window.get())));
  }

  // Rotate the display, windows should be repositioned to be within the screen
  // bounds.
  UpdateDisplay("600x200/r");
  EXPECT_EQ(gfx::Rect(200, 600), root_window->bounds());
  for (const auto& window : windows) {
    EXPECT_TRUE(root_window->bounds().Contains(
        GetTransformedTargetBounds(window.get())));
  }
}

TEST_P(OverviewSessionTest, AcceleratorInOverviewSession) {
  ToggleOverview();
  auto* accelerator_controller = Shell::Get()->accelerator_controller();
  auto* ewh = AcceleratorControllerImpl::TestApi(accelerator_controller)
                  .GetExitWarningHandler();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_FALSE(IsUIShown(ewh));

  PressAndReleaseKey(ui::VKEY_Q, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_TRUE(IsUIShown(ewh));
}

// Tests that overview session will exit when clicking on the empty area in
// overview.
TEST_P(OverviewSessionTest, ExitOverviewWhenClickingEmptyArea) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  ToggleOverview();
  OverviewController* overview_controller = GetOverviewController();
  ASSERT_TRUE(overview_controller->InOverviewSession());
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());

  auto* overview_item = GetOverviewItemForWindow(window.get());
  EXPECT_TRUE(overview_item);
  const auto outside_point =
      gfx::ToRoundedPoint(
          overview_item->GetTransformedBounds().bottom_right()) +
      gfx::Vector2d(20, 20);

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(outside_point);
  event_generator->ClickLeftButton();
  EXPECT_FALSE(overview_controller->InOverviewSession());
}

// Tests hitting the escape and back keys exits overview mode.
TEST_P(OverviewSessionTest, ExitOverviewWithKey) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());

  // Tests that in tablet mode, if we snap the only overview window, we cannot
  // exit overview mode.
  EnterTabletMode();
  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  GetSplitViewController()->SnapWindow(window.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  PressAndReleaseKey(ui::VKEY_BROWSER_BACK);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
}

// Regression test for clusterfuzz crash. https://crbug.com/920568
TEST_P(OverviewSessionTest, TypeThenPressEscapeTwice) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  ToggleOverview();

  // Type some characters.
  PressAndReleaseKey(ui::VKEY_A);
  PressAndReleaseKey(ui::VKEY_B);
  PressAndReleaseKey(ui::VKEY_C);
  EXPECT_TRUE(GetOverviewSession()->GetOverviewFocusWindow());

  // Pressing escape twice should not crash.
  PressAndReleaseKey(ui::VKEY_ESCAPE);
  PressAndReleaseKey(ui::VKEY_ESCAPE);
}

TEST_P(OverviewSessionTest, CancelOverviewOnMouseClick) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 10, 100, 100)));
  // Move mouse to point in the background page. Sending an event here will pass
  // it to the WallpaperController in both regular and overview mode.
  GetEventGenerator()->MoveMouseTo(gfx::Point(0, 0));

  // Clicking on the background page while not in overview should not toggle
  // overview.
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(InOverviewSession());

  // Switch to overview mode. Clicking should now exit overview mode.
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  // Choose a point that doesn't intersect with the window or the desks bar.
  const gfx::Point point_in_background_page = GetGridBounds().CenterPoint();
  GetEventGenerator()->MoveMouseTo(point_in_background_page);
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(InOverviewSession());
}

// Tests tapping on the desktop itself to cancel overview mode.
TEST_P(OverviewSessionTest, CancelOverviewOnTap) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 10, 100, 100)));

  // Tapping on the background page while not in overview should not toggle
  // overview.
  GetEventGenerator()->GestureTapAt(gfx::Point(0, 0));
  EXPECT_FALSE(InOverviewSession());

  // Switch to overview mode. Tapping should now exit overview mode.
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  // A point that doesn't intersect with the window nor the desks bar. This
  // causes events located at the point to be passed to WallpaperController, and
  // not the window.
  const gfx::Point point_in_background_page = GetGridBounds().CenterPoint();
  GetEventGenerator()->GestureTapAt(point_in_background_page);
  EXPECT_FALSE(InOverviewSession());
}

// Start dragging a window and activate overview mode. This test should not
// crash or DCHECK inside aura::Window::StackChildRelativeTo().
TEST_P(OverviewSessionTest, OverviewWhileDragging) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window.get(), gfx::PointF(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  ASSERT_TRUE(resizer.get());
  gfx::PointF location = resizer->GetInitialLocation();
  location.Offset(20, 20);
  resizer->Drag(location, 0);
  ToggleOverview();
  resizer->RevertDrag();
}

// Verify that the overview no windows indicator appears when entering overview
// mode with no windows.
TEST_P(OverviewSessionTest, NoWindowsIndicator) {
  UpdateDisplay("400x300,400x300");

  // Verify that by entering overview mode without windows, the no items
  // indicator appears.
  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  ASSERT_EQ(0u, GetOverviewItemsForRoot(0).size());
  for (auto& grid : GetOverviewSession()->grid_list())
    EXPECT_TRUE(grid->no_windows_widget());
}

// Verify that the overview no windows indicator position is as expected.
TEST_P(OverviewSessionTest, NoWindowsIndicatorPosition) {
  UpdateDisplay("400x300");

  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  RoundedLabelWidget* no_windows_widget =
      GetOverviewSession()->grid_list()[0]->no_windows_widget();
  ASSERT_TRUE(no_windows_widget);

  display::Screen* screen = display::Screen::GetScreen();

  // The expected y of the label will be the screen minus the shelf and desks
  // bar.
  auto get_expected_y = [&screen]() -> int {
    const int display_height = screen->GetPrimaryDisplay().bounds().height();
    const int grid_y = kDeskBarZeroStateHeight;
    int grid_height = display_height - ShelfConfig::Get()->shelf_size() -
                      kDeskBarZeroStateHeight;
    return grid_y + grid_height / 2;
  };

  // Verify that originally the label is in the center of the workspace. For
  // forest, the padding calculations are much more complicated and we need to
  // account for the birch bar, so we just check that the widget is roughly
  // centered vertically.
  gfx::Point no_windows_centerpoint =
      no_windows_widget->GetWindowBoundsInScreen().CenterPoint();
  if (features::IsForestFeatureEnabled()) {
    EXPECT_EQ(200, no_windows_centerpoint.x());
    EXPECT_GT(no_windows_centerpoint.y(), kDeskBarZeroStateHeight);
    EXPECT_LT(no_windows_centerpoint.y(),
              screen->GetPrimaryDisplay().bounds().height() -
                  ShelfConfig::Get()->shelf_size());
  } else {
    EXPECT_EQ(gfx::Point(200, get_expected_y()), no_windows_centerpoint);
  }

  // Verify that after rotating the display, the label is centered in the
  // workspace.
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  no_windows_centerpoint =
      no_windows_widget->GetWindowBoundsInScreen().CenterPoint();
  if (features::IsForestFeatureEnabled()) {
    EXPECT_EQ(150, no_windows_centerpoint.x());
    EXPECT_GT(no_windows_centerpoint.y(), kDeskBarZeroStateHeight);
    EXPECT_LT(no_windows_centerpoint.y(),
              screen->GetPrimaryDisplay().bounds().height() -
                  ShelfConfig::Get()->shelf_size());
  } else {
    EXPECT_EQ(gfx::Point(150, get_expected_y()), no_windows_centerpoint);
  }
}

// Tests that toggling overview on removes any resize shadows that may have been
// present.
TEST_P(OverviewSessionTest, DragMinimizedWindowHasStableSize) {
  UpdateDisplay(base::StringPrintf("1920x1200*%s", display::kDsfStr_1_777));
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  WindowState::Get(window.get())->Minimize();
  ToggleOverview();
  auto* overview_item = GetOverviewItemForWindow(window.get());
  auto* widget = overview_item->item_widget();

  gfx::Rect workarea =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  gfx::PointF drag_point(workarea.CenterPoint());
  GetOverviewSession()->InitiateDrag(overview_item, drag_point,
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/overview_item);
  gfx::Size target_size =
      GetTransformedTargetBounds(widget->GetNativeWindow()).size();

  drag_point.Offset(0, 10.5f);
  GetOverviewSession()->Drag(overview_item, drag_point);
  gfx::Size new_target_size =
      GetTransformedTargetBounds(widget->GetNativeWindow()).size();
  EXPECT_EQ(target_size, new_target_size);
  target_size = new_target_size;

  drag_point.Offset(0, 10.5f);
  GetOverviewSession()->Drag(overview_item, drag_point);
  EXPECT_EQ(target_size,
            GetTransformedTargetBounds(widget->GetNativeWindow()).size());

  GetOverviewSession()->CompleteDrag(overview_item, drag_point);
}

// Tests that the bounds of the grid do not intersect the shelf or its hotseat.
TEST_P(OverviewSessionTest, OverviewGridBounds) {
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());

  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  const gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  EXPECT_FALSE(GetGridBounds().Intersects(shelf_bounds));

  if (!features::IsForestFeatureEnabled()) {
    const gfx::Rect hotseat_bounds =
        shelf->hotseat_widget()->GetWindowBoundsInScreen();
    EXPECT_FALSE(GetGridBounds().Intersects(hotseat_bounds));
  }
}

TEST_P(OverviewSessionTest, NoWindowsIndicatorPositionSplitview) {
  UpdateDisplay("400x300");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(GetOverviewSession());
  RoundedLabelWidget* no_windows_widget =
      GetOverviewSession()->grid_list()[0]->no_windows_widget();
  EXPECT_FALSE(no_windows_widget);

  // Tests that when snapping a window to the left in splitview, the no windows
  // indicator shows up in the middle of the right side of the screen.
  GetSplitViewController()->SnapWindow(window.get(), SnapPosition::kPrimary);
  no_windows_widget = GetOverviewSession()->grid_list()[0]->no_windows_widget();
  ASSERT_TRUE(no_windows_widget);

  // Take that into account of the divider width.
  const int bounds_left = 200 + kSplitviewDividerShortSideLength / 2;
  int expected_x = bounds_left + (400 - (bounds_left)) / 2;
  const int expected_y = (300 - ShelfConfig::Get()->in_app_shelf_size()) / 2;

  // The x location should be in the center. The y location is roughly in the
  // center. A lot of calculations go towards the padding and birch and desks
  // bar for the y location.
  gfx::Point no_windows_centerpoint =
      no_windows_widget->GetWindowBoundsInScreen().CenterPoint();
  EXPECT_EQ(gfx::Point(expected_x, expected_y), no_windows_centerpoint);

  // Tests that when snapping a window to the right in splitview, the no windows
  // indicator shows up in the middle of the left side of the screen.
  GetSplitViewController()->SnapWindow(window.get(), SnapPosition::kSecondary);
  no_windows_centerpoint =
      no_windows_widget->GetWindowBoundsInScreen().CenterPoint();
  expected_x = /*bounds_right=*/(200 - 4) / 2;
  EXPECT_EQ(gfx::Point(expected_x, expected_y), no_windows_centerpoint);
}

// Tests that the no windows indicator shows properly after adding an item.
TEST_P(OverviewSessionTest, NoWindowsIndicatorAddItem) {
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  GetSplitViewController()->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(GetOverviewSession()->grid_list()[0]->no_windows_widget());

  GetOverviewSession()->AddItem(window.get(), /*reposition=*/true,
                                /*animate=*/false, /*ignored_items=*/{},
                                /*index=*/0u);
  EXPECT_FALSE(GetOverviewSession()->grid_list()[0]->no_windows_widget());
}

// Tests that we do not exit overview mode until all the grids are empty.
TEST_P(OverviewSessionTest, ExitOverviewWhenAllGridsEmpty) {
  UpdateDisplay("500x400,500x400,500x400");

  // Create two windows with widgets (widgets are needed to close the windows
  // later in the test), one each on the first two monitors.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<views::Widget> widget1(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  std::unique_ptr<views::Widget> widget2(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));
  aura::Window* window1 = widget1->GetNativeWindow();
  aura::Window* window2 = widget2->GetNativeWindow();
  ASSERT_TRUE(
      window_util::MoveWindowToDisplay(window2, GetSecondaryDisplay().id()));
  ASSERT_EQ(root_windows[0], window1->GetRootWindow());
  ASSERT_EQ(root_windows[1], window2->GetRootWindow());

  // Enter overview mode. Verify that the no windows indicator is not visible on
  // any display.
  ToggleOverview();
  auto& grids = GetOverviewSession()->grid_list();
  ASSERT_TRUE(GetOverviewSession());
  ASSERT_EQ(3u, grids.size());
  for (auto& grid : grids)
    EXPECT_FALSE(grid->no_windows_widget());

  OverviewItem* item1 =
      static_cast<OverviewItem*>(GetOverviewItemForWindow(window1));
  OverviewItem* item2 =
      static_cast<OverviewItem*>(GetOverviewItemForWindow(window2));
  ASSERT_TRUE(item1 && item2);

  // Close `item2`. Verify that we are still in overview mode because `window1`
  // is still open. All the grids should not have a no windows widget.
  item2->CloseWindow();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetOverviewSession());
  ASSERT_EQ(3u, grids.size());
  EXPECT_FALSE(grids[0]->empty());
  EXPECT_TRUE(grids[1]->empty());
  EXPECT_TRUE(grids[2]->empty());
  for (auto& grid : grids)
    EXPECT_FALSE(grid->no_windows_widget());

  // Close `item1`. Verify that since no windows are open, we exit overview
  // mode.
  item1->CloseWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetOverviewSession());
}

// Tests window list animation states are correctly updated.
TEST_P(OverviewSessionTest, SetWindowListAnimationStates) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window3.get())->IsFullscreen());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsFullscreen());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Enter overview.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window3->layer()->GetAnimator()->is_animating());

  ToggleOverview();
}

// Tests window list animation states are correctly updated with selected
// window.
TEST_P(OverviewSessionTest, SetWindowListAnimationStatesWithSelectedWindow) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window3.get())->IsFullscreen());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsFullscreen());

  // Enter overview.
  ToggleOverview();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Click on |window3| to activate it and exit overview.
  // Should only set |should_animate_when_exiting_| and
  // |should_be_observed_when_exiting_| on window 3.
  TweenTester tester1(window1.get());
  TweenTester tester2(window2.get());
  TweenTester tester3(window3.get());
  ClickWindow(window3.get());
  EXPECT_EQ(gfx::Tween::ZERO, tester1.tween_type());
  EXPECT_EQ(gfx::Tween::ZERO, tester2.tween_type());
  EXPECT_EQ(gfx::Tween::EASE_OUT, tester3.tween_type());
}

// Tests OverviewWindowAnimationObserver can handle deleted window.
TEST_P(OverviewSessionTest,
       OverviewWindowAnimationObserverCanHandleDeletedWindow) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window3.get())->IsFullscreen());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsFullscreen());

  // Enter overview.
  ToggleOverview();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Click on |window3| to activate it and exit overview.
  // Should only set |should_animate_when_exiting_| and
  // |should_be_observed_when_exiting_| on window 3.
  {
    TweenTester tester1(window1.get());
    TweenTester tester2(window2.get());
    TweenTester tester3(window3.get());
    ClickWindow(window3.get());
    EXPECT_EQ(gfx::Tween::ZERO, tester1.tween_type());
    EXPECT_EQ(gfx::Tween::ZERO, tester2.tween_type());
    EXPECT_EQ(gfx::Tween::EASE_OUT, tester3.tween_type());
  }
  // Destroy |window1| and |window2| before |window3| finishes animation can be
  // handled in OverviewWindowAnimationObserver.
  window1.reset();
  window2.reset();
}

// Tests can handle OverviewWindowAnimationObserver was deleted.
TEST_P(OverviewSessionTest, HandleOverviewWindowAnimationObserverWasDeleted) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window3.get())->IsFullscreen());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsFullscreen());

  // Enter overview.
  ToggleOverview();

  // Click on |window2| to activate it and exit overview. Should only set
  // |should_animate_when_exiting_| and |should_be_observed_when_exiting_| on
  // window 2. Because the animation duration is zero in test, the
  // OverviewWindowAnimationObserver will delete itself immediately before
  // |window3| is added to it.
  ClickWindow(window2.get());
  EXPECT_FALSE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window3->layer()->GetAnimator()->is_animating());
}

// Tests can handle |gained_active| window is not in the |overview_grid| when
// OnWindowActivated.
TEST_P(OverviewSessionTest, HandleActiveWindowNotInOverviewGrid) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window3.get())->IsFullscreen());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(WindowState::Get(window1.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window3.get())->IsFullscreen());

  // Enter overview.
  ToggleOverview();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Create and active a new window should exit overview without error.
  auto widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);

  TweenTester tester1(window1.get());
  TweenTester tester2(window2.get());
  TweenTester tester3(window3.get());

  ClickWindow(widget->GetNativeWindow());

  // `window1` and `window2` should animate.
  EXPECT_EQ(gfx::Tween::ACCEL_20_DECEL_100, tester1.tween_type());
  EXPECT_EQ(gfx::Tween::ACCEL_20_DECEL_100, tester2.tween_type());
  EXPECT_EQ(gfx::Tween::ZERO, tester3.tween_type());
}

// Tests that AlwaysOnTopWindow can be handled correctly in new overview
// animations.
TEST_P(OverviewSessionTest, HandleAlwaysOnTopWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window5(
      CreateTestWindow(gfx::Rect(200, 200, 400, 400)));
  std::unique_ptr<aura::Window> window6(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window7(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window8(CreateTestWindow(bounds));
  window3->SetProperty(aura::client::kZOrderingKey,
                       ui::ZOrderLevel::kFloatingWindow);
  window5->SetProperty(aura::client::kZOrderingKey,
                       ui::ZOrderLevel::kFloatingWindow);

  // Control z order and MRU order.
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Will be maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());

  const WMEvent toggle_maximize_event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(window6.get())->OnWMEvent(&toggle_maximize_event);
  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  ASSERT_TRUE(WindowState::Get(window2.get())->IsFullscreen());
  ASSERT_TRUE(WindowState::Get(window7.get())->IsFullscreen());
  ASSERT_TRUE(WindowState::Get(window6.get())->IsMaximized());

  // Helper to check if `window` is visibly animating. In some overview
  // animations, we use tween zero, so there is no visible animation though it
  // technically is animating according to the ui::LayerAnimator API.
  auto is_visibly_animating = [](aura::Window* window) -> bool {
    ui::LayerAnimatorTestController controller(window->layer()->GetAnimator());
    ui::LayerAnimationSequence* sequence =
        controller.GetRunningSequence(ui::LayerAnimationElement::TRANSFORM);
    if (!sequence)
      return false;
    // There's only one element per sequence in the overview animation so this
    // is fine.
    ui::LayerAnimationElement* element = sequence->FirstElement();
    if (!element)
      return false;
    return element->tween_type() != gfx::Tween::ZERO;
  };

  // Case 1: Click on `window1` to activate it and exit overview.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ToggleOverview();
  // For entering animation, only animate `window1`, `window2`, `window3` and
  // `window5`. `window2` is fullscreen so all windows except `window1`,
  // `window3` and `window5` are occluded.
  EXPECT_TRUE(is_visibly_animating(window1.get()));
  EXPECT_TRUE(is_visibly_animating(window2.get()));
  EXPECT_TRUE(is_visibly_animating(window3.get()));
  EXPECT_FALSE(is_visibly_animating(window4.get()));
  EXPECT_TRUE(is_visibly_animating(window5.get()));
  EXPECT_FALSE(is_visibly_animating(window6.get()));
  EXPECT_FALSE(is_visibly_animating(window7.get()));
  EXPECT_FALSE(is_visibly_animating(window8.get()));
  WaitForOverviewEnterAnimation();

  // Click on `window1` to activate it and exit overview. `window2` occludes
  // everything but `window1`, `window3` and `window5`, and `window1` is
  // occluded by `window3`. So `window2`, `window3` and `window5` should be
  // animated.
  ClickWindow(window1.get());
  EXPECT_FALSE(is_visibly_animating(window1.get()));
  EXPECT_TRUE(is_visibly_animating(window2.get()));
  EXPECT_TRUE(is_visibly_animating(window3.get()));
  EXPECT_FALSE(is_visibly_animating(window4.get()));
  EXPECT_TRUE(is_visibly_animating(window5.get()));
  EXPECT_FALSE(is_visibly_animating(window6.get()));
  EXPECT_FALSE(is_visibly_animating(window7.get()));
  EXPECT_FALSE(is_visibly_animating(window8.get()));
  WaitForOverviewExitAnimation();

  // Case 2: Click on `window3` to activate it and exit overview. Since
  // `window2` is fullscreen, all windows after it are occluded, except
  // `window3` and `window5`, which are always on top. `window1` is not occluded
  // by `window2` but has the same bounds as `window3` so is occluded.
  // Reset window z-order. Need to toggle fullscreen first to workaround
  // https://crbug.com/816224.
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);

  // Enter overview.
  ToggleOverview();
  WaitForOverviewEnterAnimation();

  ClickWindow(window3.get());
  EXPECT_FALSE(is_visibly_animating(window1.get()));
  EXPECT_TRUE(is_visibly_animating(window2.get()));
  EXPECT_TRUE(is_visibly_animating(window3.get()));
  EXPECT_FALSE(is_visibly_animating(window4.get()));
  EXPECT_TRUE(is_visibly_animating(window5.get()));
  EXPECT_FALSE(is_visibly_animating(window6.get()));
  EXPECT_FALSE(is_visibly_animating(window7.get()));
  EXPECT_FALSE(is_visibly_animating(window8.get()));
  WaitForOverviewExitAnimation();

  // Case 3: Click on maximized `window6` to activate it and exit overview.
  // `window6` will become the topmost regular z-order window and will occlude
  // everything except `window2` as it is fullscreen and `window3` and `window5`
  // as they are always on top. Reset window z-order. Need to toggle fullscreen
  // first to workaround https://crbug.com/816224.
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  // Enter overview.
  ToggleOverview();
  WaitForOverviewEnterAnimation();

  ClickWindow(window6.get());
  EXPECT_FALSE(is_visibly_animating(window1.get()));
  EXPECT_FALSE(is_visibly_animating(window2.get()));
  EXPECT_TRUE(is_visibly_animating(window3.get()));
  EXPECT_FALSE(is_visibly_animating(window4.get()));
  EXPECT_TRUE(is_visibly_animating(window5.get()));
  EXPECT_TRUE(is_visibly_animating(window6.get()));
  EXPECT_FALSE(is_visibly_animating(window7.get()));
  EXPECT_FALSE(is_visibly_animating(window8.get()));
  WaitForOverviewExitAnimation();

  // Case 4: Click on `window8` to activate it and exit overview.
  // Should animate `window8`, `window1`, `window2`, `window3` and `window5`
  // because `window3` and `window5` are AlwaysOnTop windows and `window2` is
  // fullscreen.
  // Reset window z-order. Need to toggle fullscreen first to workaround
  // https://crbug.com/816224.
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);

  // Enter overview.
  ToggleOverview();
  WaitForOverviewEnterAnimation();

  ClickWindow(window8.get());
  EXPECT_FALSE(is_visibly_animating(window1.get()));
  EXPECT_TRUE(is_visibly_animating(window2.get()));
  EXPECT_TRUE(is_visibly_animating(window3.get()));
  EXPECT_FALSE(is_visibly_animating(window4.get()));
  EXPECT_TRUE(is_visibly_animating(window5.get()));
  EXPECT_FALSE(is_visibly_animating(window6.get()));
  EXPECT_FALSE(is_visibly_animating(window7.get()));
  EXPECT_TRUE(is_visibly_animating(window8.get()));
  WaitForOverviewExitAnimation();
}

// Verify that the selector item can animate after the item is dragged and
// released.
TEST_P(OverviewSessionTest, WindowItemCanAnimateOnDragRelease) {
  base::HistogramTester histogram_tester;
  UpdateDisplay("500x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EnterTabletMode();
  ToggleOverview();
  auto* item2 = GetOverviewItemForWindow(window2.get());
  // Drag |item2| in a way so that |window2| does not get activated.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      gfx::ToRoundedPoint(item2->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();

  generator->MoveMouseTo(gfx::Point(250, 200));
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 0);

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  generator->ReleaseLeftButton();
  EXPECT_TRUE(window2->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 1);
}

// Verify that the overview items titlebar and close button change visibility
// when a item is being dragged.
TEST_P(OverviewSessionTest, OverviewItemTitleCloseVisibilityOnDrag) {
  base::HistogramTester histogram_tester;
  UpdateDisplay("500x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  EnterTabletMode();
  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  // Start the drag on |item1|. Verify the dragged item, |item1| has both the
  // close button and titlebar hidden. The close button opacity however is
  // opaque as its a child of the header which handles fading away the whole
  // header. All other items, |item2| should only have the close button hidden.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();

  // The title is always shown and the layer is created only after the drag has
  // started moving.
  EXPECT_TRUE(GetCloseButton(item1)->layer());
  EXPECT_EQ(0.f, GetCloseButtonOpacity(item1));
  EXPECT_TRUE(GetCloseButton(item2)->layer());
  EXPECT_EQ(0.f, GetCloseButtonOpacity(item2));

  // Drag |item1| in a way so that |window1| does not get activated (drags
  // within a certain threshold count as clicks). Verify the close button and
  // titlebar is visible for all items.
  generator->MoveMouseTo(gfx::Point(250, 200));
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 0);

  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1.f, GetTitlebarOpacity(item1));
  EXPECT_EQ(1.f, GetCloseButtonOpacity(item1));
  EXPECT_EQ(1.f, GetTitlebarOpacity(item2));
  EXPECT_EQ(1.f, GetCloseButtonOpacity(item2));
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 1);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 1);
}

// Tests that overview widgets are stacked in the correct order.
TEST_P(OverviewSessionTest, OverviewWidgetStackingOrder) {
  base::HistogramTester histogram_tester;
  // Create three windows, including one minimized.
  std::unique_ptr<aura::Window> minimized(CreateTestWindow());
  WindowState::Get(minimized.get())->Minimize();
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());

  aura::Window* parent = window->parent();
  EXPECT_EQ(parent, minimized->parent());

  EnterTabletMode();
  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(minimized.get());
  auto* item2 = GetOverviewItemForWindow(window.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());

  views::Widget* widget1 = item1->item_widget();
  views::Widget* widget2 = item2->item_widget();
  views::Widget* widget3 = item3->item_widget();

  // The original order of stacking is determined by the order the associated
  // window was activated.
  EXPECT_TRUE(window_util::IsStackedBelow(widget2->GetNativeWindow(),
                                          widget3->GetNativeWindow()));
  EXPECT_TRUE(window_util::IsStackedBelow(widget1->GetNativeWindow(),
                                          widget2->GetNativeWindow()));

  // Verify that the item widget is stacked below the window.
  EXPECT_TRUE(
      window_util::IsStackedBelow(widget1->GetNativeWindow(), minimized.get()));
  EXPECT_TRUE(
      window_util::IsStackedBelow(widget2->GetNativeWindow(), window.get()));
  EXPECT_TRUE(
      window_util::IsStackedBelow(widget3->GetNativeWindow(), window3.get()));

  // Drag the first window. Verify that it's item widget is not stacked above
  // the other two.
  const gfx::Point start_drag =
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_drag);
  generator->PressLeftButton();
  EXPECT_TRUE(window_util::IsStackedBelow(widget2->GetNativeWindow(),
                                          widget1->GetNativeWindow()));
  EXPECT_TRUE(window_util::IsStackedBelow(widget3->GetNativeWindow(),
                                          widget1->GetNativeWindow()));

  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 0);

  // Drag to origin and then back to the start to avoid activating the window or
  // entering splitview.
  generator->MoveMouseTo(gfx::Point());
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 1);

  generator->MoveMouseTo(start_drag);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 2);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 0);

  generator->ReleaseLeftButton();

  // Verify the stacking order is same as before dragging started.
  EXPECT_TRUE(window_util::IsStackedBelow(widget2->GetNativeWindow(),
                                          widget3->GetNativeWindow()));
  EXPECT_TRUE(window_util::IsStackedBelow(widget1->GetNativeWindow(),
                                          widget2->GetNativeWindow()));

  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 2);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 1);
}

// Test that dragging an overview item to snap creates a drop target stacked at
// the bottom. Test that ending the drag removes the drop target.
TEST_P(OverviewSessionTest, DropTargetStackedAtBottomForOverviewItem) {
  EnterTabletMode();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  aura::Window* parent = window1->parent();
  ASSERT_EQ(parent, window2->parent());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window1.get())->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  generator->MoveMouseBy(5, 0);
  ASSERT_TRUE(GetDropTarget(0));
  EXPECT_TRUE(window_util::IsStackedBelow(
      GetDropTarget(0)->item_widget()->GetNativeWindow(), window2.get()));
  generator->ReleaseLeftButton();
  EXPECT_FALSE(GetDropTarget(0));
}

// Verify that a windows which enter overview mode have a visible backdrop, if
// the window is to be letter or pillar fitted.
TEST_P(OverviewSessionTest, Backdrop) {
  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Window |wide|, with size (400, 160) will be resized to (300, 160)
  // when the 400x300 is rotated to 300x400, and should be considered a normal
  // overview window after display change.
  UpdateDisplay("400x300");
  std::unique_ptr<aura::Window> wide(CreateTestWindow(gfx::Rect(400, 160)));
  std::unique_ptr<aura::Window> tall(CreateTestWindow(gfx::Rect(100, 300)));
  std::unique_ptr<aura::Window> normal(CreateTestWindow(gfx::Rect(300, 300)));

  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  auto* wide_item = GetOverviewItemForWindow(wide.get());
  auto* tall_item = GetOverviewItemForWindow(tall.get());
  auto* normal_item = GetOverviewItemForWindow(normal.get());

  // Only very tall and very wide windows will have a backdrop. The backdrop
  // only gets created if we need it once during the overview session.
  ASSERT_TRUE(GetBackdropView(wide_item));
  EXPECT_TRUE(GetBackdropView(wide_item)->GetVisible());
  EXPECT_TRUE(GetBackdropView(tall_item));
  ASSERT_TRUE(GetBackdropView(tall_item)->GetVisible());
  EXPECT_FALSE(GetBackdropView(normal_item));

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);

  // After rotation the former wide window will be a normal window and its
  // backdrop will still be there but invisible.
  ASSERT_TRUE(GetBackdropView(wide_item));
  EXPECT_FALSE(GetBackdropView(wide_item)->GetVisible());
  EXPECT_TRUE(GetBackdropView(tall_item));
  ASSERT_TRUE(GetBackdropView(tall_item)->GetVisible());
  EXPECT_FALSE(GetBackdropView(normal_item));

  // Test that leaving overview mode cleans up properly.
  ToggleOverview();
}

// Test that the rounded corners are removed during animations.
TEST_P(OverviewSessionTest, RoundedCornersVisibility) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Test that entering overview mode normally will disable all the rounded
  // corners until the animation is complete.
  EnterTabletMode();
  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_FALSE(HasRoundedCorner(item1));
  EXPECT_FALSE(HasRoundedCorner(item2));
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  EXPECT_TRUE(HasRoundedCorner(item1));
  EXPECT_TRUE(HasRoundedCorner(item2));

  // Tests that entering overview mode with all windows minimized (launcher
  // button pressed) will still disable all the rounded corners until the
  // animation is complete.
  ToggleOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);
  WindowState::Get(window1.get())->Minimize();
  WindowState::Get(window2.get())->Minimize();

  ToggleOverview();
  item1 = GetOverviewItemForWindow(window1.get());
  item2 = GetOverviewItemForWindow(window2.get());
  EXPECT_FALSE(HasRoundedCorner(item1));
  EXPECT_FALSE(HasRoundedCorner(item2));
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  EXPECT_TRUE(HasRoundedCorner(item1));
  EXPECT_TRUE(HasRoundedCorner(item2));

  // Test that leaving overview mode cleans up properly.
  ToggleOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);
}

// Test that the shadow disappears while dragging an overview item.
TEST_P(OverviewSessionTest, ShadowVisibilityDragging) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EnterTabletMode();
  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first window. Verify that the shadow was removed for the first
  // window but still exists for the second window as we do not make shadow
  // for a dragged window.
  const gfx::Point start_drag =
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_drag);
  generator->PressLeftButton();
  EXPECT_FALSE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window2->layer()->GetAnimator()->is_animating());

  EXPECT_TRUE(GetShadowBounds(item1).IsEmpty());
  EXPECT_FALSE(GetShadowBounds(item2).IsEmpty());

  // Drag to horizontally and then back to the start to avoid activating the
  // window, drag to close or entering splitview. Verify that the shadow is
  // invisible on both items during animation.
  generator->MoveMouseTo(gfx::Point(0, start_drag.y()));

  // The drop target window should be created with no shadow.
  auto* drop_target_item = GetDropTarget(0);
  ASSERT_TRUE(drop_target_item);
  EXPECT_TRUE(GetShadowBounds(drop_target_item).IsEmpty());

  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();

  generator->MoveMouseTo(start_drag);
  generator->ReleaseLeftButton();
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(GetShadowBounds(item1).IsEmpty());
  EXPECT_TRUE(GetShadowBounds(item2).IsEmpty());

  // Verify that the shadow is visble again after animation is finished.
  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(GetShadowBounds(item1).IsEmpty());
  EXPECT_FALSE(GetShadowBounds(item2).IsEmpty());
}

// Tests that the shadows in overview mode are placed correctly.
TEST_P(OverviewSessionTest, ShadowBounds) {
  // Helper function to check if the bounds of a shadow owned by |shadow_parent|
  // is contained within the bounds of |widget|.
  auto contains = [&](views::Widget* widget, OverviewItemBase* shadow_parent) {
    return gfx::Rect(widget->GetNativeWindow()->bounds().size())
        .Contains(GetShadowBounds(shadow_parent));
  };

  // Helper function which returns the ratio of the shadow owned by
  // |shadow_parent| width and height.
  auto shadow_ratio = [&](OverviewItemBase* shadow_parent) {
    gfx::RectF boundsf = gfx::RectF(GetShadowBounds(shadow_parent));
    return boundsf.width() / boundsf.height();
  };

  // Helper function which returns the ratio of the item width and height minus
  // the header and window margin.
  auto item_ratio = [](OverviewItemBase* item) {
    const gfx::RectF boundsf = item->target_bounds();
    return boundsf.width() / boundsf.height();
  };

  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Set top view insets to 0 so it is easy to check the ratios of the
  // shadows match the ratios of the untransformed windows.
  UpdateDisplay("900x800");
  std::unique_ptr<aura::Window> wide(
      CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect(400, 100)));
  std::unique_ptr<aura::Window> tall(
      CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect(100, 400)));
  std::unique_ptr<aura::Window> normal(
      CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect(200, 200)));
  wide->SetProperty(aura::client::kTopViewInset, 0);
  tall->SetProperty(aura::client::kTopViewInset, 0);
  normal->SetProperty(aura::client::kTopViewInset, 0);

  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  auto* wide_item = GetOverviewItemForWindow(wide.get());
  auto* tall_item = GetOverviewItemForWindow(tall.get());
  auto* normal_item = GetOverviewItemForWindow(normal.get());

  views::Widget* wide_widget = wide_item->item_widget();
  views::Widget* tall_widget = tall_item->item_widget();
  views::Widget* normal_widget = normal_item->item_widget();

  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();

  // Verify all the shadows are within the bounds of their respective item
  // widgets when the overview windows are positioned without animations.
  SetGridBounds(grid, gfx::Rect(400, 800));
  grid->PositionWindows(false);
  EXPECT_TRUE(contains(wide_widget, wide_item));
  EXPECT_TRUE(contains(tall_widget, tall_item));
  EXPECT_TRUE(contains(normal_widget, normal_item));

  // Verify the shadow of window with normal type preserves the ratio of the
  // original window. Otherwise, it preserves the ratio of the item bounds minus
  // the header of window margin.
  EXPECT_NEAR(shadow_ratio(wide_item), item_ratio(wide_item), 0.01f);
  EXPECT_NEAR(shadow_ratio(tall_item), item_ratio(tall_item), 0.01f);
  EXPECT_NEAR(shadow_ratio(normal_item), item_ratio(normal_item), 0.01f);

  // Verify all the shadows are within the bounds of their respective item
  // widgets when the overview windows are positioned with animations.
  SetGridBounds(grid, gfx::Rect(400, 800));
  grid->PositionWindows(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(contains(wide_widget, wide_item));
  EXPECT_TRUE(contains(tall_widget, tall_item));
  EXPECT_TRUE(contains(normal_widget, normal_item));

  EXPECT_NEAR(shadow_ratio(wide_item), item_ratio(wide_item), 0.01f);
  EXPECT_NEAR(shadow_ratio(tall_item), item_ratio(tall_item), 0.01f);
  EXPECT_NEAR(shadow_ratio(normal_item), item_ratio(normal_item), 0.01f);

  // Test that leaving overview mode cleans up properly.
  ToggleOverview();
}

// Verify that attempting to drag with a secondary finger works as expected.
TEST_P(OverviewSessionTest, DraggingWithTwoFingers) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  EnterTabletMode();
  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());

  const gfx::RectF original_bounds1 = item1->target_bounds();
  const gfx::RectF original_bounds2 = item2->target_bounds();

  constexpr int kTouchId1 = 1;
  constexpr int kTouchId2 = 2;

  // Dispatches a long press event at the event generators current location.
  // Long press is one way to start dragging in splitview.
  auto dispatch_long_press = [this]() {
    ui::GestureEventDetails event_details(ui::EventType::kGestureLongPress);
    const gfx::Point location = GetEventGenerator()->current_screen_location();
    ui::GestureEvent long_press(location.x(), location.y(), 0,
                                ui::EventTimeForNow(), event_details);
    GetEventGenerator()->Dispatch(&long_press);
  };

  // Verify that the bounds of the tapped window expand when touched.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(original_bounds1.CenterPoint()));
  generator->PressTouchId(kTouchId1);
  dispatch_long_press();
  EXPECT_GT(item1->target_bounds().width(), original_bounds1.width());
  EXPECT_GT(item1->target_bounds().height(), original_bounds1.height());

  // Verify that attempting to touch the second window with a second finger does
  // nothing to the second window. The first window remains the window to be
  // dragged.
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(original_bounds2.CenterPoint()));
  generator->PressTouchId(kTouchId2);
  dispatch_long_press();
  EXPECT_GT(item1->target_bounds().width(), original_bounds1.width());
  EXPECT_GT(item1->target_bounds().height(), original_bounds1.height());
  EXPECT_EQ(item2->target_bounds(), original_bounds2);

  // Verify the first window moves on drag.
  gfx::PointF last_center_point = item1->target_bounds().CenterPoint();
  generator->MoveTouchIdBy(kTouchId1, 40, 40);
  EXPECT_NE(last_center_point, item1->target_bounds().CenterPoint());
  EXPECT_EQ(original_bounds2.CenterPoint(),
            item2->target_bounds().CenterPoint());

  // Verify the first window moves on drag, even if we switch to a second
  // finger.
  last_center_point = item1->target_bounds().CenterPoint();
  generator->ReleaseTouchId(kTouchId2);
  generator->PressTouchId(kTouchId2);
  generator->MoveTouchIdBy(kTouchId2, 40, 40);
  EXPECT_NE(last_center_point, item1->target_bounds().CenterPoint());
  EXPECT_EQ(original_bounds2.CenterPoint(),
            item2->target_bounds().CenterPoint());
}

// Verify that shadows on windows disappear for the duration of overview mode.
TEST_P(OverviewSessionTest, ShadowDisappearsInOverview) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  // Verify that the shadow is initially visible.
  ::wm::ShadowController* shadow_controller = Shell::Get()->shadow_controller();
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window.get()));

  // Verify that the shadow is invisible after entering overview mode.
  ToggleOverview();
  EXPECT_FALSE(shadow_controller->IsShadowVisibleForWindow(window.get()));

  // Verify that the shadow is visible again after exiting overview mode.
  ToggleOverview();
  EXPECT_TRUE(shadow_controller->IsShadowVisibleForWindow(window.get()));
}

// Verify that PIP windows will be excluded from the overview, but not hidden.
TEST_P(OverviewSessionTest, PipWindowShownButExcludedFromOverview) {
  std::unique_ptr<aura::Window> pip_window(
      CreateTestWindow(gfx::Rect(200, 200)));
  WindowState* window_state = WindowState::Get(pip_window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);

  // Enter overview.
  ToggleOverview();

  // PIP window should be visible but not in the overview.
  EXPECT_TRUE(pip_window->IsVisible());
  EXPECT_FALSE(GetOverviewItemForWindow(pip_window.get()));
}

// Tests the PositionWindows function works as expected.
TEST_P(OverviewSessionTest, PositionWindows) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());

  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());
  const gfx::RectF bounds1 = item1->target_bounds();
  const gfx::RectF bounds2 = item2->target_bounds();
  const gfx::RectF bounds3 = item3->target_bounds();

  // Verify that the bounds remain the same when calling PositionWindows again.
  GetOverviewSession()->PositionWindows(/*animate=*/false);
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_EQ(bounds2, item2->target_bounds());
  EXPECT_EQ(bounds3, item3->target_bounds());

  // Verify that |item2| and |item3| change bounds when calling PositionWindows
  // while ignoring |item1|.
  GetOverviewSession()->PositionWindows(/*animate=*/false, {item1});
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_NE(bounds2, item2->target_bounds());
  EXPECT_NE(bounds3, item3->target_bounds());

  // Return the windows to their original bounds.
  GetOverviewSession()->PositionWindows(/*animate=*/false);

  // Verify that items that are animating before closing are ignored by
  // PositionWindows.
  SetAnimatingToClose(item1, true);
  SetAnimatingToClose(item2, true);
  GetOverviewSession()->PositionWindows(/*animate=*/false);
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_EQ(bounds2, item2->target_bounds());
  EXPECT_NE(bounds3, item3->target_bounds());
}

// Tests the grid bounds are as expected with different shelf auto hide
// behaviors and alignments.
TEST_P(OverviewSessionTest, GridBounds) {
  UpdateDisplay("700x600");
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(200, 200)));

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);

  // Test that with the bottom shelf, the grid should take up the entire display
  // minus the shelf area on the bottom regardless of auto hide behavior.
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 700, 600 - shelf_size), GetGridBounds());
  ToggleOverview();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 700, 600 - shelf_size), GetGridBounds());
  ToggleOverview();

  // Test that with the right shelf, the grid should take up the entire display
  // minus the shelf area on the right regardless of auto hide behavior.
  shelf->SetAlignment(ShelfAlignment::kRight);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 700 - shelf_size, 600), GetGridBounds());
  ToggleOverview();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 700 - shelf_size, 600), GetGridBounds());
  ToggleOverview();
}

// Tests that windows that have a backdrop can still be tapped normally.
// Regression test for crbug.com/938645.
TEST_P(OverviewSessionTest, SelectingWindowWithBackdrop) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(500, 200)));

  ToggleOverview();
  auto* item = GetOverviewItemForWindow(window.get());
  ASSERT_EQ(OverviewItemFillMode::kLetterBoxed,
            item->GetOverviewItemFillMode());

  // Tap the target.
  GetEventGenerator()->set_current_screen_location(
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint()));
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(InOverviewSession());
}

TEST_P(OverviewSessionTest, ShelfAlignmentChangeWhileInOverview) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);
  ToggleOverview();
  shelf->SetAlignment(ShelfAlignment::kRight);
  EXPECT_FALSE(InOverviewSession());
}

namespace {
class TestEventHandler : public ui::EventHandler {
 public:
  TestEventHandler() = default;
  ~TestEventHandler() override = default;
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override {
    if (event->type() != ui::EventType::kKeyPressed) {
      return;
    }

    has_seen_event_ = true;
    event->SetHandled();
    event->StopPropagation();
  }
  bool HasSeenEvent() { return has_seen_event_; }
  void Reset() { has_seen_event_ = false; }

 private:
  bool has_seen_event_ = false;
};
}  // namespace

// Test that keys are eaten when entering overview mode.
TEST_P(OverviewSessionTest, EatKeysDuringStartAnimation) {
  std::unique_ptr<aura::Window> test_window(CreateTestWindow());
  TestEventHandler test_event_handler;
  test_window->SetTargetHandler(&test_event_handler);
  test_window->Focus();

  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Keys shouldn't be eaten by overview session normally.
  PressAndReleaseKey(ui::VKEY_A);
  ASSERT_TRUE(test_window->HasFocus());
  EXPECT_TRUE(test_event_handler.HasSeenEvent());
  test_event_handler.Reset();

  // Keys should be eaten by overview session when entering overview mode.
  ToggleOverview();
  ASSERT_TRUE(OverviewController::Get()->IsInStartAnimation());
  ASSERT_TRUE(test_window->HasFocus());
  PressAndReleaseKey(ui::VKEY_B);
  EXPECT_FALSE(test_event_handler.HasSeenEvent());
  EXPECT_TRUE(InOverviewSession());

  WaitForOverviewEnterAnimation();
  ASSERT_FALSE(OverviewController::Get()->IsInStartAnimation());
  EXPECT_FALSE(test_window->HasFocus());

  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_C);
  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(test_event_handler.HasSeenEvent());
}

// Tests that in tablet mode, tapping on the background will go to home screen.
TEST_P(OverviewSessionTest, TapOnBackgroundGoToHome) {
  EnterTabletMode();
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  WindowState* window_state = WindowState::Get(window.get());

  EXPECT_FALSE(window_state->IsMinimized());
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsHomeScreenVisible());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  // Tap on the background. The tap location should be out of the tapping area
  // for back gesture. Otherwise, the touch event will be consumed and no
  // gesture event will be generated.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  GetEventGenerator()->GestureTapAt(
      gfx::Point(BackGestureEventHandler::kStartGoingBackLeftEdgeInset, 10));
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kExitAnimationComplete);

  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(window_state->IsMinimized());
  EXPECT_TRUE(Shell::Get()->app_list_controller()->IsHomeScreenVisible());
}

// Tests that in tablet mode, tapping on the background in split view mode will
// be no-op.
TEST_P(OverviewSessionTest, TapOnBackgroundInSplitView) {
  EnterTabletMode();
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window1(CreateTestWindow());

  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsHomeScreenVisible());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  GetSplitViewController()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());

  // Tap on the background.
  GetEventGenerator()->GestureTapAt(gfx::Point(10, 10));

  EXPECT_TRUE(InOverviewSession());
  EXPECT_FALSE(Shell::Get()->app_list_controller()->IsHomeScreenVisible());
  EXPECT_TRUE(GetSplitViewController()->InSplitViewMode());
}

// Tests starting the overview session using kFadeInEnter type.
TEST_P(OverviewSessionTest, FadeIn) {
  EnterTabletMode();
  // Create a minimized window.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  WindowState::Get(window.get())->Minimize();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ToggleOverview(OverviewEnterExitType::kFadeInEnter);
  ASSERT_TRUE(InOverviewSession());

  auto* item = GetOverviewItemForWindow(window.get())
                   ->GetLeafItemForWindow(window.get());

  // Verify that the item widget's transform is not animated as part of the
  // animation.
  views::Widget* widget = item->item_widget();
  EXPECT_FALSE(widget->GetLayer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));

  // Opacity should be animated to full opacity.
  EXPECT_EQ(1.0f, widget->GetLayer()->GetTargetOpacity());
  EXPECT_TRUE(widget->GetLayer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::OPACITY));

  // Validate item bounds are within the grid.
  const gfx::Rect bounds = gfx::ToEnclosedRect(item->target_bounds());
  EXPECT_TRUE(GetGridBounds().Contains(bounds));
  EXPECT_EQ(OverviewEnterExitType::kFadeInEnter,
            GetOverviewSession()->enter_exit_overview_type());
}

// Tests exiting the overview session using kFadeOutExit type.
TEST_P(OverviewSessionTest, FadeOutExit) {
  EnterTabletMode();
  std::unique_ptr<aura::Window> test_window(CreateAppWindow());

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  EXPECT_FALSE(WindowState::Get(test_window.get())->IsMinimized());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Grab the item widget before the session starts shutting down. The widget
  // should outlive the session, at least until the animations are done - given
  // that NON_ZERO_DURATION animation duration scale, it should be safe to
  // dereference the widget pointer immediately (synchronously) after the
  // session ends.
  auto* item = GetOverviewItemForWindow(test_window.get());
  views::Widget* grid_item_widget = item->item_widget();
  gfx::Rect item_bounds = grid_item_widget->GetWindowBoundsInScreen();

  ToggleOverview(OverviewEnterExitType::kFadeOutExit);
  ASSERT_FALSE(InOverviewSession());

  // The test window should be minimized as overview fade out exit starts.
  EXPECT_TRUE(WindowState::Get(test_window.get())->IsMinimized());

  // Verify that the item widget's transform is not animated as part of the
  // animation, and that item widget bounds are not changed after minimizing the
  // window.
  EXPECT_FALSE(grid_item_widget->GetLayer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::TRANSFORM));
  EXPECT_EQ(item_bounds, grid_item_widget->GetWindowBoundsInScreen());

  // Opacity should be animated to zero opacity.
  EXPECT_EQ(0.0f, grid_item_widget->GetLayer()->GetTargetOpacity());
  EXPECT_TRUE(grid_item_widget->GetLayer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::OPACITY));
}

// Tests that accessibility overrides are set as expected on overview related
// widgets.
TEST_P(OverviewSessionTest, AccessibilityFocusAnnotator) {
  // TODO(crbug.com/1360638): The body of this test is only run when Desk
  // Templates is turned OFF *and* Save & Recall is turned ON. Once the flag
  // flip for Save & Recall has truly landed, remove the `NoSavedDesks` variant
  // of this test below and remove the Save & Recall feature check at the start
  // of this test.
  if (DeskTemplatesOn() || !saved_desk_util::ShouldShowSavedDesksOptions()) {
    return;
  }

  base::AutoReset<bool> disable =
      OverviewController::Get()->SetDisableAppIdCheckForTests();

  auto window3 = CreateAppWindow(gfx::Rect(100, 100));
  auto window2 = CreateAppWindow(gfx::Rect(100, 100));
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));

  ToggleOverview();
  WaitForOverviewEnterAnimation();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  ASSERT_TRUE(focus_widget);

  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  auto* desk_widget = const_cast<views::Widget*>(grid->desks_widget());
  ASSERT_TRUE(desk_widget);

  // Overview items are in MRU order, so the expected order in the grid list is
  // the reverse creation order.
  auto* item_widget1 = GetOverviewItemForWindow(window1.get())->item_widget();
  auto* item_widget2 = GetOverviewItemForWindow(window2.get())->item_widget();
  auto* item_widget3 = GetOverviewItemForWindow(window3.get())->item_widget();

  // With this flag enabled, there are is no saved desk save desk container.
  if (features::IsSavedDeskUiRevampEnabled()) {
    // Order should be [focus_widget, item_widget1, item_widget2, item_widget3,
    // desk_widget, save_widget].
    CheckA11yOverrides("focus", focus_widget, desk_widget, item_widget1);
    CheckA11yOverrides("item1", item_widget1, focus_widget, item_widget2);
    CheckA11yOverrides("item2", item_widget2, item_widget1, item_widget3);
    CheckA11yOverrides("item3", item_widget3, item_widget2, desk_widget);
    CheckA11yOverrides("desk", desk_widget, item_widget3, focus_widget);

    // Remove `window2`. The new order should be [focus_widget, item_widget1,
    // item_widget3, desk_widget, save_widget].
    window2.reset();
    CheckA11yOverrides("focus", focus_widget, desk_widget, item_widget1);
    CheckA11yOverrides("item1", item_widget1, focus_widget, item_widget3);
    CheckA11yOverrides("item3", item_widget3, item_widget1, desk_widget);
    CheckA11yOverrides("desk", desk_widget, item_widget3, focus_widget);
    return;
  }

  SavedDeskSaveDeskButton* save_button = grid->GetSaveDeskForLaterButton();
  ASSERT_TRUE(save_button);
  views::Widget* save_widget = save_button->GetWidget();

  // Order should be [focus_widget, item_widget1, item_widget2, item_widget3,
  // desk_widget, save_widget].
  CheckA11yOverrides("focus", focus_widget, save_widget, item_widget1);
  CheckA11yOverrides("item1", item_widget1, focus_widget, item_widget2);
  CheckA11yOverrides("item2", item_widget2, item_widget1, item_widget3);
  CheckA11yOverrides("item3", item_widget3, item_widget2, desk_widget);
  CheckA11yOverrides("desk", desk_widget, item_widget3, save_widget);
  CheckA11yOverrides("save", save_widget, desk_widget, focus_widget);

  // Remove `window2`. The new order should be [focus_widget, item_widget1,
  // item_widget3, desk_widget, save_widget].
  window2.reset();
  CheckA11yOverrides("focus", focus_widget, save_widget, item_widget1);
  CheckA11yOverrides("item1", item_widget1, focus_widget, item_widget3);
  CheckA11yOverrides("item3", item_widget3, item_widget1, desk_widget);
  CheckA11yOverrides("desk", desk_widget, item_widget3, save_widget);
  CheckA11yOverrides("save", save_widget, desk_widget, focus_widget);
}

// Tests that accessibility overrides are set as expected on overview related
// widgets.
TEST_P(OverviewSessionTest, AccessibilityFocusAnnotatorNoSavedDesks) {
  // If saved desk is enabled, the a11y order changes. This is tested in
  // the saved desk test suite.
  if (DeskTemplatesOn() || saved_desk_util::ShouldShowSavedDesksOptions()) {
    return;
  }

  base::AutoReset<bool> disable =
      OverviewController::Get()->SetDisableAppIdCheckForTests();

  auto window3 = CreateAppWindow(gfx::Rect(100, 100));
  auto window2 = CreateAppWindow(gfx::Rect(100, 100));
  auto window1 = CreateAppWindow(gfx::Rect(100, 100));

  ToggleOverview();
  WaitForOverviewEnterAnimation();

  auto* focus_widget = views::Widget::GetWidgetForNativeWindow(
      GetOverviewSession()->GetOverviewFocusWindow());
  DCHECK(focus_widget);

  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  auto* desk_widget = const_cast<views::Widget*>(grid->desks_widget());
  DCHECK(desk_widget);

  // Overview items are in MRU order, so the expected order in the grid list is
  // the reverse creation order.
  auto* item_widget1 = GetOverviewItemForWindow(window1.get())->item_widget();
  auto* item_widget2 = GetOverviewItemForWindow(window2.get())->item_widget();
  auto* item_widget3 = GetOverviewItemForWindow(window3.get())->item_widget();

  // Order should be [focus_widget, item_widget1, item_widget2, item_widget3,
  // desk_widget].
  CheckA11yOverrides("focus", focus_widget, desk_widget, item_widget1);
  CheckA11yOverrides("item1", item_widget1, focus_widget, item_widget2);
  CheckA11yOverrides("item2", item_widget2, item_widget1, item_widget3);
  CheckA11yOverrides("item3", item_widget3, item_widget2, desk_widget);
  CheckA11yOverrides("desk", desk_widget, item_widget3, focus_widget);

  // Remove |window2|. The new order should be [focus_widget, item_widget1,
  // item_widget3, desk_widget].
  window2.reset();
  CheckA11yOverrides("focus", focus_widget, desk_widget, item_widget1);
  CheckA11yOverrides("item1", item_widget1, focus_widget, item_widget3);
  CheckA11yOverrides("item3", item_widget3, item_widget1, desk_widget);
  CheckA11yOverrides("desk", desk_widget, item_widget3, focus_widget);
}

// Tests that removing a transient child during overview does not result in a
// crash when exiting overview.
TEST_P(OverviewSessionTest, RemoveTransientNoCrash) {
  auto child = CreateTestWindow();
  auto parent = CreateTestWindow();
  wm::AddTransientChild(parent.get(), child.get());

  ToggleOverview();
  wm::RemoveTransientChild(parent.get(), child.get());
  ToggleOverview();
}

// Tests that closing the overview item destroys the entire transient tree. Note
// that closing does not destroy transient children which are ShellSurfaceBase,
// but this test covers the regular case.
TEST_P(OverviewSessionTest, ClosingTransientTree) {
  // Release ownership as it will get deleted by the transient window manager,
  // when the associated overview item is closed later.
  auto* window = CreateAppWindow().release();

  auto* child_window1 = CreateAppWindow().release();
  wm::AddTransientChild(window, child_window1);

  // Add a second child that is not backed by a widget.
  auto* child_window2 = CreateTestWindow().release();
  wm::AddTransientChild(window, child_window2);

  TestDestroyedWidgetObserver widget_observer(
      views::Widget::GetWidgetForNativeWindow(window));
  TestDestroyedWidgetObserver child_widget_observer(
      views::Widget::GetWidgetForNativeWindow(child_window1));

  ToggleOverview();

  // There is a uaf that happens after adding a new desk and removing a desk,
  // which transfers all windows to the new desk, removes the OverviewItem for
  // the window and then adds a new `OverviewItem` for the window. We replicate
  // that over here. See crbug.com/1317875.
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  RemoveDesk(controller->active_desk(), DeskCloseType::kCombineDesks);

  OverviewItem* item =
      static_cast<OverviewItem*>(GetOverviewItemForWindow(window));
  ASSERT_TRUE(item);
  item->CloseWindow();

  // `NativeWidgetAura::Close()` fires a post task.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(widget_observer.widget_destroyed());
  EXPECT_TRUE(child_widget_observer.widget_destroyed());
}

// Tests that enabling or disabling ChromeVox works in overview mode. Regression
// test for b/270929836.
TEST_P(OverviewSessionTest, ToggleChromeVox) {
  ToggleOverview();
  PressAndReleaseKey(ui::VKEY_Z, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
  PressAndReleaseKey(ui::VKEY_Z, ui::EF_ALT_DOWN | ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled());
}

TEST_P(OverviewSessionTest, FrameThrottlingBrowser) {
  FrameThrottlingController* frame_throttling_controller =
      Shell::Get()->frame_throttling_controller();
  const int window_count = 5;
  std::vector<viz::FrameSinkId> ids{
      {1u, 1u}, {2u, 2u}, {3u, 3u}, {4u, 4u}, {5u, 5u}};
  std::vector<std::unique_ptr<aura::Window>> windows;
  windows.reserve(window_count + 1);
  for (int i = 0; i < window_count; ++i) {
    windows.emplace_back(
        CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect()));
    windows[i]->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
    windows[i]->SetEmbedFrameSinkId(ids[i]);
  }

  ToggleOverview();
  EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
              testing::UnorderedElementsAreArray(ids));

  // Add a new window to overview.
  std::unique_ptr<aura::Window> new_window(
      CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect()));
  constexpr viz::FrameSinkId new_window_id{6u, 6u};
  new_window->SetEmbedFrameSinkId(new_window_id);
  new_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::BROWSER);
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  grid->AppendItem(new_window.get(), /*reposition=*/false, /*animate=*/false,
                   /*use_spawn_animation=*/false);
  windows.push_back(std::move(new_window));
  ids.push_back(new_window_id);
  EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
              testing::UnorderedElementsAreArray(ids));

  // Remove windows one by one.
  for (int i = 0; i < window_count + 1; ++i) {
    aura::Window* window = windows[i].get();
    ids.erase(ids.begin());
    auto* item = grid->GetOverviewItemContaining(window);
    grid->RemoveItem(item, /*item_destroying=*/false, /*reposition=*/false);
    EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
                testing::UnorderedElementsAreArray(ids));
  }
}

TEST_P(OverviewSessionTest, FrameThrottlingLacros) {
  FrameThrottlingController* frame_throttling_controller =
      Shell::Get()->frame_throttling_controller();
  const int window_count = 5;
  std::vector<viz::FrameSinkId> ids{
      {1u, 1u}, {2u, 2u}, {3u, 3u}, {4u, 4u}, {5u, 5u}};
  std::vector<std::unique_ptr<aura::Window>> windows;
  windows.reserve(window_count + 1);
  for (int i = 0; i < window_count; ++i) {
    windows.emplace_back(
        CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect()));
    windows[i]->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::LACROS);
    windows[i]->SetEmbedFrameSinkId(ids[i]);
  }
  for (auto& w : windows)
    EXPECT_FALSE(w->GetProperty(ash::kFrameRateThrottleKey));

  ToggleOverview();
  EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
              testing::UnorderedElementsAreArray(ids));
  for (auto& w : windows)
    EXPECT_TRUE(w->GetProperty(ash::kFrameRateThrottleKey));

  // Add a new window to overview.
  std::unique_ptr<aura::Window> new_window(
      CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect()));
  constexpr viz::FrameSinkId new_window_id{6u, 6u};
  new_window->SetEmbedFrameSinkId(new_window_id);
  new_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::LACROS);
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  grid->AppendItem(new_window.get(), /*reposition=*/false, /*animate=*/false,
                   /*use_spawn_animation=*/false);
  windows.push_back(std::move(new_window));
  ids.push_back(new_window_id);
  EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
              testing::UnorderedElementsAreArray(ids));
  for (auto& w : windows)
    EXPECT_TRUE(w->GetProperty(ash::kFrameRateThrottleKey));

  // Remove windows one by one.
  for (int i = 0; i < window_count + 1; ++i) {
    aura::Window* window = windows[i].get();
    ids.erase(ids.begin());
    auto* item = grid->GetOverviewItemContaining(window);
    grid->RemoveItem(item, /*item_destroying=*/false, /*reposition=*/false);
    EXPECT_THAT(frame_throttling_controller->GetFrameSinkIdsToThrottle(),
                testing::UnorderedElementsAreArray(ids));
    EXPECT_FALSE(window->GetProperty(ash::kFrameRateThrottleKey));
  }
}

TEST_P(OverviewSessionTest, FrameThrottlingArc) {
  testing::NiceMock<MockFrameThrottlingObserver> observer;
  FrameThrottlingController* frame_throttling_controller =
      Shell::Get()->frame_throttling_controller();
  frame_throttling_controller->AddArcObserver(&observer);

  const int window_count = 5;
  std::vector<std::unique_ptr<aura::Window>> windows;
  windows.reserve(window_count + 1);
  for (int i = 0; i < window_count; ++i) {
    windows.emplace_back(
        CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect()));
    windows[i]->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  }

  auto windows_to_throttle =
      base::ToVector(windows, &std::unique_ptr<aura::Window>::get);
  EXPECT_CALL(observer,
              OnThrottlingStarted(
                  testing::UnorderedElementsAreArray(windows_to_throttle),
                  frame_throttling_controller->GetCurrentThrottledFrameRate()));
  ToggleOverview();

  // Add a new window to overview.
  std::unique_ptr<aura::Window> new_window(
      CreateTestWindowInShellWithDelegate(nullptr, -1, gfx::Rect()));
  new_window->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  windows_to_throttle.push_back(new_window.get());
  EXPECT_CALL(observer, OnThrottlingEnded());
  EXPECT_CALL(observer,
              OnThrottlingStarted(
                  testing::UnorderedElementsAreArray(windows_to_throttle),
                  frame_throttling_controller->GetCurrentThrottledFrameRate()));
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  grid->AppendItem(new_window.get(), /*reposition=*/false, /*animate=*/false,
                   /*use_spawn_animation=*/false);
  windows.push_back(std::move(new_window));

  // Remove windows one by one. Once one window is out of the overview grid, no
  // more windows will be throttled.
  for (int i = 0; i < window_count + 1; ++i) {
    aura::Window* window = windows[i].get();
    if (i == 0)
      EXPECT_CALL(observer, OnThrottlingEnded());
    EXPECT_CALL(observer, OnThrottlingStarted(testing::_, testing::_)).Times(0);
    auto* item = grid->GetOverviewItemContaining(window);
    grid->RemoveItem(item, /*item_destroying=*/false, /*reposition=*/false);
  }
  frame_throttling_controller->RemoveArcObserver(&observer);
}

// Tests that if we combine a desk in overview, the overview applied clipping is
// removed properly (other portions of the window will not be visible on exiting
// overview). Regression test for http://b/282010852.
TEST_P(OverviewSessionTest, WindowClippingAfterCombiningDesks) {
  // Need at least two desks to combine them.
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);

  // Overview clip is used to apply an animation to remove the normal header and
  // keep it hidden during overview. So we need a non-zero top inset to
  // reproduce the bug.
  auto normal_window = CreateAppWindow();
  normal_window->SetProperty(aura::client::kTopViewInset, 32);
  ASSERT_TRUE(normal_window->layer()->clip_rect().IsEmpty());

  ui::ScopedAnimationDurationScaleMode scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  ASSERT_FALSE(normal_window->layer()->clip_rect().IsEmpty());

  // Combine the two desks while inside overview.
  RemoveDesk(controller->active_desk(), DeskCloseType::kCombineDesks);
  ui::LayerAnimationStoppedWaiter().Wait(normal_window->layer());

  // Tests that on exiting overview, the clip is removed.
  ToggleOverview();
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(normal_window->layer()->clip_rect().IsEmpty());
}

// Tests that if we tab while the desks bar is sliding out, there is no crash.
// Regression test for http://b/302708219.
TEST_P(OverviewSessionTest, TabbingDuringExitAnimation) {
  ui::ScopedAnimationDurationScaleMode scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ToggleOverview();
  WaitForOverviewEnterAnimation();

  auto* overview_grid = GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const views::Widget* desks_widget = overview_grid->desks_widget();

  // First activate the desks bar by clicking it (but do not click on the desk
  // preview because that will exit overview). See bug details for why we need
  // to activate the desks bar first.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      desks_widget->GetWindowBoundsInScreen().origin());
  generator->ClickLeftButton();
  ASSERT_TRUE(wm::IsActiveWindow(desks_widget->GetNativeWindow()));

  // Exit overview. This will slide out the desks widget.
  ToggleOverview();

  // Try tab focus traversal while the animation is in progress. There should be
  // no crash.
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
  PressAndReleaseKey(ui::VKEY_TAB);
}

TEST_P(OverviewSessionTest,
       OcclusionUpdatedOnOverviewToggleForVirtualDeskPreviewsSingleWindow) {
  using OcclusionState = aura::Window::OcclusionState;

  // We don't need to worry about virtual desk previews not showing up if we
  // have snapshots, so this test tests the case where we don't have snapshots.
  if (SnapshotOn()) {
    GTEST_SKIP();
  }
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // First ensure there are two desks.
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(2u, controller->desks().size());

  Desk* desk1 = controller->desks()[0].get();
  Desk* desk2 = controller->desks()[1].get();

  // Create one window on an inactive desk.
  std::unique_ptr<aura::Window> window(CreateAppWindow(gfx::Rect(100, 100)));
  controller->SendToDeskAtIndex(window.get(), 1);
  EXPECT_TRUE(base::Contains(desk2->windows(), window.get()));
  EXPECT_TRUE(desk1->is_active());
  window->TrackOcclusionState();

  // Window should be hidden on an inactive desk.
  EXPECT_EQ(OcclusionState::HIDDEN, window->GetOcclusionState());

  // Enter overview mode.
  ToggleOverview();

  // Window should immediately be marked as visible.
  EXPECT_EQ(OcclusionState::VISIBLE, window->GetOcclusionState());
  WaitForOverviewEnterAnimation();

  // Window should stay visible.
  EXPECT_EQ(OcclusionState::VISIBLE, window->GetOcclusionState());

  // Exit overview mode.
  ToggleOverview();

  // Window should still be visible until the animation finishes.
  EXPECT_EQ(OcclusionState::VISIBLE, window->GetOcclusionState());
  WaitForOverviewExitAnimation();

  // Overview mode pauses occlusion on exit for a while, so wait for this state.
  WaitForOcclusionStateChange(window.get(), OcclusionState::HIDDEN);
}

TEST_P(OverviewSessionTest,
       OcclusionUpdatedOnOverviewToggleForVirtualDeskPreviewsTwoWindows) {
  using OcclusionState = aura::Window::OcclusionState;

  // We don't need to worry about virtual desk previews not showing up if we
  // have snapshots, so this test tests the case where we don't have snapshots.
  if (SnapshotOn()) {
    GTEST_SKIP();
  }
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // First ensure there are two desks.
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(2u, controller->desks().size());

  Desk* desk1 = controller->desks()[0].get();
  Desk* desk2 = controller->desks()[1].get();

  // Create one window on the active desk.
  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(100, 100)));
  controller->SendToDeskAtIndex(window1.get(), 0);
  EXPECT_TRUE(base::Contains(desk1->windows(), window1.get()));
  window1->TrackOcclusionState();

  // Create one window on an inactive desk.
  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(100, 100)));
  controller->SendToDeskAtIndex(window2.get(), 1);
  EXPECT_TRUE(base::Contains(desk2->windows(), window2.get()));
  EXPECT_TRUE(desk1->is_active());
  window2->TrackOcclusionState();

  // `window2` should be hidden on an inactive desk.
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::HIDDEN, window2->GetOcclusionState());

  // Enter overview mode.
  ToggleOverview();

  // `window2` will not immediately be marked as visible, because the desks
  // widget is only shown after the animation finishes.
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  WaitForOverviewEnterAnimation();

  // `window2` should stay visible.
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());

  // Exit overview mode.
  ToggleOverview();

  // `window2` should still be visible until the animation finishes.
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
  EXPECT_EQ(OcclusionState::VISIBLE, window2->GetOcclusionState());
  WaitForOverviewExitAnimation();

  // Overview mode pauses occlusion on exit for a while, so wait for this state.
  WaitForOcclusionStateChange(window2.get(), OcclusionState::HIDDEN);
  EXPECT_EQ(OcclusionState::VISIBLE, window1->GetOcclusionState());
}

// Verify the following behavior when dragging an `OverviewItem` to the new desk
// button on a different display:
// 1. The new desk button on the target display changes to
// `DeskIconButton::State::kActive`.
// 2. New desk buttons on other displays remain in
// `DeskIconButton::State::kExpanded`.
// 3. Upon dropping the OverviewItem, all new desk buttons (including the target
// display) are restored to `DeskIconButton::State::kExpanded` state.
TEST_P(OverviewSessionTest, NewDeskButtonStateUpdateOnMultiDisplay) {
  auto skip_scale_up_new_desk_button_duration = OverviewWindowDragController::
      SkipNewDeskButtonScaleUpDurationForTesting();

  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const auto& displays = display_manager->active_display_list();
  ASSERT_EQ(2U, displays.size());

  const gfx::Point point_in_display1(502, 300);
  ASSERT_TRUE(displays[0].bounds().Contains(point_in_display1));
  ASSERT_FALSE(displays[1].bounds().Contains(point_in_display1));

  std::unique_ptr<aura::Window> window =
      CreateAppWindow(gfx::Rect(10, 10, 200, 100));
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  ASSERT_TRUE(IsWindowInItsCorrespondingOverviewGrid(window.get()));

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

  OverviewItemBase* overview_item = GetOverviewItemForWindow(window.get());
  ASSERT_TRUE(overview_item);

  // Drag the `overview_item` to new desk button on display #2 w/o releasing the
  // mouse. Verify that the new desk button on display #2 turns into
  // `DeskIconButton::State::kActive` state.
  auto* event_generator = GetEventGenerator();
  DragItemToPoint(overview_item,
                  new_desk_button1->GetBoundsInScreen().CenterPoint(),
                  event_generator, /*by_touch_gestures=*/false, /*drop=*/false);
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button0->state());
  EXPECT_EQ(DeskIconButton::State::kActive, new_desk_button1->state());

  // Drag the `overview_item` back to display #1 w/o and drop. Verify that the
  // new desk buttons on all displays are restored to
  // `DeskIconButton::State::kExpanded` state.
  DragItemToPoint(overview_item, point_in_display1, event_generator,
                  /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button0->state());
  EXPECT_EQ(DeskIconButton::State::kExpanded, new_desk_button1->state());
}

// Verify that when an overview item is moved to a different display, it
// is properly removed from the original grid and displayed in the new one with
// no crash. See original crash reported at http://b/320479135.
TEST_P(OverviewSessionTest,
       NoCrashWhenSettingOverviewItemBoundsOnAnotherDisplay) {
  UpdateDisplay("800x700,801+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  EXPECT_EQ(2U, display_manager->GetNumDisplays());
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  std::unique_ptr<aura::Window> window =
      CreateAppWindow(gfx::Rect(10, 10, 200, 100));
  // Explicitly call `set_allow_set_bounds_direct()` to true to trigger the same
  // stack trace.
  WindowState::Get(window.get())->set_allow_set_bounds_direct(true);
  aura::Window* old_root_window = window->GetRootWindow();

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  const auto& grids = GetOverviewSession()->grid_list();
  ASSERT_EQ(2u, grids.size());
  auto* grid0 = grids[0].get();
  ASSERT_TRUE(grid0);
  const auto& overview_items = grid0->item_list();
  ASSERT_EQ(overview_items.size(), 1u);
  EXPECT_TRUE(IsWindowInItsCorrespondingOverviewGrid(window.get()));

  // Verify that when setting the window bounds to another display, the window
  // will be moved properly.
  window->SetBoundsInScreen(
      gfx::Rect(900, 10, 200, 100),
      display::Screen::GetScreen()->GetDisplayNearestWindow(
          Shell::GetAllRootWindows()[1].get()));
  EXPECT_NE(window->GetRootWindow(), old_root_window);
  EXPECT_TRUE(IsWindowInItsCorrespondingOverviewGrid(window.get()));
}

// Used to replicate the behavior of the Crostini app window, which would set
// the window bounds to its registered display on the window's visibility
// changed. See
// `AppServiceAppWindowCrostiniTracker::OnWindowVisibilityChanged()` for more
// details.
class CrostiniWindowVisibilityObserver : public aura::WindowObserver {
 public:
  explicit CrostiniWindowVisibilityObserver(aura::Window* window)
      : window_(window) {
    window->AddObserver(this);
  }

  ~CrostiniWindowVisibilityObserver() override {
    window_->RemoveObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (visible) {
      auto current_display =
          display::Screen::GetScreen()->GetDisplayNearestWindow(window);
      const auto dst_display =
          display::Screen::GetScreen()->GetPrimaryDisplay();
      window->SetBoundsInScreen(
          gfx::Rect(dst_display.bounds().origin(), window->bounds().size()),
          dst_display);
    }
  }

 private:
  raw_ptr<aura::Window> window_;
};

// Test verifies that dragging a minimized Crostini window to an external
// display in Overview mode and then clicking to activate it doesn't cause a
// crash. The crash would typically occur due to the
// `AppServiceAppWindowCrostiniTracker` attempting to move the window back to
// its registered display, which triggers a `CHECK_EQ(root_window_,
// window->GetRootWindow())` crash in `OverviewItem::SetItemBounds()`. See
// http://b/334911238 for more details.
TEST_P(OverviewSessionTest,
       NoCrashWhenSettingMinimizedOverviewItemBoundsOnAnotherDisplay) {
  UpdateDisplay("1410x940,1411+0-2560x1440");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  EXPECT_EQ(2U, display_manager->GetNumDisplays());
  const auto& displays = display_manager->active_display_list();
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  const gfx::Point point_in_display2(2500, 500);
  ASSERT_FALSE(displays[0].bounds().Contains(point_in_display2));
  ASSERT_TRUE(displays[1].bounds().Contains(point_in_display2));

  std::unique_ptr<aura::Window> window(
      CreateAppWindow(gfx::Rect(10, 10, 500, 300)));

  WMEvent minimize_event(WM_EVENT_MINIMIZE);
  WindowState::Get(window.get())->OnWMEvent(&minimize_event);
  ASSERT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());

  CrostiniWindowVisibilityObserver visibility_observer(window.get());

  ToggleOverview();
  WaitForOverviewEntered();
  ASSERT_TRUE(IsInOverviewSession());

  const auto& grids = GetOverviewSession()->grid_list();
  ASSERT_EQ(2u, grids.size());
  auto grid0 = grids[0].get();
  ASSERT_TRUE(grid0);
  const auto& overview_items = grid0->item_list();
  ASSERT_EQ(overview_items.size(), 1u);
  EXPECT_TRUE(IsWindowInItsCorrespondingOverviewGrid(window.get()));

  auto* event_generator = GetEventGenerator();
  auto* overview_item = overview_items[0].get();
  ASSERT_TRUE(overview_item);
  DragItemToPoint(overview_item, point_in_display2, event_generator,
                  /*by_touch_gestures=*/false, /*drop=*/true);
  EXPECT_TRUE(IsWindowInItsCorrespondingOverviewGrid(window.get()));

  // Verify that the windows are moved to the `displays[1]` properly.
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(displays[1].id(),
            screen->GetDisplayNearestWindow(window.get()).id());

  event_generator->set_current_screen_location(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window.get())->target_bounds().CenterPoint()));

  // Verify that there will be no crash when activating the minimized Crostini
  // window.
  event_generator->ClickLeftButton();
}

TEST_P(OverviewSessionTest, OverviewItemViewAccessibleProperties) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  wm::ActivateWindow(window.get());
  ToggleOverview();
  auto* overview_item_view =
      static_cast<OverviewItem*>(GetOverviewItemForWindow(window.get()))
          ->overview_item_view();
  ui::AXNodeData data;

  ASSERT_TRUE(overview_item_view);
  overview_item_view->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kGenericContainer);
  EXPECT_EQ(overview_item_view->GetViewAccessibility().GetCachedDescription(),
            l10n_util::GetStringUTF16(
                IDS_ASH_OVERVIEW_CLOSABLE_HIGHLIGHT_ITEM_A11Y_EXTRA_TIP));
}

// If you update the parameterisation of OverviewSessionTest also update the
// parameterisation of OverviewRasterScaleTest below.
INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    OverviewSessionTest,
    testing::Combine(testing::Bool(), testing::Bool()),
    OverviewSessionTestParamsToString);

class OverviewRasterScaleTest : public OverviewSessionTest {
 public:
  OverviewRasterScaleTest() = default;
  OverviewRasterScaleTest(const OverviewRasterScaleTest&) = delete;
  OverviewRasterScaleTest& operator=(const OverviewRasterScaleTest&) = delete;
  ~OverviewRasterScaleTest() override = default;

  // OverviewSessionTest:
  void SetUp() override {
    OverviewSessionTest::SetUp();

    Shell::Get()
        ->raster_scale_controller()
        ->set_raster_scale_slop_proportion_for_testing(0.0f);
    Shell::Get()
        ->overview_controller()
        ->set_occlusion_pause_duration_for_start_for_test(
            base::Milliseconds(0));
    Shell::Get()
        ->overview_controller()
        ->set_occlusion_pause_duration_for_end_for_test(base::Milliseconds(0));
  }

  float ExpectedRasterScale(aura::Window* window,
                            gfx::Rect start_bounds,
                            bool window_grows) {
    auto* item = GetOverviewItemForWindow(window);
    CHECK(item);

    // If the window is minimized, the widget size is changed. Otherwise, it's
    // transformed via the transform window. Use the target bounds if it's not
    // minimized. If it's minimized, it won't have its size animated so it's
    // safe to look at the item view size.
    auto end_bounds = window->layer()->GetTargetTransform().MapRect(
        gfx::RectF(window->GetTargetBounds()));

    if (WindowState::Get(window)->IsMinimized()) {
      const auto insets = gfx::Insets::TLBR(
          window->GetProperty(aura::client::kTopViewInset), 0, 0, 0);
      start_bounds.Inset(insets);
      const auto size = item->GetLeafItemForWindow(window)
                            ->overview_item_view()
                            ->GetPreviewViewSize();
      end_bounds = gfx::RectF(gfx::Rect(size));
    }

    auto transform = gfx::TransformBetweenRects(gfx::RectF(start_bounds),
                                                gfx::RectF(end_bounds));
    auto scale_2d = transform.To2dScale();
    auto scale = std::max(scale_2d.x(), scale_2d.y());
    // Specify 1.0's manually, since they are easy to know, and we want to
    // minimize the amount of extra computation for raster scale expectations.
    EXPECT_NE(1.0f, scale);
    if (window_grows) {
      EXPECT_GT(scale, 1.0);
    } else {
      EXPECT_LT(scale, 1.0);
    }
    return scale;
  }

  void MinimizeAndCheckWindow(aura::Window* window) {
    WMEvent minimize_event(WM_EVENT_MINIMIZE);
    WindowState::Get(window)->OnWMEvent(&minimize_event);
    EXPECT_FALSE(window->IsVisible());
    EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());
    ASSERT_TRUE(WindowState::Get(window)->IsMinimized());
  }

  void MaximizeAndCheckWindow(aura::Window* window) {
    WMEvent maximize_event(WM_EVENT_MAXIMIZE);
    WindowState::Get(window)->OnWMEvent(&maximize_event);
    EXPECT_TRUE(window->IsVisible());
    ASSERT_TRUE(WindowState::Get(window)->IsMaximized());
  }
};

// Tests raster scale changes for a single window which grows when entering
// overview mode.
TEST_P(OverviewRasterScaleTest,
       RasterScaleAnimatedSingleWindowEnterGrowExitShrink) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(kInitWindowBoundsToGrow));
  auto tracker = RasterScaleChangeTracker(window.get());

  gfx::Rect start_bounds = GetTransformedTargetBounds(window.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  ToggleOverview();

  // Since the window gets larger, we need to use the more detailed raster
  // before the animation starts.
  auto raster_scale =
      ExpectedRasterScale(window.get(), start_bounds, /*window_grows=*/true);
  EXPECT_EQ(std::vector<float>{raster_scale}, tracker.TakeRasterScaleChanges());
  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  // No change after animation.
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());

  ToggleOverview();

  // Expect no raster scale change as we need to keep the higher detail during
  // the shrink animation.
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  // After completion, restore to normal raster scale.
  EXPECT_EQ(std::vector<float>{1.0}, tracker.TakeRasterScaleChanges());
}

// Tests raster scale changes for a single window which shrinks when entering
// overview mode.
TEST_P(OverviewRasterScaleTest,
       RasterScaleAnimatedSingleWindowEnterShrinkExitGrow) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(kInitWindowBoundsToShrink));
  auto tracker = RasterScaleChangeTracker(window.get());

  gfx::Rect start_bounds = GetTransformedTargetBounds(window.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  ToggleOverview();

  // Since the window gets smaller, we need to keep the more detailed raster
  // until after the animation finishes.
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  auto raster_scale =
      ExpectedRasterScale(window.get(), start_bounds, /*window_grows=*/false);
  EXPECT_EQ(std::vector<float>{raster_scale}, tracker.TakeRasterScaleChanges());

  ToggleOverview();

  // Expect a raster scale change as we need to use the higher detail during
  // the grow animation.
  EXPECT_EQ(std::vector<float>{1.0}, tracker.TakeRasterScaleChanges());
  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
}

// Tests raster scale changes for a minimized single window which grows when
// entering overview mode.
TEST_P(OverviewRasterScaleTest,
       RasterScaleMinimizedSingleWindowEnterGrowExitShrink) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(kInitWindowBoundsToGrow));
  auto tracker = RasterScaleChangeTracker(window.get());

  gfx::Rect start_bounds = GetTransformedTargetBounds(window.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  MinimizeAndCheckWindow(window.get());

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  ToggleOverview();

  // Since the window will be shown larger immediately, change raster scale
  // immediately.
  auto raster_scale =
      ExpectedRasterScale(window.get(), start_bounds, /*window_grows=*/true);
  EXPECT_EQ(std::vector<float>{raster_scale}, tracker.TakeRasterScaleChanges());
  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  // No change after enter.
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());

  ToggleOverview();

  // Expect no raster scale change as we need to keep the higher detail until
  // everything is hidden.
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  // After completion, restore to normal raster scale.
  EXPECT_EQ(std::vector<float>{1.0}, tracker.TakeRasterScaleChanges());
}

// Tests raster scale changes for a minimized single window which shrinks when
// entering overview mode.
TEST_P(OverviewRasterScaleTest,
       RasterScaleMinimizedSingleWindowEnterShrinkExitGrow) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(kInitWindowBoundsToShrink));
  auto tracker = RasterScaleChangeTracker(window.get());

  gfx::Rect start_bounds = GetTransformedTargetBounds(window.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  MinimizeAndCheckWindow(window.get());

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  ToggleOverview();

  // Since the window is minimized, it won't be animated and we can show the
  // less detailed version immediately.
  auto raster_scale =
      ExpectedRasterScale(window.get(), start_bounds, /*window_grows=*/false);
  EXPECT_EQ(std::vector<float>{raster_scale}, tracker.TakeRasterScaleChanges());
  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  // No change after enter.
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());

  ToggleOverview();

  // Expect no raster scale change as that will be more performant to keep lower
  // detail.
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  // After completion, restore to normal raster scale.
  EXPECT_EQ(std::vector<float>{1.0}, tracker.TakeRasterScaleChanges());
}

// Tests raster scale changes for a more complex case with multiple windows in
// different states.
TEST_P(OverviewRasterScaleTest, RasterScaleMultipleWindows) {
  std::unique_ptr<aura::Window> window_grow_animated(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_animated(
      CreateTestWindow(kInitWindowBoundsToShrink));
  std::unique_ptr<aura::Window> window_grow_minimized(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_minimized(
      CreateTestWindow(kInitWindowBoundsToShrink));

  MinimizeAndCheckWindow(window_grow_minimized.get());
  MinimizeAndCheckWindow(window_shrink_minimized.get());

  auto tracker_grow_animated =
      RasterScaleChangeTracker(window_grow_animated.get());
  auto tracker_shrink_animated =
      RasterScaleChangeTracker(window_shrink_animated.get());
  auto tracker_grow_minimized =
      RasterScaleChangeTracker(window_grow_minimized.get());
  auto tracker_shrink_minimized =
      RasterScaleChangeTracker(window_shrink_minimized.get());

  gfx::Rect start_bounds_grow_animated =
      GetTransformedTargetBounds(window_grow_animated.get());
  gfx::Rect start_bounds_shrink_animated =
      GetTransformedTargetBounds(window_shrink_animated.get());
  gfx::Rect start_bounds_grow_minimized =
      GetTransformedTargetBounds(window_grow_minimized.get());
  gfx::Rect start_bounds_shrink_minimized =
      GetTransformedTargetBounds(window_shrink_minimized.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_grow_animated =
      ExpectedRasterScale(window_grow_animated.get(),
                          start_bounds_grow_animated, /*window_grows=*/true);
  float raster_scale_shrink_animated =
      ExpectedRasterScale(window_shrink_animated.get(),
                          start_bounds_shrink_animated, /*window_grows=*/false);
  float raster_scale_grow_minimized =
      ExpectedRasterScale(window_grow_minimized.get(),
                          start_bounds_grow_minimized, /*window_grows=*/true);
  float raster_scale_shrink_minimized = ExpectedRasterScale(
      window_shrink_minimized.get(), start_bounds_shrink_minimized,
      /*window_grows=*/false);

  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  ToggleOverview();

  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_minimized.TakeRasterScaleChanges());
}

// Tests raster scale changes when a maximized window exists with windows on
// top.
TEST_P(OverviewRasterScaleTest, RasterScaleMaximizedWithGrowingRestoredOnTop) {
  std::unique_ptr<aura::Window> window_maximized(
      CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window_grow(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink(
      CreateTestWindow(kInitWindowBoundsToShrink));

  MaximizeAndCheckWindow(window_maximized.get());

  window_maximized->parent()->StackChildAtTop(window_grow.get());
  window_maximized->parent()->StackChildAtTop(window_shrink.get());

  auto tracker_maximized = RasterScaleChangeTracker(window_maximized.get());
  auto tracker_grow = RasterScaleChangeTracker(window_grow.get());
  auto tracker_shrink = RasterScaleChangeTracker(window_shrink.get());

  gfx::Rect start_bounds_maximized =
      GetTransformedTargetBounds(window_maximized.get());
  gfx::Rect start_bounds_grow = GetTransformedTargetBounds(window_grow.get());
  gfx::Rect start_bounds_shrink =
      GetTransformedTargetBounds(window_shrink.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_maximized = ExpectedRasterScale(
      window_maximized.get(), start_bounds_maximized, /*window_grows=*/false);
  float raster_scale_grow = ExpectedRasterScale(
      window_grow.get(), start_bounds_grow, /*window_grows=*/true);
  float raster_scale_shrink = ExpectedRasterScale(
      window_shrink.get(), start_bounds_shrink, /*window_grows=*/false);

  // Maximized needs to keep detail while it shrinks.
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow},
            tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink},
            tracker_shrink.TakeRasterScaleChanges());

  ToggleOverview();

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0}, tracker_shrink.TakeRasterScaleChanges());

  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0}, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink.TakeRasterScaleChanges());
}

// Tests raster scale changes when a maximized window exists with windows below.
TEST_P(OverviewRasterScaleTest, RasterScaleMaximizedWithGrowingRestoredBelow) {
  std::unique_ptr<aura::Window> window_grow(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink(
      CreateTestWindow(kInitWindowBoundsToShrink));
  std::unique_ptr<aura::Window> window_maximized(
      CreateTestWindow(gfx::Rect(100, 100)));

  MaximizeAndCheckWindow(window_maximized.get());

  window_maximized->parent()->StackChildAtBottom(window_grow.get());
  window_maximized->parent()->StackChildAtBottom(window_shrink.get());

  auto tracker_maximized = RasterScaleChangeTracker(window_maximized.get());
  auto tracker_grow = RasterScaleChangeTracker(window_grow.get());
  auto tracker_shrink = RasterScaleChangeTracker(window_shrink.get());

  gfx::Rect start_bounds_maximized =
      GetTransformedTargetBounds(window_maximized.get());
  gfx::Rect start_bounds_grow = GetTransformedTargetBounds(window_grow.get());
  gfx::Rect start_bounds_shrink =
      GetTransformedTargetBounds(window_shrink.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_maximized = ExpectedRasterScale(
      window_maximized.get(), start_bounds_maximized, /*window_grows=*/false);
  float raster_scale_grow = ExpectedRasterScale(
      window_grow.get(), start_bounds_grow, /*window_grows=*/true);
  float raster_scale_shrink = ExpectedRasterScale(
      window_shrink.get(), start_bounds_shrink, /*window_grows=*/false);

  // Both windows are covered, so they can have their final raster scale applied
  // immediately.
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow},
            tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink},
            tracker_shrink.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink.TakeRasterScaleChanges());

  ToggleOverview();

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink.TakeRasterScaleChanges());

  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  // Windows will be covered and not animate, so they wait until the animation
  // has finished to update.
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0}, tracker_grow.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0}, tracker_shrink.TakeRasterScaleChanges());
}

// Tests raster scale changes for a more complex case with multiple windows in
// different states when the overview mode animation is cancelled while entering
// and exiting.
TEST_P(OverviewRasterScaleTest, RasterScaleMultipleWindowsCancel) {
  std::unique_ptr<aura::Window> window_grow_covered(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_covered(
      CreateTestWindow(kInitWindowBoundsToShrink));
  std::unique_ptr<aura::Window> window_maximized(
      CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window_grow_animated(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_animated(
      CreateTestWindow(kInitWindowBoundsToShrink));
  std::unique_ptr<aura::Window> window_grow_minimized(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_minimized(
      CreateTestWindow(kInitWindowBoundsToShrink));

  MinimizeAndCheckWindow(window_grow_minimized.get());
  MinimizeAndCheckWindow(window_shrink_minimized.get());

  MaximizeAndCheckWindow(window_maximized.get());

  window_maximized->parent()->StackChildAtBottom(window_grow_covered.get());
  window_maximized->parent()->StackChildAtBottom(window_shrink_covered.get());
  window_maximized->parent()->StackChildAtTop(window_shrink_animated.get());
  window_maximized->parent()->StackChildAtTop(window_grow_animated.get());

  window_grow_animated->Focus();

  auto tracker_grow_covered =
      RasterScaleChangeTracker(window_grow_covered.get());
  auto tracker_shrink_covered =
      RasterScaleChangeTracker(window_shrink_covered.get());
  auto tracker_maximized = RasterScaleChangeTracker(window_maximized.get());
  auto tracker_grow_animated =
      RasterScaleChangeTracker(window_grow_animated.get());
  auto tracker_shrink_animated =
      RasterScaleChangeTracker(window_shrink_animated.get());
  auto tracker_grow_minimized =
      RasterScaleChangeTracker(window_grow_minimized.get());
  auto tracker_shrink_minimized =
      RasterScaleChangeTracker(window_shrink_minimized.get());

  gfx::Rect start_bounds_grow_covered =
      GetTransformedTargetBounds(window_grow_covered.get());
  gfx::Rect start_bounds_shrink_covered =
      GetTransformedTargetBounds(window_shrink_covered.get());
  gfx::Rect start_bounds_maximized =
      GetTransformedTargetBounds(window_maximized.get());
  gfx::Rect start_bounds_grow_animated =
      GetTransformedTargetBounds(window_grow_animated.get());
  gfx::Rect start_bounds_shrink_animated =
      GetTransformedTargetBounds(window_shrink_animated.get());
  gfx::Rect start_bounds_grow_minimized =
      GetTransformedTargetBounds(window_grow_minimized.get());
  gfx::Rect start_bounds_shrink_minimized =
      GetTransformedTargetBounds(window_shrink_minimized.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_grow_covered =
      ExpectedRasterScale(window_grow_covered.get(), start_bounds_grow_covered,
                          /*window_grows=*/true);
  float raster_scale_shrink_covered = ExpectedRasterScale(
      window_shrink_covered.get(), start_bounds_shrink_covered,
      /*window_grows=*/false);
  float raster_scale_maximized =
      ExpectedRasterScale(window_maximized.get(), start_bounds_maximized,
                          /*window_grows=*/false);
  float raster_scale_grow_animated =
      ExpectedRasterScale(window_grow_animated.get(),
                          start_bounds_grow_animated, /*window_grows=*/true);
  float raster_scale_shrink_animated = ExpectedRasterScale(
      window_shrink_animated.get(), start_bounds_shrink_animated,
      /*window_grows=*/false);
  float raster_scale_grow_minimized =
      ExpectedRasterScale(window_grow_minimized.get(),
                          start_bounds_grow_minimized, /*window_grows=*/true);
  float raster_scale_shrink_minimized = ExpectedRasterScale(
      window_shrink_minimized.get(), start_bounds_shrink_minimized,
      /*window_grows=*/false);

  EXPECT_EQ(std::vector<float>{raster_scale_grow_covered},
            tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_covered},
            tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  // Cancel overview mode by focusing another window.
  EXPECT_TRUE(InOverviewSession());
  window_shrink_animated->Focus();
  EXPECT_FALSE(InOverviewSession());

  // Animation will start to reverse, no reason to change the raster scale here.
  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  // Enter overview mode so we can test cancelling exit.
  ToggleOverview();

  EXPECT_EQ(std::vector<float>{raster_scale_grow_covered},
            tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_covered},
            tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  // In overview mode. Start exiting and then cancel.
  ToggleOverview();

  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  // Cancel leaving overview mode.
  EXPECT_FALSE(InOverviewSession());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  // Cancelling shouldn't change any raster scales.
  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  // Cancel entering overview mode.
  EXPECT_TRUE(InOverviewSession());
  ToggleOverview();
  EXPECT_FALSE(InOverviewSession());

  // Cancelling shouldn't change any raster scales.
  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  // Cancel leaving overview mode.
  EXPECT_FALSE(InOverviewSession());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  // Cancelling shouldn't change any raster scales.
  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  // Finally fully enter overview mode.
  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());
}

// Tests raster scale changes for transient windows.
TEST_P(OverviewRasterScaleTest, RasterScaleTransientChildWindows) {
  std::unique_ptr<aura::Window> window_grow_covered(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_covered(
      CreateTestWindow(kInitWindowBoundsToShrink));
  std::unique_ptr<aura::Window> window_maximized(
      CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window_grow_animated(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_animated(
      CreateTestWindow(kInitWindowBoundsToShrink));
  std::unique_ptr<aura::Window> window_grow_minimized(
      CreateTestWindow(kInitWindowBoundsToGrow));
  std::unique_ptr<aura::Window> window_shrink_minimized(
      CreateTestWindow(kInitWindowBoundsToShrink));

  std::unique_ptr<aura::Window> window_grow_covered_transient(
      CreateTestWindow(gfx::Rect(25, 25)));
  std::unique_ptr<aura::Window> window_shrink_covered_transient(
      CreateTestWindow(gfx::Rect(25, 25)));
  std::unique_ptr<aura::Window> window_maximized_transient(
      CreateTestWindow(gfx::Rect(25, 25)));
  std::unique_ptr<aura::Window> window_grow_animated_transient(
      CreateTestWindow(gfx::Rect(25, 25)));
  std::unique_ptr<aura::Window> window_shrink_animated_transient(
      CreateTestWindow(gfx::Rect(25, 25)));
  std::unique_ptr<aura::Window> window_grow_minimized_transient(
      CreateTestWindow(gfx::Rect(25, 25)));
  std::unique_ptr<aura::Window> window_shrink_minimized_transient(
      CreateTestWindow(gfx::Rect(25, 25)));

  wm::AddTransientChild(window_grow_covered.get(),
                        window_grow_covered_transient.get());
  wm::AddTransientChild(window_shrink_covered.get(),
                        window_shrink_covered_transient.get());
  wm::AddTransientChild(window_maximized.get(),
                        window_maximized_transient.get());
  wm::AddTransientChild(window_grow_animated.get(),
                        window_grow_animated_transient.get());
  wm::AddTransientChild(window_shrink_animated.get(),
                        window_shrink_animated_transient.get());
  wm::AddTransientChild(window_grow_minimized.get(),
                        window_grow_minimized_transient.get());
  wm::AddTransientChild(window_shrink_minimized.get(),
                        window_shrink_minimized_transient.get());

  MinimizeAndCheckWindow(window_grow_minimized.get());
  MinimizeAndCheckWindow(window_shrink_minimized.get());
  MaximizeAndCheckWindow(window_maximized.get());

  window_maximized->parent()->StackChildAtBottom(window_grow_covered.get());
  window_maximized->parent()->StackChildAtBottom(window_shrink_covered.get());
  window_maximized->parent()->StackChildAtTop(window_shrink_animated.get());
  window_maximized->parent()->StackChildAtTop(window_grow_animated.get());

  auto tracker_grow_covered =
      RasterScaleChangeTracker(window_grow_covered.get());
  auto tracker_shrink_covered =
      RasterScaleChangeTracker(window_shrink_covered.get());
  auto tracker_maximized = RasterScaleChangeTracker(window_maximized.get());
  auto tracker_grow_animated =
      RasterScaleChangeTracker(window_grow_animated.get());
  auto tracker_shrink_animated =
      RasterScaleChangeTracker(window_shrink_animated.get());
  auto tracker_grow_minimized =
      RasterScaleChangeTracker(window_grow_minimized.get());
  auto tracker_shrink_minimized =
      RasterScaleChangeTracker(window_shrink_minimized.get());

  auto tracker_grow_covered_transient =
      RasterScaleChangeTracker(window_grow_covered_transient.get());
  auto tracker_shrink_covered_transient =
      RasterScaleChangeTracker(window_shrink_covered_transient.get());
  auto tracker_maximized_transient =
      RasterScaleChangeTracker(window_maximized_transient.get());
  auto tracker_grow_animated_transient =
      RasterScaleChangeTracker(window_grow_animated_transient.get());
  auto tracker_shrink_animated_transient =
      RasterScaleChangeTracker(window_shrink_animated_transient.get());
  auto tracker_grow_minimized_transient =
      RasterScaleChangeTracker(window_grow_minimized_transient.get());
  auto tracker_shrink_minimized_transient =
      RasterScaleChangeTracker(window_shrink_minimized_transient.get());

  gfx::Rect start_bounds_grow_covered =
      GetTransformedTargetBounds(window_grow_covered.get());
  gfx::Rect start_bounds_shrink_covered =
      GetTransformedTargetBounds(window_shrink_covered.get());
  gfx::Rect start_bounds_maximized =
      GetTransformedTargetBounds(window_maximized.get());
  gfx::Rect start_bounds_grow_animated =
      GetTransformedTargetBounds(window_grow_animated.get());
  gfx::Rect start_bounds_shrink_animated =
      GetTransformedTargetBounds(window_shrink_animated.get());
  gfx::Rect start_bounds_grow_minimized =
      GetTransformedTargetBounds(window_grow_minimized.get());
  gfx::Rect start_bounds_shrink_minimized =
      GetTransformedTargetBounds(window_shrink_minimized.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(empty, tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_grow_covered =
      ExpectedRasterScale(window_grow_covered.get(), start_bounds_grow_covered,
                          /*window_grows=*/true);
  float raster_scale_shrink_covered = ExpectedRasterScale(
      window_shrink_covered.get(), start_bounds_shrink_covered,
      /*window_grows=*/false);
  float raster_scale_maximized =
      ExpectedRasterScale(window_maximized.get(), start_bounds_maximized,
                          /*window_grows=*/false);
  float raster_scale_grow_animated =
      ExpectedRasterScale(window_grow_animated.get(),
                          start_bounds_grow_animated, /*window_grows=*/true);
  float raster_scale_shrink_animated = ExpectedRasterScale(
      window_shrink_animated.get(), start_bounds_shrink_animated,
      /*window_grows=*/false);
  float raster_scale_grow_minimized =
      ExpectedRasterScale(window_grow_minimized.get(),
                          start_bounds_grow_minimized, /*window_grows=*/true);
  float raster_scale_shrink_minimized = ExpectedRasterScale(
      window_shrink_minimized.get(), start_bounds_shrink_minimized,
      /*window_grows=*/false);

  EXPECT_EQ(std::vector<float>{raster_scale_grow_covered},
            tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_covered},
            tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(std::vector<float>{raster_scale_grow_covered},
            tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_covered},
            tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(empty, tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  // Exit overview mode.
  ToggleOverview();

  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(empty, tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  // Re-enter overview mode to test adding/removing transient child windows.
  ToggleOverview();

  EXPECT_EQ(std::vector<float>{raster_scale_grow_covered},
            tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_covered},
            tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(std::vector<float>{raster_scale_grow_covered},
            tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_covered},
            tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(empty, tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  wm::RemoveTransientChild(window_grow_covered.get(),
                           window_grow_covered_transient.get());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_covered_transient.TakeRasterScaleChanges());

  wm::RemoveTransientChild(window_shrink_covered.get(),
                           window_shrink_covered_transient.get());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_covered_transient.TakeRasterScaleChanges());

  wm::RemoveTransientChild(window_maximized.get(),
                           window_maximized_transient.get());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized_transient.TakeRasterScaleChanges());

  wm::RemoveTransientChild(window_grow_animated.get(),
                           window_grow_animated_transient.get());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_animated_transient.TakeRasterScaleChanges());

  wm::RemoveTransientChild(window_shrink_animated.get(),
                           window_shrink_animated_transient.get());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_animated_transient.TakeRasterScaleChanges());

  wm::RemoveTransientChild(window_grow_minimized.get(),
                           window_grow_minimized_transient.get());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_minimized_transient.TakeRasterScaleChanges());

  wm::RemoveTransientChild(window_shrink_minimized.get(),
                           window_shrink_minimized_transient.get());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  // Add back the transient child windows and expect the raster scales to be
  // set.

  wm::AddTransientChild(window_grow_covered.get(),
                        window_grow_covered_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_covered},
            tracker_grow_covered_transient.TakeRasterScaleChanges());

  wm::AddTransientChild(window_shrink_covered.get(),
                        window_shrink_covered_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_covered},
            tracker_shrink_covered_transient.TakeRasterScaleChanges());

  wm::AddTransientChild(window_maximized.get(),
                        window_maximized_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized_transient.TakeRasterScaleChanges());

  wm::AddTransientChild(window_grow_animated.get(),
                        window_grow_animated_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_animated},
            tracker_grow_animated_transient.TakeRasterScaleChanges());

  wm::AddTransientChild(window_shrink_animated.get(),
                        window_shrink_animated_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_animated},
            tracker_shrink_animated_transient.TakeRasterScaleChanges());

  wm::AddTransientChild(window_grow_minimized.get(),
                        window_grow_minimized_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale_grow_minimized},
            tracker_grow_minimized_transient.TakeRasterScaleChanges());

  wm::AddTransientChild(window_shrink_minimized.get(),
                        window_shrink_minimized_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale_shrink_minimized},
            tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  // Test adding/removing transient windows during overview animation.

  // Exit overview mode to test adding/removing transient child windows.
  ToggleOverview();

  EXPECT_EQ(empty, tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(empty, tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_minimized_transient.TakeRasterScaleChanges());

  // Expect that transient children added during the overview mode animation
  // have their raster scale set at the end of the animation.
  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_covered.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_covered.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_animated.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_minimized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_minimized.TakeRasterScaleChanges());

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_covered_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_maximized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_shrink_animated_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_grow_minimized_transient.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_shrink_minimized_transient.TakeRasterScaleChanges());
}

// Tests that adding a window as a transient window to another window will
// update its raster scale.
TEST_P(OverviewRasterScaleTest,
       RasterScaleAddRemoveTransientChildWindowsDuringOverviewMode) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(100, 100)));

  std::unique_ptr<aura::Window> window_transient(
      CreateTestWindow(kInitWindowBoundsToShrink));

  auto tracker = RasterScaleChangeTracker(window.get());
  auto tracker_transient = RasterScaleChangeTracker(window_transient.get());

  gfx::Rect start_bounds = GetTransformedTargetBounds(window.get());
  gfx::Rect start_bounds_transient =
      GetTransformedTargetBounds(window_transient.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_transient.TakeRasterScaleChanges());

  ToggleOverview();

  auto raster_scale =
      ExpectedRasterScale(window.get(), start_bounds, /*window_grows=*/true);
  float raster_scale_transient =
      ExpectedRasterScale(window_transient.get(), start_bounds_transient,
                          /*window_grows=*/false);

  EXPECT_EQ(std::vector<float>{raster_scale}, tracker.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_transient.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_transient},
            tracker_transient.TakeRasterScaleChanges());

  // Add transient windows and expect the raster scales to be updated to the
  // larger value.
  wm::AddTransientChild(window.get(), window_transient.get());
  EXPECT_EQ(std::vector<float>{raster_scale},
            tracker_transient.TakeRasterScaleChanges());
}

// Tests that adding windows to overview mode will update existing raster
// scales.
TEST_P(OverviewRasterScaleTest,
       RasterScaleAddWindowsDuringOverviewModeByCombiningAVirtualDesk) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // First ensure there are 3 desks.
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ASSERT_EQ(3u, controller->desks().size());

  Desk* desk1 = controller->desks()[0].get();
  Desk* desk2 = controller->desks()[1].get();
  Desk* desk3 = controller->desks()[2].get();

  // Create three windows, one on each desk. Need to use `CreateAppWindow` to
  // work with desks.
  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(100, 100)));
  auto tracker1 = RasterScaleChangeTracker(window1.get());
  gfx::Rect start_bounds = GetTransformedTargetBounds(window1.get());
  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  controller->SendToDeskAtIndex(window1.get(), 0);
  EXPECT_TRUE(base::Contains(desk1->windows(), window1.get()));

  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(100, 100)));
  auto tracker2 = RasterScaleChangeTracker(window2.get());
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());
  controller->SendToDeskAtIndex(window2.get(), 1);
  EXPECT_TRUE(base::Contains(desk2->windows(), window2.get()));

  std::unique_ptr<aura::Window> window3(CreateAppWindow(gfx::Rect(100, 100)));
  auto tracker3 = RasterScaleChangeTracker(window3.get());
  EXPECT_EQ(empty, tracker3.TakeRasterScaleChanges());
  controller->SendToDeskAtIndex(window3.get(), 2);
  EXPECT_TRUE(base::Contains(desk3->windows(), window3.get()));

  EXPECT_TRUE(desk1->is_active());

  // Enter overview mode
  ToggleOverview();

  auto raster_scale =
      ExpectedRasterScale(window1.get(), start_bounds, /*window_grows=*/true);
  EXPECT_EQ(std::vector<float>{raster_scale},
            tracker1.TakeRasterScaleChanges());

  // `window2` and `window3` are not visible on the overview, so expect no
  // raster scale changes.
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker3.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker3.TakeRasterScaleChanges());

  // Combine `desk3` and expect the raster scale for window3 to be updated since
  // it is moved to the active desk1.
  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  EXPECT_EQ(1u, overview_grid->item_list().size());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);
  ASSERT_EQ(3u, desks_bar_view->mini_views().size());
  auto* mini_view = desks_bar_view->mini_views()[2].get();
  EXPECT_EQ(desk3, mini_view->desk());
  if (features::IsSavedDeskUiRevampEnabled()) {
    views::MenuItemView* combine_item_view =
        DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
            Shell::GetPrimaryRootWindow(), DeskBarViewBase::Type::kOverview,
            /*index=*/2, DeskActionContextMenu::CommandId::kCombineDesks);
    LeftClickOn(combine_item_view);
  } else {
    CombineDesksViaMiniView(mini_view, GetEventGenerator());
  }

  EXPECT_TRUE(desk1->is_active());
  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale},
            tracker3.TakeRasterScaleChanges());

  // Now combine the active desk (`desk1`), and expect only `window2` to be
  // updated.
  EXPECT_EQ(2u, overview_grid->item_list().size());
  mini_view = desks_bar_view->mini_views()[0];
  EXPECT_EQ(desk1, mini_view->desk());
  if (features::IsSavedDeskUiRevampEnabled()) {
    views::MenuItemView* combine_item_view =
        DesksTestApi::OpenDeskContextMenuAndGetMenuItem(
            Shell::GetPrimaryRootWindow(), DeskBarViewBase::Type::kOverview,
            /*index=*/0, DeskActionContextMenu::CommandId::kCombineDesks);
    LeftClickOn(combine_item_view);
  } else {
    CombineDesksViaMiniView(mini_view, GetEventGenerator());
  }

  EXPECT_TRUE(desk2->is_active());
  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale},
            tracker2.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker3.TakeRasterScaleChanges());
}

// Tests that moving windows from overview mode to a different virtual desk
// works.
TEST_P(OverviewRasterScaleTest,
       RasterScaleMoveWindowToVirtualDeskDuringOverviewMode) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // First ensure there are 3 desks.
  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  EXPECT_EQ(2u, controller->desks().size());

  Desk* desk1 = controller->desks()[0].get();
  Desk* desk2 = controller->desks()[1].get();

  // Create two windows on the first desk.
  std::unique_ptr<aura::Window> window1(CreateAppWindow(gfx::Rect(100, 100)));
  auto tracker1 = RasterScaleChangeTracker(window1.get());
  gfx::Rect start_bounds = GetTransformedTargetBounds(window1.get());
  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  controller->SendToDeskAtIndex(window1.get(), 0);
  EXPECT_TRUE(base::Contains(desk1->windows(), window1.get()));

  std::unique_ptr<aura::Window> window2(CreateAppWindow(gfx::Rect(100, 100)));
  auto tracker2 = RasterScaleChangeTracker(window2.get());
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());
  controller->SendToDeskAtIndex(window2.get(), 0);
  EXPECT_TRUE(base::Contains(desk1->windows(), window2.get()));

  EXPECT_TRUE(desk1->is_active());

  // Enter overview mode
  ToggleOverview();

  auto raster_scale =
      ExpectedRasterScale(window1.get(), start_bounds, /*window_grows=*/true);
  EXPECT_EQ(std::vector<float>{raster_scale},
            tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale},
            tracker2.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());

  // Move `window2` to `desk2` and expect it to go back to 1.0 raster scale.
  EXPECT_TRUE(controller->MoveWindowFromActiveDeskTo(
      window2.get(), desk2, window2->GetRootWindow(),
      DesksMoveWindowFromActiveDeskSource::kDragAndDrop));

  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0f}, tracker2.TakeRasterScaleChanges());

  // Exit overview mode
  ToggleOverview();
  EXPECT_EQ(empty, tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());

  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{1.0f}, tracker1.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker2.TakeRasterScaleChanges());
}

// Tests raster scale changes work in tablet mode.
// TODO(crbug.com/40949385): Fix flaky test.
TEST_P(OverviewRasterScaleTest, DISABLED_RasterScaleTabletMode) {
  EnterTabletMode();

  std::unique_ptr<aura::Window> window_maximized(
      CreateTestWindow(gfx::Rect(600, 600)));
  std::unique_ptr<aura::Window> window_minimized(
      CreateTestWindow(gfx::Rect(600, 600)));

  MinimizeAndCheckWindow(window_minimized.get());
  MaximizeAndCheckWindow(window_maximized.get());

  auto tracker_maximized = RasterScaleChangeTracker(window_maximized.get());
  auto tracker_minimized = RasterScaleChangeTracker(window_minimized.get());

  gfx::Rect start_bounds_maximized =
      GetTransformedTargetBounds(window_maximized.get());
  gfx::Rect start_bounds_minimized =
      GetTransformedTargetBounds(window_minimized.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_maximized =
      ExpectedRasterScale(window_maximized.get(), start_bounds_maximized,
                          /*window_grows=*/false);
  float raster_scale_minimized =
      ExpectedRasterScale(window_minimized.get(), start_bounds_minimized,
                          /*window_grows=*/false);

  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_minimized},
            tracker_minimized.TakeRasterScaleChanges());

  // Cancel entering overview mode.
  ToggleOverview();

  // Animation will start to reverse, no reason to change the raster scale here.
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{1.0},
            tracker_minimized.TakeRasterScaleChanges());

  // Enter overview mode so we can test cancelling exit.
  ToggleOverview();

  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_minimized},
            tracker_minimized.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  // In overview mode. Start exiting and then cancel.
  ToggleOverview();

  EXPECT_EQ(std::vector<float>{1.0},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  // Cancel leaving overview mode.
  EXPECT_FALSE(InOverviewSession());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  // Cancelling shouldn't change any raster scales.
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  // Cancel entering overview mode.
  EXPECT_TRUE(InOverviewSession());
  ToggleOverview();
  EXPECT_FALSE(InOverviewSession());

  // Cancelling shouldn't change any raster scales.
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  // Cancel leaving overview mode.
  EXPECT_FALSE(InOverviewSession());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  // Cancelling shouldn't change any raster scales.
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  // Finally fully enter overview mode.
  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());
}

// Tests raster scale changes work during screen rotations.
TEST_P(OverviewRasterScaleTest, RasterScaleScreenRotation) {
  UpdateDisplay("1600x1200");
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  std::unique_ptr<aura::Window> window_maximized(
      CreateTestWindow(gfx::Rect(600, 600)));
  std::unique_ptr<aura::Window> window_minimized(
      CreateTestWindow(gfx::Rect(600, 600)));

  MinimizeAndCheckWindow(window_minimized.get());
  MaximizeAndCheckWindow(window_maximized.get());

  auto tracker_maximized = RasterScaleChangeTracker(window_maximized.get());
  auto tracker_minimized = RasterScaleChangeTracker(window_minimized.get());

  gfx::Rect start_bounds_maximized =
      GetTransformedTargetBounds(window_maximized.get());
  gfx::Rect start_bounds_minimized =
      GetTransformedTargetBounds(window_minimized.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_maximized =
      ExpectedRasterScale(window_maximized.get(), start_bounds_maximized,
                          /*window_grows=*/false);
  float raster_scale_minimized =
      ExpectedRasterScale(window_minimized.get(), start_bounds_minimized,
                          /*window_grows=*/false);

  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_minimized},
            tracker_minimized.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  // Rotate the screen 180 degrees and expect no raster scale changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());
}

// Tests raster scale changes work during screen rotations in tablet mode.
TEST_P(OverviewRasterScaleTest, RasterScaleScreenRotationTabletMode) {
  UpdateDisplay("1600x1200");
  EnterTabletMode();

  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  std::unique_ptr<aura::Window> window_maximized(
      CreateTestWindow(gfx::Rect(600, 600)));
  std::unique_ptr<aura::Window> window_minimized(
      CreateTestWindow(gfx::Rect(600, 600)));

  MinimizeAndCheckWindow(window_minimized.get());
  MaximizeAndCheckWindow(window_maximized.get());

  auto tracker_maximized = RasterScaleChangeTracker(window_maximized.get());
  auto tracker_minimized = RasterScaleChangeTracker(window_minimized.get());

  gfx::Rect start_bounds_maximized =
      GetTransformedTargetBounds(window_maximized.get());
  gfx::Rect start_bounds_minimized =
      GetTransformedTargetBounds(window_minimized.get());
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const std::vector<float> empty;
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  ToggleOverview();

  float raster_scale_maximized =
      ExpectedRasterScale(window_maximized.get(), start_bounds_maximized,
                          /*window_grows=*/false);
  float raster_scale_minimized =
      ExpectedRasterScale(window_minimized.get(), start_bounds_minimized,
                          /*window_grows=*/false);

  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(std::vector<float>{raster_scale_minimized},
            tracker_minimized.TakeRasterScaleChanges());

  WaitForOverviewEnterAnimation();
  // Wait for the occlusion tracker to be unpaused after overview enter.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(std::vector<float>{raster_scale_maximized},
            tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  // Rotate the screen 180 degrees and expect no raster scale changes.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());

  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(empty, tracker_maximized.TakeRasterScaleChanges());
  EXPECT_EQ(empty, tracker_minimized.TakeRasterScaleChanges());
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         OverviewRasterScaleTest,
                         testing::Combine(testing::Bool(), testing::Bool()),
                         OverviewSessionTestParamsToString);

class FloatOverviewSessionTest : public OverviewTestBase {
 public:
  FloatOverviewSessionTest() = default;
  FloatOverviewSessionTest(const FloatOverviewSessionTest&) = delete;
  FloatOverviewSessionTest& operator=(const FloatOverviewSessionTest&) = delete;
  ~FloatOverviewSessionTest() override = default;

  // Checks if the float container is in its regular position. Returns false if
  // it is not true on any of the root windows.
  bool IsFloatContainerNormalStacked() const {
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      if (features::IsForestFeatureEnabled()) {
        // The float container should be the top-most child of the
        // `ShutdownScreenshotContainer` when the feature `ForestFeature` is
        // enabled.
        auto* shutdown_screenshot_container =
            root->GetChildById(kShellWindowId_ShutdownScreenshotContainer);
        EXPECT_EQ(root->GetChildById(kShellWindowId_FloatContainer),
                  shutdown_screenshot_container->children().back());
      } else {
        // The float container should above the always on top container and
        // below the app list container when the `ForestFeature` is not enabled.
        if (!window_util::IsStackedBelow(
                root->GetChildById(kShellWindowId_AlwaysOnTopContainer),
                root->GetChildById(kShellWindowId_FloatContainer))) {
          return false;
        }
        if (!window_util::IsStackedBelow(
                root->GetChildById(kShellWindowId_FloatContainer),
                root->GetChildById(kShellWindowId_AppListContainer))) {
          return false;
        }
      }
    }

    return true;
  }

  bool IsFloatContainerBelowActiveDesk() const {
    for (aura::Window* root : Shell::GetAllRootWindows()) {
      if (!window_util::IsStackedBelow(
              root->GetChildById(kShellWindowId_FloatContainer),
              root->GetChildById(kShellWindowId_DeskContainerA))) {
        return false;
      }
    }

    return true;
  }
};

// Tests that the float container is stacked properly when entering and exiting
// overview mode.
TEST_F(FloatOverviewSessionTest, FloatContainerStacking) {
  UpdateDisplay("800x600,800x600");

  // We need at least one window for an overview enter animation.
  auto window = CreateAppWindow();

  ui::ScopedAnimationDurationScaleMode duration_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  EXPECT_TRUE(IsFloatContainerNormalStacked());

  // Enter overview. The float container remains above the active desk until
  // after the overview enter animation is over.
  ToggleOverview();
  EXPECT_FALSE(IsFloatContainerBelowActiveDesk());
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(IsFloatContainerBelowActiveDesk());

  // Exit overview. The float container is stacked in its normal position prior
  // to the exit animation starting.
  ToggleOverview();
  EXPECT_FALSE(IsFloatContainerBelowActiveDesk());
  WaitForOverviewExitAnimation();
  // Wait for the occlusion tracker to be unpaused after overview exit.
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(IsFloatContainerNormalStacked());

  // Start overview but exit before the animation is complete. Verify the float
  // container is stacked in its normal position.
  ToggleOverview();
  ToggleOverview();
  EXPECT_TRUE(IsFloatContainerNormalStacked());
}

// Tests that when we drag in overview, and there is a floated window, the
// float container gets restacked so it will appear under the dragged window.
// See b/252504134 for more details.
TEST_F(FloatOverviewSessionTest, DraggingWithFloatedWindow) {
  UpdateDisplay("800x600,800x600");

  // Create one normal and one floated window.
  auto normal_window = CreateAppWindow();
  auto floated_window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  ToggleOverview();
  ASSERT_TRUE(IsFloatContainerBelowActiveDesk());

  auto* normal_item = GetOverviewItemForWindow(normal_window.get());
  auto* floated_item = GetOverviewItemForWindow(floated_window.get());

  // Start dragging the floated window. Check that the float container gets
  // stacked above the desk container after dragging starts.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(floated_item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  generator->MoveMouseBy(10, 10);
  EXPECT_TRUE(IsFloatContainerNormalStacked());

  generator->ReleaseLeftButton();

  // Dragging the normal window does not cause restacking as it is already on
  // top of other windows like it should be.
  ASSERT_TRUE(IsFloatContainerBelowActiveDesk());
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(normal_item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  generator->MoveMouseBy(10, 10);
  EXPECT_TRUE(IsFloatContainerBelowActiveDesk());

  generator->ReleaseLeftButton();
  ASSERT_TRUE(InOverviewSession());
  EXPECT_TRUE(IsFloatContainerBelowActiveDesk());

  // Tests that the stacking order is correct if we start dragging a normal
  // overview item, and then exit overview.
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(normal_item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  ToggleOverview();
  // `OverviewWindowDragController` gets deleted using `DeleteSoon()`.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(IsFloatContainerNormalStacked());
}

// Tests that clicking the normal window to activate it does not result in a
// crash. Regression test for b/258818000.
TEST_F(FloatOverviewSessionTest, ClickingWithFloatedWindow) {
  // Create one normal and one floated window.
  auto normal_window = CreateAppWindow();
  auto floated_window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  ToggleOverview();
  auto* normal_item = GetOverviewItemForWindow(normal_window.get());
  GetEventGenerator()->set_current_screen_location(
      gfx::ToRoundedPoint(normal_item->target_bounds().CenterPoint()));
  GetEventGenerator()->ClickLeftButton();
}

// Tests that dragging a normal window while there is a floated window to a new
// desk does not result in a crash. Regression test for http://b/261757970.
TEST_F(FloatOverviewSessionTest, DraggingToNewDeskWithFloatedWindow) {
  // Create one normal and one floated window.
  auto normal_window = CreateAppWindow();
  auto floated_window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  // Enter overview and start dragging on the normal window.
  ToggleOverview();
  auto* normal_item = GetOverviewItemForWindow(normal_window.get());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(normal_item->target_bounds().CenterPoint()));
  generator->PressLeftButton();

  // Drag the normal window to the new desk button; this will create a new desk
  // and drop the normal window in it.
  OverviewGrid* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  const auto* desks_bar_view = overview_grid->desks_bar_view();
  ASSERT_TRUE(desks_bar_view);

  const DeskIconButton* new_desk_button = desks_bar_view->new_desk_button();
  ASSERT_TRUE(new_desk_button);
  ASSERT_TRUE(new_desk_button->GetVisible());
  generator->DragMouseTo(new_desk_button->GetBoundsInScreen().CenterPoint());

  // Check that a new desk has been created, and there should be no crash when
  // dropping the window.
  generator->ReleaseLeftButton();
  auto* controller = DesksController::Get();
  EXPECT_EQ(2u, controller->desks().size());
  EXPECT_TRUE(base::Contains(controller->GetDeskAtIndex(1)->windows(),
                             normal_window.get()));
}

// Tests that the overview item associated with the floated window appears
// underneath the about to be dragged window after long pressing.
TEST_F(FloatOverviewSessionTest, LongPressingWithFloatedWindow) {
  // Shorten the long press times so we don't have to delay as long.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_short_press_time(base::Milliseconds(1));
  gesture_config->set_show_press_delay_in_ms(1);

  // Create one normal and one floated window.
  auto normal_window = CreateAppWindow();
  auto floated_window = CreateAppWindow();
  PressAndReleaseKey(ui::VKEY_F, ui::EF_ALT_DOWN | ui::EF_COMMAND_DOWN);
  ASSERT_TRUE(WindowState::Get(floated_window.get())->IsFloated());

  ToggleOverview();
  ASSERT_TRUE(IsFloatContainerBelowActiveDesk());

  // Simulate a long press on the overview item of the floated window.
  auto* float_item = GetOverviewItemForWindow(floated_window.get());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(float_item->target_bounds().CenterPoint()));
  generator->PressTouch();
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
  run_loop.Run();

  // After long pressing, the float container should be stacked above the desk
  // container so that the overview item of the float window appears above
  // during the drag.
  EXPECT_TRUE(IsFloatContainerNormalStacked());

  // Test that on release, the float container is stacked below the desk
  // container again.
  generator->ReleaseTouch();
  EXPECT_TRUE(IsFloatContainerBelowActiveDesk());
}

class TabletModeOverviewSessionTest : public OverviewTestBase {
 public:
  TabletModeOverviewSessionTest() = default;
  TabletModeOverviewSessionTest(const TabletModeOverviewSessionTest&) = delete;
  TabletModeOverviewSessionTest& operator=(
      const TabletModeOverviewSessionTest&) = delete;
  ~TabletModeOverviewSessionTest() override = default;

  // OverviewTestBase:
  void SetUp() override {
    OverviewTestBase::SetUp();
    EnterTabletMode();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 protected:
  void GenerateScrollSequence(const gfx::Point& start, const gfx::Point& end) {
    GetEventGenerator()->GestureScrollSequence(start, end,
                                               base::Milliseconds(100), 1000);
  }

  void DispatchLongPress(OverviewItemBase* item) {
    const gfx::Point point =
        gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
    ui::GestureEvent long_press(
        point.x(), point.y(), 0, base::TimeTicks::Now(),
        ui::GestureEventDetails(ui::EventType::kGestureLongPress));
    GetEventGenerator()->Dispatch(&long_press);
  }

  // Creates `n` test windows. They are created in reverse order, so that the
  // first window in the vector is the MRU window.
  std::vector<std::unique_ptr<aura::Window>> CreateAppWindows(int n) {
    std::vector<std::unique_ptr<aura::Window>> windows(n);
    for (int i = n - 1; i >= 0; --i) {
      windows[i] = CreateTestWindow();
    }
    return windows;
  }
};

// Tests that windows are in proper positions in the new overview layout.
TEST_F(TabletModeOverviewSessionTest, CheckNewLayoutWindowPositions) {
  auto windows = CreateAppWindows(6);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* item1 = GetOverviewItemForWindow(windows[0].get());
  auto* item2 = GetOverviewItemForWindow(windows[1].get());
  auto* item3 = GetOverviewItemForWindow(windows[2].get());
  auto* item4 = GetOverviewItemForWindow(windows[3].get());

  const gfx::RectF item1_bounds = item1->target_bounds();
  const gfx::RectF item2_bounds = item2->target_bounds();
  const gfx::RectF item3_bounds = item3->target_bounds();
  const gfx::RectF item4_bounds = item4->target_bounds();

  // |window1| should be in the top left position. |window2| should be directly
  // below |window1|, thus sharing the same x-value but not the same y-value.
  EXPECT_EQ(item1_bounds.x(), item2_bounds.x());
  EXPECT_LT(item1_bounds.y(), item2_bounds.y());
  // |window3| should be directly right of |window1|, thus sharing the same
  // y-value, but not the same x-value.
  EXPECT_LT(item1_bounds.x(), item3_bounds.x());
  EXPECT_EQ(item1_bounds.y(), item3_bounds.y());
  // |window4| should be directly right of |window2| and directly below
  // |window3|.
  EXPECT_LT(item2_bounds.x(), item4_bounds.x());
  EXPECT_EQ(item2_bounds.y(), item4_bounds.y());
  EXPECT_EQ(item3_bounds.x(), item4_bounds.x());
  EXPECT_LT(item3_bounds.y(), item4_bounds.y());
}

// Tests that with the tablet mode layout, some of the windows are offscreen.
TEST_F(TabletModeOverviewSessionTest, CheckOffscreenWindows) {
  auto windows = CreateAppWindows(10);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* item0 = GetOverviewItemForWindow(windows[0].get());
  auto* item1 = GetOverviewItemForWindow(windows[1].get());
  auto* item8 = GetOverviewItemForWindow(windows[8].get());
  auto* item9 = GetOverviewItemForWindow(windows[9].get());

  const gfx::RectF screen_bounds(GetGridBounds());
  const gfx::RectF item0_bounds = item0->target_bounds();
  const gfx::RectF item1_bounds = item1->target_bounds();
  const gfx::RectF item8_bounds = item8->target_bounds();
  const gfx::RectF item9_bounds = item9->target_bounds();

  // |item6| should be in the same row of windows as |item0|, but offscreen
  // (one screen length away).
  EXPECT_FALSE(screen_bounds.Contains(item8_bounds));
  EXPECT_EQ(item0_bounds.y(), item8_bounds.y());
  // |item7| should be in the same row of windows as |item1|, but offscreen
  // and below |item6|.
  EXPECT_FALSE(screen_bounds.Contains(item9_bounds));
  EXPECT_EQ(item1_bounds.y(), item9_bounds.y());
  EXPECT_LT(item8_bounds.y(), item9_bounds.y());
}

// Tests to see if windows are not shifted if all already available windows
// fit on screen.
TEST_F(TabletModeOverviewSessionTest, CheckNoOverviewItemShift) {
  auto windows = CreateAppWindows(4);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* item0 = GetOverviewItemForWindow(windows[0].get());
  const gfx::RectF before_shift_bounds = item0->target_bounds();

  GenerateScrollSequence(gfx::Point(100, 60), gfx::Point(0, 50));
  EXPECT_EQ(before_shift_bounds, item0->target_bounds());
}

// Tests to see if windows are shifted if at least one window is
// partially/completely positioned offscreen.
TEST_F(TabletModeOverviewSessionTest, CheckOverviewItemShift) {
  auto windows = CreateAppWindows(9);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* item0 = GetOverviewItemForWindow(windows[0].get());
  const gfx::RectF before_shift_bounds = item0->target_bounds();

  GenerateScrollSequence(gfx::Point(100, 60), gfx::Point(0, 50));
  EXPECT_LT(item0->target_bounds(), before_shift_bounds);
}

// Tests to see if windows remain in bounds after scrolling extremely far.
TEST_F(TabletModeOverviewSessionTest, CheckOverviewItemScrollingBounds) {
  auto windows = CreateAppWindows(8);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  // Scroll an extreme amount to see if windows on the far left are still in
  // bounds. First, align the left-most window (|windows[0]|) to the left-hand
  // bound and store the item's location. Then, scroll a far amount and check to
  // see if the item moved at all.
  auto* leftmost_window = GetOverviewItemForWindow(windows[0].get());

  GenerateScrollSequence(
      gfx::Point(BackGestureEventHandler::kStartGoingBackLeftEdgeInset + 5, 50),
      gfx::Point(5000, 50));
  const gfx::RectF left_bounds = leftmost_window->target_bounds();
  GenerateScrollSequence(
      gfx::Point(BackGestureEventHandler::kStartGoingBackLeftEdgeInset + 5, 50),
      gfx::Point(5000, 50));
  EXPECT_EQ(left_bounds, leftmost_window->target_bounds());

  // Scroll an extreme amount to see if windows on the far right are still in
  // bounds. First, align the right-most window (|windows[7]|) to the right-hand
  // bound and store the item's location. Then, scroll a far amount and check to
  // see if the item moved at all.
  auto* rightmost_window = GetOverviewItemForWindow(windows[7].get());
  GenerateScrollSequence(gfx::Point(5000, 50), gfx::Point(0, 50));
  const gfx::RectF right_bounds = rightmost_window->target_bounds();
  GenerateScrollSequence(gfx::Point(5000, 50), gfx::Point(0, 50));
  EXPECT_EQ(right_bounds, rightmost_window->target_bounds());
}

// Tests that destroying a window does not cause a crash while scrolling the
// overview grid. Regression test for https://crbug.com/1200605.
TEST_F(TabletModeOverviewSessionTest, WindowDestroyWhileScrolling) {
  auto windows = CreateAppWindows(8);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  // Start a scroll sequence.
  int x = 500;
  const int y = 200;
  base::TimeTicks timestamp = ui::EventTimeForNow();
  auto* event_generator = GetEventGenerator();
  ui::TouchEvent press(ui::EventType::kTouchPressed, gfx::Point(x, y),
                       timestamp, ui::PointerDetails());
  event_generator->Dispatch(&press);

  // Scroll a bit to the left, so the overview items that are offscreen on the
  // right start to become visible.
  const base::TimeDelta step_delay = base::Milliseconds(5);
  for (int i = 0; i < 10; ++i) {
    timestamp += step_delay;
    ui::TouchEvent move(ui::EventType::kTouchMoved, gfx::Point(x, y), timestamp,
                        ui::PointerDetails());
    event_generator->Dispatch(&move);
    x -= 5;
  }

  // Delete one of the windows.
  std::erase(windows, windows[2]);

  // Continue scrolling and then end the scroll. There should be no crash.
  for (int i = 0; i < 10; ++i) {
    timestamp += step_delay;
    ui::TouchEvent move(ui::EventType::kTouchMoved, gfx::Point(x, y), timestamp,
                        ui::PointerDetails());
    event_generator->Dispatch(&move);
    x -= 5;
  }

  ui::TouchEvent release(ui::EventType::kTouchReleased, gfx::Point(x, y),
                         timestamp, ui::PointerDetails());
  event_generator->Dispatch(&release);
}

// Tests that removing a desk does not cause a crash while scrolling the
// overview grid. Regression test for https://crbug.com/1455360.
TEST_F(TabletModeOverviewSessionTest, DeskRemovalWhileScrolling) {
  // The crash happened when closing a desk (which would add its app windows as
  // items in overview) midway through a scroll. Create two desks with windows;
  // the first desk has enough windows so that overview is scrollable.
  auto desk1_windows = CreateAppWindows(15);

  auto* controller = DesksController::Get();
  controller->NewDesk(DesksCreationRemovalSource::kKeyboard);
  ActivateDesk(controller->GetDeskAtIndex(1));
  auto desk2_windows = CreateAppWindows(2);

  // Activate the desk with 15 windows. There may be more than the windows we
  // created (i.e. backdrop, nudges), so we assert greater than.
  ActivateDesk(controller->GetDeskAtIndex(0));
  ASSERT_GT(controller->GetDeskAtIndex(0)->windows().size(), 15u);
  ASSERT_GT(controller->GetDeskAtIndex(1)->windows().size(), 2u);

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  // Start scrolling the overview grid.
  GetEventGenerator()->PressTouch(gfx::Point(400, 300));
  GetEventGenerator()->MoveTouchBy(-50, 0);

  // Remove the desk and continue scrolling. There should be no crash.
  RemoveDesk(controller->GetDeskAtIndex(1), DeskCloseType::kCombineDesks);
  GetEventGenerator()->MoveTouchBy(-50, 0);
}

// Tests the windows are stacked correctly when entering or exiting splitview
// while in tablet mode.
TEST_F(TabletModeOverviewSessionTest, StackingOrderSplitViewWindow) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateUnsnappableWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  // Snap `window1` to the left and `window3` to the right. Activate `window3`
  // so that it is stacked above `window1`.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window3.get(), SnapPosition::kSecondary);
  wm::ActivateWindow(window3.get());
  ASSERT_TRUE(window_util::IsStackedBelow(window1.get(), window3.get()));

  // Test that on entering overview, `window3` is stacked below `window1`, so
  // that when we scroll the grid, it will be seen under `window1`.
  ToggleOverview();
  ASSERT_FALSE(GetOverviewItemForWindow(window1.get()));
  ASSERT_TRUE(GetOverviewItemForWindow(window2.get()));
  ASSERT_TRUE(GetOverviewItemForWindow(window3.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(window3.get(), window1.get()));

  // Test that `window2` has a cannot snap widget indicating that it cannot be
  // snapped, and that both `window2` and the widget are lower z-order than
  // `window1`.
  views::Widget* cannot_snap_widget =
      GetCannotSnapWidget(GetOverviewItemForWindow(window2.get()));
  ASSERT_TRUE(cannot_snap_widget);
  aura::Window* cannot_snap_window = cannot_snap_widget->GetNativeWindow();
  ASSERT_EQ(window1->parent(), cannot_snap_window->parent());
  EXPECT_TRUE(window_util::IsStackedBelow(window2.get(), window1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(cannot_snap_window, window1.get()));

  // Test that on exiting overview, the relative stacking order between
  // `window3` and `window1` remains unchanged.
  ToggleOverview();
  EXPECT_TRUE(window_util::IsStackedBelow(window1.get(), window3.get()));
}

// Tests the windows are remain stacked underneath the split view window after
// dragging or long pressing.
TEST_F(TabletModeOverviewSessionTest, StackingOrderAfterGestureEvent) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);

  // Tests that if we long press, but cancel the event, the window stays stacked
  // under the snapped window.
  auto* item = GetOverviewItemForWindow(window2.get());
  const gfx::PointF item_center = item->target_bounds().CenterPoint();
  DispatchLongPress(item);
  ui::GestureEvent gesture_end(
      item_center.x(), item_center.y(), 0, ui::EventTimeForNow(),
      ui::GestureEventDetails(ui::EventType::kGestureEnd));
  item->HandleGestureEvent(&gesture_end, item);
  EXPECT_TRUE(window_util::IsStackedBelow(window2.get(), window1.get()));

  // Tests that if we drag the window around, then release, the window also
  // stays stacked under the snapped window.
  ASSERT_TRUE(InOverviewSession());
  const gfx::Vector2dF delta(15.f, 15.f);
  DispatchLongPress(item);
  GetOverviewSession()->Drag(item, item_center + delta);
  GetOverviewSession()->CompleteDrag(item, item_center + delta);
  EXPECT_TRUE(window_util::IsStackedBelow(window2.get(), window1.get()));
}

// Test that scrolling occurs if started on top of a window using the window's
// center-point as a start.
TEST_F(TabletModeOverviewSessionTest, HorizontalScrollingOnOverviewItem) {
  auto windows = CreateAppWindows(9);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* leftmost_window = GetOverviewItemForWindow(windows[0].get());
  const gfx::Point topleft_window_center =
      gfx::ToRoundedPoint(leftmost_window->target_bounds().CenterPoint());
  const gfx::RectF left_bounds = leftmost_window->target_bounds();

  GenerateScrollSequence(topleft_window_center, gfx::Point(-500, 50));
  EXPECT_LT(leftmost_window->target_bounds(), left_bounds);
}

// Tests that dragging a fullscreened window to snap in overview does not result
// in a u-a-f. Regression test for crbug.com/1330042.
TEST_F(TabletModeOverviewSessionTest, SnappingFullscreenWindow) {
  UpdateDisplay("800x600");

  auto window = CreateAppWindow(gfx::Rect(300, 300));

  const WMEvent fullscreen_event(WM_EVENT_FULLSCREEN);
  WindowState::Get(window.get())->OnWMEvent(&fullscreen_event);
  EXPECT_TRUE(WindowState::Get(window.get())->IsFullscreen());

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* item = GetOverviewItemForWindow(window.get());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  generator->MoveMouseTo(gfx::Point(10, 300));
  generator->ReleaseLeftButton();

  EXPECT_TRUE(WindowState::Get(window.get())->IsSnapped());
}

class ContinuousOverviewAnimationTest
    : public OverviewTestBase,
      public testing::WithParamInterface<bool> {
 public:
  ContinuousOverviewAnimationTest() = default;
  ContinuousOverviewAnimationTest(const ContinuousOverviewAnimationTest&) =
      delete;
  ContinuousOverviewAnimationTest& operator=(
      const ContinuousOverviewAnimationTest&) = delete;
  ~ContinuousOverviewAnimationTest() override = default;

  // OverviewTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kContinuousOverviewScrollAnimation,
                              features::kDeskButton,
                              features::kDeskBarWindowOcclusionOptimization},
        /*disabled_features=*/{});
    OverviewTestBase::SetUp();

    // TODO(zxdan): try to get and set the reverse scrolling with input device
    // settings controller. Toggle natural scrolling. Behavior should always
    // stay the same.
    PrefService* pref_service =
        Shell::Get()->session_controller()->GetActivePrefService();
    const bool enabled = GetParam();
    pref_service->SetBoolean(prefs::kTouchpadEnabled, true);
    pref_service->SetBoolean(prefs::kNaturalScroll, enabled);
  }

  // If `complete_scroll` is false, end the scroll with the fingers still on the
  // trackpad.
  void ThreeFingerScroll(float x_offset,
                         float y_offset,
                         bool complete_scroll,
                         const gfx::Point& start = gfx::Point()) {
    // When natural (reverse) scroll is ON, the horizontal offset stays same
    // while the vertical offset is flipped.
    const bool is_reverse_on = GetParam();
    GetEventGenerator()->ScrollSequence(
        start, base::Milliseconds(5), x_offset,
        is_reverse_on ? -y_offset : y_offset,
        /*steps=*/100, /*num_fingers=*/3,
        /*end_state=*/
        complete_scroll
            ? ui::test::EventGenerator::ScrollSequenceType::UpToFling
            : ui::test::EventGenerator::ScrollSequenceType::ScrollOnly);
  }

  void SetShowDeskButton(bool visible) {
    SetShowDeskButtonInShelfPref(
        Shell::Get()->session_controller()->GetActivePrefService(), visible);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that 3 finger scroll while hovering the desk button does not open
// the app list.
TEST_P(ContinuousOverviewAnimationTest, ScrollOnDeskButtonDoesNotOpenAppList) {
  ShelfViewTestAPI test_api(GetPrimaryShelf()->GetShelfViewForTesting());

  SetShowDeskButton(true);
  // The button should be visible.
  EXPECT_TRUE(test_api.shelf_view()
                  ->shelf_widget()
                  ->desk_button_widget()
                  ->GetLayer()
                  ->GetTargetVisibility());

  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  // Perform a very long swipe up gesture from the center of the desk button.
  const float long_scroll = WmGestureHandler::kVerticalThresholdDp + 200.f;
  ThreeFingerScroll(0, long_scroll, /*complete_scroll=*/true,
                    test_api.shelf_view()
                        ->shelf_widget()
                        ->desk_button_widget()
                        ->GetLayer()
                        ->bounds()
                        .CenterPoint());
  // We should be in overview mode.
  ASSERT_TRUE(InOverviewSession());
  // Desk button widget should be invisible in overview mode.
  EXPECT_FALSE(test_api.shelf_view()
                   ->shelf_widget()
                   ->desk_button_widget()
                   ->GetLayer()
                   ->GetTargetVisibility());
  // 3 finger scroll from the desk button widget should not show the app list.
  GetAppListTestHelper()->CheckVisibility(false);
}

// Tests that continuous scrolls slowly shrink active windows and increase the
// opacity of minimized windows, regardless of the state of `NaturalScroll`.
TEST_P(ContinuousOverviewAnimationTest, WindowSizesAndOpacities) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());
  std::unique_ptr<aura::Window> minimized_window(CreateTestWindow());
  WindowState::Get(minimized_window.get())->Minimize();

  // Get the original positions.
  const gfx::Rect original_bounds1 = window1->bounds();
  const gfx::Rect original_bounds2 = window2->bounds();
  const gfx::Rect original_bounds3 = window3->bounds();

  // Get the final positions by toggling overview mode regularly.
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());

  const gfx::Rect final_bounds1 = gfx::ToEnclosedRect(item1->target_bounds());
  const gfx::Rect final_bounds2 = gfx::ToEnclosedRect(item2->target_bounds());
  const gfx::Rect final_bounds3 = gfx::ToEnclosedRect(item3->target_bounds());

  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());

  // Swipe up a little bit and keep the fingers rested on the trackpad so that
  // the window placements are paused. Technically, we are in an overview
  // session, but the windows have not been placed in their final positions yet
  // due to the scroll still being in progress.
  const float short_scroll = 50.f;
  ThreeFingerScroll(0, short_scroll, /*complete_scroll=*/false);
  ASSERT_TRUE(InOverviewSession());

  // Get the current window positions and opacities.
  int top_inset = window1.get()->GetProperty(aura::client::kTopViewInset);
  gfx::RectF curr_bounds1 =
      window_util::GetTransformedBounds(window1.get(), top_inset);
  gfx::RectF curr_bounds2 =
      window_util::GetTransformedBounds(window2.get(), top_inset);
  gfx::RectF curr_bounds3 =
      window_util::GetTransformedBounds(window3.get(), top_inset);

  // Each active window should be smaller than their original state, but larger
  // than their final overview mode state.
  EXPECT_LT(curr_bounds1.width(), original_bounds1.width());
  EXPECT_GT(curr_bounds1.width(), final_bounds1.width());
  EXPECT_LT(curr_bounds2.width(), original_bounds2.width());
  EXPECT_GT(curr_bounds2.width(), final_bounds2.width());
  EXPECT_LT(curr_bounds3.width(), original_bounds3.width());
  EXPECT_GT(curr_bounds3.width(), final_bounds3.width());

  EXPECT_LT(curr_bounds1.height(), original_bounds1.height());
  EXPECT_GT(curr_bounds1.height(), final_bounds1.height());
  EXPECT_LT(curr_bounds2.height(), original_bounds2.height());
  EXPECT_GT(curr_bounds2.height(), final_bounds2.height());
  EXPECT_LT(curr_bounds3.height(), original_bounds3.height());
  EXPECT_GT(curr_bounds3.height(), final_bounds3.height());

  // Confirm the opacity of minimized windows is not 100%.
  float opacity = GetOverviewItemForWindow(minimized_window.get())
                      ->GetLeafItemForWindow(minimized_window.get())
                      ->item_widget()
                      ->GetLayer()
                      ->opacity();
  EXPECT_NE(opacity, 1.f);
  EXPECT_NE(opacity, 0.f);
}

// Tests that the opacity of the "No recent items" label is continuous.
TEST_P(ContinuousOverviewAnimationTest, NoRecentItemsLabel) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Start scrolling to enter overview. The no recent items label should have an
  // opacity between 0.f and 1.f and not be animating.
  const float short_scroll = 50.f;
  ThreeFingerScroll(0, short_scroll, /*complete_scroll=*/false);
  ASSERT_TRUE(InOverviewSession());

  views::Widget* no_windows_widget =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow())
          ->no_windows_widget();
  ASSERT_TRUE(no_windows_widget);

  ui::Layer* no_windows_layer = no_windows_widget->GetLayer();

  EXPECT_GT(no_windows_layer->opacity(), 0.f);
  EXPECT_LT(no_windows_layer->opacity(), 1.f);
  EXPECT_FALSE(no_windows_layer->GetAnimator()->is_animating());

  // Complete the enter overview scroll. The no recent items label should be
  // opaque.
  const float long_scroll = 500.f;
  ThreeFingerScroll(0, long_scroll, /*complete_scroll=*/false);
  EXPECT_EQ(1.f, no_windows_layer->opacity());

  ThreeFingerScroll(0, short_scroll, /*complete_scroll=*/true);
  EXPECT_EQ(1.f, no_windows_layer->opacity());
  WaitForOverviewEnterAnimation();
  ASSERT_TRUE(InOverviewSession());

  // Start scrolling to exit overview. The no recent items label should have an
  // opacity between 0.f and 1.f and not be animating.
  ThreeFingerScroll(0, -short_scroll, /*complete_scroll=*/false);
  EXPECT_GT(no_windows_layer->opacity(), 0.f);
  EXPECT_LT(no_windows_layer->opacity(), 1.f);
  EXPECT_FALSE(no_windows_layer->GetAnimator()->is_animating());
}

// Test that the rounded corners and shadows are shown at the correct times
// throughout a continuous scroll.
TEST_P(ContinuousOverviewAnimationTest, WindowCornerRadiiAndShadows) {
  std::unique_ptr<aura::Window> active_window(CreateTestWindow());
  std::unique_ptr<aura::Window> minimized_window(CreateTestWindow());
  WindowState::Get(minimized_window.get())->Minimize();

  // Swipe up a little bit and keep the fingers rested on the trackpad so
  // that the window placements are paused.
  const float short_scroll = 50.f;
  ThreeFingerScroll(0, short_scroll, /*complete_scroll=*/false);
  ASSERT_TRUE(InOverviewSession());

  auto* active_item = GetOverviewItemForWindow(active_window.get());
  auto* minimized_item = GetOverviewItemForWindow(minimized_window.get());

  // If a window is minimized, it should immediately show rounded corners.
  // Otherwise, retain sharp corners until the enter animation ends.
  EXPECT_FALSE(HasRoundedCorner(active_item));
  EXPECT_TRUE(HasRoundedCorner(minimized_item));

  // Shadows are hidden until the continuous swipe is over.
  EXPECT_TRUE(GetShadowBounds(active_item).IsEmpty());
  EXPECT_TRUE(GetShadowBounds(minimized_item).IsEmpty());

  // Reset.
  ToggleOverview();
  ASSERT_FALSE(InOverviewSession());

  // Give us some time to check the entry animation since we will be triggering
  // it by scrolling up and then lifting the fingers off of the trackpad.
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Scroll up more than 50% of the threshold then let go of the trackpad.
  const float medium_scroll =
      (WmGestureHandler::kVerticalThresholdDp / 2.f) + 1.f;
  ThreeFingerScroll(0, medium_scroll, /*complete_scroll=*/true);

  // Get the overview items again since this is a new overview session.
  active_item = GetOverviewItemForWindow(active_window.get());
  minimized_item = GetOverviewItemForWindow(minimized_window.get());

  // Rounded corners are shown once the fingers lift. Shadows on minimized
  // windows are shown, but shadows on non-minimized windows are hidden until
  // the animation is finished.
  EXPECT_TRUE(HasRoundedCorner(active_item));
  EXPECT_TRUE(GetShadowBounds(active_item).IsEmpty());
  EXPECT_TRUE(HasRoundedCorner(minimized_item));
  EXPECT_FALSE(GetShadowBounds(minimized_item).IsEmpty());

  // Ensure overview has been entered completely.
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  ASSERT_TRUE(InOverviewSession());

  // All items should have rounded corners and shadows.
  EXPECT_TRUE(HasRoundedCorner(active_item));
  EXPECT_TRUE(HasRoundedCorner(minimized_item));
  EXPECT_FALSE(GetShadowBounds(active_item).IsEmpty());
  EXPECT_FALSE(GetShadowBounds(minimized_item).IsEmpty());
}

// Tests that scrolls enter/exit overview mode as expected, regardless of the
// state of `NaturalScroll`.
TEST_P(ContinuousOverviewAnimationTest, ReverseGesturesTest) {
  const float long_scroll = 600.f;
  const float short_scroll = 50.f;
  ASSERT_FALSE(InOverviewSession());

  // Test an incorrect, complete, scroll.
  ThreeFingerScroll(0, -long_scroll, /*complete_scroll=*/true);
  ASSERT_FALSE(InOverviewSession());

  // Test a correct, complete, scroll.
  ThreeFingerScroll(0, long_scroll, /*complete_scroll=*/true);
  ASSERT_TRUE(InOverviewSession());

  // Test an incorrect, complete, scroll.
  ThreeFingerScroll(0, long_scroll, /*complete_scroll=*/true);
  ASSERT_TRUE(InOverviewSession());

  // Test a correct, complete, scroll.
  ThreeFingerScroll(0, -long_scroll, /*complete_scroll=*/true);
  ASSERT_FALSE(InOverviewSession());

  // Test an incorrect, incomplete, scroll.
  ThreeFingerScroll(0, -short_scroll, /*complete_scroll=*/false);
  ASSERT_FALSE(InOverviewSession());

  // Test a correct, incomplete, scroll.
  ThreeFingerScroll(0, short_scroll, /*complete_scroll=*/false);
  ASSERT_TRUE(InOverviewSession());
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         ContinuousOverviewAnimationTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "NaturalScrollOn"
                                             : "NaturalScrollOff";
                         });

// A unique test class for testing flings in overview as those rely on observing
// compositor animations which require a mock time task environment.
class OverviewSessionFlingTest : public AshTestBase {
 public:
  OverviewSessionFlingTest()
      : AshTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~OverviewSessionFlingTest() override = default;

  OverviewSessionFlingTest(const OverviewSessionFlingTest&) = delete;
  OverviewSessionFlingTest& operator=(const OverviewSessionFlingTest&) = delete;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();

    // Overview flinging is only available in tablet mode.
    base::RunLoop().RunUntilIdle();
    TabletModeControllerTestApi().EnterTabletMode();
    base::RunLoop().RunUntilIdle();
  }
};

TEST_F(OverviewSessionFlingTest, BasicFling) {
  std::vector<std::unique_ptr<aura::Window>> windows(16);
  for (int i = 15; i >= 0; --i)
    windows[i] = CreateTestWindow();

  ToggleOverview();
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  OverviewGridEventHandler* grid_event_handler = grid->grid_event_handler();

  auto* item = GetOverviewItemForWindow(windows[2].get());
  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());

  // Create a scroll sequence which results in a fling.
  const gfx::Vector2d shift(-200, 0);
  GetEventGenerator()->GestureScrollSequence(item_center, item_center + shift,
                                             base::Milliseconds(10), 10);

  ui::Compositor* const compositor =
      windows[0]->GetRootWindow()->layer()->GetCompositor();
  ui::DrawWaiterForTest::WaitForCompositingStarted(compositor);
  ASSERT_TRUE(grid_event_handler->IsFlingInProgressForTesting());

  // Test that the scroll offset decreases as we advance the clock. Check the
  // scroll offset instead of the item bounds as there is an optimization which
  // does not update the item bounds of invisible elements. On some iterations,
  // there may not be enough time passed to decay the velocity so the scroll
  // offset will not change, but the overall change should be substantial.
  constexpr int kMaxLoops = 10;
  const float initial_scroll_offset = OverviewGridTestApi(grid).scroll_offset();
  float previous_scroll_offset = initial_scroll_offset;
  for (int i = 0;
       i < kMaxLoops && grid_event_handler->IsFlingInProgressForTesting();
       ++i) {
    task_environment()->FastForwardBy(base::Milliseconds(50));
    ui::DrawWaiterForTest::WaitForCompositingStarted(compositor);

    float scroll_offset = OverviewGridTestApi(grid).scroll_offset();
    EXPECT_LE(scroll_offset, previous_scroll_offset);
    previous_scroll_offset = scroll_offset;
  }

  EXPECT_LT(OverviewGridTestApi(grid).scroll_offset(),
            initial_scroll_offset - 100.f);
}

// Tests that a vertical scroll sequence will close the window it is scrolled
// on.
TEST_F(TabletModeOverviewSessionTest, VerticalScrollingOnOverviewItem) {
  constexpr int kNumWidgets = 8;
  std::vector<std::unique_ptr<views::Widget>> widgets(kNumWidgets);
  for (int i = kNumWidgets - 1; i >= 0; --i) {
    widgets[i] =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  }
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* leftmost_window =
      GetOverviewItemForWindow(widgets[0]->GetNativeWindow());
  const gfx::Point topleft_window_center =
      gfx::ToRoundedPoint(leftmost_window->target_bounds().CenterPoint());
  const gfx::Point end_point = topleft_window_center - gfx::Vector2d(0, 300);

  GenerateScrollSequence(topleft_window_center, end_point);
  EXPECT_TRUE(widgets[0]->IsClosed());
}

// Test that scrolling occurs if we hit the associated keyboard shortcut.
TEST_F(TabletModeOverviewSessionTest, CheckScrollingWithKeyboardShortcut) {
  auto windows = CreateAppWindows(9);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  auto* leftmost_window = GetOverviewItemForWindow(windows[0].get());
  const gfx::RectF left_bounds = leftmost_window->target_bounds();

  PressAndReleaseKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  EXPECT_LT(leftmost_window->target_bounds(), left_bounds);
}

// Test that tapping a window in overview closes overview mode.
TEST_F(TabletModeOverviewSessionTest, CheckWindowActivateOnTap) {
  base::UserActionTester user_action_tester;
  auto windows = CreateAppWindows(8);
  wm::ActivateWindow(windows[1].get());

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  // Tap on |windows[1]| to exit overview.
  GetEventGenerator()->GestureTapAt(
      GetTransformedTargetBounds(windows[1].get()).CenterPoint());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |windows[1]| remains active. Click on it to exit overview.
  ASSERT_EQ(windows[1].get(), window_util::GetFocusedWindow());
  ToggleOverview();
  ClickWindow(windows[1].get());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

TEST_F(TabletModeOverviewSessionTest, LayoutValidAfterRotation) {
  if (!features::IsForestFeatureEnabled()) {
    return;
  }

  UpdateDisplay("1366x768");
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  auto windows = CreateAppWindows(9);

  // Helper to determine whether a grid layout is valid. It is considered valid
  // if the left edge of the first item is close enough to the left edge of the
  // grid bounds and if the right edge of the last item is close enough to the
  // right edge of the grid bounds. Either of these being false would mean there
  // is a large padding which shouldn't be there.
  auto layout_valid = [&windows, this](int expected_padding) {
    auto* first_item = GetOverviewItemForWindow(windows.front().get());
    auto* last_item = GetOverviewItemForWindow(windows.back().get());

    const gfx::Rect first_bounds =
        gfx::ToEnclosedRect(first_item->target_bounds());
    const gfx::Rect last_bounds =
        gfx::ToEnclosedRect(last_item->target_bounds());

    const gfx::Rect grid_bounds = GetGridBounds();
    const bool first_bounds_valid =
        first_bounds.x() <= (grid_bounds.x() + expected_padding);
    const bool last_bounds_valid =
        last_bounds.right() >= (grid_bounds.right() - expected_padding);
    return first_bounds_valid && last_bounds_valid;
  };

  // Enter overview and scroll to the edge of the grid. The layout should remain
  // valid.
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  // The expected padding should be the x position of the first item, before the
  // grid gets shifted.
  const int expected_padding =
      GetOverviewItemForWindow(windows.front().get())->target_bounds().x();
  GenerateScrollSequence(gfx::Point(1300, 10), gfx::Point(100, 10));
  EXPECT_TRUE(layout_valid(expected_padding));

  // Tests that the layout is still valid after a couple rotations.
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_TRUE(layout_valid(expected_padding));

  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_TRUE(layout_valid(expected_padding));
}

// Tests that windows snap through long press and drag to left or right side of
// the screen.
TEST_F(TabletModeOverviewSessionTest, DragOverviewWindowToSnap) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());

  // Dispatches a long press event at the |overview_item1|'s current location to
  // start dragging in SplitView. Drags |overview_item1| to the left border of
  // the screen. SplitView should trigger and upon completing drag,
  // |overview_item1| should snap to the left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  const gfx::PointF snap_left_location =
      gfx::PointF(GetGridBounds().left_center());

  DispatchLongPress(overview_item1);
  GetOverviewSession()->Drag(
      overview_item1,
      gfx::PointF(overview_item1->target_bounds().left_center()));
  GetOverviewSession()->CompleteDrag(overview_item1, snap_left_location);

  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());

  // Dispatches a long press event at the |overview_item2|'s current location to
  // start dragging in SplitView. Drags |overview_item2| to the right border of
  // the screen. Upon completing drag, |overview_item2| should snap to the
  // right.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  const gfx::PointF snap_right_location =
      gfx::PointF(GetGridBounds().right_center());

  DispatchLongPress(overview_item2);
  GetOverviewSession()->Drag(
      overview_item2,
      gfx::PointF(overview_item2->target_bounds().right_center()));
  GetOverviewSession()->CompleteDrag(overview_item2, snap_right_location);

  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());
}

// Verify that if the window item has been dragged enough vertically, the window
// will be closed.
TEST_F(TabletModeOverviewSessionTest, DragToClose) {
  // This test requires a widget.
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  auto* item = GetOverviewItemForWindow(widget->GetNativeWindow());
  const gfx::PointF start = item->target_bounds().CenterPoint();
  ASSERT_TRUE(item);

  // This drag has not covered enough distance, so the widget is not closed and
  // we remain in overview mode.
  GetOverviewSession()->InitiateDrag(item, start, /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item);
  GetOverviewSession()->Drag(item, start + gfx::Vector2dF(0, 80));
  GetOverviewSession()->CompleteDrag(item, start + gfx::Vector2dF(0, 80));
  ASSERT_TRUE(GetOverviewSession());

  // Verify that the second drag has enough vertical distance, so the widget
  // will be closed and overview mode will be exited.
  GetOverviewSession()->InitiateDrag(item, start, /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item);
  GetOverviewSession()->Drag(item, start + gfx::Vector2dF(0, 180));
  GetOverviewSession()->CompleteDrag(item, start + gfx::Vector2dF(0, 180));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetOverviewSession());
  EXPECT_TRUE(widget->IsClosed());
}

// Verify that if the window item has been flung enough vertically, the window
// will be closed.
TEST_F(TabletModeOverviewSessionTest, FlingToClose) {
  // This test requires a widget.
  std::unique_ptr<views::Widget> widget(
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET));

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(1u, GetOverviewSession()->grid_list()[0]->GetNumWindows());

  auto* item = GetOverviewItemForWindow(widget->GetNativeWindow());
  const gfx::PointF start = item->target_bounds().CenterPoint();
  ASSERT_TRUE(item);

  // Verify that items flung horizontally do not close the item.
  GetOverviewSession()->InitiateDrag(item, start, /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item);
  GetOverviewSession()->Drag(item, start + gfx::Vector2dF(0, 50));
  GetOverviewSession()->Fling(item, start, 2500, 0);
  ASSERT_TRUE(GetOverviewSession());

  // Verify that items flung vertically but without enough velocity do not
  // close the item.
  GetOverviewSession()->InitiateDrag(item, start, /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item);
  GetOverviewSession()->Drag(item, start + gfx::Vector2dF(0, 50));
  GetOverviewSession()->Fling(item, start, 0, 1500);
  ASSERT_TRUE(GetOverviewSession());

  // Verify that flinging the item closes it, and since it is the last item in
  // overview mode, overview mode is exited.
  GetOverviewSession()->InitiateDrag(item, start, /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item);
  GetOverviewSession()->Drag(item, start + gfx::Vector2dF(0, 50));
  GetOverviewSession()->Fling(item, start, 0, 2500);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetOverviewSession());
  EXPECT_TRUE(widget->IsClosed());
}

// Tests that nudging occurs in the most basic case, which is we have one row
// and one item which is about to be deleted by dragging. If the item is deleted
// we still only have one row, so the other items should nudge while the item is
// being dragged.
TEST_F(TabletModeOverviewSessionTest, BasicNudging) {
  // Set up three equal windows, which take up one row on the overview grid.
  // When one of them is deleted we are still left with all the windows on one
  // row.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());

  const gfx::RectF item1_bounds = item1->target_bounds();
  const gfx::RectF item2_bounds = item2->target_bounds();
  const gfx::RectF item3_bounds = item3->target_bounds();

  // Drag |item1| vertically. |item2| and |item3| bounds should change as they
  // should be nudging towards their final bounds.
  GetOverviewSession()->InitiateDrag(item1, item1_bounds.CenterPoint(),
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item1);
  GetOverviewSession()->Drag(
      item1, item1_bounds.CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_NE(item2_bounds, item2->target_bounds());
  EXPECT_NE(item3_bounds, item3->target_bounds());

  // Drag |item1| back to its start drag location and release, so that it does
  // not get deleted.
  GetOverviewSession()->Drag(item1, item1_bounds.CenterPoint());
  GetOverviewSession()->CompleteDrag(item1, item1_bounds.CenterPoint());

  // Drag |item3| vertically. |item1| and |item2| bounds should change as they
  // should be nudging towards their final bounds.
  GetOverviewSession()->InitiateDrag(item3, item3_bounds.CenterPoint(),
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item3);
  GetOverviewSession()->Drag(
      item3, item3_bounds.CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_NE(item1_bounds, item1->target_bounds());
  EXPECT_NE(item2_bounds, item2->target_bounds());
}

// Tests that no nudging occurs when the number of rows in overview mode change
// if the item to be deleted results in the overview grid to change number of
// rows.
TEST_F(TabletModeOverviewSessionTest, NoNudgingWhenNumRowsChange) {
  UpdateDisplay("800x700");

  // Set up four equal windows, which would split into two rows in overview
  // mode. Removing one window would leave us with three windows, which only
  // takes a single row in overview.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  std::unique_ptr<aura::Window> window4 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());
  auto* item4 = GetOverviewItemForWindow(window4.get());

  const gfx::RectF item1_bounds = item1->target_bounds();
  const gfx::RectF item2_bounds = item2->target_bounds();
  const gfx::RectF item3_bounds = item3->target_bounds();
  const gfx::RectF item4_bounds = item4->target_bounds();

  // Ensure there are two rows in overview.
  ASSERT_EQ(item1_bounds.y(), item2_bounds.y());
  ASSERT_EQ(item3_bounds.y(), item4_bounds.y());
  ASSERT_NE(item1_bounds.y(), item3_bounds.y());

  // Drag |item1| past the drag to swipe threshold. None of the other window
  // bounds should change, as none of them should be nudged.
  GetOverviewSession()->InitiateDrag(item1, item1_bounds.CenterPoint(),
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item1);
  GetOverviewSession()->Drag(
      item1, item1_bounds.CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_EQ(item2_bounds, item2->target_bounds());
  EXPECT_EQ(item3_bounds, item3->target_bounds());
  EXPECT_EQ(item4_bounds, item4->target_bounds());
}

// Tests that no nudging occurs when the item to be deleted results in an item
// from the previous row to drop down to the current row, thus causing the items
// to the right of the item to be shifted right, which is visually unacceptable.
TEST_F(TabletModeOverviewSessionTest, NoNudgingWhenLastItemOnPreviousRowDrops) {
  UpdateDisplay("800x700");

  // Set up five equal windows, which would split into two rows in overview
  // mode. Removing one window would cause the rows to rearrange, with the third
  // item dropping down from the first row to the second row. Create the windows
  // backward so the the window indexs match the order seen in overview, as
  // overview windows are ordered by MRU.
  const int kWindows = 5;
  std::unique_ptr<aura::Window> windows[kWindows];
  for (int i = kWindows - 1; i >= 0; --i)
    windows[i] = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  OverviewItemBase* items[kWindows];
  gfx::RectF item_bounds[kWindows];
  for (int i = 0; i < kWindows; ++i) {
    items[i] = GetOverviewItemForWindow(windows[i].get());
    item_bounds[i] = items[i]->target_bounds();
  }

  // Drag the forth item past the drag to swipe threshold. None of the other
  // window bounds should change, as none of them should be nudged, because
  // deleting the fourth item will cause the third item to drop down from the
  // first row to the second.
  GetOverviewSession()->InitiateDrag(items[3], item_bounds[3].CenterPoint(),
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/items[3]);
  GetOverviewSession()->Drag(
      items[3], item_bounds[3].CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_EQ(item_bounds[0], items[0]->target_bounds());
  EXPECT_EQ(item_bounds[1], items[1]->target_bounds());
  EXPECT_EQ(item_bounds[2], items[2]->target_bounds());
  EXPECT_EQ(item_bounds[4], items[4]->target_bounds());

  // Drag the fourth item back to its start drag location and release, so that
  // it does not get deleted.
  GetOverviewSession()->Drag(items[3], item_bounds[3].CenterPoint());
  GetOverviewSession()->CompleteDrag(items[3], item_bounds[3].CenterPoint());

  // Drag the first item past the drag to swipe threshold. The second and third
  // items should nudge as expected as there is no item dropping down to their
  // row. The fourth and fifth items should not nudge as they are in a different
  // row than the first item.
  GetOverviewSession()->InitiateDrag(items[0], item_bounds[0].CenterPoint(),
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/items[0]);
  GetOverviewSession()->Drag(
      items[0], item_bounds[0].CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_NE(item_bounds[1], items[1]->target_bounds());
  EXPECT_NE(item_bounds[2], items[2]->target_bounds());
  EXPECT_EQ(item_bounds[3], items[3]->target_bounds());
  EXPECT_EQ(item_bounds[4], items[4]->target_bounds());
}

// Tests that there is no crash when destroying a window during a nudge drag.
// Regression test for https://crbug.com/997335.
TEST_F(TabletModeOverviewSessionTest, DestroyWindowDuringNudge) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  auto* item = GetOverviewItemForWindow(window1.get());
  const gfx::PointF item_center = item->target_bounds().CenterPoint();

  // Drag |item1| vertically to start nudging.
  GetOverviewSession()->InitiateDrag(item, item_center,
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/item);
  GetOverviewSession()->Drag(item, item_center + gfx::Vector2dF(0, 160));

  // Destroy |window2| and |window3|,then keep dragging. There should be no
  // crash.
  window2.reset();
  window3.reset();
  GetOverviewSession()->Drag(item, item_center + gfx::Vector2dF(0, 260));
}

TEST_F(TabletModeOverviewSessionTest, MultiTouch) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  // Dispatches a long press event to start drag mode.
  auto* item = GetOverviewItemForWindow(window1.get());
  DispatchLongPress(item);
  GetOverviewSession()->Drag(item, gfx::PointF(10.f, 500.f));
  const gfx::Rect item_bounds = item->GetWindow()->GetBoundsInScreen();

  // Tap on a point on the wallpaper. Normally this would exit overview, but not
  // while a drag is underway.
  GetEventGenerator()->set_current_screen_location(gfx::Point(10, 10));
  GetEventGenerator()->PressTouch();
  GetEventGenerator()->ReleaseTouch();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(item_bounds, item->GetWindow()->GetBoundsInScreen());

  // Long press on another item, the bounds of both items should be unchanged.
  auto* item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Rect item2_bounds = item2->GetWindow()->GetBoundsInScreen();
  DispatchLongPress(item2);
  EXPECT_EQ(item_bounds, item->GetWindow()->GetBoundsInScreen());
  EXPECT_EQ(item2_bounds, item2->GetWindow()->GetBoundsInScreen());

  // Clicking on a point on the wallpaper should still exit overview.
  GetEventGenerator()->set_current_screen_location(gfx::Point(10, 10));
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
}

// Tests that when exiting overview in a way that causes windows to minimize,
// rounded corners are removed, otherwise they will be visible after
// unminimizing. Regression test for https://crbug.com/1146240.
TEST_F(TabletModeOverviewSessionTest, MinimizedRoundedCorners) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window(CreateTestWindow(bounds));

  // Enter overview. Spin the run loop since rounded corners are applied on a
  // post task.
  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  // Tap on a point on the wallpaper to minimize the window and exit overview.
  GetEventGenerator()->set_current_screen_location(gfx::Point(10, 10));
  GetEventGenerator()->ClickLeftButton();

  // Tests that the window layer has rounded corners removed after exiting
  // overview.
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_EQ(gfx::RoundedCornersF(), window->layer()->rounded_corner_radii());
}

// Tests the UAF issue reported in b/301368132 has been fixed. The overview
// item in `OverviewWindowDragController::CompleteDrag()` may be reset in
// `OverviewGrid::RemoveItem()` and is accessed again when getting the
// window for `ScopedFloatContainerStacker::OnDragFinished()`.
TEST_F(TabletModeOverviewSessionTest, AvoidUaFOnCompleteDrag) {
  std::unique_ptr<aura::Window> window = CreateAppWindow(gfx::Rect(100, 100));
  WindowState* window_state = WindowState::Get(window.get());
  const WindowSnapWMEvent snap_type(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_type);
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state->GetStateType());
  OverviewController* overview_controller = OverviewController::Get();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  window_state->Minimize();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  auto* overview_item = GetOverviewItemForWindow(window.get());

  // Trigger `OverviewWindowDragController::CompleteDrag()` and verify that
  // there will be no crash.
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  event_generator->ClickLeftButton();
  EXPECT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state->GetStateType());
}

// Test the split view and overview functionalities in tablet mode.
class SplitViewOverviewSessionTest : public OverviewTestBase {
 public:
  SplitViewOverviewSessionTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDeskBarWindowOcclusionOptimization,
                              chromeos::features::
                                  kOverviewSessionInitOptimizations},
        /*disabled_features=*/{});
  }

  SplitViewOverviewSessionTest(const SplitViewOverviewSessionTest&) = delete;
  SplitViewOverviewSessionTest& operator=(const SplitViewOverviewSessionTest&) =
      delete;

  ~SplitViewOverviewSessionTest() override = default;

  enum class SelectorItemLocation {
    CENTER,
    ORIGIN,
    TOP_RIGHT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT,
  };

  void SetUp() override {
    OverviewTestBase::SetUp();
    EnterTabletMode();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  SplitViewDivider* split_view_divider() {
    return split_view_controller()->split_view_divider();
  }

  bool IsDividerAnimating() {
    return split_view_controller()->IsDividerAnimating();
  }

  void SkipDividerSnapAnimation() {
    if (!IsDividerAnimating())
      return;
    split_view_controller()->StopAndShoveAnimatedDivider();
    split_view_controller()->EndResizeWithDividerImpl();
    split_view_controller()->EndSplitViewAfterResizingAtEdgeIfAppropriate();
  }

  void EndSplitView() { split_view_controller()->EndSplitView(); }

  void CheckWindowResizingPerformanceHistograms(
      const char* trace,
      int with_empty_overview_grid,
      int max_latency_with_empty_overview_grid,
      int with_nonempty_overview_grid,
      int max_latency_with_nonempty_overview_grid) {
    CheckForDuplicateTraceName(trace);
    SCOPED_TRACE(trace);

    histograms_.ExpectTotalCount(
        "Ash.SplitViewResize.PresentationTime.ClamshellMode.SingleWindow",
        with_empty_overview_grid);
    histograms_.ExpectTotalCount(
        "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
        "SingleWindow",
        max_latency_with_empty_overview_grid);
    histograms_.ExpectTotalCount(
        "Ash.SplitViewResize.PresentationTime.ClamshellMode.WithOverview",
        with_nonempty_overview_grid);
    histograms_.ExpectTotalCount(
        "Ash.SplitViewResize.PresentationTime.MaxLatency.ClamshellMode."
        "WithOverview",
        max_latency_with_nonempty_overview_grid);
  }

 protected:
  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
        bounds);
    return window;
  }

  aura::Window* CreateWindowWithMinimumSize(const gfx::Rect& bounds,
                                            const gfx::Size& size) {
    auto* delegate =
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(delegate, -1, bounds);
    delegate->set_minimum_size(size);
    return window;
  }

  // Returns the expected overview bounds including the hotseat inset. See
  // `ShrinkBoundsByHotseatInset()`.
  // TODO(sophiewen): Refactor this for both `SplitViewOverviewSessionTest`
  // and `FasterSplitScreenSetupTest` and make this work for multi-display.
  gfx::Rect GetExpectedOverviewBounds() {
    aura::Window* root_window = Shell::GetPrimaryRootWindow();
    gfx::Rect overview_bounds(GetWorkAreaInScreen(root_window));

    if (auto* split_view_drag_indicators =
            GetOverviewGridForRoot(root_window)->split_view_drag_indicators();
        split_view_drag_indicators) {
      // If we are dragging to snap, `SplitViewOverviewSession` is not active
      // yet, but the overview grid bounds are split.
      gfx::Rect left_bounds, right_bounds;
      overview_bounds.SplitVertically(left_bounds, right_bounds);
      // If we are dragging to snap in tablet mode, `split_view_divider` hasn't
      // been created yet, but we still need to subtract the divider width.
      const int divider_width = display::Screen::GetScreen()->InTabletMode()
                                    ? kSplitviewDividerShortSideLength / 2
                                    : 0;
      switch (split_view_drag_indicators->current_window_dragging_state()) {
        case SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary:
          // If we are dragging to snap left, the grid bounds are on the right.
          right_bounds.set_x(right_bounds.x() + divider_width);
          right_bounds.set_width(right_bounds.width() - divider_width);
          return right_bounds;
        case SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary:
          // If we are dragging to snap right, the grid bounds are on the left.
          left_bounds.set_width(left_bounds.width() - divider_width);
          return left_bounds;
        case SplitViewDragIndicators::WindowDraggingState::kNoDrag:
          break;
        case SplitViewDragIndicators::WindowDraggingState::kOtherDisplay:
        case SplitViewDragIndicators::WindowDraggingState::kFromOverview:
        case SplitViewDragIndicators::WindowDraggingState::kFromTop:
        case SplitViewDragIndicators::WindowDraggingState::kFromShelf:
        case SplitViewDragIndicators::WindowDraggingState::kFromFloat:
          NOTREACHED();
      }
    }

    return GetGridBoundsInScreen(root_window);
  }

  gfx::Rect GetSplitViewDividerBounds(bool is_dragging) {
    if (!split_view_controller()->InTabletSplitViewMode()) {
      return gfx::Rect();
    }
    return split_view_controller()
        ->split_view_divider()
        ->GetDividerBoundsInScreen(is_dragging);
  }

  gfx::Rect GetWorkAreaInScreen(aura::Window* window) {
    return screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
        window);
  }

  // Drags a overview item |item| from its center or one of its corners
  // to |end_location|. This should be used over
  // DragWindowTo(OverviewItem*, gfx::Point) when testing snapping a
  // window, but the windows centerpoint may be inside a snap region, thus the
  // window will not snapped. This function is mostly used to test splitview so
  // |long_press| is default to true. Set |long_press| to false if we do not
  // want to long press after every press, which enables dragging vertically to
  // close an item.
  void DragWindowTo(OverviewItemBase* item,
                    const gfx::PointF& end_location,
                    SelectorItemLocation location,
                    bool long_press = true) {
    gfx::PointF start_location;
    switch (location) {
      case SelectorItemLocation::CENTER:
        start_location = item->target_bounds().CenterPoint();
        break;
      case SelectorItemLocation::ORIGIN:
        start_location = item->target_bounds().origin();
        break;
      case SelectorItemLocation::TOP_RIGHT:
        start_location = item->target_bounds().top_right();
        break;
      case SelectorItemLocation::BOTTOM_RIGHT:
        start_location = item->target_bounds().bottom_right();
        break;
      case SelectorItemLocation::BOTTOM_LEFT:
        start_location = item->target_bounds().bottom_left();
        break;
      default:
        NOTREACHED();
    }
    GetOverviewSession()->InitiateDrag(item, start_location,
                                       /*is_touch_dragging=*/true,
                                       /*event_source_item=*/item);
    if (long_press)
      GetOverviewSession()->StartNormalDragMode(start_location);
    GetOverviewSession()->Drag(item, end_location);
    GetOverviewSession()->CompleteDrag(item, end_location);
  }

  // Drags a overview item |item| from its center point to |end_location|.
  void DragWindowTo(OverviewItemBase* item, const gfx::PointF& end_location) {
    DragWindowTo(item, end_location, SelectorItemLocation::CENTER, true);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests that dragging an overview item to the edge of the screen snaps the
// window. If two windows are snapped to left and right side of the screen, exit
// the overview mode.
TEST_F(SplitViewOverviewSessionTest, DragOverviewWindowToSnap) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));

  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());

  // Drag |window2| selector item to attempt to snap to left. Since there is
  // already one left snapped window |window1|, |window1| will be put in
  // overview mode.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF(0, 0));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window2.get());
  EXPECT_TRUE(GetOverviewController()->overview_session()->IsWindowInOverview(
      window1.get()));

  // Drag |window3| selector item to snap to right.
  auto* overview_item3 = GetOverviewItemForWindow(window3.get());
  const gfx::PointF end_location3(GetWorkAreaInScreen(window3.get()).width(),
                                  0.f);
  DragWindowTo(overview_item3, end_location3);

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window3.get());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
}

// Regression test for http://b/323136574, where a floated window should not
// have an unclipped size when it's in a partial overview session.
TEST_F(SplitViewOverviewSessionTest, FloatedWindowsHaveNoUnclippedSize) {
  std::unique_ptr<aura::Window> window1 = CreateAppWindow();
  std::unique_ptr<aura::Window> window2 = CreateAppWindow();

  // Float `window1` and then snap `window2`. A partial overview session should
  // start.
  Shell::Get()->float_controller()->ToggleFloat(window1.get());
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFloated());

  const WindowSnapWMEvent event(
      WM_EVENT_CYCLE_SNAP_SECONDARY,
      WindowSnapActionSource::kKeyboardShortcutToSnap);
  auto* window2_state = WindowState::Get(window2.get());
  window2_state->OnWMEvent(&event);
  EXPECT_TRUE(window2_state->IsSnapped());

  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  auto* window1_item = GetOverviewItemForWindow(window1.get());
  ASSERT_TRUE(window1_item);

  EXPECT_FALSE(window1_item->unclipped_size_for_testing());
}

// Verify the correct behavior when dragging windows in overview mode.
TEST_F(SplitViewOverviewSessionTest, OverviewDragControllerBehavior) {
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_short_press_time(base::Milliseconds(1));
  gesture_config->set_show_press_delay_in_ms(1);

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  auto* window_item1 = GetOverviewItemForWindow(window1.get());
  auto* window_item2 = GetOverviewItemForWindow(window2.get());

  // Verify that if a drag is orginally horizontal, the drag behavior is drag to
  // snap.
  using DragBehavior = OverviewWindowDragController::DragBehavior;
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(window_item1->target_bounds().CenterPoint()));
  generator->PressTouch();

  // Simulate a long press, which is required to snap windows.
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
  run_loop.Run();

  OverviewWindowDragController* drag_controller =
      GetOverviewSession()->window_drag_controller();
  ASSERT_TRUE(drag_controller);
  EXPECT_EQ(DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior_for_testing());
  generator->MoveTouchBy(20, 0);
  EXPECT_EQ(DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior_for_testing());
  generator->ReleaseTouch();
  EXPECT_EQ(DragBehavior::kNoDrag,
            drag_controller->current_drag_behavior_for_testing());

  // Verify that if a drag is orginally vertical, the drag behavior is drag to
  // close.
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(window_item2->target_bounds().CenterPoint()));
  generator->PressTouch();

  // Use small increments otherwise a fling event will be fired.
  for (int j = 0; j < 20; ++j)
    generator->MoveTouchBy(0, 1);

  // A new instance of drag controller gets created each time a drag starts.
  drag_controller = GetOverviewSession()->window_drag_controller();
  EXPECT_EQ(DragBehavior::kDragToClose,
            drag_controller->current_drag_behavior_for_testing());
}

// Verify the window grid size changes as expected when dragging items around in
// overview mode when split view is enabled.
TEST_F(SplitViewOverviewSessionTest,
       OverviewGridSizeWhileDraggingWithSplitView) {
  // Add three windows and enter overview mode.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  // Select window one and start the drag.
  const int window_width =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().width();
  auto* overview_item = GetOverviewItemForWindow(window1.get());
  gfx::RectF overview_item_bounds = overview_item->target_bounds();
  gfx::PointF start_location(overview_item_bounds.CenterPoint());
  GetOverviewSession()->InitiateDrag(overview_item, start_location,
                                     /*is_touch_dragging=*/false,
                                     /*event_source_item=*/overview_item);

  // Verify that when dragged to the left, the window grid is located where the
  // right window of split view mode should be.
  const gfx::PointF left(0, 0);
  GetOverviewSession()->Drag(overview_item, left);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->primary_window() == nullptr);
  EXPECT_EQ(GetExpectedOverviewBounds(), GetGridBounds());

  // Verify that when dragged to the right, the window grid is located where the
  // left window of split view mode should be.
  const gfx::PointF right(window_width, 0);
  GetOverviewSession()->Drag(overview_item, right);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->secondary_window() == nullptr);
  EXPECT_EQ(GetExpectedOverviewBounds(), GetGridBounds());

  // Verify that when dragged to the center, the window grid is has the
  // dimensions of the work area.
  const gfx::PointF center(window_width / 2, 0);
  GetOverviewSession()->Drag(overview_item, center);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_EQ(GetWorkAreaInScreen(window1.get()), GetGridBounds());

  // Snap window1 to the left and initialize dragging for window2.
  GetOverviewSession()->Drag(overview_item, left);
  GetOverviewSession()->CompleteDrag(overview_item, left);
  ASSERT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  ASSERT_EQ(window1.get(), split_view_controller()->primary_window());
  overview_item = GetOverviewItemForWindow(window2.get());
  overview_item_bounds = overview_item->target_bounds();
  start_location = overview_item_bounds.CenterPoint();
  GetOverviewSession()->InitiateDrag(overview_item, start_location,
                                     /*is_touch_dragging=*/false,
                                     /*event_source_item=*/overview_item);

  // Verify that when there is a snapped window, the window grid bounds remain
  // constant despite overview items being dragged left and right.
  GetOverviewSession()->Drag(overview_item, left);
  const gfx::Rect expected_grid_bounds = GetExpectedOverviewBounds();
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  GetOverviewSession()->Drag(overview_item, right);
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  GetOverviewSession()->Drag(overview_item, center);
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
}

// Tests dragging a unsnappable window.
TEST_F(SplitViewOverviewSessionTest, DraggingUnsnappableAppWithSplitView) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  // The grid bounds should be the size of the root window minus the shelf.
  const gfx::Rect root_window_bounds =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen();
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())->GetIdealBounds();
  const gfx::Rect expected_grid_bounds =
      SubtractRects(root_window_bounds, shelf_bounds);

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  // Verify that after dragging the unsnappable window to the left and right,
  // the window grid bounds do not change.
  auto* overview_item = GetOverviewItemForWindow(unsnappable_window.get());
  GetOverviewSession()->InitiateDrag(
      overview_item, overview_item->target_bounds().CenterPoint(),
      /*is_touch_dragging=*/false, /*event_source_item=*/overview_item);
  GetOverviewSession()->Drag(overview_item, gfx::PointF());
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  GetOverviewSession()->Drag(overview_item,
                             gfx::PointF(root_window_bounds.right(), 0.f));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  GetOverviewSession()->Drag(
      overview_item, gfx::PointF(root_window_bounds.right() / 2.f, 0.f));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
}

// Test that if an unsnappable window is dragged from overview to where another
// window is already snapped, then there is no snap preview, and if the drag
// ends there, then there is no DCHECK failure (or crash).
TEST_F(SplitViewOverviewSessionTest,
       DragUnsnappableWindowFromOverviewToSnappedWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  ASSERT_EQ(1u, GetOverviewSession()->grid_list().size());
  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  auto* overview_item =
      overview_grid->GetOverviewItemContaining(unsnappable_window.get());
  GetOverviewSession()->InitiateDrag(
      overview_item, overview_item->target_bounds().CenterPoint(),
      /*is_touch_dragging=*/false, /*event_source_item=*/overview_item);
  GetOverviewSession()->Drag(overview_item, gfx::PointF());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            overview_grid->split_view_drag_indicators()
                ->current_window_dragging_state());
  GetOverviewSession()->CompleteDrag(overview_item, gfx::PointF());
}

TEST_F(SplitViewOverviewSessionTest, Clipping) {
  // Helper to check if two rectangles have roughly the same aspect ratio. They
  // may be off by a bit due to insets but should have roughly the same shape.
  auto aspect_ratio_near = [](const gfx::Rect& rect1, const gfx::Rect& rect2) {
    DCHECK_GT(rect1.height(), 0);
    DCHECK_GT(rect2.height(), 0);
    constexpr float kEpsilon = 0.07f;
    const float rect1_aspect_ratio =
        static_cast<float>(rect1.width()) / rect1.height();
    const float rect2_aspect_ratio =
        static_cast<float>(rect2.width()) / rect2.height();
    return std::abs(rect2_aspect_ratio - rect1_aspect_ratio) < kEpsilon;
  };

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();  // Minimized.
  std::unique_ptr<aura::Window> window4 = CreateTestWindow();  // Has top inset.
  WindowState::Get(window3.get())->Minimize();
  window4->SetProperty(aura::client::kTopViewInset, 32);

  for (bool portrait : {false, true}) {
    SCOPED_TRACE(portrait ? "Portrait" : "Landscape");
    if (portrait) {
      ScreenOrientationControllerTestApi test_api(
          Shell::Get()->screen_orientation_controller());
      test_api.SetDisplayRotation(display::Display::ROTATE_90,
                                  display::Display::RotationSource::ACTIVE);
    }

    const gfx::Rect clipping1 = window1->layer()->clip_rect();
    const gfx::Rect clipping2 = window2->layer()->clip_rect();
    const gfx::Rect clipping3 = window3->layer()->clip_rect();
    const gfx::Rect clipping4 = window4->layer()->clip_rect();
    const gfx::Rect maximized_bounds =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            window1.get());
    const gfx::Rect split_view_bounds_right =
        split_view_controller()->GetSnappedWindowBoundsInScreen(
            SnapPosition::kSecondary,
            /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
            /*account_for_divider_width=*/true);

    ToggleOverview();

    // Tests that after entering overview, windows with no top inset and
    // minimized windows still have no clip.
    ASSERT_TRUE(GetOverviewController()->InOverviewSession());
    EXPECT_EQ(clipping1, window1->layer()->clip_rect());
    EXPECT_EQ(clipping2, window2->layer()->clip_rect());
    EXPECT_EQ(clipping3, window3->layer()->clip_rect());
    EXPECT_NE(clipping4, window4->layer()->clip_rect());
    const gfx::Rect overview_clipping4 = window4->layer()->clip_rect();

    auto* item1 = GetOverviewItemForWindow(window1.get());
    auto* item2 = GetOverviewItemForWindow(window2.get());
    auto* item3 = GetOverviewItemForWindow(window3.get());
    auto* item4 = GetOverviewItemForWindow(window4.get());
    GetOverviewSession()->InitiateDrag(
        item1, item1->target_bounds().CenterPoint(),
        /*is_touch_dragging=*/false, /*event_source_item=*/item1);

    // Tests that after we drag to a preview area, the items target bounds have
    // a matching aspect ratio to what the window would have if it were to be
    // snapped in splitview. The window clipping should match this, but the
    // windows regular bounds remain unchanged (maximized).
    GetOverviewSession()->Drag(item1, gfx::PointF());
    EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
              GetOverviewSession()
                  ->grid_list()[0]
                  ->split_view_drag_indicators()
                  ->current_window_dragging_state());
    EXPECT_FALSE(window2->layer()->clip_rect().IsEmpty());
    EXPECT_TRUE(aspect_ratio_near(window2->layer()->clip_rect(),
                                  split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item2->GetTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window2->GetBoundsInScreen(), maximized_bounds));

    // The actual window of a minimized window should not be clipped. The
    // clipped layer will be the WindowPreviewView of the associated
    // OverviewItemView.
    EXPECT_TRUE(window3->layer()->clip_rect().IsEmpty());
    ui::Layer* preview_layer = item3->GetLeafItemForWindow(window3.get())
                                   ->overview_item_view()
                                   ->preview_view()
                                   ->layer();
    EXPECT_FALSE(preview_layer->clip_rect().IsEmpty());
    EXPECT_FALSE(preview_layer->transform().IsIdentity());
    // The clip rect is affected by |preview_layer|'s transform so apply it.
    const gfx::Rect clip_rects3 =
        preview_layer->transform().MapRect(preview_layer->clip_rect());
    EXPECT_TRUE(aspect_ratio_near(clip_rects3, split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item3->GetTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window3->GetBoundsInScreen(), maximized_bounds));

    // A window with top view inset should be clipped, but with a new clipping
    // than the original overview clipping.
    EXPECT_FALSE(window4->layer()->clip_rect().IsEmpty());
    EXPECT_NE(overview_clipping4, window4->layer()->clip_rect());
    EXPECT_TRUE(aspect_ratio_near(window4->layer()->clip_rect(),
                                  split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item4->GetTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window4->GetBoundsInScreen(), maximized_bounds));

    // Tests that after snapping, the aspect ratios should be the same as being
    // in the preview area.
    GetOverviewSession()->CompleteDrag(item1, gfx::PointF());
    ASSERT_EQ(SplitViewController::State::kPrimarySnapped,
              split_view_controller()->state());
    EXPECT_FALSE(window2->layer()->clip_rect().IsEmpty());
    EXPECT_TRUE(aspect_ratio_near(window2->layer()->clip_rect(),
                                  split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item2->GetTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window2->GetBoundsInScreen(), maximized_bounds));

    EXPECT_TRUE(window3->layer()->clip_rect().IsEmpty());
    EXPECT_TRUE(aspect_ratio_near(clip_rects3, split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item3->GetTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window3->GetBoundsInScreen(), maximized_bounds));

    EXPECT_FALSE(window4->layer()->clip_rect().IsEmpty());
    EXPECT_NE(overview_clipping4, window4->layer()->clip_rect());
    EXPECT_TRUE(aspect_ratio_near(window4->layer()->clip_rect(),
                                  split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item4->GetTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window4->GetBoundsInScreen(), maximized_bounds));

    // Tests that the clipping is reset after exiting overview.
    EndSplitView();
    ToggleOverview();
    EXPECT_EQ(clipping1, window1->layer()->clip_rect());
    EXPECT_EQ(clipping2, window2->layer()->clip_rect());
    EXPECT_EQ(clipping3, window3->layer()->clip_rect());
    EXPECT_EQ(clipping4, window4->layer()->clip_rect());
  }
}

// Tests that when splitview is inactive, there is no need for aspect ratio
// changes, so there is no clipping on the overview windows. Regression test for
// crbug.com/1020440.
TEST_F(SplitViewOverviewSessionTest, NoClippingWhenSplitviewDisabled) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Splitview is disabled when ChromeVox is enabled.
  Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
      true, A11Y_NOTIFICATION_NONE);
  ASSERT_FALSE(ShouldAllowSplitView());
  const gfx::Rect clipping1 = window1->layer()->clip_rect();
  const gfx::Rect clipping2 = window2->layer()->clip_rect();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(clipping1, window1->layer()->clip_rect());
  EXPECT_EQ(clipping2, window2->layer()->clip_rect());

  // Drag to the edge of the screen. There should be no clipping and no crash.
  auto* item1 = GetOverviewItemForWindow(window1.get());
  GetOverviewSession()->InitiateDrag(
      item1, item1->target_bounds().CenterPoint(),
      /*is_touch_dragging=*/false, /*event_source_item=*/item1);
  GetOverviewSession()->Drag(item1, gfx::PointF());
  EXPECT_EQ(clipping1, window1->layer()->clip_rect());
  EXPECT_EQ(clipping2, window2->layer()->clip_rect());
}

// Tests that if there is only one window in the MRU window list in the overview
// mode, snapping the window to one side of the screen will not end the overview
// mode even if there is no more window left in the overview window grid.
TEST_F(SplitViewOverviewSessionTest, EmptyWindowsListNotExitOverview) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());

  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());

  // Test that overview mode is active in this single window case.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());

  // Create a new window should exit the overview mode.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  // If there are only 2 snapped windows, close one of them should enter
  // overview mode.
  window2.reset();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());

  // If there are more than 2 windows in overview
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window4.get());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  ToggleOverview();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  window3.reset();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  window4.reset();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());

  // Test that if there is only 1 snapped window, and no window in the overview
  // grid, ToggleOverview() can't end overview.
  ToggleOverview();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());

  EndSplitView();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());

  // Test that ToggleOverview() can end overview if we're not in split view
  // mode.
  ToggleOverview();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());

  // Now enter overview and split view again. Test that exiting tablet mode can
  // end split view and overview correctly.
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());

  // Test that closing all windows in overview can end overview if we're not in
  // split view mode.
  ToggleOverview();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  window1.reset();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
}

// Tests using Alt+[ on a maximized window.
TEST_F(SplitViewOverviewSessionTest, AltLeftSquareBracketOnMaximizedWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  wm::ActivateWindow(snapped_window.get());
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kMaximized, snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(InOverviewSession());
  const WindowSnapWMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_PRIMARY);
  snapped_window_state->OnWMEvent(&alt_left_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(InOverviewSession());
}

// Tests using Alt+] on a maximized window.
TEST_F(SplitViewOverviewSessionTest, AltRightSquareBracketOnMaximizedWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  wm::ActivateWindow(snapped_window.get());
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kMaximized, snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(InOverviewSession());
  const WindowSnapWMEvent alt_right_square_bracket(
      WM_EVENT_CYCLE_SNAP_SECONDARY);
  snapped_window_state->OnWMEvent(&alt_right_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->secondary_window());
  EXPECT_TRUE(InOverviewSession());
}

// Tests using Alt+[ and Alt+] on an unsnappable window.
TEST_F(SplitViewOverviewSessionTest, AltSquareBracketOnUnsnappableWindow) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();
  std::unique_ptr<aura::Window> other_window = CreateTestWindow();
  wm::ActivateWindow(unsnappable_window.get());
  WindowState* unsnappable_window_state =
      WindowState::Get(unsnappable_window.get());
  const auto expect_unsnappable_window_is_active_and_maximized =
      [this, &unsnappable_window, unsnappable_window_state]() {
        EXPECT_TRUE(wm::IsActiveWindow(unsnappable_window.get()));
        EXPECT_EQ(WindowStateType::kMaximized,
                  unsnappable_window_state->GetStateType());
        EXPECT_FALSE(split_view_controller()->InSplitViewMode());
        EXPECT_FALSE(InOverviewSession());
      };
  expect_unsnappable_window_is_active_and_maximized();
  const WindowSnapWMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_PRIMARY);
  unsnappable_window_state->OnWMEvent(&alt_left_square_bracket);
  expect_unsnappable_window_is_active_and_maximized();
  const WindowSnapWMEvent alt_right_square_bracket(
      WM_EVENT_CYCLE_SNAP_SECONDARY);
  unsnappable_window_state->OnWMEvent(&alt_right_square_bracket);
  expect_unsnappable_window_is_active_and_maximized();
}

// Tests using Alt+[ on a left snapped window, and Alt+] on a right snapped
// window.
TEST_F(SplitViewOverviewSessionTest, AltSquareBracketOnSameSideSnappedWindow) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  const auto test_unsnapping_window1 = [this,
                                        &window1](WMEventType event_type) {
    wm::ActivateWindow(window1.get());
    WindowState* window1_state = WindowState::Get(window1.get());
    const WindowSnapWMEvent event(event_type);
    window1_state->OnWMEvent(&event);
    EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
    EXPECT_EQ(WindowStateType::kMaximized, window1_state->GetStateType());
    EXPECT_FALSE(split_view_controller()->InSplitViewMode());
    EXPECT_FALSE(InOverviewSession());
  };
  // Test Alt+[ with active window snapped on left and overview on right.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_PRIMARY);
  // Test Alt+] with active window snapped on right and overview on left.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kSecondary);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_SECONDARY);
  // Test Alt+[ with active window snapped on left and other window snapped on
  // right, if the left window is the default snapped window.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_PRIMARY);
  // Test Alt+[ with active window snapped on left and other window snapped on
  // right, if the right window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_PRIMARY);
  // Test Alt+] with active window snapped on right and other window snapped on
  // left, if the left window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kSecondary);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_SECONDARY);
  // Test Alt+] with active window snapped on right and other window snapped on
  // left, if the right window is the default snapped window.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kSecondary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kPrimary);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_SECONDARY);
}

// Tests using Alt+[ on a right snapped window, and Alt+] on a left snapped
// window.
TEST_F(SplitViewOverviewSessionTest,
       AltSquareBracketOnOppositeSideSnappedWindow) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  const auto test_left_snapping_window1 = [this, &window1, &window2]() {
    wm::ActivateWindow(window1.get());
    WindowState* window1_state = WindowState::Get(window1.get());
    const WindowSnapWMEvent alt_left_square_bracket(
        WM_EVENT_CYCLE_SNAP_PRIMARY);
    window1_state->OnWMEvent(&alt_left_square_bracket);
    EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
    EXPECT_EQ(WindowStateType::kPrimarySnapped, window1_state->GetStateType());
    EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
              split_view_controller()->state());
    EXPECT_EQ(window1.get(), split_view_controller()->primary_window());
    ASSERT_TRUE(InOverviewSession());
    EXPECT_TRUE(GetOverviewItemForWindow(window2.get()));
  };
  const auto test_right_snapping_window1 = [this, &window1, &window2]() {
    wm::ActivateWindow(window1.get());
    WindowState* window1_state = WindowState::Get(window1.get());
    const WindowSnapWMEvent alt_right_square_bracket(
        WM_EVENT_CYCLE_SNAP_SECONDARY);
    window1_state->OnWMEvent(&alt_right_square_bracket);
    EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
    EXPECT_EQ(WindowStateType::kSecondarySnapped,
              window1_state->GetStateType());
    EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
              split_view_controller()->state());
    EXPECT_EQ(window1.get(), split_view_controller()->secondary_window());
    ASSERT_TRUE(InOverviewSession());
    EXPECT_TRUE(GetOverviewItemForWindow(window2.get()));
  };
  // Test Alt+[ with active window snapped on right and overview on left.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kSecondary);
  test_left_snapping_window1();
  // Test Alt+] with active window snapped on left and overview on right.
  test_right_snapping_window1();
  // Test Alt+[ with active window snapped on right and other window snapped on
  // left, if the right window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kPrimary);
  test_left_snapping_window1();
  // Test Alt+] with active window snapped on left and other window snapped on
  // right, if the left window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  test_right_snapping_window1();
  // Test Alt+[ with active window snapped on right and other window snapped on
  // left, if the left window is the default snapped window.
  EndSplitView();
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kSecondary);
  test_left_snapping_window1();
  // Test Alt+] with active window snapped on left and other window snapped on
  // right, if the right window is the default snapped window.
  EndSplitView();
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  test_right_snapping_window1();
}

// Test the overview window drag functionalities when screen rotates.
TEST_F(SplitViewOverviewSessionTest, SplitViewRotationTest) {
  UpdateDisplay("807x407");
  int64_t display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display::test::ScopedSetInternalDisplayId set_internal(display_manager,
                                                         display_id);
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());

  // Set the screen orientation to LANDSCAPE_PRIMARY.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapePrimary);

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Test that dragging |window1| to the left of the screen snaps it to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to right.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  gfx::Rect work_area_rect = GetWorkAreaInScreen(window2.get());
  gfx::PointF end_location2(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());

  // Test that |left_window_| was snapped to left after rotated 0 degree.
  gfx::Rect left_window_bounds =
      split_view_controller()->primary_window()->GetBoundsInScreen();
  EXPECT_EQ(left_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(left_window_bounds.y(), work_area_rect.y());
  EndSplitView();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitPrimary);
  ToggleOverview();

  // Test that dragging |window1| to the top of the screen snaps it to left.
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to right.
  overview_item2 = GetOverviewItemForWindow(window2.get());
  work_area_rect = GetWorkAreaInScreen(window2.get());
  end_location2 = gfx::PointF(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());

  // Test that |left_window_| was snapped to top after rotated 270 degree.
  left_window_bounds =
      split_view_controller()->primary_window()->GetBoundsInScreen();
  EXPECT_EQ(left_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(left_window_bounds.y(), work_area_rect.y());
  EndSplitView();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kLandscapeSecondary);
  ToggleOverview();

  // Test that dragging |window1| to the left of the screen snaps it to right.
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kSecondarySnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to left.
  overview_item2 = GetOverviewItemForWindow(window2.get());
  work_area_rect = GetWorkAreaInScreen(window2.get());
  end_location2 = gfx::PointF(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window2.get());

  // Test that |right_window_| was snapped to left after rotated 180 degree.
  gfx::Rect right_window_bounds =
      split_view_controller()->secondary_window()->GetBoundsInScreen();
  EXPECT_EQ(right_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(right_window_bounds.y(), work_area_rect.y());
  EndSplitView();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            chromeos::OrientationType::kPortraitSecondary);
  ToggleOverview();

  // Test that dragging |window1| to the top of the screen snaps it to right.
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kSecondarySnapped);
  EXPECT_EQ(split_view_controller()->secondary_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to left.
  overview_item2 = GetOverviewItemForWindow(window2.get());
  work_area_rect = GetWorkAreaInScreen(window2.get());
  end_location2 = gfx::PointF(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window2.get());

  // Test that |right_window_| was snapped to top after rotated 90 degree.
  right_window_bounds =
      split_view_controller()->secondary_window()->GetBoundsInScreen();
  EXPECT_EQ(right_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(right_window_bounds.y(), work_area_rect.y());
  EndSplitView();
}

// Test that when split view mode and overview mode are both active at the same
// time, dragging the split view divider resizes the bounds of snapped window
// and the bounds of overview window grids at the same time.
TEST_F(SplitViewOverviewSessionTest, SplitViewOverviewBothActiveTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  const gfx::Rect window1_bounds = window1->GetBoundsInScreen();
  const gfx::Rect overview_grid_bounds = GetGridBounds();
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(false /* is_dragging */);

  // Test that window1, divider, overview grid are aligned horizontally.
  EXPECT_EQ(window1_bounds.right(), divider_bounds.x());
  EXPECT_EQ(divider_bounds.right(), overview_grid_bounds.x());

  const gfx::Point resize_start_location(divider_bounds.CenterPoint());
  split_view_divider()->StartResizeWithDivider(resize_start_location);
  const gfx::Point resize_end_location(300, 0);
  split_view_divider()->EndResizeWithDivider(resize_end_location);
  SkipDividerSnapAnimation();

  const gfx::Rect window1_bounds_after_resize = window1->GetBoundsInScreen();
  const gfx::Rect overview_grid_bounds_after_resize = GetGridBounds();
  const gfx::Rect divider_bounds_after_resize =
      GetSplitViewDividerBounds(false /* is_dragging */);

  // Test that window1, divider, overview grid are still aligned horizontally
  // after resizing.
  EXPECT_EQ(window1_bounds.right(), divider_bounds.x());
  EXPECT_EQ(divider_bounds.right(), overview_grid_bounds.x());

  // Test that window1, divider, overview grid's bounds are changed after
  // resizing.
  EXPECT_NE(window1_bounds, window1_bounds_after_resize);
  EXPECT_NE(overview_grid_bounds, overview_grid_bounds_after_resize);
  EXPECT_NE(divider_bounds, divider_bounds_after_resize);
}

// Verify that selecting an unsnappable window while in split view works as
// intended.
TEST_F(SplitViewOverviewSessionTest, SelectUnsnappableWindowInSplitView) {
  // Create one snappable and one unsnappable window.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  // Snap the snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Select the unsnappable window.
  auto* overview_item = GetOverviewItemForWindow(unsnappable_window.get());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  generator->ClickLeftButton();

  // Verify that we are out of split view and overview mode, and that the active
  // window is the unsnappable window.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(unsnappable_window.get(), window_util::GetActiveWindow());

  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);

  // Split view mode should be active. Overview mode should be ended.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());

  ToggleOverview();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());

  // Now select the unsnappable window.
  overview_item = GetOverviewItemForWindow(unsnappable_window.get());
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  generator->ClickLeftButton();

  // Split view mode should be ended. And the unsnappable window should be the
  // active window now.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(unsnappable_window.get(), window_util::GetActiveWindow());
}

// Verify that when in overview mode, the selector items unsnappable indicator
// shows up when expected.
TEST_F(SplitViewOverviewSessionTest, OverviewUnsnappableIndicatorVisibility) {
  // Create three windows; two normal and one unsnappable, so that when after
  // snapping |window1| to enter split view we can test the state of each normal
  // and unsnappable windows.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  auto* snappable_overview_item = GetOverviewItemForWindow(window2.get());
  auto* unsnappable_overview_item =
      GetOverviewItemForWindow(unsnappable_window.get());

  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  EXPECT_FALSE(GetCannotSnapWidget(snappable_overview_item));
  ASSERT_FALSE(GetCannotSnapWidget(unsnappable_overview_item));

  // Snap the extra snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(GetCannotSnapWidget(snappable_overview_item));
  views::Widget* cannot_snap_widget =
      GetCannotSnapWidget(unsnappable_overview_item);
  ASSERT_TRUE(cannot_snap_widget);
  EXPECT_EQ(1.f, cannot_snap_widget->GetLayer()->opacity());

  // Exiting the splitview will hide the unsnappable label.
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(/*is_dragging=*/false);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(0, 0);
  SkipDividerSnapAnimation();

  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(0.f, cannot_snap_widget->GetLayer()->opacity());
}

// Verify that during "normal" dragging from overview (not drag-to-close), the
// dragged item's unsnappable indicator is temporarily suppressed.
TEST_F(SplitViewOverviewSessionTest,
       OverviewUnsnappableIndicatorVisibilityWhileDragging) {
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_short_press_time(base::Milliseconds(1));
  gesture_config->set_show_press_delay_in_ms(1);

  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();
  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  auto* unsnappable_overview_item =
      GetOverviewItemForWindow(unsnappable_window.get());
  views::Widget* cannot_snap_widget =
      GetCannotSnapWidget(unsnappable_overview_item);
  ASSERT_TRUE(cannot_snap_widget);
  ui::Layer* unsnappable_layer = cannot_snap_widget->GetLayer();
  ASSERT_EQ(1.f, unsnappable_layer->opacity());

  // Test that the unsnappable label is temporarily suppressed during mouse
  // dragging.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Point drag_starting_point = gfx::ToRoundedPoint(
      unsnappable_overview_item->target_bounds().CenterPoint());
  generator->set_current_screen_location(drag_starting_point);
  generator->PressLeftButton();
  using DragBehavior = OverviewWindowDragController::DragBehavior;
  EXPECT_EQ(DragBehavior::kUndefined,
            GetOverviewSession()
                ->window_drag_controller()
                ->current_drag_behavior_for_testing());
  EXPECT_EQ(1.f, unsnappable_layer->opacity());
  generator->MoveMouseBy(0, 20);
  EXPECT_EQ(DragBehavior::kNormalDrag,
            GetOverviewSession()
                ->window_drag_controller()
                ->current_drag_behavior_for_testing());
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
  generator->ReleaseLeftButton();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Test that the unsnappable label is temporarily suppressed during "normal"
  // touch dragging (not drag-to-close).
  generator->set_current_screen_location(drag_starting_point);
  generator->PressTouch();
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
    run_loop.Run();
  }
  EXPECT_EQ(DragBehavior::kNormalDrag,
            GetOverviewSession()
                ->window_drag_controller()
                ->current_drag_behavior_for_testing());
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
  generator->MoveTouchBy(20, 0);
  generator->ReleaseTouch();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Test that the unsnappable label reappears if "normal" touch dragging (not
  // drag-to-close) ends when the item has not been actually dragged anywhere.
  // This case improves test coverage because it is handled in
  // |OverviewWindowDragController::ResetGesture| instead of
  // |OverviewWindowDragController::CompleteNormalDrag|.
  generator->set_current_screen_location(drag_starting_point);
  generator->PressTouch();
  {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(2));
    run_loop.Run();
  }
  EXPECT_EQ(DragBehavior::kNormalDrag,
            GetOverviewSession()
                ->window_drag_controller()
                ->current_drag_behavior_for_testing());
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
  generator->ReleaseTouch();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Test that the unsnappable label persists in drag-to-close mode.
  generator->set_current_screen_location(drag_starting_point);
  generator->PressTouch();
  // Use small increments otherwise a fling event will be fired.
  for (int j = 0; j < 20; ++j)
    generator->MoveTouchBy(0, 1);
  EXPECT_EQ(DragBehavior::kDragToClose,
            GetOverviewSession()
                ->window_drag_controller()
                ->current_drag_behavior_for_testing());
  // Drag-to-close mode affects the opacity of the whole overview item,
  // including the unsnappable label.
  EXPECT_EQ(unsnappable_overview_item->GetWindow()->layer()->opacity(),
            unsnappable_layer->opacity());
  generator->ReleaseTouch();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());
}

// Verify that an item's unsnappable indicator is updated for display rotation.
TEST_F(SplitViewOverviewSessionTest,
       OverviewUnsnappableIndicatorVisibilityAfterDisplayRotation) {
  UpdateDisplay("900x800");
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  // Because of its minimum size, |overview_window| is snappable in horizontal
  // split view but not in vertical split view.
  std::unique_ptr<aura::Window> overview_window(
      CreateWindowWithMinimumSize(gfx::Rect(400, 600), gfx::Size(300, 500)));
  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  auto* overview_item = GetOverviewItemForWindow(overview_window.get());
  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  EXPECT_FALSE(GetCannotSnapWidget(overview_item));

  // Rotate to primary portrait orientation. The unsnappable indicator appears.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  views::Widget* cannot_snap_widget = GetCannotSnapWidget(overview_item);
  ASSERT_TRUE(cannot_snap_widget);
  ui::Layer* unsnappable_layer = cannot_snap_widget->GetLayer();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Rotate to primary landscape orientation. The unsnappable indicator hides.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
}

// Test that when splitview mode and overview mode are both active at the same
// time, dragging divider behaviors are correct.
TEST_F(SplitViewOverviewSessionTest, DragDividerToExitTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Drag the divider toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_divider()->StartResizeWithDivider(divider_bounds.CenterPoint());
  split_view_divider()->EndResizeWithDivider(gfx::Point(0, 0));
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Now drag |window2| selector item to snap to left.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF());
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Drag the divider toward closing the overview window grid.
  divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  const gfx::Rect display_bounds = GetWorkAreaInScreen(window2.get());
  split_view_divider()->StartResizeWithDivider(divider_bounds.CenterPoint());
  split_view_divider()->EndResizeWithDivider(display_bounds.bottom_right());
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is also ended. |window2|
  // should be activated.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(OverviewController::Get()->InOverviewSession());
  EXPECT_EQ(window2.get(), window_util::GetActiveWindow());
}

TEST_F(SplitViewOverviewSessionTest, OverviewItemLongPressed) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  auto* overview_item = GetOverviewItemForWindow(window1.get());
  gfx::PointF start_location(overview_item->target_bounds().CenterPoint());
  const gfx::RectF original_bounds(overview_item->target_bounds());

  // Verify that when a overview item receives a resetting gesture, we
  // stay in overview mode and the bounds of the item are the same as they were
  // before the press sequence started.
  GetOverviewSession()->InitiateDrag(overview_item, start_location,
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/overview_item);
  GetOverviewSession()->ResetDraggedWindowGesture();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(original_bounds, overview_item->target_bounds());

  // Verify that when a overview item is tapped, we exit overview mode,
  // and the current active window is the item.
  GetOverviewSession()->InitiateDrag(overview_item, start_location,
                                     /*is_touch_dragging=*/true,
                                     /*event_source_item=*/overview_item);
  GetOverviewSession()->ActivateDraggedWindow();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());
}

TEST_F(SplitViewOverviewSessionTest, SnappedWindowBoundsTest) {
  const gfx::Rect bounds(400, 400);
  const int kMinimumBoundSize = 100;
  const gfx::Size size(kMinimumBoundSize, kMinimumBoundSize);

  std::unique_ptr<aura::Window> window1(
      CreateWindowWithMinimumSize(bounds, size));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithMinimumSize(bounds, size));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithMinimumSize(bounds, size));
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Then drag the divider to left toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  split_view_divider()->StartResizeWithDivider(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  split_view_divider()->EndResizeWithDivider(gfx::Point(20, 20));
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  // Test that |window1| has the dimensions of a tablet mode maxed window, so
  // that when it is placed back on the grid it will not look skinny.
  EXPECT_LE(window1->bounds().x(), 0);
  EXPECT_EQ(window1->bounds().width(), screen_width);

  // Drag |window2| selector item to snap to right.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Rect work_area_rect = GetWorkAreaInScreen(window2.get());
  gfx::Point end_location2 =
      gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, gfx::PointF(end_location2));
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller()->state());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());

  // Then drag the divider to right toward closing the snapped window.
  divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_divider()->StartResizeWithDivider(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  end_location2.Offset(-20, -20);
  split_view_divider()->EndResizeWithDivider(end_location2);
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  // Test that |window2| has the dimensions of a tablet mode maxed window, so
  // that when it is placed back on the grid it will not look skinny.
  EXPECT_GE(window2->bounds().x(), 0);
  EXPECT_EQ(window2->bounds().width(), screen_width);
}

TEST_F(SplitViewOverviewSessionTest, ResizePastFixedDividerPositions) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));

  // Start overview and drag to snap `window1` in split view.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  const gfx::Point start_point(window1->GetBoundsInScreen().right_center());
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(start_point);

  // Resize the window to less than 1/3 of the work area.
  const int work_area_length(GetWorkAreaInScreen(window1.get()).width());
  int window_length = 200;
  ASSERT_LT(window_length, work_area_length * chromeos::kOneThirdSnapRatio);
  split_view_divider()->StartResizeWithDivider(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint());
  split_view_divider()->EndResizeWithDivider(
      gfx::Point(window_length, start_point.y()));

  // We remain in overview and the divider will be animated to 1/2.
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  SkipDividerSnapAnimation();
  EXPECT_NEAR(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint().x(),
      work_area_length * chromeos::kOneThirdSnapRatio, 1.f);

  // Resize the window to greater than 2/3 of the work area.
  window_length = 600;
  ASSERT_GT(window_length, work_area_length * chromeos::kTwoThirdSnapRatio);
  split_view_divider()->StartResizeWithDivider(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint());
  split_view_divider()->EndResizeWithDivider(
      gfx::Point(window_length, start_point.y()));

  // We remain in overview and the divider will be animated to 1/2.
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  SkipDividerSnapAnimation();
  EXPECT_NEAR(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint().x(),
      work_area_length * chromeos::kTwoThirdSnapRatio, 1.f);
}

// Test snapped window bounds with adjustment for the minimum size of a window.
TEST_F(SplitViewOverviewSessionTest, SnappedWindowBoundsWithMinimumSizeTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  std::unique_ptr<aura::Window> window2(CreateWindowWithMinimumSize(
      bounds, gfx::Size(work_area.width() / 3 + 20, 0)));

  // Snap `window1` in split view, then resize it to 1/3 the work area, which is
  // less than the minimum size of `window2`.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  split_view_divider()->StartResizeWithDivider(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint());
  split_view_divider()->EndResizeWithDivider(
      gfx::Point(work_area.width() / 3, 10));
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  // Use `EXPECT_NEAR` for reasons related to rounding and divider thickness.
  constexpr int kDividerWidth = kSplitviewDividerShortSideLength;
  ASSERT_NEAR(work_area.width() / 3, window1->GetBoundsInScreen().width(),
              kDividerWidth);

  // Long press to start a drag on `item2` to the left, on top of `window1`, to
  // show the left highlight preview.
  auto* item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Point drag_starting_point(
      gfx::ToRoundedPoint(item2->GetTransformedBounds().CenterPoint()));
  ui::test::EventGenerator* generator = GetEventGenerator();
  LongGestureTap(drag_starting_point, generator, /*release_touch=*/false);
  DragItemToPoint(item2, gfx::Point(0, 0), generator,
                  /*by_touch_gestures=*/true, /*drop=*/false);
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Test that the highlight bounds are 1/2 the work area, since that's the
  // closest fixed divider ratio for `window2`.
  gfx::Rect left_highlight_bounds(work_area);
  left_highlight_bounds.set_width(work_area.width() / 2 - kDividerWidth / 2);
  left_highlight_bounds.Inset(kHighlightScreenEdgePaddingDp);
  auto* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(window1->GetRootWindow());
  EXPECT_EQ(left_highlight_bounds, overview_grid->split_view_drag_indicators()
                                       ->GetLeftHighlightViewBounds());

  // Drop `item2` back at its starting point.
  generator->MoveTouch(drag_starting_point);
  generator->ReleaseTouch();

  // Now resize `window1` where `window2` can't fit in the secondary position.
  split_view_divider()->StartResizeWithDivider(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint());
  split_view_divider()->EndResizeWithDivider(
      gfx::Point(work_area.width() * 2 / 3, 10));
  ASSERT_NEAR(work_area.width() * 2 / 3, window1->GetBoundsInScreen().width(),
              kDividerWidth);

  // Drag `window2` to show the right highlight preview.
  DragItemToPoint(item2, work_area.top_right(), generator,
                  /*by_touch_gestures=*/false, /*drop=*/false);

  // Test that the highlight bounds are 1/2 the work area.
  gfx::Rect right_highlight_bounds(work_area);
  right_highlight_bounds.set_x(work_area.width() / 2 + kDividerWidth / 2);
  right_highlight_bounds.set_width(work_area.width() / 2 - kDividerWidth / 2);
  right_highlight_bounds.Inset(kHighlightScreenEdgePaddingDp);
  EXPECT_EQ(right_highlight_bounds,
            overview_grid->split_view_drag_indicators()
                ->GetRightHighlightViewBoundsForTesting());
  generator->ReleaseTouch();
}

// Verify that if the split view divider is dragged all the way to the edge, the
// window being dragged gets returned to the overview list, if overview mode is
// still active.
TEST_F(SplitViewOverviewSessionTest,
       DividerDraggedToEdgeReturnsWindowToOverviewList) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  // Drag |window1| selector item to snap to left. There should be two items on
  // the overview grid afterwards, |window2| and |window3|.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(split_view_controller()->split_view_divider()->divider_widget());
  const std::vector<aura::Window*> window_list =
      GetWindowsListInOverviewGrids();
  EXPECT_EQ(2u, window_list.size());
  EXPECT_FALSE(base::Contains(window_list, window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Drag the divider to the left edge.
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(/*is_dragging=*/false);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(0, 0);
  SkipDividerSnapAnimation();

  // Verify that it is still in overview mode and that |window1| is returned to
  // the overview list.
  EXPECT_TRUE(InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  const std::vector<aura::Window*> new_window_list =
      GetWindowsListInOverviewGrids();
  EXPECT_EQ(3u, new_window_list.size());
  EXPECT_TRUE(base::Contains(new_window_list, window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
}

// Verify that if overview mode is active and the split view divider is dragged
// all the way to the opposite edge, then the split view window is reinserted
// into the overview grid at the correct position according to MRU order, and
// the stacking order is also correct.
TEST_F(
    SplitViewOverviewSessionTest,
    SplitViewWindowReinsertedToOverviewAtCorrectPositionWhenSplitViewIsEnded) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(window1.get()), gfx::PointF());
  DragWindowTo(GetOverviewItemForWindow(window2.get()),
               gfx::PointF(799.f, 0.f));
  EXPECT_EQ(window1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(window2.get(), split_view_controller()->secondary_window());
  ToggleOverview();
  // Drag the divider to the left edge.
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(/*is_dragging=*/false);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(0, 0);
  SkipDividerSnapAnimation();

  // Verify the grid arrangement.
  ASSERT_TRUE(InOverviewSession());
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

// Verify that if a window is dragged from overview and snapped in place of
// another split view window, then the old split view window is reinserted into
// the overview grid at the correct position according to MRU order, and the
// stacking order is also correct.
TEST_F(
    SplitViewOverviewSessionTest,
    SplitViewWindowReinsertedToOverviewAtCorrectPositionWhenAnotherWindowTakesItsPlace) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(window1.get()), gfx::PointF());
  DragWindowTo(GetOverviewItemForWindow(window2.get()),
               gfx::PointF(799.f, 0.f));
  EXPECT_EQ(window1.get(), split_view_controller()->primary_window());
  EXPECT_EQ(window2.get(), split_view_controller()->secondary_window());
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(window3.get()), gfx::PointF());
  EXPECT_EQ(window3.get(), split_view_controller()->primary_window());

  // Verify the grid arrangement.
  ASSERT_TRUE(InOverviewSession());
  const std::vector<raw_ptr<aura::Window, VectorExperimental>>
      expected_mru_list = {window3.get(), window2.get(), window1.get(),
                           window4.get()};
  const std::vector<aura::Window*> expected_overview_list = {
      window2.get(), window1.get(), window4.get()};
  EXPECT_EQ(
      expected_mru_list,
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_EQ(expected_overview_list, GetWindowsListInOverviewGrids());

  // Verify the stacking order.
  aura::Window* parent = window1->parent();
  ASSERT_EQ(parent, window2->parent());
  ASSERT_EQ(parent, window4->parent());
  EXPECT_TRUE(window_util::IsStackedBelow(
      GetOverviewItemForWindow(window1.get())->item_widget()->GetNativeWindow(),
      GetOverviewItemForWindow(window2.get())
          ->item_widget()
          ->GetNativeWindow()));
  EXPECT_TRUE(window_util::IsStackedBelow(
      GetOverviewItemForWindow(window4.get())->item_widget()->GetNativeWindow(),
      GetOverviewItemForWindow(window1.get())
          ->item_widget()
          ->GetNativeWindow()));
}

// Verify that if the split view divider is dragged close to the edge, the grid
// bounds will be fixed to a third of the work area width and start sliding off
// the screen instead of continuing to shrink.
TEST_F(SplitViewOverviewSessionTest,
       OverviewHasMinimumBoundsWhenDividerDragged) {
  UpdateDisplay("600x400");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Snap a window to the left and test dragging the divider towards the right
  // edge of the screen.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  OverviewGrid* grid = GetOverviewSession()->grid_list()[0].get();
  ASSERT_TRUE(grid);

  // Drag the divider to the right edge.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(/*is_dragging=*/false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->PressLeftButton();

  // Tests that near the right edge, the grid bounds are fixed at 200 and are
  // partially off screen to the right. Drag with at least 2 steps to
  // simulate a real mouse drag movement.
  generator->MoveMouseTo(gfx::Point(580, 0), /*count=*/2);
  gfx::Rect grid_bounds = OverviewGridTestApi(grid).bounds();
  EXPECT_EQ(200, grid_bounds.width());
  EXPECT_GT(grid_bounds.right(), 600);
  generator->ReleaseLeftButton();
  SkipDividerSnapAnimation();

  // Releasing close to the edge will activate the left window and exit
  // overview.
  ASSERT_FALSE(InOverviewSession());
  ToggleOverview();
  // Snap a window to the right and test dragging the divider towards the left
  // edge of the screen.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kSecondary);
  grid = GetOverviewSession()->grid_list()[0].get();
  ASSERT_TRUE(grid);

  // Drag the divider to the left edge.
  divider_bounds = GetSplitViewDividerBounds(/*is_dragging=*/false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->PressLeftButton();

  // Drag with at least 2 steps to simulate a real mouse drag movement.
  generator->MoveMouseTo(gfx::Point(20, 0), /*count=*/2);
  // Tests that near the left edge, the grid bounds are fixed at 200 and are
  // partially off screen to the left.
  grid_bounds = OverviewGridTestApi(grid).bounds();
  EXPECT_EQ(200, grid_bounds.width());
  EXPECT_LT(grid_bounds.x(), 0);
  generator->ReleaseLeftButton();
  SkipDividerSnapAnimation();
}

// Test that when splitview mode is active, minimizing one of the snapped window
// will insert the minimized window back to overview mode if overview mode is
// active at the moment.
TEST_F(SplitViewOverviewSessionTest, InsertMinimizedWindowBackToOverview) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_TRUE(InOverviewSession());

  // Minimize |window1| will put |window1| back to overview grid.
  WindowState::Get(window1.get())->Minimize();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(GetOverviewItemForWindow(window1.get()));

  // Now snap both |window1| and |window2|.
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->primary_window(), window1.get());
  EXPECT_EQ(split_view_controller()->secondary_window(), window2.get());

  // Minimize |window1| will open overview and put |window1| to overview grid.
  WindowState::Get(window1.get())->Minimize();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kSecondarySnapped);
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(GetOverviewItemForWindow(window1.get()));

  // Minimize |window2| also put |window2| to overview grid.
  WindowState::Get(window2.get())->Minimize();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(GetOverviewItemForWindow(window1.get()));
  EXPECT_TRUE(GetOverviewItemForWindow(window2.get()));
}

// Test that when splitview and overview are both active at the same time, if
// overview is ended due to snapping a window in splitview, the tranform of each
// window in the overview grid is restored.
TEST_F(SplitViewOverviewSessionTest, SnappedWindowAnimationObserverTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  // There are four ways to exit overview mode. Verify in each case the
  // tranform of each window in the overview window grid has been restored.

  // 1. Overview is ended by dragging a item in overview to snap to splitview.
  // Drag |window1| selector item to snap to left. There should be two items on
  // the overview grid afterwards, |window2| and |window3|.
  ToggleOverview();
  EXPECT_FALSE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  // Drag |window2| to snap to right.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Rect work_area_rect =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window2.get());
  const gfx::PointF end_location2(work_area_rect.width(), 0);
  DragWindowTo(overview_item2, end_location2);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());

  // 2. Overview is ended by ToggleOverview() directly.
  // ToggleOverview() will open overview grid in the non-default side of the
  // split screen.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  // ToggleOverview() directly.
  ToggleOverview();
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());

  // 3. Overview is ended by actviating an existing window.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  wm::ActivateWindow(window2.get());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());

  // 4. Overview is ended by activating a new window.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  wm::ActivateWindow(window4.get());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window4->layer()->GetTargetTransform().IsIdentity());
}

// Test that when split view and overview are both active at the same time,
// double tapping on the divider can swap the window's position with the
// overview window grid's postion.
TEST_F(SplitViewOverviewSessionTest, SwapWindowAndOverviewGrid) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateAppWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateAppWindow(bounds));

  ToggleOverview();
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SnapPosition::kPrimary);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  // Test that the grid bounds are approximately equal to the bounds of a
  // snapped window (minus hotseat insets on the grid).
  EXPECT_EQ(
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SnapPosition::kSecondary,
          /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
          /*account_for_divider_width=*/true),
      GetGridBounds());

  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kSecondarySnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SnapPosition::kSecondary);
  EXPECT_EQ(
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SnapPosition::kPrimary,
          /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
          /*account_for_divider_width=*/true),
      GetGridBounds());
}

// Test that in tablet mode, pressing tab key in overview should not crash.
TEST_F(SplitViewOverviewSessionTest, NoCrashWhenPressTabKey) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(400, 400)));
  std::unique_ptr<aura::Window> window2(CreateWindow(gfx::Rect(400, 400)));

  // In overview, there should be no crash when pressing tab key.
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());
  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(InOverviewSession());

  // When splitview and overview are both active, there should be no crash when
  // pressing tab key.
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  PressAndReleaseKey(ui::VKEY_TAB);
  EXPECT_TRUE(InOverviewSession());
}

// Tests closing a snapped window while in overview mode.
TEST_F(SplitViewOverviewSessionTest, ClosingSplitViewWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Now close the snapped |window1|. We should remain in overview mode and the
  // overview focus window should regain focus.
  window1.reset();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(GetOverviewSession()->GetOverviewFocusWindow(),
            window_util::GetFocusedWindow());
}

// Test that you cannot drag from overview during the split view divider
// animation.
TEST_F(SplitViewOverviewSessionTest,
       CannotDragFromOverviewDuringSplitViewDividerAnimation) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);

  gfx::Point divider_drag_point =
      split_view_controller()
          ->split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  split_view_divider()->StartResizeWithDivider(divider_drag_point);
  divider_drag_point.Offset(20, 0);
  split_view_divider()->ResizeWithDivider(divider_drag_point);
  split_view_divider()->EndResizeWithDivider(divider_drag_point);
  ASSERT_TRUE(IsDividerAnimating());

  auto* overview_item = GetOverviewItemForWindow(overview_window.get());
  GetOverviewSession()->InitiateDrag(
      overview_item, overview_item->target_bounds().CenterPoint(),
      /*is_touch_dragging=*/true, /*event_source_item=*/overview_item);
  EXPECT_FALSE(overview_item->IsDragItem());
}

// Tests that a window which is dragged to a splitview zone is destroyed, the
// grid bounds return to a non-splitview bounds.
TEST_F(SplitViewOverviewSessionTest, GridBoundsAfterWindowDestroyed) {
  // Create two windows otherwise we exit overview after one window is
  // destroyed.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  const gfx::Rect grid_bounds = GetGridBounds();
  // Drag the item such that the splitview preview area shows up and the grid
  // bounds shrink.
  auto* overview_item = GetOverviewItemForWindow(window1.get());
  GetOverviewSession()->InitiateDrag(
      overview_item, overview_item->target_bounds().CenterPoint(),
      /*is_touch_dragging=*/true, /*event_source_item=*/overview_item);
  GetOverviewSession()->Drag(overview_item, gfx::PointF(1.f, 1.f));
  EXPECT_NE(grid_bounds, GetGridBounds());

  // Tests that when the dragged window is destroyed, the grid bounds return to
  // their normal size.
  window1.reset();
  EXPECT_EQ(grid_bounds, GetGridBounds());
}

// Tests that overview stays active if we have a snapped window.
TEST_F(SplitViewOverviewSessionTest, OnScreenLock) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Overview should exit if no snapped window after locking/unlocking.
  ToggleOverview();
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  ASSERT_FALSE(InOverviewSession());

  ToggleOverview();
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kPrimary);

  // Lock and unlock the machine. Test that we are still in overview and
  // splitview.
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(InOverviewSession());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
}

// Verify that selecting an minimized snappable window while in split view
// triggers auto snapping.
TEST_F(SplitViewOverviewSessionTest,
       SelectMinimizedSnappableWindowInSplitView) {
  // Create two snappable windows.
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> minimized_window = CreateTestWindow();
  WindowState::Get(minimized_window.get())->Minimize();

  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());

  // Snap a window to enter split view mode.
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());

  // Select the minimized window.
  auto* overview_item = GetOverviewItemForWindow(minimized_window.get());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  generator->ClickLeftButton();

  // Verify that both windows are in a snapped state and overview mode is ended.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(
      split_view_controller()->IsWindowInSplitView(snapped_window.get()));
  EXPECT_EQ(
      split_view_controller()->GetPositionOfSnappedWindow(snapped_window.get()),
      SnapPosition::kPrimary);
  EXPECT_TRUE(
      split_view_controller()->IsWindowInSplitView(minimized_window.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(
                minimized_window.get()),
            SnapPosition::kSecondary);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_EQ(minimized_window.get(), window_util::GetActiveWindow());
}

// Verify no crash (or DCHECK failure) if you exit and re-enter mirror mode
// while in tablet split view with empty overview.
TEST_F(SplitViewOverviewSessionTest,
       ExitAndReenterMirrorModeWithEmptyOverview) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, std::nullopt);
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, std::nullopt);
}

// Tests that there is no crash when dragging the divider in portrait mode.
// Regression test for https://crbug.com/1267486.
TEST_F(SplitViewOverviewSessionTest, NoCrashWhenDraggingDividerInPortrait) {
  // The crash only occured in portrait mode.
  UpdateDisplay("600x800");
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  // Note that this snaps `window1` to the top.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);

  // Drag the divider all the way to the bottom. There should be no crash.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      split_view_controller()
          ->split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint());
  generator->PressTouch();
  generator->MoveTouchBy(0, 600);
  generator->ReleaseTouch();
}

// Tests that in tablet mode, after minimizing and unminimizng a snapped window,
// it is visible to the user. Regression test for b/267391123.
TEST_F(SplitViewOverviewSessionTest, WindowVisibleAfterMinimizeUnminimize) {
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  auto* window_state = WindowState::Get(window.get());

  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(InOverviewSession());
  ASSERT_FALSE(GetOverviewItemForWindow(window.get()));

  window_state->Minimize();
  ASSERT_TRUE(InOverviewSession());
  ASSERT_TRUE(GetOverviewItemForWindow(window.get()));

  window->Show();
  wm::ActivateWindow(window.get());
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
}

// Tests the divider gains and loses activation in tablet mode.
TEST_F(SplitViewOverviewSessionTest, KeyboardFocus) {
  std::unique_ptr<aura::Window> window = CreateAppWindow();
  split_view_controller()->SnapWindow(window.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(InOverviewSession());

  auto* divider_widget =
      split_view_controller()->split_view_divider()->divider_widget();
  EXPECT_FALSE(divider_widget->IsActive());

  // Test the divider gains activation.
  while (!divider_widget->IsActive()) {
    PressAndReleaseKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  }

  // Test the divider loses activation.
  PressAndReleaseKey(ui::VKEY_BROWSER_FORWARD, ui::EF_CONTROL_DOWN);
  EXPECT_FALSE(divider_widget->IsActive());
}

// Test the split view and overview functionalities in clamshell mode. Split
// view is only active when overview is active in clamshell mode.
class SplitViewOverviewSessionInClamshellTest
    : public SplitViewOverviewSessionTest {
 public:
  SplitViewOverviewSessionInClamshellTest() = default;

  SplitViewOverviewSessionInClamshellTest(
      const SplitViewOverviewSessionInClamshellTest&) = delete;
  SplitViewOverviewSessionInClamshellTest& operator=(
      const SplitViewOverviewSessionInClamshellTest&) = delete;

  ~SplitViewOverviewSessionInClamshellTest() override = default;

  // AshTestBase:
  void SetUp() override {
    SplitViewOverviewSessionTest::SetUp();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
    DCHECK(ShouldAllowSplitView());
  }

  aura::Window* CreateWindowWithHitTestComponent(int hit_test_component,
                                                 const gfx::Rect& bounds) {
    return CreateTestWindowInShellWithDelegate(
        new TestWindowHitTestDelegate(hit_test_component), 0, bounds);
  }

 private:
  class TestWindowHitTestDelegate : public aura::test::TestWindowDelegate {
   public:
    explicit TestWindowHitTestDelegate(int hit_test_component) {
      set_window_component(hit_test_component);
    }

    TestWindowHitTestDelegate(const TestWindowHitTestDelegate&) = delete;
    TestWindowHitTestDelegate& operator=(const TestWindowHitTestDelegate&) =
        delete;

    ~TestWindowHitTestDelegate() override = default;

   private:
    // aura::Test::TestWindowDelegate:
    void OnWindowDestroyed(aura::Window* window) override { delete this; }
  };
};

// Test some basic functionalities in clamshell splitview mode.
TEST_F(SplitViewOverviewSessionInClamshellTest, BasicFunctionalitiesTest) {
  UpdateDisplay("600x400");
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // 1. Test the 1 window scenario.
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateAppWindow(bounds));
  WindowState* window_state1 = WindowState::Get(window1.get());
  EXPECT_FALSE(window_state1->IsSnapped());
  ToggleOverview();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  // Since the only window is snapped, overview and splitview should be both
  // ended.
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kPrimarySnapped);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 2. Test if one window is snapped, the other windows are showing in
  // overview, close all windows in overview will end overview and also
  // splitview.
  std::unique_ptr<aura::Window> window2(CreateAppWindow(bounds));
  ToggleOverview();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(600, 300));
  // SplitView and overview are both active at the moment.
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(GetOverviewController()->overview_session()->IsWindowInOverview(
      window2.get()));
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kSecondarySnapped);
  // Close |window2| will end overview and splitview.
  window2.reset();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 3. Test that snap 2 windows will end overview and splitview.
  std::unique_ptr<aura::Window> window3(CreateAppWindow(bounds));
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  auto* overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(600, 300));
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kPrimarySnapped);
  EXPECT_EQ(WindowState::Get(window3.get())->GetStateType(),
            WindowStateType::kSecondarySnapped);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Maximize `window3` as `window1` and `window3` may form a Snap Group with
  // `kSnapGroup` enabled.
  WindowState::Get(window3.get())->Maximize();

  // 4. Test if one window is snapped, the other windows are showing in
  // overview, we can drag another window in overview to snap in splitview, and
  // the previous snapped window will be put back into overview.
  std::unique_ptr<aura::Window> window4(CreateAppWindow(bounds));
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_FALSE(GetOverviewController()->overview_session()->IsWindowInOverview(
      window1.get()));
  overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(0, 0));
  EXPECT_FALSE(GetOverviewController()->overview_session()->IsWindowInOverview(
      window3.get()));
  EXPECT_TRUE(GetOverviewController()->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kPrimarySnapped);
  EXPECT_EQ(WindowState::Get(window3.get())->GetStateType(),
            WindowStateType::kPrimarySnapped);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  // End overview, test that we'll not auto-snap a window to the right side of
  // the screen.
  EXPECT_EQ(WindowState::Get(window4.get())->GetStateType(),
            WindowStateType::kDefault);
  ToggleOverview();
  EXPECT_EQ(WindowState::Get(window4.get())->GetStateType(),
            WindowStateType::kDefault);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 5. Test if one window is snapped, the other windows are showing in
  // overview, activating an new window will not auto-snap the new window.
  // Overview and splitview should be ended.
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  EXPECT_EQ(WindowState::Get(window5.get())->GetStateType(),
            WindowStateType::kDefault);
  wm::ActivateWindow(window5.get());
  EXPECT_EQ(WindowState::Get(window5.get())->GetStateType(),
            WindowStateType::kDefault);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 6. Test if one window is snapped, the other window is showing in overview,
  // close the snapped window will end split view, but overview is still active.
  ToggleOverview();
  const gfx::Rect overview_bounds = GetGridBounds();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_NE(GetGridBounds(), overview_bounds);
  EXPECT_EQ(GetGridBounds(), GetExpectedOverviewBounds());
  window1.reset();
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  // Overview bounds will adjust from snapped bounds to fullscreen bounds.
  EXPECT_EQ(GetGridBounds(), overview_bounds);

  // 7. Test if split view mode is active, open the app list will end both
  // overview and splitview.
  overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(0, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  // Open app list.
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  app_list_controller->ToggleAppList(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window3.get()).id(),
      AppListShowSource::kSearchKey, base::TimeTicks());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 8. Test if splitview is not active, open the app list will end overview if
  // overview is active.
  ToggleOverview();
  // Open app list.
  app_list_controller->ToggleAppList(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window3.get()).id(),
      AppListShowSource::kSearchKey, base::TimeTicks());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

TEST_F(SplitViewOverviewSessionInClamshellTest,
       ResizePastFixedDividerPositions) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));

  // Start overview and drag to snap `window1` in split view.
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(window1.get()), gfx::PointF(0, 0));
  EXPECT_TRUE(RootWindowController::ForWindow(window1.get())
                  ->split_view_overview_session());

  const gfx::Point start_point(window1->GetBoundsInScreen().right_center());
  auto* generator = GetEventGenerator();
  generator->set_current_screen_location(start_point);

  // Resize the window to less than 1/3 of the work area. Test we end overview.
  const int work_area_length(GetWorkAreaInScreen(window1.get()).width());
  int window_length = 200;
  ASSERT_LT(window_length, work_area_length * chromeos::kOneThirdSnapRatio);
  generator->DragMouseTo(gfx::Point(window_length, start_point.y()));
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());

  // Start overview and snap `window1` in split view again.
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(window1.get()), gfx::PointF(0, 0));

  // Resize the window to greater than 2/3 of the work area. Test we end
  // overview.
  window_length = 600;
  ASSERT_GT(window_length, work_area_length * chromeos::kTwoThirdSnapRatio);
  generator->set_current_screen_location(start_point);
  generator->DragMouseTo(gfx::Point(window_length, start_point.y()));
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
}

// Test overview exit animation histograms when you drag to snap two windows on
// opposite sides.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       BothSnappedOverviewExitAnimationHistogramTest) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::FAST_DURATION);
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> left_window(CreateAppWindow(bounds));
  std::unique_ptr<aura::Window> right_window(CreateAppWindow(bounds));
  CheckOverviewEnterExitHistogram("Init", {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0});

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("EnterOverview", {1, 0, 0, 0, 0},
                                  {0, 0, 0, 0, 0});

  DragWindowTo(GetOverviewItemForWindow(left_window.get()), gfx::PointF(0, 0));
  DragWindowTo(GetOverviewItemForWindow(right_window.get()),
               gfx::PointF(799, 300));
  WaitForOverviewExitAnimation();
  CheckOverviewEnterExitHistogram("SnapBothSides", {1, 0, 0, 0, 0},
                                  {1, 0, 0, 0, 1});
}

// Test that when overview and splitview are both active, only resize that
// happens on eligible window components will change snapped window bounds and
// overview bounds at the same time.
TEST_F(SplitViewOverviewSessionInClamshellTest, ResizeWindowTest) {
  UpdateDisplay("600x400");
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithHitTestComponent(HTLEFT, bounds));
  std::unique_ptr<aura::Window> window3(
      CreateWindowWithHitTestComponent(HTTOP, bounds));
  std::unique_ptr<aura::Window> window4(
      CreateWindowWithHitTestComponent(HTBOTTOM, bounds));

  ToggleOverview();
  gfx::Rect overview_full_bounds = GetGridBounds();
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_EQ(GetGridBounds(), GetExpectedOverviewBounds());
  gfx::Rect overview_snapped_bounds = GetGridBounds();

  // Resize that happens on the right edge of the left snapped window will
  // resize the window and overview at the same time.
  ui::test::EventGenerator generator1(Shell::GetPrimaryRootWindow(),
                                      window1.get());
  generator1.PressLeftButton();
  CheckWindowResizingPerformanceHistograms("BeforeResizingLeftSnappedWindow1",
                                           0, 0, 0, 0);
  const int drag_x(50);
  generator1.MoveMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("WhileResizingLeftSnappedWindow1", 0,
                                           0, 1, 0);
  generator1.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow1", 0,
                                           0, 1, 1);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_NE(GetGridBounds(), overview_snapped_bounds);
  EXPECT_EQ(GetGridBounds(), GetExpectedOverviewBounds());
  EXPECT_TRUE(RootWindowController::ForWindow(Shell::GetPrimaryRootWindow())
                  ->split_view_overview_session());

  // Verify the overview width has decreased by the same amount the window has
  // increased.
  EXPECT_EQ(overview_snapped_bounds.width() - drag_x, GetGridBounds().width());
  const gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());
  EXPECT_EQ(work_area.width(),
            GetGridBounds().width() + window1->GetBoundsInScreen().width());

  // Resize that happens on the left edge of the left snapped window will end
  // overview. The same for the resize that happens on the top or bottom edge of
  // the left snapped window.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF(0, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ui::test::EventGenerator generator2(Shell::GetPrimaryRootWindow(),
                                      window2.get());
  generator2.DragMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow2", 0,
                                           0, 1, 1);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  auto* overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(0, 0));
  ui::test::EventGenerator generator3(Shell::GetPrimaryRootWindow(),
                                      window3.get());
  generator3.DragMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow3", 0,
                                           0, 1, 1);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  auto* overview_item4 = GetOverviewItemForWindow(window4.get());
  DragWindowTo(overview_item4, gfx::PointF(0, 0));
  ui::test::EventGenerator generator4(Shell::GetPrimaryRootWindow(),
                                      window4.get());
  generator4.DragMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow4", 0,
                                           0, 1, 1);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Now try snapping on the right.
  ToggleOverview();
  overview_full_bounds = GetGridBounds();
  overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF(599, 0));
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_EQ(GetGridBounds(), GetExpectedOverviewBounds());
  overview_snapped_bounds = GetGridBounds();

  ui::test::EventGenerator generator5(Shell::GetPrimaryRootWindow(),
                                      window2.get());
  generator5.PressLeftButton();
  CheckWindowResizingPerformanceHistograms("BeforeResizingRightSnappedWindow2",
                                           0, 0, 1, 1);
  generator5.MoveMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("WhileResizingRightSnappedWindow2",
                                           0, 0, 2, 1);
  generator5.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow2",
                                           0, 0, 2, 2);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_NE(GetGridBounds(), overview_snapped_bounds);
  EXPECT_EQ(GetGridBounds(), GetExpectedOverviewBounds());
  EXPECT_EQ(overview_snapped_bounds.width() + 50, GetGridBounds().width());
  EXPECT_EQ(work_area.width(),
            GetGridBounds().width() + window2->GetBoundsInScreen().width());

  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(599, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ui::test::EventGenerator generator6(Shell::GetPrimaryRootWindow(),
                                      window1.get());
  generator6.DragMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow1",
                                           0, 0, 2, 2);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(599, 0));
  ui::test::EventGenerator generator7(Shell::GetPrimaryRootWindow(),
                                      window3.get());
  generator7.DragMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow3",
                                           0, 0, 2, 2);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  overview_item4 = GetOverviewItemForWindow(window4.get());
  DragWindowTo(overview_item4, gfx::PointF(599, 0));
  ui::test::EventGenerator generator8(Shell::GetPrimaryRootWindow(),
                                      window4.get());
  generator8.DragMouseBy(drag_x, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow4",
                                           0, 0, 2, 2);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test closing the split view window while resizing it.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       CloseWindowWhileResizingItTest) {
  UpdateDisplay("600x400");
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> split_view_window(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> overview_window(CreateWindow(bounds));
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(split_view_window.get()),
               gfx::PointF(0.f, 0.f));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     split_view_window.get());
  generator.PressLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterPressingMouseButton", 0, 0, 0,
                                           0);
  generator.MoveMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("WhileResizing", 0, 0, 1, 0);
  split_view_window.reset();
  CheckWindowResizingPerformanceHistograms("AfterClosing", 0, 0, 1, 1);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  generator.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterReleasingMouseButton", 0, 0, 1,
                                           1);
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Tests that there will be no crash when dragging a snapped window in overview
// toward the edge. In this case, the overview components will become too small
// to meet the minimum requirement of the fundamental UI layers such as virtual
// desk bar, shadow. See the regression behavior in http://b/324478757.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       NoCrashWhenDraggingSnappedWindowToEdge) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  // Create another desk to ensure the desk bar shows in overview.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  EXPECT_TRUE(IsInOverviewSession());
  std::unique_ptr<aura::Window> window1(
      CreateAppWindow(gfx::Rect(0, 0, 200, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateAppWindow(gfx::Rect(100, 100, 200, 100)));
  const WindowSnapWMEvent event(
      WM_EVENT_SNAP_PRIMARY, chromeos::kDefaultSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  auto* window1_state = WindowState::Get(window1.get());
  window1_state->OnWMEvent(&event);
  WaitForOverviewEntered();
  EXPECT_TRUE(window1_state->IsSnapped());
  EXPECT_TRUE(IsInOverviewSession());

  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      window1.get()->GetBoundsInScreen().right_center());
  gfx::Point drag_end_point = GetWorkAreaInScreen(window1.get()).right_center();
  drag_end_point.Offset(/*delta_x=*/-10, 0);
  event_generator->PressLeftButton();
  event_generator->MoveMouseTo(drag_end_point);
  EXPECT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(WindowState::Get(window1.get())->is_dragged());

  // Verify that shadow is applied on the overview item.
  auto* overview_item2 = GetOverviewItemForWindow(window2.get());
  const auto shadow_content_bounds =
      overview_item2->get_shadow_content_bounds_for_testing();
  EXPECT_FALSE(shadow_content_bounds.IsEmpty());
}

// Tests that when a split view window carries over to clamshell split view
// while the divider is being dragged, the window resize is properly completed.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       CarryOverToClamshellSplitViewWhileResizing) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  auto* snapped_window_state_delegate = new FakeWindowStateDelegate();
  snapped_window_state->SetDelegate(
      base::WrapUnique(snapped_window_state_delegate));

  // Enter clamshell split view and then switch to tablet mode.
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  EnterTabletMode();
  ASSERT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  ASSERT_EQ(snapped_window.get(), split_view_controller()->primary_window());

  // Start dragging the divider.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      split_view_controller()
          ->split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint());
  generator->PressTouch();
  // Drag the divider by an amount big enough to be considered
  // EventType::kGestureScrollBegin.
  generator->MoveTouchBy(7, 0);
  EXPECT_TRUE(snapped_window_state_delegate->drag_in_progress());
  EXPECT_NE(nullptr, snapped_window_state->drag_details());

  // End tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  ASSERT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  ASSERT_EQ(snapped_window.get(), split_view_controller()->primary_window());
  EXPECT_FALSE(snapped_window_state_delegate->drag_in_progress());
  EXPECT_EQ(nullptr, snapped_window_state->drag_details());
}

// Test that overview and clamshell split view end if you double click the edge
// of the split view window where it meets the overview grid.
TEST_F(SplitViewOverviewSessionInClamshellTest, HorizontalMaximizeTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> snapped_window(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow(bounds);
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  ASSERT_FALSE(split_view_controller()->IsDividerAnimating());
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ui::test::EventGenerator(Shell::GetPrimaryRootWindow(), snapped_window.get())
      .DoubleClickLeftButton();
  ASSERT_FALSE(split_view_controller()->IsDividerAnimating());
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that when laptop splitview mode is active, moving the snapped window
// will end splitview and overview at the same time.
TEST_F(SplitViewOverviewSessionInClamshellTest, MoveWindowTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithHitTestComponent(HTCAPTION, bounds));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithHitTestComponent(HTCAPTION, bounds));

  ToggleOverview();
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  ui::test::EventGenerator generator1(Shell::GetPrimaryRootWindow(),
                                      window1.get());
  generator1.DragMouseBy(50, 50);
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that in clamshell splitview mode, if the snapped window is minimized,
// splitview mode and overview mode are both ended.
TEST_F(SplitViewOverviewSessionInClamshellTest, MinimizedWindowTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Drag |window1| selector item to snap to left.
  auto* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(GetOverviewController()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Now minimize the snapped |window1|.
  WindowState::Get(window1.get())->Minimize();
  EXPECT_FALSE(GetOverviewController()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test snapped window bounds with adjustment for the minimum size of a window.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       SnappedWindowBoundsWithMinimumSizeTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  const int window2_minimum_size = 350;
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithMinimumSize(bounds, gfx::Size(window2_minimum_size, 0)));

  // Snap `window1` in split view, then resize it to `window1_size`, which is
  // less than `window2_minimum_size`.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);
  ui::test::EventGenerator* generator = GetEventGenerator();
  ASSERT_TRUE(RootWindowController::ForWindow(window1.get())
                  ->split_view_overview_session());
  generator->MoveMouseTo(window1->GetBoundsInScreen().width(), 10);
  int window1_size = 300;
  generator->DragMouseTo(window1_size, 10);
  ASSERT_EQ(window1_size, window1->GetBoundsInScreen().width());

  // Drag `window2` to the left, on top of `window1`, to show the left highlight
  // preview.
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Point drag_starting_point(
      gfx::ToRoundedPoint(item2->GetTransformedBounds().CenterPoint()));
  DragItemToPoint(item2, gfx::Point(0, 0), generator,
                  /*by_touch_gestures=*/false, /*drop=*/false);

  // Test that the highlight bounds are adjusted for `window2_minimum_size`.
  gfx::Rect left_highlight_bounds(work_area.x(), work_area.y(),
                                  window2_minimum_size, work_area.height());
  left_highlight_bounds.Inset(kHighlightScreenEdgePaddingDp);
  auto* overview_grid =
      GetOverviewSession()->GetGridWithRootWindow(window1->GetRootWindow());
  EXPECT_EQ(left_highlight_bounds, overview_grid->split_view_drag_indicators()
                                       ->GetLeftHighlightViewBounds());

  // Drop the `window2` item back at its starting point.
  generator->MoveMouseTo(drag_starting_point);
  generator->ReleaseLeftButton();
  ASSERT_TRUE(RootWindowController::ForWindow(window1.get())
                  ->split_view_overview_session());

  // Now resize `window1` where `window2` can't fit in the secondary position.
  window1_size = 500;
  generator->MoveMouseTo(window1->GetBoundsInScreen().width(), 10);
  generator->DragMouseTo(window1_size, 0);
  ASSERT_TRUE(RootWindowController::ForWindow(window1.get())
                  ->split_view_overview_session());
  ASSERT_EQ(window1_size, window1->GetBoundsInScreen().width());

  // Drag `window2` to show the right highlight preview.
  DragItemToPoint(item2, work_area.top_right(), generator,
                  /*by_touch_gestures=*/false, /*drop=*/false);

  // Test that the highlight bounds are adjusted for `window2_minimum_size`.
  gfx::Rect right_highlight_bounds(work_area.right() - window2_minimum_size,
                                   work_area.y(), window2_minimum_size,
                                   work_area.height());
  right_highlight_bounds.Inset(kHighlightScreenEdgePaddingDp);
  EXPECT_EQ(right_highlight_bounds,
            overview_grid->split_view_drag_indicators()
                ->GetRightHighlightViewBoundsForTesting());
  generator->ReleaseLeftButton();
}

// Tests that on a display in portrait orientation, clamshell split view still
// uses snap positions on the left and right.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       PortraitClamshellSplitViewSnapPositionsTest) {
  UpdateDisplay("800x600/l");
  const int height = 800 - ShelfConfig::Get()->shelf_size();
  ASSERT_EQ(gfx::Rect(0, 0, 600, height),
            screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
                Shell::GetPrimaryRootWindow()));
  // Check that snapped window bounds represent snapping on the left and right.
  const gfx::Rect top_snapped_bounds(600, height / 2);
  const gfx::Rect bottom_snapped_bounds(0, height / 2, 600, height / 2);
  const gfx::Rect left_snapped_bounds(300, height);
  const gfx::Rect right_snapped_bounds(300, 0, 300, height);
  EXPECT_EQ(
      top_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SnapPosition::kPrimary,
          /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
          /*account_for_divider_width=*/false));
  EXPECT_EQ(
      bottom_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SnapPosition::kSecondary,
          /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
          /*account_for_divider_width=*/false));

  // Switch from clamshell mode to tablet mode and then back to clamshell mode.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  tablet_mode_controller_test_api.DetachAllMice();
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  tablet_mode_controller_test_api.OpenLidToAngle(315.0f);
  EXPECT_TRUE(display::Screen::GetScreen()->InTabletMode());
  tablet_mode_controller_test_api.OpenLidToAngle(90.0f);
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  // Check the snapped window bounds again. They should be the same as before.
  EXPECT_EQ(
      top_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SnapPosition::kPrimary,
          /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
          /*account_for_divider_width=*/false));
  EXPECT_EQ(
      bottom_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SnapPosition::kSecondary,
          /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
          /*account_for_divider_width=*/false));
}

// Tests that the ratio between the divider position and the work area width is
// the same before and after changing the display orientation in clamshell mode.
TEST_F(SplitViewOverviewSessionInClamshellTest, DisplayOrientationChangeTest) {
  UpdateDisplay("600x400");
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> split_view_window(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> overview_window(CreateWindow(bounds));
  ToggleOverview();
  split_view_controller()->SnapWindow(split_view_window.get(),
                                      SnapPosition::kPrimary);
  const auto test_many_orientation_changes =
      [this](const std::string& description) {
        SCOPED_TRACE(description);
        for (display::Display::Rotation rotation :
             {display::Display::ROTATE_270, display::Display::ROTATE_180,
              display::Display::ROTATE_90, display::Display::ROTATE_0,
              display::Display::ROTATE_180, display::Display::ROTATE_0}) {
          const auto compute_window_snap_ratio = [this]() {
            const display::Display& display =
                display::Screen::GetScreen()->GetPrimaryDisplay();
            const bool is_horizontal = IsLayoutHorizontal(display);
            const gfx::Rect work_area = display.work_area();
            const int size =
                is_horizontal ? work_area.width() : work_area.height();
            const gfx::Rect window_bounds(split_view_controller()
                                              ->GetDefaultSnappedWindow()
                                              ->GetBoundsInScreen());
            const int window_size =
                is_horizontal ? window_bounds.width() : window_bounds.height();
            return static_cast<float>(window_size) / static_cast<float>(size);
          };
          const float before = compute_window_snap_ratio();
          Shell::Get()->display_manager()->SetDisplayRotation(
              display::Screen::GetScreen()->GetPrimaryDisplay().id(), rotation,
              display::Display::RotationSource::ACTIVE);
          const float after = compute_window_snap_ratio();
          EXPECT_NEAR(before, after, 0.001f);
        }
      };

  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  test_many_orientation_changes("centered divider");
  EXPECT_EQ(split_view_window->GetBoundsInScreen().width() * 2,
            work_area.width());
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      split_view_window->GetBoundsInScreen().right_center());
  event_generator->DragMouseBy(50, 50);
  EXPECT_NE(split_view_window->GetBoundsInScreen().width() * 2,
            work_area.width());
  test_many_orientation_changes("off-center divider");
}

// Verify that an item's unsnappable indicator is updated for display rotation.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       OverviewUnsnappableIndicatorVisibilityAfterDisplayRotation) {
  UpdateDisplay("900x600");
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  // Because of its minimum size, |overview_window| is snappable in clamshell
  // split view with landscape display orientation but not with portrait display
  // orientation.
  std::unique_ptr<aura::Window> overview_window(
      CreateWindowWithMinimumSize(gfx::Rect(400, 400), gfx::Size(400, 500)));
  ToggleOverview();
  ASSERT_TRUE(GetOverviewController()->InOverviewSession());
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  auto* overview_item = GetOverviewItemForWindow(overview_window.get());
  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  EXPECT_FALSE(GetCannotSnapWidget(overview_item));

  // Rotate to primary portrait orientation. The unsnappable indicator appears.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_270,
                                      display::Display::RotationSource::ACTIVE);
  views::Widget* cannot_snap_widget = GetCannotSnapWidget(overview_item);
  ASSERT_TRUE(cannot_snap_widget);
  ui::Layer* unsnappable_layer = cannot_snap_widget->GetLayer();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Rotate to primary landscape orientation. The unsnappable indicator hides.
  display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_0,
                                      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
}

// Tests that dragging a window from overview creates a drop target on the same
// display, even if the window bounds are mostly on another display.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       DragFromOverviewWithBoundsMostlyOnAnotherDisplay) {
  UpdateDisplay("700x600,700x600");
  const aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const display::DisplayIdList display_ids =
      display_manager()->GetConnectedDisplayIdList();
  ASSERT_EQ(2u, display_ids.size());
  ASSERT_EQ(root_windows[0], Shell::GetRootWindowForDisplayId(display_ids[0]));
  ASSERT_EQ(root_windows[1], Shell::GetRootWindowForDisplayId(display_ids[1]));

  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect creation_bounds(0, 0, 600, 600);
  ASSERT_EQ(display_ids[0], screen->GetDisplayMatching(creation_bounds).id());
  const gfx::Rect bounds(550, 0, 600, 600);
  ASSERT_EQ(display_ids[1], screen->GetDisplayMatching(bounds).id());
  std::unique_ptr<aura::Window> window = CreateTestWindow(creation_bounds);
  window->SetBoundsInScreen(bounds,
                            display_manager()->GetDisplayForId(display_ids[0]));

  ToggleOverview();
  auto* overview_item = GetOverviewItemForWindow(window.get());
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  gfx::PointF drag_point = overview_item->target_bounds().CenterPoint();
  GetOverviewSession()->InitiateDrag(overview_item, drag_point,
                                     /*is_touch_dragging=*/false,
                                     /*event_source_item=*/overview_item);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point.Offset(5.f, 0.f);
  GetOverviewSession()->Drag(overview_item, drag_point);
  EXPECT_FALSE(GetDropTarget(1));
  ASSERT_TRUE(GetDropTarget(0));
  EXPECT_EQ(root_windows[0], GetDropTarget(0)->root_window());
  GetOverviewSession()->CompleteDrag(overview_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
}

// Tests that cycle snap do not start overview.
TEST_F(SplitViewOverviewSessionInClamshellTest, CycleSnapNotStartOverview) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());

  const WindowSnapWMEvent cycle_snap_primary(WM_EVENT_CYCLE_SNAP_PRIMARY);
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->OnWMEvent(&cycle_snap_primary);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window1_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());

  const WindowSnapWMEvent cycle_snap_secondary(WM_EVENT_CYCLE_SNAP_SECONDARY);
  window1_state->OnWMEvent(&cycle_snap_secondary);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window1_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Tests using Alt+[ on a left split view window.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       AltLeftSquareBracketOnLeftSplitViewWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kPrimary);
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            snapped_window_state->GetStateType());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  const WindowSnapWMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_PRIMARY);
  snapped_window_state->OnWMEvent(&alt_left_square_bracket);
  EXPECT_EQ(WindowStateType::kNormal, snapped_window_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Tests using Alt+] on a right split view window.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       AltRightSquareBracketOnRightSplitViewWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kSecondary);
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            snapped_window_state->GetStateType());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  const WindowSnapWMEvent alt_right_square_bracket(
      WM_EVENT_CYCLE_SNAP_SECONDARY);
  snapped_window_state->OnWMEvent(&alt_right_square_bracket);
  EXPECT_EQ(WindowStateType::kNormal, snapped_window_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Tests using Alt+[ on a right split view window, and Alt+] on a left split
// view window.
TEST_F(SplitViewOverviewSessionInClamshellTest,
       AltSquareBracketOnSplitViewWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  // Enter clamshell split view with |snapped_window| on the right.
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SnapPosition::kSecondary);
  wm::ActivateWindow(snapped_window.get());
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->secondary_window());
  EXPECT_TRUE(InOverviewSession());
  // Test using Alt+[ to put |snapped_window| on the left.
  const WindowSnapWMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_PRIMARY);
  snapped_window_state->OnWMEvent(&alt_left_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->primary_window());
  EXPECT_TRUE(InOverviewSession());
  // Test using Alt+] to put |snapped_window| on the right.
  const WindowSnapWMEvent alt_right_square_bracket(
      WM_EVENT_CYCLE_SNAP_SECONDARY);
  snapped_window_state->OnWMEvent(&alt_right_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->secondary_window());
  EXPECT_TRUE(InOverviewSession());
}

using SplitViewOverviewSessionInClamshellTestMultiDisplayOnly =
    SplitViewOverviewSessionInClamshellTest;

// Test |SplitViewController::Get|.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       GetSplitViewController) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root2);
  EXPECT_EQ(root_windows[0],
            SplitViewController::Get(window1.get())->root_window());
  EXPECT_EQ(root_windows[1],
            SplitViewController::Get(window2.get())->root_window());
}

// Test |SplitViewController::GetSnappedWindowBoundsInScreen|.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       GetSnappedBounds) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const int height = 600 - ShelfConfig::Get()->shelf_size();
  ASSERT_EQ(gfx::Rect(0, 0, 800, height),
            screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
                root_windows[0]));
  ASSERT_EQ(gfx::Rect(800, 0, 800, height),
            screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
                root_windows[1]));

  EXPECT_EQ(
      gfx::Rect(0, 0, 400, height),
      SplitViewController::Get(root_windows[0])
          ->GetSnappedWindowBoundsInScreen(
              SnapPosition::kPrimary,
              /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
              /*account_for_divider_width=*/false));
  EXPECT_EQ(
      gfx::Rect(400, 0, 400, height),
      SplitViewController::Get(root_windows[0])
          ->GetSnappedWindowBoundsInScreen(
              SnapPosition::kSecondary,
              /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
              /*account_for_divider_width=*/false));
  EXPECT_EQ(
      gfx::Rect(800, 0, 400, height),
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(
              SnapPosition::kPrimary,
              /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
              /*account_for_divider_width=*/false));
  EXPECT_EQ(
      gfx::Rect(1200, 0, 400, height),
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(
              SnapPosition::kSecondary,
              /*window_for_minimum_size=*/nullptr, chromeos::kDefaultSnapRatio,
              /*account_for_divider_width=*/false));
}

// Test that if clamshell split view is started by snapping a window that is the
// only overview window, then split view ends as soon as it starts, and overview
// ends along with it.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       SplitViewEndsImmediatelyIfOverviewIsEmpty) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds_within_root1);
  ToggleOverview();
  SplitViewController::Get(root_windows[0])
      ->SnapWindow(window.get(), SnapPosition::kPrimary);
  EXPECT_FALSE(InOverviewSession());
  EXPECT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
}

// Test that if clamshell split view is started by snapping a window on one
// display while there is an overview window on another display, then split view
// stays active (instead of ending as soon as it starts), and overview also
// stays active. Then close the overview window and verify that split view and
// overview are ended.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       SplitViewViableWithOverviewWindowOnOtherDisplay) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root2);
  ToggleOverview();
  SplitViewController::Get(root_windows[0])
      ->SnapWindow(window1.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  window2.reset();
  EXPECT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Test dragging to snap an overview item on an external display.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DraggingOnExternalDisplay) {
  UpdateDisplay("800x600,800+0-800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root2);

  ToggleOverview();

  OverviewGrid* grid_on_root2 =
      GetOverviewSession()->GetGridWithRootWindow(root_windows[1]);
  auto* item1 = grid_on_root2->GetOverviewItemContaining(window1.get());
  auto* item2 = grid_on_root2->GetOverviewItemContaining(window2.get());
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_windows[1]);
  const SplitViewDragIndicators* indicators =
      grid_on_root2->split_view_drag_indicators();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();

  // TODO(http://b/300700394): Avoid hardcoding these numbers.
  const gfx::Point right_snap_point(1599, 300);
  event_generator->MoveMouseTo(right_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary,
            indicators->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SnapPosition::kPrimary,
                                           /*window_for_minimum_size=*/nullptr,
                                           chromeos::kDefaultSnapRatio,
                                           /*account_for_divider_width=*/false),
      OverviewGridTestApi(grid_on_root2).bounds());
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller->state());
  EXPECT_EQ(window1.get(), split_view_controller->secondary_window());

  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item2->target_bounds().CenterPoint()));
  GetEventGenerator()->PressLeftButton();
  const gfx::Point left_of_middle(1150, 300);
  event_generator->MoveMouseTo(left_of_middle);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            indicators->current_window_dragging_state());
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(SplitViewController::State::kSecondarySnapped,
            split_view_controller->state());
  EXPECT_EQ(window1.get(), split_view_controller->secondary_window());

  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item2->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();
  const gfx::Point left_snap_point(810, 300);
  event_generator->MoveMouseTo(left_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            indicators->current_window_dragging_state());
  event_generator->ReleaseLeftButton();
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller->state());
}

// Test dragging from one display to another.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       MultiDisplayDragging) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const display::Display display_with_root1 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]);
  const display::Display display_with_root2 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds_within_root2);
  ToggleOverview();
  OverviewGrid* grid_on_root1 =
      GetOverviewSession()->GetGridWithRootWindow(root_windows[0]);
  OverviewGrid* grid_on_root2 =
      GetOverviewSession()->GetGridWithRootWindow(root_windows[1]);
  auto* item1 = grid_on_root1->GetOverviewItemContaining(window1.get());
  const SplitViewDragIndicators* indicators_on_root1 =
      grid_on_root1->split_view_drag_indicators();
  const SplitViewDragIndicators* indicators_on_root2 =
      grid_on_root2->split_view_drag_indicators();

  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint()));
  event_generator->PressLeftButton();
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(),
            OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(),
            OverviewGridTestApi(grid_on_root2).bounds());

  const gfx::Point root1_left_snap_point(0, 300);
  event_generator->MoveMouseTo(root1_left_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[0])
          ->GetSnappedWindowBoundsInScreen(SnapPosition::kSecondary,
                                           /*window_for_minimum_size=*/nullptr,
                                           chromeos::kDefaultSnapRatio,
                                           /*account_for_divider_width=*/false),
      OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(),
            OverviewGridTestApi(grid_on_root2).bounds());

  const gfx::Point root1_middle_point(400, 300);
  event_generator->MoveMouseTo(root1_middle_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(),
            OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(),
            OverviewGridTestApi(grid_on_root2).bounds());

  const gfx::Point root1_right_snap_point(799, 300);
  event_generator->MoveMouseTo(root1_right_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[0])
          ->GetSnappedWindowBoundsInScreen(SnapPosition::kPrimary,
                                           /*window_for_minimum_size=*/nullptr,
                                           chromeos::kDefaultSnapRatio,
                                           /*account_for_divider_width=*/false),
      OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(),
            OverviewGridTestApi(grid_on_root2).bounds());

  const gfx::Point root2_left_snap_point(800, 300);
  event_generator->MoveMouseTo(root2_left_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(),
            OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SnapPosition::kSecondary,
                                           /*window_for_minimum_size=*/nullptr,
                                           chromeos::kDefaultSnapRatio,
                                           /*account_for_divider_width=*/false),
      OverviewGridTestApi(grid_on_root2).bounds());

  const gfx::Point root2_left_snap_point_away_from_edge(816, 300);
  event_generator->MoveMouseTo(root2_left_snap_point_away_from_edge);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(),
            OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapPrimary,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SnapPosition::kSecondary,
                                           /*window_for_minimum_size=*/nullptr,
                                           chromeos::kDefaultSnapRatio,
                                           /*account_for_divider_width=*/false),
      OverviewGridTestApi(grid_on_root2).bounds());

  const gfx::Point root2_right_snap_point(1599, 300);
  event_generator->MoveMouseTo(root2_right_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(),
            OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapSecondary,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SnapPosition::kPrimary,
                                           /*window_for_minimum_size=*/nullptr,
                                           chromeos::kDefaultSnapRatio,
                                           /*account_for_divider_width=*/false),
      OverviewGridTestApi(grid_on_root2).bounds());

  const gfx::Point root2_middle_point(1200, 300);
  event_generator->MoveMouseTo(root2_middle_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(),
            OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(),
            OverviewGridTestApi(grid_on_root2).bounds());

  event_generator->ReleaseLeftButton();
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(),
            OverviewGridTestApi(grid_on_root1).bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(),
            OverviewGridTestApi(grid_on_root2).bounds());
}

// Verify the drop target positions for multi-display dragging.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DropTargetPositionTest) {
  wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const display::Display display_with_root1 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[0]);
  const display::Display display_with_root2 =
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]);
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  // Named for MRU order, which is in reverse of creation order.
  std::unique_ptr<aura::Window> window6 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  ToggleOverview();
  OverviewGrid* grid1 =
      GetOverviewSession()->GetGridWithRootWindow(root_windows[0]);
  OverviewGrid* grid2 =
      GetOverviewSession()->GetGridWithRootWindow(root_windows[1]);
  auto* item4 = grid2->GetOverviewItemContaining(window4.get());
  // Start dragging |item4| from |grid2|.
  cursor_manager->SetDisplay(display_with_root2);
  GetOverviewSession()->InitiateDrag(
      item4, item4->target_bounds().CenterPoint(),
      /*is_touch_dragging=*/false, /*event_source_item=*/item4);
  GetOverviewSession()->Drag(item4, gfx::PointF(1200.f, 0.f));
  // On the grid where the drag starts (|grid2|), the drop target is inserted at
  // the index immediately following the dragged item (|item4|).
  ASSERT_EQ(4u, grid2->item_list().size());
  EXPECT_EQ(GetDropTarget(1), grid2->item_list()[2].get());
  // Drag over |grid1|.
  cursor_manager->SetDisplay(display_with_root1);
  GetOverviewSession()->Drag(item4, gfx::PointF(400.f, 0.f));
  // On other grids (such as |grid1|), the drop target is inserted at the
  // correct position according to MRU order (between the overview items for
  // |window3| and |window5|).
  ASSERT_EQ(4u, grid1->item_list().size());
  EXPECT_EQ(GetDropTarget(0), grid1->item_list()[2].get());
}

// Verify that the drop target in each overview grid has the correct bounds when
// a maximized window is being dragged.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DropTargetBoundsForMaximizedWindowDraggedToOtherDisplay) {
  UpdateDisplay("1200x400,1200x400/l");
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  WindowState::Get(window.get())->Maximize();
  ToggleOverview();
  auto* item = GetOverviewItemForWindow(window.get());
  // Verify that |item| is letter boxed. The bounds of |item|, minus the margin
  // should have an aspect ratio of 2 : 1.
  gfx::RectF item_bounds = item->target_bounds();
  EXPECT_EQ(OverviewItemFillMode::kLetterBoxed,
            item->GetOverviewItemFillMode());
  EXPECT_EQ(2.f, item_bounds.width() / item_bounds.height());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(gfx::ToRoundedPoint(item_bounds.CenterPoint()));
  event_generator->PressLeftButton();

  // Drag to the middle of the secondary display to avoid triggering the drag
  // snap indicator animation.
  event_generator->MoveMouseTo(gfx::Point(1400, 500));

  auto* drop_target = GetDropTarget(1);
  ASSERT_TRUE(drop_target);

  // Verify that `drop_target` is effectively pillar boxed. Avoid calling
  // `OverviewItem::GetOverviewItemFillMode()`, because it does not work for
  // drop targets (and that is okay). The bounds of `drop_target`, minus the
  // margin should have an aspect ratio of 1 : 2.
  const gfx::Size drop_target_size =
      drop_target->item_widget()->GetWindowBoundsInScreen().size();
  EXPECT_EQ(0.5f, static_cast<float>(drop_target_size.width()) /
                      drop_target_size.height());
}

// Verify that the drop target in each overview grid has bounds representing
// anticipation that if the dragged window is dropped into that grid, it will
// shrink to fit into the corresponding work area.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DropTargetBoundsOnDisplayWhereDraggedWindowDoesNotFitIntoWorkArea) {
  UpdateDisplay("600x500,600+0-1200x1000");
  // Drags `item` from the right display to the left display and back, and
  // returns the bounds of the drop target that appears on the left display.
  const auto root1_drop_target_bounds = [this](OverviewItemBase* item) {
    const gfx::Point drag_starting_point =
        gfx::ToRoundedPoint(item->target_bounds().CenterPoint());
    auto* event_generator = GetEventGenerator();
    event_generator->MoveMouseTo(drag_starting_point);
    event_generator->PressLeftButton();
    event_generator->MoveMouseTo(gfx::Point(300, 0));
    event_generator->MoveMouseTo(drag_starting_point);

    CHECK(GetDropTarget(0));
    const gfx::RectF result = GetDropTarget(0)->target_bounds();
    event_generator->ReleaseLeftButton();
    return result;
  };

  // |window1| has the size that |window2| would become if moved to the left
  // display.
  std::unique_ptr<aura::Window> window1 =
      CreateTestWindow(gfx::Rect(600, 0, 600, 400));
  std::unique_ptr<aura::Window> window2 =
      CreateTestWindow(gfx::Rect(600, 0, 1000, 400));
  // |window3| has the size that |window4| would become if moved to the left
  // display.
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(
      gfx::Rect(600, 0, 400, 600 - ShelfConfig::Get()->shelf_size()));
  std::unique_ptr<aura::Window> window4 =
      CreateTestWindow(gfx::Rect(600, 0, 400, 1000));

  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());
  auto* item4 = GetOverviewItemForWindow(window4.get());

  // For good test coverage in each case, the dragged window and the drop target
  // have different `OverviewItemFillMode` values.
  EXPECT_EQ(OverviewItemFillMode::kNormal, item1->GetOverviewItemFillMode());
  EXPECT_EQ(OverviewItemFillMode::kLetterBoxed,
            item2->GetOverviewItemFillMode());
  EXPECT_EQ(OverviewItemFillMode::kNormal, item3->GetOverviewItemFillMode());
  EXPECT_EQ(OverviewItemFillMode::kPillarBoxed,
            item4->GetOverviewItemFillMode());

  EXPECT_EQ(root1_drop_target_bounds(item1), root1_drop_target_bounds(item2));
  EXPECT_EQ(root1_drop_target_bounds(item3), root1_drop_target_bounds(item4));
}

// Test dragging from one overview grid and dropping into another overview grid.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DragAndDropIntoAnotherOverviewGrid) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ASSERT_EQ(root_windows[0], window->GetRootWindow());
  ToggleOverview();
  OverviewGrid* grid1 =
      GetOverviewSession()->GetGridWithRootWindow(root_windows[0]);
  OverviewGrid* grid2 =
      GetOverviewSession()->GetGridWithRootWindow(root_windows[1]);

  // Drag |window| from |grid1| and drop into |grid2|.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      gfx::ToRoundedPoint(grid1->GetOverviewItemContaining(window.get())
                              ->target_bounds()
                              .CenterPoint()));
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  generator->MoveMouseTo(1200, 300);
  generator->ReleaseLeftButton();

  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_TRUE(grid1->empty());
  auto* item = grid2->GetOverviewItemContaining(window.get());
  ASSERT_TRUE(item);
  EXPECT_EQ(root_windows[1], item->root_window());
}

// Test that overview widgets are stacked in the correct order after an overview
// window is dragged from one overview grid and dropped into another. Also test
// that the destination overview grid is arranged in the correct order.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       OverviewWidgetStackingOrderAndGridOrderWithMultiDisplayDragging) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root2);
  aura::Window* parent_on_root1 = window2->parent();
  aura::Window* parent_on_root2 = window1->parent();
  ASSERT_NE(parent_on_root1, parent_on_root2);
  ASSERT_EQ(window3->parent(), parent_on_root2);
  ToggleOverview();
  auto* item1 = GetOverviewItemForWindow(window1.get());
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());

  ASSERT_EQ(root_windows[0], item2->root_window());
  // Verify that |item1| is stacked above |item3| (because we created |window1|
  // after |window3|).
  EXPECT_TRUE(
      window_util::IsStackedBelow(item3->item_widget()->GetNativeWindow(),
                                  item1->item_widget()->GetNativeWindow()));

  // Verify that the item widget for each window is stacked below that window.
  EXPECT_TRUE(window_util::IsStackedBelow(
      item1->item_widget()->GetNativeWindow(), window1.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(
      item2->item_widget()->GetNativeWindow(), window2.get()));
  EXPECT_TRUE(window_util::IsStackedBelow(
      item3->item_widget()->GetNativeWindow(), window3.get()));

  // Drag |item2| from the left display and drop into the right display.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      gfx::ToRoundedPoint(item2->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  generator->MoveMouseTo(1200, 300);
  generator->ReleaseLeftButton();
  // |item2| is now a dangling pointer and we have to refresh it, because when
  // an overview window is dragged from one grid and dropped into another, the
  // original item is destroyed and a new one is created.
  item2 = GetOverviewItemForWindow(window2.get());

  ASSERT_EQ(window2->parent(), parent_on_root2);
  ASSERT_EQ(root_windows[1], item2->root_window());
  // With all three items on one grid, verify that their stacking order
  // corresponds to the MRU order of the windows. The new |item2| is sandwiched
  // between |item1| and |item3|.
  EXPECT_TRUE(
      window_util::IsStackedBelow(item2->item_widget()->GetNativeWindow(),
                                  item1->item_widget()->GetNativeWindow()));
  EXPECT_TRUE(
      window_util::IsStackedBelow(item3->item_widget()->GetNativeWindow(),
                                  item2->item_widget()->GetNativeWindow()));
  // Verify that the item widget for the new |item2| is stacked below |window2|.
  EXPECT_TRUE(window_util::IsStackedBelow(
      item2->item_widget()->GetNativeWindow(), window2.get()));

  // Verify that the right grid is in MRU order.
  const std::vector<aura::Window*> expected_order = {
      window1.get(), window2.get(), window3.get()};
  EXPECT_EQ(expected_order, GetWindowsListInOverviewGrids());
}

// Test dragging from one display to another and then snapping.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DragFromOneDisplayToAnotherAndSnap) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  SplitViewController* split_view_controller1 =
      SplitViewController::Get(root_windows[0]);
  SplitViewController* split_view_controller2 =
      SplitViewController::Get(root_windows[1]);
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root1);
  ToggleOverview();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(gfx::ToRoundedPoint(
      GetOverviewItemForWindow(window2.get())->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  generator->MoveMouseTo(800, 300);
  generator->ReleaseLeftButton();
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller1->state());
  EXPECT_EQ(SplitViewController::State::kPrimarySnapped,
            split_view_controller2->state());
  EXPECT_EQ(window2.get(), split_view_controller2->primary_window());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_TRUE(InOverviewSession());
}

// Verify that window resizing performance is recorded to the correct histogram
// depending on whether the overview grid is empty.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       WindowResizingPerformanceHistogramsTest) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds_within_root1));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds_within_root2));
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds_within_root2);
  ToggleOverview();
  SplitViewController::Get(root_windows[0])
      ->SnapWindow(window1.get(), SnapPosition::kPrimary);
  SplitViewController::Get(root_windows[1])
      ->SnapWindow(window2.get(), SnapPosition::kPrimary);
  // Resize |window1|, which is in split view with an empty overview grid.
  ui::test::EventGenerator generator1(root_windows[0], window1.get());
  generator1.PressLeftButton();
  CheckWindowResizingPerformanceHistograms("BeforeResizingWindow1", 0, 0, 0, 0);
  generator1.MoveMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("WhileResizingWindow1", 1, 0, 0, 0);
  generator1.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterResizingWindow1", 1, 1, 0, 0);
  // Resize |window2|, which is in split view with a nonempty overview grid.
  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  ui::test::EventGenerator generator2(root_windows[1], window2.get());
  generator2.PressLeftButton();
  CheckWindowResizingPerformanceHistograms("BeforeResizingWindow2", 1, 1, 0, 0);
  generator2.MoveMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("WhileResizingWindow2", 1, 1, 1, 0);
  generator2.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterResizingWindow2", 1, 1, 1, 1);
}

// Verify that the user action "SplitView_MultiDisplaySplitView" is recorded
// when multi-display split view starts, and that a value is recorded to the
// histogram "Ash.SplitView.TimeInMultiDisplaySplitView" when multi-display
// split view ends. This test does not actually examine the timing values
// recorded to the histogram, but this test does provide evidence of timing
// accuracy as the time in multi-display split view is measured from the time
// when the user action "SplitView_MultiDisplaySplitView" is recorded.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       MultiDisplaySplitViewMetrics) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  // Verifies that multi-display split view has started exactly |start_count|
  // times and ended exactly |end_count| times. If not, then the output will
  // include |description| to indicate where the test failed.
  const auto verify = [&user_action_tester, &histogram_tester](
                          const char* description, int start_count,
                          int end_count) {
    SCOPED_TRACE(description);
    EXPECT_EQ(start_count, user_action_tester.GetActionCount(
                               "SplitView_MultiDisplaySplitView"));
    histogram_tester.ExpectTotalCount(
        "Ash.SplitView.TimeInMultiDisplaySplitView", end_count);
  };

  UpdateDisplay("800x600,800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(3u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  const gfx::Rect bounds_within_root3(1600, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds_within_root3);
  SplitViewController* split_view_controller1 =
      SplitViewController::Get(root_windows[0]);
  SplitViewController* split_view_controller2 =
      SplitViewController::Get(root_windows[1]);
  SplitViewController* split_view_controller3 =
      SplitViewController::Get(root_windows[2]);
  verify("1. Unit test set up", 0, 0);
  ToggleOverview();
  split_view_controller1->SnapWindow(window1.get(), SnapPosition::kPrimary);
  verify("2. Number of displays in split view changed from 0 to 1", 0, 0);
  split_view_controller2->SnapWindow(window3.get(), SnapPosition::kPrimary);
  verify("3. Number of displays in split view changed from 1 to 2", 1, 0);
  ToggleOverview();
  verify("4. Number of displays in split view changed from 2 to 0", 1, 1);
  ToggleOverview();
  split_view_controller1->SnapWindow(window1.get(), SnapPosition::kPrimary);
  verify("5. Number of displays in split view changed from 0 to 1", 1, 1);
  split_view_controller2->SnapWindow(window3.get(), SnapPosition::kPrimary);
  verify("6. Number of displays in split view changed from 1 to 2", 2, 1);
  split_view_controller3->SnapWindow(window5.get(), SnapPosition::kPrimary);
  verify("7. Number of displays in split view changed from 2 to 3", 2, 1);
  ToggleOverview();
  verify("8. Number of displays in split view changed from 3 to 0", 2, 2);
  ToggleOverview();
  split_view_controller1->SnapWindow(window1.get(), SnapPosition::kPrimary);
  verify("9. Number of displays in split view changed from 0 to 1", 2, 2);
  split_view_controller2->SnapWindow(window3.get(), SnapPosition::kPrimary);
  verify("10. Number of displays in split view changed from 1 to 2", 3, 2);
  split_view_controller3->SnapWindow(window5.get(), SnapPosition::kPrimary);
  verify("11. Number of displays in split view changed from 2 to 3", 3, 2);
  // For good test coverage, after multi-display split view started with
  // |split_view_controller2|, now we end split view on |split_view_controller2|
  // first, and then end multi-display split view with |split_view_controller3|.
  window3.reset();
  verify("12. Number of displays in split view changed from 3 to 2", 3, 2);
  window5.reset();
  verify("13. Number of displays in split view changed from 2 to 1", 3, 3);
  window1.reset();
  verify("14. Number of displays in split view changed from 1 to 0", 3, 3);
  split_view_controller1->SnapWindow(window2.get(), SnapPosition::kPrimary);
  verify("15. Number of displays in split view changed from 0 to 1", 3, 3);
  // In this case, multi-display split view ends as soon as it starts. The
  // metrics should report that as starting and ending multi-display split view.
  split_view_controller2->SnapWindow(window4.get(), SnapPosition::kPrimary);
  verify(
      "16. Multi-display split view started by snapping last overview window",
      4, 4);
}

// Verify that |SplitViewController::CanSnapWindow| checks that the minimum size
// of the window fits into the left or top, with the default divider position.
// (If the work area length is odd, then the right or bottom will be one pixel
// larger.)
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       SnapWindowWithMinimumSizeTest) {
  UpdateDisplay("800x600,750x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  // It should make no difference which root window has the window passed to
  // |SplitViewController::CanSnapWindow|. What should matter is the root window
  // of the |SplitViewController|. To verify, we test with |bounds_within_root1|
  // and |bounds_within_root2|, and expect the same results.
  for (const gfx::Rect& bounds : {bounds_within_root1, bounds_within_root2}) {
    SCOPED_TRACE(bounds.ToString());
    aura::test::TestWindowDelegate* delegate =
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate();
    std::unique_ptr<aura::Window> window(
        CreateTestWindowInShellWithDelegate(delegate, /*id=*/-1, bounds));
    // Before setting a minimum size, expect that |window| can be snapped in
    // split view on either root window.
    EXPECT_TRUE(SplitViewController::Get(root_windows[0])
                    ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
    EXPECT_TRUE(SplitViewController::Get(root_windows[1])
                    ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
    // Either root window can accommodate a minimum size < 1/2 of the
    // work area width.
    delegate->set_minimum_size(gfx::Size(375, 0));
    EXPECT_TRUE(SplitViewController::Get(root_windows[0])
                    ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
    EXPECT_TRUE(SplitViewController::Get(root_windows[1])
                    ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
    // Only the first root window can accommodate a minimum size 396 wide.
    delegate->set_minimum_size(gfx::Size(396, 0));
    EXPECT_TRUE(SplitViewController::Get(root_windows[0])
                    ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
    EXPECT_FALSE(
        SplitViewController::Get(root_windows[1])
            ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
    // Neither root window can accommodate a minimum size > 1/2 of the work area
    // width.
    delegate->set_minimum_size(gfx::Size(401, 0));
    EXPECT_FALSE(
        SplitViewController::Get(root_windows[0])
            ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
    EXPECT_FALSE(
        SplitViewController::Get(root_windows[1])
            ->CanSnapWindow(window.get(), chromeos::kDefaultSnapRatio));
  }
}

// Verify that when in overview mode, the selector items unsnappable indicator
// shows up when expected.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       OverviewUnsnappableIndicatorVisibility) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window3 =
      CreateUnsnappableWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window4 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window5 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window6 =
      CreateUnsnappableWindow(bounds_within_root2);
  ToggleOverview();
  auto* item2 = GetOverviewItemForWindow(window2.get());
  auto* item3 = GetOverviewItemForWindow(window3.get());
  auto* item5 = GetOverviewItemForWindow(window5.get());
  auto* item6 = GetOverviewItemForWindow(window6.get());

  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  ASSERT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(GetCannotSnapWidget(item2));
  EXPECT_FALSE(GetCannotSnapWidget(item3));
  EXPECT_FALSE(GetCannotSnapWidget(item5));
  EXPECT_FALSE(GetCannotSnapWidget(item6));

  SplitViewController::Get(root_windows[0])
      ->SnapWindow(window1.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(GetCannotSnapWidget(item2));
  ASSERT_TRUE(GetCannotSnapWidget(item3));
  ui::Layer* item3_unsnappable_layer = GetCannotSnapWidget(item3)->GetLayer();
  EXPECT_EQ(1.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(GetCannotSnapWidget(item5));
  EXPECT_FALSE(GetCannotSnapWidget(item6));

  SplitViewController::Get(root_windows[1])
      ->SnapWindow(window4.get(), SnapPosition::kPrimary);
  ASSERT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_TRUE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(GetCannotSnapWidget(item2));
  EXPECT_EQ(1.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(GetCannotSnapWidget(item5));
  ASSERT_TRUE(GetCannotSnapWidget(item6));
  ui::Layer* item6_unsnappable_layer = GetCannotSnapWidget(item6)->GetLayer();
  EXPECT_EQ(1.f, item6_unsnappable_layer->opacity());

  SplitViewController::Get(root_windows[0])->EndSplitView();
  ASSERT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_TRUE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(GetCannotSnapWidget(item2));
  EXPECT_EQ(0.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(GetCannotSnapWidget(item5));
  EXPECT_EQ(1.f, item6_unsnappable_layer->opacity());

  SplitViewController::Get(root_windows[1])->EndSplitView();
  ASSERT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(GetCannotSnapWidget(item2));
  EXPECT_EQ(0.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(GetCannotSnapWidget(item5));
  EXPECT_EQ(0.f, item6_unsnappable_layer->opacity());
}

// Test that enabling the docked magnifier ends clamshell split view on all
// displays.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DockedMagnifierEndsClamshellSplitView) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds_within_root2);
  ToggleOverview();
  SplitViewController::Get(root_windows[0])
      ->SnapWindow(window1.get(), SnapPosition::kPrimary);
  SplitViewController::Get(root_windows[1])
      ->SnapWindow(window3.get(), SnapPosition::kPrimary);
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_TRUE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  Shell::Get()->docked_magnifier_controller()->SetEnabled(true);
  EXPECT_FALSE(InOverviewSession());
  EXPECT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
}

// Tests the no windows widget does not show in faster split screen setup.
TEST_F(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       NoWindowsWidget) {
  UpdateDisplay("800x600,800x600");
  const aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  // Enter overview normally. Test we show the no windows widget.
  ToggleOverview();
  EXPECT_TRUE(GetOverviewGridForRoot(root_windows[0])->no_windows_widget());
  EXPECT_TRUE(GetOverviewGridForRoot(root_windows[1])->no_windows_widget());
  ToggleOverview();

  // Test faster splitscreen setup with 1 window: Snapping the only window won't
  // start partial overview so no widget.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 200)));
  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifyNotSplitViewOrOverviewSession(w1.get());

  // Test overview -> drag to snap setup with 1 window: Overview will end so no
  // widget.
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  DragWindowTo(GetOverviewItemForWindow(w1.get()), gfx::PointF(0, 0));
  auto* window_state = WindowState::Get(w1.get());
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state->GetStateType());
  VerifyNotSplitViewOrOverviewSession(w1.get());

  // Create 2 windows on the first display, then snap to start partial overview.
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(0, 0, 200, 200)));

  // Test faster splitscreen setup with 2 windows.
  SnapOneTestWindow(w1.get(), chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  VerifySplitViewOverviewSession(w1.get());

  // TODO(b/313505530): Determine when to show the widget.
  EXPECT_FALSE(GetOverviewGridForRoot(root_windows[0])->no_windows_widget());
  EXPECT_FALSE(GetOverviewGridForRoot(root_windows[1])->no_windows_widget());

  // Exit partial overview and enter full overview.
  ToggleOverview();
  VerifyNotSplitViewOrOverviewSession(w1.get());
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  // Test overview -> drag to snap setup with 2 windows.
  DragWindowTo(GetOverviewItemForWindow(w1.get()), gfx::PointF(0, 0));
  ASSERT_EQ(chromeos::WindowStateType::kPrimarySnapped,
            window_state->GetStateType());
  VerifySplitViewOverviewSession(w1.get());

  // TODO(b/313505530): Determine when to show the widget.
  EXPECT_FALSE(GetOverviewGridForRoot(root_windows[0])->no_windows_widget());
  EXPECT_FALSE(GetOverviewGridForRoot(root_windows[1])->no_windows_widget());
}

// -----------------------------------------------------------------------------
// OverviewWallpaperTest:

// Test fixture to validate wallpaper changes within overview, including clip,
// rounded corners, and the wallpaper underlay, which is a themed solid color
// layer stacked below the wallpaper.
class OverviewWallpaperTest : public OverviewTestBase {
 public:
  OverviewWallpaperTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kForestFeature,
                              features::kDeskBarWindowOcclusionOptimization},
        /*disabled_features=*/{});
  }
  OverviewWallpaperTest(const OverviewWallpaperTest&) = delete;
  OverviewWallpaperTest& operator=(const OverviewWallpaperTest&) = delete;
  ~OverviewWallpaperTest() override = default;

  gfx::Rect GetDisplayBoundsForRootWindow(aura::Window* root_window) {
    return display::Screen::GetScreen()
        ->GetDisplayNearestWindow(root_window)
        .bounds();
  }

  ui::Layer* GetWallpaperViewLayer() {
    return Shell::GetPrimaryRootWindowController()
        ->wallpaper_widget_controller()
        ->wallpaper_view()
        ->layer();
  }

  // Test that:
  // Upon Entering Overview:
  // -Wallpaper view layer should be clipped across all displays;
  // - Wallpaper underlay layer should be visible across all displays.
  // Upon Exiting Overview:
  // - Wallpaper view layer should be restored across all displays;
  // - Wallpaper underlay layer should not be visible across all displays.
  void VerifyLayersBoundsOnAllDisplays(bool in_overview) {
    for (auto root : Shell::GetAllRootWindows()) {
      auto* wallpaper_widget_controller =
          RootWindowController::ForWindow(root)->wallpaper_widget_controller();
      auto* wallpaper_view_layer =
          wallpaper_widget_controller->wallpaper_view()->layer();
      auto* wallpaper_underlay_layer =
          wallpaper_widget_controller->wallpaper_underlay_layer();
      EXPECT_EQ(root->bounds(), wallpaper_underlay_layer->bounds());
      EXPECT_EQ(in_overview, wallpaper_underlay_layer->IsVisible());
      EXPECT_EQ(in_overview, !wallpaper_view_layer->clip_rect().IsEmpty());
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the wallpaper layer's clipping (with rounded corners) is applied
// correctly during overview sessions, restores fully upon exit, and that the
// wallpaper underlay layer's visibility refreshes properly upon entering and
// exiting overview.
TEST_F(OverviewWallpaperTest, WallpaperClipRectAndRoundedCorners) {
  const gfx::Rect display_bounds(
      GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));
  auto* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  EXPECT_FALSE(wallpaper_underlay_layer->IsVisible());
  EXPECT_EQ(display_bounds, wallpaper_view_layer->bounds());

  // Ensure the wallpaper begins with its original dimensions (matching the
  // active display) and has square corners.
  EXPECT_EQ(display_bounds, wallpaper_view_layer->bounds());
  EXPECT_TRUE(wallpaper_view_layer->clip_rect().IsEmpty());
  EXPECT_TRUE(wallpaper_view_layer->rounded_corner_radii().IsEmpty());

  // Enter overview mode and verify that the wallpaper is clipped with rounded
  // corners.
  ToggleOverview();
  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  EXPECT_EQ(overview_grid->GetWallpaperClipBounds(),
            wallpaper_view_layer->clip_rect());
  EXPECT_EQ(kWallpaperClipRoundedCornerRadii,
            wallpaper_view_layer->rounded_corner_radii());

  EXPECT_TRUE(wallpaper_underlay_layer->IsVisible());

  // Exit overview. Check that the wallpaper has been fully restored and the
  // wallpaper underlay layer becomes invisible.
  ToggleOverview();
  EXPECT_EQ(display_bounds, wallpaper_view_layer->bounds());
  EXPECT_TRUE(wallpaper_view_layer->clip_rect().IsEmpty());
  EXPECT_TRUE(wallpaper_view_layer->rounded_corner_radii().IsEmpty());

  EXPECT_FALSE(wallpaper_underlay_layer->IsVisible());
}

// Tests that the wallpaper clipping and wallpaper underlay layer refresh their
// bounds appropriately on display change.
TEST_F(OverviewWallpaperTest, DisplayChange) {
  auto* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();

  UpdateDisplay("800x600");
  const gfx::Rect display_bounds1(
      GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));

  EXPECT_EQ(display_bounds1, wallpaper_underlay_layer->bounds());
  EXPECT_EQ(display_bounds1, wallpaper_view_layer->bounds());

  UpdateDisplay("1200x900");
  const gfx::Rect display_bounds2(
      GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));
  EXPECT_EQ(display_bounds2, wallpaper_underlay_layer->bounds());
  EXPECT_EQ(display_bounds2, wallpaper_view_layer->bounds());

  ToggleOverview();
  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  EXPECT_EQ(display_bounds2, wallpaper_underlay_layer->bounds());
  EXPECT_EQ(overview_grid->GetWallpaperClipBounds(),
            wallpaper_view_layer->clip_rect());
  EXPECT_TRUE(wallpaper_underlay_layer->IsVisible());

  UpdateDisplay("800x600");
  const gfx::Rect display_bounds3(
      GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));
  EXPECT_EQ(display_bounds3, wallpaper_underlay_layer->bounds());
  EXPECT_EQ(display_bounds3, wallpaper_view_layer->bounds());
}

// Tests that when rotating display, the bounds of the clip wallpaper will be
// adjusted properly.
TEST_F(OverviewWallpaperTest, DisplayRotation) {
  UpdateDisplay("900x600");
  auto* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  auto* display_manager = Shell::Get()->display_manager();

  for (auto rotation :
       {display::Display::ROTATE_270, display::Display::ROTATE_180,
        display::Display::ROTATE_90, display::Display::ROTATE_0}) {
    display_manager->SetDisplayRotation(
        WindowTreeHostManager::GetPrimaryDisplayId(), rotation,
        display::Display::RotationSource::USER);
    const gfx::Rect display_bounds(
        GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));
    EXPECT_EQ(display_bounds, wallpaper_underlay_layer->bounds());
    EXPECT_TRUE(display_bounds.Contains(wallpaper_view_layer->bounds()));
  }
}

// Verifies that wallpaper clipping and underlay layer visibility update
// properly on multiple displays during overview transitions.
TEST_F(OverviewWallpaperTest, MultiDisplayTest) {
  UpdateDisplay("800x700,801+0-800x700,1602+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  ASSERT_EQ(3U, display_manager->GetNumDisplays());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  VerifyLayersBoundsOnAllDisplays(/*in_overview=*/true);

  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());
  VerifyLayersBoundsOnAllDisplays(/*in_overview=*/false);
}

// Tests that wallpaper clip rect updates properly on all displays on overview
// grid effective bounds change (e.g., virtual desktop bar state changes).
TEST_F(OverviewWallpaperTest, WallpaperClipRefreshWithMultiDisplay) {
  UpdateDisplay("800x700,801+0-800x700,1602+0-800x700");
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  ASSERT_EQ(3U, display_manager->GetNumDisplays());

  ToggleOverview();

  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  auto* desks_bar_view = overview_grid->desks_bar_view();

  // The virtual desks bar is at zero state initially.
  EXPECT_EQ(DeskBarViewBase::State::kZero, desks_bar_view->state());
  SCOPED_TRACE("Desks bar at zero state");
  VerifyLayersBoundsOnAllDisplays(/*in_overview=*/true);

  // Upon expanding the virtual desks bar to state 'kExpanded' by creating a new
  // desk, the wallpaper clip rect bounds will be refreshed.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  EXPECT_EQ(DeskBarViewBase::State::kExpanded, desks_bar_view->state());
  SCOPED_TRACE("Desks bar at expanded state");
  VerifyLayersBoundsOnAllDisplays(/*in_overview=*/true);

  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());
  SCOPED_TRACE("Overview exit");
  VerifyLayersBoundsOnAllDisplays(/*in_overview=*/false);
}

// Tests that the wallpaper is clipped in partial overview mode and adjusts
// correctly when the snapped window is resized.
TEST_F(OverviewWallpaperTest, PartialOverviewVisualsAndResize) {
  const gfx::Rect display_bounds(
      GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));
  auto* wallpaper_view_layer = GetWallpaperViewLayer();
  std::unique_ptr<aura::Window> win1(
      CreateAppWindow(gfx::Rect(10, 10, 100, 100)));
  std::unique_ptr<aura::Window> win2(
      CreateAppWindow(gfx::Rect(500, 10, 200, 200)));
  // Check the wallpaper's original state before initiating partial overview.
  EXPECT_EQ(display_bounds, wallpaper_view_layer->bounds());
  EXPECT_TRUE(wallpaper_view_layer->clip_rect().IsEmpty());
  EXPECT_TRUE(wallpaper_view_layer->rounded_corner_radii().IsEmpty());

  // Snap one test window to start partial overview.
  SnapOneTestWindow(win1.get(), chromeos::WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(IsInOverviewSession());
  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  // Verify that wallpaper is clipped properly with rounded corners applied in
  // partial overview.
  EXPECT_EQ(overview_grid->GetWallpaperClipBounds(),
            wallpaper_view_layer->clip_rect());
  EXPECT_EQ(kWallpaperClipRoundedCornerRadii,
            wallpaper_view_layer->rounded_corner_radii());

  // Verify that the wallpaper's clip rect updates responsively when the snapped
  // window is dragged in partial overview.
  auto* event_generator = GetEventGenerator();
  const gfx::Point drag_starting_point =
      win1.get()->GetBoundsInScreen().right_center();
  event_generator->set_current_screen_location(drag_starting_point);
  event_generator->PressLeftButton();
  event_generator->MoveMouseBy(-10, 0);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(WindowState::Get(win1.get())->is_dragged());
  EXPECT_EQ(overview_grid->GetWallpaperClipBounds(),
            wallpaper_view_layer->clip_rect());
  EXPECT_EQ(kWallpaperClipRoundedCornerRadii,
            wallpaper_view_layer->rounded_corner_radii());
  event_generator->ReleaseLeftButton();
}

// Tests that snapping a window in full Overview hides desks widgets; closing
// the window restores full Overview and shows the desks widgets again.
TEST_F(OverviewWallpaperTest, HideDesksWidgetInPartialOverview) {
  std::unique_ptr<aura::Window> win1(
      CreateAppWindow(gfx::Rect(10, 10, 200, 100)));
  std::unique_ptr<aura::Window> win2(
      CreateAppWindow(gfx::Rect(20, 20, 500, 200)));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  auto* desks_widget = overview_grid->desks_widget();
  ASSERT_TRUE(desks_widget->IsVisible());

  SnapOneTestWindow(win2.get(), chromeos::WindowStateType::kSecondarySnapped,
                    chromeos::kTwoThirdSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(desks_widget->IsVisible());

  // Verify that the desks bar remain invisible in tablet partial overview mode.
  EnterTabletMode();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(desks_widget->IsVisible());

  LeaveTabletMode();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(desks_widget->IsVisible());

  // Closing `w2` in partial overview restores full Overview mode, making desks
  // widgets visible again.
  win2.reset();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_TRUE(desks_widget->IsVisible());
}

// Tests the no windows widget doesn't show during dragging to partial overview.
// Regression test for http://b/313505530.
TEST_F(OverviewWallpaperTest, NoWindowsWidget) {
  UpdateDisplay("800x600,800x600");
  const aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);

  // Enter full overview with no windows. Test we show the no windows widget.
  ToggleOverview();
  EXPECT_TRUE(GetOverviewGridForRoot(root_windows[0])->no_windows_widget());
  EXPECT_TRUE(GetOverviewGridForRoot(root_windows[1])->no_windows_widget());
  ToggleOverview();

  // Enter full overview with windows only on display 1.
  std::unique_ptr<aura::Window> w1(CreateAppWindow(gfx::Rect(0, 0, 200, 200)));
  std::unique_ptr<aura::Window> w2(CreateAppWindow(gfx::Rect(0, 0, 200, 200)));
  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  // TODO(b/313505530): Determine whether to show the widget.
  auto* grid0 = GetOverviewGridForRoot(root_windows[0]);
  auto* grid1 = GetOverviewGridForRoot(root_windows[1]);
  EXPECT_FALSE(grid0->no_windows_widget());
  EXPECT_FALSE(grid1->no_windows_widget());
  ASSERT_TRUE(grid0->desks_widget()->IsVisible());
  ASSERT_TRUE(grid1->desks_widget()->IsVisible());

  // Start dragging. Test we don't show the widget.
  auto* event_generator = GetEventGenerator();
  auto* overview_item = GetOverviewItemForWindow(w1.get());
  DragItemToPoint(overview_item, gfx::Point(0, 0), event_generator,
                  /*by_touch_gestures=*/false, /*drop=*/false);
  EXPECT_FALSE(grid0->no_windows_widget());
  EXPECT_FALSE(grid1->no_windows_widget());

  // Release the drag. Test we don't show the widget.
  event_generator->ReleaseLeftButton();
  ASSERT_EQ(WindowStateType::kPrimarySnapped,
            WindowState::Get(w1.get())->GetStateType());
  EXPECT_FALSE(grid0->no_windows_widget());
  EXPECT_FALSE(grid1->no_windows_widget());

  // Test the split view UI is on display 1 but not display 2, with no toast on
  // display 2.
  VerifySplitViewOverviewSession(w1.get());
  EXPECT_FALSE(grid1->GetSplitViewSetupView());
}

// Tests that the wallpaper view layer clips correctly with animation upon
// entering Overview mode and that both the wallpaper view layer and underlay
// layer restore properly upon exiting.
TEST_F(OverviewWallpaperTest, WallpaperClipAnimation) {
  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  const gfx::Rect display_bounds(
      GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));

  auto* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  EXPECT_FALSE(wallpaper_underlay_layer->IsVisible());

  ui::LayerAnimator* wallpaper_view_layer_animator =
      wallpaper_view_layer->GetAnimator();
  ASSERT_FALSE(wallpaper_view_layer_animator->is_animating());
  ASSERT_EQ(display_bounds, wallpaper_view_layer->bounds());

  ToggleOverview();
  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  const auto& wallpaper_clip_bounds = overview_grid->GetWallpaperClipBounds();
  EXPECT_TRUE(wallpaper_view_layer_animator->is_animating());
  EXPECT_TRUE(
      wallpaper_view_layer->clip_rect().Contains(wallpaper_clip_bounds));
  EXPECT_TRUE(display_bounds.Contains(wallpaper_view_layer->clip_rect()));

  ui::LayerAnimationStoppedWaiter layer_animation_stopped_waiter;
  layer_animation_stopped_waiter.Wait(wallpaper_view_layer);
  EXPECT_TRUE(wallpaper_underlay_layer->IsVisible());
  EXPECT_FALSE(wallpaper_view_layer_animator->is_animating());
  EXPECT_EQ(wallpaper_view_layer->clip_rect(), wallpaper_clip_bounds);

  ToggleOverview();
  EXPECT_TRUE(wallpaper_view_layer_animator->is_animating());
  EXPECT_TRUE(display_bounds.Contains(wallpaper_view_layer->clip_rect()));

  layer_animation_stopped_waiter.Wait(wallpaper_view_layer);
  EXPECT_FALSE(wallpaper_view_layer_animator->is_animating());
  ASSERT_EQ(display_bounds, wallpaper_view_layer->bounds());
  EXPECT_FALSE(wallpaper_underlay_layer->IsVisible());
  EXPECT_TRUE(wallpaper_view_layer->clip_rect().IsEmpty());
}

// Tests that we skip the wallpaper clipping when there is a maximized window.
TEST_F(OverviewWallpaperTest, NoAnimationWithMaximizedWindow) {
  std::unique_ptr<aura::Window> window1(CreateAppWindow());
  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  ASSERT_TRUE(WindowState::Get(window1.get())->IsMaximized());

  ui::ScopedAnimationDurationScaleMode animation_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // The wallpaper is completely occluded by the maximized window + shelf here,
  // so we can optimize and skip the animation.
  ToggleOverview();
  ui::Layer* wallpaper_view_layer = GetWallpaperViewLayer();
  ui::LayerAnimator* animator = wallpaper_view_layer->GetAnimator();
  EXPECT_FALSE(animator->is_animating());

  WaitForOverviewEnterAnimation();
  VerifyLayersBoundsOnAllDisplays(/*in_overview=*/true);

  // The wallpaper is visible in overview, so it has to animate.
  ToggleOverview();
  EXPECT_TRUE(animator->is_animating());
  WaitForOverviewExitAnimation();

  // The wallpaper is not tracked as an overview exit animation. Ensure it's
  // animation is complete here. This is a no-op if the wallpaper animation
  // completes before the overview exit animations.
  ui::LayerAnimationStoppedWaiter().Wait(wallpaper_view_layer);
  VerifyLayersBoundsOnAllDisplays(/*in_overview=*/false);
}

// Tests that the shelf's opaque background transitions from visible (default)
// to invisible (overview) and back to visible (overview exit).
TEST_F(OverviewWallpaperTest, ShelfOpaqueBackground) {
  EXPECT_FALSE(display::Screen::GetScreen()->InTabletMode());
  ShelfWidget* shelf_widget = GetPrimaryShelf()->shelf_widget();
  ui::Layer* opaque_background_layer =
      shelf_widget->GetDelegateViewOpaqueBackgroundLayerForTesting();
  ASSERT_TRUE(opaque_background_layer->IsVisible());

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());
  EXPECT_FALSE(opaque_background_layer->IsVisible());

  ToggleOverview();
  ASSERT_FALSE(IsInOverviewSession());
  EXPECT_TRUE(opaque_background_layer->IsVisible());
}

// Tests that wallpaper clip rect bounds update upon virtual desk bar state
// changes, and that desk bar background is not configured.
TEST_F(OverviewWallpaperTest, VirtualDesksBarStateChange) {
  const gfx::Rect display_bounds(
      GetDisplayBoundsForRootWindow(Shell::GetPrimaryRootWindow()));
  auto* wallpaper_widget_controller =
      Shell::GetPrimaryRootWindowController()->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  EXPECT_EQ(display_bounds, wallpaper_view_layer->bounds());
  EXPECT_TRUE(wallpaper_view_layer->clip_rect().IsEmpty());
  EXPECT_TRUE(wallpaper_view_layer->rounded_corner_radii().IsEmpty());
  EXPECT_FALSE(wallpaper_underlay_layer->IsVisible());

  ToggleOverview();
  OverviewGrid* overview_grid = GetOverviewSession()->grid_list()[0].get();
  auto* desks_bar_view = overview_grid->desks_bar_view();
  // The virtual desks bar is at zero state initially.
  EXPECT_EQ(DeskBarViewBase::State::kZero, desks_bar_view->state());
  EXPECT_FALSE(desks_bar_view->background_view());
  gfx::Rect wallpaper_clip_rect_for_zero_state(
      wallpaper_view_layer->clip_rect());
  EXPECT_EQ(overview_grid->GetWallpaperClipBounds(),
            wallpaper_clip_rect_for_zero_state);
  EXPECT_EQ(kWallpaperClipRoundedCornerRadii,
            wallpaper_view_layer->rounded_corner_radii());
  EXPECT_TRUE(wallpaper_underlay_layer->IsVisible());

  // Upon expanding the virtual desks bar to state 'kExpanded' by creating a new
  // desk, the wallpaper clip rect bounds will be refreshed.
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  EXPECT_EQ(DeskBarViewBase::State::kExpanded, desks_bar_view->state());
  gfx::Rect wallpaper_clip_rect_for_expanded_state(
      wallpaper_view_layer->clip_rect());

  // The updated `wallpaper_clip_rect_for_expanded_state` will be smaller than
  // the previous `wallpaper_clip_rect_for_zero_state`.
  EXPECT_TRUE(wallpaper_clip_rect_for_zero_state.Contains(
      wallpaper_clip_rect_for_expanded_state));
  EXPECT_EQ(overview_grid->GetWallpaperClipBounds(),
            wallpaper_clip_rect_for_expanded_state);
  EXPECT_EQ(kWallpaperClipRoundedCornerRadii,
            wallpaper_view_layer->rounded_corner_radii());
  EXPECT_TRUE(wallpaper_underlay_layer->IsVisible());
}

// Tests that the wallpaper clip does not intersect the desk bar when while
// dragging an item. Regression test for http://b/339882124.
TEST_F(OverviewWallpaperTest, VerticalDeskBar) {
  UpdateDisplay("800x1200");
  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);

  auto window = CreateAppWindow(gfx::Rect(400, 400));

  ToggleOverview();

  // Move the overview item a bit so that the top and bottom snap indicators
  // show up.
  auto* item = GetOverviewItemForWindow(window.get());
  GetEventGenerator()->set_current_screen_location(
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint()));
  GetEventGenerator()->PressLeftButton();
  GetEventGenerator()->MoveMouseBy(10, 10);

  // Test that the desks bar does not intersect the wallpaper clip. The desk bar
  // is in screen bounds while the clip is in parent bounds. However, with one
  // display screen bounds are equivalent to bounds in root window, and since
  // the wallpaper view is the size of the root window, it is also in root
  // window bounds, so a conversion is unnecessary.
  auto* desks_bar_view = GetOverviewSession()->grid_list()[0]->desks_bar_view();
  auto* wallpaper_view_layer = GetWallpaperViewLayer();
  const int desk_bar_bottom = desks_bar_view->GetBoundsInScreen().bottom();
  const int clip_top = wallpaper_view_layer->clip_rect().y();
  // A little overlap is ok since the desk bar has a transparent background.
  EXPECT_NEAR(desk_bar_bottom, clip_top, 30);
}

TEST_F(OverviewWallpaperTest, CenterOverviewItems) {
  auto window1 = CreateAppWindow(gfx::Rect(0, 0, 100, 50));
  auto window2 = CreateAppWindow(gfx::Rect(20, 10, 200, 100));
  auto window3 = CreateAppWindow(gfx::Rect(30, 20, 300, 200));
  auto window4 = CreateAppWindow(gfx::Rect(40, 30, 400, 300));
  auto window5 = CreateAppWindow(gfx::Rect(50, 40, 500, 400));
  auto window6 = CreateAppWindow(gfx::Rect(60, 50, 600, 500));
  auto window7 = CreateAppWindow(gfx::Rect(70, 60, 700, 600));
  auto window8 = CreateAppWindow(gfx::Rect(80, 70, 100, 100));
  auto window9 = CreateAppWindow(gfx::Rect(90, 80, 200, 200));
  auto window10 = CreateAppWindow(gfx::Rect(100, 90, 300, 300));
  auto window11 = CreateAppWindow(gfx::Rect(110, 100, 500, 500));

  ToggleOverview();
  ASSERT_TRUE(IsInOverviewSession());

  const auto* overview_grid =
      GetOverviewGridForRoot(Shell::GetPrimaryRootWindow());
  ASSERT_TRUE(overview_grid);
  const auto& overview_items = overview_grid->item_list();
  ASSERT_EQ(overview_items.size(), 11u);

  // If the middle of the bounding box which contains the bounds of the overview
  // items is aligned with the middle of the grid, then the overview items are
  // centered.
  gfx::RectF union_bounds;
  for (const auto& overview_item : overview_items) {
    union_bounds.Union(overview_item->target_bounds());
  }

  EXPECT_NEAR(overview_grid->GetGridEffectiveBounds().CenterPoint().x(),
              union_bounds.CenterPoint().x(), 1);
}

// Tests that the drop target bounds are configured to match the overview item
// it is a placeholder for with the center overview items processing. See
// regression at http://b/330386194.
TEST_F(OverviewWallpaperTest, DropTargetBounds) {
  // Pre-add a desk to prevent the desks bar from expanding when dragging
  // starts.
  auto* desks_controller = DesksController::Get();
  desks_controller->NewDesk(DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());

  auto window0 = CreateAppWindow(gfx::Rect(10, 10, 200, 100));
  auto window1 = CreateAppWindow(gfx::Rect(20, 20, 300, 200));
  auto window2 = CreateAppWindow(gfx::Rect(30, 30, 220, 110));
  auto window3 = CreateAppWindow(gfx::Rect(30, 20, 300, 200));
  auto window4 = CreateAppWindow(gfx::Rect(40, 30, 400, 300));
  auto window5 = CreateAppWindow(gfx::Rect(50, 40, 500, 400));

  OverviewController* overview_controller = OverviewController::Get();
  overview_controller->StartOverview(OverviewStartAction::kTests,
                                     OverviewEnterExitType::kImmediateEnter);
  ASSERT_TRUE(overview_controller->InOverviewSession());

  aura::Window* primary_root_window = Shell::GetPrimaryRootWindow();
  auto* overview_grid = GetOverviewGridForRoot(primary_root_window);
  ASSERT_TRUE(overview_grid);
  const auto& item_list = overview_grid->item_list();
  ASSERT_EQ(6u, item_list.size());

  for (const auto& overview_item : item_list) {
    auto* event_generator = GetEventGenerator();
    const gfx::RectF target_bounds_before_dragging =
        overview_item->target_bounds();
    for (const bool by_touch : {false, true}) {
      DragItemToPoint(overview_item.get(),
                      primary_root_window->GetBoundsInScreen().CenterPoint(),
                      event_generator, by_touch, /*drop=*/false);
      ASSERT_TRUE(overview_controller->InOverviewSession());

      auto* drop_target = overview_grid->drop_target();
      ASSERT_TRUE(drop_target);

      // Verify that the bounds of the `drop_target` will be the same as the
      // `target_bounds_before_dragging`.
      ASSERT_EQ(gfx::RectF(drop_target->target_bounds()),
                target_bounds_before_dragging);

      if (by_touch) {
        event_generator->ReleaseTouch();
      } else {
        event_generator->ReleaseLeftButton();
      }
    }
  }
}

}  // namespace ash
