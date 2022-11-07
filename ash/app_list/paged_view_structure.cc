// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/paged_view_structure.h"

#include <algorithm>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "ui/views/view_model.h"

namespace ash {

PagedViewStructure::ScopedSanitizeLock::ScopedSanitizeLock(
    PagedViewStructure* view_structure)
    : view_structure_(view_structure) {
  ++view_structure_->sanitize_locks_;
}

PagedViewStructure::ScopedSanitizeLock::~ScopedSanitizeLock() {
  --view_structure_->sanitize_locks_;
  view_structure_->Sanitize();
}

PagedViewStructure::PagedViewStructure(AppsGridView* apps_grid_view)
    : apps_grid_view_(apps_grid_view) {}

PagedViewStructure::~PagedViewStructure() {
  DCHECK_EQ(0, sanitize_locks_);
}

void PagedViewStructure::Init(Mode mode) {
  mode_ = mode;
}

std::unique_ptr<PagedViewStructure::ScopedSanitizeLock>
PagedViewStructure::GetSanitizeLock() {
  return std::make_unique<ScopedSanitizeLock>(this);
}

void PagedViewStructure::LoadFromMetadata() {
  const auto* view_model = apps_grid_view_->view_model();

  pages_.clear();
  pages_.emplace_back();

  if (mode_ == Mode::kSinglePage) {
    // Copy the view model to a single page.
    pages_[0].reserve(view_model->view_size());
    for (size_t i = 0; i < view_model->view_size(); ++i) {
      pages_[0].push_back(view_model->view_at(i));
    }
    return;
  }

  // Copy the view model to N full pages.
  for (size_t i = 0; i < view_model->view_size(); ++i) {
    if (pages_.back().size() ==
        static_cast<size_t>(TilesPerPage(pages_.size() - 1))) {
      pages_.emplace_back();
    }
    pages_.back().push_back(view_model->view_at(i));
  }
}

void PagedViewStructure::Move(AppListItemView* view,
                              const GridIndex& target_index) {
  // Do not sanitize view structure after Remove() call.
  std::unique_ptr<ScopedSanitizeLock> sanitize_lock = GetSanitizeLock();
  Remove(view);
  Add(view, target_index);
}

void PagedViewStructure::Remove(AppListItemView* view) {
  for (auto& page : pages_) {
    auto iter = base::ranges::find(page, view);
    if (iter != page.end()) {
      page.erase(iter);
      break;
    }
  }

  Sanitize();
}

void PagedViewStructure::Add(AppListItemView* view,
                             const GridIndex& target_index) {
  const int view_structure_size = total_pages();
  if (target_index.page < view_structure_size) {
    // Adding to an existing page.
    CHECK_LE(target_index.slot, items_on_page(target_index.page));
  } else {
    // Adding to a new page at the end.
    CHECK_EQ(target_index.page, view_structure_size);
    CHECK_EQ(target_index.slot, 0);
  }

  if (target_index.page == view_structure_size)
    pages_.emplace_back();

  auto& page = pages_[target_index.page];
  page.insert(page.begin() + target_index.slot, view);

  Sanitize();
}

GridIndex PagedViewStructure::GetIndexFromModelIndex(int model_index) const {
  if (mode_ == Mode::kSinglePage)
    return GridIndex(0, model_index);

  int current_page = 0;
  while (model_index >= TilesPerPage(current_page)) {
    model_index -= TilesPerPage(current_page);
    ++current_page;
  }
  return GridIndex(current_page, model_index);
}

int PagedViewStructure::GetModelIndexFromIndex(const GridIndex& index) const {
  if (mode_ == Mode::kSinglePage) {
    DCHECK_EQ(index.page, 0);
    return index.slot;
  }

  int model_index = 0;
  for (int i = 0; i < index.page; i++) {
    model_index += TilesPerPage(i);
  }
  model_index += index.slot;
  return model_index;
}

GridIndex PagedViewStructure::GetLastTargetIndex() const {
  if (apps_grid_view_->view_model()->view_size() == 0)
    return GridIndex(0, 0);

  size_t view_index = apps_grid_view_->view_model()->view_size();

  // If a view in the current view model is being dragged, then ignore it.
  if (apps_grid_view_->drag_view())
    --view_index;
  return GetIndexFromModelIndex(view_index);
}

GridIndex PagedViewStructure::GetLastTargetIndexOfPage(int page_index) const {
  if (mode_ == Mode::kSinglePage) {
    DCHECK_EQ(page_index, 0);
    return GetLastTargetIndex();
  }

  if (page_index == apps_grid_view_->GetTotalPages() - 1)
    return GetLastTargetIndex();

  return GridIndex(page_index, TilesPerPage(page_index) - 1);
}

int PagedViewStructure::GetTargetModelIndexForMove(
    AppListItem* moved_item,
    const GridIndex& index) const {
  return GetModelIndexFromIndex(index);
}

int PagedViewStructure::GetTargetItemListIndexForMove(
    AppListItem* moved_item,
    const GridIndex& index) const {
  if (mode_ == Mode::kFullPages)
    return GetModelIndexFromIndex(index);

  DCHECK_EQ(index.page, 0);
  GridIndex current_index(0, 0);
  size_t current_item_index = 0;

  const auto* item_list = apps_grid_view_->item_list_;
  while (current_item_index < item_list->item_count() &&
         item_list->item_at(current_item_index)->is_page_break()) {
    ++current_item_index;
  }

  while (current_item_index < item_list->item_count() &&
         current_index != index) {
    if (!item_list->item_at(current_item_index)->is_page_break())
      ++current_index.slot;
    ++current_item_index;
  }
  DCHECK_EQ(current_index, index);
  return current_item_index;
}

bool PagedViewStructure::IsValidReorderTargetIndex(
    const GridIndex& index) const {
  if (apps_grid_view_->IsValidIndex(index))
    return true;

  // The user can drag an item view to another page's end. Also covers the case
  // where a dragged folder item is being reparented to the last target index of
  // the root level grid.
  if (index.page < total_pages() &&
      GetLastTargetIndexOfPage(index.page) == index) {
    return true;
  }

  return false;
}

void PagedViewStructure::AppendPage() {
  DCHECK_NE(mode_, Mode::kSinglePage);
  pages_.emplace_back();
}

bool PagedViewStructure::IsFullPage(int page_index) const {
  if (page_index >= total_pages())
    return false;
  return static_cast<int>(pages_[page_index].size()) ==
         TilesPerPage(page_index);
}

void PagedViewStructure::Sanitize() {
  if (sanitize_locks_ == 0) {
    ClearOverflow();
    ClearEmptyPages();
  }
}

void PagedViewStructure::ClearOverflow() {
  std::vector<AppListItemView*> overflow_views;
  auto iter = pages_.begin();
  while (iter != pages_.end() || !overflow_views.empty()) {
    if (iter == pages_.end()) {
      // Add additional page if overflowing item views remain.
      pages_.emplace_back();
      iter = pages_.end() - 1;
    }

    const size_t max_item_views =
        TilesPerPage(static_cast<int>(iter - pages_.begin()));
    auto& page = *iter;

    if (!overflow_views.empty()) {
      // Put overflowing item views in current page.
      page.insert(page.begin(), overflow_views.begin(), overflow_views.end());
      overflow_views.clear();
    }

    if (page.size() > max_item_views) {
      // Remove overflowing item views from current page.
      overflow_views.insert(overflow_views.begin(),
                            page.begin() + max_item_views, page.end());
      page.erase(page.begin() + max_item_views, page.end());
    }

    ++iter;
  }
}

void PagedViewStructure::ClearEmptyPages() {
  auto iter = pages_.begin();
  while (iter != pages_.end()) {
    if (iter->empty()) {
      // Remove empty page.
      iter = pages_.erase(iter);
    } else {
      ++iter;
    }
  }
}

int PagedViewStructure::TilesPerPage(int page) const {
  return apps_grid_view_->TilesPerPage(page);
}

}  // namespace ash
