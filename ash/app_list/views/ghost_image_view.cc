// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/ghost_image_view.h"

#include <algorithm>
#include <memory>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"

namespace ash {

namespace {

constexpr int kGhostCircleStrokeWidth = 2;
constexpr int kGhostColorOpacity = 0x4D;  // 30% opacity.
constexpr int kRootGridGhostColor = gfx::kGoogleGrey200;
constexpr int kInFolderGhostColor = gfx::kGoogleGrey700;
constexpr base::TimeDelta kGhostFadeInOutLength =
    base::TimeDelta::FromMilliseconds(180);
constexpr gfx::Tween::Type kGhostTween = gfx::Tween::FAST_OUT_SLOW_IN;

}  // namespace

GhostImageView::GhostImageView(bool is_folder, bool is_in_folder, int page)
    : is_hiding_(false),
      is_in_folder_(is_in_folder),
      is_folder_(is_folder),
      page_(page) {}

GhostImageView::~GhostImageView() {
  StopObservingImplicitAnimations();
}

void GhostImageView::Init(AppListItemView* drag_view,
                          const gfx::Rect& drop_target_bounds) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetOpacity(0.0f);
  SetBoundsRect(drop_target_bounds);
  icon_bounds_ = drag_view->GetIconBounds();

  if (is_folder_) {
    inner_icon_radius_ =
        drag_view->GetAppListConfig().item_icon_in_folder_icon_size().width() /
        2;

    AppListFolderItem* folder_item =
        static_cast<AppListFolderItem*>(drag_view->item());
    num_items_ = std::min(FolderImage::kNumFolderTopItems,
                          folder_item->item_list()->item_count());

    std::vector<gfx::Rect> top_icon_bounds = FolderImage::GetTopIconsBounds(
        drag_view->GetAppListConfig(), icon_bounds_, num_items_.value());

    // Push back the position for each app to be shown within the folder icon.
    for (size_t i = 0; i < num_items_.value(); i++) {
      inner_folder_icon_origins_.push_back(top_icon_bounds[i].CenterPoint());
    }
  }
}

void GhostImageView::FadeOut() {
  if (is_hiding_)
    return;
  is_hiding_ = true;
  DoAnimation(true /* fade out */);
}

void GhostImageView::FadeIn() {
  DoAnimation(false /* fade in */);
}

void GhostImageView::SetTransitionOffset(
    const gfx::Vector2d& transition_offset) {
  SetPosition(bounds().origin() + transition_offset);
}

const char* GhostImageView::GetClassName() const {
  return "GhostImageView";
}

void GhostImageView::DoAnimation(bool hide) {
  ui::ScopedLayerAnimationSettings animation(layer()->GetAnimator());
  animation.SetTransitionDuration(kGhostFadeInOutLength);
  animation.SetTweenType(kGhostTween);

  if (hide) {
    animation.AddObserver(this);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    layer()->SetOpacity(0.0f);
    return;
  }
  layer()->SetOpacity(1.0f);
}

void GhostImageView::OnPaint(gfx::Canvas* canvas) {
  const gfx::PointF circle_center(icon_bounds_.CenterPoint());

  // Draw a circle to represent the ghost image icon.
  cc::PaintFlags circle_flags;
  circle_flags.setAntiAlias(true);
  circle_flags.setColor(is_in_folder_ ? kInFolderGhostColor
                                      : kRootGridGhostColor);
  circle_flags.setAlpha(kGhostColorOpacity);
  circle_flags.setStyle(cc::PaintFlags::kStroke_Style);
  circle_flags.setStrokeWidth(kGhostCircleStrokeWidth);

  const float ghost_radius = icon_bounds_.width() / 2;

  // Draw a circle to represent an app or folder outline.
  canvas->DrawCircle(circle_center, ghost_radius, circle_flags);

  if (is_folder_) {
    // Draw a mask so inner folder icons do not overlap the outer circle.
    SkPath outer_circle_mask;
    outer_circle_mask.addCircle(circle_center.x(), circle_center.y(),
                                ghost_radius - kGhostCircleStrokeWidth / 2);
    canvas->ClipPath(outer_circle_mask, true);

    // Draw ghost items within the ghost folder circle.
    for (size_t i = 0; i < num_items_.value(); i++) {
      canvas->DrawCircle(inner_folder_icon_origins_[i], inner_icon_radius_,
                         circle_flags);
    }
  }
  ImageView::OnPaint(canvas);
}

void GhostImageView::OnImplicitAnimationsCompleted() {
  // Delete this GhostImageView when the fade out animation is done.
  delete this;
}

}  // namespace ash
