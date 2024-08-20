// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shelf/shelf_layout_manager.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/animation/animation_change_type.h"
#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_view.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/controls/contextual_tooltip.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/root_window_controller.h"
#include "ash/screen_util.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shelf/desk_button_widget.h"
#include "ash/shelf/drag_handle.h"
#include "ash/shelf/home_to_overview_nudge_controller.h"
#include "ash/shelf/hotseat_widget.h"
#include "ash/shelf/in_app_to_home_nudge_controller.h"
#include "ash/shelf/login_shelf_widget.h"
#include "ash/shelf/scrollable_shelf_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_metrics.h"
#include "ash/shelf/shelf_navigation_widget.h"
#include "ash/shelf/shelf_view.h"
#include "ash/shelf/shelf_widget.h"
#include "ash/shelf/swipe_home_to_overview_controller.h"
#include "ash/shell.h"
#include "ash/system/locale/locale_update_controller_impl.h"
#include "ash/system/status_area_widget.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/fullscreen_window_finder.h"
#include "ash/wm/lock_state_controller.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/work_area_insets.h"
#include "ash/wm/workspace/workspace_types.h"
#include "ash/wm/workspace_controller.h"
#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/single_thread_task_runner.h"
#include "components/prefs/pref_service.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/compositor/animation_throughput_reporter.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/android/gesture_event_type.h"
#include "ui/events/event.h"
#include "ui/events/event_handler.h"
#include "ui/events/gesture_event_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
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
// auto-hidden shelf. The region is used to make it easier to trigger showing
// the auto-hidden shelf when the shelf is on the boundary between displays.
constexpr int kMaxAutoHideShowShelfRegionSize = 10;

// Delay before showing the shelf. This is after the mouse stops moving.
constexpr int kShelfPalmRejectionSwipeOffset = 80;

// The minimum size of the area in the shelf where a user can perform the swipe
// gesture to show the bubble launcher in clamshell mode. The user is able to
// use a swipe upward gesture on screen or on the trackpad to show the bubble
// launcher. The area allowed to recognize the gesture will be between the home
// button and the first app in the shelf. When the scrollable shelf is full, we
// allow a minimum width for the shelf to recognize the gesture.
constexpr int kQuickShowMinAllowDistance = 100;

const constexpr char* const kStylusAppIds[] = {
    "fhapgmpiiiigioilnjmkiohjhlegnceb",  // Cursive/A4 Dogfood
    "apignacaigpffemhdbhmnajajaccbckh",  // Cursive/A4 Live
    "ieailfmhaghpphfffooibmlghaeopach",  // Canvas
    "eilembjdkfgodjkcjnpgpaenohkicgjd",  // Google Keep Web
    "ifeodkfobgahmoofeclbhkdacaaopkek",  // Google Keep ARC
    "gjcfgmjegppjhimhlldbhhkfgkdjngcc",  // Squid
    "afihfgfghkmdmggakhkgnfhlikhdpima",  // Infinite Painter
    "ffhnkanmhmmnfebldhpffiopadhbeimp"   // Goodnotes
};

// Minimum velocity required for the shelf to process a gesture fling event.
constexpr int kShelfFlingVelocityThresehold = 100;

// Returns the `aura::client::DragDropClient` for the given `shelf_widget`. Note
// that this may return `nullptr` if the browser is performing its shutdown
// sequence.
aura::client::DragDropClient* GetDragDropClient(ShelfWidget* shelf_widget) {
  if (shelf_widget) {
    if (aura::Window* window = shelf_widget->GetNativeWindow()) {
      if (aura::Window* root_window = window->GetRootWindow())
        return aura::client::GetDragDropClient(root_window);
    }
  }
  return nullptr;
}

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
  return parent && parent->GetId() == kShellWindowId_AppListContainer;
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

gfx::Insets CalculateShelfInsets(ShelfAlignment alignment,
                                 ShelfVisibilityState visibility_state) {
  const int default_shelf_inset =
      GetShelfInset(visibility_state, ShelfConfig::Get()->shelf_size());
  // In tablet mode, keep horizontal shelf inset at in-app shelf size
  // to avoid work area updates when the shelf size changes when going to and
  // from home screen (shelf size rules differ on home screen).
  const int horizontal_inset =
      Shell::Get()->IsInTabletMode()
          ? GetShelfInset(visibility_state,
                          ShelfConfig::Get()->in_app_shelf_size())
          : default_shelf_inset;
  return SelectValueByShelfAlignment(
      alignment, gfx::Insets::TLBR(0, 0, horizontal_inset, 0),
      gfx::Insets::TLBR(0, default_shelf_inset, 0, 0),
      gfx::Insets::TLBR(0, 0, 0, default_shelf_inset));
}

int GetOffset(int offset, const char* pref_name) {
  PrefService* prefs =
      Shell::Get()->session_controller()->GetLastActiveUserPrefService();
  return prefs->GetBoolean(pref_name) ? -offset : offset;
}

// Converts the offset to a relative value based on the allowed direction of
// the swipe for a visible/hidden app list.
int GetScrollOffsetInAllowedDirection(int offset, bool app_list_visible) {
  return app_list_visible ? -offset : offset;
}

// The floated window is the dragged window if it is not tucked, magnetized to
// the bottom, and above the event.
bool CanDragFloatedWindowFromShelf(aura::Window* floated_window,
                                   const gfx::Point& location_in_screen) {
  if (!Shell::Get()->float_controller()->IsFloatedWindowAlignedWithShelf(
          floated_window)) {
    return false;
  }
  DCHECK(floated_window->IsVisible());
  const gfx::Rect floated_window_bounds = floated_window->GetBoundsInScreen();
  return location_in_screen.x() <= floated_window_bounds.right() &&
         location_in_screen.x() >= floated_window_bounds.x();
}

// Checks if the only visible window is floated and not the dragged window, in
// which case we treat it as swipe home to overview.
bool IsDragOverShelfWithFloatedWindow(const gfx::Point& location_in_screen) {
  aura::Window* top_window = window_util::GetTopNonFloatedWindow();
  if (top_window && top_window->IsVisible()) {
    return false;
  }
  aura::Window* floated_window = window_util::GetFloatedWindowForActiveDesk();
  return floated_window &&
         !CanDragFloatedWindowFromShelf(floated_window, location_in_screen);
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
  if (mru_windows.empty()) {
    return nullptr;
  }

  SplitViewController* split_view_controller =
      SplitViewController::Get(Shell::GetPrimaryRootWindow());
  const bool is_in_splitview = split_view_controller->InSplitViewMode();
  // Cannot drag anything if in non splitview overview.
  if (!is_in_splitview &&
      Shell::Get()->overview_controller()->InOverviewSession()) {
    return nullptr;
  }

  if (aura::Window* floated_window =
          window_util::GetFloatedWindowForActiveDesk();
      floated_window &&
      CanDragFloatedWindowFromShelf(floated_window, location_in_screen)) {
    return floated_window;
  }

  // If split view mode is not active, use the first MRU window.
  if (!is_in_splitview) {
    aura::Window* window = window_util::GetTopNonFloatedWindow();
    return window && window->IsVisible() ? window : nullptr;
  }

  aura::Window* window = nullptr;
  // If split view mode is active, use the event location to decide which
  // window should be the dragged window.
  aura::Window* left_window = split_view_controller->primary_window();
  aura::Window* right_window = split_view_controller->secondary_window();
  const int divider_position = split_view_controller->GetDividerPosition();
  const bool is_landscape = IsCurrentScreenOrientationLandscape();
  const bool is_primary = IsCurrentScreenOrientationPrimary();
  const gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          split_view_controller->GetDefaultSnappedWindow());
  if (is_landscape) {
    if (location_in_screen.x() < work_area.x() + divider_position) {
      window = is_primary ? left_window : right_window;
    } else {
      window = is_primary ? right_window : left_window;
    }
  } else {
    window = is_primary ? right_window : left_window;
  }
  return window && window->IsVisible() ? window : nullptr;
}

// Calculates the type of hotseat gesture which should be recorded in histogram.
// Returns the null value if no gesture should be recorded.
std::optional<InAppShelfGestures> CalculateHotseatGestureToRecord(
    std::optional<ShelfWindowDragResult> window_drag_result,
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
    return std::nullopt;

  if (current_state == HotseatState::kHidden)
    return InAppShelfGestures::kSwipeDownToHide;

  if (current_state == HotseatState::kExtended)
    return InAppShelfGestures::kSwipeUpToShow;

  return std::nullopt;
}

bool IsInImmersiveFullscreen() {
  WindowState* active_window = WindowState::ForActiveWindow();
  return active_window && active_window->IsInImmersiveFullscreen();
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

  HotseatEventHandler(const HotseatEventHandler&) = delete;
  HotseatEventHandler& operator=(const HotseatEventHandler&) = delete;

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
  const raw_ptr<ShelfLayoutManager> shelf_layout_manager_;  // unowned.
};

}  // namespace

ShelfLayoutManager::State::State() = default;

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

bool ShelfLayoutManager::State::IsShelfAutoHiddenInSession() const {
  return in_session_visibility_state == SHELF_AUTO_HIDE &&
         in_session_auto_hide_state == SHELF_AUTO_HIDE_HIDDEN;
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

// ShelfLayoutManager::ScopedVisibilityLock ------------------------------------

ShelfLayoutManager::ScopedVisibilityLock::ScopedVisibilityLock(
    ShelfLayoutManager* shelf)
    : shelf_(shelf->weak_factory_.GetWeakPtr()) {
  ++shelf_->suspend_visibility_update_;
}

ShelfLayoutManager::ScopedVisibilityLock::~ScopedVisibilityLock() {
  if (!shelf_)
    return;
  --shelf_->suspend_visibility_update_;
  DCHECK_GE(shelf_->suspend_visibility_update_, 0);
  if (shelf_->suspend_visibility_update_ == 0)
    shelf_->UpdateVisibilityState(/*force_layout=*/false);
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
  shelf_->AddObserver(this);
  auto* shell = Shell::Get();
  shell->AddShellObserver(this);
  SplitViewController::Get(shelf_widget_->GetNativeWindow())->AddObserver(this);

  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    snap_group_controller->AddObserver(this);
  }

  ShelfConfig::Get()->AddObserver(this);
  shell->overview_controller()->AddObserver(this);
  shell->app_list_controller()->AddObserver(this);
  shell->lock_state_controller()->AddObserver(this);
  shell->activation_client()->AddObserver(this);
  shell->locale_update_controller()->AddObserver(this);
  state_.session_state = shell->session_controller()->GetSessionState();
  shelf_background_type_ = ComputeShelfBackgroundType();
  wallpaper_controller_observation_.Observe(shell->wallpaper_controller());

  // DesksController could be null when virtual desks feature is not enabled.
  if (DesksController* desks_controller = DesksController::Get()) {
    desks_controller->AddObserver(this);
  }
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

  if (SnapGroupController* snap_group_controller = SnapGroupController::Get()) {
    snap_group_controller->RemoveObserver(this);
  }

  shelf_->RemoveObserver(this);
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

void ShelfLayoutManager::UpdateShelfWorkAreaInsets() {
  if (suspend_work_area_update_ || in_shutdown_) {
    return;
  }

  auto* shelf_native_window = shelf_widget_->GetNativeWindow();
  if (!shelf_native_window) {
    return;
  }

  gfx::Insets shelf_insets =
      CalculateShelfInsets(shelf_->alignment(), visibility_state());

  gfx::Insets in_session_shelf_insets;
  // Shelf alignment will be updated after session state change, therefore we
  // need to check if it's `kBottomLocked` here. See bugs:
  //   https://crbug.com/173127
  //   https://crbug.com/1177572
  //   https://crbug.com/1344702
  //   https://crbug.com/1344718
  if (shelf_->alignment() == ShelfAlignment::kBottomLocked) {
    // If shelf is set to auto-hide, use empty insets so that application window
    // could use the right work area.
    if (shelf_->auto_hide_behavior() == ShelfAutoHideBehavior::kAlways ||
        shelf_->in_session_auto_hide_behavior() ==
            ShelfAutoHideBehavior::kAlways) {
      in_session_shelf_insets = gfx::Insets();
    } else {
      in_session_shelf_insets = CalculateShelfInsets(
          shelf_->in_session_alignment(), state_.in_session_visibility_state);
    }
  } else {
    in_session_shelf_insets = shelf_insets;
  }

  if (Shell::Get()->IsInTabletMode() && IsVisible()) {
    gfx::Rect shelf_bounds_for_workarea_calculation =
        GetIdealBoundsForWorkAreaCalculation();
    wm::ConvertRectToScreen(shelf_native_window->GetRootWindow(),
                            &shelf_bounds_for_workarea_calculation);

    UpdateWorkAreaInsetsAndNotifyObserversInternal(
        shelf_bounds_for_workarea_calculation, shelf_insets,
        in_session_shelf_insets);
  } else {
    UpdateWorkAreaInsetsAndNotifyObserversInternal(
        shelf_widget_->GetTargetBounds(), shelf_insets,
        in_session_shelf_insets);
  }
}

void ShelfLayoutManager::UpdateDisplayWorkArea() {
  if (suspend_work_area_update_ || in_shutdown_) {
    return;
  }

  auto* shelf_native_window = shelf_widget_->GetNativeWindow();
  if (!shelf_native_window) {
    return;
  }

  base::AutoReset scoped_update(&updating_work_area_, true);

  UpdateShelfWorkAreaInsets();

  display_ = display::Screen::GetScreen()->GetDisplayNearestWindow(
      shelf_native_window);
  const bool in_overview =
      Shell::Get()->overview_controller()->InOverviewSession();
  const bool in_splitview =
      SplitViewController::Get(shelf_native_window)->InSplitViewMode();
  const WorkAreaInsets* const work_area =
      WorkAreaInsets::ForWindow(shelf_native_window);
  const gfx::Insets user_work_area_insets = work_area->user_work_area_insets();
  if (state_.IsActiveSessionState()) {
    if (!in_overview && (shelf_->alignment() != ShelfAlignment::kBottomLocked ||
                         display_.work_area() == display_.bounds())) {
      gfx::Insets insets;
      // If user session is blocked (login to new user session or add user
      // to the existing session - multi-profile) then give 100% of work
      // area only if keyboard is not shown.
      // TODO(agawronska): Could this be called from WorkAreaInsets?
      if (!state_.IsAddingSecondaryUser() || work_area->IsKeyboardShown()) {
        insets = user_work_area_insets;
      }
      Shell::Get()
          ->window_tree_host_manager()
          ->UpdateWorkAreaOfDisplayNearestWindow(shelf_native_window, insets);
    } else if (in_overview && in_splitview) {
      // When in the split view with Overview enabled, the display work area
      // should be updated to guarantee snapped window has correct bounds.
      Shell::Get()
          ->window_tree_host_manager()
          ->UpdateWorkAreaOfDisplayNearestWindow(shelf_native_window,
                                                 user_work_area_insets);
    }
  } else {
    Shell::Get()
        ->window_tree_host_manager()
        ->UpdateWorkAreaOfDisplayNearestWindow(
            shelf_native_window, work_area->GetAccessibilityInsets());
  }
}

void ShelfLayoutManager::LayoutShelf(bool animate) {
  // Do not animate if the shelf container is animating.
  animate &= !IsShelfContainerAnimating();
  // The ShelfWidget may be partially closed (no native widget) during shutdown
  // or before it's been fully initialized so skip layout.
  if (in_shutdown_ || !shelf_widget_->native_widget())
    return;

  CalculateTargetBounds();
  UpdateBoundsAndOpacity(animate);
}

void ShelfLayoutManager::UpdateVisibilityState(bool force_layout) {
  // Bail out early after shelf is destroyed or visibility update is suspended.
  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  if (in_shutdown_ || !shelf_window || suspend_visibility_update_)
    return;

  SetState(CalculateShelfVisibility(), force_layout);

  const WorkspaceWindowState window_state =
      GetShelfWorkspaceWindowState(shelf_window);
  UpdateWorkspaceMask(window_state);
  SendA11yAlertForFullscreenWorkspaceState(window_state);
}

void ShelfLayoutManager::UpdateVisibilityStateForBackGesture() {
  base::AutoReset<bool> back_gesture(&state_forced_by_back_gesture_, true);
  SetState(SHELF_VISIBLE, /*force_layout=*/false);
}

void ShelfLayoutManager::UpdateAutoHideState() {
  ShelfAutoHideState auto_hide_state =
      CalculateAutoHideState(visibility_state());
  if (auto_hide_state != state_.auto_hide_state) {
    if (auto_hide_state == SHELF_AUTO_HIDE_HIDDEN) {
      UpdateVisibilityState(/*force_layout=*/false);
    } else {
      if (!auto_hide_timer_.IsRunning()) {
        mouse_over_shelf_when_auto_hide_timer_started_ =
            shelf_widget_->GetWindowBoundsInScreen().Contains(
                display::Screen::GetScreen()->GetCursorScreenPoint());
        drag_over_shelf_when_auto_hide_timer_started_ =
            in_drag_drop_ && shelf_widget_->GetWindowBoundsInScreen().Contains(
                                 last_drag_drop_position_in_screen_);
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
  in_mouse_drag_ =
      (event->type() == ui::EventType::kMouseDragged ||
       (in_mouse_drag_ && event->type() != ui::EventType::kMouseReleased &&
        event->type() != ui::EventType::kMouseCaptureChanged)) &&
      !IsShelfWindow(target) && !IsStatusAreaWindow(target);

  // Don't update during shutdown because synthetic mouse events (e.g. mouse
  // exit) may be generated during status area widget teardown.
  if (visibility_state() != SHELF_AUTO_HIDE || in_shutdown_)
    return;

  if (event->type() == ui::EventType::kMousePressed ||
      event->type() == ui::EventType::kMouseMoved) {
    if (shelf_->shelf_widget()->GetVisibleShelfBounds().Contains(
            display::Screen::GetScreen()->GetCursorScreenPoint())) {
      UpdateAutoHideState();
      last_seen_mouse_position_was_over_shelf_ = true;
    } else {
      // The event happened outside the shelf's bounds. If it's a click, hide
      // the shelf immediately. If it's a mouse-out, hide after a delay (but
      // only if it really is a mouse-out, meaning the mouse actually exited the
      // shelf bounds as opposed to having been outside all along).
      if (event->type() == ui::EventType::kMousePressed) {
        UpdateAutoHideState();
      } else if (last_seen_mouse_position_was_over_shelf_) {
        StartAutoHideTimer();
      }
      last_seen_mouse_position_was_over_shelf_ = false;
    }
  }
}

void ShelfLayoutManager::UpdateContextualNudges() {
  if (!features::IsHideShelfControlsInTabletModeEnabled()) {
    return;
  }

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
  // allowed by the current shelf state. Also, the nudge is disable in tast
  // tests to prevent waiting too long for the nudge animation.
  const bool allow_home_to_overview_nudge =
      in_tablet_mode && !in_app_shelf &&
      !ShelfConfig::Get()->shelf_controls_shown() && !in_overview_mode &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshNoNudges);
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
  if (!features::IsHideShelfControlsInTabletModeEnabled()) {
    return;
  }

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
        event->type() == ui::EventType::kGestureTap) {
      UpdateAutoHideState();
    }

    // When hotseat is hidden and in app shelf is shown, then a tap on the
    // shelf should hide it.
    if (is_shelf_window && !IsStatusAreaWindow(target) &&
        hotseat_state() == HotseatState::kHidden &&
        ShelfConfig::Get()->is_in_app()) {
      UpdateAutoHideState();
    }

    // Complete gesture drag when Shelf is visible in auto-hide mode. It is
    // called when swiping Shelf up to show.
    if (is_shelf_window && !IsStatusAreaWindow(target) &&
        visibility_state() == SHELF_AUTO_HIDE &&
        state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN &&
        event->type() == ui::EventType::kGestureEnd &&
        drag_status_ != kDragNone) {
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
  if (!Shell::Get()->IsInTabletMode())
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
      in_overview ? ui::EventType::kGestureTap : ui::EventType::kGestureBegin;

  // Record gesture metrics only for `interesting_type` to avoid over counting.
  if (event->type() == interesting_type) {
    UMA_HISTOGRAM_ENUMERATION(
        kHotseatGestureHistogramName,
        InAppShelfGestures::kHotseatHiddenDueToInteractionOutsideOfShelf);
  }

  UpdateVisibilityState(/*force_layout=*/false);
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

  if (event_in_screen.type() == ui::EventType::kGestureScrollBegin) {
    return StartGestureDrag(event_in_screen);
  }

  if (drag_status_ != kDragInProgress &&
      drag_status_ != kDragHomeToOverviewInProgress &&
      drag_status_ != kFlingBubbleLauncherInProgress) {
    return false;
  }

  // In certain edge cases, SHOW_PRESS gesture may come just as scroll starts.
  // Ignore it, as it's not actoinable, and should not cancel drag and drop.
  // See b/277846859 for more details.
  if (event_in_screen.type() == ui::EventType::kGestureShowPress) {
    return true;
  }

  if (event_in_screen.type() == ui::EventType::kGestureScrollUpdate) {
    UpdateGestureDrag(event_in_screen);
    return true;
  }

  if (event_in_screen.type() == ui::EventType::kScrollFlingStart) {
    if (MaybeHandleShelfFling(event_in_screen))
      return true;
  }

  if (event_in_screen.type() == ui::EventType::kGestureScrollEnd ||
      event_in_screen.type() == ui::EventType::kScrollFlingStart) {
    if (event_in_screen.type() == ui::EventType::kScrollFlingStart) {
      last_drag_velocity_ =
          event_in_screen.AsGestureEvent()->details().velocity_y();
    }
    if (drag_status_ == kDragHomeToOverviewInProgress) {
      CompleteDragHomeToOverview(event_in_screen);
    } else if (drag_status_ == kFlingBubbleLauncherInProgress) {
      CompleteShelfFling(event_in_screen);
    } else {
      CompleteDrag(event_in_screen);
    }
    return true;
  }

  // Unexpected event. Reset the state and let the event fall through.
  CancelDrag(std::nullopt);
  return false;
}

void ShelfLayoutManager::ProcessMouseEventFromShelf(
    const ui::MouseEvent& event_in_screen) {
  ui::EventType event_type = event_in_screen.type();
  DCHECK(event_type == ui::EventType::kMousePressed ||
         event_type == ui::EventType::kMouseDragged ||
         event_type == ui::EventType::kMouseReleased);

  if (!IsDragAllowed())
    return;

  switch (event_type) {
    case ui::EventType::kMousePressed:
      AttemptToDragByMouse(event_in_screen);
      break;
    case ui::EventType::kMouseDragged:
      UpdateMouseDrag(event_in_screen);
      return;
    case ui::EventType::kMouseReleased:
      ReleaseMouseDrag(event_in_screen);
      return;
    default:
      NOTREACHED();
  }
}

void ShelfLayoutManager::ProcessGestureEventFromShelfWidget(
    ui::GestureEvent* event_in_screen) {
  if (ProcessGestureEvent(*event_in_screen))
    event_in_screen->StopPropagation();
}

void ShelfLayoutManager::ProcessScrollOffset(int offset,
                                             const ui::LocatedEvent& event) {
  const int adjusted_offset = GetScrollOffsetInAllowedDirection(
      offset, Shell::Get()->app_list_controller()->IsVisible(display_.id()));
  if (adjusted_offset <=
      ShelfConfig::Get()->mousewheel_scroll_offset_threshold()) {
    return;
  }

  if (!IsLocationInBubbleLauncherShowBounds(event.root_location())) {
    return;
  }

  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(shelf_widget_->GetNativeWindow())
          .id(),
      AppListShowSource::kScrollFromShelf, event.time_stamp());
}

void ShelfLayoutManager::ProcessScrollEventFromShelf(ui::ScrollEvent* event) {
  if (shelf_->IsHorizontalAlignment()) {
    ProcessScrollOffset(GetOffset(event->y_offset(), prefs::kNaturalScroll),
                        *event);
  } else {
    int offset = shelf_->alignment() == ShelfAlignment::kLeft
                     ? -event->x_offset()
                     : event->x_offset();
    offset = GetOffset(offset, prefs::kNaturalScroll),
    ProcessScrollOffset(offset, *event);
  }
}

bool ShelfLayoutManager::IsBubbleLauncherShowOnGestureScrollAvailable() {
  if (!state_.IsShelfVisible())
    return false;

  if (Shell::Get()->IsInTabletMode())
    return false;

  return Shell::Get()->app_list_controller();
}

bool ShelfLayoutManager::MaybeHandleShelfFling(
    const ui::GestureEvent& event_in_screen) {
  if (!IsBubbleLauncherShowOnGestureScrollAvailable())
    return false;
  int velocity = shelf_->SelectValueForShelfAlignment(
      event_in_screen.AsGestureEvent()->details().velocity_y(),
      -event_in_screen.AsGestureEvent()->details().velocity_x(),
      event_in_screen.AsGestureEvent()->details().velocity_x());

  const int adjusted_velocity = GetScrollOffsetInAllowedDirection(
      velocity, Shell::Get()->app_list_controller()->IsVisible(display_.id()));

  if (adjusted_velocity > -kShelfFlingVelocityThresehold)
    return false;

  if (!IsLocationInBubbleLauncherShowBounds(drag_start_point_in_screen_))
    return false;

  Shell::Get()->app_list_controller()->ToggleAppList(
      display::Screen::GetScreen()
          ->GetDisplayNearestWindow(shelf_widget_->GetNativeWindow())
          .id(),
      AppListShowSource::kSwipeFromShelf, event_in_screen.time_stamp());

  return true;
}

bool ShelfLayoutManager::IsLocationInBubbleLauncherShowBounds(
    const gfx::Point& location_in_screen) {
  const gfx::Rect shelf_bounds_in_screen =
      shelf_->shelf_widget()->GetWindowBoundsInScreen();

  if (!shelf_bounds_in_screen.Contains(location_in_screen))
    return false;

  // Handle events that are close enough to the home button.
  const int distance_from_start = shelf_->PrimaryAxisValue(
      (base::i18n::IsRTL()
           ? shelf_bounds_in_screen.right() - location_in_screen.x()
           : location_in_screen.x() - shelf_bounds_in_screen.x()),
      location_in_screen.y());

  if (distance_from_start < kQuickShowMinAllowDistance)
    return true;

  // Don't handle swipes that would be outside app list bubble bounds.
  if (shelf_->IsHorizontalAlignment() &&
      distance_from_start >
          Shell::Get()->app_list_controller()->GetPreferredBubbleWidth(
              shelf_widget_->GetNativeWindow())) {
    return false;
  }

  // For events that fall between min and max distance for swipes, only handle
  // swipes that are outside hotseat bounds.
  ScrollableShelfView* scrollable_shelf_view =
      shelf_->hotseat_widget()->scrollable_shelf_view();
  gfx::Rect available_bounds_in_screen =
      scrollable_shelf_view->GetHotseatBackgroundBounds();
  views::View::ConvertRectToScreen(scrollable_shelf_view,
                                   &available_bounds_in_screen);

  // Only handle swipes that would fall on the mirrored left side of the
  // shelf before the hotseat.
  const int mirrored_left_for_hotseat =
      base::i18n::IsRTL()
          ? shelf_bounds_in_screen.right() - available_bounds_in_screen.right()
          : available_bounds_in_screen.x();
  return distance_from_start <
         shelf_->PrimaryAxisValue(mirrored_left_for_hotseat,
                                  available_bounds_in_screen.y());
}

void ShelfLayoutManager::ProcessMouseWheelEventFromShelf(
    ui::MouseWheelEvent* event) {
  const int y_offset =
      GetOffset(event->offset().y(), prefs::kMouseReverseScroll);
  ProcessScrollOffset(y_offset, *event);
}

ShelfBackgroundType ShelfLayoutManager::ComputeShelfBackgroundType() const {
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

  const aura::Window* shelf_native_window = shelf_widget_->GetNativeWindow();
  const bool in_split_view_mode =
      SplitViewController::Get(shelf_native_window)->InSplitViewMode();

  // Shelf defaults to rounded corners. We square them when a Snap Group is
  // created and fully visible. Maybe restore rounded corners on Snap Group
  // removal if no visible snap groups remain.
  SnapGroupController* snap_group_controller = SnapGroupController::Get();
  const bool has_visible_snap_group =
      snap_group_controller &&
      snap_group_controller->GetTopmostVisibleSnapGroup(
          shelf_native_window->GetRootWindow());
  const bool maximized =
      in_split_view_mode || has_visible_snap_group ||
      state_.window_state == WorkspaceWindowState::kFullscreen ||
      state_.window_state == WorkspaceWindowState::kMaximized;
  const bool in_overview = IsInOverviewSession();
  if (Shell::Get()->IsInTabletMode()) {
    const bool app_list_target_visibility =
        Shell::Get()->app_list_controller() &&
        Shell::Get()->app_list_controller()->GetTargetVisibility(display_.id());
    if (app_list_target_visibility) {
      // TODO(https://crbug.com/1058205): Test this behavior.
      // If the IME virtual keyboard is showing, the shelf should appear in-app.
      // The workspace area in tablet mode is always the in-app workspace area,
      // and the virtual keyboard places itself on screen based on workspace
      // area.
      if (ShelfConfig::Get()->is_virtual_keyboard_shown())
        return ShelfBackgroundType::kInApp;

      return ShelfBackgroundType::kHomeLauncher;
    }

    return in_overview ? ShelfBackgroundType::kOverview
                       : ShelfBackgroundType::kInApp;
  }

  if (maximized) {
    // When a window is maximized, if the auto-hide shelf is enabled and we are
    // in clamshell mode, the shelf will keep the default transparent
    // background.
    if (state_.visibility_state == SHELF_AUTO_HIDE)
      return ShelfBackgroundType::kDefaultBg;

    return ShelfBackgroundType::kMaximized;
  }

  if (in_overview)
    return ShelfBackgroundType::kOverview;

  return ShelfBackgroundType::kDefaultBg;
}

void ShelfLayoutManager::MaybeUpdateShelfBackground(AnimationChangeType type) {
  const ShelfBackgroundType new_background_type = ComputeShelfBackgroundType();
  if (new_background_type == shelf_background_type_) {
    return;
  }

  shelf_background_type_ = new_background_type;
  for (auto& observer : observers_)
    observer.OnBackgroundUpdated(shelf_background_type_, type);
}

bool ShelfLayoutManager::ShouldBlurShelfBackground() {
  if (!is_background_blur_enabled_)
    return false;

  return shelf_background_type_ == ShelfBackgroundType::kDefaultBg &&
         state_.session_state == session_manager::SessionState::ACTIVE;
}

bool ShelfLayoutManager::HasVisibleWindow() const {
  aura::Window* root = shelf_widget_->GetNativeWindow()->GetRootWindow();
  const aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal(
          kActiveDesk);
  // Process the window list and check if there are any visible windows.
  // Ignore app list windows that may be animating to hide after dismissal.
  for (aura::Window* window : windows) {
    if (window->IsVisible() && !IsAppListWindow(window) &&
        root->Contains(window)) {
      return true;
    }
  }
  return false;
}

void ShelfLayoutManager::CancelDragOnShelfIfInProgress() {
  if (drag_status_ == kDragInProgress ||
      drag_status_ == kDragHomeToOverviewInProgress) {
    CancelDrag(std::nullopt);
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
      UpdateVisibilityState(/*force_layout=*/false);
    } break;
  }
}

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
  UpdateDisplayWorkArea();
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

void ShelfLayoutManager::OnShelfAutoHideBehaviorChanged() {
  UpdateVisibilityState(/*force_layout=*/false);
}

void ShelfLayoutManager::OnUserWorkAreaInsetsChanged(
    aura::Window* root_window) {
  LayoutShelf();
  UpdateDisplayWorkArea();
}

void ShelfLayoutManager::OnPinnedStateChanged(aura::Window* pinned_window) {
  // Shelf needs to be hidden on entering to pinned mode, or restored
  // on exiting from pinned mode.
  UpdateVisibilityState(/*force_layout=*/false);
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

void ShelfLayoutManager::OnSnapGroupAdded(SnapGroup* snap_group) {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnSnapGroupRemoving(SnapGroup* snap_group,
                                             SnapGroupExitPoint exit_pint) {
  MaybeUpdateShelfBackground(AnimationChangeType::ANIMATE);
}

void ShelfLayoutManager::OnOverviewModeWillStart() {
  // If a shelf window is active before overview starts, deactivate it to avoid
  // overview window activation issues.
  // TODO(b/289287310): Consolidate behavior: shelf and overview.
  auto* active_window = window_util::GetActiveWindow();
  if (active_window && IsShelfWindow(active_window)) {
    wm::DeactivateWindow(active_window);
  }
  overview_mode_will_start_ = true;
}

void ShelfLayoutManager::OnOverviewModeStarting() {
  overview_mode_will_start_ = false;
  if (!overview_suspend_work_area_update_) {
    overview_suspend_work_area_update_.emplace(this);
  }
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
  // If `overview_suspend_work_area_update_` is already set because the previous
  // overview transition was canceled, `emplace()` will resume work area
  // updates, then immediately suspend it again. This is not only wasteful, but
  // causes transient shelf state updates from the temporary resume which is
  // confusing and results in test failures.
  if (!overview_suspend_work_area_update_) {
    overview_suspend_work_area_update_.emplace(this);
  }
}

void ShelfLayoutManager::OnOverviewModeEndingAnimationComplete(bool canceled) {
  // If transition is canceled, keep work area updates suspended, as new
  // overview transition is about to start.
  if (canceled)
    return;
  overview_suspend_work_area_update_.reset();
}

void ShelfLayoutManager::OnOverviewModeEnded() {
  UpdateVisibilityState(/*force_layout=*/false);
}

void ShelfLayoutManager::OnAppListVisibilityWillChange(bool shown,
                                                       int64_t display_id) {
  // We respond to "will change" and "did change" notifications in the same
  // way.
  OnAppListVisibilityChanged(shown, display_id);
}

void ShelfLayoutManager::OnAppListVisibilityChanged(bool shown,
                                                    int64_t display_id) {
  DVLOG(1) << __PRETTY_FUNCTION__ << " shown " << shown << " display "
           << display_id;
  // Shell may be under destruction.
  if (!shelf_widget_ || !shelf_widget_->GetNativeWindow())
    return;

  if (display_.id() != display_id)
    return;

  UpdateVisibilityState(/*force_layout=*/false);
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
}

void ShelfLayoutManager::OnWindowActivated(ActivationReason reason,
                                           aura::Window* gained_active,
                                           aura::Window* lost_active) {
  if (!IsShelfWindow(gained_active) &&
      !(window_drag_controller_ &&
        window_drag_controller_->during_window_restoration())) {
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

  // Animate shelf layout if the container is not animating.
  bool animate_background = !IsShelfContainerAnimating();
  MaybeUpdateShelfBackground(animate_background
                                 ? AnimationChangeType::ANIMATE
                                 : AnimationChangeType::IMMEDIATE);
  HideContextualNudges();
  {
    base::AutoReset<bool> immediate_transition(
        &state_change_animation_disabled_,
        !animate_background || state_.IsActiveSessionState() ||
            was_locked != state_.IsScreenLocked());
    UpdateShelfVisibilityAfterLoginUIChange();
  }
  if (was_adding_user == state_.IsAddingSecondaryUser()) {
    UpdateContextualNudges();
  }
}

void ShelfLayoutManager::OnLoginStatusChanged(LoginStatus loing_status) {
  UpdateVisibilityState(/*force_layout=*/false);
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
  if (updating_work_area_ || phase_ == ShelfLayoutPhase::kMoving ||
      changed_metrics == display::DisplayObserver::DISPLAY_METRIC_WORK_AREA) {
    return;
  }

  // Layout may be needed if the display arrangement has changed.
  LayoutShelf();

  // Update |user_work_area_bounds_| for the new display arrangement.
  UpdateShelfWorkAreaInsets();
}

void ShelfLayoutManager::OnLocaleChanged() {
  shelf_->login_shelf_widget()->HandleLocaleChange();
  shelf_->status_area_widget()->HandleLocaleChange();
  shelf_->navigation_widget()->HandleLocaleChange();
  if (features::IsDeskButtonEnabled()) {
    shelf_widget_->desk_button_widget()->HandleLocaleChange();
  }

  // Layout update is needed when language changes between LTR and RTL.
  LayoutShelf();
}

void ShelfLayoutManager::OnDeskSwitchAnimationLaunching() {
  ++suspend_visibility_update_;
}

void ShelfLayoutManager::OnDeskSwitchAnimationFinished() {
  --suspend_visibility_update_;
  DCHECK_GE(suspend_visibility_update_, 0);
  if (!suspend_visibility_update_) {
    // Force layout so the desk button will show after a desk switch from
    // overview.
    UpdateVisibilityState(/*force_layout=*/true);
  }
}

float ShelfLayoutManager::GetOpacity() const {
  return target_opacity_;
}

void ShelfLayoutManager::LockAutoHideState(bool lock_auto_hide_state) {
  if (is_auto_hide_state_locked_ == lock_auto_hide_state)
    return;
  is_auto_hide_state_locked_ = lock_auto_hide_state;
  // If unlocking, recompute the current state, but do it after the current
  // event is processed.
  if (!is_auto_hide_state_locked_) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ShelfLayoutManager::UpdateAutoHideState,
                                  weak_factory_.GetWeakPtr()));
  }
}

void ShelfLayoutManager::OnShelfConfigUpdated() {
  UpdateVisibilityState(/*force_layout=*/true);
  MaybeUpdateShelfBackground(AnimationChangeType::IMMEDIATE);
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

void ShelfLayoutManager::SuspendWorkAreaUpdate() {
  ++suspend_work_area_update_;
}

void ShelfLayoutManager::ResumeWorkAreaUpdate() {
  --suspend_work_area_update_;
  DCHECK_GE(suspend_work_area_update_, 0);

  if (suspend_work_area_update_ || in_shutdown_)
    return;

  UpdateVisibilityState(/*force_layout=*/true);
}

void ShelfLayoutManager::SetState(ShelfVisibilityState visibility_state,
                                  bool force_layout) {
  if (suspend_visibility_update_)
    return;

  State state;
  const HotseatState previous_hotseat_state = hotseat_state();
  state.visibility_state = visibility_state;
  state.auto_hide_state = CalculateAutoHideState(visibility_state);
  if (state_.IsActiveSessionState()) {
    state_.in_session_visibility_state = visibility_state;
    state_.in_session_auto_hide_state = state.auto_hide_state;
  }

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
  const bool force_update = force_layout ||
                            drag_status_ == kDragCancelInProgress ||
                            drag_status_ == kDragCompleteInProgress;

  if (!force_update && state_.Equals(state) &&
      previous_hotseat_state == new_hotseat_state) {
    return;  // Nothing changed.
  }

  if (visibility_state == SHELF_AUTO_HIDE &&
      state_.visibility_state != SHELF_AUTO_HIDE) {
    DCHECK(!drag_drop_observer_);
    // It's possible that the `drag_drop_client` might be `nullptr` if the
    // browser is performing its shutdown sequence.
    if (auto* drag_drop_client = GetDragDropClient(shelf_widget_)) {
      drag_drop_observer_ = std::make_unique<ScopedDragDropObserver>(
          drag_drop_client,
          /*event_callback=*/base::BindRepeating(
              &ShelfLayoutManager::UpdateAutoHideForDragDrop,
              base::Unretained(this)));
    }
  } else if (visibility_state != SHELF_AUTO_HIDE &&
             state_.visibility_state == SHELF_AUTO_HIDE) {
    drag_drop_observer_.reset();
  }

  StopAutoHideTimer();

  State old_state = state_;
  state_ = state;

  AnimationChangeType change_type = AnimationChangeType::ANIMATE;
  bool delay_background_change = false;

  // Do not animate the background when:
  // - Going from a hidden / auto-hidden shelf in fullscreen to a visible shelf
  //   in tablet mode.
  // - Going from an auto-hidden shelf in tablet mode to a visible shelf in
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

  CalculateTargetBounds();
  HotseatWidget* hotseat_widget = shelf_->hotseat_widget();
  hotseat_widget->SetState(new_hotseat_state);

  // Called before UpdateBoundsAndOpacity(). Because creation of the hotseat
  // bounds animation which is triggered by hotseat state update requires the
  // state transition type.
  HotseatWidget::ScopedInStateTransition scoped_in_state_transition(
      hotseat_widget, previous_hotseat_state, new_hotseat_state);

  UpdateBoundsAndOpacity(/*animate=*/!state_change_animation_disabled_);
  UpdateDisplayWorkArea();

  if (old_state.visibility_state != visibility_state) {
    for (auto& observer : observers_) {
      observer.OnShelfVisibilityStateChanged(*state_.visibility_state);
    }
  }

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
  if (!Shell::Get()->IsInTabletMode() || !shelf_->IsHorizontalAlignment()) {
    return HotseatState::kShownClamshell;
  }

  auto* app_list_controller = Shell::Get()->app_list_controller();
  // If the app list controller is null, we are probably in the middle of
  // a shutdown, let's not change the hotseat state.
  if (!app_list_controller) {
    return hotseat_state();
  }
  const auto* overview_controller = Shell::Get()->overview_controller();
  const bool in_overview =
      ((overview_controller && overview_controller->InOverviewSession()) ||
       overview_mode_will_start_) &&
      !overview_controller->IsCompletingShutdownAnimations();
  const bool app_list_target_visibility =
      app_list_controller->GetTargetVisibility(display_.id()) ||
      app_list_controller->ShouldHomeLauncherBeVisible();

  // TODO(https://crbug.com/1058205): Test this behavior.
  if (ShelfConfig::Get()->is_virtual_keyboard_shown()) {
    return HotseatState::kHidden;
  }

  // Only force to show if there is not a pending drag operation.
  if (shelf_widget_->IsHotseatForcedShowInTabletMode() &&
      drag_status_ == kDragNone) {
    return app_list_target_visibility ? HotseatState::kShownHomeLauncher
                                      : HotseatState::kExtended;
  }

  const int hotseat_size = shelf_->hotseat_widget()->GetHotseatSize();
  switch (drag_status_) {
    case kDragNone:
    case kDragHomeToOverviewInProgress: {
      if (app_list_target_visibility && !in_overview) {
        return HotseatState::kShownHomeLauncher;
      }
      // Show the hotseat if the shelf view's context menu is showing.
      if (shelf_->hotseat_widget()->IsShowingShelfMenu()) {
        return HotseatState::kExtended;
      }

      if (in_overview) {
        // Maintain the ShownHomeLauncher state if we enter overview mode
        // from it.
        if (hotseat_state() == HotseatState::kShownHomeLauncher) {
          return HotseatState::kShownHomeLauncher;
        }
        return HotseatState::kHidden;
      }

      if (state_forced_by_back_gesture_) {
        return HotseatState::kExtended;
      }

      if (visibility_state == SHELF_AUTO_HIDE) {
        return HotseatState::kHidden;
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
    case kDragCompleteInProgress:
      if (visibility_state == SHELF_AUTO_HIDE) {
        // When the shelf is auto hidden and the drag is being completed, the
        // auto-hide state has been finalized, so ensure the hotseat matches.
        DCHECK_EQ(drag_auto_hide_state_, auto_hide_state);
        return auto_hide_state == SHELF_AUTO_HIDE_SHOWN
                   ? HotseatState::kExtended
                   : HotseatState::kHidden;
      }
      [[fallthrough]];
    case kDragCancelInProgress: {
      // If the drag being completed is not a Hotseat drag, don't change the
      // state.
      if (!hotseat_is_in_drag_) {
        return hotseat_state();
      }

      if (app_list_target_visibility) {
        return HotseatState::kShownHomeLauncher;
      }

      if (in_overview && should_hide_hotseat_) {
        return HotseatState::kHidden;
      }

      if (shelf_->hotseat_widget()->IsExtended()) {
        return HotseatState::kExtended;
      }

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

      if (dragged_to_bezel) {
        return HotseatState::kHidden;
      }

      if (std::abs(last_drag_velocity_) >= 120) {
        if (last_drag_velocity_ > 0) {
          return HotseatState::kHidden;
        }
        return HotseatState::kExtended;
      }

      const int top_of_hotseat_to_screen_bottom =
          screen_bottom -
          shelf_->hotseat_widget()->GetWindowBoundsInScreen().y();
      const bool dragged_over_half_hotseat_size =
          top_of_hotseat_to_screen_bottom < hotseat_size / 2;
      if (dragged_over_half_hotseat_size) {
        return HotseatState::kHidden;
      }

      return HotseatState::kExtended;
    }
    default:
      // Do not change the hotseat state until the drag is complete or
      // canceled.
      return hotseat_state();
  }
  NOTREACHED();
}

ShelfVisibilityState ShelfLayoutManager::CalculateShelfVisibility() {
  if (shelf_->ShouldHideOnSecondaryDisplay(state_.session_state)) {
    // Needed to hide system tray on secondary display.
    return SHELF_HIDDEN;
  }

  if (!state_.IsActiveSessionState()) {
    // Needed to show system tray in non active session state.
    return SHELF_VISIBLE;
  }

  if (Shell::Get()->screen_pinning_controller()->IsPinned()) {
    return SHELF_HIDDEN;
  }

  if (Shell::Get()->session_controller()->IsRunningInAppMode()) {
    return SHELF_HIDDEN;
  }

  aura::Window* shelf_window = shelf_widget_->GetNativeWindow();
  const WorkspaceWindowState window_state =
      GetShelfWorkspaceWindowState(shelf_window);
  switch (window_state) {
    case WorkspaceWindowState::kFullscreen:
      if (IsShelfAutoHideForFullscreenMaximized()) {
        return SHELF_AUTO_HIDE;
      }
      if (IsShelfHiddenForFullscreen()) {
        return SHELF_HIDDEN;
      }
      // The shelf is sometimes not hidden when in immersive fullscreen.
      // Force the shelf to be auto hidden in this case.
      return SHELF_AUTO_HIDE;
    case WorkspaceWindowState::kMaximized:
      if (IsShelfAutoHideForFullscreenMaximized()) {
        return SHELF_AUTO_HIDE;
      }
      break;
    case WorkspaceWindowState::kDefault:
      break;
  }

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

  // We should not set the dim state if the shelf is hidden. Shelf will be
  // undimmed when it transitions into a visible state.
  if (!state_.IsShelfVisible())
    return false;

  // We do not want the auto-hide state to change while setting up animations.
  std::unique_ptr<Shelf::ScopedAutoHideLock> auto_hide_lock =
      std::make_unique<Shelf::ScopedAutoHideLock>(shelf_);

  dimmed_for_inactivity_ = dimmed;

  CalculateTargetBounds();

  const base::TimeDelta dim_animation_duration =
      ShelfConfig::Get()->DimAnimationDuration();
  const gfx::Tween::Type dim_animation_tween =
      ShelfConfig::Get()->DimAnimationTween();

  const bool animate = !dim_animation_duration.is_zero();
  std::optional<ui::AnimationThroughputReporter> navigation_widget_reporter;
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

  shelf_->login_shelf_widget()->SetLoginShelfButtonOpacity(target_opacity_);

  return true;
}

void ShelfLayoutManager::UpdateBoundsAndOpacity(bool animate) {
  DCHECK_EQ(phase_, ShelfLayoutPhase::kAiming) << " Aim before moving!";
  phase_ = ShelfLayoutPhase::kMoving;
  ShelfNavigationWidget* nav_widget = shelf_->navigation_widget();
  HotseatWidget* hotseat_widget = shelf_->hotseat_widget();
  StatusAreaWidget* status_widget = shelf_widget_->status_area_widget();
  // If the current shelf widget bounds is below the auto hidden bounds in the
  // auto hide state, set |animate| to false to prevent the shelf widget
  // animating upward and then disappearing with the opacity changes to 0
  // while hiding. See crbug.com/1203861.
  bool force_immediate_shelf_widget_transition = false;
  if (visibility_state() == SHELF_AUTO_HIDE &&
      auto_hide_state() == SHELF_AUTO_HIDE_HIDDEN && animate) {
    gfx::Rect current_shelf_bounds =
        shelf_->shelf_widget()->GetWindowBoundsInScreen();
    gfx::Rect shelf_target_bounds = shelf_->shelf_widget()->GetTargetBounds();
    force_immediate_shelf_widget_transition =
        shelf_->SelectValueForShelfAlignment(
            current_shelf_bounds.y() >= shelf_target_bounds.y(),
            current_shelf_bounds.right() <= shelf_target_bounds.right(),
            current_shelf_bounds.x() >= shelf_target_bounds.x());
  }
  shelf_->shelf_widget()->UpdateLayout(
      animate && !force_immediate_shelf_widget_transition);
  hotseat_widget->UpdateLayout(animate);
  status_widget->UpdateLayout(animate);
  nav_widget->UpdateLayout(animate);
  if (features::IsDeskButtonEnabled()) {
    shelf_widget_->desk_button_widget()->UpdateLayout(animate);
  }
  shelf_->login_shelf_widget()->UpdateLayout(animate);

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
  for (aura::Window* window : windows) {
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

void ShelfLayoutManager::UpdateTargetBounds(const State& state,
                                            HotseatState hotseat_target_state) {
  shelf_->shelf_widget()->CalculateTargetBounds();
  shelf_->status_area_widget()->CalculateTargetBounds();
  shelf_->navigation_widget()->CalculateTargetBounds();

  if (features::IsDeskButtonEnabled() &&
      shelf_->desk_button_widget()->ShouldReserveSpaceFromShelf()) {
    // If the desk button should be on the shelf, reserve space for it in the
    // hotseat before drawing the hotseat.
    CalculateDeskButtonAndHotseatTargetBounds();
  } else {
    // If no desk button widget, do not reserve any space.
    shelf_->hotseat_widget()->ReserveSpaceForAdjacentWidgets(gfx::Insets());
    shelf_->hotseat_widget()->CalculateTargetBounds();
  }

  shelf_->login_shelf_widget()->CalculateTargetBounds();

  target_opacity_ = ComputeTargetOpacity(state);

  if (drag_status_ == kDragInProgress)
    UpdateTargetBoundsForGesture(hotseat_target_state);
}

void ShelfLayoutManager::CalculateTargetBounds() {
  if (phase_ == ShelfLayoutPhase::kMoving)
    DVLOG(1) << "Careful when switching targets mid-move!";
  phase_ = ShelfLayoutPhase::kAiming;
  HotseatState hotseat_target_state =
      CalculateHotseatState(visibility_state(), auto_hide_state());
  UpdateTargetBounds(state_, hotseat_target_state);
}

void ShelfLayoutManager::UpdateWorkAreaInsetsAndNotifyObserversInternal(
    const gfx::Rect& shelf_bounds_for_workarea_calculation,
    const gfx::Insets& shelf_insets,
    const gfx::Insets& in_session_shelf_insets) {
  WorkAreaInsets::ForWindow(shelf_widget_->GetNativeWindow())
      ->SetShelfBoundsAndInsets(shelf_bounds_for_workarea_calculation,
                                shelf_insets, in_session_shelf_insets);
  for (auto& observer : observers_)
    observer.OnWorkAreaInsetsChanged();
}

void ShelfLayoutManager::UpdateWorkAreaInsetsAndNotifyObservers(
    const gfx::Rect& shelf_bounds_for_workarea_calculation,
    const gfx::Insets& shelf_insets,
    const gfx::Insets& in_session_shelf_insets) {
  UpdateWorkAreaInsetsAndNotifyObserversInternal(
      shelf_bounds_for_workarea_calculation, shelf_insets,
      in_session_shelf_insets);
  auto* shelf_native_window = shelf_widget_->GetNativeWindow();
  Shell::Get()
      ->window_tree_host_manager()
      ->UpdateWorkAreaOfDisplayNearestWindow(shelf_native_window, shelf_insets);
}

void ShelfLayoutManager::HandleScrollableShelfContainerBoundsChange() {
  // Update desk button widget layout with animation.
  DeskButtonWidget* desk_button = shelf_widget_->desk_button_widget();
  if (desk_button && desk_button->ShouldReserveSpaceFromShelf()) {
    CalculateDeskButtonAndHotseatTargetBounds();
    desk_button->UpdateLayout(/*animate=*/true);
  }
}

void ShelfLayoutManager::UpdateTargetBoundsForGesture(
    HotseatState hotseat_target_state) {
  // Home launcher style shelf should not be able to be pulled up.
  if (hotseat_target_state == HotseatState::kShownHomeLauncher)
    return;

  // TODO(crbug.com/40646496): Add tests for the hotseat bounds logic.
  CHECK_EQ(kDragInProgress, drag_status_);

  bool shelf_hidden_at_start = false;
  if (drag_auto_hide_state_ == SHELF_AUTO_HIDE_HIDDEN &&
      visibility_state() == SHELF_AUTO_HIDE &&
      auto_hide_state() != SHELF_AUTO_HIDE_SHOWN) {
    shelf_hidden_at_start = true;
  }
  const int shelf_size = ShelfConfig::Get()->shelf_size();
  const gfx::Rect available_bounds =
      screen_util::GetDisplayBoundsWithShelf(shelf_widget_->GetNativeWindow());
  const int baseline = shelf_->SelectValueForShelfAlignment(
      available_bounds.bottom() - (shelf_hidden_at_start ? 0 : shelf_size),
      available_bounds.x() - (shelf_hidden_at_start ? shelf_size : 0),
      available_bounds.right() - (shelf_hidden_at_start ? 0 : shelf_size));
  const int shelf_position = baseline + drag_amount_;

  if (!Shell::Get()->IsInTabletMode()) {
    // Do not allow the shelf to be dragged more than |shelf_size| from the
    // baseline.
    int shelf_ideal_bound = shelf_->SelectValueForShelfAlignment(
        available_bounds.bottom() - shelf_size, 0,
        available_bounds.right() - shelf_size);
    int adjusted_shelf_position = shelf_->SelectValueForShelfAlignment(
        std::max(shelf_ideal_bound, static_cast<int>(shelf_position)),
        std::min(shelf_ideal_bound, static_cast<int>(shelf_position)),
        std::max(shelf_ideal_bound, static_cast<int>(shelf_position)));
    shelf_->shelf_widget()->UpdateTargetBoundsForGesture(
        adjusted_shelf_position);
    shelf_->hotseat_widget()->UpdateTargetBoundsForGesture(
        adjusted_shelf_position);
    if (features::IsDeskButtonEnabled()) {
      shelf_->desk_button_widget()->UpdateTargetBoundsForGesture(
          adjusted_shelf_position);
    }
    shelf_->navigation_widget()->UpdateTargetBoundsForGesture(
        adjusted_shelf_position);
    shelf_->status_area_widget()->UpdateTargetBoundsForGesture(
        adjusted_shelf_position);
    return;
  }

  // Do not allow the shelf to be dragged more than |shelf_size| from the
  // bottom of the display.
  int adjusted_shelf_position = std::max(available_bounds.bottom() - shelf_size,
                                         static_cast<int>(shelf_position));
  const bool move_shelf_with_hotseat =
      !Shell::Get()->overview_controller()->InOverviewSession() &&
      visibility_state() == SHELF_AUTO_HIDE;
  if (move_shelf_with_hotseat) {
    // Window drags only happen after the hotseat has been dragged up to its
    // full height. After the drag moves a window, do not allow the drag to
    // move the hotseat down.
    if (IsWindowDragInProgress())
      adjusted_shelf_position = available_bounds.bottom() - shelf_size;
    gfx::Rect updated_target_bounds = shelf_->shelf_widget()->GetTargetBounds();
    updated_target_bounds.set_y(adjusted_shelf_position);
    shelf_->shelf_widget()->set_target_bounds(updated_target_bounds);
    shelf_->navigation_widget()->UpdateTargetBoundsForGesture(
        adjusted_shelf_position);
    shelf_->status_area_widget()->UpdateTargetBoundsForGesture(
        adjusted_shelf_position);
  }
  const int hotseat_extended_offset =
      shelf_->hotseat_widget()->GetHotseatSize() +
      Shell::Get()->shelf_config()->hotseat_bottom_padding();
  const int hotseat_offset_baseline =
      (hotseat_target_state == HotseatState::kExtended)
          ? -hotseat_extended_offset
          : shelf_size;
  bool use_hotseat_offset_baseline =
      (hotseat_target_state == HotseatState::kExtended &&
       visibility_state() == SHELF_AUTO_HIDE) ||
      (hotseat_target_state == HotseatState::kHidden &&
       visibility_state() != SHELF_AUTO_HIDE);
  int hotseat_offset =
      std::max(-hotseat_extended_offset,
               static_cast<int>(
                   (use_hotseat_offset_baseline ? hotseat_offset_baseline : 0) +
                   drag_amount_));
  // Window drags only happen after the hotseat has been dragged up to its
  // full height. After the drag moves a window, do not allow the drag to move
  // the hotseat down.
  if (IsWindowDragInProgress())
    hotseat_offset = -hotseat_extended_offset;
  gfx::Rect hotseat_bounds = shelf_->hotseat_widget()->GetTargetBounds();
  hotseat_bounds.set_y(hotseat_offset + adjusted_shelf_position);
  shelf_->hotseat_widget()->set_target_bounds(hotseat_bounds);
}

void ShelfLayoutManager::UpdateAutoHideForDragDrop(
    ScopedDragDropObserver::EventType event_type,
    const ui::DropTargetEvent* event) {
  DCHECK_EQ(visibility_state(), SHELF_AUTO_HIDE);
  if (event_type != ScopedDragDropObserver::EventType::kDragUpdated) {
    if (!in_mouse_drag_ && in_drag_drop_ &&
        shelf_->shelf_widget()->GetVisibleShelfBounds().Contains(
            last_drag_drop_position_in_screen_)) {
      // In this case the gesture drag and drop was ended so we need to
      // reset the |last_drag_drop_position_in_screen_| and call
      // UpdateAutoHideState() to check whether we need to hide the shelf.
      last_drag_drop_position_in_screen_ = gfx::Point(0, 0);
      UpdateAutoHideState();
    }
    // If this is a gesture drag, `in_mouse_drag_` will already be false here.
    // If this is a mouse drag, the mouse event handler will not receive a
    // MOUSE_RELEASED or MOUSE_CAPTURE_CHANGED event when the drag ends, so we
    // must manually reset `in_mouse_drag_` here.
    in_mouse_drag_ = false;
    in_drag_drop_ = false;
    return;
  }

  in_drag_drop_ = true;
  last_drag_drop_position_in_screen_ = event->root_location();
  ::wm::ConvertPointToScreen(shelf_->GetWindow()->GetRootWindow(),
                             &last_drag_drop_position_in_screen_);
  bool is_drag_over_shelf =
      shelf_->shelf_widget()->GetVisibleShelfBounds().Contains(
          last_drag_drop_position_in_screen_);

  if (is_drag_over_shelf != last_seen_drag_position_was_over_shelf_) {
    last_seen_drag_position_was_over_shelf_ = is_drag_over_shelf;
    if (is_drag_over_shelf) {
      // If a drag enters the shelf area, changes to shelf visibility
      // (i.e. potentially showing the shelf) should take place immediately.
      UpdateAutoHideState();
    } else {
      // If a drag exits the shelf area, the shelf should hide after a delay.
      StartAutoHideTimer();
    }
  }
}

void ShelfLayoutManager::UpdateAutoHideStateNow() {
  SetState(visibility_state(), /*force_layout=*/false);

  // If the state did not change, the auto-hide timer may still be running.
  StopAutoHideTimer();
}

void ShelfLayoutManager::StartAutoHideTimer() {
  auto_hide_timer_.Start(FROM_HERE, base::Milliseconds(kAutoHideDelayMS), this,
                         &ShelfLayoutManager::UpdateAutoHideStateNow);
}

void ShelfLayoutManager::StopAutoHideTimer() {
  auto_hide_timer_.Stop();
  mouse_over_shelf_when_auto_hide_timer_started_ = false;
  drag_over_shelf_when_auto_hide_timer_started_ = false;
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
  // TODO(crbug.com/40510582): Remove this when the login webui fake-shelf is
  // replaced with views.
  if (!Shell::Get()->session_controller()->IsActiveUserSessionStarted())
    return SHELF_AUTO_HIDE_HIDDEN;

  if (visibility_state != SHELF_AUTO_HIDE)
    return SHELF_AUTO_HIDE_HIDDEN;

  // Don't update the auto-hide state if it is locked.
  if (shelf_->auto_hide_lock())
    return state_.auto_hide_state;

  if (shelf_->disable_auto_hide()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  const bool in_tablet_mode = Shell::Get()->IsInTabletMode();
  // Don't let the shelf auto hide when in tablet mode and Chromevox is on.
  if (in_tablet_mode &&
      Shell::Get()->accessibility_controller()->spoken_feedback().enabled()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (auto* app_list_controller = Shell::Get()->app_list_controller();
      !in_tablet_mode &&
      app_list_controller->GetTargetVisibility(display_.id())) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->ShouldShowShelf()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (shelf_widget_->IsShowingMenu())
    return SHELF_AUTO_HIDE_SHOWN;

  if (shelf_widget_->IsActive() || shelf_->navigation_widget()->IsActive() ||
      shelf_->hotseat_widget()->IsActive() ||
      (shelf_widget_->status_area_widget() &&
       shelf_widget_->status_area_widget()->IsActive()) ||
      (shelf_->desk_button_widget() &&
       shelf_->desk_button_widget()->IsActive())) {
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

  if (drag_status_ == kDragHomeToOverviewInProgress) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  if (drag_status_ == kDragCompleteInProgress ||
      drag_status_ == kDragCancelInProgress ||
      drag_status_ == kFlingBubbleLauncherInProgress) {
    return drag_auto_hide_state_;
  }

  if (in_drag_drop_)
    return CalculateAutoHideStateBasedOnDragLocation();

  // Don't show if the user is dragging the mouse without a payload.
  if (in_mouse_drag_)
    return SHELF_AUTO_HIDE_HIDDEN;

  const auto auto_hide_state_from_cursor =
      CalculateAutoHideStateBasedOnCursorLocation();
  if (auto_hide_state_from_cursor.has_value())
    return *auto_hide_state_from_cursor;

  if (window_drag_controller_ &&
      window_drag_controller_->during_window_restoration()) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  return SHELF_AUTO_HIDE_HIDDEN;
}

ShelfAutoHideState
ShelfLayoutManager::CalculateAutoHideStateBasedOnDragLocation() const {
  gfx::Rect shelf_region = shelf_->shelf_widget()->GetVisibleShelfBounds();
  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->IsMessageBubbleShown() &&
      IsVisible()) {
    // Increase the the hit test area to prevent the shelf from disappearing
    // when the drag is over the bubble gap.
    ShelfAlignment alignment = shelf_->alignment();
    shelf_region.Inset(gfx::Insets::TLBR(
        shelf_->IsHorizontalAlignment() ? -kNotificationBubbleGapHeight : 0,
        alignment == ShelfAlignment::kRight ? -kNotificationBubbleGapHeight : 0,
        0,
        alignment == ShelfAlignment::kLeft ? -kNotificationBubbleGapHeight
                                           : 0));
  }

  if (shelf_region.Contains(last_drag_drop_position_in_screen_))
    return SHELF_AUTO_HIDE_SHOWN;

  // See `CalculateAutoHideStateBasedOnCursorLocation()` for an explanation of
  // this code, which makes it easier to show the shelf even if the user
  // overshoots.
  if ((state_.auto_hide_state == SHELF_AUTO_HIDE_SHOWN ||
       drag_over_shelf_when_auto_hide_timer_started_) &&
      GetAutoHideShowShelfRegionInScreen().Contains(
          last_drag_drop_position_in_screen_)) {
    return SHELF_AUTO_HIDE_SHOWN;
  }

  return SHELF_AUTO_HIDE_HIDDEN;
}

std::optional<ShelfAutoHideState>
ShelfLayoutManager::CalculateAutoHideStateBasedOnCursorLocation() const {
  // No mouse is available in tablet mode. So there is no point to calculate
  // the auto-hide state by the cursor location in this scenario.
  const bool in_tablet_mode = Shell::Get()->IsInTabletMode();
  if (in_tablet_mode)
    return std::nullopt;

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
    shelf_region.Inset(gfx::Insets::TLBR(
        shelf_->IsHorizontalAlignment() ? -kNotificationBubbleGapHeight : 0,
        alignment == ShelfAlignment::kRight ? -kNotificationBubbleGapHeight : 0,
        0,
        alignment == ShelfAlignment::kLeft ? -kNotificationBubbleGapHeight
                                           : 0));
  }

  gfx::Point cursor_position_in_screen =
      display::Screen::GetScreen()->GetCursorScreenPoint();
  // Cursor is invisible in tablet mode and plug in an external mouse in
  // tablet mode will switch to clamshell mode.
  if (shelf_region.Contains(cursor_position_in_screen) && !in_tablet_mode)
    return SHELF_AUTO_HIDE_SHOWN;

  // When the shelf is auto-hidden and the shelf is on the boundary between
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

  return std::nullopt;
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

  // Calculate whether `window` is contained by the login shelf widget.
  const aura::Window* login_shelf_window =
      shelf_->login_shelf_widget()->GetNativeWindow();
  bool window_in_login_shelf_widget =
      (login_shelf_window && login_shelf_window->Contains(window));

  // Calculate whether `window` is contained by the desk button widget.
  bool window_in_desk_button_widget = false;
  if (features::IsDeskButtonEnabled()) {
    const aura::Window* desk_button_window =
        shelf_->desk_button_widget()->GetNativeWindow();
    window_in_desk_button_widget =
        (desk_button_window && desk_button_window->Contains(window));
  }

  return (shelf_window && shelf_window->Contains(window)) ||
         (navigation_window && navigation_window->Contains(window)) ||
         (hotseat_window && hotseat_window->Contains(window)) ||
         (status_area_window && status_area_window->Contains(window)) ||
         (drag_handle_nudge_window &&
          drag_handle_nudge_window->Contains(window)) ||
         window_in_login_shelf_widget || window_in_desk_button_widget;
}

bool ShelfLayoutManager::IsStatusAreaWindow(aura::Window* window) {
  if (!window)
    return false;
  const aura::Window* status_window =
      shelf_widget_->status_area_widget()->GetNativeWindow();
  return status_window && status_window->Contains(window);
}

void ShelfLayoutManager::UpdateShelfVisibilityAfterLoginUIChange() {
  UpdateVisibilityState(/*force_layout=*/true);
}

float ShelfLayoutManager::ComputeTargetOpacity(const State& state) const {
  if (Shell::Get()->IsInTabletMode()) {
    // The shelf should not become transparent during the animation to
    // HomeLauncher.
    auto* app_list_controller = Shell::Get()->app_list_controller();
    if (app_list_controller->GetTargetVisibility(display_.id()) &&
        !app_list_controller->IsVisible(display_.id())) {
      return 1.0f;
    }
  }

  float opacity_when_visible = kDefaultShelfOpacity;
  if (dimmed_for_inactivity_) {
    opacity_when_visible =
        (ComputeShelfBackgroundType() == ShelfBackgroundType::kMaximized)
            ? kMaximizedShelfDimOpacity
            : kFloatingShelfDimOpacity;
  }

  if (drag_status_ == kDragInProgress ||
      state.visibility_state == SHELF_VISIBLE) {
    return opacity_when_visible;
  }

  // In Chrome OS Material Design, when shelf is hidden during auto-hide state,
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

  // The shelf should be shown when any bubble in the status area is shown.
  if (shelf_widget_->status_area_widget() &&
      shelf_widget_->status_area_widget()->ShouldShowShelf()) {
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

bool ShelfLayoutManager::IsActiveWindowStylusApp() const {
  WindowState* active_window_state = WindowState::ForActiveWindow();
  if (!active_window_state)
    return false;

  aura::Window* active_window = active_window_state->window();

  if (!active_window || active_window->GetRootWindow() !=
                            shelf_widget_->GetNativeWindow()->GetRootWindow()) {
    return false;
  }

  const ShelfID active_id =
      ShelfID::Deserialize(active_window->GetProperty(kShelfIDKey));

  if (active_id.IsNull())
    return false;

  return base::Contains(kStylusAppIds, active_id.app_id);
}

////////////////////////////////////////////////////////////////////////////////
// Gesture drag related functions:
bool ShelfLayoutManager::StartGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  if (drag_status_ != kDragNone)
    return false;

  float scroll_y_hint = gesture_in_screen.details().scroll_y_hint();

  // In tablet mode, let swipe_home_to_overview_controller handle swipe up
  // gestures on the home launcher screen.
  if (Shell::Get()->IsInTabletMode() &&
      ((Shell::Get()->app_list_controller()->IsVisible(display_.id()) &&
        Shell::Get()->app_list_controller()->GetTargetVisibility(
            display_.id())) ||
       IsDragOverShelfWithFloatedWindow(gesture_in_screen.location())) &&
      scroll_y_hint < 0) {
    drag_status_ = kDragHomeToOverviewInProgress;
    swipe_home_to_overview_controller_ =
        std::make_unique<SwipeHomeToOverviewController>(display_.id());
    return true;
  }

  if (Shell::Get()->IsInTabletMode() &&
      Shell::Get()->app_list_controller()->IsVisible(display_.id())) {
    return true;
  }

  if (StartShelfDrag(gesture_in_screen,
                     gfx::Vector2dF(gesture_in_screen.details().scroll_x_hint(),
                                    scroll_y_hint))) {
    return true;
  }

  if (IsBubbleLauncherShowOnGestureScrollAvailable()) {
    drag_status_ = kFlingBubbleLauncherInProgress;
    return true;
  }

  return false;
}

void ShelfLayoutManager::UpdateGestureDrag(
    const ui::GestureEvent& gesture_in_screen) {
  float scroll_x = gesture_in_screen.details().scroll_x();
  float scroll_y = gesture_in_screen.details().scroll_y();

  if (drag_status_ == kDragHomeToOverviewInProgress) {
    DCHECK(swipe_home_to_overview_controller_);
    swipe_home_to_overview_controller_->Drag(
        gesture_in_screen.details().bounding_box_f().CenterPoint(), scroll_x,
        scroll_y);
    return;
  }

  if (drag_status_ == kFlingBubbleLauncherInProgress) {
    if (drag_start_point_in_screen_ == gfx::Point())
      drag_start_point_in_screen_ = gesture_in_screen.location();
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
  last_mouse_drag_position_in_screen_ = mouse_in_screen.location();
}

void ShelfLayoutManager::StartMouseDrag(const ui::MouseEvent& mouse_in_screen) {
  float scroll_y_hint =
      mouse_in_screen.y() - last_mouse_drag_position_in_screen_.y();
  StartShelfDrag(mouse_in_screen,
                 gfx::Vector2dF(mouse_in_screen.x() -
                                    last_mouse_drag_position_in_screen_.x(),
                                scroll_y_hint));
}

void ShelfLayoutManager::UpdateMouseDrag(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ == kDragNone)
    return;

  DCHECK(drag_status_ == kDragAttempt || drag_status_ == kDragInProgress ||
         drag_status_ == kDragHomeToOverviewInProgress);

  if (drag_status_ == kDragAttempt) {
    // Do not start drag for the small offset.
    if (abs(mouse_in_screen.location().y() -
            last_mouse_drag_position_in_screen_.y()) < kMouseDragThreshold) {
      return;
    }

    // Mouse events do not provide the drag offset like gesture events. So
    // start mouse drag when mouse is moved.
    StartMouseDrag(mouse_in_screen);
  } else {
    int scroll_x = mouse_in_screen.location().x() -
                   last_mouse_drag_position_in_screen_.x();
    int scroll_y = mouse_in_screen.location().y() -
                   last_mouse_drag_position_in_screen_.y();
    UpdateDrag(mouse_in_screen, scroll_x, scroll_y);
    last_mouse_drag_position_in_screen_ = mouse_in_screen.location();
  }
}

void ShelfLayoutManager::ReleaseMouseDrag(
    const ui::MouseEvent& mouse_in_screen) {
  if (drag_status_ == kDragNone)
    return;

  DCHECK(drag_status_ == kDragAttempt ||
         drag_status_ == kDragHomeToOverviewInProgress ||
         drag_status_ == kDragInProgress);

  switch (drag_status_) {
    case kDragAttempt:
      drag_status_ = kDragNone;
      break;
    case kDragHomeToOverviewInProgress:
      CompleteDragHomeToOverview(mouse_in_screen);
      break;
    case kDragInProgress:
      CompleteDrag(mouse_in_screen);
      break;
    default:
      NOTREACHED();
  }
  last_mouse_drag_position_in_screen_ = gfx::Point();
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

bool ShelfLayoutManager::StartShelfDrag(const ui::LocatedEvent& event_in_screen,
                                        const gfx::Vector2dF& scroll_hint) {
  const bool is_tablet_mode = Shell::Get()->IsInTabletMode();
  // Disable the shelf dragging if the fullscreen app list is opened.
  if (Shell::Get()->app_list_controller()->IsVisible(display_.id()) &&
      !is_tablet_mode) {
    return false;
  }

  // Clamshell launcher does not support shelf drags unless autohide
  // is enabled or the shelf is autohidden for immersive fullscreen.
  if (!is_tablet_mode &&
      shelf_->auto_hide_behavior() != ShelfAutoHideBehavior::kAlways &&
      !IsInImmersiveFullscreen()) {
    return false;
  }

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
    // For tablet mode, we allow a certain offset between the drag event offset
    // and the hotseat location to avoid accidentally extendeing the hotseat in
    // certain conditions.
    drag_amount_ = is_tablet_mode && IsActiveWindowStylusApp()
                       ? kShelfPalmRejectionSwipeOffset
                       : 0;
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
  if (!Shell::Get()->IsInTabletMode())
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
  if (drag_start_point_in_screen_ == gfx::Point())
    drag_start_point_in_screen_ = event_in_screen.location();
  drag_amount_ += shelf_->PrimaryAxisValue(scroll_y, scroll_x);
  if (event_in_screen.type() == ui::EventType::kScrollFlingStart) {
    last_drag_velocity_ =
        event_in_screen.AsGestureEvent()->details().velocity_y();
  }

  // Prevent unnecessary layout and paint work when handling window drag from
  // shelf.
  if (!IsWindowDragInProgress()) {
    LayoutShelf();
  }

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

void ShelfLayoutManager::CompleteDrag(const ui::LocatedEvent& event_in_screen) {
  // End the possible window drag before checking the shelf visibility.
  std::optional<ShelfWindowDragResult> window_drag_result =
      MaybeEndWindowDrag(event_in_screen);
  HotseatState old_hotseat_state = hotseat_state();

  const bool transitioned_from_overview_to_home =
      MaybeEndDragFromOverviewToHome(event_in_screen);
  allow_fling_from_overview_to_home_ = true;

  // Fling from overview to home should be allowed only if window_drag_handler_
  // is not handling a window.
  DCHECK(!transitioned_from_overview_to_home ||
         !window_drag_result.has_value());

  std::optional<base::AutoReset<bool>> reset;
  if (window_drag_result &&
      (*window_drag_result == ShelfWindowDragResult::kGoToOverviewMode ||
       *window_drag_result == ShelfWindowDragResult::kGoToSplitviewMode)) {
    reset.emplace(&should_hide_hotseat_, true);
  }

  if (ShouldChangeVisibilityAfterDrag(event_in_screen))
    CompleteDragWithChangedVisibility();
  else
    CancelDrag(window_drag_result);

  // Hotseat gestures are only meaningful in tablet mode.
  if (Shell::Get()->IsInTabletMode()) {
    std::optional<InAppShelfGestures> gesture_to_record =
        CalculateHotseatGestureToRecord(window_drag_result,
                                        transitioned_from_overview_to_home,
                                        old_hotseat_state, hotseat_state());
    if (gesture_to_record.has_value()) {
      UMA_HISTOGRAM_ENUMERATION(kHotseatGestureHistogramName,
                                gesture_to_record.value());
    }
  }
}

void ShelfLayoutManager::CompleteShelfFling(
    const ui::LocatedEvent& event_in_screen) {
  drag_status_ = kDragNone;
  drag_start_point_in_screen_ = gfx::Point();
  last_drag_velocity_ = 0;
}

void ShelfLayoutManager::CompleteDragHomeToOverview(
    const ui::LocatedEvent& event_in_screen) {
  std::optional<float> velocity_y;
  if (event_in_screen.type() == ui::EventType::kScrollFlingStart) {
    velocity_y = std::make_optional(
        event_in_screen.AsGestureEvent()->details().velocity_y());
  }
  DCHECK(swipe_home_to_overview_controller_);
  swipe_home_to_overview_controller_->EndDrag(event_in_screen.location_f(),
                                              velocity_y);
  swipe_home_to_overview_controller_.reset();
  drag_status_ = kDragNone;
}

void ShelfLayoutManager::CancelDrag(
    std::optional<ShelfWindowDragResult> window_drag_result) {
  if (drag_status_ == kDragHomeToOverviewInProgress) {
    swipe_home_to_overview_controller_->CancelDrag();
    swipe_home_to_overview_controller_.reset();
  } else {
    // Set |drag_status_| to kDragCancelInProgress to set the
    // auto-hide state to |drag_auto_hide_state_|, which is the
    // visibility state before starting drag.
    drag_status_ = kDragCancelInProgress;
    UpdateVisibilityState(/*force_layout=*/false);

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

  // Gesture drag will only change the auto-hide state of the shelf but not the
  // auto-hide behavior. Auto-hide behavior can only be changed through the
  // context menu of the shelf. Set |drag_status_| to kDragCompleteInProgress to
  // set the auto-hide state to |drag_auto_hide_state_|.
  drag_status_ = kDragCompleteInProgress;

  UpdateVisibilityState(/*force_layout=*/false);

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

  if (event_in_screen.type() == ui::EventType::kGestureScrollEnd ||
      event_in_screen.type() == ui::EventType::kMouseReleased) {
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

  if (event_in_screen.type() == ui::EventType::kScrollFlingStart) {
    return IsSwipingCorrectDirection();
  }

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
  if (!Shell::Get()->IsInTabletMode())
    return false;
  if (drag_status_ != kDragInProgress)
    return false;

  // Do not drag on an auto-hidden shelf or a hidden shelf.
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

std::optional<ShelfWindowDragResult> ShelfLayoutManager::MaybeEndWindowDrag(
    const ui::LocatedEvent& event_in_screen) {
  if (!IsWindowDragInProgress())
    return std::nullopt;

  shelf_widget_->GetDragHandle()->SetWindowDragFromShelfInProgress(false);

  DCHECK_EQ(drag_status_, kDragInProgress);
  std::optional<float> velocity_y;
  if (event_in_screen.type() == ui::EventType::kScrollFlingStart) {
    velocity_y = std::make_optional(
        event_in_screen.AsGestureEvent()->details().velocity_y());
  }

  return window_drag_controller_->EndDrag(event_in_screen.location_f(),
                                          velocity_y);
}

bool ShelfLayoutManager::MaybeEndDragFromOverviewToHome(
    const ui::LocatedEvent& event_in_screen) {
  if (!Shell::Get()->IsInTabletMode())
    return false;

  if (!allow_fling_from_overview_to_home_ ||
      !Shell::Get()->overview_controller()->InOverviewSession()) {
    return false;
  }

  shelf_widget_->GetDragHandle()->SetWindowDragFromShelfInProgress(false);

  if (event_in_screen.type() != ui::EventType::kScrollFlingStart) {
    return false;
  }

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

  Shell::Get()->app_list_controller()->GoHome(display_.id());
  return true;
}

void ShelfLayoutManager::MaybeCancelWindowDrag() {
  if (!IsWindowDragInProgress())
    return;

  DCHECK(drag_status_ == kDragInProgress ||
         drag_status_ == kDragCancelInProgress);
  shelf_widget_->GetDragHandle()->SetWindowDragFromShelfInProgress(false);
  window_drag_controller_->CancelDrag();
}

bool ShelfLayoutManager::IsWindowDragInProgress() const {
  return window_drag_controller_ && window_drag_controller_->drag_started();
}

void ShelfLayoutManager::UpdateVisibilityStateForTrayBubbleChange(
    bool bubble_shown) {
  std::optional<base::AutoReset<bool>> reset;

  // Hides the hotseat when the hotseat is in kExtended mode and the system tray
  // shows.
  if (bubble_shown && hotseat_state() == HotseatState::kExtended) {
    reset.emplace(&should_hide_hotseat_, true);
  }

  UpdateVisibilityState(/*force_layout=*/false);
}

void ShelfLayoutManager::HandleShelfAlignmentChange() {
  base::AutoReset<bool> immediate_transition(&state_change_animation_disabled_,
                                             true);

  // The desk button widget needs to know that the alignment is changing early
  // so that it can calculate the correct preferred length.
  if (features::IsDeskButtonEnabled()) {
    shelf_->desk_button_widget()->PrepareForAlignmentChange();
  }

  UpdateVisibilityState(/*force_layout=*/true);
}

void ShelfLayoutManager::OnShelfTrayBubbleVisibilityChanged(bool bubble_shown) {
  // Uses base::CancelableOnceClosure to handle two edge cases: (1)
  // ShelfLayoutManager is destructed before the callback runs. (2) The previous
  // callback is still pending.
  visibility_update_for_tray_callback_.Reset(base::BindOnce(
      &ShelfLayoutManager::UpdateVisibilityStateForTrayBubbleChange,
      base::Unretained(this), /*bubble_shown=*/bubble_shown));

  // OnShelfTrayBubbleVisibilityChanged is called when the visibility of a
  // status area tray bubble is set, which is before the tray bubble is
  // created/destructed. Meanwhile, we rely on the state of tray bubble to
  // calculate the auto-hide state.
  // Use SingleThreadTaskRunner::CurrentDefaultHandle to specify that the task
  // runs on the UI thread.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, visibility_update_for_tray_callback_.callback());
}

bool ShelfLayoutManager::IsShelfContainerAnimating() const {
  // TODO(oshima): We're re-layouting during shelf construction. We probably
  // should wait and then layout once after shelf is fully constructed.
  if (!shelf_widget_ || !shelf_widget_->native_widget_private() ||
      !shelf_widget_->GetNativeWindow()) {
    return false;
  }
  return shelf_widget_->GetNativeWindow()
      ->parent()
      ->layer()
      ->GetAnimator()
      ->is_animating();
}

void ShelfLayoutManager::CalculateDeskButtonAndHotseatTargetBounds() {
  CHECK(features::IsDeskButtonEnabled() && shelf_->desk_button_widget() &&
        shelf_->desk_button_widget()->ShouldReserveSpaceFromShelf());

  auto reserve_space_for_desk_button_widget = [](Shelf* shelf) {
    const int length_needed =
        DeskButtonWidget::GetMaxLength(shelf->IsHorizontalAlignment());
    shelf->hotseat_widget()->ReserveSpaceForAdjacentWidgets(
        shelf->IsHorizontalAlignment()
            ? (base::i18n::IsRTL() ? gfx::Insets::TLBR(0, 0, 0, length_needed)
                                   : gfx::Insets::TLBR(0, length_needed, 0, 0))
            : gfx::Insets::TLBR(length_needed, 0, 0, 0));
  };

  // First reserve space for the desk button widget and calculate target bounds
  // of the hotseat widget.
  reserve_space_for_desk_button_widget(shelf_);
  shelf_->hotseat_widget()->CalculateTargetBounds();

  // Then calculate target bounds of the desk button widget.
  shelf_->desk_button_widget()->CalculateTargetBounds();
}

}  // namespace ash
