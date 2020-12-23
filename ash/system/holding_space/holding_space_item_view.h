// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/holding_space/holding_space_model.h"
#include "ash/public/cpp/holding_space/holding_space_model_observer.h"
#include "base/scoped_observation.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/metadata/metadata_header_macros.h"

namespace views {
class InkDropContainerView;
class ToggleImageButton;
}  // namespace views

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceItemViewDelegate;

// Base class for `HoldingSpaceItemChipView` and
// `HoldingSpaceItemScreenCaptureView`. Note that `HoldingSpaceItemView` may
// temporarily outlive its associated `HoldingSpaceItem` when it is being
// animated out.
class ASH_EXPORT HoldingSpaceItemView : public views::InkDropHostView,
                                        public HoldingSpaceModelObserver {
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
  void AddLayerBeneathView(ui::Layer* layer) override;
  void RemoveLayerBeneathView(ui::Layer* layer) override;
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

  // HoldingSpaceModelObserver:
  void OnHoldingSpaceItemUpdated(const HoldingSpaceItem* item) override;

  // Starts a drag from this view at the location specified by the given `event`
  // and with the specified `source`. Note that this method copies the logic of
  // `views::View::DoDrag()` as a workaround to that API being private.
  void StartDrag(const ui::LocatedEvent& event,
                 ui::mojom::DragEventSource source);

  const HoldingSpaceItem* item() const { return item_; }
  const std::string& item_id() const { return item_id_; }

  void SetSelected(bool selected);
  bool selected() const { return selected_; }

 protected:
  views::ToggleImageButton* AddPin(views::View* parent);
  virtual void OnPinVisiblityChanged(bool pin_visible) {}

 private:
  void OnPaintFocus(gfx::Canvas* canvas, gfx::Size size);
  void OnPaintSelect(gfx::Canvas* canvas, gfx::Size size);
  void OnPinPressed();
  void UpdatePin();

  HoldingSpaceItemViewDelegate* const delegate_;
  const HoldingSpaceItem* const item_;

  // Cache the id of the associated holding space item so that it can be
  // accessed even after `item_` has been destroyed. Note that `item_` may be
  // destroyed if this view is in the process of animating out.
  const std::string item_id_;

  // Owned by view hierarchy.
  views::InkDropContainerView* ink_drop_container_ = nullptr;
  views::ToggleImageButton* pin_ = nullptr;

  // Owners for the layers used to paint focused and selected states.
  std::unique_ptr<ui::LayerOwner> selected_layer_owner_;
  std::unique_ptr<ui::LayerOwner> focused_layer_owner_;

  // Whether or not this view is selected.
  bool selected_ = false;

  base::ScopedObservation<HoldingSpaceModel, HoldingSpaceModelObserver>
      model_observer_{this};

  base::WeakPtrFactory<HoldingSpaceItemView> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_ITEM_VIEW_H_
