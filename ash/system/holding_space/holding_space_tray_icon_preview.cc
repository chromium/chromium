// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon_preview.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/holding_space/holding_space_image.h"
#include "ash/public/cpp/holding_space/holding_space_item.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/holding_space/holding_space_progress_indicator.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "base/bind.h"
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
    base::Milliseconds(250);

// The delay with which preview icon is dropped into the holding space tray
// icon.
constexpr base::TimeDelta kBounceAnimationBaseDelay = base::Milliseconds(150);

// The duration of shift animation.
constexpr base::TimeDelta kShiftAnimationDuration = base::Milliseconds(250);

// Helpers ---------------------------------------------------------------------

// Returns true if small previews should be used given the current shelf
// configuration, false otherwise.
bool ShouldUseSmallPreviews() {
  ShelfConfig* const shelf_config = ShelfConfig::Get();
  return shelf_config->in_tablet_mode() && shelf_config->is_in_app();
}

// Returns the size for previews. If `use_small_previews` is absent it will be
// determined from the current shelf configuration.
gfx::Size GetPreviewSize(
    const absl::optional<bool>& use_small_previews = absl::nullopt) {
  return use_small_previews.value_or(ShouldUseSmallPreviews())
             ? gfx::Size(kHoldingSpaceTrayIconSmallPreviewSize,
                         kHoldingSpaceTrayIconSmallPreviewSize)
             : gfx::Size(kHoldingSpaceTrayIconDefaultPreviewSize,
                         kHoldingSpaceTrayIconDefaultPreviewSize);
}

// Returns the shadow details for painting elevation.
const gfx::ShadowDetails& GetShadowDetails() {
  const gfx::Size size(GetPreviewSize());
  const int radius = std::min(size.height(), size.width()) / 2;
  return gfx::ShadowDetails::Get(kElevation, radius);
}

// Adjust the specified `origin` for shadow margins.
void AdjustOriginForShadowMargins(gfx::Point& origin, const Shelf* shelf) {
  const gfx::ShadowValues& values(GetShadowDetails().values);
  const gfx::Insets margins(gfx::ShadowValue::GetMargin(values));
  if (shelf->IsHorizontalAlignment()) {
    // When the `shelf` is horizontally aligned the `origin` will already have
    // been offset to center the preview `layer()` vertically within its parent
    // container so no further vertical offset  is needed.
    const int offset = margins.width() / 2;
    origin.Offset(base::i18n::IsRTL() ? -offset : offset, 0);
  } else {
    origin.Offset(margins.width() / 2, margins.height() / 2);
  }
}

// Enlarges the specified `size` for shadow margins.
void EnlargeForShadowMargins(gfx::Size& size) {
  const gfx::ShadowValues& values(GetShadowDetails().values);
  const gfx::Insets margins(gfx::ShadowValue::GetMargin(values));
  size.Enlarge(-margins.width(), -margins.height());
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
    : shelf_(shelf),
      container_(container),
      item_(item),
      progress_indicator_(HoldingSpaceProgressIndicator::CreateForItem(item_)),
      use_small_previews_(ShouldUseSmallPreviews()) {
  // Initialize the `contents_image_`.
  OnHoldingSpaceItemImageChanged();

  item_deletion_subscription_ = item->AddDeletionCallback(base::BindRepeating(
      &HoldingSpaceTrayIconPreview::OnHoldingSpaceItemDeleted,
      base::Unretained(this)));

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

  int pre_translate_y = -preview_size.height();
  if (IsHorizontal(shelf_->alignment())) {
    const gfx::Size& container_size = container_->size();
    pre_translate_y = -container_size.height() +
                      (container_size.height() - preview_size.height()) / 2;
  }

  gfx::Transform pre_transform;
  pre_transform.Translate(transform_.To2dTranslation().x(), pre_translate_y);

  CreateLayer(pre_transform);

  gfx::Transform mid_transform(transform_);
  mid_transform.Translate(0, preview_size.height() * 0.25f);

  ui::ScopedLayerAnimationSettings scoped_settings(layer()->GetAnimator());
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

  layer()->GetAnimator()->StartAnimation(sequence.release());
}

void HoldingSpaceTrayIconPreview::AnimateOut(
    base::OnceClosure animate_out_closure) {
  animate_out_closure_ = std::move(animate_out_closure);
  pending_index_.reset();
  index_.reset();

  if (!layer()) {
    std::move(animate_out_closure_).Run();
    return;
  }

  ui::ScopedLayerAnimationSettings animation_settings(layer()->GetAnimator());
  SetUpAnimation(&animation_settings);
  animation_settings.AddObserver(this);

  layer()->SetOpacity(0.f);
  layer()->SetVisible(false);
}

void HoldingSpaceTrayIconPreview::AnimateShift(base::TimeDelta delay) {
  DCHECK(index_.has_value());
  DCHECK(pending_index_.has_value());

  index_ = *pending_index_;
  pending_index_.reset();

  bool created_layer = false;
  if (!layer() && NeedsLayer()) {
    CreateLayer(transform_);
    created_layer = true;
  }

  // Calculate the target preview transform for the new position in the icon.
  // Avoid adjustments based on relative index change, as the current transform
  // may not match the previous index in case the icon view has been resized
  // since last update - see `AdjustTransformForContainerSizeChange()`.
  transform_ = gfx::Transform();
  gfx::Vector2dF translation(index_.value() * GetPreviewSize().width() / 2, 0);
  AdjustForShelfAlignmentAndTextDirection(&translation);
  transform_.Translate(translation);

  if (!layer())
    return;

  // If the `layer()` has just been created because it is shifting into the
  // viewport, animate in its opacity.
  if (created_layer)
    layer()->SetOpacity(0.f);

  ui::ScopedLayerAnimationSettings scoped_settings(layer()->GetAnimator());
  scoped_settings.AddObserver(this);
  scoped_settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);

  auto opacity_sequence = std::make_unique<ui::LayerAnimationSequence>();
  if (created_layer) {
    opacity_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
        ui::LayerAnimationElement::OPACITY, delay));
    opacity_sequence->AddElement(
        ui::LayerAnimationElement::CreateOpacityElement(
            1.f, kShiftAnimationDuration));
  }

  auto transform_sequence = std::make_unique<ui::LayerAnimationSequence>();
  transform_sequence->AddElement(ui::LayerAnimationElement::CreatePauseElement(
      ui::LayerAnimationElement::TRANSFORM, delay));

  std::unique_ptr<ui::LayerAnimationElement> shift =
      ui::LayerAnimationElement::CreateTransformElement(
          transform_, kShiftAnimationDuration);
  shift->set_tween_type(gfx::Tween::FAST_OUT_SLOW_IN);
  transform_sequence->AddElement(std::move(shift));

  layer()->GetAnimator()->StartTogether(
      {opacity_sequence.release(), transform_sequence.release()});
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

  // Because shelf orientation has changed, the target `transform_` needs to be
  // updated. First stop the current animation to immediately advance to target
  // end values.
  const auto weak_ptr = weak_factory_.GetWeakPtr();
  if (layer() && layer()->GetAnimator()->is_animating())
    layer()->GetAnimator()->StopAnimating();

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

  if (layer()) {
    UpdateLayerBounds();
    layer()->SetTransform(transform_);
  }
}

void HoldingSpaceTrayIconPreview::OnShelfConfigChanged() {
  // If the change in shelf configuration hasn't affected whether or not small
  // previews should be used, no action needs to be taken.
  const bool use_small_previews = ShouldUseSmallPreviews();
  if (use_small_previews_ == use_small_previews)
    return;

  use_small_previews_ = use_small_previews;

  // Because the size of previews is changing, the target `transform_` needs to
  // be updated. First stop the current animation to immediately advance to
  // target end values.
  const auto weak_ptr = weak_factory_.GetWeakPtr();
  if (layer() && layer()->GetAnimator()->is_animating())
    layer()->GetAnimator()->StopAnimating();

  // This instance may have been deleted as a result of stopping the current
  // animation if it was in the process of animating out.
  if (!weak_ptr)
    return;

  // Adjust `translation` to account for the change in size.
  DCHECK(index_);
  gfx::Vector2dF translation(*index_ * GetPreviewSize().width() / 2, 0);
  AdjustForShelfAlignmentAndTextDirection(&translation);
  transform_.MakeIdentity();
  transform_.Translate(translation);

  if (layer()) {
    UpdateLayerBounds();
    layer()->SetTransform(transform_);
  }

  // Invalidate `contents_image_` so it is resized.
  OnHoldingSpaceItemImageChanged();
}

void HoldingSpaceTrayIconPreview::OnThemeChanged() {
  // Invalidate `contents_image_` so that file type icons will use the correct
  // foreground color to contrast the preview's background. Note that this will
  // trigger `layer()` invalidation at which time the background will be painted
  // in the appropriate color for the current theme.
  OnHoldingSpaceItemImageChanged();

  // Invalidate the `progress_indicator_` layer so that it gets repainted with
  // colors that are appropriately themed.
  progress_indicator_->InvalidateLayer();
}

void HoldingSpaceTrayIconPreview::OnPaintLayer(
    const ui::PaintContext& context) {
  ui::PaintRecorder recorder(context, layer()->size());
  gfx::Canvas* canvas = recorder.canvas();

  // The `layer()` was enlarged so that the shadow would be painted outside of
  // desired preview bounds. Content bounds should be clamped to preview size.
  gfx::Rect contents_bounds = gfx::Rect(layer()->size());
  contents_bounds.ClampToCenteredSize(GetPreviewSize());

  // Background.
  // NOTE: The background radius is shrunk by a single pixel to avoid being
  // painted outside `contents_image_` bounds as might otherwise occur due to
  // pixel rounding. Failure to do so could result in paint artifacts.
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(AshColorProvider::Get()->GetBaseLayerColor(
      AshColorProvider::BaseLayerType::kOpaque));
  flags.setLooper(gfx::CreateShadowDrawLooper(GetShadowDetails().values));
  canvas->DrawCircle(
      gfx::PointF(contents_bounds.CenterPoint()),
      std::min(contents_bounds.width(), contents_bounds.height()) / 2.f - 0.5f,
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
  if (!NeedsLayer())
    DestroyLayer();

  // NOTE: Running `animate_out_closure_` may delete `this`.
  if (animate_out_closure_)
    std::move(animate_out_closure_).Run();
}

void HoldingSpaceTrayIconPreview::OnViewBoundsChanged(views::View* view) {
  DCHECK_EQ(container_, view);
  if (layer())
    UpdateLayerBounds();
}

void HoldingSpaceTrayIconPreview::OnViewIsDeleting(views::View* view) {
  DCHECK_EQ(container_, view);
  container_observer_.Reset();
}

void HoldingSpaceTrayIconPreview::OnHoldingSpaceItemImageChanged() {
  if (item_) {
    const gfx::Size size(GetPreviewSize());
    contents_image_ = gfx::ImageSkia(
        std::make_unique<ContentsImageSource>(item_->image().GetImageSkia(
            size, AshColorProvider::Get()->IsDarkModeEnabled())),
        size);
  } else {
    contents_image_ = gfx::ImageSkia();
  }
  InvalidateLayer();
}

void HoldingSpaceTrayIconPreview::OnHoldingSpaceItemDeleted() {
  item_ = nullptr;
}

void HoldingSpaceTrayIconPreview::CreateLayer(
    const gfx::Transform& initial_transform) {
  DCHECK(!layer());
  DCHECK(!layer_owner_.OwnsLayer());

  auto new_layer = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  new_layer->set_delegate(this);
  new_layer->SetFillsBoundsOpaquely(false);
  new_layer->SetTransform(initial_transform);
  new_layer->Add(progress_indicator_->layer());
  layer_owner_.Reset(std::move(new_layer));

  UpdateLayerBounds();
  container_->layer()->Add(layer());
}

void HoldingSpaceTrayIconPreview::DestroyLayer() {
  if (layer())
    layer_owner_.ReleaseLayer();
}

bool HoldingSpaceTrayIconPreview::NeedsLayer() const {
  return index_ && *index_ < kHoldingSpaceTrayIconMaxVisiblePreviews;
}

void HoldingSpaceTrayIconPreview::InvalidateLayer() {
  if (layer())
    layer()->SchedulePaint(gfx::Rect(layer()->size()));
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
  DCHECK(layer());

  // The shadow for `layer()` should be painted outside desired preview bounds.
  gfx::Size size = GetPreviewSize();
  EnlargeForShadowMargins(size);

  // With a horizontal shelf in RTL, `layer()` is aligned with its parent
  // layer's right bound and translated with a negative offset. In all other
  // cases, `layer()` is aligned with its parent layer's left/top bound and
  // translated with a positive offset.
  gfx::Point origin;
  if (shelf_->IsHorizontalAlignment()) {
    gfx::Rect container_bounds = container_->GetLocalBounds();
    if (base::i18n::IsRTL())
      origin = container_bounds.top_right() - gfx::Vector2d(size.width(), 0);
    origin.Offset(0, (container_bounds.height() - size.height()) / 2);
  }

  AdjustOriginForShadowMargins(origin, shelf_);
  gfx::Rect bounds(origin, size);
  if (bounds == layer()->bounds())
    return;

  layer()->SetBounds(bounds);

  // The `layer()` was enlarged so that the shadow would be painted outside of
  // desired preview bounds. Progress indicator bounds are clamped to preview
  // size.
  gfx::Rect progress_indicator_bounds(layer()->size());
  progress_indicator_bounds.ClampToCenteredSize(GetPreviewSize());
  progress_indicator_->layer()->SetBounds(progress_indicator_bounds);
}

}  // namespace ash
