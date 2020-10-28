// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon_item.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/shadow_util.h"
#include "ui/gfx/skia_paint_util.h"

namespace ash {

namespace {

// Appearance.
constexpr int kElevation = 2;

// Returns the shadow details to use when painting elevation for `layer`.
const gfx::ShadowDetails GetShadowDetails(const ui::Layer* layer) {
  DCHECK(layer);
  const gfx::Size size = layer->size();
  return gfx::ShadowDetails::Get(
      kElevation, /*radius=*/std::min(size.width(), size.height()) / 2);
}

// Performs set up of the specified `animation_settings`.
void SetUpAnimation(ui::ScopedLayerAnimationSettings* animation_settings) {
  animation_settings->SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  animation_settings->SetTransitionDuration(
      ShelfConfig::Get()->shelf_animation_duration());
  animation_settings->SetTweenType(gfx::Tween::EASE_OUT);
}

}  // namespace

// HoldingSpaceTrayIconItem ----------------------------------------------------

HoldingSpaceTrayIconItem::HoldingSpaceTrayIconItem(HoldingSpaceTrayIcon* icon,
                                                   const HoldingSpaceItem* item)
    : icon_(icon), item_(item) {}

HoldingSpaceTrayIconItem::~HoldingSpaceTrayIconItem() = default;

// TODO(crbug.com/1142572): Handle side shelf.
void HoldingSpaceTrayIconItem::AnimateIn() {
  CreateLayer();

  gfx::Transform pre_transform(transform_);
  pre_transform.Translate(0, -kTrayItemSize);
  layer_->SetTransform(pre_transform);

  icon_->layer()->Add(layer_.get());

  ui::ScopedLayerAnimationSettings animation_settings(layer_->GetAnimator());
  SetUpAnimation(&animation_settings);

  layer_->SetTransform(transform_);
}

// TODO(crbug.com/1142572): Handle side shelf.
void HoldingSpaceTrayIconItem::AnimateOut(
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

// TODO(crbug.com/1142572): Handle side shelf and RTL.
void HoldingSpaceTrayIconItem::AnimateShift() {
  transform_.Translate(kTrayItemSize / 2, 0);

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

// TODO(crbug.com/1142572): Handle side shelf and RTL.
void HoldingSpaceTrayIconItem::AnimateUnshift() {
  transform_.Translate(-kTrayItemSize / 2, 0);

  if (!layer_ && !NeedsLayer())
    return;

  if (!layer_) {
    CreateLayer();

    gfx::Transform pre_transform(transform_);
    pre_transform.Translate(kTrayItemSize / 2, 0);
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

void HoldingSpaceTrayIconItem::OnPaintLayer(const ui::PaintContext& context) {
  DCHECK(layer_);

  const gfx::ShadowDetails& shadow = GetShadowDetails(layer_.get());
  const gfx::Insets shadow_margins(gfx::ShadowValue::GetMargin(shadow.values));

  gfx::Size contents_size = layer_->size();
  contents_size.Enlarge(shadow_margins.width(), shadow_margins.height());
  gfx::Rect contents_bounds = layer_->bounds();
  contents_bounds.ClampToCenteredSize(contents_size);

  ui::PaintRecorder recorder(context, layer_->size());
  PaintBackground(recorder.canvas(), contents_bounds);
  PaintContents(recorder.canvas(), contents_bounds);
}

void HoldingSpaceTrayIconItem::OnDeviceScaleFactorChanged(
    float old_device_scale_factor,
    float new_device_scale_factor) {
  DCHECK(layer_);
  layer_->SchedulePaint(layer_->bounds());
}

void HoldingSpaceTrayIconItem::OnImplicitAnimationsCompleted() {
  if (layer_->visible())
    return;

  icon_->layer()->Remove(layer_.get());
  layer_.reset();

  if (animate_out_closure_) {
    // NOTE: Running `animate_out_closure_` may delete `this`.
    std::move(animate_out_closure_).Run();
  }
}

void HoldingSpaceTrayIconItem::CreateLayer() {
  DCHECK(!layer_);
  layer_ = std::make_unique<ui::Layer>(ui::LAYER_TEXTURED);
  layer_->SetBounds(gfx::Rect(0, 0, kTrayItemSize, kTrayItemSize));
  layer_->SetTransform(transform_);
  layer_->set_delegate(this);
}

// TODO(crbug.com/1142572): Handle side shelf.
bool HoldingSpaceTrayIconItem::NeedsLayer() const {
  const float x = transform_.To2dTranslation().x();
  return x < kHoldingSpaceTrayIconMaxVisibleItems * kTrayItemSize / 2;
}

// TODO(crbug.com/1142572): Support theming.
void HoldingSpaceTrayIconItem::PaintBackground(
    gfx::Canvas* canvas,
    const gfx::Rect& contents_bounds) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(SK_ColorWHITE);
  flags.setLooper(
      gfx::CreateShadowDrawLooper(GetShadowDetails(layer_.get()).values));
  canvas->DrawCircle(
      contents_bounds.CenterPoint(),
      std::min(contents_bounds.width(), contents_bounds.height()) / 2, flags);
}

// TODO(crbug.com/1142572): Implement.
void HoldingSpaceTrayIconItem::PaintContents(gfx::Canvas* canvas,
                                             const gfx::Rect& contents_bounds) {
  NOTIMPLEMENTED();
}

}  // namespace ash
