// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_

#include <stddef.h>
#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "ui/views/view.h"

namespace ash {
namespace test {
class SearchResultListViewTest;
}

class AppListMainView;
class AppListViewDelegate;

// SearchResultListView displays SearchResultList with a list of
// SearchResultView.
class APP_LIST_EXPORT SearchResultListView : public SearchResultContainerView {
 public:
  SearchResultListView(AppListMainView* main_view,
                       AppListViewDelegate* view_delegate);
  ~SearchResultListView() override;

  void SearchResultActivated(SearchResultView* view,
                             int event_flags,
                             bool by_button_press);

  void SearchResultActionActivated(SearchResultView* view,
                                   size_t action_index,
                                   int event_flags);

  void OnSearchResultInstalled(SearchResultView* view);

  // Handles vertical focus movement triggered by VKEY_UP/VKEY_DOWN.
  bool HandleVerticalFocusMovement(SearchResultView* view, bool arrow_up);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

  // Overridden from ui::ListModelObserver:
  void ListItemsRemoved(size_t start, size_t count) override;

  // Overridden from SearchResultContainerView:
  SearchResultView* GetResultViewAt(size_t index) override;
  void NotifyFirstResultYIndex(int y_index) override;
  int GetYSize() override;
  SearchResultBaseView* GetFirstResultView() override;

  AppListMainView* app_list_main_view() const { return main_view_; }

 protected:
  // Overridden from views::View:
  void VisibilityChanged(View* starting_from, bool is_visible) override;

 private:
  friend class test::SearchResultListViewTest;

  // Overridden from SearchResultContainerView:
  int DoUpdate() override;

  // Overridden from views::View:
  void Layout() override;
  int GetHeightForWidth(int w) const override;

  // Logs the set of recommendations (impressions) that were shown to the user
  // after a period of time.
  void LogImpressions();

  AppListMainView* main_view_;          // Owned by views hierarchy.
  AppListViewDelegate* view_delegate_;  // Not owned.

  views::View* results_container_;

  std::vector<SearchResultView*> search_result_views_;  // Not owned.

  // Used for logging impressions shown to users.
  base::OneShotTimer impression_timer_;
  base::OneShotTimer zero_state_file_impression_timer_;
  base::OneShotTimer drive_quick_access_impression_timer_;
  bool previous_found_zero_state_file_ = false;
  bool previous_found_drive_quick_access_ = false;

  DISALLOW_COPY_AND_ASSIGN(SearchResultListView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
