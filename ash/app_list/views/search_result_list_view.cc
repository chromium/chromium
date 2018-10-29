// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_list_view.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_result.h"
#include "ash/app_list/views/app_list_main_view.h"
#include "ash/app_list/views/search_result_view.h"
#include "base/bind.h"
#include "base/time/time.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace {

constexpr int kMaxResults = 5;

}  // namespace

namespace app_list {

SearchResultListView::SearchResultListView(AppListMainView* main_view,
                                           AppListViewDelegate* view_delegate)
    : main_view_(main_view),
      view_delegate_(view_delegate),
      results_container_(new views::View) {
  results_container_->SetLayoutManager(
      std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical));

  for (int i = 0; i < kMaxResults; ++i) {
    search_result_views_.emplace_back(
        new SearchResultView(this, view_delegate_));
    results_container_->AddChildView(search_result_views_.back());
  }
  AddChildView(results_container_);
}

SearchResultListView::~SearchResultListView() = default;

SearchResultView* SearchResultListView::GetResultViewAt(size_t index) {
  DCHECK(index >= 0 && index < search_result_views_.size());
  return search_result_views_[index];
}

void SearchResultListView::ListItemsRemoved(size_t start, size_t count) {
  size_t last = std::min(start + count, search_result_views_.size());
  for (size_t i = start; i < last; ++i)
    GetResultViewAt(i)->ClearResultNoRepaint();

  SearchResultContainerView::ListItemsRemoved(start, count);
}

void SearchResultListView::NotifyFirstResultYIndex(int y_index) {
  for (size_t i = 0; i < static_cast<size_t>(num_results()); ++i)
    GetResultViewAt(i)->result()->set_distance_from_origin(i + y_index);
}

int SearchResultListView::GetYSize() {
  return num_results();
}

SearchResultBaseView* SearchResultListView::GetFirstResultView() {
  DCHECK(results_container_->has_children());
  return num_results() <= 0 ? nullptr : search_result_views_[0];
}

int SearchResultListView::DoUpdate() {
  std::vector<SearchResult*> display_results =
      SearchModel::FilterSearchResultsByDisplayType(
          results(), ash::SearchResultDisplayType::kList, /*excludes=*/{},
          results_container_->child_count());

  for (size_t i = 0; i < static_cast<size_t>(results_container_->child_count());
       ++i) {
    SearchResultView* result_view = GetResultViewAt(i);
    result_view->set_is_last_result(i == display_results.size() - 1);
    if (i < display_results.size()) {
      result_view->SetResult(display_results[i]);
      result_view->SetVisible(true);
    } else {
      result_view->SetResult(NULL);
      result_view->SetVisible(false);
    }
  }

  set_container_score(
      display_results.empty() ? 0 : display_results.front()->display_score());

  return display_results.size();
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
                                                 int event_flags) {
  if (view_delegate_ && view->result()) {
    RecordSearchResultOpenSource(view->result(), view_delegate_->GetModel(),
                                 view_delegate_->GetSearchModel());
    view_delegate_->OpenSearchResult(view->result()->id(), event_flags);
  }
}

void SearchResultListView::SearchResultActionActivated(SearchResultView* view,
                                                       size_t action_index,
                                                       int event_flags) {
  if (view_delegate_ && view->result()) {
    view_delegate_->InvokeSearchResultAction(view->result()->id(), action_index,
                                             event_flags);
  }
}

void SearchResultListView::OnSearchResultInstalled(SearchResultView* view) {
  if (main_view_ && view->result())
    main_view_->OnResultInstalled(view->result());
}

}  // namespace app_list
