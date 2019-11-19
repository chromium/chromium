// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/paged_view_structure.h"

#include <algorithm>

#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "base/stl_util.h"
#include "ui/views/view_model.h"

namespace ash {

PagedViewStructure::PagedViewStructure(AppsGridView* apps_grid_view)
    : apps_grid_view_(apps_grid_view) {}

PagedViewStructure::PagedViewStructure(const PagedViewStructure& other) =
    default;

PagedViewStructure::~PagedViewStructure() = default;

void PagedViewStructure::LoadFromMetadata() {
  int model_index = 0;
  pages_.clear();
  pages_.emplace_back();

  for (size_t i = 0; i < apps_grid_view_->item_list_->item_count(); ++i) {
    const auto* item = apps_grid_view_->item_list_->item_at(i);
    if (item->is_page_break()) {
      // Create a new page if a "page break" item is detected and current page
      // is not empty. Otherwise, ignore the "page break" item.
      if (!pages_.back().empty())
        pages_.emplace_back();
      continue;
    }

    // Create a new page if the current page is full.
    const size_t current_page_max_items =
        apps_grid_view_->TilesPerPage(pages_.size() - 1);
    if (pages_.back().size() == current_page_max_items)
      pages_.emplace_back();

    pages_.back().emplace_back(
        apps_grid_view_->view_model()->view_at(model_index++));
  }

  // Remove trailing empty page if exist.
  if (pages_.back().empty())
    pages_.pop_back();
}

void PagedViewStructure::SaveToMetadata() {
  auto* item_list = apps_grid_view_->item_list_;
  size_t item_index = 0;
  AppListModel* model = apps_grid_view_->model_;

  for (const auto& page : pages_) {
    // Skip all "page break" items before current page.
    while (item_index < item_list->item_count() &&
           item_list->item_at(item_index)->is_page_break()) {
      ++item_index;
    }

    // A "page break" item may exist between two app items in a full page after
    // moving an item to the page from another page or folder. The last item in
    // the page was pushed to the next page by ClearOverflow() while the "page
    // break" item behind still exist. In this case, we need to remove the "page
    // break" item.
    for (size_t i = 0;
         i < page.size() && item_index < item_list->item_count();) {
      const auto* item = item_list->item_at(item_index);
      if (item->is_page_break()) {
        // Remove AppListItemListObserver temporarily to avoid |pages_| being
        // reloaded.
        item_list->RemoveObserver(apps_grid_view_);

        // Do not increase |item_index| after this call because it modifies
        // |item_list|.
        model->DeleteItem(item->id());
        item_list->AddObserver(apps_grid_view_);
        continue;
      }

      DCHECK_EQ(item, page[i]->item());
      ++i;
      ++item_index;
    }

    if (item_index < item_list->item_count() &&
        !item_list->item_at(item_index)->is_page_break()) {
      // Remove AppListItemListObserver temporarily to avoid |pages_| being
      // reloaded.
      item_list->RemoveObserver(apps_grid_view_);

      // There's no "page break" item at the end of current page, so add one to
      // push overflowing items to next page.
      model->AddPageBreakItemAfter(item_list->item_at(item_index - 1));
      item_list->AddObserver(apps_grid_view_);
    }
  }

  // Note that we do not remove redundant "page break" items here because the
  // item list we can access here may not be complete (e.g. Devices that do not
  // support ARC++ or Crostini apps filter out those items.). We leave this
  // operation to AppListSyncableService which has complete item list.
}

void PagedViewStructure::Move(AppListItemView* view,
                              const GridIndex& target_index,
                              bool clear_overflow,
                              bool clear_empty_pages) {
  Remove(view, false /* clear_overflow */, false /* clear_empty_pages */);
  Add(view, target_index, clear_overflow, clear_empty_pages);
}

void PagedViewStructure::Remove(AppListItemView* view,
                                bool clear_overflow,
                                bool clear_empty_pages) {
  for (auto& page : pages_) {
    auto iter = std::find(page.begin(), page.end(), view);
    if (iter != page.end()) {
      page.erase(iter);
      break;
    }
  }

  if (clear_overflow)
    ClearOverflow();

  if (clear_empty_pages)
    ClearEmptyPages();
}

void PagedViewStructure::Add(AppListItemView* view,
                             const GridIndex& target_index,
                             bool clear_overflow,
                             bool clear_empty_pages) {
  const int view_structure_size = total_pages();
  CHECK((target_index.page < view_structure_size &&
         target_index.slot <= items_on_page(target_index.page)) ||
        (target_index.page == view_structure_size && target_index.slot == 0));

  if (target_index.page == view_structure_size)
    pages_.emplace_back();

  auto& page = pages_[target_index.page];
  page.insert(page.begin() + target_index.slot, view);

  if (clear_overflow)
    ClearOverflow();

  if (clear_empty_pages)
    ClearEmptyPages();
}

GridIndex PagedViewStructure::GetIndexFromModelIndex(int model_index) const {
  AppListItemView* view = apps_grid_view_->view_model()->view_at(model_index);
  for (size_t i = 0; i < pages_.size(); ++i) {
    auto& page = pages_[i];
    for (size_t j = 0; j < page.size(); ++j) {
      if (page[j] == view)
        return GridIndex(i, j);
    }
  }
  return GetLastTargetIndex();
}

int PagedViewStructure::GetModelIndexFromIndex(const GridIndex& index) const {
  auto* view_model = apps_grid_view_->view_model();
  if (index.page >= total_pages() || index.slot >= items_on_page(index.page))
    return view_model->view_size();

  AppListItemView* view = pages_[index.page][index.slot];
  return view_model->GetIndexOfView(view);
}

GridIndex PagedViewStructure::GetLastTargetIndex() const {
  if (apps_grid_view_->view_model()->view_size() == 0)
    return GridIndex(0, 0);

  int last_page_index = total_pages() - 1;
  int target_slot = CalculateTargetSlot(pages_.back());
  if (target_slot == apps_grid_view_->TilesPerPage(last_page_index)) {
    // The last page is full, so the last target visual index is the first slot
    // in the next new page.
    target_slot = 0;
    ++last_page_index;
  }
  return GridIndex(last_page_index, target_slot);
}

GridIndex PagedViewStructure::GetLastTargetIndexOfPage(int page_index) const {
  const int page_size = total_pages();
  DCHECK_LT(0, apps_grid_view_->view_model()->view_size());
  DCHECK_LE(page_index, page_size);

  if (page_index == page_size)
    return GridIndex(page_index, 0);

  int target_slot = CalculateTargetSlot(pages_[page_index]);
  if (target_slot == apps_grid_view_->TilesPerPage(page_index)) {
    // The specified page is full, so the last target visual index is the last
    // slot in the page_index.
    --target_slot;
  }
  return GridIndex(page_index, target_slot);
}

int PagedViewStructure::GetTargetModelIndexForMove(
    AppListItemView* moved_view,
    const GridIndex& index) const {
  int target_model_index = 0;
  const int max_page = std::min(index.page, total_pages());
  for (int i = 0; i < max_page; ++i) {
    auto& page = pages_[i];
    target_model_index += page.size();

    // Skip the item view to be moved in the page if found.
    // Decrement |target_model_index| if |moved_view| is in this page because it
    // is represented by a placeholder.
    auto iter = std::find(page.begin(), page.end(), moved_view);
    if (iter != page.end())
      --target_model_index;
  }

  // If the target visual index is in the same page, do not skip the item view
  // because the following item views will fill the gap in the page.
  target_model_index += index.slot;
  return target_model_index;
}

int PagedViewStructure::GetTargetItemIndexForMove(
    AppListItemView* moved_view,
    const GridIndex& index) const {
  GridIndex current_index(0, 0);
  size_t current_item_index = 0;
  size_t offset = 0;
  const auto* item_list = apps_grid_view_->item_list_;

  // Skip the leading "page break" items.
  while (current_item_index < item_list->item_count() &&
         item_list->item_at(current_item_index)->is_page_break()) {
    ++current_item_index;
  }

  while (current_item_index < item_list->item_count()) {
    while (current_item_index < item_list->item_count() &&
           !item_list->item_at(current_item_index)->is_page_break() &&
           current_index != index) {
      if (moved_view->item() == item_list->item_at(current_item_index) &&
          current_index.page < index.page) {
        // If the item view is moved to a following page, we need to skip the
        // item view. If the view is moved to the same page, do not skip the
        // item view because the following item views will fill the gap left
        // after dragging complete.
        offset = 1;
      }
      ++current_index.slot;
      ++current_item_index;
    }

    if (current_index == index)
      return current_item_index - offset;

    // Skip the "page break" items at the end of the page.
    while (current_item_index < item_list->item_count() &&
           item_list->item_at(current_item_index)->is_page_break()) {
      ++current_item_index;
    }
    ++current_index.page;
    current_index.slot = 0;
  }
  DCHECK(current_index == index);
  return current_item_index - offset;
}

bool PagedViewStructure::IsValidReorderTargetIndex(
    const GridIndex& index) const {
  if (apps_grid_view_->IsValidIndex(index))
    return true;

  // The user can drag an item view to another page's end.
  if (index.page <= total_pages() &&
      GetLastTargetIndexOfPage(index.page) == index) {
    return true;
  }

  return false;
}

void PagedViewStructure::AppendPage() {
  pages_.emplace_back();
}

bool PagedViewStructure::IsFullPage(int page_index) const {
  if (page_index >= total_pages())
    return false;
  return static_cast<int>(pages_[page_index].size()) ==
         apps_grid_view_->TilesPerPage(page_index);
}

int PagedViewStructure::CalculateTargetSlot(const Page& page) const {
  size_t target_slot = page.size();
  if (base::Contains(page, apps_grid_view_->drag_view()))
    --target_slot;
  return static_cast<int>(target_slot);
}

bool PagedViewStructure::ClearOverflow() {
  bool changed = false;
  std::vector<AppListItemView*> overflow_views;
  auto iter = pages_.begin();
  while (iter != pages_.end() || !overflow_views.empty()) {
    if (iter == pages_.end()) {
      // Add additional page if overflowing item views remain.
      pages_.emplace_back();
      iter = pages_.end() - 1;
      changed = true;
    }

    const size_t max_item_views =
        apps_grid_view_->TilesPerPage(static_cast<int>(iter - pages_.begin()));
    auto& page = *iter;

    if (!overflow_views.empty()) {
      // Put overflowing item views in current page.
      page.insert(page.begin(), overflow_views.begin(), overflow_views.end());
      overflow_views.clear();
      changed = true;
    }

    if (page.size() > max_item_views) {
      // Remove overflowing item views from current page.
      overflow_views.insert(overflow_views.begin(),
                            page.begin() + max_item_views, page.end());
      page.erase(page.begin() + max_item_views, page.end());
      changed = true;
    }

    ++iter;
  }
  return changed;
}

bool PagedViewStructure::ClearEmptyPages() {
  bool changed = false;
  auto iter = pages_.begin();
  while (iter != pages_.end()) {
    if (iter->empty()) {
      // Remove empty page.
      iter = pages_.erase(iter);
      changed = true;
    } else {
      ++iter;
    }
  }
  return changed;
}

}  // namespace ash
