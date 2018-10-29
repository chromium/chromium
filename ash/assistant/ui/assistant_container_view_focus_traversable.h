// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_FOCUS_TRAVERSABLE_H_
#define ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_FOCUS_TRAVERSABLE_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/focus_search.h"

namespace ash {

class AssistantContainerView;

// AssistantContainerViewFocusSearch -------------------------------------------

// AssistantContainerViewFocusSearch is an implementation of views::FocusSearch
// belonging to AssistantContainerViewFocusTraversable. When there is no
// currently focused view, it delegates to AssistantContainerView to find the
// first focusable view for the given UI state.
class AssistantContainerViewFocusSearch : public views::FocusSearch {
 public:
  explicit AssistantContainerViewFocusSearch(
      AssistantContainerView* assistant_container_view);
  ~AssistantContainerViewFocusSearch() override;

  // views::FocusSearch:
  views::View* FindNextFocusableView(
      views::View* starting_from,
      SearchDirection search_direction,
      TraversalDirection traversal_direction,
      StartingViewPolicy check_starting_view,
      AnchoredDialogPolicy can_go_into_anchored_dialog,
      views::FocusTraversable** focus_traversable,
      views::View** focus_traversable_view) override;

 private:
  AssistantContainerView* const assistant_container_view_;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerViewFocusSearch);
};

// AssistantContainerViewFocusTraversable --------------------------------------

// AssistantContainerViewFocusTraversable is an implementation of
// views::FocusTraversable belonging to AssistantContainerView. It wraps an
// AssistantContainerViewFocusSearch instance which allows us to override focus
// behavior as needed.
class AssistantContainerViewFocusTraversable : public views::FocusTraversable {
 public:
  explicit AssistantContainerViewFocusTraversable(
      AssistantContainerView* assistant_container_view);
  ~AssistantContainerViewFocusTraversable() override;

  // views::FocusTraversable:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;

 private:
  AssistantContainerView* const assistant_container_view_;
  AssistantContainerViewFocusSearch focus_search_;

  DISALLOW_COPY_AND_ASSIGN(AssistantContainerViewFocusTraversable);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_CONTAINER_VIEW_FOCUS_TRAVERSABLE_H_
