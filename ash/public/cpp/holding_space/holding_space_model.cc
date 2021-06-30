// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <algorithm>

#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"

namespace ash {

// HoldingSpaceModel::ScopedItemUpdate -----------------------------------------

HoldingSpaceModel::ScopedItemUpdate::~ScopedItemUpdate() {
  bool did_update = false;

  // Update backing file.
  if (file_path_ && file_system_url_) {
    did_update |=
        item_->SetBackingFile(file_path_.value(), file_system_url_.value());
  }

  // Update pause.
  if (paused_)
    did_update |= item_->SetPaused(paused_.value());

  // Update progress.
  if (progress_)
    did_update |= item_->SetProgress(progress_.value());

  // Update secondary text.
  if (secondary_text_)
    did_update |= item_->SetSecondaryText(secondary_text_.value());

  // Update text.
  if (text_)
    did_update |= item_->SetText(text_.value());

  // Notify observers if and only if an update occurred.
  if (did_update) {
    for (auto& observer : model_->observers_)
      observer.OnHoldingSpaceItemUpdated(item_);
  }
}

HoldingSpaceModel::ScopedItemUpdate&
HoldingSpaceModel::ScopedItemUpdate::SetBackingFile(
    const base::FilePath& file_path,
    const GURL& file_system_url) {
  file_path_ = file_path;
  file_system_url_ = file_system_url;
  return *this;
}

HoldingSpaceModel::ScopedItemUpdate&
HoldingSpaceModel::ScopedItemUpdate::SetPaused(bool paused) {
  paused_ = paused;
  return *this;
}

HoldingSpaceModel::ScopedItemUpdate&
HoldingSpaceModel::ScopedItemUpdate::SetProgress(
    const HoldingSpaceProgress& progress) {
  progress_ = progress;
  return *this;
}

HoldingSpaceModel::ScopedItemUpdate&
HoldingSpaceModel::ScopedItemUpdate::SetSecondaryText(
    const absl::optional<std::u16string>& secondary_text) {
  secondary_text_ = secondary_text;
  return *this;
}

HoldingSpaceModel::ScopedItemUpdate&
HoldingSpaceModel::ScopedItemUpdate::SetText(
    const absl::optional<std::u16string>& text) {
  text_ = text;
  return *this;
}

HoldingSpaceModel::ScopedItemUpdate::ScopedItemUpdate(HoldingSpaceModel* model,
                                                      HoldingSpaceItem* item)
    : model_(model), item_(item) {
  DCHECK(model_);
  DCHECK(item_);
}

// HoldingSpaceModel -----------------------------------------------------------

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

    if (item->IsInitialized())
      ++initialized_item_counts_by_type_[item->type()];

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

void HoldingSpaceModel::InitializeOrRemoveItem(const std::string& id,
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
  DCHECK(!item->IsInitialized());

  item->Initialize(file_system_url);
  ++initialized_item_counts_by_type_[item->type()];

  for (auto& observer : observers_)
    observer.OnHoldingSpaceItemInitialized(item);
}

std::unique_ptr<HoldingSpaceModel::ScopedItemUpdate>
HoldingSpaceModel::UpdateItem(const std::string& id) {
  auto item_it =
      std::find_if(items_.begin(), items_.end(),
                   [&id](const std::unique_ptr<HoldingSpaceItem>& item) {
                     return item->id() == id;
                   });
  DCHECK(item_it != items_.end());
  return base::WrapUnique(new ScopedItemUpdate(this, item_it->get()));
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

      if (item_ptrs.back()->IsInitialized())
        --initialized_item_counts_by_type_[item_ptrs.back()->type()];
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

  initialized_item_counts_by_type_.clear();

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

bool HoldingSpaceModel::ContainsInitializedItemOfType(
    HoldingSpaceItem::Type type) const {
  auto it = initialized_item_counts_by_type_.find(type);
  return it != initialized_item_counts_by_type_.end() && it->second > 0u;
}

void HoldingSpaceModel::AddObserver(HoldingSpaceModelObserver* observer) {
  observers_.AddObserver(observer);
}

void HoldingSpaceModel::RemoveObserver(HoldingSpaceModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace ash
