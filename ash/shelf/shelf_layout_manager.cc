// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/home_screen/home_launcher_gesture_handler.h"
#include "ash/home_screen/home_screen_controller.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/ash_features.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shell.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_types.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/constants/chromeos_switches.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/gesture_event_details.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

using ShelfWindowDragResult =
    DragWindowFromShelfController::ShelfWindowDragResult;

// Default Target Dim Opacity for floating shelf.
constexpr float kFloatingShelfDimOpacity = 0.74f;

// Target Dim Opacity for shelf when shelf is in the maximized state.
constexpr float kMaximizedShelfDimOpacity = 0.6f;

// Default opacity for shelf without dimming.
constexpr float kDefaultShelfOpacity = 1.0f;

// Delay before showing the shelf. This is after the mouse stops moving.
constexpr int kAutoHideDelayMS = 200;

// Duration of the animation to show or hide the shelf.
constexpr int kAnimationDurationMS = 200;

// To avoid hiding the shelf when the mouse transitions from a message bubble
// into the shelf, the hit test area is enlarged by this amount of pixels to
// keep the shelf from hiding.
constexpr int kNotificationBubbleGapHeight = 6;

// The maximum size of the region on the display opposing the shelf managed by
// this ShelfLayoutManager which can trigger showing the shelf.
// For instance:
// - Primary display is left of secondary display.
// - Shelf is left aligned
// - This ShelfLayoutManager manages the shelf for the secondary display.
// |kMaxAutoHideShowShelfRegionSize| refers to the maximum size of the region
// from the right edge of the primary display which can trigger showing the
// auto hidden shelf. The region is used to make it easier to trigger showing
// the auto hidden shelf when the shelf is on the boundary between displays.
constexpr int kMaxAutoHideShowShelfRegionSize = 10;

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

// Returns true if the window is in the app list window container.
bool IsAppListWindow(const aura::Window* window) {
  const aura::Window* parent = window->parent();
  return parent && parent->id() == kShellWindowId_AppListContainer;
}

bool IsTabletModeEnabled() {
  // Shell could be destroying. Shell destroys TabletModeController before
  // closing all windows.
  return Shell::Get()->tablet_mode_controller() &&
         Shell::Get()->tablet_mode_controller()->InTabletMode();
}

bool IsHotseatEnabled() {
  return IsTabletModeEnabled() && chromeos::switches::ShouldShowShelfHotseat();
}

// Returns HomeLauncherGestureHandler mode that should be used to handle shelf
// gestures.
ash::HomeLauncherGestureHandler::Mode
GetHomeLauncherGestureHandlerModeForDrag() {
  if (features::IsDragFromShelfToHomeOrOverviewEnabled() &&
      IsHotseatEnabled() && Shell::Get()->home_screen_controller() &&
      Shell::Get()->home_screen_controller()->IsHomeScreenVisible() &&
      Shell::Get()->overview_controller() &&
      !Shell::Get()->overview_controller()->InOverviewSession()) {
    return HomeLauncherGestureHandler::Mode::kSwipeHomeToOverview;
  }

  return HomeLauncherGestureHandler::Mode::kSlideUpToShow;
}

// Returns the |WorkspaceWindowState| of the currently active desk on the root
// window of |shelf_window|.
WorkspaceWindowState GetShelfWorkspaceWindowState(aura::Window* shelf_window) {
  DCHECK(shelf_window);
  // Shelf window does not belong to any desk, use the root to get the active
  // desk's workspace state.
  auto* controller =
      GetActiveWorkspaceController(shelf_window->GetRootWindow());
  DCHECK(controller);
  return controller->GetWindowState();
}

// Returns shelf's work area inset for given |visibility_state| and |size|.
int GetShelfInset(ShelfVisibilityState visibility_state, int size) {
  return visibility_state == SHELF_VISIBLE ? size : 0;
}

// Returns the window that can be dragged from shelf into home screen or
// overview at |location_in_screen|. Returns nullptr if there is no such
// window.
aura::Window* GetWindowForDragToHomeOrOverview(
    const gfx::Point& location_in_screen) {
  if (!IsTabletModeEnabled())
    return nullptr;

  auto mru_windows =
      Shell::Get()->mru_window_tracker()->BuildWindowForCycleList(kActiveDesk);
  if (mru_windows.empty())
    return nullptr;

  aura::Window* window = nullptr;
  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const bool is_in_splitview = split_view_controller->InSplitViewMode();
  const bool is_in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  if (!is_in_splitview && !is_in_overview) {
    // If split view mode is not active, use the first MRU window.
    window = mru_windows[0];
  } else if (is_in_splitview) {
    // If split view mode is active, use the event location to decide which
    // window should be the dragged window.
    aura::Window* left_window = split_view_controller->left_window();
    aura::Window* right_window = split_view_controller->right_window();
    const int divider_position = split_view_controller->divider_position();
    const bool is_landscape = IsCurrentScreenOrientationLandscape();
    const bool is_primary = IsCurrentScreenOrientationPrimary();
    const gfx::Rect work_area =
        screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
            split_view_controller->GetDefaultSnappedWindow());
    if (is_landscape) {
      if (location_in_screen.x() < work_area.x() + divider_position)
        window = is_primary ? left_window : right_window;
      else
        window = is_primary ? right_window : left_window;
    } else {
      window = is_primary ? right_window : left_window;
    }
  }
  return window && window->IsVisible() ? window : nullptr;
}

// Calculates the type of hotseat gesture which should be recorded in histogram.
// Returns the null value if no gesture should be recorded.
base::Optional<InAppShelfGestures> CalculateHotseatGestureToRecord(
    base::Optional<ShelfWindowDragResult> window_drag_result,
    HotseatState old_state,
    HotseatState current_state) {
  if (window_drag_result.has_value() &&
      window_drag_result == ShelfWindowDragResult::kGoToOverviewMode &&
      old_state == HotseatState::kHidden) {
    return InAppShelfGestures::kSwipeUpToShow;
  }

  if (window_drag_result.has_value() &&
      window_drag_result == ShelfWindowDragResult::kGoToHomeScreen) {
    return InAppShelfGestures::kFlingUpToShowHomeScreen;
  }

  if (old_state == current_state)
    return base::nullopt;

  if (current_state == HotseatState::kHidden)
    return InAppShelfGestures::kSwipeDownToHide;

  if (current_state == HotseatState::kExtended)
    return InAppShelfGestures::kSwipeUpToShow;

  return base::nullopt;
}

// Sets the shelf opacity to 0 when the shelf is done hiding to avoid getting
// rid of blur.
class HideAnimationObserver : public ui::ImplicitAnimationObserver {
 public:
  explicit HideAnimationObserver(ui::Layer* layer) : layer_(layer) {}

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsScheduled() override {}

  void OnImplicitAnimationsCompleted() override { layer_->SetOpacity(0); }

 private:
  // Unowned.
  ui::Layer* layer_;

  DISALLOW_COPY_AND_ASSIGN(HideAnimationObserver);
};

// Forwards gesture events to ShelfLayoutManager to hide the hotseat
// when it is kExtended.
class HotseatEventHandler : public ui::EventHandler,
                            public ShelfLayoutManagerObserver {
 public:
  explicit HotseatEventHandler(ShelfLayoutManager* shelf_layout_manager)
      : shelf_layout_manager_(shelf_layout_manager) {
    shelf_layout_manager_->AddObserver(this);
    Shell::Get()->AddPreTargetHandler(this);
  }
  ~HotseatEventHandler() override {
    shelf_layout_manager_->RemoveObserver(this);
    Shell::Get()->RemovePreTargetHandler(this);
  }

  // ShelfLayoutManagerObserver:
  void OnHotseatStateChanged(HotseatState old_state,
                             HotseatState new_state) override {
    should_forward_event_ = new_state == HotseatState::kExtended;
  }

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override {
    if (!should_forward_event_)
      return;
    shelf_layout_manager_->ProcessGestureEventOfInAppHotseat(
        event, static_cast<aura::Window*>(event->target()));
  }

 private:
  // Whether events should get forwarded to ShelfLayoutManager.
  bool should_forward_event_ = false;
  ShelfLayoutManager* const shelf_layout_manager_;  // unowned.
  DISALLOW_COPY_AND_ASSIGN(HotseatEventHandler);
};

}  // namespace

// ShelfLayoutManager::UpdateShelfObserver -------------------------------------

// UpdateShelfObserver is used to delay updating the background until the
// animation completes.
class ShelfLayoutManager::UpdateShelfObserver
    : public ui::ImplicitAnimationObserver {
 public:
  explicit UpdateShelfObserver(ShelfLayoutManager* shelf) : shelf_(shelf) {
    shelf_->update_shelf_observer_ = this;
  }

  void Detach() { shelf_ = nullptr; }

  void OnImplicitAnimationsCompleted() override {
    if (shelf_)
      shelf_->MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
    delete this;
  }

 private:
  ~UpdateShelfObserver() override {
    if (shelf_)
      shelf_->update_shelf_observer_ = nullptr;
  }

  // Shelf we're in. nullptr if deleted before we're deleted.
  ShelfLayoutManager* shelf_;

  DISALLOW_COPY_AND_ASSIGN(UpdateShelfObserver);
};

ShelfLayoutManager::State::State()
    : visibility_state(SHELF_VISIBLE),
      auto_hide_state(SHELF_AUTO_HIDE_HIDDEN),
      window_state(WorkspaceWindowState::kDefault),
      pre_lock_screen_animation_active(false),
      session_state(session_manager::SessionState::UNKNOWN) {}

bool ShelfLayoutManager::State::IsAddingSecondaryUser() const {
  return session_state == session_manager::SessionState::LOGIN_SECONDARY;
}

bool ShelfLayoutManager::State::IsScreenLocked() const {
  return session_state == session_manager::SessionState::LOCKED;
}

bool ShelfLayoutManager::State::IsActiveSessionState() const {
  return session_state == session_manager::SessionState::ACTIVE;
}

bool ShelfLayoutManager::State::IsShelfAutoHidden() const {
  return visibility_state == SHELF_AUTO_HIDE &&
         auto_hide_state == SHELF_AUTO_HIDE_HIDDEN;
}

bool ShelfLayoutManager::State::IsShelfVisible() const {
  return visibility_state == SHELF_VISIBLE ||
         (visibility_state == SHELF_AUTO_HIDE &&
          auto_hide_state == SHELF_AUTO_HIDE_SHOWN);
}

bool ShelfLayoutManager::State::Equals(const State& other) const {
  return other.visibility_state == visibility_state &&
         (visibility_state != SHELF_AUTO_HIDE ||
          other.auto_hide_state == auto_hide_state) &&
         other.window_state == window_state &&
         other.pre_lock_screen_animation_active ==
             pre_lock_screen_animation_active &&
         other.session_state == session_state;
}

// ShelfLayoutManager::ScopedSuspendVisibilityUpdate ---------------------------

ShelfLayoutManager::ScopedSuspendWorkAreaUpdate::ScopedSuspendWorkAreaUpdate(
    ShelfLayoutManager* manager)
    : manager_(manager) {
  manager_->SuspendWorkAreaUpdate();
}

ShelfLayoutManager::ScopedSuspendWorkAreaUpdate::
    ~ScopedSuspendWorkAreaUpdate() {
  manager_->ResumeWorkAreaUpdate();
}

// ShelfLayoutManager ----------------------------------------------------------

ShelfLayoutManager::ShelfLayoutManager(ShelfWidget* shelf_widget, Shelf* shelf)
    : shelf_widget_(shelf_widget),
      shelf_(shelf),
      is_background_blur_enabled_(features::IsBackgroundBlurEnabled()) {
  DCHECK(shelf_widget_);
  DCHECK(shelf_);
}

ShelfLayoutManager::~ShelfLayoutManager() {
  if (update_shelf_observer_)
    update_shelf_observer_->Detach();

  // |hotseat_event_handler_| needs to be released before ShelfLayoutManager.
  hotseat_event_handler_.reset();

  // Ensures that |overview_suspend_work_area_update_| is released before
  // ShelfLayoutManager.
  overview_suspend_work_area_update_.reset();

  for (auto& observer : observers_)
    observer.WillDeleteShelfLayoutManager();
  display::Screen::GetScreen()->RemoveObserver(this);
  auto* shell = Shell::Get();
  shell->locale_update_controller()->RemoveObserver(this);
  shell->RemoveShellObserver(this);
  shell->lock_state_controller()->RemoveObserver(this);
  if (shell->app_list_controller())
    shell->app_list_controller()->RemoveObserver(this);
  if (shell->overview_controller())
    shell->overview_controller()->RemoveObserver(this);
}

void ShelfLayoutManager::InitObservers() {
  auto* shell = Shell::Get();
  shell->AddShellObserver(this);
  SplitViewController::Get(shelf_widget_->GetNativeWindow())->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->app_list_controller()->AddObserver(this);
  shell->lock_state_controller()->AddObserver(this);
  shell->activation_client()->AddObserver(this);
  shell->locale_update_controller()->AddObserver(this);
  state_.session_state = shell->session_controller()->GetSessionState();
  shelf_background_type_ = GetShelfBackgroundType();
  wallpaper_controller_observer_.Add(shell->wallpaper_controller());
  display::Screen::GetScreen()->AddObserver(this);

  // DesksController could be null when virtual desks feature is not enabled.
  if (DesksController::Get())
    DesksController::Get()->AddObserver(this);
}

void ShelfLayoutManager::PrepareForShutdown() {
  in_shutdown_ = true;

  // Stop observing changes to avoid updating a partially destructed shelf.
  Shell::Get()->activation_client()->RemoveObserver(this);

  // DesksController could be null when virtual desks feature is not enabled.
  if (DesksController::Get())
    DesksController::Get()->RemoveObserver(this);

  SplitViewController::Get(shelf_widget_->GetNativeWindow())
      ->RemoveObserver(this);
}

bool ShelfLayoutManager::IsVisible() const {
  // status_area_widget() may be nullptr during the shutdown.
  return shelf_widget_->status_area_widget() &&
         shelf_widget_->status_area_widget()->IsVisible() &&
         state_.IsShelfVisible();
}

gfx::Rect ShelfLayoutManager::GetIdealBounds() const {
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  gfx::Rect rect(screen_util::GetDisplayBoundsInParent(shelf_window));
  return SelectValueForShelfAlignment(
      gfx::Rect(rect.x(), rect.bottom() - shelf_size, rect.width(), shelf_size),
      gfx::Rect(rect.x(), rect.y(), shelf_size, rect.height()),
      gfx::Rect(rect.right() - shelf_size, rect.y(), shelf_size,
                rect.height()));
}

gfx::Rect ShelfLayoutManager::GetIdealBoundsForWorkAreaCalculation() const {
  if (!IsTabletModeEnabled() || !chromeos::switches::ShouldShowShelfHotseat())
    return GetIdealBounds();

  // For the work-area calculation in tablet mode, always use in-app shelf
  // bounds, because when the shelf is not in-app the UI is either showing
  // AppList or Overview, and updating the WorkArea with the new Shelf size
  // would cause unnecessary work.
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  gfx::Rect rect(screen_util::GetDisplayBoundsInParent(shelf_window));
  const int in_app_shelf_size = ShelfConfig::Get()->in_app_shelf_size();
  rect.set_y(rect.bottom() - in_app_shelf_size);
  rect.set_height(in_app_shelf_size);
  return rect;
}

void ShelfLayoutManager::UpdateVisibilityState() {
  // Bail out early after shelf is destroyed or visibility update is suspended.
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  if (in_shutdown_ || !shelf_window || suspend_visibility_update_)
    return;

  const WorkspaceWindowState window_state =
      GetShelfWorkspaceWindowState(shelf_window);

  if (shelf_->ShouldHideOnSecondaryDisplay(state_.session_state)) {
    // Needed to hide system tray on secondary display.
    SetState(SHELF_HIDDEN);
  } else if (!state_.IsActiveSessionState()) {
    // Needed to show system tray in non active session state.
    SetState(SHELF_VISIBLE);
  } else if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
    SetState(SHELF_HIDDEN);
  } else {
    // TODO(zelidrag): Verify shelf drag animation still shows on the device
    // when we are in SHELF_AUTO_HIDE_ALWAYS_HIDDEN.
    switch (window_state) {
      case WorkspaceWindowState::kFullscreen:
        if (IsShelfAutoHideForFullscreenMaximized()) {
          SetState(SHELF_AUTO_HIDE);
        } else if (IsShelfHiddenForFullscreen()) {
          SetState(SHELF_HIDDEN);
        } else {
          // The shelf is sometimes not hidden when in immersive fullscreen.
          // Force the shelf to be auto hidden in this case.
          SetState(SHELF_AUTO_HIDE);
        }
        break;
      case WorkspaceWindowState::kMaximized:
        SetState(IsShelfAutoHideForFullscreenMaximized()
                     ? SHELF_AUTO_HIDE
                     : CalculateShelfVisibility());
        break;
      case WorkspaceWindowState::kDefault:
        SetState(CalculateShelfVisibility());
        break;
    }
  }

  UpdateWorkspaceMask(window_state);
  SendA11yAlertForFullscreenWorkspaceState(window_state);
}

void ShelfLayoutManager::UpdateAutoHideState() {
  ShelfAutoHideState auto_hide_state =
      CalculateAutoHideState(state_.visibility_state);
  if (auto_hide_state != state_.auto_hide_state) {
    if (auto_hide_state == SHELF_AUTO_HIDE_HIDDEN) {
      // Hides happen immediately.
      SetState(state_.visibility_state);
    } else {
      if (!auto_hide_timer_.IsRunning()) {
        mouse_over_shelf_when_auto_hide_timer_started_ =
            shelf_widget_->GetWindowBoundsInScreen().Contains(
                display::Screen::GetScreen()->GetCursorScreenPoint());
      }
      StartAutoHideTimer();
    }
  } else {
    StopAutoHideTimer();
  }
}

void ShelfLayoutManager::UpdateAutoHideForMouseEvent(ui::MouseEvent* event,
                                                     aura::Window* target) {
  // This also checks IsShelfWindow() and IsStatusAreaWindow() to make sure we
  // don't attempt to hide the shelf if the mouse down occurs on the shelf.
  in_mouse_drag_ = (event->type() == ui::ET_MOUSE_DRAGGED ||
                    (in_mouse_drag_ && event->type() != ui::ET_MOUSE_RELEASED &&
                     event->type() != ui::ET_MOUSE_CAPTURE_CHANGED)) &&
                   !IsShelfWindow(target) && !IsStatusAreaWindow(target);

  // Don't update during shutdown because synthetic mouse events (e.g. mouse
  // exit) may be generated during status area widget teardown.
  if (visibility_state() != SHELF_AUTO_HIDE || in_shutdown_)
    return;

  if (event->type() == ui::ET_MOUSE_PRESSED ||
      event->type() == ui::ET_MOUSE_MOVED) {
    if (GetVisibleShelfBounds().Contains(
            display::Screen::GetScreen()->GetCursorScreenPoint())) {
      UpdateAutoHideState();
      last_seen_mouse_position_was_over_shelf_ = true;
    } else {
      // The event happened outside the shelf's bounds. If it's a click, hide
      // the shelf immediately. If it's a mouse-out, hide after a delay (but
      // only if it really is a mouse-out, meaning the mouse actually exited the
      // shelf bounds as opposed to having been outside all along).
      if (event->type() == ui::ET_MOUSE_PRESSED)
        UpdateAutoHideState();
      else if (last_seen_mouse_position_was_over_shelf_)
        StartAutoHideTimer();
      last_seen_mouse_position_was_over_shelf_ = false;
    }
  }
}

void ShelfLayoutManager::ProcessGestureEventOfAutoHideShelf(
    ui::GestureEvent* event,
    aura::Window* target) {
  const bool is_shelf_window = IsShelfWindow(target);
  // Skip event processing if shelf widget is fully visible to let the default
  // event dispatching do its work.
  if (IsVisible() || in_shutdown_) {
    // Tap outside of the AUTO_HIDE_SHOWN shelf should hide it.
    if (!is_shelf_window && !IsStatusAreaWindow(target) &&
        visibility_state() == SHELF_AUTO_HIDE &&
        state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN &&
        event->type() == ui::ET_GESTURE_TAP) {
      UpdateAutoHideState();
    }

    // Complete gesture drag when Shelf is visible in auto-hide mode. It is
    // called when swiping Shelf up to show.
    if (is_shelf_window && !IsStatusAreaWindow(target) &&
        visibility_state() == SHELF_AUTO_HIDE &&
        state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN &&
        event->type() == ui::ET_GESTURE_END && drag_status_ != kDragNone) {
      CompleteDrag(*event);
    }
    return;
  }

  if (is_shelf_window) {
    ui::GestureEvent event_in_screen(*event);
    gfx::Point location_in_screen(event->location());
    ::wm::ConvertPointToScreen(target, &location_in_screen);
    event_in_screen.set_location(location_in_screen);
    if (ProcessGestureEvent(event_in_screen))
      event->StopPropagation();
  }
}

void ShelfLayoutManager::ProcessGestureEventOfInAppHotseat(
    ui::GestureEvent* event,
    aura::Window* target) {
  if (!IsHotseatEnabled())
    return;
  DCHECK_EQ(hotseat_state(), HotseatState::kExtended);

  if (IsShelfWindow(target) || drag_status_ != DragStatus::kDragNone)
    return;

  base::AutoReset<bool> hide_hotseat(&should_hide_hotseat_, true);
  UpdateVisibilityState();
}

void ShelfLayoutManager::AddObserver(ShelfLayoutManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void ShelfLayoutManager::RemoveObserver(ShelfLayoutManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool ShelfLayoutManager::ProcessGestureEvent(
    const ui::GestureEvent& event_in_screen) {
  if (!IsDragAllowed())
    return false;

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_BEGIN)
    return StartGestureDrag(event_in_screen);

  if (drag_status_ != kDragInProgress &&
      drag_status_ != kDragAppListInProgress &&
      drag_status_ != kDragHomeToOverviewInProgress) {
    return false;
  }

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateGestureDrag(event_in_screen);
    return true;
  }

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_END ||
      event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
      last_drag_velocity_ =
          event_in_screen.AsGestureEvent()->details().velocity_y();
    }
    if (drag_status_ == kDragAppListInProgress ||
        drag_status_ == kDragHomeToOverviewInProgress) {
      CompleteAppListDrag(event_in_screen);
    } else {
      CompleteDrag(event_in_screen);
    }
    return true;
  }

  // Unexpected event. Reset the state and let the event fall through.
  CancelDrag();
  return false;
}

void ShelfLayoutManager::ProcessMouseEventFromShelf(
    const ui::MouseEvent& event_in_screen) {
  ui::EventType event_type = event_in_screen.type();
  DCHECK(event_type == ui::ET_MOUSE_PRESSED ||
         event_type == ui::ET_MOUSE_DRAGGED ||
         event_type == ui::ET_MOUSE_RELEASED);

  if (!IsDragAllowed())
    return;

  switch (event_type) {
    case ui::ET_MOUSE_PRESSED:
      AttemptToDragByMouse(event_in_screen);
      break;
    case ui::ET_MOUSE_DRAGGED:
      UpdateMouseDrag(event_in_screen);
      return;
    case ui::ET_MOUSE_RELEASED:
      ReleaseMouseDrag(event_in_screen);
      return;
    default:
      NOTREACHED();
      return;
  }
}

void ShelfLayoutManager::ProcessGestureEventFromShelfWidget(
    ui::GestureEvent* event_in_screen) {
  if (ProcessGestureEvent(*event_in_screen))
    event_in_screen->StopPropagation();
}

void ShelfLayoutManager::ProcessMouseWheelEventFromShelf(
    ui::MouseWheelEvent* event) {
  if (event->y_offset() <=
      ShelfConfig::Get()->mousewheel_scroll_offset_threshold()) {
    return;
  }
  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(shelf_widget_->GetNativeWindow())
          .id(),
      kScrollFromShelf, event->time_stamp());
}

ShelfBackgroundType ShelfLayoutManager::GetShelfBackgroundType() const {
  if (state_.pre_lock_screen_animation_active)
    return SHELF_BACKGROUND_DEFAULT;

  // Handle all other non active screen states, including OOBE and pre-login.
  if (state_.session_state == session_manager::SessionState::OOBE)
    return SHELF_BACKGROUND_OOBE;
  if (state_.session_state != session_manager::SessionState::ACTIVE) {
    if (Shell::Get()->wallpaper_controller()->HasShownAnyWallpaper() &&
        !Shell::Get()->wallpaper_controller()->IsWallpaperBlurred()) {
      return SHELF_BACKGROUND_LOGIN_NONBLURRED_WALLPAPER;
    }
    return SHELF_BACKGROUND_LOGIN;
  }

  const bool in_split_view_mode =
      SplitViewController::Get(shelf_widget_->GetNativeWindow())
          ->InSplitViewMode();
  const bool maximized =
      in_split_view_mode ||
      state_.window_state == WorkspaceWindowState::kFullscreen ||
      (state_.window_state == WorkspaceWindowState::kMaximized &&
       !Shell::Get()
            ->home_screen_controller()
            ->home_launcher_gesture_handler()
            ->GetActiveWindow());
  const bool app_list_is_visible =
      Shell::Get()->app_list_controller() &&
      Shell::Get()->app_list_controller()->IsVisible();
  if (IsTabletModeEnabled()) {
    // If the home launcher is shown, being animated, or dragged, show the
    // home launcher background.
    if (app_list_is_visible ||
        Shell::Get()->app_list_controller()->home_launcher_transition_state() !=
            AppListControllerImpl::HomeLauncherTransitionState::kFinished)
      return SHELF_BACKGROUND_HOME_LAUNCHER;
  } else if (app_list_is_visible) {
    return maximized ? SHELF_BACKGROUND_MAXIMIZED_WITH_APP_LIST
                     : SHELF_BACKGROUND_APP_LIST;
  }

  if (maximized) {
    return SHELF_BACKGROUND_MAXIMIZED;
  }

  if (Shell::Get()->overview_controller() &&
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return SHELF_BACKGROUND_OVERVIEW;
  }

  return SHELF_BACKGROUND_DEFAULT;
}

void ShelfLayoutManager::MaybeUpdateShelfBackground(AnimationChangeType type) {
  const ShelfBackgroundType new_background_type(GetShelfBackgroundType());

  if (new_background_type == shelf_background_type_)
    return;

  shelf_background_type_ = new_background_type;
  for (auto& observer : observers_)
    observer.OnBackgroundUpdated(shelf_background_type_, type);
}

bool ShelfLayoutManager::ShouldBlurShelfBackground() {
  return is_background_blur_enabled_ &&
         shelf_background_type_ == SHELF_BACKGROUND_DEFAULT &&
         state_.session_state == session_manager::SessionState::ACTIVE;
}

bool ShelfLayoutManager::HasVisibleWindow() const {
  aura::Window* root = shelf_widget_->GetNativeWindow()->GetRootWindow();
  const aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);
  // Process the window list and check if there are any visible windows.
  // Ignore app list windows that may be animating to hide after dismissal.
  for (auto* window : windows) {
    if (window->IsVisible() && !IsAppListWindow(window) &&
        root->Contains(window)) {
      return true;
    }
  }
  auto* pip_container = Shell::GetContainer(root, kShellWindowId_PipContainer);
  // The PIP window is not activatable and is not in the MRU list, but count
  // it as a visible window for shelf auto-hide purposes. See crbug.com/942991.
  return !pip_container->children().empty();
}

void ShelfLayoutManager::CancelDragOnShelfIfInProgress() {
  if (drag_status_ == kDragInProgress ||
      drag_status_ == kDragAppListInProgress ||
      drag_status_ == kDragHomeToOverviewInProgress) {
    CancelDrag();
  }
}

void ShelfLayoutManager::SuspendVisibilityUpdateForShutdown() {
  ++suspend_visibility_update_;
}

void ShelfLayoutManager::OnShelfItemSelected(ShelfAction action) {
  switch (action) {
    case SHELF_ACTION_NONE:
    case SHELF_ACTION_APP_LIST_SHOWN:
    case SHELF_ACTION_APP_LIST_DISMISSED:
    case SHELF_ACTION_APP_LIST_BACK:
    case SHELF_ACTION_WINDOW_MINIMIZED:
      break;
    case SHELF_ACTION_NEW_WINDOW_CREATED:
    case SHELF_ACTION_WINDOW_ACTIVATED: {
      base::AutoReset<bool> reset(&should_hide_hotseat_, true);
      UpdateVisibilityState();
    } break;
  }
}

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
}

void ShelfLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  WmDefaultLayoutManager::SetChildBounds(child, requested_bounds);
  // We may contain other widgets (such as frame maximize bubble) but they don't
  // effect the layout in anyway.
  if (!updating_bounds_ &&
      ((shelf_widget_->GetNativeWindow() == child) ||
       (shelf_widget_->status_area_widget()->GetNativeWindow() == child))) {
    LayoutShelf();
  }
}

void ShelfLayoutManager::OnShelfAutoHideBehaviorChanged(
    aura::Window* root_window) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnUserWorkAreaInsetsChanged(
    aura::Window* root_window) {
  LayoutShelf();
}

void ShelfLayoutManager::OnPinnedStateChanged(aura::Window* pinned_window) {
  // Shelf needs to be hidden on entering to pinned mode, or restored
  // on exiting from pinned mode.
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnSplitViewStateChanged(
    SplitViewController::State previous_state,
    SplitViewController::State state) {
  if (previous_state == SplitViewController::State::kNoSnap ||
      state == SplitViewController::State::kNoSnap) {
    MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
  }
}

void ShelfLayoutManager::OnOverviewModeWillStart() {
  overview_mode_will_start_ = true;
}

void ShelfLayoutManager::OnOverviewModeStarting() {
  overview_mode_will_start_ = false;
  overview_suspend_work_area_update_.emplace(this);
}

void ShelfLayoutManager::OnOverviewModeStartingAnimationComplete(
    bool canceled) {
  // If transition is canceled, keep work area updates suspended, as new
  // overview transition is about to start.
  if (canceled)
    return;
  overview_suspend_work_area_update_.reset();
}

void ShelfLayoutManager::OnOverviewModeEnding(OverviewSession* session) {
  overview_suspend_work_area_update_.emplace(this);
}

void ShelfLayoutManager::OnOverviewModeEndingAnimationComplete(bool canceled) {
  // If transition is canceled, keep work area updates suspended, as new
  // overview transition is about to start.
  if (canceled)
    return;
  overview_suspend_work_area_update_.reset();
}

void ShelfLayoutManager::OnOverviewModeEnded() {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnAppListVisibilityWillChange(bool shown,
                                                       int64_t display_id) {
  // We respond to "will change" and "did change" notifications in the same
  // way.
  OnAppListVisibilityChanged(shown, display_id);
}

void ShelfLayoutManager::OnAppListVisibilityChanged(bool shown,
                                                    int64_t display_id) {
  // Shell may be under destruction.
  if (!shelf_widget_ || !shelf_widget_->GetNativeWindow())
    return;

  if (display_.id() != display_id)
    return;

  UpdateVisibilityState();
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
}

void ShelfLayoutManager::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  UpdateAutoHideStateNow();
}

void ShelfLayoutManager::OnLockStateEvent(LockStateObserver::EventType event) {
  if (event == EVENT_LOCK_ANIMATION_STARTED) {
    // Enter the screen locked state and update the visibility to avoid an odd
    // animation when transitioning the orientation from L/R to bottom.
    state_.pre_lock_screen_animation_active = true;
    UpdateShelfVisibilityAfterLoginUIChange();
  } else {
    state_.pre_lock_screen_animation_active = false;
  }
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnSessionStateChanged(
    session_manager::SessionState state) {
  // Check transition changes to/from the add user to session and change the
  // shelf alignment accordingly
  const bool was_adding_user = state_.IsAddingSecondaryUser();
  const bool was_locked = state_.IsScreenLocked();
  state_.session_state = state;
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
  if (was_adding_user != state_.IsAddingSecondaryUser()) {
    UpdateShelfVisibilityAfterLoginUIChange();
    return;
  }

  // Force the shelf to layout for alignment (bottom if locked, restore the
  // previous alignment otherwise).
  if (was_locked != state_.IsScreenLocked())
    UpdateShelfVisibilityAfterLoginUIChange();

  CalculateTargetBoundsAndUpdateWorkArea(hotseat_state());
  UpdateBoundsAndOpacity(true /* animate */, nullptr);
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnLoginStatusChanged(LoginStatus loing_status) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnWallpaperBlurChanged() {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnFirstWallpaperShown() {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  // Update |user_work_area_bounds_| for the new display arrangement.
  CalculateTargetBoundsAndUpdateWorkArea(hotseat_state());
}

void ShelfLayoutManager::OnLocaleChanged() {
  // Layout update is needed when language changes between LTR and RTL.
  LayoutShelfAndUpdateBounds();
}

void ShelfLayoutManager::OnDeskSwitchAnimationLaunching() {
  ++suspend_visibility_update_;
}

void ShelfLayoutManager::OnDeskSwitchAnimationFinished() {
  --suspend_visibility_update_;
  DCHECK_GE(suspend_visibility_update_, 0);
  if (!suspend_visibility_update_)
    UpdateVisibilityState();
}

int ShelfLayoutManager::CalculateHotseatYInShelf(
    HotseatState hotseat_target_state) const {
  DCHECK(shelf_->IsHorizontalAlignment());
  int hotseat_distance_from_bottom_of_display;
  const int hotseat_size = ShelfConfig::Get()->hotseat_size();
  switch (hotseat_target_state) {
    case HotseatState::kShown: {
      // When the hotseat state is HotseatState::kShown in tablet mode, the
      // home launcher is showing. Elevate the hotseat a few px to match the
      // navigation and status area.
      const bool use_padding = IsHotseatEnabled();
      hotseat_distance_from_bottom_of_display =
          hotseat_size +
          (use_padding ? ShelfConfig::Get()->hotseat_bottom_padding() : 0);
    } break;
    case HotseatState::kHidden:
      // Show the hotseat offscreen.
      hotseat_distance_from_bottom_of_display = 0;
      break;
    case HotseatState::kExtended:
      // Show the hotseat at its extended position.
      hotseat_distance_from_bottom_of_display =
          ShelfConfig::Get()->in_app_shelf_size() +
          ShelfConfig::Get()->hotseat_bottom_padding() + hotseat_size;
      break;
  }
  const int target_shelf_size = hotseat_target_state == HotseatState::kShown
                                    ? ShelfConfig::Get()->system_shelf_size()
                                    : ShelfConfig::Get()->in_app_shelf_size();
  const int hotseat_y_in_shelf =
      -(hotseat_distance_from_bottom_of_display - target_shelf_size);
  return hotseat_y_in_shelf;
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

ShelfLayoutManager::TargetBounds::TargetBounds() : opacity(0.0f) {}

ShelfLayoutManager::TargetBounds::~TargetBounds() = default;

void ShelfLayoutManager::SuspendWorkAreaUpdate() {
  ++suspend_work_area_update_;
}

void ShelfLayoutManager::ResumeWorkAreaUpdate() {
  --suspend_work_area_update_;
  DCHECK_GE(suspend_work_area_update_, 0);

  if (suspend_work_area_update_ || in_shutdown_)
    return;

  UpdateVisibilityState();

  CalculateTargetBoundsAndUpdateWorkArea(hotseat_state());
  UpdateBoundsAndOpacity(/*animate=*/true, nullptr);
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::SetState(ShelfVisibilityState visibility_state) {
  if (suspend_visibility_update_)
    return;

  State state;
  const HotseatState previous_hotseat_state = hotseat_state();
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);
  state.window_state =
      GetShelfWorkspaceWindowState(shelf_widget_->GetNativeWindow());
  HotseatState new_hotseat_state =
      CalculateHotseatState(state.visibility_state, state.auto_hide_state);
  // Preserve the log in screen states.
  state.session_state = state_.session_state;
  state.pre_lock_screen_animation_active =
      state_.pre_lock_screen_animation_active;

  // Force an update because drag events affect the shelf bounds and we
  // should animate back to the normal bounds at the end of the drag event.
  bool force_update = (drag_status_ == kDragCancelInProgress ||
                       drag_status_ == kDragCompleteInProgress);

  if (!force_update && state_.Equals(state) &&
      previous_hotseat_state == new_hotseat_state) {
    return;  // Nothing changed.
  }

  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(visibility_state);

  StopAutoHideTimer();

  State old_state = state_;
  state_ = state;

  AnimationChangeType change_type = AnimationChangeType::ANIMATE;
  bool delay_background_change = false;

  // Do not animate the background when:
  // - Going from a hidden / auto hidden shelf in fullscreen to a visible shelf
  //   in tablet mode.
  // - Going from an auto hidden shelf in tablet mode to a visible shelf in
  //   tablet mode.
  // - Doing so would result in animating the opacity of the shelf while it is
  //   showing blur.
  if (state.window_state == WorkspaceWindowState::kMaximized &&
      ((state.visibility_state == SHELF_VISIBLE &&
        old_state.visibility_state != SHELF_VISIBLE) ||
       is_background_blur_enabled_)) {
    change_type = AnimationChangeType::IMMEDIATE;
  } else {
    // Delay the animation when the shelf was hidden, and has just been made
    // visible (e.g. using a gesture-drag).
    if (state.visibility_state == SHELF_VISIBLE &&
        old_state.IsShelfAutoHidden()) {
      delay_background_change = true;
    }
  }

  if (delay_background_change) {
    if (update_shelf_observer_)
      update_shelf_observer_->Detach();
    // |update_shelf_observer_| deletes itself when the animation is done.
    update_shelf_observer_ = new UpdateShelfObserver(this);
  } else {
    MaybeUpdateShelfBackground(change_type);
  }

  CalculateTargetBoundsAndUpdateWorkArea(new_hotseat_state);
  UpdateBoundsAndOpacity(true /* animate */, delay_background_change
                                                 ? update_shelf_observer_
                                                 : nullptr);

  // OnAutoHideStateChanged Should be emitted when:
  //  - firstly state changed to auto-hide from other state
  //  - or, auto_hide_state has changed
  if ((old_state.visibility_state != state_.visibility_state &&
       state_.visibility_state == SHELF_AUTO_HIDE) ||
      old_state.auto_hide_state != state_.auto_hide_state) {
    for (auto& observer : observers_)
      observer.OnAutoHideStateChanged(state_.auto_hide_state);
  }

  // Do not set the hotseat state until after bounds have been set because
  // observers rely on final bounds.
  shelf_widget_->hotseat_widget()->SetState(new_hotseat_state);
  if (previous_hotseat_state != hotseat_state()) {
    if (hotseat_state() == HotseatState::kExtended)
      hotseat_event_handler_ = std::make_unique<HotseatEventHandler>(this);
    else
      hotseat_event_handler_.reset();
    for (auto& observer : observers_)
      observer.OnHotseatStateChanged(previous_hotseat_state, hotseat_state());
  }
}

HotseatState ShelfLayoutManager::CalculateHotseatState(
    ShelfVisibilityState visibility_state,
    ShelfAutoHideState auto_hide_state) {
  if (!IsHotseatEnabled() || !shelf_->IsHorizontalAlignment())
    return HotseatState::kShown;

  auto* app_list_controller = Shell::Get()->app_list_controller();
  // If the app list controller is null, we are probably in the middle of
  // a shutdown, let's not change the hotseat state.
  if (!app_list_controller)
    return hotseat_state();
  const auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview =
      ((overview_controller && overview_controller->InOverviewSession()) ||
       overview_mode_will_start_) &&
      !overview_controller->IsCompletingShutdownAnimations();
  const bool app_list_visible =
      app_list_controller->IsVisible() ||
      app_list_controller->GetTargetVisibility() ||
      (!in_overview && app_list_controller->ShouldHomeLauncherBeVisible());

  // Only force to show if there is not a pending drag operation.
  if (shelf_widget_->is_hotseat_forced_to_show() && drag_status_ == kDragNone)
    return app_list_visible ? HotseatState::kShown : HotseatState::kExtended;

  bool in_split_view = false;
  if (in_overview) {
    auto* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    in_split_view =
        split_view_controller && split_view_controller->InSplitViewMode();
  }
  switch (drag_status_) {
    case kDragNone:
    case kDragHomeToOverviewInProgress: {
      switch (app_list_controller->home_launcher_transition_state()) {
        case AppListControllerImpl::HomeLauncherTransitionState::kMostlyShown:
          return HotseatState::kShown;
        case AppListControllerImpl::HomeLauncherTransitionState::kMostlyHidden:
          return in_overview ? HotseatState::kExtended : HotseatState::kHidden;
        case AppListControllerImpl::HomeLauncherTransitionState::kFinished:
          // Consider the AppList visible if it is beginning to show. Also
          // detect the case where the last window is being minimized.
          if (app_list_visible)
            return HotseatState::kShown;

          // Show the hotseat if the shelf view's context menu is showing.
          if (shelf_widget_->hotseat_widget()->IsShowingShelfMenu())
            return HotseatState::kExtended;

          if (in_split_view)
            return HotseatState::kHidden;
          if (in_overview)
            return HotseatState::kExtended;
          if (visibility_state == SHELF_AUTO_HIDE) {
            if (auto_hide_state == SHELF_AUTO_HIDE_HIDDEN ||
                should_hide_hotseat_) {
              return HotseatState::kHidden;
            }
            return HotseatState::kExtended;
          }
          if (shelf_widget_->hotseat_widget()->is_manually_extended() &&
              !should_hide_hotseat_) {
            return HotseatState::kExtended;
          }
          // If none of the conditions above were met means that the state
          // changed because of an action other than a user intervention.
          // We should hide the hotseat and reset the |is_manually extended|
          // flag to false.
          shelf_widget_->hotseat_widget()->set_manually_extended(false);
          return HotseatState::kHidden;
      }
    }
    case kDragCompleteInProgress:
      if (visibility_state == SHELF_AUTO_HIDE) {
        // When the shelf is autohidden and the drag is being completed, the
        // auto hide state has been finalized, so ensure the hotseat matches.
        DCHECK_EQ(drag_auto_hide_state_, auto_hide_state);
        return auto_hide_state == SHELF_AUTO_HIDE_SHOWN
                   ? HotseatState::kExtended
                   : HotseatState::kHidden;
      }
      FALLTHROUGH;
    case kDragCancelInProgress: {
      // If the drag being completed is not a Hotseat drag, don't change the
      // state.
      if (!hotseat_is_in_drag_)
        return hotseat_state();

      if (shelf_widget_->hotseat_widget()->IsExtended())
        return HotseatState::kExtended;
      // |drag_amount_| is relative to the top of the hotseat when the drag
      // begins with an extended hotseat. Correct for this to get
      // |total_amount_dragged|.
      const int drag_base = (hotseat_state() == HotseatState::kExtended &&
                             state_.visibility_state == SHELF_VISIBLE)
                                ? (ShelfConfig::Get()->hotseat_size() +
                                   ShelfConfig::Get()->hotseat_bottom_padding())
                                : 0;
      const float total_amount_dragged = drag_base + drag_amount_;
      const float end_of_drag_in_screen =
          drag_start_point_in_screen_.y() + total_amount_dragged;
      const int screen_bottom =
          display::Screen::GetScreen()
              ->GetDisplayNearestView(shelf_widget_->GetNativeView())
              .bounds()
              .bottom();
      const bool dragged_to_bezel =
          std::ceil(end_of_drag_in_screen) >= screen_bottom;

      const int top_of_hotseat_to_screen_bottom =
          screen_bottom -
          shelf_widget_->hotseat_widget()->GetWindowBoundsInScreen().y();
      const bool dragged_over_half_hotseat_size =
          top_of_hotseat_to_screen_bottom <
          ShelfConfig::Get()->hotseat_size() / 2;

      // Drags to the bezel may have large velocities, even if the drag is slow.
      // Decide the state based on position first, before checking
      // |last_drag_velocity_|.
      if (dragged_to_bezel || dragged_over_half_hotseat_size)
        return HotseatState::kHidden;
      if (std::abs(last_drag_velocity_) >= 120) {
        if (last_drag_velocity_ > 0)
          return HotseatState::kHidden;
        return HotseatState::kExtended;
      }
      return HotseatState::kExtended;
    }
    case kDragAppListInProgress:
      return app_list_controller->home_launcher_transition_state() ==
                     AppListControllerImpl::HomeLauncherTransitionState::
                         kMostlyHidden
                 ? HotseatState::kHidden
                 : HotseatState::kShown;
    default:
      // Do not change the hotseat state until the drag is complete or
      // canceled.
      return hotseat_state();
  }
  NOTREACHED();
  return HotseatState::kShown;
}

ShelfVisibilityState ShelfLayoutManager::CalculateShelfVisibility() {
  switch (shelf_->auto_hide_behavior()) {
    case SHELF_AUTO_HIDE_BEHAVIOR_ALWAYS:
      return SHELF_AUTO_HIDE;
    case SHELF_AUTO_HIDE_BEHAVIOR_NEVER:
      return SHELF_VISIBLE;
    case SHELF_AUTO_HIDE_ALWAYS_HIDDEN:
      return SHELF_HIDDEN;
  }
  return SHELF_VISIBLE;
}

void ShelfLayoutManager::LayoutShelfAndUpdateBounds() {
  CalculateTargetBoundsAndUpdateWorkArea(hotseat_state());
  UpdateBoundsAndOpacity(false, nullptr);

  // Update insets in ShelfWindowTargeter when shelf bounds change.
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(visibility_state());
}

void ShelfLayoutManager::LayoutShelf() {
  // The ShelfWidget may be partially closed (no native widget) during shutdown
  // so skip layout.
  if (in_shutdown_)
    return;

  LayoutShelfAndUpdateBounds();
}

void ShelfLayoutManager::SetDimmed(bool dimmed) {
  if (dimmed_for_inactivity_ == dimmed)
    return;

  dimmed_for_inactivity_ = dimmed;
  LayoutShelfAndUpdateBounds();
}

void ShelfLayoutManager::UpdateBoundsAndOpacity(
    bool animate,
    ui::ImplicitAnimationObserver* observer) {
  hide_animation_observer_.reset();
  if (GetLayer(shelf_widget_)->opacity() != target_bounds_.opacity) {
    if (target_bounds_.opacity == 0) {
      // On hide, set the opacity after the animation completes.
      hide_animation_observer_ =
          std::make_unique<HideAnimationObserver>(GetLayer(shelf_widget_));
    } else {
      // On show, set the opacity before the animation begins to ensure the blur
      // is shown while the shelf moves.
      GetLayer(shelf_widget_)->SetOpacity(target_bounds_.opacity);
    }
  }

  ShelfNavigationWidget* nav_widget = shelf_widget_->navigation_widget();
  HotseatWidget* hotseat_widget = shelf_widget_->hotseat_widget();
  StatusAreaWidget* status_widget = shelf_widget_->status_area_widget();
  base::AutoReset<bool> auto_reset_updating_bounds(&updating_bounds_, true);
  {
    ui::ScopedLayerAnimationSettings shelf_animation_setter(
        GetLayer(shelf_widget_)->GetAnimator());
    ui::ScopedLayerAnimationSettings nav_animation_setter(
        GetLayer(nav_widget)->GetAnimator());
    ui::ScopedLayerAnimationSettings hotseat_animation_setter(
        GetLayer(hotseat_widget)->GetAnimator());
    ui::ScopedLayerAnimationSettings status_animation_setter(
        GetLayer(status_widget)->GetAnimator());

    if (hide_animation_observer_)
      shelf_animation_setter.AddObserver(hide_animation_observer_.get());

    if (animate) {
      auto duration = base::TimeDelta::FromMilliseconds(kAnimationDurationMS);
      shelf_animation_setter.SetTransitionDuration(duration);
      shelf_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      shelf_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      nav_animation_setter.SetTransitionDuration(duration);
      nav_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      nav_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      hotseat_animation_setter.SetTransitionDuration(duration);
      hotseat_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      hotseat_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
      status_animation_setter.SetTransitionDuration(duration);
      status_animation_setter.SetTweenType(gfx::Tween::EASE_OUT);
      status_animation_setter.SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    } else {
      StopAnimating();
      shelf_animation_setter.SetTransitionDuration(base::TimeDelta());
      nav_animation_setter.SetTransitionDuration(base::TimeDelta());
      hotseat_animation_setter.SetTransitionDuration(base::TimeDelta());
      status_animation_setter.SetTransitionDuration(base::TimeDelta());
    }
    if (observer)
      status_animation_setter.AddObserver(observer);

    gfx::Rect shelf_bounds = target_bounds_.shelf_bounds;
    shelf_widget_->SetBounds(shelf_bounds);

    GetLayer(nav_widget)->SetOpacity(target_bounds_.opacity);
    GetLayer(hotseat_widget)->SetOpacity(target_bounds_.opacity);
    GetLayer(status_widget)->SetOpacity(target_bounds_.opacity);

    // Having a window which is visible but does not have an opacity is an
    // illegal state. We therefore hide the shelf here if required.
    if (!target_bounds_.opacity) {
      nav_widget->Hide();
      status_widget->Hide();
    }

    // Setting visibility during an animation causes the visibility property to
    // animate. Override the animation settings to immediately set the
    // visibility property. Opacity will still animate.

    gfx::Rect status_bounds = target_bounds_.status_bounds_in_shelf;
    status_bounds.Offset(target_bounds_.shelf_bounds.OffsetFromOrigin());
    status_widget->SetBounds(status_bounds);

    gfx::Vector2d nav_offset = target_bounds_.shelf_bounds.OffsetFromOrigin();
    gfx::Rect nav_bounds = target_bounds_.nav_bounds_in_shelf;
    nav_bounds.Offset(nav_offset);
    nav_widget->SetBounds(nav_bounds);

    gfx::Vector2d hotseat_offset =
        target_bounds_.shelf_bounds.OffsetFromOrigin();
    gfx::Rect hotseat_bounds = target_bounds_.hotseat_bounds_in_shelf;
    hotseat_bounds.Offset(hotseat_offset);
    hotseat_widget->SetBounds(hotseat_bounds);

    // Do not update the work area during overview animation.
    if (!suspend_work_area_update_) {
      // Do not update the work area when the alignment changes to BOTTOM_LOCKED
      // to prevent window movement when the screen is locked: crbug.com/622431
      // The work area is initialized with BOTTOM_LOCKED insets to prevent
      // window movement on async preference initialization in tests:
      // crbug.com/834369
      display_ = display::Screen::GetScreen()->GetDisplayNearestWindow(
          shelf_widget_->GetNativeWindow());
      bool in_overview =
          Shell::Get()->overview_controller()->InOverviewSession();
      if (!in_overview && !state_.IsScreenLocked() &&
          (shelf_->alignment() != SHELF_ALIGNMENT_BOTTOM_LOCKED ||
           display_.work_area() == display_.bounds())) {
        gfx::Insets insets;
        // If user session is blocked (login to new user session or add user to
        // the existing session - multi-profile) then give 100% of work area
        // only if keyboard is not shown.
        // TODO(agawronska): Could this be called from WorkAreaInsets?
        const WorkAreaInsets* const work_area =
            WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow());
        if (!state_.IsAddingSecondaryUser() || work_area->IsKeyboardShown())
          insets = work_area->user_work_area_insets();

        Shell::Get()->SetDisplayWorkAreaInsets(shelf_widget_->GetNativeWindow(),
                                               insets);
      }
    }
  }

  // Set an empty border to avoid the shelf view and status area overlapping.
  // TODO(msw): Avoid setting bounds of views within the shelf widget here.
  gfx::Rect shelf_bounds = gfx::Rect(target_bounds_.shelf_bounds.size());
  shelf_widget_->GetContentsView()->SetBorder(views::CreateEmptyBorder(
      shelf_bounds.InsetsFrom(target_bounds_.shelf_bounds_in_shelf)));
  shelf_widget_->GetContentsView()->Layout();

  // Never show the navigation widget or the hotseat outside of an active
  // session.
  if (!state_.IsActiveSessionState()) {
    nav_widget->Hide();
    hotseat_widget->Hide();
  }

  // Setting visibility during an animation causes the visibility property to
  // animate. Set the visibility property without an animation.
  if (target_bounds_.opacity) {
    if (state_.IsActiveSessionState()) {
      nav_widget->ShowInactive();
      hotseat_widget->ShowInactive();
    }
    status_widget->Show();
  }
}

bool ShelfLayoutManager::IsDraggingWindowFromTopOrCaptionArea() const {
  // Currently dragging maximized or fullscreen window from the top or the
  // caption area is only allowed in tablet mode.
  if (!IsTabletModeEnabled())
    return false;

  // TODO(minch): Check active window directly if removed search field
  // in overview mode. http://crbug.com/866679
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kActiveDesk);
  for (auto* window : windows) {
    WindowState* window_state = WindowState::Get(window);
    if (window_state && window_state->is_dragged() &&
        (window_state->IsMaximized() || window_state->IsFullscreen()) &&
        (window_state->drag_details()->window_component == HTCLIENT ||
         window_state->drag_details()->window_component == HTCAPTION)) {
      return true;
    }
  }
  return false;
}

void ShelfLayoutManager::StopAnimating() {
  GetLayer(shelf_widget_)->GetAnimator()->StopAnimating();
  GetLayer(shelf_widget_->status_area_widget())->GetAnimator()->StopAnimating();
}

void ShelfLayoutManager::CalculateTargetBounds(
    const State& state,
    HotseatState hotseat_target_state) {
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const int home_button_edge_spacing =
      ShelfConfig::Get()->home_button_edge_spacing();
  // By default, show the whole shelf on the screen.
  int shelf_in_screen_portion = shelf_size;
  const WorkAreaInsets* const work_area =
      WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow());

  if (state.IsShelfAutoHidden()) {
    shelf_in_screen_portion =
        Shell::Get()->app_list_controller()->home_launcher_transition_state() ==
                AppListControllerImpl::HomeLauncherTransitionState::kMostlyShown
            ? shelf_size
            : ShelfConfig::Get()->hidden_shelf_in_screen_portion();
  } else if (state.visibility_state == SHELF_HIDDEN ||
             work_area->IsKeyboardShown()) {
    shelf_in_screen_portion = 0;
  }

  gfx::Rect available_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget_->GetNativeWindow());
  available_bounds.Inset(work_area->GetAccessibilityInsets());

  int shelf_width = PrimaryAxisValue(available_bounds.width(), shelf_size);
  int shelf_height = PrimaryAxisValue(shelf_size, available_bounds.height());
  const int shelf_primary_position = SelectValueForShelfAlignment(
      available_bounds.bottom() - shelf_in_screen_portion,
      available_bounds.x() - shelf_size + shelf_in_screen_portion,
      available_bounds.right() - shelf_in_screen_portion);
  gfx::Point shelf_origin = SelectValueForShelfAlignment(
      gfx::Point(available_bounds.x(), shelf_primary_position),
      gfx::Point(shelf_primary_position, available_bounds.y()),
      gfx::Point(shelf_primary_position, available_bounds.y()));

  target_bounds_.shelf_bounds = screen_util::SnapBoundsToDisplayEdge(
      gfx::Rect(shelf_origin.x(), shelf_origin.y(), shelf_width, shelf_height),
      shelf_widget_->GetNativeWindow());

  gfx::Size status_size(
      shelf_widget_->status_area_widget()->GetWindowBoundsInScreen().size());
  if (shelf_->IsHorizontalAlignment())
    status_size.set_height(shelf_size);
  else
    status_size.set_width(shelf_size);

  gfx::Point status_origin = SelectValueForShelfAlignment(
      gfx::Point(0, 0),
      gfx::Point(shelf_width - status_size.width(),
                 shelf_height - status_size.height()),
      gfx::Point(0, shelf_height - status_size.height()));
  if (shelf_->IsHorizontalAlignment() && !base::i18n::IsRTL())
    status_origin.set_x(shelf_width - status_size.width());
  target_bounds_.status_bounds_in_shelf = gfx::Rect(status_origin, status_size);

  gfx::Point nav_origin =
      gfx::Point(home_button_edge_spacing, home_button_edge_spacing);
  const gfx::Size nav_size = shelf_widget_->navigation_widget()->GetIdealSize();
  if (shelf_->IsHorizontalAlignment() && base::i18n::IsRTL())
    nav_origin.set_x(shelf_width - nav_size.width() - nav_origin.x());
  target_bounds_.nav_bounds_in_shelf = gfx::Rect(nav_origin, nav_size);

  gfx::Point hotseat_origin;
  int hotseat_width;
  int hotseat_height;
  if (shelf_->IsHorizontalAlignment()) {
    hotseat_width =
        shelf_width - target_bounds_.nav_bounds_in_shelf.size().width() -
        home_button_edge_spacing - ShelfConfig::Get()->app_icon_group_margin() -
        status_size.width();
    int hotseat_x = base::i18n::IsRTL()
                        ? target_bounds_.nav_bounds_in_shelf.x() -
                              home_button_edge_spacing - hotseat_width
                        : target_bounds_.nav_bounds_in_shelf.right() +
                              home_button_edge_spacing;
    if (hotseat_target_state != HotseatState::kShown) {
      // Give the hotseat more space if it is shown outside of the shelf.
      hotseat_width = available_bounds.width();
      hotseat_x = 0;
    }
    hotseat_origin =
        gfx::Point(hotseat_x, CalculateHotseatYInShelf(hotseat_target_state));
    hotseat_height = ShelfConfig::Get()->hotseat_size();
  } else {
    hotseat_origin = gfx::Point(0, target_bounds_.nav_bounds_in_shelf.bottom() +
                                       home_button_edge_spacing);
    hotseat_width = shelf_width;
    hotseat_height =
        shelf_height - target_bounds_.nav_bounds_in_shelf.size().height() -
        home_button_edge_spacing - ShelfConfig::Get()->app_icon_group_margin() -
        status_size.height();
  }
  target_bounds_.hotseat_bounds_in_shelf =
      gfx::Rect(hotseat_origin, gfx::Size(hotseat_width, hotseat_height));

  target_bounds_.opacity = ComputeTargetOpacity(state);

  if (drag_status_ == kDragInProgress)
    UpdateTargetBoundsForGesture(hotseat_target_state);

  target_bounds_.shelf_insets = SelectValueForShelfAlignment(
      gfx::Insets(0, 0,
                  GetShelfInset(state.visibility_state,
                                IsHotseatEnabled()
                                    ? ShelfConfig::Get()->in_app_shelf_size()
                                    : shelf_height),
                  0),
      gfx::Insets(0, GetShelfInset(state.visibility_state, shelf_width), 0, 0),
      gfx::Insets(0, 0, 0, GetShelfInset(state.visibility_state, shelf_width)));

  // This needs to happen after calling UpdateTargetBoundsForGesture(), because
  // that can change the size of the shelf.
  const bool showing_login_shelf = !state.IsActiveSessionState();
  if (chromeos::switches::ShouldShowScrollableShelf() && !showing_login_shelf) {
    target_bounds_.shelf_bounds_in_shelf = SelectValueForShelfAlignment(
        gfx::Rect(target_bounds_.nav_bounds_in_shelf.right(), 0,
                  shelf_width - status_size.width() -
                      target_bounds_.nav_bounds_in_shelf.width() -
                      home_button_edge_spacing,
                  target_bounds_.shelf_bounds.height()),
        gfx::Rect(0, target_bounds_.nav_bounds_in_shelf.height(),
                  target_bounds_.shelf_bounds.width(),
                  shelf_height - status_size.height() -
                      target_bounds_.nav_bounds_in_shelf.height() -
                      home_button_edge_spacing),
        gfx::Rect(0, target_bounds_.nav_bounds_in_shelf.height(),
                  target_bounds_.shelf_bounds.width(),
                  shelf_height - status_size.height() -
                      target_bounds_.nav_bounds_in_shelf.height() -
                      home_button_edge_spacing));
  } else {
    target_bounds_.shelf_bounds_in_shelf = SelectValueForShelfAlignment(
        gfx::Rect(0, 0, shelf_width - status_size.width(),
                  target_bounds_.shelf_bounds.height()),
        gfx::Rect(0, 0, target_bounds_.shelf_bounds.width(),
                  shelf_height - status_size.height()),
        gfx::Rect(0, 0, target_bounds_.shelf_bounds.width(),
                  shelf_height - status_size.height()));
  }
}

void ShelfLayoutManager::CalculateTargetBoundsAndUpdateWorkArea(
    HotseatState hotseat_target_state) {
  CalculateTargetBounds(state_, hotseat_target_state);
  gfx::Rect shelf_bounds_for_workarea_calculation = target_bounds_.shelf_bounds;
  // When the hotseat is enabled, only use the in-app shelf bounds when
  // calculating the work area. This prevents windows resizing unnecesarily.
  if (IsHotseatEnabled()) {
    shelf_bounds_for_workarea_calculation =
        GetIdealBoundsForWorkAreaCalculation();
  }
  if (!suspend_work_area_update_) {
    WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow())
        ->SetShelfBoundsAndInsets(shelf_bounds_for_workarea_calculation,
                                  target_bounds_.shelf_insets);
    for (auto& observer : observers_)
      observer.OnWorkAreaInsetsChanged();
  }
}

void ShelfLayoutManager::UpdateTargetBoundsForGesture(
    HotseatState hotseat_target_state) {
  // TODO(https://crbug.com/1002132): Add tests for the hotseat bounds logic.
  CHECK_EQ(kDragInProgress, drag_status_);
  const bool horizontal = shelf_->IsHorizontalAlignment();
  const int shelf_size = ShelfConfig::Get()->shelf_size();

  int resistance_free_region = 0;
  bool shelf_hidden_at_start = false;
  if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_HIDDEN &&
      visibility_state() == SHELF_AUTO_HIDE &&
      auto_hide_state() != SHELF_AUTO_HIDE_SHOWN) {
    // If the shelf was hidden when the drag started (and the state hasn't
    // changed since then, e.g. because the tray-menu was shown because of the
    // drag), then allow the drag some resistance-free region at first to make
    // sure the shelf sticks with the finger until the shelf is visible.
    resistance_free_region =
        shelf_size - ShelfConfig::Get()->hidden_shelf_in_screen_portion();
    shelf_hidden_at_start = true;
  }

  float translate = 0.f;
  if (IsHotseatEnabled()) {
    // The drag up gesture should not taper off when the hotseat is enabled
    // because there should be a linear transition to the home launcher gesture.
    translate = drag_amount_;
  } else {
    const bool resist = SelectValueForShelfAlignment(
        drag_amount_<-resistance_free_region, drag_amount_>
            resistance_free_region,
        drag_amount_ < -resistance_free_region);
    if (resist) {
      float diff = fabsf(drag_amount_) - resistance_free_region;
      diff = std::min(diff, sqrtf(diff));
      if (drag_amount_ < 0)
        translate = -resistance_free_region - diff;
      else
        translate = resistance_free_region + diff;
    } else {
      translate = drag_amount_;
    }
  }

  const gfx::Rect available_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget_->GetNativeWindow());
  const int baseline = SelectValueForShelfAlignment(
      available_bounds.bottom() - (shelf_hidden_at_start ? 0 : shelf_size),
      available_bounds.x() - (shelf_hidden_at_start ? shelf_size : 0),
      available_bounds.right() - (shelf_hidden_at_start ? 0 : shelf_size));

  if (horizontal) {
    if (!IsHotseatEnabled()) {
      target_bounds_.shelf_bounds.set_y(baseline + translate);
      target_bounds_.nav_bounds_in_shelf.set_y(
          ShelfConfig::Get()->button_spacing());
      target_bounds_.hotseat_bounds_in_shelf.set_y(0);
      target_bounds_.status_bounds_in_shelf.set_y(0);
      return;
    }

    const bool move_shelf_with_hotseat = visibility_state() == SHELF_AUTO_HIDE;
    if (move_shelf_with_hotseat) {
      // Do not allow the shelf to be dragged more than |shelf_size| from the
      // bottom of the display.
      int shelf_y = std::max(available_bounds.bottom() - shelf_size,
                             static_cast<int>(baseline + translate));
      // Window drags only happen after the hotseat has been dragged up to its
      // full height. After the drag moves a window, do not allow the drag to
      // move the hotseat down.
      if (IsWindowDragInProgress())
        shelf_y = available_bounds.bottom() - shelf_size;
      target_bounds_.shelf_bounds.set_y(shelf_y);
    }

    int hotseat_y = 0;
    const int hotseat_extended_y =
        Shell::Get()->shelf_config()->hotseat_size() +
        Shell::Get()->shelf_config()->hotseat_bottom_padding();
    const int hotseat_baseline =
        (hotseat_target_state == HotseatState::kExtended) ? -hotseat_extended_y
                                                          : shelf_size;
    bool use_hotseat_baseline =
        (hotseat_target_state == HotseatState::kExtended &&
         visibility_state() == SHELF_AUTO_HIDE) ||
        (hotseat_target_state == HotseatState::kHidden &&
         visibility_state() != SHELF_AUTO_HIDE);
    hotseat_y = std::max(
        -hotseat_extended_y,
        static_cast<int>((use_hotseat_baseline ? hotseat_baseline : 0) +
                         translate));
    // Window drags only happen after the hotseat has been dragged up to its
    // full height. After the drag moves a window, do not allow the drag to move
    // the hotseat down.
    if (IsWindowDragInProgress())
      hotseat_y = -hotseat_extended_y;
    target_bounds_.hotseat_bounds_in_shelf.set_y(hotseat_y);
    return;
  }

  target_bounds_.shelf_bounds.set_x(baseline + translate);
  target_bounds_.nav_bounds_in_shelf.set_x(
      ShelfConfig::Get()->button_spacing());
  target_bounds_.hotseat_bounds_in_shelf.set_x(0);
  target_bounds_.status_bounds_in_shelf.set_x(0);
}

void ShelfLayoutManager::UpdateAutoHideStateNow() {
  SetState(state_.visibility_state);

  // If the state did not change, the auto hide timer may still be running.
  StopAutoHideTimer();
}

void ShelfLayoutManager::StartAutoHideTimer() {
  auto_hide_timer_.Start(FROM_HERE,
                         base::TimeDelta::FromMilliseconds(kAutoHideDelayMS),
                         this, &ShelfLayoutManager::UpdateAutoHideStateNow);
}

void ShelfLayoutManager::StopAutoHideTimer() {
  auto_hide_timer_.Stop();
  mouse_over_shelf_when_auto_hide_timer_started_ = false;
}

gfx::Rect ShelfLayoutManager::GetVisibleShelfBounds() const {
  gfx::Rect shelf_region = shelf_widget_->GetWindowBoundsInScreen();
  DCHECK(!display_.bounds().IsEmpty());
  shelf_region.Intersect(display_.bounds());
  return screen_util::SnapBoundsToDisplayEdge(shelf_region,
                                              shelf_widget_->GetNativeWindow());
}

gfx::Rect ShelfLayoutManager::GetAutoHideShowShelfRegionInScreen() const {
  gfx::Rect shelf_bounds_in_screen = GetVisibleShelfBounds();
  gfx::Vector2d offset = SelectValueForShelfAlignment(
      gfx::Vector2d(0, shelf_bounds_in_screen.height()),
      gfx::Vector2d(-kMaxAutoHideShowShelfRegionSize, 0),
      gfx::Vector2d(shelf_bounds_in_screen.width(), 0));

  gfx::Rect show_shelf_region_in_screen = shelf_bounds_in_screen;
  show_shelf_region_in_screen += offset;
  if (shelf_->IsHorizontalAlignment())
    show_shelf_region_in_screen.set_height(kMaxAutoHideShowShelfRegionSize);
  else
    show_shelf_region_in_screen.set_width(kMaxAutoHideShowShelfRegionSize);

  // TODO(pkotwicz): Figure out if we need any special handling when the
  // keyboard is visible.
  return screen_util::SnapBoundsToDisplayEdge(show_shelf_region_in_screen,
                                              shelf_widget_->GetNativeWindow());
}

ShelfAutoHideState ShelfLayoutManager::CalculateAutoHideState(
    ShelfVisibilityState visibility_state) const {
  if (is_auto_hide_state_locked_)
    return state_.auto_hide_state;

  // Shelf is not available before login.
  // TODO(crbug.com/701157): Remove this when the login webui fake-shelf is
  // replaced with views.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return SHELF_AUTO_HIDE_HIDDEN;

  if (visibility_state != SHELF_AUTO_HIDE)
    return SHELF_AUTO_HIDE_HIDDEN;

  // Don't update the auto hide state if it is locked.
  if (shelf_->auto_hide_lock())
    return state_.auto_hide_state;

  // Don't let the shelf auto-hide when in tablet mode and Chromevox is on.
  if (IsTabletModeEnabled() &&
      Shell::Get()->accessibility_controller()->spoken_feedback_enabled()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (shelf_widget_->IsShowingAppList() && !IsTabletModeEnabled())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->ShouldShowShelf()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (shelf_widget_->IsShowingMenu())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->hotseat_widget()->IsShowingOverflowBubble())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsActive() ||
      shelf_widget_->navigation_widget()->IsActive() ||
      shelf_widget_->hotseat_widget()->IsActive() ||
      (shelf_widget_->status_area_widget() &&
       shelf_widget_->status_area_widget()->IsActive())) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  // If there are no visible windows do not hide the shelf.
  if (!HasVisibleWindow())
    return SHELF_AUTO_HIDE_SHOWN;

  if (IsDraggingWindowFromTopOrCaptionArea())
    return SHELF_AUTO_HIDE_SHOWN;

  // Do not hide the shelf if overview mode is active.
  if (Shell::Get()->overview_controller() &&
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (drag_status_ == kDragAppListInProgress ||
      drag_status_ == kDragHomeToOverviewInProgress) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (drag_status_ == kDragCompleteInProgress ||
      drag_status_ == kDragCancelInProgress) {
    return drag_auto_hide_state_;
  }

  // Don't show if the user is dragging the mouse.
  if (in_mouse_drag_)
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Rect shelf_region = GetVisibleShelfBounds();
  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->IsMessageBubbleShown() &&
      IsVisible()) {
    // Increase the the hit test area to prevent the shelf from disappearing
    // when the mouse is over the bubble gap.
    ShelfAlignment alignment = shelf_->alignment();
    shelf_region.Inset(
        alignment == SHELF_ALIGNMENT_RIGHT ? -kNotificationBubbleGapHeight : 0,
        shelf_->IsHorizontalAlignment() ? -kNotificationBubbleGapHeight : 0,
        alignment == SHELF_ALIGNMENT_LEFT ? -kNotificationBubbleGapHeight : 0,
        0);
  }

  // Do not perform any checks based on the cursor position if the mouse cursor
  // is currently hidden.
  if (!shelf_widget_->IsMouseEventsEnabled())
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Point cursor_position_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  // Cursor is invisible in tablet mode and plug in an external mouse in tablet
  // mode will switch to clamshell mode.
  if (shelf_region.Contains(cursor_position_in_screen) &&
      !IsTabletModeEnabled()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  // When the shelf is auto hidden and the shelf is on the boundary between two
  // displays, it is hard to trigger showing the shelf. For instance, if a
  // user's primary display is left of their secondary display, it is hard to
  // unautohide a left aligned shelf on the secondary display.
  // It is hard because:
  // - It is hard to stop the cursor in the shelf "light bar" and not overshoot.
  // - The cursor is warped to the other display if the cursor gets to the edge
  //   of the display.
  // Show the shelf if the cursor started on the shelf and the user overshot the
  // shelf slightly to make it easier to show the shelf in this situation. We
  // do not check |auto_hide_timer_|.IsRunning() because it returns false when
  // the timer's task is running.
  if ((state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN ||
       mouse_over_shelf_when_auto_hide_timer_started_) &&
      GetAutoHideShowShelfRegionInScreen().Contains(
          cursor_position_in_screen)) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  return SHELF_AUTO_HIDE_HIDDEN;
}

bool ShelfLayoutManager::IsShelfWindow(aura::Window* window) {
  if (!window)
    return false;
  const aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  const aura::Window* navigation_window =
      shelf_widget_->navigation_widget()->GetNativeWindow();
  const aura::Window* hotseat_window =
      shelf_widget_->hotseat_widget()->GetNativeWindow();
  const aura::Window* status_area_window =
      shelf_widget_->status_area_widget()->GetNativeWindow();
  return (shelf_window && shelf_window->Contains(window)) ||
         (navigation_window && navigation_window->Contains(window)) ||
         (hotseat_window && hotseat_window->Contains(window)) ||
         (status_area_window && status_area_window->Contains(window));
}

bool ShelfLayoutManager::IsStatusAreaWindow(aura::Window* window) {
  if (!window)
    return false;
  const aura::Window* status_window =
      shelf_widget_->status_area_widget()->GetNativeWindow();
  return status_window && status_window->Contains(window);
}

void ShelfLayoutManager::UpdateShelfVisibilityAfterLoginUIChange() {
  UpdateVisibilityState();
  LayoutShelf();
}

float ShelfLayoutManager::ComputeTargetOpacity(const State& state) const {
  // The shelf should not become transparent during the animation to or from
  // HomeLauncher.
  if (chromeos::switches::ShouldShowShelfHotseat() && IsTabletModeEnabled() &&
      Shell::Get()->app_list_controller()->home_launcher_transition_state() !=
          AppListControllerImpl::HomeLauncherTransitionState::kFinished) {
    return 1.0f;
  }

  float opacity_when_visible = kDefaultShelfOpacity;
  if (dimmed_for_inactivity_) {
    opacity_when_visible =
        (GetShelfBackgroundType() == SHELF_BACKGROUND_MAXIMIZED)
            ? kMaximizedShelfDimOpacity
            : kFloatingShelfDimOpacity;
  }

  if (drag_status_ == kDragInProgress ||
      state.visibility_state == SHELF_VISIBLE) {
    return opacity_when_visible;
  }

  // In Chrome OS Material Design, when shelf is hidden during auto hide state,
  // target bounds are also hidden. So the window can extend to the edge of
  // screen.
  return (state.visibility_state == SHELF_AUTO_HIDE &&
          state.auto_hide_state == SHELF_AUTO_HIDE_SHOWN)
             ? opacity_when_visible
             : 0.0f;
}

bool ShelfLayoutManager::IsShelfHiddenForFullscreen() const {
  // If the non-fullscreen app list should be shown, the shelf should not be
  // hidden.
  if (!IsTabletModeEnabled() &&
      Shell::Get()->app_list_controller()->GetTargetVisibility()) {
    return false;
  }

  const aura::Window* fullscreen_window = GetWindowForFullscreenModeInRoot(
      shelf_widget_->GetNativeWindow()->GetRootWindow());
  return fullscreen_window &&
         WindowState::Get(fullscreen_window)->GetHideShelfWhenFullscreen();
}

bool ShelfLayoutManager::IsShelfAutoHideForFullscreenMaximized() const {
  WindowState* active_window = WindowState::ForActiveWindow();
  return active_window &&
         active_window->autohide_shelf_when_maximized_or_fullscreen();
}

bool ShelfLayoutManager::ShouldHomeGestureHandleEvent(float scroll_y) const {
  // If the shelf is not visible, home gesture shouldn't trigger.
  if (!IsVisible())
    return false;

  const bool up_on_shown_hotseat =
      hotseat_state() == HotseatState::kShown && scroll_y < 0;
  if (IsHotseatEnabled() && up_on_shown_hotseat) {
    return GetHomeLauncherGestureHandlerModeForDrag() ==
           HomeLauncherGestureHandler::Mode::kSwipeHomeToOverview;
  }

  if (IsHotseatEnabled()) {
    if (features::IsDragFromShelfToHomeOrOverviewEnabled() &&
        hotseat_state() != HotseatState::kShown) {
      // If hotseat is hidden or extended (in-app or in-overview), do not let
      // HomeLauncherGestureHandler to handle the events.
      return false;
    }

    const bool up_on_extended_hotseat =
        hotseat_state() == HotseatState::kExtended && scroll_y < 0;
    if (!up_on_extended_hotseat)
      return false;
  }

  // Scroll down events should never be handled, unless they are currently being
  // handled
  if (scroll_y >= 0 && drag_status_ != kDragAppListInProgress &&
      drag_status_ != kDragHomeToOverviewInProgress) {
    return false;
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
// Gesture drag related functions:
bool ShelfLayoutManager::StartGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  if (drag_status_ != kDragNone)
    return false;

  float scroll_y_hint = gesture_in_screen.details().scroll_y_hint();
  if (StartAppListDrag(gesture_in_screen, scroll_y_hint))
    return true;

  if (ShouldHomeGestureHandleEvent(scroll_y_hint)) {
    DragStatus previous_drag_status = drag_status_;
    HomeLauncherGestureHandler* home_launcher_handler =
        Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
    const HomeLauncherGestureHandler::Mode target_mode =
        GetHomeLauncherGestureHandlerModeForDrag();
    drag_status_ =
        target_mode == HomeLauncherGestureHandler::Mode::kSwipeHomeToOverview
            ? kDragHomeToOverviewInProgress
            : kDragAppListInProgress;
    if (home_launcher_handler->OnPressEvent(target_mode,
                                            gesture_in_screen.location_f())) {
      return true;
    }
    drag_status_ = previous_drag_status;
  }

  if (Shell::Get()->app_list_controller()->IsVisible())
    return true;

  return StartShelfDrag(gesture_in_screen);
}

void ShelfLayoutManager::UpdateGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  float scroll_x = gesture_in_screen.details().scroll_x();
  float scroll_y = gesture_in_screen.details().scroll_y();

  HomeLauncherGestureHandler* home_launcher_handler =
      Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
  if (home_launcher_handler->IsDragInProgress() &&
      home_launcher_handler->OnScrollEvent(
          gesture_in_screen.details().bounding_box_f().CenterPoint(), scroll_x,
          scroll_y)) {
    return;
  }

  UpdateDrag(gesture_in_screen, scroll_x, scroll_y);
}

////////////////////////////////////////////////////////////////////////////////
// Mouse drag related functions:

void ShelfLayoutManager::AttemptToDragByMouse(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ != kDragNone)
    return;

  drag_status_ = kDragAttempt;
  last_mouse_drag_position_ = mouse_in_screen.location();
}

void ShelfLayoutManager::StartMouseDrag(const ui::MouseEvent& mouse_in_screen) {
  float scroll_y_hint = mouse_in_screen.y() - last_mouse_drag_position_.y();
  if (!StartAppListDrag(mouse_in_screen, scroll_y_hint))
    StartShelfDrag(mouse_in_screen);
}

void ShelfLayoutManager::UpdateMouseDrag(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ == kDragNone)
    return;

  DCHECK(drag_status_ == kDragAttempt || drag_status_ == kDragInProgress ||
         drag_status_ == kDragAppListInProgress ||
         drag_status_ == kDragHomeToOverviewInProgress);

  if (drag_status_ == kDragAttempt) {
    // Do not start drag for the small offset.
    if (abs(mouse_in_screen.location().y() - last_mouse_drag_position_.y()) <
        kMouseDragThreshold) {
      return;
    }

    // Mouse events do not provide the drag offset like gesture events. So
    // start mouse drag when mouse is moved.
    StartMouseDrag(mouse_in_screen);
  } else {
    int scroll_x =
        mouse_in_screen.location().x() - last_mouse_drag_position_.x();
    int scroll_y =
        mouse_in_screen.location().y() - last_mouse_drag_position_.y();
    UpdateDrag(mouse_in_screen, scroll_x, scroll_y);
    last_mouse_drag_position_ = mouse_in_screen.location();
  }
}

void ShelfLayoutManager::ReleaseMouseDrag(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ == kDragNone)
    return;

  DCHECK(drag_status_ == kDragAttempt ||
         drag_status_ == kDragAppListInProgress ||
         drag_status_ == kDragHomeToOverviewInProgress ||
         drag_status_ == kDragInProgress);

  switch (drag_status_) {
    case kDragAttempt:
      drag_status_ = kDragNone;
      break;
    case kDragAppListInProgress:
    case kDragHomeToOverviewInProgress:
      CompleteAppListDrag(mouse_in_screen);
      break;
    case kDragInProgress:
      CompleteDrag(mouse_in_screen);
      break;
    default:
      NOTREACHED();
  }
  last_mouse_drag_position_ = gfx::Point();
}

////////////////////////////////////////////////////////////////////////////////
// Drag related functions:

bool ShelfLayoutManager::IsDragAllowed() const {
  // The gestures are disabled in the lock/login screen.
  SessionControllerImpl* controller = Shell::Get()->session_controller();
  if (!controller->NumberOfLoggedInUsers() || controller->IsScreenLocked())
    return false;

  if (IsShelfHiddenForFullscreen())
    return false;

  return true;
}

bool ShelfLayoutManager::StartAppListDrag(
    const ui::LocatedEvent& event_in_screen,
    float scroll_y_hint) {
  // If the home screen is available, gesture dragging is handled by
  // HomeLauncherGestureHandler.
  if (IsTabletModeEnabled() && event_in_screen.IsGestureEvent())
    return false;

  // Fullscreen app list can only be dragged from bottom alignment shelf.
  if (!shelf_->IsHorizontalAlignment())
    return false;

  // If the shelf is not visible, swiping up should show the shelf.
  if (!IsVisible())
    return false;

  // Do not show the fullscreen app list until the overflow bubble has been
  // closed.
  if (shelf_widget_->hotseat_widget()->IsShowingOverflowBubble())
    return false;

  // If the app list is already open, swiping up on the shelf should keep it
  // open.
  if (Shell::Get()->app_list_controller()->IsVisible())
    return false;

  // Swipes down on shelf should hide the shelf.
  if (scroll_y_hint >= 0)
    return false;

  const gfx::Rect shelf_bounds = GetIdealBounds();
  shelf_background_type_before_drag_ = shelf_background_type_;
  drag_status_ = kDragAppListInProgress;

  Shell::Get()->app_list_controller()->Show(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(shelf_widget_->GetNativeWindow())
          .id(),
      kSwipeFromShelf, event_in_screen.time_stamp());
  Shell::Get()->app_list_controller()->UpdateYPositionAndOpacity(
      shelf_bounds.y(), GetAppListBackgroundOpacityOnShelfOpacity());
  launcher_above_shelf_bottom_amount_ =
      shelf_bounds.bottom() - event_in_screen.location().y();

  return true;
}

bool ShelfLayoutManager::StartShelfDrag(
    const ui::LocatedEvent& event_in_screen) {
  // Disable the shelf dragging if the fullscreen app list is opened.
  if (Shell::Get()->app_list_controller()->IsVisible() &&
      !IsTabletModeEnabled())
    return false;

  // Also disable shelf drags until the overflow shelf is closed.
  if (shelf_widget_->hotseat_widget()->IsShowingOverflowBubble())
    return false;

  drag_status_ = kDragInProgress;
  drag_auto_hide_state_ = visibility_state() == SHELF_AUTO_HIDE
                              ? auto_hide_state()
                              : SHELF_AUTO_HIDE_SHOWN;
  MaybeSetupHotseatDrag(event_in_screen);
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);

  // For the hotseat, |drag_amount_| is relative to the top of the shelf.
  // To keep the hotseat from jumping to the top of the shelf on drag, set the
  // offset to the hotseats extended position.
  if (hotseat_state() == HotseatState::kExtended &&
      visibility_state() == SHELF_VISIBLE) {
    drag_amount_ = -(ShelfConfig::Get()->hotseat_size() +
                     ShelfConfig::Get()->hotseat_bottom_padding());
  } else {
    drag_amount_ = 0.f;
  }

  MaybeStartDragWindowFromShelf(event_in_screen, /*scroll_y=*/base::nullopt);

  return true;
}

void ShelfLayoutManager::MaybeSetupHotseatDrag(
    const ui::LocatedEvent& event_in_screen) {
  if (!IsHotseatEnabled())
    return;
  // Do not allow Hotseat dragging when the hotseat is shown within the shelf.
  if (hotseat_state() == HotseatState::kShown)
    return;

  hotseat_is_in_drag_ = true;
}

void ShelfLayoutManager::UpdateDrag(const ui::LocatedEvent& event_in_screen,
                                    float scroll_x,
                                    float scroll_y) {
  if (drag_status_ == kDragAppListInProgress) {
    // Dismiss the app list if the shelf changed to vertical alignment during
    // dragging.
    if (!shelf_->IsHorizontalAlignment()) {
      Shell::Get()->app_list_controller()->DismissAppList();
      launcher_above_shelf_bottom_amount_ = 0.f;
      drag_status_ = kDragNone;
      return;
    }
    const gfx::Rect shelf_bounds = GetIdealBounds();
    Shell::Get()->app_list_controller()->UpdateYPositionAndOpacity(
        std::min(event_in_screen.location().y(), shelf_bounds.y()),
        GetAppListBackgroundOpacityOnShelfOpacity());
    launcher_above_shelf_bottom_amount_ =
        shelf_bounds.bottom() - event_in_screen.location().y();
  } else {
    if (drag_start_point_in_screen_ == gfx::Point())
      drag_start_point_in_screen_ = event_in_screen.location();
    drag_amount_ += PrimaryAxisValue(scroll_y, scroll_x);
    if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
      last_drag_velocity_ =
          event_in_screen.AsGestureEvent()->details().velocity_y();
    }
    LayoutShelf();
    MaybeUpdateWindowDrag(event_in_screen, scroll_x, scroll_y);
  }
}

void ShelfLayoutManager::CompleteDrag(const ui::LocatedEvent& event_in_screen) {
  // End the possible window drag before checking the shelf visibility.
  base::Optional<ShelfWindowDragResult> window_drag_result =
      MaybeEndWindowDrag(event_in_screen);
  HotseatState old_hotseat_state = hotseat_state();

  if (ShouldChangeVisibilityAfterDrag(event_in_screen))
    CompleteDragWithChangedVisibility();
  else
    CancelDrag();

  // Hotseat gestures are meaningful only in tablet mode with hotseat enabled.
  if (chromeos::switches::ShouldShowShelfHotseat() && IsTabletModeEnabled()) {
    base::Optional<InAppShelfGestures> gesture_to_record =
        CalculateHotseatGestureToRecord(window_drag_result, old_hotseat_state,
                                        hotseat_state());
    if (gesture_to_record.has_value()) {
      UMA_HISTOGRAM_ENUMERATION(kHotseatGestureHistogramName,
                                gesture_to_record.value());
    }
  }
}

void ShelfLayoutManager::CompleteAppListDrag(
    const ui::LocatedEvent& event_in_screen) {
  // Change the shelf alignment to vertical during drag will reset
  // |drag_status_| to |kDragNone|.
  if (drag_status_ == kDragNone)
    return;

  HomeLauncherGestureHandler* home_launcher_handler =
      Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
  DCHECK(home_launcher_handler);
  base::Optional<float> velocity_y;
  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    velocity_y = base::make_optional(
        event_in_screen.AsGestureEvent()->details().velocity_y());
  }
  if (home_launcher_handler->OnReleaseEvent(event_in_screen.location_f(),
                                            velocity_y)) {
    drag_status_ = kDragNone;
    return;
  }

  using ash::AppListViewState;
  AppListViewState app_list_state =
      Shell::Get()->app_list_controller()->CalculateStateAfterShelfDrag(
          event_in_screen, launcher_above_shelf_bottom_amount_);

  // Keep auto-hide shelf visible if failed to open the app list.
  base::Optional<Shelf::ScopedAutoHideLock> auto_hide_lock;
  if (app_list_state == AppListViewState::kClosed)
    auto_hide_lock.emplace(shelf_);
  Shell::Get()->app_list_controller()->EndDragFromShelf(app_list_state);
  drag_status_ = kDragNone;
}

void ShelfLayoutManager::CancelDrag() {
  if (drag_status_ == kDragAppListInProgress ||
      drag_status_ == kDragHomeToOverviewInProgress) {
    HomeLauncherGestureHandler* home_launcher_handler =
        Shell::Get()->home_screen_controller()->home_launcher_gesture_handler();
    DCHECK(home_launcher_handler);
    if (home_launcher_handler->IsDragInProgress())
      home_launcher_handler->Cancel();
    else
      Shell::Get()->app_list_controller()->DismissAppList();
  } else {
    MaybeCancelWindowDrag();
    // Set |drag_status_| to kDragCancelInProgress to set the
    // auto hide state to |drag_auto_hide_state_|, which is the
    // visibility state before starting drag.
    drag_status_ = kDragCancelInProgress;
    UpdateVisibilityState();
  }
  if (hotseat_is_in_drag_) {
    // If the gesture started the overview session, the hotseat will be
    // extended, but should not be marked as manually extended, as
    // extending the hotseat was not the primary goal of the gesture.
    shelf_widget_->hotseat_widget()->set_manually_extended(
        hotseat_state() == HotseatState::kExtended &&
        !Shell::Get()->overview_controller()->InOverviewSession());
  }
  hotseat_is_in_drag_ = false;
  drag_status_ = kDragNone;
  drag_start_point_in_screen_ = gfx::Point();
}

void ShelfLayoutManager::CompleteDragWithChangedVisibility() {
  shelf_widget_->Deactivate();
  shelf_widget_->status_area_widget()->Deactivate();

  drag_auto_hide_state_ = drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN
                              ? SHELF_AUTO_HIDE_HIDDEN
                              : SHELF_AUTO_HIDE_SHOWN;

  // Gesture drag will only change the auto hide state of the shelf but not the
  // auto hide behavior. Auto hide behavior can only be changed through the
  // context menu of the shelf. Set |drag_status_| to
  // kDragCompleteInProgress to set the auto hide state to
  // |drag_auto_hide_state_|.
  drag_status_ = kDragCompleteInProgress;

  UpdateVisibilityState();
  drag_status_ = kDragNone;
  hotseat_is_in_drag_ = false;
}

float ShelfLayoutManager::GetAppListBackgroundOpacityOnShelfOpacity() {
  float opacity = shelf_widget_->GetBackgroundAlphaValue(
                      shelf_background_type_before_drag_) /
                  static_cast<float>(ShelfBackgroundAnimator::kMaxAlpha);
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  if (launcher_above_shelf_bottom_amount_ < shelf_size)
    return opacity;
  float launcher_above_shelf_amount =
      std::max(0.f, launcher_above_shelf_bottom_amount_ - shelf_size);
  float coefficient = std::min(
      launcher_above_shelf_amount / (AppListView::kNumOfShelfSize * shelf_size),
      1.0f);
  float app_list_view_opacity = is_background_blur_enabled_
                                    ? AppListView::kAppListOpacityWithBlur
                                    : AppListView::kAppListOpacity;
  return app_list_view_opacity * coefficient + (1 - coefficient) * opacity;
}

bool ShelfLayoutManager::IsSwipingCorrectDirection() {
  switch (shelf_->alignment()) {
    case SHELF_ALIGNMENT_BOTTOM:
    case SHELF_ALIGNMENT_BOTTOM_LOCKED:
    case SHELF_ALIGNMENT_RIGHT:
      if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN)
        return drag_amount_ > 0;
      return drag_amount_ < 0;
    case SHELF_ALIGNMENT_LEFT:
      if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN)
        return drag_amount_ < 0;
      return drag_amount_ > 0;
  }
  return false;
}

bool ShelfLayoutManager::ShouldChangeVisibilityAfterDrag(
    const ui::LocatedEvent& event_in_screen) {
  // Shelf can be visible in 1) SHELF_VISIBLE or 2) SHELF_AUTO_HIDE but in
  // SHELF_AUTO_HIDE_SHOWN. See details in IsVisible. Dragging on SHELF_VISIBLE
  // shelf should not change its visibility since it should be kept visible.
  if (visibility_state() == SHELF_VISIBLE)
    return false;

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_END ||
      event_in_screen.type() == ui::ET_MOUSE_RELEASED) {
    // The visibility of the shelf changes only if the shelf was dragged X%
    // along the correct axis. If the shelf was already visible, then the
    // direction of the drag does not matter.
    const float kDragHideThreshold = 0.4f;
    const gfx::Rect bounds = GetIdealBounds();
    const float drag_ratio =
        fabs(drag_amount_) /
        (shelf_->IsHorizontalAlignment() ? bounds.height() : bounds.width());

    return IsSwipingCorrectDirection() && drag_ratio > kDragHideThreshold;
  }

  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START)
    return IsSwipingCorrectDirection();

  return false;
}

void ShelfLayoutManager::UpdateWorkspaceMask(
    WorkspaceWindowState window_state) {
  // Disable the mask on NonLockScreenContainer if maximized/fullscreen window
  // is on top.
  // TODO(oshima): Investigate if we can remove SetMasksToBounds calls
  // crbug.com/898236.
  auto* root_window_controller =
      RootWindowController::ForWindow(shelf_widget_->GetNativeWindow());
  auto* container = root_window_controller->GetContainer(
      kShellWindowId_NonLockScreenContainersContainer);
  switch (window_state) {
    case WorkspaceWindowState::kMaximized:
    case WorkspaceWindowState::kFullscreen:
      container->layer()->SetMasksToBounds(false);
      break;
    case WorkspaceWindowState::kDefault:
      container->layer()->SetMasksToBounds(true);
      break;
  }
}

void ShelfLayoutManager::SendA11yAlertForFullscreenWorkspaceState(
    WorkspaceWindowState current_workspace_window_state) {
  if (previous_workspace_window_state_ != WorkspaceWindowState::kFullscreen &&
      current_workspace_window_state == WorkspaceWindowState::kFullscreen) {
    Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
        AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_ENTERED);
  } else if (previous_workspace_window_state_ ==
                 WorkspaceWindowState::kFullscreen &&
             current_workspace_window_state !=
                 WorkspaceWindowState::kFullscreen) {
    Shell::Get()->accessibility_controller()->TriggerAccessibilityAlert(
        AccessibilityAlert::WORKSPACE_FULLSCREEN_STATE_EXITED);
  }
  previous_workspace_window_state_ = current_workspace_window_state;
}

bool ShelfLayoutManager::MaybeStartDragWindowFromShelf(
    const ui::LocatedEvent& event_in_screen,
    base::Optional<float> scroll_y) {
  if (!features::IsDragFromShelfToHomeOrOverviewEnabled())
    return false;
  if (!IsTabletModeEnabled())
    return false;
  if (drag_status_ != kDragInProgress)
    return false;

  // Do not drag on a auto-hide hidden shelf or a hidden shelf.
  if ((visibility_state() == SHELF_AUTO_HIDE &&
       auto_hide_state() == SHELF_AUTO_HIDE_HIDDEN) ||
      visibility_state() == SHELF_HIDDEN) {
    return false;
  }

  // Do not drag on kShown hotseat (it should be in home screen).
  if (hotseat_state() == HotseatState::kShown)
    return false;

  // If hotseat is hidden when drag starts, do not start drag window if hotseat
  // hasn't been fully dragged up.
  if (hotseat_state() == HotseatState::kHidden) {
    ShelfConfig* shelf_config = ShelfConfig::Get();
    const int full_drag_amount =
        -(shelf_config->shelf_size() + shelf_config->hotseat_bottom_padding() +
          shelf_config->hotseat_size());
    if (drag_amount_ > full_drag_amount)
      return false;
  } else if (hotseat_state() == HotseatState::kExtended) {
    // If the start location is above the shelf (e.g., on the extended hotseat),
    // do not allow the drag.
    const gfx::Rect shelf_bounds = GetVisibleShelfBounds();
    if (event_in_screen.location().y() < shelf_bounds.y())
      return false;
    // Do not start drag if it's a downward update event.
    if (scroll_y.has_value() && *scroll_y > 0)
      return false;
  }

  // Do not allow window drag if the previous dragged window is still animating.
  if (window_drag_controller_ &&
      window_drag_controller_->IsDraggedWindowAnimating()) {
    return false;
  }

  aura::Window* window =
      GetWindowForDragToHomeOrOverview(event_in_screen.location());
  if (!window)
    return false;

  window_drag_controller_ = std::make_unique<DragWindowFromShelfController>(
      window, event_in_screen.location(), hotseat_state());
  return true;
}

void ShelfLayoutManager::MaybeUpdateWindowDrag(
    const ui::LocatedEvent& event_in_screen,
    float scroll_x,
    float scroll_y) {
  if (!IsWindowDragInProgress() &&
      !MaybeStartDragWindowFromShelf(event_in_screen,
                                     base::make_optional(scroll_y))) {
    return;
  }

  DCHECK_EQ(drag_status_, kDragInProgress);
  window_drag_controller_->Drag(event_in_screen.location(), scroll_x, scroll_y);
}

base::Optional<ShelfWindowDragResult> ShelfLayoutManager::MaybeEndWindowDrag(
    const ui::LocatedEvent& event_in_screen) {
  if (!IsWindowDragInProgress())
    return base::nullopt;

  DCHECK_EQ(drag_status_, kDragInProgress);
  base::Optional<float> velocity_y;
  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    velocity_y = base::make_optional(
        event_in_screen.AsGestureEvent()->details().velocity_y());
  }

  return window_drag_controller_->EndDrag(event_in_screen.location(),
                                          velocity_y);
}

void ShelfLayoutManager::MaybeCancelWindowDrag() {
  if (!IsWindowDragInProgress())
    return;

  DCHECK_EQ(drag_status_, kDragInProgress);
  window_drag_controller_->CancelDrag();
}

bool ShelfLayoutManager::IsWindowDragInProgress() const {
  return window_drag_controller_ && window_drag_controller_->drag_started();
}

}  // namespace ash
