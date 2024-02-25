// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_PREVIEW_H_
#define ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_PREVIEW_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class HoldingSpaceItem;
class ProgressIndicator;
class Shelf;
enum class ShelfAlignment;

// Class to visually represent a single holding space item within the holding
// space tray icon in the shelf. While determined to be within the icon's
// viewport, each instance will manage a layer for the holding space tray icon.
class ASH_EXPORT HoldingSpaceTrayIconPreview
    : public ui::LayerDelegate,
      public ui::ImplicitAnimationObserver,
      public views::ViewObserver {
 public:
  static constexpr char kClassName[] = "HoldingSpaceTrayIconPreview";
  static constexpr char kImageLayerName[] =
      "HoldingSpaceTrayIconPreview::Image";

  HoldingSpaceTrayIconPreview(Shelf* shelf,
                              views::View* container,
                              const HoldingSpaceItem* item);
  HoldingSpaceTrayIconPreview(const HoldingSpaceTrayIconPreview&) = delete;
  HoldingSpaceTrayIconPreview& operator=(const HoldingSpaceTrayIconPreview&) =
      delete;
  ~HoldingSpaceTrayIconPreview() override;

  // Animates this preview in. The item is animated at `*pending_index_`. This
  // will move `pending_index_` value to `index_`.
  // `additional_delay` - the delay that should be added on top of initial delay
  // when starting the animation.
  void AnimateIn(base::TimeDelta additional_delay);

  // Animates this preview out, invoking the specified closure on completion.
  void AnimateOut(base::OnceClosure animate_out_closure);

  // Shifts this preview. The item is shifted to `*pending_index_`. This
  // will move `pending_index_` value to `index_`.
  void AnimateShift(base::TimeDelta delay);

  // Updates the preview transform to keep relative position to the end of the
  // visible bounds when the icon container size changes.
  // Transform is updated without animation.
  void AdjustTransformForContainerSizeChange(const gfx::Vector2d& size_change);

  // Invoked when the `shelf_` has changed from `old_shelf_alignment` to
  // `new_shelf_alignment`.
  void OnShelfAlignmentChanged(ShelfAlignment old_shelf_alignment,
                               ShelfAlignment new_shelf_alignment);

  // Invoked when the `shelf_` configuration has changed.
  void OnShelfConfigChanged();

  // Invoked when the theme of the parent `container_` has changed.
  void OnThemeChanged();

  ui::Layer* layer() { return layer_owner_.layer(); }

  const std::optional<size_t>& index() const { return index_; }

  const std::optional<size_t>& pending_index() const { return pending_index_; }
  void set_pending_index(size_t index) { pending_index_ = index; }

 private:
  class ImageLayerOwner;

  // ui::LayerDelegate:
  void OnPaintLayer(const ui::PaintContext& context) override;
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;
  void OnViewIsDeleting(views::View* observed_view) override;

  // Creates a layer for this preview. The layer will be owned by
  // `layer_owner_`. Note that a layer may be created multiple times throughout
  // this preview's lifetime as the preview will only have a layer while in the
  // viewport for the holding space tray `container_`. |initial_transform| - The
  // transform that should be set on the layer.
  void CreateLayer(const gfx::Transform& initial_transform);

  // Destroys the layer for this preview, if it was previously created.
  void DestroyLayer();

  // Returns whether this preview needs a layer for its current `transform_`.
  // Since 'layer_owner_' has a layer only while the preview appears in the
  // viewport for the holding space tray `container_`, this is used to gate
  // creation/deletion of the preview layer.
  bool NeedsLayer() const;

  // Schedules repaint of `layer()`, no-oping if it doesn't exist.
  void InvalidateLayer();

  // Updates the bounds of `layer()`.
  void UpdateLayerBounds();

  // Adjusts the specified `vector_2df` for shelf alignment and text direction.
  // The given `vector_2df` should specify the desired value for horizontal
  // alignment in LTR and will be adjusted for vertical alignment and/or RTL.
  void AdjustForShelfAlignmentAndTextDirection(gfx::Vector2dF* vector_2df);

  // Returns the color resolved for the specified `color_id`.
  SkColor GetColor(ui::ColorId color_id) const;

  // The shelf whose holding space tray icon this preview belongs.
  const raw_ptr<Shelf> shelf_;

  // The view that contains all preview layers belonging to the holding space
  // icon.
  const raw_ptr<views::View> container_;

  // Owns the `ui::Layer` which paints the image representation of the
  // associated holding space item.
  std::unique_ptr<ImageLayerOwner> image_layer_owner_;

  // Owns the `ui::Layer` which paints indicate of progress for the associated
  // holding space item. NOTE: The `ui::Layer` is *not* painted if the holding
  // space item is not in-progress.
  std::unique_ptr<ProgressIndicator> progress_indicator_;

  // Whether or not this preview is currently using small dimensions. This is
  // done when in tablet mode and an app is in use.
  bool use_small_previews_ = false;

  // This is a proxy for `layer()`'s transform and represents the target
  // position of this preview. Because `layer()` only exists while in
  // `container_`'s viewport, we need to manage transform ourselves and continue
  // to update it even when `layer()` doesn't exist.
  gfx::Transform transform_;

  // The layer serving as the visual representation of the associated holding
  // space item in the holding space icon in the shelf. This only exists while
  // in the `container_`s viewport as determined by the current `transform_`.
  ui::LayerOwner layer_owner_;

  // Closure to invoke on completion of `AnimateOut()`. It is expected that this
  // preview may be deleted during invocation.
  base::OnceClosure animate_out_closure_;

  // If set, the preview index within the holding space tray icon. May be unset
  // during icon update transition before the preview is animated in.
  std::optional<size_t> index_;

  // If set, the index within the holding space tray icon to which the preview
  // is about to move. Set while the holding space tray icon is updating.
  std::optional<size_t> pending_index_;

  // The `layer()` for this preview is parented by `container_`'s layer. It is
  // necessary to observe and react to bounds changes in `container_` to keep
  // `layer()`'s bounds in sync.
  base::ScopedObservation<views::View, views::ViewObserver> container_observer_{
      this};

  base::WeakPtrFactory<HoldingSpaceTrayIconPreview> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_SYSTEM_HOLDING_SPACE_HOLDING_SPACE_TRAY_ICON_PREVIEW_H_
