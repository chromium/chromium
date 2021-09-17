// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_task_container_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

using views::GridLayout;

namespace ash {
namespace {
// Suggested tasks padding in dips
constexpr int kSuggestedTasksHorizontalPadding = 6;

// Suggested tasks layout constants.
constexpr int kColumnSpacing = 8;
constexpr int kRowSpacing = 8;
constexpr size_t kMaxFilesForContinueSection = 4;

bool IsFileType(AppListSearchResultType type) {
  return type == AppListSearchResultType::kFileChip ||
         type == AppListSearchResultType::kDriveChip;
}

struct CompareByDisplayIndexAndPositionPriority {
  bool operator()(const SearchResult* result1,
                  const SearchResult* result2) const {
    SearchResultDisplayIndex index1 = result1->display_index();
    SearchResultDisplayIndex index2 = result2->display_index();
    if (index1 != index2)
      return index1 < index2;
    return result1->position_priority() > result2->position_priority();
  }
};

std::vector<SearchResult*> GetTasksResultsFromSuggestionChips(
    SearchModel* search_model) {
  SearchModel::SearchResults* results = search_model->results();
  auto file_chips_filter = [](const SearchResult& r) -> bool {
    return IsFileType(r.result_type()) &&
           r.display_type() == SearchResultDisplayType::kChip;
  };
  std::vector<SearchResult*> file_chips_results =
      SearchModel::FilterSearchResultsByFunction(
          results, base::BindRepeating(file_chips_filter),
          /*max_results=*/4);

  std::sort(file_chips_results.begin(), file_chips_results.end(),
            CompareByDisplayIndexAndPositionPriority());

  return file_chips_results;
}

}  // namespace

ContinueTaskContainerView::ContinueTaskContainerView(
    AppListViewDelegate* view_delegate,
    int columns,
    OnResultsChanged update_callback)
    : view_delegate_(view_delegate), update_callback_(update_callback) {
  DCHECK(!update_callback_.is_null());

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kSuggestedTasksHorizontalPadding)));

  GridLayout* layout = SetLayoutManager(std::make_unique<GridLayout>());
  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  for (int i = 0; i < columns; i++) {
    if (i > 0)
      column_set->AddPaddingColumn(GridLayout::kFixedSize, kColumnSpacing);
    column_set->AddColumn(
        GridLayout::FILL, GridLayout::CENTER, /*resize_percent=*/1.0,
        GridLayout::ColumnSize::kUsePreferred, /*fixed_width=*/0,
        /*min_width=*/0);
  }

  for (size_t i = 0; i < kMaxFilesForContinueSection; ++i) {
    if (i % columns == 0) {
      if (i > 0) {
        layout->StartRowWithPadding(GridLayout::kFixedSize, kColumnSetId,
                                    GridLayout::kFixedSize, kRowSpacing);
      } else {
        layout->StartRow(GridLayout::kFixedSize, kColumnSetId);
      }
    }
    ContinueTaskView* task =
        layout->AddView(std::make_unique<ContinueTaskView>(view_delegate));
    // This view has a predefined number of placeholder tasks views which toggle
    // visibility depending on the result being null or not. The container's
    // visibility is handled on the parent view.
    task->SetVisible(false);
    task->set_index_in_container(i);
    suggestion_tasks_views_.emplace_back(task);
  }
}

ContinueTaskContainerView::~ContinueTaskContainerView() {}

void ContinueTaskContainerView::ListItemsAdded(size_t start, size_t count) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::ListItemsRemoved(size_t start, size_t count) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::ListItemMoved(size_t index,
                                              size_t target_index) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::ListItemsChanged(size_t start, size_t count) {
  ScheduleUpdate();
}

void ContinueTaskContainerView::Update() {
  // Invalidate this callback to cancel a scheduled update.
  update_factory_.InvalidateWeakPtrs();
  std::vector<SearchResult*> tasks =
      GetTasksResultsFromSuggestionChips(view_delegate_->GetSearchModel());

  // Update search results here.
  for (size_t i = 0; i < suggestion_tasks_views_.size(); ++i) {
    suggestion_tasks_views_[i]->SetResult(i < tasks.size() ? tasks[i]
                                                           : nullptr);
  }

  auto* notifier = view_delegate_->GetNotifier();
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* task : tasks)
      notifier_results.emplace_back(task->id(), task->metrics_type());
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kChip,
                                   notifier_results);
  }
  num_results_ = std::min(kMaxFilesForContinueSection, tasks.size());
  if (!update_callback_.is_null())
    update_callback_.Run();
}

void ContinueTaskContainerView::SetResults(
    SearchModel::SearchResults* results) {
  list_model_observation_.Reset();

  results_ = results;
  if (results_)
    list_model_observation_.Observe(results);

  Update();
}

void ContinueTaskContainerView::DisableFocusForShowingActiveFolder(
    bool disabled) {
  for (views::View* child : suggestion_tasks_views_)
    child->SetEnabled(!disabled);
}

void ContinueTaskContainerView::ScheduleUpdate() {
  // When search results are added one by one, each addition generates an update
  // request. Consolidates those update requests into one Update call.
  if (!update_factory_.HasWeakPtrs()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ContinueTaskContainerView::Update,
                                  update_factory_.GetWeakPtr()));
  }
}

BEGIN_METADATA(ContinueTaskContainerView, views::View)
END_METADATA

}  // namespace ash
