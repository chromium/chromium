// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider.h"

#include <algorithm>
#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/snap_group/snap_group_controller.h"
#include "ash/wm/splitview/split_view_constants.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/splitview/split_view_divider_view.h"
#include "ash/wm/splitview/split_view_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_util.h"
#include "base/auto_reset.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/window_targeter.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

namespace {

gfx::Point GetBoundedPosition(const gfx::Point& location_in_screen,
                              const gfx::Rect& bounds_in_screen) {
  return gfx::Point(std::clamp(location_in_screen.x(), bounds_in_screen.x(),
                               bounds_in_screen.right() - 1),
                    std::clamp(location_in_screen.y(), bounds_in_screen.y(),
                               bounds_in_screen.bottom() - 1));
}

gfx::Rect GetWorkAreaBoundsInScreen(aura::Window* window) {
  return screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
      window);
}

// Returns the widget init params needed to create the widget.
views::Widget::InitParams CreateWidgetInitParams(
    aura::Window* parent_window,
    const std::string& widget_name) {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.opacity = views::Widget::InitParams::WindowOpacity::kOpaque;
  params.activatable = views::Widget::InitParams::Activatable::kNo;
  params.parent = parent_window;
  params.init_properties_container.SetProperty(kHideInDeskMiniViewKey, true);
  // Exclude the divider from getting transformed with its transient parent
  // window when we are resizing. The divider will set its own transforms.
  params.init_properties_container.SetProperty(
      kExcludeFromTransientTreeTransformKey, true);
  params.name = widget_name;
  return params;
}

}  // namespace

SplitViewDivider::SplitViewDivider(LayoutDividerController* controller)
    : controller_(controller) {}

SplitViewDivider::~SplitViewDivider() = default;

// static
gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(
    const gfx::Rect& work_area_bounds_in_screen,
    bool landscape,
    int divider_position,
    bool is_dragging) {
  const int dragging_diff = (kSplitviewDividerEnlargedShortSideLength -
                             kSplitviewDividerShortSideLength) /
                            2;
  if (landscape) {
    return is_dragging
               ? gfx::Rect(work_area_bounds_in_screen.x() + divider_position -
                               dragging_diff,
                           work_area_bounds_in_screen.y(),
                           kSplitviewDividerEnlargedShortSideLength,
                           work_area_bounds_in_screen.height())
               : gfx::Rect(work_area_bounds_in_screen.x() + divider_position,
                           work_area_bounds_in_screen.y(),
                           kSplitviewDividerShortSideLength,
                           work_area_bounds_in_screen.height());
  } else {
    return is_dragging
               ? gfx::Rect(work_area_bounds_in_screen.x(),
                           work_area_bounds_in_screen.y() + divider_position -
                               dragging_diff,
                           work_area_bounds_in_screen.width(),
                           kSplitviewDividerEnlargedShortSideLength)
               : gfx::Rect(work_area_bounds_in_screen.x(),
                           work_area_bounds_in_screen.y() + divider_position,
                           work_area_bounds_in_screen.width(),
                           kSplitviewDividerShortSideLength);
  }
}

void SplitViewDivider::ShutDown() {
  if (divider_view_) {
    divider_view_->OnShuttingDown();
  }
  CloseDividerWidget();
}

bool SplitViewDivider::HasDividerWidget() const {
  return !!divider_widget_;
}

void SplitViewDivider::ShowFor(int divider_position) {
  divider_position_ = divider_position;
  CloseDividerWidget();

  // Order here matters: we first refresh the observed windows, since the widget
  // will be added to the topmost of `observed_windows_`. Then after the widget
  // is created, we refresh the stacking order of all the windows.
  for (aura::Window* window : controller_->GetLayoutWindows()) {
    AddObservedWindow(window);
  }
  CreateDividerWidget(divider_position);
  RefreshStackingOrder();
}

void SplitViewDivider::CloseDividerWidget() {
  for (aura::Window* window : observed_windows_) {
    window->RemoveObserver(this);
    wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);
  }
  observed_windows_.clear();
  transient_windows_observations_.RemoveAllObservations();

  dragged_window_ = nullptr;

  if (divider_widget_) {
    // Disable any event handling on the divider while we are closing the
    // widget.
    divider_view_->SetCanProcessEventsWithinSubtree(false);
    divider_widget_->GetNativeWindow()->SetEventTargetingPolicy(
        aura::EventTargetingPolicy::kNone);
    divider_view_ = nullptr;
    divider_widget_->Close();
    divider_widget_ = nullptr;
  }
}

void SplitViewDivider::UpdateDividerPosition(
    const gfx::Point& location_in_screen) {
  if (IsLayoutHorizontal(GetRootWindow())) {
    divider_position_ += location_in_screen.x() - previous_event_location_.x();
  } else {
    divider_position_ += location_in_screen.y() - previous_event_location_.y();
  }
  divider_position_ = std::max(0, divider_position_);
}

void SplitViewDivider::StartResizeWithDivider(
    const gfx::Point& location_in_screen) {
  // `is_resizing_with_divider_` may be true here, because you can start
  // dragging the divider with a pointing device while already dragging it by
  // touch, or vice versa. It is possible by using the emulator or
  // chrome://flags/#force-tablet-mode. Bailing out here does not stop the user
  // from dragging by touch and with a pointing device simultaneously; it just
  // avoids duplicate calls to `CreateDragDetails()` and `OnDragStarted()`. We
  // also bail out here if you try to start dragging the divider during its snap
  // animation.
  // TODO(sophiewen): Consider refactoring `DividerSnapAnimation` to here.
  if (is_resizing_with_divider_ ||
      SplitViewController::Get(GetRootWindow())->IsDividerAnimating()) {
    return;
  }

  is_resizing_with_divider_ = true;
  UpdateDividerBounds();
  previous_event_location_ = location_in_screen;

  controller_->StartResizeWithDivider(location_in_screen);

  for (aura::Window* window : observed_windows_) {
    if (window == nullptr) {
      continue;
    }

    WindowState* window_state = WindowState::Get(window);
    gfx::Point location_in_parent(location_in_screen);
    wm::ConvertPointFromScreen(window->parent(), &location_in_parent);
    int window_component = GetWindowComponentForResize(window);
    window_state->CreateDragDetails(gfx::PointF(location_in_parent),
                                    window_component,
                                    wm::WINDOW_MOVE_SOURCE_TOUCH);

    window_state->OnDragStarted(window_component);
  }
}

void SplitViewDivider::ResizeWithDivider(const gfx::Point& location_in_screen) {
  if (!is_resizing_with_divider_) {
    return;
  }

  base::AutoReset<bool> auto_reset(&processing_resize_event_, true);

  const gfx::Rect work_area_bounds =
      GetWorkAreaBoundsInScreen(divider_widget_->GetNativeWindow());
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);

  // Order here matters: we first update `divider_position_`, then the
  // LayoutDividerController will transform and update the window and divider
  // bounds in `UpdateResizeWithDivider()`.
  UpdateDividerPosition(modified_location_in_screen);
  controller_->UpdateResizeWithDivider(modified_location_in_screen);

  previous_event_location_ = modified_location_in_screen;
}

void SplitViewDivider::EndResizeWithDivider(
    const gfx::Point& location_in_screen) {
  if (!is_resizing_with_divider_) {
    return;
  }

  is_resizing_with_divider_ = false;

  const gfx::Rect work_area_bounds =
      GetWorkAreaBoundsInScreen(divider_widget_->GetNativeWindow());
  gfx::Point modified_location_in_screen =
      GetBoundedPosition(location_in_screen, work_area_bounds);

  // Order here matters: we first update `divider_position_`, then the
  // LayoutDividerController will transform and update the window and divider
  // bounds in `EndResizeWithDivider()`.
  UpdateDividerPosition(modified_location_in_screen);

  // If the delegate is done with resizing, finish resizing and clean up.
  // Otherwise it will be called later, in
  // `DividerSnapAnimation::AnimationEnded()`.
  if (controller_->EndResizeWithDivider(modified_location_in_screen)) {
    CleanUpWindowResizing();
  }
}

void SplitViewDivider::CleanUpWindowResizing() {
  is_resizing_with_divider_ = false;
  // Always call `OnResizeEnding()` since `CleanUpWindowResizing()` may be after
  // an animation and we need to restore the window transforms.
  controller_->OnResizeEnding();
  FinishWindowResizing();
  controller_->OnResizeEnded();
}

void SplitViewDivider::DoSpawningAnimation(int spawning_position) {
  static_cast<SplitViewDividerView*>(divider_widget_->GetContentsView())
      ->DoSpawningAnimation(spawning_position);
}

void SplitViewDivider::UpdateDividerBounds() {
  divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
}

gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(bool is_dragging) {
  const gfx::Rect work_area_bounds_in_screen =
      GetWorkAreaBoundsInScreen(divider_widget_->GetNativeWindow());
  const bool landscape = IsCurrentScreenOrientationLandscape();
  return GetDividerBoundsInScreen(work_area_bounds_in_screen, landscape,
                                  divider_position_, is_dragging);
}

void SplitViewDivider::SetAdjustable(bool adjustable) {
  if (adjustable == IsAdjustable()) {
    return;
  }

  divider_widget_->GetNativeWindow()->SetEventTargetingPolicy(
      adjustable ? aura::EventTargetingPolicy::kTargetAndDescendants
                 : aura::EventTargetingPolicy::kNone);
  static_cast<SplitViewDividerView*>(divider_view_)
      ->SetDividerBarVisible(adjustable);
}

bool SplitViewDivider::IsAdjustable() const {
  DCHECK(divider_widget_);
  DCHECK(divider_widget_->GetNativeView());
  return divider_widget_->GetNativeWindow()->event_targeting_policy() !=
         aura::EventTargetingPolicy::kNone;
}

void SplitViewDivider::AddObservedWindow(aura::Window* window) {
  // TODO(b/322890782): Change this back to a CHECK and add `window` directly.
  if (base::Contains(observed_windows_, window)) {
    return;
  }
  window->AddObserver(this);
  observed_windows_.push_back(window);
  wm::TransientWindowManager* transient_manager =
      wm::TransientWindowManager::GetOrCreate(window);
  transient_manager->AddObserver(this);
  for (aura::Window* transient_window :
       transient_manager->transient_children()) {
    StartObservingTransientChild(transient_window);
  }
  // Don't refresh here, since we may not have created the divider widget yet.
}

void SplitViewDivider::RemoveObservedWindow(aura::Window* window) {
  auto iter = base::ranges::find(observed_windows_, window);
  if (iter != observed_windows_.end()) {
    window->RemoveObserver(this);
    observed_windows_.erase(iter);
    wm::TransientWindowManager* transient_manager =
        wm::TransientWindowManager::GetOrCreate(window);
    transient_manager->RemoveObserver(this);
    for (aura::Window* transient_window :
         transient_manager->transient_children()) {
      StopObservingTransientChild(transient_window);
    }
    RefreshStackingOrder();
  }
}

void SplitViewDivider::OnKeyboardOccludedBoundsChangedInPortrait(
    const gfx::Rect& work_area,
    int y) {
  // If the divider widget doesn't exist, i.e. in clamshell split view, we are
  // done.
  if (!divider_widget_) {
    return;
  }

  CHECK(!IsLayoutHorizontal(GetRootWindow()));

  // Else subtract the divider width and update the widget bounds. Note we
  // *don't* update `divider_position_` since it may be used to restore the
  // window bounds in `SplitViewController::OnWindowActivated()`.
  // TODO(b/331459348): Investigate why we don't update `divider_position_` and
  // fix this code.
  const int divider_position = y - kSplitviewDividerShortSideLength;
  divider_widget_->SetBounds(
      GetDividerBoundsInScreen(work_area, /*landscape=*/false, divider_position,
                               /*is_dragging=*/false));

  // Make split view divider unadjustable.
  SetAdjustable(false);
}

void SplitViewDivider::OnWindowDragStarted(aura::Window* dragged_window) {
  dragged_window_ = dragged_window;
  RefreshStackingOrder();
}

void SplitViewDivider::OnWindowDragEnded() {
  dragged_window_ = nullptr;
  RefreshStackingOrder();
}

void SplitViewDivider::OnWindowDestroying(aura::Window* window) {
  RemoveObservedWindow(window);
}

void SplitViewDivider::OnWindowBoundsChanged(aura::Window* window,
                                             const gfx::Rect& old_bounds,
                                             const gfx::Rect& new_bounds,
                                             ui::PropertyChangeReason reason) {
  if (is_resizing_with_divider_ &&
      display::Screen::GetScreen()->InTabletMode() &&
      base::Contains(observed_windows_, window)) {
    // Bounds may be changed while we are processing a resize event. In this
    // case, we don't update the windows transform here, since it will be done
    // soon anyway. If we are *not* currently processing a resize, it means the
    // bounds of a window have been updated "async", and we need to update the
    // window's transform.
    if (!processing_resize_event_) {
      // TODO(b/308819668): Remove this reference to `SplitViewController` when
      // we move `divider_position` to here.
      const int divider_position =
          SplitViewController::Get(GetRootWindow())->GetDividerPosition();
      for (aura::Window* window_to_transform : observed_windows_) {
        SetWindowTransformDuringResizing(window_to_transform, divider_position);
      }
    }
  }

  // We only care about the bounds change of windows in
  // |transient_windows_observations_|.
  if (!transient_windows_observations_.IsObservingSource(window))
    return;

  // |window|'s transient parent must be one of the windows in
  // |observed_windows_|.
  aura::Window* transient_parent = nullptr;
  for (aura::Window* observed_window : observed_windows_) {
    if (wm::HasTransientAncestor(window, observed_window)) {
      transient_parent = observed_window;
      break;
    }
  }
  DCHECK(transient_parent);

  gfx::Rect transient_bounds = window->GetBoundsInScreen();
  transient_bounds.AdjustToFit(transient_parent->GetBoundsInScreen());
  window->SetBoundsInScreen(
      transient_bounds,
      display::Screen::GetScreen()->GetDisplayNearestWindow(window));
}

void SplitViewDivider::OnWindowStackingChanged(aura::Window* window) {
  RefreshStackingOrder();
}

void SplitViewDivider::OnWindowAddedToRootWindow(aura::Window* window) {
  // Stop observing `window` if it no longer belongs to the same root window as
  // of the `controller_`.
  RemoveObservedWindow(window);
}

void SplitViewDivider::OnWindowVisibilityChanged(aura::Window* window,
                                                 bool visible) {
  RefreshStackingOrder();
}

void SplitViewDivider::OnTransientChildAdded(aura::Window* window,
                                             aura::Window* transient) {
  StartObservingTransientChild(transient);
}

void SplitViewDivider::OnTransientChildRemoved(aura::Window* window,
                                               aura::Window* transient) {
  StopObservingTransientChild(transient);
}

void SplitViewDivider::CreateDividerWidget(int divider_position) {
  CHECK_GE(observed_windows_.size(), 1u);
  // Native widget owns this widget.
  divider_widget_ = new views::Widget;
  divider_widget_->set_focus_on_creation(false);
  aura::Window* parent_container = nullptr;
  aura::Window* top_window = window_util::GetTopMostWindow(observed_windows_);
  CHECK(top_window);
  parent_container = top_window->parent();
  CHECK(parent_container);
  divider_widget_->Init(
      CreateWidgetInitParams(parent_container, "SplitViewDivider"));
  divider_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  // TODO(b/314018158): Remove `SplitViewController` from
  // `SplitViewDividerView`.
  divider_view_ =
      divider_widget_->SetContentsView(std::make_unique<SplitViewDividerView>(
          SplitViewController::Get(top_window->GetRootWindow()), this));
  divider_widget_->SetBounds(GetDividerBoundsInScreen(
      /*work_area_bounds_in_screen=*/GetWorkAreaBoundsInScreen(
          observed_windows_.front()),
      /*landscape=*/IsCurrentScreenOrientationLandscape(), divider_position,
      /*is_dragging=*/false));
  auto* divider_widget_native_window = divider_widget_->GetNativeWindow();
  divider_widget_native_window->SetProperty(kLockedToRootKey, true);

  // Use a window targeter and enlarge the hit region to allow located events
  // that are slightly outside the divider widget bounds be consumed by
  // `divider_widget_`.
  auto window_targeter = std::make_unique<aura::WindowTargeter>();
  window_targeter->SetInsets(gfx::Insets::VH(-kSplitViewDividerExtraInset,
                                             -kSplitViewDividerExtraInset));
  divider_widget_native_window->SetEventTargeter(std::move(window_targeter));

  // Explicitly `set_parent_controls_lifetime` to false so that the lifetime of
  // the divider will only be managed by `this`, which avoids UAF on window
  // destroying.
  wm::TransientWindowManager::GetOrCreate(divider_widget_native_window)
      ->set_parent_controls_lifetime(false);
  divider_widget_->Show();
}

aura::Window* SplitViewDivider::GetRootWindow() const {
  return divider_widget_->GetNativeWindow()->GetRootWindow();
}

void SplitViewDivider::RefreshStackingOrder() {
  // Skip the recursive update.
  if (pause_update_) {
    return;
  }

  base::AutoReset<bool> lock(&pause_update_, true);

  if (observed_windows_.empty() || !divider_widget_) {
    return;
  }

  aura::Window::Windows visible_observed_windows;
  for (aura::Window* window : observed_windows_) {
    if (window->IsVisible()) {
      visible_observed_windows.push_back(window);
    }
  }

  aura::Window* divider_window = divider_widget_->GetNativeWindow();
  if (visible_observed_windows.empty()) {
    divider_window->Hide();
    return;
  }

  aura::Window* top_window =
      window_util::GetTopMostWindow(visible_observed_windows);
  CHECK(top_window);
  CHECK(top_window->IsVisible());

  auto* divider_sibling_window =
      dragged_window_ ? dragged_window_.get() : top_window;
  CHECK(divider_sibling_window);

  // To get `divider_window` prepared to be the transient window of the
  // `top_window` below, remove `divider_window` as the transient child from its
  // transient parent if any.
  auto* transient_parent = wm::GetTransientParent(divider_window);
  if (transient_parent) {
    wm::RemoveTransientChild(transient_parent, divider_window);
  }

  CHECK(!wm::GetTransientParent(divider_window));

  // The divider needs to have the same parent of the `divider_sibling_window`
  // otherwise we need to reparent the divider as below.
  if (divider_sibling_window->parent() != divider_window->parent()) {
    views::Widget::ReparentNativeView(divider_window,
                                      divider_sibling_window->parent());
  }

  if (dragged_window_) {
    divider_window->parent()->StackChildBelow(divider_window, dragged_window_);
    return;
  }

  // Refresh the stacking order of the other window.
  aura::Window* top_window_parent = top_window->parent();
  // Keep a copy as the order of children will be changed while iterating.
  const auto children = top_window_parent->children();

  // Iterate through the siblings of the top window in an increasing z-order
  // which reflects the relative order of siblings.
  for (aura::Window* window : children) {
    if (!base::Contains(visible_observed_windows, window) ||
        window == top_window) {
      continue;
    }

    top_window_parent->StackChildAbove(window, top_window);
    top_window_parent->StackChildAbove(top_window, window);
  }

  // Add the `divider_window` as a transient child of the `top_window`. In
  // this way, on new transient window added, the divider will be stacked above
  // the `top_window` but under the new transient window which is handled in
  // `TransientWindowManager::RestackTransientDescendants()`.
  wm::AddTransientChild(top_window, divider_window);

  top_window_parent->StackChildAbove(divider_window, top_window);
  divider_window->Show();
}

void SplitViewDivider::StartObservingTransientChild(aura::Window* transient) {
  // Explicitly check and early return if the `transient` is the divider native
  // window.
  if (divider_widget_ && transient == divider_widget_->GetNativeWindow()) {
    return;
  }

  // For now, we only care about dialog bubbles type transient child. We may
  // observe other types transient child window as well if need arises in the
  // future.
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(transient);
  if (!widget || !widget->widget_delegate()->AsBubbleDialogDelegate())
    return;

  // At this moment, the transient window may not have the valid bounds yet.
  // Start observe the transient window.
  transient_windows_observations_.AddObservation(transient);
}

void SplitViewDivider::StopObservingTransientChild(aura::Window* transient) {
  if (transient_windows_observations_.IsObservingSource(transient))
    transient_windows_observations_.RemoveObservation(transient);
}

gfx::Point SplitViewDivider::GetEndDragLocationInScreen(
    aura::Window* window) const {
  DCHECK(base::Contains(observed_windows_, window));
  gfx::Point end_location(previous_event_location_);

  const SnapPosition snap_position =
      controller_->GetPositionOfSnappedWindow(window);
  const gfx::Rect bounds = controller_->GetSnappedWindowBoundsInScreen(
      snap_position, window, window_util::GetSnapRatioForWindow(window));

  const bool is_physical_left_or_top =
      IsPhysicalLeftOrTop(snap_position, window);
  if (IsLayoutHorizontal(window)) {
    end_location.set_x(is_physical_left_or_top ? bounds.right() : bounds.x());
  } else {
    end_location.set_y(is_physical_left_or_top ? bounds.bottom() : bounds.y());
  }
  return end_location;
}

void SplitViewDivider::FinishWindowResizing() {
  for (aura::Window* window : observed_windows_) {
    WindowState* window_state = WindowState::Get(window);
    if (window_state->is_dragged()) {
      window_state->OnCompleteDrag(
          gfx::PointF(GetEndDragLocationInScreen(window)));
      window_state->DeleteDragDetails();
    }
  }
}

}  // namespace ash
