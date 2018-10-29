// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <memory>
#include <utility>

#include "ash/accelerators/accelerator_controller.h"
#include "ash/accelerators/accelerator_table.h"
#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/focus_cycler.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/immersive/immersive_fullscreen_controller_test_api.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_constants.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/shell_test_api.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/status_area_widget_test_helper.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/test_system_tray_item.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/test/ash_test_base.h"
#include "ash/wallpaper/wallpaper_controller.h"
#include "ash/window_factory.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/overview/window_selector_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/scoped_feature_list.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_animation_duration_scale_mode.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/events/test/event_generator.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_ui.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

void StepWidgetLayerAnimatorToEnd(views::Widget* widget) {
  widget->GetNativeView()->layer()->GetAnimator()->Step(
      base::TimeTicks::Now() + base::TimeDelta::FromSeconds(1));
}

ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}

// Class which waits till the shelf finishes animating to the target size and
// counts the number of animation steps.
class ShelfAnimationWaiter : views::WidgetObserver {
 public:
  explicit ShelfAnimationWaiter(const gfx::Rect& target_bounds)
      : target_bounds_(target_bounds),
        animation_steps_(0),
        done_waiting_(false) {
    GetShelfWidget()->AddObserver(this);
  }

  ~ShelfAnimationWaiter() override { GetShelfWidget()->RemoveObserver(this); }

  // Wait till the shelf finishes animating to its expected bounds.
  void WaitTillDoneAnimating() {
    if (IsDoneAnimating())
      done_waiting_ = true;
    else
      base::RunLoop().Run();
  }

  // Returns true if the animation has completed and it was valid.
  bool WasValidAnimation() const {
    return done_waiting_ && animation_steps_ > 0;
  }

 private:
  // Returns true if shelf has finished animating to the target position.
  bool IsDoneAnimating() const {
    gfx::Rect current_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
    return current_bounds.origin() == target_bounds_.origin();
  }

  // views::WidgetObserver override.
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (done_waiting_)
      return;

    ++animation_steps_;
    if (IsDoneAnimating()) {
      done_waiting_ = true;
      base::RunLoop::QuitCurrentWhenIdleDeprecated();
    }
  }

  gfx::Rect target_bounds_;
  int animation_steps_;
  bool done_waiting_;

  DISALLOW_COPY_AND_ASSIGN(ShelfAnimationWaiter);
};

class ShelfDragCallback {
 public:
  ShelfDragCallback(const gfx::Rect& auto_hidden_shelf_bounds,
                    const gfx::Rect& visible_shelf_bounds)
      : auto_hidden_shelf_bounds_(auto_hidden_shelf_bounds),
        visible_shelf_bounds_(visible_shelf_bounds),
        was_visible_on_drag_start_(false) {
    EXPECT_EQ(auto_hidden_shelf_bounds_.size(), visible_shelf_bounds_.size());
  }

  virtual ~ShelfDragCallback() = default;

  void ProcessScroll(ui::EventType type, const gfx::Vector2dF& delta) {
    if (GetShelfLayoutManager()->visibility_state() == SHELF_HIDDEN)
      return;

    if (type == ui::ET_GESTURE_SCROLL_BEGIN) {
      scroll_ = gfx::Vector2dF();
      was_visible_on_drag_start_ = GetShelfLayoutManager()->IsVisible();
      return;
    }

    // The state of the shelf at the end of the gesture is tested separately.
    if (type == ui::ET_GESTURE_SCROLL_END)
      return;

    if (type == ui::ET_GESTURE_SCROLL_UPDATE)
      scroll_.Add(delta);

    Shelf* shelf = AshTestBase::GetPrimaryShelf();
    gfx::Rect shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

    float scroll_delta =
        GetShelfLayoutManager()->PrimaryAxisValue(scroll_.y(), scroll_.x());
    bool increasing_drag =
        GetShelfLayoutManager()->SelectValueForShelfAlignment(
            scroll_delta<0, scroll_delta> 0, scroll_delta < 0);
    const int shelf_size = GetShelfLayoutManager()->PrimaryAxisValue(
        shelf_bounds.height(), shelf_bounds.width());
    if (was_visible_on_drag_start_) {
      if (increasing_drag) {
        // If dragging inwards from the visible state, then the shelf should
        // 'overshoot', but not by more than the scroll delta.
        const int bounds_delta =
            GetShelfLayoutManager()->SelectValueForShelfAlignment(
                visible_shelf_bounds_.y() - shelf_bounds.y(),
                shelf_bounds.x() - visible_shelf_bounds_.x(),
                visible_shelf_bounds_.x() - shelf_bounds.x());
        EXPECT_GE(bounds_delta, 0);
        EXPECT_LE(bounds_delta, std::abs(scroll_delta));
      } else {
        // If dragging outwards from the visible state, then the shelf should
        // move out.
        if (SHELF_ALIGNMENT_BOTTOM == shelf->alignment())
          EXPECT_LE(visible_shelf_bounds_.y(), shelf_bounds.y());
        else if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
          EXPECT_LE(shelf_bounds.x(), visible_shelf_bounds_.x());
        else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment())
          EXPECT_LE(visible_shelf_bounds_.x(), shelf_bounds.x());
      }
    } else {
      // The shelf is invisible at the start of the drag.
      if (increasing_drag) {
        // Moving the shelf into the screen.
        if (std::abs(scroll_delta) < shelf_size) {
          // Tests that the shelf sticks with the touch point during the drag
          // until the shelf is completely visible.
          if (SHELF_ALIGNMENT_BOTTOM == shelf->alignment()) {
            EXPECT_EQ(shelf_bounds.y(), auto_hidden_shelf_bounds_.y() +
                                            kHiddenShelfInScreenPortion -
                                            std::abs(scroll_delta));
          } else if (SHELF_ALIGNMENT_LEFT == shelf->alignment()) {
            EXPECT_EQ(shelf_bounds.x(), auto_hidden_shelf_bounds_.x() -
                                            kHiddenShelfInScreenPortion +
                                            std::abs(scroll_delta));
          } else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment()) {
            EXPECT_EQ(shelf_bounds.x(), auto_hidden_shelf_bounds_.x() +
                                            kHiddenShelfInScreenPortion -
                                            std::abs(scroll_delta));
          }
        } else {
          // Tests that after the shelf is completely visible, the shelf starts
          // resisting the drag.
          if (SHELF_ALIGNMENT_BOTTOM == shelf->alignment()) {
            EXPECT_GT(shelf_bounds.y(), auto_hidden_shelf_bounds_.y() +
                                            kHiddenShelfInScreenPortion -
                                            std::abs(scroll_delta));
          } else if (SHELF_ALIGNMENT_LEFT == shelf->alignment()) {
            EXPECT_LT(shelf_bounds.x(), auto_hidden_shelf_bounds_.x() -
                                            kHiddenShelfInScreenPortion +
                                            std::abs(scroll_delta));
          } else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment()) {
            EXPECT_GT(shelf_bounds.x(), auto_hidden_shelf_bounds_.x() +
                                            kHiddenShelfInScreenPortion -
                                            std::abs(scroll_delta));
          }
        }
      }
    }
  }

 private:
  const gfx::Rect auto_hidden_shelf_bounds_;
  const gfx::Rect visible_shelf_bounds_;
  gfx::Vector2dF scroll_;
  bool was_visible_on_drag_start_;

  DISALLOW_COPY_AND_ASSIGN(ShelfDragCallback);
};

class ShelfLayoutObserverTest : public ShelfLayoutManagerObserver {
 public:
  ShelfLayoutObserverTest() : changed_auto_hide_state_(false) {}

  ~ShelfLayoutObserverTest() override = default;

  bool changed_auto_hide_state() const { return changed_auto_hide_state_; }

 private:
  // ShelfLayoutManagerObserver:
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override {
    changed_auto_hide_state_ = true;
  }

  bool changed_auto_hide_state_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutObserverTest);
};

class WallpaperShownWaiter : public WallpaperControllerObserver {
 public:
  WallpaperShownWaiter() {
    Shell::Get()->wallpaper_controller()->AddObserver(this);
  }

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

  DISALLOW_COPY_AND_ASSIGN(WallpaperShownWaiter);
};

}  // namespace

class ShelfLayoutManagerTest : public AshTestBase {
 public:
  ShelfLayoutManagerTest() = default;

  // Calls the private SetState() function.
  void SetState(ShelfLayoutManager* layout_manager,
                ShelfVisibilityState state) {
    layout_manager->SetState(state);
  }

  void UpdateAutoHideStateNow() {
    GetShelfLayoutManager()->UpdateAutoHideStateNow();
  }

  aura::Window* CreateTestWindow() {
    aura::Window* window = window_factory::NewWindow().release();
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    ParentWindowInPrimaryRootWindow(window);
    return window;
  }

  aura::Window* CreateTestWindowInParent(aura::Window* root_window) {
    aura::Window* window = window_factory::NewWindow().release();
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    window->SetType(aura::client::WINDOW_TYPE_NORMAL);
    window->Init(ui::LAYER_TEXTURED);
    aura::client::ParentWindowWithContext(window, root_window, gfx::Rect());
    return window;
  }

  // Create a simple widget in the current context (will delete on TearDown).
  views::Widget* CreateTestWidget() {
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_WINDOW);
    params.bounds = gfx::Rect(0, 0, 200, 200);
    params.context = CurrentContext();
    views::Widget* widget = new views::Widget;
    widget->Init(params);
    widget->Show();
    return widget;
  }

  void RunGestureDragTests(gfx::Vector2d);

  // Turn on the lock screen.
  void LockScreen() {
    mojom::SessionInfoPtr info = mojom::SessionInfo::New();
    info->state = session_manager::SessionState::LOCKED;
    ash::Shell::Get()->session_controller()->SetSessionInfo(std::move(info));
  }

  // Turn off the lock screen.
  void UnlockScreen() {
    mojom::SessionInfoPtr info = mojom::SessionInfo::New();
    info->state = session_manager::SessionState::ACTIVE;
    ash::Shell::Get()->session_controller()->SetSessionInfo(std::move(info));
  }

  int64_t GetPrimaryDisplayId() {
    return display::Screen::GetScreen()->GetPrimaryDisplay().id();
  }

  void StartScroll(gfx::Point start) {
    timestamp_ = base::TimeTicks::Now();
    current_point_ = start;
    ui::GestureEvent event = ui::GestureEvent(
        current_point_.x(), current_point_.y(), ui::EF_NONE, timestamp_,
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_BEGIN, 0, -1.0f));
    GetShelfLayoutManager()->ProcessGestureEvent(event);
  }

  void UpdateScroll(float delta_y) {
    IncreaseTimestamp();
    current_point_.set_y(current_point_.y() + delta_y);
    ui::GestureEvent event = ui::GestureEvent(
        current_point_.x(), current_point_.y(), ui::EF_NONE, timestamp_,
        ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_UPDATE, 0, delta_y));
    GetShelfLayoutManager()->ProcessGestureEvent(event);
  }

  void EndScroll(bool is_fling, float velocity_y) {
    IncreaseTimestamp();
    ui::GestureEventDetails event_details =
        is_fling
            ? ui::GestureEventDetails(ui::ET_SCROLL_FLING_START, 0, velocity_y)
            : ui::GestureEventDetails(ui::ET_GESTURE_SCROLL_END);
    ui::GestureEvent event =
        ui::GestureEvent(current_point_.x(), current_point_.y(), ui::EF_NONE,
                         timestamp_, event_details);
    GetShelfLayoutManager()->ProcessGestureEvent(event);
  }

  void IncreaseTimestamp() {
    timestamp_ += base::TimeDelta::FromMilliseconds(25);
  }

  wm::WorkspaceWindowState GetWorkspaceWindowState() const {
    const auto* shelf_window = GetShelfWidget()->GetNativeWindow();
    return RootWindowController::ForWindow(shelf_window)
        ->GetWorkspaceWindowState();
  }

  const ui::Layer* GetNonLockScreenContainersContainerLayer() const {
    const auto* shelf_window = GetShelfWidget()->GetNativeWindow();
    return shelf_window->GetRootWindow()
        ->GetChildById(kShellWindowId_NonLockScreenContainersContainer)
        ->layer();
  }

 private:
  base::TimeTicks timestamp_;
  gfx::Point current_point_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerTest);
};

void ShelfLayoutManagerTest::RunGestureDragTests(gfx::Vector2d delta) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // The time delta should be large enough to prevent accidental fling creation.
  const base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);

  aura::Window* window = widget->GetNativeWindow();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();

  gfx::Rect shelf_shown = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect window_bounds_with_shelf = window->bounds();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  gfx::Rect window_bounds_with_noshelf = window->bounds();
  gfx::Rect shelf_hidden = GetShelfWidget()->GetWindowBoundsInScreen();

  // Tests the gesture drag on always shown shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  layout_manager->LayoutShelf();

  ui::test::EventGenerator* generator = GetEventGenerator();
  const int kNumScrollSteps = 4;
  ShelfDragCallback handler(shelf_hidden, shelf_shown);

  // Swipe up on the always shown shelf should not change any state.
  gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
  gfx::Point end = start + delta;

  // Swipe down on the always shown shelf should not auto-hide it.
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Verify that the shelf can still enter auto hide if the |widget_| has been
  // put into fullscreen.
  widget->SetFullscreen(true);
  wm::WindowState* window_state = wm::GetWindowState(window);
  window_state->SetHideShelfWhenFullscreen(false);
  window_state->SetInImmersiveFullscreen(true);
  layout_manager->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping up should show the shelf if shelf is hidden in fullscreen mode.
  generator->GestureScrollSequence(end, start, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping down should hide the shelf.
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Verify that after toggling fullscreen to off, the shelf is visible.
  widget->SetFullscreen(false);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Minimize the visible window, the shelf should be shown if there are no
  // visible windows, even in auto-hide mode.
  window_state->Minimize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Tests gesture drag on auto-hide shelf.
  window_state->Maximize();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up the auto-hide shelf should show it.
  generator->GestureScrollSequenceWithCallback(
      end, start, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  // Gesture drag should not change the auto hide behavior of shelf, even though
  // its visibility has been changed.
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  // The auto-hide shelf is above the window, which should not change the bounds
  // of the window.
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down very little. It shouldn't change any state.
  end = start + delta;
  if (shelf->IsHorizontalAlignment())
    end.set_y(start.y() + shelf_shown.height() * 3 / 10);
  else if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
    end.set_x(start.x() - shelf_shown.width() * 3 / 10);
  else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment())
    end.set_x(start.x() + shelf_shown.width() * 3 / 10);
  generator->GestureScrollSequence(start, end, kTimeDelta, 5);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down to hide the shelf.
  end = start + delta;
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up in extended hit region to show it.
  gfx::Point extended_start = start;
  if (shelf->IsHorizontalAlignment())
    extended_start.set_y(GetShelfWidget()->GetWindowBoundsInScreen().y() - 1);
  else if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
    extended_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().right() +
                         1);
  else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment())
    extended_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().x() - 1);
  end = extended_start - delta;
  generator->GestureScrollSequenceWithCallback(
      extended_start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  end = start + delta;
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up outside the hit area. This should not change anything.
  gfx::Point outside_start =
      gfx::Point((GetShelfWidget()->GetWindowBoundsInScreen().x() +
                  GetShelfWidget()->GetWindowBoundsInScreen().right()) /
                     2,
                 GetShelfWidget()->GetWindowBoundsInScreen().y() - 50);
  end = outside_start + delta;
  generator->GestureScrollSequence(outside_start, end, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up from below the shelf where a bezel would be, this should show the
  // shelf.
  gfx::Point below_start = start;
  if (shelf->IsHorizontalAlignment())
    below_start.set_y(GetShelfWidget()->GetWindowBoundsInScreen().bottom() + 1);
  else if (SHELF_ALIGNMENT_LEFT == shelf->alignment())
    below_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().x() - 1);
  else if (SHELF_ALIGNMENT_RIGHT == shelf->alignment())
    below_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().right() + 1);
  end = below_start - delta;
  generator->GestureScrollSequence(below_start, end, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  end = start + delta;
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Put |widget| into fullscreen. Set the shelf to be auto hidden when |widget|
  // is fullscreen. (eg browser immersive fullscreen).
  widget->SetFullscreen(true);
  wm::GetWindowState(window)->SetHideShelfWhenFullscreen(false);
  layout_manager->UpdateVisibilityState();

  gfx::Rect window_bounds_fullscreen = window->bounds();
  EXPECT_TRUE(widget->IsFullscreen());

  EXPECT_EQ(window_bounds_with_noshelf.ToString(),
            window_bounds_fullscreen.ToString());

  // Swipe up. This should show the shelf.
  end = below_start - delta;
  generator->GestureScrollSequenceWithCallback(
      below_start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());

  // Swipe down to hide the shelf.
  end = start + delta;
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());

  // Set the shelf to be hidden when |widget| is fullscreen. (eg tab fullscreen
  // with or without immersive browser fullscreen).
  wm::GetWindowState(window)->SetHideShelfWhenFullscreen(true);

  layout_manager->UpdateVisibilityState();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Swipe-up. This should not change anything.
  end = start - delta;
  generator->GestureScrollSequenceWithCallback(
      below_start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());

  // Minimize actually, otherwise further event may be affected since widget
  // is fullscreen status.
  widget->Minimize();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(layout_manager->HasVisibleWindow());

  // The shelf should be shown because there are no more visible windows.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());

  // Swipe-down to hide. This should have no effect because there are no visible
  // windows.
  end = start + delta;
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  widget->Restore();
  RunAllPendingInMessageLoop();
  EXPECT_TRUE(layout_manager->HasVisibleWindow());

  // Swipe up on the shelf. This should show the shelf but should not change the
  // auto-hide behavior, since auto-hide behavior can only be changed through
  // context menu of the shelf.
  end = below_start - delta;
  generator->GestureScrollSequenceWithCallback(
      below_start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  widget->Close();
  RunAllPendingInMessageLoop();
  EXPECT_FALSE(layout_manager->HasVisibleWindow());

  // Swipe-down to hide. This should have no effect because there are no visible
  // windows.
  end = start + delta;
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up again on AUTO_HIDE_SHOWN shelf shouldn't change any state.
  // Swipe up on auto-hide shown shelf should still keep shelf shown.
  end = start - delta;
  generator->GestureScrollSequenceWithCallback(
      start, end, kTimeDelta, kNumScrollSteps,
      base::Bind(&ShelfDragCallback::ProcessScroll,
                 base::Unretained(&handler)));
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
}

class ShelfLayoutManagerNonHomeLauncherTest : public ShelfLayoutManagerTest {
 public:
  ShelfLayoutManagerNonHomeLauncherTest() = default;
  ~ShelfLayoutManagerNonHomeLauncherTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {}, {app_list_features::kEnableHomeLauncher});
    ShelfLayoutManagerTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerNonHomeLauncherTest);
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

  // And show it again.
  SetState(manager, SHELF_VISIBLE);
  // Run the animation to completion.
  StepWidgetLayerAnimatorToEnd(shelf_widget);
  StepWidgetLayerAnimatorToEnd(shelf_widget->status_area_widget());
  EXPECT_EQ(SHELF_VISIBLE, manager->visibility_state());
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(shelf_height, display.GetWorkAreaInsets().bottom());

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

// Makes sure the shelf is sized when the status area changes size.
TEST_F(ShelfLayoutManagerTest, ShelfUpdatedWhenStatusAreaChangesSize) {
  Shelf* shelf = GetPrimaryShelf();
  ASSERT_TRUE(shelf);
  ShelfWidget* shelf_widget = GetShelfWidget();
  ASSERT_TRUE(shelf_widget);
  ASSERT_TRUE(shelf_widget->status_area_widget());
  shelf_widget->status_area_widget()->SetBounds(gfx::Rect(0, 0, 200, 200));
  EXPECT_EQ(200, shelf_widget->GetContentsView()->width() -
                     shelf->GetShelfViewForTesting()->width());
}

// Various assertions around auto-hide.
TEST_F(ShelfLayoutManagerTest, AutoHide) {
  ui::test::EventGenerator* generator = GetEventGenerator();

  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  layout_manager->LayoutShelf();

  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const int display_bottom = display.bounds().bottom();
  EXPECT_EQ(display_bottom - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(display_bottom, display.work_area().bottom());

  // Move the mouse to the bottom of the screen.
  generator->MoveMouseTo(0, display_bottom - 1);

  // Shelf should be shown again (but it shouldn't have changed the work area).
  SetState(layout_manager, SHELF_AUTO_HIDE);
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  layout_manager->LayoutShelf();
  EXPECT_EQ(display_bottom - layout_manager->GetIdealBounds().height(),
            GetShelfWidget()->GetWindowBoundsInScreen().y());
  EXPECT_EQ(display_bottom, display.work_area().bottom());

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
  EXPECT_EQ(display_bottom - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

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
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);

  // Create a window because the shelf is always shown when no windows are
  // visible.
  CreateTestWidget();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
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
  WallpaperController* wallpaper_controller =
      Shell::Get()->wallpaper_controller();
  WallpaperShownWaiter waiter;

  mojom::SessionInfoPtr info = mojom::SessionInfo::New();
  info->state = session_manager::SessionState::LOGIN_PRIMARY;
  Shell::Get()->session_controller()->SetSessionInfo(std::move(info));
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // No wallpaper.
  ASSERT_FALSE(wallpaper_controller->HasShownAnyWallpaper());
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN, GetShelfWidget()->GetBackgroundType());

  // Showing wallpaper is asynchronous.
  wallpaper_controller->ShowDefaultWallpaperForTesting();
  waiter.Wait();
  ASSERT_TRUE(wallpaper_controller->HasShownAnyWallpaper());

  // Non-blurred wallpaper.
  wallpaper_controller->UpdateWallpaperBlur(/*blur=*/false);
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER,
            GetShelfWidget()->GetBackgroundType());

  // Blurred wallpaper.
  wallpaper_controller->UpdateWallpaperBlur(/*blur=*/true);
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN, GetShelfWidget()->GetBackgroundType());
}

// Assertions around the lock screen showing.
TEST_F(ShelfLayoutManagerTest, VisibleWhenLockScreenShowing) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  views::Widget* widget = CreateTestWidget();
  widget->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  layout_manager->LayoutShelf();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display.bounds().bottom() - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  std::unique_ptr<views::Widget> lock_widget(AshTestBase::CreateTestWidget(
      nullptr, kShellWindowId_LockScreenContainer, gfx::Rect(200, 200)));
  lock_widget->Maximize();

  // Lock the screen.
  LockScreen();
  // Showing a widget in the lock screen should force the shelf to be visible.
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_BACKGROUND_LOGIN, GetShelfWidget()->GetBackgroundType());

  UnlockScreen();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());
}

// Tests that the shelf should be visible when in overview mode.
TEST_F(ShelfLayoutManagerTest, VisibleInOverview) {
  std::unique_ptr<aura::Window> window(CreateTestWindow());
  window->Show();
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // LayoutShelf() forces the animation to completion, at which point the
  // shelf should go off the screen.
  GetShelfLayoutManager()->LayoutShelf();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(display.bounds().bottom() - kHiddenShelfInScreenPortion,
            GetShelfWidget()->GetWindowBoundsInScreen().y());

  WindowSelectorController* window_selector_controller =
      Shell::Get()->window_selector_controller();
  // Tests that the shelf is visible when in overview mode and its color is
  // overlap.
  window_selector_controller->ToggleOverview();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());

  // Test that on exiting overview mode, the shelf returns to auto hide state.
  window_selector_controller->ToggleOverview();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Assertions around SetAutoHideBehavior.
TEST_F(ShelfLayoutManagerTest, SetAutoHideBehavior) {
  Shelf* shelf = GetPrimaryShelf();
  views::Widget* widget = CreateTestWidget();

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  widget->Maximize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  display::Screen* screen = display::Screen::GetScreen();
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(screen->GetPrimaryDisplay().work_area().bottom(),
            widget->GetWorkAreaBoundsInScreen().bottom());

  ui::ScopedAnimationDurationScaleMode animation_duration(
      ui::ScopedAnimationDurationScaleMode::SLOW_DURATION);

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
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

// Ensure a SHELF_VISIBLE shelf stays visible when the app list is shown.
TEST_F(ShelfLayoutManagerTest, OpenAppListWithShelfVisibleState) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

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
  GetShelfLayoutManager()->UpdateVisibilityState();
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
  UpdateDisplay("0+0-200x200,+200+0-100x100");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(root_windows.size(), 2U);

  // Get the shelves in both displays and set them to be 'AutoHide'.
  Shelf* shelf_1 = Shelf::ForWindow(root_windows[0]);
  Shelf* shelf_2 = Shelf::ForWindow(root_windows[1]);
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->GetWindow()->GetRootWindow(),
            shelf_2->GetWindow()->GetRootWindow());
  shelf_1->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_1->shelf_layout_manager()->LayoutShelf();
  shelf_2->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_2->shelf_layout_manager()->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 = CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_1->Show();
  aura::Window* window_2 = CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_2->Show();

  EXPECT_EQ(shelf_1->GetWindow()->GetRootWindow(), window_1->GetRootWindow());
  EXPECT_EQ(shelf_2->GetWindow()->GetRootWindow(), window_2->GetRootWindow());

  // Activate one window in one display.
  wm::ActivateWindow(window_1);

  Shell::Get()->UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());

  // Show the app list; only the shelf on the same display should be shown.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  Shell::Get()->UpdateShelfVisibility();
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());

  // Hide the app list, both shelves should be hidden.
  GetAppListTestHelper()->DismissAndRunLoop();
  Shell::Get()->UpdateShelfVisibility();
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
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window->Show();
  wm::ActivateWindow(window);
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Show the app list and the shelf should be temporarily visible.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());
  GetAppListTestHelper()->CheckVisibility(true);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Hide the app list and the shelf should be hidden again.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have a single display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowSingleDisplay) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  aura::Window* window = CreateTestWindow();
  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window->Show();
  wm::ActivateWindow(window);

  // Enable system modal dialog, and make sure shelf is still hidden.
  ShellTestApi().SimulateModalWindowOpenForTest(true);
  EXPECT_TRUE(Shell::IsSystemModalWindowOpen());
  EXPECT_FALSE(wm::CanActivateWindow(window));
  Shell::Get()->UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
}

// Tests the correct behavior of the shelf when there is a system modal window
// open when we have dual display.
TEST_F(ShelfLayoutManagerTest, ShelfWithSystemModalWindowDualDisplay) {
  // Create two displays.
  UpdateDisplay("200x200,100x100");
  aura::Window::Windows root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2U, root_windows.size());

  // Get the shelves in both displays and set them to be 'AutoHide'.
  Shelf* shelf_1 = Shelf::ForWindow(root_windows[0]);
  Shelf* shelf_2 = Shelf::ForWindow(root_windows[1]);
  EXPECT_NE(shelf_1, shelf_2);
  EXPECT_NE(shelf_1->GetWindow()->GetRootWindow(),
            shelf_2->GetWindow()->GetRootWindow());
  shelf_1->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_1->shelf_layout_manager()->LayoutShelf();
  shelf_2->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  shelf_2->shelf_layout_manager()->LayoutShelf();

  // Create a window in each display and show them in maximized state.
  aura::Window* window_1 = CreateTestWindowInParent(root_windows[0]);
  window_1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window_1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  window_1->Show();
  aura::Window* window_2 = CreateTestWindowInParent(root_windows[1]);
  window_2->SetBounds(gfx::Rect(201, 0, 100, 100));
  window_2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
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
  Shell::Get()->UpdateShelfVisibility();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_1->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_1->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf_2->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf_2->GetAutoHideState());
}

// Tests that the shelf is only hidden for a fullscreen window at the front and
// toggles visibility when another window is activated.
TEST_F(ShelfLayoutManagerTest, FullscreenWindowInFrontHidesShelf) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_DEFAULT, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Create a window and make it full screen.
  aura::Window* window1 = CreateTestWindow();
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window1->Show();
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  aura::Window* window2 = CreateTestWindow();
  window2->SetBounds(gfx::Rect(0, 0, 100, 100));
  window2->Show();

  wm::GetWindowState(window1)->Activate();
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  wm::GetWindowState(window2)->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  wm::GetWindowState(window1)->Activate();
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
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  window1->Show();

  aura::Window* window2 = CreateTestWindow();
  window2->SetBoundsInScreen(gfx::Rect(800, 0, 100, 100),
                             display::Screen::GetScreen()->GetAllDisplays()[1]);
  window2->Show();

  EXPECT_EQ(root_windows[0], window1->GetRootWindow());
  EXPECT_EQ(root_windows[1], window2->GetRootWindow());

  wm::GetWindowState(window2)->Activate();
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

  wm::PinWindow(window1, /* trusted */ false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());

  wm::GetWindowState(window1)->Restore();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
}

// Tests SHELF_ALIGNMENT_(LEFT, RIGHT).
TEST_F(ShelfLayoutManagerTest, SetAlignment) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  // Force an initial layout.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  gfx::Rect shelf_bounds(GetShelfWidget()->GetWindowBoundsInScreen());
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().left());
  EXPECT_GE(shelf_bounds.width(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_LEFT, GetPrimaryShelf()->alignment());
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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().left());
  EXPECT_EQ(0, display.work_area().x());

  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  ASSERT_NE(-1, display.id());
  EXPECT_EQ(layout_manager->GetIdealBounds().width(),
            display.GetWorkAreaInsets().right());
  EXPECT_GE(shelf_bounds.width(),
            GetShelfWidget()->GetContentsView()->GetPreferredSize().width());
  EXPECT_EQ(SHELF_ALIGNMENT_RIGHT, GetPrimaryShelf()->alignment());
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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  display = display::Screen::GetScreen()->GetPrimaryDisplay();
  EXPECT_EQ(0, display.GetWorkAreaInsets().right());
  EXPECT_EQ(0, display.bounds().right() - display.work_area().right());
}

TEST_F(ShelfLayoutManagerTest, GestureDrag) {
  // Slop is an implementation detail of gesture recognition, and complicates
  // these tests. Ignore it.
  ui::GestureConfiguration::GetInstance()
      ->set_max_touch_move_in_pixels_for_click(0);
  Shelf* shelf = GetPrimaryShelf();
  {
    SCOPED_TRACE("BOTTOM");
    shelf->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
    RunGestureDragTests(gfx::Vector2d(0, 120));
    GetAppListTestHelper()->WaitUntilIdle();
  }

  {
    SCOPED_TRACE("LEFT");
    shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
    RunGestureDragTests(gfx::Vector2d(-120, 0));
    GetAppListTestHelper()->WaitUntilIdle();
  }

  {
    SCOPED_TRACE("RIGHT");
    shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
    RunGestureDragTests(gfx::Vector2d(120, 0));
    GetAppListTestHelper()->WaitUntilIdle();
  }
}

// If swiping up on shelf ends with fling event, the app list state should
// depends on the fling velocity.
TEST_F(ShelfLayoutManagerTest, FlingUpOnShelfForFullscreenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Starts the drag from the center of the shelf's bottom.
  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start =
      gfx::Point(shelf_widget_bounds.x() + shelf_widget_bounds.width() / 2,
                 shelf_widget_bounds.bottom());

  // Fling up that exceeds the velocity threshold should show the fullscreen app
  // list.
  StartScroll(start);
  UpdateScroll(-ShelfLayoutManager::kAppListDragSnapToPeekingThreshold);
  EndScroll(true /* is_fling */,
            -(ShelfLayoutManager::kAppListDragVelocityThreshold + 10));
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Fling down that exceeds the velocity threshold should close the app list.
  StartScroll(start);
  UpdateScroll(-ShelfLayoutManager::kAppListDragSnapToPeekingThreshold);
  EndScroll(true /* is_fling */,
            ShelfLayoutManager::kAppListDragVelocityThreshold + 10);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Fling the app list not exceed the velocity threshold, the state depends on
  // the drag amount.
  StartScroll(start);
  UpdateScroll(-(ShelfLayoutManager::kAppListDragSnapToPeekingThreshold - 10));
  EndScroll(true /* is_fling */,
            -(ShelfLayoutManager::kAppListDragVelocityThreshold - 10));
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::PEEKING);
}

// Tests that duplicate swipe up from bottom bezel should not make app list
// undraggable. (See https://crbug.com/896934)
TEST_F(ShelfLayoutManagerTest, DuplicateDragUpFromBezel) {
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Start the drag from the bottom bezel to the area that snaps to fullscreen
  // state.
  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start =
      gfx::Point(shelf_widget_bounds.x() + shelf_widget_bounds.width() / 2,
                 shelf_widget_bounds.bottom() + 1);
  gfx::Point end = gfx::Point(
      start.x(), shelf_widget_bounds.bottom() -
                     ShelfLayoutManager::kAppListDragSnapToPeekingThreshold -
                     10);
  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  constexpr int kNumScrollSteps = 4;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);

  // Start the same drag event from bezel.
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);

  // Start the drag from top screen to the area that snaps to closed state. (The
  // launcher is still draggable now.)
  start.set_y(
      display::Screen::GetScreen()->GetPrimaryDisplay().work_area().y());
  end.set_y(shelf_widget_bounds.bottom() -
            ShelfLayoutManager::kAppListDragSnapToClosedThreshold + 10);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);
}

// Change the shelf alignment during dragging should dismiss the app list.
TEST_F(ShelfLayoutManagerTest, ChangeShelfAlignmentDuringAppListDragging) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  StartScroll(GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint());
  UpdateScroll(-ShelfLayoutManager::kAppListDragSnapToPeekingThreshold);
  GetAppListTestHelper()->WaitUntilIdle();
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  // Note, value -10 here has no specific meaning, it only used to make the
  // event scroll up a little bit.
  UpdateScroll(-10);
  EndScroll(false /* is_fling */, 0.f);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
}

TEST_F(ShelfLayoutManagerNonHomeLauncherTest,
       SwipingUpOnShelfInTabletModeForFullscreenAppList) {
  // Animations triggered by immersive mode cause this test to fail.
  ImmersiveFullscreenControllerTestApi::GlobalAnimationDisabler
      animation_disabler;
  Shell* shell = Shell::Get();
  shell->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  Shelf* shelf = GetPrimaryShelf();
  GetShelfLayoutManager()->LayoutShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Note: A window must be visible in order to hide the shelf. The test will
  // make the window fullscreened, so make the window resizeable and
  // maximizable.
  std::unique_ptr<aura::Window> window(
      AshTestBase::CreateTestWindow(gfx::Rect(0, 0, 400, 400)));
  window->SetProperty(aura::client::kResizeBehaviorKey,
                      ws::mojom::kResizeBehaviorCanResize |
                          ws::mojom::kResizeBehaviorCanMaximize);
  wm::ActivateWindow(window.get());

  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  constexpr int kNumScrollSteps = 4;

  // Starts the drag from the center of the shelf's bottom.
  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start =
      gfx::Point(shelf_widget_bounds.x() + shelf_widget_bounds.width() / 2,
                 shelf_widget_bounds.bottom());
  gfx::Vector2d delta;

  // Swiping up more than the threshold should show the app list.
  delta.set_y(ShelfLayoutManager::kAppListDragSnapToFullscreenThreshold + 10);
  gfx::Point end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Swiping up less or equal to the threshold should dismiss the app list.
  delta.set_y(ShelfLayoutManager::kAppListDragSnapToFullscreenThreshold - 10);
  end = start - delta;
  // TODO(minch): investigate failure without EnableMaximizeMode again here.
  // http://crbug.com/746481.
  shell->tablet_mode_controller()->EnableTabletModeWindowManager(true);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);
  GetShelfLayoutManager()->UpdateVisibilityState();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Swiping down on the shelf should do nothing as always shown shelf can not
  // be dragged down to hide.
  end = start + delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Verify that the shelf can still enter auto hide if the window requests to
  // be fullscreened.
  wm::WindowState* window_state = wm::GetWindowState(window.get());
  const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
  window_state->OnWMEvent(&event);
  window_state->SetHideShelfWhenFullscreen(false);
  window_state->SetInImmersiveFullscreen(true);
  GetShelfLayoutManager()->UpdateVisibilityState();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping up should show the shelf but not the app list if shelf is hidden.
  generator->GestureScrollSequence(end, start, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping down should hide the shelf.
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Verify that after toggling fullscreen to off, the shelf is visible.
  window_state->OnWMEvent(&event);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Minimize the visible window, the shelf should be shown if there are no
  // visible windows, even in auto-hide mode.
  window_state->Minimize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping up on the shelf in this state should open the app list.
  delta.set_y(ShelfLayoutManager::kAppListDragSnapToFullscreenThreshold + 10);
  end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);
}

TEST_F(ShelfLayoutManagerTest,
       SwipingUpOnShelfInLaptopModeForFullscreenAppList) {
  Shelf* shelf = GetPrimaryShelf();
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  constexpr int kNumScrollSteps = 4;

  // Starts the drag from the center of the shelf's bottom.
  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Point start =
      gfx::Point(shelf_widget_bounds.x() + shelf_widget_bounds.width() / 2,
                 shelf_widget_bounds.bottom());
  gfx::Vector2d delta;

  // Swiping up less than the close threshold should close the app list.
  delta.set_y(ShelfLayoutManager::kAppListDragSnapToClosedThreshold - 10);
  gfx::Point end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Swiping up more than the close threshold but less than peeking threshold
  // should keep the app list at PEEKING state.
  delta.set_y(ShelfLayoutManager::kAppListDragSnapToPeekingThreshold - 10);
  end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::PEEKING);

  // Closing the app list.
  GetAppListTestHelper()->DismissAndRunLoop();
  GetAppListTestHelper()->CheckVisibility(false);
  GetAppListTestHelper()->CheckState(app_list::AppListViewState::CLOSED);

  // Swiping up more than the peeking threshold should keep the app list at
  // FULLSCREEN_ALL_APPS state.
  delta.set_y(ShelfLayoutManager::kAppListDragSnapToPeekingThreshold + 10);
  end = start - delta;
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  GetAppListTestHelper()->WaitUntilIdle();
  GetAppListTestHelper()->CheckVisibility(true);
  GetAppListTestHelper()->CheckState(
      app_list::AppListViewState::FULLSCREEN_ALL_APPS);
}

// Swiping on shelf when fullscreen app list is opened should have no effect.
TEST_F(ShelfLayoutManagerTest, SwipingOnShelfIfFullscreenAppListOpened) {
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  aura::Window* root_window =
      RootWindowController::ForTargetRootWindow()->GetRootWindow();
  layout_manager->OnAppListVisibilityChanged(true, root_window);
  EXPECT_EQ(SHELF_ALIGNMENT_BOTTOM, shelf->alignment());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Note: A window must be visible in order to hide the shelf.
  CreateTestWidget();

  ui::test::EventGenerator* generator = GetEventGenerator();
  constexpr base::TimeDelta kTimeDelta = base::TimeDelta::FromMilliseconds(100);
  constexpr int kNumScrollSteps = 4;
  gfx::Point start = GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();

  // Swiping down on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  gfx::Point end = start + gfx::Vector2d(0, 120);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping left on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  shelf->SetAlignment(SHELF_ALIGNMENT_LEFT);
  end = start + gfx::Vector2d(-120, 0);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());

  // Swiping right on shelf when the fullscreen app list is opened
  // should not hide the shelf.
  shelf->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  end = start + gfx::Vector2d(120, 0);
  generator->GestureScrollSequence(start, end, kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_BEHAVIOR_NEVER, shelf->auto_hide_behavior());
}

TEST_F(ShelfLayoutManagerTest, WindowVisibilityDisablesAutoHide) {
  UpdateDisplay("800x600,800x600");
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Create a visible window so auto-hide behavior is enforced
  views::Widget* dummy = CreateTestWidget();

  // Window visible => auto hide behaves normally.
  layout_manager->UpdateVisibilityState();
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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
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
    generator->GestureScrollSequence(start, end,
                                     base::TimeDelta::FromMilliseconds(10), 5);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

    ShelfAnimationWaiter waiter(visible_bounds);
    // Wait till the animation completes and check that it occurred.
    waiter.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter.WasValidAnimation());
  }
}

// Tests that the shelf animates to the visible bounds after a swipe up on
// the auto hidden shelf.
TEST_F(ShelfLayoutManagerTest, ShelfAnimatesToVisibleWhenGestureInComplete) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
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

    gfx::Point start =
        GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
    gfx::Point end(start.x(), start.y() - 100);
    ui::test::EventGenerator* generator = GetEventGenerator();

    generator->GestureScrollSequence(start, end,
                                     base::TimeDelta::FromMilliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
    ShelfAnimationWaiter waiter(visible_bounds);
    waiter.WaitTillDoneAnimating();
    EXPECT_TRUE(waiter.WasValidAnimation());
  }
}

// Tests that the shelf animates to the auto hidden bounds after a swipe down
// on the visible shelf.
TEST_F(ShelfLayoutManagerTest, ShelfAnimatesToHiddenWhenGestureOutComplete) {
  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  gfx::Rect visible_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  // Create a visible window, otherwise the shelf will not hide.
  CreateTestWidget();

  gfx::Rect auto_hidden_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  {
    // Enable the animations so that we can make sure they do occur.
    ui::ScopedAnimationDurationScaleMode regular_animations(
        ui::ScopedAnimationDurationScaleMode::NON_ZERO_DURATION);

    ui::test::EventGenerator* generator = GetEventGenerator();

    // Show the shelf first.
    display::Display display =
        display::Screen::GetScreen()->GetPrimaryDisplay();
    const int half_width = display.bounds().width() / 2;
    const int bottom_edge = display.bounds().bottom();
    generator->MoveMouseTo(half_width,
                           bottom_edge - kHiddenShelfInScreenPortion / 2);
    ShelfAnimationWaiter waiter1(visible_bounds);
    waiter1.WaitTillDoneAnimating();
    EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());

    // Now swipe down.
    gfx::Point start =
        GetShelfWidget()->GetWindowBoundsInScreen().CenterPoint();
    gfx::Point end = gfx::Point(start.x(), start.y() + 100);
    generator->GestureScrollSequence(start, end,
                                     base::TimeDelta::FromMilliseconds(10), 1);
    EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
    EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
    ShelfAnimationWaiter waiter2(auto_hidden_bounds);
    waiter2.WaitTillDoneAnimating();
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
  wm::GetWindowState(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(true);
  widget_two->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Now the flag takes effect once window two is maximized.
  widget_two->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  // The hide_shelf_when_active flag should override the behavior of the
  // hide_shelf_when_fullscreen flag even if the window is currently fullscreen.
  wm::GetWindowState(window_two)->SetHideShelfWhenFullscreen(false);
  widget_two->SetFullscreen(true);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  wm::GetWindowState(window_two)->Restore();

  // With the flag off, shelf no longer auto-hides.
  widget_one->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  wm::GetWindowState(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(false);
  widget_two->Activate();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  wm::GetWindowState(window_two)
      ->set_autohide_shelf_when_maximized_or_fullscreen(true);
  window_two->SetProperty(aura::client::kAlwaysOnTopKey, true);

  auto* shelf_window = shelf->GetWindow();
  aura::Window* container = shelf_window->GetRootWindow()->GetChildById(
      kShellWindowId_AlwaysOnTopContainer);
  EXPECT_TRUE(base::ContainsValue(container->children(), window_two));

  widget_two->Maximize();
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());

  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_MAXIMIZED, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());
}

TEST_F(ShelfLayoutManagerTest, ShelfFlickerOnTrayActivation) {
  Shelf* shelf = GetPrimaryShelf();

  // Create a visible window so auto-hide behavior is enforced.
  CreateTestWidget();

  // Turn on auto-hide for the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Show the status menu. That should make the shelf visible again.
  Shell::Get()->accelerator_controller()->PerformActionIfEnabled(
      TOGGLE_SYSTEM_TRAY_BUBBLE);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_TRUE(GetPrimaryUnifiedSystemTray()->IsBubbleShown());
}

TEST_F(ShelfLayoutManagerTest, WorkAreaChangeWorkspace) {
  // Make sure the shelf is always visible.
  Shelf* shelf = GetPrimaryShelf();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
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
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  // Both windows should be resized according to the shelf status.
  EXPECT_EQ(widget_one->GetNativeWindow()->bounds().ToString(),
            widget_two->GetNativeWindow()->bounds().ToString());
  // Resized to small.
  EXPECT_LT(area_when_shelf_shown,
            widget_one->GetNativeWindow()->bounds().size().GetArea());

  // Now show the shelf.
  shelf->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);

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
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  Shell::Get()->lock_state_controller()->StartLockAnimationAndLockImmediately();
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
}

TEST_F(ShelfLayoutManagerTest, WorkspaceMask) {
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_DEFAULT, GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  // Overlaps with shelf.
  w1->SetBounds(GetShelfLayoutManager()->GetIdealBounds());
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF,
            GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_MAXIMIZED, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_FULLSCREEN);
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  w2->Show();
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF,
            GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w2.reset();
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_FULL_SCREEN, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_WINDOW_OVERLAPS_SHELF,
            GetWorkspaceWindowState());
  EXPECT_TRUE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());
}

TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColor) {
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  std::unique_ptr<aura::Window> w2(CreateTestWindow());
  w2->Show();
  wm::ActivateWindow(w2.get());
  // Overlaps with shelf.
  w2->SetBounds(GetShelfLayoutManager()->GetIdealBounds());

  // Still background is 'maximized'.
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());
  w2->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MINIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());

  std::unique_ptr<aura::Window> w3(CreateTestWindow());
  w3->SetProperty(aura::client::kModalKey, ui::MODAL_TYPE_WINDOW);
  ::wm::AddTransientChild(w1.get(), w3.get());
  w3->Show();
  wm::ActivateWindow(w3.get());

  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_MAXIMIZED, GetWorkspaceWindowState());
  EXPECT_FALSE(GetNonLockScreenContainersContainerLayer()->GetMasksToBounds());

  w3.reset();
  w1.reset();
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());
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
                      ws::mojom::kResizeBehaviorCanResize |
                          ws::mojom::kResizeBehaviorCanMaximize);
  wm::ActivateWindow(window.get());

  // Show the AppList over |window|.
  GetAppListTestHelper()->ShowAndRunLoop(GetPrimaryDisplayId());

  // Transition to tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  // |window| should be maximized, and the shelf background should match the
  // maximized state.
  EXPECT_EQ(wm::WORKSPACE_WINDOW_STATE_MAXIMIZED, GetWorkspaceWindowState());
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());
}

// Test the background color for split view mode.
TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColorInSplitView) {
  // Split view is only enabled in tablet mode.
  Shell::Get()->tablet_mode_controller()->EnableTabletModeWindowManager(true);

  std::unique_ptr<aura::Window> window1(CreateTestWindow());
  window1->SetProperty(aura::client::kResizeBehaviorKey,
                       ws::mojom::kResizeBehaviorCanResize |
                           ws::mojom::kResizeBehaviorCanMaximize);
  window1->Show();

  SplitViewController* split_view_controller =
      Shell::Get()->split_view_controller();
  split_view_controller->SnapWindow(window1.get(), SplitViewController::LEFT);
  EXPECT_EQ(SHELF_BACKGROUND_SPLIT_VIEW, GetShelfWidget()->GetBackgroundType());

  std::unique_ptr<aura::Window> window2(CreateTestWindow());
  window2->SetProperty(aura::client::kResizeBehaviorKey,
                       ws::mojom::kResizeBehaviorCanResize |
                           ws::mojom::kResizeBehaviorCanMaximize);
  window2->Show();
  split_view_controller->SnapWindow(window2.get(), SplitViewController::RIGHT);
  EXPECT_EQ(SHELF_BACKGROUND_SPLIT_VIEW, GetShelfWidget()->GetBackgroundType());

  // Ending split view mode will maximize the two windows.
  split_view_controller->EndSplitView();
  EXPECT_EQ(SHELF_BACKGROUND_MAXIMIZED, GetShelfWidget()->GetBackgroundType());
}

// Verify that the shelf doesn't have the opaque background if it's auto-hide
// status.
TEST_F(ShelfLayoutManagerTest, ShelfBackgroundColorAutoHide) {
  EXPECT_EQ(SHELF_BACKGROUND_DEFAULT, GetShelfWidget()->GetBackgroundType());

  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);
  std::unique_ptr<aura::Window> w1(CreateTestWindow());
  w1->Show();
  wm::ActivateWindow(w1.get());
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());
  w1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  EXPECT_EQ(SHELF_BACKGROUND_OVERLAP, GetShelfWidget()->GetBackgroundType());
}

// Verify the hit bounds of the status area extend to the edge of the shelf.
TEST_F(ShelfLayoutManagerTest, StatusAreaHitBoxCoversEdge) {
  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  ui::test::EventGenerator* generator = GetEventGenerator();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  gfx::Rect inset_display_bounds = display.bounds();
  inset_display_bounds.Inset(0, 0, 1, 1);

  // Test bottom right pixel for bottom alignment.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_BOTTOM);
  generator->MoveMouseTo(inset_display_bounds.bottom_right());
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom right pixel for right alignment.
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_RIGHT);
  generator->MoveMouseTo(inset_display_bounds.bottom_right());
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_TRUE(status_area_widget->IsMessageBubbleShown());
  generator->ClickLeftButton();
  EXPECT_FALSE(status_area_widget->IsMessageBubbleShown());

  // Test bottom left pixel for left alignment.
  generator->MoveMouseTo(inset_display_bounds.bottom_left());
  GetPrimaryShelf()->SetAlignment(SHELF_ALIGNMENT_LEFT);
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
  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_ALWAYS_HIDDEN);
  gfx::Rect hide_target_bounds = status_window->GetTargetBounds();
  EXPECT_GT(hide_target_bounds.y(), initial_bounds.y());

  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_NEVER);
  gfx::Rect reshow_target_bounds = status_window->GetTargetBounds();
  EXPECT_EQ(initial_bounds, reshow_target_bounds);
}

// Tests that during shutdown, that window activation changes are properly
// handled, and do not crash (crbug.com/458768)
TEST_F(ShelfLayoutManagerTest, ShutdownHandlesWindowActivation) {
  GetPrimaryShelf()->SetAutoHideBehavior(SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS);

  aura::Window* window1 = CreateTestWindowInShellWithId(0);
  window1->SetBounds(gfx::Rect(0, 0, 100, 100));
  window1->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
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

  StatusAreaWidget* status_area_widget = GetShelfWidget()->status_area_widget();
  EXPECT_TRUE(status_area_widget->IsVisible());
  // Shelf should be in the first display's area.
  gfx::Rect status_area_bounds(status_area_widget->GetWindowBoundsInScreen());
  EXPECT_TRUE(gfx::Rect(0, 0, 500, 400).Contains(status_area_bounds));
  EXPECT_EQ(gfx::Point(500, 400), status_area_bounds.bottom_right());
}

class ShelfLayoutManagerKeyboardTest : public AshTestBase {
 public:
  ShelfLayoutManagerKeyboardTest() = default;
  ~ShelfLayoutManagerKeyboardTest() override = default;

  // AshTestBase:
  void SetUp() override {
    AshTestBase::SetUp();
    UpdateDisplay("800x600");
    keyboard::SetTouchKeyboardEnabled(true);
    keyboard::SetAccessibilityKeyboardEnabled(true);
    Shell::Get()->EnableKeyboard();
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
                              const gfx::Rect& bounds) {
    keyboard::KeyboardStateDescriptor state;
    state.visual_bounds = bounds;
    state.occluded_bounds = bounds;
    state.displaced_bounds = is_locked ? bounds : gfx::Rect();
    state.is_visible = !bounds.IsEmpty();
    layout_manager->OnKeyboardVisibilityStateChanged(state.is_visible);
    layout_manager->OnKeyboardAppearanceChanged(state);
  }

  const gfx::Rect& keyboard_bounds() const { return keyboard_bounds_; }

 private:
  gfx::Rect keyboard_bounds_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManagerKeyboardTest);
};

TEST_F(ShelfLayoutManagerKeyboardTest, ShelfNotMoveOnKeyboardOpen) {
  gfx::Rect orig_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  InitKeyboardBounds();
  auto* kb_controller = keyboard::KeyboardController::Get();
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
  auto* kb_controller = keyboard::KeyboardController::Get();
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
  auto* kb_controller = keyboard::KeyboardController::Get();
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

}  // namespace ash
