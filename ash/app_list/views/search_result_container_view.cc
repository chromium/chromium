// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/search_result_container_view.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace ash {

SearchResultContainerView::SearchResultContainerView(
    AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate) {
  DCHECK(view_delegate);
}

SearchResultContainerView::~SearchResultContainerView() {
  if (results_)
    results_->RemoveObserver(this);
}

void SearchResultContainerView::SetResults(
    SearchModel::SearchResults* results) {
  if (results_)
    results_->RemoveObserver(this);

  results_ = results;
  if (results_)
    results_->AddObserver(this);

  Update();
}

void SearchResultContainerView::NotifyFirstResultYIndex(int /*y_index*/) {
  NOTREACHED();
}

int SearchResultContainerView::GetYSize() {
  NOTREACHED();
  return 0;
}

void SearchResultContainerView::Update() {
  update_factory_.InvalidateWeakPtrs();
  num_results_ = DoUpdate();
  Layout();
  if (delegate_)
    delegate_->OnSearchResultContainerResultsChanged();
}

bool SearchResultContainerView::UpdateScheduled() {
  return update_factory_.HasWeakPtrs();
}

const char* SearchResultContainerView::GetClassName() const {
  return "SearchResultContainerView";
}

void SearchResultContainerView::OnViewFocused(View* observed_view) {
  if (delegate_) {
    delegate_->OnSearchResultContainerResultFocused(
        static_cast<SearchResultBaseView*>(observed_view));
  }
}

void SearchResultContainerView::AddObservedResultView(
    SearchResultBaseView* result_view) {
  result_view_observer_.Add(result_view);
}

void SearchResultContainerView::RemoveObservedResultView(
    SearchResultBaseView* result_view) {
  result_view_observer_.Remove(result_view);
}

void SearchResultContainerView::ListItemsAdded(size_t /*start*/,
                                               size_t /*count*/) {
  ScheduleUpdate();
}

void SearchResultContainerView::ListItemsRemoved(size_t /*start*/,
                                                 size_t /*count*/) {
  ScheduleUpdate();
}

void SearchResultContainerView::ListItemMoved(size_t /*index*/,
                                              size_t /*target_index*/) {
  ScheduleUpdate();
}

void SearchResultContainerView::ListItemsChanged(size_t /*start*/,
                                                 size_t /*count*/) {
  ScheduleUpdate();
}

SearchResultBaseView* SearchResultContainerView::GetFirstResultView() {
  return nullptr;
}

void SearchResultContainerView::SetShown(bool shown) {
  if (shown_ == shown) {
    return;
  }
  shown_ = shown;
  OnShownChanged();
}

void SearchResultContainerView::OnShownChanged() {}

void SearchResultContainerView::ScheduleUpdate() {
  // When search results are added one by one, each addition generates an update
  // request. Consolidates those update requests into one Update call.
  if (!update_factory_.HasWeakPtrs()) {
    if (delegate_)
      delegate_->OnSearchResultContainerResultsChanging();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&SearchResultContainerView::Update,
                                  update_factory_.GetWeakPtr()));
  }
}

}  // namespace ash
