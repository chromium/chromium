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
  uint32_t updated_fields = 0u;

  // Cache computed fields.
  const std::u16string accessible_name = item_->GetAccessibleName();

  // Update accessible name.
  if (accessible_name_) {
    if (item_->SetAccessibleName(accessible_name_.value())) {
      updated_fields |=
          HoldingSpaceModelObserver::UpdatedField::kAccessibleName;
    }
  }

  // Update backing file.
  if (file_path_ && file_system_url_) {
    if (item_->SetBackingFile(file_path_.value(), file_system_url_.value()))
      updated_fields |= HoldingSpaceModelObserver::UpdatedField::kBackingFile;
  }

  // Update pause.
  if (paused_) {
    if (item_->SetPaused(paused_.value()))
      updated_fields |= HoldingSpaceModelObserver::UpdatedField::kPaused;
  }

  // Update progress.
  if (progress_) {
    if (item_->SetProgress(progress_.value()))
      updated_fields |= HoldingSpaceModelObserver::UpdatedField::kProgress;
  }

  // Update secondary text.
  if (secondary_text_) {
    if (item_->SetSecondaryText(secondary_text_.value()))
      updated_fields |= HoldingSpaceModelObserver::UpdatedField::kSecondaryText;
  }

  // Update secondary text color.
  if (secondary_text_color_) {
    if (item_->SetSecondaryTextColor(secondary_text_color_.value())) {
      updated_fields |=
          HoldingSpaceModelObserver::UpdatedField::kSecondaryTextColor;
    }
  }

  // Update text.
  if (text_) {
    if (item_->SetText(text_.value()))
      updated_fields |= HoldingSpaceModelObserver::UpdatedField::kText;
  }

  // Invalidate image if necessary. Note that this does not trigger an observer
  // event as the image itself can be subscribed to independently for updates.
  if (invalidate_image_)
    item_->InvalidateImage();

  // Calculate changes to computed fields.
  if (accessible_name != item_->GetAccessibleName())
    updated_fields |= HoldingSpaceModelObserver::UpdatedField::kAccessibleName;

  // Notify observers if and only if an update occurred.
  if (updated_fields != 0u) {
    for (auto& observer : model_->observers_)
      observer.OnHoldingSpaceItemUpdated(item_, updated_fields);
  }
}

HoldingSpaceModel::ScopedItemUpdate&
HoldingSpaceModel::ScopedItemUpdate::SetAccessibleName(
    const absl::optional<std::u16string>& accessible_name) {
  accessible_name_ = accessible_name;
  return *this;
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
HoldingSpaceModel::ScopedItemUpdate::SetInvalidateImage(bool invalidate_image) {
  invalidate_image_ = invalidate_image;
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
HoldingSpaceModel::ScopedItemUpdate::SetSecondaryTextColor(
    const absl::optional<cros_styles::ColorName>& secondary_text_color) {
  secondary_text_color_ = secondary_text_color;
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
