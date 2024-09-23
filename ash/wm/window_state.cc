// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include <absl/cleanup/cleanup.h>

#include <optional>
#include <utility>

#include "ash/accessibility/accessibility_controller.h"
#include "ash/focus_cycler.h"
#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/app_types_util.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/default_state.h"
#include "ash/wm/float/float_controller.h"
#include "ash/wm/pip/pip_controller.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_restore/window_restore_controller.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/types/cxx23_to_underlying.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/frame/caption_buttons/snap_controller.h"
#include "chromeos/ui/frame/frame_utils.h"
#include "chromeos/ui/frame/multitask_menu/multitask_menu_metrics.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/screen.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/ime_util_chromeos.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// The threshold for us to judge if drag to maximize behavior is mis-triggered.
// If a window is dragged to maximized and remains maximized longer than this
// threshold, then drag to maximize behavior is not mis-triggered, otherwise it
// will be counted as one mis-trigger.
constexpr base::TimeDelta kDragToMaximizeMisTriggerThreshold = base::Seconds(5);

constexpr char kDragToMaximizeMisTriggersHistogramName[] =
    "Ash.Window.DragMaximized.NumberOfMisTriggers";
constexpr char kValidDragMaximizedHistogramName[] =
    "Ash.Window.DragMaximized.Valid";

using ::chromeos::kHideShelfWhenFullscreenKey;
using ::chromeos::kImmersiveIsActive;
using ::chromeos::kPartialSplitDurationHistogramName;
using ::chromeos::kWindowManagerManagesOpacityKey;
using ::chromeos::WindowStateType;

// This defines the map from different window states to their restore layers.
// The assumption is that a window state with higher restore layer number can
// restore back to a window state with lower restore layer number, but not the
// other way around. For example, a window whose window state is kMinimized can
// restore to kMaximized window state, but kMaximized window state can not
// restore back to kMinimized window state. Please see
// go/window-state-restore-history for details.
// Note the map does not contain all WindowStateTypes, for the ones that's not
// in the map, they can't be put into the window state restore history stack,
// and restore from those state will simply go back to kNormal window state.
constexpr auto kWindowStateRestoreHistoryLayerMap =
    base::MakeFixedFlatMap<WindowStateType, int>({
        {WindowStateType::kNormal, 0},
        {WindowStateType::kDefault, 0},
        {WindowStateType::kPrimarySnapped, 1},
        {WindowStateType::kSecondarySnapped, 1},
        {WindowStateType::kMaximized, 2},
        {WindowStateType::kFullscreen, 3},
        {WindowStateType::kFloated, 3},
        {WindowStateType::kPip, 4},
        {WindowStateType::kMinimized, 4},
    });

// Whether the window state type is valid for putting in the restore history.
bool IsValidForRestoreHistory(WindowStateType state_type) {
  return kWindowStateRestoreHistoryLayerMap.contains(state_type);
}

// Returns true if |current_state| can restore back to |previous_state|.
// Normally, a state can only restore back to another state at a lower level.
bool CanRestoreState(WindowStateType current_state,
                     WindowStateType previous_state) {
  if (!IsValidForRestoreHistory(current_state) ||
      !IsValidForRestoreHistory(previous_state)) {
    return false;
  }

  if (kWindowStateRestoreHistoryLayerMap.at(current_state) >
      kWindowStateRestoreHistoryLayerMap.at(previous_state)) {
    return true;
  }

  return false;
}

bool IsTabletModeEnabled() {
  return display::Screen::GetScreen()->InTabletMode();
}

bool IsToplevelContainer(aura::Window* window) {
  DCHECK(window);
  int container_id = window->GetId();
  // ArcVirtualKeyboard is implemented as a exo window which requires
  // WindowState to manage its state.
  return IsActivatableShellWindowId(container_id) ||
         container_id == kShellWindowId_ArcVirtualKeyboardContainer;
}

// ARC windows will not be in a top level container until they are associated
// with a task. We still want a WindowState created for these windows and their
// transient children as they will be moved to a top level container soon.
bool IsTemporarilyHiddenForFullrestore(aura::Window* window) {
  if (window->GetProperty(app_restore::kParentToHiddenContainerKey))
    return true;

  auto* transient_parent = wm::GetTransientParent(window);
  return transient_parent && transient_parent->GetProperty(
                                 app_restore::kParentToHiddenContainerKey);
}

bool IsPartial(float snap_ratio) {
  return cc::MathUtil::IsWithinEpsilon(snap_ratio,
                                       chromeos::kOneThirdSnapRatio) ||
         cc::MathUtil::IsWithinEpsilon(snap_ratio,
                                       chromeos::kTwoThirdSnapRatio);
}

// A tentative class to set the bounds on the window.
// TODO(oshima): Once all logic is cleaned up, move this to the real layout
// manager with proper friendship.
class BoundsSetter : public aura::LayoutManager {
 public:
  BoundsSetter() = default;

  BoundsSetter(const BoundsSetter&) = delete;
  BoundsSetter& operator=(const BoundsSetter&) = delete;

  ~BoundsSetter() override = default;

  // aura::LayoutManager overrides:
  void OnWindowResized() override {}
  void OnWindowAddedToLayout(aura::Window* child) override {}
  void OnWillRemoveWindowFromLayout(aura::Window* child) override {}
  void OnWindowRemovedFromLayout(aura::Window* child) override {}
  void OnChildWindowVisibilityChanged(aura::Window* child,
                                      bool visible) override {}
  void SetChildBounds(aura::Window* child,
                      const gfx::Rect& requested_bounds) override {}

  void SetBounds(aura::Window* window, const gfx::Rect& bounds) {
    if (window->GetTargetBounds() != bounds)
      SetChildBoundsDirect(window, bounds);
  }
};

WMEventType WMEventTypeFromShowState(
    ui::mojom::WindowShowState requested_show_state) {
  switch (requested_show_state) {
    case ui::mojom::WindowShowState::kDefault:
    case ui::mojom::WindowShowState::kNormal:
      return WM_EVENT_NORMAL;
    case ui::mojom::WindowShowState::kMinimized:
      return WM_EVENT_MINIMIZE;
    case ui::mojom::WindowShowState::kMaximized:
      return WM_EVENT_MAXIMIZE;
    case ui::mojom::WindowShowState::kFullscreen:
      return WM_EVENT_FULLSCREEN;
    case ui::mojom::WindowShowState::kInactive:
      return WM_EVENT_SHOW_INACTIVE;

    case ui::mojom::WindowShowState::kEnd:
      NOTREACHED() << "No WMEvent defined for the show state:"
                   << requested_show_state;
  }
  return WM_EVENT_NORMAL;
}

// Returns the snap ratio for the given `window` and `snap_event`.
// - In tablet mode, window will snap to the prefixed snap ratios and some
// adjustments will be made to account for window minimum size if needed. See
// `SplitViewController::FindClosestPositionRatio()` for more details;
// - In clamshell mode, window can be snapped with an arbitrary snap ratio and
// we need to consider the window minimum size and adjust the window snap ratio
// before committing the snap event if needed.
float GetTargetSnapRatio(aura::Window* window,
                         const WindowSnapWMEvent* snap_event) {
  if (Shell::Get()->IsInTabletMode()) {
    return snap_event->snap_ratio();
  }

  const gfx::Rect work_area(
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          window->GetRootWindow()));
  const bool is_horizontal = IsLayoutHorizontal(window);
  const float window_minimum_length =
      GetMinimumWindowLength(window, is_horizontal);
  const float snap_ratio = snap_event->snap_ratio();
  return std::max(window_minimum_length /
                      (is_horizontal ? work_area.width() : work_area.height()),
                  snap_ratio);
}

// This applies after the wm event has been applied and window bounds have been
// modified.
float AdjustCurrentSnapRatio(aura::Window* window,
                             const gfx::Rect& target_bounds) {
  gfx::Rect maximized_bounds =
      screen_util::GetMaximizedWindowBoundsInParent(window);
  const int divider_delta =
      ShouldConsiderDivider(window) ? kSplitviewDividerShortSideLength / 2 : 0;
  if (IsLayoutHorizontal(window)) {
    return static_cast<float>(target_bounds.width() + divider_delta) /
           maximized_bounds.width();
  }
  return static_cast<float>(target_bounds.height() + divider_delta) /
         maximized_bounds.height();
}

// Move all transient children to |dst_root|, including the ones in the child
// windows and transient children of the transient children.
void MoveAllTransientChildrenToNewRoot(aura::Window* window) {
  aura::Window* dst_root = window->GetRootWindow();
  for (aura::Window* transient_child : wm::GetTransientChildren(window)) {
    if (!transient_child->parent())
      continue;
    const int container_id = transient_child->parent()->GetId();
    DCHECK_GE(container_id, 0);
    aura::Window* container = dst_root->GetChildById(container_id);
    if (container->Contains(transient_child))
      continue;
    gfx::Rect child_bounds = transient_child->bounds();
    wm::ConvertRectToScreen(dst_root, &child_bounds);
    container->AddChild(transient_child);
    transient_child->SetBoundsInScreen(
        child_bounds,
        display::Screen::GetScreen()->GetDisplayNearestWindow(window));

    // Transient children may have transient children.
    MoveAllTransientChildrenToNewRoot(transient_child);
  }
  // Move transient children of the child windows if any.
  for (aura::Window* child : window->children())
    MoveAllTransientChildrenToNewRoot(child);
}

void ReportAshPipEvents(AshPipEvents event) {
  UMA_HISTOGRAM_ENUMERATION(kAshPipEventsHistogramName, event);
}

void ReportAshPipAndroidPipUseTime(base::TimeDelta duration) {
  UMA_HISTOGRAM_CUSTOM_TIMES(kAshPipAndroidPipUseTimeHistogramName, duration,
                             base::Seconds(1), base::Hours(10), 50);
}

// Notifies the window restore controller to write to file.
void SaveWindowForWindowRestore(WindowState* window_state) {
  if (auto* controller = WindowRestoreController::Get())
    controller->SaveWindow(window_state);
}

bool ShouldSetExplicitOpaqueRegionsForOcclusion(WindowState* window_state) {
  // If the window manager manages the window opacity, set the opaque regions
  // explicitly if the window must be transparent (e.g. has rounded corners).
  return chromeos::ShouldWindowStateHaveRoundedCorners(
             window_state->GetStateType()) &&
         window_state->window()->GetProperty(
             ash::kWindowManagerManagesOpacityKey);
}

}  // namespace

constexpr base::TimeDelta WindowState::kBoundsChangeSlideDuration;

WindowState::ScopedBoundsChangeAnimation::ScopedBoundsChangeAnimation(
    aura::Window* window,
    BoundsChangeAnimationType bounds_animation_type)
    : window_(window) {
  window_->AddObserver(this);
  previous_bounds_animation_type_ =
      WindowState::Get(window_)->bounds_animation_type_;
  WindowState::Get(window_)->bounds_animation_type_ = bounds_animation_type;
}

WindowState::ScopedBoundsChangeAnimation::~ScopedBoundsChangeAnimation() {
  if (window_) {
    WindowState::Get(window_)->bounds_animation_type_ =
        previous_bounds_animation_type_;
    window_->RemoveObserver(this);
    window_ = nullptr;
  }
}

void WindowState::ScopedBoundsChangeAnimation::OnWindowDestroying(
    aura::Window* window) {
  window_->RemoveObserver(this);
  window_ = nullptr;
}

WindowState::~WindowState() {
  // WindowState is registered as an owned property of |window_|, and window
  // unregisters all of its observers in its d'tor before destroying its
  // properties. As a result, window_->RemoveObserver() doesn't need to (and
  // shouldn't) be called here.

  // Records the number of mis-triggers of drag to maximize behavior if
  // `window_` has been dragged to maximized during its lifetime.
  if (has_ever_been_dragged_to_maximized_) {
    base::UmaHistogramCounts100(kDragToMaximizeMisTriggersHistogramName,
                                num_of_drag_to_maximize_mis_triggers_);
  }
}

bool WindowState::HasDelegate() const {
  return !!delegate_;
}

void WindowState::SetDelegate(std::unique_ptr<WindowStateDelegate> delegate) {
  DCHECK((!delegate_.get() && !!delegate.get()) ||
         (!!delegate_.get() && !delegate.get()));
  delegate_ = std::move(delegate);
}

void WindowState::CreatePersistentWindowInfo(
    bool was_landscape_before_rotation,
    const gfx::Rect& restore_bounds_in_parent,
    bool for_display_removal) {
  if (for_display_removal) {
    CHECK(!persistent_window_info_of_display_removal_);
    persistent_window_info_of_display_removal_ =
        std::make_unique<PersistentWindowInfo>(
            window_, was_landscape_before_rotation, restore_bounds_in_parent);
    return;
  }
  CHECK(!persistent_window_info_of_screen_rotation_);
  persistent_window_info_of_screen_rotation_ =
      std::make_unique<PersistentWindowInfo>(
          window_, was_landscape_before_rotation, restore_bounds_in_parent);
}

WindowStateType WindowState::GetStateType() const {
  return current_state_->GetType();
}

bool WindowState::IsMinimized() const {
  return IsMinimizedWindowStateType(GetStateType());
}

bool WindowState::IsMaximized() const {
  return GetStateType() == WindowStateType::kMaximized;
}

bool WindowState::IsFullscreen() const {
  return GetStateType() == WindowStateType::kFullscreen;
}

bool WindowState::IsMaximizedOrFullscreenOrPinned() const {
  return IsMaximizedOrFullscreenOrPinnedWindowStateType(GetStateType());
}

bool WindowState::IsSnapped() const {
  return GetStateType() == WindowStateType::kPrimarySnapped ||
         GetStateType() == WindowStateType::kSecondarySnapped;
}

bool WindowState::IsPinned() const {
  return GetStateType() == WindowStateType::kPinned ||
         GetStateType() == WindowStateType::kTrustedPinned;
}

bool WindowState::IsTrustedPinned() const {
  return GetStateType() == WindowStateType::kTrustedPinned;
}

bool WindowState::IsPip() const {
  return GetStateType() == WindowStateType::kPip;
}

bool WindowState::IsFloated() const {
  return GetStateType() == WindowStateType::kFloated;
}

int64_t WindowState::GetFullscreenTargetDisplayId() const {
  return window_->GetProperty(aura::client::kFullscreenTargetDisplayIdKey);
}

bool WindowState::IsNormalStateType() const {
  return IsNormalWindowStateType(GetStateType());
}

bool WindowState::IsNormalOrSnapped() const {
  return IsNormalStateType() || IsSnapped();
}

bool WindowState::IsVerticalOrHorizontalMaximized() const {
  return IsNormalStateType() && HasRestoreBounds();
}

bool WindowState::IsNonVerticalOrHorizontalMaximizedNormalState() const {
  return IsNormalStateType() && !HasRestoreBounds();
}

bool WindowState::IsActive() const {
  return wm::IsActiveWindow(window_);
}

bool WindowState::IsUserPositionable() const {
  return window_util::IsWindowUserPositionable(window_);
}

bool WindowState::CanFullscreen() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          aura::client::kResizeBehaviorCanFullscreen) != 0;
}

bool WindowState::CanMaximize() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          aura::client::kResizeBehaviorCanMaximize) != 0;
}

bool WindowState::CanMinimize() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          aura::client::kResizeBehaviorCanMinimize) != 0;
}

bool WindowState::CanResize() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          aura::client::kResizeBehaviorCanResize) != 0;
}

bool WindowState::CanActivate() const {
  return wm::CanActivateWindow(window_);
}

bool WindowState::CanSnap() {
  return CanSnapOnDisplay(GetDisplay());
}

bool WindowState::CanSnapOnDisplay(display::Display display) const {
  const bool can_resize = CanResize();
  const bool can_resizable_snap = !IsPip() && can_resize && CanMaximize();
  return can_resizable_snap ||
         (!can_resize && CanUnresizableSnapOnDisplay(display));
}

bool WindowState::HasRestoreBounds() const {
  gfx::Rect* bounds = window_->GetProperty(aura::client::kRestoreBoundsKey);
  return bounds != nullptr && !bounds->IsEmpty();
}

void WindowState::Maximize() {
  wm::SetWindowState(window_, ui::mojom::WindowShowState::kMaximized);
}

void WindowState::Minimize() {
  wm::SetWindowState(window_, ui::mojom::WindowShowState::kMinimized);
}

void WindowState::Unminimize() {
  wm::Unminimize(window_);
}

void WindowState::Activate() {
  wm::ActivateWindow(window_);
}

void WindowState::Deactivate() {
  wm::DeactivateWindow(window_);
}

void WindowState::Restore() {
  const WMEvent event(WM_EVENT_RESTORE);
  OnWMEvent(&event);
}

bool WindowState::IsRestoring(WindowStateType previous_state) const {
  return CanRestoreState(previous_state, GetStateType());
}

void WindowState::DisableZOrdering(aura::Window* window_on_top) {
  ui::ZOrderLevel z_order = GetZOrdering();
  if (z_order != ui::ZOrderLevel::kNormal && !IsPip()) {
    // |window_| is hidden first to avoid canceling fullscreen mode when it is
    // no longer always on top and gets added to default container. This avoids
    // sending redundant OnFullscreenStateChanged to the layout manager. The
    // |window_| visibility is restored after it no longer obscures the
    // |window_on_top|.
    bool visible = window_->IsVisible();
    if (visible)
      window_->Hide();
    window_->SetProperty(aura::client::kZOrderingKey, ui::ZOrderLevel::kNormal);
    // Technically it is possible that a |window_| could make itself
    // always_on_top really quickly. This is probably not a realistic case but
    // check if the two windows are in the same container just in case.
    if (window_on_top && window_on_top->parent() == window_->parent())
      window_->parent()->StackChildAbove(window_on_top, window_);
    if (visible)
      window_->Show();
    cached_z_order_ = z_order;
  }
}

void WindowState::RestoreZOrdering() {
  if (cached_z_order_ != ui::ZOrderLevel::kNormal) {
    window_->SetProperty(aura::client::kZOrderingKey, cached_z_order_);
    cached_z_order_ = ui::ZOrderLevel::kNormal;
  }
}

void WindowState::OnWMEvent(const WMEvent* event) {
  // A float/unfloat may trigger another event. If that's the case, we don't
  // want to handle the nested event and let the original event take care of
  // things.
  if (is_handling_float_event_) {
    return;
  }

  std::unique_ptr<base::AutoReset<bool>> snap_event_lock;
  if (const WindowSnapWMEvent* snap_event = event->AsSnapEvent()) {
    snap_event_lock =
        std::make_unique<base::AutoReset<bool>>(&is_handling_snap_event_, true);
    // Save `event` requested snap ratio.
    const float target_snap_ratio = GetTargetSnapRatio(window_, snap_event);
    snap_ratio_ = std::make_optional(target_snap_ratio);
    snap_action_source_ = std::make_optional(snap_event->snap_action_source());
    if (IsPartial(target_snap_ratio)) {
      partial_start_time_ = base::TimeTicks::Now();
    } else {
      // If a different snap ratio was requested, partial may have just ended.
      MaybeRecordPartialDuration();
    }
  }

  if (event->type() == WM_EVENT_FLOAT ||
      (current_state_->GetType() == chromeos::WindowStateType::kFloated &&
       event->IsTransitionEvent())) {
    {
      // Block nested events caused by float/unfloat events to ensure the float
      // animation is completed.
      base::AutoReset<bool> float_lock(&is_handling_float_event_, true);
      current_state_->OnWMEvent(this, event);
    }
    // Certain events need to be processed only after `is_handling_float_event_`
    // is reset. See `SnapGroupController::OnFloatUnfloatCompleted()`.
    if (auto* snap_group_controller = SnapGroupController::Get()) {
      snap_group_controller->OnFloatUnfloatCompleted(window_);
    }
  } else {
    current_state_->OnWMEvent(this, event);
  }

  // The current snap ratio may be different from the requested snap ratio, if
  // the window has a minimum size requirement.
  // Update snap ratio except for the following cases:
  // 1. We are currently handling a top-level snap event, during which we should
  // respect the snap event ratio;
  // 2. The workspace window resizer is about to start a drag to unsnap, but the
  // state type has not been updated yet; see `WorkspaceWindowResizer::Drag()`.
  if (!is_handling_snap_event_ && can_update_snap_ratio_ &&
      event->IsBoundsEvent()) {
    UpdateSnapRatio();
  }
}

gfx::Rect WindowState::GetCurrentBoundsInScreen() const {
  gfx::Rect bounds_in_screen = window_->GetTargetBounds();
  wm::ConvertRectToScreen(window_->parent(), &bounds_in_screen);
  return bounds_in_screen;
}

void WindowState::SaveCurrentBoundsForRestore() {
  SetRestoreBoundsInScreen(GetCurrentBoundsInScreen());
}

gfx::Rect WindowState::GetRestoreBoundsInScreen() const {
  gfx::Rect* restore_bounds =
      window_->GetProperty(aura::client::kRestoreBoundsKey);
  return restore_bounds ? *restore_bounds : gfx::Rect();
}

gfx::Rect WindowState::GetRestoreBoundsInParent() const {
  gfx::Rect result = GetRestoreBoundsInScreen();
  wm::ConvertRectFromScreen(window_->parent(), &result);
  return result;
}

void WindowState::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  window_->SetProperty(aura::client::kRestoreBoundsKey, bounds);
}

void WindowState::SetRestoreBoundsInParent(const gfx::Rect& bounds) {
  gfx::Rect bounds_in_screen = bounds;
  wm::ConvertRectToScreen(window_->parent(), &bounds_in_screen);
  SetRestoreBoundsInScreen(bounds_in_screen);
}

void WindowState::ClearRestoreBounds() {
  window_->ClearProperty(aura::client::kRestoreBoundsKey);
  window_->ClearProperty(wm::kVirtualKeyboardRestoreBoundsKey);
}

bool WindowState::VerticallyShrinkWindow(const gfx::Rect& work_area) {
  if (!HasRestoreBounds())
    return false;
  // Check if window is not work area vertical maximized.
  gfx::Rect bounds = window_->bounds();
  if (bounds.height() != work_area.height() || bounds.y() != work_area.y())
    return false;

  gfx::Rect restore_bounds = GetRestoreBoundsInParent();
  gfx::Rect new_bounds = restore_bounds;

  // Shrink from work area maximized window.
  if (bounds == work_area) {
    new_bounds = gfx::Rect(work_area.x(), restore_bounds.y(), work_area.width(),
                           restore_bounds.height());
    // Restore bounds is not cleared here in case a 2nd shrink is called next.
  } else {
    ClearRestoreBounds();
  }

  SetBoundsDirectCrossFade(new_bounds);
  return true;
}

bool WindowState::HorizontallyShrinkWindow(const gfx::Rect& work_area) {
  if (!HasRestoreBounds())
    return false;
  // Check if window is not work area horizontal maximized.
  gfx::Rect bounds = window_->bounds();
  if (bounds.width() != work_area.width() || bounds.x() != work_area.x())
    return false;

  gfx::Rect restore_bounds = GetRestoreBoundsInParent();
  gfx::Rect new_bounds = restore_bounds;

  // Shrink from work area maximized window.
  if (bounds == work_area) {
    new_bounds = gfx::Rect(restore_bounds.x(), work_area.y(),
                           restore_bounds.width(), work_area.height());
  } else {
    ClearRestoreBounds();
  }
  SetBoundsDirectCrossFade(new_bounds);
  return true;
}

std::unique_ptr<WindowState::State> WindowState::SetStateObject(
    std::unique_ptr<WindowState::State> new_state) {
  current_state_->DetachState(this);
  std::unique_ptr<WindowState::State> old_object = std::move(current_state_);
  current_state_ = std::move(new_state);
  current_state_->AttachState(this, old_object.get());
  return old_object;
}

void WindowState::UpdateSnapRatio() {
  if (!IsSnapped())
    return;
  ForceUpdateSnapRatio(window_->GetTargetBounds());
}

void WindowState::ForceUpdateSnapRatio(const gfx::Rect& target_bounds) {
  snap_ratio_ =
      std::make_optional(AdjustCurrentSnapRatio(window_, target_bounds));
  // If the snap ratio was adjusted, partial may have ended.
  MaybeRecordPartialDuration();
}

void WindowState::AddObserver(WindowStateObserver* observer) {
  observer_list_.AddObserver(observer);
}

void WindowState::RemoveObserver(WindowStateObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool WindowState::GetHideShelfWhenFullscreen() const {
  return window_->GetProperty(kHideShelfWhenFullscreenKey);
}

void WindowState::SetHideShelfWhenFullscreen(bool value) {
  base::AutoReset<bool> resetter(&ignore_property_change_, true);
  window_->SetProperty(kHideShelfWhenFullscreenKey, value);
}

bool WindowState::GetWindowPositionManaged() const {
  return window_->GetProperty(kWindowPositionManagedTypeKey);
}

void WindowState::SetWindowPositionManaged(bool managed) {
  window_->SetProperty(kWindowPositionManagedTypeKey, managed);
}

bool WindowState::CanConsumeSystemKeys() const {
  return window_->GetProperty(kCanConsumeSystemKeysKey);
}

void WindowState::SetCanConsumeSystemKeys(bool can_consume_system_keys) {
  window_->SetProperty(kCanConsumeSystemKeysKey, can_consume_system_keys);
}

bool WindowState::IsInImmersiveFullscreen() const {
  return window_->GetProperty(kImmersiveIsActive);
}

void WindowState::SetBoundsChangedByUser(bool bounds_changed_by_user) {
  bounds_changed_by_user_ = bounds_changed_by_user;
  if (bounds_changed_by_user) {
    pre_auto_manage_window_bounds_.reset();
    pre_added_to_workspace_window_bounds_.reset();
    persistent_window_info_of_display_removal_.reset();
    persistent_window_info_of_screen_rotation_.reset();
  }
}

std::unique_ptr<PresentationTimeRecorder> WindowState::OnDragStarted(
    int window_component) {
  DCHECK(drag_details_);

  if (delegate_) {
    return delegate_->OnDragStarted(window_component);
  }

  return nullptr;
}

void WindowState::OnCompleteDrag(const gfx::PointF& location) {
  DCHECK(drag_details_);
  if (delegate_) {
    delegate_->OnDragFinished(/*cancel=*/false, location);
  }

  SaveWindowForWindowRestore(this);
}

void WindowState::OnRevertDrag(const gfx::PointF& location) {
  DCHECK(drag_details_);
  if (delegate_) {
    delegate_->OnDragFinished(/*cancel=*/true, location);
  }
}

void WindowState::OnActivationLost() {
  if (IsPip()) {
    views::Widget::GetWidgetForNativeWindow(window_)
        ->widget_delegate()
        ->SetCanActivate(false);
  }
}

display::Display WindowState::GetDisplay() const {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
}

WindowStateType WindowState::GetRestoreWindowState() const {
  WindowStateType restore_state =
      window_state_restore_history_.empty() ||
              window_state_restore_history_.back() == WindowStateType::kDefault
          ? WindowStateType::kNormal
          : window_state_restore_history_.back();

  // Floated state has a limitation of one floated window per desk. So if we try
  // to restore a window to floated state, and there is a existing floated
  // window on the desk, we do not float the window as doing so would unfloat
  // the existing floated window.
  if (IsMinimized() && restore_state == WindowStateType::kFloated) {
    if (window_util::GetFloatedWindowForActiveDesk()) {
      return IsTabletModeEnabled() ? GetWindowTypeOnMaximizable()
                                   : WindowStateType::kNormal;
    }
  }

  // Different with the restore behaviors in clamshell mode, a window can not be
  // restored to kNormal window state if it's a maximize-able window.
  // We should still be able to restore a fullscreen/minimized/snapped window to
  // kMaximized window state for a maximize-able window, and also should be able
  // to support restoring a fullscreen/minimized/maximized window to snapped
  // window states.
  if (IsTabletModeEnabled()) {
    // In tablet mode, if we reset a floated window that's previously snapped
    // (float another window will reset currently floated window), maximize
    // floated window instead of restore floated window back to snapped state.
    if (restore_state == WindowStateType::kNormal ||
        (IsFloated() &&
         (restore_state == WindowStateType::kPrimarySnapped ||
          restore_state == WindowStateType::kSecondarySnapped))) {
      restore_state = GetWindowTypeOnMaximizable();
    }
  }

  return restore_state;
}

void WindowState::TrackDragToMaximizeBehavior() {
  if (!has_ever_been_dragged_to_maximized_)
    has_ever_been_dragged_to_maximized_ = true;

  // If drag to maximize is triggered again before we check for the previous
  // one, then the previous one must be a mis-trigger. Record the mis-trigger
  // and reset `drag_to_maximize_mis_trigger_timer_`.
  if (drag_to_maximize_mis_trigger_timer_.IsRunning()) {
    num_of_drag_to_maximize_mis_triggers_++;
    base::UmaHistogramBoolean(kValidDragMaximizedHistogramName, false);
    drag_to_maximize_mis_trigger_timer_.Stop();
  }

  drag_to_maximize_mis_trigger_timer_.Start(
      FROM_HERE, kDragToMaximizeMisTriggerThreshold, this,
      &WindowState::CheckAndRecordDragMaximizedBehavior);
}

base::AutoReset<bool> WindowState::GetScopedIgnorePropertyChange() {
  return base::AutoReset<bool>(&ignore_property_change_, true);
}

void WindowState::CreateDragDetails(const gfx::PointF& point_in_parent,
                                    int window_component,
                                    wm::WindowMoveSource source) {
  drag_details_ = std::make_unique<DragDetails>(window_, point_in_parent,
                                                window_component, source);
}

void WindowState::DeleteDragDetails() {
  drag_details_.reset();
}

void WindowState::SetAndClearRestoreBounds() {
  DCHECK(HasRestoreBounds());
  SetBoundsInScreen(GetRestoreBoundsInScreen());
  ClearRestoreBounds();
}

WindowState::WindowState(aura::Window* window)
    : window_(window),
      current_state_(
          new DefaultState(chromeos::ToWindowStateType(GetShowState()))) {
  window_->AddObserver(this);
  UpdateWindowPropertiesFromStateType();
  OnPrePipStateChange(WindowStateType::kDefault);
}

ui::ZOrderLevel WindowState::GetZOrdering() const {
  return window_->GetProperty(aura::client::kZOrderingKey);
}

ui::mojom::WindowShowState WindowState::GetShowState() const {
  return window_->GetProperty(aura::client::kShowStateKey);
}

void WindowState::SetBoundsInScreen(const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds_in_parent = bounds_in_screen;
  wm::ConvertRectFromScreen(window_->parent(), &bounds_in_parent);
  window_->SetBounds(bounds_in_parent);
}

void WindowState::AdjustSnappedBoundsForDisplayWorkspaceChange(
    gfx::Rect* bounds) {
  // Tablet mode should use bounds calculation in SplitViewController.
  // However, transient state from transitioning clamshell to tablet mode
  // might end up calling this function during work area changes, so we avoid
  // unnecessary task in that case when it will be overwritten by tablet mode
  // work.
  if (is_dragged() || !IsSnapped() ||
      display::Screen::GetScreen()->InTabletMode()) {
    return;
  }
  gfx::Rect maximized_bounds =
      screen_util::GetMaximizedWindowBoundsInParent(window_);

  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window_);

  // For snapped window, `GetSnappedWindowBounds` computes bounds position
  // from snap type and size from |snap_ratio|.
  const gfx::Rect snapped_bounds =
      snap_ratio_ ? GetSnappedWindowBounds(
                        maximized_bounds, display, window_,
                        GetStateType() == WindowStateType::kPrimarySnapped
                            ? ash::SnapViewType::kPrimary
                            : ash::SnapViewType::kSecondary,
                        *snap_ratio_)
                  : maximized_bounds;
  bounds->set_origin(snapped_bounds.origin());

  // If |snap_ratio_| exists adjust the size of the window. Otherwise only
  // maximize it vertically for horizontal screen and maximize horizontally for
  // vertical screen.
  if (snap_ratio_) {
    bounds->set_size(snapped_bounds.size());
  } else if (IsLayoutHorizontal(display)) {
    bounds->set_height(snapped_bounds.height());
  } else {
    bounds->set_width(snapped_bounds.width());
  }
}

void WindowState::UpdateWindowPropertiesFromStateType() {
  ui::mojom::WindowShowState new_window_state =
      ToWindowShowState(current_state_->GetType());
  if (new_window_state != GetShowState()) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(aura::client::kShowStateKey, new_window_state);
  }

  if (GetStateType() != window_->GetProperty(chromeos::kWindowStateTypeKey)) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(chromeos::kWindowStateTypeKey, GetStateType());
  }

  if (window_->GetProperty(ash::kWindowManagerManagesOpacityKey)) {
    const gfx::Size& size = window_->bounds().size();
    if (ShouldSetExplicitOpaqueRegionsForOcclusion(this)) {
      window_->SetTransparent(true);
      window_->SetOpaqueRegionsForOcclusion({gfx::Rect(size)});
    } else {
      window_->SetOpaqueRegionsForOcclusion({});
      window_->SetTransparent(false);
    }
  }
}

void WindowState::NotifyPreStateTypeChange(
    WindowStateType old_window_state_type) {
  for (auto& observer : observer_list_)
    observer.OnPreWindowStateTypeChange(this, old_window_state_type);
  OnPrePipStateChange(old_window_state_type);
}

void WindowState::NotifyPostStateTypeChange(
    WindowStateType old_window_state_type) {
  for (auto& observer : observer_list_)
    observer.OnPostWindowStateTypeChange(this, old_window_state_type);
  OnPostPipStateChange(old_window_state_type);
  UpdateWindowStateRestoreHistoryStack(old_window_state_type);
  SaveWindowForWindowRestore(this);
  if (chromeos::IsSnappedWindowStateType(old_window_state_type)) {
    // If the state type is no longer snapped, partial may have ended.
    MaybeRecordPartialDuration();
  }
}

void WindowState::OnPostPipStateChange(WindowStateType old_window_state_type) {
  if (old_window_state_type == WindowStateType::kPip) {
    // The animation type may be FADE_OUT_SLIDE_IN at this point, which we don't
    // want it to be anymore if the window is not PIP anymore.
    wm::SetWindowVisibilityAnimationType(
        window_, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
  }
}

void WindowState::SetBoundsDirectForTesting(const gfx::Rect& bounds) {
  SetBoundsDirect(bounds);
}

void WindowState::SetBoundsDirect(const gfx::Rect& bounds_in_parent) {
  gfx::Rect actual_new_bounds(bounds_in_parent);
  // Ensure we don't go smaller than our minimum bounds in "normal" window
  // modes
  if (window_->delegate() && !IsMaximized() && !IsFullscreen()) {
    // Get the minimum usable size of the minimum size and the screen size.
    gfx::Size min_size = window_->delegate()
                             ? window_->delegate()->GetMinimumSize()
                             : gfx::Size();
    gfx::Size max_size = window_->delegate()
                             ? window_->delegate()->GetMaximumSize()
                             : gfx::Size();
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
    min_size.SetToMin(display.work_area().size());

    actual_new_bounds.set_width(
        std::max(min_size.width(), actual_new_bounds.width()));
    actual_new_bounds.set_height(
        std::max(min_size.height(), actual_new_bounds.height()));
    if (!max_size.IsEmpty()) {
      DCHECK_LE(min_size.width(), max_size.width());
      DCHECK_LE(min_size.height(), max_size.height());
      actual_new_bounds.set_width(
          std::min(max_size.width(), actual_new_bounds.width()));
      actual_new_bounds.set_height(
          std::min(max_size.height(), actual_new_bounds.height()));
    }

    // Changing the size of the PIP window can detach it from one of the edges
    // of the screen, which makes the snap fraction logic fail. Ensure to snap
    // it again.
    if (IsPip() && !is_dragged() &&
        !Shell::Get()->pip_controller()->is_tucked()) {
      wm::ConvertRectToScreen(window_->GetRootWindow(), &actual_new_bounds);
      actual_new_bounds = CollisionDetectionUtils::GetRestingPosition(
          display, actual_new_bounds,
          CollisionDetectionUtils::RelativePriority::kPictureInPicture);
      wm::ConvertRectFromScreen(window_->GetRootWindow(), &actual_new_bounds);
    }
  }
  BoundsSetter().SetBounds(window_, actual_new_bounds);
}

void WindowState::SetBoundsConstrained(const gfx::Rect& bounds_in_parent) {
  const gfx::Rect work_area_in_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_);
  gfx::Rect child_bounds_in_parent(bounds_in_parent);

  // The window's size should be smaller than the work area.
  AdjustBoundsSmallerThan(work_area_in_parent.size(), &child_bounds_in_parent);
  // Normal windows should have the top of the bounds visible.
  // TODO(minch): Adjust the x position as well to match the functionality of
  // this function.
  if (window_->GetType() == aura::client::WINDOW_TYPE_NORMAL &&
      child_bounds_in_parent.y() < work_area_in_parent.y()) {
    child_bounds_in_parent.set_y(work_area_in_parent.y());
  }

  SetBoundsDirect(child_bounds_in_parent);
}

void WindowState::SetBoundsDirectAnimated(const gfx::Rect& bounds_in_parent,
                                          base::TimeDelta duration,
                                          gfx::Tween::Type tween_type) {
  if (wm::WindowAnimationsDisabled(window_)) {
    SetBoundsDirect(bounds_in_parent);
    return;
  }
  ui::Layer* layer = window_->layer();
  ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTweenType(tween_type);
  slide_settings.SetTransitionDuration(duration);
  SetBoundsDirect(bounds_in_parent);
}

void WindowState::SetBoundsDirectCrossFade(const gfx::Rect& bounds_in_parent,
                                           std::optional<bool> float_state) {
  // Some test results in invoking CrossFadeToBounds when window is not visible.
  // No animation is necessary in that case, thus just change the bounds and
  // quit.
  if (!window_->TargetVisibility()) {
    SetBoundsConstrained(bounds_in_parent);
    return;
  }

  // If the window already has a transform in place, do not use the cross fade
  // animation, set the bounds directly instead, or animation is disabled.
  if (!window_->layer()->GetTargetTransform().IsIdentity() ||
      wm::WindowAnimationsDisabled(window_)) {
    SetBoundsDirect(bounds_in_parent);
    return;
  }

  // Create fresh layers for the window and all its children to paint into.
  // Takes ownership of the old layer and all its children, which will be
  // cleaned up after the animation completes.
  // Specify |set_bounds| to true here to keep the old bounds in the child
  // windows of |window|.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      wm::RecreateLayers(window_);

  // Resize the window to the new size, which will force a layout and paint.
  SetBoundsDirect(bounds_in_parent);

  if (float_state) {
    CrossFadeAnimationForFloatUnfloat(window_, std::move(old_layer_owner),
                                      *float_state);
    return;
  }

  SCOPED_CRASH_KEY_NUMBER("333095196", "state_type",
                          base::to_underlying(GetStateType()));

  CrossFadeAnimation(window_, std::move(old_layer_owner));
}

void WindowState::OnPrePipStateChange(WindowStateType old_window_state_type) {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window_);
  const bool was_pip = old_window_state_type == WindowStateType::kPip;
  auto* const pip_controller = Shell::Get()->pip_controller();
  if (IsPip()) {
    // Set this window to `PipController`.
    // The window has to be set to the controller before
    // `widget->Deactivate()` because this sometimes calls
    // `PipController::UpdatePipBounds()`.
    pip_controller->SetPipWindow(window_);

    CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
        window_, CollisionDetectionUtils::RelativePriority::kPictureInPicture);
    // widget may not exit in some unit tests.
    // TODO(oshima): Fix unit tests and add DCHECK.
    if (widget) {
      widget->widget_delegate()->SetCanActivate(false);
      if (widget->IsActive())
        widget->Deactivate();
      Shell::Get()->focus_cycler()->AddWidget(widget);
    }
    wm::SetWindowVisibilityAnimationType(
        window_, WINDOW_VISIBILITY_ANIMATION_TYPE_FADE_IN_SLIDE_OUT);

    // There may already be a system ui window on the initial position.
    pip_controller->UpdatePipBounds();

    if (!was_pip) {
      if (widget && widget->GetContentsView()) {
        widget->GetContentsView()->GetViewAccessibility().AnnounceText(
            l10n_util::GetStringUTF16(IDS_ENTER_PIP_A11Y_NOTIFICATION));
      }
    }

    CollectPipEnterExitMetrics(/*enter=*/true);

    // PIP window shouldn't be tracked in MruWindowTracker.
    window_->SetProperty(ash::kExcludeInMruKey, true);
  } else if (was_pip) {
    if (widget) {
      widget->widget_delegate()->SetCanActivate(true);
      Shell::Get()->focus_cycler()->RemoveWidget(widget);
    }
    wm::SetWindowVisibilityAnimationType(
        window_, wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);

    CollectPipEnterExitMetrics(/*enter=*/false);
    window_->ClearProperty(ash::kExcludeInMruKey);

    // Unset PiP window when exiting PiP state to another state.
    pip_controller->UnsetPipWindow(window_);
  }
  // PIP uses the snap fraction to place the PIP window at the correct position
  // after screen rotation, system UI area change, etc. Make sure to reset this
  // when the window enters/exits PIP so the obsolete fraction won't be used.
  if (IsPip() || was_pip)
    ash::PipPositioner::ClearSnapFraction(this);
}

void WindowState::CollectPipEnterExitMetrics(bool enter) {
  const bool is_arc = IsArcWindow(window_);
  if (enter) {
    pip_start_time_ = base::TimeTicks::Now();

    ReportAshPipEvents(AshPipEvents::PIP_START);
    ReportAshPipEvents(is_arc ? AshPipEvents::ANDROID_PIP_START
                              : AshPipEvents::CHROME_PIP_START);
  } else {
    ReportAshPipEvents(AshPipEvents::PIP_END);
    ReportAshPipEvents(is_arc ? AshPipEvents::ANDROID_PIP_END
                              : AshPipEvents::CHROME_PIP_END);

    if (is_arc) {
      DCHECK(!pip_start_time_.is_null());
      const auto session_duration = base::TimeTicks::Now() - pip_start_time_;
      ReportAshPipAndroidPipUseTime(session_duration);
    }
    pip_start_time_ = base::TimeTicks();
  }
}

void WindowState::MaybeRecordPartialDuration() {
  // No-op if `partial_start_time_` is null, i.e. partial never started.
  if (!partial_start_time_.is_null()) {
    base::UmaHistogramCustomCounts(
        kPartialSplitDurationHistogramName,
        (base::TimeTicks::Now() - partial_start_time_).InMinutes(), /*min=*/1,
        /*exclusive_max=*/base::Days(7).InMinutes(), 50);
    partial_start_time_ = base::TimeTicks();
  }
}

void WindowState::UpdateWindowStateRestoreHistoryStack(
    chromeos::WindowStateType previous_state_type) {
  WindowStateType current_state_type = GetStateType();

  if (!IsValidForRestoreHistory(current_state_type)) {
    window_state_restore_history_.clear();
    window_->ClearProperty(aura::client::kRestoreShowStateKey);
    return;
  }

  // We'll need to pop out any window state that the `current_state_type` can
  // not restore back to (i.e., whose restore order is equal or higher than
  // `current_state_type`).
  for (auto state : base::Reversed(window_state_restore_history_)) {
    if (CanRestoreState(current_state_type, state)) {
      break;
    }
    window_state_restore_history_.pop_back();
  }

  if (IsValidForRestoreHistory(previous_state_type) &&
      CanRestoreState(current_state_type, previous_state_type)) {
    window_state_restore_history_.push_back(previous_state_type);
  }

  // TODO(xdai): For now we don't save the restore history in tablet mode in the
  // window property, so that when exiting tablet mode, the window can still
  // restore back to its old window state (see the test case
  // TabletModeWindowManagerTest.UnminimizeInTabletMode). We should revisit this
  // logic.
  if (!IsTabletModeEnabled()) {
    window_->SetProperty(aura::client::kRestoreShowStateKey,
                         chromeos::ToWindowShowState(GetRestoreWindowState()));
  }

  // This is a special logic for windows that were created from full restore.
  // In those cases, the full history of window states is truncated. We detect
  // this by asserting that any non-normal windows that have no previous history
  // must have a truncated history.
  //
  // Unfortunately this case is particularly tricky because the restore bounds
  // will be set externally, using the same windows property key.
  //
  // If we detect that we are in full restore, we will artificially create a
  // normal restore state in history to retain the bounds.
  if (window_state_restore_history_.empty() && HasRestoreBounds() &&
      !IsNormalStateType()) {
    window_state_restore_history_.push_back(WindowStateType::kDefault);
  }
}

chromeos::WindowStateType WindowState::GetWindowTypeOnMaximizable() const {
  return CanMaximize() && wm::GetTransientParent(window_) == nullptr
             ? WindowStateType::kMaximized
             : WindowStateType::kNormal;
}

// static
WindowState* WindowState::Get(aura::Window* window) {
  if (!window)
    return nullptr;

  WindowState* state = window->GetProperty(kWindowStateKey);
  if (state)
    return state;

  if (window->GetType() == aura::client::WINDOW_TYPE_CONTROL)
    return nullptr;

  DCHECK(window->parent());

  // WindowState is only for windows in top level container, unless they are
  // temporarily hidden when launched by window restore. The will be reparented
  // to a top level container soon, and need a WindowState.
  if (!IsToplevelContainer(window->parent()) &&
      !IsTemporarilyHiddenForFullrestore(window)) {
    return nullptr;
  }

  state = new WindowState(window);
  window->SetProperty(kWindowStateKey, state);

  return state;
}

// static
const WindowState* WindowState::Get(const aura::Window* window) {
  return Get(const_cast<aura::Window*>(window));
}

// static
WindowState* WindowState::ForActiveWindow() {
  aura::Window* active = window_util::GetActiveWindow();
  return active ? WindowState::Get(active) : nullptr;
}

void WindowState::OnWindowPropertyChanged(aura::Window* window,
                                          const void* key,
                                          intptr_t old) {
  DCHECK_EQ(window_, window);
  if (key == aura::client::kShowStateKey) {
    if (!ignore_property_change_) {
      WMEvent event(WMEventTypeFromShowState(GetShowState()));
      OnWMEvent(&event);
    }
    return;
  }
  if (key == kWindowPipTypeKey) {
    if (window->GetProperty(kWindowPipTypeKey)) {
      WMEvent event(WM_EVENT_PIP);
      OnWMEvent(&event);
    } else {
      // Currently "restore" is not implemented.
      NOTIMPLEMENTED();
    }
    return;
  }
  if (key == chromeos::kWindowStateTypeKey) {
    if (!ignore_property_change_) {
      // This change came from somewhere else. Revert it.
      window->SetProperty(chromeos::kWindowStateTypeKey, GetStateType());
    }
    return;
  }
  if (key == aura::client::kWindowWorkspaceKey ||
      key == aura::client::kDeskUuidKey) {
    // Save the window for window restore purposes unless
    // |ignore_property_change_| is true. Note that moving windows across
    // displays will also trigger a kWindowWorkspaceKey change, even if the
    // value stays the same, so we do not need to save the window when it
    // changes root windows (OnWindowAddedToRootWindow).
    if (!ignore_property_change_)
      SaveWindowForWindowRestore(this);
    return;
  }

  // The shelf visibility should be updated if kHideShelfWhenFullscreenKey or
  // kImmersiveIsActive change - these property affect the shelf behavior, and
  // the shelf is expected to be hidden when fullscreen or immersive mode start.
  const bool requires_shelf_visibility_update =
      (key == kHideShelfWhenFullscreenKey &&
       old != window->GetProperty(kHideShelfWhenFullscreenKey)) ||
      (key == kImmersiveIsActive &&
       old != window->GetProperty(kImmersiveIsActive));

  if (requires_shelf_visibility_update && !ignore_property_change_) {
    Shelf::UpdateShelfVisibility();
    return;
  }
}

void WindowState::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window_, window);
  if (wm::GetTransientParent(window)) {
    return;
  }
  MoveAllTransientChildrenToNewRoot(window);
}

void WindowState::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);

  // If the window is destroyed during PIP, count that as exiting.
  if (IsPip())
    CollectPipEnterExitMetrics(/*enter=*/false);

  MaybeRecordPartialDuration();

  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (widget)
    Shell::Get()->focus_cycler()->RemoveWidget(widget);

  current_state_->OnWindowDestroying(this);
  delegate_.reset();
}

void WindowState::OnWindowBoundsChanged(aura::Window* window,
                                        const gfx::Rect& old_bounds,
                                        const gfx::Rect& new_bounds,
                                        ui::PropertyChangeReason reason) {
  CHECK_EQ(window_, window);
  if (window_->GetTransparent() &&
      ShouldSetExplicitOpaqueRegionsForOcclusion(this)) {
    window_->SetOpaqueRegionsForOcclusion({gfx::Rect(new_bounds.size())});
  }

  if (reason != ui::PropertyChangeReason::FROM_ANIMATION && !is_dragged())
    SaveWindowForWindowRestore(this);
}

void WindowState::OnWindowParentChanged(aura::Window* window,
                                        aura::Window* parent) {
  if (window != window_) {
    return;
  }
  // If the window is moved to another desk, partial may have ended.
  MaybeRecordPartialDuration();
}

void WindowState::OnWindowVisibilityChanged(aura::Window* window,
                                            bool visible) {
  if (IsPip() && window == window_) {
    if (visible) {
      // If this window is a PiP and its SnapFraction is null.
      // Note that, at this point, ARC PiP may not be ready as visibility can be
      // updated when it transitions from minimized to PiP. In this case, snap
      // fraction is updated in
      // `ClientControlledShellSurface::OnPostWidgetCommit`.
      if (!PipPositioner::HasSnapFraction(this) && !IsArcWindow(window)) {
        PipPositioner::SaveSnapFraction(this, window_->GetBoundsInScreen());
      }
      Shell::Get()->pip_controller()->SetPipWindow(window);
    } else {
      Shell::Get()->pip_controller()->UnsetPipWindow(window);
    }
  }

  // From here, we are only interested if the parent visibility changes, i.e.
  // desk changes.
  if (window != window_->parent()) {
    return;
  }
  // If the parent just became visible and `window_` is partial split, start
  // recording.
  if (visible && snap_ratio_ && IsPartial(*snap_ratio_)) {
    partial_start_time_ = base::TimeTicks::Now();
  }
  // If the parent is no longer visible, partial may have ended.
  if (!visible) {
    MaybeRecordPartialDuration();
  }
}

bool WindowState::CanUnresizableSnapOnDisplay(display::Display display) const {
  DCHECK(!CanResize());

  if (IsPip())
    return false;

  if (IsTabletModeEnabled())
    return false;

  const gfx::Size* preferred_size =
      window_->GetProperty(kUnresizableSnappedSizeKey);
  if (!preferred_size || preferred_size->IsZero())
    return false;

  const auto orientation = GetSnapDisplayOrientation(display);
  const bool is_horizontal =
      orientation == chromeos::OrientationType::kLandscapePrimary ||
      orientation == chromeos::OrientationType::kLandscapeSecondary;

  const gfx::Rect work_area = display.work_area();
  if (is_horizontal && (preferred_size->width() == 0 ||
                        work_area.width() < preferred_size->width())) {
    return false;
  }
  if (!is_horizontal && (preferred_size->height() == 0 ||
                         work_area.height() < preferred_size->height())) {
    return false;
  }

  return true;
}

void WindowState::RecordWindowSnapActionSource(
    WindowSnapActionSource snap_action_source) {
  base::UmaHistogramEnumeration(kWindowSnapActionSourceHistogram,
                                snap_action_source);
}

void WindowState::CheckAndRecordDragMaximizedBehavior() {
  if (!IsMaximized()) {
    num_of_drag_to_maximize_mis_triggers_++;
    base::UmaHistogramBoolean(kValidDragMaximizedHistogramName, false);
  } else {
    base::UmaHistogramBoolean(kValidDragMaximizedHistogramName, true);
  }
}

void WindowState::ReadOutWindowCycleSnapAction(int message_id) {
  Shell::Get()
      ->accessibility_controller()
      ->TriggerAccessibilityAlertWithMessage(
          l10n_util::GetStringUTF8(message_id));
}

}  // namespace ash
