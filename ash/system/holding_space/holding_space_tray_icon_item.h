// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_ITEM_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_ITEM_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_delegate.h"

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class HoldingSpaceItem;
class HoldingSpaceTrayIcon;
enum class ShelfAlignment;

// Class to visually represent a single holding space item within the holding
// space tray icon in the shelf. While determined to be within the icon's
// viewport, each instance will manage a layer for the holding space tray icon.
class ASH_EXPORT HoldingSpaceTrayIconItem
    : public ui::LayerDelegate,
      public ui::ImplicitAnimationObserver {
 public:
  HoldingSpaceTrayIconItem(HoldingSpaceTrayIcon*, const HoldingSpaceItem*);
  HoldingSpaceTrayIconItem(const HoldingSpaceTrayIconItem&) = delete;
  HoldingSpaceTrayIconItem& operator=(const HoldingSpaceTrayIconItem&) = delete;
  ~HoldingSpaceTrayIconItem() override;

  // Animates this item in.
  void AnimateIn();

  // Animates this item out, invoking the specified closure on completion.
  void AnimateOut(base::OnceClosure animate_out_closure);

  // Animates a shift of this item.
  void AnimateShift();

  // Animates an unshift of this item.
  void AnimateUnshift();

  // Invoked when the shelf associated with `icon_` has changed from
  // `old_shelf_alignment` to `new_shelf_alignment`.
  void OnShelfAlignmentChanged(ShelfAlignment old_shelf_alignment,
                               ShelfAlignment new_shelf_alignment);

  // Returns the holding space `item_` visually represented by this instance.
  const HoldingSpaceItem* item() const { return item_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Creates the `layer_` for this item. Note that `layer_` may be created
  // multiple times throughout this instance's lifetime as `layer_` will only
  // exist while in the viewport for the holding space tray `icon_`.
  void CreateLayer();

  // Returns whether this item needs a layer for its current `transform_`. Since
  // we only maintain `layer_` while it appears in the viewport for the holding
  // space tray `icon_`, this is used to gate creation/deletion of `layer_`.
  bool NeedsLayer() const;

  // Schedules repaint of `layer_`, no-oping if it doesn't exist.
  void InvalidateLayer();

  HoldingSpaceTrayIcon* const icon_;
  const HoldingSpaceItem* item_;

  // A cached representation of the associated holding space `item_`'s image
  // which has been cropped, resized, and clipped to a circle to be painted at
  // `layer_`'s contents bounds.
  std::unique_ptr<gfx::ImageSkia> contents_image_;

  // This is a proxy for `layer_`'s transform and represents the target
  // position of this item. Because `layer_` only exists while in `icon_`'s
  // viewport, we need to manage transform ourselves and continue to update it
  // even when `layer_` doesn't exist.
  gfx::Transform transform_;

  // The layer serving as the visual representation of the associated holding
  // space `item_` in the holding space `icon_` in the shelf. This only exists
  // while in the `icon_`s viewport as determined by the current `transform_`.
  std::unique_ptr<ui::Layer> layer_;

  // Closure to invoke on completion of `AnimateOut()`. It is expected that this
  // instance may be deleted during invocation.
  base::OnceClosure animate_out_closure_;

  base::WeakPtrFactory<HoldingSpaceTrayIconItem> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_ITEM_H_
