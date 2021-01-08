// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon_preview.h"

#include <algorithm>

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "base/i18n/rtl.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_sequence.h"
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
constexpr int kElevation = 1;

// The duration of each of the preview icon bounce animation.
constexpr base::TimeDelta kBounceAnimationSegmentDuration =
    base::TimeDelta::FromMilliseconds(250);

// The delay with which preview icon is dropped into the holding space tray
// icon.
constexpr base::TimeDelta kBounceAnimationBaseDelay =
    base::TimeDelta::FromMilliseconds(150);

// The duration of shift animation.
constexpr base::TimeDelta kShiftAnimationDuration =
    base::TimeDelta::FromMilliseconds(250);

// Helpers ---------------------------------------------------------------------

// Returns the preview icon contents size.
gfx::Size GetPreviewSize() {
  return gfx::Size(kHoldingSpaceTrayIconPreviewSize,
                   kHoldingSpaceTrayIconPreviewSize);
}

// Returns the shadow details for painting elevation.
const gfx::ShadowDetails& GetShadowDetails() {
  const gfx::Size size(GetPreviewSize());
  const int radius = std::min(size.height(), size.width()) / 2;
  return gfx::ShadowDetails::Get(kElevation, radius);
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
  explicit ContentsImageSource(gfx::ImageSkia item_image)
      : item_image_(item_image) {}
  ContentsImageSource(const ContentsImageSource&) = delete;
  ContentsImageSource& operator=(const ContentsImageSource&) = delete;
  ~ContentsImageSource() override = default;

 private:
  // gfx::ImageSkiaSource:
  gfx::ImageSkiaRep GetImageForScale(float scale) override {
    gfx::ImageSkia image = item_image_;

    // The `image` should already be sized appropriately.
    DCHECK_EQ(image.size(), GetPreviewSize());

    // Clip to circle.
    // NOTE: Since `image` is a square, the center x-coordinate, center
    // y-coordinate, and radius all equal the same value.
    const int radius = image.width() / 2;
    gfx::Canvas canvas(image.size(), scale, /*is_opaque=*/false);
    canvas.ClipPath(SkPath::Circle(/*cx=*/radius, /*cy=*/radius, radius),
                    /*anti_alias=*/true);
    canvas.DrawImageInt(image, /*x=*/0, /*y=*/0);
    return gfx::ImageSkiaRep(canvas.GetBitmap(), scale);
  }

  const gfx::ImageSkia item_image_;
};

}  // namespace

// HoldingSpaceTrayIconPreview -------------------------------------------------

HoldingSpaceTrayIconPreview::HoldingSpaceTrayIconPreview(
    Shelf* shelf,
    views::View* container,
    const HoldingSpaceItem* item)
    : shelf_(shelf), container_(container), item_(item) {
  const gfx::Size size(GetPreviewSize());
  contents_image_ = gfx::ImageSkia(
      std::make_unique<ContentsImageSource>(item->image().GetImageSkia(size)),
      size);
  image_subscription_ =
      item->image().AddImageSkiaChangedCallback(base::BindRepeating(
          &HoldingSpaceTrayIconPreview::OnHoldingSpaceItemImageChanged,
          base::Unretained(this)));
  container_observer_.Observe(container_);
}

HoldingSpaceTrayIconPreview::~HoldingSpaceTrayIconPreview() = default;

void HoldingSpaceTrayIconPreview::AnimateIn(base::TimeDelta additional_delay) {
  DCHECK(transform_.IsIdentity());
  DCHECK(!index_.has_value());
  DCHECK(pending_index_.has_value());

  index_ = *pending_index_;
  pending_index_.reset();

  const gfx::Size preview_size = GetPreviewSize();
  if (*index_ > 0u) {
    gfx::Vector2dF translation(*index_ * preview_size.width() / 2, 0);
    AdjustForShelfAlignmentAndTextDirection(&translation);
    transform_.Translate(translation);
  }

  if (!NeedsLayer())
    return;

  gfx::Transform pre_transform;
  pre_transform.Translate(transform_.To2dTranslation().x(),
                          -preview_size.height());

  CreateLayer(pre_transform);

  gfx::Transform mid_transform(transform_);
  mid_transform.Translate(0, preview_size.height() * 0.25f);

  ui::ScopedLayerAnimationSettings scoped_settings(layer_->GetAnimator());
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  std::unique_ptr<ui::LayerAnimationSequence> sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::TRANSFORM,
      kBounceAnimationBaseDelay + additional_delay));

  std::unique_ptr<ui::LayerAnimationElement> initial_drop =
      ui::LayerAnimationElement::CreateTransformElement(
          mid_transform, kBounceAnimationSegmentDuration);
  initial_drop->set_tween_type(gfx::Tween::EASE_OUT_4);
  sequence->AddElement(std::move(initial_drop));

  std::unique_ptr<ui::LayerAnimationElement> rebound =
      ui::LayerAnimationElement::CreateTransformElement(
          transform_, kBounceAnimationSegmentDuration);
  rebound->set_tween_type(gfx::Tween::FAST_OUT_SLOW_IN_3);
  sequence->AddElement(std::move(rebound));

  layer_->GetAnimator()->StartAnimation(sequence.release());
}

void HoldingSpaceTrayIconPreview::AnimateOut(
    base::OnceClosure animate_out_closure) {
  animate_out_closure_ = std::move(animate_out_closure);
  pending_index_.reset();
  index_.reset();

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

void HoldingSpaceTrayIconPreview::AnimateShift(base::TimeDelta delay) {
  DCHECK(index_.has_value());
  DCHECK(pending_index_.has_value());

  index_ = *pending_index_;
  pending_index_.reset();

  if (!layer_ && NeedsLayer())
    CreateLayer(transform_);

  // Calculate the target preview transform for the new position in the icon.
  // Avoid adjustments based on relative index change, as the current transform
  // may not match the previous index in case the icon view has been resized
  // since last update - see `AdjustTransformForContainerSizeChange()`.
  transform_ = gfx::Transform();
  gfx::Vector2dF translation(index_.value() * GetPreviewSize().width() / 2, 0);
  AdjustForShelfAlignmentAndTextDirection(&translation);
  transform_.Translate(translation);

  if (!layer_)
    return;

  ui::ScopedLayerAnimationSettings scoped_settings(layer_->GetAnimator());
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  std::unique_ptr<ui::LayerAnimationSequence> sequence =
      std::make_unique<ui::LayerAnimationSequence>();
  sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::TRANSFORM, delay));

  std::unique_ptr<ui::LayerAnimationElement> shift =
      ui::LayerAnimationElement::CreateTransformElement(
          transform_, kShiftAnimationDuration);
  shift->set_tween_type(gfx::Tween::FAST_OUT_SLOW_IN);
  sequence->AddElement(std::move(shift));

  layer_->GetAnimator()->StartAnimation(sequence.release());
}

void HoldingSpaceTrayIconPreview::AdjustTransformForContainerSizeChange(
    const gfx::Vector2d& size_change) {
  if (!index_.has_value())
    return;
  int direction = base::i18n::IsRTL() ? -1 : 1;
  transform_.Translate(direction * size_change.x(), size_change.y());
  if (layer()) {
    // Update the layer transform. The current layer transform may be different
    // from `transform_` if a transform animation is in progress, so calculate
    // the new target transform using the current layer transform as the base.
    gfx::Transform layer_transform = layer()->transform();
    layer_transform.Translate(direction * size_change.x(), size_change.y());
    layer()->SetTransform(layer_transform);
  }
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
  const gfx::Rect contents_bounds = gfx::Rect(GetPreviewSize());

  ui::PaintRecorder recorder(context, contents_bounds.size());
  gfx::Canvas* canvas = recorder.canvas();

  // Background.
  // NOTE: The background radius is shrunk by a single pixel to avoid being
  // painted outside `contents_image_` bounds as might otherwise occur due to
  // pixel rounding. Failure to do so could result in white paint artifacts.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(SK_ColorWHITE);
  flags.setLooper(gfx::CreateShadowDrawLooper(GetShadowDetails().values));
  canvas->DrawCircle(
      gfx::PointF(contents_bounds.CenterPoint()),
      std::min(contents_bounds.width(), contents_bounds.height()) / 2 - 0.5,
      flags);

  // Contents.
  // NOTE: The `contents_image_` should already be resized.
  if (!contents_image_.isNull()) {
    DCHECK_EQ(contents_image_.size(), contents_bounds.size());
    canvas->DrawImageInt(contents_image_, contents_bounds.x(),
                         contents_bounds.y());
  }
}

void HoldingSpaceTrayIconPreview::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  InvalidateLayer();
}

void HoldingSpaceTrayIconPreview::OnImplicitAnimationsCompleted() {
  if (!NeedsLayer()) {
    container_->layer()->Remove(layer_.get());
    layer_.reset();
  }

  // NOTE: Running `animate_out_closure_` may delete `this`.
  if (animate_out_closure_)
    std::move(animate_out_closure_).Run();
}

void HoldingSpaceTrayIconPreview::OnViewBoundsChanged(views::View* view) {
  DCHECK_EQ(container_, view);
  if (layer_)
    UpdateLayerBounds();
}

void HoldingSpaceTrayIconPreview::OnViewIsDeleting(views::View* view) {
  DCHECK_EQ(container_, view);
  container_observer_.Reset();
}

void HoldingSpaceTrayIconPreview::OnHoldingSpaceItemImageChanged() {
  const gfx::Size size(GetPreviewSize());
  contents_image_ = gfx::ImageSkia(
      std::make_unique<ContentsImageSource>(item_->image().GetImageSkia(size)),
      size);
  InvalidateLayer();
}

void HoldingSpaceTrayIconPreview::CreateLayer(
    const gfx::Transform& initial_transform) {
  DCHECK(!layer_);
  layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  layer_->SetFillsBoundsOpaquely(false);
  layer_->SetTransform(initial_transform);
  layer_->set_delegate(this);
  UpdateLayerBounds();

  container_->layer()->Add(layer_.get());
}

bool HoldingSpaceTrayIconPreview::NeedsLayer() const {
  return index_ && *index_ <= kHoldingSpaceTrayIconMaxVisiblePreviews;
}

void HoldingSpaceTrayIconPreview::InvalidateLayer() {
  if (layer_)
    layer_->SchedulePaint(gfx::Rect(layer_->size()));
}

void HoldingSpaceTrayIconPreview::AdjustForShelfAlignmentAndTextDirection(
    gfx::Vector2dF* vector_2df) {
  if (!shelf_->IsHorizontalAlignment()) {
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
  const gfx::Size size = GetPreviewSize();
  gfx::Point origin;
  if (shelf_->IsHorizontalAlignment() && base::i18n::IsRTL()) {
    origin = container_->GetLocalBounds().top_right() -
             gfx::Vector2d(size.width(), 0);
  }
  gfx::Rect bounds(origin, size);
  if (bounds != layer_->bounds())
    layer_->SetBounds(bounds);
}

}  // namespace ash
