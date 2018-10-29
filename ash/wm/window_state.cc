// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_state.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/window_animation_types.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/public/interfaces/window_state_type.mojom.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
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
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/painter.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/ime_util_chromeos.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace wm {
namespace {

// TODO(edcourtney): Move this to a PIP specific file, once it's created.
const int kPipRoundedCornerRadius = 8;

bool IsTabletModeEnabled() {
  return Shell::Get()
      ->tablet_mode_controller()
      ->IsTabletModeWindowManagerEnabled();
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

WMEventType WMEventTypeFromWindowPinType(ash::mojom::WindowPinType type) {
  switch (type) {
    case ash::mojom::WindowPinType::NONE:
      return WM_EVENT_NORMAL;
    case ash::mojom::WindowPinType::PINNED:
      return WM_EVENT_PIN;
    case ash::mojom::WindowPinType::TRUSTED_PINNED:
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

}  // namespace

constexpr base::TimeDelta WindowState::kBoundsChangeSlideDuration;

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
  DCHECK(!delegate_.get());
  delegate_ = std::move(delegate);
}

mojom::WindowStateType WindowState::GetStateType() const {
  return current_state_->GetType();
}

bool WindowState::IsMinimized() const {
  return GetStateType() == mojom::WindowStateType::MINIMIZED;
}

bool WindowState::IsMaximized() const {
  return GetStateType() == mojom::WindowStateType::MAXIMIZED;
}

bool WindowState::IsFullscreen() const {
  return GetStateType() == mojom::WindowStateType::FULLSCREEN;
}

bool WindowState::IsMaximizedOrFullscreenOrPinned() const {
  return GetStateType() == mojom::WindowStateType::MAXIMIZED ||
         GetStateType() == mojom::WindowStateType::FULLSCREEN || IsPinned();
}

bool WindowState::IsSnapped() const {
  return GetStateType() == mojom::WindowStateType::LEFT_SNAPPED ||
         GetStateType() == mojom::WindowStateType::RIGHT_SNAPPED;
}

bool WindowState::IsPinned() const {
  return GetStateType() == mojom::WindowStateType::PINNED ||
         GetStateType() == mojom::WindowStateType::TRUSTED_PINNED;
}

bool WindowState::IsTrustedPinned() const {
  return GetStateType() == mojom::WindowStateType::TRUSTED_PINNED;
}

bool WindowState::IsPip() const {
  return GetStateType() == mojom::WindowStateType::PIP;
}

bool WindowState::IsNormalStateType() const {
  return GetStateType() == mojom::WindowStateType::NORMAL ||
         GetStateType() == mojom::WindowStateType::DEFAULT;
}

bool WindowState::IsNormalOrSnapped() const {
  return IsNormalStateType() || IsSnapped();
}

bool WindowState::IsActive() const {
  return ::wm::IsActiveWindow(window_);
}

bool WindowState::IsUserPositionable() const {
  return (window_->type() == aura::client::WINDOW_TYPE_NORMAL ||
          window_->type() == aura::client::WINDOW_TYPE_PANEL);
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
       ws::mojom::kResizeBehaviorCanMaximize) == 0) {
    return false;
  }

  return !HasMaximumWidthOrHeight();
}

bool WindowState::CanMinimize() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          ws::mojom::kResizeBehaviorCanMinimize) != 0;
}

bool WindowState::CanResize() const {
  return (window_->GetProperty(aura::client::kResizeBehaviorKey) &
          ws::mojom::kResizeBehaviorCanResize) != 0;
}

bool WindowState::CanActivate() const {
  return ::wm::CanActivateWindow(window_);
}

bool WindowState::CanSnap() const {
  const bool is_panel_window =
      window_->type() == aura::client::WINDOW_TYPE_PANEL;

  if (!CanResize() || is_panel_window || IsPip())
    return false;

  // Allow windows with no maximum width or height to be snapped.
  // TODO(oshima): We should probably snap if the maximum size is defined
  // and greater than the snapped size.
  return !HasMaximumWidthOrHeight();
}

bool WindowState::HasRestoreBounds() const {
  return window_->GetProperty(aura::client::kRestoreBoundsKey) != nullptr;
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

void WindowState::DisableAlwaysOnTop(aura::Window* window_on_top) {
  if (GetAlwaysOnTop()) {
    // |window_| is hidden first to avoid canceling fullscreen mode when it is
    // no longer always on top and gets added to default container. This avoids
    // sending redundant OnFullscreenStateChanged to the layout manager. The
    // |window_| visibility is restored after it no longer obscures the
    // |window_on_top|.
    bool visible = window_->IsVisible();
    if (visible)
      window_->Hide();
    window_->SetProperty(aura::client::kAlwaysOnTopKey, false);
    // Technically it is possible that a |window_| could make itself
    // always_on_top really quickly. This is probably not a realistic case but
    // check if the two windows are in the same container just in case.
    if (window_on_top && window_on_top->parent() == window_->parent())
      window_->parent()->StackChildAbove(window_on_top, window_);
    if (visible)
      window_->Show();
    cached_always_on_top_ = true;
  }
}

void WindowState::RestoreAlwaysOnTop() {
  if (cached_always_on_top_) {
    cached_always_on_top_ = false;
    window_->SetProperty(aura::client::kAlwaysOnTopKey, true);
  }
}

void WindowState::OnWMEvent(const WMEvent* event) {
  current_state_->OnWMEvent(this, event);

  UpdateSnappedWidthRatio(event);
}

void WindowState::SaveCurrentBoundsForRestore() {
  gfx::Rect bounds_in_screen = window_->bounds();
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
  window_->SetProperty(aura::client::kRestoreBoundsKey, new gfx::Rect(bounds));
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
  return window_->GetProperty(aura::client::kImmersiveFullscreenKey);
}

void WindowState::SetInImmersiveFullscreen(bool enabled) {
  base::AutoReset<bool> resetter(&ignore_property_change_, true);
  window_->SetProperty(aura::client::kImmersiveFullscreenKey, enabled);
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
      ignored_by_shelf_(false),
      can_consume_system_keys_(false),
      unminimize_to_restore_bounds_(false),
      hide_shelf_when_fullscreen_(true),
      autohide_shelf_when_maximized_or_fullscreen_(false),
      cached_always_on_top_(false),
      ignore_property_change_(false),
      current_state_(new DefaultState(ToWindowStateType(GetShowState()))) {
  window_->AddObserver(this);
  UpdatePipState();
}

bool WindowState::GetAlwaysOnTop() const {
  return window_->GetProperty(aura::client::kAlwaysOnTopKey);
}

ui::WindowShowState WindowState::GetShowState() const {
  return window_->GetProperty(aura::client::kShowStateKey);
}

ash::mojom::WindowPinType WindowState::GetPinType() const {
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
  if (GetStateType() == mojom::WindowStateType::LEFT_SNAPPED)
    bounds->set_x(maximized_bounds.x());
  else if (GetStateType() == mojom::WindowStateType::RIGHT_SNAPPED)
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
  ash::mojom::WindowPinType pin_type = ash::mojom::WindowPinType::NONE;
  if (GetStateType() == mojom::WindowStateType::PINNED)
    pin_type = ash::mojom::WindowPinType::PINNED;
  else if (GetStateType() == mojom::WindowStateType::TRUSTED_PINNED)
    pin_type = ash::mojom::WindowPinType::TRUSTED_PINNED;
  if (pin_type != GetPinType()) {
    base::AutoReset<bool> resetter(&ignore_property_change_, true);
    window_->SetProperty(kWindowPinTypeKey, pin_type);
  }
}

void WindowState::NotifyPreStateTypeChange(
    mojom::WindowStateType old_window_state_type) {
  for (auto& observer : observer_list_)
    observer.OnPreWindowStateTypeChange(this, old_window_state_type);
}

void WindowState::NotifyPostStateTypeChange(
    mojom::WindowStateType old_window_state_type) {
  for (auto& observer : observer_list_)
    observer.OnPostWindowStateTypeChange(this, old_window_state_type);
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
  ::wm::SnapWindowToPixelBoundary(window_);
}

void WindowState::SetBoundsConstrained(const gfx::Rect& bounds) {
  gfx::Rect work_area_in_parent =
      screen_util::GetDisplayWorkAreaBoundsInParent(window_);
  gfx::Rect child_bounds(bounds);
  AdjustBoundsSmallerThan(work_area_in_parent.size(), &child_bounds);
  SetBoundsDirect(child_bounds);
}

void WindowState::SetBoundsDirectAnimated(const gfx::Rect& bounds,
                                          base::TimeDelta duration) {
  ui::Layer* layer = window_->layer();
  ui::ScopedLayerAnimationSettings slide_settings(layer->GetAnimator());
  slide_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  slide_settings.SetTransitionDuration(duration);
  SetBoundsDirect(bounds);
}

void WindowState::SetBoundsDirectCrossFade(const gfx::Rect& new_bounds,
                                           gfx::Tween::Type animation_type) {
  // Some test results in invoking CrossFadeToBounds when window is not visible.
  // No animation is necessary in that case, thus just change the bounds and
  // quit.
  if (!window_->TargetVisibility()) {
    SetBoundsConstrained(new_bounds);
    return;
  }

  // If the window already has a transform in place, do not use the cross fade
  // animation, set the bounds directly instead.
  if (!window_->layer()->GetTargetTransform().IsIdentity()) {
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

  CrossFadeAnimation(window_, std::move(old_layer_owner), animation_type);
}

void WindowState::UpdatePipRoundedCorners() {
  auto* layer = window()->layer();
  if (!IsPip()) {
    // Only remove the mask layer if it is from the existing PIP mask.
    if (layer && pip_mask_ && layer->layer_mask_layer() == pip_mask_->layer())
      layer->SetMaskLayer(nullptr);
    pip_mask_.reset();
    return;
  }

  gfx::Rect bounds = window()->bounds();
  if (layer && (!pip_mask_ || pip_mask_->layer()->size() != bounds.size())) {
    layer->SetMaskLayer(nullptr);
    pip_mask_ = views::Painter::CreatePaintedLayer(
        views::Painter::CreateSolidRoundRectPainter(SK_ColorBLACK,
                                                    kPipRoundedCornerRadius));
    pip_mask_->layer()->SetBounds(bounds);
    pip_mask_->layer()->SetFillsBoundsOpaquely(false);
    layer->SetFillsBoundsOpaquely(false);
    layer->SetMaskLayer(pip_mask_->layer());
  }
}

void WindowState::UpdatePipState() {
  ::wm::SetWindowVisibilityAnimationType(
      window(), IsPip() ? WINDOW_VISIBILITY_ANIMATION_TYPE_SLIDE_OUT
                        : ::wm::WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT);
}

void WindowState::UpdatePipBounds() {
  gfx::Rect new_bounds =
      PipPositioner::GetPositionAfterMovementAreaChange(this);
  if (window()->GetBoundsInScreen() != new_bounds) {
    wm::SetBoundsEvent event(wm::WM_EVENT_SET_BOUNDS, new_bounds,
                             /*animate=*/true);
    OnWMEvent(&event);
  }
}

WindowState* GetActiveWindowState() {
  aura::Window* active = GetActiveWindow();
  return active ? GetWindowState(active) : nullptr;
}

WindowState* GetWindowState(aura::Window* window) {
  if (!window)
    return nullptr;
  WindowState* settings = window->GetProperty(kWindowStateKey);
  if (!settings) {
    settings = new WindowState(window);
    window->SetProperty(kWindowStateKey, settings);
  }
  return settings;
}

const WindowState* GetWindowState(const aura::Window* window) {
  return GetWindowState(const_cast<aura::Window*>(window));
}

void WindowState::OnWindowPropertyChanged(aura::Window* window,
                                          const void* key,
                                          intptr_t old) {
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
  if (key == kWindowStateTypeKey) {
    if (!ignore_property_change_) {
      // This change came from somewhere else. Revert it.
      window->SetProperty(kWindowStateTypeKey, GetStateType());
    }
    return;
  }
  if (key == kHideShelfWhenFullscreenKey ||
      key == aura::client::kImmersiveFullscreenKey) {
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
  current_state_->OnWindowDestroying(this);
  delegate_.reset();
}

}  // namespace wm
}  // namespace ash
