// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "base/callback.h"
#include "base/observer_list.h"
#include "url/gurl.h"

namespace base {
class FilePath;
}  // namespace base

namespace ash {

class HoldingSpaceModelObserver;

// The data model for the temporary holding space UI. It contains the list of
// items that should be shown in the temporary holding space UI - each item will
// represent a piece of data added to the holding space by the user (for
// example, text, URLs, or images).
// The main goal of the class is to provide UI implementation agnostic
// information about items added to the holding space, and to provide an
// interface to propagate holding space changes between ash and Chrome.
class ASH_PUBLIC_EXPORT HoldingSpaceModel {
 public:
  using ItemList = std::vector<std::unique_ptr<HoldingSpaceItem>>;

  HoldingSpaceModel();
  HoldingSpaceModel(const HoldingSpaceModel& other) = delete;
  HoldingSpaceModel& operator=(const HoldingSpaceModel& other) = delete;
  ~HoldingSpaceModel();

  // Adds a single holding space item to the model.
  void AddItem(std::unique_ptr<HoldingSpaceItem> item);

  // Adds multiple holding space items to the model.
  void AddItems(std::vector<std::unique_ptr<HoldingSpaceItem>> items);

  // Removes a single holding space item from the model.
  void RemoveItem(const std::string& id);

  // Removes multiple holding space items from the model.
  void RemoveItems(const std::set<std::string>& ids);

  // Finalizes a partially initialized holding space item using the provided
  // file system URL. The item will be removed if the file system url is empty.
  void FinalizeOrRemoveItem(const std::string& id, const GURL& file_system_url);

  // Updates the backing file for a single holding space item to the specified
  // `file_path` and `file_system_url`.
  void UpdateBackingFileForItem(const std::string& id,
                                const base::FilePath& file_path,
                                const GURL& file_system_url);

  // Removes all holding space items from the model for which the specified
  // `predicate` returns true.
  using Predicate = base::RepeatingCallback<bool(const HoldingSpaceItem*)>;
  void RemoveIf(Predicate predicate);

  // Invalidates image representations for items for which the specified
  // `predicate` returns true.
  void InvalidateItemImageIf(Predicate predicate);

  // Removes all the items from the model.
  void RemoveAll();

  // Gets a single holding space item.
  // Returns nullptr if the item does not exist in the model.
  const HoldingSpaceItem* GetItem(const std::string& id) const;

  // Gets a single holding space item with the specified `type` backed by the
  // specified `file_path`. Returns `nullptr` if the item does not exist in the
  // model.
  const HoldingSpaceItem* GetItem(HoldingSpaceItem::Type type,
                                  const base::FilePath& file_path) const;

  // Returns whether or not there exists a holding space item of the specified
  // `type` backed by the specified `file_path`.
  bool ContainsItem(HoldingSpaceItem::Type type,
                    const base::FilePath& file_path) const;

  // Returns true if the model contains any finalized items of the specified
  // `type`, false otherwise.
  bool ContainsFinalizedItemOfType(HoldingSpaceItem::Type type) const;

  const ItemList& items() const { return items_; }

  void AddObserver(HoldingSpaceModelObserver* observer);
  void RemoveObserver(HoldingSpaceModelObserver* observer);

 private:
  // The list of items added to the model in the order they have been added to
  // the model.
  ItemList items_;

  // Caches the count of finalized items in the model for each holding space
  // item type. Used to quickly look up whether the model contains any finalized
  // items of a given type.
  std::map<HoldingSpaceItem::Type, size_t> finalized_item_counts_by_type_;

  base::ObserverList<HoldingSpaceModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
