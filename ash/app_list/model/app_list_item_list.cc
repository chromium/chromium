// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item_list.h"

#include <utility>

#include "ash/app_list/model/app_list_item.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"

namespace ash {

AppListItemList::AppListItemList() = default;

AppListItemList::~AppListItemList() = default;

void AppListItemList::AddObserver(AppListItemListObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItemList::RemoveObserver(AppListItemListObserver* observer) {
  DCHECK(observers_.HasObserver(observer));
  observers_.RemoveObserver(observer);
}

AppListItem* AppListItemList::FindItem(const std::string& id) {
  for (const auto& item : app_list_items_) {
    if (item->id() == id)
      return item.get();
  }
  return nullptr;
}

// TODO(https://crbug.com/883971): Make it return iterator to avoid unnecessary
// check in this code.
bool AppListItemList::FindItemIndex(const std::string& id, size_t* index) {
  for (size_t i = 0; i < app_list_items_.size(); ++i) {
    if (app_list_items_[i]->id() == id) {
      *index = i;
      return true;
    }
  }
  return false;
}

void AppListItemList::MoveItem(size_t from_index, size_t to_index) {
  DCHECK_LT(from_index, item_count());
  DCHECK_LT(to_index, item_count());
  if (from_index == to_index || item_count() == 1)
    return;

  auto target_item = std::move(app_list_items_[from_index]);
  DVLOG(2) << "MoveItem: " << from_index << " -> " << to_index << " ["
           << target_item->position().ToDebugString() << "]";
  // Remove the target item
  app_list_items_.erase(app_list_items_.begin() + from_index);

  // Update the position
  AppListItem* prev = to_index > 0 ? item_at(to_index - 1) : nullptr;
  AppListItem* next = to_index < item_count() ? item_at(to_index) : nullptr;
  CHECK_NE(prev, next);
  syncer::StringOrdinal new_position;
  if (!prev) {
    new_position = next->position().CreateBefore();
  } else if (!next) {
    new_position = prev->position().CreateAfter();
  } else {
    // It is possible that items were added with the same ordinal. To
    // successfully move the item we need to fix this. We do not try to fix this
    // when an item is added in order to avoid possible edge cases with sync.
    if (prev->position().Equals(next->position()))
      FixItemPosition(to_index);
    new_position = prev->position().CreateBetween(next->position());
  }
  target_item->set_position(new_position);

  DVLOG(2) << "Move: "
           << " Prev: " << (prev ? prev->position().ToDebugString() : "(none)")
           << " Next: " << (next ? next->position().ToDebugString() : "(none)")
           << " -> " << new_position.ToDebugString();

  // Insert the item and notify observers.
  app_list_items_.insert(app_list_items_.begin() + to_index,
                         std::move(target_item));
  AppListItem* item = item_at(to_index);
  for (auto& observer : observers_)
    observer.OnListItemMoved(from_index, to_index, item);
}

void AppListItemList::SetItemPosition(AppListItem* item,
                                      syncer::StringOrdinal new_position) {
  DCHECK(item);
  size_t from_index;
  if (!FindItemIndex(item->id(), &from_index)) {
    LOG(ERROR) << "SetItemPosition: Not in list: " << item->id().substr(0, 8);
    return;
  }
  DCHECK(item_at(from_index) == item);
  if (!new_position.IsValid()) {
    size_t last_index = app_list_items_.size() - 1;
    if (from_index == last_index)
      return;  // Already last item, do nothing.
    new_position = item_at(last_index)->position().CreateAfter();
  }
  // First check if the order would remain the same, in which case just update
  // the position.
  size_t to_index = GetItemSortOrderIndex(new_position, item->id());
  if (to_index == from_index) {
    DVLOG(2) << "SetItemPosition: No change: " << item->id().substr(0, 8);
    item->set_position(new_position);
    return;
  }
  // Remove the item and get the updated to index.
  auto target_item = std::move(app_list_items_[from_index]);
  app_list_items_.erase(app_list_items_.begin() + from_index);
  to_index = GetItemSortOrderIndex(new_position, target_item->id());
  DVLOG(2) << "SetItemPosition: " << target_item->id().substr(0, 8) << " -> "
           << new_position.ToDebugString() << " From: " << from_index
           << " To: " << to_index;
  target_item->set_position(new_position);
  app_list_items_.insert(app_list_items_.begin() + to_index,
                         std::move(target_item));
  for (auto& observer : observers_)
    observer.OnListItemMoved(from_index, to_index, item);
}

AppListItem* AppListItemList::AddPageBreakItemAfter(
    const AppListItem* previous_item) {
  size_t previous_index;
  CHECK(FindItemIndex(previous_item->id(), &previous_index));
  CHECK(!previous_item->IsInFolder());

  const size_t next_index = previous_index + 1;
  const AppListItem* next_item =
      next_index < item_count() ? item_at(next_index) : nullptr;
  syncer::StringOrdinal position;
  if (!next_item) {
    position = previous_item->position().CreateAfter();
  } else {
    // It is possible that items were added with the same ordinal. To
    // successfully add the page break item we need to fix this. We do not try
    // to fix this when an item is added in order to avoid possible edge cases
    // with sync.
    if (previous_item->position().Equals(next_item->position()))
      FixItemPosition(next_index);
    position = previous_item->position().CreateBetween(next_item->position());
  }

  auto page_break_item = std::make_unique<AppListItem>(base::GenerateGUID());
  page_break_item->set_position(position);
  page_break_item->set_is_page_break(true);

  AppListItem* item = page_break_item.get();
  size_t index = GetItemSortOrderIndex(item->position(), item->id());
  app_list_items_.insert(app_list_items_.begin() + index,
                         std::move(page_break_item));
  return item;
}

void AppListItemList::HighlightItemInstalledFromUI(const std::string& id) {
  // Items within folders are not highlighted (apps are never installed to a
  // folder initially). So just search the top-level list.
  size_t index;
  if (FindItemIndex(highlighted_id_, &index)) {
    for (auto& observer : observers_)
      observer.OnAppListItemHighlight(index, false);
  }
  highlighted_id_ = id;
  if (!FindItemIndex(highlighted_id_, &index)) {
    // If the item isin't in the app list yet, it will be highlighted later, in
    // AddItem().
    return;
  }

  for (auto& observer : observers_)
    observer.OnAppListItemHighlight(index, true);
}

// AppListItemList private

syncer::StringOrdinal AppListItemList::CreatePositionBefore(
    const syncer::StringOrdinal& position) {
  if (app_list_items_.empty())
    return syncer::StringOrdinal::CreateInitialOrdinal();

  size_t nitems = app_list_items_.size();
  size_t index;
  if (!position.IsValid()) {
    index = nitems;
  } else {
    for (index = 0; index < nitems; ++index) {
      if (!item_at(index)->position().LessThan(position))
        break;
    }
  }
  if (index == 0)
    return item_at(0)->position().CreateBefore();
  if (index == nitems)
    return item_at(nitems - 1)->position().CreateAfter();
  return item_at(index - 1)->position().CreateBetween(
      item_at(index)->position());
}

AppListItem* AppListItemList::AddItem(std::unique_ptr<AppListItem> item_ptr) {
  AppListItem* item = item_ptr.get();
  CHECK(std::find_if(app_list_items_.cbegin(), app_list_items_.cend(),
                     [item](const std::unique_ptr<AppListItem>& item_p) {
                       return item_p.get() == item;
                     }) == app_list_items_.cend());
  EnsureValidItemPosition(item);
  size_t index = GetItemSortOrderIndex(item->position(), item->id());
  app_list_items_.insert(app_list_items_.begin() + index, std::move(item_ptr));
  for (auto& observer : observers_)
    observer.OnListItemAdded(index, item);

  if (item->id() == highlighted_id_) {
    // Item not present when highlight requested, so highlight it now.
    for (auto& observer : observers_)
      observer.OnAppListItemHighlight(index, true);
  }
  return item;
}

void AppListItemList::DeleteItem(const std::string& id) {
  std::unique_ptr<AppListItem> item = RemoveItem(id);
  // |item| will be deleted on destruction.
}

std::unique_ptr<AppListItem> AppListItemList::RemoveItem(
    const std::string& id) {
  size_t index;
  if (!FindItemIndex(id, &index))
    LOG(FATAL) << "RemoveItem: Not found: " << id;
  return RemoveItemAt(index);
}

std::unique_ptr<AppListItem> AppListItemList::RemoveItemAt(size_t index) {
  CHECK_LT(index, item_count());
  auto item = std::move(app_list_items_[index]);
  app_list_items_.erase(app_list_items_.begin() + index);
  for (auto& observer : observers_)
    observer.OnListItemRemoved(index, item.get());
  return item;
}

void AppListItemList::DeleteItemAt(size_t index) {
  std::unique_ptr<AppListItem> item = RemoveItemAt(index);
  // |item| will be deleted on destruction.
}

void AppListItemList::EnsureValidItemPosition(AppListItem* item) {
  syncer::StringOrdinal position = item->position();
  if (position.IsValid())
    return;
  size_t nitems = app_list_items_.size();
  if (nitems == 0) {
    position = syncer::StringOrdinal::CreateInitialOrdinal();
  } else {
    position = item_at(nitems - 1)->position().CreateAfter();
  }
  item->set_position(position);
}

size_t AppListItemList::GetItemSortOrderIndex(
    const syncer::StringOrdinal& position,
    const std::string& id) {
  DCHECK(position.IsValid());
  for (size_t index = 0; index < app_list_items_.size(); ++index) {
    if (position.LessThan(item_at(index)->position()) ||
        (position.Equals(item_at(index)->position()) &&
         (id < item_at(index)->id()))) {
      return index;
    }
  }
  return app_list_items_.size();
}

void AppListItemList::FixItemPosition(size_t index) {
  DVLOG(1) << "FixItemPosition: " << index;
  size_t nitems = item_count();
  DCHECK_LT(index, nitems);
  DCHECK_GT(index, 0u);
  // Update the position of |index| and any necessary subsequent items.
  // First, find the next item that has a different position.
  AppListItem* prev = item_at(index - 1);
  size_t last_index = index + 1;
  for (; last_index < nitems; ++last_index) {
    if (!item_at(last_index)->position().Equals(prev->position()))
      break;
  }
  AppListItem* last = last_index < nitems ? item_at(last_index) : nullptr;
  for (size_t i = index; i < last_index; ++i) {
    AppListItem* cur = item_at(i);
    if (last)
      cur->set_position(prev->position().CreateBetween(last->position()));
    else
      cur->set_position(prev->position().CreateAfter());
    prev = cur;
  }
  AppListItem* item = item_at(index);
  for (auto& observer : observers_)
    observer.OnListItemMoved(index, index, item);
}

}  // namespace ash
