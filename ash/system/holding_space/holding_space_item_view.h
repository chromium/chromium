// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/views/animation/ink_drop_host_view.h"

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceItemContextMenu;

// Base class for HoldingSpaceItemChipView and HoldingSpaceItemScreenshotView.
class ASH_EXPORT HoldingSpaceItemView : public views::InkDropHostView {
 public:
  explicit HoldingSpaceItemView(const HoldingSpaceItem* item);
  HoldingSpaceItemView(const HoldingSpaceItemView&) = delete;
  HoldingSpaceItemView& operator=(const HoldingSpaceItemView&) = delete;
  ~HoldingSpaceItemView() override;

  // views::InkDropHostView:
  int GetDragOperations(const gfx::Point& point) override;
  SkColor GetInkDropBaseColor() const override;
  void WriteDragData(const gfx::Point& point, ui::OSExchangeData*) override;

  const HoldingSpaceItem* item() const { return item_; }

 private:
  const HoldingSpaceItem* const item_;
  std::unique_ptr<HoldingSpaceItemContextMenu> const context_menu_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
