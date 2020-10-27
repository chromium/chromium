// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/callback.h"
#include "base/observer_list.h"

namespace ash {

class HoldingSpaceItem;
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

  // Removes a single holding space item from the model.
  void RemoveItem(const std::string& id);

  // Removes all holding space items from the model for which the specified
  // `predicate` returns true.
  using Predicate = base::RepeatingCallback<bool(const HoldingSpaceItem*)>;
  void RemoveIf(Predicate predicate);

  // Removes all the items from the model.
  void RemoveAll();

  // Gets a single holding space item.
  // Returns nullptr if the item does not exist in the model.
  const HoldingSpaceItem* GetItem(const std::string& id) const;

  const ItemList& items() const { return items_; }

  void AddObserver(HoldingSpaceModelObserver* observer);
  void RemoveObserver(HoldingSpaceModelObserver* observer);

 private:
  // The list of items added to the model in the order they have been added to
  // the model.
  ItemList items_;

  base::ObserverList<HoldingSpaceModelObserver> observers_;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_H_
