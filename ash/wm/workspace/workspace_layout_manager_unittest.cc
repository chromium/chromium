// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/accessibility/test_accessibility_controller_client.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "ash/keyboard/ui/keyboard_ui.h"
#include "ash/keyboard/ui/keyboard_ui_controller.h"
#include "ash/keyboard/ui/keyboard_util.h"
#include "ash/keyboard/ui/test/keyboard_test_util.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/keyboard/keyboard_switches.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_prefs.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "ash/public/cpp/window_backdrop.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shell.h"
#include "ash/shell_observer.h"
#include "ash/style/rounded_label_widget.h"
#include "ash/system/notification_center/ash_message_popup_collection.h"
#include "ash/system/notification_center/message_popup_animation_waiter.h"
#include "ash/system/notification_center/notification_center_tray.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/test_window_builder.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wm/always_on_top_controller.h"
#include "ash/wm/client_controlled_state.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/test/fake_window_state.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/backdrop_controller.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ash/wm/workspace_controller.h"
#include "ash/wm/workspace_controller_test_api.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/audio/sounds.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/ui_base_switches.h"
#include "ui/base/ui_base_types.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/layer_type.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::WindowStateType;
const char kTestAppId[] = "test-app-id";

class MaximizeDelegateView : public views::WidgetDelegateView {
 public:
  explicit MaximizeDelegateView(const gfx::Rect& initial_bounds)
      : initial_bounds_(initial_bounds) {}

  MaximizeDelegateView(const MaximizeDelegateView&) = delete;
  MaximizeDelegateView& operator=(const MaximizeDelegateView&) = delete;

  ~MaximizeDelegateView() override = default;

  bool GetSavedWindowPlacement(
      const views::Widget* widget,
      gfx::Rect* bounds,
      ui::mojom::WindowShowState* show_state) const override {
    *bounds = initial_bounds_;
    *show_state = ui::mojom::WindowShowState::kMaximized;
    return true;
  }

 private:
  const gfx::Rect initial_bounds_;
};

class TestShellObserver : public ShellObserver {
 public:
  TestShellObserver() : call_count_(0), is_fullscreen_(false) {
    Shell::Get()->AddShellObserver(this);
  }

  TestShellObserver(const TestShellObserver&) = delete;
  TestShellObserver& operator=(const TestShellObserver&) = delete;

  ~TestShellObserver() override { Shell::Get()->RemoveShellObserver(this); }

  void OnFullscreenStateChanged(bool is_fullscreen,
                                aura::Window* container) override {
    call_count_++;
    is_fullscreen_ = is_fullscreen;
  }

  int call_count() const { return call_count_; }

  bool is_fullscreen() const { return is_fullscreen_; }

 private:
  int call_count_;
  bool is_fullscreen_;
};

display::Display GetDisplayNearestWindow(aura::Window* window) {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window);
}

display::ManagedDisplayInfo CreateDisplayInfo(int64_t id, gfx::Rect bounds) {
  display::ManagedDisplayInfo info = display::CreateDisplayInfo(id, bounds);
  info.SetRotation(display::Display::ROTATE_0,
                   display::Display::RotationSource::ACTIVE);
  // Each display should have at least one native mode.
  display::ManagedDisplayMode mode(bounds.size(), /*refresh_rate=*/60.f,
                                   /*is_interlaced=*/true,
                                   /*native=*/true);
  info.SetManagedDisplayModes({mode});
  return info;
}

class ScopedStickyKeyboardEnabler {
 public:
  ScopedStickyKeyboardEnabler()
      : accessibility_controller_(Shell::Get()->accessibility_controller()),
        enabled_(accessibility_controller_->virtual_keyboard().enabled()) {
    accessibility_controller_->virtual_keyboard().SetEnabled(true);
  }

  ScopedStickyKeyboardEnabler(const ScopedStickyKeyboardEnabler&) = delete;
  ScopedStickyKeyboardEnabler& operator=(const ScopedStickyKeyboardEnabler&) =
      delete;

  ~ScopedStickyKeyboardEnabler() {
    accessibility_controller_->virtual_keyboard().SetEnabled(enabled_);
  }

 private:
  raw_ptr<AccessibilityController> accessibility_controller_;
  const bool enabled_;
};

}  // namespace

using WorkspaceLayoutManagerTest = AshTestBase;

// Verifies that a window containing a restore coordinate will be restored to
// to the size prior to minimize, keeping the restore rectangle in tact (if
// there is one).
TEST_F(WorkspaceLayoutManagerTest, RestoreFromMinimizeKeepsRestore) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 15, 125, 35)));

  WindowState* window_state = WindowState::Get(window.get());

  // This will not be used for un-minimizing window.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(0, 0, 100, 100));
  window_state->Minimize();
  window_state->Restore();
  EXPECT_EQ("0,0 100x100", window_state->GetRestoreBoundsInScreen().ToString());
  EXPECT_EQ("10,15 125x35", window->bounds().ToString());

  UpdateDisplay("400x300,500x400");
  window->SetBoundsInScreen(gfx::Rect(600, 0, 125, 100), GetSecondaryDisplay());
  EXPECT_EQ(Shell::Get()->GetAllRootWindows()[1], window->GetRootWindow());
  window_state->Minimize();
  // This will not be used for un-minimizing window.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(0, 0, 100, 100));
  window_state->Restore();
  EXPECT_EQ("600,0 125x100", window->GetBoundsInScreen().ToString());

  // Make sure the unminimized window moves inside the display when
  // 2nd display is disconnected.
  window_state->Minimize();
  UpdateDisplay("400x300");
  window_state->Restore();
  EXPECT_EQ(Shell::GetPrimaryRootWindow(), window->GetRootWindow());
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
}

TEST_F(WorkspaceLayoutManagerTest, KeepMinimumVisibilityInDisplays) {
  UpdateDisplay("300x400,400x500");
  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();

  Shell::Get()->display_manager()->SetLayoutForCurrentDisplays(
      display::test::CreateDisplayLayout(Shell::Get()->display_manager(),
                                         display::DisplayPlacement::TOP, 0));

  EXPECT_EQ("0,-500 400x500", root_windows[1]->GetBoundsInScreen().ToString());

  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(10, -400, 200, 200)));
  EXPECT_EQ("10,-400 200x200", window1->GetBoundsInScreen().ToString());

  // Make sure the caption is visible.
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(10, -600, 200, 200)));
  EXPECT_EQ("10,-500 200x200", window2->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, NoMinimumVisibilityForPopupWindows) {
  UpdateDisplay("300x400");

  // Create a popup window out of display boundaries and make sure it is not
  // moved to have minimum visibility.
  std::unique_ptr<aura::Window> window(CreateTestWindow(
      gfx::Rect(400, 100, 50, 50), aura::client::WINDOW_TYPE_POPUP));
  EXPECT_EQ("400,100 50x50", window->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, KeepRestoredWindowInDisplay) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(1, 2, 130, 40)));
  WindowState* window_state = WindowState::Get(window.get());

  // Maximized -> Normal transition.
  window_state->Maximize();
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 130, 40));
  window_state->Restore();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_GE(window->bounds().y(), 0);

  // Minimized -> Normal transition.
  window_state->SetBoundsDirectForTesting(gfx::Rect(-100, -100, 130, 40));
  window_state->Minimize();
  EXPECT_FALSE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  EXPECT_EQ("-100,-100 130x40", window->bounds().ToString());
  window->Show();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_GE(window->bounds().y(), 0);

  // Fullscreen -> Normal transition.
  window->SetBounds(gfx::Rect(0, 0, 130, 40));  // reset bounds.
  ASSERT_EQ("0,0 130x40", window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(window->bounds(), window->GetRootWindow()->bounds());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(-100, -100, 130, 40));
  window_state->Restore();
  EXPECT_TRUE(
      Shell::GetPrimaryRootWindow()->bounds().Intersects(window->bounds()));
  // Y bounds should not be negative.
  EXPECT_GE(window->bounds().y(), 0);
}

TEST_F(WorkspaceLayoutManagerTest, MaximizeInDisplayToBeRestored) {
  UpdateDisplay("300x400,400x500");

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();

  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(1, 2, 130, 40)));
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 130, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(
      gfx::Rect(300, 0, 400, 500 - ShelfConfig::Get()->shelf_size()).ToString(),
      window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 130x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(295, 0, 130, 40));
  window_state->Maximize();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ(
      gfx::Rect(300, 0, 400, 500 - ShelfConfig::Get()->shelf_size()).ToString(),
      window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("295,0 130x40", window->GetBoundsInScreen().ToString());

  // Restoring widget state.
  std::unique_ptr<views::Widget> w1(new views::Widget);
  views::Widget::InitParams params(
      views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  params.delegate = new MaximizeDelegateView(gfx::Rect(400, 0, 130, 40));
  params.context = GetContext();
  w1->Init(std::move(params));
  EXPECT_EQ(root_windows[0], w1->GetNativeWindow()->GetRootWindow());
  w1->Show();
  EXPECT_TRUE(w1->IsMaximized());
  EXPECT_EQ(root_windows[1], w1->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ(
      gfx::Rect(300, 0, 400, 500 - ShelfConfig::Get()->shelf_size()).ToString(),
      w1->GetWindowBoundsInScreen().ToString());
  w1->Restore();
  EXPECT_EQ(root_windows[1], w1->GetNativeWindow()->GetRootWindow());
  EXPECT_EQ("400,0 130x40", w1->GetWindowBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, FullscreenInDisplayToBeRestored) {
  UpdateDisplay("300x400,400x500");

  aura::Window::Windows root_windows = Shell::Get()->GetAllRootWindows();

  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  EXPECT_EQ(root_windows[0], window->GetRootWindow());

  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetRestoreBoundsInScreen(gfx::Rect(400, 0, 130, 40));
  // Maximize the window in 2nd display as the restore bounds
  // is inside 2nd display.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("400,0 130x40", window->GetBoundsInScreen().ToString());

  // If the restore bounds intersects with the current display,
  // don't move.
  window_state->SetRestoreBoundsInScreen(gfx::Rect(295, 0, 130, 40));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("300,0 400x500", window->GetBoundsInScreen().ToString());

  window_state->Restore();
  EXPECT_EQ(root_windows[1], window->GetRootWindow());
  EXPECT_EQ("295,0 130x40", window->GetBoundsInScreen().ToString());
}

// aura::WindowObserver implementation used by
// DontClobberRestoreBoundsWindowObserver. This code mirrors what
// BrowserFrameAsh does. In particular when this code sees the window was
// maximized it changes the bounds of a secondary window. The secondary window
// mirrors the status window.
class DontClobberRestoreBoundsWindowObserver : public aura::WindowObserver {
 public:
  DontClobberRestoreBoundsWindowObserver() : window_(nullptr) {}

  DontClobberRestoreBoundsWindowObserver(
      const DontClobberRestoreBoundsWindowObserver&) = delete;
  DontClobberRestoreBoundsWindowObserver& operator=(
      const DontClobberRestoreBoundsWindowObserver&) = delete;

  void set_window(aura::Window* window) { window_ = window; }

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (!window_)
      return;

    if (WindowState::Get(window)->IsMaximized()) {
      aura::Window* w = window_;
      window_ = nullptr;

      gfx::Rect shelf_bounds(AshTestBase::GetPrimaryShelf()->GetIdealBounds());
      const gfx::Rect& window_bounds(w->bounds());
      w->SetBounds(gfx::Rect(window_bounds.x(), shelf_bounds.y() - 1,
                             window_bounds.width(), window_bounds.height()));
    }
  }

 private:
  raw_ptr<aura::Window> window_;
};

// Creates a window, maximized the window and from within the maximized
// notification sets the bounds of a window to overlap the shelf. Verifies this
// doesn't effect the restore bounds.
TEST_F(WorkspaceLayoutManagerTest, DontClobberRestoreBounds) {
  DontClobberRestoreBoundsWindowObserver window_observer;
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(10, 20, 30, 40));
  // NOTE: for this test to exercise the failure the observer needs to be added
  // before the parent set. This mimics what BrowserFrameAsh does.
  window->AddObserver(&window_observer);
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();

  WindowState* window_state = WindowState::Get(window.get());
  window_state->Activate();

  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(12, 20, 30, 40)));
  ::wm::AddTransientChild(window.get(), window2.get());
  window2->Show();

  window_observer.set_window(window2.get());
  window_state->Maximize();
  EXPECT_EQ("10,20 30x40", window_state->GetRestoreBoundsInScreen().ToString());
  window->RemoveObserver(&window_observer);
}

// Verifies when a window is maximized all descendant windows have a size.
TEST_F(WorkspaceLayoutManagerTest, ChildBoundsResetOnMaximize) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 30, 40)));
  window->Show();
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Activate();
  std::unique_ptr<aura::Window> child_window =
      ChildTestWindowBuilder(window.get(), gfx::Rect(5, 6, 7, 8)).Build();
  window_state->Maximize();
  EXPECT_EQ("5,6 7x8", child_window->bounds().ToString());
}

// Verifies a window created with maximized state has the maximized
// bounds.
TEST_F(WorkspaceLayoutManagerTest, MaximizeWithEmptySize) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  aura::Window* active_desk_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          desks_util::GetActiveDeskContainerId());
  active_desk_container->AddChild(window.get());
  window->Show();
  gfx::Rect work_area(GetPrimaryDisplay().work_area());
  EXPECT_EQ(work_area.ToString(), window->GetBoundsInScreen().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, WindowShouldBeOnScreenWhenAdded) {
  // Normal window bounds shouldn't be changed.
  gfx::Rect window_bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(CreateTestWindow(window_bounds));
  EXPECT_EQ(window_bounds, window->bounds());

  // If the window is out of the workspace, it would be moved on screen.
  gfx::Rect root_window_bounds = Shell::GetPrimaryRootWindow()->bounds();
  window_bounds.Offset(root_window_bounds.width(), root_window_bounds.height());
  ASSERT_FALSE(window_bounds.Intersects(root_window_bounds));
  std::unique_ptr<aura::Window> out_window(CreateTestWindow(window_bounds));
  EXPECT_EQ(window_bounds.size(), out_window->bounds().size());
  gfx::Rect bounds = out_window->bounds();
  bounds.Intersect(root_window_bounds);

  // 30% of the window edge must be visible.
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  aura::Window* parent = out_window->parent();
  parent->RemoveChild(out_window.get());
  out_window->SetBounds(gfx::Rect(-200, -200, 200, 200));
  // UserHasChangedWindowPositionOrSize flag shouldn't turn off this behavior.
  WindowState::Get(window.get())->SetBoundsChangedByUser(true);
  parent->AddChild(out_window.get());
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  // Make sure we always make more than 1/3 of the window edge visible even
  // if the initial bounds intersects with display.
  window_bounds.SetRect(-150, -150, 200, 200);
  bounds = window_bounds;
  bounds.Intersect(root_window_bounds);

  // Make sure that the initial bounds' visible area is less than 26%
  // so that the auto adjustment logic kicks in.
  ASSERT_LT(bounds.width(), out_window->bounds().width() * 0.26);
  ASSERT_LT(bounds.height(), out_window->bounds().height() * 0.26);
  ASSERT_TRUE(window_bounds.Intersects(root_window_bounds));

  std::unique_ptr<aura::Window> partially_out_window(
      CreateTestWindow(window_bounds));
  EXPECT_EQ(window_bounds.size(), partially_out_window->bounds().size());
  bounds = partially_out_window->bounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);

  // Make sure the window whose 30% width/height is bigger than display
  // will be placed correctly.
  window_bounds.SetRect(-1900, -1900, 3000, 3000);
  std::unique_ptr<aura::Window> window_bigger_than_display(
      CreateTestWindow(window_bounds));
  EXPECT_GE(root_window_bounds.width(),
            window_bigger_than_display->bounds().width());
  EXPECT_GE(root_window_bounds.height(),
            window_bigger_than_display->bounds().height());

  bounds = window_bigger_than_display->bounds();
  bounds.Intersect(root_window_bounds);
  EXPECT_GT(bounds.width(), out_window->bounds().width() * 0.29);
  EXPECT_GT(bounds.height(), out_window->bounds().height() * 0.29);
}

// Verifies the size of a window is enforced to be smaller than the work area.
TEST_F(WorkspaceLayoutManagerTest, SizeToWorkArea) {
  // Normal window bounds shouldn't be changed.
  gfx::Size work_area(GetPrimaryDisplay().work_area().size());
  const gfx::Rect window_bounds(100, 101, work_area.width() + 1,
                                work_area.height() + 2);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(window_bounds));
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
            window->bounds().ToString());

  // Directly setting the bounds triggers a slightly different code path. Verify
  // that too.
  window->SetBounds(window_bounds);
  EXPECT_EQ(gfx::Rect(gfx::Point(100, 101), work_area).ToString(),
            window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, NotifyFullscreenChanges) {
  TestShellObserver observer;
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(1, 2, 30, 40)));
  WindowState* window_state1 = WindowState::Get(window1.get());
  WindowState* window_state2 = WindowState::Get(window2.get());
  window_state2->Activate();

  const WMEvent toggle_fullscreen_event(WM_EVENT_TOGGLE_FULLSCREEN);
  window_state2->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_EQ(1, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  // When window1 moves to the front the fullscreen state should change.
  window_state1->Activate();
  EXPECT_EQ(2, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());

  // It should change back if window2 becomes active again.
  window_state2->Activate();
  EXPECT_EQ(3, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  window_state2->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_EQ(4, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());

  window_state2->OnWMEvent(&toggle_fullscreen_event);
  EXPECT_EQ(5, observer.call_count());
  EXPECT_TRUE(observer.is_fullscreen());

  // Closing the window should change the fullscreen state.
  window2.reset();
  EXPECT_EQ(6, observer.call_count());
  EXPECT_FALSE(observer.is_fullscreen());
}

// For crbug.com/673803, snapped window may not adjust snapped bounds on work
// area changed properly if window's layer is doing animation. We should use
// GetTargetBounds to check if snapped bounds need to be changed.
TEST_F(WorkspaceLayoutManagerTest,
       SnappedWindowMayNotAdjustBoundsOnWorkAreaChanged) {
  UpdateDisplay("600x400");
  std::unique_ptr<aura::Window> window(
      CreateTestWindow(gfx::Rect(10, 20, 100, 200)));
  WindowState* window_state = WindowState::Get(window.get());
  auto insets = gfx::Insets::TLBR(0, 0, 56, 0);
  WorkAreaInsets::ForWindow(window.get())
      ->UpdateWorkAreaInsetsForTest(window.get(), gfx::Rect(), insets, insets);
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  EXPECT_EQ(WindowStateType::kPrimarySnapped, window_state->GetStateType());
  const gfx::Rect kWorkAreaBounds = GetPrimaryDisplay().work_area();
  gfx::Rect expected_bounds =
      gfx::Rect(kWorkAreaBounds.x(), kWorkAreaBounds.y(),
                kWorkAreaBounds.width() / 2, kWorkAreaBounds.height());
  EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());

  ui::ScopedAnimationDurationScaleMode test_duration_mode(
      ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);
  // The following two `UpdateWorkAreaInsetsForTest` calls simulate the case of
  // crbug.com/673803 that work area first becomes fullscreen and then returns
  // to the original state.
  WorkAreaInsets::ForWindow(window.get())
      ->UpdateWorkAreaInsetsForTest(window.get(), gfx::Rect(), gfx::Insets(),
                                    gfx::Insets());
  ui::LayerAnimator* animator = window->layer()->GetAnimator();
  EXPECT_TRUE(animator->is_animating());
  WorkAreaInsets::ForWindow(window.get())
      ->UpdateWorkAreaInsetsForTest(window.get(), gfx::Rect(), insets, insets);
  animator->StopAnimating();
  EXPECT_FALSE(animator->is_animating());
  EXPECT_EQ(expected_bounds.ToString(), window->bounds().ToString());
}

// Tests that under the case of two snapped windows, if there is a display work
// area width change, the snapped window width is updated upon snapped width
// ratio (crbug.com/688583).
TEST_F(WorkspaceLayoutManagerTest, AdjustSnappedBoundsWidth) {
  UpdateDisplay("600x400");
  // Create two snapped windows, one left snapped, one right snapped.
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(10, 20, 100, 200)));
  WindowState* window1_state = WindowState::Get(window1.get());
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window1_state->OnWMEvent(&snap_left);
  const gfx::Rect work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  const gfx::Rect expected_left_snapped_bounds = gfx::Rect(
      work_area.x(), work_area.y(), work_area.width() / 2, work_area.height());
  EXPECT_EQ(expected_left_snapped_bounds, window1->bounds());

  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(10, 20, 100, 200)));
  WindowState* window2_state = WindowState::Get(window2.get());
  const WindowSnapWMEvent snap_right(WM_EVENT_SNAP_SECONDARY);
  window2_state->OnWMEvent(&snap_right);
  const gfx::Rect expected_right_snapped_bounds =
      gfx::Rect(work_area.right() - work_area.width() / 2, work_area.y(),
                work_area.width() / 2, work_area.height());
  EXPECT_EQ(expected_right_snapped_bounds, window2->bounds());

  // Set shelf alignment to left, which will change display work area.
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAlignment(ShelfAlignment::kLeft);
  const gfx::Rect new_work_area =
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  EXPECT_NE(work_area, new_work_area);

  const gfx::Rect new_expected_left_snapped_bounds =
      gfx::Rect(new_work_area.x(), new_work_area.y(), new_work_area.width() / 2,
                new_work_area.height());
  EXPECT_EQ(new_expected_left_snapped_bounds, window1->bounds());
  const gfx::Rect new_expected_right_snapped_bounds = gfx::Rect(
      new_work_area.right() - new_work_area.width() / 2, new_work_area.y(),
      new_work_area.width() / 2, new_work_area.height());
  EXPECT_EQ(new_expected_right_snapped_bounds, window2->bounds());

  // Set shelf alignment to bottom again.
  shelf->SetAlignment(ShelfAlignment::kBottom);
  EXPECT_EQ(expected_left_snapped_bounds, window1->bounds());
  EXPECT_EQ(expected_right_snapped_bounds, window2->bounds());
}

// Do not adjust window bounds to ensure minimum visibility for transient
// windows (crbug.com/624806).
TEST_F(WorkspaceLayoutManagerTest,
       DoNotAdjustTransientWindowBoundsToEnsureMinimumVisibility) {
  UpdateDisplay("300x400");
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(gfx::Rect(10, 0, 100, 200));
  ParentWindowInPrimaryRootWindow(window.get());
  window->Show();

  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(10, 0, 40, 20)));
  ::wm::AddTransientChild(window.get(), window2.get());
  window2->Show();

  gfx::Rect expected_bounds = window2->bounds();
  WorkAreaInsets::ForWindow(window.get())
      ->UpdateWorkAreaInsetsForTest(window.get(), gfx::Rect(),
                                    gfx::Insets::TLBR(50, 0, 0, 0),
                                    gfx::Insets::TLBR(50, 0, 0, 0));
  EXPECT_EQ(expected_bounds.ToString(), window2->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerTest, EnsureWindowStateInOverlay) {
  std::unique_ptr<aura::Window> window =
      std::make_unique<aura::Window>(nullptr, aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  auto* overlay_container =
      Shell::GetPrimaryRootWindowController()->GetContainer(
          kShellWindowId_OverlayContainer);
  overlay_container->AddChild(window.get());
  EXPECT_TRUE(window->GetProperty(kWindowStateKey));
}

// Make sure window bounds is correct with session state lock/unlock. It tests
// cases with various shelf settings.
TEST_F(WorkspaceLayoutManagerTest, WindowBoundsWithSessionState) {
  TestSessionControllerClient* client = GetSessionControllerClient();
  Shelf* shelf = GetPrimaryShelf();

  constexpr ShelfAlignment alignments[] = {
      ShelfAlignment::kLeft, ShelfAlignment::kRight, ShelfAlignment::kBottom};
  constexpr ShelfAutoHideBehavior auto_hide_behaviors[] = {
      ShelfAutoHideBehavior::kAlways, ShelfAutoHideBehavior::kNever};

  // Test normal window.
  for (auto alignment : alignments) {
    for (auto auto_hide_behavior : auto_hide_behaviors) {
      SCOPED_TRACE("Primary shelf alignment: " +
                   base::NumberToString(static_cast<int>(alignment)));
      SCOPED_TRACE("Primary shelf auto hide behavior: " +
                   base::NumberToString(static_cast<int>(auto_hide_behavior)));

      // Update the shelf setting.
      shelf->SetAlignment(alignment);
      shelf->SetAutoHideBehavior(auto_hide_behavior);
      ASSERT_EQ(alignment, shelf->alignment());
      ASSERT_EQ(auto_hide_behavior, shelf->auto_hide_behavior());

      // Create the test window, and set its bounds to match the entire
      // workarea.
      std::unique_ptr<aura::Window> window = CreateTestWindow();
      const gfx::Rect bounds =
          screen_util::GetDisplayWorkAreaBoundsInParent(window.get());
      window->SetBounds(bounds);
      EXPECT_EQ(bounds, window->bounds());

      // Lock/unlock the session, the test window should remain the same bounds.
      client->SetSessionState(session_manager::SessionState::LOCKED);
      EXPECT_EQ(bounds, window->bounds());
      client->SetSessionState(session_manager::SessionState::ACTIVE);
      EXPECT_EQ(bounds, window->bounds());
    }
  }

  // Test maximized window.
  for (auto alignment : alignments) {
    for (auto auto_hide_behavior : auto_hide_behaviors) {
      SCOPED_TRACE("Primary shelf alignment: " +
                   base::NumberToString(static_cast<int>(alignment)));
      SCOPED_TRACE("Primary shelf auto hide behavior: " +
                   base::NumberToString(static_cast<int>(auto_hide_behavior)));

      // Update the shelf setting.
      shelf->SetAlignment(alignment);
      shelf->SetAutoHideBehavior(auto_hide_behavior);
      ASSERT_EQ(alignment, shelf->alignment());
      ASSERT_EQ(auto_hide_behavior, shelf->auto_hide_behavior());

      // Create the test window, and maximize it.
      std::unique_ptr<aura::Window> window = CreateTestWindow();
      const gfx::Rect bounds =
          screen_util::GetDisplayWorkAreaBoundsInParent(window.get());
      WindowState::Get(window.get())->Maximize();
      EXPECT_EQ(bounds, window->bounds());

      // Lock/unlock the session, the test window should remain the same bounds.
      client->SetSessionState(session_manager::SessionState::LOCKED);
      EXPECT_EQ(bounds, window->bounds());
      client->SetSessionState(session_manager::SessionState::ACTIVE);
      EXPECT_EQ(bounds, window->bounds());
    }
  }
}

// Make sure maximized window bounds is correct with multiple displays. It tests
// cases when disconnecting secondary display in lock session with various shelf
// settings.
TEST_F(WorkspaceLayoutManagerTest, WindowBoundsWithMultiDisplays) {
  TestSessionControllerClient* client = GetSessionControllerClient();

  constexpr ShelfAlignment alignments[] = {
      ShelfAlignment::kLeft, ShelfAlignment::kRight, ShelfAlignment::kBottom};
  constexpr ShelfAutoHideBehavior auto_hide_behaviors[] = {
      ShelfAutoHideBehavior::kAlways, ShelfAutoHideBehavior::kNever};

  // Test maximized window.
  for (auto primary_alignment : alignments) {
    for (auto primary_auto_hide_behavior : auto_hide_behaviors) {
      for (auto secondary_alignment : alignments) {
        for (auto secondary_auto_hide_behavior : auto_hide_behaviors) {
          SCOPED_TRACE(
              "Primary shelf alignment: " +
              base::NumberToString(static_cast<int>(primary_alignment)));
          SCOPED_TRACE("Primary shelf auto hide behavior: " +
                       base::NumberToString(
                           static_cast<int>(primary_auto_hide_behavior)));
          SCOPED_TRACE(
              "Secondary shelf alignment: " +
              base::NumberToString(static_cast<int>(secondary_alignment)));
          SCOPED_TRACE("Secondary shelf auto hide behavior: " +
                       base::NumberToString(
                           static_cast<int>(secondary_auto_hide_behavior)));

          // Update to 2 displays.
          UpdateDisplay("800x600,500x400");
          Shelf* primary_shelf = GetPrimaryShelf();
          Shelf* secondary_shelf =
              Shell::GetAllRootWindowControllers().back()->shelf();

          // Update shelf for the primary display.
          primary_shelf->SetAlignment(primary_alignment);
          primary_shelf->SetAutoHideBehavior(primary_auto_hide_behavior);
          ASSERT_EQ(primary_alignment, primary_shelf->alignment());
          ASSERT_EQ(primary_auto_hide_behavior,
                    primary_shelf->auto_hide_behavior());

          // Update shelf for the secondary display.
          secondary_shelf->SetAlignment(secondary_alignment);
          secondary_shelf->SetAutoHideBehavior(secondary_auto_hide_behavior);
          ASSERT_EQ(secondary_alignment, secondary_shelf->alignment());
          ASSERT_EQ(secondary_auto_hide_behavior,
                    secondary_shelf->auto_hide_behavior());

          // Create a test window, move it to the secondary display, and
          // maximize it.
          std::unique_ptr<aura::Window> window = CreateTestWindow();
          window_util::MoveWindowToDisplay(window.get(),
                                           GetSecondaryDisplay().id());
          WindowState::Get(window.get())->Maximize();
          EXPECT_EQ(screen_util::GetDisplayWorkAreaBoundsInParent(window.get()),
                    window->bounds());

          // Lock the session, disconnect the secondary display, then unlock the
          // session. The maximized window should be moved to the primary
          // display with correct bounds.
          client->SetSessionState(session_manager::SessionState::LOCKED);
          UpdateDisplay("800x600");
          client->SetSessionState(session_manager::SessionState::ACTIVE);
          EXPECT_EQ(screen_util::GetDisplayWorkAreaBoundsInParent(window.get()),
                    window->bounds());
        }
      }
    }
  }
}

// Make sure maximized window bounds is correct with external display
// disconnected in docked mode.
// Please see b/252557955.
TEST_F(WorkspaceLayoutManagerTest,
       WindowBoundsWithExternalDisplayDisconnected) {
  // Set up two displays.
  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(1, gfx::Rect(800, 600));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(2, gfx::Rect(1, 1, 900, 700));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  int64_t internal_display_id = GetPrimaryDisplay().id();
  int64_t external_display_id = GetSecondaryDisplay().id();
  EXPECT_EQ(1, GetPrimaryDisplay().id());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  // Set up two shelves.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  Shelf* primary_shelf = GetPrimaryShelf();
  Shelf* secondary_shelf = Shell::GetAllRootWindowControllers().back()->shelf();
  SetShelfAlignmentPref(prefs, internal_display_id, ShelfAlignment::kLeft);
  SetShelfAutoHideBehaviorPref(prefs, internal_display_id,
                               ShelfAutoHideBehavior::kAlways);
  SetShelfAlignmentPref(prefs, external_display_id, ShelfAlignment::kBottom);
  SetShelfAutoHideBehaviorPref(prefs, external_display_id,
                               ShelfAutoHideBehavior::kNever);
  ASSERT_EQ(ShelfAlignment::kLeft, primary_shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            primary_shelf->auto_hide_behavior());
  ASSERT_EQ(ShelfAlignment::kBottom, secondary_shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kNever,
            secondary_shelf->auto_hide_behavior());

  // Maximized test window on external display.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window_util::MoveWindowToDisplay(window.get(), external_display_id);
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(800, 0, 900, 652), window->GetBoundsInScreen());

  // Lock the session.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->SetSessionState(session_manager::SessionState::LOCKED);

  // Emulate the docked mode, which turns off the internal display only.
  display_info_list.clear();
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2, GetPrimaryDisplay().id());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Disconnect external display.
  display_info_list.clear();
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(2, GetPrimaryDisplay().id());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Emulate resuming from sleep state that was in docked mode.
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1, GetPrimaryDisplay().id());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Unlock the session.
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(ShelfAlignment::kLeft, primary_shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            primary_shelf->auto_hide_behavior());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(800, 600), window->GetBoundsInScreen());
}

// Make sure maximized window bounds is correct with external display
// disconnected and reconnected in lock session.
// Please see b/252557955.
TEST_F(WorkspaceLayoutManagerTest,
       WindowBoundsWithExternalDisplayDisconnectedAndReconnected) {
  // Set up two displays.
  const display::ManagedDisplayInfo internal_display_info =
      CreateDisplayInfo(1, gfx::Rect(800, 600));
  const display::ManagedDisplayInfo external_display_info =
      CreateDisplayInfo(2, gfx::Rect(1, 1, 900, 700));
  std::vector<display::ManagedDisplayInfo> display_info_list;
  display_info_list.push_back(internal_display_info);
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  display::test::DisplayManagerTestApi(display_manager())
      .SetFirstDisplayAsInternalDisplay();
  int64_t internal_display_id = GetPrimaryDisplay().id();
  int64_t external_display_id = GetSecondaryDisplay().id();
  EXPECT_EQ(1, GetPrimaryDisplay().id());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  // Set up two shelves.
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  Shelf* primary_shelf = GetPrimaryShelf();
  Shelf* secondary_shelf = Shell::GetAllRootWindowControllers().back()->shelf();
  SetShelfAlignmentPref(prefs, internal_display_id, ShelfAlignment::kLeft);
  SetShelfAutoHideBehaviorPref(prefs, internal_display_id,
                               ShelfAutoHideBehavior::kAlways);
  SetShelfAlignmentPref(prefs, external_display_id, ShelfAlignment::kBottom);
  SetShelfAutoHideBehaviorPref(prefs, external_display_id,
                               ShelfAutoHideBehavior::kAlways);
  ASSERT_EQ(ShelfAlignment::kLeft, primary_shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            primary_shelf->auto_hide_behavior());
  ASSERT_EQ(ShelfAlignment::kBottom, secondary_shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            secondary_shelf->auto_hide_behavior());

  // Maximized test window on external display.
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  window_util::MoveWindowToDisplay(window.get(), external_display_id);
  WindowState* window_state = WindowState::Get(window.get());
  window_state->Maximize();
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(800, 0, 900, 700), window->GetBoundsInScreen());

  // Lock the session.
  TestSessionControllerClient* client = GetSessionControllerClient();
  client->SetSessionState(session_manager::SessionState::LOCKED);

  // Disconnect external display.
  display_info_list.clear();
  display_info_list.push_back(internal_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1, GetPrimaryDisplay().id());
  EXPECT_EQ(1U, display_manager()->num_connected_displays());
  EXPECT_EQ(1U, display_manager()->GetNumDisplays());

  // Reconnect external display.
  display_info_list.push_back(external_display_info);
  display_manager()->OnNativeDisplaysChanged(display_info_list);
  EXPECT_EQ(1, GetPrimaryDisplay().id());
  EXPECT_EQ(2U, display_manager()->num_connected_displays());
  EXPECT_EQ(2U, display_manager()->GetNumDisplays());

  // Unlock the session.
  client->SetSessionState(session_manager::SessionState::ACTIVE);
  ASSERT_EQ(ShelfAlignment::kLeft, primary_shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            primary_shelf->auto_hide_behavior());
  secondary_shelf = Shell::GetAllRootWindowControllers().back()->shelf();
  ASSERT_EQ(ShelfAlignment::kBottom, secondary_shelf->alignment());
  ASSERT_EQ(ShelfAutoHideBehavior::kAlways,
            secondary_shelf->auto_hide_behavior());
  EXPECT_TRUE(window_state->IsMaximized());
  EXPECT_EQ(gfx::Rect(800, 0, 900, 700), window->GetBoundsInScreen());
}

TEST_F(WorkspaceLayoutManagerTest,
       PipReturnsToPositionAfterNotificationIsDismissed) {
  // Create a new PiP window using TestWindowBuilder().
  // Set SetShow to false upon creation to simulate the window being created
  // as a PiP rather than being changed to PiP.
  std::unique_ptr<aura::Window> pip_window(
      TestWindowBuilder()
          .AllowAllWindowStates()
          .SetBounds(gfx::Rect(432, 416, 360, 128))
          .SetShow(false)
          .Build()
          .release());
  WindowState* window_state = WindowState::Get(pip_window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  pip_window->SetProperty(aura::client::kZOrderingKey,
                          ui::ZOrderLevel::kFloatingWindow);
  EXPECT_TRUE(window_state->IsPip());
  gfx::Rect old_bounds = pip_window->GetBoundsInScreen();
  pip_window->Show();

  // Create a new popup notification window.
  std::string notification_id("notification_id");
  const message_center::NotifierId notifier_id(
      message_center::NotifierType::APPLICATION, kTestAppId);
  std::unique_ptr<message_center::Notification> notification =
      std::make_unique<message_center::Notification>(
          message_center::NOTIFICATION_TYPE_SIMPLE, notification_id,
          u"Test Web Notification", u"Notification message body.",
          ui::ImageModel(), u"www.test.org", GURL(), notifier_id,
          message_center::RichNotificationData(), /*delegate=*/nullptr);
  message_center::MessageCenter::Get()->AddNotification(
      std::move(notification));
  MessagePopupAnimationWaiter(
      GetPrimaryNotificationCenterTray()->popup_collection())
      .Wait();

  // PiP window has moved due to the popup notification window.
  EXPECT_NE(old_bounds, pip_window->GetBoundsInScreen());

  // Remove the popup notification window.
  message_center::MessageCenter::Get()->RemoveNotification(notification_id,
                                                           /*by_user=*/true);
  MessagePopupAnimationWaiter(
      GetPrimaryNotificationCenterTray()->popup_collection())
      .Wait();

  // Now, the PiP window has returned to its original position.
  EXPECT_EQ(old_bounds, pip_window->GetBoundsInScreen());
}

// Tests no crash after keyboard bounds change. Regression test for
// b/325673844.
TEST_F(WorkspaceLayoutManagerTest,
       NoCrashAfterKeyboardDisplacingBoundsChanged) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  WindowState* window_state = WindowState::Get(window.get());
  const WindowSnapWMEvent snap_left(WM_EVENT_SNAP_PRIMARY);
  window_state->OnWMEvent(&snap_left);
  const gfx::Rect bounds_without_vk(window->GetBoundsInScreen());

  // Show the virtual keyboard. Test the bounds are updated.
  SetVirtualKeyboardEnabled(true);
  auto* keyboard_controller = keyboard::KeyboardUIController::Get();
  keyboard_controller->ShowKeyboard(true);
  EXPECT_TRUE(window_state->IsSnapped());
  gfx::Rect bounds_with_vk(GetPrimaryDisplay().work_area());
  bounds_with_vk.set_width(bounds_with_vk.width() / 2);
  EXPECT_EQ(bounds_with_vk, window->GetBoundsInScreen());
  EXPECT_NE(bounds_without_vk, bounds_with_vk);

  // Hide the virtual keyboard. Test the bounds are restored to the initial
  // snapped bounds.
  keyboard_controller->HideKeyboardByUser();
  EXPECT_TRUE(window_state->IsSnapped());
  EXPECT_EQ(bounds_without_vk, window->GetBoundsInScreen());

  // Test that click on the caption button does not crash.
  const gfx::Rect window_bounds(window->GetBoundsInScreen());
  const gfx::Point drag_point(window_bounds.CenterPoint().x(),
                              window_bounds.y() + 10);
  auto* event_generator = GetEventGenerator();
  event_generator->set_current_screen_location(drag_point);
  event_generator->ClickLeftButton();
}

// Tests that when a window is snapped, then maximized, then dragged out to an
// area *not* to snap, the window is *not* restored to snap state. Regression
// test for http://b/324477985.
TEST_F(WorkspaceLayoutManagerTest, DragToSnapThenMaximize) {
  UpdateDisplay("800x600");

  auto get_drag_point = [](aura::Window* win) {
    auto* frame = NonClientFrameViewAsh::Get(win);
    return frame->GetHeaderView()->GetBoundsInScreen().CenterPoint();
  };

  // Create a normal window, then drag to snap to the right.
  std::unique_ptr<aura::Window> window(CreateAppWindow());
  const gfx::Rect normal_bounds(window->GetBoundsInScreen());
  ASSERT_EQ(gfx::Rect(0, 0, 300, 300), normal_bounds);
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(window.get());
  auto* event_generator = GetEventGenerator();
  event_generator->MoveMouseTo(get_drag_point(window.get()));
  event_generator->DragMouseTo(work_area.right_center());
  WindowState* window_state = WindowState::Get(window.get());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state->GetStateType());
  gfx::Rect snapped_bounds(work_area);
  snapped_bounds.set_x(work_area.x() + work_area.width() / 2);
  snapped_bounds.set_width(work_area.width() / 2);
  ASSERT_EQ(snapped_bounds, window->GetBoundsInScreen());
  ASSERT_NE(normal_bounds, snapped_bounds);

  // Maximize the window. The window will have snapped restore state.
  window_state->Maximize();
  ASSERT_EQ(work_area, window->GetBoundsInScreen());
  EXPECT_EQ(WindowStateType::kMaximized, window_state->GetStateType());
  EXPECT_EQ(WindowStateType::kSecondarySnapped,
            window_state->GetRestoreWindowState());

  // Drag the maximized window to the center of the work area.
  event_generator->MoveMouseTo(get_drag_point(window.get()));
  event_generator->DragMouseTo(work_area.CenterPoint());
  EXPECT_NE(WindowStateType::kSecondarySnapped, window_state->GetStateType());
  EXPECT_NE(snapped_bounds, window->GetBoundsInScreen());
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetStateType());
  EXPECT_EQ(WindowStateType::kNormal, window_state->GetRestoreWindowState());

  // Drag the window to snap again.
  event_generator->MoveMouseTo(get_drag_point(window.get()));
  event_generator->DragMouseTo(work_area.right_center());
  EXPECT_EQ(WindowStateType::kSecondarySnapped, window_state->GetStateType());
}

// Following "Solo" tests were originally written for BaseLayoutManager.
using WorkspaceLayoutManagerSoloTest = AshTestBase;

// Tests normal->maximize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Maximize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  // Maximized window fills the work area, not the whole display.
  EXPECT_EQ(
      screen_util::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Tests normal->minimize->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Minimize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMinimized);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_EQ(bounds, window->bounds());
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  EXPECT_TRUE(window->IsVisible());
  EXPECT_FALSE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_EQ(bounds, window->bounds());
}

// Tests that activation of a minimized window unminimizes the window.
TEST_F(WorkspaceLayoutManagerSoloTest, UnminimizeWithActivation) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  WindowState::Get(window.get())->Minimize();
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(WindowState::Get(window.get())->IsActive());
  WindowState::Get(window.get())->Activate();
  EXPECT_FALSE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(window.get())->IsActive());
}

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
    window_state_request_count_++;
  }

  void HandleBoundsRequest(WindowState* window_state,
                           WindowStateType requested_state,
                           const gfx::Rect& bounds,
                           int64_t display_id) override {}

  int window_state_request_count() { return window_state_request_count_; }

 private:
  int window_state_request_count_{0};
};

// Tests that activation of a minimized client-controlled window requests
// minimization of the window.
TEST_F(WorkspaceLayoutManagerSoloTest,
       UnminimizeClientControlledWithActivation) {
  std::unique_ptr<aura::Window> window = CreateTestWindow();
  WindowState::Get(window.get())->Minimize();
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_FALSE(WindowState::Get(window.get())->IsActive());

  // Make the window client-controlled.
  auto d = std::make_unique<TestClientControlledStateDelegate>();
  auto* delegate = d.get();
  auto state = std::make_unique<ClientControlledState>(std::move(d));
  WindowState* window_state = WindowState::Get(window.get());
  // Make sure that the window is minimized.
  state->EnterNextState(window_state, WindowStateType::kMinimized);
  window_state->SetStateObject(std::move(state));

  window_state->Activate();
  // As it's client controlled, the window isn't restored yet.
  EXPECT_TRUE(WindowState::Get(window.get())->IsMinimized());
  EXPECT_TRUE(WindowState::Get(window.get())->IsActive());
  EXPECT_EQ(1, delegate->window_state_request_count());
}

// A aura::WindowObserver which sets the focus when the window becomes visible.
class FocusDuringUnminimizeWindowObserver : public aura::WindowObserver {
 public:
  FocusDuringUnminimizeWindowObserver()
      : window_(nullptr), show_state_(ui::mojom::WindowShowState::kEnd) {}

  FocusDuringUnminimizeWindowObserver(
      const FocusDuringUnminimizeWindowObserver&) = delete;
  FocusDuringUnminimizeWindowObserver& operator=(
      const FocusDuringUnminimizeWindowObserver&) = delete;

  ~FocusDuringUnminimizeWindowObserver() override { SetWindow(nullptr); }

  void SetWindow(aura::Window* window) {
    if (window_)
      window_->RemoveObserver(this);
    window_ = window;
    if (window_)
      window_->AddObserver(this);
  }

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window_) {
      if (visible)
        aura::client::GetFocusClient(window_)->FocusWindow(window_);
      show_state_ = window_->GetProperty(aura::client::kShowStateKey);
    }
  }

  ui::mojom::WindowShowState GetShowStateAndReset() {
    ui::mojom::WindowShowState ret = show_state_;
    show_state_ = ui::mojom::WindowShowState::kEnd;
    return ret;
  }

 private:
  raw_ptr<aura::Window> window_;
  ui::mojom::WindowShowState show_state_;
};

// Make sure that the window's show state is correct in
// WindowObserver::OnWindowTargetVisibilityChanged(), and setting focus in this
// callback doesn't cause DCHECK error.  See crbug.com/168383.
TEST_F(WorkspaceLayoutManagerSoloTest, FocusDuringUnminimize) {
  FocusDuringUnminimizeWindowObserver observer;
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(100, 100, 100, 100)));
  observer.SetWindow(window.get());
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMinimized);
  EXPECT_FALSE(window->IsVisible());
  EXPECT_EQ(ui::mojom::WindowShowState::kMinimized,
            observer.GetShowStateAndReset());
  window->Show();
  EXPECT_TRUE(window->IsVisible());
  EXPECT_EQ(ui::mojom::WindowShowState::kNormal,
            observer.GetShowStateAndReset());
  observer.SetWindow(nullptr);
}

// Tests maximized window size during root window resize.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  gfx::Rect initial_work_area_bounds =
      screen_util::GetMaximizedWindowBoundsInParent(window.get());
  EXPECT_EQ(initial_work_area_bounds.ToString(), window->bounds().ToString());
  // Enlarge the root window.  We should still match the work area size.
  UpdateDisplay("900x700");
  EXPECT_EQ(
      screen_util::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window->bounds().ToString());
  EXPECT_NE(
      initial_work_area_bounds.ToString(),
      screen_util::GetMaximizedWindowBoundsInParent(window.get()).ToString());
}

// Tests normal->fullscreen->normal.
TEST_F(WorkspaceLayoutManagerSoloTest, Fullscreen) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  // Fullscreen window fills the whole display.
  EXPECT_EQ(GetDisplayNearestWindow(window.get()).bounds().ToString(),
            window->bounds().ToString());
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Tests that fullscreen window causes always_on_top windows to stack below.
TEST_F(WorkspaceLayoutManagerSoloTest, FullscreenSuspendsAlwaysOnTop) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> fullscreen_window(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> always_on_top_window1(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> always_on_top_window2(
      CreateTestWindowInShellWithBounds(bounds));
  always_on_top_window1->SetProperty(aura::client::kZOrderingKey,
                                     ui::ZOrderLevel::kFloatingWindow);
  always_on_top_window2->SetProperty(aura::client::kZOrderingKey,
                                     ui::ZOrderLevel::kFloatingWindow);
  // Making a window fullscreen temporarily suspends always on top state.
  fullscreen_window->SetProperty(aura::client::kShowStateKey,
                                 ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            always_on_top_window1->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            always_on_top_window2->GetProperty(aura::client::kZOrderingKey));
  EXPECT_NE(nullptr,
            GetWindowForFullscreenModeForContext(fullscreen_window.get()));

  // Adding a new always-on-top window is not affected by fullscreen.
  std::unique_ptr<aura::Window> always_on_top_window3(
      CreateTestWindowInShellWithBounds(bounds));
  always_on_top_window3->SetProperty(aura::client::kZOrderingKey,
                                     ui::ZOrderLevel::kFloatingWindow);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window3->GetProperty(aura::client::kZOrderingKey));

  // Making fullscreen window normal restores always on top windows.
  fullscreen_window->SetProperty(aura::client::kShowStateKey,
                                 ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window1->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window2->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window3->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(nullptr,
            GetWindowForFullscreenModeForContext(fullscreen_window.get()));
}

TEST_F(WorkspaceLayoutManagerSoloTest,
       FullscreenDoesNotSuspendAlwaysOnTopForPip) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> fullscreen_window(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> pip_window(
      CreateTestWindowInShellWithBounds(bounds));

  WindowState* window_state = WindowState::Get(pip_window.get());
  const WMEvent enter_pip(WM_EVENT_PIP);
  window_state->OnWMEvent(&enter_pip);
  pip_window->SetProperty(aura::client::kZOrderingKey,
                          ui::ZOrderLevel::kFloatingWindow);
  EXPECT_TRUE(window_state->IsPip());
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            pip_window->GetProperty(aura::client::kZOrderingKey));

  // Making a window fullscreen temporarily suspends always on top state, but
  // should not do so for PIP.
  fullscreen_window->SetProperty(aura::client::kShowStateKey,
                                 ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            pip_window->GetProperty(aura::client::kZOrderingKey));
  EXPECT_NE(nullptr,
            GetWindowForFullscreenModeForContext(fullscreen_window.get()));

  // Making fullscreen window normal does not affect PIP.
  fullscreen_window->SetProperty(aura::client::kShowStateKey,
                                 ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            pip_window->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(nullptr,
            GetWindowForFullscreenModeForContext(fullscreen_window.get()));
}

// Similary, pinned window causes always_on_top_ windows to stack below.
TEST_F(WorkspaceLayoutManagerSoloTest, PinnedSuspendsAlwaysOnTop) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> pinned_window(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> always_on_top_window1(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> always_on_top_window2(
      CreateTestWindowInShellWithBounds(bounds));
  always_on_top_window1->SetProperty(aura::client::kZOrderingKey,
                                     ui::ZOrderLevel::kFloatingWindow);
  always_on_top_window2->SetProperty(aura::client::kZOrderingKey,
                                     ui::ZOrderLevel::kFloatingWindow);

  // Making a window pinned temporarily suspends always on top state.
  const bool trusted = false;
  window_util::PinWindow(pinned_window.get(), trusted);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            always_on_top_window1->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            always_on_top_window2->GetProperty(aura::client::kZOrderingKey));

  // Adding a new always-on-top window also is affected by pinned mode.
  std::unique_ptr<aura::Window> always_on_top_window3(
      CreateTestWindowInShellWithBounds(bounds));
  always_on_top_window3->SetProperty(aura::client::kZOrderingKey,
                                     ui::ZOrderLevel::kFloatingWindow);
  EXPECT_EQ(ui::ZOrderLevel::kNormal,
            always_on_top_window3->GetProperty(aura::client::kZOrderingKey));

  // Making pinned window normal restores always on top windows.
  WindowState::Get(pinned_window.get())->Restore();
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window1->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window2->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            always_on_top_window3->GetProperty(aura::client::kZOrderingKey));
}

TEST_F(WorkspaceLayoutManagerSoloTest, PinnedDoesNotSuspendAlwaysOnTopForPip) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> pinned_window(
      CreateTestWindowInShellWithBounds(bounds));
  std::unique_ptr<aura::Window> pip_window(
      CreateTestWindowInShellWithBounds(bounds));
  {
    WindowState* window_state = WindowState::Get(pip_window.get());
    const WMEvent enter_pip(WM_EVENT_PIP);
    window_state->OnWMEvent(&enter_pip);
    pip_window->SetProperty(aura::client::kZOrderingKey,
                            ui::ZOrderLevel::kFloatingWindow);
    EXPECT_TRUE(window_state->IsPip());
    EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
              pip_window->GetProperty(aura::client::kZOrderingKey));
  }

  // Making a window pinned temporarily suspends always on top state, except
  // for PIP.
  const bool trusted = false;
  window_util::PinWindow(pinned_window.get(), trusted);
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            pip_window->GetProperty(aura::client::kZOrderingKey));

  // Adding a new PIP window should still end up always on top.
  std::unique_ptr<aura::Window> pip_window2(
      CreateTestWindowInShellWithBounds(bounds));
  {
    WindowState* window_state = WindowState::Get(pip_window2.get());
    const WMEvent enter_pip(WM_EVENT_PIP);
    window_state->OnWMEvent(&enter_pip);
    pip_window2->SetProperty(aura::client::kZOrderingKey,
                             ui::ZOrderLevel::kFloatingWindow);
    EXPECT_TRUE(window_state->IsPip());
    EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
              pip_window2->GetProperty(aura::client::kZOrderingKey));
  }

  // Making pinned window normal should not affect existing PIP windows.
  WindowState::Get(pinned_window.get())->Restore();
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            pip_window->GetProperty(aura::client::kZOrderingKey));
  EXPECT_EQ(ui::ZOrderLevel::kFloatingWindow,
            pip_window2->GetProperty(aura::client::kZOrderingKey));
}

// Tests fullscreen window size during root window resize.
TEST_F(WorkspaceLayoutManagerSoloTest, FullscreenRootWindowResize) {
  gfx::Rect bounds(100, 100, 200, 200);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(bounds));
  // Fullscreen window fills the whole display.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kFullscreen);
  EXPECT_EQ(GetDisplayNearestWindow(window.get()).bounds().ToString(),
            window->bounds().ToString());
  // Enlarge the root window.  We should still match the display size.
  UpdateDisplay("800x600");
  EXPECT_EQ(GetDisplayNearestWindow(window.get()).bounds().ToString(),
            window->bounds().ToString());
}

// Tests that when the screen gets smaller the windows aren't bigger than
// the screen.
TEST_F(WorkspaceLayoutManagerSoloTest, RootWindowResizeShrinksWindows) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(10, 20, 500, 400)));
  gfx::Rect work_area = GetDisplayNearestWindow(window.get()).work_area();
  // Invariant: Window is smaller than work area.
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window narrower than our window.
  UpdateDisplay("300x400");
  work_area = GetDisplayNearestWindow(window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Make the root window shorter than our window.
  UpdateDisplay("300x200");
  work_area = GetDisplayNearestWindow(window.get()).work_area();
  EXPECT_LE(window->bounds().width(), work_area.width());
  EXPECT_LE(window->bounds().height(), work_area.height());

  // Enlarging the root window does not change the window bounds.
  gfx::Rect old_bounds = window->bounds();
  UpdateDisplay("800x600");
  EXPECT_EQ(old_bounds.width(), window->bounds().width());
  EXPECT_EQ(old_bounds.height(), window->bounds().height());
}

// Verifies maximizing sets the restore bounds, and restoring
// restores the bounds.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeSetsRestoreBounds) {
  const gfx::Rect initial_bounds(10, 20, 30, 40);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(initial_bounds));
  EXPECT_EQ(initial_bounds, window->bounds());
  WindowState* window_state = WindowState::Get(window.get());

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ("10,20 30x40", window_state->GetRestoreBoundsInParent().ToString());

  // Restore it, which should restore bounds and reset restore bounds.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  EXPECT_EQ("10,20 30x40", window->bounds().ToString());
  EXPECT_FALSE(window_state->HasRestoreBounds());
}

// Verifies maximizing keeps the restore bounds if set.
TEST_F(WorkspaceLayoutManagerSoloTest, MaximizeResetsRestoreBounds) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  WindowState* window_state = WindowState::Get(window.get());
  window_state->SetRestoreBoundsInParent(gfx::Rect(10, 11, 12, 13));

  // Maximize it, which will keep the previous restore bounds.
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kMaximized);
  EXPECT_EQ("10,11 12x13", window_state->GetRestoreBoundsInParent().ToString());
}

// Verifies that the restore bounds do not get reset when restoring to a
// maximzied state from a minimized state.
TEST_F(WorkspaceLayoutManagerSoloTest,
       BoundsAfterRestoringToMaximizeFromMinimize) {
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  gfx::Rect bounds(10, 15, 25, 35);
  window->SetBounds(bounds);

  WindowState* window_state = WindowState::Get(window.get());
  // Maximize it, which should reset restore bounds.
  window_state->Maximize();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());
  // Minimize the window. The restore bounds should not change.
  window_state->Minimize();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());

  // Show the window again. The window should be maximized, and the restore
  // bounds should not change.
  window->Show();
  EXPECT_EQ(bounds.ToString(),
            window_state->GetRestoreBoundsInParent().ToString());
  EXPECT_TRUE(window_state->IsMaximized());

  window_state->Restore();
  EXPECT_EQ(bounds.ToString(), window->bounds().ToString());
}

// Verify if the window is not resized during screen lock. See: crbug.com/173127
TEST_F(WorkspaceLayoutManagerSoloTest, NotResizeWhenScreenIsLocked) {
  SetCanLockScreen(true);
  std::unique_ptr<aura::Window> window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  // window with AlwaysOnTop will be managed by BaseLayoutManager.
  window->SetProperty(aura::client::kZOrderingKey,
                      ui::ZOrderLevel::kFloatingWindow);
  window->Show();

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);

  window->SetBounds(
      screen_util::GetMaximizedWindowBoundsInParent(window.get()));
  gfx::Rect window_bounds = window->bounds();
  EXPECT_EQ(
      screen_util::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window_bounds.ToString());

  // The window size should not get touched while we are in lock screen.
  GetSessionControllerClient()->LockScreen();
  ShelfLayoutManager* shelf_layout_manager = shelf->shelf_layout_manager();
  shelf_layout_manager->UpdateVisibilityState(/*force_layout=*/false);
  EXPECT_EQ(window_bounds.ToString(), window->bounds().ToString());

  // Coming out of the lock screen the window size should still remain.
  GetSessionControllerClient()->UnlockScreen();
  shelf_layout_manager->UpdateVisibilityState(/*force_layout=*/false);
  EXPECT_EQ(
      screen_util::GetMaximizedWindowBoundsInParent(window.get()).ToString(),
      window_bounds.ToString());
  EXPECT_EQ(window_bounds.ToString(), window->bounds().ToString());
}

// Following tests are written to test the backdrop functionality.

namespace {

WorkspaceLayoutManager* GetWorkspaceLayoutManager(aura::Window* container) {
  return static_cast<WorkspaceLayoutManager*>(container->layout_manager());
}

class WorkspaceLayoutManagerBackdropTest : public AshTestBase {
 public:
  WorkspaceLayoutManagerBackdropTest() : default_container_(nullptr) {}

  WorkspaceLayoutManagerBackdropTest(
      const WorkspaceLayoutManagerBackdropTest&) = delete;
  WorkspaceLayoutManagerBackdropTest& operator=(
      const WorkspaceLayoutManagerBackdropTest&) = delete;

  ~WorkspaceLayoutManagerBackdropTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("800x600");
    default_container_ = Shell::GetPrimaryRootWindowController()->GetContainer(
        desks_util::GetActiveDeskContainerId());
  }

  // Turn tablet mode on / off.
  void SetTabletModeEnabled(bool enabled) {
    Shell::Get()->tablet_mode_controller()->SetEnabledForTest(enabled);
    ASSERT_EQ(enabled, display::Screen::GetScreen()->InTabletMode());
  }

  aura::Window* CreateTestWindowInParent(aura::Window* root_window) {
    aura::Window* window = new aura::Window(nullptr);
    window->SetProperty(aura::client::kShowStateKey,
                        ui::mojom::WindowShowState::kNormal);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window, root_window, gfx::Rect(),
                                          display::kInvalidDisplayId);
    return window;
  }

  // Return the default container.
  aura::Window* default_container() { return default_container_; }

  SplitViewController* split_view_controller() {
    return SplitViewController::Get(Shell::GetPrimaryRootWindow());
  }

  // Return the order of windows (top most first) as they are in the default
  // container. If the window is visible it will be a big letter, otherwise a
  // small one. The backdrop will be an X and unknown windows will be shown as
  // '!'.
  std::string GetWindowOrderAsString(aura::Window* backdrop,
                                     aura::Window* wa,
                                     aura::Window* wb,
                                     aura::Window* wc) {
    std::string result;
    aura::Window::Windows children = default_container()->children();
    for (int i = static_cast<int>(children.size()) - 1; i >= 0; --i) {
      if (!result.empty())
        result += ",";
      if (children[i] == wa)
        result += children[i]->IsVisible() ? "A" : "a";
      else if (children[i] == wb)
        result += children[i]->IsVisible() ? "B" : "b";
      else if (children[i] == wc)
        result += children[i]->IsVisible() ? "C" : "c";
      else if (children[i] == backdrop)
        result += children[i]->IsVisible() ? "X" : "x";
      else
        result += "!";
    }
    return result;
  }

 private:
  // The default container.
  raw_ptr<aura::Window, DanglingUntriaged> default_container_;
};

constexpr std::optional<Sound> kNoSoundKey = std::nullopt;

}  // namespace

// Check that creating the BackDrop without destroying it does not lead into
// a crash.
TEST_F(WorkspaceLayoutManagerBackdropTest, BackdropCrashTest) {
  SetTabletModeEnabled(true);
}

// Verify basic assumptions about the backdrop.
TEST_F(WorkspaceLayoutManagerBackdropTest, BasicBackdropTests) {
  // The background widget will be created when there is a window.
  SetTabletModeEnabled(true);
  ASSERT_EQ(0u, default_container()->children().size());

  {
    // Add a window and make sure that the backdrop is the second child.
    std::unique_ptr<aura::Window> window(
        CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
    window->Show();
    ASSERT_EQ(2U, default_container()->children().size());
    EXPECT_TRUE(default_container()->children()[0]->IsVisible());
    EXPECT_TRUE(default_container()->children()[1]->IsVisible());
    EXPECT_EQ(window.get(), default_container()->children()[1]);
    EXPECT_EQ(default_container()->bounds().ToString(),
              default_container()->children()[0]->bounds().ToString());
  }

  // With the window gone the backdrop should be destroyed.
  ASSERT_EQ(0U, default_container()->children().size());
}

// Verify that the backdrop gets properly created and placed.
TEST_F(WorkspaceLayoutManagerBackdropTest, VerifyBackdropAndItsStacking) {
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  window1->Show();

  // Get the default container and check that only a single window is in there.
  ASSERT_EQ(1U, default_container()->children().size());
  EXPECT_EQ(window1.get(), default_container()->children()[0]);
  EXPECT_EQ("A",
            GetWindowOrderAsString(nullptr, window1.get(), nullptr, nullptr));

  // Create 2 more windows and check that they are also in the container.
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(10, 2, 3, 4)));
  std::unique_ptr<aura::Window> window3(
      CreateTestWindow(gfx::Rect(20, 2, 3, 4)));
  window2->Show();
  window3->Show();

  aura::Window* backdrop = nullptr;
  EXPECT_EQ("C,B,A", GetWindowOrderAsString(backdrop, window1.get(),
                                            window2.get(), window3.get()));

  // Enter tablet mode and check that the backdrop window shows up where it
  // should be (second highest number).
  SetTabletModeEnabled(true);
  backdrop = default_container()->children()[2];
  EXPECT_EQ("C,X,B,A", GetWindowOrderAsString(backdrop, window1.get(),
                                              window2.get(), window3.get()));

  // Switch the order of windows and check that it still remains in that
  // location.
  default_container()->StackChildAtTop(window2.get());
  EXPECT_EQ("B,X,C,A", GetWindowOrderAsString(backdrop, window1.get(),
                                              window2.get(), window3.get()));

  // Make the top window invisible and check.
  window2->Hide();
  EXPECT_EQ("b,C,X,A", GetWindowOrderAsString(backdrop, window1.get(),
                                              window2.get(), window3.get()));
  // Then delete window after window and see that everything is in order.
  window1.reset();
  EXPECT_EQ("b,C,X", GetWindowOrderAsString(backdrop, window1.get(),
                                            window2.get(), window3.get()));
  window3.reset();
  EXPECT_EQ("b", GetWindowOrderAsString(nullptr, window1.get(), window2.get(),
                                        window3.get()));
}

// Tests that when hidding the shelf, that the backdrop stays fullscreen.
TEST_F(WorkspaceLayoutManagerBackdropTest,
       ShelfVisibilityDoesNotChangesBounds) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* shelf_layout_manager = shelf->shelf_layout_manager();
  SetTabletModeEnabled(true);
  base::RunLoop().RunUntilIdle();
  const gfx::Size fullscreen_size = GetPrimaryDisplay().size();

  std::unique_ptr<aura::Window> window(CreateTestWindow(gfx::Rect(1, 2, 3, 4)));
  window->Show();

  ASSERT_EQ(SHELF_VISIBLE, shelf_layout_manager->visibility_state());

  EXPECT_EQ(fullscreen_size,
            default_container()->children()[0]->bounds().size());
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);
  shelf_layout_manager->UpdateVisibilityState(/*force_layout=*/false);

  // When the shelf is re-shown WorkspaceLayoutManager shrinks all children but
  // the backdrop.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  shelf_layout_manager->UpdateVisibilityState(/*force_layout=*/false);
  EXPECT_EQ(fullscreen_size,
            default_container()->children()[0]->bounds().size());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlwaysHidden);
  shelf_layout_manager->UpdateVisibilityState(/*force_layout=*/false);
  EXPECT_EQ(fullscreen_size,
            default_container()->children()[0]->bounds().size());
}

TEST_F(WorkspaceLayoutManagerBackdropTest, BackdropTest) {
  WorkspaceController* wc = ShellTestApi().workspace_controller();
  WorkspaceControllerTestApi test_helper(wc);

  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window1->SetName("1");
  window1->Show();
  std::unique_ptr<aura::Window> window2(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window2->SetName("2");
  window2->Show();
  std::unique_ptr<aura::Window> window3(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  window3->SetName("3");
  window3->Show();
  EXPECT_FALSE(test_helper.GetBackdropWindow());

  WindowBackdrop::Get(window2.get())
      ->SetBackdropMode(WindowBackdrop::BackdropMode::kEnabled);
  aura::Window* backdrop = test_helper.GetBackdropWindow();
  EXPECT_TRUE(backdrop);
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(backdrop, children[1]);
    EXPECT_EQ(window2.get(), children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Setting the property to the one below the backdrop window shouldn't change
  // the state.
  WindowBackdrop::Get(window1.get())
      ->SetBackdropMode(WindowBackdrop::BackdropMode::kEnabled);
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(backdrop, children[1]);
    EXPECT_EQ(window2.get(), children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Setting the property to the top will move the backdrop up.
  WindowBackdrop::Get(window3.get())
      ->SetBackdropMode(WindowBackdrop::BackdropMode::kEnabled);
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(window2.get(), children[1]);
    EXPECT_EQ(backdrop, children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Disabling the backdrop in the middle will not change the backdrop position.
  WindowBackdrop::Get(window2.get())->DisableBackdrop();
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(window2.get(), children[1]);
    EXPECT_EQ(backdrop, children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Disabling the backdrop on top will move the backdrop to bottom.
  WindowBackdrop::Get(window3.get())->DisableBackdrop();
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(backdrop, children[0]);
    EXPECT_EQ(window1.get(), children[1]);
    EXPECT_EQ(window2.get(), children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Toggle overview.
  EnterOverview();
  base::RunLoop().RunUntilIdle();
  backdrop = test_helper.GetBackdropWindow();
  ASSERT_TRUE(backdrop);
  EXPECT_FALSE(backdrop->IsVisible());

  ExitOverview();
  base::RunLoop().RunUntilIdle();
  backdrop = test_helper.GetBackdropWindow();
  ASSERT_TRUE(backdrop);
  EXPECT_TRUE(backdrop->IsVisible());
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(backdrop, children[0]);
    EXPECT_EQ(window1.get(), children[1]);
    EXPECT_EQ(window2.get(), children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Enabling tablet mode will put the backdrop on the top most window that can
  // have backdrop.
  SetTabletModeEnabled(true);
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(backdrop, children[0]);
    EXPECT_EQ(window1.get(), children[1]);
    EXPECT_EQ(window2.get(), children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }
  // Restore the backdrop on |window2| and |window3| will update the backdrop.
  WindowBackdrop::Get(window2.get())->RestoreBackdrop();
  WindowBackdrop::Get(window3.get())->RestoreBackdrop();
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(window2.get(), children[1]);
    EXPECT_EQ(backdrop, children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Toggle overview with the delegate.
  EnterOverview();
  base::RunLoop().RunUntilIdle();
  backdrop = test_helper.GetBackdropWindow();
  ASSERT_TRUE(backdrop);
  EXPECT_FALSE(backdrop->IsVisible());
  ExitOverview();
  base::RunLoop().RunUntilIdle();
  backdrop = test_helper.GetBackdropWindow();
  ASSERT_TRUE(backdrop);
  EXPECT_TRUE(backdrop->IsVisible());
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(window2.get(), children[1]);
    EXPECT_EQ(backdrop, children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Exiting tablet mode will move the backdrop back to window3 as window3 is
  // the top most window with kEnabled backdrop mode.
  SetTabletModeEnabled(false);
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(window2.get(), children[1]);
    EXPECT_EQ(backdrop, children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Re-enter tablet mode. window3 still has the backdrop.
  SetTabletModeEnabled(true);
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(window2.get(), children[1]);
    EXPECT_EQ(backdrop, children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }

  // Explicitly disable backdrop on window3 will move the backdrop to window2.
  WindowBackdrop::Get(window3.get())->DisableBackdrop();
  {
    aura::Window::Windows children = window1->parent()->children();
    EXPECT_EQ(4U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(backdrop, children[1]);
    EXPECT_EQ(window2.get(), children[2]);
    EXPECT_EQ(window3.get(), children[3]);
  }
}

TEST_F(WorkspaceLayoutManagerBackdropTest,
       DoNotShowBackdropDuringWallpaperPreview) {
  WorkspaceController* wc = ShellTestApi().workspace_controller();
  WorkspaceControllerTestApi test_helper(wc);
  WallpaperControllerTestApi wallpaper_test_api(
      Shell::Get()->wallpaper_controller());

  std::unique_ptr<aura::Window> wallpaper_picker_window(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  std::unique_ptr<aura::Window> window1(
      CreateTestWindow(gfx::Rect(0, 0, 100, 100)));
  WindowState::Get(wallpaper_picker_window.get())->Activate();

  // Enter tablet mode. The backdrop is shown behind the wallpaper picker
  // window.
  SetTabletModeEnabled(true);
  aura::Window* backdrop = test_helper.GetBackdropWindow();
  {
    aura::Window::Windows children =
        wallpaper_picker_window->parent()->children();
    EXPECT_EQ(3U, children.size());
    EXPECT_EQ(window1.get(), children[0]);
    EXPECT_EQ(backdrop, children[1]);
    EXPECT_EQ(wallpaper_picker_window.get(), children[2]);
  }

  // Start wallpaper preview. The backdrop should move to window1.
  wallpaper_test_api.StartWallpaperPreview();
  {
    aura::Window::Windows children =
        wallpaper_picker_window->parent()->children();
    EXPECT_EQ(3U, children.size());
    EXPECT_EQ(backdrop, children[0]);
    EXPECT_EQ(window1.get(), children[1]);
    EXPECT_EQ(wallpaper_picker_window.get(), children[2]);
  }
}

TEST_F(WorkspaceLayoutManagerBackdropTest, SpokenFeedbackFullscreenBackground) {
  WorkspaceController* wc = ShellTestApi().workspace_controller();
  WorkspaceControllerTestApi test_helper(wc);
  AccessibilityController* controller =
      Shell::Get()->accessibility_controller();
  TestAccessibilityControllerClient client;

  aura::test::TestWindowDelegate delegate;
  std::unique_ptr<aura::Window> window(CreateTestWindowInShellWithDelegate(
      &delegate, 0, gfx::Rect(0, 0, 100, 100)));
  window->Show();

  WindowBackdrop::Get(window.get())
      ->SetBackdropMode(WindowBackdrop::BackdropMode::kEnabled);
  EXPECT_TRUE(test_helper.GetBackdropWindow());

  ui::test::EventGenerator* generator = GetEventGenerator();

  generator->MoveMouseTo(300, 300);
  generator->ClickLeftButton();
  EXPECT_EQ(kNoSoundKey, client.GetPlayedEarconAndReset());

  generator->MoveMouseRelativeTo(window.get(), 10, 10);
  generator->ClickLeftButton();
  EXPECT_EQ(kNoSoundKey, client.GetPlayedEarconAndReset());

  // Enable spoken feedback.
  controller->SetSpokenFeedbackEnabled(true, A11Y_NOTIFICATION_NONE);
  EXPECT_TRUE(controller->spoken_feedback().enabled());

  generator->MoveMouseTo(300, 300);
  generator->ClickLeftButton();
  EXPECT_EQ(Sound::kVolumeAdjust, client.GetPlayedEarconAndReset());

  generator->MoveMouseRelativeTo(window.get(), 10, 10);
  generator->ClickLeftButton();
  EXPECT_EQ(kNoSoundKey, client.GetPlayedEarconAndReset());

  // Disable spoken feedback. Shadow underlay is restored.
  controller->SetSpokenFeedbackEnabled(false, A11Y_NOTIFICATION_NONE);
  EXPECT_FALSE(controller->spoken_feedback().enabled());

  generator->MoveMouseTo(300, 300);
  generator->ClickLeftButton();
  EXPECT_EQ(kNoSoundKey, client.GetPlayedEarconAndReset());

  generator->MoveMouseTo(70, 70);
  generator->ClickLeftButton();
  EXPECT_EQ(kNoSoundKey, client.GetPlayedEarconAndReset());
}

class WorkspaceLayoutManagerKeyboardTest : public AshTestBase {
 public:
  WorkspaceLayoutManagerKeyboardTest() : layout_manager_(nullptr) {}

  WorkspaceLayoutManagerKeyboardTest(
      const WorkspaceLayoutManagerKeyboardTest&) = delete;
  WorkspaceLayoutManagerKeyboardTest& operator=(
      const WorkspaceLayoutManagerKeyboardTest&) = delete;

  ~WorkspaceLayoutManagerKeyboardTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("800x600");
    aura::Window* active_desk_container =
        Shell::GetPrimaryRootWindowController()->GetContainer(
            desks_util::GetActiveDeskContainerId());
    layout_manager_ = GetWorkspaceLayoutManager(active_desk_container);
  }

  void ShowKeyboard() {
    layout_manager_->OnKeyboardDisplacingBoundsChanged(keyboard_bounds_);

    aura::Window* root = Shell::GetPrimaryRootWindow();
    WorkAreaInsets* work_area_insets = WorkAreaInsets::ForWindow(root);

    restore_work_area_insets_ =
        work_area_insets->in_session_user_work_area_insets();

    gfx::Insets insets = gfx::Insets::TLBR(0, 0, keyboard_bounds_.height(), 0);
    work_area_insets->UpdateWorkAreaInsetsForTest(root, gfx::Rect(),
                                                  gfx::Insets(), insets);

    ash::KeyboardStateDescriptor state{/*is_visible=*/true,
                                       /*is_temporary=*/false, keyboard_bounds_,
                                       keyboard_bounds_, keyboard_bounds_};
    work_area_insets->OnKeyboardAppearanceChanged(state);
  }

  void HideKeyboard() {
    aura::Window* root = Shell::GetPrimaryRootWindow();
    WorkAreaInsets* work_area_insets = WorkAreaInsets::ForWindow(root);
    work_area_insets->UpdateWorkAreaInsetsForTest(
        root, gfx::Rect(), gfx::Insets(), restore_work_area_insets_);

    ash::KeyboardStateDescriptor state{/*is_visible=*/true,
                                       /*is_temporary=*/false, gfx::Rect(),
                                       gfx::Rect(), gfx::Rect()};
    work_area_insets->OnKeyboardAppearanceChanged(state);

    layout_manager_->OnKeyboardDisplacingBoundsChanged(gfx::Rect());
  }

  // Initializes the keyboard bounds using the bottom half of the work area.
  void InitKeyboardBounds() {
    gfx::Rect work_area(GetPrimaryDisplay().work_area());
    keyboard_bounds_.SetRect(work_area.x(),
                             work_area.y() + work_area.height() / 2,
                             work_area.width(), work_area.height() / 2);
  }

  const gfx::Rect& keyboard_bounds() const { return keyboard_bounds_; }

 private:
  gfx::Insets restore_work_area_insets_;
  gfx::Rect keyboard_bounds_;
  raw_ptr<WorkspaceLayoutManager, DanglingUntriaged> layout_manager_;
};

// Tests that when a child window gains focus the top level window containing it
// is resized to fit the remaining workspace area.
TEST_F(WorkspaceLayoutManagerKeyboardTest, ChildWindowFocused) {
  ScopedStickyKeyboardEnabler sticky_enabler;

  InitKeyboardBounds();

  gfx::Rect work_area(GetPrimaryDisplay().work_area());

  std::unique_ptr<aura::Window> parent_window(
      CreateToplevelTestWindow(work_area));
  std::unique_ptr<aura::Window> window(CreateTestWindow(work_area));
  parent_window->AddChild(window.get());

  wm::ActivateWindow(window.get());

  int available_height =
      GetPrimaryDisplay().bounds().height() - keyboard_bounds().height();

  gfx::Rect initial_window_bounds(50, 50, 100, 500);
  parent_window->SetBounds(initial_window_bounds);
  EXPECT_EQ(initial_window_bounds.ToString(),
            parent_window->bounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(50, 0, 100, available_height).ToString(),
            parent_window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(initial_window_bounds.ToString(),
            parent_window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerKeyboardTest, AdjustWindowForA11yKeyboard) {
  ScopedStickyKeyboardEnabler sticky_enabler;

  InitKeyboardBounds();
  gfx::Rect work_area(GetPrimaryDisplay().work_area());

  std::unique_ptr<aura::Window> window(CreateToplevelTestWindow(work_area));

  int available_height =
      GetPrimaryDisplay().bounds().height() - keyboard_bounds().height();

  wm::ActivateWindow(window.get());

  EXPECT_EQ(gfx::Rect(work_area).ToString(), window->bounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(work_area.origin(),
                      gfx::Size(work_area.width(), available_height))
                .ToString(),
            window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(gfx::Rect(work_area).ToString(), window->bounds().ToString());

  gfx::Rect small_window_bound(50, 50, 100, 500);
  window->SetBounds(small_window_bound);
  EXPECT_EQ(small_window_bound.ToString(), window->bounds().ToString());
  ShowKeyboard();
  EXPECT_EQ(gfx::Rect(50, 0, 100, available_height).ToString(),
            window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(small_window_bound.ToString(), window->bounds().ToString());

  gfx::Rect occluded_window_bounds(
      50, keyboard_bounds().y() + keyboard_bounds().height() / 2, 50,
      keyboard_bounds().height() / 2);
  window->SetBounds(occluded_window_bounds);
  EXPECT_EQ(occluded_window_bounds.ToString(),
            occluded_window_bounds.ToString());
  ShowKeyboard();
  EXPECT_EQ(
      gfx::Rect(50, keyboard_bounds().y() - keyboard_bounds().height() / 2,
                occluded_window_bounds.width(), occluded_window_bounds.height())
          .ToString(),
      window->bounds().ToString());
  HideKeyboard();
  EXPECT_EQ(occluded_window_bounds.ToString(), window->bounds().ToString());
}

TEST_F(WorkspaceLayoutManagerKeyboardTest, IgnoreKeyboardBoundsChange) {
  ScopedStickyKeyboardEnabler sticky_enabler;
  InitKeyboardBounds();

  std::unique_ptr<aura::Window> window(CreateTestWindow(keyboard_bounds()));
  WindowState::Get(window.get())->set_ignore_keyboard_bounds_change(true);
  wm::ActivateWindow(window.get());

  EXPECT_EQ(keyboard_bounds(), window->bounds());
  ShowKeyboard();
  EXPECT_EQ(keyboard_bounds(), window->bounds());
}

// When kAshUseNewVKWindowBehavior flag enabled, do not change accessibility
// keyboard work area in non-sticky mode.
TEST_F(WorkspaceLayoutManagerKeyboardTest,
       IgnoreWorkAreaChangeinNonStickyMode) {
  keyboard::SetAccessibilityKeyboardEnabled(true);
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardUIController::Get();

  gfx::Rect work_area(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area());

  gfx::Rect orig_window_bounds(0, 100, work_area.width(),
                               work_area.height() - 100);
  std::unique_ptr<aura::Window> window(
      CreateToplevelTestWindow(orig_window_bounds));

  wm::ActivateWindow(window.get());
  EXPECT_EQ(orig_window_bounds, window->bounds());

  // Open keyboard in non-sticky mode.
  kb_controller->ShowKeyboard(false);

  // Window should not be shifted up.
  EXPECT_EQ(orig_window_bounds, window->bounds());

  kb_controller->HideKeyboardExplicitlyBySystem();
  EXPECT_EQ(orig_window_bounds, window->bounds());

  // Open keyboard in sticky mode.
  kb_controller->ShowKeyboard(true);
  ASSERT_TRUE(keyboard::test::WaitUntilShown());

  int shift =
      work_area.height() - kb_controller->GetKeyboardWindow()->bounds().y();
  gfx::Rect changed_window_bounds(orig_window_bounds);
  changed_window_bounds.Offset(0, -shift);
  // Window should be shifted up.
  EXPECT_EQ(changed_window_bounds, window->bounds());

  kb_controller->HideKeyboardExplicitlyBySystem();
  EXPECT_EQ(orig_window_bounds, window->bounds());
}

// Test that backdrop works in split view mode.
TEST_F(WorkspaceLayoutManagerBackdropTest, BackdropForSplitViewTest) {
  SetTabletModeEnabled(true);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);

  auto CreateWindow = [this](const gfx::Rect& bounds) {
    aura::Window* window = CreateTestWindowInShellWithDelegate(
        aura::test::TestWindowDelegate::CreateSelfDestroyingDelegate(), -1,
        bounds);
    return window;
  };

  const gfx::Rect bounds(0, 0, 400, 400);
  std::unique_ptr<aura::Window> window1(CreateWindow(bounds));
  window1->Show();

  // Test that backdrop window is visible and is the second child in the
  // container. Its bounds should be the same as the container bounds.
  EXPECT_EQ(2U, default_container()->children().size());
  for (aura::Window* child : default_container()->children()) {
    EXPECT_TRUE(child->IsVisible());
  }

  BackdropController* backdrop_controller =
      GetActiveWorkspaceController(Shell::GetPrimaryRootWindow())
          ->layout_manager()
          ->backdrop_controller();
  ASSERT_EQ(backdrop_controller->backdrop_window(),
            default_container()->children()[0]);

  EXPECT_EQ(window1.get(), default_container()->children()[1]);
  EXPECT_EQ(default_container()->bounds(),
            default_container()->children()[0]->bounds());

  // Snap the window to left. Test that the backdrop window is still visible
  // and is the third child (split view divider as one of the children) in the
  // container. Its bounds should be the same as the snapped window's bounds.
  split_view_controller()->SnapWindow(window1.get(), SnapPosition::kPrimary);

  EXPECT_TRUE(OverviewController::Get()->InOverviewSession());
  // One of the windows in the default container is the overview
  // no_windows_widget window. Exclude it.
  aura::Window::Windows children = default_container()->children();
  children.erase(std::remove_if(children.begin(), children.end(),
                                [](aura::Window* window) {
                                  return window == OverviewController::Get()
                                                       ->overview_session()
                                                       ->grid_list()[0]
                                                       ->no_windows_widget()
                                                       ->GetNativeWindow();
                                }),
                 children.end());
  EXPECT_EQ(3U, children.size());

  // Backdrop is hidden in overview mode. So test the window and backdrop
  // separately.
  EXPECT_FALSE(children[0]->IsVisible());
  EXPECT_TRUE(children[1]->IsVisible());
  EXPECT_EQ(window1.get(), children[1]);
  EXPECT_EQ(window1->bounds(), children[0]->bounds());

  // Now snap another window to right. Test that the backdrop window is still
  // visible but is now the fourth window in the container. Its bounds should
  // still be the same as the container bounds.
  std::unique_ptr<aura::Window> window2(CreateWindow(bounds));
  split_view_controller()->SnapWindow(window2.get(), SnapPosition::kSecondary);

  // The split view divider is also a child of the default desk container. Spin
  // the run loop here so that the post task to close the divider widget when
  // snapping two windows gets run.
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(4U, default_container()->children().size());
  for (aura::Window* child : default_container()->children()) {
    EXPECT_TRUE(child->IsVisible());
  }

  EXPECT_EQ(window1.get(), default_container()->children()[1]);
  EXPECT_EQ(window2.get(), default_container()->children()[2]);
  EXPECT_EQ(default_container()->bounds(),
            default_container()->children()[0]->bounds());

  // Test activation change correctly updates the backdrop.
  wm::ActivateWindow(window1.get());
  EXPECT_EQ(window1.get(), default_container()->children()[2]);
  EXPECT_EQ(window2.get(), default_container()->children()[1]);
  EXPECT_EQ(default_container()->bounds(),
            default_container()->children()[0]->bounds());

  wm::ActivateWindow(window2.get());
  EXPECT_EQ(window1.get(), default_container()->children()[1]);
  EXPECT_EQ(window2.get(), default_container()->children()[2]);
  EXPECT_EQ(default_container()->bounds(),
            default_container()->children()[0]->bounds());
}

TEST_F(WorkspaceLayoutManagerBackdropTest,
       BackdropWindowIsNotReparentedFromAlwaysOnTopContainer) {
  WorkspaceController* wc = ShellTestApi().workspace_controller();
  WorkspaceControllerTestApi test_helper(wc);
  RootWindowController* controller = Shell::GetPrimaryRootWindowController();
  AlwaysOnTopController* always_on_top_controller =
      controller->always_on_top_controller();

  std::unique_ptr<aura::Window> always_on_top_window(
      CreateTestWindowInShellWithBounds(gfx::Rect(1, 2, 3, 4)));
  always_on_top_window->Show();
  always_on_top_window->SetProperty(aura::client::kZOrderingKey,
                                    ui::ZOrderLevel::kFloatingWindow);
  WindowBackdrop::Get(always_on_top_window.get())
      ->SetBackdropMode(WindowBackdrop::BackdropMode::kEnabled);

  aura::Window* always_on_top_container =
      always_on_top_controller->GetContainer(always_on_top_window.get());
  // AlwaysOnTopContainer has |always_on_top_window| and a backdrop window
  // at this moment.
  ASSERT_EQ(always_on_top_container->children().size(), 2U);

  always_on_top_window->SetProperty(aura::client::kZOrderingKey,
                                    ui::ZOrderLevel::kNormal);
  // The backdrop window will be destroyed immediately after
  // |always_on_top_window| moves to the default container.
  EXPECT_TRUE(always_on_top_container->children().empty());
}

}  // namespace ash
