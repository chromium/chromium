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

namespace views {
class Label;
}  // namespace views

namespace ash {

namespace test {
class SearchResultListViewTest;
}

class AppListMainView;
class AppListViewDelegate;

// SearchResultListView displays SearchResultList with a list of
// SearchResultView.
class ASH_EXPORT SearchResultListView : public SearchResultContainerView {
 public:
  enum class SearchResultListType {
    // kUnified list view contains all search results with the display type
    // SearchResultDisplayType::kList. No category labels are shown. This should
    // be used when productivity launcher is disabled.
    kUnified,
    // kBestMatch list view contains the results that are the best match for the
    // current query. This category should be used when productivity launcher is
    // enabled. All search results will show up under this category until search
    // metadata is updated with the other category labels.
    // TODO(crbug/1216097) add other SearchResultListTypes.
    kBestMatch,
  };

  SearchResultListView(AppListMainView* main_view,
                       AppListViewDelegate* view_delegate);

  SearchResultListView(const SearchResultListView&) = delete;
  SearchResultListView& operator=(const SearchResultListView&) = delete;
  ~SearchResultListView() override;

  // Updates the type of search results the list view shows.
  void SetListType(SearchResultListType list_type);

  void SearchResultActivated(SearchResultView* view,
                             int event_flags,
                             bool by_button_press);

  void SearchResultActionActivated(SearchResultView* view, size_t action_index);

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;

  // Overridden from ui::ListModelObserver:
  void ListItemsRemoved(size_t start, size_t count) override;

  // Overridden from SearchResultContainerView:
  SearchResultView* GetResultViewAt(size_t index) override;

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
  void OnThemeChanged() override;

  // Logs the set of recommendations (impressions) that were shown to the user
  // after a period of time.
  void LogImpressions();

  // Returns search results specific to Assistant if any are available.
  std::vector<SearchResult*> GetAssistantResults();

  // Returns regular search results with Assistant search results appended.
  std::vector<SearchResult*> GetSearchResults();

  AppListMainView* main_view_;          // Owned by views hierarchy.
  AppListViewDelegate* view_delegate_;  // Not owned.

  views::View* results_container_;

  std::vector<SearchResultView*> search_result_views_;  // Not owned.

  // The SearchResultListViewType dictates what kinds of results will be shown.
  SearchResultListType list_type_ = SearchResultListType::kUnified;
  views::Label* title_label_ = nullptr;  // Owned by view hierarchy.
  // Used for logging impressions shown to users.
  base::OneShotTimer impression_timer_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_LIST_VIEW_H_
