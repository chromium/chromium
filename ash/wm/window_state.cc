// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include <memory>
#include <utility>

#include "ash/focus_cycler.h"
#include "ash/metrics/pip_uma.h"
#include "ash/public/cpp/app_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_pin_type.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/collision_detection/collision_detection_utils.h"
#include "ash/wm/default_state.h"
#include "ash/wm/pip/pip_positioner.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "base/auto_reset.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/ime_util_chromeos.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

bool IsTabletModeEnabled() {
  return Shell::Get()->tablet_mode_controller()->InTabletMode();
}

bool IsToplevelContainer(aura::Window* window) {
  DCHECK(window);
  int container_id = window->id();
  // ArcVirtualKeyboard is implemented as a exo window which requires
  // WindowState to manage its state.
  return IsActivatableShellWindowId(container_id) ||
         container_id == kShellWindowId_ArcVirtualKeyboardContainer;
}

// A tentative class to set the bounds on the window.
// TODO(oshima): Once all logic is cleaned up, move this to the real layout
// manager with proper friendship.
class BoundsSetter : public aura::LayoutManager {
 public:
  BoundsSetter() = default;
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
    SetChildBoundsDirect(window, bounds);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BoundsSetter);
};

// Animation metrics reporter which reports animation smoothness percentages for
// the given histogram name, and then self destructs.
class WindowAnimationMetricsReporter : public ui::AnimationMetricsReporter {
 public:
  explicit WindowAnimationMetricsReporter(const std::string& histogram_name)
      : histogram_name_(histogram_name) {}
  ~WindowAnimationMetricsReporter() override = default;

  // ui::AnimationMetricsReporter:
  void Report(int value) override {
    base::UmaHistogramPercentage(histogram_name_, value);
    delete this;
  }

 private:
  const std::string histogram_name_;
  DISALLOW_COPY_AND_ASSIGN(WindowAnimationMetricsReporter);
};

WMEventType WMEventTypeFromShowState(ui::WindowShowState requested_show_state) {
  switch (requested_show_state) {
    case ui::SHOW_STATE_DEFAULT:
    case ui::SHOW_STATE_NORMAL:
      return WM_EVENT_NORMAL;
    case ui::SHOW_STATE_MINIMIZED:
      return WM_EVENT_MINIMIZE;
    case ui::SHOW_STATE_MAXIMIZED:
      return WM_EVENT_MAXIMIZE;
    case ui::SHOW_STATE_FULLSCREEN:
      return WM_EVENT_FULLSCREEN;
    case ui::SHOW_STATE_INACTIVE:
      return WM_EVENT_SHOW_INACTIVE;

    case ui::SHOW_STATE_END:
      NOTREACHED() << "No WMEvent defined for the show state:"
                   << requested_show_state;
  }
  return WM_EVENT_NORMAL;
}

WMEventType WMEventTypeFromWindowPinType(ash::WindowPinType type) {
  switch (type) {
    case ash::WindowPinType::kNone:
      return WM_EVENT_NORMAL;
    case ash::WindowPinType::kPinned:
      return WM_EVENT_PIN;
    case ash::WindowPinType::kTrustedPinned:
      return WM_EVENT_TRUSTED_PIN;
  }
  NOTREACHED() << "No WMEvent defined for the window pin type:" << type;
  return WM_EVENT_NORMAL;
}

float GetCurrentSnappedWidthRatio(aura::Window* window) {
  gfx::Rect maximized_bounds =
      screen_util::GetMaximizedWindowBoundsInParent(window);
  return static_cast<float>(window->bounds().width()) /
         static_cast<float>(maximized_bounds.width());
}

// Move all transient children to |dst_root|, including the ones in the child
// windows and transient children of the transient children.
void MoveAllTransientChildrenToNewRoot(aura::Window* window) {
  aura::Window* dst_root = window->GetRootWindow();
  for (aura::Window* transient_child : ::wm::GetTransientChildren(window)) {
    if (!transient_child->parent())
      continue;
    const int container_id = transient_child->parent()->id();
    DCHECK_GE(container_id, 0);
    aura::Window* container = dst_root->GetChildById(container_id);
    if (container->Contains(transient_child))
      continue;
    gfx::Rect child_bounds = transient_child->bounds();
    ::wm::ConvertRectToScreen(dst_root, &child_bounds);
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
                             base::TimeDelta::FromSeconds(1),
                             base::TimeDelta::FromHours(10), 50);
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
}

bool WindowState::HasDelegate() const {
  return !!delegate_;
}

void WindowState::SetDelegate(std::unique_ptr<WindowStateDelegate> delegate) {
  DCHECK((!delegate_.get() && !!delegate.get()) ||
         (!!delegate_.get() && !delegate.get()));
  delegate_ = std::move(delegate);
}

WindowStateType WindowState::GetStateType() const {
  return current_state_->GetType();
}

bool WindowState::IsMinimized() const {
  return GetStateType() == WindowStateType::kMinimized;
}

bool WindowState::IsMaximized() const {
  return GetStateType() == WindowStateType::kMaximized;
}

bool WindowState::IsFullscreen() const {
  return GetStateType() == WindowStateType::kFullscreen;
}

bool WindowState::IsMaximizedOrFullscreenOrPinned() const {
  return GetStateType() == WindowStateType::kMaximized ||
         GetStateType() == WindowStateType::kFullscreen || IsPinned();
}

bool WindowState::IsSnapped() const {
  return GetStateType() == WindowStateType::kLeftSnapped ||
         GetStateType() == WindowStateType::kRightSnapped;
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

bool WindowState::IsNormalStateType() const {
  return GetStateType() == WindowStateType::kNormal ||
         GetStateType() == WindowStateType::kDefault;
}

bool WindowState::IsNormalOrSnapped() const {
  return IsNormalStateType() || IsSnapped();
}

bool WindowState::IsActive() const {
  return wm::IsActiveWindow(window_);
}

bool WindowState::IsUserPositionable() const {
  return window_util::IsWindowUserPositionable(window_);
}

bool WindowState::HasMaximumWidthOrHeight() const {
  if (!window_->delegate())
    return false;

  const gfx::Size max_size = window_->delegate()->GetMaximumSize();
  return max_size.width() || max_size.height();
}

bool WindowState::CanMaximize() const {
  // Window must allow maximization and have no maximum width or height.
  if ((window_->GetProperty(aura::client::kResizeBehaviorKey) &
       aura::client::kResizeBehaviorCanMaximize) == 0) {
    return false;
  }

  return !HasMaximumWidthOrHeight();
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

bool WindowState::CanSnap() const {
  if (!CanResize() || IsPip())
    return false;

  // Allow windows with no maximum width or height to be snapped.
  // TODO(oshima): We should probably snap if the maximum size is defined
  // and greater than the snapped size.
  return !HasMaximumWidthOrHeight();
}

bool WindowState::HasRestoreBounds() const {
  gfx::Rect* bounds = window_->GetProperty(aura::client::kRestoreBoundsKey);
  return bounds != nullptr && !bounds->IsEmpty();
}

void WindowState::Maximize() {
  ::wm::SetWindowState(window_, ui::SHOW_STATE_MAXIMIZED);
}

void WindowState::Minimize() {
  ::wm::SetWindowState(window_, ui::SHOW_STATE_MINIMIZED);
}

void WindowState::Unminimize() {
  ::wm::Unminimize(window_);
}

void WindowState::Activate() {
  wm::ActivateWindow(window_);
}

void WindowState::Deactivate() {
  wm::DeactivateWindow(window_);
}

void WindowState::Restore() {
  if (!IsNormalStateType()) {
    const WMEvent event(WM_EVENT_NORMAL);
    OnWMEvent(&event);
  }
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
  current_state_->OnWMEvent(this, event);

  UpdateSnappedWidthRatio(event);
}

void WindowState::SaveCurrentBoundsForRestore() {
  gfx::Rect bounds_in_screen = window_->GetTargetBounds();
  ::wm::ConvertRectToScreen(window_->parent(), &bounds_in_screen);
  SetRestoreBoundsInScreen(bounds_in_screen);
}

gfx::Rect WindowState::GetRestoreBoundsInScreen() const {
  gfx::Rect* restore_bounds =
      window_->GetProperty(aura::client::kRestoreBoundsKey);
  return restore_bounds ? *restore_bounds : gfx::Rect();
}

gfx::Rect WindowState::GetRestoreBoundsInParent() const {
  gfx::Rect result = GetRestoreBoundsInScreen();
  ::wm::ConvertRectFromScreen(window_->parent(), &result);
  return result;
}

void WindowState::SetRestoreBoundsInScreen(const gfx::Rect& bounds) {
  window_->SetProperty(aura::client::kRestoreBoundsKey, bounds);
}

void WindowState::SetRestoreBoundsInParent(const gfx::Rect& bounds) {
  gfx::Rect bounds_in_screen = bounds;
  ::wm::ConvertRectToScreen(window_->parent(), &bounds_in_screen);
  SetRestoreBoundsInScreen(bounds_in_screen);
}

void WindowState::ClearRestoreBounds() {
  window_->ClearProperty(aura::client::kRestoreBoundsKey);
  window_->ClearProperty(::wm::kVirtualKeyboardRestoreBoundsKey);
}

std::unique_ptr<WindowState::State> WindowState::SetStateObject(
    std::unique_ptr<WindowState::State> new_state) {
  current_state_->DetachState(this);
  std::unique_ptr<WindowState::State> old_object = std::move(current_state_);
  current_state_ = std::move(new_state);
  current_state_->AttachState(this, old_object.get());
  return old_object;
}

void WindowState::UpdateSnappedWidthRatio(const WMEvent* event) {
  if (!IsSnapped()) {
    snapped_width_ratio_.reset();
    return;
  }

  const WMEventType type = event->type();
  // Initializes |snapped_width_ratio_| whenever |event| is snapping event.
  if (type == WM_EVENT_SNAP_LEFT || type == WM_EVENT_SNAP_RIGHT ||
      type == WM_EVENT_CYCLE_SNAP_LEFT || type == WM_EVENT_CYCLE_SNAP_RIGHT) {
    // Since |UpdateSnappedWidthRatio()| is called post WMEvent taking effect,
    // |window_|'s bounds is in a correct state for ratio update.
    snapped_width_ratio_ =
        base::make_optional(GetCurrentSnappedWidthRatio(window_));
    return;
  }

  // |snapped_width_ratio_| under snapped state may change due to bounds event.
  if (event->IsBoundsEvent()) {
    snapped_width_ratio_ =
        base::make_optional(GetCurrentSnappedWidthRatio(window_));
  }
}

void WindowState::SetPreAutoManageWindowBounds(const gfx::Rect& bounds) {
  pre_auto_manage_window_bounds_ = base::make_optional(bounds);
}

void WindowState::SetPreAddedToWorkspaceWindowBounds(const gfx::Rect& bounds) {
  pre_added_to_workspace_window_bounds_ = base::make_optional(bounds);
}

void WindowState::SetPersistentWindowInfo(
    const PersistentWindowInfo& persistent_window_info) {
  persistent_window_info_ = base::make_optional(persistent_window_info);
}

void WindowState::ResetPersistentWindowInfo() {
  persistent_window_info_.reset();
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

void WindowState::set_bounds_changed_by_user(bool bounds_changed_by_user) {
  bounds_changed_by_user_ = bounds_changed_by_user;
  if (bounds_changed_by_user) {
    pre_auto_manage_window_bounds_.reset();
    pre_added_to_workspace_window_bounds_.reset();
    persistent_window_info_.reset();
  }
}

void WindowState::OnDragStarted(int window_component) {
  DCHECK(drag_details_);
  if (delegate_)
    delegate_->OnDragStarted(window_component);
}

void WindowState::OnCompleteDrag(const gfx::Point& location) {
  DCHECK(drag_details_);
  if (delegate_)
    delegate_->OnDragFinished(/*canceled=*/false, location);
}

void WindowState::OnRevertDrag(const gfx::Point& location) {
  DCHECK(drag_details_);
  if (delegate_)
    delegate_->OnDragFinished(/*canceled=*/true, location);
}

void WindowState::OnActivationLost() {
  if (IsPip()) {
    views::Widget::GetWidgetForNativeWindow(window())
        ->widget_delegate()
        ->SetCanActivate(false);
  }
}

display::Display WindowState::GetDisplay() {
  return display::Screen::GetScreen()->GetDisplayNearestWindow(window());
}

void WindowState::CreateDragDetails(const gfx::Point& point_in_parent,
                                    int window_component,
                                    ::wm::WindowMoveSource source) {
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
      bounds_changed_by_user_(false),
      can_consume_system_keys_(false),
      unminimize_to_restore_bounds_(false),
      hide_shelf_when_fullscreen_(true),
      autohide_shelf_when_maximized_or_fullscreen_(false),
      cached_z_order_(ui::ZOrderLevel::kNormal),
      ignore_property_change_(false),
      current_state_(new DefaultState(ToWindowStateType(GetShowState()))) {
  window_->AddObserver(this);
  UpdateWindowPropertiesFromStateType();
  OnPrePipStateChange(WindowStateType::kDefault);
}

ui::ZOrderLevel WindowState::GetZOrdering() const {
  return window_->GetProperty(aura::client::kZOrderingKey);
}

ui::WindowShowState WindowState::GetShowState() const {
  return window_->GetProperty(aura::client::kShowStateKey);
}

ash::WindowPinType WindowState::GetPinType() const {
  return window_->GetProperty(kWindowPinTypeKey);
}

void WindowState::SetBoundsInScreen(const gfx::Rect& bounds_in_screen) {
  gfx::Rect bounds_in_parent = bounds_in_screen;
  ::wm::ConvertRectFromScreen(window_->parent(), &bounds_in_parent);
  window_->SetBounds(bounds_in_parent);
}

void WindowState::AdjustSnappedBounds(gfx::Rect* bounds) {
  if (is_dragged() || !IsSnapped())
    return;
  gfx::Rect maximized_bounds =
      screen_util::GetMaximizedWindowBoundsInParent(window_);
  if (snapped_width_ratio_) {
    bounds->set_width(
        static_cast<int>(*snapped_width_ratio_ * maximized_bounds.width()));
  }
  if (GetStateType() == WindowStateType::kLeftSnapped)
    bounds->set_x(maximized_bounds.x());
  else if (GetStateType() == WindowStateType::kRightSnapped)
    bounds->set_x(maximized_bounds.right() - bounds->width());
  bounds->set_y(maximized_bounds.y());
  bounds->set_height(maximized_bounds.height());
}

void WindowState::UpdateWindowPropertiesFromStateType() {
  ui::WindowShowState new_window_state =
      ToWindowShowState(current_state_->GetType());
  // Clear |kPreMinimizedShowStateKey| property only when the window is actually
  // Unminimized and not in tablet mode.
  if (new_window_state != ui::SHOW_STATE_MINIMIZED && IsMinimized() &&
      !IsTabletModeEnabled()) {
    window()->ClearProperty(aura::client::kPreMinimizedShowStateKey);
  }
  if (new_window_state != GetShowState()) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(aura::client::kShowStateKey, new_window_state);
  }

  if (GetStateType() != window_->GetProperty(kWindowStateTypeKey)) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(kWindowStateTypeKey, GetStateType());
  }

  // sync up current window show state with PinType property.
  ash::WindowPinType pin_type = ash::WindowPinType::kNone;
  if (GetStateType() == WindowStateType::kPinned)
    pin_type = ash::WindowPinType::kPinned;
  else if (GetStateType() == WindowStateType::kTrustedPinned)
    pin_type = ash::WindowPinType::kTrustedPinned;
  if (pin_type != GetPinType()) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(kWindowPinTypeKey, pin_type);
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
}

void WindowState::OnPostPipStateChange(WindowStateType old_window_state_type) {
  if (old_window_state_type == WindowStateType::kPip) {
    // The animation type may be FADE_OUT_SLIDE_IN at this point, which we don't
    // want it to be anymore if the window is not PIP anymore.
    ::wm::SetWindowVisibilityAnimationType(
        window_, ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
  }
}

void WindowState::SetBoundsDirect(const gfx::Rect& bounds) {
  gfx::Rect actual_new_bounds(bounds);
  // Ensure we don't go smaller than our minimum bounds in "normal" window
  // modes
  if (window_->delegate() && !IsMaximized() && !IsFullscreen()) {
    // Get the minimum usable size of the minimum size and the screen size.
    gfx::Size min_size = window_->delegate()
                             ? window_->delegate()->GetMinimumSize()
                             : gfx::Size();
    const display::Display display =
        display::Screen::GetScreen()->GetDisplayNearestWindow(window_);
    min_size.SetToMin(display.work_area().size());

    actual_new_bounds.set_width(
        std::max(min_size.width(), actual_new_bounds.width()));
    actual_new_bounds.set_height(
        std::max(min_size.height(), actual_new_bounds.height()));
  }
  BoundsSetter().SetBounds(window_, actual_new_bounds);
}

void WindowState::SetBoundsConstrained(const gfx::Rect& bounds) {
  gfx::Rect work_area_in_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_);
  gfx::Rect child_bounds(bounds);
  AdjustBoundsSmallerThan(work_area_in_parent.size(), &child_bounds);
  SetBoundsDirect(child_bounds);
}

void WindowState::SetBoundsDirectAnimated(const gfx::Rect& bounds,
                                          base::TimeDelta duration,
                                          gfx::Tween::Type tween_type) {
  if (::wm::WindowAnimationsDisabled(window_)) {
    SetBoundsDirect(bounds);
    return;
  }
  ui::Layer* layer = window_->layer();
  ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTweenType(tween_type);
  slide_settings.SetTransitionDuration(duration);
  if (animation_smoothness_histogram_name_) {
    slide_settings.SetAnimationMetricsReporter(
        new WindowAnimationMetricsReporter(
            *animation_smoothness_histogram_name_));
  }
  SetBoundsDirect(bounds);
}

void WindowState::SetBoundsDirectCrossFade(const gfx::Rect& new_bounds) {
  // Some test results in invoking CrossFadeToBounds when window is not visible.
  // No animation is necessary in that case, thus just change the bounds and
  // quit.
  if (!window_->TargetVisibility()) {
    SetBoundsConstrained(new_bounds);
    return;
  }

  // If the window already has a transform in place, do not use the cross fade
  // animation, set the bounds directly instead, or animation is disabled.
  if (!window_->layer()->GetTargetTransform().IsIdentity() ||
      ::wm::WindowAnimationsDisabled(window_)) {
    SetBoundsDirect(new_bounds);
    return;
  }

  // Create fresh layers for the window and all its children to paint into.
  // Takes ownership of the old layer and all its children, which will be
  // cleaned up after the animation completes.
  // Specify |set_bounds| to true here to keep the old bounds in the child
  // windows of |window|.
  std::unique_ptr<ui::LayerTreeOwner> old_layer_owner =
      ::wm::RecreateLayers(window_);

  // Resize the window to the new size, which will force a layout and paint.
  SetBoundsDirect(new_bounds);

  CrossFadeAnimation(window_, std::move(old_layer_owner));
}

void WindowState::OnPrePipStateChange(WindowStateType old_window_state_type) {
  auto* widget = views::Widget::GetWidgetForNativeWindow(window());
  const bool was_pip = old_window_state_type == WindowStateType::kPip;
  if (IsPip()) {
    CollisionDetectionUtils::MarkWindowPriorityForCollisionDetection(
        window(), CollisionDetectionUtils::RelativePriority::kPictureInPicture);
    // widget may not exit in some unit tests.
    // TODO(oshima): Fix unit tests and add DCHECK.
    if (widget) {
      widget->widget_delegate()->SetCanActivate(false);
      if (widget->IsActive())
        widget->Deactivate();
      Shell::Get()->focus_cycler()->AddWidget(widget);
    }
    ::wm::SetWindowVisibilityAnimationType(
        window(), WINDOW_VISIBILITY_ANIMATION_TYPE_FADE_IN_SLIDE_OUT);
    // There may already be a system ui window on the initial position.
    UpdatePipBounds();
    if (!was_pip) {
      window()->SetProperty(ash::kPrePipWindowStateTypeKey,
                            old_window_state_type);
    }

    CollectPipEnterExitMetrics(/*enter=*/true);
  } else if (was_pip) {
    if (widget) {
      widget->widget_delegate()->SetCanActivate(true);
      Shell::Get()->focus_cycler()->RemoveWidget(widget);
    }
    ::wm::SetWindowVisibilityAnimationType(
        window(), ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);

    CollectPipEnterExitMetrics(/*enter=*/false);
  }
  // PIP uses restore bounds in its own special context. Reset it in PIP
  // enter/exit transition so that it won't be used wrongly.
  if (IsPip() || was_pip)
    ClearRestoreBounds();
}

void WindowState::UpdatePipBounds() {
  gfx::Rect new_bounds =
      PipPositioner::GetPositionAfterMovementAreaChange(this);
  ::wm::ConvertRectFromScreen(window()->GetRootWindow(), &new_bounds);
  if (window()->bounds() != new_bounds) {
    SetBoundsWMEvent event(new_bounds, /*animate=*/true);
    OnWMEvent(&event);
  }
}

void WindowState::CollectPipEnterExitMetrics(bool enter) {
  const bool is_arc = window_util::IsArcWindow(window());
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

// static
WindowState* WindowState::Get(aura::Window* window) {
  if (!window)
    return nullptr;

  WindowState* state = window->GetProperty(kWindowStateKey);
  if (state)
    return state;

  if (window->type() == aura::client::WINDOW_TYPE_CONTROL)
    return nullptr;

  DCHECK(window->parent());

  if (!IsToplevelContainer(window->parent()))
    return nullptr;

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
  if (key == kWindowPinTypeKey) {
    if (!ignore_property_change_) {
      WMEvent event(WMEventTypeFromWindowPinType(GetPinType()));
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
  if (key == kWindowStateTypeKey) {
    if (!ignore_property_change_) {
      // This change came from somewhere else. Revert it.
      window->SetProperty(kWindowStateTypeKey, GetStateType());
    }
    return;
  }
  if (key == kHideShelfWhenFullscreenKey || key == kImmersiveIsActive) {
    if (!ignore_property_change_) {
      // This change came from outside ash. Update our shelf visibility based
      // on our changed state.
      ash::Shell::Get()->UpdateShelfVisibility();
    }
    return;
  }
}

void WindowState::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window_, window);
  if (::wm::GetTransientParent(window))
    return;
  MoveAllTransientChildrenToNewRoot(window);
}

void WindowState::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window_, window);

  // If the window is destroyed during PIP, count that as exiting.
  if (IsPip())
    CollectPipEnterExitMetrics(/*enter=*/false);

  auto* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (widget)
    Shell::Get()->focus_cycler()->RemoveWidget(widget);

  current_state_->OnWindowDestroying(this);
  delegate_.reset();
}

}  // namespace ash
