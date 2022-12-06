// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/client_controlled_state.h"

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/aura/client/aura_constants.h"
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
             requested_state == WindowStateType::kSecondarySnapped);
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
    AshTestBase::SetUp();

    widget_delegate_ = new TestWidgetDelegate();

    views::Widget::InitParams params;
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    params.parent = Shell::GetPrimaryRootWindow()->GetChildById(
        desks_util::GetActiveDeskContainerId());
    params.bounds = kInitialBounds;
    params.delegate = widget_delegate_;

    widget_ = std::make_unique<views::Widget>();
    widget_->Init(std::move(params));
    WindowState* window_state = WindowState::Get(window());
    window_state->set_allow_set_bounds_direct(true);
    auto delegate = std::make_unique<TestClientControlledStateDelegate>();
    state_delegate_ = delegate.get();
    auto state = std::make_unique<ClientControlledState>(std::move(delegate));
    state_ = state.get();
    window_state->SetStateObject(std::move(state));
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

 private:
  ClientControlledState* state_ = nullptr;
  TestClientControlledStateDelegate* state_delegate_ = nullptr;
  TestWidgetDelegate* widget_delegate_ = nullptr;  // owned by itself.
  std::unique_ptr<views::Widget> widget_;
};

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

}  // namespace ash
