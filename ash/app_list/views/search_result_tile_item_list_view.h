// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_
#define ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_

#include <vector>

#include "ash/app_list/views/search_result_container_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "base/macros.h"
#include "base/timer/timer.h"

namespace views {
class BoxLayout;
class Separator;
class Textfield;
}  // namespace views

namespace ash {

class AppListViewDelegate;
class SearchResultPageView;

// Displays a list of SearchResultTileItemView.
class APP_LIST_EXPORT SearchResultTileItemListView
    : public SearchResultContainerView {
 public:
  SearchResultTileItemListView(SearchResultPageView* search_result_page_view,
                               views::Textfield* search_box,
                               AppListViewDelegate* view_delegate);
  ~SearchResultTileItemListView() override;

  // Overridden from SearchResultContainerView:
  SearchResultTileItemView* GetResultViewAt(size_t index) override;
  void NotifyFirstResultYIndex(int y_index) override;
  int GetYSize() override;
  SearchResultBaseView* GetFirstResultView() override;

  // Overridden from views::View:
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  const char* GetClassName() const override;
  void Layout() override;

  const std::vector<SearchResultTileItemView*>& tile_views_for_test() const {
    return tile_views_;
  }

  // Overridden from SearchResultContainerView:
  void OnShownChanged() override;

 protected:
  // View overrides:
  void VisibilityChanged(View* starting_from, bool is_visible) override;

 private:
  // Overridden from SearchResultContainerView:
  int DoUpdate() override;

  std::vector<SearchResult*> GetDisplayResults();

  base::string16 GetUserTypedQuery();

  void OnPlayStoreImpressionTimer();

  // Cleans up when the view is hid due to closing the suggestion widow
  // or closing the launcher.
  void CleanUpOnViewHide();

  std::vector<SearchResultTileItemView*> tile_views_;

  std::vector<views::Separator*> separator_views_;

  // Owned by the views hierarchy.
  SearchResultPageView* const search_result_page_view_ = nullptr;
  views::Textfield* search_box_ = nullptr;
  views::BoxLayout* layout_ = nullptr;

  base::string16 recent_playstore_query_;

  base::OneShotTimer playstore_impression_timer_;
  const bool is_play_store_app_search_enabled_;

  const bool is_app_reinstall_recommendation_enabled_;

  const size_t max_search_result_tiles_;

  DISALLOW_COPY_AND_ASSIGN(SearchResultTileItemListView);
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SEARCH_RESULT_TILE_ITEM_LIST_VIEW_H_
