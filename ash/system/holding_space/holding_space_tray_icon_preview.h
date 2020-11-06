// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_PREVIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_PREVIEW_H_

#include <memory>
#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "base/scoped_observer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

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
class ASH_EXPORT HoldingSpaceTrayIconPreview
    : public ui::LayerDelegate,
      public ui::ImplicitAnimationObserver,
      public views::ViewObserver {
 public:
  HoldingSpaceTrayIconPreview(HoldingSpaceTrayIcon*, const HoldingSpaceItem*);
  HoldingSpaceTrayIconPreview(const HoldingSpaceTrayIconPreview&) = delete;
  HoldingSpaceTrayIconPreview& operator=(const HoldingSpaceTrayIconPreview&) =
      delete;
  ~HoldingSpaceTrayIconPreview() override;

  // Animates this preview in at the specified `index`.
  void AnimateIn(size_t index);

  // Animates this preview out, invoking the specified closure on completion.
  void AnimateOut(base::OnceClosure animate_out_closure);

  // Animates a shift of this preview.
  void AnimateShift();

  // Animates an unshift of this preview.
  void AnimateUnshift();

  // Invoked when the shelf associated with `icon_` has changed from
  // `old_shelf_alignment` to `new_shelf_alignment`.
  void OnShelfAlignmentChanged(ShelfAlignment old_shelf_alignment,
                               ShelfAlignment new_shelf_alignment);

  // Returns the holding space `item_` visually represented by this preview.
  const HoldingSpaceItem* item() const { return item_; }

 private:
  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // Creates the `layer_` for this preview. Note that `layer_` may be created
  // multiple times throughout this preview's lifetime as `layer_` will only
  // exist while in the viewport for the holding space tray `icon_`.
  void CreateLayer();

  // Returns whether this preview needs a layer for its current `transform_`.
  // Since we only maintain `layer_` while it appears in the viewport for the
  // holding space tray `icon_`, this is used to gate creation/deletion of
  // `layer_`.
  bool NeedsLayer() const;

  // Schedules repaint of `layer_`, no-oping if it doesn't exist.
  void InvalidateLayer();

  // Updates the bounds of `layer_`.
  void UpdateLayerBounds();

  // Adjusts the specified `vector_2df` for shelf alignment and text direction.
  // The given `vector_2df` should specify the desired value for horizontal
  // alignment in LTR and will be adjusted for vertical alignment and/or RTL.
  void AdjustForShelfAlignmentAndTextDirection(gfx::Vector2dF* vector_2df);

  HoldingSpaceTrayIcon* const icon_;
  const HoldingSpaceItem* item_;

  // A cached representation of the associated holding space `item_`'s image
  // which has been cropped, resized, and clipped to a circle to be painted at
  // `layer_`'s contents bounds.
  std::unique_ptr<gfx::ImageSkia> contents_image_;

  // This is a proxy for `layer_`'s transform and represents the target
  // position of this preview. Because `layer_` only exists while in `icon_`'s
  // viewport, we need to manage transform ourselves and continue to update it
  // even when `layer_` doesn't exist.
  gfx::Transform transform_;

  // The layer serving as the visual representation of the associated holding
  // space `item_` in the holding space `icon_` in the shelf. This only exists
  // while in the `icon_`s viewport as determined by the current `transform_`.
  std::unique_ptr<ui::Layer> layer_;

  // Closure to invoke on completion of `AnimateOut()`. It is expected that this
  // preview may be deleted during invocation.
  base::OnceClosure animate_out_closure_;

  // The `layer_` for this preview is parented by `icon_`'s layer. It is
  // necessary to observe and react to bounds changes in `icon_` to keep
  // `layer_`'s bounds in sync.
  ScopedObserver<views::View, views::ViewObserver> icon_observer_{this};

  base::WeakPtrFactory<HoldingSpaceTrayIconPreview> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_PREVIEW_H_
