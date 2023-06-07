// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/splitview/split_view_divider.h"

#include <memory>

#include "ash/display/screen_orientation_controller.h"
#include "ash/public/cpp/shell_window_ids.h"
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
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "ui/aura/window_targeter.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter_delegate.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/core/transient_window_manager.h"
#include "ui/wm/core/window_util.h"

namespace ash {

SplitViewDivider::SplitViewDivider(SplitViewController* controller)
    : controller_(controller) {
  // Observe currently snapped windows.
  for (auto snap_pos : {SplitViewController::SnapPosition::kPrimary,
                        SplitViewController::SnapPosition::kSecondary}) {
    auto* window = controller_->GetSnappedWindow(snap_pos);
    if (window) {
      AddObservedWindow(window);
    }
  }

  // Create the divider widget after adding observed windows which the parent
  // container of the divider will depend on.
  CreateDividerWidget(controller);
}

SplitViewDivider::~SplitViewDivider() {
  divider_widget_->Close();
  for (auto* window : observed_windows_) {
    window->RemoveObserver(this);
    ::wm::TransientWindowManager::GetOrCreate(window)->RemoveObserver(this);
  }
  dragged_window_ = nullptr;
  observed_windows_.clear();
}

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

void SplitViewDivider::DoSpawningAnimation(int spawning_position) {
  static_cast<SplitViewDividerView*>(divider_widget_->GetContentsView())
      ->DoSpawningAnimation(spawning_position);
}

void SplitViewDivider::UpdateDividerBounds() {
  divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
}

gfx::Rect SplitViewDivider::GetDividerBoundsInScreen(bool is_dragging) {
  const gfx::Rect work_area_bounds_in_screen =
      screen_util::GetDisplayWorkAreaBoundsInScreenForActiveDeskContainer(
          controller_->root_window()->GetChildById(
              desks_util::GetActiveDeskContainerId()));
  const int divider_position = controller_->divider_position();
  const bool landscape = IsCurrentScreenOrientationLandscape();
  return GetDividerBoundsInScreen(work_area_bounds_in_screen, landscape,
                                  divider_position, is_dragging);
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
  CHECK(!base::Contains(observed_windows_, window));
  window->AddObserver(this);
  observed_windows_.push_back(window);
  ::wm::TransientWindowManager* transient_manager =
      ::wm::TransientWindowManager::GetOrCreate(window);
  transient_manager->AddObserver(this);
  for (auto* transient_window : transient_manager->transient_children()) {
    StartObservingTransientChild(transient_window);
  }
  RefreshStackingOrder();
}

void SplitViewDivider::RemoveObservedWindow(aura::Window* window) {
  auto iter = base::ranges::find(observed_windows_, window);
  if (iter != observed_windows_.end()) {
    window->RemoveObserver(this);
    observed_windows_.erase(iter);
    ::wm::TransientWindowManager* transient_manager =
        ::wm::TransientWindowManager::GetOrCreate(window);
    transient_manager->RemoveObserver(this);
    for (auto* transient_window : transient_manager->transient_children()) {
      StopObservingTransientChild(transient_window);
    }
    RefreshStackingOrder();
  }
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
  if (!controller_->InSplitViewMode())
    return;

  // We only care about the bounds change of windows in
  // |transient_windows_observations_|.
  if (!transient_windows_observations_.IsObservingSource(window))
    return;

  // |window|'s transient parent must be one of the windows in
  // |observed_windows_|.
  aura::Window* transient_parent = nullptr;
  for (auto* observed_window : observed_windows_) {
    if (::wm::HasTransientAncestor(window, observed_window)) {
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
  // Skip the recursive update.
  if (pause_update_) {
    return;
  }

  base::AutoReset<bool> lock(&pause_update_, true);
  RefreshStackingOrder();
}

void SplitViewDivider::OnWindowAddedToRootWindow(aura::Window* window) {
  // Stop observing `window` if it no longer belongs to the same root window as
  // of the `controller_`.
  RemoveObservedWindow(window);
}

void SplitViewDivider::OnTransientChildAdded(aura::Window* window,
                                             aura::Window* transient) {
  StartObservingTransientChild(transient);
}

void SplitViewDivider::OnTransientChildRemoved(aura::Window* window,
                                               aura::Window* transient) {
  StopObservingTransientChild(transient);
}

void SplitViewDivider::CreateDividerWidget(SplitViewController* controller) {
  CHECK(!divider_widget_);
  // Native widget owns this widget.
  divider_widget_ = new views::Widget;
  divider_widget_->set_focus_on_creation(false);
  aura::Window* parent_container = nullptr;
  if (observed_windows_.empty()) {
    // `observed_windows_` may still be empty for tablet mode, in this case we
    // need to get a default parent container for the `divider_widget_`.
    // TODO(michelefan): Remove this logic after refactoring the divider
    // creation and removal logic in `SplitViewController`.
    parent_container =
        desks_util::GetActiveDeskContainerForRoot(controller_->root_window());
  } else {
    aura::Window* top_window = window_util::GetTopMostWindow(observed_windows_);
    CHECK(top_window);
    parent_container = top_window->parent();
  }
  CHECK(parent_container);
  divider_widget_->Init(
      CreateWidgetInitParams(parent_container, "SplitViewDivider"));
  divider_widget_->SetVisibilityAnimationTransition(
      views::Widget::ANIMATE_NONE);
  divider_view_ = divider_widget_->SetContentsView(
      std::make_unique<SplitViewDividerView>(controller, this));
  divider_widget_->SetBounds(GetDividerBoundsInScreen(/*is_dragging=*/false));
  auto* divider_widget_native_window = divider_widget_->GetNativeWindow();
  divider_widget_native_window->SetProperty(kLockedToRootKey, true);

  // Use a window targeter and enlarge the hit region to allow located events
  // that are slightly outside the divider widget bounds be consumed by
  // `divider_widget_`.
  auto window_targeter = std::make_unique<aura::WindowTargeter>();
  window_targeter->SetInsets(gfx::Insets::VH(-kSplitViewDividerExtraInset,
                                             -kSplitViewDividerExtraInset));
  divider_widget_native_window->SetEventTargeter(std::move(window_targeter));

  divider_widget_->Show();
}

void SplitViewDivider::RefreshStackingOrder() {
  if (observed_windows_.empty() || !divider_widget_) {
    return;
  }

  aura::Window* top_window = window_util::GetTopMostWindow(observed_windows_);
  CHECK(top_window);
  aura::Window* divider_window = divider_widget_->GetNativeWindow();

  auto* divider_sibling_window =
      dragged_window_ ? dragged_window_.get() : top_window;
  CHECK(divider_sibling_window);

  // The divider needs to have the same parent of the `divider_sibling_window`
  // otherwise we need to reparent the divider as below.
  if (divider_sibling_window->parent() != divider_window->parent()) {
    views::Widget::ReparentNativeView(divider_window,
                                      divider_sibling_window->parent());
    CHECK(!wm::GetTransientParent(divider_window));
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
  for (auto* window : children) {
    if (!base::Contains(observed_windows_, window)) {
      continue;
    }

    if (window == top_window) {
      break;
    }

    top_window_parent->StackChildAbove(window, top_window);
    top_window_parent->StackChildAbove(top_window, window);
  }

  top_window_parent->StackChildAbove(divider_window, top_window);
}

void SplitViewDivider::StartObservingTransientChild(aura::Window* transient) {
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

}  // namespace ash
