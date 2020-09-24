// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_list_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/search_box_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "ash/public/cpp/app_list/vector_icons/vector_icons.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr base::TimeDelta kImpressionThreshold =
    base::TimeDelta::FromSeconds(3);
constexpr base::TimeDelta kZeroStateImpressionThreshold =
    base::TimeDelta::FromSeconds(1);

SearchResultIdWithPositionIndices GetSearchResultsForLogging(
    std::vector<SearchResultView*> search_result_views) {
  SearchResultIdWithPositionIndices results;
  for (const auto* item : search_result_views) {
    if (item->result()) {
      results.emplace_back(SearchResultIdWithPositionIndex(
          item->result()->id(), item->index_in_container()));
    }
  }
  return results;
}

bool IsZeroStateFile(const SearchResult& result) {
  return result.result_type() == AppListSearchResultType::kZeroStateFile;
}

bool IsDriveQuickAccess(const SearchResult& result) {
  return result.result_type() == AppListSearchResultType::kDriveQuickAccess;
}

void LogFileImpressions(SearchResultType result_type) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppList.ZeroStateResultsList.FileImpressions",
                            result_type, SEARCH_RESULT_TYPE_BOUNDARY);
}

}  // namespace

SearchResultListView::SearchResultListView(AppListMainView* main_view,
                                           AppListViewDelegate* view_delegate)
    : SearchResultContainerView(view_delegate),
      main_view_(main_view),
      view_delegate_(view_delegate),
      results_container_(new views::View) {
  results_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));

  size_t result_count =
      AppListConfig::instance().max_search_result_list_items() +
      AppListConfig::instance().max_assistant_search_result_list_items();

  for (size_t i = 0; i < result_count; ++i) {
    search_result_views_.emplace_back(
        new SearchResultView(this, view_delegate_));
    search_result_views_.back()->set_index_in_container(i);
    results_container_->AddChildView(search_result_views_.back());
    AddObservedResultView(search_result_views_.back());
  }
  AddChildView(results_container_);
}

SearchResultListView::~SearchResultListView() = default;

void SearchResultListView::ListItemsRemoved(size_t start, size_t count) {
  size_t last = std::min(start + count, search_result_views_.size());
  for (size_t i = start; i < last; ++i)
    GetResultViewAt(i)->ClearResult();

  SearchResultContainerView::ListItemsRemoved(start, count);
}

SearchResultView* SearchResultListView::GetResultViewAt(size_t index) {
  DCHECK(index >= 0 && index < search_result_views_.size());
  return search_result_views_[index];
}

int SearchResultListView::DoUpdate() {
  if (!GetWidget() || !GetWidget()->IsVisible()) {
    for (auto* result_view : search_result_views_) {
      result_view->SetResult(nullptr);
      result_view->SetVisible(false);
    }
    return 0;
  }

  std::vector<SearchResult*> display_results = GetSearchResults();

  // TODO(crbug.com/1076270): The logic for zero state and Drive quick access
  // files below exists only for metrics, and can be folded into the
  // AppListNotifier and done in chrome.
  bool found_zero_state_file = false;
  bool found_drive_quick_access = false;

  for (size_t i = 0; i < search_result_views_.size(); ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    if (i < display_results.size()) {
      if (IsZeroStateFile(*display_results[i])) {
        found_zero_state_file = true;
      } else if (IsDriveQuickAccess(*display_results[i])) {
        found_drive_quick_access = true;
      }

      result_view->SetResult(display_results[i]);
      result_view->SetVisible(true);
    } else {
      result_view->SetResult(nullptr);
      result_view->SetVisible(false);
    }
  }

  auto* notifier = view_delegate_->GetNotifier();
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* result : display_results)
      notifier_results.emplace_back(result->id(), result->metrics_type());
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kList,
                                   notifier_results);
  }

  // Logic for logging impression of items that were shown to user.
  // Each time DoUpdate() called, start a timer that will be fired after a
  // certain amount of time |kImpressionThreshold|. If during the waiting time,
  // there's another DoUpdate() called, reset the timer and start a new timer
  // with updated result list.
  if (impression_timer_.IsRunning())
    impression_timer_.Stop();
  impression_timer_.Start(FROM_HERE, kImpressionThreshold, this,
                          &SearchResultListView::LogImpressions);

  // Log impressions for local zero state files.
  if (!found_zero_state_file)
    zero_state_file_impression_timer_.Stop();
  if (found_zero_state_file && !previous_found_zero_state_file_) {
    zero_state_file_impression_timer_.Start(
        FROM_HERE, kZeroStateImpressionThreshold,
        base::BindOnce(&LogFileImpressions, ZERO_STATE_FILE));
  }
  previous_found_zero_state_file_ = found_zero_state_file;

  // Log impressions for Drive Quick Access files.
  if (!found_drive_quick_access)
    drive_quick_access_impression_timer_.Stop();
  if (found_drive_quick_access && !previous_found_drive_quick_access_) {
    drive_quick_access_impression_timer_.Start(
        FROM_HERE, kZeroStateImpressionThreshold,
        base::BindOnce(&LogFileImpressions, DRIVE_QUICK_ACCESS));
  }
  previous_found_drive_quick_access_ = found_drive_quick_access;

  set_container_score(
      display_results.empty()
          ? -1.0
          : AppListConfig::instance().results_list_container_score());

  return display_results.size();
}

void SearchResultListView::LogImpressions() {
  // Since no items is actually clicked, send the position index of clicked item
  // as -1.
  if (main_view_->search_box_view()->is_search_box_active()) {
    view_delegate_->NotifySearchResultsForLogging(
        view_delegate_->GetSearchModel()->search_box()->text(),
        GetSearchResultsForLogging(search_result_views_),
        -1 /* position_index */);
  }
}

void SearchResultListView::Layout() {
  results_container_->SetBoundsRect(GetLocalBounds());
}

gfx::Size SearchResultListView::CalculatePreferredSize() const {
  return results_container_->GetPreferredSize();
}

const char* SearchResultListView::GetClassName() const {
  return "SearchResultListView";
}

int SearchResultListView::GetHeightForWidth(int w) const {
  return results_container_->GetHeightForWidth(w);
}

void SearchResultListView::SearchResultActivated(SearchResultView* view,
                                                 int event_flags,
                                                 bool by_button_press) {
  if (!view_delegate_ || !view || !view->result())
    return;

  auto* result = view->result();

  RecordSearchResultOpenSource(result, view_delegate_->GetModel(),
                               view_delegate_->GetSearchModel());
  view_delegate_->LogResultLaunchHistogram(
      SearchResultLaunchLocation::kResultList, view->index_in_container());
  view_delegate_->NotifySearchResultsForLogging(
      view_delegate_->GetSearchModel()->search_box()->text(),
      GetSearchResultsForLogging(search_result_views_),
      view->index_in_container());

  view_delegate_->OpenSearchResult(
      result->id(), event_flags, AppListLaunchedFrom::kLaunchedFromSearchBox,
      AppListLaunchType::kSearchResult, -1 /* suggestion_index */,
      !by_button_press && view->is_default_result() /* launch_as_default */);
}

void SearchResultListView::SearchResultActionActivated(SearchResultView* view,
                                                       size_t action_index,
                                                       int event_flags) {
  if (view_delegate_ && view->result()) {
    OmniBoxZeroStateAction action = GetOmniBoxZeroStateAction(action_index);
    if (action == OmniBoxZeroStateAction::kRemoveSuggestion) {
      view_delegate_->InvokeSearchResultAction(view->result()->id(),
                                               action_index, event_flags);
    } else if (action == OmniBoxZeroStateAction::kAppendSuggestion) {
      // Make sure ChromeVox will focus on the search box.
      main_view_->search_box_view()->search_box()->NotifyAccessibilityEvent(
          ax::mojom::Event::kSelection, true);
      main_view_->search_box_view()->UpdateQuery(view->result()->title());
    }
  }
}

void SearchResultListView::OnSearchResultInstalled(SearchResultView* view) {
  if (main_view_ && view->result())
    main_view_->OnResultInstalled(view->result());
}

void SearchResultListView::VisibilityChanged(View* starting_from,
                                             bool is_visible) {
  SearchResultContainerView::VisibilityChanged(starting_from, is_visible);
  // We only do this work when is_visible is false.
  if (is_visible)
    return;

  zero_state_file_impression_timer_.Stop();
  drive_quick_access_impression_timer_.Stop();
  previous_found_zero_state_file_ = false;
  previous_found_drive_quick_access_ = false;
}

std::vector<SearchResult*> SearchResultListView::GetAssistantResults() {
  // Only show Assistant results if there are no tiles. There is not enough
  // room in launcher to display Assistant results if there are tiles visible.
  bool visible_tiles = !SearchModel::FilterSearchResultsByDisplayType(
                            results(), SearchResult::DisplayType::kTile,
                            /*excludes=*/{}, /*max_results=*/1)
                            .empty();

  if (visible_tiles)
    return std::vector<SearchResult*>();

  return SearchModel::FilterSearchResultsByFunction(
      results(), base::BindRepeating([](const SearchResult& search_result) {
        return search_result.display_type() == SearchResultDisplayType::kList &&
               search_result.result_type() ==
                   AppListSearchResultType::kAssistantText;
      }),
      /*max_results=*/
      AppListConfig::instance().max_assistant_search_result_list_items());
}

std::vector<SearchResult*> SearchResultListView::GetSearchResults() {
  std::vector<SearchResult*> search_results =
      SearchModel::FilterSearchResultsByFunction(
          results(), base::BindRepeating([](const SearchResult& result) {
            return result.display_type() == SearchResultDisplayType::kList &&
                   result.result_type() !=
                       AppListSearchResultType::kAssistantText;
          }),
          /*max_results=*/
          AppListConfig::instance().max_search_result_list_items());

  std::vector<SearchResult*> assistant_results = GetAssistantResults();

  search_results.insert(search_results.end(), assistant_results.begin(),
                        assistant_results.end());

  return search_results;
}

}  // namespace ash
