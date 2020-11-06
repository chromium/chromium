// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon_preview.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "base/i18n/rtl.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_source.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

namespace {

// Appearance.
constexpr int kElevation = 2;

// Helpers ---------------------------------------------------------------------

// Returns the shadow details to use when painting elevation.
const gfx::ShadowDetails& GetShadowDetails() {
  return gfx::ShadowDetails::Get(kElevation, /*radius=*/kTrayItemSize / 2);
}

// Returns contents bounds for painting, having accounted for shadow details.
gfx::Rect GetContentsBounds() {
  gfx::Size contents_size(kTrayItemSize, kTrayItemSize);
  gfx::Rect contents_bounds(contents_size);

  const gfx::ShadowDetails& shadow = GetShadowDetails();
  const gfx::Insets shadow_margins(gfx::ShadowValue::GetMargin(shadow.values));

  contents_size.Enlarge(shadow_margins.width(), shadow_margins.height());
  contents_bounds.ClampToCenteredSize(contents_size);

  return contents_bounds;
}

// Returns whether the specified `shelf_alignment` is horizontal.
bool IsHorizontal(ShelfAlignment shelf_alignment) {
  switch (shelf_alignment) {
    case ShelfAlignment::kBottom:
    case ShelfAlignment::kBottomLocked:
      return true;
    case ShelfAlignment::kLeft:
    case ShelfAlignment::kRight:
      return false;
  }
}

// Performs set up of the specified `animation_settings`.
void SetUpAnimation(ui::ScopedLayerAnimationSettings* animation_settings) {
  animation_settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation_settings->SetTransitionDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  animation_settings->SetTweenType(gfx::Tween::EASE_OUT);
}

// ContentsImageSource ---------------------------------------------------------

class ContentsImageSource : public gfx::ImageSkiaSource {
 public:
  explicit ContentsImageSource(const HoldingSpaceItem* item) : item_(item) {}
  ContentsImageSource(const ContentsImageSource&) = delete;
  ContentsImageSource& operator=(const ContentsImageSource&) = delete;
  ~ContentsImageSource() override = default;

 private:
  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    gfx::ImageSkia image = item_->image().image_skia();

    // Crop to square (if necessary).
    gfx::Size square_size = image.size();
    square_size.SetToMin(gfx::Size(square_size.height(), square_size.width()));
    if (image.size() != square_size) {
      gfx::Rect square_rect(image.size());
      square_rect.ClampToCenteredSize(square_size);
      image = gfx::ImageSkiaOperations::ExtractSubset(image, square_rect);
    }

    // Resize to contents size (if necessary).
    gfx::Size contents_size = GetContentsBounds().size();
    if (image.size() != contents_size) {
      image = gfx::ImageSkiaOperations::CreateResizedImage(
          image, skia::ImageOperations::ResizeMethod::RESIZE_BEST,
          contents_size);
    }

    // Clip to circle.
    // NOTE: Since `image` has already been cropped to a square, the center
    // x-coordinate, center y-coordinate, and radius all equal the same value.
    const int radius = image.width() / 2;
    gfx::Canvas canvas(image.size(), scale, /*is_opaque=*/false);
    canvas.ClipPath(SkPath::Circle(/*cx=*/radius, /*cy=*/radius, radius),
                    /*anti_alias=*/true);
    canvas.DrawImageInt(image, /*x=*/0, /*y=*/0);
    return gfx::ImageSkiaRep(canvas.GetBitmap(), scale);
  }

  const HoldingSpaceItem* item_;
};

// ContentsImage ---------------------------------------------------------------

class ContentsImage : public gfx::ImageSkia {
 public:
  ContentsImage(const HoldingSpaceItem* item,
                base::RepeatingClosure image_invalidated_closure)
      : gfx::ImageSkia(std::make_unique<ContentsImageSource>(item),
                       GetContentsBounds().size()),
        image_invalidated_closure_(image_invalidated_closure) {
    image_subscription_ = item->image().AddImageSkiaChangedCallback(
        base::BindRepeating(&ContentsImage::OnHoldingSpaceItemImageChanged,
                            base::Unretained(this)));
  }

  ContentsImage(const ContentsImage&) = delete;
  ContentsImage& operator=(const ContentsImage&) = delete;
  ~ContentsImage() = default;

 private:
  void OnHoldingSpaceItemImageChanged() {
    // Invalidate cached image reps.
    for (const gfx::ImageSkiaRep& image_rep : image_reps()) {
      RemoveRepresentation(image_rep.scale());
      RemoveUnsupportedRepresentationsForScale(image_rep.scale());
    }
    // Notify closure of invalidation.
    image_invalidated_closure_.Run();
  }

  base::RepeatingClosure image_invalidated_closure_;
  std::unique_ptr<HoldingSpaceImage::Subscription> image_subscription_;
};

}  // namespace

// HoldingSpaceTrayIconPreview -------------------------------------------------

HoldingSpaceTrayIconPreview::HoldingSpaceTrayIconPreview(
    HoldingSpaceTrayIcon* icon,
    const HoldingSpaceItem* item)
    : icon_(icon), item_(item) {
  contents_image_ = std::make_unique<ContentsImage>(
      item_, base::BindRepeating(&HoldingSpaceTrayIconPreview::InvalidateLayer,
                                 base::Unretained(this)));
  icon_observer_.Add(icon_);
}

HoldingSpaceTrayIconPreview::~HoldingSpaceTrayIconPreview() = default;

void HoldingSpaceTrayIconPreview::AnimateIn(size_t index) {
  DCHECK(transform_.IsIdentity());

  if (index > 0u) {
    gfx::Vector2dF translation(index * kTrayItemSize / 2, 0);
    AdjustForShelfAlignmentAndTextDirection(&translation);
    transform_.Translate(translation);
  }

  if (!NeedsLayer())
    return;

  CreateLayer();

  gfx::Transform pre_transform(transform_);
  pre_transform.Translate(0, -kTrayItemSize);
  layer_->SetTransform(pre_transform);

  icon_->layer()->Add(layer_.get());

  if (index > 0u) {
    ui::Layer* const parent = layer_->parent();
    const std::vector<ui::Layer*> children = parent->children();
    parent->StackBelow(layer_.get(), children[children.size() - index - 1]);
  }

  ui::ScopedLayerAnimationSettings animation_settings(layer_->GetAnimator());
  SetUpAnimation(&animation_settings);

  layer_->SetTransform(transform_);
}

void HoldingSpaceTrayIconPreview::AnimateOut(
    base::OnceClosure animate_out_closure) {
  animate_out_closure_ = std::move(animate_out_closure);

  if (!layer_) {
    std::move(animate_out_closure_).Run();
    return;
  }

  ui::ScopedLayerAnimationSettings animation_settings(layer_->GetAnimator());
  SetUpAnimation(&animation_settings);
  animation_settings.AddObserver(this);

  layer_->SetOpacity(0.f);
  layer_->SetVisible(false);
}

void HoldingSpaceTrayIconPreview::AnimateShift() {
  gfx::Vector2dF translation(kTrayItemSize / 2, 0);
  AdjustForShelfAlignmentAndTextDirection(&translation);
  transform_.Translate(translation);

  if (!layer_)
    return;

  ui::ScopedLayerAnimationSettings animation_settings(layer_->GetAnimator());
  SetUpAnimation(&animation_settings);
  animation_settings.AddObserver(this);

  layer_->SetTransform(transform_);

  if (!NeedsLayer()) {
    layer_->SetOpacity(0.f);
    layer_->SetVisible(false);
  }
}

void HoldingSpaceTrayIconPreview::AnimateUnshift() {
  gfx::Vector2dF translation(-kTrayItemSize / 2, 0);
  AdjustForShelfAlignmentAndTextDirection(&translation);
  transform_.Translate(translation);

  if (!layer_ && !NeedsLayer())
    return;

  if (!layer_) {
    CreateLayer();

    gfx::Transform pre_transform(transform_);
    pre_transform.Translate(-translation);
    layer_->SetTransform(pre_transform);

    layer_->SetOpacity(0.f);

    icon_->layer()->Add(layer_.get());
    icon_->layer()->StackAtBottom(layer_.get());
  }

  layer_->SetVisible(true);

  ui::ScopedLayerAnimationSettings animation_settings(layer_->GetAnimator());
  SetUpAnimation(&animation_settings);

  layer_->SetTransform(transform_);
  layer_->SetOpacity(1.f);
}

void HoldingSpaceTrayIconPreview::OnShelfAlignmentChanged(
    ShelfAlignment old_shelf_alignment,
    ShelfAlignment new_shelf_alignment) {
  // If shelf orientation has not changed, no action needs to be taken.
  if (IsHorizontal(old_shelf_alignment) == IsHorizontal(new_shelf_alignment))
    return;

  // Since shelf orientation has changed, the target `transform_` needs to be
  // updated. First stop the current animation to immediately advance to target
  // end values.
  const auto& weak_ptr = weak_factory_.GetWeakPtr();
  if (layer_ && layer_->GetAnimator()->is_animating())
    layer_->GetAnimator()->StopAnimating();

  // This instance may have been deleted as a result of stopping the current
  // animation if it was in the process of animating out.
  if (!weak_ptr)
    return;

  // Swap x-coordinate and y-coordinate of the target `transform_` since the
  // shelf has changed orientation from horizontal to vertical or vice versa.
  gfx::Vector2dF translation = transform_.To2dTranslation();

  // In LTR, `translation` is always a positive offset. With a horizontal shelf,
  // offset is relative to the parent layer's left bound while with a vertical
  // shelf, offset is relative to the parent layer's top bound. In RTL, positive
  // offset is still used for vertical shelf but with a horizontal shelf the
  // `translation` is a negative offset from the parent layer's right bound. For
  // this reason, a change in shelf orientation in RTL requires a negation of
  // the current `translation`.
  if (base::i18n::IsRTL())
    translation = -translation;

  gfx::Transform swapped_transform;
  swapped_transform.Translate(translation.y(), translation.x());
  transform_ = swapped_transform;

  if (layer_) {
    UpdateLayerBounds();
    layer_->SetTransform(transform_);
  }
}

// TODO(crbug.com/1142572): Support theming.
void HoldingSpaceTrayIconPreview::OnPaintLayer(
    const ui::PaintContext& context) {
  const gfx::Rect contents_bounds = GetContentsBounds();

  ui::PaintRecorder recorder(context, gfx::Size(kTrayItemSize, kTrayItemSize));
  gfx::Canvas* canvas = recorder.canvas();

  // Background.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(SK_ColorWHITE);
  flags.setLooper(gfx::CreateShadowDrawLooper(GetShadowDetails().values));
  canvas->DrawCircle(
      contents_bounds.CenterPoint(),
      std::min(contents_bounds.width(), contents_bounds.height()) / 2, flags);

  // Contents.
  DCHECK(contents_image_);
  if (!contents_image_->isNull()) {
    canvas->DrawImageInt(*contents_image_, contents_bounds.x(),
                         contents_bounds.y());
  }
}

void HoldingSpaceTrayIconPreview::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  InvalidateLayer();
}

void HoldingSpaceTrayIconPreview::OnImplicitAnimationsCompleted() {
  if (layer_->visible())
    return;

  icon_->layer()->Remove(layer_.get());
  layer_.reset();

  // NOTE: Running `animate_out_closure_` may delete `this`.
  if (animate_out_closure_)
    std::move(animate_out_closure_).Run();
}

void HoldingSpaceTrayIconPreview::OnViewBoundsChanged(views::View* view) {
  DCHECK_EQ(icon_, view);
  if (layer_)
    UpdateLayerBounds();
}

void HoldingSpaceTrayIconPreview::OnViewIsDeleting(views::View* view) {
  DCHECK_EQ(icon_, view);
  icon_observer_.Remove(icon_);
}

void HoldingSpaceTrayIconPreview::CreateLayer() {
  DCHECK(!layer_);
  layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetTransform(transform_);
  layer_->set_delegate(this);
  UpdateLayerBounds();
}

bool HoldingSpaceTrayIconPreview::NeedsLayer() const {
  // With horizontal shelf in RTL, `primary_axis_translation` is expected to be
  // negative prior to taking its absolute value since it represents an offset
  // relative to the parent layer's right bound.
  const float primary_axis_translation =
      std::abs(icon_->shelf()->PrimaryAxisValue(
          /*horizontal=*/transform_.To2dTranslation().x(),
          /*vertical=*/transform_.To2dTranslation().y()));
  return primary_axis_translation <
         kHoldingSpaceTrayIconMaxVisiblePreviews * kTrayItemSize / 2;
}

void HoldingSpaceTrayIconPreview::InvalidateLayer() {
  if (layer_)
    layer_->SchedulePaint(gfx::Rect(layer_->size()));
}

void HoldingSpaceTrayIconPreview::AdjustForShelfAlignmentAndTextDirection(
    gfx::Vector2dF* vector_2df) {
  if (!icon_->shelf()->IsHorizontalAlignment()) {
    const float x = vector_2df->x();
    vector_2df->set_x(vector_2df->y());
    vector_2df->set_y(x);
    return;
  }
  // With a horizontal shelf in RTL, translation is a negative offset relative
  // to the parent layer's right bound. This requires negation of `vector_2df`.
  if (base::i18n::IsRTL())
    vector_2df->Scale(-1.f);
}

void HoldingSpaceTrayIconPreview::UpdateLayerBounds() {
  DCHECK(layer_);
  // With a horizontal shelf in RTL, `layer_` is aligned with its parent layer's
  // right bound and translated with a negative offset. In all other cases,
  // `layer_` is aligned with its parent layer's left/top bound and translated
  // with a positive offset.
  gfx::Point origin;
  if (icon_->shelf()->IsHorizontalAlignment() && base::i18n::IsRTL()) {
    origin = icon_->GetLocalBounds().top_right();
    origin -= gfx::Vector2d(kTrayItemSize, 0);
  }
  gfx::Rect bounds(origin, gfx::Size(kTrayItemSize, kTrayItemSize));
  if (bounds != layer_->bounds())
    layer_->SetBounds(bounds);
}

}  // namespace ash
