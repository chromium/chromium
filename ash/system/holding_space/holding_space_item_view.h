// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_

#include "ash/ash_export.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceItemViewDelegate;

// Base class for HoldingSpaceItemChipView and HoldingSpaceItemScreenshotView.
class ASH_EXPORT HoldingSpaceItemView : public views::InkDropHostView {
 public:
  METADATA_HEADER(HoldingSpaceItemView);

  HoldingSpaceItemView(HoldingSpaceItemViewDelegate*, const HoldingSpaceItem*);
  HoldingSpaceItemView(const HoldingSpaceItemView&) = delete;
  HoldingSpaceItemView& operator=(const HoldingSpaceItemView&) = delete;
  ~HoldingSpaceItemView() override;

  // Returns `view` cast as a `HoldingSpaceItemView`. Note that this performs a
  // DCHECK to assert that `view` is in fact a `HoldingSpaceItemView` instance.
  static HoldingSpaceItemView* Cast(views::View* view);

  // views::InkDropHostView:
  SkColor GetInkDropBaseColor() const override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

  const HoldingSpaceItem* item() const { return item_; }

  void SetSelected(bool selected);
  bool selected() const { return selected_; }

 private:
  HoldingSpaceItemViewDelegate* delegate_;
  const HoldingSpaceItem* const item_;

  // Whether or not this view is selected.
  bool selected_ = false;
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
