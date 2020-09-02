// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIPS_CONTAINER_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIPS_CONTAINER_H_

#include "ash/ash_export.h"
#include "ui/views/view.h"

namespace views {
class ColumnSet;
class GridLayout;
}  // namespace views

namespace ash {

class HoldingSpaceItem;

// A container view which automatically arranges item chips into a 2 column
// grid.
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

  void AddItemChip(const HoldingSpaceItem* item);

 private:
  views::GridLayout* layout_ = nullptr;
  views::ColumnSet* column_set_ = nullptr;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_CHIPS_CONTAINER_H_
