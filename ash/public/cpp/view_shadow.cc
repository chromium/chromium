// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/view_shadow.h"

#include "ui/compositor/layer.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/views/view.h"

namespace ash {

ViewShadow::ViewShadow(views::View* view, int elevation)
    : view_(view), shadow_(std::make_unique<ui::Shadow>()) {
  if (!view_->layer())
    view_->SetPaintToLayer();
  shadow_->Init(elevation);
  view_->AddLayerBeneathView(shadow_->layer());
  shadow_->SetContentBounds(view_->layer()->bounds());
  view_->AddObserver(this);
  shadow_->AddObserver(this);
}

ViewShadow::~ViewShadow() {
  if (view_)
    OnViewIsDeleting(view_);
}

void ViewShadow::SetRoundedCornerRadius(int corner_radius) {
  if (!view_)
    return;
  view_->layer()->SetRoundedCornerRadius(gfx::RoundedCornersF(corner_radius));
  shadow_->SetRoundedCornerRadius(corner_radius);
}

void ViewShadow::OnLayerRecreated(ui::Layer* old_layer) {
  if (!view_)
    return;
  view_->RemoveLayerBeneathViewKeepInLayerTree(old_layer);
  view_->AddLayerBeneathView(shadow_->layer());
}

void ViewShadow::OnLayerTargetBoundsChanged(views::View* view) {
  shadow_->SetContentBounds(view->layer()->bounds());
}

void ViewShadow::OnViewIsDeleting(views::View* view) {
  shadow_->RemoveObserver(this);
  shadow_.reset();
  view_->RemoveObserver(this);
  view_ = nullptr;
}

}  // namespace ash
