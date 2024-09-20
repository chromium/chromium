// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/test/shelf_layout_manager_test_base.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/shelf_layout_manager.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace_controller.h"
#include "base/functional/bind.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/view.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

using ::chromeos::kImmersiveIsActive;

ShelfWidget* GetShelfWidget() {
  return AshTestBase::GetPrimaryShelf()->shelf_widget();
}

ShelfLayoutManager* GetShelfLayoutManager() {
  return AshTestBase::GetPrimaryShelf()->shelf_layout_manager();
}

class ShelfDragCallback {
 public:
  ShelfDragCallback(const gfx::Rect& auto_hidden_shelf_bounds,
                    const gfx::Rect& visible_shelf_bounds)
      : auto_hidden_shelf_bounds_(auto_hidden_shelf_bounds),
        visible_shelf_bounds_(visible_shelf_bounds) {
    EXPECT_EQ(auto_hidden_shelf_bounds_.size(), visible_shelf_bounds_.size());
  }

  ShelfDragCallback(const ShelfDragCallback&) = delete;
  ShelfDragCallback& operator=(const ShelfDragCallback&) = delete;

  virtual ~ShelfDragCallback() = default;

  void ProcessScroll(ui::EventType type, const gfx::Vector2dF& delta) {
    ProcessScrollInternal(type, delta, true);
  }

  void ProcessScrollNoBoundsCheck(ui::EventType type,
                                  const gfx::Vector2dF& delta) {
    ProcessScrollInternal(type, delta, false);
  }

  void ProcessScrollInternal(ui::EventType type,
                             const gfx::Vector2dF& delta,
                             bool bounds_check) {
    if (GetShelfLayoutManager()->visibility_state() == SHELF_HIDDEN)
      return;

    if (type == ui::EventType::kGestureScrollBegin) {
      scroll_ = gfx::Vector2dF();
      was_visible_on_drag_start_ = GetShelfLayoutManager()->IsVisible();
      return;
    }

    // The state of the shelf at the end of the gesture is tested separately.
    if (type == ui::EventType::kGestureScrollEnd) {
      return;
    }

    if (type == ui::EventType::kGestureScrollUpdate) {
      scroll_.Add(delta);
    }

    Shelf* shelf = AshTestBase::GetPrimaryShelf();
    gfx::Rect shelf_bounds = GetShelfWidget()->GetWindowBoundsInScreen();

    float scroll_delta = shelf->PrimaryAxisValue(scroll_.y(), scroll_.x());
    bool increasing_drag = shelf->SelectValueForShelfAlignment(
        scroll_delta<0, scroll_delta> 0, scroll_delta < 0);
    const int shelf_size =
        shelf->PrimaryAxisValue(shelf_bounds.height(), shelf_bounds.width());
    if (was_visible_on_drag_start_) {
      if (increasing_drag) {
        // If dragging inwards from the visible state, then the shelf should
        // 'overshoot', but not by more than the scroll delta.
        const int bounds_delta = shelf->SelectValueForShelfAlignment(
            visible_shelf_bounds_.y() - shelf_bounds.y(),
            shelf_bounds.x() - visible_shelf_bounds_.x(),
            visible_shelf_bounds_.x() - shelf_bounds.x());
        EXPECT_GE(bounds_delta, 0);
        EXPECT_LE(bounds_delta, std::abs(scroll_delta));
      } else {
        // If dragging outwards from the visible state, then the shelf should
        // move out.
        if (ShelfAlignment::kBottom == shelf->alignment())
          EXPECT_LE(visible_shelf_bounds_.y(), shelf_bounds.y());
        else if (ShelfAlignment::kLeft == shelf->alignment())
          EXPECT_LE(shelf_bounds.x(), visible_shelf_bounds_.x());
        else if (ShelfAlignment::kRight == shelf->alignment())
          EXPECT_LE(visible_shelf_bounds_.x(), shelf_bounds.x());
      }
    } else {
      // The shelf is invisible at the start of the drag.
      if (increasing_drag && bounds_check) {
        constexpr float kEpsilon = 1.f;
        // Moving the shelf into the screen.
        if (std::abs(scroll_delta) < shelf_size) {
          // Tests that the shelf sticks with the touch point during the drag
          // until the shelf is completely visible.
          if (ShelfAlignment::kBottom == shelf->alignment()) {
            EXPECT_NEAR(
                shelf_bounds.y(),
                auto_hidden_shelf_bounds_.y() +
                    ShelfConfig::Get()->hidden_shelf_in_screen_portion() -
                    std::abs(scroll_delta),
                kEpsilon);
          } else if (ShelfAlignment::kLeft == shelf->alignment()) {
            EXPECT_NEAR(
                shelf_bounds.x(),
                auto_hidden_shelf_bounds_.x() -
                    ShelfConfig::Get()->hidden_shelf_in_screen_portion() +
                    std::abs(scroll_delta),
                kEpsilon);
          } else if (ShelfAlignment::kRight == shelf->alignment()) {
            EXPECT_NEAR(
                shelf_bounds.x(),
                auto_hidden_shelf_bounds_.x() +
                    ShelfConfig::Get()->hidden_shelf_in_screen_portion() -
                    std::abs(scroll_delta),
                kEpsilon);
          }
        } else {
          // Tests that after the shelf is completely visible, the shelf starts
          // resisting the drag.
          if (ShelfAlignment::kBottom == shelf->alignment()) {
            EXPECT_GT(shelf_bounds.y(),
                      auto_hidden_shelf_bounds_.y() +
                          ShelfConfig::Get()->hidden_shelf_in_screen_portion() -
                          std::abs(scroll_delta));
          } else if (ShelfAlignment::kLeft == shelf->alignment()) {
            EXPECT_LT(shelf_bounds.x(),
                      auto_hidden_shelf_bounds_.x() -
                          ShelfConfig::Get()->hidden_shelf_in_screen_portion() +
                          std::abs(scroll_delta));
          } else if (ShelfAlignment::kRight == shelf->alignment()) {
            EXPECT_GT(shelf_bounds.x(),
                      auto_hidden_shelf_bounds_.x() +
                          ShelfConfig::Get()->hidden_shelf_in_screen_portion() -
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
  bool was_visible_on_drag_start_ = false;
};
}  // namespace

void ShelfLayoutManagerTestBase::SetState(ShelfLayoutManager* layout_manager,
                                          ShelfVisibilityState state) {
  layout_manager->SetState(state, /*force_layout=*/false);
}

void ShelfLayoutManagerTestBase::UpdateAutoHideStateNow() {
  GetShelfLayoutManager()->UpdateAutoHideStateNow();
}

aura::Window* ShelfLayoutManagerTestBase::CreateTestWindow() {
  aura::Window* window = new aura::Window(nullptr);
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  ParentWindowInPrimaryRootWindow(window);
  return window;
}

aura::Window* ShelfLayoutManagerTestBase::CreateTestWindowInParent(
    aura::Window* root_window) {
  aura::Window* window = new aura::Window(nullptr);
  window->SetProperty(aura::client::kShowStateKey,
                      ui::mojom::WindowShowState::kNormal);
  window->SetType(aura::client::WINDOW_TYPE_NORMAL);
  window->Init(ui::LAYER_TEXTURED);
  aura::client::ParentWindowWithContext(window, root_window, gfx::Rect(),
                                        display::kInvalidDisplayId);
  return window;
}

views::Widget* ShelfLayoutManagerTestBase::CreateTestWidget() {
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.bounds = gfx::Rect(0, 0, 200, 200);
  params.context = GetContext();
  views::Widget* widget = new views::Widget;
  widget->Init(std::move(params));
  widget->Show();
  return widget;
}

gfx::Rect ShelfLayoutManagerTestBase::GetVisibleShelfWidgetBoundsInScreen() {
  gfx::Rect bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  bounds.Intersect(display::Screen::GetScreen()->GetPrimaryDisplay().bounds());
  return bounds;
}

void ShelfLayoutManagerTestBase::LockScreen() {
  GetSessionControllerClient()->LockScreen();
}

void ShelfLayoutManagerTestBase::UnlockScreen() {
  GetSessionControllerClient()->UnlockScreen();
}

int64_t ShelfLayoutManagerTestBase::GetPrimaryDisplayId() {
  return display::Screen::GetScreen()->GetPrimaryDisplay().id();
}

void ShelfLayoutManagerTestBase::StartScroll(gfx::Point start) {
  timestamp_ = base::TimeTicks::Now();
  current_point_ = start;
  ui::GestureEvent event = ui::GestureEvent(
      current_point_.x(), current_point_.y(), ui::EF_NONE, timestamp_,
      ui::GestureEventDetails(ui::EventType::kGestureScrollBegin, 0, -1.0f));
  GetShelfLayoutManager()->ProcessGestureEvent(event);
}

void ShelfLayoutManagerTestBase::UpdateScroll(const gfx::Vector2d& delta) {
  IncreaseTimestamp();
  current_point_ += delta;
  ui::GestureEvent event = ui::GestureEvent(
      current_point_.x(), current_point_.y(), ui::EF_NONE, timestamp_,
      ui::GestureEventDetails(ui::EventType::kGestureScrollUpdate, delta.x(),
                              delta.y()));
  GetShelfLayoutManager()->ProcessGestureEvent(event);
}

void ShelfLayoutManagerTestBase::EndScroll(bool is_fling, float velocity_y) {
  IncreaseTimestamp();
  ui::GestureEventDetails event_details =
      is_fling ? ui::GestureEventDetails(ui::EventType::kScrollFlingStart, 0,
                                         velocity_y)
               : ui::GestureEventDetails(ui::EventType::kGestureScrollEnd);
  ui::GestureEvent event =
      ui::GestureEvent(current_point_.x(), current_point_.y(), ui::EF_NONE,
                       timestamp_, event_details);
  GetShelfLayoutManager()->ProcessGestureEvent(event);
}

void ShelfLayoutManagerTestBase::IncreaseTimestamp() {
  timestamp_ += base::Milliseconds(25);
}

WorkspaceWindowState ShelfLayoutManagerTestBase::GetWorkspaceWindowState()
    const {
  // Shelf window does not belong to any desk, use the root to get the active
  // desk's workspace state.
  auto* shelf_window = GetShelfWidget()->GetNativeWindow();
  auto* controller =
      GetActiveWorkspaceController(shelf_window->GetRootWindow());
  DCHECK(controller);

  return controller->GetWindowState();
}

const ui::Layer*
ShelfLayoutManagerTestBase::GetNonLockScreenContainersContainerLayer() const {
  const auto* shelf_window = GetShelfWidget()->GetNativeWindow();
  return shelf_window->GetRootWindow()
      ->GetChildById(kShellWindowId_NonLockScreenContainersContainer)
      ->layer();
}

// If |layout_manager->auto_hide_timer_| is running, stops it, runs its task,
// and returns true. Otherwise, returns false.
bool ShelfLayoutManagerTestBase::TriggerAutoHideTimeout() const {
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  if (!layout_manager->auto_hide_timer_.IsRunning())
    return false;

  layout_manager->auto_hide_timer_.FireNow();
  return true;
}

// Performs a swipe up gesture to show an auto-hidden shelf.
void ShelfLayoutManagerTestBase::SwipeUpOnShelf() {
  gfx::Rect display_bounds =
      display::Screen::GetScreen()->GetPrimaryDisplay().bounds();
  const gfx::Point start(display_bounds.bottom_center());
  const gfx::Point end(start + gfx::Vector2d(0, -80));
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
}

void ShelfLayoutManagerTestBase::SwipeDownOnShelf() {
  gfx::Point start(GetPrimaryShelf()
                       ->shelf_widget()
                       ->shelf_view_for_testing()
                       ->GetBoundsInScreen()
                       .top_center());
  const gfx::Point end(start + gfx::Vector2d(0, 40));
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
}

void ShelfLayoutManagerTestBase::FlingUpOnShelf() {
  const gfx::Point location_start(display::Screen::GetScreen()
                                      ->GetPrimaryDisplay()
                                      .bounds()
                                      .bottom_center());
  const gfx::Point location_end(location_start.x(), 10);
  FlingBetweenLocations(location_start, location_end);
}

void ShelfLayoutManagerTestBase::FlingBetweenLocations(gfx::Point start,
                                                       gfx::Point end) {
  const base::TimeDelta kTimeDelta = base::Milliseconds(10);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
}

void ShelfLayoutManagerTestBase::DragHotseatDownToBezel() {
  gfx::Rect shelf_widget_bounds = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect hotseat_bounds =
      GetShelfWidget()->hotseat_widget()->GetWindowBoundsInScreen();
  gfx::Point start = hotseat_bounds.top_center();
  const gfx::Point end =
      gfx::Point(shelf_widget_bounds.x() + shelf_widget_bounds.width() / 2,
                 shelf_widget_bounds.bottom() + 1);
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);
  const int kNumScrollSteps = 4;
  GetEventGenerator()->GestureScrollSequence(start, end, kTimeDelta,
                                             kNumScrollSteps);
}

// Drag Shelf from |start| to |target| by mouse.
void ShelfLayoutManagerTestBase::MouseDragShelfTo(const gfx::Point& start,
                                                  const gfx::Point& target) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  generator->MoveMouseTo(start);
  generator->PressLeftButton();
  generator->DragMouseTo(target);
  generator->ReleaseLeftButton();
}

// Move mouse to show Shelf in auto-hide mode.
void ShelfLayoutManagerTestBase::MoveMouseToShowAutoHiddenShelf() {
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  const int display_bottom = display.bounds().bottom();
  GetEventGenerator()->MoveMouseTo(1, display_bottom - 1);
  ASSERT_TRUE(TriggerAutoHideTimeout());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, GetPrimaryShelf()->GetAutoHideState());
}

// Move mouse to |location| and do a two-finger scroll.
void ShelfLayoutManagerTestBase::DoTwoFingerScrollAtLocation(
    gfx::Point location,
    int x_offset,
    int y_offset,
    bool reverse_scroll) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kNaturalScroll, reverse_scroll);
  y_offset = reverse_scroll ? -y_offset : y_offset;
  GetEventGenerator()->ScrollSequence(location, base::TimeDelta(), x_offset,
                                      y_offset, /*steps=*/1,
                                      /*num_fingers=*/2);
}

// Move mouse to |location| and do a mousewheel scroll.
void ShelfLayoutManagerTestBase::DoMouseWheelScrollAtLocation(
    gfx::Point location,
    int delta_y,
    bool reverse_scroll) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  prefs->SetBoolean(prefs::kMouseReverseScroll, reverse_scroll);
  delta_y = reverse_scroll ? -delta_y : delta_y;
  GetEventGenerator()->MoveMouseTo(location);
  GetEventGenerator()->MoveMouseWheel(/*delta_x=*/0, delta_y);
}

void ShelfLayoutManagerTestBase::RunGestureDragTests(
    const gfx::Point& edge_to_hide,
    const gfx::Point& edge_to_show) {
  ui::test::EventGenerator* generator = GetEventGenerator();
  display::Display display = display::Screen::GetScreen()->GetPrimaryDisplay();
  generator->MoveMouseTo(display.bounds().CenterPoint());

  Shelf* shelf = GetPrimaryShelf();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);

  views::Widget* widget = CreateTestWidget();
  widget->Maximize();

  // The time delta should be large enough to prevent accidental fling creation.
  const base::TimeDelta kTimeDelta = base::Milliseconds(100);

  aura::Window* window = widget->GetNativeWindow();
  ShelfLayoutManager* layout_manager = GetShelfLayoutManager();
  layout_manager->LayoutShelf();

  gfx::Rect shelf_shown = GetShelfWidget()->GetWindowBoundsInScreen();
  gfx::Rect window_bounds_with_shelf = window->bounds();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  gfx::Rect window_bounds_with_noshelf = window->bounds();
  gfx::Rect shelf_hidden = GetShelfWidget()->GetWindowBoundsInScreen();

  // Tests the gesture drag on always shown shelf.
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kNever);
  layout_manager->LayoutShelf();

  const int kNumScrollSteps = 4;
  ShelfDragCallback handler(shelf_hidden, shelf_shown);

  // Swipe down on the always shown shelf should not auto-hide it.
  {
    SCOPED_TRACE("SWIPE_DOWN_ALWAYS_SHOWN");
    generator->GestureScrollSequenceWithCallback(
        edge_to_hide, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_shelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Verify that the shelf can still enter auto hide if the |widget_| has been
  // put into fullscreen.
  widget->SetFullscreen(true);
  WindowState* window_state = WindowState::Get(window);
  window_state->SetHideShelfWhenFullscreen(false);
  window->SetProperty(kImmersiveIsActive, true);
  layout_manager->UpdateVisibilityState(/*force_layout=*/false);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  // Swiping up should show the shelf if shelf is hidden in fullscreen mode.
  generator->GestureScrollSequence(edge_to_hide, edge_to_show, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  // Swiping down should hide the shelf.
  generator->GestureScrollSequence(edge_to_show, edge_to_hide, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  // Verify that after toggling fullscreen to off, the shelf is visible.
  widget->SetFullscreen(false);
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());

  // Minimize the visible window, the shelf should be shown if there are no
  // visible windows, even in auto-hide mode.
  window_state->Minimize();
  EXPECT_EQ(SHELF_VISIBLE, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideBehavior::kNever, shelf->auto_hide_behavior());

  // Tests gesture drag on auto-hide shelf.
  window_state->Maximize();
  shelf->SetAutoHideBehavior(ShelfAutoHideBehavior::kAlways);
  layout_manager->LayoutShelf();
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());

  // Swipe up the auto-hide shelf should show it.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_SHOW");
    generator->GestureScrollSequenceWithCallback(
        edge_to_hide, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  // Gesture drag should not change the auto hide behavior of shelf, even though
  // its visibility has been changed.
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  // The auto-hide shelf is above the window, which should not change the bounds
  // of the window.
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down very little. It shouldn't change any state.
  gfx::Point new_point(edge_to_show);
  gfx::Vector2d diff = edge_to_hide - edge_to_show;
  new_point.Offset(diff.x() * 3 / 10, diff.y() * 3 / 10);
  generator->GestureScrollSequence(edge_to_show, new_point, kTimeDelta, 5);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_1");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up in extended hit region to show it.
  gfx::Point extended_start = edge_to_show;
  if (shelf->IsHorizontalAlignment())
    extended_start.set_y(GetShelfWidget()->GetWindowBoundsInScreen().y() - 1);
  else if (ShelfAlignment::kLeft == shelf->alignment())
    extended_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().right() +
                         1);
  else if (ShelfAlignment::kRight == shelf->alignment())
    extended_start.set_x(GetShelfWidget()->GetWindowBoundsInScreen().x() - 1);
  {
    SCOPED_TRACE("SWIPE_UP_EXTENDED_HIT");
    generator->GestureScrollSequenceWithCallback(
        extended_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }

  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_2");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up outside the hit area. This should not change anything.
  gfx::Point outside_start =
      GetShelfWidget()->GetWindowBoundsInScreen().top_center();
  outside_start.set_y(outside_start.y() - 50);
  gfx::Vector2d delta = edge_to_hide - edge_to_show;
  generator->GestureScrollSequence(outside_start, outside_start + delta,
                                   kTimeDelta, kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  // Swipe up from the bottom of the shelf, this should show the shelf.
  gfx::Point below_start = edge_to_hide;
  generator->GestureScrollSequence(edge_to_hide, edge_to_show, kTimeDelta,
                                   kNumScrollSteps);
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe down again to hide.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_3");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(window_bounds_with_noshelf.ToString(), window->bounds().ToString());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Put |widget| into fullscreen. Set the shelf to be auto hidden when |widget|
  // is fullscreen. (eg browser immersive fullscreen).
  widget->SetFullscreen(true);
  WindowState::Get(window)->SetHideShelfWhenFullscreen(false);
  layout_manager->UpdateVisibilityState(/*force_layout=*/false);

  gfx::Rect window_bounds_fullscreen = window->bounds();
  EXPECT_TRUE(widget->IsFullscreen());

  EXPECT_EQ(window_bounds_with_noshelf.ToString(),
            window_bounds_fullscreen.ToString());

  // Swipe up. This should show the shelf.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_1");
    // Do not check bounds because the events outside of the bounds
    // will be clipped.
    generator->GestureScrollSequenceWithCallback(
        below_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScrollNoBoundsCheck,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());

  // Swipe down to hide the shelf.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_4");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_HIDDEN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_hidden.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
  EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());

  // Set the shelf to be hidden when |widget| is fullscreen. (eg tab fullscreen
  // with or without immersive browser fullscreen).
  WindowState::Get(window)->SetHideShelfWhenFullscreen(true);

  layout_manager->UpdateVisibilityState(/*force_layout=*/false);
  EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  // Swipe-up. This should not change anything.
  {
    SCOPED_TRACE("SWIPE_UP_NO_CHANGE");
    generator->GestureScrollSequenceWithCallback(
        below_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
    EXPECT_EQ(SHELF_HIDDEN, shelf->GetVisibilityState());
    EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
    EXPECT_EQ(window_bounds_fullscreen.ToString(), window->bounds().ToString());
  }

  // Minimize actually, otherwise further event may be affected since widget
  // is fullscreen status.
  widget->Minimize();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(layout_manager->HasVisibleWindow());

  // The shelf should be shown because there are no more visible windows.
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());

  // Swipe-down to hide. This should have no effect because there are no visible
  // windows.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_5");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Change the window state back to its Normal state. We do that by sending
  // a WM_EVENT_NORMAL to the window, instead of calling Widget::Restore()
  // function, because restoring from a kMinimized window state will take
  // the window back to its pre-minimized window state.
  WMEvent restore_event(WM_EVENT_NORMAL);
  WindowState::Get(widget->GetNativeWindow())->OnWMEvent(&restore_event);

  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(layout_manager->HasVisibleWindow());

  // Swipe up on the shelf. This should show the shelf but should not change the
  // auto-hide behavior, since auto-hide behavior can only be changed through
  // context menu of the shelf.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_2");
    // Do not check bounds because the events outside of the bounds
    // will be clipped.
    generator->GestureScrollSequenceWithCallback(
        below_start, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScrollNoBoundsCheck,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  widget->Close();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(layout_manager->HasVisibleWindow());

  // Swipe-down to hide. This should have no effect because there are no visible
  // windows.
  {
    SCOPED_TRACE("SWIPE_DOWN_AUTO_HIDE_6");
    generator->GestureScrollSequenceWithCallback(
        edge_to_show, edge_to_hide, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(ShelfAutoHideBehavior::kAlways, shelf->auto_hide_behavior());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());

  // Swipe up again on AUTO_HIDE_SHOWN shelf shouldn't change any state.
  // Swipe up on auto-hide shown shelf should still keep shelf shown.
  {
    SCOPED_TRACE("SWIPE_UP_AUTO_HIDE_4");
    generator->GestureScrollSequenceWithCallback(
        edge_to_hide, edge_to_show, kTimeDelta, kNumScrollSteps,
        base::BindRepeating(&ShelfDragCallback::ProcessScroll,
                            base::Unretained(&handler)));
  }
  EXPECT_EQ(SHELF_AUTO_HIDE, shelf->GetVisibilityState());
  EXPECT_EQ(SHELF_AUTO_HIDE_SHOWN, shelf->GetAutoHideState());
  EXPECT_EQ(shelf_shown.ToString(),
            GetShelfWidget()->GetWindowBoundsInScreen().ToString());
}

bool ShelfLayoutManagerTestBase::RunVisibilityUpdateForTrayCallback() {
  if (!GetShelfLayoutManager()
           ->visibility_update_for_tray_callback_.callback()) {
    return false;
  }
  GetShelfLayoutManager()
      ->visibility_update_for_tray_callback_.callback()
      .Run();
  return true;
}

}  //  namespace ash
