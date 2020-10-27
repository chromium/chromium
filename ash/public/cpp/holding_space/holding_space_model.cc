// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <algorithm>

#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/check.h"

namespace ash {

HoldingSpaceModel::HoldingSpaceModel() = default;
HoldingSpaceModel::~HoldingSpaceModel() = default;

void HoldingSpaceModel::AddItem(std::unique_ptr<HoldingSpaceItem> item) {
  DCHECK(!GetItem(item->id()));
  const HoldingSpaceItem* item_ptr = item.get();
  items_.push_back(std::move(item));

  for (auto& observer : observers_)
    observer.OnHoldingSpaceItemAdded(item_ptr);
}

void HoldingSpaceModel::RemoveItem(const std::string& id) {
  auto item_it = std::find_if(
      items_.begin(), items_.end(),
      [&id](const std::unique_ptr<HoldingSpaceItem>& item) -> bool {
        return item->id() == id;
      });

  DCHECK(item_it != items_.end());

  // Keep the item around at least until the observers have been notified of the
  // item removal.
  std::unique_ptr<HoldingSpaceItem> item = std::move(*item_it);

  items_.erase(item_it);

  for (auto& observer : observers_)
    observer.OnHoldingSpaceItemRemoved(item.get());
}

void HoldingSpaceModel::RemoveIf(Predicate predicate) {
  for (int i = items_.size() - 1; i >= 0; --i) {
    const HoldingSpaceItem* item = items_.at(i).get();
    if (predicate.Run(item))
      RemoveItem(item->id());
  }
}

void HoldingSpaceModel::RemoveAll() {
  // Clear the item list, but keep the items around until the observers have
  // been notified of the item removal.
  ItemList items;
  items.swap(items_);

  for (auto& item : items) {
    for (auto& observer : observers_)
      observer.OnHoldingSpaceItemRemoved(item.get());
  }
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

void HoldingSpaceModel::AddObserver(HoldingSpaceModelObserver* observer) {
  observers_.AddObserver(observer);
}

void HoldingSpaceModel::RemoveObserver(HoldingSpaceModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
