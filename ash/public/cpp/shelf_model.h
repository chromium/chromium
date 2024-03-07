// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SHELF_MODEL_H_
#define ASH_PUBLIC_CPP_SHELF_MODEL_H_

#include <map>
#include <memory>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/shelf_item.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

class AppWindowShelfItemController;

namespace ash {

class ShelfItemDelegate;
class ShelfModelObserver;

// An id for the AppList item, which is added in the ShelfModel constructor.
// Generated as crx_file::id_util::GenerateId("org.chromium.applist")
ASH_PUBLIC_EXPORT extern const char kAppListId[];

// An id for the BackButton item, which is added in the ShelfModel constructor.
// Generated as crx_file::id_util::GenerateId("org.chromium.backbutton")
ASH_PUBLIC_EXPORT extern const char kBackButtonId[];

// Model used for shelf items. Owns ShelfItemDelegates but does not create them.
class ASH_PUBLIC_EXPORT ShelfModel {
 public:
  // Get or set a weak pointer to the singleton ShelfModel instance, not owned.
  static ShelfModel* Get();
  static void SetInstance(ShelfModel* shelf_model);

  // Used to mark the current shelf model mutation as user-triggered, while
  // the instance of this class is in scope.
  class ScopedUserTriggeredMutation {
   public:
    explicit ScopedUserTriggeredMutation(ShelfModel* model) : model_(model) {
      model_->current_mutation_is_user_triggered_++;
    }

    ~ScopedUserTriggeredMutation() {
      model_->current_mutation_is_user_triggered_--;
      DCHECK_GE(model_->current_mutation_is_user_triggered_, 0);
    }

   private:
    raw_ptr<ShelfModel> model_ = nullptr;
  };

  // Some classes in ash have the ability to insert an item into the ShelfModel,
  // but with no knowledge of the item beyond an |app_id|. This delegate creates
  // an explicit mechanism for those classes to fetch both a ShelfItem and a
  // ShelfItemDelegate.
  //
  // If we were designing the architecture from scratch, we probably would not
  // need this class at all. The point of this class is to take a previous
  // implicit dependency from //ash on //chrome and make it explicit.
  class ShelfItemFactory {
   public:
    // Creates a shelf item for an app..
    virtual std::unique_ptr<ShelfItem> CreateShelfItemForApp(
        const ash::ShelfID& app_id,
        ash::ShelfItemStatus status,
        ash::ShelfItemType shelf_item_type,
        const std::u16string& title) = 0;

    // Creates a shelf item delegate for a given `app_id`.
    virtual std::unique_ptr<ShelfItemDelegate> CreateShelfItemDelegateForAppId(
        const std::string& app_id) = 0;
  };

  ShelfModel();

  ShelfModel(const ShelfModel&) = delete;
  ShelfModel& operator=(const ShelfModel&) = delete;

  ~ShelfModel();

  // Adds an item to the shelf, using the default factory to construct a
  // delegate. If a delegate cannot be constructed for this type of app, then no
  // item will be added.
  // Prefer to use AddItem directly when the delegate can be easily created.
  void AddAndPinAppWithFactoryConstructedDelegate(const std::string& app_id);

  // This function can only be called with |app_id| is already present in the
  // shelf. Changes the ShelfItem state to be pinned. This method has no effect
  // if the item is already pinned.
  void PinExistingItemWithID(const std::string& app_id);

  // Checks if the app with |app_id_| is pinned to the shelf.
  bool IsAppPinned(const std::string& app_id) const;

  // Returns whether the app specified by `app_id` is allowed to be set with the
  // target pin state. If `target_pin` is true, the target is to pin the item;
  // otherwise, the target is to unpin the item.
  // Returns true if the app's current pin state matches the target state, even
  // if the app pin state is not modifiable (e.g. due to policy).
  bool AllowedToSetAppPinState(const std::string& app_id,
                               bool target_pin) const;

  // Unpins app item with |app_id|.
  void UnpinAppWithID(const std::string& app_id);

  // Cleans up the ShelfItemDelegates.
  void DestroyItemDelegates();

  // Adds a new item to the model. Returns the resulting index.
  int Add(const ShelfItem& item, std::unique_ptr<ShelfItemDelegate> delegate);

  // Adds the item. |index| is the requested insertion index, which may be
  // modified to meet type-based ordering. Returns the actual insertion index.
  int AddAt(int index,
            const ShelfItem& item,
            std::unique_ptr<ShelfItemDelegate> delegate);

  // Removes the item at |index|.
  void RemoveItemAt(int index);

  // Removes the item with id |shelf_id| and passes ownership of its
  // ShelfItemDelegate to the caller. This is useful if you want to remove an
  // item from the shelf temporarily and be able to restore its behavior later.
  std::unique_ptr<ShelfItemDelegate> RemoveItemAndTakeShelfItemDelegate(
      const ShelfID& shelf_id);

  // Returns whether the item with the given index can be swapped with the
  // next (or previous) item. Example cases when a swap cannot happen are:
  // trying to swap the first item with the previous one, trying to swap
  // the last item with the next one, trying to swap a pinned item with an
  // unpinned item.
  bool CanSwap(int index, bool with_next) const;

  // Swaps the item at the given index with the next one if |with_next| is
  // true, or with the previous one if |with_next| is false. Returns true
  // if the requested swap has happened, and false otherwise.
  bool Swap(int index, bool with_next);

  // Moves the item at |index| to |target_index|. |target_index| is in terms
  // of the model *after* the item at |index| is removed.
  void Move(int index, int target_index);

  // Resets the item at the specified index. The item's id should not change.
  void Set(int index, const ShelfItem& item);

  // Updates the items' |is_on_active_desk| from the given vector
  // |items_desk_updates|. Items whose indices are not included in
  // |items_desk_updates| will remain unchanged.
  struct ItemDeskUpdate {
    // The index of the item being updated.
    int index = -1;
    // The new value of the item's |ShelfItem::is_on_active_desk|.
    bool is_on_active_desk = false;
  };
  void UpdateItemsForDeskChange(
      const std::vector<ItemDeskUpdate>& items_desk_updates);

  // Returns the ID of the currently active item, or an empty ShelfID if
  // nothing is currently active.
  const ShelfID& active_shelf_id() const { return active_shelf_id_; }

  // Returns whether the mutation that is currently being made in the model
  // was user-triggered.
  bool is_current_mutation_user_triggered() const {
    return current_mutation_is_user_triggered_ > 0;
  }

  // Sets |shelf_id| to be the newly active shelf item.
  void SetActiveShelfID(const ShelfID& shelf_id);

  // Notifies observers that the status of the item corresponding to |id|
  // has changed.
  void OnItemStatusChanged(const ShelfID& id);

  // Notifies observers that an item has been dragged off the shelf (it is still
  // being dragged).
  void OnItemRippedOff();

  // Notifies observers that an item that was dragged off the shelf has been
  // dragged back onto the shelf (it is still being dragged).
  void OnItemReturnedFromRipOff(int index);

  // Update the ShelfItem with |app_id| to set whether the item currently has a
  // notification.
  void UpdateItemNotification(const std::string& app_id, bool has_badge);

  // Returns the index of the item with id |shelf_id|, or -1 if none exists.
  int ItemIndexByID(const ShelfID& shelf_id) const;

  // Returns the |index| of the item matching |type| in |items_|.
  // Returns -1 if the matching item is not found.
  int GetItemIndexForType(ShelfItemType type);

  // Returns the index of the first running application or the index where the
  // first running application would go if there are no running (non pinned)
  // applications yet.
  int FirstRunningAppIndex() const;

  // Returns a pointer of ShelfItem with the given |shelf_id| in this model.
  // Or, nullptr if not found.
  const ShelfItem* ItemByID(const ShelfID& shelf_id) const;

  // Returns the index of the matching ShelfItem or -1 if the |app_id| doesn't
  // match a ShelfItem.
  int ItemIndexByAppID(const std::string& app_id) const;

  const ShelfItems& items() const { return items_; }
  int item_count() const { return static_cast<int>(items_.size()); }

  // Sets |item_delegate| for the given |shelf_id| and takes ownership.
  void ReplaceShelfItemDelegate(
      const ShelfID& shelf_id,
      std::unique_ptr<ShelfItemDelegate> item_delegate);

  // Returns ShelfItemDelegate for |shelf_id|, or nullptr if none exists.
  ShelfItemDelegate* GetShelfItemDelegate(const ShelfID& shelf_id) const;

  // Sets the ShelfItemFactory.
  void SetShelfItemFactory(ShelfItemFactory* factory);

  // Returns AppWindowShelfItemController for |shelf_id|, or nullptr if none
  // exists.
  AppWindowShelfItemController* GetAppWindowShelfItemController(
      const ShelfID& shelf_id);

  void AddObserver(ShelfModelObserver* observer);
  void RemoveObserver(ShelfModelObserver* observer);

 private:
  // Makes sure |index| is in line with the type-based order of items. If that
  // is not the case, adjusts index by shifting it to the valid range and
  // returns the new value.
  int ValidateInsertionIndex(ShelfItemType type, int index) const;

  ShelfItems items_;

  // This pointer must outlive this class.
  raw_ptr<ShelfItemFactory, DanglingUntriaged> shelf_item_factory_ = nullptr;

  // The shelf ID of the currently active shelf item, or an empty ID if
  // nothing is active.
  ShelfID active_shelf_id_;

  // A counter to determine whether any mutation currently in progress in
  // the model is the result of a manual user intervention. If a shelf item
  // is added once an app has been installed, it is not considered a direct
  // user interaction.
  int current_mutation_is_user_triggered_ = 0;

  base::ObserverList<ShelfModelObserver>::UncheckedAndDanglingUntriaged
      observers_;

  std::map<ShelfID, std::unique_ptr<ShelfItemDelegate>>
      id_to_item_delegate_map_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SHELF_MODEL_H_
