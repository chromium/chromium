// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/ui/photo_view.h"

#include <algorithm>

#include "ash/ambient/ambient_constants.h"
#include "ash/ambient/ambient_controller.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

PhotoView::PhotoView(AmbientController* ambient_controller)
    : ambient_controller_(ambient_controller) {
  Init();
}

PhotoView::~PhotoView() {
  // |ambient_controller_| outlives this view.
  ambient_controller_->RemovePhotoModelObserver(this);
}

const char* PhotoView::GetClassName() const {
  return "PhotoView";
}

void PhotoView::AddedToWidget() {
  // Set the bounds to show |image_view_curr_| for the first time.
  // TODO(b/140066694): Handle display configuration changes, e.g. resolution,
  // rotation, etc.
  const gfx::Size widget_size = GetWidget()->GetRootView()->size();
  image_view_prev_->SetImageSize(widget_size);
  image_view_curr_->SetImageSize(widget_size);
  image_view_next_->SetImageSize(widget_size);
  gfx::Rect view_bounds = gfx::Rect(GetPreferredSize());
  const int width = widget_size.width();
  view_bounds.set_x(-width);
  SetBoundsRect(view_bounds);
}

void PhotoView::OnImagesChanged() {
  UpdateImages();
  StartSlideAnimation();
}

void PhotoView::Init() {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStart);

  image_view_prev_ = AddChildView(std::make_unique<views::ImageView>());
  image_view_curr_ = AddChildView(std::make_unique<views::ImageView>());
  image_view_next_ = AddChildView(std::make_unique<views::ImageView>());

  // |ambient_controller_| outlives this view.
  ambient_controller_->AddPhotoModelObserver(this);
}

void PhotoView::UpdateImages() {
  // TODO(b/140193766): Investigate a more efficient way to update images and do
  // layer animation.
  auto& model = ambient_controller_->model();
  image_view_prev_->SetImage(model.GetPrevImage());
  image_view_curr_->SetImage(model.GetCurrImage());
  image_view_next_->SetImage(model.GetNextImage());
}

void PhotoView::StartSlideAnimation() {
  if (!CanAnimate())
    return;

  ui::Layer* layer = this->layer();
  const int x_offset = image_view_prev_->GetPreferredSize().width();
  gfx::Transform transform;
  transform.Translate(x_offset, 0);
  layer->SetTransform(transform);
  {
    ui::ScopedLayerAnimationSettings animation(layer->GetAnimator());
    animation.SetTransitionDuration(kAnimationDuration);
    animation.SetTweenType(gfx::Tween::EASE_OUT);
    animation.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
    layer->SetTransform(gfx::Transform());
  }
}

bool PhotoView::CanAnimate() const {
  // Cannot do slide animation from previous to current image.
  return !image_view_prev_->GetImage().isNull();
}

}  // namespace ash
