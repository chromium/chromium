// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/rounded_corner_decorator.h"

#include "ash/public/cpp/ash_features.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/compositor_extra/shadow.h"
#include "ui/wm/core/shadow_controller.h"

namespace ash {

RoundedCornerDecorator::RoundedCornerDecorator(aura::Window* shadow_window,
                                               aura::Window* layer_window,
                                               ui::Layer* layer,
                                               int radius)
    : layer_window_(layer_window), layer_(layer), radius_(radius) {
  layer_window_->AddObserver(this);
  layer_->AddObserver(this);
  layer_->SetRoundedCornerRadius({radius_, radius_, radius_, radius_});
  layer_->SetIsFastRoundedCorner(true);

  // Update the shadow if necessary.
  ui::Shadow* shadow = wm::ShadowController::GetShadowForWindow(shadow_window);
  if (shadow)
    shadow->SetRoundedCornerRadius(radius_);
}

RoundedCornerDecorator::~RoundedCornerDecorator() {
  Shutdown();
}

bool RoundedCornerDecorator::IsValid() {
  return !!layer_;
}

void RoundedCornerDecorator::LayerDestroyed(ui::Layer* layer) {
  Shutdown();
}

void RoundedCornerDecorator::OnWindowDestroying(aura::Window* window) {
  Shutdown();
}

void RoundedCornerDecorator::Shutdown() {
  if (!IsValid())
    return;
  layer_->SetRoundedCornerRadius({0, 0, 0, 0});

  layer_->RemoveObserver(this);
  layer_window_->RemoveObserver(this);
  layer_ = nullptr;
  layer_window_ = nullptr;
}

}  // namespace ash
