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
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/table_layout.h"

using views::BoxLayout;
using views::FlexLayout;
using views::TableLayout;

namespace ash {
namespace {
// Suggested tasks layout constants.
constexpr int kColumnInnerSpacingClamshell = 8;
constexpr int kColumnOuterSpacingClamshell = 6;
constexpr int kColumnSpacingTablet = 16;
constexpr int kRowSpacing = 8;
constexpr size_t kMaxFilesForContinueSection = 4;

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

std::vector<SearchResult*> GetTasksResultsForContinueSection(
    SearchModel::SearchResults* results) {
  auto continue_filter = [](const SearchResult& r) -> bool {
    return r.display_type() == SearchResultDisplayType::kContinue;
  };
  std::vector<SearchResult*> continue_results;
  continue_results = SearchModel::FilterSearchResultsByFunction(
      results, base::BindRepeating(continue_filter),
      /*max_results=*/4);

  std::sort(continue_results.begin(), continue_results.end(),
            CompareByDisplayIndexAndPositionPriority());

  return continue_results;
}

}  // namespace

ContinueTaskContainerView::ContinueTaskContainerView(
    AppListViewDelegate* view_delegate,
    int columns,
    OnResultsChanged update_callback,
    bool tablet_mode)
    : view_delegate_(view_delegate),
      update_callback_(update_callback),
      tablet_mode_(tablet_mode) {
  DCHECK(!update_callback_.is_null());

  if (tablet_mode_) {
    InitializeFlexLayout();
  } else {
    columns_ = columns;
    InitializeTableLayout();
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
      GetTasksResultsForContinueSection(results_);

  RemoveAllChildViews();
  suggestion_tasks_views_.clear();
  num_results_ = std::min(kMaxFilesForContinueSection, tasks.size());

  for (size_t i = 0; i < num_results_; ++i) {
    auto task =
        std::make_unique<ContinueTaskView>(view_delegate_, tablet_mode_);
    if (i == 0)
      task->SetProperty(views::kMarginsKey, gfx::Insets());
    task->set_index_in_container(i);
    task->SetResult(tasks[i]);
    suggestion_tasks_views_.emplace_back(task.get());
    AddChildView(std::move(task));
  }

  auto* notifier = view_delegate_->GetNotifier();
  if (notifier) {
    std::vector<AppListNotifier::Result> notifier_results;
    for (const auto* task : tasks)
      notifier_results.emplace_back(task->id(), task->metrics_type());
    notifier->NotifyResultsUpdated(SearchResultDisplayType::kList,
                                   notifier_results);
  }
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

void ContinueTaskContainerView::InitializeFlexLayout() {
  DCHECK(tablet_mode_);
  DCHECK(!table_layout_);
  DCHECK(!columns_);

  flex_layout_ = SetLayoutManager(std::make_unique<FlexLayout>());
  flex_layout_->SetOrientation(views::LayoutOrientation::kHorizontal)
      .SetMainAxisAlignment(views::LayoutAlignment::kCenter)
      .SetDefault(views::kMarginsKey,
                  gfx::Insets(0, kColumnSpacingTablet, 0, 0))
      .SetDefault(views::kFlexBehaviorKey,
                  views::FlexSpecification(
                      views::MinimumFlexSizeRule::kScaleToMinimumSnapToZero,
                      views::MaximumFlexSizeRule::kScaleToMaximum));
}

void ContinueTaskContainerView::InitializeTableLayout() {
  DCHECK(!tablet_mode_);
  DCHECK(!flex_layout_);
  DCHECK_GT(columns_, 0);

  table_layout_ = SetLayoutManager(std::make_unique<views::TableLayout>());
  std::vector<size_t> linked_columns;
  for (int i = 0; i < columns_; i++) {
    if (i == 0) {
      table_layout_->AddPaddingColumn(views::TableLayout::kFixedSize,
                                      kColumnOuterSpacingClamshell);
    } else {
      table_layout_->AddPaddingColumn(views::TableLayout::kFixedSize,
                                      kColumnInnerSpacingClamshell);
    }
    table_layout_->AddColumn(
        views::LayoutAlignment::kStretch, views::LayoutAlignment::kCenter,
        /*horizontal_resize=*/1.0f, views::TableLayout::ColumnSize::kFixed,
        /*fixed_width=*/0, /*min_width=*/0);
    linked_columns.push_back(2 * i + 1);
  }
  table_layout_->AddPaddingColumn(views::TableLayout::kFixedSize,
                                  kColumnOuterSpacingClamshell);

  table_layout_->LinkColumnSizes(linked_columns);
  // Continue section only shows if there are 3 or more suggestions, so there
  // are always 2 rows.
  table_layout_->AddRows(1, views::TableLayout::kFixedSize);
  table_layout_->AddPaddingRow(views::TableLayout::kFixedSize, kRowSpacing);
  table_layout_->AddRows(1, views::TableLayout::kFixedSize);
}

BEGIN_METADATA(ContinueTaskContainerView, views::View)
END_METADATA

}  // namespace ash
