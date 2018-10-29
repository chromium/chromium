// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view_focus_traversable.h"

#include "ash/assistant/ui/assistant_container_view.h"

namespace ash {

// AssistantContainerViewFocusSearch -------------------------------------------

AssistantContainerViewFocusSearch::AssistantContainerViewFocusSearch(
    AssistantContainerView* assistant_container_view)
    : views::FocusSearch(/*root=*/assistant_container_view,
                         /*cycle=*/true,
                         /*accessibility_mode=*/false),
      assistant_container_view_(assistant_container_view) {}

AssistantContainerViewFocusSearch::~AssistantContainerViewFocusSearch() =
    default;

views::View* AssistantContainerViewFocusSearch::FindNextFocusableView(
    views::View* starting_from,
    SearchDirection search_direction,
    TraversalDirection traversal_direction,
    StartingViewPolicy check_starting_view,
    AnchoredDialogPolicy can_go_into_anchored_dialog,
    views::FocusTraversable** focus_traversable,
    views::View** focus_traversable_view) {
  views::FocusManager* focus_manager =
      assistant_container_view_->GetFocusManager();

  // If there is no currently focused view we'll give AssistantContainerView
  // an opportunity to explicitly specified which view to focus first.
  views::View* next_focusable_view = nullptr;
  if (focus_manager && !focus_manager->GetFocusedView())
    next_focusable_view = assistant_container_view_->FindFirstFocusableView();

  // When we are not explicitly overriding the next focusable view we defer
  // back to views::FocusSearch's default behaviour.
  return next_focusable_view
             ? next_focusable_view
             : views::FocusSearch::FindNextFocusableView(
                   starting_from, search_direction, traversal_direction,
                   check_starting_view, can_go_into_anchored_dialog,
                   focus_traversable, focus_traversable_view);
}

// AssistantContainerViewFocusTraversable --------------------------------------

AssistantContainerViewFocusTraversable::AssistantContainerViewFocusTraversable(
    AssistantContainerView* assistant_container_view)
    : assistant_container_view_(assistant_container_view),
      focus_search_(assistant_container_view) {}

AssistantContainerViewFocusTraversable::
    ~AssistantContainerViewFocusTraversable() = default;

views::FocusSearch* AssistantContainerViewFocusTraversable::GetFocusSearch() {
  return &focus_search_;
}

views::FocusTraversable*
AssistantContainerViewFocusTraversable::GetFocusTraversableParent() {
  return nullptr;
}

views::View*
AssistantContainerViewFocusTraversable::GetFocusTraversableParentView() {
  return assistant_container_view_;
}

}  // namespace ash
