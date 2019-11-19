// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shelf_model.h"

#include <algorithm>
#include <utility>

#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model_observer.h"

namespace ash {

namespace {

static ShelfModel* g_shelf_model = nullptr;

int ShelfItemTypeToWeight(ShelfItemType type) {
  switch (type) {
    case TYPE_BROWSER_SHORTCUT:
    case TYPE_PINNED_APP:
      return 1;
    case TYPE_APP:
      return 2;
    case TYPE_DIALOG:
      return 3;
    case TYPE_UNDEFINED:
      NOTREACHED() << "ShelfItemType must be set";
      return -1;
  }

  NOTREACHED() << "Invalid type " << type;
  return 1;
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

void ShelfModel::PinAppWithID(const std::string& app_id) {
  const ShelfID shelf_id(app_id);

  // If the app is already pinned, do nothing and return.
  if (IsAppPinned(shelf_id.app_id))
    return;

  // Convert an existing item to be pinned, or create a new pinned item.
  const int index = ItemIndexByID(shelf_id);
  if (index >= 0) {
    ShelfItem item = items_[index];
    DCHECK_EQ(item.type, TYPE_APP);
    DCHECK(!item.pinned_by_policy);
    item.type = TYPE_PINNED_APP;
    Set(index, item);
  } else if (!shelf_id.IsNull()) {
    ShelfItem item;
    item.type = TYPE_PINNED_APP;
    item.id = shelf_id;
    Add(item);
  }
}

bool ShelfModel::IsAppPinned(const std::string& app_id) {
  const int index = ItemIndexByID(ShelfID(app_id));
  return index >= 0 && (items_[index].type == TYPE_PINNED_APP ||
                        items_[index].type == TYPE_BROWSER_SHORTCUT);
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

int ShelfModel::Add(const ShelfItem& item) {
  return AddAt(items_.size(), item);
}

int ShelfModel::AddAt(int index, const ShelfItem& item) {
  // Items should have unique non-empty ids to avoid undefined model behavior.
  DCHECK(!item.id.IsNull()) << " The id is null.";
  DCHECK_EQ(ItemIndexByID(item.id), -1) << " The id is not unique: " << item.id;
  index = ValidateInsertionIndex(item.type, index);
  items_.insert(items_.begin() + index, item);
  items_[index].has_notification =
      app_id_to_notification_id_.count(item.id.app_id) > 0;
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
    return;
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

void ShelfModel::RemoveNotificationRecord(const std::string& notification_id) {
  auto notification_id_it = notification_id_to_app_id_.find(notification_id);

  // Two maps are required here because when this notification has been
  // delivered, the MessageCenter has already deleted the notification, so we
  // can't fetch the corresponding App Id.
  // If we have a record of this notification, erase it from both maps.
  if (notification_id_it == notification_id_to_app_id_.end())
    return;

  // Save the AppId so the app can be updated.
  const std::string app_id = notification_id_it->second;

  auto app_id_it = app_id_to_notification_id_.find(app_id);

  // Remove the notification_id.
  app_id_it->second.erase(notification_id);

  // If the set is empty erase the pair.
  if (app_id_it->second.empty())
    app_id_to_notification_id_.erase(app_id_it);

  // Erase the pair in the NotificationId -> AppId map.
  notification_id_to_app_id_.erase(notification_id_it);

  UpdateItemNotificationsAndNotifyObservers(app_id);
}

void ShelfModel::AddNotificationRecord(const std::string& app_id,
                                       const std::string& notification_id) {
  auto it = app_id_to_notification_id_.find(app_id);
  if (it != app_id_to_notification_id_.end()) {
    // The app_id exists in the map, modify the set.
    it->second.insert(notification_id);
  } else {
    // The app_id hasn't been recorded yet, create a set.
    app_id_to_notification_id_.insert(
        std::pair<std::string, std::set<std::string>>(app_id,
                                                      {notification_id}));
  }

  notification_id_to_app_id_.insert(
      std::pair<std::string, std::string>(notification_id, app_id));

  UpdateItemNotificationsAndNotifyObservers(app_id);
}

int ShelfModel::ItemIndexByID(const ShelfID& shelf_id) const {
  ShelfItems::const_iterator i = ItemByID(shelf_id);
  return i == items_.end() ? -1 : static_cast<int>(i - items_.begin());
}

int ShelfModel::GetItemIndexForType(ShelfItemType type) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].type == type)
      return i;
  }
  return -1;
}

ShelfItems::const_iterator ShelfModel::ItemByID(const ShelfID& shelf_id) const {
  for (ShelfItems::const_iterator i = items_.begin(); i != items_.end(); ++i) {
    if (i->id == shelf_id)
      return i;
  }
  return items_.end();
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

void ShelfModel::SetShelfItemDelegate(
    const ShelfID& shelf_id,
    std::unique_ptr<ShelfItemDelegate> item_delegate) {
  // Create a copy of the id that can be safely accessed if |shelf_id| is backed
  // by a controller that will be deleted in the assignment below.
  const ShelfID safe_shelf_id = shelf_id;
  if (item_delegate)
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

AppWindowLauncherItemController* ShelfModel::GetAppWindowLauncherItemController(
    const ShelfID& shelf_id) {
  ShelfItemDelegate* item_delegate = GetShelfItemDelegate(shelf_id);
  return item_delegate ? item_delegate->AsAppWindowLauncherItemController()
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

void ShelfModel::UpdateItemNotificationsAndNotifyObservers(
    const std::string& app_id) {
  int index = ItemIndexByAppID(app_id);
  // If the item is not pinned or active on the shelf.
  if (index == -1)
    return;

  const bool has_notification = app_id_to_notification_id_.count(app_id) > 0;
  if (items_[index].has_notification == has_notification)
    return;

  items_[index].has_notification = has_notification;

  for (auto& observer : observers_)
    observer.ShelfItemChanged(index, items_[index]);
}

}  // namespace ash
