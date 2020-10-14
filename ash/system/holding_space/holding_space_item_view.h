// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class ToggleImageButton;
}  // namespace views

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

  // Returns if `view` is an instance of `HoldingSpaceItemView`.
  static bool IsInstance(views::View* view);

  // views::InkDropHostView:
  SkColor GetInkDropBaseColor() const override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool OnKeyPressed(const ui::KeyEvent& event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;

  const HoldingSpaceItem* item() const { return item_; }

  void SetSelected(bool selected);
  bool selected() const { return selected_; }

 protected:
  void AddPin(views::View* parent);

 private:
  void OnPaintFocus(gfx::Canvas* canvas, gfx::Size size);
  void OnPaintSelect(gfx::Canvas* canvas, gfx::Size size);
  void OnPinPressed();
  void UpdatePin();

  HoldingSpaceItemViewDelegate* const delegate_;
  const HoldingSpaceItem* const item_;
  views::ToggleImageButton* pin_ = nullptr;

  // Owners for the layers used to paint focused and selected states.
  std::unique_ptr<ui::LayerOwner> selected_layer_owner_;
  std::unique_ptr<ui::LayerOwner> focused_layer_owner_;

  // Whether or not this view is selected.
  bool selected_ = false;

  base::WeakPtrFactory<HoldingSpaceItemView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
