// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item_list.h"

#include <utility>

#include "ash/app_list/model/app_list_item.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"

namespace ash {

AppListItemList::AppListItemList(AppListModelDelegate* app_list_model_delegate)
    : app_list_model_delegate_(app_list_model_delegate) {}

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

// TODO(crbug.com/40593633): Make it return iterator to avoid unnecessary
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
  // TODO(https://crbug.com/1257779): this function triggers updates in item
  // positions from ash so this function should be moved to
  // `AppListModelDelegate`.

  DCHECK_LT(from_index, item_count());
  // Speculative fix for crash, possibly due to single-item folders
  // (see https://crbug.com/937431).
  // A folder could have single item due to the following reasons:
  // (1) The folder is allowed to contain only one item. Or
  // (2) The app list sync is in progress. For example, when the app list is
  // syncing two apps under the same folder, one app could be added to the
  // folder before the other by a noticeable time interval. As a result, the
  // folder contains one item temporarily.
  if (item_count() <= 1)
    return;

  // Speculative fix for crash, possibly due |to_index| == item_count().
  // Make |to_index| point to the last item. https://crbug.com/1166011
  if (to_index >= item_count()) {
    DCHECK_GT(item_count(), 1u);
    to_index = item_count() - 1;
  }
  if (from_index == to_index)
    return;

  AppListItem* target_item = app_list_items_[from_index].get();
  DVLOG(2) << "MoveItem: " << from_index << " -> " << to_index << " ["
           << target_item->position().ToDebugString() << "]";

  if (from_index < to_index) {
    // Calculate as if the item at `from_index` is removed from the list.
    ++to_index;
  }

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

  DVLOG(2) << "Move: "
           << " Prev: " << (prev ? prev->position().ToDebugString() : "(none)")
           << " Next: " << (next ? next->position().ToDebugString() : "(none)")
           << " -> " << new_position.ToDebugString();

  // Update app list items through a delegate so that the browser side always
  // updates app list items before the ash side.
  app_list_model_delegate_->RequestPositionUpdate(
      target_item->id(), new_position, RequestPositionUpdateReason::kMoveItem);
}

bool AppListItemList::SetItemPosition(AppListItem* item,
                                      syncer::StringOrdinal new_position) {
  DCHECK(item);
  size_t from_index;
  if (!FindItemIndex(item->id(), &from_index)) {
    LOG(ERROR) << "SetItemPosition: Not in list: " << item->id().substr(0, 8);
    return false;
  }
  DCHECK(item_at(from_index) == item);
  if (!new_position.IsValid()) {
    size_t last_index = app_list_items_.size() - 1;
    if (from_index == last_index)
      return false;  // Already last item, do nothing.
    new_position = item_at(last_index)->position().CreateAfter();
  }
  // First check if the order would remain the same, in which case just update
  // the position.
  size_t to_index = GetItemSortOrderIndex(new_position, item->id());
  if (to_index == from_index) {
    DVLOG(2) << "SetItemPosition: No change: " << item->id().substr(0, 8);
    item->set_position(new_position);
    return false;
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
  return true;
}

std::string AppListItemList::ToString() {
  std::string out;
  for (size_t i = 0; i < app_list_items_.size(); ++i) {
    out.append(base::NumberToString(i));
    out.append(": ");
    out.append(app_list_items_[i]->id());
    out.append("\n");
  }
  return out;
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
  CHECK(!base::Contains(app_list_items_, item,
                        &std::unique_ptr<AppListItem>::get));
  EnsureValidItemPosition(item);
  size_t index = GetItemSortOrderIndex(item->position(), item->id());
  app_list_items_.insert(app_list_items_.begin() + index, std::move(item_ptr));
  for (auto& observer : observers_)
    observer.OnListItemAdded(index, item);

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
  // TODO(https://crbug.com/1257779): this function triggers updates in item
  // positions from ash so this function should be moved to
  // `AppListModelDelegate`.

  DVLOG(1) << "FixItemPosition: " << index;
  size_t nitems = item_count();
  DCHECK_LT(index, nitems);
  DCHECK_GT(index, 0u);
  // Update the position of |index| and any necessary subsequent items.
  // First, find the next item that has a different position.
  const syncer::StringOrdinal duplicate_position =
      item_at(index - 1)->position();
  size_t last_index = index + 1;
  for (; last_index < nitems; ++last_index) {
    if (!item_at(last_index)->position().Equals(duplicate_position))
      break;
  }

  // Store the pairs of ids and new positions before requesting to update
  // positions. Because position update may result in item list reorder.
  std::vector<std::pair<std::string, syncer::StringOrdinal>> id_position_pairs;

  syncer::StringOrdinal new_position = duplicate_position;
  AppListItem* last = last_index < nitems ? item_at(last_index) : nullptr;
  for (size_t i = index; i < last_index; ++i) {
    new_position = last ? new_position.CreateBetween(last->position())
                        : new_position.CreateAfter();
    id_position_pairs.emplace_back(item_at(i)->id(), new_position);
  }

  for (const auto& pair : id_position_pairs) {
    app_list_model_delegate_->RequestPositionUpdate(
        pair.first, pair.second, RequestPositionUpdateReason::kFixItem);
  }
}

}  // namespace ash
