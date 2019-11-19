// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_tile_item_list_view.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <set>
#include <string>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/search_result_page_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/app_list/internal_app_id_constants.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

// Layout constants used when fullscreen app list feature is enabled.
constexpr int kItemListVerticalSpacing = 16;
constexpr int kItemListHorizontalSpacing = 12;
constexpr int kBetweenItemSpacing = 8;
constexpr int kSeparatorLeftRightPadding = 4;
constexpr int kSeparatorHeight = 46;
constexpr int kSeparatorTopPadding = 10;

constexpr SkColor kSeparatorColor = SkColorSetA(gfx::kGoogleGrey900, 0x24);

// The Delay before recording play store app results impression, i.e., if the
// play store results are displayed less than the duration, we assume user
// won't have chance to see them clearly and click on them, and wont' log
// the impression.
constexpr int kPlayStoreImpressionDelayInMs = 1000;

// Returns true if the search result is an installable app.
bool IsResultAnInstallableApp(SearchResult* result) {
  SearchResult::ResultType result_type = result->result_type();
  return result_type == AppListSearchResultType::kPlayStoreApp ||
         result_type == AppListSearchResultType::kPlayStoreReinstallApp ||
         result_type == AppListSearchResultType::kInstantApp;
}

bool IsPlayStoreApp(SearchResult* result) {
  return result->result_type() == AppListSearchResultType::kPlayStoreApp;
}

}  // namespace

SearchResultTileItemListView::SearchResultTileItemListView(
    SearchResultPageView* search_result_page_view,
    views::Textfield* search_box,
    AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate),
      search_result_page_view_(search_result_page_view),
      search_box_(search_box),
      is_play_store_app_search_enabled_(
          app_list_features::IsPlayStoreAppSearchEnabled()),
      is_app_reinstall_recommendation_enabled_(
          app_list_features::IsAppReinstallZeroStateEnabled()),
      max_search_result_tiles_(
          AppListConfig::instance().max_search_result_tiles()) {
  layout_ = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal,
      gfx::Insets(kItemListVerticalSpacing, kItemListHorizontalSpacing),
      kBetweenItemSpacing));
  for (size_t i = 0; i < max_search_result_tiles_; ++i) {
    if (is_app_reinstall_recommendation_enabled_ ||
        is_play_store_app_search_enabled_) {
      views::Separator* separator =
          AddChildView(std::make_unique<views::Separator>());
      separator->SetVisible(false);
      separator->SetBorder(views::CreateEmptyBorder(
          kSeparatorTopPadding, kSeparatorLeftRightPadding,
          AppListConfig::instance().search_tile_height() - kSeparatorHeight,
          kSeparatorLeftRightPadding));
      separator->SetColor(kSeparatorColor);
      separator_views_.push_back(separator);
      layout_->SetFlexForView(separator, 0);
    }

    SearchResultTileItemView* tile_item =
        AddChildView(std::make_unique<SearchResultTileItemView>(
            view_delegate, nullptr /* pagination model */,
            false /* show_in_apps_page */));
    tile_item->set_index_in_container(i);
    tile_item->SetParentBackgroundColor(
        AppListConfig::instance().card_background_color());
    tile_views_.push_back(tile_item);
    AddObservedResultView(tile_item);
  }

  // Tile items are shown horizontally.
  set_horizontally_traversable(true);
}

SearchResultTileItemListView::~SearchResultTileItemListView() = default;

SearchResultTileItemView* SearchResultTileItemListView::GetResultViewAt(
    size_t index) {
  DCHECK(index >= 0 && index < tile_views_.size());
  return tile_views_[index];
}

void SearchResultTileItemListView::NotifyFirstResultYIndex(int y_index) {
  for (size_t i = 0; i < static_cast<size_t>(num_results()); ++i)
    GetResultViewAt(i)->result()->set_distance_from_origin(i + y_index);
}

int SearchResultTileItemListView::GetYSize() {
  return num_results() ? 1 : 0;
}

SearchResultBaseView* SearchResultTileItemListView::GetFirstResultView() {
  DCHECK(!tile_views_.empty());
  return num_results() <= 0 ? nullptr : tile_views_[0];
}

int SearchResultTileItemListView::DoUpdate() {
  if (!GetWidget() || !GetWidget()->IsVisible() || !GetWidget()->IsActive()) {
    for (size_t i = 0; i < max_search_result_tiles_; ++i) {
      SearchResultBaseView* result_view = GetResultViewAt(i);
      result_view->SetResult(nullptr);
      result_view->SetVisible(false);
    }
    return 0;
  }

  std::vector<SearchResult*> display_results = GetDisplayResults();

  std::set<std::string> result_id_removed, result_id_added;
  bool is_result_an_installable_app = false;
  bool is_previous_result_installable_app = false;
  int installed_app_index = -1;
  int playstore_app_index = -1;
  int reinstall_app_index = -1;
  int app_group_index = -1;
  bool found_playstore_results = false;

  for (size_t i = 0; i < max_search_result_tiles_; ++i) {
    // If the current result at i exists, wants to be notified and is a
    // different id, notify it that it is being hidden.
    SearchResult* current_result = tile_views_[i]->result();
    if (current_result != nullptr) {
      result_id_removed.insert(current_result->id());
    }

    if (i >= display_results.size()) {
      if (is_app_reinstall_recommendation_enabled_ ||
          is_play_store_app_search_enabled_) {
        separator_views_[i]->SetVisible(false);
      }

      GetResultViewAt(i)->SetResult(nullptr);
      continue;
    }

    SearchResult* item = display_results[i];
    if (IsPlayStoreApp(item)) {
      ++playstore_app_index;
      app_group_index = playstore_app_index;
      found_playstore_results = true;
    } else if (item->result_type() ==
               AppListSearchResultType::kPlayStoreReinstallApp) {
      ++reinstall_app_index;
      app_group_index = playstore_app_index;
    } else {
      ++installed_app_index;
      app_group_index = installed_app_index;
    }

    GetResultViewAt(i)->SetResult(item);
    GetResultViewAt(i)->set_group_index_in_container_view(app_group_index);
    result_id_added.insert(item->id());
    is_result_an_installable_app = IsResultAnInstallableApp(item);

    if (is_play_store_app_search_enabled_ ||
        is_app_reinstall_recommendation_enabled_) {
      if (i > 0 && (is_result_an_installable_app !=
                    is_previous_result_installable_app)) {
        // Add a separator between installed apps and installable apps.
        // This assumes the search results are already separated in groups for
        // installed and installable apps.
        separator_views_[i]->SetVisible(true);
      } else {
        separator_views_[i]->SetVisible(false);
      }
    }

    is_previous_result_installable_app = is_result_an_installable_app;
  }

  // Track play store results and start the timer for recording their impression
  // UMA metrics.
  base::string16 user_typed_query = GetUserTypedQuery();
  if (found_playstore_results && user_typed_query != recent_playstore_query_) {
    recent_playstore_query_ = user_typed_query;
    playstore_impression_timer_.Stop();
    playstore_impression_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(kPlayStoreImpressionDelayInMs), this,
        &SearchResultTileItemListView::OnPlayStoreImpressionTimer);
    // Set the starting time in result view for play store results.
    base::TimeTicks result_display_start = base::TimeTicks::Now();
    for (size_t i = 0; i < max_search_result_tiles_; ++i) {
      SearchResult* result = GetResultViewAt(i)->result();
      if (result && IsPlayStoreApp(result)) {
        GetResultViewAt(i)->set_result_display_start_time(result_display_start);
      }
    }
  } else if (!found_playstore_results) {
    playstore_impression_timer_.Stop();
  }

  // notify visibility changes, if needed.
  std::set<std::string> actual_added_ids =
      base::STLSetDifference<std::set<std::string>>(result_id_added,
                                                    result_id_removed);

  for (const std::string& added_id : actual_added_ids) {
    SearchResult* added =
        view_delegate()->GetSearchModel()->FindSearchResult(added_id);
    if (added != nullptr && added->notify_visibility_change()) {
      view_delegate()->OnSearchResultVisibilityChanged(added->id(), shown());
    }
  }
  if (shown() != false) {
    std::set<std::string> actual_removed_ids =
        base::STLSetDifference<std::set<std::string>>(result_id_removed,
                                                      result_id_added);
    // we only notify removed items if we're in the middle of showing.
    for (const std::string& removed_id : actual_removed_ids) {
      SearchResult* removed =
          view_delegate()->GetSearchModel()->FindSearchResult(removed_id);
      if (removed != nullptr && removed->notify_visibility_change()) {
        view_delegate()->OnSearchResultVisibilityChanged(removed->id(),
                                                         false /*=shown*/);
      }
    }
  }

  set_container_score(
      display_results.empty() ? 0 : display_results.front()->display_score());

  return display_results.size();
}

std::vector<SearchResult*> SearchResultTileItemListView::GetDisplayResults() {
  base::string16 raw_query = search_box_->GetText();
  base::string16 query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);

  // We ask for |max_search_result_tiles_| policy tile results first,
  // then add them to their preferred position in the tile list if found.
  auto policy_tiles_filter =
      base::BindRepeating([](const SearchResult& r) -> bool {
        return r.display_location() ==
                   SearchResultDisplayLocation::kTileListContainer &&
               r.display_index() != SearchResultDisplayIndex::kUndefined &&
               r.display_type() == SearchResultDisplayType::kRecommendation;
      });
  std::vector<SearchResult*> policy_tiles_results =
      is_app_reinstall_recommendation_enabled_ && query.empty()
          ? SearchModel::FilterSearchResultsByFunction(
                results(), policy_tiles_filter, max_search_result_tiles_)
          : std::vector<SearchResult*>();

  SearchResult::DisplayType display_type =
      app_list_features::IsZeroStateSuggestionsEnabled()
          ? (query.empty() ? SearchResultDisplayType::kRecommendation
                           : SearchResultDisplayType::kTile)
          : SearchResultDisplayType::kTile;
  size_t display_num = max_search_result_tiles_ - policy_tiles_results.size();

  // Do not display the repeat reinstall results or continue reading app in the
  // search result list.
  auto non_policy_tiles_filter = base::BindRepeating(
      [](const SearchResult::DisplayType& display_type,
         const SearchResult& r) -> bool {
        return r.display_type() == display_type &&
               r.result_type() !=
                   AppListSearchResultType::kPlayStoreReinstallApp &&
               r.id() != kInternalAppIdContinueReading;
      },
      display_type);
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), non_policy_tiles_filter, display_num);

  // Policy tile results will be appended to the final tiles list
  // based on their specified index. If the requested index is out of
  // range of the current list, the result will be appended to the back.
  std::sort(policy_tiles_results.begin(), policy_tiles_results.end(),
            [](const SearchResult* r1, const SearchResult* r2) -> bool {
              return r1->display_index() < r2->display_index();
            });

  const SearchResultDisplayIndex display_results_last_index =
      static_cast<SearchResultDisplayIndex>(display_results.size() - 1);
  for (auto* result : policy_tiles_results) {
    const SearchResultDisplayIndex result_index = result->display_index();
    if (result_index > display_results_last_index) {
      display_results.emplace_back(result);
    } else {
      // TODO(newcomer): Remove this check once we determine the root cause for
      // https://crbug.com/992344.
      CHECK_GE(result_index, SearchResultDisplayIndex::kFirstIndex);
      display_results.emplace(display_results.begin() + result_index, result);
    }
  }

  return display_results;
}

base::string16 SearchResultTileItemListView::GetUserTypedQuery() {
  base::string16 search_box_text = search_box_->GetText();
  gfx::Range range = search_box_->GetSelectedRange();
  base::string16 raw_query = range.is_empty()
                                 ? search_box_text
                                 : search_box_text.substr(0, range.start());
  base::string16 query;
  base::TrimWhitespace(raw_query, base::TRIM_ALL, &query);
  return query;
}

void SearchResultTileItemListView::OnPlayStoreImpressionTimer() {
  size_t playstore_app_num = 0;
  for (const auto* tile_view : tile_views_) {
    SearchResult* result = tile_view->result();
    if (result == nullptr)
      continue;
    if (IsPlayStoreApp(result))
      ++playstore_app_num;
  }

  // Log the UMA metrics of play store impression.
  base::RecordAction(
      base::UserMetricsAction("AppList_ShowPlayStoreQueryResults"));

  DCHECK_LE(playstore_app_num, max_search_result_tiles_);
  UMA_HISTOGRAM_EXACT_LINEAR("Apps.AppListPlayStoreSearchAppsDisplayed",
                             playstore_app_num, max_search_result_tiles_);
}

void SearchResultTileItemListView::CleanUpOnViewHide() {
  playstore_impression_timer_.Stop();
  recent_playstore_query_.clear();
}

bool SearchResultTileItemListView::OnKeyPressed(const ui::KeyEvent& event) {
  // Let the FocusManager handle Left/Right keys.
  if (!IsUnhandledUpDownKeyEvent(event))
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

void SearchResultTileItemListView::Layout() {
  const bool flex = GetContentsBounds().width() < GetPreferredSize().width();
  layout_->SetDefaultFlex(flex ? 1 : 0);
  layout_->set_between_child_spacing(flex ? 1 : kBetweenItemSpacing);

  views::View::Layout();
}

void SearchResultTileItemListView::OnShownChanged() {
  SearchResultContainerView::OnShownChanged();
  for (const auto* tile_view : tile_views_) {
    SearchResult* result = tile_view->result();
    if (result == nullptr) {
      continue;
    }
    if (result->notify_visibility_change()) {
      view_delegate()->OnSearchResultVisibilityChanged(result->id(), shown());
    }
  }
}

void SearchResultTileItemListView::VisibilityChanged(View* starting_from,
                                                     bool is_visible) {
  SearchResultContainerView::VisibilityChanged(starting_from, is_visible);
  // We only do this work when is_visible is false, since this is how we
  // receive the event. We filter and only run when shown.
  if (is_visible && shown()) {
    return;
  }

  CleanUpOnViewHide();

  for (const auto* tile_view : tile_views_) {
    SearchResult* result = tile_view->result();
    if (result == nullptr) {
      continue;
    }
    if (result->notify_visibility_change()) {
      view_delegate()->OnSearchResultVisibilityChanged(result->id(),
                                                       false /*=visible*/);
    }
  }
}

}  // namespace ash
