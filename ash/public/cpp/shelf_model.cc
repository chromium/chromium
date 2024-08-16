// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_model.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/strings/string_util.h"

namespace ash {

namespace {

static ShelfModel* g_shelf_model = nullptr;

int ShelfItemTypeToWeight(ShelfItemType type) {
  switch (type) {
    case TYPE_PINNED_APP:
    case TYPE_BROWSER_SHORTCUT:
      return 1;
    case TYPE_APP:
    case TYPE_UNPINNED_BROWSER_SHORTCUT:
      return 2;
    case TYPE_DIALOG:
      return 3;
    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set";
  }

  NOTREACHED() << "Invalid type " << type;
}

bool CompareByWeight(const ShelfItem& a, const ShelfItem& b) {
  return ShelfItemTypeToWeight(a.type) < ShelfItemTypeToWeight(b.type);
}

}  // namespace

ShelfModel* ShelfModel::Get() {
  DCHECK(g_shelf_model);
  return g_shelf_model;
}

void ShelfModel::SetInstance(ShelfModel* shelf_model) {
  g_shelf_model = shelf_model;
}

ShelfModel::ShelfModel() = default;

ShelfModel::~ShelfModel() = default;

void ShelfModel::AddAndPinAppWithFactoryConstructedDelegate(
    const std::string& app_id) {
  DCHECK_LT(ItemIndexByAppID(app_id), 0);

  std::unique_ptr<ShelfItemDelegate> delegate =
      shelf_item_factory_->CreateShelfItemDelegateForAppId(app_id);
  std::unique_ptr<ShelfItem> item = shelf_item_factory_->CreateShelfItemForApp(
      ash::ShelfID(app_id), STATUS_CLOSED, TYPE_PINNED_APP,
      /*title=*/std::u16string());

  Add(*item, std::move(delegate));
}

void ShelfModel::PinExistingItemWithID(const std::string& app_id) {
  const int index = ItemIndexByAppID(app_id);
  DCHECK_GE(index, 0);

  if (IsAppPinned(app_id))
    return;

  ShelfItem item = items_[index];
  DCHECK_EQ(item.type, TYPE_APP);
  DCHECK(!item.IsPinStateForced());
  item.type = TYPE_PINNED_APP;
  Set(index, item);
}

bool ShelfModel::IsAppPinned(const std::string& app_id) const {
  const int index = ItemIndexByID(ShelfID(app_id));
  if (index < 0)
    return false;
  return IsPinnedShelfItemType(items_[index].type);
}

bool ShelfModel::AllowedToSetAppPinState(const std::string& app_id,
                                         bool target_pin) const {
  if (IsAppPinned(app_id) == target_pin)
    return true;

  const ShelfID shelf_id(app_id);
  const int index = ItemIndexByID(shelf_id);

  if (index < 0) {
    // Allow to pin an app which is not open.
    return !shelf_id.IsNull() && target_pin;
  }

  const ShelfItem& item = items_[index];
  if (item.pinned_by_policy)
    return false;

  // Allow to unpin a pinned app or pin a running app.
  return (item.type == TYPE_PINNED_APP && !target_pin) ||
         (item.type == TYPE_APP && target_pin);
}

void ShelfModel::UnpinAppWithID(const std::string& app_id) {
  // If the app is already not pinned, do nothing and return.
  if (!IsAppPinned(app_id))
    return;

  // Remove the item if it is closed, or mark it as unpinned.
  const int index = ItemIndexByID(ShelfID(app_id));
  ShelfItem item = items_[index];
  DCHECK_EQ(item.type, TYPE_PINNED_APP);
  DCHECK(!item.pinned_by_policy);
  if (item.status == STATUS_CLOSED) {
    RemoveItemAt(index);
  } else {
    item.type = TYPE_APP;
    Set(index, item);
  }
}

void ShelfModel::DestroyItemDelegates() {
  // Some ShelfItemDelegates access this model in their destructors and hence
  // need early cleanup.
  id_to_item_delegate_map_.clear();
}

int ShelfModel::Add(const ShelfItem& item,
                    std::unique_ptr<ShelfItemDelegate> delegate) {
  return AddAt(items_.size(), item, std::move(delegate));
}

int ShelfModel::AddAt(int index,
                      const ShelfItem& item,
                      std::unique_ptr<ShelfItemDelegate> delegate) {
  // Update the delegate map immediately. We don't send a
  // ShelfItemDelegateChanged() call when adding items to the model.
  delegate->set_shelf_id(item.id);
  id_to_item_delegate_map_[item.id] = std::move(delegate);

  // Items should have unique non-empty ids to avoid undefined model behavior.
  DCHECK(!item.id.IsNull()) << " The id is null.";
  DCHECK_EQ(ItemIndexByID(item.id), -1) << " The id is not unique: " << item.id;
  index = ValidateInsertionIndex(item.type, index);
  items_.insert(items_.begin() + index, item);
  for (auto& observer : observers_)
    observer.ShelfItemAdded(index);

  return index;
}

void ShelfModel::RemoveItemAt(int index) {
  DCHECK(index >= 0 && index < item_count());
  ShelfItem old_item(items_[index]);
  items_.erase(items_.begin() + index);
  id_to_item_delegate_map_.erase(old_item.id);
  for (auto& observer : observers_)
    observer.ShelfItemRemoved(index, old_item);
}

std::unique_ptr<ShelfItemDelegate>
ShelfModel::RemoveItemAndTakeShelfItemDelegate(const ShelfID& shelf_id) {
  const int index = ItemIndexByID(shelf_id);
  if (index < 0)
    return nullptr;

  auto it = id_to_item_delegate_map_.find(shelf_id);
  std::unique_ptr<ShelfItemDelegate> item = std::move(it->second);
  RemoveItemAt(index);
  return item;
}

bool ShelfModel::CanSwap(int index, bool with_next) const {
  const int target_index = with_next ? index + 1 : index - 1;

  // Out of bounds issues, or trying to swap the first item with the previous
  // one, or the last item with the next one.
  if (index < 0 || target_index >= item_count() || target_index < 0)
    return false;

  const ShelfItem source_item = items()[index];
  const ShelfItem target_item = items()[target_index];
  // Trying to swap two items of different pin states.
  if (!SamePinState(source_item.type, target_item.type))
    return false;

  return true;
}

bool ShelfModel::Swap(int index, bool with_next) {
  if (!CanSwap(index, with_next))
    return false;

  const int target_index = with_next ? index + 1 : index - 1;
  Move(index, target_index);
  return true;
}

void ShelfModel::Move(int index, int target_index) {
  if (index == target_index)
    return;
  // TODO: this needs to enforce valid ranges.
  ShelfItem item(items_[index]);
  items_.erase(items_.begin() + index);
  items_.insert(items_.begin() + target_index, item);
  for (auto& observer : observers_)
    observer.ShelfItemMoved(index, target_index);
}

void ShelfModel::Set(int index, const ShelfItem& item) {
  if (index < 0 || index >= item_count()) {
    NOTREACHED();
  }

  int new_index = item.type == items_[index].type
                      ? index
                      : ValidateInsertionIndex(item.type, index);

  ShelfItem old_item(items_[index]);
  items_[index] = item;
  DCHECK(old_item.id == item.id);
  for (auto& observer : observers_)
    observer.ShelfItemChanged(index, old_item);

  // If the type changes confirm that the item is still in the right order.
  if (new_index != index) {
    // The move function works by removing one item and then inserting it at the
    // new location. However - by removing the item first the order will change
    // so that our target index needs to be corrected.
    // TODO(skuhne): Moving this into the Move function breaks lots of unit
    // tests. So several functions were already using this incorrectly.
    // That needs to be cleaned up.
    if (index < new_index)
      new_index--;

    Move(index, new_index);
  }
}

void ShelfModel::UpdateItemsForDeskChange(
    const std::vector<ItemDeskUpdate>& items_desk_updates) {
  for (const auto& item : items_desk_updates) {
    const int index = item.index;
    DCHECK(index >= 0 && index < item_count());
    items_[index].is_on_active_desk = item.is_on_active_desk;
  }

  for (auto& observer : observers_)
    observer.ShelfItemsUpdatedForDeskChange();
}

// TODO(manucornet): Add some simple unit tests for this method.
void ShelfModel::SetActiveShelfID(const ShelfID& shelf_id) {
  if (active_shelf_id_ == shelf_id)
    return;

  ShelfID old_active_id = active_shelf_id_;
  active_shelf_id_ = shelf_id;
  if (!old_active_id.IsNull())
    OnItemStatusChanged(old_active_id);
  if (!active_shelf_id_.IsNull())
    OnItemStatusChanged(active_shelf_id_);
}

void ShelfModel::OnItemStatusChanged(const ShelfID& id) {
  for (auto& observer : observers_)
    observer.ShelfItemStatusChanged(id);
}

void ShelfModel::OnItemRippedOff() {
  for (auto& observer : observers_)
    observer.ShelfItemRippedOff();
}

void ShelfModel::OnItemReturnedFromRipOff(int index) {
  for (auto& observer : observers_)
    observer.ShelfItemReturnedFromRipOff(index);
}

int ShelfModel::ItemIndexByID(const ShelfID& shelf_id) const {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].id == shelf_id)
      return static_cast<int>(i);
  }
  return -1;
}

int ShelfModel::GetItemIndexForType(ShelfItemType type) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].type == type)
      return i;
  }
  return -1;
}

const ShelfItem* ShelfModel::ItemByID(const ShelfID& shelf_id) const {
  int index = ItemIndexByID(shelf_id);
  return index >= 0 ? &items_[index] : nullptr;
}

int ShelfModel::ItemIndexByAppID(const std::string& app_id) const {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (!app_id.compare(items_[i].id.app_id))
      return i;
  }
  return -1;
}

int ShelfModel::FirstRunningAppIndex() const {
  ShelfItem weight_dummy;
  weight_dummy.type = TYPE_APP;
  return std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                          CompareByWeight) -
         items_.begin();
}

void ShelfModel::ReplaceShelfItemDelegate(
    const ShelfID& shelf_id,
    std::unique_ptr<ShelfItemDelegate> item_delegate) {
  DCHECK(item_delegate);
  // Create a copy of the id that can be safely accessed if |shelf_id| is backed
  // by a controller that will be deleted in the assignment below.
  const ShelfID safe_shelf_id = shelf_id;
  item_delegate->set_shelf_id(safe_shelf_id);
  // This assignment replaces any ShelfItemDelegate already registered for
  // |shelf_id|.
  std::unique_ptr<ShelfItemDelegate> old_item_delegate =
      std::move(id_to_item_delegate_map_[safe_shelf_id]);
  id_to_item_delegate_map_[safe_shelf_id] = std::move(item_delegate);
  for (auto& observer : observers_) {
    observer.ShelfItemDelegateChanged(
        safe_shelf_id, old_item_delegate.get(),
        id_to_item_delegate_map_[safe_shelf_id].get());
  }
}

ShelfItemDelegate* ShelfModel::GetShelfItemDelegate(
    const ShelfID& shelf_id) const {
  auto it = id_to_item_delegate_map_.find(shelf_id);
  if (it != id_to_item_delegate_map_.end())
    return it->second.get();
  return nullptr;
}

void ShelfModel::SetShelfItemFactory(ShelfModel::ShelfItemFactory* factory) {
  shelf_item_factory_ = factory;
}

AppWindowShelfItemController* ShelfModel::GetAppWindowShelfItemController(
    const ShelfID& shelf_id) {
  ShelfItemDelegate* item_delegate = GetShelfItemDelegate(shelf_id);
  return item_delegate ? item_delegate->AsAppWindowShelfItemController()
                       : nullptr;
}

void ShelfModel::AddObserver(ShelfModelObserver* observer) {
  observers_.AddObserver(observer);
}

void ShelfModel::RemoveObserver(ShelfModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

int ShelfModel::ValidateInsertionIndex(ShelfItemType type, int index) const {
  DCHECK(index >= 0 && index <= item_count() + 1);

  // Clamp |index| to the allowed range for the type as determined by |weight|.
  ShelfItem weight_dummy;
  weight_dummy.type = type;
  index = std::max(std::lower_bound(items_.begin(), items_.end(), weight_dummy,
                                    CompareByWeight) -
                       items_.begin(),
                   static_cast<ShelfItems::difference_type>(index));
  index = std::min(std::upper_bound(items_.begin(), items_.end(), weight_dummy,
                                    CompareByWeight) -
                       items_.begin(),
                   static_cast<ShelfItems::difference_type>(index));

  return index;
}

void ShelfModel::UpdateItemNotification(const std::string& app_id,
                                        bool has_badge) {
  int index = ItemIndexByAppID(app_id);
  // If the item is not pinned or active on the shelf.
  if (index == -1)
    return;

  if (items_[index].has_notification == has_badge)
    return;

  items_[index].has_notification = has_badge;

  for (auto& observer : observers_)
    observer.ShelfItemChanged(index, items_[index]);
}

}  // namespace ash
