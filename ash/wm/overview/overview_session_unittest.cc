// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_session.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accelerators/exit_warning_handler.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/gestures/back_gesture/back_gesture_event_handler.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_grid_event_handler.h"
#include "ash/wm/overview/overview_highlight_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_wallpaper_controller.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/rounded_label_widget.h"
#include "ash/wm/overview/scoped_overview_transform_window.h"
#include "ash/wm/resize_shadow.h"
#include "ash/wm/resize_shadow_controller.h"
#include "ash/wm/splitview/multi_display_overview_and_split_view_test.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/tablet_mode/tablet_mode_browser_window_drag_delegate.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/tablet_mode/tablet_mode_window_resizer.h"
#include "ash/wm/window_preview_view.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/compositor/test/draw_waiter_for_test.h"
#include "ui/compositor/test/test_utils.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_utils.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

constexpr const char kActiveWindowChangedFromOverview[] =
    "WindowSelector_ActiveWindowChanged";

// Helper function to get the index of |child|, given its parent window
// |parent|.
int IndexOf(aura::Window* child, aura::Window* parent) {
  aura::Window::Windows children = parent->children();
  auto it = std::find(children.begin(), children.end(), child);
  DCHECK(it != children.end());

  return static_cast<int>(std::distance(children.begin(), it));
}

class TweenTester : public ui::LayerAnimationObserver {
 public:
  explicit TweenTester(aura::Window* window) : window_(window) {
    window->layer()->GetAnimator()->AddObserver(this);
  }

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
  aura::Window* window_;
  bool will_animate_ = false;

  DISALLOW_COPY_AND_ASSIGN(TweenTester);
};

}  // namespace

// TODO(bruthig): Move all non-simple method definitions out of class
// declaration.
class OverviewSessionTest : public MultiDisplayOverviewAndSplitViewTest {
 public:
  OverviewSessionTest() = default;
  ~OverviewSessionTest() override = default;

  // AshTestBase:
  void SetUp() override {
    MultiDisplayOverviewAndSplitViewTest::SetUp();

    aura::Env::GetInstance()->set_throttle_input_on_resize_for_testing(false);
    shelf_view_test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());
    shelf_view_test_api_->SetAnimationDuration(
        base::TimeDelta::FromMilliseconds(1));
    ScopedOverviewTransformWindow::SetImmediateCloseForTests(
        /*immediate=*/true);
    OverviewWallpaperController::SetDoNotChangeWallpaperForTests();
    PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(true);
  }
  void TearDown() override {
    PresentationTimeRecorder::SetReportPresentationTimeImmediatelyForTest(
        false);
    trace_names_.clear();
    MultiDisplayOverviewAndSplitViewTest::TearDown();
  }

  // Enters tablet mode. Needed by tests that test dragging and or splitview,
  // which are tablet mode only.
  void EnterTabletMode() {
    // Ensure calls to SetEnabledForTest complete.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
    base::RunLoop().RunUntilIdle();
  }

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) {
    const gfx::Rect window1_bounds = GetTransformedTargetBounds(window1);
    const gfx::Rect window2_bounds = GetTransformedTargetBounds(window2);
    return window1_bounds.Intersects(window2_bounds);
  }

  OverviewController* overview_controller() {
    return Shell::Get()->overview_controller();
  }

  OverviewSession* overview_session() {
    return overview_controller()->overview_session_.get();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  gfx::Rect GetTransformedBounds(aura::Window* window) {
    gfx::Rect bounds_in_screen = window->layer()->bounds();
    ::wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
    gfx::RectF bounds(bounds_in_screen);
    gfx::Transform transform(gfx::TransformAboutPivot(
        gfx::ToFlooredPoint(bounds.origin()), window->layer()->transform()));
    transform.TransformRect(&bounds);
    return ToStableSizeRoundedRect(bounds);
  }

  gfx::Rect GetTransformedTargetBounds(aura::Window* window) {
    gfx::Rect bounds_in_screen = window->layer()->GetTargetBounds();
    ::wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
    gfx::RectF bounds(bounds_in_screen);
    gfx::Transform transform(
        gfx::TransformAboutPivot(gfx::ToFlooredPoint(bounds.origin()),
                                 window->layer()->GetTargetTransform()));
    transform.TransformRect(&bounds);
    return ToStableSizeRoundedRect(bounds);
  }

  gfx::Rect GetTransformedBoundsInRootWindow(aura::Window* window) {
    gfx::RectF bounds = gfx::RectF(gfx::SizeF(window->bounds().size()));
    aura::Window* root = window->GetRootWindow();
    CHECK(window->layer());
    CHECK(root->layer());
    gfx::Transform transform;
    if (!window->layer()->GetTargetTransformRelativeTo(root->layer(),
                                                       &transform)) {
      return gfx::Rect();
    }
    transform.TransformRect(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

  void ClickWindow(aura::Window* window) {
    ui::test::EventGenerator event_generator(window->GetRootWindow(), window);
    event_generator.ClickLeftButton();
  }

  bool InOverviewSession() {
    return overview_controller()->InOverviewSession();
  }

  OverviewItem* GetDropTarget(int grid_index) {
    return overview_session()->grid_list_[grid_index]->GetDropTarget();
  }

  views::ImageButton* GetCloseButton(OverviewItem* item) {
    return item->overview_item_view_->close_button();
  }

  views::Label* GetLabelView(OverviewItem* item) {
    return item->overview_item_view_->title_label();
  }

  views::View* GetBackdropView(OverviewItem* item) {
    return item->overview_item_view_->backdrop_view();
  }

  WindowPreviewView* GetPreviewView(OverviewItem* item) {
    return item->overview_item_view_->preview_view();
  }

  float GetCloseButtonOpacity(OverviewItem* item) {
    return GetCloseButton(item)->layer()->opacity();
  }

  float GetTitlebarOpacity(OverviewItem* item) {
    return item->overview_item_view_->header_view()->layer()->opacity();
  }

  // Tests that a window is contained within a given OverviewItem, and that both
  // the window and its matching close button are within the same screen.
  void CheckWindowAndCloseButtonInScreen(aura::Window* window,
                                         OverviewItem* window_item) {
    const gfx::Rect screen_bounds =
        window_item->root_window()->GetBoundsInScreen();
    EXPECT_TRUE(window_item->Contains(window));
    EXPECT_TRUE(screen_bounds.Contains(GetTransformedTargetBounds(window)));
    EXPECT_TRUE(screen_bounds.Contains(
        GetCloseButton(window_item)->GetBoundsInScreen()));
  }

  void SetGridBounds(OverviewGrid* grid, const gfx::Rect& bounds) {
    grid->bounds_ = bounds;
  }

  gfx::Rect GetGridBounds() {
    if (overview_session())
      return overview_session()->grid_list_[0]->bounds_;

    return gfx::Rect();
  }

  const ScopedOverviewTransformWindow& transform_window(
      OverviewItem* item) const {
    return item->transform_window_;
  }

  void CheckOverviewEnterExitHistogram(const char* trace,
                                       std::vector<int>&& enter_counts,
                                       std::vector<int>&& exit_counts) {
    CheckForDuplicateTraceName(trace);

    // Overview histograms recorded via ui::ThroughputTracker is reported
    // on the next frame presented after animation stops. Wait for the next
    // frame with a 100ms timeout for the report, regardless of whether there
    // is a next frame.
    ignore_result(ui::WaitForNextFrameToBePresented(
        Shell::GetPrimaryRootWindow()->layer()->GetCompositor(),
        base::TimeDelta::FromMilliseconds(100)));

    {
      SCOPED_TRACE(trace + std::string(".Enter"));
      CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Enter",
                             std::move(enter_counts));
    }
    {
      SCOPED_TRACE(trace + std::string(".Exit"));
      CheckOverviewHistogram("Ash.Overview.AnimationSmoothness.Exit",
                             std::move(exit_counts));
    }
  }

  // Creates a window which cannot be snapped by splitview.
  std::unique_ptr<aura::Window> CreateUnsnappableWindow(
      const gfx::Rect& bounds = gfx::Rect()) {
    std::unique_ptr<aura::Window> window = CreateTestWindow(bounds);
    window->SetProperty(aura::client::kResizeBehaviorKey,
                        aura::client::kResizeBehaviorNone);
    return window;
  }

  bool HasRoundedCorner(OverviewItem* item) {
    const ui::Layer* layer = item->transform_window_.IsMinimized()
                                 ? GetPreviewView(item)->layer()
                                 : transform_window(item).window()->layer();
    return !layer->rounded_corner_radii().IsEmpty();
  }

  static void StubForTest(ExitWarningHandler* ewh) {
    ewh->stub_timer_for_test_ = true;
  }
  static bool is_ui_shown(ExitWarningHandler* ewh) { return !!ewh->widget_; }

 protected:
  void CheckForDuplicateTraceName(const char* trace) {
    DCHECK(!base::Contains(trace_names_, trace)) << trace;
    trace_names_.push_back(trace);
  }

  base::HistogramTester histograms_;

 private:
  void CheckOverviewHistogram(const char* histogram,
                              std::vector<int>&& counts) {
    ASSERT_EQ(5u, counts.size());

    histograms_.ExpectTotalCount(histogram + std::string(".ClamshellMode"),
                                 counts[0]);
    histograms_.ExpectTotalCount(
        histogram + std::string(".SingleClamshellMode"), counts[1]);
    histograms_.ExpectTotalCount(histogram + std::string(".TabletMode"),
                                 counts[2]);
    histograms_.ExpectTotalCount(
        histogram + std::string(".MinimizedTabletMode"), counts[3]);
    histograms_.ExpectTotalCount(histogram + std::string(".SplitView"),
                                 counts[4]);
  }

  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_api_;
  std::vector<std::string> trace_names_;

  DISALLOW_COPY_AND_ASSIGN(OverviewSessionTest);
};

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
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

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
  WaitForOverviewEnterAnimation();

  EXPECT_EQ(overview_session()->GetOverviewFocusWindow(),
            window_util::GetFocusedWindow());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  CheckOverviewEnterExitHistogram("Enter", {1, 0, 0, 0, 0}, {0, 0, 0, 0, 0});

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
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

// Tests that the ordering of windows is stable across different overview
// sessions even when the windows have the same bounds.
TEST_P(OverviewSessionTest, WindowsOrder) {
  std::unique_ptr<aura::Window> window1(CreateTestWindowInShellWithId(1));
  std::unique_ptr<aura::Window> window2(CreateTestWindowInShellWithId(2));
  std::unique_ptr<aura::Window> window3(CreateTestWindowInShellWithId(3));

  // The order of windows in overview mode is MRU.
  WindowState::Get(window1.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItem>>& overview1 =
      GetOverviewItemsForRoot(0);
  EXPECT_EQ(1, overview1[0]->GetWindow()->id());
  EXPECT_EQ(3, overview1[1]->GetWindow()->id());
  EXPECT_EQ(2, overview1[2]->GetWindow()->id());
  ToggleOverview();

  // Activate the second window.
  WindowState::Get(window2.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<OverviewItem>>& overview2 =
      GetOverviewItemsForRoot(0);

  // The order should be MRU.
  EXPECT_EQ(2, overview2[0]->GetWindow()->id());
  EXPECT_EQ(1, overview2[1]->GetWindow()->id());
  EXPECT_EQ(3, overview2[2]->GetWindow()->id());
  ToggleOverview();
}

// Tests selecting a window by tapping on it.
TEST_P(OverviewSessionTest, BasicGesture) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(window1.get(), window_util::GetFocusedWindow());
  ToggleOverview();
  EXPECT_EQ(overview_session()->GetOverviewFocusWindow(),
            window_util::GetFocusedWindow());
  GetEventGenerator()->GestureTapAt(
      GetTransformedTargetBounds(window2.get()).CenterPoint());
  EXPECT_EQ(window2.get(), window_util::GetFocusedWindow());
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

  // Highlight |window2| using the arrow keys. Activate it (and exit overview)
  // by pressing the return key.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ASSERT_TRUE(HighlightOverviewWindow(window2.get()));
  SendKey(ui::VKEY_RETURN);
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
  ASSERT_TRUE(HighlightOverviewWindow(window1.get()));
  SendKey(ui::VKEY_RETURN);
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
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(400, 400)));

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
  ui::MouseEvent event1(ui::ET_MOUSE_PRESSED, point1, point1,
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
  ui::MouseEvent event2(ui::ET_MOUSE_PRESSED, point2, point2,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);

  // Now the transparent window should be intercepting this event.
  EXPECT_NE(window.get(), targeter->FindTargetForEvent(root_target, &event2));
}

// Tests that clicking on the close button effectively closes the window.
TEST_P(OverviewSessionTest, CloseButton) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  std::unique_ptr<views::Widget> minimized_widget(CreateTestWidget());
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

  std::unique_ptr<views::Widget> widget = CreateTestWidget();

  ToggleOverview();
  ShellTestApi().WaitForOverviewAnimationState(
      OverviewAnimationState::kEnterAnimationComplete);
  // Click the close button.
  OverviewItem* item = GetOverviewItemForWindow(widget->GetNativeWindow());
  const gfx::Point point =
      GetCloseButton(item)->GetBoundsInScreen().CenterPoint();
  GetEventGenerator()->set_current_screen_location(point);
  GetEventGenerator()->ClickLeftButton();
  ASSERT_FALSE(widget->IsClosed());
  ASSERT_TRUE(InOverviewSession());

  // The shadow bounds are empty, which means its not visible.
  EXPECT_EQ(gfx::Rect(), item->GetShadowBoundsForTesting());
}

// Tests minimizing/unminimizing in overview mode.
TEST_P(OverviewSessionTest, MinimizeUnminimize) {
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
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
  std::unique_ptr<views::Widget> widget(CreateTestWidget());
  widget->SetBounds(gfx::Rect(650, 0, 400, 400));
  aura::Window* window2 = widget->GetNativeWindow();
  window2->SetProperty(aura::client::kTopViewInset, kHeaderHeightDp);
  views::Widget::ReparentNativeView(window2, window->parent());
  ASSERT_EQ(Shell::GetAllRootWindows()[1], window2->GetRootWindow());

  ToggleOverview();
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window2);
  gfx::Point point(bounds.right() - 5, bounds.y() + 5);
  ui::test::EventGenerator event_generator(window2->GetRootWindow(), point);

  EXPECT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());
}

// Tests entering overview mode with two windows and selecting one.
TEST_P(OverviewSessionTest, FullscreenWindow) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window1.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFullscreen());

  // Enter overview and select the fullscreen window.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("FullscreenWindowEnter1", {0, 1, 0, 0, 0},
                                  {0, 0, 0, 0, 0});
  ClickWindow(window1.get());
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFullscreen());
  CheckOverviewEnterExitHistogram("FullscreenWindowExit1", {0, 1, 0, 0, 0},
                                  {0, 1, 0, 0, 0});

  // Entering overview and selecting another window, the previous window remains
  // fullscreen.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("FullscreenWindowEnter2", {0, 2, 0, 0, 0},
                                  {0, 1, 0, 0, 0});
  ClickWindow(window2.get());
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsFullscreen());
  CheckOverviewEnterExitHistogram("FullscreenWindowExit2", {0, 2, 0, 0, 0},
                                  {1, 1, 0, 0, 0});
}

// Tests entering overview mode with maximized window.
TEST_P(OverviewSessionTest, MaximizedWindow) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window1.get());

  const WMEvent maximize_event(WM_EVENT_MAXIMIZE);
  WindowState::Get(window1.get())->OnWMEvent(&maximize_event);
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());

  // Enter overview and select the fullscreen window.
  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("MaximizedWindowEnter1", {0, 1, 0, 0, 0},
                                  {0, 0, 0, 0, 0});
  ClickWindow(window1.get());
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  CheckOverviewEnterExitHistogram("MaximizedWindowExit1", {0, 1, 0, 0, 0},
                                  {0, 1, 0, 0, 0});

  ToggleOverview();
  WaitForOverviewEnterAnimation();
  CheckOverviewEnterExitHistogram("MaximizedWindowEnter2", {0, 2, 0, 0, 0},
                                  {0, 1, 0, 0, 0});
  ClickWindow(window2.get());
  WaitForOverviewExitAnimation();
  EXPECT_TRUE(WindowState::Get(window1.get())->IsMaximized());
  CheckOverviewEnterExitHistogram("MaximizedWindowExit2", {0, 2, 0, 0, 0},
                                  {1, 1, 0, 0, 0});
}

TEST_P(OverviewSessionTest, TabletModeHistograms) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

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
TEST_P(OverviewSessionTest, FullscreenWindowTabletMode) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

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

// Tests that if a window is dragged while overview is open, the activation
// of the dragged window does not cancel overview.
TEST_P(OverviewSessionTest, ActivateDraggedWindowNotCancelOverview) {
  UpdateDisplay("800x600");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  window1->SetProperty(aura::client::kAppType,
                       static_cast<int>(AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  EXPECT_FALSE(InOverviewSession());

  // Start drag on |window1|.
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window1.get(), gfx::PointF(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_TOUCH));
  EXPECT_TRUE(InOverviewSession());

  resizer->Drag(gfx::PointF(400, 0), 0);
  EXPECT_TRUE(InOverviewSession());

  wm::ActivateWindow(window1.get());
  EXPECT_TRUE(InOverviewSession());

  resizer->CompleteDrag();
  EXPECT_FALSE(InOverviewSession());
}

// Tests that activate a non-dragged window during window drag will not cancel
// overview mode.
TEST_P(OverviewSessionTest, ActivateAnotherWindowDuringDragNotCancelOverview) {
  UpdateDisplay("800x600");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  window1->SetProperty(aura::client::kAppType,
                       static_cast<int>(AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  EXPECT_FALSE(InOverviewSession());

  // Start drag on |window1|.
  wm::ActivateWindow(window1.get());
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window1.get(), gfx::PointF(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_TOUCH));
  EXPECT_TRUE(InOverviewSession());

  // Activate |window2| should not cancel overview mode.
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(WindowState::Get(window2.get())->is_dragged());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  EXPECT_TRUE(InOverviewSession());
}

// Tests that if an overview item is dragged, the activation of the
// corresponding window does not cancel overview.
TEST_P(OverviewSessionTest, ActivateDraggedOverviewWindowNotCancelOverview) {
  UpdateDisplay("800x600");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  ToggleOverview();
  OverviewItem* item = GetOverviewItemForWindow(window.get());
  gfx::PointF drag_point = item->target_bounds().CenterPoint();
  overview_session()->InitiateDrag(item, drag_point,
                                   /*is_touch_dragging=*/false);
  drag_point.Offset(5.f, 0.f);
  overview_session()->Drag(item, drag_point);
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
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  gfx::PointF drag_point = item1->target_bounds().CenterPoint();
  overview_session()->InitiateDrag(item1, drag_point,
                                   /*is_touch_dragging=*/false);
  drag_point.Offset(5.f, 0.f);
  overview_session()->Drag(item1, drag_point);
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
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  gfx::PointF drag_point = item1->target_bounds().CenterPoint();
  overview_session()->InitiateDrag(item1, drag_point,
                                   /*is_touch_dragging=*/false);
  drag_point.Offset(5.f, 0.f);
  overview_session()->Drag(item1, drag_point);
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
  EXPECT_EQ(overview_session()->GetOverviewFocusWindow(),
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
  child->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
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
  child->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
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
  UpdateDisplay("400x400");
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());
  UpdateDisplay("400x400,400x400");
  EXPECT_FALSE(InOverviewSession());
}

// Tests removing a display during overview.
TEST_P(OverviewSessionTest, RemoveDisplay) {
  UpdateDisplay("400x400,400x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(450, 0, 100, 100)));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());
  UpdateDisplay("400x400");
  EXPECT_FALSE(InOverviewSession());
}

// Tests removing a display during overview with NON_ZERO_DURATION animation.
TEST_P(OverviewSessionTest, RemoveDisplayWithAnimation) {
  UpdateDisplay("400x400,400x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow(gfx::Rect(100, 100)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(450, 0, 100, 100)));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("400x400");
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
  SendKey(ui::VKEY_TAB);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that tab key does not cause crash if pressed just after overview
// session exits, and a child window was active before session start.
TEST_P(OverviewSessionTest,
       NoCrashOnTabAfterExitWithChildWindowInitiallyFocused) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  std::unique_ptr<aura::Window> child_window = CreateChildWindow(window.get());

  wm::ActivateWindow(child_window.get());

  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  ToggleOverview();
  SendKey(ui::VKEY_TAB);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that tab key does not cause crash if pressed just after overview
// session exits when no windows existed before starting overview session.
TEST_P(OverviewSessionTest, NoCrashOnTabAfterExitWithNoWindows) {
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  ToggleOverview();
  SendKey(ui::VKEY_TAB);
  EXPECT_FALSE(InOverviewSession());
}

// Tests that dragging a window from overview creates a drop target on the same
// display.
TEST_P(OverviewSessionTest, DropTargetOnCorrectDisplayForDraggingFromOverview) {
  UpdateDisplay("600x600,600x600");
  EnterTabletMode();
  // DisplayConfigurationObserver enables mirror mode when tablet mode is
  // enabled. Disable mirror mode to test multiple displays.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  base::RunLoop().RunUntilIdle();

  const aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  std::unique_ptr<aura::Window> primary_screen_window =
      CreateTestWindow(gfx::Rect(0, 0, 600, 600));
  ASSERT_EQ(root_windows[0], primary_screen_window->GetRootWindow());
  std::unique_ptr<aura::Window> secondary_screen_window =
      CreateTestWindow(gfx::Rect(600, 0, 600, 600));
  ASSERT_EQ(root_windows[1], secondary_screen_window->GetRootWindow());

  ToggleOverview();
  OverviewItem* primary_screen_item =
      GetOverviewItemForWindow(primary_screen_window.get());
  OverviewItem* secondary_screen_item =
      GetOverviewItemForWindow(secondary_screen_window.get());

  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  gfx::PointF drag_point = primary_screen_item->target_bounds().CenterPoint();
  overview_session()->InitiateDrag(primary_screen_item, drag_point,
                                   /*is_touch_dragging=*/true);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point.Offset(5.f, 0.f);
  overview_session()->Drag(primary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(1));
  ASSERT_TRUE(GetDropTarget(0));
  EXPECT_EQ(root_windows[0], GetDropTarget(0)->root_window());
  overview_session()->CompleteDrag(primary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point = secondary_screen_item->target_bounds().CenterPoint();
  overview_session()->InitiateDrag(secondary_screen_item, drag_point,
                                   /*is_touch_dragging=*/true);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point.Offset(5.f, 0.f);
  overview_session()->Drag(secondary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  ASSERT_TRUE(GetDropTarget(1));
  EXPECT_EQ(root_windows[1], GetDropTarget(1)->root_window());
  overview_session()->CompleteDrag(secondary_screen_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
}

// Tests that the drop target is removed if a window is destroyed while being
// dragged from the top.
TEST_P(OverviewSessionTest,
       DropTargetRemovedIfWindowDraggedFromTopIsDestroyed) {
  EnterTabletMode();
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  window->SetProperty(aura::client::kAppType,
                      static_cast<int>(AppType::BROWSER));
  std::unique_ptr<WindowResizer> resizer =
      CreateWindowResizer(window.get(), gfx::PointF(400, 0), HTCAPTION,
                          ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  ASSERT_TRUE(InOverviewSession());
  EXPECT_TRUE(GetDropTarget(0));
  resizer.reset();
  window.reset();
  ASSERT_TRUE(InOverviewSession());
  EXPECT_FALSE(GetDropTarget(0));
}

namespace {

// A simple window delegate that returns the specified hit-test code when
// requested and applies a minimum size constraint if there is one.
class TestDragWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  TestDragWindowDelegate() { set_window_component(HTCAPTION); }
  ~TestDragWindowDelegate() override = default;

 private:
  // aura::Test::TestWindowDelegate:
  void OnWindowDestroyed(aura::Window* window) override { delete this; }

  DISALLOW_COPY_AND_ASSIGN(TestDragWindowDelegate);
};

}  // namespace

// Tests that toggling overview on and off does not cancel drag.
TEST_P(OverviewSessionTest, DragDropInProgress) {
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      new TestDragWindowDelegate(), -1, gfx::Rect(100, 100)));

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
  const base::string16 window_title = base::UTF8ToUTF16("My window");
  window->SetTitle(window_title);
  ToggleOverview();
  OverviewItem* window_item = GetOverviewItemsForRoot(0).back().get();
  views::Label* label = GetLabelView(window_item);
  ASSERT_TRUE(label);

  // Verify the label matches the window title.
  EXPECT_EQ(window_title, label->GetText());

  // Update the window title and check that the label is updated, too.
  const base::string16 updated_title = base::UTF8ToUTF16("Updated title");
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
  auto* ewh = accelerator_controller->GetExitWarningHandlerForTest();
  ASSERT_TRUE(ewh);
  StubForTest(ewh);
  EXPECT_FALSE(is_ui_shown(ewh));

  ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
  event_generator.PressKey(ui::VKEY_Q, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  event_generator.ReleaseKey(ui::VKEY_Q,
                             ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  EXPECT_TRUE(is_ui_shown(ewh));
}

// Tests hitting the escape and back keys exits overview mode.
TEST_P(OverviewSessionTest, ExitOverviewWithKey) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  SendKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(overview_controller()->InOverviewSession());

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  SendKey(ui::VKEY_BROWSER_BACK);
  EXPECT_FALSE(overview_controller()->InOverviewSession());

  // Tests that in tablet mode, if we snap the only overview window, we cannot
  // exit overview mode.
  EnterTabletMode();
  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  SendKey(ui::VKEY_ESCAPE);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  SendKey(ui::VKEY_BROWSER_BACK);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
}

// Regression test for clusterfuzz crash. https://crbug.com/920568
TEST_P(OverviewSessionTest, TypeThenPressEscapeTwice) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  ToggleOverview();

  // Type some characters.
  SendKey(ui::VKEY_A);
  SendKey(ui::VKEY_B);
  SendKey(ui::VKEY_C);
  EXPECT_TRUE(overview_session()->GetOverviewFocusWindow());

  // Pressing escape twice should not crash.
  SendKey(ui::VKEY_ESCAPE);
  SendKey(ui::VKEY_ESCAPE);
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
  // Verify that by entering overview mode without windows, the no items
  // indicator appears.
  ToggleOverview();
  ASSERT_TRUE(overview_session());
  ASSERT_EQ(0u, GetOverviewItemsForRoot(0).size());
  EXPECT_TRUE(overview_session()->no_windows_widget_for_testing());
}

// Verify that the overview no windows indicator position is as expected.
TEST_P(OverviewSessionTest, NoWindowsIndicatorPosition) {
  UpdateDisplay("400x300");

  ToggleOverview();
  ASSERT_TRUE(overview_session());
  RoundedLabelWidget* no_windows_widget =
      overview_session()->no_windows_widget_for_testing();
  ASSERT_TRUE(no_windows_widget);

  // Verify that originally the label is in the center of the workspace.
  // Midpoint of height minus shelf.
  int expected_y = (300 - ShelfConfig::Get()->shelf_size()) / 2;
  EXPECT_EQ(gfx::Point(200, expected_y),
            no_windows_widget->GetWindowBoundsInScreen().CenterPoint());

  // Verify that after rotating the display, the label is centered in the
  // workspace 300x(400-shelf).
  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  expected_y = (400 - ShelfConfig::Get()->shelf_size()) / 2;
  EXPECT_EQ(gfx::Point(150, (400 - ShelfConfig::Get()->shelf_size()) / 2),
            no_windows_widget->GetWindowBoundsInScreen().CenterPoint());
}

// Tests that toggling overview on removes any resize shadows that may have been
// present.
TEST_P(OverviewSessionTest, DragMinimizedWindowHasStableSize) {
  UpdateDisplay(base::StringPrintf("1920x1200*%s", display::kDsfStr_1_777));
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  WindowState::Get(window.get())->Minimize();
  ToggleOverview();
  OverviewItem* overview_item = GetOverviewItemForWindow(window.get());
  auto* widget = overview_item->item_widget();

  gfx::Rect workarea =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  gfx::PointF drag_point(workarea.CenterPoint());
  overview_session()->InitiateDrag(overview_item, drag_point,
                                   /*is_touch_dragging=*/true);
  gfx::Size target_size =
      GetTransformedTargetBounds(widget->GetNativeWindow()).size();

  drag_point.Offset(0, 10.5f);
  overview_session()->Drag(overview_item, drag_point);
  gfx::Size new_target_size =
      GetTransformedTargetBounds(widget->GetNativeWindow()).size();
  EXPECT_EQ(target_size, new_target_size);
  target_size = new_target_size;

  drag_point.Offset(0, 10.5f);
  overview_session()->Drag(overview_item, drag_point);
  EXPECT_EQ(target_size,
            GetTransformedTargetBounds(widget->GetNativeWindow()).size());

  overview_session()->CompleteDrag(overview_item, drag_point);
}

// Tests that the bounds of the grid do not intersect the shelf or its hotseat.
TEST_P(OverviewSessionTest, OverviewGridBounds) {
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(overview_session());

  Shelf* shelf = Shelf::ForWindow(Shell::GetPrimaryRootWindow());
  const gfx::Rect shelf_bounds = shelf->GetIdealBounds();
  const gfx::Rect hotseat_bounds =
      shelf->hotseat_widget()->GetWindowBoundsInScreen();
  EXPECT_FALSE(GetGridBounds().Intersects(shelf_bounds));
  EXPECT_FALSE(GetGridBounds().Intersects(hotseat_bounds));
}

TEST_P(OverviewSessionTest, NoWindowsIndicatorPositionSplitview) {
  UpdateDisplay("400x300");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(overview_session());
  RoundedLabelWidget* no_windows_widget =
      overview_session()->no_windows_widget_for_testing();
  EXPECT_FALSE(no_windows_widget);

  // Tests that when snapping a window to the left in splitview, the no windows
  // indicator shows up in the middle of the right side of the screen.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  no_windows_widget = overview_session()->no_windows_widget_for_testing();
  ASSERT_TRUE(no_windows_widget);

  // There is a 8dp divider in splitview, the indicator should take that into
  // account.
  const int bounds_left = 200 + 4;
  int expected_x = bounds_left + (400 - (bounds_left)) / 2;
  int workarea_bottom_inset = ShelfConfig::Get()->shelf_size();
  if (chromeos::switches::ShouldShowShelfHotseat())
    workarea_bottom_inset = ShelfConfig::Get()->in_app_shelf_size();
  const int expected_y = (300 - workarea_bottom_inset) / 2;
  EXPECT_EQ(gfx::Point(expected_x, expected_y),
            no_windows_widget->GetWindowBoundsInScreen().CenterPoint());

  // Tests that when snapping a window to the right in splitview, the no windows
  // indicator shows up in the middle of the left side of the screen.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::RIGHT);
  expected_x = /*bounds_right=*/(200 - 4) / 2;
  EXPECT_EQ(gfx::Point(expected_x, expected_y),
            no_windows_widget->GetWindowBoundsInScreen().CenterPoint());
}

// Tests that the no windows indicator shows properly after adding an item.
TEST_P(OverviewSessionTest, NoWindowsIndicatorAddItem) {
  EnterTabletMode();
  std::unique_ptr<aura::Window> window(CreateTestWindow());

  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(overview_session()->no_windows_widget_for_testing());

  overview_session()->AddItem(window.get(), /*reposition=*/true,
                              /*animate=*/false, /*ignored_items=*/{},
                              /*index=*/0u);
  EXPECT_FALSE(overview_session()->no_windows_widget_for_testing());
}

// Verify that when opening overview mode with multiple displays, the no items
// indicator on the primary grid if there are no windows.
TEST_P(OverviewSessionTest, NoWindowsIndicatorPositionMultiDisplay) {
  UpdateDisplay("400x400,400x400,400x400");

  // Enter overview mode. Verify that the no windows indicator is located on the
  // primary display.
  ToggleOverview();
  ASSERT_TRUE(overview_session());
  RoundedLabelWidget* no_windows_widget =
      overview_session()->no_windows_widget_for_testing();
  const int expected_y = (400 - ShelfConfig::Get()->shelf_size()) / 2;
  EXPECT_EQ(gfx::Point(200, expected_y),
            no_windows_widget->GetWindowBoundsInScreen().CenterPoint());
}

// Tests that we do not exit overview mode until all the grids are empty.
TEST_P(OverviewSessionTest, ExitOverviewWhenAllGridsEmpty) {
  UpdateDisplay("400x400,400x400,400x400");

  // Create two windows with widgets (widgets are needed to close the windows
  // later in the test), one each on the first two monitors.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  std::unique_ptr<views::Widget> widget1(CreateTestWidget());
  std::unique_ptr<views::Widget> widget2(CreateTestWidget());
  aura::Window* window1 = widget1->GetNativeWindow();
  aura::Window* window2 = widget2->GetNativeWindow();
  ASSERT_TRUE(
      window_util::MoveWindowToDisplay(window2, GetSecondaryDisplay().id()));
  ASSERT_EQ(root_windows[0], window1->GetRootWindow());
  ASSERT_EQ(root_windows[1], window2->GetRootWindow());

  // Enter overview mode. Verify that the no windows indicator is not visible on
  // any display.
  ToggleOverview();
  auto& grids = overview_session()->grid_list();
  ASSERT_TRUE(overview_session());
  ASSERT_EQ(3u, grids.size());
  EXPECT_FALSE(overview_session()->no_windows_widget_for_testing());

  OverviewItem* item1 = GetOverviewItemForWindow(window1);
  OverviewItem* item2 = GetOverviewItemForWindow(window2);
  ASSERT_TRUE(item1 && item2);

  // Close |item2|. Verify that we are still in overview mode because |window1|
  // is still open. The non primary root grids are empty however.
  item2->CloseWindow();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(overview_session());
  ASSERT_EQ(3u, grids.size());
  EXPECT_FALSE(grids[0]->empty());
  EXPECT_TRUE(grids[1]->empty());
  EXPECT_TRUE(grids[2]->empty());
  EXPECT_FALSE(overview_session()->no_windows_widget_for_testing());

  // Close |item1|. Verify that since no windows are open, we exit overview
  // mode.
  item1->CloseWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(overview_session());
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
  auto widget = CreateTestWidget();

  TweenTester tester1(window1.get());
  TweenTester tester2(window2.get());
  TweenTester tester3(window3.get());

  ClickWindow(widget->GetNativeWindow());

  // |window1| and |window2| should animate.
  EXPECT_EQ(gfx::Tween::EASE_OUT, tester1.tween_type());
  EXPECT_EQ(gfx::Tween::EASE_OUT, tester2.tween_type());
  EXPECT_EQ(gfx::Tween::ZERO, tester3.tween_type());
}

// Tests that AlwaysOnTopWindow can be handled correctly in new overview
// animations.
// Fails consistently; see https://crbug.com/812497.
TEST_P(OverviewSessionTest, DISABLED_HandleAlwaysOnTopWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window5(CreateTestWindow(bounds));
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

  EXPECT_FALSE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window6.get())->IsFullscreen());
  EXPECT_FALSE(WindowState::Get(window7.get())->IsMaximized());

  const WMEvent toggle_maximize_event(WM_EVENT_TOGGLE_MAXIMIZE);
  WindowState::Get(window6.get())->OnWMEvent(&toggle_maximize_event);
  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  WindowState::Get(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  WindowState::Get(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(WindowState::Get(window2.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window7.get())->IsFullscreen());
  EXPECT_TRUE(WindowState::Get(window6.get())->IsMaximized());

  // Case 1: Click on |window1| to activate it and exit overview.
  std::unique_ptr<ui::ScopedAnimationDurationScaleMode> test_duration_mode =
      std::make_unique<ui::ScopedAnimationDurationScaleMode>(
          ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  ToggleOverview();
  // For entering animation, only animate |window1|, |window2|, |window3| and
  // |window5| because |window3| and |window5| are AlwaysOnTop windows and
  // |window2| is fullscreen.
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window3->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window4->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window5->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window6->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window7->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window8->layer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();

  // Click on |window1| to activate it and exit overview.
  // Should animate |window1|, |window2|, |window3| and |window5| because
  // |window3| and |window5| are AlwaysOnTop windows and |window2| is
  // fullscreen.
  ClickWindow(window1.get());
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window3->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window4->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window5->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window6->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window7->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window8->layer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();

  // Case 2: Click on |window3| to activate it and exit overview.
  // Should animate |window1|, |window2|, |window3| and |window5|.
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
  test_duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  test_duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ClickWindow(window3.get());
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window3->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window4->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window5->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window6->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window7->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window8->layer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();

  // Case 3: Click on maximized |window6| to activate it and exit overview.
  // Should animate |window6|, |window3| and |window5| because |window3| and
  // |window5| are AlwaysOnTop windows. |window6| is maximized.
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
  test_duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  test_duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ClickWindow(window6.get());
  EXPECT_FALSE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window3->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window4->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window5->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window6->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window7->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window8->layer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();

  // Case 4: Click on |window8| to activate it and exit overview.
  // Should animate |window8|, |window1|, |window2|, |window3| and |window5|
  // because |window3| and |window5| are AlwaysOnTop windows and |window2| is
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
  test_duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::ZERO_DURATION);
  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  test_duration_mode = std::make_unique<ui::ScopedAnimationDurationScaleMode>(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  ClickWindow(window8.get());
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window3->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window4->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window5->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window6->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window7->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window8->layer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();
}

// Verify that the selector item can animate after the item is dragged and
// released.
TEST_P(OverviewSessionTest, WindowItemCanAnimateOnDragRelease) {
  base::HistogramTester histogram_tester;
  UpdateDisplay("400x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EnterTabletMode();
  ToggleOverview();
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  // Drag |item2| in a way so that |window2| does not get activated.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      gfx::ToRoundedPoint(item2->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();

  generator->MoveMouseTo(gfx::Point(200, 200));
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
  UpdateDisplay("400x400");
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  EnterTabletMode();
  ToggleOverview();
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  // Start the drag on |item1|. Verify the dragged item, |item1| has both the
  // close button and titlebar hidden. The close button opacity however is
  // opaque as its a child of the header which handles fading away the whole
  // header. All other items, |item2| should only have the close button hidden.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint()));
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0.f, GetTitlebarOpacity(item1));
  EXPECT_EQ(1.f, GetCloseButtonOpacity(item1));
  EXPECT_EQ(1.f, GetTitlebarOpacity(item2));
  EXPECT_EQ(0.f, GetCloseButtonOpacity(item2));

  // Drag |item1| in a way so that |window1| does not get activated (drags
  // within a certain threshold count as clicks). Verify the close button and
  // titlebar is visible for all items.
  generator->MoveMouseTo(gfx::Point(200, 200));
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
  OverviewItem* item1 = GetOverviewItemForWindow(minimized.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window.get());
  OverviewItem* item3 = GetOverviewItemForWindow(window3.get());

  views::Widget* widget1 = item1->item_widget();
  views::Widget* widget2 = item2->item_widget();
  views::Widget* widget3 = item3->item_widget();

  // The original order of stacking is determined by the order the associated
  // window was activated.
  EXPECT_GT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget2->GetNativeWindow(), parent),
            IndexOf(widget1->GetNativeWindow(), parent));

  // Verify that the item widget is stacked below the window.
  EXPECT_LT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(minimized.get(), parent));
  EXPECT_LT(IndexOf(widget2->GetNativeWindow(), parent),
            IndexOf(window.get(), parent));
  EXPECT_LT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(window3.get(), parent));

  // Drag the first window. Verify that it's item widget is not stacked above
  // the other two.
  const gfx::Point start_drag =
      gfx::ToRoundedPoint(item1->target_bounds().CenterPoint());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_drag);
  generator->PressLeftButton();
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget3->GetNativeWindow(), parent));
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
  EXPECT_GT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget2->GetNativeWindow(), parent),
            IndexOf(widget1->GetNativeWindow(), parent));

  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.TabletMode", 2);
  histogram_tester.ExpectTotalCount(
      "Ash.Overview.WindowDrag.PresentationTime.MaxLatency.TabletMode", 1);
}

// Test that dragging a window from the top creates a drop target stacked at the
// bottom. Test that dropping into overview removes the drop target.
TEST_P(OverviewSessionTest, DropTargetStackedAtBottomForWindowDraggedFromTop) {
  UpdateDisplay("800x600");
  EnterTabletMode();
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  window1->SetProperty(aura::client::kAppType,
                       static_cast<int>(AppType::BROWSER));
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  aura::Window* parent = window1->parent();
  ASSERT_EQ(parent, window2->parent());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  std::unique_ptr<WindowResizer> resizer =
      CreateWindowResizer(window1.get(), gfx::PointF(400, 0), HTCAPTION,
                          ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  ASSERT_TRUE(GetDropTarget(0));
  EXPECT_LT(IndexOf(GetDropTarget(0)->GetWindow(), parent),
            IndexOf(window2.get(), parent));
  resizer->Drag(gfx::PointF(400, 500), ui::EF_NONE);
  resizer->CompleteDrag();
  EXPECT_FALSE(GetDropTarget(0));
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
  EXPECT_LT(IndexOf(GetDropTarget(0)->GetWindow(), parent),
            IndexOf(window2.get(), parent));
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
  OverviewItem* wide_item = GetOverviewItemForWindow(wide.get());
  OverviewItem* tall_item = GetOverviewItemForWindow(tall.get());
  OverviewItem* normal_item = GetOverviewItemForWindow(normal.get());

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
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
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
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
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

  EXPECT_TRUE(item1->GetShadowBoundsForTesting().IsEmpty());
  EXPECT_FALSE(item2->GetShadowBoundsForTesting().IsEmpty());

  // Drag to horizontally and then back to the start to avoid activating the
  // window, drag to close or entering splitview. Verify that the shadow is
  // invisible on both items during animation.
  generator->MoveMouseTo(gfx::Point(0, start_drag.y()));

  // The drop target window should be created with no shadow.
  OverviewItem* drop_target_item = GetDropTarget(0);
  ASSERT_TRUE(drop_target_item);
  EXPECT_TRUE(drop_target_item->GetShadowBoundsForTesting().IsEmpty());

  generator->MoveMouseTo(start_drag);
  generator->ReleaseLeftButton();
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(item1->GetShadowBoundsForTesting().IsEmpty());
  EXPECT_TRUE(item2->GetShadowBoundsForTesting().IsEmpty());

  // Verify that the shadow is visble again after animation is finished.
  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(item1->GetShadowBoundsForTesting().IsEmpty());
  EXPECT_FALSE(item2->GetShadowBoundsForTesting().IsEmpty());
}

// Tests that the shadows in overview mode are placed correctly.
TEST_P(OverviewSessionTest, ShadowBounds) {
  // Helper function to check if the bounds of a shadow owned by |shadow_parent|
  // is contained within the bounds of |widget|.
  auto contains = [](views::Widget* widget, OverviewItem* shadow_parent) {
    return gfx::Rect(widget->GetNativeWindow()->bounds().size())
        .Contains(shadow_parent->GetShadowBoundsForTesting());
  };

  // Helper function which returns the ratio of the shadow owned by
  // |shadow_parent| width and height.
  auto shadow_ratio = [](OverviewItem* shadow_parent) {
    gfx::RectF boundsf = gfx::RectF(shadow_parent->GetShadowBoundsForTesting());
    return boundsf.width() / boundsf.height();
  };

  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Set top view insets to 0 so it is easy to check the ratios of the
  // shadows match the ratios of the untransformed windows.
  UpdateDisplay("800x800");
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
  OverviewItem* wide_item = GetOverviewItemForWindow(wide.get());
  OverviewItem* tall_item = GetOverviewItemForWindow(tall.get());
  OverviewItem* normal_item = GetOverviewItemForWindow(normal.get());

  views::Widget* wide_widget = wide_item->item_widget();
  views::Widget* tall_widget = tall_item->item_widget();
  views::Widget* normal_widget = normal_item->item_widget();

  OverviewGrid* grid = overview_session()->grid_list()[0].get();

  // Verify all the shadows are within the bounds of their respective item
  // widgets when the overview windows are positioned without animations.
  SetGridBounds(grid, gfx::Rect(400, 800));
  grid->PositionWindows(false);
  EXPECT_TRUE(contains(wide_widget, wide_item));
  EXPECT_TRUE(contains(tall_widget, tall_item));
  EXPECT_TRUE(contains(normal_widget, normal_item));

  // Verify the shadows preserve the ratios of the original windows.
  EXPECT_NEAR(shadow_ratio(wide_item), 4.f, 0.01f);
  EXPECT_NEAR(shadow_ratio(tall_item), 0.25f, 0.01f);
  EXPECT_NEAR(shadow_ratio(normal_item), 1.f, 0.01f);

  // Verify all the shadows are within the bounds of their respective item
  // widgets when the overview windows are positioned with animations.
  SetGridBounds(grid, gfx::Rect(400, 800));
  grid->PositionWindows(true);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(contains(wide_widget, wide_item));
  EXPECT_TRUE(contains(tall_widget, tall_item));
  EXPECT_TRUE(contains(normal_widget, normal_item));

  EXPECT_NEAR(shadow_ratio(wide_item), 4.f, 0.01f);
  EXPECT_NEAR(shadow_ratio(tall_item), 0.25f, 0.01f);
  EXPECT_NEAR(shadow_ratio(normal_item), 1.f, 0.01f);

  // Test that leaving overview mode cleans up properly.
  ToggleOverview();
}

// Verify that attempting to drag with a secondary finger works as expected.
TEST_P(OverviewSessionTest, DraggingWithTwoFingers) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  EnterTabletMode();
  ToggleOverview();
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());

  const gfx::RectF original_bounds1 = item1->target_bounds();
  const gfx::RectF original_bounds2 = item2->target_bounds();

  constexpr int kTouchId1 = 1;
  constexpr int kTouchId2 = 2;

  // Dispatches a long press event at the event generators current location.
  // Long press is one way to start dragging in splitview.
  auto dispatch_long_press = [this]() {
    ui::GestureEventDetails event_details(ui::ET_GESTURE_LONG_PRESS);
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
  EXPECT_FALSE(HighlightOverviewWindow(pip_window.get()));
}

// Tests the PositionWindows function works as expected.
TEST_P(OverviewSessionTest, PositionWindows) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());

  ToggleOverview();
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  OverviewItem* item3 = GetOverviewItemForWindow(window3.get());
  const gfx::RectF bounds1 = item1->target_bounds();
  const gfx::RectF bounds2 = item2->target_bounds();
  const gfx::RectF bounds3 = item3->target_bounds();

  // Verify that the bounds remain the same when calling PositionWindows again.
  overview_session()->PositionWindows(/*animate=*/false);
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_EQ(bounds2, item2->target_bounds());
  EXPECT_EQ(bounds3, item3->target_bounds());

  // Verify that |item2| and |item3| change bounds when calling PositionWindows
  // while ignoring |item1|.
  overview_session()->PositionWindows(/*animate=*/false, {item1});
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_NE(bounds2, item2->target_bounds());
  EXPECT_NE(bounds3, item3->target_bounds());

  // Return the windows to their original bounds.
  overview_session()->PositionWindows(/*animate=*/false);

  // Verify that items that are animating before closing are ignored by
  // PositionWindows.
  item1->set_animating_to_close(true);
  item2->set_animating_to_close(true);
  overview_session()->PositionWindows(/*animate=*/false);
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_EQ(bounds2, item2->target_bounds());
  EXPECT_NE(bounds3, item3->target_bounds());
}

// Tests that overview mode is entered with kWindowDragged mode when a window is
// dragged from the top of the screen. For the purposes of this test, we use a
// browser window.
TEST_P(OverviewSessionTest, DraggingFromTopAnimation) {
  EnterTabletMode();
  std::unique_ptr<views::Widget> widget(CreateTestWidget(
      nullptr, desks_util::GetActiveDeskContainerId(), gfx::Rect(200, 200)));
  widget->GetNativeWindow()->SetProperty(aura::client::kTopViewInset, 20);

  // Drag from the the top of the app to enter overview.
  ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  WindowState* window_state = WindowState::Get(widget->GetNativeWindow());
  window_state->CreateDragDetails(event.location_f(), HTCAPTION,
                                  ::wm::WINDOW_MOVE_SOURCE_TOUCH);
  auto drag_controller = std::make_unique<TabletModeWindowResizer>(
      window_state, std::make_unique<TabletModeBrowserWindowDragDelegate>());
  ui::Event::DispatcherApi dispatch_helper(&event);
  dispatch_helper.set_target(widget->GetNativeWindow());
  drag_controller->Drag(event.location_f(), event.flags());

  ASSERT_TRUE(InOverviewSession());
  EXPECT_EQ(OverviewEnterExitType::kImmediateEnter,
            overview_session()->enter_exit_overview_type());
}

// Tests the grid bounds are as expected with different shelf auto hide
// behaviors and alignments.
TEST_P(OverviewSessionTest, GridBounds) {
  UpdateDisplay("600x600");
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(200, 200)));

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kBottom);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);

  // Test that with the bottom shelf, the grid should take up the entire display
  // minus the shelf area on the bottom regardless of auto hide behavior.
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 600, 600 - shelf_size), GetGridBounds());
  ToggleOverview();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 600, 600 - shelf_size), GetGridBounds());
  ToggleOverview();

  // Test that with the right shelf, the grid should take up the entire display
  // minus the shelf area on the right regardless of auto hide behavior.
  shelf->SetAlignment(ShelfAlignment::kRight);
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 600 - shelf_size, 600), GetGridBounds());
  ToggleOverview();

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  ToggleOverview();
  EXPECT_EQ(gfx::Rect(0, 0, 600 - shelf_size, 600), GetGridBounds());
  ToggleOverview();
}

// Tests that windows that have a backdrop can still be tapped normally.
// Regression test for crbug.com/938645.
TEST_P(OverviewSessionTest, SelectingWindowWithBackdrop) {
  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(500, 200)));

  ToggleOverview();
  OverviewItem* item = GetOverviewItemForWindow(window.get());
  ASSERT_EQ(OverviewGridWindowFillMode::kLetterBoxed,
            item->GetWindowDimensionsType());

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
    if (event->type() != ui::ET_KEY_PRESSED)
      return;

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
  SendKey(ui::VKEY_A);
  ASSERT_TRUE(test_window->HasFocus());
  EXPECT_TRUE(test_event_handler.HasSeenEvent());
  test_event_handler.Reset();

  // Keys should be eaten by overview session when entering overview mode.
  ToggleOverview();
  ASSERT_TRUE(Shell::Get()->overview_controller()->IsInStartAnimation());
  ASSERT_TRUE(test_window->HasFocus());
  SendKey(ui::VKEY_B);
  EXPECT_FALSE(test_event_handler.HasSeenEvent());
  EXPECT_TRUE(InOverviewSession());

  WaitForOverviewEnterAnimation();
  ASSERT_FALSE(Shell::Get()->overview_controller()->IsInStartAnimation());
  EXPECT_FALSE(test_window->HasFocus());

  ToggleOverview();
  SendKey(ui::VKEY_C);
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
  EXPECT_FALSE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());
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
  EXPECT_TRUE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());
}

// Tests that in tablet mode, tapping on the background in split view mode will
// be no-op.
TEST_P(OverviewSessionTest, TapOnBackgroundInSplitView) {
  EnterTabletMode();
  UpdateDisplay("800x600");
  std::unique_ptr<aura::Window> window1(CreateTestWindow());

  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  EXPECT_FALSE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());

  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Tap on the background.
  GetEventGenerator()->GestureTapAt(gfx::Point(10, 10));

  EXPECT_TRUE(InOverviewSession());
  EXPECT_FALSE(Shell::Get()->home_screen_controller()->IsHomeScreenVisible());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
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

  OverviewItem* item = GetOverviewItemForWindow(window.get());

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

  // Header is expected to be shown immediately.
  EXPECT_EQ(
      1.0f,
      item->overview_item_view()->header_view()->layer()->GetTargetOpacity());

  EXPECT_EQ(OverviewEnterExitType::kFadeInEnter,
            overview_session()->enter_exit_overview_type());
}

// Tests exiting the overview session using kFadeOutExit type.
TEST_P(OverviewSessionTest, FadeOutExit) {
  EnterTabletMode();
  // Create a test window.
  std::unique_ptr<views::Widget> test_widget(CreateTestWidget());
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  EXPECT_FALSE(test_widget->IsMinimized());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Grab the item widget before the session starts shutting down. The widget
  // should outlive the session, at least until the animations are done - given
  // that NON_ZERO_DURATION animation duration scale, it should be safe to
  // dereference the widget pointer immediately (synchronously) after the
  // session ends.
  OverviewItem* item = GetOverviewItemForWindow(test_widget->GetNativeWindow());
  views::Widget* grid_item_widget = item->item_widget();
  gfx::Rect item_bounds = grid_item_widget->GetWindowBoundsInScreen();

  ToggleOverview(OverviewEnterExitType::kFadeOutExit);
  ASSERT_FALSE(InOverviewSession());

  // The test window should be minimized as overview fade out exit starts.
  EXPECT_TRUE(test_widget->IsMinimized());

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
  auto window3 = CreateTestWindow(gfx::Rect(100, 100));
  auto window2 = CreateTestWindow(gfx::Rect(100, 100));
  auto window1 = CreateTestWindow(gfx::Rect(100, 100));

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

  // Helper that takes in a current widget and checks if the accessibility next
  // and previous focus widgets match the given.
  auto check_a11y_overrides = [](const std::string& id, views::Widget* widget,
                                 views::Widget* expected_previous,
                                 views::Widget* expected_next) -> void {
    SCOPED_TRACE(id);
    views::View* contents_view = widget->GetContentsView();
    views::ViewAccessibility& view_accessibility =
        contents_view->GetViewAccessibility();
    EXPECT_EQ(expected_previous, view_accessibility.GetPreviousFocus());
    EXPECT_EQ(expected_next, view_accessibility.GetNextFocus());
  };

  // Order should be [focus_widget, desk_widget, item_widget1, item_widget2,
  // item_widget3].
  check_a11y_overrides("focus", focus_widget, item_widget3, desk_widget);
  check_a11y_overrides("desk", desk_widget, focus_widget, item_widget1);
  check_a11y_overrides("item1", item_widget1, desk_widget, item_widget2);
  check_a11y_overrides("item2", item_widget2, item_widget1, item_widget3);
  check_a11y_overrides("item3", item_widget3, item_widget2, focus_widget);

  // Remove |window2|. The new order should be [focus_widget, desk_widget,
  // item_widget1, item_widget3].
  window2.reset();
  check_a11y_overrides("focus", focus_widget, item_widget3, desk_widget);
  check_a11y_overrides("desk", desk_widget, focus_widget, item_widget1);
  check_a11y_overrides("item1", item_widget1, desk_widget, item_widget3);
  check_a11y_overrides("item3", item_widget3, item_widget1, focus_widget);
}

class TabletModeOverviewSessionTest : public OverviewSessionTest {
 public:
  TabletModeOverviewSessionTest() = default;
  ~TabletModeOverviewSessionTest() override = default;

  TabletModeOverviewSessionTest(const TabletModeOverviewSessionTest&) = delete;
  TabletModeOverviewSessionTest& operator=(
      const TabletModeOverviewSessionTest&) = delete;

  void SetUp() override {
    OverviewSessionTest::SetUp();
    EnterTabletMode();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

 protected:
  void GenerateScrollSequence(const gfx::Point& start, const gfx::Point& end) {
    GetEventGenerator()->GestureScrollSequence(
        start, end, base::TimeDelta::FromMilliseconds(100), 1000);
  }

  void DispatchLongPress(OverviewItem* item) {
    ui::TouchEvent long_press(
        ui::ET_GESTURE_LONG_PRESS,
        gfx::ToRoundedPoint(item->target_bounds().CenterPoint()),
        base::TimeTicks::Now(),
        ui::PointerDetails(ui::EventPointerType::kTouch));
    GetEventGenerator()->Dispatch(&long_press);
  }

  // Creates |n| test windows. They are created in reverse order, so that the
  // first window in the vector is the MRU window.
  std::vector<std::unique_ptr<aura::Window>> CreateTestWindows(int n) {
    std::vector<std::unique_ptr<aura::Window>> windows(n);
    for (int i = n - 1; i >= 0; --i)
      windows[i] = CreateTestWindow();
    return windows;
  }
};

// Tests that windows are in proper positions in the new overview layout.
TEST_P(TabletModeOverviewSessionTest, CheckNewLayoutWindowPositions) {
  auto windows = CreateTestWindows(6);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  OverviewItem* item1 = GetOverviewItemForWindow(windows[0].get());
  OverviewItem* item2 = GetOverviewItemForWindow(windows[1].get());
  OverviewItem* item3 = GetOverviewItemForWindow(windows[2].get());
  OverviewItem* item4 = GetOverviewItemForWindow(windows[3].get());

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

TEST_P(TabletModeOverviewSessionTest, CheckOffscreenWindows) {
  auto windows = CreateTestWindows(8);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  OverviewItem* item0 = GetOverviewItemForWindow(windows[0].get());
  OverviewItem* item1 = GetOverviewItemForWindow(windows[1].get());
  OverviewItem* item6 = GetOverviewItemForWindow(windows[6].get());
  OverviewItem* item7 = GetOverviewItemForWindow(windows[7].get());

  const gfx::RectF screen_bounds(GetGridBounds());
  const gfx::RectF item0_bounds = item0->target_bounds();
  const gfx::RectF item1_bounds = item1->target_bounds();
  const gfx::RectF item6_bounds = item6->target_bounds();
  const gfx::RectF item7_bounds = item7->target_bounds();

  // |item6| should be in the same row of windows as |item0|, but offscreen
  // (one screen length away).
  EXPECT_FALSE(screen_bounds.Contains(item6_bounds));
  EXPECT_EQ(item0_bounds.y(), item6_bounds.y());
  // |item7| should be in the same row of windows as |item1|, but offscreen
  // and below |item6|.
  EXPECT_FALSE(screen_bounds.Contains(item7_bounds));
  EXPECT_EQ(item1_bounds.y(), item7_bounds.y());
  EXPECT_LT(item6_bounds.y(), item7_bounds.y());
}

// Tests to see if windows are not shifted if all already available windows
// fit on screen.
TEST_P(TabletModeOverviewSessionTest, CheckNoOverviewItemShift) {
  auto windows = CreateTestWindows(4);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  OverviewItem* item0 = GetOverviewItemForWindow(windows[0].get());
  const gfx::RectF before_shift_bounds = item0->target_bounds();

  GenerateScrollSequence(gfx::Point(100, 50), gfx::Point(0, 50));
  EXPECT_EQ(before_shift_bounds, item0->target_bounds());
}

// Tests to see if windows are shifted if at least one window is
// partially/completely positioned offscreen.
TEST_P(TabletModeOverviewSessionTest, CheckOverviewItemShift) {
  auto windows = CreateTestWindows(7);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  OverviewItem* item0 = GetOverviewItemForWindow(windows[0].get());
  const gfx::RectF before_shift_bounds = item0->target_bounds();

  GenerateScrollSequence(gfx::Point(100, 50), gfx::Point(0, 50));
  EXPECT_LT(item0->target_bounds(), before_shift_bounds);
}

// Tests to see if windows remain in bounds after scrolling extremely far.
TEST_P(TabletModeOverviewSessionTest, CheckOverviewItemScrollingBounds) {
  auto windows = CreateTestWindows(8);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  // Scroll an extreme amount to see if windows on the far left are still in
  // bounds. First, align the left-most window (|windows[0]|) to the left-hand
  // bound and store the item's location. Then, scroll a far amount and check to
  // see if the item moved at all.
  OverviewItem* leftmost_window = GetOverviewItemForWindow(windows[0].get());

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
  OverviewItem* rightmost_window = GetOverviewItemForWindow(windows[7].get());
  GenerateScrollSequence(gfx::Point(5000, 50), gfx::Point(0, 50));
  const gfx::RectF right_bounds = rightmost_window->target_bounds();
  GenerateScrollSequence(gfx::Point(5000, 50), gfx::Point(0, 50));
  EXPECT_EQ(right_bounds, rightmost_window->target_bounds());
}

// Tests the windows are stacked correctly when entering or exiting splitview
// while the new overivew layout is enabled.
TEST_P(TabletModeOverviewSessionTest, StackingOrderSplitviewWindow) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateUnsnappableWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  // Snap |window1| to the left and exit overview. |window3| should have higher
  // z-order now, since it is the MRU window.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ToggleOverview();
  ASSERT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  ASSERT_GT(IndexOf(window3.get(), window3->parent()),
            IndexOf(window1.get(), window1->parent()));

  // Test that on entering overview, |window3| is of a lower z-order, so that
  // when we scroll the grid, it will be seen under |window1|.
  ToggleOverview();
  EXPECT_LT(IndexOf(window3.get(), window3->parent()),
            IndexOf(window1.get(), window1->parent()));

  // Test that |window2| has a cannot snap widget indicating that it cannot be
  // snapped, and that both |window2| and the widget are lower z-order than
  // |window1|.
  views::Widget* cannot_snap_widget =
      static_cast<views::Widget*>(GetOverviewItemForWindow(window2.get())
                                      ->cannot_snap_widget_for_testing());
  ASSERT_TRUE(cannot_snap_widget);
  aura::Window* cannot_snap_window = cannot_snap_widget->GetNativeWindow();
  ASSERT_EQ(window1->parent(), cannot_snap_window->parent());
  EXPECT_LT(IndexOf(window2.get(), window2->parent()),
            IndexOf(window1.get(), window1->parent()));
  EXPECT_LT(IndexOf(cannot_snap_window, cannot_snap_window->parent()),
            IndexOf(window1.get(), window1->parent()));

  // Test that on exiting overview, |window3| becomes activated, so it returns
  // to being higher on the z-order than |window1|.
  ToggleOverview();
  EXPECT_GT(IndexOf(window3.get(), window3->parent()),
            IndexOf(window1.get(), window1->parent()));
}

// Tests the windows are remain stacked underneath the split view window after
// dragging or long pressing.
TEST_P(TabletModeOverviewSessionTest, StackingOrderAfterGestureEvent) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);

  // Tests that if we long press, but cancel the event, the window stays stacked
  // under the snapped window.
  OverviewItem* item = GetOverviewItemForWindow(window2.get());
  const gfx::PointF item_center = item->target_bounds().CenterPoint();
  DispatchLongPress(item);
  ui::GestureEvent gesture_end(item_center.x(), item_center.y(), 0,
                               ui::EventTimeForNow(),
                               ui::GestureEventDetails(ui::ET_GESTURE_END));
  item->HandleGestureEvent(&gesture_end);
  EXPECT_GT(IndexOf(window1.get(), window1->parent()),
            IndexOf(window2.get(), window2->parent()));

  // Tests that if we drag the window around, then release, the window also
  // stays stacked under the snapped window.
  ASSERT_TRUE(InOverviewSession());
  const gfx::Vector2dF delta(15.f, 15.f);
  DispatchLongPress(item);
  overview_session()->Drag(item, item_center + delta);
  overview_session()->CompleteDrag(item, item_center + delta);
  EXPECT_GT(IndexOf(window1.get(), window1->parent()),
            IndexOf(window2.get(), window2->parent()));
}

// Test that scrolling occurs if started on top of a window using the window's
// center-point as a start.
TEST_P(TabletModeOverviewSessionTest, HorizontalScrollingOnOverviewItem) {
  auto windows = CreateTestWindows(8);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  OverviewItem* leftmost_window = GetOverviewItemForWindow(windows[0].get());
  const gfx::Point topleft_window_center =
      gfx::ToRoundedPoint(leftmost_window->target_bounds().CenterPoint());
  const gfx::RectF left_bounds = leftmost_window->target_bounds();

  GenerateScrollSequence(topleft_window_center, gfx::Point(-500, 50));
  EXPECT_LT(leftmost_window->target_bounds(), left_bounds);
}

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

  OverviewItem* item = GetOverviewItemForWindow(windows[2].get());
  const gfx::Point item_center =
      gfx::ToRoundedPoint(item->target_bounds().CenterPoint());

  // Create a scroll sequence which results in a fling.
  const gfx::Vector2d shift(-200, 0);
  GetEventGenerator()->GestureScrollSequence(
      item_center, item_center + shift, base::TimeDelta::FromMilliseconds(10),
      10);

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
  const float initial_scroll_offset = grid->scroll_offset();
  float previous_scroll_offset = initial_scroll_offset;
  for (int i = 0;
       i < kMaxLoops && grid_event_handler->IsFlingInProgressForTesting();
       ++i) {
    task_environment()->FastForwardBy(base::TimeDelta::FromMilliseconds(50));
    ui::DrawWaiterForTest::WaitForCompositingStarted(compositor);

    float scroll_offset = grid->scroll_offset();
    EXPECT_LE(scroll_offset, previous_scroll_offset);
    previous_scroll_offset = scroll_offset;
  }

  EXPECT_LT(grid->scroll_offset(), initial_scroll_offset - 100.f);
}

// Tests that a vertical scroll sequence will close the window it is scrolled
// on.
TEST_P(TabletModeOverviewSessionTest, VerticalScrollingOnOverviewItem) {
  constexpr int kNumWidgets = 8;
  std::vector<std::unique_ptr<views::Widget>> widgets(kNumWidgets);
  for (int i = kNumWidgets - 1; i >= 0; --i)
    widgets[i] = CreateTestWidget();
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  OverviewItem* leftmost_window =
      GetOverviewItemForWindow(widgets[0]->GetNativeWindow());
  const gfx::Point topleft_window_center =
      gfx::ToRoundedPoint(leftmost_window->target_bounds().CenterPoint());
  const gfx::Point end_point = topleft_window_center - gfx::Vector2d(0, 300);

  GenerateScrollSequence(topleft_window_center, end_point);
  EXPECT_TRUE(widgets[0]->IsClosed());
}

// Test that scrolling occurs if we hit the associated keyboard shortcut.
TEST_P(TabletModeOverviewSessionTest, CheckScrollingWithKeyboardShortcut) {
  auto windows = CreateTestWindows(8);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  OverviewItem* leftmost_window = GetOverviewItemForWindow(windows[0].get());
  const gfx::RectF left_bounds = leftmost_window->target_bounds();

  SendKey(ui::VKEY_RIGHT, ui::EF_CONTROL_DOWN);
  EXPECT_LT(leftmost_window->target_bounds(), left_bounds);
}

// Test that tapping a window in overview closes overview mode.
TEST_P(TabletModeOverviewSessionTest, CheckWindowActivateOnTap) {
  base::UserActionTester user_action_tester;
  auto windows = CreateTestWindows(8);
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

TEST_P(TabletModeOverviewSessionTest, LayoutValidAfterRotation) {
  UpdateDisplay("1366x768");
  display::test::ScopedSetInternalDisplayId set_internal(
      Shell::Get()->display_manager(),
      display::Screen::GetScreen()->GetPrimaryDisplay().id());
  auto windows = CreateTestWindows(7);

  // Helper to determine whether a grid layout is valid. It is considered valid
  // if the left edge of the first item is close enough to the left edge of the
  // grid bounds and if the right edge of the last item is close enough to the
  // right edge of the grid bounds. Either of these being false would mean there
  // is a large padding which shouldn't be there.
  auto layout_valid = [&windows, this](int expected_padding) {
    OverviewItem* first_item = GetOverviewItemForWindow(windows.front().get());
    OverviewItem* last_item = GetOverviewItemForWindow(windows.back().get());

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
TEST_P(TabletModeOverviewSessionTest, DragOverviewWindowToSnap) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateTestWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  ASSERT_FALSE(split_view_controller()->InSplitViewMode());

  // Dispatches a long press event at the |overview_item1|'s current location to
  // start dragging in SplitView. Drags |overview_item1| to the left border of
  // the screen. SplitView should trigger and upon completing drag,
  // |overview_item1| should snap to the left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  const gfx::PointF snap_left_location =
      gfx::PointF(GetGridBounds().left_center());

  DispatchLongPress(overview_item1);
  overview_session()->Drag(
      overview_item1,
      gfx::PointF(overview_item1->target_bounds().left_center()));
  overview_session()->CompleteDrag(overview_item1, snap_left_location);

  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Dispatches a long press event at the |overview_item2|'s current location to
  // start dragging in SplitView. Drags |overview_item2| to the right border of
  // the screen. Upon completing drag, |overview_item2| should snap to the
  // right.
  OverviewItem* overview_item2 = GetOverviewItemForWindow(window2.get());
  const gfx::PointF snap_right_location =
      gfx::PointF(GetGridBounds().right_center());

  DispatchLongPress(overview_item2);
  overview_session()->Drag(
      overview_item2,
      gfx::PointF(overview_item2->target_bounds().right_center()));
  overview_session()->CompleteDrag(overview_item2, snap_right_location);

  EXPECT_FALSE(InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());
}

// Verify that if the window item has been dragged enough vertically, the window
// will be closed.
TEST_P(TabletModeOverviewSessionTest, DragToClose) {
  // This test requires a widget.
  std::unique_ptr<views::Widget> widget(CreateTestWidget());

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* item = GetOverviewItemForWindow(widget->GetNativeWindow());
  const gfx::PointF start = item->target_bounds().CenterPoint();
  ASSERT_TRUE(item);

  // This drag has not covered enough distance, so the widget is not closed and
  // we remain in overview mode.
  overview_session()->InitiateDrag(item, start, /*is_touch_dragging=*/true);
  overview_session()->Drag(item, start + gfx::Vector2dF(0, 80));
  overview_session()->CompleteDrag(item, start + gfx::Vector2dF(0, 80));
  ASSERT_TRUE(overview_session());

  // Verify that the second drag has enough vertical distance, so the widget
  // will be closed and overview mode will be exited.
  overview_session()->InitiateDrag(item, start, /*is_touch_dragging=*/true);
  overview_session()->Drag(item, start + gfx::Vector2dF(0, 180));
  overview_session()->CompleteDrag(item, start + gfx::Vector2dF(0, 180));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(overview_session());
  EXPECT_TRUE(widget->IsClosed());
}

// Verify that if the window item has been flung enough vertically, the window
// will be closed.
TEST_P(TabletModeOverviewSessionTest, FlingToClose) {
  // This test requires a widget.
  std::unique_ptr<views::Widget> widget(CreateTestWidget());

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(1u, overview_session()->grid_list()[0]->size());

  OverviewItem* item = GetOverviewItemForWindow(widget->GetNativeWindow());
  const gfx::PointF start = item->target_bounds().CenterPoint();
  ASSERT_TRUE(item);

  // Verify that items flung horizontally do not close the item.
  overview_session()->InitiateDrag(item, start, /*is_touch_dragging=*/true);
  overview_session()->Drag(item, start + gfx::Vector2dF(0, 50));
  overview_session()->Fling(item, start, 2500, 0);
  ASSERT_TRUE(overview_session());

  // Verify that items flung vertically but without enough velocity do not
  // close the item.
  overview_session()->InitiateDrag(item, start, /*is_touch_dragging=*/true);
  overview_session()->Drag(item, start + gfx::Vector2dF(0, 50));
  overview_session()->Fling(item, start, 0, 1500);
  ASSERT_TRUE(overview_session());

  // Verify that flinging the item closes it, and since it is the last item in
  // overview mode, overview mode is exited.
  overview_session()->InitiateDrag(item, start, /*is_touch_dragging=*/true);
  overview_session()->Drag(item, start + gfx::Vector2dF(0, 50));
  overview_session()->Fling(item, start, 0, 2500);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(overview_session());
  EXPECT_TRUE(widget->IsClosed());
}

// Tests that nudging occurs in the most basic case, which is we have one row
// and one item which is about to be deleted by dragging. If the item is deleted
// we still only have one row, so the other items should nudge while the item is
// being dragged.
TEST_P(TabletModeOverviewSessionTest, BasicNudging) {
  // Set up three equal windows, which take up one row on the overview grid.
  // When one of them is deleted we are still left with all the windows on one
  // row.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  OverviewItem* item3 = GetOverviewItemForWindow(window3.get());

  const gfx::RectF item1_bounds = item1->target_bounds();
  const gfx::RectF item2_bounds = item2->target_bounds();
  const gfx::RectF item3_bounds = item3->target_bounds();

  // Drag |item1| vertically. |item2| and |item3| bounds should change as they
  // should be nudging towards their final bounds.
  overview_session()->InitiateDrag(item1, item1_bounds.CenterPoint(),
                                   /*is_touch_dragging=*/true);
  overview_session()->Drag(item1,
                           item1_bounds.CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_NE(item2_bounds, item2->target_bounds());
  EXPECT_NE(item3_bounds, item3->target_bounds());

  // Drag |item1| back to its start drag location and release, so that it does
  // not get deleted.
  overview_session()->Drag(item1, item1_bounds.CenterPoint());
  overview_session()->CompleteDrag(item1, item1_bounds.CenterPoint());

  // Drag |item3| vertically. |item1| and |item2| bounds should change as they
  // should be nudging towards their final bounds.
  overview_session()->InitiateDrag(item3, item3_bounds.CenterPoint(),
                                   /*is_touch_dragging=*/true);
  overview_session()->Drag(item3,
                           item3_bounds.CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_NE(item1_bounds, item1->target_bounds());
  EXPECT_NE(item2_bounds, item2->target_bounds());
}

// Tests that no nudging occurs when the number of rows in overview mode change
// if the item to be deleted results in the overview grid to change number of
// rows.
TEST_P(TabletModeOverviewSessionTest, NoNudgingWhenNumRowsChange) {
  // Set up four equal windows, which would split into two rows in overview
  // mode. Removing one window would leave us with three windows, which only
  // takes a single row in overview.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  std::unique_ptr<aura::Window> window4 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  OverviewItem* item3 = GetOverviewItemForWindow(window3.get());
  OverviewItem* item4 = GetOverviewItemForWindow(window4.get());

  const gfx::RectF item1_bounds = item1->target_bounds();
  const gfx::RectF item2_bounds = item2->target_bounds();
  const gfx::RectF item3_bounds = item3->target_bounds();
  const gfx::RectF item4_bounds = item4->target_bounds();

  // Drag |item1| past the drag to swipe threshold. None of the other window
  // bounds should change, as none of them should be nudged.
  overview_session()->InitiateDrag(item1, item1_bounds.CenterPoint(),
                                   /*is_touch_dragging=*/true);
  overview_session()->Drag(item1,
                           item1_bounds.CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_EQ(item2_bounds, item2->target_bounds());
  EXPECT_EQ(item3_bounds, item3->target_bounds());
  EXPECT_EQ(item4_bounds, item4->target_bounds());
}

// Tests that no nudging occurs when the item to be deleted results in an item
// from the previous row to drop down to the current row, thus causing the items
// to the right of the item to be shifted right, which is visually unacceptable.
TEST_P(TabletModeOverviewSessionTest, NoNudgingWhenLastItemOnPreviousRowDrops) {
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
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* items[kWindows];
  gfx::RectF item_bounds[kWindows];
  for (int i = 0; i < kWindows; ++i) {
    items[i] = GetOverviewItemForWindow(windows[i].get());
    item_bounds[i] = items[i]->target_bounds();
  }

  // Drag the forth item past the drag to swipe threshold. None of the other
  // window bounds should change, as none of them should be nudged, because
  // deleting the fourth item will cause the third item to drop down from the
  // first row to the second.
  overview_session()->InitiateDrag(items[3], item_bounds[3].CenterPoint(),
                                   /*is_touch_dragging=*/true);
  overview_session()->Drag(
      items[3], item_bounds[3].CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_EQ(item_bounds[0], items[0]->target_bounds());
  EXPECT_EQ(item_bounds[1], items[1]->target_bounds());
  EXPECT_EQ(item_bounds[2], items[2]->target_bounds());
  EXPECT_EQ(item_bounds[4], items[4]->target_bounds());

  // Drag the fourth item back to its start drag location and release, so that
  // it does not get deleted.
  overview_session()->Drag(items[3], item_bounds[3].CenterPoint());
  overview_session()->CompleteDrag(items[3], item_bounds[3].CenterPoint());

  // Drag the first item past the drag to swipe threshold. The second and third
  // items should nudge as expected as there is no item dropping down to their
  // row. The fourth and fifth items should not nudge as they are in a different
  // row than the first item.
  overview_session()->InitiateDrag(items[0], item_bounds[0].CenterPoint(),
                                   /*is_touch_dragging=*/true);
  overview_session()->Drag(
      items[0], item_bounds[0].CenterPoint() + gfx::Vector2dF(0, 160));
  EXPECT_NE(item_bounds[1], items[1]->target_bounds());
  EXPECT_NE(item_bounds[2], items[2]->target_bounds());
  EXPECT_EQ(item_bounds[3], items[3]->target_bounds());
  EXPECT_EQ(item_bounds[4], items[4]->target_bounds());
}

// Tests that there is no crash when destroying a window during a nudge drag.
// Regression test for https://crbug.com/997335.
TEST_P(TabletModeOverviewSessionTest, DestroyWindowDuringNudge) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* item = GetOverviewItemForWindow(window1.get());
  const gfx::PointF item_center = item->target_bounds().CenterPoint();

  // Drag |item1| vertically to start nudging.
  overview_session()->InitiateDrag(item, item_center,
                                   /*is_touch_dragging=*/true);
  overview_session()->Drag(item, item_center + gfx::Vector2dF(0, 160));

  // Destroy |window2| and |window3|,then keep dragging. There should be no
  // crash.
  window2.reset();
  window3.reset();
  overview_session()->Drag(item, item_center + gfx::Vector2dF(0, 260));
}

TEST_P(TabletModeOverviewSessionTest, MultiTouch) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateTestWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  // Dispatches a long press event to start drag mode.
  OverviewItem* item = GetOverviewItemForWindow(window1.get());
  DispatchLongPress(item);
  overview_session()->Drag(item, gfx::PointF(10.f, 500.f));
  const gfx::Rect item_bounds = item->GetWindow()->GetBoundsInScreen();

  // Tap on a point on the wallpaper. Normally this would exit overview, but not
  // while a drag is underway.
  GetEventGenerator()->set_current_screen_location(gfx::Point(10, 10));
  GetEventGenerator()->PressTouch();
  GetEventGenerator()->ReleaseTouch();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(item_bounds, item->GetWindow()->GetBoundsInScreen());

  // Long press on another item, the bounds of both items should be unchanged.
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Rect item2_bounds = item2->GetWindow()->GetBoundsInScreen();
  DispatchLongPress(item2);
  EXPECT_EQ(item_bounds, item->GetWindow()->GetBoundsInScreen());
  EXPECT_EQ(item2_bounds, item2->GetWindow()->GetBoundsInScreen());

  // Clicking on a point on the wallpaper should still exit overview.
  GetEventGenerator()->set_current_screen_location(gfx::Point(10, 10));
  GetEventGenerator()->ClickLeftButton();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
}

// Test the split view and overview functionalities in tablet mode.
class SplitViewOverviewSessionTest : public OverviewSessionTest {
 public:
  SplitViewOverviewSessionTest() = default;
  ~SplitViewOverviewSessionTest() override = default;

  enum class SelectorItemLocation {
    CENTER,
    ORIGIN,
    TOP_RIGHT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT,
  };

  void SetUp() override {
    OverviewSessionTest::SetUp();
    EnterTabletMode();
  }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  bool IsDividerAnimating() {
    return split_view_controller()->IsDividerAnimating();
  }

  void SkipDividerSnapAnimation() {
    if (!IsDividerAnimating())
      return;
    split_view_controller()->StopAndShoveAnimatedDivider();
    split_view_controller()->EndResizeImpl();
    split_view_controller()->EndTabletSplitViewAfterResizingIfAppropriate();
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
        new SplitViewTestWindowDelegate, -1, bounds);
    return window;
  }

  aura::Window* CreateWindowWithMinimumSize(const gfx::Rect& bounds,
                                            const gfx::Size& size) {
    SplitViewTestWindowDelegate* delegate = new SplitViewTestWindowDelegate();
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(delegate, -1, bounds);
    delegate->set_minimum_size(size);
    return window;
  }

  gfx::Rect GetSplitViewLeftWindowBounds() {
    return split_view_controller()->GetSnappedWindowBoundsInScreen(
        SplitViewController::LEFT, split_view_controller()->left_window());
  }

  gfx::Rect GetSplitViewRightWindowBounds() {
    return split_view_controller()->GetSnappedWindowBoundsInScreen(
        SplitViewController::RIGHT, split_view_controller()->right_window());
  }

  gfx::Rect GetSplitViewDividerBounds(bool is_dragging) {
    if (!split_view_controller()->InSplitViewMode())
      return gfx::Rect();
    return split_view_controller()
        ->split_view_divider_->GetDividerBoundsInScreen(is_dragging);
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
  void DragWindowTo(OverviewItem* item,
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
        break;
    }
    overview_session()->InitiateDrag(item, start_location,
                                     /*is_touch_dragging=*/true);
    if (long_press)
      overview_session()->StartNormalDragMode(start_location);
    overview_session()->Drag(item, end_location);
    overview_session()->CompleteDrag(item, end_location);
  }

  // Drags a overview item |item| from its center point to |end_location|.
  void DragWindowTo(OverviewItem* item, const gfx::PointF& end_location) {
    DragWindowTo(item, end_location, SelectorItemLocation::CENTER, true);
  }

 private:
  class SplitViewTestWindowDelegate : public aura::test::TestWindowDelegate {
   public:
    SplitViewTestWindowDelegate() = default;
    ~SplitViewTestWindowDelegate() override = default;

    // aura::test::TestWindowDelegate:
    void OnWindowDestroying(aura::Window* window) override { window->Hide(); }
    void OnWindowDestroyed(aura::Window* window) override { delete this; }
  };

  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewSessionTest);
};

// Tests that dragging an overview item to the edge of the screen snaps the
// window. If two windows are snapped to left and right side of the screen, exit
// the overview mode.
TEST_P(SplitViewOverviewSessionTest, DragOverviewWindowToSnap) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Drag |window1| selector item to snap to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));

  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Drag |window2| selector item to attempt to snap to left. Since there is
  // already one left snapped window |window1|, |window1| will be put in
  // overview mode.
  OverviewItem* overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF(0, 0));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_TRUE(overview_controller()->overview_session()->IsWindowInOverview(
      window1.get()));

  // Drag |window3| selector item to snap to right.
  OverviewItem* overview_item3 = GetOverviewItemForWindow(window3.get());
  const gfx::PointF end_location3(GetWorkAreaInScreen(window3.get()).width(),
                                  0.f);
  DragWindowTo(overview_item3, end_location3);

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
}

// Verify the correct behavior when dragging windows in overview mode.
TEST_P(SplitViewOverviewSessionTest, OverviewDragControllerBehavior) {
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_show_press_delay_in_ms(1);

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* window_item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* window_item2 = GetOverviewItemForWindow(window2.get());

  // Verify that if a drag is orginally horizontal, the drag behavior is drag to
  // snap.
  using DragBehavior = OverviewWindowDragController::DragBehavior;
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(window_item1->target_bounds().CenterPoint()));
  generator->PressTouch();

  // Simulate a long press, which is required to snap windows.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromMilliseconds(2));
  run_loop.Run();

  OverviewWindowDragController* drag_controller =
      overview_session()->window_drag_controller();
  ASSERT_TRUE(drag_controller);
  EXPECT_EQ(DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior());
  generator->MoveTouchBy(20, 0);
  EXPECT_EQ(DragBehavior::kNormalDrag,
            drag_controller->current_drag_behavior());
  generator->ReleaseTouch();
  EXPECT_EQ(DragBehavior::kNoDrag, drag_controller->current_drag_behavior());

  // Verify that if a drag is orginally vertical, the drag behavior is drag to
  // close.
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(window_item2->target_bounds().CenterPoint()));
  generator->PressTouch();

  // Use small increments otherwise a fling event will be fired.
  for (int j = 0; j < 20; ++j)
    generator->MoveTouchBy(0, 1);

  // A new instance of drag controller gets created each time a drag starts.
  drag_controller = overview_session()->window_drag_controller();
  EXPECT_EQ(DragBehavior::kDragToClose,
            drag_controller->current_drag_behavior());
}

// Verify the window grid size changes as expected when dragging items around in
// overview mode when split view is enabled.
TEST_P(SplitViewOverviewSessionTest,
       OverviewGridSizeWhileDraggingWithSplitView) {
  // Add three windows and enter overview mode.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> window3(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  // Select window one and start the drag.
  const int window_width =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().width();
  OverviewItem* overview_item = GetOverviewItemForWindow(window1.get());
  gfx::RectF overview_item_bounds = overview_item->target_bounds();
  gfx::PointF start_location(overview_item_bounds.CenterPoint());
  overview_session()->InitiateDrag(overview_item, start_location,
                                   /*is_touch_dragging=*/false);

  // Verify that when dragged to the left, the window grid is located where the
  // right window of split view mode should be.
  const gfx::PointF left(0, 0);
  overview_session()->Drag(overview_item, left);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->left_window() == nullptr);
  EXPECT_EQ(ShrinkBoundsByHotseatInset(GetSplitViewRightWindowBounds()),
            GetGridBounds());

  // Verify that when dragged to the right, the window grid is located where the
  // left window of split view mode should be.
  const gfx::PointF right(window_width, 0);
  overview_session()->Drag(overview_item, right);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->right_window() == nullptr);
  EXPECT_EQ(ShrinkBoundsByHotseatInset(GetSplitViewLeftWindowBounds()),
            GetGridBounds());

  // Verify that when dragged to the center, the window grid is has the
  // dimensions of the work area.
  const gfx::PointF center(window_width / 2, 0);
  overview_session()->Drag(overview_item, center);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_EQ(ShrinkBoundsByHotseatInset(GetWorkAreaInScreen(window1.get())),
            GetGridBounds());

  // Snap window1 to the left and initialize dragging for window2.
  overview_session()->Drag(overview_item, left);
  overview_session()->CompleteDrag(overview_item, left);
  ASSERT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  ASSERT_EQ(window1.get(), split_view_controller()->left_window());
  overview_item = GetOverviewItemForWindow(window2.get());
  overview_item_bounds = overview_item->target_bounds();
  start_location = overview_item_bounds.CenterPoint();
  overview_session()->InitiateDrag(overview_item, start_location,
                                   /*is_touch_dragging=*/false);

  // Verify that when there is a snapped window, the window grid bounds remain
  // constant despite overview items being dragged left and right.
  overview_session()->Drag(overview_item, left);
  EXPECT_EQ(GetSplitViewRightWindowBounds(), GetGridBounds());
  overview_session()->Drag(overview_item, right);
  EXPECT_EQ(GetSplitViewRightWindowBounds(), GetGridBounds());
  overview_session()->Drag(overview_item, center);
  EXPECT_EQ(GetSplitViewRightWindowBounds(), GetGridBounds());
}

// Tests dragging a unsnappable window.
TEST_P(SplitViewOverviewSessionTest, DraggingUnsnappableAppWithSplitView) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  // The grid bounds should be the size of the root window minus the shelf.
  const gfx::Rect root_window_bounds =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen();
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())->GetIdealBounds();
  const gfx::Rect expected_grid_bounds = ShrinkBoundsByHotseatInset(
      SubtractRects(root_window_bounds, shelf_bounds));

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  // Verify that after dragging the unsnappable window to the left and right,
  // the window grid bounds do not change.
  OverviewItem* overview_item =
      GetOverviewItemForWindow(unsnappable_window.get());
  overview_session()->InitiateDrag(overview_item,
                                   overview_item->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  overview_session()->Drag(overview_item, gfx::PointF());
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  overview_session()->Drag(overview_item,
                           gfx::PointF(root_window_bounds.right(), 0.f));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  overview_session()->Drag(overview_item,
                           gfx::PointF(root_window_bounds.right() / 2.f, 0.f));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
}

// Test that if an unsnappable window is dragged from overview to where another
// window is already snapped, then there is no snap preview, and if the drag
// ends there, then there is no DCHECK failure (or crash).
TEST_P(SplitViewOverviewSessionTest,
       DragUnsnappableWindowFromOverviewToSnappedWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  ASSERT_EQ(1u, overview_session()->grid_list().size());
  OverviewGrid* overview_grid = overview_session()->grid_list()[0].get();
  OverviewItem* overview_item =
      overview_grid->GetOverviewItemContaining(unsnappable_window.get());
  overview_session()->InitiateDrag(overview_item,
                                   overview_item->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  overview_session()->Drag(overview_item, gfx::PointF());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            overview_grid->split_view_drag_indicators()
                ->current_window_dragging_state());
  overview_session()->CompleteDrag(overview_item, gfx::PointF());
}

TEST_P(SplitViewOverviewSessionTest, Clipping) {
  // Helper to check if two rectangles have roughly the same aspect ratio. They
  // may be off by a bit due to insets but should have roughly the same shape.
  auto aspect_ratio_near = [](const gfx::Rect& rect1, const gfx::Rect& rect2) {
    DCHECK_GT(rect1.height(), 0);
    DCHECK_GT(rect2.height(), 0);
    constexpr float kEpsilon = 0.05f;
    const float rect1_aspect_ratio =
        float{rect1.width()} / float{rect1.height()};
    const float rect2_aspect_ratio =
        float{rect2.width()} / float{rect2.height()};
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
            SplitViewController::SnapPosition::RIGHT,
            /*window_for_minimum_size=*/nullptr);

    ToggleOverview();

    // Tests that after entering overview, windows with no top inset and
    // minimized windows still have no clip.
    ASSERT_TRUE(overview_controller()->InOverviewSession());
    EXPECT_EQ(clipping1, window1->layer()->clip_rect());
    EXPECT_EQ(clipping2, window2->layer()->clip_rect());
    EXPECT_EQ(clipping3, window3->layer()->clip_rect());
    EXPECT_NE(clipping4, window4->layer()->clip_rect());
    const gfx::Rect overview_clipping4 = window4->layer()->clip_rect();

    OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
    OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
    OverviewItem* item3 = GetOverviewItemForWindow(window3.get());
    OverviewItem* item4 = GetOverviewItemForWindow(window4.get());
    overview_session()->InitiateDrag(item1,
                                     item1->target_bounds().CenterPoint(),
                                     /*is_touch_dragging=*/false);

    // Tests that after we drag to a preview area, the items target bounds have
    // a matching aspect ratio to what the window would have if it were to be
    // snapped in splitview. The window clipping should match this, but the
    // windows regular bounds remain unchanged (maximized).
    overview_session()->Drag(item1, gfx::PointF());
    EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
              overview_session()
                  ->grid_list()[0]
                  ->split_view_drag_indicators()
                  ->current_window_dragging_state());
    EXPECT_FALSE(window2->layer()->clip_rect().IsEmpty());
    EXPECT_TRUE(aspect_ratio_near(window2->layer()->clip_rect(),
                                  split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item2->GetWindowTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window2->GetBoundsInScreen(), maximized_bounds));

    // The actual window of a minimized window should not be clipped. The
    // clipped layer will be the WindowPreviewView of the associated
    // OverviewItemView.
    EXPECT_TRUE(window3->layer()->clip_rect().IsEmpty());
    ui::Layer* preview_layer =
        item3->overview_item_view()->preview_view()->layer();
    EXPECT_FALSE(preview_layer->clip_rect().IsEmpty());
    EXPECT_FALSE(preview_layer->transform().IsIdentity());
    // The clip rect is affected by |preview_layer|'s transform so apply it.
    gfx::RectF clip_rect3_f(preview_layer->clip_rect());
    preview_layer->transform().TransformRect(&clip_rect3_f);
    const gfx::Rect clip_rects3 = gfx::ToEnclosedRect(clip_rect3_f);
    EXPECT_TRUE(aspect_ratio_near(clip_rects3, split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item3->GetWindowTargetBoundsWithInsets()),
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
        gfx::ToEnclosedRect(item4->GetWindowTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window4->GetBoundsInScreen(), maximized_bounds));

    // Tests that after snapping, the aspect ratios should be the same as being
    // in the preview area.
    overview_session()->CompleteDrag(item1, gfx::PointF());
    ASSERT_EQ(SplitViewController::State::kLeftSnapped,
              split_view_controller()->state());
    EXPECT_FALSE(window2->layer()->clip_rect().IsEmpty());
    EXPECT_TRUE(aspect_ratio_near(window2->layer()->clip_rect(),
                                  split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item2->GetWindowTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window2->GetBoundsInScreen(), maximized_bounds));

    EXPECT_TRUE(window3->layer()->clip_rect().IsEmpty());
    EXPECT_TRUE(aspect_ratio_near(clip_rects3, split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item3->GetWindowTargetBoundsWithInsets()),
        split_view_bounds_right));
    EXPECT_TRUE(
        aspect_ratio_near(window3->GetBoundsInScreen(), maximized_bounds));

    EXPECT_FALSE(window4->layer()->clip_rect().IsEmpty());
    EXPECT_NE(overview_clipping4, window4->layer()->clip_rect());
    EXPECT_TRUE(aspect_ratio_near(window4->layer()->clip_rect(),
                                  split_view_bounds_right));
    EXPECT_TRUE(aspect_ratio_near(
        gfx::ToEnclosedRect(item4->GetWindowTargetBoundsWithInsets()),
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
TEST_P(SplitViewOverviewSessionTest, NoClippingWhenSplitviewDisabled) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Splitview is disabled when chromeVox is enabled.
  Shell::Get()->accessibility_controller()->SetSpokenFeedbackEnabled(
      true, A11Y_NOTIFICATION_NONE);
  ASSERT_FALSE(ShouldAllowSplitView());
  const gfx::Rect clipping1 = window1->layer()->clip_rect();
  const gfx::Rect clipping2 = window2->layer()->clip_rect();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(clipping1, window1->layer()->clip_rect());
  EXPECT_EQ(clipping2, window2->layer()->clip_rect());

  // Drag to the edge of the screen. There should be no clipping and no crash.
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  overview_session()->InitiateDrag(item1, item1->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  overview_session()->Drag(item1, gfx::PointF());
  EXPECT_EQ(clipping1, window1->layer()->clip_rect());
  EXPECT_EQ(clipping2, window2->layer()->clip_rect());
}

// Tests that if there is only one window in the MRU window list in the overview
// mode, snapping the window to one side of the screen will not end the overview
// mode even if there is no more window left in the overview window grid.
TEST_P(SplitViewOverviewSessionTest, EmptyWindowsListNotExitOverview) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(overview_controller()->InOverviewSession());

  // Drag |window1| selector item to snap to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());

  // Test that overview mode is active in this single window case.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_TRUE(overview_controller()->InOverviewSession());

  // Create a new window should exit the overview mode.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  // If there are only 2 snapped windows, close one of them should enter
  // overview mode.
  window2.reset();
  EXPECT_TRUE(overview_controller()->InOverviewSession());

  // If there are more than 2 windows in overview
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window4.get());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  ToggleOverview();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  window3.reset();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  window4.reset();
  EXPECT_TRUE(overview_controller()->InOverviewSession());

  // Test that if there is only 1 snapped window, and no window in the overview
  // grid, ToggleOverview() can't end overview.
  ToggleOverview();
  EXPECT_TRUE(overview_controller()->InOverviewSession());

  EndSplitView();
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller()->InOverviewSession());

  // Test that ToggleOverview() can end overview if we're not in split view
  // mode.
  ToggleOverview();
  EXPECT_FALSE(overview_controller()->InOverviewSession());

  // Now enter overview and split view again. Test that exiting tablet mode can
  // end split view and overview correctly.
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller()->InOverviewSession());

  // Test that closing all windows in overview can end overview if we're not in
  // split view mode.
  ToggleOverview();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  window1.reset();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
}

// Tests using Alt+[ on a maximized window.
TEST_P(SplitViewOverviewSessionTest, AltLeftSquareBracketOnMaximizedWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  wm::ActivateWindow(snapped_window.get());
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kMaximized, snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(InOverviewSession());
  const WMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_LEFT);
  snapped_window_state->OnWMEvent(&alt_left_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kLeftSnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(InOverviewSession());
}

// Tests using Alt+] on a maximized window.
TEST_P(SplitViewOverviewSessionTest, AltRightSquareBracketOnMaximizedWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  wm::ActivateWindow(snapped_window.get());
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kMaximized, snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller()->state());
  EXPECT_FALSE(InOverviewSession());
  const WMEvent alt_right_square_bracket(WM_EVENT_CYCLE_SNAP_RIGHT);
  snapped_window_state->OnWMEvent(&alt_right_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kRightSnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->right_window());
  EXPECT_TRUE(InOverviewSession());
}

// Tests using Alt+[ and Alt+] on an unsnappable window.
TEST_P(SplitViewOverviewSessionTest, AltSquareBracketOnUnsnappableWindow) {
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
  const WMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_LEFT);
  unsnappable_window_state->OnWMEvent(&alt_left_square_bracket);
  expect_unsnappable_window_is_active_and_maximized();
  const WMEvent alt_right_square_bracket(WM_EVENT_CYCLE_SNAP_RIGHT);
  unsnappable_window_state->OnWMEvent(&alt_right_square_bracket);
  expect_unsnappable_window_is_active_and_maximized();
}

// Tests using Alt+[ on a left snapped window, and Alt+] on a right snapped
// window.
TEST_P(SplitViewOverviewSessionTest, AltSquareBracketOnSameSideSnappedWindow) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  const auto test_unsnapping_window1 = [this,
                                        &window1](WMEventType event_type) {
    wm::ActivateWindow(window1.get());
    WindowState* window1_state = WindowState::Get(window1.get());
    const WMEvent event(event_type);
    window1_state->OnWMEvent(&event);
    EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
    EXPECT_EQ(WindowStateType::kMaximized, window1_state->GetStateType());
    EXPECT_FALSE(split_view_controller()->InSplitViewMode());
    EXPECT_FALSE(InOverviewSession());
  };
  // Test Alt+[ with active window snapped on left and overview on right.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_LEFT);
  // Test Alt+] with active window snapped on right and overview on left.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_RIGHT);
  // Test Alt+[ with active window snapped on left and other window snapped on
  // right, if the left window is the default snapped window.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_LEFT);
  // Test Alt+[ with active window snapped on left and other window snapped on
  // right, if the right window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_LEFT);
  // Test Alt+] with active window snapped on right and other window snapped on
  // left, if the left window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_RIGHT);
  // Test Alt+] with active window snapped on right and other window snapped on
  // left, if the right window is the default snapped window.
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  test_unsnapping_window1(WM_EVENT_CYCLE_SNAP_RIGHT);
}

// Tests using Alt+[ on a right snapped window, and Alt+] on a left snapped
// window.
TEST_P(SplitViewOverviewSessionTest,
       AltSquareBracketOnOppositeSideSnappedWindow) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  const auto test_left_snapping_window1 = [this, &window1, &window2]() {
    wm::ActivateWindow(window1.get());
    WindowState* window1_state = WindowState::Get(window1.get());
    const WMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_LEFT);
    window1_state->OnWMEvent(&alt_left_square_bracket);
    EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
    EXPECT_EQ(WindowStateType::kLeftSnapped, window1_state->GetStateType());
    EXPECT_EQ(SplitViewController::State::kLeftSnapped,
              split_view_controller()->state());
    EXPECT_EQ(window1.get(), split_view_controller()->left_window());
    ASSERT_TRUE(InOverviewSession());
    EXPECT_TRUE(GetOverviewItemForWindow(window2.get()));
  };
  const auto test_right_snapping_window1 = [this, &window1, &window2]() {
    wm::ActivateWindow(window1.get());
    WindowState* window1_state = WindowState::Get(window1.get());
    const WMEvent alt_right_square_bracket(WM_EVENT_CYCLE_SNAP_RIGHT);
    window1_state->OnWMEvent(&alt_right_square_bracket);
    EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
    EXPECT_EQ(WindowStateType::kRightSnapped, window1_state->GetStateType());
    EXPECT_EQ(SplitViewController::State::kRightSnapped,
              split_view_controller()->state());
    EXPECT_EQ(window1.get(), split_view_controller()->right_window());
    ASSERT_TRUE(InOverviewSession());
    EXPECT_TRUE(GetOverviewItemForWindow(window2.get()));
  };
  // Test Alt+[ with active window snapped on right and overview on left.
  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  test_left_snapping_window1();
  // Test Alt+] with active window snapped on left and overview on right.
  test_right_snapping_window1();
  // Test Alt+[ with active window snapped on right and other window snapped on
  // left, if the right window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  test_left_snapping_window1();
  // Test Alt+] with active window snapped on left and other window snapped on
  // right, if the left window is the default snapped window.
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  test_right_snapping_window1();
  // Test Alt+[ with active window snapped on right and other window snapped on
  // left, if the left window is the default snapped window.
  EndSplitView();
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  test_left_snapping_window1();
  // Test Alt+] with active window snapped on left and other window snapped on
  // right, if the right window is the default snapped window.
  EndSplitView();
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  test_right_snapping_window1();
}

// Test the overview window drag functionalities when screen rotates.
TEST_P(SplitViewOverviewSessionTest, SplitViewRotationTest) {
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
            OrientationLockType::kLandscapePrimary);

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Test that dragging |window1| to the left of the screen snaps it to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to right.
  OverviewItem* overview_item2 = GetOverviewItemForWindow(window2.get());
  gfx::Rect work_area_rect = GetWorkAreaInScreen(window2.get());
  gfx::PointF end_location2(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  // Test that |left_window_| was snapped to left after rotated 0 degree.
  gfx::Rect left_window_bounds =
      split_view_controller()->left_window()->GetBoundsInScreen();
  EXPECT_EQ(left_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(left_window_bounds.y(), work_area_rect.y());
  EndSplitView();

  // Rotate the screen by 270 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitPrimary);
  ToggleOverview();

  // Test that dragging |window1| to the top of the screen snaps it to left.
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to right.
  overview_item2 = GetOverviewItemForWindow(window2.get());
  work_area_rect = GetWorkAreaInScreen(window2.get());
  end_location2 = gfx::PointF(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  // Test that |left_window_| was snapped to top after rotated 270 degree.
  left_window_bounds =
      split_view_controller()->left_window()->GetBoundsInScreen();
  EXPECT_EQ(left_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(left_window_bounds.y(), work_area_rect.y());
  EndSplitView();

  // Rotate the screen by 180 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_180,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kLandscapeSecondary);
  ToggleOverview();

  // Test that dragging |window1| to the left of the screen snaps it to right.
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to left.
  overview_item2 = GetOverviewItemForWindow(window2.get());
  work_area_rect = GetWorkAreaInScreen(window2.get());
  end_location2 = gfx::PointF(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());

  // Test that |right_window_| was snapped to left after rotated 180 degree.
  gfx::Rect right_window_bounds =
      split_view_controller()->right_window()->GetBoundsInScreen();
  EXPECT_EQ(right_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(right_window_bounds.y(), work_area_rect.y());
  EndSplitView();

  // Rotate the screen by 90 degree.
  test_api.SetDisplayRotation(display::Display::ROTATE_90,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(test_api.GetCurrentOrientation(),
            OrientationLockType::kPortraitSecondary);
  ToggleOverview();

  // Test that dragging |window1| to the top of the screen snaps it to right.
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to left.
  overview_item2 = GetOverviewItemForWindow(window2.get());
  work_area_rect = GetWorkAreaInScreen(window2.get());
  end_location2 = gfx::PointF(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kBothSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());

  // Test that |right_window_| was snapped to top after rotated 90 degree.
  right_window_bounds =
      split_view_controller()->right_window()->GetBoundsInScreen();
  EXPECT_EQ(right_window_bounds.x(), work_area_rect.x());
  EXPECT_EQ(right_window_bounds.y(), work_area_rect.y());
  EndSplitView();
}

// Test that when split view mode and overview mode are both active at the same
// time, dragging the split view divider resizes the bounds of snapped window
// and the bounds of overview window grids at the same time.
TEST_P(SplitViewOverviewSessionTest, SplitViewOverviewBothActiveTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  const gfx::Rect window1_bounds = window1->GetBoundsInScreen();
  const gfx::Rect overview_grid_bounds = GetGridBounds();
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(false /* is_dragging */);

  // Test that window1, divider, overview grid are aligned horizontally.
  EXPECT_EQ(window1_bounds.right(), divider_bounds.x());
  EXPECT_EQ(divider_bounds.right(), overview_grid_bounds.x());

  const gfx::Point resize_start_location(divider_bounds.CenterPoint());
  split_view_controller()->StartResize(resize_start_location);
  const gfx::Point resize_end_location(300, 0);
  split_view_controller()->EndResize(resize_end_location);
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
TEST_P(SplitViewOverviewSessionTest, SelectUnsnappableWindowInSplitView) {
  // Create one snappable and one unsnappable window.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  // Snap the snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());

  // Select the unsnappable window.
  OverviewItem* overview_item =
      GetOverviewItemForWindow(unsnappable_window.get());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  generator->ClickLeftButton();

  // Verify that we are out of split view and overview mode, and that the active
  // window is the unsnappable window.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_EQ(unsnappable_window.get(), window_util::GetActiveWindow());

  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Split view mode should be active. Overview mode should be ended.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(overview_controller()->InOverviewSession());

  ToggleOverview();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_TRUE(overview_controller()->InOverviewSession());

  // Now select the unsnappable window.
  overview_item = GetOverviewItemForWindow(unsnappable_window.get());
  generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  generator->ClickLeftButton();

  // Split view mode should be ended. And the unsnappable window should be the
  // active window now.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_EQ(unsnappable_window.get(), window_util::GetActiveWindow());
}

// Verify that when in overview mode, the selector items unsnappable indicator
// shows up when expected.
TEST_P(SplitViewOverviewSessionTest, OverviewUnsnappableIndicatorVisibility) {
  // Create three windows; two normal and one unsnappable, so that when after
  // snapping |window1| to enter split view we can test the state of each normal
  // and unsnappable windows.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* snappable_overview_item =
      GetOverviewItemForWindow(window2.get());
  OverviewItem* unsnappable_overview_item =
      GetOverviewItemForWindow(unsnappable_window.get());

  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  EXPECT_FALSE(snappable_overview_item->cannot_snap_widget_for_testing());
  ASSERT_FALSE(unsnappable_overview_item->cannot_snap_widget_for_testing());

  // Snap the extra snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(snappable_overview_item->cannot_snap_widget_for_testing());
  ASSERT_TRUE(unsnappable_overview_item->cannot_snap_widget_for_testing());
  ui::Layer* unsnappable_layer =
      unsnappable_overview_item->cannot_snap_widget_for_testing()
          ->GetNativeWindow()
          ->layer();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Exiting the splitview will hide the unsnappable label.
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(/*is_dragging=*/false);
  GetEventGenerator()->set_current_screen_location(
      divider_bounds.CenterPoint());
  GetEventGenerator()->DragMouseTo(0, 0);
  SkipDividerSnapAnimation();

  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
}

// Verify that during "normal" dragging from overview (not drag-to-close), the
// dragged item's unsnappable indicator is temporarily suppressed.
TEST_P(SplitViewOverviewSessionTest,
       OverviewUnsnappableIndicatorVisibilityWhileDragging) {
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_long_press_time_in_ms(1);
  gesture_config->set_show_press_delay_in_ms(1);

  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();
  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  OverviewItem* unsnappable_overview_item =
      GetOverviewItemForWindow(unsnappable_window.get());
  ASSERT_TRUE(unsnappable_overview_item->cannot_snap_widget_for_testing());
  ui::Layer* unsnappable_layer =
      unsnappable_overview_item->cannot_snap_widget_for_testing()
          ->GetNativeWindow()
          ->layer();
  ASSERT_EQ(1.f, unsnappable_layer->opacity());

  // Test that the unsnappable label is temporarily suppressed during mouse
  // dragging.
  ui::test::EventGenerator* generator = GetEventGenerator();
  const gfx::Point drag_starting_point = gfx::ToRoundedPoint(
      unsnappable_overview_item->target_bounds().CenterPoint());
  generator->set_current_screen_location(drag_starting_point);
  generator->PressLeftButton();
  using DragBehavior = OverviewWindowDragController::DragBehavior;
  EXPECT_EQ(
      DragBehavior::kUndefined,
      overview_session()->window_drag_controller()->current_drag_behavior());
  EXPECT_EQ(1.f, unsnappable_layer->opacity());
  generator->MoveMouseBy(0, 20);
  EXPECT_EQ(
      DragBehavior::kNormalDrag,
      overview_session()->window_drag_controller()->current_drag_behavior());
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
  generator->ReleaseLeftButton();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Test that the unsnappable label is temporarily suppressed during "normal"
  // touch dragging (not drag-to-close).
  generator->set_current_screen_location(drag_starting_point);
  generator->PressTouch();
  {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(2));
    run_loop.Run();
  }
  EXPECT_EQ(
      DragBehavior::kNormalDrag,
      overview_session()->window_drag_controller()->current_drag_behavior());
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
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(),
        base::TimeDelta::FromMilliseconds(2));
    run_loop.Run();
  }
  EXPECT_EQ(
      DragBehavior::kNormalDrag,
      overview_session()->window_drag_controller()->current_drag_behavior());
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
  generator->ReleaseTouch();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Test that the unsnappable label persists in drag-to-close mode.
  generator->set_current_screen_location(drag_starting_point);
  generator->PressTouch();
  // Use small increments otherwise a fling event will be fired.
  for (int j = 0; j < 20; ++j)
    generator->MoveTouchBy(0, 1);
  EXPECT_EQ(
      DragBehavior::kDragToClose,
      overview_session()->window_drag_controller()->current_drag_behavior());
  // Drag-to-close mode affects the opacity of the whole overview item,
  // including the unsnappable label.
  EXPECT_EQ(unsnappable_overview_item->GetWindow()->layer()->opacity(),
            unsnappable_layer->opacity());
  generator->ReleaseTouch();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());
}

// Verify that an item's unsnappable indicator is updated for display rotation.
TEST_P(SplitViewOverviewSessionTest,
       OverviewUnsnappableIndicatorVisibilityAfterDisplayRotation) {
  UpdateDisplay("800x800");
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  // Because of its minimum size, |overview_window| is snappable in horizontal
  // split view but not in vertical split view.
  std::unique_ptr<aura::Window> overview_window(
      CreateWindowWithMinimumSize(gfx::Rect(400, 600), gfx::Size(300, 500)));
  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  OverviewItem* overview_item = GetOverviewItemForWindow(overview_window.get());
  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  EXPECT_FALSE(overview_item->cannot_snap_widget_for_testing());

  // Rotate to primary portrait orientation. The unsnappable indicator appears.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ScreenOrientationControllerTestApi test_api(
      Shell::Get()->screen_orientation_controller());
  test_api.SetDisplayRotation(display::Display::ROTATE_270,
                              display::Display::RotationSource::ACTIVE);
  ASSERT_TRUE(overview_item->cannot_snap_widget_for_testing());
  ui::Layer* unsnappable_layer =
      overview_item->cannot_snap_widget_for_testing()->GetLayer();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Rotate to primary landscape orientation. The unsnappable indicator hides.
  test_api.SetDisplayRotation(display::Display::ROTATE_0,
                              display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
}

// Test that when splitview mode and overview mode are both active at the same
// time, dragging divider behaviors are correct.
TEST_P(SplitViewOverviewSessionTest, DragDividerToExitTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Drag the divider toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  split_view_controller()->EndResize(gfx::Point(0, 0));
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Now drag |window2| selector item to snap to left.
  OverviewItem* overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF());
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Drag the divider toward closing the overview window grid.
  divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  const gfx::Rect display_bounds = GetWorkAreaInScreen(window2.get());
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  split_view_controller()->EndResize(display_bounds.bottom_right());
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is also ended. |window2|
  // should be activated.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(Shell::Get()->overview_controller()->InOverviewSession());
  EXPECT_EQ(window2.get(), window_util::GetActiveWindow());
}

TEST_P(SplitViewOverviewSessionTest, OverviewItemLongPressed) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  OverviewItem* overview_item = GetOverviewItemForWindow(window1.get());
  gfx::PointF start_location(overview_item->target_bounds().CenterPoint());
  const gfx::RectF original_bounds(overview_item->target_bounds());

  // Verify that when a overview item receives a resetting gesture, we
  // stay in overview mode and the bounds of the item are the same as they were
  // before the press sequence started.
  overview_session()->InitiateDrag(overview_item, start_location,
                                   /*is_touch_dragging=*/true);
  overview_session()->ResetDraggedWindowGesture();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(original_bounds, overview_item->target_bounds());

  // Verify that when a overview item is tapped, we exit overview mode,
  // and the current active window is the item.
  overview_session()->InitiateDrag(overview_item, start_location,
                                   /*is_touch_dragging=*/true);
  overview_session()->ActivateDraggedWindow();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_EQ(window1.get(), window_util::GetActiveWindow());
}

TEST_P(SplitViewOverviewSessionTest, SnappedWindowBoundsTest) {
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
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Then drag the divider to left toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  split_view_controller()->EndResize(gfx::Point(20, 20));
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Test that |window1| has the dimensions of a tablet mode maxed window, so
  // that when it is placed back on the grid it will not look skinny.
  EXPECT_LE(window1->bounds().x(), 0);
  EXPECT_EQ(window1->bounds().width(), screen_width);

  // Drag |window2| selector item to snap to right.
  OverviewItem* overview_item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Rect work_area_rect = GetWorkAreaInScreen(window2.get());
  gfx::Point end_location2 =
      gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(overview_item2, gfx::PointF(end_location2));
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());

  // Then drag the divider to right toward closing the snapped window.
  divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  end_location2.Offset(-20, -20);
  split_view_controller()->EndResize(end_location2);
  SkipDividerSnapAnimation();

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(Shell::Get()->overview_controller()->InOverviewSession());
  // Test that |window2| has the dimensions of a tablet mode maxed window, so
  // that when it is placed back on the grid it will not look skinny.
  EXPECT_GE(window2->bounds().x(), 0);
  EXPECT_EQ(window2->bounds().width(), screen_width);
}

// Test snapped window bounds with adjustment for the minimum size of a window.
TEST_P(SplitViewOverviewSessionTest, SnappedWindowBoundsWithMinimumSizeTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateTestWindow(bounds));
  const int work_area_length =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          Shell::GetPrimaryRootWindow())
          .width();
  std::unique_ptr<aura::Window> window2(CreateWindowWithMinimumSize(
      bounds, gfx::Size(work_area_length / 3 + 20, 0)));

  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  split_view_controller()->StartResize(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint());
  split_view_controller()->EndResize(gfx::Point(work_area_length / 3, 10));
  SkipDividerSnapAnimation();
  // Use |EXPECT_NEAR| for reasons related to rounding and divider thickness.
  EXPECT_NEAR(
      work_area_length / 3,
      split_view_controller()
          ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                           /*window_for_minimum_size=*/nullptr)
          .width(),
      8);
  EXPECT_NEAR(work_area_length / 2,
              split_view_controller()
                  ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                                   window2.get())
                  .width(),
              8);
  EXPECT_NEAR(
      work_area_length * 2 / 3,
      split_view_controller()
          ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                           /*window_for_minimum_size=*/nullptr)
          .width(),
      8);
  EXPECT_NEAR(work_area_length * 2 / 3,
              split_view_controller()
                  ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                                   window2.get())
                  .width(),
              8);
  split_view_controller()->StartResize(
      GetSplitViewDividerBounds(/*is_dragging=*/false).CenterPoint());
  split_view_controller()->EndResize(gfx::Point(work_area_length * 2 / 3, 10));
  EXPECT_NEAR(
      work_area_length * 2 / 3,
      split_view_controller()
          ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                           /*window_for_minimum_size=*/nullptr)
          .width(),
      8);
  EXPECT_NEAR(work_area_length * 2 / 3,
              split_view_controller()
                  ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                                   window2.get())
                  .width(),
              8);
  EXPECT_NEAR(
      work_area_length / 3,
      split_view_controller()
          ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                           /*window_for_minimum_size=*/nullptr)
          .width(),
      8);
  EXPECT_NEAR(work_area_length / 2,
              split_view_controller()
                  ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                                   window2.get())
                  .width(),
              8);
}

// Verify that if the split view divider is dragged all the way to the edge, the
// window being dragged gets returned to the overview list, if overview mode is
// still active.
TEST_P(SplitViewOverviewSessionTest,
       DividerDraggedToEdgeReturnsWindowToOverviewList) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  // Drag |window1| selector item to snap to left. There should be two items on
  // the overview grid afterwards, |window2| and |window3|.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ASSERT_TRUE(split_view_controller()->split_view_divider());
  std::vector<aura::Window*> window_list =
      overview_controller()->GetWindowsListInOverviewGridsForTest();
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
  window_list = overview_controller()->GetWindowsListInOverviewGridsForTest();
  EXPECT_EQ(3u, window_list.size());
  EXPECT_TRUE(base::Contains(window_list, window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
}

// Verify that if overview mode is active and the split view divider is dragged
// all the way to the opposite edge, then the split view window is reinserted
// into the overview grid at the correct position according to MRU order, and
// the stacking order is also correct.
TEST_P(
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
  EXPECT_EQ(window1.get(), split_view_controller()->left_window());
  EXPECT_EQ(window2.get(), split_view_controller()->right_window());
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
  const std::vector<aura::Window*> expected_mru_list = {
      window2.get(), window1.get(), window3.get()};
  const std::vector<aura::Window*> expected_overview_list = {
      window2.get(), window1.get(), window3.get()};
  EXPECT_EQ(
      expected_mru_list,
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_EQ(expected_overview_list,
            overview_controller()->GetWindowsListInOverviewGridsForTest());

  // Verify the stacking order.
  aura::Window* parent = window1->parent();
  ASSERT_EQ(parent, window2->parent());
  ASSERT_EQ(parent, window3->parent());
  EXPECT_GT(IndexOf(GetOverviewItemForWindow(window2.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent),
            IndexOf(GetOverviewItemForWindow(window1.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent));
  EXPECT_GT(IndexOf(GetOverviewItemForWindow(window1.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent),
            IndexOf(GetOverviewItemForWindow(window3.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent));
}

// Verify that if a window is dragged from overview and snapped in place of
// another split view window, then the old split view window is reinserted into
// the overview grid at the correct position according to MRU order, and the
// stacking order is also correct.
TEST_P(
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
  EXPECT_EQ(window1.get(), split_view_controller()->left_window());
  EXPECT_EQ(window2.get(), split_view_controller()->right_window());
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(window3.get()), gfx::PointF());
  EXPECT_EQ(window3.get(), split_view_controller()->left_window());

  // Verify the grid arrangement.
  ASSERT_TRUE(InOverviewSession());
  const std::vector<aura::Window*> expected_mru_list = {
      window3.get(), window2.get(), window1.get(), window4.get()};
  const std::vector<aura::Window*> expected_overview_list = {
      window2.get(), window1.get(), window4.get()};
  EXPECT_EQ(
      expected_mru_list,
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk));
  EXPECT_EQ(expected_overview_list,
            overview_controller()->GetWindowsListInOverviewGridsForTest());

  // Verify the stacking order.
  aura::Window* parent = window1->parent();
  ASSERT_EQ(parent, window2->parent());
  ASSERT_EQ(parent, window4->parent());
  EXPECT_GT(IndexOf(GetOverviewItemForWindow(window2.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent),
            IndexOf(GetOverviewItemForWindow(window1.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent));
  EXPECT_GT(IndexOf(GetOverviewItemForWindow(window1.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent),
            IndexOf(GetOverviewItemForWindow(window4.get())
                        ->item_widget()
                        ->GetNativeWindow(),
                    parent));
}

// Verify that if the split view divider is dragged close to the edge, the grid
// bounds will be fixed to a third of the work area width and start sliding off
// the screen instead of continuing to shrink.
TEST_P(SplitViewOverviewSessionTest,
       OverviewHasMinimumBoundsWhenDividerDragged) {
  UpdateDisplay("600x400");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Snap a window to the left and test dragging the divider towards the right
  // edge of the screen.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  OverviewGrid* grid = overview_session()->grid_list()[0].get();
  ASSERT_TRUE(grid);

  // Drag the divider to the right edge.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(/*is_dragging=*/false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->PressLeftButton();

  // Tests that near the right edge, the grid bounds are fixed at 200 and are
  // partially off screen to the right.
  generator->MoveMouseTo(580, 0);
  EXPECT_EQ(200, grid->bounds().width());
  EXPECT_GT(grid->bounds().right(), 600);
  generator->ReleaseLeftButton();
  SkipDividerSnapAnimation();

  // Releasing close to the edge will activate the left window and exit
  // overview.
  ASSERT_FALSE(InOverviewSession());
  ToggleOverview();
  // Snap a window to the right and test dragging the divider towards the left
  // edge of the screen.
  split_view_controller()->SnapWindow(window1.get(),
                                      SplitViewController::RIGHT);
  grid = overview_session()->grid_list()[0].get();
  ASSERT_TRUE(grid);

  // Drag the divider to the left edge.
  divider_bounds = GetSplitViewDividerBounds(/*is_dragging=*/false);
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  generator->PressLeftButton();

  generator->MoveMouseTo(20, 0);
  // Tests that near the left edge, the grid bounds are fixed at 200 and are
  // partially off screen to the left.
  EXPECT_EQ(200, grid->bounds().width());
  EXPECT_LT(grid->bounds().x(), 0);
  generator->ReleaseLeftButton();
  SkipDividerSnapAnimation();
}

// Test that when splitview mode is active, minimizing one of the snapped window
// will insert the minimized window back to overview mode if overview mode is
// active at the moment.
TEST_P(SplitViewOverviewSessionTest, InsertMinimizedWindowBackToOverview) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
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
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  // Minimize |window1| will open overview and put |window1| to overview grid.
  WindowState::Get(window1.get())->Minimize();
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
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
TEST_P(SplitViewOverviewSessionTest, SnappedWindowAnimationObserverTest) {
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
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  // Drag |window2| to snap to right.
  OverviewItem* overview_item2 = GetOverviewItemForWindow(window2.get());
  const gfx::Rect work_area_rect =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window2.get());
  const gfx::PointF end_location2(work_area_rect.width(), 0);
  DragWindowTo(overview_item2, end_location2);
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
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
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  // ToggleOverview() directly.
  ToggleOverview();
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());

  // 3. Overview is ended by actviating an existing window.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  wm::ActivateWindow(window2.get());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());

  // 4. Overview is ended by activating a new window.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  wm::ActivateWindow(window4.get());
  EXPECT_EQ(SplitViewController::State::kBothSnapped,
            split_view_controller()->state());
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window4->layer()->GetTargetTransform().IsIdentity());
}

// Test that when split view and overview are both active at the same time,
// double tapping on the divider can swap the window's position with the
// overview window grid's postion.
TEST_P(SplitViewOverviewSessionTest, SwapWindowAndOverviewGrid) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kLeftSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_EQ(
      GetGridBounds(),
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));

  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::State::kRightSnapped);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  EXPECT_EQ(
      GetGridBounds(),
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr));
}

// Verify the behavior when trying to exit overview with one snapped window
// is as expected.
TEST_P(SplitViewOverviewSessionTest, ExitOverviewWithOneSnapped) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(400, 400)));

  // Tests that we cannot exit overview when there is one snapped window and no
  // windows in overview normally.
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  ToggleOverview();
  ASSERT_TRUE(InOverviewSession());

  // Tests that we can exit overview if we swipe up from the shelf.
  ToggleOverview(OverviewEnterExitType::kSwipeFromShelf);
  EXPECT_FALSE(InOverviewSession());
}

// Test that in tablet mode, pressing tab key in overview should not crash.
TEST_P(SplitViewOverviewSessionTest, NoCrashWhenPressTabKey) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(400, 400)));
  std::unique_ptr<aura::Window> window2(CreateWindow(gfx::Rect(400, 400)));

  // In overview, there should be no crash when pressing tab key.
  ToggleOverview();
  EXPECT_TRUE(InOverviewSession());
  SendKey(ui::VKEY_TAB);
  EXPECT_TRUE(InOverviewSession());

  // When splitview and overview are both active, there should be no crash when
  // pressing tab key.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  SendKey(ui::VKEY_TAB);
  EXPECT_TRUE(InOverviewSession());
}

// Tests closing a snapped window while in overview mode.
TEST_P(SplitViewOverviewSessionTest, ClosingSplitViewWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Drag |window1| selector item to snap to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Now close the snapped |window1|. We should remain in overview mode and the
  // overview focus window should regain focus.
  window1.reset();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_EQ(overview_session()->GetOverviewFocusWindow(),
            window_util::GetFocusedWindow());
}

// Test that you cannot drag from overview during the split view divider
// animation.
TEST_P(SplitViewOverviewSessionTest,
       CannotDragFromOverviewDuringSplitViewDividerAnimation) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);

  gfx::Point divider_drag_point =
      split_view_controller()
          ->split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint();
  split_view_controller()->StartResize(divider_drag_point);
  divider_drag_point.Offset(20, 0);
  split_view_controller()->Resize(divider_drag_point);
  split_view_controller()->EndResize(divider_drag_point);
  ASSERT_TRUE(IsDividerAnimating());

  OverviewItem* overview_item = GetOverviewItemForWindow(overview_window.get());
  overview_session()->InitiateDrag(overview_item,
                                   overview_item->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/true);
  EXPECT_FALSE(overview_item->IsDragItem());
}

// Tests that a window which is dragged to a splitview zone is destroyed, the
// grid bounds return to a non-splitview bounds.
TEST_P(SplitViewOverviewSessionTest, GridBoundsAfterWindowDestroyed) {
  // Create two windows otherwise we exit overview after one window is
  // destroyed.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  const gfx::Rect grid_bounds = GetGridBounds();
  // Drag the item such that the splitview preview area shows up and the grid
  // bounds shrink.
  OverviewItem* overview_item = GetOverviewItemForWindow(window1.get());
  overview_session()->InitiateDrag(overview_item,
                                   overview_item->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/true);
  overview_session()->Drag(overview_item, gfx::PointF(1.f, 1.f));
  EXPECT_NE(grid_bounds, GetGridBounds());

  // Tests that when the dragged window is destroyed, the grid bounds return to
  // their normal size.
  window1.reset();
  EXPECT_EQ(grid_bounds, GetGridBounds());
}

// Tests that overview stays active if we have a snapped window.
TEST_P(SplitViewOverviewSessionTest, OnScreenLock) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Overview should exit if no snapped window after locking/unlocking.
  ToggleOverview();
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  ASSERT_FALSE(InOverviewSession());

  ToggleOverview();
  split_view_controller()->SnapWindow(window2.get(), SplitViewController::LEFT);

  // Lock and unlock the machine. Test that we are still in overview and
  // splitview.
  GetSessionControllerClient()->LockScreen();
  GetSessionControllerClient()->UnlockScreen();
  EXPECT_TRUE(InOverviewSession());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
}

// Verify that selecting an minimized snappable window while in split view
// triggers auto snapping.
TEST_P(SplitViewOverviewSessionTest,
       SelectMinimizedSnappableWindowInSplitView) {
  // Create two snappable windows.
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> minimized_window = CreateTestWindow();
  WindowState::Get(minimized_window.get())->Minimize();

  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());

  // Snap a window to enter split view mode.
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());

  // Select the minimized window.
  OverviewItem* overview_item =
      GetOverviewItemForWindow(minimized_window.get());
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
      SplitViewController::LEFT);
  EXPECT_TRUE(
      split_view_controller()->IsWindowInSplitView(minimized_window.get()));
  EXPECT_EQ(split_view_controller()->GetPositionOfSnappedWindow(
                minimized_window.get()),
            SplitViewController::RIGHT);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_EQ(minimized_window.get(), window_util::GetActiveWindow());
}

// Verify no crash (or DCHECK failure) if you exit and re-enter mirror mode
// while in tablet split view with empty overview.
TEST_P(SplitViewOverviewSessionTest,
       ExitAndReenterMirrorModeWithEmptyOverview) {
  UpdateDisplay("800x600,800x600");
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  display_manager()->SetMirrorMode(display::MirrorMode::kNormal, base::nullopt);
}

// Test the split view and overview functionalities in clamshell mode. Split
// view is only active when overview is active in clamshell mode.
class SplitViewOverviewSessionInClamshellTest
    : public SplitViewOverviewSessionTest {
 public:
  SplitViewOverviewSessionInClamshellTest() = default;
  ~SplitViewOverviewSessionInClamshellTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDragToSnapInClamshellMode);
    OverviewSessionTest::SetUp();
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
    ~TestWindowHitTestDelegate() override = default;

   private:
    // aura::Test::TestWindowDelegate:
    void OnWindowDestroyed(aura::Window* window) override { delete this; }

    DISALLOW_COPY_AND_ASSIGN(TestWindowHitTestDelegate);
  };

  base::test::ScopedFeatureList scoped_feature_list_;
  DISALLOW_COPY_AND_ASSIGN(SplitViewOverviewSessionInClamshellTest);
};

// Test some basic functionalities in clamshell splitview mode.
TEST_P(SplitViewOverviewSessionInClamshellTest, BasicFunctionalitiesTest) {
  UpdateDisplay("600x400");
  EXPECT_FALSE(Shell::Get()->tablet_mode_controller()->InTabletMode());

  // 1. Test the 1 window scenario.
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  WindowState* window_state1 = WindowState::Get(window1.get());
  EXPECT_FALSE(window_state1->IsSnapped());
  ToggleOverview();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  // Drag |window1| selector item to snap to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  // Since the only window is snapped, overview and splitview should be both
  // ended.
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kLeftSnapped);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 2. Test if one window is snapped, the other windows are showing in
  // overview, close all windows in overview will end overview and also
  // splitview.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(600, 300));
  // SplitView and overview are both active at the moment.
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(split_view_controller()->IsWindowInSplitView(window1.get()));
  EXPECT_TRUE(overview_controller()->overview_session()->IsWindowInOverview(
      window2.get()));
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kRightSnapped);
  // Close |window2| will end overview and splitview.
  window2.reset();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 3. Test that snap 2 windows will end overview and splitview.
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  OverviewItem* overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(600, 300));
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kLeftSnapped);
  EXPECT_EQ(WindowState::Get(window3.get())->GetStateType(),
            WindowStateType::kRightSnapped);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 4. Test if one window is snapped, the other windows are showing in
  // overview, we can drag another window in overview to snap in splitview, and
  // the previous snapped window will be put back into overview.
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_FALSE(overview_controller()->overview_session()->IsWindowInOverview(
      window1.get()));
  overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(0, 0));
  EXPECT_FALSE(overview_controller()->overview_session()->IsWindowInOverview(
      window3.get()));
  EXPECT_TRUE(overview_controller()->overview_session()->IsWindowInOverview(
      window1.get()));
  EXPECT_EQ(window_state1->GetStateType(), WindowStateType::kLeftSnapped);
  EXPECT_EQ(WindowState::Get(window3.get())->GetStateType(),
            WindowStateType::kLeftSnapped);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  // End overview, test that we'll not auto-snap a window to the right side of
  // the screen.
  EXPECT_EQ(WindowState::Get(window4.get())->GetStateType(),
            WindowStateType::kDefault);
  ToggleOverview();
  EXPECT_EQ(WindowState::Get(window4.get())->GetStateType(),
            WindowStateType::kDefault);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 5. Test if one window is snapped, the other window are showing in overview,
  // activating an new window will not auto-snap the new window. Overview and
  // splitview should be ended.
  ToggleOverview();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  EXPECT_EQ(WindowState::Get(window5.get())->GetStateType(),
            WindowStateType::kDefault);
  wm::ActivateWindow(window5.get());
  EXPECT_EQ(WindowState::Get(window5.get())->GetStateType(),
            WindowStateType::kDefault);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 6. Test if one window is snapped, the other window is showing in overview,
  // close the snapped window will end split view, but overview is still active.
  ToggleOverview();
  const gfx::Rect overview_bounds = GetGridBounds();
  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_NE(GetGridBounds(), overview_bounds);
  EXPECT_EQ(GetGridBounds(), GetSplitViewRightWindowBounds());
  window1.reset();
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  // Overview bounds will adjust from snapped bounds to fullscreen bounds.
  EXPECT_EQ(GetGridBounds(), overview_bounds);

  // 7. Test if split view mode is active, open the app list will end both
  // overview and splitview.
  overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(0, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  // Open app list.
  AppListControllerImpl* app_list_controller =
      Shell::Get()->app_list_controller();
  app_list_controller->ToggleAppList(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window3.get()).id(),
      AppListShowSource::kSearchKey, base::TimeTicks());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // 8. Test if splitview is not active, open the app list will end overview if
  // overview is active.
  ToggleOverview();
  // Open app list.
  app_list_controller->ToggleAppList(
      display::Screen::GetScreen()->GetDisplayNearestWindow(window3.get()).id(),
      AppListShowSource::kSearchKey, base::TimeTicks());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test overview exit animation histograms when you drag to snap two windows on
// opposite sides.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       BothSnappedOverviewExitAnimationHistogramTest) {
  ui::ScopedAnimationDurationScaleMode anmatin_scale(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> left_window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> right_window(CreateWindow(bounds));
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
TEST_P(SplitViewOverviewSessionInClamshellTest, ResizeWindowTest) {
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
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_EQ(GetGridBounds(), GetSplitViewRightWindowBounds());
  gfx::Rect overview_snapped_bounds = GetGridBounds();

  // Resize that happens on the right edge of the left snapped window will
  // resize the window and overview at the same time.
  ui::test::EventGenerator generator1(Shell::GetPrimaryRootWindow(),
                                      window1.get());
  generator1.PressLeftButton();
  CheckWindowResizingPerformanceHistograms("BeforeResizingLeftSnappedWindow1",
                                           0, 0, 0, 0);
  generator1.MoveMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("WhileResizingLeftSnappedWindow1", 0,
                                           0, 1, 0);
  generator1.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow1", 0,
                                           0, 1, 1);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_NE(GetGridBounds(), overview_snapped_bounds);
  EXPECT_EQ(GetGridBounds(), GetSplitViewRightWindowBounds());

  // Resize that happens on the left edge of the left snapped window will end
  // overview. The same for the resize that happens on the top or bottom edge of
  // the left snapped window.
  OverviewItem* overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF(0, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ui::test::EventGenerator generator2(Shell::GetPrimaryRootWindow(),
                                      window2.get());
  generator2.DragMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow2", 0,
                                           0, 1, 1);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  OverviewItem* overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(0, 0));
  ui::test::EventGenerator generator3(Shell::GetPrimaryRootWindow(),
                                      window3.get());
  generator3.DragMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow3", 0,
                                           0, 1, 1);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  OverviewItem* overview_item4 = GetOverviewItemForWindow(window4.get());
  DragWindowTo(overview_item4, gfx::PointF(0, 0));
  ui::test::EventGenerator generator4(Shell::GetPrimaryRootWindow(),
                                      window4.get());
  generator4.DragMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingLeftSnappedWindow4", 0,
                                           0, 1, 1);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  // Now try snapping on the right.
  ToggleOverview();
  overview_full_bounds = GetGridBounds();
  overview_item2 = GetOverviewItemForWindow(window2.get());
  DragWindowTo(overview_item2, gfx::PointF(599, 0));
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_EQ(GetGridBounds(), GetSplitViewLeftWindowBounds());
  overview_snapped_bounds = GetGridBounds();

  ui::test::EventGenerator generator5(Shell::GetPrimaryRootWindow(),
                                      window2.get());
  generator5.PressLeftButton();
  CheckWindowResizingPerformanceHistograms("BeforeResizingRightSnappedWindow2",
                                           0, 0, 1, 1);
  generator5.MoveMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("WhileResizingRightSnappedWindow2",
                                           0, 0, 2, 1);
  generator5.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow2",
                                           0, 0, 2, 2);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_NE(GetGridBounds(), overview_full_bounds);
  EXPECT_NE(GetGridBounds(), overview_snapped_bounds);
  EXPECT_EQ(GetGridBounds(), GetSplitViewLeftWindowBounds());

  overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(599, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ui::test::EventGenerator generator6(Shell::GetPrimaryRootWindow(),
                                      window1.get());
  generator6.DragMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow1",
                                           0, 0, 2, 2);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  overview_item3 = GetOverviewItemForWindow(window3.get());
  DragWindowTo(overview_item3, gfx::PointF(599, 0));
  ui::test::EventGenerator generator7(Shell::GetPrimaryRootWindow(),
                                      window3.get());
  generator7.DragMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow3",
                                           0, 0, 2, 2);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());

  ToggleOverview();
  overview_item4 = GetOverviewItemForWindow(window4.get());
  DragWindowTo(overview_item4, gfx::PointF(599, 0));
  ui::test::EventGenerator generator8(Shell::GetPrimaryRootWindow(),
                                      window4.get());
  generator8.DragMouseBy(50, 50);
  CheckWindowResizingPerformanceHistograms("AfterResizingRightSnappedWindow4",
                                           0, 0, 2, 2);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test closing the split view window while resizing it.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       CloseWindowWhileResizingItTest) {
  UpdateDisplay("600x400");
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> split_view_window(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> overview_window(CreateWindow(bounds));
  ToggleOverview();
  DragWindowTo(GetOverviewItemForWindow(split_view_window.get()),
               gfx::PointF(0.f, 0.f));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
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
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  generator.ReleaseLeftButton();
  CheckWindowResizingPerformanceHistograms("AfterReleasingMouseButton", 0, 0, 1,
                                           1);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

class TestWindowStateDelegate : public WindowStateDelegate {
 public:
  TestWindowStateDelegate() = default;
  TestWindowStateDelegate(const TestWindowStateDelegate&) = delete;
  TestWindowStateDelegate& operator=(const TestWindowStateDelegate&) = delete;
  ~TestWindowStateDelegate() override = default;

  // WindowStateDelegate:
  void OnDragStarted(int component) override { drag_in_progress_ = true; }
  void OnDragFinished(bool cancel, const gfx::PointF& location) override {
    drag_in_progress_ = false;
  }

  bool drag_in_progress() { return drag_in_progress_; }

 private:
  bool drag_in_progress_ = false;
};

// Tests that when a split view window carries over to clamshell split view
// while the divider is being dragged, the window resize is properly completed.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       CarryOverToClamshellSplitViewWhileResizing) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  TestWindowStateDelegate* snapped_window_state_delegate =
      new TestWindowStateDelegate();
  snapped_window_state->SetDelegate(
      base::WrapUnique(snapped_window_state_delegate));

  // Enter clamshell split view and then switch to tablet mode.
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  EnterTabletMode();
  ASSERT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  ASSERT_EQ(snapped_window.get(), split_view_controller()->left_window());

  // Start dragging the divider.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_screen_location(
      split_view_controller()
          ->split_view_divider()
          ->GetDividerBoundsInScreen(/*is_dragging=*/false)
          .CenterPoint());
  generator->PressTouch();
  generator->MoveTouchBy(5, 0);
  EXPECT_TRUE(snapped_window_state_delegate->drag_in_progress());
  EXPECT_NE(nullptr, snapped_window_state->drag_details());

  // End tablet mode.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  ASSERT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  ASSERT_EQ(snapped_window.get(), split_view_controller()->left_window());
  EXPECT_FALSE(snapped_window_state_delegate->drag_in_progress());
  EXPECT_EQ(nullptr, snapped_window_state->drag_details());
}

// Test that overview and clamshell split view end if you double click the edge
// of the split view window where it meets the overview grid.
TEST_P(SplitViewOverviewSessionInClamshellTest, HorizontalMaximizeTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> snapped_window(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow(bounds);
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  ui::test::EventGenerator(Shell::GetPrimaryRootWindow(), snapped_window.get())
      .DoubleClickLeftButton();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that when laptop splitview mode is active, moving the snapped window
// will end splitview and overview at the same time.
TEST_P(SplitViewOverviewSessionInClamshellTest, MoveWindowTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithHitTestComponent(HTCAPTION, bounds));
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithHitTestComponent(HTCAPTION, bounds));

  ToggleOverview();
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  ui::test::EventGenerator generator1(Shell::GetPrimaryRootWindow(),
                                      window1.get());
  generator1.DragMouseBy(50, 50);
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test that in clamshell splitview mode, if the snapped window is minimized,
// splitview mode and overview mode are both ended.
TEST_P(SplitViewOverviewSessionInClamshellTest, MinimizedWindowTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Drag |window1| selector item to snap to left.
  OverviewItem* overview_item1 = GetOverviewItemForWindow(window1.get());
  DragWindowTo(overview_item1, gfx::PointF(0, 0));
  EXPECT_TRUE(overview_controller()->InOverviewSession());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());

  // Now minimize the snapped |window1|.
  WindowState::Get(window1.get())->Minimize();
  EXPECT_FALSE(overview_controller()->InOverviewSession());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
}

// Test snapped window bounds with adjustment for the minimum size of a window.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       SnappedWindowBoundsWithMinimumSizeTest) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  const int window2_minimum_size = 350;
  std::unique_ptr<aura::Window> window2(
      CreateWindowWithMinimumSize(bounds, gfx::Size(window2_minimum_size, 0)));

  ToggleOverview();
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window1.get());
  int divider_position = split_view_controller()->divider_position();
  generator.MoveMouseTo(divider_position, 10);
  divider_position = 300;
  generator.DragMouseTo(divider_position, 10);
  EXPECT_EQ(divider_position, split_view_controller()
                                  ->GetSnappedWindowBoundsInScreen(
                                      SplitViewController::LEFT,
                                      /*window_for_minimum_size=*/nullptr)
                                  .width());
  EXPECT_EQ(window2_minimum_size,
            split_view_controller()
                ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                                 window2.get())
                .width());
  const int work_area_length =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window1.get())
          .width();
  EXPECT_EQ(
      work_area_length - divider_position,
      split_view_controller()
          ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                           /*window_for_minimum_size=*/nullptr)
          .width());
  EXPECT_EQ(work_area_length - divider_position,
            split_view_controller()
                ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                                 window2.get())
                .width());
  divider_position = 500;
  generator.DragMouseTo(divider_position, 10);
  EXPECT_EQ(divider_position, split_view_controller()
                                  ->GetSnappedWindowBoundsInScreen(
                                      SplitViewController::LEFT,
                                      /*window_for_minimum_size=*/nullptr)
                                  .width());
  EXPECT_EQ(divider_position, split_view_controller()
                                  ->GetSnappedWindowBoundsInScreen(
                                      SplitViewController::LEFT, window2.get())
                                  .width());
  EXPECT_EQ(
      work_area_length - divider_position,
      split_view_controller()
          ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                           /*window_for_minimum_size=*/nullptr)
          .width());
  EXPECT_EQ(window2_minimum_size,
            split_view_controller()
                ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                                 window2.get())
                .width());
}

// Tests that on a display in portrait orientation, clamshell split view still
// uses snap positions on the left and right.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       PortraitClamshellSplitViewSnapPositionsTest) {
  UpdateDisplay("800x600/l");
  const int height = 800 - ShelfConfig::Get()->shelf_size();
  ASSERT_EQ(gfx::Rect(0, 0, 600, height),
            screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
                Shell::GetPrimaryRootWindow()));
  // Check that snapped window bounds represent snapping on the left and right.
  const gfx::Rect left_snapped_bounds(0, 0, 300, height);
  EXPECT_EQ(
      left_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr));
  const gfx::Rect right_snapped_bounds(300, 0, 300, height);
  EXPECT_EQ(
      right_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));
  // Switch from clamshell mode to tablet mode and then back to clamshell mode.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();
  TabletModeControllerTestApi tablet_mode_controller_test_api;
  tablet_mode_controller_test_api.DetachAllMice();
  EXPECT_FALSE(tablet_mode_controller_test_api.IsTabletModeStarted());
  tablet_mode_controller_test_api.OpenLidToAngle(315.0f);
  EXPECT_TRUE(tablet_mode_controller_test_api.IsTabletModeStarted());
  tablet_mode_controller_test_api.OpenLidToAngle(90.0f);
  EXPECT_FALSE(tablet_mode_controller_test_api.IsTabletModeStarted());
  // Check the snapped window bounds again. They should be the same as before.
  EXPECT_EQ(
      left_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr));
  EXPECT_EQ(
      right_snapped_bounds,
      split_view_controller()->GetSnappedWindowBoundsInScreen(
          SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));
}

// Tests that the ratio between the divider position and the work area width is
// the same before and after changing the display orientation in clamshell mode.
TEST_P(SplitViewOverviewSessionInClamshellTest, DisplayOrientationChangeTest) {
  UpdateDisplay("600x400");
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> split_view_window(
      CreateWindowWithHitTestComponent(HTRIGHT, bounds));
  std::unique_ptr<aura::Window> overview_window(CreateWindow(bounds));
  ToggleOverview();
  split_view_controller()->SnapWindow(split_view_window.get(),
                                      SplitViewController::LEFT);
  const auto test_many_orientation_changes =
      [this](const std::string& description) {
        SCOPED_TRACE(description);
        for (display::Display::Rotation rotation :
             {display::Display::ROTATE_270, display::Display::ROTATE_180,
              display::Display::ROTATE_90, display::Display::ROTATE_0,
              display::Display::ROTATE_180, display::Display::ROTATE_0}) {
          const auto compute_divider_position_ratio = [this]() {
            return static_cast<float>(
                       split_view_controller()->divider_position()) /
                   static_cast<float>(display::Screen::GetScreen()
                                          ->GetPrimaryDisplay()
                                          .work_area()
                                          .width());
          };
          const float before = compute_divider_position_ratio();
          Shell::Get()->display_manager()->SetDisplayRotation(
              display::Screen::GetScreen()->GetPrimaryDisplay().id(), rotation,
              display::Display::RotationSource::ACTIVE);
          const float after = compute_divider_position_ratio();
          EXPECT_NEAR(before, after, 0.001f);
        }
      };
  EXPECT_EQ(split_view_controller()->GetDefaultDividerPosition(),
            split_view_controller()->divider_position());
  test_many_orientation_changes("centered divider");
  EXPECT_EQ(split_view_controller()->GetDefaultDividerPosition(),
            split_view_controller()->divider_position());
  ui::test::EventGenerator(Shell::GetPrimaryRootWindow(),
                           split_view_window.get())
      .DragMouseBy(50, 50);
  EXPECT_NE(split_view_controller()->GetDefaultDividerPosition(),
            split_view_controller()->divider_position());
  test_many_orientation_changes("off-center divider");
}

// Verify that an item's unsnappable indicator is updated for display rotation.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       OverviewUnsnappableIndicatorVisibilityAfterDisplayRotation) {
  UpdateDisplay("900x600");
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  // Because of its minimum size, |overview_window| is snappable in clamshell
  // split view with landscape display orientation but not with portrait display
  // orientation.
  std::unique_ptr<aura::Window> overview_window(
      CreateWindowWithMinimumSize(gfx::Rect(400, 400), gfx::Size(400, 0)));
  ToggleOverview();
  ASSERT_TRUE(overview_controller()->InOverviewSession());
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->InSplitViewMode());
  OverviewItem* overview_item = GetOverviewItemForWindow(overview_window.get());
  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  EXPECT_FALSE(overview_item->cannot_snap_widget_for_testing());

  // Rotate to primary portrait orientation. The unsnappable indicator appears.
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  const int64_t display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_270,
                                      display::Display::RotationSource::ACTIVE);
  ASSERT_TRUE(overview_item->cannot_snap_widget_for_testing());
  ui::Layer* unsnappable_layer =
      overview_item->cannot_snap_widget_for_testing()->GetLayer();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Rotate to primary landscape orientation. The unsnappable indicator hides.
  display_manager->SetDisplayRotation(display_id, display::Display::ROTATE_0,
                                      display::Display::RotationSource::ACTIVE);
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
}

// Tests that dragging a window from overview creates a drop target on the same
// display, even if the window bounds are mostly on another display.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       DragFromOverviewWithBoundsMostlyOnAnotherDisplay) {
  UpdateDisplay("600x600,600x600");
  const aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const display::DisplayIdList display_ids =
      display_manager()->GetCurrentDisplayIdList();
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
  OverviewItem* overview_item = GetOverviewItemForWindow(window.get());
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  gfx::PointF drag_point = overview_item->target_bounds().CenterPoint();
  overview_session()->InitiateDrag(overview_item, drag_point,
                                   /*is_touch_dragging=*/false);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
  drag_point.Offset(5.f, 0.f);
  overview_session()->Drag(overview_item, drag_point);
  EXPECT_FALSE(GetDropTarget(1));
  ASSERT_TRUE(GetDropTarget(0));
  EXPECT_EQ(root_windows[0], GetDropTarget(0)->root_window());
  overview_session()->CompleteDrag(overview_item, drag_point);
  EXPECT_FALSE(GetDropTarget(0));
  EXPECT_FALSE(GetDropTarget(1));
}

// Tests that Alt+[ and Alt+] do not start overview.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       AltSquareBracketNotStartOverview) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  wm::ActivateWindow(window1.get());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
  // Alt+[
  const WMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_LEFT);
  WindowState* window1_state = WindowState::Get(window1.get());
  window1_state->OnWMEvent(&alt_left_square_bracket);
  EXPECT_EQ(WindowStateType::kLeftSnapped, window1_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
  // Alt+]
  const WMEvent alt_right_square_bracket(WM_EVENT_CYCLE_SNAP_RIGHT);
  window1_state->OnWMEvent(&alt_right_square_bracket);
  EXPECT_EQ(WindowStateType::kRightSnapped, window1_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Tests using Alt+[ on a left split view window.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       AltLeftSquareBracketOnLeftSplitViewWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::LEFT);
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kLeftSnapped,
            snapped_window_state->GetStateType());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  const WMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_LEFT);
  snapped_window_state->OnWMEvent(&alt_left_square_bracket);
  EXPECT_EQ(WindowStateType::kNormal, snapped_window_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Tests using Alt+] on a right split view window.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       AltRightSquareBracketOnRightSplitViewWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::RIGHT);
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kRightSnapped,
            snapped_window_state->GetStateType());
  EXPECT_TRUE(split_view_controller()->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  const WMEvent alt_right_square_bracket(WM_EVENT_CYCLE_SNAP_RIGHT);
  snapped_window_state->OnWMEvent(&alt_right_square_bracket);
  EXPECT_EQ(WindowStateType::kNormal, snapped_window_state->GetStateType());
  EXPECT_FALSE(split_view_controller()->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Tests using Alt+[ on a right split view window, and Alt+] on a left split
// view window.
TEST_P(SplitViewOverviewSessionInClamshellTest,
       AltSquareBracketOnSplitViewWindow) {
  std::unique_ptr<aura::Window> snapped_window = CreateTestWindow();
  std::unique_ptr<aura::Window> overview_window = CreateTestWindow();
  // Enter clamshell split view with |snapped_window| on the right.
  ToggleOverview();
  split_view_controller()->SnapWindow(snapped_window.get(),
                                      SplitViewController::RIGHT);
  wm::ActivateWindow(snapped_window.get());
  WindowState* snapped_window_state = WindowState::Get(snapped_window.get());
  EXPECT_EQ(WindowStateType::kRightSnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->right_window());
  EXPECT_TRUE(InOverviewSession());
  // Test using Alt+[ to put |snapped_window| on the left.
  const WMEvent alt_left_square_bracket(WM_EVENT_CYCLE_SNAP_LEFT);
  snapped_window_state->OnWMEvent(&alt_left_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kLeftSnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->left_window());
  EXPECT_TRUE(InOverviewSession());
  // Test using Alt+] to put |snapped_window| on the right.
  const WMEvent alt_right_square_bracket(WM_EVENT_CYCLE_SNAP_RIGHT);
  snapped_window_state->OnWMEvent(&alt_right_square_bracket);
  EXPECT_TRUE(wm::IsActiveWindow(snapped_window.get()));
  EXPECT_EQ(WindowStateType::kRightSnapped,
            snapped_window_state->GetStateType());
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller()->state());
  EXPECT_EQ(snapped_window.get(), split_view_controller()->right_window());
  EXPECT_TRUE(InOverviewSession());
}

using SplitViewOverviewSessionInClamshellTestMultiDisplayOnly =
    SplitViewOverviewSessionInClamshellTest;

// Test |SplitViewController::Get|.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
              SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr));
  EXPECT_EQ(
      gfx::Rect(400, 0, 400, height),
      SplitViewController::Get(root_windows[0])
          ->GetSnappedWindowBoundsInScreen(
              SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));
  EXPECT_EQ(
      gfx::Rect(800, 0, 400, height),
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(
              SplitViewController::LEFT, /*window_for_minimum_size=*/nullptr));
  EXPECT_EQ(
      gfx::Rect(1200, 0, 400, height),
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(
              SplitViewController::RIGHT, /*window_for_minimum_size=*/nullptr));
}

// Test that if clamshell split view is started by snapping a window that is the
// only overview window, then split view ends as soon as it starts, and overview
// ends along with it.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       SplitViewEndsImmediatelyIfOverviewIsEmpty) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root1(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window = CreateTestWindow(bounds_within_root1);
  ToggleOverview();
  SplitViewController::Get(root_windows[0])
      ->SnapWindow(window.get(), SplitViewController::LEFT);
  EXPECT_FALSE(InOverviewSession());
  EXPECT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
}

// Test that if clamshell split view is started by snapping a window on one
// display while there is an overview window on another display, then split view
// stays active (instead of ending as soon as it starts), and overview also
// stays active. Then close the overview window and verify that split view and
// overview are ended.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
      ->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_TRUE(InOverviewSession());
  window2.reset();
  EXPECT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_FALSE(InOverviewSession());
}

// Test dragging to snap an overview item on an external display.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DraggingOnExternalDisplay) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  const gfx::Rect bounds_within_root2(800, 0, 400, 400);
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root2);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root2);
  ToggleOverview();
  OverviewGrid* grid_on_root2 =
      overview_session()->GetGridWithRootWindow(root_windows[1]);
  OverviewItem* item1 = grid_on_root2->GetOverviewItemContaining(window1.get());
  OverviewItem* item2 = grid_on_root2->GetOverviewItemContaining(window2.get());
  SplitViewController* split_view_controller =
      SplitViewController::Get(root_windows[1]);
  SplitViewDragIndicators* indicators =
      grid_on_root2->split_view_drag_indicators();

  Shell::Get()->cursor_manager()->SetDisplay(
      display::Screen::GetScreen()->GetDisplayNearestWindow(root_windows[1]));
  overview_session()->InitiateDrag(item1, item1->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  const gfx::PointF right_snap_point(1599.f, 300.f);
  overview_session()->Drag(item1, right_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            indicators->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                           /*window_for_minimum_size=*/nullptr),
      grid_on_root2->bounds());
  overview_session()->CompleteDrag(item1, right_snap_point);
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller->state());
  EXPECT_EQ(window1.get(), split_view_controller->right_window());

  overview_session()->InitiateDrag(item2, item2->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  const gfx::PointF left_of_middle(1150.f, 300.f);
  overview_session()->Drag(item2, left_of_middle);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            indicators->current_window_dragging_state());
  overview_session()->CompleteDrag(item2, left_of_middle);
  EXPECT_EQ(SplitViewController::State::kRightSnapped,
            split_view_controller->state());
  EXPECT_EQ(window1.get(), split_view_controller->right_window());

  overview_session()->InitiateDrag(item2, item2->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  const gfx::PointF left_snap_point(810.f, 300.f);
  overview_session()->Drag(item2, left_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            indicators->current_window_dragging_state());
  overview_session()->CompleteDrag(item2, left_snap_point);
  EXPECT_EQ(SplitViewController::State::kNoSnap,
            split_view_controller->state());
}

// Test dragging from one display to another.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       MultiDisplayDragging) {
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
  std::unique_ptr<aura::Window> window1 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window2 = CreateTestWindow(bounds_within_root1);
  std::unique_ptr<aura::Window> window3 = CreateTestWindow(bounds_within_root2);
  ToggleOverview();
  OverviewGrid* grid_on_root1 =
      overview_session()->GetGridWithRootWindow(root_windows[0]);
  OverviewGrid* grid_on_root2 =
      overview_session()->GetGridWithRootWindow(root_windows[1]);
  OverviewItem* item1 = grid_on_root1->GetOverviewItemContaining(window1.get());
  SplitViewDragIndicators* indicators_on_root1 =
      grid_on_root1->split_view_drag_indicators();
  SplitViewDragIndicators* indicators_on_root2 =
      grid_on_root2->split_view_drag_indicators();

  ASSERT_EQ(display_with_root1.id(), cursor_manager->GetDisplay().id());
  overview_session()->InitiateDrag(item1, item1->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(), grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(), grid_on_root2->bounds());

  const gfx::PointF root1_left_snap_point(0.f, 300.f);
  overview_session()->Drag(item1, root1_left_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[0])
          ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                           /*window_for_minimum_size=*/nullptr),
      grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(), grid_on_root2->bounds());

  const gfx::PointF root1_middle_point(400.f, 300.f);
  overview_session()->Drag(item1, root1_middle_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(), grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(), grid_on_root2->bounds());

  const gfx::PointF root1_right_snap_point(799.f, 300.f);
  overview_session()->Drag(item1, root1_right_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[0])
          ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                           /*window_for_minimum_size=*/nullptr),
      grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(), grid_on_root2->bounds());

  const gfx::PointF root2_left_snap_point(800.f, 300.f);
  cursor_manager->SetDisplay(display_with_root2);
  overview_session()->Drag(item1, root2_left_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(), grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                           /*window_for_minimum_size=*/nullptr),
      grid_on_root2->bounds());

  const gfx::PointF root2_left_snap_point_away_from_edge(816.f, 300.f);
  overview_session()->Drag(item1, root2_left_snap_point_away_from_edge);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(), grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapLeft,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SplitViewController::RIGHT,
                                           /*window_for_minimum_size=*/nullptr),
      grid_on_root2->bounds());

  const gfx::PointF root2_right_snap_point(1599.f, 300.f);
  overview_session()->Drag(item1, root2_right_snap_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(), grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kToSnapRight,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(
      SplitViewController::Get(root_windows[1])
          ->GetSnappedWindowBoundsInScreen(SplitViewController::LEFT,
                                           /*window_for_minimum_size=*/nullptr),
      grid_on_root2->bounds());

  const gfx::PointF root2_middle_point(1200.f, 300.f);
  overview_session()->Drag(item1, root2_middle_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kOtherDisplay,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(), grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kFromOverview,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(), grid_on_root2->bounds());

  overview_session()->CompleteDrag(item1, root2_middle_point);
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root1->current_window_dragging_state());
  EXPECT_EQ(display_with_root1.work_area(), grid_on_root1->bounds());
  EXPECT_EQ(SplitViewDragIndicators::WindowDraggingState::kNoDrag,
            indicators_on_root2->current_window_dragging_state());
  EXPECT_EQ(display_with_root2.work_area(), grid_on_root2->bounds());
}

// Verify the drop target positions for multi-display dragging.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
      overview_session()->GetGridWithRootWindow(root_windows[0]);
  OverviewGrid* grid2 =
      overview_session()->GetGridWithRootWindow(root_windows[1]);
  OverviewItem* item4 = grid2->GetOverviewItemContaining(window4.get());
  // Start dragging |item4| from |grid2|.
  cursor_manager->SetDisplay(display_with_root2);
  overview_session()->InitiateDrag(item4, item4->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  overview_session()->Drag(item4, gfx::PointF(1200.f, 0.f));
  // On the grid where the drag starts (|grid2|), the drop target is inserted at
  // the index immediately following the dragged item (|item4|).
  ASSERT_EQ(4u, grid2->window_list().size());
  EXPECT_EQ(grid2->GetDropTarget(), grid2->window_list()[2].get());
  // Drag over |grid1|.
  cursor_manager->SetDisplay(display_with_root1);
  overview_session()->Drag(item4, gfx::PointF(400.f, 0.f));
  // On other grids (such as |grid1|), the drop target is inserted at the
  // correct position according to MRU order (between the overview items for
  // |window3| and |window5|).
  ASSERT_EQ(4u, grid1->window_list().size());
  EXPECT_EQ(grid1->GetDropTarget(), grid1->window_list()[2].get());
}

// Verify that the drop target in each overview grid has the correct bounds when
// a maximized window is being dragged.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DropTargetBoundsForMaximizedWindowDraggedToOtherDisplay) {
  UpdateDisplay("1000x400,1000x400/l");
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  WindowState::Get(window.get())->Maximize();
  ToggleOverview();
  OverviewItem* item = GetOverviewItemForWindow(window.get());
  // Verify that |item| is letter boxed. The bounds of |item|, minus the margin
  // should have an aspect ratio of 2 : 1.
  gfx::RectF item_bounds = item->target_bounds();
  item_bounds.Inset(gfx::InsetsF(kWindowMargin));
  EXPECT_EQ(OverviewGridWindowFillMode::kLetterBoxed,
            item->GetWindowDimensionsType());
  EXPECT_EQ(2.f, item_bounds.width() / item_bounds.height());
  overview_session()->InitiateDrag(item, item->target_bounds().CenterPoint(),
                                   /*is_touch_dragging=*/false);
  Shell::Get()->cursor_manager()->SetDisplay(
      display::test::DisplayManagerTestApi(display_manager())
          .GetSecondaryDisplay());
  overview_session()->Drag(item, gfx::PointF(1200.f, 0.f));
  OverviewItem* drop_target = GetDropTarget(1);
  ASSERT_TRUE(drop_target);
  // Verify that |drop_target| is effectively pillar boxed. Avoid calling
  // |OverviewItem::GetWindowDimensionsType|, because it does not work for drop
  // targets (and that is okay). The bounds of |drop_target|, minus the margin
  // should have an aspect ratio of 1 : 2.
  gfx::RectF drop_target_bounds = drop_target->target_bounds();
  drop_target_bounds.Inset(gfx::InsetsF(kWindowMargin));
  EXPECT_EQ(0.5f, drop_target_bounds.width() / drop_target_bounds.height());
}

// Verify that the drop target in each overview grid has bounds representing
// anticipation that if the dragged window is dropped into that grid, it will
// shrink to fit into the corresponding work area.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DropTargetBoundsOnDisplayWhereDraggedWindowDoesNotFitIntoWorkArea) {
  UpdateDisplay("600x600,1200x1200");
  // Drags |item| from the right display to the left display and back, and
  // returns the bounds of the drop target that appears on the left display.
  const auto root1_drop_target_bounds = [this](OverviewItem* item) {
    wm::CursorManager* cursor_manager = Shell::Get()->cursor_manager();
    const gfx::PointF drag_starting_point = item->target_bounds().CenterPoint();
    display::test::DisplayManagerTestApi display_manager_test(
        display_manager());
    cursor_manager->SetDisplay(display_manager_test.GetSecondaryDisplay());
    overview_session()->InitiateDrag(item, drag_starting_point,
                                     /*is_touch_dragging=*/false);
    cursor_manager->SetDisplay(
        display::Screen::GetScreen()->GetPrimaryDisplay());
    overview_session()->Drag(item, gfx::PointF(300.f, 0.f));
    cursor_manager->SetDisplay(display_manager_test.GetSecondaryDisplay());
    overview_session()->Drag(item, drag_starting_point);
    DCHECK(GetDropTarget(0));
    const gfx::RectF result = GetDropTarget(0)->target_bounds();
    overview_session()->CompleteDrag(item, drag_starting_point);
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
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  OverviewItem* item3 = GetOverviewItemForWindow(window3.get());
  OverviewItem* item4 = GetOverviewItemForWindow(window4.get());

  // For good test coverage in each case, the dragged window and the drop target
  // have different |OverviewGridWindowFillMode| values.
  EXPECT_EQ(OverviewGridWindowFillMode::kNormal,
            item1->GetWindowDimensionsType());
  EXPECT_EQ(OverviewGridWindowFillMode::kLetterBoxed,
            item2->GetWindowDimensionsType());
  EXPECT_EQ(OverviewGridWindowFillMode::kNormal,
            item3->GetWindowDimensionsType());
  EXPECT_EQ(OverviewGridWindowFillMode::kPillarBoxed,
            item4->GetWindowDimensionsType());

  EXPECT_EQ(root1_drop_target_bounds(item1), root1_drop_target_bounds(item2));
  EXPECT_EQ(root1_drop_target_bounds(item3), root1_drop_target_bounds(item4));
}

// Test dragging from one overview grid and dropping into another overview grid.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       DragAndDropIntoAnotherOverviewGrid) {
  UpdateDisplay("800x600,800x600");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  ASSERT_EQ(root_windows[0], window->GetRootWindow());
  ToggleOverview();
  OverviewGrid* grid1 =
      overview_session()->GetGridWithRootWindow(root_windows[0]);
  OverviewGrid* grid2 =
      overview_session()->GetGridWithRootWindow(root_windows[1]);

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
  OverviewItem* item = grid2->GetOverviewItemContaining(window.get());
  ASSERT_TRUE(item);
  EXPECT_EQ(root_windows[1], item->root_window());
}

// Test that overview widgets are stacked in the correct order after an overview
// window is dragged from one overview grid and dropped into another. Also test
// that the destination overview grid is arranged in the correct order.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
  OverviewItem* item1 = GetOverviewItemForWindow(window1.get());
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  OverviewItem* item3 = GetOverviewItemForWindow(window3.get());

  ASSERT_EQ(root_windows[0], item2->root_window());
  // Verify that |item1| is stacked above |item3| (because we created |window1|
  // after |window3|).
  EXPECT_GT(IndexOf(item1->item_widget()->GetNativeWindow(), parent_on_root2),
            IndexOf(item3->item_widget()->GetNativeWindow(), parent_on_root2));
  // Verify that the item widget for each window is stacked below that window.
  EXPECT_LT(IndexOf(item1->item_widget()->GetNativeWindow(), parent_on_root2),
            IndexOf(window1.get(), parent_on_root2));
  EXPECT_LT(IndexOf(item2->item_widget()->GetNativeWindow(), parent_on_root1),
            IndexOf(window2.get(), parent_on_root1));
  EXPECT_LT(IndexOf(item3->item_widget()->GetNativeWindow(), parent_on_root2),
            IndexOf(window3.get(), parent_on_root2));

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
  EXPECT_GT(IndexOf(item1->item_widget()->GetNativeWindow(), parent_on_root2),
            IndexOf(item2->item_widget()->GetNativeWindow(), parent_on_root2));
  EXPECT_GT(IndexOf(item2->item_widget()->GetNativeWindow(), parent_on_root2),
            IndexOf(item3->item_widget()->GetNativeWindow(), parent_on_root2));
  // Verify that the item widget for the new |item2| is stacked below |window2|.
  EXPECT_LT(IndexOf(item2->item_widget()->GetNativeWindow(), parent_on_root2),
            IndexOf(window2.get(), parent_on_root2));

  // Verify that the right grid is in MRU order.
  const std::vector<aura::Window*> expected_order = {
      window1.get(), window2.get(), window3.get()};
  EXPECT_EQ(expected_order,
            overview_controller()->GetWindowsListInOverviewGridsForTest());
}

// Test dragging from one display to another and then snapping.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
  EXPECT_EQ(SplitViewController::State::kLeftSnapped,
            split_view_controller2->state());
  EXPECT_EQ(window2.get(), split_view_controller2->left_window());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_TRUE(InOverviewSession());
}

// Verify that window resizing performance is recorded to the correct histogram
// depending on whether the overview grid is empty.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
      ->SnapWindow(window1.get(), SplitViewController::LEFT);
  SplitViewController::Get(root_windows[1])
      ->SnapWindow(window2.get(), SplitViewController::LEFT);
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
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
  split_view_controller1->SnapWindow(window1.get(), SplitViewController::LEFT);
  verify("2. Number of displays in split view changed from 0 to 1", 0, 0);
  split_view_controller2->SnapWindow(window3.get(), SplitViewController::LEFT);
  verify("3. Number of displays in split view changed from 1 to 2", 1, 0);
  ToggleOverview();
  verify("4. Number of displays in split view changed from 2 to 0", 1, 1);
  ToggleOverview();
  split_view_controller1->SnapWindow(window1.get(), SplitViewController::LEFT);
  verify("5. Number of displays in split view changed from 0 to 1", 1, 1);
  split_view_controller2->SnapWindow(window3.get(), SplitViewController::LEFT);
  verify("6. Number of displays in split view changed from 1 to 2", 2, 1);
  split_view_controller3->SnapWindow(window5.get(), SplitViewController::LEFT);
  verify("7. Number of displays in split view changed from 2 to 3", 2, 1);
  ToggleOverview();
  verify("8. Number of displays in split view changed from 3 to 0", 2, 2);
  ToggleOverview();
  split_view_controller1->SnapWindow(window1.get(), SplitViewController::LEFT);
  verify("9. Number of displays in split view changed from 0 to 1", 2, 2);
  split_view_controller2->SnapWindow(window3.get(), SplitViewController::LEFT);
  verify("10. Number of displays in split view changed from 1 to 2", 3, 2);
  split_view_controller3->SnapWindow(window5.get(), SplitViewController::LEFT);
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
  split_view_controller1->SnapWindow(window2.get(), SplitViewController::LEFT);
  verify("15. Number of displays in split view changed from 0 to 1", 3, 3);
  // In this case, multi-display split view ends as soon as it starts. The
  // metrics should report that as starting and ending multi-display split view.
  split_view_controller2->SnapWindow(window4.get(), SplitViewController::LEFT);
  verify(
      "16. Multi-display split view started by snapping last overview window",
      4, 4);
}

// Verify that |SplitViewController::CanSnapWindow| checks that the minimum size
// of the window fits into the left or top, with the default divider position.
// (If the work area length is odd, then the right or bottom will be one pixel
// larger.)
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
       SnapWindowWithMinimumSizeTest) {
  // The divider is 8 thick. For the default divider position, the remaining 792
  // of the work area on the first root window is divided into 396 on each side,
  // and the remaining 791 of the work area on the second root window is divided
  // into 395 on the left and 396 on the right (the left side is what matters).
  UpdateDisplay("800x600,799x600");
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
    EXPECT_TRUE(
        SplitViewController::Get(root_windows[0])->CanSnapWindow(window.get()));
    EXPECT_TRUE(
        SplitViewController::Get(root_windows[1])->CanSnapWindow(window.get()));
    // Either root window can accommodate a minimum size 395 wide.
    delegate->set_minimum_size(gfx::Size(395, 0));
    EXPECT_TRUE(
        SplitViewController::Get(root_windows[0])->CanSnapWindow(window.get()));
    EXPECT_TRUE(
        SplitViewController::Get(root_windows[1])->CanSnapWindow(window.get()));
    // Only the first root window can accommodate a minimum size 396 wide.
    delegate->set_minimum_size(gfx::Size(396, 0));
    EXPECT_TRUE(
        SplitViewController::Get(root_windows[0])->CanSnapWindow(window.get()));
    EXPECT_FALSE(
        SplitViewController::Get(root_windows[1])->CanSnapWindow(window.get()));
    // Neither root window can accommodate a minimum size 397 wide.
    delegate->set_minimum_size(gfx::Size(397, 0));
    EXPECT_FALSE(
        SplitViewController::Get(root_windows[0])->CanSnapWindow(window.get()));
    EXPECT_FALSE(
        SplitViewController::Get(root_windows[1])->CanSnapWindow(window.get()));
  }
}

// Verify that when in overview mode, the selector items unsnappable indicator
// shows up when expected.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
  OverviewItem* item2 = GetOverviewItemForWindow(window2.get());
  OverviewItem* item3 = GetOverviewItemForWindow(window3.get());
  OverviewItem* item5 = GetOverviewItemForWindow(window5.get());
  OverviewItem* item6 = GetOverviewItemForWindow(window6.get());

  // Note: |cannot_snap_label_view_| and its parent will be created on demand.
  ASSERT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(item2->cannot_snap_widget_for_testing());
  EXPECT_FALSE(item3->cannot_snap_widget_for_testing());
  EXPECT_FALSE(item5->cannot_snap_widget_for_testing());
  EXPECT_FALSE(item6->cannot_snap_widget_for_testing());

  SplitViewController::Get(root_windows[0])
      ->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(item2->cannot_snap_widget_for_testing());
  ASSERT_TRUE(item3->cannot_snap_widget_for_testing());
  ui::Layer* item3_unsnappable_layer =
      item3->cannot_snap_widget_for_testing()->GetNativeWindow()->layer();
  EXPECT_EQ(1.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(item5->cannot_snap_widget_for_testing());
  EXPECT_FALSE(item6->cannot_snap_widget_for_testing());

  SplitViewController::Get(root_windows[1])
      ->SnapWindow(window4.get(), SplitViewController::LEFT);
  ASSERT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_TRUE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(item2->cannot_snap_widget_for_testing());
  EXPECT_EQ(1.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(item5->cannot_snap_widget_for_testing());
  ASSERT_TRUE(item6->cannot_snap_widget_for_testing());
  ui::Layer* item6_unsnappable_layer =
      item6->cannot_snap_widget_for_testing()->GetNativeWindow()->layer();
  EXPECT_EQ(1.f, item6_unsnappable_layer->opacity());

  SplitViewController::Get(root_windows[0])->EndSplitView();
  ASSERT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_TRUE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(item2->cannot_snap_widget_for_testing());
  EXPECT_EQ(0.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(item5->cannot_snap_widget_for_testing());
  EXPECT_EQ(1.f, item6_unsnappable_layer->opacity());

  SplitViewController::Get(root_windows[1])->EndSplitView();
  ASSERT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  ASSERT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  EXPECT_FALSE(item2->cannot_snap_widget_for_testing());
  EXPECT_EQ(0.f, item3_unsnappable_layer->opacity());
  EXPECT_FALSE(item5->cannot_snap_widget_for_testing());
  EXPECT_EQ(0.f, item6_unsnappable_layer->opacity());
}

// Test that enabling the docked magnifier ends clamshell split view on all
// displays.
TEST_P(SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
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
      ->SnapWindow(window1.get(), SplitViewController::LEFT);
  SplitViewController::Get(root_windows[1])
      ->SnapWindow(window3.get(), SplitViewController::LEFT);
  EXPECT_TRUE(InOverviewSession());
  EXPECT_TRUE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_TRUE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
  Shell::Get()->docked_magnifier_controller()->SetEnabled(true);
  EXPECT_FALSE(InOverviewSession());
  EXPECT_FALSE(SplitViewController::Get(root_windows[0])->InSplitViewMode());
  EXPECT_FALSE(SplitViewController::Get(root_windows[1])->InSplitViewMode());
}

INSTANTIATE_TEST_SUITE_P(All, OverviewSessionTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, TabletModeOverviewSessionTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All, SplitViewOverviewSessionTest, testing::Bool());
INSTANTIATE_TEST_SUITE_P(All,
                         SplitViewOverviewSessionInClamshellTest,
                         testing::Bool());
INSTANTIATE_TEST_SUITE_P(
    All,
    SplitViewOverviewSessionInClamshellTestMultiDisplayOnly,
    testing::Values(true));

}  // namespace ash
