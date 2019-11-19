// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_

#include <vector>

#include "ash/app_list/app_list_export.h"
#include "ash/app_list/model/search/search_result.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"

namespace ash {

class SearchResultActionsViewDelegate;
class SearchResultView;
class SearchResultImageButton;

// SearchResultActionsView displays a SearchResult::Actions in a button
// strip. Each action is presented as a button and horizontally laid out.
class APP_LIST_EXPORT SearchResultActionsView : public views::View,
                                                public views::ButtonListener {
 public:
  explicit SearchResultActionsView(SearchResultActionsViewDelegate* delegate);
  ~SearchResultActionsView() override;

  void SetActions(const SearchResult::Actions& actions);

  bool IsValidActionIndex(size_t action_index) const;

  bool IsSearchResultHoveredOrSelected() const;

  // Updates the button UI upon the SearchResultView's UI state change.
  void UpdateButtonsOnStateChanged();

  // Called when one of the action buttons changes state.
  void ActionButtonStateChanged();

  // views::View:
  const char* GetClassName() const override;

  // Selects the result action expected to be initially selected when the parent
  // result view gets selected.
  // |reverse_tab_order| - Whether the parent result view was selected in
  //     reverse tab order.
  //  Returns whether an action was selected (returns false if selected_action_
  //  is not set).
  bool SelectInitialAction(bool reverse_tab_order);

  // Select the next result action that should be selected during tab traversal.
  // It will not change selection if the next selection would be invalid.
  // Note that "no selected action" is treated as a valid (zero) state.
  //
  // |reverse_tab_order| - Whether the selection should be changed assuming
  //     reverse tab order.
  // Returns whether the selection was changed (which includes selected action
  // getting cleared).
  bool SelectNextAction(bool reverse_tab_order);

  // Clears selected action state.
  void ClearSelectedAction();

  // Returns the selected action index, or -1 if an action is not selected.
  int GetSelectedAction() const;

  // Whether an action is currently selected.
  bool HasSelectedAction() const;

 private:
  void CreateImageButton(const SearchResult::Action& action, int action_index);

  // Returns the number of available actions.
  size_t GetActionCount() const;

  // views::View overrides:
  void ChildVisibilityChanged(views::View* child) override;

  // views::ButtonListener overrides:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // If an action is currently selected, the selected action index.
  base::Optional<int> selected_action_;

  SearchResultActionsViewDelegate* delegate_;  // Not owned.
  std::vector<SearchResultImageButton*> buttons_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultActionsView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_ACTIONS_VIEW_H_
