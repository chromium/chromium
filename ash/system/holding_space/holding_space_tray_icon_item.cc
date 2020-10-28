// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/holding_space/holding_space_tray_icon_item.h"

#include "ash/public/cpp/holding_space/holding_space_constants.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/system/holding_space/holding_space_tray_icon.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"

namespace ash {

namespace {

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

// TODO(crbug.com/1142572): Layer should visually represent `item_`.
void HoldingSpaceTrayIconItem::CreateLayer() {
  DCHECK(!layer_);
  layer_ = std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR);
  layer_->SetBounds(gfx::Rect(0, 0, kTrayItemSize, kTrayItemSize));
  layer_->SetColor(SkColorSetA(SK_ColorWHITE, 0.26 * 0xFF));
  layer_->SetIsFastRoundedCorner(true);
  layer_->SetRoundedCornerRadius(gfx::RoundedCornersF(kTrayItemSize / 2));
  layer_->SetTransform(transform_);
}

// TODO(crbug.com/1142572): Handle side shelf.
bool HoldingSpaceTrayIconItem::NeedsLayer() const {
  const float x = transform_.To2dTranslation().x();
  return x < kHoldingSpaceTrayIconMaxVisibleItems * kTrayItemSize / 2;
}

}  // namespace ash
