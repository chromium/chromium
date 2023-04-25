// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

#include <queue>

#include "ash/display/screen_orientation_controller.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_test_util.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/header_view.h"
#include "chromeos/ui/wm/constants.h"
#include "chromeos/ui/wm/features.h"
#include "chromeos/ui/wm/window_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::WindowStateType;

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
  }

  WindowStateType old_state() const { return old_state_; }

  WindowStateType new_state() const { return new_state_; }

  const gfx::Rect& requested_bounds() const { return requested_bounds_; }

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

class TestWindowStateDelegate : public WindowStateDelegate {
 public:
  TestWindowStateDelegate() = default;
  TestWindowStateDelegate(const TestWindowStateDelegate&) = delete;
  TestWindowStateDelegate& operator=(const TestWindowStateDelegate&) = delete;
  ~TestWindowStateDelegate() override = default;

  // WindowStateDelegate:
  std::unique_ptr<PresentationTimeRecorder> OnDragStarted(
      int component) override {
    drag_in_progress_ = true;
    return nullptr;
  }
  void OnDragFinished(bool cancel, const gfx::PointF& location) override {
    drag_in_progress_ = false;
  }

  bool drag_in_progress() const { return drag_in_progress_; }

 private:
  bool drag_in_progress_ = false;
};

}  // namespace

class ClientControlledStateTest : public AshTestBase {
 public:
  ClientControlledStateTest() = default;

  ClientControlledStateTest(const ClientControlledStateTest&) = delete;
  ClientControlledStateTest& operator=(const ClientControlledStateTest&) =
      delete;

  ~ClientControlledStateTest() override = default;

  void SetUp() override {
    // We need to enable the flag before `AshTestBase::SetUp()` to make
    // FloatController instantiated in Shell.
    scoped_feature_list_.InitAndEnableFeature(
        chromeos::wm::features::kWindowLayoutMenu);
    AshTestBase::SetUp();

    widget_delegate_ = new TestWidgetDelegate();

    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
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
    auto window_state_delegate = std::make_unique<TestWindowStateDelegate>();
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
  TestWindowStateDelegate* window_state_delegate() {
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

 private:
  raw_ptr<ClientControlledState, ExperimentalAsh> state_ = nullptr;
  raw_ptr<TestClientControlledStateDelegate, ExperimentalAsh> state_delegate_ =
      nullptr;
  raw_ptr<TestWidgetDelegate, ExperimentalAsh> widget_delegate_ =
      nullptr;  // owned by itself.
  raw_ptr<TestWindowStateDelegate, ExperimentalAsh> window_state_delegate_ =
      nullptr;
  std::unique_ptr<views::Widget> widget_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

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
  EXPECT_EQ(ui::SHOW_STATE_NORMAL, widget()->GetNativeWindow()->GetProperty(
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
  gfx::Rect bounds = screen->GetPrimaryDisplay().work_area();

  const WMEvent center_event(WM_EVENT_CENTER);
  window_state()->OnWMEvent(&center_event);
  EXPECT_NEAR(bounds.CenterPoint().x(),
              delegate()->requested_bounds().CenterPoint().x(), 1);
  EXPECT_NEAR(bounds.CenterPoint().y(),
              delegate()->requested_bounds().CenterPoint().y(), 1);
}

TEST_F(ClientControlledStateTest, SnapWindow) {
  // Snap disabled.
  display::Screen* screen = display::Screen::GetScreen();
  gfx::Rect work_area = screen->GetPrimaryDisplay().work_area();
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
  EXPECT_NEAR(work_area.CenterPoint().x(),
              delegate()->requested_bounds().right(), 1);
  EXPECT_EQ(work_area.height(), delegate()->requested_bounds().height());
  EXPECT_TRUE(delegate()->requested_bounds().origin().IsOrigin());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());

  delegate()->Reset();

  window_state()->OnWMEvent(&snap_right_event);
  EXPECT_NEAR(work_area.CenterPoint().x(), delegate()->requested_bounds().x(),
              1);
  EXPECT_EQ(work_area.height(), delegate()->requested_bounds().height());
  EXPECT_EQ(work_area.bottom_right(),
            delegate()->requested_bounds().bottom_right());
  EXPECT_EQ(WindowStateType::kDefault, delegate()->old_state());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
}

TEST_F(ClientControlledStateTest, PartialSnap) {
  // Snap enabled.
  widget_delegate()->EnableSnap();

  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();

  // Test that snap from half to partial works.
  const WindowSnapWMEvent snap_left_half(WM_EVENT_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_left_half);
  gfx::Rect expected_bounds(work_area.x(), work_area.y(),
                            work_area.width() * chromeos::kDefaultSnapRatio,
                            work_area.height());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  EXPECT_EQ(expected_bounds, delegate()->requested_bounds());

  const WindowSnapWMEvent snap_left_partial(WM_EVENT_SNAP_PRIMARY,
                                            chromeos::kTwoThirdSnapRatio);
  window_state()->OnWMEvent(&snap_left_partial);
  expected_bounds.set_width(work_area.width() * chromeos::kTwoThirdSnapRatio);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  EXPECT_EQ(expected_bounds, delegate()->requested_bounds());

  // Test that snap from primary to secondary works.
  const WindowSnapWMEvent snap_right_half(WM_EVENT_SNAP_SECONDARY);
  window_state()->OnWMEvent(&snap_right_half);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
  expected_bounds.set_x(work_area.width() * chromeos::kDefaultSnapRatio);
  expected_bounds.set_width(work_area.width() * chromeos::kDefaultSnapRatio);
  EXPECT_EQ(expected_bounds, delegate()->requested_bounds());

  const WindowSnapWMEvent snap_right_partial(WM_EVENT_SNAP_SECONDARY,
                                             chromeos::kOneThirdSnapRatio);
  window_state()->OnWMEvent(&snap_right_partial);
  EXPECT_EQ(WindowStateType::kSecondarySnapped, delegate()->new_state());
  expected_bounds.set_x(
      std::round(work_area.width() * chromeos::kTwoThirdSnapRatio));
  expected_bounds.set_width(
      std::round(work_area.width() * chromeos::kOneThirdSnapRatio));
  EXPECT_EQ(expected_bounds, delegate()->requested_bounds());
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

TEST_F(ClientControlledStateTest, SnapMinimizeAndUnminimize) {
  UpdateDisplay("800x600");
  widget_delegate()->EnableSnap();

  const WindowSnapWMEvent snap_left_event(WM_EVENT_CYCLE_SNAP_PRIMARY);
  window_state()->OnWMEvent(&snap_left_event);
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(gfx::Rect(0, 0, 400, 600 - ShelfConfig::Get()->shelf_size()),
            delegate()->requested_bounds());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());

  const gfx::Rect resized_bounds(0, 0, 300,
                                 600 - ShelfConfig::Get()->shelf_size());
  const SetBoundsWMEvent set_bounds_event(resized_bounds,
                                          delegate()->display_id());
  window_state()->OnWMEvent(&set_bounds_event);
  state()->set_bounds_locally(true);
  window()->SetBounds(resized_bounds);
  state()->set_bounds_locally(false);
  EXPECT_EQ(resized_bounds, delegate()->requested_bounds());

  widget()->Minimize();
  state()->EnterNextState(window_state(), delegate()->new_state());

  ::wm::Unminimize(widget()->GetNativeWindow());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  EXPECT_EQ(resized_bounds, delegate()->requested_bounds());
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
  auto widget2 = CreateTestWidget();
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
  auto widget2 = CreateTestWidget();
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
  widget()->SetBounds(gfx::Rect(700, 0, 100, 200));
  state()->set_bounds_locally(false);
  EXPECT_EQ(gfx::Rect(700, 0, 100, 200), widget()->GetWindowBoundsInScreen());

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
  EXPECT_EQ(gfx::Rect(575, 0, 100, 200), delegate()->requested_bounds());
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
  ASSERT_EQ(true, Shell::Get()->tablet_mode_controller()->InTabletMode());
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
  split_view_controller->SnapWindow(
      window_state()->window(), SplitViewController::SnapPosition::kSecondary);

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

TEST_F(ClientControlledStateTest, ResizeSnappedWindowInTabletMode) {
  window()->SetProperty(aura::client::kAppType,
                        static_cast<int>(AppType::ARC_APP));
  ASSERT_EQ(chromeos::OrientationType::kLandscapePrimary,
            GetCurrentScreenOrientation());
  auto* const split_view_controller = SplitViewController::Get(window());

  // Enter tablet mode
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_EQ(true, Shell::Get()->tablet_mode_controller()->InTabletMode());

  // Snap a window
  widget_delegate()->EnableSnap();
  split_view_controller->SnapWindow(
      window(), SplitViewController::SnapPosition::kPrimary);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, delegate()->new_state());
  state()->EnterNextState(window_state(), delegate()->new_state());
  EXPECT_TRUE(window_state()->IsSnapped());

  // Move the divider
  const gfx::Rect initial_bounds = delegate()->requested_bounds();
  auto* const split_view_divider = split_view_controller->split_view_divider();
  const gfx::Rect divider_bounds =
      split_view_divider->GetDividerBoundsInScreen(false);
  ui::test::EventGenerator* const generator = GetEventGenerator();
  generator->set_current_screen_location(divider_bounds.CenterPoint());
  const gfx::Rect display_bounds =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window());
  const gfx::Point resize_point(display_bounds.width() * 0.33f, 0);
  generator->DragMouseTo(resize_point);
  EXPECT_GT(initial_bounds.width(), delegate()->requested_bounds().width());
}

TEST_F(ClientControlledStateTest, FlingFloatedWindowInTabletMode) {
  // The AppType must be set to any except `AppType::NON_APP` (default value) to
  // make it floatable.
  window()->SetProperty(aura::client::kAppType,
                        static_cast<int>(AppType::ARC_APP));
  widget_delegate()->EnableFloat();
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Enter tablet mode
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_EQ(true, Shell::Get()->tablet_mode_controller()->InTabletMode());

  // Float window.
  const WMEvent float_event(WM_EVENT_FLOAT);
  window_state()->OnWMEvent(&float_event);
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
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
            if (event_type != ui::ET_GESTURE_SCROLL_UPDATE) {
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

  auto* const float_controller = Shell::Get()->float_controller();

  // The AppType must be set to any except `AppType::NON_APP` (default value) to
  // make it floatable.
  window()->SetProperty(aura::client::kAppType,
                        static_cast<int>(AppType::ARC_APP));
  widget_delegate()->EnableFloat();
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Enter tablet mode
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  ASSERT_TRUE(Shell::Get()->tablet_mode_controller()->InTabletMode());

  // Float window.
  const WMEvent float_event(WM_EVENT_FLOAT);
  window_state()->OnWMEvent(&float_event);
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
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

  // Bounds change should be blocked while animating.
  const auto start_bounds = window()->GetBoundsInScreen();
  state()->set_bounds_locally(true);
  widget()->SetBounds(gfx::Rect(0, 0, 256, 256));
  state()->set_bounds_locally(false);
  EXPECT_EQ(window()->GetBoundsInScreen(), start_bounds);

  EXPECT_TRUE(window()->IsVisible());
  ShellTestApi().WaitForWindowFinishAnimating(window());
  EXPECT_FALSE(window()->IsVisible());
  EXPECT_TRUE(float_controller->IsFloatedWindowTuckedForTablet(window()));

  // Test untucking.
  float_controller->MaybeUntuckFloatedWindowForTablet(window());
  ShellTestApi().WaitForWindowFinishAnimating(window());
  EXPECT_TRUE(window()->IsVisible());
  EXPECT_FALSE(float_controller->IsFloatedWindowTuckedForTablet(window()));
  EXPECT_EQ(FloatController::GetPreferredFloatWindowTabletBounds(window()),
            delegate()->requested_bounds());
}

TEST_P(ClientControlledStateTestClamshellAndTablet, MoveFloatedWindow) {
  // The AppType must be set to any except `AppType::NON_APP` (default value) to
  // make it floatable.
  window()->SetProperty(aura::client::kAppType,
                        static_cast<int>(AppType::ARC_APP));
  if (InTabletMode()) {
    // Resizing must be enabled in tablet mode to float.
    widget_delegate()->EnableFloat();
  }
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Float window.
  const WMEvent float_event(WM_EVENT_FLOAT);
  window_state()->OnWMEvent(&float_event);
  ApplyPendingRequestedBounds();
  state()->EnterNextState(window_state(), delegate()->new_state());
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
}

TEST_P(ClientControlledStateTestClamshellAndTablet, FloatWindow) {
  // The AppType must be set to any except `AppType::NON_APP` (default value) to
  // make it floatable.
  window()->SetProperty(aura::client::kAppType,
                        static_cast<int>(AppType::ARC_APP));
  if (InTabletMode()) {
    // Resizing must be enabled in tablet mode to float.
    widget_delegate()->EnableFloat();
  }
  ASSERT_TRUE(chromeos::wm::CanFloatWindow(window()));

  // Test float.
  const WMEvent float_event(WM_EVENT_FLOAT);
  window_state()->OnWMEvent(&float_event);
  EXPECT_EQ(
      InTabletMode()
          ? FloatController::GetPreferredFloatWindowTabletBounds(window())
          : FloatController::GetPreferredFloatWindowClamshellBounds(window()),
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
  EXPECT_EQ(
      InTabletMode()
          ? FloatController::GetPreferredFloatWindowTabletBounds(window())
          : FloatController::GetPreferredFloatWindowClamshellBounds(window()),
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
  EXPECT_EQ(
      InTabletMode()
          ? FloatController::GetPreferredFloatWindowTabletBounds(window())
          : FloatController::GetPreferredFloatWindowClamshellBounds(window()),
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
  auto* const overview_controller = Shell::Get()->overview_controller();
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
  auto* const overview_item = GetOverviewItemForWindow(window());
  auto* const event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(
      gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
  event_generator->DragMouseTo(0, 0);

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
  EXPECT_TRUE(window_state()->IsSnapped());
  EXPECT_TRUE(split_view_controller->InSplitViewMode());
  EXPECT_EQ(split_view_controller->state(),
            SplitViewController::State::kPrimarySnapped);
  EXPECT_EQ(split_view_controller->primary_window(), window());
  EXPECT_TRUE(overview_controller->InOverviewSession());
}

TEST_P(ClientControlledStateTestClamshellAndTablet,
       DragOverviewWindowToSnapBothSide) {
  auto* const overview_controller = Shell::Get()->overview_controller();
  auto* const split_view_controller = SplitViewController::Get(window());
  auto* const event_generator = GetEventGenerator();

  widget_delegate()->EnableSnap();

  // Create a normal (non-client-controlled) window in addition to `window()`
  // (client-controlled window) to fill the one side of the split view.
  auto non_client_controlled_window = CreateAppWindow();

  // Enter overview.
  ToggleOverview();
  EXPECT_TRUE(overview_controller->InOverviewSession());
  EXPECT_FALSE(split_view_controller->InSplitViewMode());

  {
    // Drag `non_client_controlled_window`'s overview item to snap to left.
    auto* const overview_item =
        GetOverviewItemForWindow(non_client_controlled_window.get());
    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
    event_generator->DragMouseTo(0, 0);
  }

  {
    // Click `window()`'s overview item to snap to right.
    auto* const overview_item = GetOverviewItemForWindow(window());
    event_generator->set_current_screen_location(
        gfx::ToRoundedPoint(overview_item->target_bounds().CenterPoint()));
    event_generator->ClickLeftButton();
  }

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

TEST_P(ClientControlledStateTestClamshellAndTablet,
       SnapBeforePreviousEventIsApplied) {
  auto* const overview_controller = Shell::Get()->overview_controller();
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

}  // namespace ash
