// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_

#include <vector>

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

class HoldingSpaceItem;

class ASH_PUBLIC_EXPORT HoldingSpaceModelObserver
    : public base::CheckedObserver {
 public:
  // Called when `items` get added to the holding space model.
  virtual void OnHoldingSpaceItemsAdded(
      const std::vector<const HoldingSpaceItem*>& items) {}

  // Called when `items` get removed from the holding space model.
  virtual void OnHoldingSpaceItemsRemoved(
      const std::vector<const HoldingSpaceItem*>& items) {}

  // Called when an `item` gets updated within the holding space model.
  virtual void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) {}

  // Called when a partially initialized holding space `item` gets finalized.
  virtual void OnHoldingSpaceItemFinalized(const HoldingSpaceItem* item) {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_
