// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/continue_section_view.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/search/search_model.h"
#include "ash/app_list/views/continue_task_view.h"
#include "ash/bubble/bubble_utils.h"
#include "ash/bubble/simple_grid_layout.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_notifier.h"
#include "base/check.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// Horizontal space between suggestions in dips.
constexpr int kHorizontalSpacing = 8;

constexpr int kContinueColumnCount = 2;
constexpr int kContinueColumnSpacing = 16;
constexpr int kContinueRowSpacing = 10;

std::unique_ptr<views::Label> CreateLabel(const std::u16string& text) {
  auto label = std::make_unique<views::Label>(text);
  bubble_utils::ApplyStyle(label.get(), bubble_utils::LabelStyle::kBody);
  return label;
}

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

ContinueSectionView::ContinueSectionView(AppListViewDelegate* view_delegate)
    : view_delegate_(view_delegate) {
  DCHECK(view_delegate_);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, gfx::Insets(),
      kHorizontalSpacing));
  layout->set_main_axis_alignment(views::BoxLayout::MainAxisAlignment::kCenter);

  // TODO(https://crbug.com/1204551): Localized strings.
  // TODO(https://crbug.com/1204551): Styling.
  auto* continue_label = AddChildView(CreateLabel(u"Label"));
  continue_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);

  suggestions_container_ = AddChildView(std::make_unique<views::View>());
  suggestions_container_->SetLayoutManager(std::make_unique<SimpleGridLayout>(
      kContinueColumnCount, kContinueColumnSpacing, kContinueRowSpacing));

  std::vector<SearchResult*> tasks =
      GetTasksResultsFromSuggestionChips(view_delegate_->GetSearchModel());

  for (SearchResult* task : tasks) {
    suggestions_container_->AddChildView(
        std::make_unique<ContinueTaskView>(task));
  }
  SetVisible(!tasks.empty());
}

ContinueSectionView::~ContinueSectionView() = default;

size_t ContinueSectionView::GetTasksSuggestionsCount() const {
  return suggestions_container_->children().size();
}

ContinueTaskView* ContinueSectionView::GetTaskViewAtForTesting(
    size_t index) const {
  DCHECK_GT(GetTasksSuggestionsCount(), index);
  return static_cast<ContinueTaskView*>(
      suggestions_container_->children()[index]);
}

BEGIN_METADATA(ContinueSectionView, views::View)
END_METADATA

}  // namespace ash
