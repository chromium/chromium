// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_background_layer.h"

#include "ui/compositor/layer_animation_element.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_delegate.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_paint_util.h"
#include "ui/views/animation/ink_drop_painted_layer_delegates.h"

namespace ash {
namespace {

constexpr float kBackgroundInitSizeDip = 48.f;
constexpr float kBackgroundSizeDip = 48.f;
constexpr float kBackgroundShadowElevationDip = 24.f;
// TODO(xiaohuic): this is 2x device size, 1x actually have a different size.
// Need to figure out a way to dynamically change sizes.
constexpr float kBackgroundLargeWidthDip = 352.5f;
constexpr float kBackgroundLargeHeightDip = 540.0f;
constexpr float kBackgroundCornerRadiusDip = 12.f;
constexpr int kBackgroundMorphDurationMs = 150;

// The minimum scale factor to use when scaling rectangle layers. Smaller values
// were causing visual anomalies.
constexpr float kMinimumRectScale = 0.0001f;

// The minimum scale factor to use when scaling circle layers. Smaller values
// were causing visual anomalies.
constexpr float kMinimumCircleScale = 0.001f;

}  // namespace

AssistantBackgroundLayer::AssistantBackgroundLayer()
    : Layer(ui::LAYER_NOT_DRAWN),
      large_size_(
          gfx::Size(kBackgroundLargeWidthDip, kBackgroundLargeHeightDip)),
      small_size_(gfx::Size(kBackgroundSizeDip, kBackgroundSizeDip)),
      center_point_(
          gfx::PointF(kBackgroundSizeDip / 2, kBackgroundSizeDip / 2)),
      circle_layer_delegate_(
          std::make_unique<views::CircleLayerDelegate>(SK_ColorWHITE,
                                                       kBackgroundSizeDip / 2)),
      rect_layer_delegate_(std::make_unique<views::RectangleLayerDelegate>(
          SK_ColorWHITE,
          gfx::SizeF(small_size_))) {
  set_name("AssistantOverlay:BACKGROUND_LAYER");
  SetBounds(gfx::Rect(0, 0, kBackgroundInitSizeDip, kBackgroundInitSizeDip));
  SetFillsBoundsOpaquely(false);
  SetMasksToBounds(false);

  shadow_values_ =
      gfx::ShadowValue::MakeMdShadowValues(kBackgroundShadowElevationDip);
  const gfx::Insets shadow_margin = gfx::ShadowValue::GetMargin(shadow_values_);

  border_shadow_delegate_ = std::make_unique<views::BorderShadowLayerDelegate>(
      shadow_values_, gfx::Rect(large_size_), SK_ColorWHITE,
      kBackgroundCornerRadiusDip);

  large_shadow_layer_ = std::make_unique<ui::Layer>();
  large_shadow_layer_->set_delegate(border_shadow_delegate_.get());
  large_shadow_layer_->SetFillsBoundsOpaquely(false);
  large_shadow_layer_->SetBounds(
      gfx::Rect(shadow_margin.left(), shadow_margin.top(),
                kBackgroundLargeWidthDip - shadow_margin.width(),
                kBackgroundLargeHeightDip - shadow_margin.height()));
  Add(large_shadow_layer_.get());

  shadow_layer_ = std::make_unique<ui::Layer>();
  shadow_layer_->set_delegate(this);
  shadow_layer_->SetFillsBoundsOpaquely(false);
  shadow_layer_->SetBounds(
      gfx::Rect(shadow_margin.left(), shadow_margin.top(),
                kBackgroundInitSizeDip - shadow_margin.width(),
                kBackgroundInitSizeDip - shadow_margin.height()));
  Add(shadow_layer_.get());

  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    AddPaintLayer(static_cast<PaintedShape>(i));
}

AssistantBackgroundLayer::~AssistantBackgroundLayer() = default;

void AssistantBackgroundLayer::MoveLargeShadow(const gfx::PointF& new_center) {
  gfx::Transform transform;
  transform.Translate(new_center.x() - kBackgroundLargeWidthDip / 2,
                      new_center.y() - kBackgroundLargeHeightDip / 2);
  large_shadow_layer_->SetTransform(transform);
}

void AssistantBackgroundLayer::AnimateToLarge(
    const gfx::PointF& new_center,
    ui::LayerAnimationObserver* animation_observer) {
  PaintedShapeTransforms transforms;
  // Setup the painted layers to be the small round size and show it
  CalculateCircleTransforms(small_size_, &transforms);
  SetTransforms(transforms);
  SetPaintedLayersVisible(true);

  // Hide the shadow layer
  shadow_layer_->SetVisible(false);
  // Also hide the large shadow layer, it will be shown when animation ends.
  large_shadow_layer_->SetVisible(false);
  // Move the shadow to the right place.
  MoveLargeShadow(new_center);

  center_point_ = new_center;
  // Animate the painted layers to the large rectangle size
  CalculateRectTransforms(large_size_, kBackgroundCornerRadiusDip, &transforms);

  AnimateToTransforms(
      transforms, base::TimeDelta::FromMilliseconds(kBackgroundMorphDurationMs),
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET,
      gfx::Tween::LINEAR_OUT_SLOW_IN, animation_observer);
}

void AssistantBackgroundLayer::SetToLarge(const gfx::PointF& new_center) {
  PaintedShapeTransforms transforms;
  SetPaintedLayersVisible(true);
  // Hide the shadow layer
  shadow_layer_->SetVisible(false);
  // Show the large shadow behind
  large_shadow_layer_->SetVisible(true);
  // Move the shadow to the right place.
  MoveLargeShadow(new_center);

  center_point_ = new_center;
  // Set the painted layers to the large rectangle size
  CalculateRectTransforms(large_size_, kBackgroundCornerRadiusDip, &transforms);
  SetTransforms(transforms);
}

void AssistantBackgroundLayer::ResetShape() {
  // This reverts to the original small round shape.
  shadow_layer_->SetVisible(true);
  large_shadow_layer_->SetVisible(false);
  SetPaintedLayersVisible(false);
  center_point_.SetPoint(small_size_.width() / 2.f, small_size_.height() / 2.f);
}

void AssistantBackgroundLayer::AddPaintLayer(PaintedShape painted_shape) {
  ui::LayerDelegate* delegate = nullptr;
  switch (painted_shape) {
    case TOP_LEFT_CIRCLE:
    case TOP_RIGHT_CIRCLE:
    case BOTTOM_RIGHT_CIRCLE:
    case BOTTOM_LEFT_CIRCLE:
      delegate = circle_layer_delegate_.get();
      break;
    case HORIZONTAL_RECT:
    case VERTICAL_RECT:
      delegate = rect_layer_delegate_.get();
      break;
    case PAINTED_SHAPE_COUNT:
      NOTREACHED() << "PAINTED_SHAPE_COUNT is not an actual shape type.";
      break;
  }

  ui::Layer* layer = new ui::Layer();
  Add(layer);

  layer->SetBounds(gfx::Rect(small_size_));
  layer->SetFillsBoundsOpaquely(false);
  layer->set_delegate(delegate);
  layer->SetVisible(true);
  layer->SetOpacity(1.0);
  layer->SetMasksToBounds(false);
  layer->set_name("PAINTED_SHAPE_COUNT:" + ToLayerName(painted_shape));

  painted_layers_[static_cast<int>(painted_shape)].reset(layer);
}

void AssistantBackgroundLayer::SetTransforms(
    const PaintedShapeTransforms transforms) {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    painted_layers_[i]->SetTransform(transforms[i]);
}

void AssistantBackgroundLayer::SetPaintedLayersVisible(bool visible) {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i)
    painted_layers_[i]->SetVisible(visible);
}

void AssistantBackgroundLayer::CalculateCircleTransforms(
    const gfx::Size& size,
    PaintedShapeTransforms* transforms_out) const {
  CalculateRectTransforms(size, std::min(size.width(), size.height()) / 2.0f,
                          transforms_out);
}

void AssistantBackgroundLayer::CalculateRectTransforms(
    const gfx::Size& desired_size,
    float corner_radius,
    PaintedShapeTransforms* transforms_out) const {
  DCHECK_GE(desired_size.width() / 2.0f, corner_radius)
      << "The circle's diameter should not be greater than the total width.";
  DCHECK_GE(desired_size.height() / 2.0f, corner_radius)
      << "The circle's diameter should not be greater than the total height.";

  gfx::SizeF size(desired_size);
  // This function can be called before the layer's been added to a view,
  // either at construction time or in tests.
  if (GetCompositor()) {
    // Modify |desired_size| so that the ripple aligns to pixel bounds.
    const float dsf = GetCompositor()->device_scale_factor();
    gfx::RectF ripple_bounds((gfx::PointF(center_point_)), gfx::SizeF());
    ripple_bounds.Inset(-gfx::InsetsF(desired_size.height() / 2.0f,
                                      desired_size.width() / 2.0f));
    ripple_bounds.Scale(dsf);
    ripple_bounds = gfx::RectF(gfx::ToEnclosingRect(ripple_bounds));
    ripple_bounds.Scale(1.0f / dsf);
    size = ripple_bounds.size();
  }

  // The shapes are drawn such that their center points are not at the origin.
  // Thus we use the CalculateCircleTransform() and CalculateRectTransform()
  // methods to calculate the complex Transforms.

  const float circle_scale = std::max(
      kMinimumCircleScale,
      corner_radius / static_cast<float>(circle_layer_delegate_->radius()));

  const float circle_target_x_offset = size.width() / 2.0f - corner_radius;
  const float circle_target_y_offset = size.height() / 2.0f - corner_radius;

  (*transforms_out)[TOP_LEFT_CIRCLE] = CalculateCircleTransform(
      circle_scale, -circle_target_x_offset, -circle_target_y_offset);
  (*transforms_out)[TOP_RIGHT_CIRCLE] = CalculateCircleTransform(
      circle_scale, circle_target_x_offset, -circle_target_y_offset);
  (*transforms_out)[BOTTOM_RIGHT_CIRCLE] = CalculateCircleTransform(
      circle_scale, circle_target_x_offset, circle_target_y_offset);
  (*transforms_out)[BOTTOM_LEFT_CIRCLE] = CalculateCircleTransform(
      circle_scale, -circle_target_x_offset, circle_target_y_offset);

  const float rect_delegate_width = rect_layer_delegate_->size().width();
  const float rect_delegate_height = rect_layer_delegate_->size().height();

  (*transforms_out)[HORIZONTAL_RECT] = CalculateRectTransform(
      std::max(kMinimumRectScale, size.width() / rect_delegate_width),
      std::max(kMinimumRectScale,
               (size.height() - 2.0f * corner_radius) / rect_delegate_height));

  (*transforms_out)[VERTICAL_RECT] = CalculateRectTransform(
      std::max(kMinimumRectScale,
               (size.width() - 2.0f * corner_radius) / rect_delegate_width),
      std::max(kMinimumRectScale, size.height() / rect_delegate_height));
}

gfx::Transform AssistantBackgroundLayer::CalculateCircleTransform(
    float scale,
    float target_center_x,
    float target_center_y) const {
  gfx::Transform transform;
  // Offset for the center point of the ripple.
  transform.Translate(center_point_.x(), center_point_.y());
  // Move circle to target.
  transform.Translate(target_center_x, target_center_y);
  transform.Scale(scale, scale);
  // Align center point of the painted circle.
  const gfx::Vector2dF circle_center_offset =
      circle_layer_delegate_->GetCenteringOffset();
  transform.Translate(-circle_center_offset.x(), -circle_center_offset.y());
  return transform;
}

gfx::Transform AssistantBackgroundLayer::CalculateRectTransform(
    float x_scale,
    float y_scale) const {
  gfx::Transform transform;
  transform.Translate(center_point_.x(), center_point_.y());
  transform.Scale(x_scale, y_scale);
  const gfx::Vector2dF rect_center_offset =
      rect_layer_delegate_->GetCenteringOffset();
  transform.Translate(-rect_center_offset.x(), -rect_center_offset.y());
  return transform;
}

void AssistantBackgroundLayer::AnimateToTransforms(
    const PaintedShapeTransforms transforms,
    base::TimeDelta duration,
    ui::LayerAnimator::PreemptionStrategy preemption_strategy,
    gfx::Tween::Type tween,
    ui::LayerAnimationObserver* animation_observer) {
  for (int i = 0; i < PAINTED_SHAPE_COUNT; ++i) {
    ui::LayerAnimator* animator = painted_layers_[i]->GetAnimator();
    ui::ScopedLayerAnimationSettings animation(animator);
    animation.SetPreemptionStrategy(preemption_strategy);
    animation.SetTweenType(tween);
    std::unique_ptr<ui::LayerAnimationElement> element =
        ui::LayerAnimationElement::CreateTransformElement(transforms[i],
                                                          duration);
    ui::LayerAnimationSequence* sequence =
        new ui::LayerAnimationSequence(std::move(element));

    if (animation_observer)
      sequence->AddObserver(animation_observer);

    animator->StartAnimation(sequence);
  }

  {
    ui::ScopedLayerAnimationSettings animation(
        large_shadow_layer_->GetAnimator());
    animation.SetTweenType(tween);
    animation.SetTransitionDuration(duration);

    large_shadow_layer_->SetVisible(true);
  }
}

std::string AssistantBackgroundLayer::ToLayerName(PaintedShape painted_shape) {
  switch (painted_shape) {
    case TOP_LEFT_CIRCLE:
      return "TOP_LEFT_CIRCLE";
    case TOP_RIGHT_CIRCLE:
      return "TOP_RIGHT_CIRCLE";
    case BOTTOM_RIGHT_CIRCLE:
      return "BOTTOM_RIGHT_CIRCLE";
    case BOTTOM_LEFT_CIRCLE:
      return "BOTTOM_LEFT_CIRCLE";
    case HORIZONTAL_RECT:
      return "HORIZONTAL_RECT";
    case VERTICAL_RECT:
      return "VERTICAL_RECT";
    case PAINTED_SHAPE_COUNT:
      NOTREACHED() << "The PAINTED_SHAPE_COUNT value should never be used.";
      return "PAINTED_SHAPE_COUNT";
  }
  return "UNKNOWN";
}

void AssistantBackgroundLayer::OnPaintLayer(const ui::PaintContext& context) {
  // Radius is based on the parent layer size, the shadow layer is expanded
  // to make room for the shadow.
  float radius = size().width() / 2.f;

  ui::PaintRecorder recorder(context, shadow_layer_->size());
  gfx::Canvas* canvas = recorder.canvas();

  cc::PaintFlags flags;
  flags.setColor(SK_ColorWHITE);
  flags.setAntiAlias(true);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  flags.setLooper(gfx::CreateShadowDrawLooper(shadow_values_));
  gfx::Rect shadow_bounds = shadow_layer_->bounds();
  canvas->DrawCircle(
      gfx::PointF(radius - shadow_bounds.x(), radius - shadow_bounds.y()),
      radius, flags);
}

}  // namespace ash
