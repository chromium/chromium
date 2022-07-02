// Copyright 2018 The Chromium Authors. All rights reserved.
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

void PagedViewStructure::AllowEmptyPages() {
  empty_pages_allowed_ = true;
}

void PagedViewStructure::LoadFromOther(const PagedViewStructure& other) {
  DCHECK_EQ(apps_grid_view_, other.apps_grid_view_);

  mode_ = other.mode_;
  pages_ = other.pages_;

  Sanitize();
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

  if (mode_ == Mode::kFullPages) {
    // Copy the view model to N full pages.
    for (size_t i = 0; i < view_model->view_size(); ++i) {
      if (pages_.back().size() ==
          static_cast<size_t>(TilesPerPage(pages_.size() - 1))) {
        pages_.emplace_back();
      }
      pages_.back().push_back(view_model->view_at(i));
    }
    return;
  }

  int model_index = 0;
  const size_t item_count = apps_grid_view_->item_list_
                                ? apps_grid_view_->item_list_->item_count()
                                : 0;
  for (size_t i = 0; i < item_count; ++i) {
    const auto* item = apps_grid_view_->item_list_->item_at(i);
    if (item->is_page_break()) {
      // Create a new page if a "page break" item is detected and current page
      // is not empty. Otherwise, ignore the "page break" item.
      if (!pages_.back().empty())
        pages_.emplace_back();
      continue;
    }

    // Create a new page if the current page is full.
    const size_t current_page_max_items = TilesPerPage(pages_.size() - 1);
    if (sanitize_locks_ == 0 && pages_.back().size() == current_page_max_items)
      pages_.emplace_back();

    pages_.back().emplace_back(
        apps_grid_view_->view_model()->view_at(model_index++));
  }

  // Remove trailing empty page if exist.
  if (!empty_pages_allowed_ && sanitize_locks_ == 0 && pages_.back().empty())
    pages_.pop_back();
}

void PagedViewStructure::SaveToMetadata() {
  // When ignoring page breaks we don't need to add or remove page breaks from
  // the data model.
  if (mode_ == Mode::kFullPages || mode_ == Mode::kSinglePage)
    return;

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

    // When removing launcher spaces is enabled, all launcher pages expect for
    // the last one should be full (i.e. no empty spaces). Therefore page break
    // items are useless. It is why we should only create page break items when
    // the feature flag is disabled.
    if (!features::IsProductivityLauncherEnabled() &&
        item_index < item_list->item_count() &&
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
                              const GridIndex& target_index) {
  // Do not sanitize view structure after Remove() call.
  std::unique_ptr<ScopedSanitizeLock> sanitize_lock = GetSanitizeLock();
  Remove(view);
  Add(view, target_index);
}

void PagedViewStructure::Remove(AppListItemView* view) {
  for (auto& page : pages_) {
    auto iter = std::find(page.begin(), page.end(), view);
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

  if (mode_ == Mode::kFullPages) {
    int current_page = 0;
    while (model_index >= TilesPerPage(current_page)) {
      model_index -= TilesPerPage(current_page);
      ++current_page;
    }
    return GridIndex(current_page, model_index);
  }

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
  if (mode_ == Mode::kSinglePage) {
    DCHECK_EQ(index.page, 0);
    return index.slot;
  }

  if (mode_ == Mode::kFullPages) {
    int model_index = 0;
    for (int i = 0; i < index.page; i++) {
      model_index += TilesPerPage(i);
    }
    model_index += index.slot;
    return model_index;
  }

  auto* view_model = apps_grid_view_->view_model();
  if (index.page >= total_pages() || index.slot >= items_on_page(index.page))
    return view_model->view_size();

  AppListItemView* view = pages_[index.page][index.slot];
  return view_model->GetIndexOfView(view).value();
}

GridIndex PagedViewStructure::GetLastTargetIndex() const {
  if (apps_grid_view_->view_model()->view_size() == 0)
    return GridIndex(0, 0);

  if (mode_ == Mode::kSinglePage || mode_ == Mode::kFullPages) {
    size_t view_index = apps_grid_view_->view_model()->view_size();

    // If a view in the current view model is being dragged, then ignore it.
    if (apps_grid_view_->drag_view())
      --view_index;
    return GetIndexFromModelIndex(view_index);
  }

  int last_page_index = total_pages() - 1;
  int target_slot = CalculateTargetSlot(pages_.back());
  if (target_slot == TilesPerPage(last_page_index)) {
    // The last page is full, so the last target visual index is the first slot
    // in the next new page.
    target_slot = 0;
    ++last_page_index;
  }
  return GridIndex(last_page_index, target_slot);
}

GridIndex PagedViewStructure::GetLastTargetIndexOfPage(int page_index) const {
  if (mode_ == Mode::kSinglePage) {
    DCHECK_EQ(page_index, 0);
    return GetLastTargetIndex();
  }

  if (mode_ == Mode::kFullPages) {
    if (page_index == apps_grid_view_->GetTotalPages() - 1)
      return GetLastTargetIndex();
    return GridIndex(page_index, TilesPerPage(page_index) - 1);
  }

  const int page_size = total_pages();
  DCHECK_LT(0u, apps_grid_view_->view_model()->view_size());
  DCHECK_LE(page_index, page_size);

  if (page_index == page_size)
    return GridIndex(page_index, 0);

  int target_slot = CalculateTargetSlot(pages_[page_index]);
  if (target_slot == TilesPerPage(page_index)) {
    // The specified page is full, so the last target visual index is the last
    // slot in the page_index.
    --target_slot;
  }
  return GridIndex(page_index, target_slot);
}

int PagedViewStructure::GetTargetModelIndexForMove(
    AppListItem* moved_item,
    const GridIndex& index) const {
  if (mode_ == Mode::kSinglePage || mode_ == Mode::kFullPages)
    return GetModelIndexFromIndex(index);

  int target_model_index = 0;
  const int max_page = std::min(index.page, total_pages());
  for (int i = 0; i < max_page; ++i) {
    auto& page = pages_[i];
    target_model_index += page.size();

    // Skip the item view to be moved in the page if found.
    // Decrement |target_model_index| if |moved_view| is in this page because it
    // is represented by a placeholder.
    auto it =
        std::find_if(page.begin(), page.end(), [&](AppListItemView* item_view) {
          return item_view->item() == moved_item;
        });

    if (it != page.end())
      --target_model_index;
  }

  // If the target visual index is in the same page, do not skip the item view
  // because the following item views will fill the gap in the page.
  target_model_index += index.slot;
  return target_model_index;
}

int PagedViewStructure::GetTargetItemListIndexForMove(
    AppListItem* moved_item,
    const GridIndex& index) const {
  if (mode_ == Mode::kFullPages)
    return GetModelIndexFromIndex(index);

  if (mode_ == Mode::kSinglePage) {
    DCHECK_EQ(index.page, 0);
    GridIndex current_index(0, 0);
    size_t current_item_index = 0;

    // Skip the leading "page break" items.
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
      if (moved_item && moved_item == item_list->item_at(current_item_index) &&
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
  DCHECK_EQ(current_index, index);
  return current_item_index - offset;
}

bool PagedViewStructure::IsValidReorderTargetIndex(
    const GridIndex& index) const {
  if (apps_grid_view_->IsValidIndex(index))
    return true;

  // The user can drag an item view to another page's end. Also covers the case
  // where a dragged folder item is being reparented to the last target index of
  // the root level grid.
  if ((index.page < total_pages() ||
       (index.page == total_pages() && mode_ == Mode::kPartialPages)) &&
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

int PagedViewStructure::CalculateTargetSlot(const Page& page) const {
  size_t target_slot = page.size();
  if (base::Contains(page, apps_grid_view_->drag_view()))
    --target_slot;
  return static_cast<int>(target_slot);
}

void PagedViewStructure::Sanitize() {
  if (sanitize_locks_ == 0)
    ClearOverflow();

  if (!empty_pages_allowed_ && sanitize_locks_ == 0)
    ClearEmptyPages();
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
