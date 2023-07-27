// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_highlight_controller.h"

#include "ash/accessibility/magnifier/docked_magnifier_controller.h"
#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/accessibility/magnifier/magnifier_utils.h"
#include "ash/accessibility/scoped_a11y_override_window_setter.h"
#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/wm/desks/cros_next_default_desk_button.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desk_name_view.h"
#include "ash/wm/desks/desk_preview_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_util.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/legacy_desk_bar_view.h"
#include "ash/wm/desks/templates/saved_desk_grid_view.h"
#include "ash/wm/desks/templates/saved_desk_item_view.h"
#include "ash/wm/desks/templates/saved_desk_library_view.h"
#include "ash/wm/desks/templates/saved_desk_name_view.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_highlightable_view.h"
#include "ash/wm/overview/overview_item.h"
#include "ash/wm/overview/overview_item_view.h"
#include "ash/wm/overview/overview_session.h"
#include "ash/wm/overview/overview_utils.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "chromeos/constants/chromeos_features.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/views/view.h"

namespace ash {

namespace {

void AddDesksBarTraversableViews(
    OverviewGrid* grid,
    std::vector<OverviewHighlightableView*>& out_traversable_views) {
  auto* bar_view = grid->desks_bar_view();
  if (!bar_view) {
    return;
  }

  const bool is_zero_state = bar_view->IsZeroState();

  // The desk items are always traversable from left to right, even in RTL
  // languages.
  if (chromeos::features::IsJellyrollEnabled()) {
    if (is_zero_state) {
      out_traversable_views.push_back(bar_view->default_desk_button());
    } else {
      for (auto* mini_view : bar_view->mini_views()) {
        out_traversable_views.push_back(mini_view->desk_preview());
        out_traversable_views.push_back(mini_view->desk_name_view());
      }
    }
    auto* new_desk_button = bar_view->new_desk_button();
    if (new_desk_button->GetEnabled()) {
      out_traversable_views.push_back(new_desk_button);
    }

    if (auto* library_button = bar_view->library_button()) {
      if (library_button->GetVisible()) {
        out_traversable_views.push_back(library_button);
      }
    }
    return;
  }

  if (is_zero_state) {
    out_traversable_views.push_back(bar_view->zero_state_default_desk_button());
    out_traversable_views.push_back(bar_view->zero_state_new_desk_button());
    // Library button is only present if the desk templates feature or the
    // save and recall feature is enabled.
    if (auto* library_button = bar_view->zero_state_library_button()) {
      if (library_button->GetVisible()) {
        out_traversable_views.push_back(library_button);
      }
    }
    return;
  }

  for (auto* mini_view : bar_view->mini_views()) {
    out_traversable_views.push_back(mini_view->desk_preview());
    out_traversable_views.push_back(mini_view->desk_name_view());
  }

  auto* new_desk_button =
      bar_view->expanded_state_new_desk_button()->GetInnerButton();
  if (new_desk_button->GetEnabled()) {
    out_traversable_views.push_back(new_desk_button);
  }

  if (auto* library_button = bar_view->expanded_state_library_button()) {
    auto* inner_library_button = library_button->GetInnerButton();
    if (library_button->GetVisible() && inner_library_button->GetEnabled()) {
      out_traversable_views.push_back(inner_library_button);
    }
  }
}

}  // namespace

OverviewHighlightController::OverviewHighlightController(
    OverviewSession* overview_session)
    : overview_session_(overview_session),
      scoped_a11y_overrider_(
          std::make_unique<ScopedA11yOverrideWindowSetter>()) {}

OverviewHighlightController::~OverviewHighlightController() = default;

void OverviewHighlightController::MoveHighlight(bool reverse) {
  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  const int count = static_cast<int>(traversable_views.size());

  // |count| can be zero when there are no overview items and no desk views (eg.
  // "No recent items" or PIP windows are shown but they aren't traversable).
  if (count == 0)
    return;

  int index = 0;
  bool item_was_deleted = false;
  if (!highlighted_view_) {
    // Pick up where we left off if |deleted_index_| has a value.
    if (deleted_index_) {
      item_was_deleted = true;
      index = *deleted_index_ >= count ? 0 : *deleted_index_;
      deleted_index_.reset();
    } else if (reverse) {
      index = count - 1;
    }
  } else {
    auto it = base::ranges::find(traversable_views, highlighted_view_);
    DCHECK(it != traversable_views.end());
    const int current_index = std::distance(traversable_views.begin(), it);
    DCHECK_GE(current_index, 0);
    index = (((reverse ? -1 : 1) + current_index) + count) % count;
  }

  // If we are moving over either end of the list of traversible views and there
  // is an active toast with an undo button for desk removal  that can be
  // highlighted, then we unfocus any traversible views while the dismiss button
  // is focused.
  if (((index == 0 && !reverse) || (index == count - 1 && reverse)) &&
      !item_was_deleted &&
      DesksController::Get()
          ->MaybeToggleA11yHighlightOnUndoDeskRemovalToast()) {
    SetFocusHighlightVisibility(false);
    highlighted_view_ = nullptr;
    return;
  }

  UpdateHighlight(traversable_views[index]);
}

void OverviewHighlightController::UpdateA11yFocusWindow(
    OverviewHighlightableView* name_view) {
  scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
      name_view->GetView()->GetWidget()->GetNativeWindow());
}

void OverviewHighlightController::MoveHighlightToView(
    OverviewHighlightableView* target_view,
    bool suppress_accessibility_event) {
  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  DCHECK(base::Contains(traversable_views, target_view));

  UpdateHighlight(target_view, suppress_accessibility_event);
}

void OverviewHighlightController::OnViewDestroyingOrDisabling(
    OverviewHighlightableView* view) {
  DCHECK(view);

  // TODO(afakhry): Refactor this code.
  const std::vector<OverviewHighlightableView*> traversable_views =
      GetTraversableViews();
  const auto it = base::ranges::find(traversable_views, view);
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
  if (DesksController::Get()
          ->MaybeActivateDeskRemovalUndoButtonOnHighlightedToast()) {
    return true;
  }

  if (!highlighted_view_)
    return false;

  highlighted_view_->MaybeActivateHighlightedView();
  return true;
}

bool OverviewHighlightController::MaybeCloseHighlightedView(
    bool primary_action) {
  if (!highlighted_view_)
    return false;

  highlighted_view_->MaybeCloseHighlightedView(primary_action);
  return true;
}

bool OverviewHighlightController::MaybeSwapHighlightedView(bool right) {
  if (!highlighted_view_)
    return false;

  highlighted_view_->MaybeSwapHighlightedView(right);
  return true;
}

bool OverviewHighlightController::MaybeActivateHighlightedViewOnOverviewExit() {
  return highlighted_view_ &&
         highlighted_view_->MaybeActivateHighlightedViewOnOverviewExit(
             overview_session_);
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

void OverviewHighlightController::ResetHighlightedView() {
  if (!highlighted_view_)
    return;

  deleted_index_.reset();
  highlighted_view_->SetHighlightVisibility(false);
  highlighted_view_ = nullptr;
}

std::vector<OverviewHighlightableView*>
OverviewHighlightController::GetTraversableViews() const {
  std::vector<OverviewHighlightableView*> traversable_views;
  traversable_views.reserve(32);  // Conservative default.

  // Note that this order matches the order of the chromevox cycling in
  // `OverviewSession::UpdateAccessibilityFocus`.
  for (auto& grid : overview_session_->grid_list()) {
    // If the saved desk library is visible, we shouldn't try to add any
    // overview items.
    if (grid->IsShowingSavedDeskLibrary()) {
      SavedDeskLibraryView* desk_library_view = grid->GetSavedDeskLibraryView();
      DCHECK(desk_library_view);
      for (SavedDeskGridView* saved_desk_grid_view :
           desk_library_view->grid_views()) {
        for (SavedDeskItemView* saved_desk_item :
             saved_desk_grid_view->grid_items()) {
          traversable_views.push_back(saved_desk_item);

          // Admin templates names cannot be edited or focused.
          SavedDeskNameView* name_view = saved_desk_item->name_view();
          if (name_view->IsFocusable())
            traversable_views.push_back(name_view);
        }
      }
    } else {
      for (auto& item : grid->window_list())
        traversable_views.push_back(item->overview_item_view());
    }

    AddDesksBarTraversableViews(grid.get(), traversable_views);

    if (grid->IsSaveDeskAsTemplateButtonVisible())
      traversable_views.push_back(grid->GetSaveDeskAsTemplateButton());
    if (grid->IsSaveDeskForLaterButtonVisible())
      traversable_views.push_back(grid->GetSaveDeskForLaterButton());
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
    scoped_a11y_overrider_->MaybeUpdateA11yOverrideWindow(
        highlighted_view_->GetView()->GetWidget()->GetNativeWindow());
    highlighted_view_->GetView()->NotifyAccessibilityEvent(
        ax::mojom::Event::kSelection, true);
  }
  // The overview "focus" works differently from regular focusing so we need to
  // update the magnifier manually here.
  magnifier_utils::MaybeUpdateActiveMagnifierFocus(
      highlighted_view_->GetMagnifierFocusPointInScreen());

  if (previous_view)
    previous_view->SetHighlightVisibility(false);
  highlighted_view_->SetHighlightVisibility(true);
}

}  // namespace ash
