// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <algorithm>

#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/check.h"

namespace ash {

HoldingSpaceModel::HoldingSpaceModel() = default;

HoldingSpaceModel::~HoldingSpaceModel() = default;

void HoldingSpaceModel::AddItem(std::unique_ptr<HoldingSpaceItem> item) {
  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  items.push_back(std::move(item));
  AddItems(std::move(items));
}

void HoldingSpaceModel::AddItems(
    std::vector<std::unique_ptr<HoldingSpaceItem>> items) {
  DCHECK(!items.empty());
  std::vector<const HoldingSpaceItem*> item_ptrs;
  for (std::unique_ptr<HoldingSpaceItem>& item : items) {
    DCHECK(!GetItem(item->id()));

    if (item->IsFinalized())
      ++finalized_item_counts_by_type_[item->type()];

    item_ptrs.push_back(item.get());
    items_.push_back(std::move(item));
  }
  for (auto& observer : observers_)
    observer.OnHoldingSpaceItemsAdded(item_ptrs);
}

void HoldingSpaceModel::RemoveItem(const std::string& id) {
  RemoveItems({id});
}

void HoldingSpaceModel::RemoveItems(const std::set<std::string>& item_ids) {
  RemoveIf(base::BindRepeating(
      [](const std::set<std::string>& item_ids, const HoldingSpaceItem* item) {
        return base::Contains(item_ids, item->id());
      },
      std::cref(item_ids)));
}

void HoldingSpaceModel::FinalizeOrRemoveItem(const std::string& id,
                                             const GURL& file_system_url) {
  if (file_system_url.is_empty()) {
    RemoveItem(id);
    return;
  }

  auto item_it = std::find_if(
      items_.begin(), items_.end(),
      [&id](const std::unique_ptr<HoldingSpaceItem>& item) -> bool {
        return id == item->id();
      });
  DCHECK(item_it != items_.end());

  HoldingSpaceItem* item = item_it->get();
  DCHECK(!item->IsFinalized());

  item->Finalize(file_system_url);
  ++finalized_item_counts_by_type_[item->type()];

  for (auto& observer : observers_)
    observer.OnHoldingSpaceItemFinalized(item);
}

void HoldingSpaceModel::UpdateBackingFileForItem(
    const std::string& id,
    const base::FilePath& file_path,
    const GURL& file_system_url) {
  auto item_it = std::find_if(
      items_.begin(), items_.end(),
      [&id](const std::unique_ptr<HoldingSpaceItem>& item) -> bool {
        return item->id() == id;
      });
  DCHECK(item_it != items_.end());

  HoldingSpaceItem* item = item_it->get();
  DCHECK(item->IsFinalized());

  item->UpdateBackingFile(file_path, file_system_url);

  for (auto& observer : observers_)
    observer.OnHoldingSpaceItemUpdated(item);
}

void HoldingSpaceModel::RemoveIf(Predicate predicate) {
  // Keep removed items around until `observers_` have been notified of removal.
  std::vector<std::unique_ptr<HoldingSpaceItem>> items;
  std::vector<const HoldingSpaceItem*> item_ptrs;

  for (int i = items_.size() - 1; i >= 0; --i) {
    std::unique_ptr<HoldingSpaceItem>& item = items_.at(i);
    if (predicate.Run(item.get())) {
      item_ptrs.push_back(item.get());
      items.push_back(std::move(item));
      items_.erase(items_.begin() + i);

      if (item_ptrs.back()->IsFinalized())
        --finalized_item_counts_by_type_[item_ptrs.back()->type()];
    }
  }

  DCHECK_EQ(items.size(), item_ptrs.size());

  if (!items.empty()) {
    for (auto& observer : observers_)
      observer.OnHoldingSpaceItemsRemoved(item_ptrs);
  }
}

void HoldingSpaceModel::InvalidateItemImageIf(Predicate predicate) {
  for (auto& item : items_) {
    if (predicate.Run(item.get()))
      item->InvalidateImage();
  }
}

void HoldingSpaceModel::RemoveAll() {
  // Clear the item list, but keep the items around until the observers have
  // been notified of the item removal.
  ItemList items;
  items.swap(items_);

  finalized_item_counts_by_type_.clear();

  std::vector<const HoldingSpaceItem*> item_ptrs;
  for (auto& item : items)
    item_ptrs.push_back(item.get());

  for (auto& observer : observers_)
    observer.OnHoldingSpaceItemsRemoved(item_ptrs);
}

const HoldingSpaceItem* HoldingSpaceModel::GetItem(
    const std::string& id) const {
  auto item_it = std::find_if(
      items_.begin(), items_.end(),
      [&id](const std::unique_ptr<HoldingSpaceItem>& item) -> bool {
        return item->id() == id;
      });

  if (item_it == items_.end())
    return nullptr;
  return item_it->get();
}

const HoldingSpaceItem* HoldingSpaceModel::GetItem(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) const {
  auto item_it = std::find_if(
      items_.begin(), items_.end(),
      [&type, &file_path](const std::unique_ptr<HoldingSpaceItem>& item) {
        return item->type() == type && item->file_path() == file_path;
      });

  if (item_it == items_.end())
    return nullptr;
  return item_it->get();
}

bool HoldingSpaceModel::ContainsItem(HoldingSpaceItem::Type type,
                                     const base::FilePath& file_path) const {
  return GetItem(type, file_path) != nullptr;
}

bool HoldingSpaceModel::ContainsFinalizedItemOfType(
    HoldingSpaceItem::Type type) const {
  auto it = finalized_item_counts_by_type_.find(type);
  return it != finalized_item_counts_by_type_.end() && it->second > 0u;
}

void HoldingSpaceModel::AddObserver(HoldingSpaceModelObserver* observer) {
  observers_.AddObserver(observer);
}

void HoldingSpaceModel::RemoveObserver(HoldingSpaceModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
