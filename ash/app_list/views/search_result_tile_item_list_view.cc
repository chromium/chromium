// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_tile_item_list_view.h"

#include <stddef.h>

#include <memory>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/i18n/rtl.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"

namespace {

// Layout constants used when fullscreen app list feature is enabled.
constexpr size_t kMaxNumSearchResultTiles = 6;
constexpr int kItemListVerticalSpacing = 16;
constexpr int kItemListHorizontalSpacing = 12;
constexpr int kBetweenItemSpacing = 8;
constexpr int kSeparatorLeftRightPadding = 4;
constexpr int kSeparatorHeight = 46;
constexpr int kSeparatorTopPadding = 10;

constexpr SkColor kSeparatorColor = SkColorSetARGB(0x1F, 0x00, 0x00, 0x00);

}  // namespace

namespace app_list {

SearchResultTileItemListView::SearchResultTileItemListView(
    SearchResultPageView* search_result_page_view,
    views::Textfield* search_box,
    AppListViewDelegate* view_delegate)
    : search_result_page_view_(search_result_page_view),
      search_box_(search_box),
      is_play_store_app_search_enabled_(
          app_list_features::IsPlayStoreAppSearchEnabled()) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal,
      gfx::Insets(kItemListVerticalSpacing, kItemListHorizontalSpacing),
      kBetweenItemSpacing));
  for (size_t i = 0; i < kMaxNumSearchResultTiles; ++i) {
    if (is_play_store_app_search_enabled_) {
      views::Separator* separator = new views::Separator;
      separator->SetVisible(false);
      separator->SetBorder(views::CreateEmptyBorder(
          kSeparatorTopPadding, kSeparatorLeftRightPadding,
          kSearchTileHeight - kSeparatorHeight, kSeparatorLeftRightPadding));
      separator->SetColor(kSeparatorColor);

      separator_views_.push_back(separator);
      AddChildView(separator);
    }

    SearchResultTileItemView* tile_item = new SearchResultTileItemView(
        view_delegate, nullptr /* pagination model */,
        false /* show_in_apps_page */);
    tile_item->SetParentBackgroundColor(kCardBackgroundColor);
    tile_views_.push_back(tile_item);
    AddChildView(tile_item);
  }
}

SearchResultTileItemListView::~SearchResultTileItemListView() = default;

void SearchResultTileItemListView::NotifyFirstResultYIndex(int y_index) {
  for (size_t i = 0; i < static_cast<size_t>(num_results()); ++i)
    tile_views_[i]->result()->set_distance_from_origin(i + y_index);
}

int SearchResultTileItemListView::GetYSize() {
  return num_results() ? 1 : 0;
}

SearchResultBaseView* SearchResultTileItemListView::GetFirstResultView() {
  DCHECK(!tile_views_.empty());
  return num_results() <= 0 ? nullptr : tile_views_[0];
}

int SearchResultTileItemListView::DoUpdate() {
  base::string16 raw_query = search_box_->text();
  base::string16 query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);

  SearchResult::DisplayType display_type =
      app_list_features::IsZeroStateSuggestionsEnabled()
          ? (query.empty() ? ash::SearchResultDisplayType::kRecommendation
                           : ash::SearchResultDisplayType::kTile)
          : ash::SearchResultDisplayType::kTile;
  // Do not display the continue reading app in the search result list.
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByDisplayType(
          results(), display_type,
          /*excludes=*/{app_list::kInternalAppIdContinueReading},
          kMaxNumSearchResultTiles);

  SearchResult::ResultType previous_type = ash::SearchResultType::kUnknown;
  for (size_t i = 0; i < kMaxNumSearchResultTiles; ++i) {
    if (i >= display_results.size()) {
      if (is_play_store_app_search_enabled_)
        separator_views_[i]->SetVisible(false);
      tile_views_[i]->SetSearchResult(nullptr);
      continue;
    }

    SearchResult* item = display_results[i];
    tile_views_[i]->SetSearchResult(item);

    if (is_play_store_app_search_enabled_) {
      if (i > 0 && item->result_type() != previous_type) {
        // Add a separator to separate search results of different types.
        // The strategy here is to only add a separator only if current search
        // result type is different from the previous one. The strategy is
        // based on the assumption that the search results are already
        // separated in groups based on their result types.
        separator_views_[i]->SetVisible(true);
      } else {
        separator_views_[i]->SetVisible(false);
      }
    }

    previous_type = item->result_type();
  }

  set_container_score(
      display_results.empty() ? 0 : display_results.front()->display_score());

  return display_results.size();
}

bool SearchResultTileItemListView::OnKeyPressed(const ui::KeyEvent& event) {
  // Let the FocusManager handle Left/Right keys.
  if (!CanProcessUpDownKeyTraversal(event))
    return false;

  views::View* next_focusable_view = nullptr;

  // Since search result tile item views have horizontal layout, hitting
  // up/down when one of them is focused moves focus to the previous/next
  // search result container.
  if (event.key_code() == ui::VKEY_UP) {
    next_focusable_view = GetFocusManager()->GetNextFocusableView(
        tile_views_.front(), GetWidget(), true, false);
    if (!search_result_page_view_->Contains(next_focusable_view)) {
      // Focus should be moved to search box when it is moved outside search
      // result page view.
      search_box_->RequestFocus();
      return true;
    }
  } else {
    DCHECK_EQ(event.key_code(), ui::VKEY_DOWN);
    next_focusable_view = GetFocusManager()->GetNextFocusableView(
        tile_views_.back(), GetWidget(), false, false);
  }

  if (next_focusable_view) {
    next_focusable_view->RequestFocus();
    return true;
  }

  // Return false to let FocusManager to handle default focus move by key
  // events.
  return false;
}

const char* SearchResultTileItemListView::GetClassName() const {
  return "SearchResultTileItemListView";
}

}  // namespace app_list
