// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_
#define ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_

#include "ash/public/cpp/ash_public_export.h"
#include "base/observer_list_types.h"

namespace ash {

class HoldingSpaceItem;

class ASH_PUBLIC_EXPORT HoldingSpaceModelObserver
    : public base::CheckedObserver {
 public:
  // Called when an item gets added to the holding space model.
  virtual void OnHoldingSpaceItemAdded(const HoldingSpaceItem* item) = 0;

  // Called when an item gets removed from the holding space model.
  virtual void OnHoldingSpaceItemRemoved(const HoldingSpaceItem* item) = 0;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_HOLDING_SPACE_HOLDING_SPACE_MODEL_OBSERVER_H_
