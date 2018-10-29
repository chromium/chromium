// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/suggestions_container_view.h"

#include <memory>

#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/contents_view.h"
#include "ash/app_list/views/search_result_tile_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_constants.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ui/views/background.h"
#include "ui/views/layout/grid_layout.h"

namespace app_list {

SuggestionsContainerView::SuggestionsContainerView(
    ContentsView* contents_view,
    PaginationModel* pagination_model)
    : contents_view_(contents_view), pagination_model_(pagination_model) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  DCHECK(contents_view);
  view_delegate_ = contents_view_->GetAppListMainView()->view_delegate();
  SetBackground(views::CreateSolidBackground(kLabelBackgroundColor));

  CreateAppsGrid(kNumStartPageTiles);
}

SuggestionsContainerView::~SuggestionsContainerView() = default;

int SuggestionsContainerView::DoUpdate() {
  // Ignore updates and disable buttons when suggestions container view is not
  // shown.
  const ash::AppListState state = contents_view_->GetActiveState();
  if (state != ash::AppListState::kStateStart &&
      state != ash::AppListState::kStateApps) {
    for (auto* view : search_result_tile_views_)
      view->SetEnabled(false);

    return num_results();
  }

  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByDisplayType(
          results(), ash::SearchResultDisplayType::kRecommendation,
          /*excludes=*/{}, kNumStartPageTiles);
  if (display_results.size() != search_result_tile_views_.size()) {
    // We should recreate the grid layout in this case.
    for (size_t i = 0; i < search_result_tile_views_.size(); ++i)
      delete search_result_tile_views_[i];
    search_result_tile_views_.clear();

    CreateAppsGrid(display_results.size());
  }

  // Update the tile item results.
  for (size_t i = 0; i < search_result_tile_views_.size(); ++i) {
    DCHECK(i < display_results.size());
    search_result_tile_views_[i]->SetSearchResult(display_results[i]);
    search_result_tile_views_[i]->SetEnabled(true);

    // Notify text change after accessible name is updated and the tile view
    // is re-enabled, so that ChromeVox will announce the updated text.
    search_result_tile_views_[i]->NotifyAccessibilityEvent(
        ax::mojom::Event::kTextChanged, true);
  }

  parent()->Layout();
  return display_results.size();
}

void SuggestionsContainerView::NotifyFirstResultYIndex(int /*y_index*/) {
  NOTREACHED();
}

int SuggestionsContainerView::GetYSize() {
  NOTREACHED();
  return 0;
}

SearchResultBaseView* SuggestionsContainerView::GetFirstResultView() {
  return nullptr;
}

const char* SuggestionsContainerView::GetClassName() const {
  return "SuggestionsContainerView";
}

void SuggestionsContainerView::CreateAppsGrid(int apps_num) {
  DCHECK(search_result_tile_views_.empty());
  views::GridLayout* tiles_layout_manager =
      SetLayoutManager(std::make_unique<views::GridLayout>(this));

  views::ColumnSet* column_set = tiles_layout_manager->AddColumnSet(0);
  for (int col = 0; col < kNumStartPageTiles; ++col) {
    column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 0,
                          views::GridLayout::USE_PREF, 0, 0);
    column_set->AddPaddingColumn(0,
                                 AppListConfig::instance().grid_tile_spacing());
  }

  // Add SearchResultTileItemViews to the container.
  int i = 0;
  search_result_tile_views_.reserve(apps_num);
  tiles_layout_manager->StartRow(0, 0);
  DCHECK_LE(apps_num, kNumStartPageTiles);
  for (; i < apps_num; ++i) {
    SearchResultTileItemView* tile_item = new SearchResultTileItemView(
        view_delegate_, pagination_model_, true /* show_in_apps_page */);
    tiles_layout_manager->AddView(tile_item);
    AddChildView(tile_item);
    tile_item->SetParentBackgroundColor(kLabelBackgroundColor);
    search_result_tile_views_.emplace_back(tile_item);
  }
}

}  // namespace app_list
