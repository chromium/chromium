// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_highlight_controller.h"

#include "ash/magnifier/docked_magnifier_controller_impl.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/shell.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/new_desk_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/view.h"

namespace ash {

// -----------------------------------------------------------------------------
// OverviewHighlightController::OverviewHighlightableView

void OverviewHighlightController::OverviewHighlightableView::
    SetHighlightVisibility(bool visible) {
  if (visible == is_highlighted_)
    return;

  is_highlighted_ = visible;
  if (is_highlighted_)
    OnViewHighlighted();
  else
    OnViewUnhighlighted();
}

gfx::Point OverviewHighlightController::OverviewHighlightableView::
    GetMagnifierFocusPointInScreen() {
  return GetView()->GetBoundsInScreen().CenterPoint();
}

// -----------------------------------------------------------------------------
// OverviewHighlightController::TestApi

OverviewHighlightController::TestApi::TestApi(
    OverviewHighlightController* highlight_controller)
    : highlight_controller_(highlight_controller) {}

OverviewHighlightController::TestApi::~TestApi() = default;

OverviewHighlightController::OverviewHighlightableView*
OverviewHighlightController::TestApi::GetHighlightView() const {
  return highlight_controller_->highlighted_view_;
}

// -----------------------------------------------------------------------------
// OverviewHighlightController

OverviewHighlightController::OverviewHighlightController(
    OverviewSession* overview_session)
    : overview_session_(overview_session) {}

OverviewHighlightController::~OverviewHighlightController() = default;

void OverviewHighlightController::MoveHighlight(bool reverse) {
  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  const int count = int{traversable_views.size()};

  // |count| can be zero when there are no overview items and no desk views (eg.
  // "No recent items" or PIP windows are shown but they aren't traversable).
  if (count == 0)
    return;

  int index = 0;
  if (!highlighted_view_) {
    // Pick up where we left off if |deleted_index_| has a value.
    if (deleted_index_) {
      index = *deleted_index_ >= count ? count - 1 : *deleted_index_;
      deleted_index_.reset();
    } else if (reverse) {
      index = count - 1;
    }
  } else {
    auto it = std::find(traversable_views.begin(), traversable_views.end(),
                        highlighted_view_);
    DCHECK(it != traversable_views.end());
    const int current_index = std::distance(traversable_views.begin(), it);
    DCHECK_GE(current_index, 0);
    index = (((reverse ? -1 : 1) + current_index) + count) % count;
  }

  UpdateHighlight(traversable_views[index]);
}

void OverviewHighlightController::MoveHighlightToView(
    OverviewHighlightableView* target_view) {
  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  auto it = std::find(traversable_views.begin(), traversable_views.end(),
                      target_view);
  DCHECK(it != traversable_views.end());

  UpdateHighlight(target_view, /*suppress_accessibility_event=*/true);
}

void OverviewHighlightController::OnViewDestroyingOrDisabling(
    OverviewHighlightableView* view) {
  DCHECK(view);

  // TODO(afakhry): Refactor this code.
  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  const auto it =
      std::find(traversable_views.begin(), traversable_views.end(), view);
  if (it == traversable_views.end())
    return;

  const int view_index = std::distance(traversable_views.begin(), it);
  DCHECK_GE(view_index, 0);

  if (view != highlighted_view_) {
    if (!deleted_index_)
      return;

    // We need to update the |deleted_index_| in case the destroying view
    // resides before a previously removed highlighted view in the highlight
    // order.
    if (view_index < *deleted_index_)
      deleted_index_ = std::max(0, --(*deleted_index_));
    return;
  }

  deleted_index_ = view_index;
  highlighted_view_->SetHighlightVisibility(false);
  highlighted_view_ = nullptr;
}

void OverviewHighlightController::SetFocusHighlightVisibility(bool visible) {
  if (highlighted_view_)
    highlighted_view_->SetHighlightVisibility(visible);
}

bool OverviewHighlightController::IsFocusHighlightVisible() const {
  return highlighted_view_ && highlighted_view_->IsViewHighlighted();
}

bool OverviewHighlightController::MaybeActivateHighlightedView() {
  if (!highlighted_view_)
    return false;

  highlighted_view_->MaybeActivateHighlightedView();
  return true;
}

bool OverviewHighlightController::MaybeCloseHighlightedView() {
  if (!highlighted_view_)
    return false;

  highlighted_view_->MaybeCloseHighlightedView();
  return true;
}

OverviewItem* OverviewHighlightController::GetHighlightedItem() const {
  if (!highlighted_view_)
    return nullptr;

  for (auto& grid : overview_session_->grid_list()) {
    for (auto& item : grid->window_list()) {
      if (highlighted_view_->GetView() == item->overview_item_view())
        return item.get();
    }
  }

  return nullptr;
}

void OverviewHighlightController::HideTabDragHighlight() {
  if (tab_dragged_view_)
    tab_dragged_view_->SetHighlightVisibility(false);
  tab_dragged_view_ = nullptr;
}

void OverviewHighlightController::ShowTabDragHighlight(
    OverviewHighlightableView* view) {
  if (tab_dragged_view_)
    tab_dragged_view_->SetHighlightVisibility(false);
  tab_dragged_view_ = view;
  tab_dragged_view_->SetHighlightVisibility(true);
}

bool OverviewHighlightController::IsTabDragHighlightVisible() const {
  return !!tab_dragged_view_;
}

std::vector<OverviewHighlightController::OverviewHighlightableView*>
OverviewHighlightController::GetTraversableViews() const {
  std::vector<OverviewHighlightableView*> traversable_views;
  traversable_views.reserve(overview_session_->num_items() +
                            (desks_util::GetMaxNumberOfDesks() + 1) *
                                Shell::Get()->GetAllRootWindows().size());
  for (auto& grid : overview_session_->grid_list()) {
    auto* bar_view = grid->desks_bar_view();
    if (bar_view) {
      // The desk items are always traversable from left to right, even in RTL
      // languages.
      for (auto* mini_view : bar_view->mini_views()) {
        traversable_views.push_back(mini_view);
        traversable_views.push_back(mini_view->desk_name_view());
      }

      if (bar_view->new_desk_button()->GetEnabled())
        traversable_views.push_back(bar_view->new_desk_button());
    }

    for (auto& item : grid->window_list())
      traversable_views.push_back(item->overview_item_view());
  }
  return traversable_views;
}

void OverviewHighlightController::UpdateHighlight(
    OverviewHighlightableView* view_to_be_highlighted,
    bool suppress_accessibility_event) {
  if (highlighted_view_ == view_to_be_highlighted)
    return;

  OverviewHighlightableView* previous_view = highlighted_view_;
  highlighted_view_ = view_to_be_highlighted;

  // Perform accessibility related tasks.
  if (!suppress_accessibility_event) {
    // Don't emit if focusing since focusing will emit an accessibility event as
    // well.
    highlighted_view_->GetView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kSelection, true);
  }
  // Note that both magnifiers are mutually exclusive. The overview "focus"
  // works differently from regular focusing so we need to update the magnifier
  // manually here.
  DockedMagnifierControllerImpl* docked_magnifier =
      Shell::Get()->docked_magnifier_controller();
  MagnificationController* fullscreen_magnifier =
      Shell::Get()->magnification_controller();
  const gfx::Point point_of_interest =
      highlighted_view_->GetMagnifierFocusPointInScreen();
  if (docked_magnifier->GetEnabled())
    docked_magnifier->CenterOnPoint(point_of_interest);
  else if (fullscreen_magnifier->IsEnabled())
    fullscreen_magnifier->CenterOnPoint(point_of_interest);

  if (previous_view)
    previous_view->SetHighlightVisibility(false);
  highlighted_view_->SetHighlightVisibility(true);
}

}  // namespace ash
