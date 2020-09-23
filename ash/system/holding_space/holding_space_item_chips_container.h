// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIPS_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIPS_CONTAINER_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace ash {

// A container view which arranges item chips into a 2 column grid.
class HoldingSpaceItemChipsContainer : public views::View {
 public:
  HoldingSpaceItemChipsContainer();
  HoldingSpaceItemChipsContainer(const HoldingSpaceItemChipsContainer& other) =
      delete;
  HoldingSpaceItemChipsContainer& operator=(
      const HoldingSpaceItemChipsContainer& other) = delete;
  ~HoldingSpaceItemChipsContainer() override;

  // views::View:
  const char* GetClassName() const override;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIPS_CONTAINER_H_
