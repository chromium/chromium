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
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/public/cpp/presentation_time_recorder.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/contextual_tooltip.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/home_to_overview_nudge_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/in_app_to_home_nudge_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
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
#include "components/prefs/pref_service.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/message_center/message_center.h"
#include "ui/views/border.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace {

// Default Target Dim Opacity for floating shelf.
constexpr float kFloatingShelfDimOpacity = 0.74f;

// Target Dim Opacity for shelf when shelf is in the maximized state.
constexpr float kMaximizedShelfDimOpacity = 0.6f;

// Default opacity for shelf without dimming.
constexpr float kDefaultShelfOpacity = 1.0f;

// Delay before showing the shelf. This is after the mouse stops moving.
constexpr int kAutoHideDelayMS = 200;

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

aura::Window* GetDragHandleNudgeWindow(ShelfWidget* shelf_widget) {
  if (!shelf_widget->GetDragHandle())
    return nullptr;
  if (!shelf_widget->GetDragHandle()->drag_handle_nudge())
    return nullptr;
  return shelf_widget->GetDragHandle()
      ->drag_handle_nudge()
      ->GetWidget()
      ->GetNativeWindow();
}

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

void SetupAnimator(ui::ScopedLayerAnimationSettings* animation_setter,
                   base::TimeDelta animation_duration,
                   gfx::Tween::Type type) {
  animation_setter->SetTransitionDuration(animation_duration);
  if (!animation_duration.is_zero()) {
    animation_setter->SetTweenType(type);
    animation_setter->SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  }
}

void AnimateOpacity(ui::Layer* layer,
                    float target_opacity,
                    base::TimeDelta animation_duration,
                    gfx::Tween::Type type) {
  ui::ScopedLayerAnimationSettings animation_setter(layer->GetAnimator());
  SetupAnimator(&animation_setter, animation_duration, type);
  layer->SetOpacity(target_opacity);
}

// Returns true if the window is in the app list window container.
bool IsAppListWindow(const aura::Window* window) {
  const aura::Window* parent = window->parent();
  return parent && parent->id() == kShellWindowId_AppListContainer;
}

bool IsHotseatEnabled() {
  return Shell::Get()->IsInTabletMode() &&
         chromeos::switches::ShouldShowShelfHotseat();
}

int GetOffset(int offset, bool from_touchpad) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();

  if (from_touchpad)
    return prefs->GetBoolean(prefs::kNaturalScroll) ? -offset : offset;

  return prefs->GetBoolean(prefs::kMouseReverseScroll) ? -offset : offset;
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
  if (!Shell::Get()->IsInTabletMode())
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
    bool transitioned_from_overview_to_home,
    HotseatState old_state,
    HotseatState current_state) {
  if (window_drag_result.has_value() &&
      (window_drag_result == ShelfWindowDragResult::kGoToOverviewMode ||
       window_drag_result == ShelfWindowDragResult::kGoToSplitviewMode) &&
      old_state == HotseatState::kHidden) {
    return InAppShelfGestures::kSwipeUpToShow;
  }

  if (window_drag_result.has_value() &&
      window_drag_result == ShelfWindowDragResult::kGoToHomeScreen) {
    return InAppShelfGestures::kFlingUpToShowHomeScreen;
  }

  if (transitioned_from_overview_to_home)
    return InAppShelfGestures::kFlingUpToShowHomeScreen;

  if (old_state == current_state)
    return base::nullopt;

  if (current_state == HotseatState::kHidden)
    return InAppShelfGestures::kSwipeDownToHide;

  if (current_state == HotseatState::kExtended)
    return InAppShelfGestures::kSwipeUpToShow;

  return base::nullopt;
}

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
  ShelfConfig::Get()->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->app_list_controller()->AddObserver(this);
  shell->lock_state_controller()->AddObserver(this);
  shell->activation_client()->AddObserver(this);
  shell->locale_update_controller()->AddObserver(this);
  state_.session_state = shell->session_controller()->GetSessionState();
  shelf_background_type_ = GetShelfBackgroundType();
  wallpaper_controller_observer_.Add(shell->wallpaper_controller());
  display::Screen::GetScreen()->AddObserver(this);
  message_center::MessageCenter::Get()->AddObserver(this);

  // DesksController could be null when virtual desks feature is not enabled.
  if (DesksController::Get())
    DesksController::Get()->AddObserver(this);
}

void ShelfLayoutManager::PrepareForShutdown() {
  // This might get called twice during shell shutdown - once at the beginning,
  // and once as part of root window controller shutdown.
  if (in_shutdown_)
    return;

  in_shutdown_ = true;

  // Stop observing changes to avoid updating a partially destructed shelf.
  Shell::Get()->activation_client()->RemoveObserver(this);
  ShelfConfig::Get()->RemoveObserver(this);

  // DesksController could be null when virtual desks feature is not enabled.
  if (DesksController::Get())
    DesksController::Get()->RemoveObserver(this);

  SplitViewController::Get(shelf_widget_->GetNativeWindow())
      ->RemoveObserver(this);

  message_center::MessageCenter::Get()->RemoveObserver(this);
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
  return shelf_->SelectValueForShelfAlignment(
      gfx::Rect(rect.x(), rect.bottom() - shelf_size, rect.width(), shelf_size),
      gfx::Rect(rect.x(), rect.y(), shelf_size, rect.height()),
      gfx::Rect(rect.right() - shelf_size, rect.y(), shelf_size,
                rect.height()));
}

gfx::Rect ShelfLayoutManager::GetIdealBoundsForWorkAreaCalculation() const {
  if (!Shell::Get()->IsInTabletMode() ||
      !chromeos::switches::ShouldShowShelfHotseat() ||
      state_.session_state != session_manager::SessionState::ACTIVE) {
    return GetIdealBounds();
  }

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

void ShelfLayoutManager::LayoutShelf(bool animate) {
  // The ShelfWidget may be partially closed (no native widget) during shutdown
  // or before it's been fully initialized so skip layout.
  if (in_shutdown_ || !shelf_widget_->native_widget())
    return;

  CalculateTargetBoundsAndUpdateWorkArea();
  UpdateBoundsAndOpacity(animate);

  // Update insets in ShelfWindowTargeter when shelf bounds change.
  for (auto& observer : observers_)
    observer.WillChangeVisibilityState(visibility_state());
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
  } else if (Shell::Get()->session_controller()->IsRunningInAppMode()) {
    SetState(SHELF_HIDDEN);
  } else {
    // TODO(zelidrag): Verify shelf drag animation still shows on the device
    // when we are in ShelfAutoHideBehavior::kAlwaysHidden.
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

void ShelfLayoutManager::UpdateVisibilityStateForBackGesture() {
  base::AutoReset<bool> back_gesture(&state_forced_by_back_gesture_, true);
  SetState(SHELF_VISIBLE);
  LayoutShelf(/*animate=*/true);
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
    if (shelf_->shelf_widget()->GetVisibleShelfBounds().Contains(
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

void ShelfLayoutManager::UpdateContextualNudges() {
  if (!ash::features::AreContextualNudgesEnabled())
    return;

  // Do not allow nudges outside of an active session.
  if (Shell::Get()->session_controller()->GetSessionState() !=
      session_manager::SessionState::ACTIVE) {
    return;
  }

  const bool in_app_shelf = ShelfConfig::Get()->is_in_app();
  const bool in_tablet_mode = ShelfConfig::Get()->in_tablet_mode();
  const bool in_overview_mode = ShelfConfig::Get()->in_overview_mode();

  contextual_tooltip::SetDragHandleNudgeDisabledForHiddenShelf(!IsVisible());

  if (in_app_shelf && in_tablet_mode && !in_app_to_home_nudge_controller_) {
    in_app_to_home_nudge_controller_ =
        std::make_unique<InAppToHomeNudgeController>(shelf_widget_);
  }

  if (in_app_to_home_nudge_controller_) {
    in_app_to_home_nudge_controller_->SetNudgeAllowedForCurrentShelf(
        in_tablet_mode, in_app_shelf,
        ShelfConfig::Get()->shelf_controls_shown());
  }

  // Create home to overview nudge controller if home to overview nudge is
  // allowed by the current shelf state.
  const bool allow_home_to_overview_nudge =
      in_tablet_mode && !in_app_shelf &&
      !ShelfConfig::Get()->shelf_controls_shown() && !in_overview_mode;
  if (allow_home_to_overview_nudge && !home_to_overview_nudge_controller_) {
    home_to_overview_nudge_controller_ =
        std::make_unique<HomeToOverviewNudgeController>(
            shelf_->hotseat_widget());
  }
  if (home_to_overview_nudge_controller_) {
    home_to_overview_nudge_controller_->SetNudgeAllowedForCurrentShelf(
        allow_home_to_overview_nudge);
  }
}

void ShelfLayoutManager::HideContextualNudges() {
  if (!ash::features::AreContextualNudgesEnabled())
    return;

  shelf_widget_->HideDragHandleNudge(
      contextual_tooltip::DismissNudgeReason::kOther);

  if (home_to_overview_nudge_controller_)
    home_to_overview_nudge_controller_->SetNudgeAllowedForCurrentShelf(false);
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

  // In overview mode, only the gesture tap event is able to make the hotseat
  // exit the extended mode.
  const bool in_overview =
      Shell::Get()->overview_controller() &&
      Shell::Get()->overview_controller()->InOverviewSession();
  ui::EventType interesting_type =
      in_overview ? ui::ET_GESTURE_TAP : ui::ET_GESTURE_BEGIN;

  // Record gesture metrics only for `interesting_type` to avoid over counting.
  if (event->type() == interesting_type) {
    UMA_HISTOGRAM_ENUMERATION(
        kHotseatGestureHistogramName,
        InAppShelfGestures::kHotseatHiddenDueToInteractionOutsideOfShelf);
  }

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
  if (shelf_widget_->HandleLoginShelfGestureEvent(event_in_screen))
    return true;

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
  CancelDrag(base::nullopt);
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
    ui::MouseWheelEvent* event,
    bool from_touchpad) {
  const int y_offset = GetOffset(event->y_offset(), from_touchpad);
  if (y_offset <= ShelfConfig::Get()->mousewheel_scroll_offset_threshold())
    return;

  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(shelf_widget_->GetNativeWindow())
          .id(),
      kScrollFromShelf, event->time_stamp());
}

ShelfBackgroundType ShelfLayoutManager::GetShelfBackgroundType() const {
  if (state_.pre_lock_screen_animation_active)
    return ShelfBackgroundType::kDefaultBg;

  // Handle all other non active screen states, including OOBE and pre-login.
  if (state_.session_state == session_manager::SessionState::OOBE)
    return ShelfBackgroundType::kOobe;
  if (state_.session_state != session_manager::SessionState::ACTIVE) {
    if (Shell::Get()->wallpaper_controller()->HasShownAnyWallpaper() &&
        !Shell::Get()
             ->wallpaper_controller()
             ->IsWallpaperBlurredForLockState()) {
      return ShelfBackgroundType::kLoginNonBlurredWallpaper;
    }
    return ShelfBackgroundType::kLogin;
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
      Shell::Get()->app_list_controller()->IsVisible(display_.id());
  const bool in_overview =
      Shell::Get()->overview_controller() &&
      Shell::Get()->overview_controller()->InOverviewSession();
  if (Shell::Get()->IsInTabletMode()) {
    if (app_list_is_visible) {
      // TODO(https://crbug.com/1058205): Test this behavior.
      // If the IME virtual keyboard is showing, the shelf should appear in-app.
      // The workspace area in tablet mode is always the in-app workspace area,
      // and the virtual keyboard places itself on screen based on workspace
      // area.
      if (ShelfConfig::Get()->is_virtual_keyboard_shown())
        return ShelfBackgroundType::kInApp;
      // If the home launcher is shown or mostly shown, show the home launcher
      // background. If it is mostly hidden, show the in-app or overview
      // background.
      if (!Shell::Get()->app_list_controller()->GetTargetVisibility(
              display_.id())) {
        if (features::IsMaintainShelfStateWhenEnteringOverviewEnabled()) {
          return in_overview ? shelf_background_type_
                             : ShelfBackgroundType::kInApp;
        }
        return in_overview ? ShelfBackgroundType::kOverview
                           : ShelfBackgroundType::kInApp;
      }
      return ShelfBackgroundType::kHomeLauncher;
    } else if (Shell::Get()
                   ->app_list_controller()
                   ->home_launcher_transition_state() !=
               AppListControllerImpl::HomeLauncherTransitionState::kFinished) {
      return ShelfBackgroundType::kHomeLauncher;
    } else if (maximized) {
      // If the home launcher is not shown but it is maximized, show the
      // in-app shelf.
      return ShelfBackgroundType::kInApp;
    }
  } else if (app_list_is_visible) {
    // When auto-hide shelf is enabled, shelf cannot be considered maximized.
    if (maximized && state_.visibility_state != SHELF_AUTO_HIDE)
      return ShelfBackgroundType::kMaximizedWithAppList;

    return ShelfBackgroundType::kAppList;
  }

  if (maximized) {
    // When a window is maximized, if the auto-hide shelf is enabled and we are
    // in clamshell mode, the shelf will keep the default transparent
    // background.
    if (!Shell::Get()->IsInTabletMode() &&
        state_.visibility_state == SHELF_AUTO_HIDE)
      return ShelfBackgroundType::kDefaultBg;

    return ShelfBackgroundType::kMaximized;
  }

  if (in_overview)
    return ShelfBackgroundType::kOverview;

  return ShelfBackgroundType::kDefaultBg;
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
  if (!is_background_blur_enabled_)
    return false;

  if (chromeos::switches::ShouldShowShelfHotseat()) {
    return shelf_background_type_ == ShelfBackgroundType::kDefaultBg &&
           state_.session_state == session_manager::SessionState::ACTIVE;
  }

  return (shelf_background_type_ == ShelfBackgroundType::kHomeLauncher ||
          shelf_background_type_ == ShelfBackgroundType::kDefaultBg) &&
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
  return false;
}

void ShelfLayoutManager::CancelDragOnShelfIfInProgress() {
  if (drag_status_ == kDragInProgress ||
      drag_status_ == kDragAppListInProgress ||
      drag_status_ == kDragHomeToOverviewInProgress) {
    CancelDrag(base::nullopt);
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
  // affect the layout in any way.
  if (phase_ != ShelfLayoutPhase::kMoving &&
      ((shelf_widget_->GetNativeWindow() == child) ||
       (shelf_widget_->status_area_widget()->GetNativeWindow() == child))) {
    LayoutShelf(/*animate=*/true);
  }
}

void ShelfLayoutManager::OnShelfAutoHideBehaviorChanged(
    aura::Window* root_window) {
  UpdateVisibilityState();
}

void ShelfLayoutManager::OnShelfAlignmentChanged(aura::Window* root_window,
                                                 ShelfAlignment old_alignment) {
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

void ShelfLayoutManager::OnShellDestroying() {
  // Prepare for shutdown early, to prevent window stacking changes that may
  // happen during shutdown (e.g. during overview controller, or tablet mode
  // controller destruction) from causing visibility and state updates.
  PrepareForShutdown();
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
  UpdateContextualNudges();
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
  if (!IsShelfWindow(gained_active) &&
      !(window_drag_controller_ &&
        window_drag_controller_->during_window_restoration_callback())) {
    shelf_->hotseat_widget()->set_manually_extended(/*value=*/false);
  }

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
  HideContextualNudges();
  if (was_adding_user != state_.IsAddingSecondaryUser()) {
    UpdateShelfVisibilityAfterLoginUIChange();
    return;
  }

  // Force the shelf to layout for alignment (bottom if locked, restore the
  // previous alignment otherwise).
  if (was_locked != state_.IsScreenLocked())
    UpdateShelfVisibilityAfterLoginUIChange();

  CalculateTargetBoundsAndUpdateWorkArea();
  UpdateBoundsAndOpacity(true /* animate */);
  UpdateVisibilityState();
  UpdateContextualNudges();
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
  if (phase_ == ShelfLayoutPhase::kMoving)
    return;

  // Update |user_work_area_bounds_| for the new display arrangement.
  CalculateTargetBoundsAndUpdateWorkArea();
}

void ShelfLayoutManager::OnLocaleChanged() {
  shelf_->shelf_widget()->HandleLocaleChange();
  shelf_->status_area_widget()->HandleLocaleChange();
  shelf_->navigation_widget()->HandleLocaleChange();

  // Layout update is needed when language changes between LTR and RTL.
  LayoutShelf();
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

float ShelfLayoutManager::GetOpacity() const {
  return target_opacity_;
}

void ShelfLayoutManager::OnShelfConfigUpdated() {
  SetState(state_.visibility_state);
  LayoutShelf(/*animate=*/true);
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
  UpdateContextualNudges();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

void ShelfLayoutManager::OnCenterVisibilityChanged(
    message_center::Visibility visibility) {
  if (!chromeos::switches::ShouldShowShelfHotseat())
    return;

  // Uses base::CancelableClosure to handle two edge cases: (1)
  // ShelfLayoutManager is destructed before the callback runs. (2) The previous
  // callback is still pending.
  visibility_update_for_tray_callback_.Reset(base::BindOnce(
      &ShelfLayoutManager::UpdateVisibilityStateForSystemTrayChange,
      base::Unretained(this), visibility));

  // OnCenterVisibilityChanged is called when the visibility of system tray
  // is set, which is before the tray bubble is created/destructed. Meanwhile,
  // we rely on the state of tray bubble to calculate the auto-hide state.
  // Use ThreadTaskRunnerHandle to specify that the task runs on the UI thread.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, visibility_update_for_tray_callback_.callback());
}

void ShelfLayoutManager::SuspendWorkAreaUpdate() {
  ++suspend_work_area_update_;
}

void ShelfLayoutManager::ResumeWorkAreaUpdate() {
  --suspend_work_area_update_;
  DCHECK_GE(suspend_work_area_update_, 0);

  if (suspend_work_area_update_ || in_shutdown_)
    return;

  UpdateVisibilityState();

  CalculateTargetBoundsAndUpdateWorkArea();
  UpdateBoundsAndOpacity(/*animate=*/true);
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
      CalculateHotseatState(visibility_state, state.auto_hide_state);

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

  if (!delay_background_change)
    MaybeUpdateShelfBackground(change_type);

  CalculateTargetBoundsAndUpdateWorkArea();
  HotseatWidget* hotseat_widget = shelf_->hotseat_widget();
  hotseat_widget->SetState(new_hotseat_state);

  // Called before UpdateBoundsAndOpacity(). Because creation of the hotseat
  // bounds animation which is triggered by hotseat state update requires the
  // state transition type.
  HotseatWidget::ScopedInStateTransition scoped_in_state_transition(
      hotseat_widget, previous_hotseat_state, new_hotseat_state);

  UpdateBoundsAndOpacity(true /* animate */);

  // OnAutoHideStateChanged Should be emitted when:
  //  - firstly state changed to auto-hide from other state
  //  - or, auto_hide_state has changed
  if ((old_state.visibility_state != state_.visibility_state &&
       state_.visibility_state == SHELF_AUTO_HIDE) ||
      old_state.auto_hide_state != state_.auto_hide_state) {
    for (auto& observer : observers_)
      observer.OnAutoHideStateChanged(state_.auto_hide_state);
  }

  if (previous_hotseat_state != hotseat_state()) {
    if (hotseat_state() == HotseatState::kExtended)
      hotseat_event_handler_ = std::make_unique<HotseatEventHandler>(this);
    else
      hotseat_event_handler_.reset();
    for (auto& observer : observers_)
      observer.OnHotseatStateChanged(previous_hotseat_state, hotseat_state());
  }

  UpdateContextualNudges();
}

HotseatState ShelfLayoutManager::CalculateHotseatState(
    ShelfVisibilityState visibility_state,
    ShelfAutoHideState auto_hide_state) const {
  if (!IsHotseatEnabled() || !shelf_->IsHorizontalAlignment())
    return HotseatState::kShownClamshell;

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
      app_list_controller->GetTargetVisibility(display_.id()) ||
      app_list_controller->ShouldHomeLauncherBeVisible();

  // TODO(https://crbug.com/1058205): Test this behavior.
  if (ShelfConfig::Get()->is_virtual_keyboard_shown())
    return HotseatState::kHidden;

  // Only force to show if there is not a pending drag operation.
  if (shelf_widget_->is_hotseat_forced_to_show() && drag_status_ == kDragNone)
    return app_list_visible ? HotseatState::kShownHomeLauncher
                            : HotseatState::kExtended;

  bool in_split_view = false;
  if (in_overview) {
    auto* split_view_controller =
        SplitViewController::Get(Shell::GetPrimaryRootWindow());
    in_split_view =
        split_view_controller && split_view_controller->InSplitViewMode();
  }
  const int hotseat_size = shelf_->hotseat_widget()->GetHotseatSize();
  switch (drag_status_) {
    case kDragNone:
    case kDragHomeToOverviewInProgress: {
      switch (app_list_controller->home_launcher_transition_state()) {
        case AppListControllerImpl::HomeLauncherTransitionState::kMostlyShown:
          return HotseatState::kShownHomeLauncher;
        case AppListControllerImpl::HomeLauncherTransitionState::kMostlyHidden:
          return in_overview ? HotseatState::kExtended : HotseatState::kHidden;
        case AppListControllerImpl::HomeLauncherTransitionState::kFinished:
          if (app_list_visible)
            return HotseatState::kShownHomeLauncher;

          // Show the hotseat if the shelf view's context menu is showing.
          if (shelf_->hotseat_widget()->IsShowingShelfMenu())
            return HotseatState::kExtended;

          if (in_overview && !in_split_view) {
            // Maintain the ShownHomeLauncher state if we enter overview mode
            // from it.
            if (features::IsMaintainShelfStateWhenEnteringOverviewEnabled() &&
                hotseat_state() == HotseatState::kShownHomeLauncher) {
              return HotseatState::kShownHomeLauncher;
            }
            return HotseatState::kExtended;
          }

          if (state_forced_by_back_gesture_)
            return HotseatState::kExtended;

          if (visibility_state == SHELF_AUTO_HIDE) {
            if (auto_hide_state == SHELF_AUTO_HIDE_HIDDEN ||
                should_hide_hotseat_) {
              return HotseatState::kHidden;
            }
            return HotseatState::kExtended;
          }
          if (shelf_->hotseat_widget()->is_manually_extended() &&
              !should_hide_hotseat_) {
            return HotseatState::kExtended;
          }

          // If none of the conditions above were met means that the state
          // changed because of an action other than a user intervention.
          // We should hide the hotseat and reset the |is_manually extended|
          // flag to false.
          shelf_->hotseat_widget()->set_manually_extended(false);
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

      if (app_list_visible)
        return HotseatState::kShownHomeLauncher;

      if (in_overview && !in_split_view)
        return HotseatState::kExtended;

      if (shelf_->hotseat_widget()->IsExtended())
        return HotseatState::kExtended;

      // |drag_amount_| is relative to the top of the hotseat when the drag
      // begins with an extended hotseat. Correct for this to get
      // |total_amount_dragged|.
      const int drag_base =
          (hotseat_state() == HotseatState::kExtended &&
           state_.visibility_state == SHELF_VISIBLE)
              ? (hotseat_size + ShelfConfig::Get()->hotseat_bottom_padding())
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

      if (dragged_to_bezel)
        return HotseatState::kHidden;

      if (std::abs(last_drag_velocity_) >= 120) {
        if (last_drag_velocity_ > 0)
          return HotseatState::kHidden;
        return HotseatState::kExtended;
      }

      const int top_of_hotseat_to_screen_bottom =
          screen_bottom -
          shelf_->hotseat_widget()->GetWindowBoundsInScreen().y();
      const bool dragged_over_half_hotseat_size =
          top_of_hotseat_to_screen_bottom < hotseat_size / 2;
      if (dragged_over_half_hotseat_size)
        return HotseatState::kHidden;

      return HotseatState::kExtended;
    }
    case kDragAppListInProgress:
      return app_list_controller->home_launcher_transition_state() ==
                     AppListControllerImpl::HomeLauncherTransitionState::
                         kMostlyHidden
                 ? HotseatState::kHidden
                 : HotseatState::kShownHomeLauncher;
    default:
      // Do not change the hotseat state until the drag is complete or
      // canceled.
      return hotseat_state();
  }
  NOTREACHED();
  return HotseatState::kNone;
}

ShelfVisibilityState ShelfLayoutManager::CalculateShelfVisibility() {
  switch (shelf_->auto_hide_behavior()) {
    case ShelfAutoHideBehavior::kAlways:
      return SHELF_AUTO_HIDE;
    case ShelfAutoHideBehavior::kNever:
      return SHELF_VISIBLE;
    case ShelfAutoHideBehavior::kAlwaysHidden:
      return SHELF_HIDDEN;
  }
  return SHELF_VISIBLE;
}

bool ShelfLayoutManager::SetDimmed(bool dimmed) {
  // Do nothing if we are already in the correct dim state.
  if (dimmed_for_inactivity_ == dimmed)
    return false;

  // We do not want the auto hide state to change while setting up animations.
  std::unique_ptr<Shelf::ScopedAutoHideLock> auto_hide_lock =
      std::make_unique<Shelf::ScopedAutoHideLock>(shelf_);

  // We should not set the dim state if the shelf is hidden. Shelf will be
  // undimmed when it transitions into a visible state.
  if (!state_.IsShelfVisible())
    return false;

  dimmed_for_inactivity_ = dimmed;

  CalculateTargetBoundsAndUpdateWorkArea();

  const base::TimeDelta dim_animation_duration =
      ShelfConfig::Get()->DimAnimationDuration();
  const gfx::Tween::Type dim_animation_tween =
      ShelfConfig::Get()->DimAnimationTween();

  const bool animate = !dim_animation_duration.is_zero();
  base::Optional<ui::AnimationThroughputReporter> navigation_widget_reporter;
  if (animate) {
    navigation_widget_reporter.emplace(
        GetLayer(shelf_->navigation_widget())->GetAnimator(),
        shelf_->GetNavigationWidgetAnimationReportCallback(hotseat_state()));
  }

  AnimateOpacity(GetLayer(shelf_->navigation_widget()), target_opacity_,
                 dim_animation_duration, dim_animation_tween);

  AnimateOpacity(shelf_->hotseat_widget()->GetShelfView()->layer(),
                 shelf_->hotseat_widget()->CalculateShelfViewOpacity(),
                 dim_animation_duration, dim_animation_tween);

  AnimateOpacity(GetLayer(shelf_->status_area_widget()), target_opacity_,
                 dim_animation_duration, dim_animation_tween);

  shelf_widget_->SetLoginShelfButtonOpacity(target_opacity_);
  return true;
}

void ShelfLayoutManager::UpdateBoundsAndOpacity(bool animate) {
  DCHECK_EQ(phase_, ShelfLayoutPhase::kAiming) << " Aim before moving!";
  phase_ = ShelfLayoutPhase::kMoving;
  ShelfNavigationWidget* nav_widget = shelf_->navigation_widget();
  HotseatWidget* hotseat_widget = shelf_->hotseat_widget();
  StatusAreaWidget* status_widget = shelf_widget_->status_area_widget();
  {
    shelf_->shelf_widget()->UpdateLayout(animate);
    hotseat_widget->UpdateLayout(animate);
    status_widget->UpdateLayout(animate);
    nav_widget->UpdateLayout(animate);

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
          (shelf_->alignment() != ShelfAlignment::kBottomLocked ||
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
  phase_ = ShelfLayoutPhase::kAtRest;
}

bool ShelfLayoutManager::IsDraggingWindowFromTopOrCaptionArea() const {
  // Currently dragging maximized or fullscreen window from the top or the
  // caption area is only allowed in tablet mode.
  if (!Shell::Get()->IsInTabletMode())
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

gfx::Insets ShelfLayoutManager::CalculateTargetBounds(
    const State& state,
    HotseatState hotseat_target_state) {
  shelf_->shelf_widget()->CalculateTargetBounds();
  shelf_->status_area_widget()->CalculateTargetBounds();
  shelf_->navigation_widget()->CalculateTargetBounds();
  shelf_->hotseat_widget()->CalculateTargetBounds();

  target_opacity_ = ComputeTargetOpacity(state);

  if (drag_status_ == kDragInProgress)
    UpdateTargetBoundsForGesture(hotseat_target_state);

  const int default_shelf_inset =
      GetShelfInset(state.visibility_state, ShelfConfig::Get()->shelf_size());
  // When hotseat is enabled, keep horizontal shelf inset at in-app shelf size
  // to avoid work area updates when the shelf size changes when going to and
  // from home screen (shelf size rules differ on home screen).
  const int horizontal_inset =
      IsHotseatEnabled()
          ? GetShelfInset(state.visibility_state,
                          ShelfConfig::Get()->in_app_shelf_size())
          : default_shelf_inset;
  return shelf_->SelectValueForShelfAlignment(
      gfx::Insets(0, 0, horizontal_inset, 0),
      gfx::Insets(0, default_shelf_inset, 0, 0),
      gfx::Insets(0, 0, 0, default_shelf_inset));
}

void ShelfLayoutManager::CalculateTargetBoundsAndUpdateWorkArea() {
  if (phase_ == ShelfLayoutPhase::kMoving)
    DVLOG(1) << "Careful when switching targets mid-move!";
  phase_ = ShelfLayoutPhase::kAiming;
  HotseatState hotseat_target_state =
      CalculateHotseatState(visibility_state(), auto_hide_state());
  gfx::Insets shelf_insets =
      CalculateTargetBounds(state_, hotseat_target_state);
  gfx::Rect shelf_bounds_for_workarea_calculation =
      shelf_->shelf_widget()->GetTargetBounds();
  // When the hotseat is enabled, only use the in-app shelf bounds when
  // calculating the work area. This prevents windows resizing unnecessarily. If
  // the shelf is not visible then use the regular calculations. Note that on
  // the home screen, the shelf is deemed visible as it is visible with a
  // transparent background.
  if (IsHotseatEnabled() && IsVisible()) {
    shelf_bounds_for_workarea_calculation =
        GetIdealBoundsForWorkAreaCalculation();
  }
  if (!suspend_work_area_update_) {
    WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow())
        ->SetShelfBoundsAndInsets(shelf_bounds_for_workarea_calculation,
                                  shelf_insets);
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
    const bool resist = shelf_->SelectValueForShelfAlignment(
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
  const int baseline = shelf_->SelectValueForShelfAlignment(
      available_bounds.bottom() - (shelf_hidden_at_start ? 0 : shelf_size),
      available_bounds.x() - (shelf_hidden_at_start ? shelf_size : 0),
      available_bounds.right() - (shelf_hidden_at_start ? 0 : shelf_size));

  const int shelf_position = baseline + translate;
  if (horizontal) {
    if (!IsHotseatEnabled()) {
      shelf_->shelf_widget()->UpdateTargetBoundsForGesture(shelf_position);
      shelf_->navigation_widget()->UpdateTargetBoundsForGesture(shelf_position);
      shelf_->hotseat_widget()->UpdateTargetBoundsForGesture(shelf_position);
      shelf_->status_area_widget()->UpdateTargetBoundsForGesture(
          shelf_position);
      return;
    }

    const bool move_shelf_with_hotseat =
        !Shell::Get()->overview_controller()->InOverviewSession() &&
        visibility_state() == SHELF_AUTO_HIDE;
    // Do not allow the shelf to be dragged more than |shelf_size| from the
    // bottom of the display.
    int adjusted_shelf_position =
        std::max(available_bounds.bottom() - shelf_size,
                 static_cast<int>(shelf_position));
    if (move_shelf_with_hotseat) {
      // Window drags only happen after the hotseat has been dragged up to its
      // full height. After the drag moves a window, do not allow the drag to
      // move the hotseat down.
      if (IsWindowDragInProgress())
        adjusted_shelf_position = available_bounds.bottom() - shelf_size;
      gfx::Rect updated_target_bounds =
          shelf_->shelf_widget()->GetTargetBounds();
      updated_target_bounds.set_y(adjusted_shelf_position);
      shelf_->shelf_widget()->set_target_bounds(updated_target_bounds);
      shelf_->navigation_widget()->UpdateTargetBoundsForGesture(
          adjusted_shelf_position);
      shelf_->status_area_widget()->UpdateTargetBoundsForGesture(
          adjusted_shelf_position);
    }

    int hotseat_y = 0;
    const int hotseat_extended_y =
        shelf_->hotseat_widget()->GetHotseatSize() +
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
    gfx::Rect hotseat_bounds = shelf_->hotseat_widget()->GetTargetBounds();
    hotseat_bounds.set_y(hotseat_y + adjusted_shelf_position);
    shelf_->hotseat_widget()->set_target_bounds(hotseat_bounds);
    return;
  }

  shelf_->shelf_widget()->UpdateTargetBoundsForGesture(shelf_position);
  shelf_->hotseat_widget()->UpdateTargetBoundsForGesture(shelf_position);
  shelf_->navigation_widget()->UpdateTargetBoundsForGesture(shelf_position);
  shelf_->status_area_widget()->UpdateTargetBoundsForGesture(shelf_position);
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

gfx::Rect ShelfLayoutManager::GetAutoHideShowShelfRegionInScreen() const {
  gfx::Rect shelf_bounds_in_screen =
      shelf_->shelf_widget()->GetVisibleShelfBounds();
  gfx::Vector2d offset = shelf_->SelectValueForShelfAlignment(
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

  const bool in_tablet_mode = Shell::Get()->IsInTabletMode();
  // Don't let the shelf auto-hide when in tablet mode and Chromevox is on.
  if (in_tablet_mode &&
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (!in_tablet_mode && shelf_widget_->IsShowingAppList())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->ShouldShowShelf()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (shelf_widget_->IsShowingMenu())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsActive() || shelf_->navigation_widget()->IsActive() ||
      shelf_->hotseat_widget()->IsActive() ||
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

  const auto auto_hide_state_from_cursor =
      CalculateAutoHideStateBasedOnCursorLocation();
  if (auto_hide_state_from_cursor.has_value())
    return *auto_hide_state_from_cursor;

  if (window_drag_controller_ &&
      window_drag_controller_->during_window_restoration_callback()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  return SHELF_AUTO_HIDE_HIDDEN;
}

base::Optional<ShelfAutoHideState>
ShelfLayoutManager::CalculateAutoHideStateBasedOnCursorLocation() const {
  // No mouse is available in tablet mode. So there is no point to calculate
  // the auto-hide state by the cursor location in this scenario.
  const bool in_tablet_mode = Shell::Get()->IsInTabletMode();
  if (in_tablet_mode)
    return base::nullopt;

  // Do not perform any checks based on the cursor position if the mouse
  // cursor is currently hidden.
  if (!shelf_widget_->IsMouseEventsEnabled())
    return SHELF_AUTO_HIDE_HIDDEN;

  gfx::Rect shelf_region = shelf_->shelf_widget()->GetVisibleShelfBounds();
  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->IsMessageBubbleShown() &&
      IsVisible()) {
    // Increase the the hit test area to prevent the shelf from disappearing
    // when the mouse is over the bubble gap.
    ShelfAlignment alignment = shelf_->alignment();
    shelf_region.Inset(
        alignment == ShelfAlignment::kRight ? -kNotificationBubbleGapHeight : 0,
        shelf_->IsHorizontalAlignment() ? -kNotificationBubbleGapHeight : 0,
        alignment == ShelfAlignment::kLeft ? -kNotificationBubbleGapHeight : 0,
        0);
  }

  gfx::Point cursor_position_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  // Cursor is invisible in tablet mode and plug in an external mouse in
  // tablet mode will switch to clamshell mode.
  if (shelf_region.Contains(cursor_position_in_screen) && !in_tablet_mode)
    return SHELF_AUTO_HIDE_SHOWN;

  // When the shelf is auto hidden and the shelf is on the boundary between
  // two displays, it is hard to trigger showing the shelf. For instance, if a
  // user's primary display is left of their secondary display, it is hard to
  // unautohide a left aligned shelf on the secondary display.
  // It is hard because:
  // - It is hard to stop the cursor in the shelf "light bar" and not
  // overshoot.
  // - The cursor is warped to the other display if the cursor gets to the
  // edge
  //   of the display.
  // Show the shelf if the cursor started on the shelf and the user overshot
  // the shelf slightly to make it easier to show the shelf in this situation.
  // We do not check |auto_hide_timer_|.IsRunning() because it returns false
  // when the timer's task is running.
  if ((state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN ||
       mouse_over_shelf_when_auto_hide_timer_started_) &&
      GetAutoHideShowShelfRegionInScreen().Contains(
          cursor_position_in_screen)) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  return base::nullopt;
}

bool ShelfLayoutManager::IsShelfWindow(aura::Window* window) {
  if (!window)
    return false;
  const aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  const aura::Window* navigation_window =
      shelf_->navigation_widget()->GetNativeWindow();
  const aura::Window* hotseat_window =
      shelf_->hotseat_widget()->GetNativeWindow();
  const aura::Window* status_area_window =
      shelf_widget_->status_area_widget()->GetNativeWindow();
  const aura::Window* drag_handle_nudge_window =
      GetDragHandleNudgeWindow(shelf_widget_);
  return (shelf_window && shelf_window->Contains(window)) ||
         (navigation_window && navigation_window->Contains(window)) ||
         (hotseat_window && hotseat_window->Contains(window)) ||
         (status_area_window && status_area_window->Contains(window)) ||
         (drag_handle_nudge_window &&
          drag_handle_nudge_window->Contains(window));
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
  if (chromeos::switches::ShouldShowShelfHotseat() &&
      Shell::Get()->IsInTabletMode() &&
      Shell::Get()->app_list_controller()->home_launcher_transition_state() !=
          AppListControllerImpl::HomeLauncherTransitionState::kFinished) {
    return 1.0f;
  }

  float opacity_when_visible = kDefaultShelfOpacity;
  if (dimmed_for_inactivity_) {
    opacity_when_visible =
        (GetShelfBackgroundType() == ShelfBackgroundType::kMaximized)
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
  if (!Shell::Get()->IsInTabletMode() &&
      Shell::Get()->app_list_controller()->GetTargetVisibility(display_.id())) {
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
      hotseat_state() == HotseatState::kShownHomeLauncher && scroll_y < 0;
  if (IsHotseatEnabled() && up_on_shown_hotseat) {
    return GetHomeLauncherGestureHandlerModeForDrag() ==
           HomeLauncherGestureHandler::Mode::kSwipeHomeToOverview;
  }

  if (IsHotseatEnabled()) {
    if (features::IsDragFromShelfToHomeOrOverviewEnabled() &&
        hotseat_state() != HotseatState::kShownHomeLauncher &&
        hotseat_state() != HotseatState::kNone) {
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
  // handled.
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

  if (Shell::Get()->app_list_controller()->IsVisible(display_.id()))
    return true;
  return StartShelfDrag(
      gesture_in_screen,
      gfx::Vector2dF(gesture_in_screen.details().scroll_x_hint(),
                     scroll_y_hint));
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
    StartShelfDrag(
        mouse_in_screen,
        gfx::Vector2dF(mouse_in_screen.x() - last_mouse_drag_position_.x(),
                       scroll_y_hint));
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
  if (Shell::Get()->IsInTabletMode() && event_in_screen.IsGestureEvent())
    return false;

  // Fullscreen app list can only be dragged from bottom alignment shelf.
  if (!shelf_->IsHorizontalAlignment())
    return false;

  // If the shelf is not visible, swiping up should show the shelf.
  if (!IsVisible())
    return false;

  // If the app list is already open, swiping up on the shelf should keep it
  // open.
  if (Shell::Get()->app_list_controller()->IsVisible(display_.id()))
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

bool ShelfLayoutManager::StartShelfDrag(const ui::LocatedEvent& event_in_screen,
                                        const gfx::Vector2dF& scroll_hint) {
  // Disable the shelf dragging if the fullscreen app list is opened.
  if (Shell::Get()->app_list_controller()->IsVisible(display_.id()) &&
      !Shell::Get()->IsInTabletMode())
    return false;

  drag_status_ = kDragInProgress;
  drag_auto_hide_state_ =
      (!Shell::Get()->overview_controller()->InOverviewSession() &&
       visibility_state() == SHELF_AUTO_HIDE)
          ? auto_hide_state()
          : SHELF_AUTO_HIDE_SHOWN;
  MaybeSetupHotseatDrag(event_in_screen);
  if (hotseat_is_in_drag_) {
    DCHECK(!hotseat_presentation_time_recorder_);
    hotseat_presentation_time_recorder_ =
        CreatePresentationTimeHistogramRecorder(
            shelf_->hotseat_widget()->GetCompositor(),
            "Ash.HotseatTransition.Drag.PresentationTime",
            "Ash.HotseatTransition.Drag.PresentationTime.MaxLatency");
  }
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);

  // For the hotseat, |drag_amount_| is relative to the top of the shelf.
  // To keep the hotseat from jumping to the top of the shelf on drag, set the
  // offset to the hotseats extended position.
  if (hotseat_state() == HotseatState::kExtended &&
      visibility_state() == SHELF_VISIBLE) {
    drag_amount_ = -(shelf_->hotseat_widget()->GetHotseatSize() +
                     ShelfConfig::Get()->hotseat_bottom_padding());
  } else {
    drag_amount_ = 0.f;
  }

  // If the start location is above the shelf (e.g., on the extended hotseat),
  // do not allow window drag when the hotseat is extended.
  const gfx::Rect shelf_bounds =
      shelf_->shelf_widget()->GetVisibleShelfBounds();
  allow_window_drag_on_extended_hotseat_ =
      event_in_screen.location_f().y() >= shelf_bounds.y();

  MaybeStartDragWindowFromShelf(event_in_screen, scroll_hint);
  return true;
}

void ShelfLayoutManager::MaybeSetupHotseatDrag(
    const ui::LocatedEvent& event_in_screen) {
  if (!IsHotseatEnabled())
    return;

  // Do not allow Hotseat dragging when the hotseat is shown within the shelf.
  if (hotseat_state() == HotseatState::kShownHomeLauncher ||
      hotseat_state() == HotseatState::kShownClamshell) {
    return;
  }

  if (hotseat_is_in_drag_)
    return;

  // Make sure hotseat is stacked above other shelf control windows when the
  // hotseat drag starts.
  shelf_->hotseat_widget()->StackAtTop();

  hotseat_is_in_drag_ = true;
}

void ShelfLayoutManager::UpdateDrag(const ui::LocatedEvent& event_in_screen,
                                    float scroll_x,
                                    float scroll_y) {
  const int starting_hotseat_y =
      shelf_->hotseat_widget()->GetTargetBounds().y();

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
    float y_position_in_root = std::min(event_in_screen.root_location_f().y(),
                                        static_cast<float>(shelf_bounds.y()));
    Shell::Get()->app_list_controller()->UpdateYPositionAndOpacity(
        y_position_in_root, GetAppListBackgroundOpacityOnShelfOpacity());
    launcher_above_shelf_bottom_amount_ =
        shelf_bounds.bottom() - event_in_screen.root_location().y();
  } else {
    if (drag_start_point_in_screen_ == gfx::Point())
      drag_start_point_in_screen_ = event_in_screen.location();
    drag_amount_ += shelf_->PrimaryAxisValue(scroll_y, scroll_x);
    if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
      last_drag_velocity_ =
          event_in_screen.AsGestureEvent()->details().velocity_y();
    }
    LayoutShelf();

    // Request a new hotseat presentation time record if the hotseat bounds
    // changed while the hotseat is in drag. If hotseat bounds remained the
    // same, there might be no changes to present, and the presentation time
    // recorder may end up recording inflated times when it's reset.
    if (hotseat_is_in_drag_ &&
        starting_hotseat_y != shelf_->hotseat_widget()->GetTargetBounds().y()) {
      DCHECK(hotseat_presentation_time_recorder_);
      hotseat_presentation_time_recorder_->RequestNext();
    }

    MaybeUpdateWindowDrag(event_in_screen, gfx::Vector2dF(scroll_x, scroll_y));
  }
}

void ShelfLayoutManager::CompleteDrag(const ui::LocatedEvent& event_in_screen) {
  // End the possible window drag before checking the shelf visibility.
  base::Optional<ShelfWindowDragResult> window_drag_result =
      MaybeEndWindowDrag(event_in_screen);
  HotseatState old_hotseat_state = hotseat_state();

  const bool transitioned_from_overview_to_home =
      MaybeEndDragFromOverviewToHome(event_in_screen);
  allow_fling_from_overview_to_home_ = false;

  // Fling from overview to home should be allowed only if window_drag_handler_
  // is not handling a window.
  DCHECK(!transitioned_from_overview_to_home ||
         !window_drag_result.has_value());

  if (ShouldChangeVisibilityAfterDrag(event_in_screen))
    CompleteDragWithChangedVisibility();
  else
    CancelDrag(window_drag_result);

  // Hotseat gestures are meaningful only in tablet mode with hotseat enabled.
  if (chromeos::switches::ShouldShowShelfHotseat() &&
      Shell::Get()->IsInTabletMode()) {
    base::Optional<InAppShelfGestures> gesture_to_record =
        CalculateHotseatGestureToRecord(window_drag_result,
                                        transitioned_from_overview_to_home,
                                        old_hotseat_state, hotseat_state());
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

void ShelfLayoutManager::CancelDrag(
    base::Optional<ShelfWindowDragResult> window_drag_result) {
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
    // Set |drag_status_| to kDragCancelInProgress to set the
    // auto hide state to |drag_auto_hide_state_|, which is the
    // visibility state before starting drag.
    drag_status_ = kDragCancelInProgress;
    UpdateVisibilityState();

    // Dragged window is finalized after drag handling is completed so drag
    // state does not interfere with updates on shelf state during window state
    // changes.
    if (window_drag_controller_.get()) {
      if (window_drag_result.has_value())
        window_drag_controller_->FinalizeDraggedWindow();
      else
        MaybeCancelWindowDrag();
    }
  }
  if (hotseat_is_in_drag_) {
    // If the gesture started the overview session, the hotseat will be
    // extended, but should not be marked as manually extended, as
    // extending the hotseat was not the primary goal of the gesture.
    shelf_->hotseat_widget()->set_manually_extended(
        hotseat_state() == HotseatState::kExtended &&
        (!Shell::Get()->overview_controller()->InOverviewSession() ||
         SplitViewController::Get(shelf_widget_->GetNativeWindow())
             ->InSplitViewMode() ||
         (window_drag_result.has_value() &&
          (window_drag_result.value() ==
               ShelfWindowDragResult::kRestoreToOriginalBounds ||
           window_drag_result.value() ==
               ShelfWindowDragResult::kGoToSplitviewMode))));

    hotseat_presentation_time_recorder_.reset();
  }
  hotseat_is_in_drag_ = false;
  drag_status_ = kDragNone;
  drag_start_point_in_screen_ = gfx::Point();
  last_drag_velocity_ = 0;
}

void ShelfLayoutManager::CompleteDragWithChangedVisibility() {
  shelf_widget_->Deactivate();
  shelf_widget_->status_area_widget()->Deactivate();

  drag_auto_hide_state_ = drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN
                              ? SHELF_AUTO_HIDE_HIDDEN
                              : SHELF_AUTO_HIDE_SHOWN;

  // Gesture drag will only change the auto hide state of the shelf but not the
  // auto hide behavior. Auto hide behavior can only be changed through the
  // context menu of the shelf. Set |drag_status_| to kDragCompleteInProgress to
  // set the auto hide state to |drag_auto_hide_state_|.
  drag_status_ = kDragCompleteInProgress;

  UpdateVisibilityState();

  // Dragged window is finalized after drag handling is completed so drag state
  // does not interfere with updates on shelf state during window state changes.
  if (window_drag_controller_.get())
    window_drag_controller_->FinalizeDraggedWindow();

  drag_status_ = kDragNone;
  if (hotseat_is_in_drag_)
    hotseat_presentation_time_recorder_.reset();
  hotseat_is_in_drag_ = false;
  last_drag_velocity_ = 0;
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
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
    case ShelfAlignment::kRight:
      if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_SHOWN)
        return drag_amount_ > 0;
      return drag_amount_ < 0;
    case ShelfAlignment::kLeft:
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

  if (Shell::Get()->overview_controller()->InOverviewSession())
    return false;

  if (event_in_screen.type() == ui::ET_GESTURE_SCROLL_END ||
      event_in_screen.type() == ui::ET_MOUSE_RELEASED) {
    // The visibility of the shelf changes only if the shelf was dragged X%
    // along the correct axis. If the shelf was already visible, then the
    // direction of the drag does not matter.
    const gfx::Rect bounds = GetIdealBounds();
    const float drag_ratio =
        fabs(drag_amount_) /
        (shelf_->IsHorizontalAlignment() ? bounds.height() : bounds.width());

    return IsSwipingCorrectDirection() &&
           drag_ratio > ShelfConfig::Get()->drag_hide_ratio_threshold();
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
    const gfx::Vector2dF& scroll) {
  if (!features::IsDragFromShelfToHomeOrOverviewEnabled())
    return false;
  if (!Shell::Get()->IsInTabletMode())
    return false;
  if (drag_status_ != kDragInProgress)
    return false;

  // Do not drag on a auto-hide hidden shelf or a hidden shelf.
  if ((visibility_state() == SHELF_AUTO_HIDE &&
       auto_hide_state() == SHELF_AUTO_HIDE_HIDDEN) ||
      visibility_state() == SHELF_HIDDEN) {
    return false;
  }

  // Do not drag on home screen.
  if (hotseat_state() == HotseatState::kShownHomeLauncher)
    return false;

  // If hotseat is hidden when drag starts, do not start drag window if hotseat
  // hasn't been fully dragged up.
  if (hotseat_state() == HotseatState::kHidden) {
    const int full_drag_amount =
        -shelf_->hotseat_widget()->GetHotseatFullDragAmount();
    if (drag_amount_ > full_drag_amount)
      return false;
  } else if (hotseat_state() == HotseatState::kExtended) {
    if (!allow_window_drag_on_extended_hotseat_)
      return false;

    // Do not start drag if it's a downward update event.
    if (scroll.y() >= 0)
      return false;
  }

  // Do not allow window drag if the previous dragged window is still animating.
  if (window_drag_controller_ &&
      window_drag_controller_->IsDraggedWindowAnimating()) {
    return false;
  }

  shelf_widget_->GetDragHandle()->SetWindowDragFromShelfInProgress(true);

  aura::Window* window =
      GetWindowForDragToHomeOrOverview(event_in_screen.location());
  allow_fling_from_overview_to_home_ = !window;
  if (!window)
    return false;

  window_drag_controller_ = std::make_unique<DragWindowFromShelfController>(
      window, event_in_screen.location_f());
  return true;
}

void ShelfLayoutManager::MaybeUpdateWindowDrag(
    const ui::LocatedEvent& event_in_screen,
    const gfx::Vector2dF& scroll) {
  if (!IsWindowDragInProgress() &&
      !MaybeStartDragWindowFromShelf(event_in_screen, scroll)) {
    return;
  }

  DCHECK_EQ(drag_status_, kDragInProgress);
  window_drag_controller_->Drag(event_in_screen.location_f(), scroll.x(),
                                scroll.y());
}

base::Optional<ShelfWindowDragResult> ShelfLayoutManager::MaybeEndWindowDrag(
    const ui::LocatedEvent& event_in_screen) {
  if (!IsWindowDragInProgress())
    return base::nullopt;

  shelf_widget_->GetDragHandle()->SetWindowDragFromShelfInProgress(false);

  DCHECK_EQ(drag_status_, kDragInProgress);
  base::Optional<float> velocity_y;
  if (event_in_screen.type() == ui::ET_SCROLL_FLING_START) {
    velocity_y = base::make_optional(
        event_in_screen.AsGestureEvent()->details().velocity_y());
  }

  return window_drag_controller_->EndDrag(event_in_screen.location_f(),
                                          velocity_y);
}

bool ShelfLayoutManager::MaybeEndDragFromOverviewToHome(
    const ui::LocatedEvent& event_in_screen) {
  if (!IsHotseatEnabled())
    return false;

  if (!allow_fling_from_overview_to_home_ ||
      !Shell::Get()->overview_controller()->InOverviewSession()) {
    return false;
  }

  shelf_widget_->GetDragHandle()->SetWindowDragFromShelfInProgress(false);

  if (event_in_screen.type() != ui::ET_SCROLL_FLING_START)
    return false;

  const float velocity_y =
      event_in_screen.AsGestureEvent()->details().velocity_y();
  if (velocity_y >
      -DragWindowFromShelfController::kVelocityToHomeScreenThreshold) {
    return false;
  }

  // If the drag started from hidden hotseat, check that the swipe length is
  // sufficiently longer than the amount needed to fling the hotseat up, to
  // reduce false positives when the user is pulling up the hotseat.
  if (hotseat_state() == HotseatState::kHidden) {
    const float kHotseatSizeMultiplier = 2;
    ShelfConfig* shelf_config = ShelfConfig::Get();
    const int drag_amount_threshold =
        -(shelf_config->shelf_size() + shelf_config->hotseat_bottom_padding() +
          kHotseatSizeMultiplier * shelf_->hotseat_widget()->GetHotseatSize());
    if (drag_amount_ > drag_amount_threshold)
      return false;
  }

  Shell::Get()->home_screen_controller()->GoHome(display_.id());
  return true;
}

void ShelfLayoutManager::MaybeCancelWindowDrag() {
  if (!IsWindowDragInProgress())
    return;

  DCHECK_EQ(drag_status_, kDragInProgress);
  shelf_widget_->GetDragHandle()->SetWindowDragFromShelfInProgress(false);
  window_drag_controller_->CancelDrag();
}

bool ShelfLayoutManager::IsWindowDragInProgress() const {
  return window_drag_controller_ && window_drag_controller_->drag_started();
}

void ShelfLayoutManager::UpdateVisibilityStateForSystemTrayChange(
    message_center::Visibility visibility) {
  base::Optional<base::AutoReset<bool>> reset;

  // Hides the hotseat when the hotseat is in kExtended mode and the system tray
  // shows.
  if (visibility == message_center::Visibility::VISIBILITY_MESSAGE_CENTER &&
      hotseat_state() == HotseatState::kExtended) {
    reset.emplace(&should_hide_hotseat_, true);
  }

  UpdateVisibilityState();
}

}  // namespace ash
