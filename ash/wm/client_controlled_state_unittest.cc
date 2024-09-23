// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

#include <queue>

#include "ash/display/screen_orientation_controller.h"
#include "ash/display/screen_orientation_controller_test_api.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_util.h"
#include "ash/test/test_widget_builder.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/float/float_test_api.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/snap_group/snap_group_test_util.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/display_util.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/frame_caption_button_container_view.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/wm/constants.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::WindowStateType;

using BoundsRequestCallback =
    base::RepeatingCallback<void(const gfx::Rect& bounds)>;
using WindowStateRequestCallback =
    base::RepeatingCallback<void(WindowStateType new_state)>;

constexpr gfx::Rect kInitialBounds(0, 0, 100, 100);

class TestClientControlledStateDelegate
    : public ClientControlledState::Delegate {
 public:
  TestClientControlledStateDelegate() = default;

  TestClientControlledStateDelegate(const TestClientControlledStateDelegate&) =
      delete;
  TestClientControlledStateDelegate& operator=(
      const TestClientControlledStateDelegate&) = delete;

  ~TestClientControlledStateDelegate() override = default;

  void HandleWindowStateRequest(WindowState* window_state,
                                WindowStateType next_state) override {
    EXPECT_FALSE(deleted_);
    old_state_ = window_state->GetStateType();
    new_state_ = next_state;
    if (window_state_request_callback_) {
      window_state_request_callback_.Run(next_state);
    }
  }

  void HandleBoundsRequest(WindowState* window_state,
                           WindowStateType requested_state,
                           const gfx::Rect& bounds,
                           int64_t display_id) override {
    requested_bounds_ = bounds;
    if (requested_state != window_state->GetStateType()) {
      DCHECK(requested_state == WindowStateType::kPrimarySnapped ||
             requested_state == WindowStateType::kSecondarySnapped ||
             requested_state == WindowStateType::kFloated);
      old_state_ = window_state->GetStateType();
      new_state_ = requested_state;
    }
    display_id_ = display_id;
    if (bounds_request_callback_) {
      bounds_request_callback_.Run(bounds);
    }
  }

  WindowStateType old_state() const { return old_state_; }

  WindowStateType new_state() const { return new_state_; }

  const gfx::Rect& requested_bounds() const { return requested_bounds_; }

  void set_bounds_request_callback(BoundsRequestCallback callback) {
    bounds_request_callback_ = std::move(callback);
  }
  void set_window_state_request_callback(WindowStateRequestCallback callback) {
    window_state_request_callback_ = std::move(callback);
  }

  int64_t display_id() const { return display_id_; }

  void Reset() {
    old_state_ = WindowStateType::kDefault;
    new_state_ = WindowStateType::kDefault;
    requested_bounds_.SetRect(0, 0, 0, 0);
    display_id_ = display::kInvalidDisplayId;
  }

  void mark_as_deleted() { deleted_ = true; }

 private:
  WindowStateType old_state_ = WindowStateType::kDefault;
  WindowStateType new_state_ = WindowStateType::kDefault;
  int64_t display_id_ = display::kInvalidDisplayId;
  gfx::Rect requested_bounds_;
  bool deleted_ = false;
  BoundsRequestCallback bounds_request_callback_;
  WindowStateRequestCallback window_state_request_callback_;
};

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() = default;

  TestWidgetDelegate(const TestWidgetDelegate&) = delete;
  TestWidgetDelegate& operator=(const TestWidgetDelegate&) = delete;

  ~TestWidgetDelegate() override = default;

  void EnableSnap() {
    SetCanMaximize(true);
    SetCanResize(true);
    GetWidget()->OnSizeConstraintsChanged();
  }

  void EnableFloat() {
    SetCanResize(true);
    GetWidget()->OnSizeConstraintsChanged();
  }

  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    return std::make_unique<NonClientFrameViewAsh>(widget);
  }
};

class TestEmptyState : public WindowState::State {
 public:
  void OnWMEvent(WindowState* window_state, const WMEvent* event) override {}
  chromeos::WindowStateType GetType() const override {
    return chromeos::WindowStateType::kDefault;
  }
  void AttachState(WindowState* window_state, State* previous_state) override {}
  void DetachState(WindowState* window_state) override {}
  void OnWindowDestroying(WindowState* window_state) override {}
};

void VerifySnappedBounds(aura::Window* window, float expected_snap_ratio) {
  const WindowState* window_state = WindowState::Get(window);
  // `window` must be in any snapped state to use this method.
  ASSERT_TRUE(window_state->IsSnapped());

  const bool in_tablet = display::Screen::GetScreen()->InTabletMode();
  const auto display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  const gfx::Rect work_area = display.work_area();
  const auto rotation = display.rotation();
  const bool is_primary =
      window_state->GetStateType() == WindowStateType::kPrimarySnapped;

  // Following conditions assume that the natural display orientation is
  // landscape.
  ASSERT_TRUE(chromeos::IsLandscapeOrientation(
      chromeos::GetDisplayNaturalOrientation(display)));
  const bool is_landscape = rotation == display::Display::ROTATE_0 ||
                            rotation == display::Display::ROTATE_180;
  const bool is_top_or_left =
      (rotation == display::Display::ROTATE_0 && is_primary) ||
      (rotation == display::Display::ROTATE_90 && !is_primary) ||
      (rotation == display::Display::ROTATE_180 && !is_primary) ||
      (rotation == display::Display::ROTATE_270 && is_primary);

  // Also consider the divider width if the window is in a snap group.
  const bool in_snap_group = [&]() {
    auto* snap_group_controller = SnapGroupController::Get();
    return snap_group_controller &&
           snap_group_controller->GetSnapGroupForGivenWindow(window);
  }();
  const int divider_margin =
      (in_tablet || in_snap_group) ? kSplitviewDividerShortSideLength / 2 : 0;
  const gfx::Size expected_size =
      is_landscape
          ? gfx::Size(work_area.width() * expected_snap_ratio - divider_margin,
                      work_area.height())
          : gfx::Size(
                work_area.width(),
                work_area.height() * expected_snap_ratio - divider_margin);
  const gfx::Point expected_origin =
      is_landscape
          ? gfx::Point(is_top_or_left
                           ? work_area.x()
                           : work_area.right() - expected_size.width(),
                       work_area.y())
          : gfx::Point(work_area.x(),
                       is_top_or_left
                           ? work_area.y()
                           : work_area.bottom() - expected_size.height());

  const gfx::Rect bounds = window->GetTargetBounds();
  // Allow 1px (3px in clamshell) rounding errors for partial snap. Note even if
  // `SnapGroup` is enabled, the window may not be in a snap group, so allow 3px
  // rounding errors.
  // TODO(b/319342277): Investigate why eps can't be 1 when clamshell mode.
  const int eps = in_tablet ? 1 : 3;
  EXPECT_NEAR(expected_size.width(), bounds.width(), is_landscape ? eps : 0);
  EXPECT_NEAR(expected_size.height(), bounds.height(), !is_landscape ? eps : 0);
  EXPECT_NEAR(expected_origin.x(), bounds.x(), is_landscape ? eps : 0);
  EXPECT_NEAR(expected_origin.y(), bounds.y(), !is_landscape ? eps : 0);
}

}  // namespace

class ClientControlledStateTest : public AshTestBase {
 public:
  ClientControlledStateTest() = default;

  ClientControlledStateTest(const ClientControlledStateTest&) = delete;
  ClientControlledStateTest& operator=(const ClientControlledStateTest&) =
      delete;

  ~ClientControlledStateTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    widget_delegate_ = new TestWidgetDelegate();

    views::Widget::InitParams params(
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
        desks_util::GetActiveDeskContainerId());
    params.bounds = kInitialBounds;
    params.delegate = widget_delegate_.get();

    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(params));
    WindowState* window_state = WindowState::Get(window());
    window_state->set_allow_set_bounds_direct(true);
    auto delegate = std::make_unique<TestClientControlledStateDelegate>();
    state_delegate_ = delegate.get();
    auto state = std::make_unique<ClientControlledState>(std::move(delegate));
    state_ = state.get();
    window_state->SetStateObject(std::move(state));
    auto window_state_delegate = std::make_unique<FakeWindowStateDelegate>();
    window_state_delegate_ = window_state_delegate.get();
    window_state->SetDelegate(std::move(window_state_delegate));
    widget_->Show();
  }

  void TearDown() override {
    widget_ = nullptr;
    AshTestBase::TearDown();
  }

  TestWidgetDelegate* widget_delegate() { return widget_delegate_; }

 protected:
  aura::Window* window() { return widget_->GetNativeWindow(); }
  WindowState* window_state() { return WindowState::Get(window()); }
  ClientControlledState* state() { return state_; }
  TestClientControlledStateDelegate* delegate() { return state_delegate_; }
  views::Widget* widget() { return widget_.get(); }
  ScreenPinningController* GetScreenPinningController() {
    return Shell::Get()->screen_pinning_controller();
  }
  FakeWindowStateDelegate* window_state_delegate() {
    return window_state_delegate_;
  }

  chromeos::HeaderView* GetHeaderView() {
    auto* const frame = NonClientFrameViewAsh::Get(window());
    DCHECK(frame);
    return frame->GetHeaderView();
  }
  void ApplyPendingRequestedBounds() {
    state()->set_bounds_locally(true);
    widget()->SetBounds(delegate()->requested_bounds());
    state()->set_bounds_locally(false);
  }
  void ClickOnOverviewItem(aura::Window* window) {
    auto* const overview_controller = OverviewController::Get();
    ASSERT_TRUE(overview_controller->InOverviewSession());
    auto* const overview_item = GetOverviewItemForWindow(window);

    auto* const event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
    event_generator->ClickLeftButton();
  }
  void SimulateUnminimizeViaShelfIcon(views::Widget* widget) {
    // When clicking an app icon on the hotseat to unminimize the window,
    // `ChromeShelfController` shows and activates the widget.
    // We here simulate the behavior because //ash should not use any component
    // from //chrome/browser/ui.
    widget->Show();
    widget->Activate();
  }
  void DragResizeSnappedWindow(aura::Window* window, int target_x) {
    ASSERT_TRUE(WindowState::Get(window)->IsSnapped());

    ui::test::EventGenerator* const generator = GetEventGenerator();
    const bool in_tablet = display::Screen::GetScreen()->InTabletMode();
    if (in_tablet) {
      auto* split_view_controller = SplitViewController::Get(window);
      const gfx::Rect divider_bounds =
          split_view_controller->split_view_divider()->GetDividerBoundsInScreen(
              false);
      generator->set_current_screen_location(divider_bounds.CenterPoint());
    } else {
      generator->set_current_screen_location(
          window->GetBoundsInScreen().right_center());
    }
    generator->DragMouseTo(gfx::Point(target_x, 0));
  }
  void DragOverviewItemToSnap(aura::Window* window, bool to_left) {
    auto* const overview_controller = OverviewController::Get();
    ASSERT_TRUE(overview_controller->InOverviewSession());

    auto* const overview_item = GetOverviewItemForWindow(window);
    auto* const event_generator = GetEventGenerator();
    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));

    const gfx::Rect work_area = display::Screen::GetScreen()
                                    ->GetDisplayNearestWindow(window)
                                    .work_area();
    event_generator->DragMouseTo(to_left ? work_area.left_center()
                                         : work_area.right_center());
  }

 private:
  raw_ptr<ClientControlledState, DanglingUntriaged> state_ = nullptr;
  raw_ptr<TestClientControlledStateDelegate, DanglingUntriaged>
      state_delegate_ = nullptr;
  raw_ptr<TestWidgetDelegate, DanglingUntriaged> widget_delegate_ =
      nullptr;  // owned by itself.
  raw_ptr<FakeWindowStateDelegate, DanglingUntriaged> window_state_delegate_ =
      nullptr;
  std::unique_ptr<views::Widget> widget_;
};

using SnapGroupClientControlledStateTest = ClientControlledStateTest;

// This suite runs test cases both in clamshell mode and tablet mode.
class ClientControlledStateTestClamshellAndTablet
    : public ClientControlledStateTest,
      public testing::WithParamInterface<bool> {
 public:
  ClientControlledStateTestClamshellAndTablet() = default;

  ClientControlledStateTestClamshellAndTablet(
      const ClientControlledStateTestClamshellAndTablet&) = delete;
  ClientControlledStateTestClamshellAndTablet& operator=(
      const ClientControlledStateTestClamshellAndTablet&) = delete;

  ~ClientControlledStateTestClamshellAndTablet() override = default;

  void SetUp() override {
    ClientControlledStateTest::SetUp();
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(InTabletMode());
  }

 protected:
  bool InTabletMode() { return GetParam(); }
};

// The parameter indicates whether the tablet mode is enabled.
INSTANTIATE_TEST_SUITE_P(All,
                         ClientControlledStateTestClamshellAndTablet,
                         testing::Bool());

TEST_F(ClientControlledStateTest, ClientControlledFlag) {
  ASSERT_TRUE(window_state()->is_client_controlled());

  // Attach `TestEmptyState` to detach `ClientControlledState`.
  window_state()->SetStateObject(std::make_unique<TestEmptyState>());
  EXPECT_FALSE(window_state()->is_client_controlled());

  // Attach `ClientControlledState` to detach `TestEmptyState`.
  window_state()->SetStateObject(std::make_unique<ClientControlledState>(
      std::make_unique<TestClientControlledStateDelegate>()));
  EXPECT_TRUE(window_state()->is_client_controlled());
}

// Make sure that calling Maximize()/Minimize()/Fullscreen() result in
// sending the state change request and won't change the state immediately.
// The state will be updated when ClientControlledState::EnterToNextState
// is called.
TEST_F(ClientControlledStateTest, Maximize) {
  widget()->Maximize();
  // The state shouldn't be updated until EnterToNextState is called.
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kMaximized, delegate()->new_state());
  // Now enters the new state.
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsMaximized());
  // Bounds is controlled by client.
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  // Maximized request should be also sent. It is up to client impl
  // how to handle it.
  widget()->SetBounds(gfx::Rect(0, 0, 100, 100));
  EXPECT_EQ(gfx::Rect(0, 0, 100, 100), delegate()->requested_bounds());

  widget()->Restore();
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kMaximized, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(ClientControlledStateTest, Minimize) {
  widget()->Minimize();
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kMinimized, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kMinimized, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  // use wm::Unminimize to unminimize.
  widget()->Minimize();
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kMinimized, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  ::wm::Unminimize(widget()->GetNativeWindow());
  EXPECT_TRUE(widget()->IsMinimized());
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal,
            widget()->GetNativeWindow()->GetProperty(
                aura::client::kRestoreShowStateKey));
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kMinimized, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_FALSE(widget()->IsMinimized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(ClientControlledStateTest, Fullscreen) {
  widget()->SetFullscreen(true);
  EXPECT_FALSE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kFullscreen, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->SetFullscreen(false);
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(WindowStateType::kFullscreen, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_FALSE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

// Make sure toggle fullscreen from maximized state goes back to
// maximized state.
TEST_F(ClientControlledStateTest, MaximizeToFullscreen) {
  widget()->Maximize();
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kMaximized, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->SetFullscreen(true);
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kMaximized, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kFullscreen, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->SetFullscreen(false);
  EXPECT_TRUE(widget()->IsFullscreen());
  EXPECT_EQ(WindowStateType::kFullscreen, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kMaximized, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_TRUE(widget()->IsMaximized());
  EXPECT_EQ(WindowStateType::kMaximized, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_FALSE(widget()->IsMaximized());
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(ClientControlledStateTest, IgnoreWorkspace) {
  widget()->Maximize();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsMaximized());
  delegate()->Reset();

  UpdateDisplay("1000x800");

  // Client is responsible to handle workspace change, so
  // no action should be taken.
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->new_state());
  EXPECT_EQ(gfx::Rect(), delegate()->requested_bounds());
}

TEST_F(ClientControlledStateTest, SetBounds) {
  constexpr gfx::Rect new_bounds(100, 100, 100, 100);
  widget()->SetBounds(new_bounds);
  EXPECT_EQ(kInitialBounds, widget()->GetWindowBoundsInScreen());
  EXPECT_EQ(new_bounds, delegate()->requested_bounds());
  state()->set_bounds_locally(true);
  widget()->SetBounds(delegate()->requested_bounds());
  state()->set_bounds_locally(false);
  EXPECT_EQ(new_bounds, widget()->GetWindowBoundsInScreen());
}

TEST_F(ClientControlledStateTest, CenterWindow) {
  display::Screen* screen = display::Screen::GetScreen();
  const gfx::Rect bounds = screen->GetPrimaryDisplay().work_area();

  gfx::Rect center_bounds = bounds;
  center_bounds.ClampToCenteredSize(window()->bounds().size());
  window()->SetBoundsInScreen(center_bounds, screen->GetPrimaryDisplay());
  EXPECT_NEAR(bounds.CenterPoint().x(),
              delegate()->requested_bounds().CenterPoint().x(), 1);
  EXPECT_NEAR(bounds.CenterPoint().y(),
              delegate()->requested_bounds().CenterPoint().y(), 1);
}

TEST_F(ClientControlledStateTest, CycleSnapWindow) {
  // Snap disabled.
  ASSERT_FALSE(window_state()->CanResize());
  ASSERT_FALSE(window_state()->CanSnap());

  // The event should be ignored.
  const WindowSnapWMEvent snap_left_event(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_left_event);
  EXPECT_FALSE(window_state()->IsSnapped());
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());

  const WindowSnapWMEvent snap_right_event(WM_EVENT_CYCLE_SNAP_SECONDARY);
  window_state()->OnWMEvent(&snap_right_event);
  EXPECT_FALSE(window_state()->IsSnapped());
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());

  // Snap enabled.
  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  window_state()->OnWMEvent(&snap_left_event);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_NE(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(kInitialBounds, window()->GetTargetBounds());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  delegate()->Reset();

  window_state()->OnWMEvent(&snap_right_event);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_NE(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
}

// Tests the entry point via selecting a window from partial overview.
TEST_F(SnapGroupClientControlledStateTest, SelectFromOverviewEntryPoint) {
  UpdateDisplay("800x600");

  // Set the client-controlled window app type so it can be recognized in
  // `GetActiveDeskAppWindowsInZOrder()`.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);

  // Create at least 1 other app window so we can start faster splitview.
  widget_delegate()->EnableSnap();
  auto non_client_controlled_window = CreateAppWindow();

  // Snap the client-controlled window using a snap action source that can start
  // faster splitview. Note `SnapOneTestWindow()` would not work here since it
  // expects the state type to be updated immediately.
  const WindowSnapWMEvent snap_primary_event(
      WM_EVENT_SNAP_PRIMARY, chromeos::kDefaultSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  window_state()->OnWMEvent(&snap_primary_event);

  // Apply pending requests.
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);

  // Test we start faster splitview, then select the normal window.
  VerifySplitViewOverviewSession(window());
  ClickOnOverviewItem(non_client_controlled_window.get());
  EXPECT_EQ(
      WindowStateType::kSecondarySnapped,
      WindowState::Get(non_client_controlled_window.get())->GetStateType());

  // Apply pending bounds changes and verify the state doesn't change.
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  // Test a snap group is created.
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(
      window(), non_client_controlled_window.get()));
  UnionBoundsEqualToWorkAreaBounds(
      snap_group_controller->GetSnapGroupForGivenWindow(window()));
}

// Tests the entry point via auto grouping on window snapped.
TEST_F(SnapGroupClientControlledStateTest, AutoGroupEntryPoint) {
  UpdateDisplay("800x600");

  // Set the client-controlled window app type so it can be recognized in
  // `GetActiveDeskAppWindowsInZOrder()`.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  widget_delegate()->EnableSnap();

  // Snap the client-controlled window. Since it's the only window, we don't
  // start faster splitview.
  const WindowSnapWMEvent snap_primary_event(
      WM_EVENT_SNAP_PRIMARY, chromeos::kDefaultSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  window_state()->OnWMEvent(&snap_primary_event);

  // Apply pending requests.
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  VerifyNotSplitViewOrOverviewSession(window());

  // Open a normal window, then snap it to the opposite side of `window()`.
  auto non_client_controlled_window = CreateAppWindow();
  SnapOneTestWindow(non_client_controlled_window.get(),
                    WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);

  // Apply pending bounds changes and verify the state doesn't change.
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  // Test a snap group is created.
  auto* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(
      window(), non_client_controlled_window.get()));
  UnionBoundsEqualToWorkAreaBounds(
      snap_group_controller->GetSnapGroupForGivenWindow(window()));
}

// Tests basic snap group divider resizing.
TEST_F(SnapGroupClientControlledStateTest, ResizeViaDivider) {
  UpdateDisplay("900x600");
  // Create a snap group with a client-controlled and normal state window.
  widget_delegate()->EnableSnap();
  auto non_client_controlled_window = CreateAppWindow();
  SnapOneTestWindow(non_client_controlled_window.get(),
                    WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(non_client_controlled_window.get());
  ClickOnOverviewItem(window());

  // Apply pending requests.
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  auto* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(window());
  ASSERT_TRUE(snap_group);
  auto* snap_group_divider = snap_group->snap_group_divider();

  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);

  // Start a drag on the divider.
  auto* event_generator = GetEventGenerator();

  // Resize to arbitrary locations with the divider.
  for (const float target_width : {300, 450, 600}) {
    const gfx::Point divider_center(snap_group_divider
                                        ->GetDividerBoundsInScreen(
                                            /*is_dragging=*/false)
                                        .CenterPoint());
    event_generator->MoveMouseTo(divider_center);
    event_generator->PressLeftButton();
    const gfx::Rect bounds_before_resizing(delegate()->requested_bounds());
    delegate()->set_bounds_request_callback(
        base::BindLambdaForTesting([&](const gfx::Rect& bounds) {
          if (bounds == bounds_before_resizing) {
            return;
          }
          // When any new bounds is requested, `OnDragStarted()` should be
          // called already.
          EXPECT_TRUE(window_state_delegate()->drag_in_progress());
          EXPECT_TRUE(window_state()->drag_details()->bounds_change &
                      WindowResizer::kBoundsChange_Resizes);
          delegate()->set_bounds_request_callback(base::NullCallback());
        }));
    ApplyPendingRequestedBounds();

    // Resize with at least 2 steps to simulate the real CUJ of dragging the
    // mouse. The default test EventGenerator sends only the start and end
    // points which is an abrupt jump between points.
    event_generator->MoveMouseTo(gfx::Point(target_width, divider_center.y()),
                                 /*count=*/2);
    ASSERT_TRUE(snap_group_divider->is_resizing_with_divider());
    EXPECT_TRUE(window_state_delegate()->drag_in_progress());
    EXPECT_TRUE(window_state()->drag_details()->bounds_change &
                WindowResizer::kBoundsChange_Resizes);

    // Apply pending requests.
    ApplyPendingRequestedBounds();
    const float expected_snap_ratio = target_width / 900;
    VerifySnappedBounds(window(), expected_snap_ratio);
    EXPECT_NEAR(target_width, window()->GetTargetBounds().width(),
                /*abs_error=*/kSplitviewDividerShortSideLength / 2);
    event_generator->ReleaseLeftButton();

    VerifySnappedBounds(window(), expected_snap_ratio);
    // The following drag info is used by client to determine how to handle the
    // bounds change.
    EXPECT_FALSE(window_state_delegate()->drag_in_progress());
  }
}

// Tests the basic functionalities of snap-to-replace.
TEST_F(SnapGroupClientControlledStateTest, SnapToReplace) {
  // Create a snap group with 2 normal windows.
  auto w1 = CreateAppWindow();
  auto w2 = CreateAppWindow();
  SnapOneTestWindow(w1.get(), WindowStateType::kPrimarySnapped,
                    chromeos::kDefaultSnapRatio);
  SnapOneTestWindow(w2.get(), WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio);
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));

  // Snap `window()` on top of `w1`.
  widget_delegate()->EnableSnap();
  const WindowSnapWMEvent snap_primary_event(
      WM_EVENT_SNAP_PRIMARY, chromeos::kDefaultSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  window_state()->OnWMEvent(&snap_primary_event);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  // Test it replaces `w1` in the group.
  EXPECT_FALSE(
      snap_group_controller->AreWindowsInSnapGroup(w1.get(), w2.get()));
  EXPECT_TRUE(snap_group_controller->AreWindowsInSnapGroup(window(), w2.get()));
}

// Tests that double click on the divider swaps the windows.
TEST_F(SnapGroupClientControlledStateTest, DoubleClickToSwap) {
  // Create a snap group.
  widget_delegate()->EnableSnap();
  auto non_client_controlled_window = CreateAppWindow();
  SnapOneTestWindow(non_client_controlled_window.get(),
                    WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(non_client_controlled_window.get());
  ClickOnOverviewItem(window());
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(
      window(), non_client_controlled_window.get()));
  auto* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(window());
  ASSERT_TRUE(snap_group);
  EXPECT_EQ(window(), snap_group->window1());
  EXPECT_EQ(non_client_controlled_window.get(), snap_group->window2());
  UnionBoundsEqualToWorkAreaBounds(snap_group);

  // Double click on the divider.
  const gfx::Rect divider_bounds(
      snap_group->snap_group_divider()->GetDividerBoundsInScreen(
          /*is_dragging=*/false));
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(divider_bounds.CenterPoint());
  event_generator->DoubleClickLeftButton();

  // Apply pending requests.
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());

  // Test the state types and windows are swapped.
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(
      window(), non_client_controlled_window.get()));
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  EXPECT_EQ(
      WindowStateType::kPrimarySnapped,
      WindowState::Get(non_client_controlled_window.get())->GetStateType());
  EXPECT_EQ(non_client_controlled_window.get(), snap_group->window1());
  EXPECT_EQ(window(), snap_group->window2());

  // TODO(b/352621475): Verify `UnionBoundsEqualToWorkAreaBounds()`. Currently
  // there may be a 1-px overlap, likely due to rounding.
}

// Tests the snap group window bounds are correct after minimize then
// unminimize.
TEST_F(SnapGroupClientControlledStateTest, SnapThenMinimize) {
  UpdateDisplay("800x600");

  // Create a snap group with a client-controlled and normal state window.
  widget_delegate()->EnableSnap();
  auto non_client_controlled_window = CreateAppWindow();
  SnapOneTestWindow(non_client_controlled_window.get(),
                    WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(non_client_controlled_window.get());
  ClickOnOverviewItem(window());

  // Apply pending requests. Test the bounds are at 1/2.
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(
      window(), non_client_controlled_window.get()));
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);

  // Minimize the client-controlled window.
  window_state()->Minimize();
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());

  // Test the group is broken.
  ASSERT_FALSE(snap_group_controller->AreWindowsInSnapGroup(
      window(), non_client_controlled_window.get()));

  // Unminimize the client-controlled window. Test the bounds are back at 1/2.
  window_state()->Unminimize();
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
}

// Tests that a client-controlled window in a snap group, when snapped to the
// opposite side, will set the correct bounds. Regression test for
// http://b/349774996.
TEST_F(SnapGroupClientControlledStateTest, SnapToOppositeSide) {
  UpdateDisplay("800x600");

  // Create a snap group with a client-controlled and normal state window.
  widget_delegate()->EnableSnap();
  auto non_client_controlled_window = CreateAppWindow();
  SnapOneTestWindow(non_client_controlled_window.get(),
                    WindowStateType::kSecondarySnapped,
                    chromeos::kDefaultSnapRatio,
                    WindowSnapActionSource::kSnapByWindowLayoutMenu);
  VerifySplitViewOverviewSession(non_client_controlled_window.get());
  ClickOnOverviewItem(window());

  // Apply pending requests. Test the bounds are at 1/2.
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  ASSERT_TRUE(snap_group_controller->AreWindowsInSnapGroup(
      window(), non_client_controlled_window.get()));
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kDefaultSnapRatio);
  auto* snap_group =
      snap_group_controller->GetSnapGroupForGivenWindow(window());
  ASSERT_TRUE(snap_group);
  UnionBoundsEqualToWorkAreaBounds(window(), non_client_controlled_window.get(),
                                   snap_group->snap_group_divider());

  // Snap to secondary 1/3.
  const WindowSnapWMEvent snap_partial_secondary(
      WM_EVENT_SNAP_SECONDARY, chromeos::kOneThirdSnapRatio,
      WindowSnapActionSource::kSnapByWindowLayoutMenu);
  window_state()->OnWMEvent(&snap_partial_secondary);

  // Apply pending requests. Test the bounds are at 1/3.
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
}

TEST_P(ClientControlledStateTestClamshellAndTablet, SnapWindow) {
  // Snap disabled.
  ASSERT_FALSE(window_state()->CanResize());
  ASSERT_FALSE(window_state()->CanSnap());

  // The event should be ignored.
  const WindowSnapWMEvent snap_primary_event(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_primary_event);
  EXPECT_FALSE(window_state()->IsSnapped());
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());

  const WindowSnapWMEvent snap_secondary_event(WM_EVENT_SNAP_SECONDARY);
  window_state()->OnWMEvent(&snap_secondary_event);
  EXPECT_FALSE(window_state()->IsSnapped());
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());

  // Snap enabled.
  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  // Snap to primary.
  window_state()->OnWMEvent(&snap_primary_event);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_NE(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(kInitialBounds, window()->GetTargetBounds());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  delegate()->Reset();

  // Snap to secondary.
  window_state()->OnWMEvent(&snap_secondary_event);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_NE(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
}

TEST_P(ClientControlledStateTestClamshellAndTablet, PartialSnap) {
  // Snap enabled.
  widget_delegate()->EnableSnap();

  // Test that snap from half to partial works.
  const WindowSnapWMEvent snap_left_half(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_left_half);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_NE(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(kInitialBounds, window()->GetTargetBounds());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  const WindowSnapWMEvent snap_left_partial(WM_EVENT_SNAP_PRIMARY,
                                            chromeos::kTwoThirdSnapRatio);
  window_state()->OnWMEvent(&snap_left_partial);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kTwoThirdSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  // Test that snap from primary to secondary works.
  const WindowSnapWMEvent snap_right_half(WM_EVENT_SNAP_SECONDARY);
  window_state()->OnWMEvent(&snap_right_half);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_NE(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
  VerifySnappedBounds(window(), chromeos::kTwoThirdSnapRatio);

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());

  const WindowSnapWMEvent snap_right_partial(WM_EVENT_SNAP_SECONDARY,
                                             chromeos::kOneThirdSnapRatio);
  window_state()->OnWMEvent(&snap_right_partial);
  // No actual state/bounds should be changed until the client applies the
  // changes.
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
}

TEST_F(ClientControlledStateTest, SnapInSecondaryDisplay) {
  UpdateDisplay("800x600, 600x500");
  widget()->SetBounds(gfx::Rect(800, 0, 100, 200));

  display::Screen* screen = display::Screen::GetScreen();

  const int64_t second_display_id = screen->GetAllDisplays()[1].id();
  EXPECT_EQ(second_display_id, screen->GetDisplayNearestWindow(window()).id());

  widget_delegate()->EnableSnap();

  // Make sure the requested bounds for snapped window is local to display.
  const WindowSnapWMEvent snap_left_event(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_left_event);

  EXPECT_EQ(second_display_id, delegate()->display_id());
  EXPECT_EQ(gfx::Rect(0, 0, 300, 500 - ShelfConfig::Get()->shelf_size()),
            delegate()->requested_bounds());

  state()->EnterNextState(window_state(), delegate()->new_state());
  // Make sure moving to another display tries to update the bounds.
  auto first_display = screen->GetAllDisplays()[0];
  delegate()->Reset();
  state()->set_bounds_locally(true);
  window()->SetBoundsInScreen(delegate()->requested_bounds(), first_display);
  state()->set_bounds_locally(false);
  EXPECT_EQ(first_display.id(), delegate()->display_id());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 600 - ShelfConfig::Get()->shelf_size()),
            delegate()->requested_bounds());
}

TEST_P(ClientControlledStateTestClamshellAndTablet, SnapMinimizeAndUnminimize) {
  UpdateDisplay("900x600");
  widget_delegate()->EnableSnap();

  const WindowSnapWMEvent snap_left_event(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_left_event);

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);

  const float target_width = 300;
  const float expected_snap_ratio = target_width / 900;
  DragResizeSnappedWindow(window(), target_width);

  // Apply pending requests.
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), expected_snap_ratio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  // Minimize.
  widget()->Minimize();
  state()->EnterNextState(window_state(), delegate()->new_state());

  // Unminimize via the `Unminimize` method.
  ::wm::Unminimize(widget()->GetNativeWindow());

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), expected_snap_ratio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());

  // Minimize again.
  widget()->Minimize();
  state()->EnterNextState(window_state(), delegate()->new_state());

  // Unminimize via drag-to-snap to the opposite side.
  if (!InTabletMode()) {
    ToggleOverview();
  }
  DragOverviewItemToSnap(window(), /*to_left=*/false);

  // The client may activate the widget before accepting the snap request.
  widget()->Activate();

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());

  // Minimize again.
  widget()->Minimize();
  state()->EnterNextState(window_state(), delegate()->new_state());

  // Unminimize via overview mode.
  if (!InTabletMode()) {
    ToggleOverview();
  }
  ClickOnOverviewItem(window());

  // The client may activate the widget before accepting the snap request.
  widget()->Activate();

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());

  // Minimize again.
  widget()->Minimize();
  state()->EnterNextState(window_state(), delegate()->new_state());

  // Unminimize via shelf icon.
  SimulateUnminimizeViaShelfIcon(widget());

  // The client may activate the widget before accepting the snap request.
  widget()->Activate();

  // Apply pending requests.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
}

// Tests that auto snapping from maximized/minimized via overview/shelf works
// for ClientControlledState.
TEST_F(ClientControlledStateTest, AutoSnap) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Snap enabled.
  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  // Create a normal (non-client-controlled) window in addition to `window()`
  // (client-controlled window) to fill the one side of the split view.
  auto non_client_controlled_window = CreateAppWindow();

  // Snap `non_client_controlled_window` to left.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  WindowState::Get(non_client_controlled_window.get())
      ->OnWMEvent(&snap_primary);

  // Click `window()`'s overview item to snap to right.
  ClickOnOverviewItem(window());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kDefaultSnapRatio);

  // Minimize `window()`.
  const WMEvent minimize(WM_EVENT_MINIMIZE);
  window_state()->OnWMEvent(&minimize);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsMinimized());
  EXPECT_FALSE(window()->IsVisible());

  // Click `window()`'s overview item to snap to right.
  ClickOnOverviewItem(window());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kDefaultSnapRatio);

  // Minimize `window()`.
  window_state()->OnWMEvent(&minimize);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsMinimized());
  EXPECT_FALSE(window()->IsVisible());

  // Unminimize `window()` by clicking the app icon on the shelf.
  SimulateUnminimizeViaShelfIcon(widget());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kDefaultSnapRatio);
}

// Tests that auto partial-snapping from maximized/minimized via overview/shelf
// works for ClientControlledState.
TEST_F(ClientControlledStateTest, AutoPartialSnap) {
  UpdateDisplay("900x600");
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Snap enabled.
  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  // Create a normal (non-client-controlled) window in addition to `window()`
  // (client-controlled window) to fill the one side of the split view.
  auto non_client_controlled_window = CreateAppWindow();

  // Snap `non_client_controlled_window` to 1/3 left.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY,
                                       chromeos::kOneThirdSnapRatio);
  WindowState::Get(non_client_controlled_window.get())
      ->OnWMEvent(&snap_primary);

  // Click `window()`'s overview item to snap to 2/3 right.
  ClickOnOverviewItem(window());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kTwoThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kOneThirdSnapRatio);

  // Minimize `window()`.
  const WMEvent minimize(WM_EVENT_MINIMIZE);
  window_state()->OnWMEvent(&minimize);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsMinimized());
  EXPECT_FALSE(window()->IsVisible());

  // Click `window()`'s overview item to snap to 2/3 right.
  ClickOnOverviewItem(window());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kTwoThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kOneThirdSnapRatio);

  // Minimize `window()`.
  window_state()->OnWMEvent(&minimize);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsMinimized());
  EXPECT_FALSE(window()->IsVisible());

  // Unminimize `window()` by clicking the app icon on the shelf.
  SimulateUnminimizeViaShelfIcon(widget());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kTwoThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kOneThirdSnapRatio);

  // Minimize `window()`.
  window_state()->OnWMEvent(&minimize);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsMinimized());
  EXPECT_FALSE(window()->IsVisible());

  // Resize `non_client_controlled_window` to 2/3 left.
  DragResizeSnappedWindow(non_client_controlled_window.get(), 600);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kTwoThirdSnapRatio);

  // Click `window()`'s overview item to snap to 1/3 right.
  ClickOnOverviewItem(window());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kTwoThirdSnapRatio);

  // Minimize `window()`.
  window_state()->OnWMEvent(&minimize);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsMinimized());
  EXPECT_FALSE(window()->IsVisible());

  // Resize `non_client_controlled_window` to 1/3 left.
  DragResizeSnappedWindow(non_client_controlled_window.get(), 300);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kOneThirdSnapRatio);

  // Unminimize `window()` by clicking the app icon on the shelf.
  SimulateUnminimizeViaShelfIcon(widget());

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  VerifySnappedBounds(window(), chromeos::kTwoThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kOneThirdSnapRatio);
}

TEST_P(ClientControlledStateTestClamshellAndTablet, SnapAndRotate) {
  // Rotation animation needs an internal display.
  const int64_t internal_display_id =
      display::test::DisplayManagerTestApi(display_manager())
          .SetFirstDisplayAsInternalDisplay();

  ScreenOrientationControllerTestApi orientation_test_api(
      Shell::Get()->screen_orientation_controller());
  // Snap enabled.
  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanSnap());

  for (const bool is_primary : {true, false}) {
    SCOPED_TRACE(::testing::Message() << "Testing in primary: " << is_primary);
    const auto target_state_type = is_primary
                                       ? WindowStateType::kPrimarySnapped
                                       : WindowStateType::kSecondarySnapped;
    for (const float snap_ratio :
         {chromeos::kDefaultSnapRatio, chromeos::kOneThirdSnapRatio,
          chromeos::kTwoThirdSnapRatio}) {
      SCOPED_TRACE(::testing::Message()
                   << "Testing in snap ratio: " << snap_ratio);
      const WindowSnapWMEvent snap_event(
          is_primary ? WM_EVENT_SNAP_PRIMARY : WM_EVENT_SNAP_SECONDARY,
          snap_ratio);
      window_state()->OnWMEvent(&snap_event);
      state()->EnterNextState(window_state(), delegate()->new_state());
      ApplyPendingRequestedBounds();
      VerifySnappedBounds(window(), snap_ratio);
      EXPECT_EQ(target_state_type, window_state()->GetStateType());

      for (const auto& rotation :
           {display::Display::ROTATE_90, display::Display::ROTATE_180,
            display::Display::ROTATE_270, display::Display::ROTATE_0}) {
        SCOPED_TRACE(::testing::Message()
                     << "Testing in rotation: "
                     << display::Display::RotationToDegrees(rotation));
        // Rotate the display.
        orientation_test_api.SetDisplayRotation(
            rotation, display::Display::RotationSource::USER);
        ASSERT_EQ(Shell::Get()
                      ->display_manager()
                      ->GetDisplayInfo(internal_display_id)
                      .GetActiveRotation(),
                  rotation);
        // Apply pending requests.
        state()->EnterNextState(window_state(), delegate()->new_state());
        ApplyPendingRequestedBounds();
        VerifySnappedBounds(window(), snap_ratio);
        EXPECT_EQ(target_state_type, window_state()->GetStateType());
      }
    }
  }
}

// Tests that resize-to-dismiss split view works for client-controlled windows.
TEST_F(ClientControlledStateTest, ResizeToDismissSplitView) {
  UpdateDisplay("900x600");
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  auto* const split_view_controller = SplitViewController::Get(window());

  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  // Create a normal (non-client-controlled) window in addition to `window()`
  // (client-controlled window) to fill the one side of the split view.
  auto non_client_controlled_window = CreateAppWindow();

  for (const bool resize_to_left : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "Testing in resize-to-left: " << resize_to_left);
    // Snap `non_client_controlled_window` to left.
    const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
    WindowState::Get(non_client_controlled_window.get())
        ->OnWMEvent(&snap_primary);
    // Snap `window()` to right.
    const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY);
    window_state()->OnWMEvent(&snap_secondary);

    state()->EnterNextState(window_state(), delegate()->new_state());
    ApplyPendingRequestedBounds();
    EXPECT_EQ(WindowStateType::kSecondarySnapped,
              window_state()->GetStateType());
    VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
    VerifySnappedBounds(non_client_controlled_window.get(),
                        chromeos::kDefaultSnapRatio);
    EXPECT_TRUE(split_view_controller->InSplitViewMode());

    views::test::WidgetDestroyedWaiter divider_destroyed_waiter(
        split_view_controller->split_view_divider()->divider_widget());

    // Move the divider to the left/right edge. It should dismiss the split view
    // and move the expanded window to front.
    DragResizeSnappedWindow(window(), resize_to_left ? 0 : 900);

    // Wait until the divider gets destroyed.
    divider_destroyed_waiter.Wait();

    state()->EnterNextState(window_state(), delegate()->new_state());
    ApplyPendingRequestedBounds();

    EXPECT_FALSE(split_view_controller->InSplitViewMode());
    EXPECT_TRUE(window_state()->IsMaximized());
    EXPECT_TRUE(window()->IsVisible());
    EXPECT_EQ(widget()->IsActive(), resize_to_left);
  }
}

// Tests that drag-caption-to-snap works for client-controlled windows. The
// order of emitted drag events and state change events matters for a client so
// this test strictly verifies the order of events.
TEST_F(ClientControlledStateTest, DragCaptionToSnap) {
  auto* const event_generator = GetEventGenerator();

  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  const gfx::Rect normal_state_bounds(200, 200, 400, 300);
  const SetBoundsWMEvent set_bounds_event(normal_state_bounds);
  window_state()->OnWMEvent(&set_bounds_event);
  ApplyPendingRequestedBounds();

  // First, tests that dragging the caption to snap to primary, and then tests
  // that dragging it to secondary.
  for (const auto target_state :
       {WindowStateType::kPrimarySnapped, WindowStateType::kSecondarySnapped}) {
    SCOPED_TRACE(::testing::Message()
                 << "Testing in drag-cation-to-snap: from "
                 << window_state()->GetStateType() << " to " << target_state);
    // Start dragging in the center of the header.
    auto* const header_view = GetHeaderView();
    gfx::Point next_cursor_point =
        header_view->GetBoundsInScreen().CenterPoint();
    event_generator->set_current_screen_location(next_cursor_point);
    event_generator->PressLeftButton();

    // Keep slightly (5px) dragging...
    delegate()->set_bounds_request_callback(
        base::BindLambdaForTesting([&](const gfx::Rect& bounds) {
          // When any new bounds is requested, `OnDragStarted()` should be
          // called already.
          EXPECT_TRUE(window_state_delegate()->drag_in_progress());
          EXPECT_TRUE(window_state()->drag_details()->bounds_change &
                      WindowResizer::kBoundsChange_Repositions);
        }));
    next_cursor_point.Offset(-5, 0);
    event_generator->MoveMouseTo(next_cursor_point);
    // The following drag info is used by client to determine how to handle the
    // bounds change.
    EXPECT_TRUE(window_state_delegate()->drag_in_progress());
    EXPECT_TRUE(window_state()->drag_details()->bounds_change &
                WindowResizer::kBoundsChange_Repositions);
    ApplyPendingRequestedBounds();
    delegate()->set_bounds_request_callback(base::NullCallback());

    // Drag it to the left edge of the screen.
    const gfx::Rect work_area =
        display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
    next_cursor_point = target_state == WindowStateType::kPrimarySnapped
                            ? work_area.left_center()
                            : work_area.right_center();
    event_generator->MoveMouseTo(next_cursor_point);
    delegate()->set_window_state_request_callback(
        base::BindLambdaForTesting([&](WindowStateType new_state) {
          if (new_state != target_state) {
            return;
          }
          // When a new state (i.e., snapped) is requested, `OnDragFinished()`
          // should be called already.
          EXPECT_FALSE(window_state_delegate()->drag_in_progress());
        }));
    event_generator->ReleaseLeftButton();
    // The following drag info is used by client to determine how to handle the
    // bounds change.
    EXPECT_FALSE(window_state_delegate()->drag_in_progress());

    // Accept the snap request.
    state()->EnterNextState(window_state(), delegate()->new_state());
    ApplyPendingRequestedBounds();
    VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
    EXPECT_EQ(target_state, window_state()->GetStateType());
  }
}

// Tests that drag-caption-to-unsnap works for client-controlled windows. The
// order of emitted drag events and state change events matters for a client so
// this test strictly verifies the order of events.
TEST_F(ClientControlledStateTest, DragCaptionToUnsnap) {
  auto* const event_generator = GetEventGenerator();

  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  // Snap `window()` to left.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_primary);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();

  // Start dragging in the center of the header.
  auto* const header_view = GetHeaderView();
  gfx::Point next_cursor_point = header_view->GetBoundsInScreen().CenterPoint();
  event_generator->set_current_screen_location(next_cursor_point);
  event_generator->PressLeftButton();

  // Keep slightly (5px) dragging...
  delegate()->set_bounds_request_callback(
      base::BindLambdaForTesting([&](const gfx::Rect& bounds) {
        // When any new bounds is requested, `OnDragStarted()` should be
        // called already.
        EXPECT_TRUE(window_state_delegate()->drag_in_progress());
        EXPECT_TRUE(window_state()->drag_details()->bounds_change &
                    WindowResizer::kBoundsChange_Repositions);
      }));
  next_cursor_point.Offset(5, 0);
  event_generator->MoveMouseTo(next_cursor_point);
  // The following drag info is used by client to determine how to handle the
  // bounds change.
  EXPECT_TRUE(window_state_delegate()->drag_in_progress());
  EXPECT_TRUE(window_state()->drag_details()->bounds_change &
              WindowResizer::kBoundsChange_Repositions);
  ApplyPendingRequestedBounds();
  delegate()->set_bounds_request_callback(base::NullCallback());

  // Drag it to the center of the screen.
  const auto work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  next_cursor_point = work_area.CenterPoint();
  event_generator->MoveMouseTo(next_cursor_point);
  delegate()->set_window_state_request_callback(
      base::BindLambdaForTesting([&](WindowStateType new_state) {
        if (new_state != chromeos::WindowStateType::kPrimarySnapped) {
          return;
        }
        // When a new state (i.e., normal) is requested, `OnDragFinished()`
        // should be called already.
        EXPECT_FALSE(window_state_delegate()->drag_in_progress());
      }));
  event_generator->ReleaseLeftButton();
  // The following drag info is used by client to determine how to handle the
  // bounds change.
  EXPECT_FALSE(window_state_delegate()->drag_in_progress());

  // Accept the restore request.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(chromeos::WindowStateType::kNormal, window_state()->GetStateType());
}

// Tests that swapping snapped windows works for client-controlled windows
TEST_F(ClientControlledStateTest, SwapSnappedWindows) {
  ShellTestApi().SetTabletModeEnabledForTest(true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  UpdateDisplay("900x600");
  auto* const split_view_controller = SplitViewController::Get(window());

  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  // Create a normal (non-client-controlled) window in addition to `window()`
  // (client-controlled window) to fill the one side of the split view.
  auto non_client_controlled_window = CreateAppWindow();
  auto* const non_client_controlled_window_state =
      WindowState::Get(non_client_controlled_window.get());

  // Snap `window()` to 1/3 left.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY,
                                       chromeos::kOneThirdSnapRatio);
  window_state()->OnWMEvent(&snap_primary);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();

  // Snap `non_client_controlled_window` to 2/3 right.
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY,
                                         chromeos::kTwoThirdSnapRatio);
  non_client_controlled_window_state->OnWMEvent(&snap_secondary);

  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            non_client_controlled_window_state->GetStateType());
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kTwoThirdSnapRatio);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Swap windows.
  split_view_controller->SwapWindows();

  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kPrimarySnapped,
            non_client_controlled_window_state->GetStateType());
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kTwoThirdSnapRatio);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
}

// Tests that to-tablet/clamshell conversion carries over the snapped ratio.
TEST_F(ClientControlledStateTest, ClamshellTabletConversionWithSnappedWindow) {
  UpdateDisplay("900x600");
  auto* const split_view_controller = SplitViewController::Get(window());

  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanResize());
  ASSERT_TRUE(window_state()->CanSnap());

  // The scenario starts in clamshell mode.
  ShellTestApi().SetTabletModeEnabledForTest(false);
  ASSERT_FALSE(display::Screen::GetScreen()->InTabletMode());

  // Create a normal (non-client-controlled) window in addition to `window()`
  // (client-controlled window) to fill the one side of the split view.
  auto non_client_controlled_window = CreateAppWindow();
  auto* const non_client_controlled_window_state =
      WindowState::Get(non_client_controlled_window.get());

  // Snap `window()` to 1/3 left.
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY,
                                       chromeos::kOneThirdSnapRatio);
  window_state()->OnWMEvent(&snap_primary);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();

  // Snap `non_client_controlled_window` to 2/3 right.
  const WindowSnapWMEvent snap_secondary(WM_EVENT_SNAP_SECONDARY,
                                         chromeos::kTwoThirdSnapRatio);
  non_client_controlled_window_state->OnWMEvent(&snap_secondary);

  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            non_client_controlled_window_state->GetStateType());
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kTwoThirdSnapRatio);
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Clamshell-to-tablet transition should carry over the bounds.
  ShellTestApi().SetTabletModeEnabledForTest(true);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            non_client_controlled_window_state->GetStateType());
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kTwoThirdSnapRatio);
  EXPECT_TRUE(split_view_controller->InSplitViewMode());

  // Tablet-to-clamshell transition should carry over the bounds.
  ShellTestApi().SetTabletModeEnabledForTest(false);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            non_client_controlled_window_state->GetStateType());
  VerifySnappedBounds(window(), chromeos::kOneThirdSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kTwoThirdSnapRatio);
  EXPECT_FALSE(split_view_controller->InSplitViewMode());
}

// Pin events should not be applied immediately. The request should be sent
// to delegate.
TEST_F(ClientControlledStateTest, Pinned) {
  ASSERT_FALSE(window_state()->IsPinned());
  ASSERT_FALSE(GetScreenPinningController()->IsPinned());

  const WMEvent pin_event(WM_EVENT_PIN);
  window_state()->OnWMEvent(&pin_event);
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kPinned, delegate()->new_state());

  state()->EnterNextState(window_state(), WindowStateType::kPinned);
  EXPECT_TRUE(window_state()->IsPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());
  EXPECT_EQ(WindowStateType::kPinned, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kPinned, delegate()->new_state());

  // All state transition events are ignored except for NORMAL.
  widget()->Maximize();
  EXPECT_EQ(WindowStateType::kPinned, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  widget()->Minimize();
  EXPECT_EQ(WindowStateType::kPinned, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());
  EXPECT_TRUE(window()->IsVisible());

  widget()->SetFullscreen(true);
  EXPECT_EQ(WindowStateType::kPinned, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  // WM/User cannot change the bounds of the pinned window.
  constexpr gfx::Rect new_bounds(100, 100, 200, 100);
  widget()->SetBounds(new_bounds);
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());
  // But client can change the bounds of the pinned window.
  state()->set_bounds_locally(true);
  widget()->SetBounds(new_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(new_bounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_TRUE(window_state()->IsPinned());
  EXPECT_EQ(WindowStateType::kPinned, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());
  state()->EnterNextState(window_state(), WindowStateType::kNormal);

  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_EQ(WindowStateType::kNormal, window_state()->GetStateType());
  EXPECT_FALSE(GetScreenPinningController()->IsPinned());

  // Two windows cannot be pinned simultaneously.
  auto widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  WindowState* window_state_2 = WindowState::Get(widget2->GetNativeWindow());
  window_state_2->OnWMEvent(&pin_event);
  EXPECT_TRUE(window_state_2->IsPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  // Pin request should fail.
  EXPECT_FALSE(window_state()->IsPinned());
  window_state()->OnWMEvent(&pin_event);
  EXPECT_NE(WindowStateType::kPinned, delegate()->new_state());
}

TEST_F(ClientControlledStateTest, TrustedPinnedBasic) {
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_FALSE(GetScreenPinningController()->IsPinned());

  const WMEvent trusted_pin_event(WM_EVENT_TRUSTED_PIN);
  window_state()->OnWMEvent(&trusted_pin_event);
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kTrustedPinned, delegate()->new_state());

  state()->EnterNextState(window_state(), WindowStateType::kTrustedPinned);
  EXPECT_TRUE(window_state()->IsPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  EXPECT_EQ(WindowStateType::kTrustedPinned, window_state()->GetStateType());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kTrustedPinned, delegate()->new_state());

  // All state transition events are ignored except for NORMAL.
  widget()->Maximize();
  EXPECT_EQ(WindowStateType::kTrustedPinned, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  widget()->Minimize();
  EXPECT_EQ(WindowStateType::kTrustedPinned, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());
  EXPECT_TRUE(window()->IsVisible());

  widget()->SetFullscreen(true);
  EXPECT_EQ(WindowStateType::kTrustedPinned, window_state()->GetStateType());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  // WM/User cannot change the bounds of the trusted-pinned window.
  constexpr gfx::Rect new_bounds(100, 100, 200, 100);
  widget()->SetBounds(new_bounds);
  EXPECT_TRUE(delegate()->requested_bounds().IsEmpty());
  // But client can change the bounds of the trusted-pinned window.
  state()->set_bounds_locally(true);
  widget()->SetBounds(new_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(new_bounds, widget()->GetWindowBoundsInScreen());

  widget()->Restore();
  EXPECT_TRUE(window_state()->IsPinned());
  EXPECT_EQ(WindowStateType::kTrustedPinned, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());
  state()->EnterNextState(window_state(), WindowStateType::kNormal);
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_EQ(WindowStateType::kNormal, window_state()->GetStateType());
  EXPECT_FALSE(GetScreenPinningController()->IsPinned());

  // Two windows cannot be trusted-pinned simultaneously.
  auto widget2 =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  WindowState* window_state_2 = WindowState::Get(widget2->GetNativeWindow());
  window_state_2->OnWMEvent(&trusted_pin_event);
  EXPECT_TRUE(window_state_2->IsTrustedPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());

  EXPECT_FALSE(window_state()->IsTrustedPinned());
  window_state()->OnWMEvent(&trusted_pin_event);
  EXPECT_NE(WindowStateType::kTrustedPinned, delegate()->new_state());
  EXPECT_TRUE(window_state_2->IsTrustedPinned());
}

TEST_F(ClientControlledStateTest, ClosePinned) {
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_FALSE(GetScreenPinningController()->IsPinned());

  const WMEvent trusted_pin_event(WM_EVENT_TRUSTED_PIN);
  window_state()->OnWMEvent(&trusted_pin_event);
  EXPECT_FALSE(window_state()->IsPinned());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kTrustedPinned, delegate()->new_state());
  state()->EnterNextState(window_state(), WindowStateType::kTrustedPinned);

  EXPECT_TRUE(window_state()->IsPinned());
  EXPECT_TRUE(GetScreenPinningController()->IsPinned());
  delegate()->mark_as_deleted();
  widget()->CloseNow();
}

TEST_F(ClientControlledStateTest, MoveWindowToDisplay) {
  UpdateDisplay("600x500, 600x500");

  display::Screen* screen = display::Screen::GetScreen();

  const int64_t first_display_id = screen->GetAllDisplays()[0].id();
  const int64_t second_display_id = screen->GetAllDisplays()[1].id();
  EXPECT_EQ(first_display_id, screen->GetDisplayNearestWindow(window()).id());

  window_util::MoveWindowToDisplay(window(), second_display_id);

  // Make sure that the boundsChange request has correct destination
  // information.
  EXPECT_EQ(second_display_id, delegate()->display_id());
  EXPECT_EQ(window()->bounds(), delegate()->requested_bounds());
}

TEST_F(ClientControlledStateTest, MoveWindowToDisplayOutOfBounds) {
  UpdateDisplay("1000x500, 600x500");

  state()->set_bounds_locally(true);
  constexpr int kWidth = 100;
  widget()->SetBounds(gfx::Rect(700, 0, kWidth, 200));
  state()->set_bounds_locally(false);
  EXPECT_EQ(gfx::Rect(700, 0, kWidth, 200),
            widget()->GetWindowBoundsInScreen());

  display::Screen* screen = display::Screen::GetScreen();

  const int64_t first_display_id = screen->GetAllDisplays()[0].id();
  const int64_t second_display_id = screen->GetAllDisplays()[1].id();
  EXPECT_EQ(first_display_id, screen->GetDisplayNearestWindow(window()).id());

  window_util::MoveWindowToDisplay(window(), second_display_id);

  // Make sure that the boundsChange request has correct destination
  // information.
  EXPECT_EQ(second_display_id, delegate()->display_id());
  // The bounds is constrained by
  // |AdjustBoundsToEnsureMinimumWindowVisibility| in the secondary
  // display.
  constexpr int kMinVisibleWidth = kWidth * kMinimumPercentOnScreenArea;
  EXPECT_EQ(gfx::Rect(600 - kMinVisibleWidth, 0, kWidth, 200),
            delegate()->requested_bounds());
}

// Make sure disconnecting primary notifies the display id change.
TEST_F(ClientControlledStateTest, DisconnectPrimary) {
  UpdateDisplay("600x500,600x500");
  SwapPrimaryDisplay();
  auto* screen = display::Screen::GetScreen();
  auto old_primary_id = screen->GetPrimaryDisplay().id();
  EXPECT_EQ(old_primary_id, window_state()->GetDisplay().id());
  gfx::Rect bounds = window()->bounds();

  UpdateDisplay("600x500");
  ASSERT_NE(old_primary_id, screen->GetPrimaryDisplay().id());
  EXPECT_EQ(delegate()->display_id(), screen->GetPrimaryDisplay().id());
  EXPECT_EQ(bounds, delegate()->requested_bounds());
}

TEST_F(ClientControlledStateTest,
       WmEventNormalIsResolvedToMaximizeInTabletMode) {
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());
  window_state()->window()->SetProperty(
      aura::client::kResizeBehaviorKey,
      aura::client::kResizeBehaviorCanMaximize);

  const WMEvent normal_event(WM_EVENT_NORMAL);
  window_state()->OnWMEvent(&normal_event);

  EXPECT_EQ(WindowStateType::kMaximized, delegate()->new_state());
}

TEST_F(ClientControlledStateTest,
       IgnoreWmEventWhenWindowIsInTransitionalSnappedState) {
  auto* split_view_controller =
      SplitViewController::Get(window_state()->window());

  widget_delegate()->EnableSnap();
  split_view_controller->SnapWindow(window_state()->window(),
                                    SnapPosition::kSecondary);

  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
  EXPECT_FALSE(window_state()->IsSnapped());

  // Ensures the window is in a transitional snapped state.
  EXPECT_TRUE(split_view_controller->IsWindowInTransitionalState(
      window_state()->window()));
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
  EXPECT_FALSE(window_state()->IsSnapped());

  // Ignores WMEvent if in a transitional state.
  widget()->Maximize();
  EXPECT_NE(WindowStateType::kMaximized, delegate()->new_state());

  // Applies snap request.
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsSnapped());

  // After exiting the transitional state, works normally.
  widget()->Maximize();
  EXPECT_EQ(WindowStateType::kMaximized, delegate()->new_state());
}

TEST_P(ClientControlledStateTestClamshellAndTablet, ResizeSnappedWindow) {
  // Set screen width.
  UpdateDisplay("1200x600");

  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            GetCurrentScreenOrientation());

  // Snap a window
  widget_delegate()->EnableSnap();
  const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_primary);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_TRUE(window_state()->IsSnapped());
  const gfx::Rect bounds_before_resizing(delegate()->requested_bounds());

  // Start drag-resizing from the center point of the work area.
  auto* const event_generator = GetEventGenerator();
  gfx::Point next_cursor_point = display::Screen::GetScreen()
                                     ->GetPrimaryDisplay()
                                     .work_area()
                                     .CenterPoint();
  event_generator->set_current_screen_location(next_cursor_point);
  event_generator->PressLeftButton();
  // Test the requested bounds do not change.
  EXPECT_EQ(bounds_before_resizing, delegate()->requested_bounds());

  // Keep dragging...
  delegate()->set_bounds_request_callback(
      base::BindLambdaForTesting([&](const gfx::Rect& bounds) {
        if (bounds == bounds_before_resizing) {
          return;
        }
        // When any new bounds is requested, `OnDragStarted()` should be called
        // already.
        EXPECT_TRUE(window_state_delegate()->drag_in_progress());
        EXPECT_TRUE(window_state()->drag_details()->bounds_change &
                    WindowResizer::kBoundsChange_Resizes);
      }));
  next_cursor_point.Offset(-50, 0);
  event_generator->MoveMouseTo(next_cursor_point);
  // The following drag info is used by client to determine how to handle the
  // bounds change.
  EXPECT_TRUE(window_state_delegate()->drag_in_progress());
  EXPECT_TRUE(window_state()->drag_details()->bounds_change &
              WindowResizer::kBoundsChange_Resizes);
  ApplyPendingRequestedBounds();
  delegate()->set_bounds_request_callback(base::NullCallback());

  // Drag to 1/3 (i.e. make the width 400).
  const float target_width = 400;
  next_cursor_point.set_x(target_width);
  event_generator->MoveMouseTo(next_cursor_point);
  event_generator->ReleaseLeftButton();
  // The following drag info is used by client to determine how to handle the
  // bounds change.
  EXPECT_FALSE(window_state_delegate()->drag_in_progress());

  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), target_width / 1200);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  // Changing display size should keep the current snap ratio.
  UpdateDisplay("900x600");
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), target_width / 1200);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
}

// Tests that a window leaves the snapped state when the client sets a new
// window state.
TEST_P(ClientControlledStateTestClamshellAndTablet,
       LeaveSnappedStateByNewStateChange) {
  auto* const split_view_controller = SplitViewController::Get(window());
  widget_delegate()->EnableSnap();

  for (const auto new_state_type :
       {WindowStateType::kMaximized, WindowStateType::kFullscreen}) {
    // Snap a window.
    const WindowSnapWMEvent snap_primary(WM_EVENT_SNAP_PRIMARY);
    window_state()->OnWMEvent(&snap_primary);
    state()->EnterNextState(window_state(), delegate()->new_state());
    ApplyPendingRequestedBounds();
    if (InTabletMode()) {
      EXPECT_TRUE(split_view_controller->InSplitViewMode());
    }
    EXPECT_EQ(window_state()->GetStateType(), WindowStateType::kPrimarySnapped);

    // The client sets a new state.
    state()->EnterNextState(window_state(), new_state_type);
    ApplyPendingRequestedBounds();
    if (InTabletMode()) {
      EXPECT_FALSE(split_view_controller->InSplitViewMode());
    }
    EXPECT_EQ(window_state()->GetStateType(), new_state_type);
  }
}

TEST_F(ClientControlledStateTest, FlingFloatedWindowInTabletMode) {
  // The AppType must be set to any except `chromeos::AppType::NON_APP` (default
  // value) to make it floatable.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  widget_delegate()->EnableFloat();
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Enter tablet mode
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Float window.
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state()->OnWMEvent(&float_event);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_TRUE(window_state()->IsFloated());
  EXPECT_EQ(kShellWindowId_FloatContainer, window()->parent()->GetId());

  // Start dragging in the center of the header and fling it to the top left.
  const auto initial_bounds = delegate()->requested_bounds();
  auto* const header_view = GetHeaderView();
  auto* const event_generator = GetEventGenerator();
  const auto start = header_view->GetBoundsInScreen().CenterPoint();
  const gfx::Vector2d offset(-20, -20);

  EXPECT_FALSE(window_state_delegate()->drag_in_progress());
  event_generator->GestureScrollSequenceWithCallback(
      start, start + offset, base::Milliseconds(10), /*steps=*/2,
      base::BindLambdaForTesting(
          [&](ui::EventType event_type, const gfx::Vector2dF& delta) {
            if (event_type != ui::EventType::kGestureScrollUpdate) {
              return;
            }
            EXPECT_TRUE(window_state_delegate()->drag_in_progress());
          }));
  EXPECT_FALSE(window_state_delegate()->drag_in_progress());

  // In tablet mode, `FloatController` magnetize the window so the
  // drag-to-top-left operation should result in placing the window at the top
  // left with padding.
  const int padding = chromeos::wm::kFloatedWindowPaddingDp;
  EXPECT_EQ(delegate()->requested_bounds(),
            gfx::Rect(gfx::Point(padding, padding), initial_bounds.size()));
}

TEST_F(ClientControlledStateTest, TuckAndUntuckFloatedWindowInTabletMode) {
  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NORMAL_DURATION);
  // This test checks the window animation state, but not interested in the
  // animation by the education.
  FloatTestApi::ScopedTuckEducationDisabler scoped_tuck_education_disabler;

  auto* const float_controller = Shell::Get()->float_controller();

  // The AppType must be set to any except `chromeos::AppType::NON_APP` (default
  // value) to make it floatable.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  widget_delegate()->EnableFloat();
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Enter tablet mode
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(display::Screen::GetScreen()->InTabletMode());

  // Float window.
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state()->OnWMEvent(&float_event);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_TRUE(window_state()->IsFloated());
  EXPECT_EQ(kShellWindowId_FloatContainer, window()->parent()->GetId());

  // Test tucking.
  // Start dragging in the center of the header and fling it to offscreen.
  auto* const header_view = GetHeaderView();
  auto* const event_generator = GetEventGenerator();
  const gfx::Point start = header_view->GetBoundsInScreen().CenterPoint();
  const gfx::Vector2d offset(10, 10);

  event_generator->GestureScrollSequence(start, start + offset,
                                         base::Milliseconds(10), /*steps=*/1);
  EXPECT_TRUE(window()->layer()->GetAnimator()->is_animating());

  // Client-requested bounds change should be blocked while animating.
  const auto start_bounds = window()->GetBoundsInScreen();
  const gfx::Rect client_requested_bounds(0, 0, 256, 256);
  state()->set_bounds_locally(true);
  widget()->SetBounds(client_requested_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(window()->GetBoundsInScreen(), start_bounds);

  EXPECT_TRUE(window()->IsVisible());
  ShellTestApi().WaitForWindowFinishAnimating(window());
  EXPECT_FALSE(window()->IsVisible());
  EXPECT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window()));
  EXPECT_FALSE(window()->layer()->GetAnimator()->is_animating());

  // Bounds change should be blocked while tucked.
  const auto tucked_bounds = window()->GetBoundsInScreen();
  state()->set_bounds_locally(true);
  widget()->SetBounds(client_requested_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(window()->GetBoundsInScreen(), tucked_bounds);

  // Rotation should update the bounds.
  Shell::Get()->display_manager()->SetDisplayRotation(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      display::Display::ROTATE_90, display::Display::RotationSource::USER);
  // Manually call the rotation animation callback here as the animator is only
  // used when a wallpaper is set, and there is no easy way to fake a wallpaper
  // in ash_unittests.
  float_controller->OnScreenRotationAnimationFinished(
      Shell::GetPrimaryRootWindowController()->GetScreenRotationAnimator(),
      /*canceled=*/false);
  EXPECT_FALSE(window()->IsVisible());
  EXPECT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window()));
  EXPECT_EQ(FloatController::GetFloatWindowTabletBounds(window()),
            window()->GetBoundsInScreen());

  // Test untucking.
  float_controller->MaybeUntuckFloatedWindowForTablet(window());
  ShellTestApi().WaitForWindowFinishAnimating(window());
  EXPECT_TRUE(window()->IsVisible());
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window()));
  EXPECT_EQ(FloatController::GetFloatWindowTabletBounds(window()),
            delegate()->requested_bounds());

  // Bounds change should NOT be blocked after untucked.
  state()->set_bounds_locally(true);
  widget()->SetBounds(client_requested_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(window()->GetBoundsInScreen(), client_requested_bounds);
}

TEST_P(ClientControlledStateTestClamshellAndTablet, MoveFloatedWindow) {
  // The AppType must be set to any except `chromeos::AppType::NON_APP` (default
  // value) to make it floatable.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  if (InTabletMode()) {
    // Resizing must be enabled in tablet mode to float.
    widget_delegate()->EnableFloat();
  }
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Float window.
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state()->OnWMEvent(&float_event);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_TRUE(window_state()->IsFloated());
  EXPECT_EQ(kShellWindowId_FloatContainer, window()->parent()->GetId());

  // Start dragging on the left of the minimize button.
  auto* const header_view = GetHeaderView();
  auto* const event_generator = GetEventGenerator();

  chromeos::FrameCaptionButtonContainerView::TestApi test_api(
      header_view->caption_button_container());
  event_generator->set_current_screen_location(
      gfx::Point(test_api.minimize_button()->GetBoundsInScreen().x() - 5,
                 // Minimize button y coordinate is at the top of the header, so
                 // use the center point of the header instead.
                 header_view->GetBoundsInScreen().CenterPoint().y()));
  event_generator->PressLeftButton();
  EXPECT_TRUE(window_state_delegate()->drag_in_progress());

  gfx::Rect expected_bounds = delegate()->requested_bounds();
  // Drag to the top left with some interval points. Verify the window is
  // aligned with the new cursor point.
  for (const gfx::Vector2d& diff :
       {gfx::Vector2d(-10, -10), gfx::Vector2d(-100, -10),
        gfx::Vector2d(-400, -400)}) {
    event_generator->MoveMouseBy(diff.x(), diff.y());
    expected_bounds.Offset(diff);

    EXPECT_TRUE(window_state_delegate()->drag_in_progress());
    EXPECT_EQ(delegate()->requested_bounds(), expected_bounds);

    ApplyPendingRequestedBounds();
  }

  event_generator->ReleaseLeftButton();
  EXPECT_FALSE(window_state_delegate()->drag_in_progress());

  if (InTabletMode()) {
    // In tablet mode, we have magnetism so the drag-to-top-left operation
    // should result in placing the window at the top left with padding.
    const int padding = chromeos::wm::kFloatedWindowPaddingDp;
    expected_bounds.set_origin(gfx::Point(padding, padding));
    EXPECT_EQ(delegate()->requested_bounds(), expected_bounds);
  } else {
    // In clamshell mode, we don't have magnetism so the window bounds should
    // persist after releasing the mouse button.
    EXPECT_EQ(delegate()->requested_bounds(), expected_bounds);
  }

  // Minimize and unminimize the window. Test that its bounds are restored.
  window_state()->Minimize();
  window_state()->Restore();
  ApplyPendingRequestedBounds();
  EXPECT_EQ(delegate()->requested_bounds(), expected_bounds);
}

TEST_P(ClientControlledStateTestClamshellAndTablet, FloatWindow) {
  // The AppType must be set to any except `chromeos::AppType::NON_APP` (default
  // value) to make it floatable.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  if (InTabletMode()) {
    // Resizing must be enabled in tablet mode to float.
    widget_delegate()->EnableFloat();
  }
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Test float.
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state()->OnWMEvent(&float_event);
  EXPECT_EQ(InTabletMode()
                ? FloatController::GetFloatWindowTabletBounds(window())
                : FloatController::GetFloatWindowClamshellBounds(
                      window(), chromeos::FloatStartLocation::kBottomRight),
            delegate()->requested_bounds());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kFloated, delegate()->new_state());

  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsFloated());
  EXPECT_EQ(kShellWindowId_FloatContainer, window()->parent()->GetId());

  // Test rotate.
  ASSERT_TRUE(chromeos::wm::IsLandscapeOrientationForWindow(window()));
  Shell::Get()->display_manager()->SetDisplayRotation(
      display::Screen::GetScreen()->GetPrimaryDisplay().id(),
      display::Display::ROTATE_90, display::Display::RotationSource::USER);
  ASSERT_FALSE(chromeos::wm::IsLandscapeOrientationForWindow(window()));
  EXPECT_EQ(InTabletMode()
                ? FloatController::GetFloatWindowTabletBounds(window())
                : FloatController::GetFloatWindowClamshellBounds(
                      window(), chromeos::FloatStartLocation::kBottomRight),
            delegate()->requested_bounds());

  // Test minimize.
  const WMEvent minimize_event(WM_EVENT_MINIMIZE);
  window_state()->OnWMEvent(&minimize_event);
  EXPECT_EQ(WindowStateType::kFloated, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kMinimized, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsMinimized());
  EXPECT_FALSE(window()->IsVisible());

  // Test unminimize.
  const WMEvent unminimize_event(WM_EVENT_RESTORE);
  window_state()->OnWMEvent(&unminimize_event);
  EXPECT_EQ(InTabletMode()
                ? FloatController::GetFloatWindowTabletBounds(window())
                : FloatController::GetFloatWindowClamshellBounds(
                      window(), chromeos::FloatStartLocation::kBottomRight),
            delegate()->requested_bounds());
  EXPECT_EQ(WindowStateType::kMinimized, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kFloated, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsFloated());
  EXPECT_EQ(kShellWindowId_FloatContainer, window()->parent()->GetId());

  // Test unfloat.
  const WMEvent restore_event(WM_EVENT_RESTORE);
  window_state()->OnWMEvent(&restore_event);
  EXPECT_EQ(WindowStateType::kFloated, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kNormal, delegate()->new_state());

  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_FALSE(window_state()->IsFloated());
  EXPECT_NE(kShellWindowId_FloatContainer, window()->parent()->GetId());
}

TEST_P(ClientControlledStateTestClamshellAndTablet,
       DragOverviewWindowToSnapOneSide) {
  auto* const overview_controller = OverviewController::Get();
  auto* const split_view_controller = SplitViewController::Get(window());

  widget_delegate()->EnableSnap();

  // Create a fake normal window in addition to `window()` (client-controlled
  // window) because we need at least two windows to keep overview mode active
  // after snapping one of them.
  auto fake_uninterested_window = CreateAppWindow();

  // Enter overview.
  ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Drag `window()`'s overview item to snap to left.
  DragOverviewItemToSnap(window(), /*to_left=*/true);

  // Ensures the window is in a transitional snapped state.
  EXPECT_TRUE(split_view_controller->IsWindowInTransitionalState(window()));
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  EXPECT_FALSE(window_state()->IsSnapped());

  // Activating window just before accepting the request shouldn't end the
  // overview.
  widget()->Activate();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Accept the snap request.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
  EXPECT_TRUE(window_state()->IsSnapped());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_EQ(split_view_controller->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller->primary_window(), window());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_P(ClientControlledStateTestClamshellAndTablet,
       DragOverviewWindowToSnapBothSide) {
  auto* const overview_controller = OverviewController::Get();
  auto* const split_view_controller = SplitViewController::Get(window());

  widget_delegate()->EnableSnap();

  // Create a normal (non-client-controlled) window in addition to `window()`
  // (client-controlled window) to fill the one side of the split view.
  auto non_client_controlled_window = CreateAppWindow();

  // Enter overview.
  ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Drag `non_client_controlled_window`'s overview item to snap to left.
  DragOverviewItemToSnap(non_client_controlled_window.get(), /*to_left=*/true);

  // Click `window()`'s overview item to snap to right.
  ClickOnOverviewItem(window());

  // Ensures the window is in a transitional snapped state.
  EXPECT_TRUE(split_view_controller->IsWindowInTransitionalState(window()));
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
  EXPECT_FALSE(window_state()->IsSnapped());

  // Activating window just before accepting the request shouldn't end the
  // overview.
  widget()->Activate();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Accept the snap request.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_TRUE(window_state()->IsSnapped());

  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  VerifySnappedBounds(non_client_controlled_window.get(),
                      chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state()->GetStateType());
  EXPECT_EQ(
      WindowStateType::kPrimarySnapped,
      WindowState::Get(non_client_controlled_window.get())->GetStateType());

  if (InTabletMode()) {
    // In tablet mode, we should keep splitview while overview should end.
    EXPECT_TRUE(split_view_controller->InSplitViewMode());
    EXPECT_EQ(split_view_controller->state(),
              SplitViewController::State::kBothSnapped);
    EXPECT_EQ(split_view_controller->secondary_window(), window());
    EXPECT_FALSE(overview_controller->InOverviewSession());
  } else {
    // In clamshell mode, we should end both splitview and overview.
    EXPECT_FALSE(split_view_controller->InSplitViewMode());
    EXPECT_EQ(split_view_controller->state(),
              SplitViewController::State::kNoSnap);
    EXPECT_FALSE(overview_controller->InOverviewSession());
  }
}

// Tests that a client-controlled window works with dragging the window to the
// edge of the screen to replace an snapped window with the dragged window.
TEST_P(ClientControlledStateTestClamshellAndTablet,
       DragOverviewWindowToReplaceSnappedWindow) {
  auto* const overview_controller = OverviewController::Get();
  auto* const split_view_controller = SplitViewController::Get(window());

  widget_delegate()->EnableSnap();

  // Create a normal (non-client-controlled) window in addition to `window()`.
  auto non_client_controlled_window = CreateAppWindow();

  // Enter overview.
  ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  // Drag `non_client_controlled_window`'s overview item to snap to left.
  DragOverviewItemToSnap(non_client_controlled_window.get(), /*to_left=*/true);
  EXPECT_EQ(
      WindowStateType::kPrimarySnapped,
      WindowState::Get(non_client_controlled_window.get())->GetStateType());

  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Drag `window()`'s overview item to snap to left.
  DragOverviewItemToSnap(window(), /*to_left=*/true);

  // Ensures the window is in a transitional snapped state.
  EXPECT_TRUE(split_view_controller->IsWindowInTransitionalState(window()));
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  EXPECT_FALSE(window_state()->IsSnapped());

  // Activating window just before accepting the request shouldn't trigger
  // another auto snapping.
  widget()->Activate();
  EXPECT_TRUE(overview_controller->InOverviewSession());

  // Accept the snap request.
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  EXPECT_TRUE(window_state()->IsSnapped());

  // `window()` should be snapped to left. And `non_client_controlled_window`
  // should be kicked out of snapped state and be in overview.
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());

  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_TRUE(GetOverviewItemForWindow(non_client_controlled_window.get()));
}

TEST_P(ClientControlledStateTestClamshellAndTablet,
       SnapBeforePreviousEventIsApplied) {
  auto* const overview_controller = OverviewController::Get();
  auto* const split_view_controller = SplitViewController::Get(window());

  widget_delegate()->EnableSnap();

  std::queue<WindowStateType> new_state_queue;
  std::queue<gfx::Rect> requested_bounds_queue;

  // Send a maximize request.
  const WMEvent maximize(WM_EVENT_MAXIMIZE);
  window_state()->OnWMEvent(&maximize);
  new_state_queue.push(delegate()->new_state());
  requested_bounds_queue.push(delegate()->requested_bounds());

  // Send a snap request.
  const WindowSnapWMEvent snap(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap);
  new_state_queue.push(delegate()->new_state());
  requested_bounds_queue.push(delegate()->requested_bounds());

  // Process requests sequentially.
  ASSERT_EQ(new_state_queue.size(), requested_bounds_queue.size());
  while (!new_state_queue.empty() && !requested_bounds_queue.empty()) {
    state()->EnterNextState(window_state(), new_state_queue.front());
    state()->set_bounds_locally(true);
    widget()->SetBounds(requested_bounds_queue.front());
    state()->set_bounds_locally(false);

    new_state_queue.pop();
    requested_bounds_queue.pop();
  }

  // The window should be snapped as it's the last requested state.
  EXPECT_TRUE(window_state()->IsSnapped());

  // In tablet mode, split view mode should be activated.
  if (InTabletMode()) {
    EXPECT_TRUE(split_view_controller->InSplitViewMode());
    EXPECT_EQ(split_view_controller->state(),
              SplitViewController::State::kPrimarySnapped);
    EXPECT_EQ(split_view_controller->primary_window(), window());
    EXPECT_TRUE(overview_controller->InOverviewSession());
  }
}

TEST_P(ClientControlledStateTestClamshellAndTablet, SnapFloatedWindow) {
  // The AppType must be set to any except `chromeos::AppType::NON_APP` (default
  // value) to make it floatable.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  widget_delegate()->EnableFloat();
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  widget_delegate()->EnableSnap();
  ASSERT_TRUE(window_state()->CanSnap());

  // Send a float request and accepts it.
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state()->OnWMEvent(&float_event);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  ASSERT_TRUE(window_state()->IsFloated());

  // Send a snap request but don't accept it yet.
  const WindowSnapWMEvent snap(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap);
  ASSERT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  ASSERT_FALSE(window_state()->IsSnapped());

  // Emit the size constraints changed event.
  widget()->OnSizeConstraintsChanged();

  // The requested bounds should be the snapped one (not floated bounds).
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  VerifySnappedBounds(window(), chromeos::kDefaultSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state()->GetStateType());
}

// Tests that floating a fullscreen window to replace a floated window works
// properly without any crash. Regression test for b/322374826.
TEST_P(ClientControlledStateTestClamshellAndTablet,
       ReplaceFloatedWindowWithFullscreenWindow) {
  // The AppType must be set to any except `chromeos::AppType::NON_APP` (default
  // value) to make it floatable.
  window()->SetProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP);
  widget_delegate()->EnableFloat();
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Make `window()` fullscreen to hide shelf.
  const WMEvent enter_fullscreen(WM_EVENT_FULLSCREEN);
  window_state()->OnWMEvent(&enter_fullscreen);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(widget()->IsFullscreen());

  // Create another client-controlled window.
  auto widget2 =
      TestWidgetBuilder()
          .SetParent(Shell::GetPrimaryRootWindow()->GetChildById(
              desks_util::GetActiveDeskContainerId()))
          .SetBounds(kInitialBounds)
          .SetTestWidgetDelegate()
          .SetWindowProperty(chromeos::kAppTypeKey, chromeos::AppType::ARC_APP)
          .SetShow(false)
          .BuildOwnsNativeWidget();
  auto* const window_state2 = WindowState::Get(widget2->GetNativeWindow());
  window_state2->set_allow_set_bounds_direct(true);
  auto delegate2 = std::make_unique<TestClientControlledStateDelegate>();
  auto* const state_delegate2_ptr = delegate2.get();
  auto state2 = std::make_unique<ClientControlledState>(std::move(delegate2));
  auto* state2_ptr = state2.get();
  window_state2->SetStateObject(std::move(state2));
  widget2->Show();

  // Float `widget2`.
  const WindowFloatWMEvent float_event(
      chromeos::FloatStartLocation::kBottomRight);
  window_state2->OnWMEvent(&float_event);
  state2_ptr->EnterNextState(window_state2, state_delegate2_ptr->new_state());
  ASSERT_TRUE(window_state2->IsFloated());

  // Float `window`.
  window_state()->OnWMEvent(&float_event);
  state()->EnterNextState(window_state(), delegate()->new_state());
  ApplyPendingRequestedBounds();
  ASSERT_TRUE(window_state()->IsFloated());

  // Floating `window` should result in unfloating `widget2`.
  state2_ptr->EnterNextState(window_state2, state_delegate2_ptr->new_state());
  EXPECT_FALSE(window_state2->IsFloated());
}

}  // namespace ash
