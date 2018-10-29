// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/home_launcher_gesture_handler.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/drag_drop/drag_drop_controller.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_view_test_api.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/overview/cleanup_animation_observer.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/overview/overview_window_drag_controller.h"
#include "ash/wm/overview/window_grid.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/overview/window_selector_item.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_drag_indicators.h"
#include "ash/wm/tablet_mode/tablet_mode_app_window_drag_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/test/events_test_utils.h"
#include "ui/gfx/animation/slide_animation.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_util.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The label covers selector item windows with a padding in order to prevent
// them from receiving user input events while in overview.
constexpr int kWindowMargin = 5;

// The overview mode header overlaps original window header. This value is used
// to set top inset property on the windows.
constexpr int kHeaderHeight = 32;

constexpr const char kActiveWindowChangedFromOverview[] =
    "WindowSelector_ActiveWindowChanged";

// A simple window delegate that returns the specified hit-test code when
// requested and applies a minimum size constraint if there is one.
class TestDragWindowDelegate : public aura::test::TestWindowDelegate {
 public:
  TestDragWindowDelegate() { set_window_component(HTCAPTION); }
  ~TestDragWindowDelegate() override = default;

 private:
  // Overridden from aura::Test::TestWindowDelegate:
  void OnWindowDestroyed(aura::Window* window) override { delete this; }

  DISALLOW_COPY_AND_ASSIGN(TestDragWindowDelegate);
};

float GetItemScale(const gfx::Rect& source,
                   const gfx::Rect& target,
                   int top_view_inset,
                   int title_height) {
  return ScopedTransformOverviewWindow::GetItemScale(
      source.size(), target.size(), top_view_inset, title_height);
}

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

// WindowState that lets us specify an initial state.
class InitialStateTestState : public wm::WindowState::State {
 public:
  explicit InitialStateTestState(mojom::WindowStateType initial_state_type)
      : state_type_(initial_state_type) {}
  ~InitialStateTestState() override = default;

  // WindowState::State overrides:
  void OnWMEvent(wm::WindowState* window_state,
                 const wm::WMEvent* event) override {}
  mojom::WindowStateType GetType() const override { return state_type_; }
  void AttachState(wm::WindowState* window_state,
                   wm::WindowState::State* previous_state) override {}
  void DetachState(wm::WindowState* window_state) override {}

 private:
  mojom::WindowStateType state_type_;

  DISALLOW_COPY_AND_ASSIGN(InitialStateTestState);
};

}  // namespace

// TODO(bruthig): Move all non-simple method definitions out of class
// declaration.
class WindowSelectorTest : public AshTestBase {
 public:
  WindowSelectorTest() = default;
  ~WindowSelectorTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    shelf_view_test_api_ = std::make_unique<ShelfViewTestAPI>(
        GetPrimaryShelf()->GetShelfViewForTesting());
    shelf_view_test_api_->SetAnimationDuration(1);
    ScopedTransformOverviewWindow::SetImmediateCloseForTests();
    WindowSelectorController::SetDoNotChangeWallpaperBlurForTests();
  }

  aura::Window* CreateWindow(const gfx::Rect& bounds) {
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(&delegate_, -1, bounds);
    window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);
    return window;
  }

  aura::Window* CreateWindowWithId(const gfx::Rect& bounds, int id) {
    aura::Window* window =
        CreateTestWindowInShellWithDelegate(&delegate_, id, bounds);
    window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);
    return window;
  }

  // Creates a Widget containing a Window with the given |bounds|. This should
  // be used when the test requires a Widget. For example any test that will
  // cause a window to be closed via
  // views::Widget::GetWidgetForNativeView(window)->Close().
  std::unique_ptr<views::Widget> CreateWindowWidget(const gfx::Rect& bounds) {
    std::unique_ptr<views::Widget> widget(new views::Widget);
    views::Widget::InitParams params;
    params.bounds = bounds;
    params.type = views::Widget::InitParams::TYPE_WINDOW;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.context = CurrentContext();
    widget->Init(params);
    widget->Show();
    aura::Window* window = widget->GetNativeWindow();
    window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);
    return widget;
  }

  bool WindowsOverlapping(aura::Window* window1, aura::Window* window2) {
    gfx::Rect window1_bounds = GetTransformedTargetBounds(window1);
    gfx::Rect window2_bounds = GetTransformedTargetBounds(window2);
    return window1_bounds.Intersects(window2_bounds);
  }

  WindowSelectorController* window_selector_controller() {
    return Shell::Get()->window_selector_controller();
  }

  WindowSelector* window_selector() {
    return window_selector_controller()->window_selector_.get();
  }

  void ToggleOverview(WindowSelector::EnterExitOverviewType type =
                          WindowSelector::EnterExitOverviewType::kNormal) {
    window_selector_controller()->ToggleOverview(type);
  }

  aura::Window* GetOverviewWindowForMinimizedState(int index,
                                                   aura::Window* window) {
    WindowSelectorItem* selector = GetWindowItemForWindow(index, window);
    return selector->GetOverviewWindowForMinimizedStateForTest();
  }

  gfx::Rect GetTransformedBounds(aura::Window* window) {
    gfx::Rect bounds_in_screen = window->layer()->bounds();
    ::wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
    gfx::RectF bounds(bounds_in_screen);
    gfx::Transform transform(gfx::TransformAboutPivot(
        gfx::ToFlooredPoint(bounds.origin()), window->layer()->transform()));
    transform.TransformRect(&bounds);
    return gfx::ToEnclosingRect(bounds);
  }

  gfx::Rect GetTransformedTargetBounds(aura::Window* window) {
    gfx::Rect bounds_in_screen = window->layer()->GetTargetBounds();
    ::wm::ConvertRectToScreen(window->parent(), &bounds_in_screen);
    gfx::RectF bounds(bounds_in_screen);
    gfx::Transform transform(
        gfx::TransformAboutPivot(gfx::ToFlooredPoint(bounds.origin()),
                                 window->layer()->GetTargetTransform()));
    transform.TransformRect(&bounds);
    return gfx::ToEnclosingRect(bounds);
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

  void SendKey(ui::KeyboardCode key, int flags = ui::EF_NONE) {
    ui::test::EventGenerator event_generator(Shell::GetPrimaryRootWindow());
    event_generator.PressKey(key, flags);
    event_generator.ReleaseKey(key, flags);
  }

  bool IsSelecting() { return window_selector_controller()->IsSelecting(); }

  const std::vector<std::unique_ptr<WindowSelectorItem>>& GetWindowItemsForRoot(
      int index) {
    return window_selector()->grid_list_[index]->window_list();
  }

  WindowSelectorItem* GetWindowItemForWindow(int grid_index,
                                             aura::Window* window) {
    const std::vector<std::unique_ptr<WindowSelectorItem>>& windows =
        GetWindowItemsForRoot(grid_index);
    auto iter =
        std::find_if(windows.cbegin(), windows.cend(),
                     [window](const std::unique_ptr<WindowSelectorItem>& item) {
                       return item->Contains(window);
                     });
    if (iter == windows.end())
      return nullptr;
    return iter->get();
  }

  // Selects |window| in the active overview session by cycling through all
  // windows in overview until it is found. Returns true if |window| was found,
  // false otherwise.
  bool SelectWindow(const aura::Window* window) {
    if (GetSelectedWindow() == nullptr)
      SendKey(ui::VKEY_TAB);
    const aura::Window* start_window = GetSelectedWindow();
    if (start_window == window)
      return true;
    do {
      SendKey(ui::VKEY_TAB);
    } while (GetSelectedWindow() != window &&
             GetSelectedWindow() != start_window);
    return GetSelectedWindow() == window;
  }

  const aura::Window* GetSelectedWindow() {
    WindowSelector* ws = window_selector();
    WindowSelectorItem* item =
        ws->grid_list_[ws->selected_grid_index_]->SelectedWindow();
    if (!item)
      return nullptr;
    return item->GetWindow();
  }

  bool selection_widget_active() {
    WindowSelector* ws = window_selector();
    return ws->grid_list_[ws->selected_grid_index_]->is_selecting();
  }

  bool showing_filter_widget() {
    return window_selector()
        ->text_filter_widget_->GetNativeWindow()
        ->layer()
        ->GetTargetTransform()
        .IsIdentity();
  }

  views::Widget* GetCloseButton(WindowSelectorItem* window) {
    return window->close_button_->GetWidget();
  }

  views::Label* GetLabelView(WindowSelectorItem* window) {
    return window->label_view_;
  }

  // Tests that a window is contained within a given WindowSelectorItem, and
  // that both the window and its matching close button are within the same
  // screen.
  void IsWindowAndCloseButtonInScreen(aura::Window* window,
                                      WindowSelectorItem* window_item) {
    aura::Window* root_window = window_item->root_window();
    EXPECT_TRUE(window_item->Contains(window));
    EXPECT_TRUE(root_window->GetBoundsInScreen().Contains(
        GetTransformedTargetBounds(window)));
    EXPECT_TRUE(
        root_window->GetBoundsInScreen().Contains(GetTransformedTargetBounds(
            GetCloseButton(window_item)->GetNativeView())));
  }

  void FilterItems(const base::StringPiece& pattern) {
    window_selector()->ContentsChanged(nullptr, base::UTF8ToUTF16(pattern));
  }

  views::Widget* text_filter_widget() {
    return window_selector()->text_filter_widget_.get();
  }

  void SetGridBounds(WindowGrid* grid, const gfx::Rect& bounds) {
    grid->bounds_ = bounds;
  }

  gfx::Rect GetGridBounds() {
    if (window_selector())
      return window_selector()->grid_list_[0]->bounds_;

    return gfx::Rect();
  }

  views::Widget* item_widget(WindowSelectorItem* item) {
    return item->item_widget_.get();
  }

  views::Widget* minimized_widget(WindowSelectorItem* item) {
    return item->transform_window_.minimized_widget();
  }

  views::Widget* backdrop_widget(WindowSelectorItem* item) {
    return item->backdrop_widget_.get();
  }

  bool HasMaskForItem(WindowSelectorItem* item) const {
    return !!item->transform_window_.mask_;
  }

  gfx::Rect GetMaskBoundsForItem(WindowSelectorItem* item) const {
    return item->transform_window_.GetMaskBoundsForTesting();
  }

 private:
  aura::test::TestWindowDelegate delegate_;
  std::unique_ptr<ShelfViewTestAPI> shelf_view_test_api_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorTest);
};

// Tests that the text field in the overview menu is repositioned and resized
// after a screen rotation.
TEST_F(WindowSelectorTest, OverviewScreenRotation) {
  const gfx::Rect bounds(400, 300);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  // In overview mode the windows should no longer overlap and the text filter
  // widget should be focused.
  ToggleOverview();

  views::Widget* text_filter = text_filter_widget();
  UpdateDisplay("400x300");

  // The text filter position is calculated as:
  // x: 0.5 * (total_bounds.width() -
  //           std::min(kTextFilterWidth, total_bounds.width())).
  // y: -kTextFilterHeight (since there's no text in the filter) - 2.
  // w: std::min(kTextFilterWidth, total_bounds.width()).
  // h: kTextFilterHeight.
  gfx::Rect expected_bounds(60, -34, 280, 32);
  EXPECT_EQ(expected_bounds, text_filter->GetClientAreaBoundsInScreen());

  // Rotates the display, which triggers the WindowSelector's
  // RepositionTextFilterOnDisplayMetricsChange method.
  UpdateDisplay("400x300/r");

  // Uses the same formulas as above using width = 300, height = 400.
  expected_bounds = gfx::Rect(10, -34, 280, 32);
  EXPECT_EQ(expected_bounds, text_filter->GetClientAreaBoundsInScreen());
}

// Tests that an a11y alert is sent on entering overview mode.
TEST_F(WindowSelectorTest, A11yAlertOnOverviewMode) {
  const gfx::Rect bounds(400, 400);
  TestAccessibilityControllerClient client;
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  controller->SetClient(client.CreateInterfacePtrAndBind());
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  EXPECT_NE(mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());
  ToggleOverview();
  controller->FlushMojoForTest();
  EXPECT_EQ(mojom::AccessibilityAlert::WINDOW_OVERVIEW_MODE_ENTERED,
            client.last_a11y_alert());
}

// Tests that there are no crashes when there is not enough screen space
// available to show all of the windows.
TEST_F(WindowSelectorTest, SmallDisplay) {
  UpdateDisplay("3x1");
  gfx::Rect bounds(0, 0, 1, 1);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  window1->SetProperty(aura::client::kTopViewInset, 0);
  window2->SetProperty(aura::client::kTopViewInset, 0);
  window3->SetProperty(aura::client::kTopViewInset, 0);
  window4->SetProperty(aura::client::kTopViewInset, 0);
  ToggleOverview();
}

// Tests entering overview mode with two windows and selecting one by clicking.
TEST_F(WindowSelectorTest, Basic) {
  const gfx::Rect bounds(400, 400);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  EXPECT_TRUE(WindowsOverlapping(window1.get(), window2.get()));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window2.get(), wm::GetFocusedWindow());
  // Hide the cursor before entering overview to test that it will be shown.
  aura::client::GetCursorClient(root_window)->HideCursor();

  // In overview mode the windows should no longer overlap and the text filter
  // widget should be focused.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));

  // Clicking window 1 should activate it.
  ClickWindow(window1.get());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));
  EXPECT_EQ(window1.get(), wm::GetFocusedWindow());

  // Cursor should have been unlocked.
  EXPECT_FALSE(aura::client::GetCursorClient(root_window)->IsCursorLocked());
}

// Tests activating minimized window.
TEST_F(WindowSelectorTest, ActivateMinimized) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  wm::WindowState* window_state = wm::GetWindowState(window.get());
  wm::WMEvent minimize_event(wm::WM_EVENT_MINIMIZE);
  window_state->OnWMEvent(&minimize_event);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED,
            wm::GetWindowState(window.get())->GetStateType());

  ToggleOverview();

  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(0.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(mojom::WindowStateType::MINIMIZED, window_state->GetStateType());
  aura::Window* window_for_minimized_window =
      GetOverviewWindowForMinimizedState(0, window.get());
  EXPECT_TRUE(window_for_minimized_window);

  const gfx::Point point =
      GetTransformedBoundsInRootWindow(window_for_minimized_window)
          .CenterPoint();
  ui::test::EventGenerator event_generator(window->GetRootWindow(), point);
  event_generator.ClickLeftButton();

  EXPECT_FALSE(IsSelecting());

  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(1.f, window->layer()->GetTargetOpacity());
  EXPECT_EQ(mojom::WindowStateType::NORMAL, window_state->GetStateType());
}

// Tests that entering overview mode with an App-list active properly focuses
// and activates the overview text filter window.
TEST_F(WindowSelectorTest, TextFilterActive) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  wm::ActivateWindow(window.get());

  EXPECT_TRUE(wm::IsActiveWindow(window.get()));
  EXPECT_EQ(window.get(), wm::GetFocusedWindow());

  // Pass an enum to satisfy the function, it is arbitrary and will not affect
  // histograms.
  GetAppListTestHelper()->ToggleAndRunLoop(GetPrimaryDisplay().id(),
                                           app_list::kShelfButton);

  // Activating overview cancels the App-list which normally would activate the
  // previously active |window1|. Overview mode should properly transfer focus
  // and activation to the text filter widget.
  ToggleOverview();
  EXPECT_FALSE(wm::IsActiveWindow(window.get()));
  EXPECT_TRUE(wm::IsActiveWindow(wm::GetFocusedWindow()));
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
}

// Verifies the whether overview mode is still active after the text filter
// window loses activation in certain circumstances.
TEST_F(WindowSelectorTest, TextFilterDeactivated) {
  UpdateDisplay("600x600");
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  wm::ActivateWindow(window.get());

  // Click somewhere on the screen not on the shelf and not on the overview
  // window. This will cause a activation change which should close overview
  // mode.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     gfx::Point(500, 400));
  generator.ClickLeftButton();
  ASSERT_FALSE(IsSelecting());

  // Click somewhere on the screen not on the shelf and not on the overview
  // window. This will cause a activation change but will not close overview
  // mode since a overview to home launcher drag is underway.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
  Shell::Get()
      ->app_list_controller()
      ->home_launcher_gesture_handler()
      ->OnPressEvent(HomeLauncherGestureHandler::Mode::kSlideUpToShow,
                     gfx::Point());
  generator.ClickLeftButton();
  EXPECT_TRUE(IsSelecting());
}

// Tests that the ordering of windows is stable across different overview
// sessions even when the windows have the same bounds.
TEST_F(WindowSelectorTest, WindowsOrder) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindowWithId(bounds, 1));
  std::unique_ptr<aura::Window> window2(CreateWindowWithId(bounds, 2));
  std::unique_ptr<aura::Window> window3(CreateWindowWithId(bounds, 3));

  // The order of windows in overview mode is MRU.
  wm::GetWindowState(window1.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview1 =
      GetWindowItemsForRoot(0);
  EXPECT_EQ(1, overview1[0]->GetWindow()->id());
  EXPECT_EQ(3, overview1[1]->GetWindow()->id());
  EXPECT_EQ(2, overview1[2]->GetWindow()->id());
  ToggleOverview();

  // Activate the second window.
  wm::GetWindowState(window2.get())->Activate();
  ToggleOverview();
  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview2 =
      GetWindowItemsForRoot(0);

  // The order should be MRU.
  EXPECT_EQ(2, overview2[0]->GetWindow()->id());
  EXPECT_EQ(1, overview2[1]->GetWindow()->id());
  EXPECT_EQ(3, overview2[2]->GetWindow()->id());
  ToggleOverview();
}

// Tests selecting a window by tapping on it.
TEST_F(WindowSelectorTest, BasicGesture) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(window1.get(), wm::GetFocusedWindow());
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window2.get());
  generator.GestureTapAt(
      GetTransformedTargetBounds(window2.get()).CenterPoint());
  EXPECT_EQ(window2.get(), wm::GetFocusedWindow());
}

// Tests that the user action WindowSelector_ActiveWindowChanged is
// recorded when the mouse/touchscreen/keyboard are used to select a window
// in overview mode which is different from the previously-active window.
TEST_F(WindowSelectorTest, ActiveWindowChangedUserActionRecorded) {
  base::UserActionTester user_action_tester;
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // Tap on |window2| to activate it and exit overview.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window2.get());
  generator.GestureTapAt(
      GetTransformedTargetBounds(window2.get()).CenterPoint());
  EXPECT_EQ(
      1, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Click on |window2| to activate it and exit overview.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_EQ(
      2, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // Select |window2| using the arrow keys. Activate it (and exit overview) by
  // pressing the return key.
  wm::ActivateWindow(window1.get());
  ToggleOverview();
  ASSERT_TRUE(SelectWindow(window2.get()));
  SendKey(ui::VKEY_RETURN);
  EXPECT_EQ(
      3, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that the user action WindowSelector_ActiveWindowChanged is not
// recorded when the mouse/touchscreen/keyboard are used to select the
// already-active window from overview mode. Also verifies that entering and
// exiting overview without selecting a window does not record the action.
TEST_F(WindowSelectorTest, ActiveWindowChangedUserActionNotRecorded) {
  base::UserActionTester user_action_tester;
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // Set |window1| to be initially active.
  wm::ActivateWindow(window1.get());
  ToggleOverview();

  // Tap on |window1| to exit overview.
  ui::test::EventGenerator generator(Shell::GetPrimaryRootWindow(),
                                     window1.get());
  generator.GestureTapAt(
      GetTransformedTargetBounds(window1.get()).CenterPoint());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Click on it to exit overview.
  ASSERT_EQ(window1.get(), wm::GetFocusedWindow());
  ToggleOverview();
  ClickWindow(window1.get());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));

  // |window1| remains active. Select using the keyboard.
  ASSERT_EQ(window1.get(), wm::GetFocusedWindow());
  ToggleOverview();
  ASSERT_TRUE(SelectWindow(window1.get()));
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
TEST_F(WindowSelectorTest, ActiveWindowChangedUserActionWindowClose) {
  base::UserActionTester user_action_tester;
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(400, 400));

  ToggleOverview();

  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() + 5);
  ui::test::EventGenerator event_generator(window->GetRootWindow(), point);

  ASSERT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  ASSERT_TRUE(widget->IsClosed());
  EXPECT_EQ(
      0, user_action_tester.GetActionCount(kActiveWindowChangedFromOverview));
}

// Tests that we do not crash and overview mode remains engaged if the desktop
// is tapped while a finger is already down over a window.
TEST_F(WindowSelectorTest, NoCrashWithDesktopTap) {
  std::unique_ptr<aura::Window> window(
      CreateWindow(gfx::Rect(200, 300, 250, 450)));

  ToggleOverview();

  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window.get());
  ui::test::EventGenerator event_generator(window->GetRootWindow(),
                                           bounds.CenterPoint());

  // Press down on the window.
  const int kTouchId = 19;
  event_generator.PressTouchId(kTouchId);

  // Tap on the desktop, which should not cause a crash. Overview mode should
  // be disengaged.
  event_generator.GestureTapAt(gfx::Point(0, 0));
  EXPECT_FALSE(IsSelecting());

  event_generator.ReleaseTouchId(kTouchId);
}

// Tests that we do not crash and a window is selected when appropriate when
// we click on a window during touch.
TEST_F(WindowSelectorTest, ClickOnWindowDuringTouch) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));

  ToggleOverview();

  gfx::Rect window1_bounds = GetTransformedBoundsInRootWindow(window1.get());
  ui::test::EventGenerator event_generator(window1->GetRootWindow(),
                                           window1_bounds.CenterPoint());

  // Clicking on |window2| while touching on |window1| should not cause a
  // crash, it should do nothing since overview only handles one click or touch
  // at a time.
  const int kTouchId = 19;
  event_generator.PressTouchId(kTouchId);
  event_generator.MoveMouseToCenterOf(window2.get());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(IsSelecting());
  EXPECT_FALSE(wm::IsActiveWindow(window2.get()));

  // Clicking on |window1| while touching on |window1| should not cause
  // a crash, overview mode should be disengaged, and |window1| should
  // be active.
  event_generator.MoveMouseToCenterOf(window1.get());
  event_generator.ClickLeftButton();
  EXPECT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));
  event_generator.ReleaseTouchId(kTouchId);
}

// Tests that a window does not receive located events when in overview mode.
TEST_F(WindowSelectorTest, WindowDoesNotReceiveEvents) {
  gfx::Rect window_bounds(20, 10, 200, 300);
  aura::Window* root_window = Shell::GetPrimaryRootWindow();
  std::unique_ptr<aura::Window> window(CreateWindow(window_bounds));

  gfx::Point point1(window_bounds.x() + 10, window_bounds.y() + 10);

  ui::MouseEvent event1(ui::ET_MOUSE_PRESSED, point1, point1,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);

  ui::EventTarget* root_target = root_window;
  ui::EventTargeter* targeter =
      root_window->GetHost()->dispatcher()->GetDefaultEventTargeter();

  // The event should target the window because we are still not in overview
  // mode.
  EXPECT_EQ(window.get(), targeter->FindTargetForEvent(root_target, &event1));

  ToggleOverview();

  // The bounds have changed, take that into account.
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window.get());
  gfx::Point point2(bounds.x() + 10, bounds.y() + 10);
  ui::MouseEvent event2(ui::ET_MOUSE_PRESSED, point2, point2,
                        ui::EventTimeForNow(), ui::EF_NONE, ui::EF_NONE);

  // Now the transparent window should be intercepting this event.
  EXPECT_NE(window.get(), targeter->FindTargetForEvent(root_target, &event2));
}

// Tests that clicking on the close button effectively closes the window.
TEST_F(WindowSelectorTest, CloseButton) {
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(0, 0, 400, 400));

  std::unique_ptr<views::Widget> minimized_widget =
      CreateWindowWidget(gfx::Rect(400, 0, 400, 400));
  minimized_widget->Minimize();

  ToggleOverview();

  aura::Window* window = widget->GetNativeWindow();
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() + 5);
  ui::test::EventGenerator event_generator(window->GetRootWindow(), point);

  EXPECT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());

  EXPECT_TRUE(IsSelecting());

  aura::Window* window_for_minimized_window =
      GetOverviewWindowForMinimizedState(0,
                                         minimized_widget->GetNativeWindow());
  ASSERT_TRUE(window_for_minimized_window);
  const gfx::Rect rect =
      GetTransformedBoundsInRootWindow(window_for_minimized_window);

  event_generator.MoveMouseTo(
      gfx::Point(rect.top_right().x() - 10, rect.top_right().y() - 10));

  EXPECT_FALSE(minimized_widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(minimized_widget->IsClosed());

  // All minimized windows are closed, so it should exit overview mode.
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(IsSelecting());
}

// Tests minimizing/unminimizing in overview mode.
TEST_F(WindowSelectorTest, MinimizeUnminimize) {
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(400, 400));
  aura::Window* window = widget->GetNativeWindow();

  ToggleOverview();

  EXPECT_FALSE(GetOverviewWindowForMinimizedState(0, window));
  widget->Minimize();
  EXPECT_TRUE(widget->IsMinimized());
  EXPECT_TRUE(IsSelecting());

  EXPECT_TRUE(GetOverviewWindowForMinimizedState(0, window));

  widget->Restore();
  EXPECT_FALSE(widget->IsMinimized());

  EXPECT_FALSE(GetOverviewWindowForMinimizedState(0, window));
  EXPECT_TRUE(IsSelecting());
}

// Tests that clicking on the close button on a secondary display effectively
// closes the window.
TEST_F(WindowSelectorTest, CloseButtonOnMultipleDisplay) {
  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();

  std::unique_ptr<aura::Window> window1(
      CreateWindow(gfx::Rect(650, 300, 250, 450)));

  // We need a widget for the close button to work because windows are closed
  // via the widget. We also use the widget to determine if the window has been
  // closed or not. We explicity create the widget so that the window can be
  // parented to a non-primary root window.
  std::unique_ptr<views::Widget> widget(new views::Widget);
  views::Widget::InitParams params;
  params.bounds = gfx::Rect(650, 0, 400, 400);
  params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
  params.parent = window1->parent();
  widget->Init(params);
  widget->Show();
  aura::Window* window = widget->GetNativeWindow();
  window->SetProperty(aura::client::kTopViewInset, kHeaderHeight);

  ASSERT_EQ(root_windows[1], window1->GetRootWindow());

  ToggleOverview();

  aura::Window* window2 = widget->GetNativeWindow();
  gfx::Rect bounds = GetTransformedBoundsInRootWindow(window2);
  gfx::Point point(bounds.top_right().x() - 1, bounds.top_right().y() + 5);
  ui::test::EventGenerator event_generator(window2->GetRootWindow(), point);

  EXPECT_FALSE(widget->IsClosed());
  event_generator.ClickLeftButton();
  EXPECT_TRUE(widget->IsClosed());
}

// Tests entering overview mode with two windows and selecting one.
TEST_F(WindowSelectorTest, FullscreenWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window1.get());

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window1.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsFullscreen());

  // Enter overview and select the fullscreen window.
  ToggleOverview();

  // The window is still fullscreen as it was selected.
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsFullscreen());

  // Entering overview and selecting another window, the previous window remains
  // fullscreen.
  ToggleOverview();
  ClickWindow(window2.get());
  EXPECT_TRUE(wm::GetWindowState(window1.get())->IsFullscreen());
}

TEST_F(WindowSelectorTest, SkipOverviewWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  window2->SetProperty(ash::kHideInOverviewKey, true);

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

// Tests that entering overview when a fullscreen window is active in maximized
// mode correctly applies the transformations to the window and correctly
// updates the window bounds on exiting overview mode: http://crbug.com/401664.
TEST_F(WindowSelectorTest, FullscreenWindowTabletMode) {
  UpdateDisplay("800x600");
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());
  gfx::Rect normal_window_bounds(window1->bounds());
  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window1.get())->OnWMEvent(&toggle_fullscreen_event);
  gfx::Rect fullscreen_window_bounds(window1->bounds());
  EXPECT_NE(normal_window_bounds, fullscreen_window_bounds);
  EXPECT_EQ(fullscreen_window_bounds, window2->GetTargetBounds());

  const gfx::Rect fullscreen(800, 600);
  const int shelf_inset = 600 - ShelfConstants::shelf_size();
  const gfx::Rect normal_work_area(800, shelf_inset);
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(gfx::Rect(800, 600),
            screen->GetDisplayNearestWindow(window1.get()).work_area());

  ToggleOverview();
  EXPECT_EQ(fullscreen,
            screen->GetDisplayNearestWindow(window1.get()).work_area());

  // Window 2 would normally resize to normal window bounds on showing the shelf
  // for overview but this is deferred until overview is exited.
  EXPECT_EQ(fullscreen_window_bounds, window2->GetTargetBounds());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), window2.get()));
  ToggleOverview();
  EXPECT_EQ(fullscreen,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  // Since the fullscreen window is still active, window2 will still have the
  // larger bounds.
  EXPECT_EQ(fullscreen_window_bounds, window2->GetTargetBounds());

  // Enter overview again and select window 2. Selecting window 2 should show
  // the shelf bringing window2 back to the normal bounds.
  ToggleOverview();
  ClickWindow(window2.get());
  // Selecting non fullscreen window should set the work area back to normal.
  EXPECT_EQ(normal_work_area,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  EXPECT_EQ(normal_window_bounds, window2->GetTargetBounds());

  ToggleOverview();
  EXPECT_EQ(normal_work_area,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
  ClickWindow(window1.get());
  // Selecting fullscreen. The work area should be updated to fullscreen as
  // well.
  EXPECT_EQ(fullscreen,
            screen->GetDisplayNearestWindow(window1.get()).work_area());
}

// Tests that beginning window selection hides the app list.
TEST_F(WindowSelectorTest, SelectingHidesAppList) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplay().id());
  GetAppListTestHelper()->CheckVisibility(true);

  ToggleOverview();
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  ToggleOverview();
}

// Tests that a minimized window's visibility and layer visibility
// stay invisible (A minimized window is cloned during overview),
// and ignored_by_shelf state is restored upon exit.
TEST_F(WindowSelectorTest, MinimizedWindowState) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  wm::WindowState* window_state = wm::GetWindowState(window1.get());
  window_state->Minimize();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  EXPECT_FALSE(window_state->ignored_by_shelf());

  ToggleOverview();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  EXPECT_TRUE(window_state->ignored_by_shelf());

  ToggleOverview();
  EXPECT_FALSE(window1->IsVisible());
  EXPECT_FALSE(window1->layer()->GetTargetVisibility());
  EXPECT_FALSE(window_state->ignored_by_shelf());
}

// Tests that a bounds change during overview is corrected for.
TEST_F(WindowSelectorTest, BoundsChangeDuringOverview) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 400, 400)));
  // Use overview headers above the window in this test.
  window->SetProperty(aura::client::kTopViewInset, 0);
  ToggleOverview();
  gfx::Rect overview_bounds = GetTransformedTargetBounds(window.get());
  window->SetBounds(gfx::Rect(200, 0, 200, 200));
  gfx::Rect new_overview_bounds = GetTransformedTargetBounds(window.get());
  EXPECT_EQ(overview_bounds.x(), new_overview_bounds.x());
  EXPECT_EQ(overview_bounds.y(), new_overview_bounds.y());
  EXPECT_EQ(overview_bounds.width(), new_overview_bounds.width());
  EXPECT_EQ(overview_bounds.height(), new_overview_bounds.height());
  ToggleOverview();
}

// Tests that a newly created window aborts overview.
TEST_F(WindowSelectorTest, NewWindowCancelsOveriew) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  // A window being created should exit overview mode.
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  EXPECT_FALSE(IsSelecting());
}

// Tests that a window activation exits overview mode.
TEST_F(WindowSelectorTest, ActivationCancelsOveriew) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  window2->Focus();
  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  // A window being activated should exit overview mode.
  window1->Focus();
  EXPECT_FALSE(IsSelecting());

  // window1 should be focused after exiting even though window2 was focused on
  // entering overview because we exited due to an activation.
  EXPECT_EQ(window1.get(), wm::GetFocusedWindow());
}

// Tests that exiting overview mode without selecting a window restores focus
// to the previously focused window.
TEST_F(WindowSelectorTest, CancelRestoresFocus) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  wm::ActivateWindow(window.get());
  EXPECT_EQ(window.get(), wm::GetFocusedWindow());

  // In overview mode, the text filter widget should be focused.
  ToggleOverview();
  EXPECT_EQ(text_filter_widget()->GetNativeWindow(), wm::GetFocusedWindow());

  // If canceling overview mode, focus should be restored.
  ToggleOverview();
  EXPECT_EQ(window.get(), wm::GetFocusedWindow());
}

// Tests that overview mode is exited if the last remaining window is destroyed.
TEST_F(WindowSelectorTest, LastWindowDestroyed) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  ToggleOverview();

  window1.reset();
  window2.reset();
  EXPECT_FALSE(IsSelecting());
}

// Tests that entering overview mode restores a window to its original
// target location.
TEST_F(WindowSelectorTest, QuickReentryRestoresInitialTransform) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
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
  EXPECT_FALSE(IsSelecting());
  EXPECT_EQ(initial_bounds, GetTransformedTargetBounds(window.get()));
}

// Tests that windows with modal child windows are transformed with the modal
// child even though not activatable themselves.
TEST_F(WindowSelectorTest, ModalChild) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> child1(CreateWindow(bounds));
  child1->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::AddTransientChild(window1.get(), child1.get());
  EXPECT_EQ(window1->parent(), child1->parent());
  ToggleOverview();
  EXPECT_TRUE(window1->IsVisible());
  EXPECT_TRUE(child1->IsVisible());
  EXPECT_EQ(GetTransformedTargetBounds(child1.get()),
            GetTransformedTargetBounds(window1.get()));
  ToggleOverview();
}

// Tests that clicking a modal window's parent activates the modal window in
// overview.
TEST_F(WindowSelectorTest, ClickModalWindowParent) {
  std::unique_ptr<aura::Window> window1(
      CreateWindow(gfx::Rect(0, 0, 180, 180)));
  std::unique_ptr<aura::Window> child1(
      CreateWindow(gfx::Rect(200, 0, 180, 180)));
  child1->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::AddTransientChild(window1.get(), child1.get());
  EXPECT_FALSE(WindowsOverlapping(window1.get(), child1.get()));
  EXPECT_EQ(window1->parent(), child1->parent());
  ToggleOverview();
  // Given that their relative positions are preserved, the windows should still
  // not overlap.
  EXPECT_FALSE(WindowsOverlapping(window1.get(), child1.get()));
  ClickWindow(window1.get());
  EXPECT_FALSE(IsSelecting());

  // Clicking on window1 should activate child1.
  EXPECT_TRUE(wm::IsActiveWindow(child1.get()));
}

// Tests that windows remain on the display they are currently on in overview
// mode, and that the close buttons are on matching displays.
TEST_F(WindowSelectorTest, MultipleDisplays) {
  UpdateDisplay("600x400,600x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect bounds1(0, 0, 400, 400);
  gfx::Rect bounds2(650, 0, 400, 400);

  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds2));
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
  IsWindowAndCloseButtonInScreen(window1.get(),
                                 GetWindowItemForWindow(0, window1.get()));
  IsWindowAndCloseButtonInScreen(window2.get(),
                                 GetWindowItemForWindow(0, window2.get()));
  IsWindowAndCloseButtonInScreen(window3.get(),
                                 GetWindowItemForWindow(1, window3.get()));
  IsWindowAndCloseButtonInScreen(window4.get(),
                                 GetWindowItemForWindow(1, window4.get()));
}

// Tests shutting down during overview.
TEST_F(WindowSelectorTest, Shutdown) {
  const gfx::Rect bounds(400, 400);
  // These windows will be deleted when the test exits and the Shell instance
  // is shut down.
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
}

// Tests removing a display during overview.
TEST_F(WindowSelectorTest, RemoveDisplay) {
  UpdateDisplay("400x400,400x400");
  gfx::Rect bounds1(0, 0, 100, 100);
  gfx::Rect bounds2(450, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds2));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_TRUE(IsSelecting());
  UpdateDisplay("400x400");
  EXPECT_FALSE(IsSelecting());
}

// Tests removing a display during overview with NON_ZERO_DURATION animation.
TEST_F(WindowSelectorTest, RemoveDisplayWithAnimation) {
  UpdateDisplay("400x400,400x400");
  gfx::Rect bounds1(0, 0, 100, 100);
  gfx::Rect bounds2(450, 0, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds2));

  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  ToggleOverview();
  EXPECT_TRUE(IsSelecting());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  UpdateDisplay("400x400");
  EXPECT_FALSE(IsSelecting());
}

// Tests that toggling overview on and off does not cancel drag.
TEST_F(WindowSelectorTest, DragDropInProgress) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      new TestDragWindowDelegate(), -1, bounds));

  ui::test::EventGenerator event_generator(window->GetRootWindow(),
                                           window.get());
  event_generator.PressLeftButton();
  event_generator.MoveMouseBy(10, 10);
  EXPECT_EQ(gfx::Rect(10, 10, 100, 100), window->bounds());

  ToggleOverview();
  ASSERT_TRUE(IsSelecting());

  event_generator.MoveMouseBy(10, 10);

  ToggleOverview();
  ASSERT_FALSE(IsSelecting());

  event_generator.MoveMouseBy(10, 10);
  event_generator.ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(gfx::Rect(30, 30, 100, 100), window->bounds());
}

// Test that a label is created under the window on entering overview mode.
TEST_F(WindowSelectorTest, CreateLabelUnderWindow) {
  std::unique_ptr<aura::Window> window(CreateWindow(gfx::Rect(0, 0, 300, 500)));
  const base::string16 window_title = base::UTF8ToUTF16("My window");
  window->SetTitle(window_title);
  ToggleOverview();
  WindowSelectorItem* window_item = GetWindowItemsForRoot(0).back().get();
  views::Label* label = GetLabelView(window_item);
  ASSERT_TRUE(label);

  // Verify the label matches the window title.
  EXPECT_EQ(window_title, label->text());

  // Update the window title and check that the label is updated, too.
  const base::string16 updated_title = base::UTF8ToUTF16("Updated title");
  window->SetTitle(updated_title);
  EXPECT_EQ(updated_title, label->text());

  // Labels are located based on target_bounds, not the actual window item
  // bounds.
  gfx::Rect label_bounds = label->GetWidget()->GetWindowBoundsInScreen();
  label_bounds.Inset(kWindowMargin, kWindowMargin);
  EXPECT_EQ(label_bounds, window_item->target_bounds());
}

// Tests that overview updates the window positions if the display orientation
// changes.
TEST_F(WindowSelectorTest, DisplayOrientationChanged) {
  aura::Window* root_window = Shell::Get()->GetPrimaryRootWindow();
  UpdateDisplay("600x200");
  EXPECT_EQ("0,0 600x200", root_window->bounds().ToString());
  gfx::Rect window_bounds(0, 0, 150, 150);
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (int i = 0; i < 3; i++)
    windows.push_back(base::WrapUnique(CreateWindow(window_bounds)));

  ToggleOverview();
  for (const auto& window : windows) {
    EXPECT_TRUE(root_window->bounds().Contains(
        GetTransformedTargetBounds(window.get())));
  }

  // Rotate the display, windows should be repositioned to be within the screen
  // bounds.
  UpdateDisplay("600x200/r");
  EXPECT_EQ("0,0 200x600", root_window->bounds().ToString());
  for (const auto& window : windows) {
    EXPECT_TRUE(root_window->bounds().Contains(
        GetTransformedTargetBounds(window.get())));
  }
}

// Tests traversing some windows in overview mode with the tab key.
TEST_F(WindowSelectorTest, BasicTabKeyNavigation) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ToggleOverview();

  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_windows =
      GetWindowItemsForRoot(0);
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[0]->GetWindow());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[1]->GetWindow());
  SendKey(ui::VKEY_TAB);
  EXPECT_EQ(GetSelectedWindow(), overview_windows[0]->GetWindow());
}

// Tests that pressing Ctrl+W while a window is selected in overview closes it.
TEST_F(WindowSelectorTest, CloseWindowWithKey) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<views::Widget> widget =
      CreateWindowWidget(gfx::Rect(0, 0, 400, 400));
  aura::Window* window1 = widget->GetNativeWindow();
  ToggleOverview();

  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(window1, GetSelectedWindow());
  SendKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  EXPECT_TRUE(widget->IsClosed());
}

// Tests traversing some windows in overview mode with the arrow keys in every
// possible direction.
TEST_F(WindowSelectorTest, BasicArrowKeyNavigation) {
  const size_t test_windows = 9;
  UpdateDisplay("800x600");
  std::vector<std::unique_ptr<aura::Window>> windows;
  for (size_t i = test_windows; i > 0; i--) {
    windows.push_back(
        base::WrapUnique(CreateWindowWithId(gfx::Rect(0, 0, 100, 100), i)));
  }

  ui::KeyboardCode arrow_keys[] = {ui::VKEY_RIGHT, ui::VKEY_DOWN, ui::VKEY_LEFT,
                                   ui::VKEY_UP};
  // The rows contain variable number of items making vertical navigation not
  // feasible. [Down] is equivalent to [Right] and [Up] is equivalent to [Left].
  int index_path_for_direction[][test_windows + 1] = {
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 1},  // Right
      {1, 2, 3, 4, 5, 6, 7, 8, 9, 1},  // Down (same as Right)
      {9, 8, 7, 6, 5, 4, 3, 2, 1, 9},  // Left
      {9, 8, 7, 6, 5, 4, 3, 2, 1, 9}   // Up (same as Left)
  };

  for (size_t key_index = 0; key_index < arraysize(arrow_keys); key_index++) {
    ToggleOverview();
    const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_windows =
        GetWindowItemsForRoot(0);
    for (size_t i = 0; i < test_windows + 1; i++) {
      SendKey(arrow_keys[key_index]);
      // TODO(flackr): Add a more readable error message by constructing a
      // string from the window IDs.
      const int index = index_path_for_direction[key_index][i];
      EXPECT_EQ(GetSelectedWindow()->id(),
                overview_windows[index - 1]->GetWindow()->id());
    }
    ToggleOverview();
  }
}

// Verifies hitting the escape and back keys exit overview mode.
TEST_F(WindowSelectorTest, ExitOverviewWithKey) {
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  SendKey(ui::VKEY_ESCAPE);
  EXPECT_FALSE(window_selector_controller()->IsSelecting());

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  SendKey(ui::VKEY_BROWSER_BACK);
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
}

// Tests basic selection across multiple monitors.
TEST_F(WindowSelectorTest, BasicMultiMonitorArrowKeyNavigation) {
  UpdateDisplay("400x400,400x400");
  gfx::Rect bounds1(0, 0, 100, 100);
  gfx::Rect bounds2(450, 0, 100, 100);
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds1));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));

  ToggleOverview();

  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_root1 =
      GetWindowItemsForRoot(0);
  const std::vector<std::unique_ptr<WindowSelectorItem>>& overview_root2 =
      GetWindowItemsForRoot(1);
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root1[0]->GetWindow());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root1[1]->GetWindow());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root2[0]->GetWindow());
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), overview_root2[1]->GetWindow());
}

// Tests first monitor when display order doesn't match left to right screen
// positions.
TEST_F(WindowSelectorTest, MultiMonitorReversedOrder) {
  UpdateDisplay("400x400,400x400");
  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(display_manager(),
                                         display::DisplayPlacement::LEFT, 0));
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  gfx::Rect bounds1(-350, 0, 100, 100);
  gfx::Rect bounds2(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  EXPECT_EQ(root_windows[1], window1->GetRootWindow());
  EXPECT_EQ(root_windows[0], window2->GetRootWindow());

  ToggleOverview();

  // Coming from the left to right, we should select window1 first being on the
  // display to the left.
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window1.get());

  ToggleOverview();
  ToggleOverview();

  // Coming from right to left, we should select window2 first being on the
  // display on the right.
  SendKey(ui::VKEY_LEFT);
  EXPECT_EQ(GetSelectedWindow(), window2.get());
}

// Tests three monitors where the grid becomes empty on one of the monitors.
TEST_F(WindowSelectorTest, ThreeMonitor) {
  UpdateDisplay("400x400,400x400,400x400");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  const gfx::Rect bounds1(0, 0, 100, 100);
  const gfx::Rect bounds2(400, 0, 100, 100);
  const gfx::Rect bounds3(800, 0, 100, 100);
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds3));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds2));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds1));
  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());
  EXPECT_EQ(root_windows[2], window3->GetRootWindow());

  ToggleOverview();

  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  EXPECT_EQ(GetSelectedWindow(), window3.get());

  // If the last window on a display closes it should select the previous
  // display's window.
  window3.reset();
  EXPECT_EQ(GetSelectedWindow(), window2.get());
  ToggleOverview();

  window3.reset(CreateWindow(bounds3));
  ToggleOverview();
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);
  SendKey(ui::VKEY_RIGHT);

  // If the window on the second display is removed, the selected window should
  // remain window3.
  EXPECT_EQ(GetSelectedWindow(), window3.get());
  window2.reset();
  EXPECT_EQ(GetSelectedWindow(), window3.get());
}

// Tests selecting a window in overview mode with the return key.
TEST_F(WindowSelectorTest, SelectWindowWithReturnKey) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ToggleOverview();

  // Pressing the return key without a selection widget should not do anything.
  SendKey(ui::VKEY_RETURN);
  EXPECT_TRUE(IsSelecting());

  // Select the first window.
  ASSERT_TRUE(SelectWindow(window1.get()));
  SendKey(ui::VKEY_RETURN);
  ASSERT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Select the second window.
  ToggleOverview();
  ASSERT_TRUE(SelectWindow(window2.get()));
  SendKey(ui::VKEY_RETURN);
  EXPECT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::IsActiveWindow(window2.get()));
}

// Creates three windows and tests filtering them by title.
TEST_F(WindowSelectorTest, BasicTextFiltering) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  base::string16 window2_title = base::UTF8ToUTF16("Highway to test");
  base::string16 window1_title = base::UTF8ToUTF16("For those about to test");
  base::string16 window0_title = base::UTF8ToUTF16("We salute you");
  window0->SetTitle(window0_title);
  window1->SetTitle(window1_title);
  window2->SetTitle(window2_title);
  ToggleOverview();

  EXPECT_FALSE(selection_widget_active());
  EXPECT_FALSE(showing_filter_widget());
  FilterItems("Test");

  // The selection widget should appear when filtering starts, and should be
  // selecting one of the matching windows above.
  EXPECT_TRUE(selection_widget_active());
  EXPECT_TRUE(showing_filter_widget());
  // window0 does not contain the text "test".
  EXPECT_NE(GetSelectedWindow(), window0.get());

  // Window 0 has no "test" on it so it should be the only dimmed item.
  const int grid_index = 0;
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window0.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window1.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window2.get())->dimmed());

  // No items match the search.
  FilterItems("I'm testing 'n testing");
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window0.get())->dimmed());
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window1.get())->dimmed());
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window2.get())->dimmed());

  // All the items should match the empty string. The filter widget should also
  // disappear.
  FilterItems("");
  EXPECT_FALSE(showing_filter_widget());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window0.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window1.get())->dimmed());
  EXPECT_FALSE(GetWindowItemForWindow(grid_index, window2.get())->dimmed());

  FilterItems("Foo");

  EXPECT_NE(1.0f, window0->layer()->GetTargetOpacity());
  EXPECT_NE(1.0f, window1->layer()->GetTargetOpacity());
  EXPECT_NE(1.0f, window2->layer()->GetTargetOpacity());

  ToggleOverview();

  EXPECT_EQ(1.0f, window0->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window1->layer()->GetTargetOpacity());
  EXPECT_EQ(1.0f, window2->layer()->GetTargetOpacity());
}

// Tests selecting in the overview with dimmed and undimmed items.
TEST_F(WindowSelectorTest, TextFilteringSelection) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  base::string16 window2_title = base::UTF8ToUTF16("Rock and roll");
  base::string16 window1_title = base::UTF8ToUTF16("Rock and");
  base::string16 window0_title = base::UTF8ToUTF16("Rock");
  window0->SetTitle(window0_title);
  window1->SetTitle(window1_title);
  window2->SetTitle(window2_title);
  ToggleOverview();
  EXPECT_TRUE(SelectWindow(window0.get()));
  EXPECT_TRUE(selection_widget_active());

  // Dim the first item, the selection should jump to the next item.
  FilterItems("Rock and");
  EXPECT_NE(GetSelectedWindow(), window0.get());

  // Cycle the selection, the dimmed window should not be selected.
  EXPECT_FALSE(SelectWindow(window0.get()));

  // Dimming all the items should hide the selection widget.
  FilterItems("Pop");
  EXPECT_FALSE(selection_widget_active());

  // Undimming one window should automatically select it.
  FilterItems("Rock and roll");
  EXPECT_EQ(GetSelectedWindow(), window2.get());
}

// Tests that transferring focus from the text filter to a window that is not a
// top level window does not cancel overview mode.
TEST_F(WindowSelectorTest, ShowTextFilterMenu) {
  gfx::Rect bounds(0, 0, 100, 100);
  std::unique_ptr<aura::Window> window0(CreateWindow(bounds));
  base::string16 window0_title = base::UTF8ToUTF16("Test");
  window0->SetTitle(window0_title);
  wm::GetWindowState(window0.get())->Minimize();
  ToggleOverview();

  EXPECT_FALSE(selection_widget_active());
  EXPECT_FALSE(showing_filter_widget());
  FilterItems("Test");

  EXPECT_TRUE(selection_widget_active());
  EXPECT_TRUE(showing_filter_widget());

  // Open system bubble shifting focus from the text filter.
  GetPrimaryUnifiedSystemTray()->ShowBubble(false /* show_by_click */);

  base::RunLoop().RunUntilIdle();

  // This should not cancel overview mode.
  ASSERT_TRUE(IsSelecting());
  EXPECT_TRUE(selection_widget_active());
  EXPECT_TRUE(showing_filter_widget());

  // Click text filter to bring focus back.
  ui::test::EventGenerator* generator = GetEventGenerator();
  gfx::Point point_in_text_filter =
      text_filter_widget()->GetWindowBoundsInScreen().CenterPoint();
  generator->MoveMouseTo(point_in_text_filter);
  generator->ClickLeftButton();
  EXPECT_TRUE(IsSelecting());

  // Cancel overview mode.
  ToggleOverview();
  ASSERT_FALSE(IsSelecting());
}

// Tests clicking on the desktop itself to cancel overview mode.
TEST_F(WindowSelectorTest, CancelOverviewOnMouseClick) {
  // Overview disabled by default.
  EXPECT_FALSE(IsSelecting());

  // Point and bounds selected so that they don't intersect. This causes
  // events located at the point to be passed to WallpaperController,
  // and not the window.
  gfx::Point point_in_background_page(0, 0);
  gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ui::test::EventGenerator* generator = GetEventGenerator();
  // Move mouse to point in the background page. Sending an event here will pass
  // it to the WallpaperController in both regular and overview mode.
  generator->MoveMouseTo(point_in_background_page);

  // Clicking on the background page while not in overview should not toggle
  // overview.
  generator->ClickLeftButton();
  EXPECT_FALSE(IsSelecting());

  // Switch to overview mode.
  ToggleOverview();
  ASSERT_TRUE(IsSelecting());

  // Click should now exit overview mode.
  generator->ClickLeftButton();
  EXPECT_FALSE(IsSelecting());
}

// Tests tapping on the desktop itself to cancel overview mode.
TEST_F(WindowSelectorTest, CancelOverviewOnTap) {
  // Overview disabled by default.
  EXPECT_FALSE(IsSelecting());

  // Point and bounds selected so that they don't intersect. This causes
  // events located at the point to be passed to WallpaperController,
  // and not the window.
  gfx::Point point_in_background_page(0, 0);
  gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  ui::test::EventGenerator* generator = GetEventGenerator();

  // Tapping on the background page while not in overview should not toggle
  // overview.
  generator->GestureTapAt(point_in_background_page);
  EXPECT_FALSE(IsSelecting());

  // Switch to overview mode.
  ToggleOverview();
  ASSERT_TRUE(IsSelecting());

  // Tap should now exit overview mode.
  generator->GestureTapAt(point_in_background_page);
  EXPECT_FALSE(IsSelecting());
}

// Tests that transformed Rect scaling preserves its aspect ratio.
// The window scale is determined by the target height and so the test is
// actually testing that the width is calculated correctly. Since all
// calculations are done with floating point values and then safely converted to
// integers (using ceiled and floored values where appropriate), the
// expectations are forgiving (use *_NEAR) within a single pixel.
TEST_F(WindowSelectorTest, TransformedRectMaintainsAspect) {
  std::unique_ptr<aura::Window> window(
      CreateWindow(gfx::Rect(10, 10, 100, 100)));
  ScopedTransformOverviewWindow transform_window(nullptr, window.get());

  gfx::Rect rect(50, 50, 200, 400);
  gfx::Rect bounds(100, 100, 50, 50);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  float scale = GetItemScale(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 400, 200);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 25, 25);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 25, 50);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);

  rect = gfx::Rect(50, 50, 50, 25);
  scale = GetItemScale(rect, bounds, 0, 0);
  transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_NEAR(scale * rect.width(), transformed_rect.width(), 1);
  EXPECT_NEAR(scale * rect.height(), transformed_rect.height(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered.
TEST_F(WindowSelectorTest, TransformedRectIsCentered) {
  std::unique_ptr<aura::Window> window(
      CreateWindow(gfx::Rect(10, 10, 100, 100)));
  ScopedTransformOverviewWindow transform_window(nullptr, window.get());
  gfx::Rect rect(50, 50, 200, 400);
  gfx::Rect bounds(100, 100, 50, 50);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, 0, 0);
  EXPECT_GE(transformed_rect.x(), bounds.x());
  EXPECT_LE(transformed_rect.right(), bounds.right());
  EXPECT_GE(transformed_rect.y(), bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(transformed_rect.y() - bounds.y(),
              bounds.bottom() - transformed_rect.bottom(), 1);
}

// Tests that transformed Rect fits in target bounds and is vertically centered
// when inset and header height are specified.
TEST_F(WindowSelectorTest, TransformedRectIsCenteredWithInset) {
  std::unique_ptr<aura::Window> window(
      CreateWindow(gfx::Rect(10, 10, 100, 100)));
  ScopedTransformOverviewWindow transform_window(nullptr, window.get());
  gfx::Rect rect(50, 50, 400, 200);
  gfx::Rect bounds(100, 100, 50, 50);
  const int inset = 20;
  const int header_height = 10;
  const float scale = GetItemScale(rect, bounds, inset, header_height);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(rect, bounds, inset,
                                                            header_height);
  // The |rect| width does not fit and therefore it gets centered outside
  // |bounds| starting before |bounds.x()| and ending after |bounds.right()|.
  EXPECT_LE(transformed_rect.x(), bounds.x());
  EXPECT_GE(transformed_rect.right(), bounds.right());
  EXPECT_GE(
      transformed_rect.y() + gfx::ToCeiledInt(scale * inset) - header_height,
      bounds.y());
  EXPECT_LE(transformed_rect.bottom(), bounds.bottom());
  EXPECT_NEAR(transformed_rect.x() - bounds.x(),
              bounds.right() - transformed_rect.right(), 1);
  EXPECT_NEAR(
      transformed_rect.y() + (int)(scale * inset) - header_height - bounds.y(),
      bounds.bottom() - transformed_rect.bottom(), 1);
}

// Verify that a window which will be displayed like a letter box on the window
// grid has the correct bounds.
TEST_F(WindowSelectorTest, TransformingLetteredRect) {
  // Create a window whose width is more than twice the height.
  const gfx::Rect original_bounds(10, 10, 300, 100);
  const int scale = 3;
  std::unique_ptr<aura::Window> window(CreateWindow(original_bounds));
  ScopedTransformOverviewWindow transform_window(nullptr, window.get());
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kLetterBoxed,
            transform_window.type());

  // Without any headers, the width should match the target, and the height
  // should be such that the aspect ratio of |original_bounds| is maintained.
  const gfx::Rect overview_bounds(0, 0, 100, 100);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(
          original_bounds, overview_bounds, 0, 0);
  EXPECT_EQ(overview_bounds.width(), transformed_rect.width());
  EXPECT_NEAR(overview_bounds.height() / scale, transformed_rect.height(), 1);

  // With headers, the width should still match the target. The height should
  // still be such that the aspect ratio is maintained, but the original header
  // which is hidden in overview needs to be accounted for.
  const int original_header = 10;
  const int overview_header = 20;
  transformed_rect = transform_window.ShrinkRectToFitPreservingAspectRatio(
      original_bounds, overview_bounds, original_header, overview_header);
  EXPECT_EQ(overview_bounds.width(), transformed_rect.width());
  EXPECT_NEAR((overview_bounds.height() - original_header) / scale,
              transformed_rect.height() - original_header / scale, 1);
  EXPECT_TRUE(overview_bounds.Contains(transformed_rect));

  // Verify that for an extreme window, the transform window stores the
  // original window selector bounds, minus the header.
  gfx::Rect selector_bounds = overview_bounds;
  selector_bounds.Inset(0, overview_header, 0, 0);
  ASSERT_TRUE(transform_window.window_selector_bounds().has_value());
  EXPECT_EQ(transform_window.window_selector_bounds().value(), selector_bounds);
}

// Verify that a window which will be displayed like a pillar box on the window
// grid has the correct bounds.
TEST_F(WindowSelectorTest, TransformingPillaredRect) {
  // Create a window whose height is more than twice the width.
  const gfx::Rect original_bounds(10, 10, 100, 300);
  const int scale = 3;
  std::unique_ptr<aura::Window> window(CreateWindow(original_bounds));
  ScopedTransformOverviewWindow transform_window(nullptr, window.get());
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kPillarBoxed,
            transform_window.type());

  // Without any headers, the height should match the target, and the width
  // should be such that the aspect ratio of |original_bounds| is maintained.
  const gfx::Rect overview_bounds(0, 0, 100, 100);
  gfx::Rect transformed_rect =
      transform_window.ShrinkRectToFitPreservingAspectRatio(
          original_bounds, overview_bounds, 0, 0);
  EXPECT_EQ(overview_bounds.height(), transformed_rect.height());
  EXPECT_NEAR(overview_bounds.width() / scale, transformed_rect.width(), 1);

  // With headers, the height should not include the area reserved for the
  // overview window title. It also needs to account for the original header
  // which will become hidden in overview mode.
  const int original_header = 10;
  const int overview_header = 20;
  transformed_rect = transform_window.ShrinkRectToFitPreservingAspectRatio(
      original_bounds, overview_bounds, original_header, overview_header);
  EXPECT_NEAR(overview_bounds.height() - overview_header,
              transformed_rect.height() - original_header / scale, 1);
  EXPECT_TRUE(overview_bounds.Contains(transformed_rect));

  // Verify that for an extreme window, the transform window stores the
  // original window selector bounds, minus the header.
  gfx::Rect selector_bounds = overview_bounds;
  selector_bounds.Inset(0, overview_header, 0, 0);
  ASSERT_TRUE(transform_window.window_selector_bounds().has_value());
  EXPECT_EQ(transform_window.window_selector_bounds().value(), selector_bounds);
}

// Start dragging a window and activate overview mode. This test should not
// crash or DCHECK inside aura::Window::StackChildRelativeTo().
TEST_F(WindowSelectorTest, OverviewWhileDragging) {
  const gfx::Rect bounds(10, 10, 100, 100);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  std::unique_ptr<WindowResizer> resizer(CreateWindowResizer(
      window.get(), gfx::Point(), HTCAPTION, ::wm::WINDOW_MOVE_SOURCE_MOUSE));
  ASSERT_TRUE(resizer.get());
  gfx::Point location = resizer->GetInitialLocation();
  location.Offset(20, 20);
  resizer->Drag(location, 0);
  ToggleOverview();
  resizer->RevertDrag();
}

// Verify that the overview no windows indicator appears when entering overview
// mode with no windows.
TEST_F(WindowSelectorTest, OverviewNoWindowsIndicator) {
  // Verify that by entering overview mode without windows, the no items
  // indicator appears.
  ToggleOverview();
  ASSERT_TRUE(window_selector());
  EXPECT_EQ(0u, GetWindowItemsForRoot(0).size());
  EXPECT_TRUE(window_selector()
                  ->grid_list_for_testing()[0]
                  ->IsNoItemsIndicatorLabelVisibleForTesting());
}

// Verify that the overview no windows indicator position is as expected.
TEST_F(WindowSelectorTest, OverviewNoWindowsIndicatorPosition) {
  UpdateDisplay("400x300");
  // Midpoint of height minus shelf.
  const int expected_y = (300 - ShelfConstants::shelf_size()) / 2;

  // Helper to check points. Uses EXPECT_NEAR on each coordinate to account for
  // rounding.
  auto check_point = [](const gfx::Point& expected, const gfx::Point& actual) {
    EXPECT_NEAR(expected.x(), actual.x(), 1);
    EXPECT_NEAR(expected.y(), actual.y(), 1);
  };

  ToggleOverview();
  ASSERT_TRUE(window_selector());

  // Verify that originally the label is in the center of the workspace.
  WindowGrid* grid = window_selector()->grid_list_for_testing()[0].get();
  check_point(gfx::Point(200, expected_y),
              grid->GetNoItemsIndicatorLabelBoundsForTesting().CenterPoint());

  // Verify that when grid bounds are on the left, the label is centered on the
  // left side of the workspace.
  grid->SetBoundsAndUpdatePositions(
      gfx::Rect(0, 0, 200, 300 - ShelfConstants::shelf_size()));
  check_point(gfx::Point(100, expected_y),
              grid->GetNoItemsIndicatorLabelBoundsForTesting().CenterPoint());

  // Verify that when grid bounds are on the right, the label is centered on the
  // right side of the workspace.
  grid->SetBoundsAndUpdatePositions(
      gfx::Rect(200, 0, 200, 300 - ShelfConstants::shelf_size()));
  check_point(gfx::Point(300, expected_y),
              grid->GetNoItemsIndicatorLabelBoundsForTesting().CenterPoint());

  // Verify that after rotating the display, the label is centered in the
  // workspace 300x(400-shelf).
  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  check_point(gfx::Point(150, (400 - ShelfConstants::shelf_size()) / 2),
              grid->GetNoItemsIndicatorLabelBoundsForTesting().CenterPoint());
}

// Verify that when opening overview mode with multiple displays, the no items
// indicator on the primary grid if there are no windows. Also verify that
// we do not exit overview mode until all the grids are empty.
TEST_F(WindowSelectorTest, OverviewNoWindowsIndicatorMultiDisplay) {
  // Helper function to help reduce lines of code. Returns the list of grids
  // in overview mode.
  auto grids = [this]() -> const std::vector<std::unique_ptr<WindowGrid>>& {
    EXPECT_TRUE(window_selector());
    return window_selector()->grid_list_for_testing();
  };

  UpdateDisplay("400x400,400x400,400x400");

  // Enter overview mode. Verify that the no windows indicator is visible on the
  // primary display but not on the other two.
  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(window_selector());
  EXPECT_TRUE(grids()[0]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[1]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[2]->IsNoItemsIndicatorLabelVisibleForTesting());
  ToggleOverview();

  // Create two windows with widgets (widgets are needed to close the windows
  // later in the test), one each on the first two monitors.
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  const gfx::Rect bounds(100, 100);
  std::unique_ptr<views::Widget> widget1(CreateWindowWidget(bounds));
  std::unique_ptr<views::Widget> widget2(CreateWindowWidget(bounds));
  aura::Window* window1 = widget1->GetNativeWindow();
  aura::Window* window2 = widget2->GetNativeWindow();
  ASSERT_TRUE(wm::MoveWindowToDisplay(window2, GetSecondaryDisplay().id()));
  ASSERT_EQ(root_windows[0], window1->GetRootWindow());
  ASSERT_EQ(root_windows[1], window2->GetRootWindow());

  // Enter overview mode. Verify that the no windows indicator is not visible on
  // any display.
  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(window_selector());
  EXPECT_EQ(3u, grids().size());
  EXPECT_FALSE(grids()[0]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[1]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[2]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[0]->empty());
  EXPECT_FALSE(grids()[1]->empty());
  EXPECT_TRUE(grids()[2]->empty());

  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1);
  WindowSelectorItem* item2 = GetWindowItemForWindow(1, window2);
  ASSERT_TRUE(item1 && item2);

  // Close |item2|. Verify that we are still in overview mode because |window1|
  // is still open. The non primary root grids are empty however.
  item2->CloseWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(window_selector());
  EXPECT_EQ(3u, grids().size());
  EXPECT_FALSE(grids()[0]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[1]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[2]->IsNoItemsIndicatorLabelVisibleForTesting());
  EXPECT_FALSE(grids()[0]->empty());
  EXPECT_TRUE(grids()[1]->empty());
  EXPECT_TRUE(grids()[2]->empty());

  // Close |item1|. Verify that since no windows are open, we exit overview
  // mode.
  item1->CloseWindow();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_selector());
}

// Verify that pressing and releasing keys does not show the overview textbox
// when there are no windows opened.
TEST_F(WindowSelectorTest, TextfilterHiddenWhenNoWindows) {
  ToggleOverview();
  ASSERT_TRUE(window_selector());

  SendKey(ui::VKEY_J);
  EXPECT_FALSE(showing_filter_widget());
}

// Tests the cases when very wide or tall windows enter overview mode.
TEST_F(WindowSelectorTest, ExtremeWindowBounds) {
  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Window |wide|, with size (400, 160) will be resized to (200, 160)
  // when the 400x200 is rotated to 200x400, and should be considered a normal
  // overview window after display change.
  UpdateDisplay("400x200");
  std::unique_ptr<aura::Window> wide(CreateWindow(gfx::Rect(10, 10, 400, 160)));
  std::unique_ptr<aura::Window> tall(CreateWindow(gfx::Rect(10, 10, 50, 200)));
  std::unique_ptr<aura::Window> normal(
      CreateWindow(gfx::Rect(10, 10, 200, 200)));

  ToggleOverview();
  WindowSelectorItem* wide_item = GetWindowItemForWindow(0, wide.get());
  WindowSelectorItem* tall_item = GetWindowItemForWindow(0, tall.get());
  WindowSelectorItem* normal_item = GetWindowItemForWindow(0, normal.get());

  // Verify the window dimension type is as expected after entering overview
  // mode.
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kLetterBoxed,
            wide_item->GetWindowDimensionsType());
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kPillarBoxed,
            tall_item->GetWindowDimensionsType());
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kNormal,
            normal_item->GetWindowDimensionsType());

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);
  // Verify that |wide| has its window dimension type updated after the display
  // change.
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kNormal,
            wide_item->GetWindowDimensionsType());
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kPillarBoxed,
            tall_item->GetWindowDimensionsType());
  EXPECT_EQ(ScopedTransformOverviewWindow::GridWindowFillMode::kNormal,
            normal_item->GetWindowDimensionsType());
}

// Tests window list animation states are correctly updated.
TEST_F(WindowSelectorTest, SetWindowListAnimationStates) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window3.get())->IsFullscreen());

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window3.get())->IsFullscreen());

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
TEST_F(WindowSelectorTest, SetWindowListAnimationStatesWithSelectedWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window3.get())->IsFullscreen());

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window3.get())->IsFullscreen());

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
TEST_F(WindowSelectorTest,
       OverviewWindowAnimationObserverCanHandleDeletedWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window3.get())->IsFullscreen());

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window3.get())->IsFullscreen());

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
TEST_F(WindowSelectorTest, HandleOverviewWindowAnimationObserverWasDeleted) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window3.get())->IsFullscreen());

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window3.get())->IsFullscreen());

  // Enter overview.
  ToggleOverview();

  // Click on |window2| to activate it and exit overview.
  // Should only set |should_animate_when_exiting_| and
  // |should_be_observed_when_exiting_| on window 2.
  // Because the animation duration is zero in test, the
  // OverviewWindowAnimationObserver will delete itself immediatelly before
  // |window3|'s is added to it.
  ClickWindow(window2.get());
  EXPECT_FALSE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window3->layer()->GetAnimator()->is_animating());
}

// Tests can handle |gained_active| window is not in the |window_grid| when
// OnWindowActivated.
TEST_F(WindowSelectorTest, HandleActiveWindowNotInWindowGrid) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window3.get())->IsFullscreen());

  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window3.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_FALSE(wm::GetWindowState(window1.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window3.get())->IsFullscreen());

  // Enter overview.
  ToggleOverview();

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // Create and active a new window should exit overview without error.
  auto widget =
      CreateTestWidget(nullptr, kShellWindowId_StatusContainer, bounds);

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
TEST_F(WindowSelectorTest, DISABLED_HandleAlwaysOnTopWindow) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window5(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window6(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window7(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window8(CreateWindow(bounds));
  window3->SetProperty(aura::client::kAlwaysOnTopKey, true);
  window5->SetProperty(aura::client::kAlwaysOnTopKey, true);

  // Control z order and MRU order.
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Will be maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());

  EXPECT_FALSE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window6.get())->IsFullscreen());
  EXPECT_FALSE(wm::GetWindowState(window7.get())->IsMaximized());

  const wm::WMEvent toggle_maximize_event(wm::WM_EVENT_TOGGLE_MAXIMIZE);
  wm::GetWindowState(window6.get())->OnWMEvent(&toggle_maximize_event);
  const wm::WMEvent toggle_fullscreen_event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_TRUE(wm::GetWindowState(window2.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window7.get())->IsFullscreen());
  EXPECT_TRUE(wm::GetWindowState(window6.get())->IsMaximized());

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
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window7.get())->OnWMEvent(&toggle_fullscreen_event);
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
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window7.get())->OnWMEvent(&toggle_fullscreen_event);
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
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window7.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::ActivateWindow(window8.get());
  wm::ActivateWindow(window7.get());  // Will be fullscreen.
  wm::ActivateWindow(window6.get());  // Maximized.
  wm::ActivateWindow(window5.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window4.get());
  wm::ActivateWindow(window3.get());  // AlwaysOnTop window.
  wm::ActivateWindow(window2.get());  // Will be fullscreen.
  wm::ActivateWindow(window1.get());
  wm::GetWindowState(window2.get())->OnWMEvent(&toggle_fullscreen_event);
  wm::GetWindowState(window7.get())->OnWMEvent(&toggle_fullscreen_event);
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
TEST_F(WindowSelectorTest, WindowItemCanAnimateOnDragRelease) {
  UpdateDisplay("400x400");
  const gfx::Rect bounds(10, 10, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  // The item dragging is only allowed in tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  ToggleOverview();
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());
  // Drag |item2| in a way so that |window2| does not get activated.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(item2->target_bounds().CenterPoint());
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();

  generator->MoveMouseTo(gfx::Point(200, 200));
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  generator->ReleaseLeftButton();
  EXPECT_TRUE(window2->layer()->GetAnimator()->IsAnimatingProperty(
      ui::LayerAnimationElement::AnimatableProperty::TRANSFORM));
  base::RunLoop().RunUntilIdle();
}

// Verify that the window selector items titlebar and close button change
// visibility when a item is being dragged.
TEST_F(WindowSelectorTest, WindowItemTitleCloseVisibilityOnDrag) {
  UpdateDisplay("400x400");
  const gfx::Rect bounds(10, 10, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  // Dragging is only allowed in tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  ToggleOverview();
  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());
  // Start the drag on |item1|. Verify the dragged item, |item1| has both the
  // close button and titlebar hidden. The close button opacity however is
  // opaque as its a child of the header which handles fading away the whole
  // header. All other items, |item2| should only have the close button hidden.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(item1->target_bounds().CenterPoint());
  generator->PressLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0.f, item1->GetTitlebarOpacityForTesting());
  EXPECT_EQ(1.f, item1->GetCloseButtonVisibilityForTesting());
  EXPECT_EQ(1.f, item2->GetTitlebarOpacityForTesting());
  EXPECT_EQ(0.f, item2->GetCloseButtonVisibilityForTesting());

  // Drag |item1| in a way so that |window1| does not get activated (drags
  // within a certain threshold count as clicks). Verify the close button and
  // titlebar is visible for all items.
  generator->MoveMouseTo(gfx::Point(200, 200));
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(1.f, item1->GetTitlebarOpacityForTesting());
  EXPECT_EQ(1.f, item1->GetCloseButtonVisibilityForTesting());
  EXPECT_EQ(1.f, item2->GetTitlebarOpacityForTesting());
  EXPECT_EQ(1.f, item2->GetCloseButtonVisibilityForTesting());
}

// Tests that overview widgets are stacked in the correct order.
TEST_F(WindowSelectorTest, OverviewWidgetStackingOrder) {
  // Create three windows, including one minimized.
  const gfx::Rect bounds(10, 10, 200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  std::unique_ptr<aura::Window> minimized(CreateWindow(bounds));
  wm::GetWindowState(minimized.get())->Minimize();
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  aura::Window* parent = window->parent();
  DCHECK_EQ(parent, minimized->parent());

  // Dragging is only allowed in tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  ToggleOverview();
  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, minimized.get());
  WindowSelectorItem* item3 = GetWindowItemForWindow(0, window3.get());

  views::Widget* widget1 = item_widget(item1);
  views::Widget* widget2 = item_widget(item2);
  views::Widget* widget3 = item_widget(item3);
  views::Widget* min_widget1 = minimized_widget(item1);
  views::Widget* min_widget2 = minimized_widget(item2);
  views::Widget* min_widget3 = minimized_widget(item3);

  // The original order of stacking is determined by the order the associated
  // window was activated (created in this case). All widgets associated with
  // minimized windows will be below non minimized windows, because a widget for
  // the minimized windows is created upon entering overview, and they are
  // explicitly stacked beneath non minimized windows so they do not cover them
  // during enter animation.
  EXPECT_GT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(widget1->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));

  // Verify that only minimized windows have minimized widgets in overview.
  EXPECT_FALSE(min_widget1);
  ASSERT_TRUE(min_widget2);
  EXPECT_FALSE(min_widget3);

  // Verify both item widgets and minimized widgets are parented to the parent
  // of the original windows.
  EXPECT_EQ(parent, widget1->GetNativeWindow()->parent());
  EXPECT_EQ(parent, widget2->GetNativeWindow()->parent());
  EXPECT_EQ(parent, widget3->GetNativeWindow()->parent());
  EXPECT_EQ(parent, min_widget2->GetNativeWindow()->parent());

  // Verify that the item widget is stacked above the window if not minimized.
  // Verify that the item widget is stacked above the minimized widget if
  // minimized.
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(window.get(), parent));
  EXPECT_GT(IndexOf(widget2->GetNativeWindow(), parent),
            IndexOf(min_widget2->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(window3.get(), parent));

  // Drag the first window. Verify that it's item widget is not stacked above
  // the other two.
  const gfx::Point start_drag = item1->target_bounds().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_drag);
  generator->PressLeftButton();
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget3->GetNativeWindow(), parent));

  // Drag to origin and then back to the start to avoid activating the window or
  // entering splitview.
  generator->MoveMouseTo(gfx::Point());
  generator->MoveMouseTo(start_drag);
  generator->ReleaseLeftButton();

  // Verify the stacking order is same as before dragging started.
  EXPECT_GT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(widget1->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));
}

// Tests that overview widgets are stacked in the correct order.
TEST_F(WindowSelectorTest, OverviewWidgetStackingOrderWithDragging) {
  const gfx::Rect bounds(10, 10, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  // Dragging is only allowed in tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  ToggleOverview();
  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());
  WindowSelectorItem* item3 = GetWindowItemForWindow(0, window3.get());
  views::Widget* widget1 = item_widget(item1);
  views::Widget* widget2 = item_widget(item2);
  views::Widget* widget3 = item_widget(item3);

  // Initially the highest stacked widget is the most recently used window, in
  // this case it is the most recently created window.
  aura::Window* parent = window1->parent();
  EXPECT_GT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget2->GetNativeWindow(), parent),
            IndexOf(widget1->GetNativeWindow(), parent));

  // Verify that during drag the dragged item widget is stacked above the other
  // two.
  const gfx::Point start_drag = item1->target_bounds().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_drag);
  generator->PressLeftButton();
  generator->MoveMouseTo(gfx::Point());
  generator->MoveMouseTo(start_drag);
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget3->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget1->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));

  // Verify that after release the ordering is the same as before dragging.
  generator->ReleaseLeftButton();
  base::RunLoop().RunUntilIdle();
  EXPECT_GT(IndexOf(widget3->GetNativeWindow(), parent),
            IndexOf(widget2->GetNativeWindow(), parent));
  EXPECT_GT(IndexOf(widget2->GetNativeWindow(), parent),
            IndexOf(widget1->GetNativeWindow(), parent));
}

// Verify that a windows which enter overview mode have a visible backdrop, if
// the window is to be letter or pillar fitted.
TEST_F(WindowSelectorTest, Backdrop) {
  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Window |wide|, with size (400, 160) will be resized to (200, 160)
  // when the 400x200 is rotated to 200x400, and should be considered a normal
  // overview window after display change.
  UpdateDisplay("400x200");
  std::unique_ptr<aura::Window> wide(CreateWindow(gfx::Rect(10, 10, 400, 160)));
  std::unique_ptr<aura::Window> tall(CreateWindow(gfx::Rect(10, 10, 50, 200)));
  std::unique_ptr<aura::Window> normal(
      CreateWindow(gfx::Rect(10, 10, 200, 200)));

  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  WindowSelectorItem* wide_item = GetWindowItemForWindow(0, wide.get());
  WindowSelectorItem* tall_item = GetWindowItemForWindow(0, tall.get());
  WindowSelectorItem* normal_item = GetWindowItemForWindow(0, normal.get());

  // Only very tall and very wide windows will have a backdrop.
  EXPECT_TRUE(backdrop_widget(wide_item));
  EXPECT_TRUE(backdrop_widget(tall_item));
  EXPECT_FALSE(backdrop_widget(normal_item));

  display::Screen* screen = display::Screen::GetScreen();
  const display::Display& display = screen->GetPrimaryDisplay();
  display_manager()->SetDisplayRotation(
      display.id(), display::Display::ROTATE_90,
      display::Display::RotationSource::ACTIVE);

  // After rotation the former wide window will be a normal window and lose its
  // backdrop.
  EXPECT_FALSE(backdrop_widget(wide_item));
  EXPECT_TRUE(backdrop_widget(tall_item));
  EXPECT_FALSE(backdrop_widget(normal_item));

  // Test that leaving overview mode cleans up properly.
  ToggleOverview();
}

// Verify that the mask that is applied to add rounded corners in overview mode
// is removed during animations and drags.
TEST_F(WindowSelectorTest, RoundedEdgeMaskVisibility) {
  UpdateDisplay("400x400");
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  wm::ActivateWindow(window2.get());
  wm::ActivateWindow(window1.get());

  // Dragging is only allowed in tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());
  EXPECT_TRUE(HasMaskForItem(item1));
  EXPECT_TRUE(HasMaskForItem(item2));

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  // Drag the first window. Verify that the mask still exists for both items as
  // we do not apply any animation to the window items at this point.
  const gfx::Point start_drag = item1->target_bounds().CenterPoint();
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start_drag);
  generator->PressLeftButton();
  EXPECT_FALSE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(window2->layer()->GetAnimator()->is_animating());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasMaskForItem(item1));
  EXPECT_TRUE(HasMaskForItem(item2));

  // Drag to horizontally and then back to the start to avoid activating the
  // window, drag to close or entering splitview. Verify that the mask is
  // invisible on both items during animation.
  generator->MoveMouseTo(gfx::Point(0, start_drag.y()));
  generator->MoveMouseTo(start_drag);
  generator->ReleaseLeftButton();
  EXPECT_TRUE(window1->layer()->GetAnimator()->is_animating());
  EXPECT_TRUE(window2->layer()->GetAnimator()->is_animating());
  EXPECT_FALSE(HasMaskForItem(item1));
  EXPECT_FALSE(HasMaskForItem(item2));

  // Verify that the mask is visble again after animation is finished.
  window1->layer()->GetAnimator()->StopAnimating();
  window2->layer()->GetAnimator()->StopAnimating();
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(HasMaskForItem(item1));
  EXPECT_TRUE(HasMaskForItem(item2));

  // Test that leaving overview mode cleans up properly.
  ToggleOverview();
}

// Verify that if the window's bounds are changed while it's in overview mode,
// the rounded edge mask's bounds are also changed accordingly.
TEST_F(WindowSelectorTest, WindowBoundsChangeTest) {
  UpdateDisplay("400x400");
  const gfx::Rect bounds(0, 0, 200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  ToggleOverview();
  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  EXPECT_TRUE(HasMaskForItem(item1));
  EXPECT_EQ(GetMaskBoundsForItem(item1), window1->bounds());
  EXPECT_EQ(GetMaskBoundsForItem(item1), bounds);

  wm::GetWindowState(window1.get())->Maximize();
  EXPECT_EQ(GetMaskBoundsForItem(item1), window1->bounds());
  EXPECT_NE(GetMaskBoundsForItem(item1), bounds);
}

// Verify that the system does not crash when exiting overview mode after
// pressing CTRL+SHIFT+U.
TEST_F(WindowSelectorTest, ExitInUnderlineMode) {
  std::unique_ptr<aura::Window> window(
      CreateWindow(gfx::Rect(10, 10, 200, 200)));

  ToggleOverview();

  // Enter underline mode on the text selector by generating CTRL+SHIFT+U
  // sequence.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->PressKey(ui::VKEY_U, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);
  generator->ReleaseKey(ui::VKEY_U, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  // Test that leaving overview mode cleans up properly.
  ToggleOverview();
}

// Tests that the shadows in overview mode are placed correctly.
TEST_F(WindowSelectorTest, ShadowBounds) {
  // Helper function to check if the bounds of a shadow owned by |shadow_parent|
  // is contained within the bounds of |widget|.
  auto contains = [](views::Widget* widget, WindowSelectorItem* shadow_parent) {
    return gfx::Rect(widget->GetNativeWindow()->bounds().size())
        .Contains(shadow_parent->GetShadowBoundsForTesting());
  };

  // Helper function which returns the ratio of the shadow owned by
  // |shadow_parent| width and height.
  auto shadow_ratio = [](WindowSelectorItem* shadow_parent) {
    gfx::RectF boundsf = gfx::RectF(shadow_parent->GetShadowBoundsForTesting());
    return boundsf.width() / boundsf.height();
  };

  // Add three windows which in overview mode will be considered wide, tall and
  // normal. Set top view insets to 0, so it is easy to check the ratios of
  // the shadows match the ratios of the untransformed windows.
  UpdateDisplay("400x400");
  std::unique_ptr<aura::Window> wide(CreateWindow(gfx::Rect(10, 10, 400, 100)));
  std::unique_ptr<aura::Window> tall(CreateWindow(gfx::Rect(10, 10, 100, 400)));
  std::unique_ptr<aura::Window> normal(
      CreateWindow(gfx::Rect(10, 10, 200, 200)));
  wide->SetProperty(aura::client::kTopViewInset, 0);
  tall->SetProperty(aura::client::kTopViewInset, 0);
  normal->SetProperty(aura::client::kTopViewInset, 0);

  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  WindowSelectorItem* wide_item = GetWindowItemForWindow(0, wide.get());
  WindowSelectorItem* tall_item = GetWindowItemForWindow(0, tall.get());
  WindowSelectorItem* normal_item = GetWindowItemForWindow(0, normal.get());

  views::Widget* wide_widget = item_widget(wide_item);
  views::Widget* tall_widget = item_widget(tall_item);
  views::Widget* normal_widget = item_widget(normal_item);

  WindowGrid* grid = window_selector()->grid_list_for_testing()[0].get();

  // Verify all the shadows are within the bounds of their respective item
  // widgets when the overview windows are positioned without animations.
  SetGridBounds(grid, gfx::Rect(200, 400));
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
  SetGridBounds(grid, gfx::Rect(200, 400));
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
// Disabled due to flakiness: crbug.com/834708
TEST_F(WindowSelectorTest, DISABLED_DraggingWithTwoFingers) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  // Dragging is only allowed in tablet mode.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  base::RunLoop().RunUntilIdle();

  ToggleOverview();
  base::RunLoop().RunUntilIdle();
  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());

  const gfx::Rect original_bounds1 = item1->target_bounds();
  const gfx::Rect original_bounds2 = item2->target_bounds();

  constexpr int kTouchId1 = 1;
  constexpr int kTouchId2 = 2;

  // Dispatches a long press event at the event generators current location.
  // Long press is one way to start dragging in splitview.
  auto dispatch_long_press = [this]() {
    ui::GestureEventDetails event_details(ui::ET_GESTURE_LONG_PRESS);
    const gfx::Point location = GetEventGenerator()->current_location();
    ui::GestureEvent long_press(location.x(), location.y(), 0,
                                ui::EventTimeForNow(), event_details);
    GetEventGenerator()->Dispatch(&long_press);
  };

  // Verify that the bounds of the tapped window expand when touched.
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(original_bounds1.CenterPoint());
  generator->PressTouchId(kTouchId1);
  dispatch_long_press();
  EXPECT_GT(item1->target_bounds().width(), original_bounds1.width());
  EXPECT_GT(item1->target_bounds().height(), original_bounds1.height());

  // Verify that attempting to touch the second window with a second finger does
  // nothing to the second window. The first window remains the window to be
  // dragged.
  generator->set_current_location(original_bounds2.CenterPoint());
  generator->PressTouchId(kTouchId2);
  dispatch_long_press();
  EXPECT_GT(item1->target_bounds().width(), original_bounds1.width());
  EXPECT_GT(item1->target_bounds().height(), original_bounds1.height());
  EXPECT_EQ(item2->target_bounds(), original_bounds2);

  // Verify the first window moves on drag.
  gfx::Point last_center_point = item1->target_bounds().CenterPoint();
  generator->MoveTouchIdBy(kTouchId1, 40, 40);
  EXPECT_NE(last_center_point, item1->target_bounds().CenterPoint());
  EXPECT_EQ(original_bounds2.CenterPoint(),
            item2->target_bounds().CenterPoint());

  // Verify the first window moves on drag, even if we switch to a second
  // finger.
  last_center_point = item1->target_bounds().CenterPoint();
  generator->ReleaseTouchId(kTouchId2);
  generator->PressTouchId(kTouchId2);
  dispatch_long_press();
  generator->MoveTouchIdBy(kTouchId2, 40, 40);
  EXPECT_NE(last_center_point, item1->target_bounds().CenterPoint());
  EXPECT_EQ(original_bounds2.CenterPoint(),
            item2->target_bounds().CenterPoint());
}

// Verify that shadows on windows disappear for the duration of overview mode.
TEST_F(WindowSelectorTest, ShadowDisappearsInOverview) {
  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

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
TEST_F(WindowSelectorTest, PipWindowShownButExcludedFromOverview) {
  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> pip_window(CreateWindow(bounds));

  wm::GetWindowState(pip_window.get())
      ->SetStateObject(std::unique_ptr<wm::WindowState::State>(
          new InitialStateTestState(mojom::WindowStateType::PIP)));

  // Enter overview.
  ToggleOverview();

  // PIP window should be visible but not in the overview.
  EXPECT_TRUE(pip_window->IsVisible());
  EXPECT_FALSE(SelectWindow(pip_window.get()));
}

// Tests the PositionWindows function works as expected.
TEST_F(WindowSelectorTest, PositionWindows) {
  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());
  WindowSelectorItem* item3 = GetWindowItemForWindow(0, window3.get());
  const gfx::Rect bounds1 = item1->target_bounds();
  const gfx::Rect bounds2 = item2->target_bounds();
  const gfx::Rect bounds3 = item3->target_bounds();

  // Verify that the bounds remain the same when calling PositionWindows again.
  window_selector()->PositionWindows(/*animate=*/false);
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_EQ(bounds2, item2->target_bounds());
  EXPECT_EQ(bounds3, item3->target_bounds());

  // Verify that |item2| and |item3| change bounds when calling PositionWindows
  // while ignoring |item1|.
  window_selector()->PositionWindows(/*animate=*/false, item1);
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_NE(bounds2, item2->target_bounds());
  EXPECT_NE(bounds3, item3->target_bounds());

  // Return the windows to their original bounds.
  window_selector()->PositionWindows(/*animate=*/false);

  // Verify that items that are animating before closing are ignored by
  // PositionWindows.
  item1->set_animating_to_close(true);
  item2->set_animating_to_close(true);
  window_selector()->PositionWindows(/*animate=*/false);
  EXPECT_EQ(bounds1, item1->target_bounds());
  EXPECT_EQ(bounds2, item2->target_bounds());
  EXPECT_NE(bounds3, item3->target_bounds());
}

namespace {

// Test class that allows us to check what whether the last overview enter or
// exit was using a slide animation. This is needed because the cached slide
// animation variable may be reset or the WindowSelector object may not be
// available after a toggle has completed. Also stores whether the animation
// complete observers fired because an animation completed or was canceled.
class TestOverviewObserver : public ShellObserver {
 public:
  TestOverviewObserver() { Shell::Get()->AddShellObserver(this); }
  ~TestOverviewObserver() override { Shell::Get()->RemoveShellObserver(this); }

  // ShellObserver:
  void OnOverviewModeStarting() override { UpdateLastAnimationWasSlide(); }
  void OnOverviewModeEnding() override { UpdateLastAnimationWasSlide(); }
  void OnOverviewModeStartingAnimationComplete(bool canceled) override {
    animation_canceled_ = canceled;
  }
  void OnOverviewModeEndingAnimationComplete(bool canceled) override {
    animation_canceled_ = canceled;
  }

  bool last_animation_was_slide() const { return last_animation_was_slide_; }
  bool animation_canceled() const { return animation_canceled_; }

 private:
  void UpdateLastAnimationWasSlide() {
    WindowSelector* selector =
        Shell::Get()->window_selector_controller()->window_selector();
    DCHECK(selector);
    last_animation_was_slide_ =
        selector->enter_exit_overview_type() ==
        WindowSelector::EnterExitOverviewType::kWindowsMinimized;
  }

  bool last_animation_was_slide_ = false;
  bool animation_canceled_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestOverviewObserver);
};

}  // namespace

// Tests the slide animation for overview is never used in clamshell.
TEST_F(WindowSelectorTest, OverviewEnterExitAnimation) {
  TestOverviewObserver observer;

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());

  ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());

  // Even with all window minimized, there should not be a slide animation.
  ASSERT_FALSE(IsSelecting());
  wm::GetWindowState(window.get())->Minimize();
  ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());
}

// Tests the slide animation for overview is used in tablet if all windows
// are minimized, and that if overview is exited from the home launcher all
// windows are minimized.
TEST_F(WindowSelectorTest, OverviewEnterExitAnimationTablet) {
  TestOverviewObserver observer;

  // Ensure calls to EnableTabletModeWindowManager complete.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  base::RunLoop().RunUntilIdle();

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_FALSE(observer.last_animation_was_slide());

  // Exit to home launcher. Slide animation should be used, and all windows
  // should be minimized.
  ToggleOverview(WindowSelector::EnterExitOverviewType::kWindowsMinimized);
  EXPECT_TRUE(observer.last_animation_was_slide());
  ASSERT_FALSE(IsSelecting());
  EXPECT_TRUE(wm::GetWindowState(window.get())->IsMinimized());

  // All windows are minimized, so we should use the slide animation.
  ToggleOverview();
  EXPECT_TRUE(observer.last_animation_was_slide());
}

// Tests that the overview enter animation observer works as expected.
TEST_F(WindowSelectorTest, OverviewEnterAnimationObserver) {
  TestOverviewObserver observer;

  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  // Test that if the animations are allowed to run out on enter the observer
  // will be notified of complete animations.
  ToggleOverview();
  WindowSelectorItem* item = GetWindowItemForWindow(0, window.get());
  window->layer()->GetAnimator()->StopAnimating();
  item_widget(item)->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();
  EXPECT_FALSE(observer.animation_canceled());

  ToggleOverview();

  // Test that if the animations are canceled after entering by exiting overview
  // right away, the observer will be notified of incomplete animations.
  ToggleOverview();
  ToggleOverview();
  EXPECT_TRUE(observer.animation_canceled());
}

// Tests that the overview exit animation observer works as expected.
TEST_F(WindowSelectorTest, OverviewExitAnimationObserver) {
  TestOverviewObserver observer;

  ui::ScopedAnimationDurationScaleMode animation_scale_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  ToggleOverview();

  // Test that if the animations are allowed to run out on exit the observer
  // will be notified of complete animations.
  ToggleOverview();
  window->layer()->GetAnimator()->StopAnimating();
  std::vector<std::unique_ptr<DelayedAnimationObserver>>& delayed_animations =
      window_selector_controller()->delayed_animations_;
  // On animation complete |delayed_animations| will erase its own members, so
  // use a while loop to avoid indexing errors.
  ASSERT_FALSE(delayed_animations.empty());
  while (!delayed_animations.empty()) {
    views::Widget* item_widget =
        static_cast<CleanupAnimationObserver*>(delayed_animations.back().get())
            ->widget_.get();
    item_widget->GetNativeWindow()->layer()->GetAnimator()->StopAnimating();
  }
  EXPECT_FALSE(observer.animation_canceled());

  // Test that if the animations are canceled after exiting by reentering
  // overview right away, the observer will be notified of incomplete
  // animations.
  ToggleOverview();
  ToggleOverview();
  EXPECT_TRUE(observer.animation_canceled());
}

// Tests that overview mode is entered with kWindowDragged mode when an app is
// dragged from the top of the screen.
TEST_F(WindowSelectorTest, DraggingFromTopAnimation) {
  // Ensure calls to EnableTabletModeWindowManager complete.
  base::RunLoop().RunUntilIdle();
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  base::RunLoop().RunUntilIdle();

  const gfx::Rect bounds(200, 200);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));
  std::unique_ptr<views::Widget> widget(CreateWindowWidget(bounds));

  // Drag from the the top of the app to enter overview.
  auto drag_controller = std::make_unique<TabletModeAppWindowDragController>();
  ui::GestureEvent event(0, 0, 0, base::TimeTicks(),
                         ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN));
  ui::Event::DispatcherApi dispatch_helper(&event);
  dispatch_helper.set_target(widget->GetNativeWindow());
  drag_controller->DragWindowFromTop(&event);

  ASSERT_TRUE(IsSelecting());
  EXPECT_EQ(WindowSelector::EnterExitOverviewType::kWindowDragged,
            window_selector()->enter_exit_overview_type());
}

class SplitViewWindowSelectorTest : public WindowSelectorTest {
 public:
  SplitViewWindowSelectorTest() = default;
  ~SplitViewWindowSelectorTest() override = default;

  enum class SelectorItemLocation {
    CENTER,
    ORIGIN,
    TOP_RIGHT,
    BOTTOM_RIGHT,
    BOTTOM_LEFT,
  };

  void SetUp() override {
    WindowSelectorTest::SetUp();
    // Ensure calls to EnableTabletModeWindowManager complete.
    base::RunLoop().RunUntilIdle();
    Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);
    base::RunLoop().RunUntilIdle();
  }

  SplitViewController* split_view_controller() {
    return Shell::Get()->split_view_controller();
  }

  void EndSplitView() { split_view_controller()->EndSplitView(); }

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

  gfx::Rect GetSplitViewLeftWindowBounds(aura::Window* window) {
    return split_view_controller()->GetSnappedWindowBoundsInScreen(
        window, SplitViewController::LEFT);
  }

  gfx::Rect GetSplitViewRightWindowBounds(aura::Window* window) {
    return split_view_controller()->GetSnappedWindowBoundsInScreen(
        window, SplitViewController::RIGHT);
  }

  gfx::Rect GetSplitViewDividerBounds(bool is_dragging) {
    if (!split_view_controller()->IsSplitViewModeActive())
      return gfx::Rect();
    return split_view_controller()
        ->split_view_divider_->GetDividerBoundsInScreen(is_dragging);
  }

  // Drags a window selector item |item| from its center or one of its corners
  // to |end_location|. This should be used over
  // DragWindowTo(WindowSelectorItem*, gfx::Point) when testing snapping a
  // window, but the windows centerpoint may be inside a snap region, thus the
  // window will not snapped. This function is mostly used to test splitview so
  // |long_press| is default to true. Set |long_press| to false if we do not
  // want to long press after every press, which enables dragging vertically to
  // close an item.
  void DragWindowTo(WindowSelectorItem* item,
                    const gfx::Point& end_location,
                    SelectorItemLocation location,
                    bool long_press = true) {
    // Start drag in the middle of the seletor item.
    gfx::Point start_location;
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
    window_selector()->InitiateDrag(item, start_location);
    if (long_press)
      window_selector()->StartSplitViewDragMode(start_location);
    window_selector()->Drag(item, end_location);
    window_selector()->CompleteDrag(item, end_location);
  }

  // Drags a window selector item |item| from its center point to
  // |end_location|.
  void DragWindowTo(WindowSelectorItem* item, const gfx::Point& end_location) {
    DragWindowTo(item, end_location, SelectorItemLocation::CENTER, true);
  }

  // Creates a window which cannot be snapped by splitview.
  std::unique_ptr<aura::Window> CreateUnsnappableWindow(
      const gfx::Rect& bounds = gfx::Rect()) {
    std::unique_ptr<aura::Window> window;
    if (bounds.IsEmpty())
      window = CreateTestWindow();
    else
      window = base::WrapUnique<aura::Window>(CreateWindow(bounds));

    window->SetProperty(aura::client::kResizeBehaviorKey,
                        ws::mojom::kResizeBehaviorNone);
    return window;
  }

  IndicatorState indicator_state() {
    DCHECK(window_selector());
    return window_selector()
        ->split_view_drag_indicators()
        ->current_indicator_state();
  }

  int GetEdgeInset(int screen_width) const {
    return screen_width * kHighlightScreenPrimaryAxisRatio +
           kHighlightScreenEdgePaddingDp;
  }

  bool IsPreviewAreaShowing() {
    return indicator_state() == IndicatorState::kPreviewAreaLeft ||
           indicator_state() == IndicatorState::kPreviewAreaRight;
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

  DISALLOW_COPY_AND_ASSIGN(SplitViewWindowSelectorTest);
};

// Tests that dragging a overview window selector item to the edge of the screen
// snaps the window. If two windows are snapped to left and right side of the
// screen, exit the overview mode.
TEST_F(SplitViewWindowSelectorTest, DragOverviewWindowToSnap) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));

  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Drag |window2| selector item to attempt to snap to left. Since there is
  // already one left snapped window |window1|, |window1| will be put in
  // overview mode.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  DragWindowTo(selector_item2, gfx::Point(0, 0));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window2.get());
  EXPECT_TRUE(
      window_selector_controller()->window_selector()->IsWindowInOverview(
          window1.get()));

  // Drag |window3| selector item to snap to right.
  WindowSelectorItem* selector_item3 =
      GetWindowItemForWindow(grid_index, window3.get());
  const gfx::Rect work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  const gfx::Point end_location3(work_area_rect.width(), 0);
  DragWindowTo(selector_item3, end_location3);

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window3.get());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
}

TEST_F(SplitViewWindowSelectorTest, Dragging) {
  Shell::Get()->aura_env()->set_throttle_input_on_resize_for_testing(false);

  ui::test::EventGenerator* generator = GetEventGenerator();

  std::unique_ptr<aura::Window> right_window = CreateTestWindow();
  std::unique_ptr<aura::Window> left_window = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* left_selector_item =
      GetWindowItemForWindow(0, left_window.get());
  WindowSelectorItem* right_selector_item =
      GetWindowItemForWindow(0, right_window.get());

  // The inset on each side of the screen which is a snap region. Items dragged
  // to and released under this region will get snapped.
  const int drag_offset = 5;
  const int drag_offset_snap_region = 48;
  const int minimum_drag_offset = 96;
  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(left_window.get()).width();
  const int edge_inset = GetEdgeInset(screen_width);
  // The selector item has a margin which does not accept events. Inset any
  // event aimed at the selector items edge so events will reach it.
  const int selector_item_inset = 20;

  // Check the two windows set up have a region which is under no snap region, a
  // region that is under the left snap region and a region that is under the
  // right snap region.
  ASSERT_GT(left_selector_item->target_bounds().CenterPoint().x(), edge_inset);
  ASSERT_LT(
      left_selector_item->target_bounds().origin().x() + selector_item_inset,
      edge_inset);
  ASSERT_GT(right_selector_item->target_bounds().top_right().x() -
                selector_item_inset,
            screen_width - edge_inset);

  // Verify if the drag is not started in either snap region, the drag still
  // must move by |drag_offset| before split view acknowledges the drag (ie.
  // starts moving the selector item).
  generator->set_current_location(
      left_selector_item->target_bounds().CenterPoint());
  generator->PressLeftButton();
  const gfx::Rect left_original_bounds = left_selector_item->target_bounds();
  generator->MoveMouseBy(drag_offset - 1, 0);
  EXPECT_EQ(left_original_bounds, left_selector_item->target_bounds());
  generator->MoveMouseBy(1, 0);
  EXPECT_NE(left_original_bounds, left_selector_item->target_bounds());
  generator->ReleaseLeftButton();

  // Verify if the drag is started in the left snap region, the drag needs to
  // move by |drag_offset_snap_region| towards the right side of the screen
  // before split view acknowledges the drag (shows the preview area).
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  generator->set_current_location(gfx::Point(
      left_selector_item->target_bounds().origin().x() + selector_item_inset,
      left_selector_item->target_bounds().CenterPoint().y()));
  generator->PressLeftButton();
  generator->MoveMouseBy(-drag_offset, 0);
  EXPECT_FALSE(IsPreviewAreaShowing());
  generator->MoveMouseBy(drag_offset_snap_region, 0);
  generator->MoveMouseBy(-minimum_drag_offset, 0);
  EXPECT_TRUE(IsPreviewAreaShowing());
  // Drag back to the middle before releasing so that we stay in overview mode
  // on release.
  generator->MoveMouseTo(left_original_bounds.CenterPoint());
  generator->ReleaseLeftButton();

  // Verify if the drag is started in the right snap region, the drag needs to
  // move by |drag_offset_snap_region| towards the left side of the screen
  // before split view acknowledges the drag.
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  generator->set_current_location(
      gfx::Point(right_selector_item->target_bounds().top_right().x() -
                     selector_item_inset,
                 right_selector_item->target_bounds().CenterPoint().y()));
  generator->PressLeftButton();
  generator->MoveMouseBy(drag_offset, 0);
  EXPECT_FALSE(IsPreviewAreaShowing());
  generator->MoveMouseBy(-drag_offset_snap_region, 0);
  generator->MoveMouseBy(minimum_drag_offset, 0);
  EXPECT_TRUE(IsPreviewAreaShowing());
}

// Verify the correct behavior when dragging windows in overview mode.
TEST_F(SplitViewWindowSelectorTest, OverviewDragControllerBehavior) {
  Shell::Get()->aura_env()->set_throttle_input_on_resize_for_testing(false);

  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kOverviewSwipeToClose);

  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* window_item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* window_item2 = GetWindowItemForWindow(0, window2.get());

  // Verify that if a drag is orginally horizontal, the drag behavior is drag to
  // snap.
  using DragBehavior = OverviewWindowDragController::DragBehavior;
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(window_item1->target_bounds().CenterPoint());
  generator->PressTouch();
  OverviewWindowDragController* drag_controller =
      window_selector()->window_drag_controller();
  EXPECT_EQ(DragBehavior::kUndefined, drag_controller->current_drag_behavior());
  generator->MoveTouchBy(20, 0);
  EXPECT_EQ(DragBehavior::kDragToSnap,
            drag_controller->current_drag_behavior());
  generator->ReleaseTouch();
  EXPECT_EQ(DragBehavior::kNoDrag, drag_controller->current_drag_behavior());

  // Verify that if a drag is orginally vertical, the drag behavior is drag to
  // close.
  generator->set_current_location(window_item2->target_bounds().CenterPoint());
  generator->PressTouch();
  drag_controller = window_selector()->window_drag_controller();
  EXPECT_EQ(DragBehavior::kUndefined, drag_controller->current_drag_behavior());

  // Use small increments otherwise a fling event will be fired.
  for (int j = 0; j < 20; ++j)
    generator->MoveTouchBy(0, 1);
  EXPECT_EQ(DragBehavior::kDragToClose,
            drag_controller->current_drag_behavior());
}

// Verify that if the window item has been dragged enough vertically, the window
// will be closed.
TEST_F(SplitViewWindowSelectorTest, DragToClose) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kOverviewSwipeToClose);

  // This test requires a widget.
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<views::Widget> widget1(CreateWindowWidget(bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* item =
      GetWindowItemForWindow(0, widget1->GetNativeWindow());
  const gfx::Point start = item->target_bounds().CenterPoint();
  ASSERT_TRUE(item);

  // This drag has not covered enough distance, so the widget is not closed and
  // we remain in overview mode.
  window_selector()->InitiateDrag(item, start);
  window_selector()->Drag(item, start + gfx::Vector2d(0, 80));
  window_selector()->CompleteDrag(item, start + gfx::Vector2d(0, 80));
  ASSERT_TRUE(window_selector());

  // Verify that the second drag has enough vertical distance, so the widget
  // will be closed and overview mode will be exited.
  window_selector()->InitiateDrag(item, start);
  window_selector()->Drag(item, start + gfx::Vector2d(0, 180));
  window_selector()->CompleteDrag(item, start + gfx::Vector2d(0, 180));
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_selector());
}

// Verify that if the window item has been flung enough vertically, the window
// will be closed.
TEST_F(SplitViewWindowSelectorTest, FlingToClose) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kOverviewSwipeToClose);

  // This test requires a widget.
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<views::Widget> widget1(CreateWindowWidget(bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(1u, window_selector()->grid_list_for_testing()[0]->size());

  WindowSelectorItem* item =
      GetWindowItemForWindow(0, widget1->GetNativeWindow());
  const gfx::Point start = item->target_bounds().CenterPoint();
  ASSERT_TRUE(item);

  // Verify that items flung horizontally do not close the item.
  window_selector()->InitiateDrag(item, start);
  window_selector()->Drag(item, start + gfx::Vector2d(0, 50));
  window_selector()->Fling(item, start, 2500, 0);
  ASSERT_TRUE(window_selector());

  // Verify that items flung vertically, but without enough velocity do not
  // close the item.
  window_selector()->InitiateDrag(item, start);
  window_selector()->Drag(item, start + gfx::Vector2d(0, 50));
  window_selector()->Fling(item, start, 0, 1500);
  ASSERT_TRUE(window_selector());

  // Verify that flinging the item closes it, and since it is the last item in
  // overview mode, overview mode is exited.
  window_selector()->InitiateDrag(item, start);
  window_selector()->Drag(item, start + gfx::Vector2d(0, 50));
  window_selector()->Fling(item, start, 0, 2500);
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(window_selector());
}

// Tests that nudging occurs in the most basic case, which is we have one row
// and one item which is about to be deleted by dragging. If the item is deleted
// we still only have one row, so the other items should nudge while the item is
// being dragged.
TEST_F(SplitViewWindowSelectorTest, BasicNudging) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kOverviewSwipeToClose);

  // Set up three equal windows, which take up one row on the overview grid.
  // When one of them is deleted we are still left with all the windows on one
  // row.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());
  WindowSelectorItem* item3 = GetWindowItemForWindow(0, window3.get());

  const gfx::Rect item1_bounds = item1->target_bounds();
  const gfx::Rect item2_bounds = item2->target_bounds();
  const gfx::Rect item3_bounds = item3->target_bounds();

  // Drag |item1| vertically. |item2| and |item3| bounds should change as they
  // should be nudging towards their final bounds.
  window_selector()->InitiateDrag(item1, item1_bounds.CenterPoint());
  window_selector()->Drag(item1,
                          item1_bounds.CenterPoint() + gfx::Vector2d(0, 160));
  EXPECT_NE(item2_bounds, item2->target_bounds());
  EXPECT_NE(item3_bounds, item3->target_bounds());

  // Drag |item1| back to its start drag location and release, so that it does
  // not get deleted.
  window_selector()->Drag(item1, item1_bounds.CenterPoint());
  window_selector()->CompleteDrag(item1, item1_bounds.CenterPoint());

  // Drag |item3| vertically. |item1| and |item2| bounds should change as they
  // should be nudging towards their final bounds.
  window_selector()->InitiateDrag(item3, item3_bounds.CenterPoint());
  window_selector()->Drag(item3,
                          item3_bounds.CenterPoint() + gfx::Vector2d(0, 160));
  EXPECT_NE(item1_bounds, item1->target_bounds());
  EXPECT_NE(item2_bounds, item2->target_bounds());
}

// Tests that no nudging occurs when the number of rows in overview mode change
// if the item to be deleted results in the overview grid to change number of
// rows.
TEST_F(SplitViewWindowSelectorTest, NoNudgingWhenNumRowsChange) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kOverviewSwipeToClose);

  // Set up four equal windows, which would split into two rows in overview
  // mode. Removing one window would leave us with three windows, which only
  // takes a single row in overview.
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  std::unique_ptr<aura::Window> window3 = CreateTestWindow();
  std::unique_ptr<aura::Window> window4 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* item1 = GetWindowItemForWindow(0, window1.get());
  WindowSelectorItem* item2 = GetWindowItemForWindow(0, window2.get());
  WindowSelectorItem* item3 = GetWindowItemForWindow(0, window3.get());
  WindowSelectorItem* item4 = GetWindowItemForWindow(0, window4.get());

  const gfx::Rect item1_bounds = item1->target_bounds();
  const gfx::Rect item2_bounds = item2->target_bounds();
  const gfx::Rect item3_bounds = item3->target_bounds();
  const gfx::Rect item4_bounds = item4->target_bounds();

  // Drag |item1| past the drag to swipe threshold. None of the other window
  // bounds should change, as none of them should be nudged.
  window_selector()->InitiateDrag(item1, item1_bounds.CenterPoint());
  window_selector()->Drag(item1,
                          item1_bounds.CenterPoint() + gfx::Vector2d(0, 160));
  EXPECT_EQ(item2_bounds, item2->target_bounds());
  EXPECT_EQ(item3_bounds, item3->target_bounds());
  EXPECT_EQ(item4_bounds, item4->target_bounds());
}

// Tests that no nudging occurs when the item to be deleted results in an item
// from the previous row to drop down to the current row, thus causing the items
// to the right of the item to be shifted right, which is visually unacceptable.
TEST_F(SplitViewWindowSelectorTest, NoNudgingWhenLastItemOnPreviousRowDrops) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(features::kOverviewSwipeToClose);

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
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* items[kWindows];
  gfx::Rect item_bounds[kWindows];
  for (int i = 0; i < kWindows; ++i) {
    items[i] = GetWindowItemForWindow(0, windows[i].get());
    item_bounds[i] = items[i]->target_bounds();
  }

  // Drag the forth item past the drag to swipe threshold. None of the other
  // window bounds should change, as none of them should be nudged, because
  // deleting the fourth item will cause the third item to drop down from the
  // first row to the second.
  window_selector()->InitiateDrag(items[3], item_bounds[3].CenterPoint());
  window_selector()->Drag(items[3],
                          item_bounds[3].CenterPoint() + gfx::Vector2d(0, 160));
  EXPECT_EQ(item_bounds[0], items[0]->target_bounds());
  EXPECT_EQ(item_bounds[1], items[1]->target_bounds());
  EXPECT_EQ(item_bounds[2], items[2]->target_bounds());
  EXPECT_EQ(item_bounds[4], items[4]->target_bounds());

  // Drag the fourth item back to its start drag location and release, so that
  // it does not get deleted.
  window_selector()->Drag(items[3], item_bounds[3].CenterPoint());
  window_selector()->CompleteDrag(items[3], item_bounds[3].CenterPoint());

  // Drag the first item past the drag to swipe threshold. The second and third
  // items should nudge as expected as there is no item dropping down to their
  // row. The fourth and fifth items should not nudge as they are in a different
  // row than the first item.
  window_selector()->InitiateDrag(items[0], item_bounds[0].CenterPoint());
  window_selector()->Drag(items[0],
                          item_bounds[0].CenterPoint() + gfx::Vector2d(0, 160));
  EXPECT_NE(item_bounds[1], items[1]->target_bounds());
  EXPECT_NE(item_bounds[2], items[2]->target_bounds());
  EXPECT_EQ(item_bounds[3], items[3]->target_bounds());
  EXPECT_EQ(item_bounds[4], items[4]->target_bounds());
}

// Verify the window grid size changes as expected when dragging items around in
// overview mode when split view is enabled.
TEST_F(SplitViewWindowSelectorTest, WindowGridSizeWhileDraggingWithSplitView) {
  // Add three windows and enter overview mode.
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Select window one and start the drag.
  const int grid_index = 0;
  const int window_width =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen().width();
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window1.get());
  gfx::Rect selector_item_bounds = selector_item->target_bounds();
  gfx::Point start_location(selector_item_bounds.CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);

  // Verify that when dragged to the left, the window grid is located where the
  // right window of split view mode should be.
  const gfx::Point left(0, 0);
  window_selector()->Drag(selector_item, left);
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(SplitViewController::NO_SNAP, split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->left_window() == nullptr);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());

  // Verify that when dragged to the right, the window grid is located where the
  // left window of split view mode should be.
  const gfx::Point right(window_width, 0);
  window_selector()->Drag(selector_item, right);
  EXPECT_EQ(SplitViewController::NO_SNAP, split_view_controller()->state());
  EXPECT_TRUE(split_view_controller()->right_window() == nullptr);
  EXPECT_EQ(GetSplitViewLeftWindowBounds(window1.get()), GetGridBounds());

  // Verify that when dragged to the center, the window grid is has the
  // dimensions of the work area.
  const gfx::Point center(window_width / 2, 0);
  window_selector()->Drag(selector_item, center);
  EXPECT_EQ(SplitViewController::NO_SNAP, split_view_controller()->state());
  EXPECT_EQ(
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window1.get()),
      GetGridBounds());

  // Snap window1 to the left and initialize dragging for window2.
  window_selector()->Drag(selector_item, left);
  window_selector()->CompleteDrag(selector_item, left);
  ASSERT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  ASSERT_EQ(window1.get(), split_view_controller()->left_window());
  selector_item = GetWindowItemForWindow(grid_index, window2.get());
  selector_item_bounds = selector_item->target_bounds();
  start_location = selector_item_bounds.CenterPoint();
  window_selector()->InitiateDrag(selector_item, start_location);

  // Verify that when there is a snapped window, the window grid bounds remain
  // constant despite window selector items being dragged left and right.
  window_selector()->Drag(selector_item, left);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());
  window_selector()->Drag(selector_item, right);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());
  window_selector()->Drag(selector_item, center);
  EXPECT_EQ(GetSplitViewRightWindowBounds(window1.get()), GetGridBounds());
}

// Tests dragging a unsnappable window.
TEST_F(SplitViewWindowSelectorTest, DraggingUnsnappableAppWithSplitView) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  // The grid bounds should be the size of the root window minus the shelf.
  const gfx::Rect root_window_bounds =
      Shell::Get()->GetPrimaryRootWindow()->GetBoundsInScreen();
  const gfx::Rect shelf_bounds =
      Shelf::ForWindow(Shell::Get()->GetPrimaryRootWindow())->GetIdealBounds();
  const gfx::Rect expected_grid_bounds =
      SubtractRects(root_window_bounds, shelf_bounds);

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Verify that after dragging the unsnappable window to the left and right,
  // the window grid bounds do not change.
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(0, unsnappable_window.get());
  window_selector()->InitiateDrag(selector_item,
                                  selector_item->target_bounds().CenterPoint());
  window_selector()->Drag(selector_item, gfx::Point(0, 0));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  window_selector()->Drag(selector_item,
                          gfx::Point(root_window_bounds.right(), 0));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
  window_selector()->Drag(selector_item,
                          gfx::Point(root_window_bounds.right() / 2, 0));
  EXPECT_EQ(expected_grid_bounds, GetGridBounds());
}

// Tests that if there is only one window in the MRU window list in the overview
// mode, snapping the window to one side of the screen will not end the overview
// mode even if there is no more window left in the overview window grid.
TEST_F(SplitViewWindowSelectorTest, EmptyWindowsListNotExitOverview) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));

  ToggleOverview();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));

  // Test that overview mode is active in this single window case.
  EXPECT_EQ(split_view_controller()->IsSplitViewModeActive(), true);
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // Create a new window should exit the overview mode.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  // If there are only 2 snapped windows, close one of them should enter
  // overview mode.
  window2.reset();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // If there are more than 2 windows in overview
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  wm::ActivateWindow(window3.get());
  wm::ActivateWindow(window4.get());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  ToggleOverview();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  window3.reset();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  window4.reset();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // Test that if there is only 1 snapped window, and no window in the overview
  // grid, ToggleOverview() can't end overview.
  ToggleOverview();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  EndSplitView();
  EXPECT_FALSE(Shell::Get()->IsSplitViewModeActive());
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // Test that ToggleOverview() can end overview if we're not in split view
  // mode.
  ToggleOverview();
  EXPECT_FALSE(window_selector_controller()->IsSelecting());

  // Now enter overview and split view again. Test that exiting tablet mode can
  // end split view and overview correctly.
  ToggleOverview();
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_TRUE(Shell::Get()->IsSplitViewModeActive());
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(false);
  EXPECT_FALSE(Shell::Get()->IsSplitViewModeActive());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());

  // Test that closing all windows in overview can end overview if we're not in
  // split view mode.
  ToggleOverview();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  window1.reset();
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
}

// Verify the split view preview area becomes visible when expected.
TEST_F(SplitViewWindowSelectorTest, PreviewAreaVisibility) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window.get()).width();
  const int edge_inset = GetEdgeInset(screen_width);

  // Verify the preview area is visible when |selector_item|'s x is in the
  // range [0, edge_inset] or [screen_width - edge_inset - 1, screen_width].
  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window.get());
  const gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  // Drag horizontally to avoid activating drag to close.
  const int y = start_location.y();
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_FALSE(IsPreviewAreaShowing());
  window_selector()->Drag(selector_item, gfx::Point(edge_inset + 1, y));
  EXPECT_FALSE(IsPreviewAreaShowing());
  window_selector()->Drag(selector_item, gfx::Point(edge_inset, y));
  EXPECT_TRUE(IsPreviewAreaShowing());

  window_selector()->Drag(selector_item,
                          gfx::Point(screen_width - edge_inset - 2, y));
  EXPECT_FALSE(IsPreviewAreaShowing());
  window_selector()->Drag(selector_item,
                          gfx::Point(screen_width - edge_inset - 1, y));
  EXPECT_TRUE(IsPreviewAreaShowing());

  // Drag back to |start_location| before compeleting the drag, otherwise
  // |selector_time| will snap to the right and the system will enter splitview,
  // making |window_drag_controller()| nullptr.
  window_selector()->Drag(selector_item, start_location);
  window_selector()->CompleteDrag(selector_item, start_location);
  EXPECT_FALSE(IsPreviewAreaShowing());
}

// Verify that the preview area never shows up when dragging a unsnappable
// window.
TEST_F(SplitViewWindowSelectorTest, PreviewAreaVisibilityUnsnappableWindow) {
  std::unique_ptr<aura::Window> window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window.get()).width();

  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window.get());
  const gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_FALSE(IsPreviewAreaShowing());
  window_selector()->Drag(selector_item, gfx::Point(0, 1));
  EXPECT_FALSE(IsPreviewAreaShowing());
  window_selector()->Drag(selector_item, gfx::Point(screen_width, 1));
  EXPECT_FALSE(IsPreviewAreaShowing());

  window_selector()->CompleteDrag(selector_item, start_location);
  EXPECT_FALSE(IsPreviewAreaShowing());
}

// Verify that the split view overview overlay has the expected state.
TEST_F(SplitViewWindowSelectorTest, SplitViewDragIndicatorsState) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int screen_width =
      screen_util::GetDisplayWorkAreaBoundsInParent(window1.get()).width();
  const int edge_inset = GetEdgeInset(screen_width);

  // Verify that when are no snapped windows, the indicator is visible once
  // there is a long press or after the drag has started.
  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, window1.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorState::kNone, indicator_state());
  window_selector()->StartSplitViewDragMode(start_location);
  EXPECT_EQ(IndicatorState::kDragArea, indicator_state());

  // Reset the gesture so we stay in overview mode.
  window_selector()->ResetDraggedWindowGesture();

  // Verify the indicator is visible once the item starts moving, and becomes a
  // preview area once we reach the left edge of the screen. Drag horizontal to
  // avoid activating drag to close.
  const int y_position = start_location.y();
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorState::kNone, indicator_state());
  window_selector()->Drag(selector_item,
                          gfx::Point(edge_inset + 1, y_position));
  EXPECT_EQ(IndicatorState::kDragArea, indicator_state());
  window_selector()->Drag(selector_item, gfx::Point(edge_inset, y_position));
  EXPECT_EQ(IndicatorState::kPreviewAreaLeft, indicator_state());

  // Snap window to the left.
  window_selector()->CompleteDrag(selector_item,
                                  gfx::Point(edge_inset, y_position));
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());
  ASSERT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());

  // Verify that when there is a left snapped window, dragging an item to the
  // right will show the right preview area.
  selector_item = GetWindowItemForWindow(grid_index, window2.get());
  start_location = selector_item->target_bounds().CenterPoint();
  window_selector()->InitiateDrag(selector_item, start_location);
  EXPECT_EQ(IndicatorState::kNone, indicator_state());
  window_selector()->Drag(selector_item,
                          gfx::Point(screen_width - 1, y_position));
  EXPECT_EQ(IndicatorState::kPreviewAreaRight, indicator_state());
  window_selector()->CompleteDrag(selector_item, start_location);
}

// Verify that the split view drag indicator is shown when expected when
// attempting to drag a unsnappable window.
TEST_F(SplitViewWindowSelectorTest,
       SplitViewDragIndicatorVisibilityUnsnappableWindow) {
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, unsnappable_window.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  window_selector()->StartSplitViewDragMode(start_location);
  EXPECT_EQ(IndicatorState::kCannotSnap, indicator_state());
  const gfx::Point end_location1(0, 0);
  window_selector()->Drag(selector_item, end_location1);
  EXPECT_EQ(IndicatorState::kCannotSnap, indicator_state());
  window_selector()->CompleteDrag(selector_item, end_location1);
  EXPECT_EQ(IndicatorState::kNone, indicator_state());
}

// Verify when the split view drag indicators state changes, the expected
// indicators will become visible or invisible.
TEST_F(SplitViewWindowSelectorTest, SplitViewDragIndicatorsVisibility) {
  auto indicator = std::make_unique<SplitViewDragIndicators>();

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

  // Check each state has the correct views displayed. Pass and empty point as
  // the location since there is no need to reparent the widget. Verify that
  // nothing is shown in the none state.
  indicator->SetIndicatorState(IndicatorState::kNone, gfx::Point());
  check_helper(indicator.get(), 0);

  const int all = to_int(IndicatorType::kLeftHighlight) |
                  to_int(IndicatorType::kLeftText) |
                  to_int(IndicatorType::kRightHighlight) |
                  to_int(IndicatorType::kRightText);
  // Verify that everything is visible in the dragging and cannot snap states.
  indicator->SetIndicatorState(IndicatorState::kDragArea, gfx::Point());
  check_helper(indicator.get(), all);
  indicator->SetIndicatorState(IndicatorState::kCannotSnap, gfx::Point());
  check_helper(indicator.get(), all);

  // Verify that only one highlight shows up for the preview area states.
  indicator->SetIndicatorState(IndicatorState::kPreviewAreaLeft, gfx::Point());
  check_helper(indicator.get(), to_int(IndicatorType::kLeftHighlight));
  indicator->SetIndicatorState(IndicatorState::kPreviewAreaRight, gfx::Point());
  check_helper(indicator.get(), to_int(IndicatorType::kRightHighlight));
}

// Verify that the split view drag indicators widget reparents when starting a
// drag on a different display.
TEST_F(SplitViewWindowSelectorTest, SplitViewDragIndicatorsWidgetReparenting) {
  // Add two displays and one window on each display.
  UpdateDisplay("600x600,600x600");
  // DisplayConfigurationObserver enables mirror mode when tablet mode is
  // enabled. Disable mirror mode to test multiple displays.
  display_manager()->SetMirrorMode(display::MirrorMode::kOff, base::nullopt);
  base::RunLoop().RunUntilIdle();

  auto root_windows = Shell::Get()->GetAllRootWindows();
  ASSERT_EQ(2u, root_windows.size());

  const gfx::Rect primary_screen_bounds(0, 0, 600, 600);
  const gfx::Rect secondary_screen_bounds(600, 0, 600, 600);
  std::unique_ptr<aura::Window> primary_screen_window(
      CreateWindow(primary_screen_bounds));
  std::unique_ptr<aura::Window> secondary_screen_window(
      CreateWindow(secondary_screen_bounds));

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Select an item on the primary display and verify the drag indicators
  // widget's parent is the primary root window.
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(0, primary_screen_window.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  window_selector()->Drag(selector_item, gfx::Point(100, start_location.y()));
  EXPECT_EQ(IndicatorState::kDragArea, indicator_state());
  EXPECT_EQ(root_windows[0], window_selector()
                                 ->split_view_drag_indicators()
                                 ->widget_->GetNativeView()
                                 ->GetRootWindow());
  // Drag the item in a way that neither opens the window nor activates
  // splitview mode.
  window_selector()->Drag(selector_item, primary_screen_bounds.CenterPoint());
  window_selector()->CompleteDrag(selector_item,
                                  primary_screen_bounds.CenterPoint());
  ASSERT_TRUE(window_selector());
  ASSERT_FALSE(split_view_controller()->IsSplitViewModeActive());

  // Select an item on the secondary display and verify the indicators widget
  // has reparented to the secondary root window.
  selector_item = GetWindowItemForWindow(1, secondary_screen_window.get());
  start_location = gfx::Point(selector_item->target_bounds().CenterPoint());
  window_selector()->InitiateDrag(selector_item, start_location);
  window_selector()->Drag(selector_item, gfx::Point(800, start_location.y()));
  EXPECT_EQ(IndicatorState::kDragArea, indicator_state());
  EXPECT_EQ(root_windows[1], window_selector()
                                 ->split_view_drag_indicators()
                                 ->widget_->GetNativeView()
                                 ->GetRootWindow());
  window_selector()->CompleteDrag(selector_item, start_location);
}

// Test the overview window drag functionalities when screen rotates.
TEST_F(SplitViewWindowSelectorTest, SplitViewRotationTest) {
  using svc = SplitViewController;

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
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(), svc::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to right.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  gfx::Rect work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  gfx::Point end_location2(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(), svc::BOTH_SNAPPED);
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
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(), svc::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to right.
  selector_item2 = GetWindowItemForWindow(grid_index, window2.get());
  work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  end_location2 = gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(), svc::BOTH_SNAPPED);
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
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(), svc::RIGHT_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());

  // Test that dragging |window2| to the right of the screen snaps it to left.
  selector_item2 = GetWindowItemForWindow(grid_index, window2.get());
  work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  end_location2 = gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2, SelectorItemLocation::ORIGIN);
  EXPECT_EQ(split_view_controller()->state(), svc::BOTH_SNAPPED);
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
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(), svc::RIGHT_SNAPPED);
  EXPECT_EQ(split_view_controller()->right_window(), window1.get());

  // Test that dragging |window2| to the bottom of the screen snaps it to left.
  selector_item2 = GetWindowItemForWindow(grid_index, window2.get());
  work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  end_location2 = gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2);
  EXPECT_EQ(split_view_controller()->state(), svc::BOTH_SNAPPED);
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
TEST_F(SplitViewWindowSelectorTest, SplitViewOverviewBothActiveTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));

  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
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
TEST_F(SplitViewWindowSelectorTest, SelectUnsnappableWindowInSplitView) {
  // Create one snappable and one unsnappable window.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  // Snap the snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());

  // Select the unsnappable window.
  const int grid_index = 0;
  WindowSelectorItem* selector_item =
      GetWindowItemForWindow(grid_index, unsnappable_window.get());
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(selector_item->target_bounds().CenterPoint());
  generator->ClickLeftButton();

  // Verify that we are out of split view and overview mode, and that the active
  // window is the unsnappable window.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(unsnappable_window.get(), wm::GetActiveWindow());

  std::unique_ptr<aura::Window> window2 = CreateTestWindow();
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  split_view_controller()->SnapWindow(window2.get(),
                                      SplitViewController::RIGHT);

  // Split view mode should be active. Overview mode should be ended.
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());

  ToggleOverview();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  EXPECT_TRUE(window_selector_controller()->IsSelecting());

  // Now select the unsnappable window.
  selector_item = GetWindowItemForWindow(grid_index, unsnappable_window.get());
  generator->set_current_location(selector_item->target_bounds().CenterPoint());
  generator->ClickLeftButton();

  // Split view mode should be ended. And the unsnappable window should be the
  // active window now.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(unsnappable_window.get(), wm::GetActiveWindow());
}

// Verify that when in overview mode, the selector items unsnappable indicator
// shows up when expected.
TEST_F(SplitViewWindowSelectorTest, OverviewUnsnappableIndicatorVisibility) {
  // Create three windows; two normal and one unsnappable, so that when after
  // snapping |window1| to enter split view we can test the state of each normal
  // and unsnappable windows.
  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  std::unique_ptr<aura::Window> unsnappable_window = CreateUnsnappableWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  const int grid_index = 0;
  WindowSelectorItem* snappable_selector_item =
      GetWindowItemForWindow(grid_index, window2.get());
  WindowSelectorItem* unsnappable_selector_item =
      GetWindowItemForWindow(grid_index, unsnappable_window.get());

  // Note: the container for |cannot_snap_label_view_| will be created
  // on demand, and its parent remains null until the container is created.
  EXPECT_FALSE(snappable_selector_item->cannot_snap_label_view_->parent());
  ASSERT_FALSE(unsnappable_selector_item->cannot_snap_label_view_->parent());

  // Snap the extra snappable window to enter split view mode.
  split_view_controller()->SnapWindow(window1.get(), SplitViewController::LEFT);
  ASSERT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(snappable_selector_item->cannot_snap_label_view_->parent());
  ASSERT_TRUE(unsnappable_selector_item->cannot_snap_label_view_->parent());
  ui::Layer* unsnappable_layer =
      unsnappable_selector_item->cannot_snap_label_view_->parent()->layer();
  EXPECT_EQ(1.f, unsnappable_layer->opacity());

  // Exiting the splitview will hide the unsnappable label.
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(/*is_dragging=*/false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(0, 0);

  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(0.f, unsnappable_layer->opacity());
}

// Test that when splitview mode and overview mode are both active at the same
// time, dragging divider behaviors are correct.
TEST_F(SplitViewWindowSelectorTest, DragDividerToExitTest) {
  UpdateDisplay("907x407");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();

  // Drag |window1| selector item to snap to left.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Drag the divider toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  split_view_controller()->EndResize(gfx::Point(0, 0));

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Now drag |window2| selector item to snap to left.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  DragWindowTo(selector_item2, gfx::Point(0, 0));
  // Test that overview mode and split view mode are both active.
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Drag the divider toward closing the overview window grid.
  divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  const gfx::Rect display_bounds =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  split_view_controller()->EndResize(display_bounds.bottom_right());

  // Test that split view mode is ended. Overview mode is also ended. |window2|
  // should be activated.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_FALSE(Shell::Get()->window_selector_controller()->IsSelecting());
  EXPECT_EQ(window2.get(), wm::GetActiveWindow());
}

TEST_F(SplitViewWindowSelectorTest, WindowSelectorItemLongPressed) {
  std::unique_ptr<aura::Window> window1 = CreateTestWindow();
  std::unique_ptr<aura::Window> window2 = CreateTestWindow();

  ToggleOverview();
  ASSERT_TRUE(window_selector_controller()->IsSelecting());

  WindowSelectorItem* selector_item = GetWindowItemForWindow(0, window1.get());
  gfx::Point start_location(selector_item->target_bounds().CenterPoint());
  const gfx::Rect original_bounds(selector_item->target_bounds());

  // Verify that when a window selector item receives a resetting gesture, we
  // stay in overview mode and the bounds of the item are the same as they were
  // before the press sequence started.
  window_selector()->InitiateDrag(selector_item, start_location);
  window_selector()->ResetDraggedWindowGesture();
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(original_bounds, selector_item->target_bounds());

  // Verify that when a window selector item is tapped, we exit overview mode,
  // and the current active window is the item.
  window_selector()->InitiateDrag(selector_item, start_location);
  window_selector()->ActivateDraggedWindow();
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(window1.get(), wm::GetActiveWindow());
}

TEST_F(SplitViewWindowSelectorTest, SnappedWindowBoundsTest) {
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
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Then drag the divider to left toward closing the snapped window.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(false /*is_dragging=*/);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  split_view_controller()->EndResize(gfx::Point(20, 20));

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Test that |window1| has the dimensions of a tablet mode maxed window, so
  // that when it is placed back on the grid it will not look skinny.
  EXPECT_LE(window1->bounds().x(), 0);
  EXPECT_EQ(window1->bounds().width(), screen_width);

  // Drag |window2| selector item to snap to right.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  const gfx::Rect work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  gfx::Point end_location2 =
      gfx::Point(work_area_rect.width(), work_area_rect.height());
  DragWindowTo(selector_item2, end_location2);
  EXPECT_EQ(SplitViewController::RIGHT_SNAPPED,
            split_view_controller()->state());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());

  // Then drag the divider to right toward closing the snapped window.
  divider_bounds = GetSplitViewDividerBounds(false /* is_dragging */);
  split_view_controller()->StartResize(divider_bounds.CenterPoint());
  // Drag the divider to a point that is close enough but still have a short
  // distance to the edge of the screen.
  end_location2.Offset(-20, -20);
  split_view_controller()->EndResize(end_location2);

  // Test that split view mode is ended. Overview mode is still active.
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(Shell::Get()->window_selector_controller()->IsSelecting());
  // Test that |window2| has the dimensions of a tablet mode maxed window, so
  // that when it is placed back on the grid it will not look skinny.
  EXPECT_GE(window2->bounds().x(), 0);
  EXPECT_EQ(window2->bounds().width(), screen_width);
}

// Verify that if the split view divider is dragged all the way to the edge, the
// window being dragged gets returned to the overview list, if overview mode is
// still active.
TEST_F(SplitViewWindowSelectorTest,
       DividerDraggedToEdgeReturnsWindowToOverviewList) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window3(CreateWindow(bounds));

  ToggleOverview();
  // Drag |window1| selector item to snap to left. There should be two items on
  // the overview grid afterwards, |window2| and |window3|.
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  EXPECT_TRUE(IsSelecting());
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  ASSERT_TRUE(split_view_controller()->split_view_divider());
  std::vector<aura::Window*> window_list =
      window_selector_controller()->GetWindowsListInOverviewGridsForTesting();
  EXPECT_EQ(2u, window_list.size());
  EXPECT_FALSE(base::ContainsValue(window_list, window1.get()));
  EXPECT_TRUE(wm::IsActiveWindow(window1.get()));

  // Drag the divider to the left edge.
  const gfx::Rect divider_bounds =
      GetSplitViewDividerBounds(/*is_dragging=*/false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->DragMouseTo(0, 0);

  // Verify that it is still in overview mode and that |window1| is returned to
  // the overview list.
  EXPECT_TRUE(IsSelecting());
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  window_list =
      window_selector_controller()->GetWindowsListInOverviewGridsForTesting();
  EXPECT_EQ(3u, window_list.size());
  EXPECT_TRUE(base::ContainsValue(window_list, window1.get()));
  EXPECT_FALSE(wm::IsActiveWindow(window1.get()));
}

// Verify that if the split view divider is dragged close to the edge, the grid
// bounds will be fixed to a third of the work area width and start sliding off
// the screen instead of continuing to shrink.
TEST_F(SplitViewWindowSelectorTest,
       OverviewHasMinimumBoundsWhenDividerDragged) {
  UpdateDisplay("600x400");

  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  // Snap a window to the left and test dragging the divider towards the right
  // edge of the screen.
  Shell::Get()->split_view_controller()->SnapWindow(window1.get(),
                                                    SplitViewController::LEFT);
  WindowGrid* grid = window_selector()->grid_list_for_testing()[0].get();
  ASSERT_TRUE(grid);

  // Drag the divider to the right edge.
  gfx::Rect divider_bounds = GetSplitViewDividerBounds(/*is_dragging=*/false);
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->PressLeftButton();

  // Tests that near the right edge, the grid bounds are fixed at 200 and are
  // partially off screen to the right.
  generator->MoveMouseTo(580, 0);
  EXPECT_EQ(200, grid->bounds().width());
  EXPECT_GT(grid->bounds().right(), 600);
  generator->ReleaseLeftButton();

  // Releasing close to the edge will activate the left window and exit
  // overview.
  ASSERT_FALSE(IsSelecting());
  ToggleOverview();
  // Snap a window to the right and test dragging the divider towards the left
  // edge of the screen.
  Shell::Get()->split_view_controller()->SnapWindow(window1.get(),
                                                    SplitViewController::RIGHT);
  grid = window_selector()->grid_list_for_testing()[0].get();
  ASSERT_TRUE(grid);

  // Drag the divider to the left edge.
  divider_bounds = GetSplitViewDividerBounds(/*is_dragging=*/false);
  generator->set_current_location(divider_bounds.CenterPoint());
  generator->PressLeftButton();

  generator->MoveMouseTo(20, 0);
  // Tests that near the left edge, the grid bounds are fixed at 200 and are
  // partially off screen to the left.
  EXPECT_EQ(200, grid->bounds().width());
  EXPECT_LT(grid->bounds().x(), 0);
  generator->ReleaseLeftButton();
}

// Test that when splitview mode is active, minimizing one of the snapped window
// will insert the minimized window back to overview mode if overview mode is
// active at the moment.
TEST_F(SplitViewWindowSelectorTest, InsertMinimizedWindowBackToOverview) {
  const gfx::Rect bounds(400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();

  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_TRUE(IsSelecting());

  // Minimize |window1| will put |window1| back to overview grid.
  wm::GetWindowState(window1.get())->Minimize();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(IsSelecting());
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window1.get()));

  // Now snap both |window1| and |window2|.
  selector_item1 = GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  wm::ActivateWindow(window2.get());
  EXPECT_FALSE(IsSelecting());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::BOTH_SNAPPED);
  EXPECT_EQ(split_view_controller()->left_window(), window1.get());
  EXPECT_EQ(split_view_controller()->right_window(), window2.get());

  // Minimize |window1| will open overview and put |window1| to overview grid.
  wm::GetWindowState(window1.get())->Minimize();
  EXPECT_TRUE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  EXPECT_TRUE(IsSelecting());
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window1.get()));

  // Minimize |window2| also put |window2| to overview grid.
  wm::GetWindowState(window2.get())->Minimize();
  EXPECT_FALSE(split_view_controller()->IsSplitViewModeActive());
  EXPECT_TRUE(IsSelecting());
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window1.get()));
  EXPECT_TRUE(GetWindowItemForWindow(grid_index, window2.get()));
}

// Test that when splitview and overview are both active at the same time, if
// overview is ended due to snapping a window in splitview, the tranform of each
// window in the overview grid is restored.
TEST_F(SplitViewWindowSelectorTest, SnappedWindowAnimationObserverTest) {
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
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  // Drag |window2| to snap to right.
  WindowSelectorItem* selector_item2 =
      GetWindowItemForWindow(grid_index, window2.get());
  const gfx::Rect work_area_rect =
      split_view_controller()->GetDisplayWorkAreaBoundsInScreen(window2.get());
  const gfx::Point end_location2(work_area_rect.width(), 0);
  DragWindowTo(selector_item2, end_location2);
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
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
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  // ToggleOverview() directly.
  ToggleOverview();
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());

  // 3. Overview is ended by actviating an existing window.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  wm::ActivateWindow(window2.get());
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());

  // 4. Overview is ended by activating a new window.
  ToggleOverview();
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_FALSE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_EQ(SplitViewController::LEFT_SNAPPED,
            split_view_controller()->state());
  std::unique_ptr<aura::Window> window4(CreateWindow(bounds));
  wm::ActivateWindow(window4.get());
  EXPECT_EQ(SplitViewController::BOTH_SNAPPED,
            split_view_controller()->state());
  EXPECT_FALSE(window_selector_controller()->IsSelecting());
  EXPECT_TRUE(window1->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window2->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window3->layer()->GetTargetTransform().IsIdentity());
  EXPECT_TRUE(window4->layer()->GetTargetTransform().IsIdentity());
}

// Test that when split view and overview are both active at the same time,
// double tapping on the divider can swap the window's position with the
// overview window grid's postion.
TEST_F(SplitViewWindowSelectorTest, SwapWindowAndOverviewGrid) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));

  ToggleOverview();
  const int grid_index = 0;
  WindowSelectorItem* selector_item1 =
      GetWindowItemForWindow(grid_index, window1.get());
  DragWindowTo(selector_item1, gfx::Point(0, 0));
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::LEFT_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::LEFT);
  EXPECT_TRUE(window_selector_controller()->IsSelecting());
  EXPECT_EQ(GetGridBounds(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::RIGHT));

  split_view_controller()->SwapWindows();
  EXPECT_EQ(split_view_controller()->state(),
            SplitViewController::RIGHT_SNAPPED);
  EXPECT_EQ(split_view_controller()->default_snap_position(),
            SplitViewController::RIGHT);
  EXPECT_EQ(GetGridBounds(),
            split_view_controller()->GetSnappedWindowBoundsInScreen(
                window1.get(), SplitViewController::LEFT));
}

// Verify the behavior when trying to exit overview with one snapped window
// is as expected.
TEST_F(SplitViewWindowSelectorTest, ExitOverviewWithOneSnapped) {
  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window(CreateWindow(bounds));

  // Tests that we cannot exit overview when there is one snapped window and no
  // windows in overview normally.
  ToggleOverview();
  split_view_controller()->SnapWindow(window.get(), SplitViewController::LEFT);
  ToggleOverview();
  ASSERT_TRUE(IsSelecting());

  // Tests that we can exit overview if we swipe up from the shelf.
  ToggleOverview(WindowSelector::EnterExitOverviewType::kSwipeFromShelf);
  EXPECT_FALSE(IsSelecting());
}

}  // namespace ash
