// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_
#define ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_

#include <memory>
#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/ash_export.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace ash {

class AppListViewDelegate;
class ResultSelectionController;
class SearchBoxView;

// The search results page for the app list bubble / clamshell launcher.
// Contains a scrolling list of search results. Does not include the search box,
// which is owned by a parent view.
class ASH_EXPORT AppListBubbleSearchPage
    : public views::View,
      public SearchResultContainerView::Delegate {
 public:
  METADATA_HEADER(AppListBubbleSearchPage);

  AppListBubbleSearchPage(AppListViewDelegate* view_delegate,
                          SearchBoxView* search_box_view);
  AppListBubbleSearchPage(const AppListBubbleSearchPage&) = delete;
  AppListBubbleSearchPage& operator=(const AppListBubbleSearchPage&) = delete;
  ~AppListBubbleSearchPage() override;

  // SearchResultContainerView::Delegate:
  void OnSearchResultContainerResultsChanging() override;
  void OnSearchResultContainerResultsChanged() override;

  // Returns true if there are search results that can be keyboard selected.
  bool CanSelectSearchResults();

  const auto& result_container_views_for_test() {
    return result_container_views_;
  }

 private:
  void OnSelectedResultChanged();

  SearchBoxView* const search_box_view_;

  // Containers for search result views. Has a single element, but is a vector
  // for compatibility with SearchBoxView. The contained view is owned by the
  // views hierarchy.
  std::vector<SearchResultContainerView*> result_container_views_;

  // Handles search result selection.
  std::unique_ptr<ResultSelectionController> result_selection_controller_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_APP_LIST_BUBBLE_SEARCH_PAGE_H_
