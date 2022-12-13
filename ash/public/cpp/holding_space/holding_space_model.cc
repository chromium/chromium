// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/holding_space/holding_space_model.h"

#include <numeric>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "ash/public/cpp/holding_space/holding_space_section.h"
#include "ash/public/cpp/holding_space/holding_space_util.h"
#include "base/bind.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/memory/ptr_util.h"
#include "base/ranges/algorithm.h"

namespace ash {

// HoldingSpaceModel::ScopedItemUpdate -----------------------------------------

HoldingSpaceModel::ScopedItemUpdate::~ScopedItemUpdate() {
  uint32_t updated_fields = 0u;

  // Cache computed fields.
  const std::u16string accessible_name = item_->GetAccessibleName();
  const std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands =
      item_->in_progress_commands();

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

  // Update in-progress commands.
  if (in_progress_commands_)
    item_->SetInProgressCommands(std::move(*in_progress_commands_));

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
  if (secondary_text_color_id_) {
    if (item_->SetSecondaryTextColorId(secondary_text_color_id_.value())) {
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
  if (in_progress_commands != item_->in_progress_commands()) {
    updated_fields |=
        HoldingSpaceModelObserver::UpdatedField::kInProgressCommands;
  }

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
HoldingSpaceModel::ScopedItemUpdate::SetInProgressCommands(
    std::vector<HoldingSpaceItem::InProgressCommand> in_progress_commands) {
  DCHECK(base::ranges::all_of(
      in_progress_commands,
      [](const HoldingSpaceItem::InProgressCommand& in_progress_command) {
        return holding_space_util::IsInProgressCommand(
            in_progress_command.command_id);
      }));
  in_progress_commands_ = std::move(in_progress_commands);
  return *this;
}

HoldingSpaceModel::ScopedItemUpdate&
HoldingSpaceModel::ScopedItemUpdate::SetInvalidateImage(bool invalidate_image) {
  invalidate_image_ = invalidate_image;
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
HoldingSpaceModel::ScopedItemUpdate::SetSecondaryTextColorId(
    const absl::optional<ui::ColorId>& secondary_text_color_id) {
  secondary_text_color_id_ = secondary_text_color_id;
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

  if (!features::IsHoldingSpacePredictabilityEnabled())
    return;

  // When the predictability feature flag is enabled, holding space items do
  // not automatically expire. Instead, a maximum item count for each section
  // is enforced such that adding new items may result in removing the oldest
  // items from the same section.
  RemoveIf(base::BindRepeating(
      [](std::map<HoldingSpaceSectionId, size_t>& item_counts_per_section_id,
         const HoldingSpaceItem* item) {
        const auto* section = GetHoldingSpaceSection(item->type());
        const auto item_count = ++item_counts_per_section_id[section->id];
        return section->max_item_count && item_count > *section->max_item_count;
      },
      base::OwnedRef(std::map<HoldingSpaceSectionId, size_t>())));
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

std::unique_ptr<HoldingSpaceItem> HoldingSpaceModel::TakeItem(
    const std::string& id) {
  auto items = RemoveIf(base::BindRepeating(
      [](const std::string& id, const HoldingSpaceItem* item) {
        return item->id() == id;
      },
      std::cref(id)));

  if (items.empty())
    return nullptr;

  DCHECK_EQ(items.size(), 1u);
  return std::move(items[0]);
}

void HoldingSpaceModel::InitializeOrRemoveItem(const std::string& id,
                                               const GURL& file_system_url) {
  if (file_system_url.is_empty()) {
    RemoveItem(id);
    return;
  }

  auto item_it = base::ranges::find(items_, id, &HoldingSpaceItem::id);
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
  auto item_it = base::ranges::find(items_, id, &HoldingSpaceItem::id);
  DCHECK(item_it != items_.end());
  return base::WrapUnique(new ScopedItemUpdate(this, item_it->get()));
}

std::vector<std::unique_ptr<HoldingSpaceItem>> HoldingSpaceModel::RemoveIf(
    Predicate predicate) {
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

  return items;
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
  auto item_it = base::ranges::find(items_, id, &HoldingSpaceItem::id);

  if (item_it == items_.end())
    return nullptr;
  return item_it->get();
}

const HoldingSpaceItem* HoldingSpaceModel::GetItem(
    HoldingSpaceItem::Type type,
    const base::FilePath& file_path) const {
  auto item_it = base::ranges::find_if(
      items_,
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
