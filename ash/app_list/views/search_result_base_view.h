// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_BASE_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_BASE_VIEW_H_

#include <optional>

#include "ash/app_list/model/search/search_result_observer.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"

namespace ash {

class SearchResult;
class SearchResultActionsView;

// Base class for views that observe and display a search result
class ASH_EXPORT SearchResultBaseView : public views::Button,
                                        public SearchResultObserver {
  METADATA_HEADER(SearchResultBaseView, views::Button)

 public:
  SearchResultBaseView();

  SearchResultBaseView(const SearchResultBaseView&) = delete;
  SearchResultBaseView& operator=(const SearchResultBaseView&) = delete;

  // Set whether the result is selected. It updates the background highlight,
  // and selects the result action associated with the result if
  // SearchBoxSelection feature is enabled.
  //
  // |reverse_tab_order| - Indicates whether the selection was set as part of
  //     reverse tab traversal. Should be set when selection was changed while
  //     handling TAB keyboard key. Ignored if |selected| is false.
  void SetSelected(bool selected, std::optional<bool> reverse_tab_order);

  // Selects the initial action that should be associated with the result view,
  // notifying a11y hierarchy of the selection. If the result view does not
  // support result actions (i.e. does not have actions_view_), this will just
  // announce the current result view selection.
  // |reverse_tab_order| - whether the action was selected in reverse tab order.
  virtual void SelectInitialResultAction(bool reverse_tab_order);

  // Selects the next result action for the view, if the result supports
  // non-default actions (see actions_view_).
  // |reverse_tab_order| - whether the action was selected while handling TAB
  // key in reverse tab order.
  //
  // Returns whether the selected result action was changed.
  virtual bool SelectNextResultAction(bool reverse_tab_order);

  // Returns the view that is currently selected - for example, if the result
  // supports action views and an action view is currently selected, this
  // should return the action view, otherwise it should return `this`.
  virtual views::View* GetSelectedView();

  SearchResult* result() const { return result_; }
  void SetResult(SearchResult* result);

  // Invoked after |result_| is updated.
  virtual void OnResultChanged() {}

  // Overridden from SearchResultObserver:
  void OnResultDestroying() override;

  // Computes the button's spoken feedback name.
  std::u16string ComputeAccessibleName() const;

  // Clears the result without calling |OnResultChanged| or |OnResultChanging|
  void ClearResult();

  bool selected() const { return selected_; }

  int index_in_container() const { return index_in_container_.value(); }

  void set_index_in_container(size_t index) { index_in_container_ = index; }

  void set_result_display_start_time(base::TimeTicks start_time) {
    result_display_start_time_ = start_time;
  }

  base::TimeTicks result_display_start_time() const {
    return result_display_start_time_;
  }

  void set_is_default_result(bool is_default) {
    is_default_result_ = is_default;
  }

  bool is_default_result() const { return is_default_result_; }

  // views::Button:
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;

  // views::View:
  void SetVisible(bool visible) override;

  SearchResultActionsView* actions_view() { return actions_view_; }

 protected:
  ~SearchResultBaseView() override;

  void UpdateAccessibleName();

  void set_actions_view(SearchResultActionsView* actions_view) {
    actions_view_ = actions_view;
  }

  // views::Button:
  void OnEnabledChanged() override;

 private:
  // If non-default result action was selected, clears the actions_view_'s
  // selection state.
  void ClearSelectedResultAction();

  void UpdateAccessibleDefaultAction();

  // Whether the result is currently selected.
  bool selected_ = false;

  // Expected to be set by result view implementations that supports
  // extra result actions. It points to the view containing result actions
  // buttons. Owned by the views hierarchy.
  raw_ptr<SearchResultActionsView, DanglingUntriaged> actions_view_ = nullptr;

  // The index of this view within a |SearchResultContainerView| that holds it.
  std::optional<int> index_in_container_;

  // The starting time when |result_| is being displayed.
  base::TimeTicks result_display_start_time_;

  // True if |result_| is selected as the default result which can be
  // activated by user by pressing ENTER key.
  bool is_default_result_ = false;
  raw_ptr<SearchResult> result_ =
      nullptr;  // Owned by SearchModel::SearchResults.
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_BASE_VIEW_H_
